/* Live AmigaOS IPC acceptance probe. The debug core runs on the 68k here,
 * NOT the ZZ9000 ARM. No board registers or shared DDR are accessed.
 * Run with a fresh nonzero hexadecimal nonce, e.g. amiga-relay-probe 1234abcd.
 * Ctrl-C or five minutes ends the probe, including while paused. */
#include <exec/types.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "protocol.h"
#include "relay.h"
#include "bridge_client.h"
#ifndef AD_RELAY_PROBE_BUILD_ID
#error "Build using scripts/build_arm_debug.py"
#endif

static uint8_t page_storage[AD_PAGE_SIZE + 63];
static struct ad_core core;
static uint8_t memory[128];

/* Core and relay are serialized in ONE 68k task and its ordinary RAM.
 * These callbacks are not suitable for a cross-processor DDR mapping. */
static void range(volatile void *p, size_t n, void *u)
{ (void)p; (void)n; (void)u; __asm__ volatile("" ::: "memory"); }
static void barrier(void *u)
{ (void)u; __asm__ volatile("" ::: "memory"); }
static const struct ad_io io = {range, range, barrier, barrier, 0};

int main(int argc, char **argv)
{
    char *end;
    unsigned long nonce;
    uint32_t tick, point = 1, values[2];
    unsigned i;
    volatile void *page = (void *)(((uintptr_t)page_storage + 63u) & ~(uintptr_t)63u);
    if (argc != 2) {
        puts("Usage: amiga-relay-probe <fresh-nonzero-hex-session>");
        return 20;
    }
    errno = 0;
    nonce = strtoul(argv[1], &end, 16);
    if (errno || !nonce || end == argv[1] || *end || argv[1][0] == '-') return 20;
    for (i = 0; i < sizeof(memory); i++) memory[i] = (uint8_t)i;
    if (ad_init(&core, page, nonce, AD_RELAY_PROBE_BUILD_ID,
                AD_FEATURE_HOST_DEMO, &io) ||
        ad_add_region(&core, memory, sizeof(memory)) != 0) return 20;
    if (ab_init("amiga-relay-probe")) {
        puts("Start amiga-bridge first.");
        return 20;
    }
    if (ad_bridge_bind(page, &io)) {
        ab_cleanup();
        return 20;
    }
    ad_log(&core, "Live AmigaOS relay probe: 68k execution, NOT ARM");
    puts("68k relay probe ready; Ctrl-C or five minutes stops it.");
    for (tick = 0; tick < 50u * 300u; tick++) {
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) break;
        ab_poll();
        ad_service(&core);
        if (core.state == AD_RUNNING && tick % 5u == 0) {
            values[0] = core.hits;
            values[1] = core.hits * 7u;
            ad_enter(&core, point, values, 2);
            point = point == 1 ? 2 : 1;
        }
        if (tick % 50u == 0) ab_heartbeat();
        Delay(1);
    }
    ad_finish(&core);
    ad_bridge_unbind();
    ab_cleanup();
    puts("68k relay probe stopped cleanly.");
    return 0;
}
