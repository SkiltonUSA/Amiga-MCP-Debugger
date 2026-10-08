/* AmigaOS development probe; independent of Sixies gameplay. */
#include <exec/types.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include "bridge_client.h"

static LONG ticks = 0;
static LONG running = 1;

int main(void)
{
    if (ab_init("sixies_probe") != 0) {
        puts("Start amiga-bridge before running sixies_probe.");
        return 20;
    }
    ab_register_var("ticks", AB_TYPE_I32, &ticks);
    ab_register_var("running", AB_TYPE_I32, &running);
    AB_I("Sixies OS probe started");
    while (running && !(SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)) {
        ab_poll();
        ++ticks;
        ab_push_var("ticks");
        ab_heartbeat();
        Delay(50);
    }
    AB_I("Probe stopped after %ld ticks", (long)ticks);
    ab_cleanup();
    return 0;
}
