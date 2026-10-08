#include "wire.h"
#ifdef FF_TIMING
/* Read-only Zynq global timer; never start/reset a clock shared with firmware.
 * Register map and rollover scheme: Xilinx standalone cortexa9 xtime_l.c.
 * Host fixture returns nanosecond ticks and does not model hardware timing. */
#ifdef FF_HOST_TEST
#include <time.h>
static uint64_t timer_ticks(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec; }
static uint32_t timer_control(void) {return 1;}
#else
static uint64_t timer_ticks(void)
{
    volatile uint32_t *t=(volatile uint32_t *)0xf8f00200u;
    uint32_t high,low;
    do {high=t[1];low=t[0];} while(high!=t[1]);
    return ((uint64_t)high<<32)|low;
}
static uint32_t timer_control(void) {return *(volatile uint32_t *)0xf8f00208u;}
#endif
#endif
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
#ifdef FF_TIMING
    uint64_t busy_ticks=0;
#endif
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
    ad_add_region(&core,(const void *)(base+FF_DATA),FF_PIXELS*2);
    ad_add_region(&core,(const void *)(base+FF_RES),64);
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
#ifdef FF_TIMING
                busy_ticks=0;
#endif
                view.cx=(int32_t)ad_get(base,FF_REQ+12);view.cy=(int32_t)ad_get(base,FF_REQ+16);
                view.step=(int32_t)ad_get(base,FF_REQ+20);view.limit=ad_get(base,FF_REQ+24);
                watch[0]=gen;watch[1]=ad_get(base,FF_REQ+28);watch[2]=ad_get(base,FF_REQ+32);
                watch[3]=view.limit;watch[4]=0;watch[5]=seq;
                if(ad_get(base,FF_REQ+4)!=session||!gen||incoming!=ad_get(base,FF_REQ)||
                   !ff_valid(&view,watch[1],watch[2])) {result=FF_INVALID;stage=5;}
                else if(ad_get(base,FF_CANCEL)!=gen) {result=FF_CANCELLED;stage=5;}
                #ifdef FF_TIMING
                else if(ad_get(base,FF_REQ+36)==1) {result=4;stage=5;}
                else if(ad_get(base,FF_REQ+36)!=0) {result=FF_INVALID;stage=5;}
#endif
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
            int complete;
#ifdef FF_TIMING
            uint64_t before=timer_ticks();
#endif
            /* Bounded service/cancel latency, with no skipped row checkpoints. */
            complete=ff_step_row(&cursor,pixels,512);
#ifdef FF_TIMING
            busy_ticks+=timer_ticks()-before;
#endif
            if(complete)stage=3;
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
            #ifdef FF_TIMING
            {
                uint64_t stamp=timer_ticks();uint32_t control=timer_control();
                uint32_t hash=ff_timing_hash(ff_result_seed(session,seq,gen,watch[1],watch[2]),busy_ticks,stamp,control);
                if(result==FF_DONE)for(i=0;i<FF_PIXELS;i++)hash=ff_hash(hash,pixels[i]);
                ad_put(base,FF_RES+20,(uint32_t)busy_ticks);
                ad_put(base,FF_RES+24,(uint32_t)(busy_ticks>>32));
                ad_put(base,FF_RES+28,(uint32_t)stamp);
                ad_put(base,FF_RES+32,(uint32_t)(stamp>>32));
                ad_put(base,FF_RES+36,control);
                ad_put(base,FF_RES+44,hash);
                ad_put(base,FF_RES+40,0x54494d33u); /* TIM3: request-bound result integrity */
            }
#endif
            barrier(0);ad_put(base,FF_RES,seq);barrier(0);stage=0;
        }
    }
    ad_log(&core,"Fractal worker returning to firmware");ad_finish(&core);
}
