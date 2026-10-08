#include "wire.h"
#ifndef ZZ_BUILD_ID
#error Build via build_zz9000_debug.py --fractal
#endif
#ifdef FF_HOST_TEST
static void barrier(void *u) { (void)u; __sync_synchronize(); }
#else
static void barrier(void *u) { (void)u; __asm__ volatile("dsb sy" ::: "memory"); }
#endif
static void range(volatile void *p,size_t n,void *u) { (void)p;(void)n;barrier(u); }
static void idle(void *u) { volatile unsigned n;(void)u;for(n=0;n<128;n++)__asm__ volatile("nop"); }
void zz_worker(volatile uint8_t *base)
{
    uint32_t sctlr,mpidr,midr,session,seq=0,gen=0,stage=0,watch[6],row=0,result=0,i,idle_ack=0;
    uint16_t pixels[FF_PIXELS];
    struct ff_cursor cursor;struct ff_view view;
    struct ad_core core;struct ad_io io={range,range,barrier,idle,0};
#ifdef FF_HOST_TEST
    sctlr=midr=0;mpidr=1;
#else
    __asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(sctlr));
    __asm__ volatile("mrc p15,0,%0,c0,c0,0":"=r"(midr));
    __asm__ volatile("mrc p15,0,%0,c0,c0,5":"=r"(mpidr));
#endif
    if((sctlr&0x1005u)||(mpidr&255u)!=1)return;
    session=ad_get(base,ZZ_CONTROL);
    ad_put(base,ZZ_DIAG+4,sctlr);ad_put(base,ZZ_DIAG+8,midr);ad_put(base,ZZ_DIAG+12,mpidr);
    for(i=0;i<FF_PIXELS;i++)pixels[i]=0;
    if(ad_init(&core,base+ZZ_PAGE,session,ZZ_BUILD_ID,
#ifdef FF_HOST_TEST
       AD_FEATURE_HOST_DEMO,
#else
       0,
#endif
       &io)||
       ad_add_region(&core,pixels,sizeof(pixels))!=0)return;
    ad_log(&core,"Mandelbrot Core1 Q14; cache-off; points 1 dispatch / 2 row / 3 result / 4 idle");
    ad_put(base,ZZ_DIAG,ZZ_READY);barrier(0);
    while(!ad_get(base,ZZ_STOP)) {
        uint32_t incoming;
        barrier(0);ad_put(base,ZZ_DIAG+16,ad_get(base,ZZ_CHALLENGE)^ZZ_XOR);
        ad_service(&core);
        /* Cancellation and shutdown remain live even at a paused checkpoint. */
        if(stage && ad_get(base,FF_CANCEL)!=gen) {result=FF_CANCELLED;stage=5;}
        if(!stage) {
            incoming=ad_get(base,FF_REQ);barrier(0);
            if(incoming && incoming!=seq) {
                seq=incoming;gen=ad_get(base,FF_REQ+8);
                view.cx=(int32_t)ad_get(base,FF_REQ+12);view.cy=(int32_t)ad_get(base,FF_REQ+16);
                view.step=(int32_t)ad_get(base,FF_REQ+20);view.limit=ad_get(base,FF_REQ+24);
                watch[0]=gen;watch[1]=ad_get(base,FF_REQ+28);watch[2]=ad_get(base,FF_REQ+32);
                watch[3]=view.limit;watch[4]=0;watch[5]=seq;
                if(ad_get(base,FF_REQ+4)!=session||!gen||incoming!=ad_get(base,FF_REQ)||
                   !ff_valid(&view,watch[1],watch[2])) {result=FF_INVALID;stage=5;}
                else if(ad_get(base,FF_CANCEL)!=gen) {result=FF_CANCELLED;stage=5;}
                else {ff_begin(&cursor,&view,watch[1],watch[2]);row=0;stage=1;}
            } else {
                /* Publish idle on transition or a new debugger command.
                 * Continuous idle publication can starve a 68k snapshot
                 * reader even though there is no application work. */
                if(core.point!=4||core.pause_pending||core.step_pending||core.ack!=idle_ack) {
                    ad_enter(&core,4,0,0);idle_ack=core.ack;
                }
                idle(0);
            }
        }
        if(stage==1 && core.state==AD_RUNNING) {ad_enter(&core,1,watch,6);stage=2;}
        if(stage==2 && core.state==AD_RUNNING) {
            if(ff_step(&cursor,pixels,64))stage=3;
            else if(cursor.pixel/FF_TW!=row) {
                row=cursor.pixel/FF_TW;watch[4]=cursor.pixel;ad_enter(&core,2,watch,6);
            }
        }
        if(stage==3 && core.state==AD_RUNNING) {
            watch[4]=cursor.pixel;ad_enter(&core,3,watch,6);stage=4;
        }
        if(stage==4 && core.state==AD_RUNNING) {result=FF_DONE;stage=5;}
        if(stage==5) {
            if(result==FF_DONE)for(i=0;i<FF_PIXELS;i++) {
                base[FF_DATA+i*2]=(uint8_t)(pixels[i]>>8);base[FF_DATA+i*2+1]=(uint8_t)pixels[i];
            }
            ad_put(base,FF_RES+4,result);ad_put(base,FF_RES+8,gen);
            ad_put(base,FF_RES+12,watch[1]);ad_put(base,FF_RES+16,watch[2]);
            barrier(0);ad_put(base,FF_RES,seq);barrier(0);stage=0;
        }
    }
    ad_log(&core,"Fractal worker returning to firmware");ad_finish(&core);
}
