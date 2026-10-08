/* Sixies cooperative ARM debugger ABI 1. All wire words are big-endian.
 * Shared storage must be application-owned, 64-byte aligned, and accessible
 * by BOTH processors. No XACP address is reserved or inferred by this SDK.
 */
#ifndef SIXIES_ARM_DEBUG_PROTOCOL_H
#define SIXIES_ARM_DEBUG_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#define AD_MAGIC 0x53414431u
#define AD_ABI 1u
#define AD_PAGE_SIZE 1088u
#define AD_STATUS_SIZE 256u
#define AD_MAILBOX 256u
#define AD_LOG_BASE 320u
#define AD_LOG_SLOTS 8u
#define AD_LOG_SIZE 96u
#define AD_LOG_TEXT 88u
#define AD_MAX_VALUES 8u
#define AD_MAX_BREAKPOINTS 8u
#define AD_MAX_REGIONS 8u
#define AD_MAX_READ 64u
#define AD_FEATURE_COOPERATIVE 1u
#define AD_FEATURE_HOST_DEMO 2u
/* Target-owned status offsets. */
#define AD_O_MAGIC 0u
#define AD_O_ABI 4u
#define AD_O_SESSION 8u
#define AD_O_BUILD 12u
#define AD_O_SEQUENCE 16u
#define AD_O_STATE 20u
#define AD_O_REASON 24u
#define AD_O_POINT 28u
#define AD_O_HITS 32u
#define AD_O_ACK 36u
#define AD_O_RESULT 40u
#define AD_O_READ_SIZE 44u
#define AD_O_FAULT 48u
#define AD_O_PC 52u
#define AD_O_SP 56u
#define AD_O_LR 60u
#define AD_O_CPSR 64u
#define AD_O_FEATURES 68u
#define AD_O_BP_COUNT 72u
#define AD_O_VALUE_COUNT 76u
#define AD_O_LOG_COUNT 80u
#define AD_O_PAUSE_PENDING 84u
#define AD_O_VALUES 96u
#define AD_O_BREAKPOINTS 128u
#define AD_O_REGION_COUNT 160u
#define AD_O_READ_DATA 192u
/* Host-owned mailbox: session, sequence, opcode, arg0, arg1, arg2. */
#define AD_M_SESSION (AD_MAILBOX + 0u)
#define AD_M_SEQUENCE (AD_MAILBOX + 4u)
#define AD_M_OPCODE (AD_MAILBOX + 8u)
#define AD_M_ARG0 (AD_MAILBOX + 12u)
#define AD_M_ARG1 (AD_MAILBOX + 16u)
#define AD_M_ARG2 (AD_MAILBOX + 20u)
enum ad_state { AD_RUNNING=1, AD_PAUSED=2, AD_FAULTED=3, AD_FINISHED=4 };
enum ad_reason { AD_INITIAL=0, AD_PAUSE=1, AD_BREAKPOINT=2, AD_STEP=3, AD_FAULT=4 };
enum ad_command { AD_CMD_PAUSE=1, AD_CMD_CONTINUE=2, AD_CMD_STEP=3,
    AD_CMD_SET_BP=4, AD_CMD_CLEAR_BP=5, AD_CMD_READ=6, AD_CMD_DETACH=7 };
enum ad_result { AD_OK=0, AD_BAD_COMMAND=1, AD_BAD_STATE=2,
    AD_BAD_RANGE=3, AD_FULL=4, AD_BAD_SESSION=5, AD_BAD_SEQUENCE=6 };
/* Cache functions must synchronize exactly the supplied range. A noncached
 * mapping may use barrier-only callbacks. Never flush/invalidate shared L2
 * globally: XX19c's Core0 services may be active. Callback lifetime >= context.
 */
struct ad_io {
    void (*pull)(volatile void *, size_t, void *);
    void (*push)(volatile void *, size_t, void *);
    void (*barrier)(void *);
    void (*idle)(void *);
    void *user;
};
static inline uint32_t ad_wire(uint32_t x) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return x;
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap32(x);
#else
#error "Compiler must define byte order"
#endif
}
static inline uint32_t ad_get(const volatile uint8_t *p, unsigned o) {
    return ad_wire(*(const volatile uint32_t *)(p+o));
}
static inline void ad_put(volatile uint8_t *p, unsigned o, uint32_t x) {
    *(volatile uint32_t *)(p+o)=ad_wire(x);
}
struct ad_region { const uint8_t *data; uint32_t size; };
struct ad_core {
    volatile uint8_t *page;
    struct ad_io io;
    uint32_t session, sequence, ack, state, reason, point, hits, result;
    uint32_t pause_pending, step_pending, features, logs;
    uint32_t values[AD_MAX_VALUES], value_count;
    uint32_t bp[AD_MAX_BREAKPOINTS], bp_count;
    struct ad_region regions[AD_MAX_REGIONS];
    uint32_t region_count;
};
int ad_init(struct ad_core *, volatile void *page, uint32_t session,
            uint32_t build_id, uint32_t features, const struct ad_io *);
int ad_add_region(struct ad_core *, const void *, uint32_t size);
void ad_service(struct ad_core *);
/* Nonblocking entry is also useful in cooperative application schedulers.
 * Do not advance the application while AD_PAUSED. */
void ad_enter(struct ad_core *, uint32_t point, const uint32_t *, unsigned count);
void ad_checkpoint(struct ad_core *, uint32_t point, const uint32_t *, unsigned count);
void ad_log(struct ad_core *, const char *);
void ad_fault(struct ad_core *, uint32_t code, uint32_t pc, uint32_t sp,
              uint32_t lr, uint32_t cpsr);
void ad_finish(struct ad_core *);
#endif
