#ifndef SIXIES_ARM_DEBUG_RELAY_H
#define SIXIES_ARM_DEBUG_RELAY_H
#include "protocol.h"
/* Lives in the application's 68k launcher, never in ARM-private memory.
 * The launcher owns/reserves the channel and keeps it alive until unbind. */
struct ad_relay {
    volatile uint8_t *page;
    struct ad_io io;
    uint32_t token;
    uint8_t snapshot[AD_PAGE_SIZE];
};
int ad_relay_init(struct ad_relay *,volatile void *,const struct ad_io *);
int ad_relay_call(struct ad_relay *,const char *,char *,int);
/* These adapter functions require the existing AmigaBridge client library.
 * Only one channel per launcher. Call bind after ab_init and ad_init; continue
 * ab_poll() while the ARM is paused. Unbind BEFORE freeing the shared channel. */
int ad_bridge_bind(volatile void *,const struct ad_io *);
void ad_bridge_unbind(void);
#endif
