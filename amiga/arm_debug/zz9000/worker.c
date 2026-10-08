#include "protocol.h"
#include "layout.h"
#ifndef ZZ_BUILD_ID
#error "Build with scripts/build_zz9000_debug.py"
#endif
static void barrier(void *u) { (void)u; __asm__ volatile("dsb sy" ::: "memory"); }
static void range(volatile void *p, size_t n, void *u)
{ (void)p; (void)n; barrier(u); }
static void idle(void *u)
{ volatile unsigned n; (void)u; for(n=0;n<10000;n++) __asm__ volatile("nop"); }

void zz_worker(volatile uint8_t *base)
{
    uint32_t sctlr, midr, mpidr, session, round=0, watch[4], i;
    uint8_t memory[128];
    struct ad_core core;
    struct ad_io io = {range, range, barrier, idle, 0};
    __asm__ volatile("mrc p15,0,%0,c1,c0,0" : "=r"(sctlr));
    __asm__ volatile("mrc p15,0,%0,c0,c0,0" : "=r"(midr));
    __asm__ volatile("mrc p15,0,%0,c0,c0,5" : "=r"(mpidr));
    /* No firmware tables, SCTLR bits or PL310/L2 state are changed. */
    if ((sctlr & 0x1005u) || (mpidr & 0xffu) != 1u) return;
    ad_put(base,ZZ_DIAG+4,sctlr); ad_put(base,ZZ_DIAG+8,midr);
    ad_put(base,ZZ_DIAG+12,mpidr);
    session=ad_get(base,ZZ_CONTROL);
    for(i=0;i<sizeof(memory);i++) memory[i]=(uint8_t)i;
    if(ad_init(&core,base+ZZ_PAGE,session,ZZ_BUILD_ID,0,&io) ||
       ad_add_region(&core,memory,sizeof(memory)) != 0) return;
    ad_log(&core,"Physical Cortex-A9 Core1: MMU/cache-off Exec-owned shared RAM");
    ad_put(base,ZZ_DIAG,ZZ_READY);barrier(0);
    while(!ad_get(base,ZZ_STOP)) {
        uint32_t challenge;
        barrier(0);challenge=ad_get(base,ZZ_CHALLENGE);
        ad_put(base,ZZ_DIAG+16,challenge^ZZ_XOR);barrier(0);
        ad_service(&core);
        if(core.state==AD_RUNNING) {
            watch[0]=round;watch[1]=round*7u;watch[2]=mpidr;watch[3]=sctlr;
            ad_enter(&core,(round&1u)+1u,watch,4);
            round++;
        }
        idle(0);
    }
    ad_log(&core,"ARM shutdown requested; returning to firmware");
    ad_finish(&core);
    ad_put(base,ZZ_DIAG+20,round);barrier(0);
}
