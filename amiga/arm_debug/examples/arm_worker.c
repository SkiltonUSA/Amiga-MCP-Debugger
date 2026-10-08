/* Link into an XACP Core1 application's existing startup/return framework.
 * Its launcher reserves shared_page, initializes a fresh session nonce, sets
 * the correct noncached/shared mapping, and binds the 68k bridge relay.
 * This function is not a firmware entry point or a flashable image. */
#include "protocol.h"
#ifndef ARM_WORKER_BUILD_ID
#error "Build with scripts/build_arm_debug.py to supply the source-derived build ID"
#endif
void arm_debug_example(volatile void *shared_page,uint32_t session,const struct ad_io *io) {
    struct ad_core debug;
    uint32_t data[16],watch[2],round,i;
    for(i=0;i<16;i++)data[i]=i;
    if(ad_init(&debug,shared_page,session,ARM_WORKER_BUILD_ID,0,io))return;
    if(ad_add_region(&debug,data,sizeof(data))<0)return;
    ad_log(&debug,"ARM worker ready");
    for(round=0;round<1000;round++){
        watch[0]=round;watch[1]=data[0];
        ad_checkpoint(&debug,1,watch,2); /* point: before_transform */
        for(i=0;i<16;i++)data[i]=data[i]*1664525u+1013904223u;
        watch[1]=data[0];
        ad_checkpoint(&debug,2,watch,2); /* point: after_transform */
    }
    ad_log(&debug,"ARM worker complete");ad_finish(&debug);
    /* Launcher unbinds after collecting results. Its existing Core1 teardown
     * restores the firmware context; this SDK must not overwrite those rules. */
}
