#include "wire.h"
#include "decoder.h"
static void barrier(void *u) {(void)u;__asm__ volatile("dsb sy" ::: "memory");}
static void range(volatile void *p,size_t n,void *u) {(void)p;(void)n;barrier(u);}
static void idle(void *u) {volatile unsigned n;(void)u;for(n=0;n<128;n++)__asm__ volatile("nop");}
static uint64_t ticks(void)
{
    volatile uint32_t *t=(volatile uint32_t *)0xf8f00200u;uint32_t h,l;
    do{h=t[1];l=t[0];}while(h!=t[1]);return ((uint64_t)h<<32)|l;
}
void zz_worker(volatile uint8_t *base)
{
    uint32_t sctlr,midr,mpidr,session,seq=0,frame=0,watch[4],idle_ack=0,dispatch=0;
    struct ad_core core;struct ad_io io={range,range,barrier,idle,0};
    struct zv_info info={0};int opened=0;
    __asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(sctlr));
    __asm__ volatile("mrc p15,0,%0,c0,c0,0":"=r"(midr));
    __asm__ volatile("mrc p15,0,%0,c0,c0,5":"=r"(mpidr));
#ifdef ZZ_VIDEO_ICACHE
    if((sctlr&0x1005u)!=0x1000u||(mpidr&255u)!=1)return;
#else
    if((sctlr&0x1005u)||(mpidr&255u)!=1)return;
#endif
    session=ad_get(base,ZZ_CONTROL);
    ad_put(base,ZZ_DIAG+4,sctlr);ad_put(base,ZZ_DIAG+8,midr);ad_put(base,ZZ_DIAG+12,mpidr);
    if(ad_init(&core,base+ZZ_PAGE,session,ZZ_BUILD_ID,0,&io))return;
    ad_add_region(&core,(const void *)(base+ZV_RES),64);
    ad_add_region(&core,(const void *)(base+ZV_PIXELS),ZV_MAX_WIDTH*ZV_MAX_HEIGHT*4);
    ad_log(&core,"ZZVideo MPEG-1; Core1 integer decoder; 1 request / 2 frame / 3 idle");
    ad_put(base,ZZ_DIAG,ZZ_READY);barrier(0);
    while(!ad_get(base,ZZ_STOP)) {
        uint32_t next,cmd,code=ZV_OK,bytes=0,hash,i;int error=0;uint64_t before,elapsed,hash_before,hash_ticks;
        struct zv_timing timing={0,0};
        ad_service(&core);barrier(0);next=ad_get(base,ZV_REQ);
        if(!next||next==seq) {
            if(core.point!=3||core.pause_pending||core.step_pending||idle_ack!=core.ack) {
                ad_enter(&core,3,0,0);idle_ack=core.ack;
            }
            idle(0);continue;
        }
        cmd=ad_get(base,ZV_REQ+8);watch[0]=next;watch[1]=cmd;watch[2]=frame;watch[3]=0;
        if(dispatch!=next) {
            if(core.state!=AD_RUNNING)continue;
            ad_enter(&core,1,watch,4);dispatch=next;
        }
        if(core.state!=AD_RUNNING)continue;
        seq=next;before=ticks();
        if(ad_get(base,ZV_REQ+4)!=session||ad_get(base,ZV_REQ+60)!=zv_hash(zv_seed(session,seq),(const void *)(base+ZV_REQ+4),56))error=-1;
        else if(cmd==ZV_OPEN) {
            size_t size=ad_get(base,ZV_REQ+12);opened=0;frame=0;
            if(size>ZV_MAX_INPUT||!size||zv_hash(zv_seed(session,seq),(const void *)(base+ZV_INPUT),size)!=ad_get(base,ZV_REQ+16))error=-1;
            else error=zv_open((uint8_t *)base+ZV_INPUT,size,(void *)(base+ZV_HEAP),ZV_HEAP_SIZE,&info);
            opened=!error;
        } else if(cmd==ZV_NEXT&&opened) {
            int rc=zv_next_timed((uint8_t *)base+ZV_PIXELS,ticks,&timing);
            if(rc<0)error=-5;
            else if(!rc)code=ZV_EOF;
            else {code=ZV_FRAME;frame++;bytes=info.width*info.height*4;}
        } else if(cmd==ZV_REWIND&&opened) {zv_rewind();frame=0;}
        else if(cmd!=ZV_CLOCK)error=-1;
        elapsed=cmd==ZV_CLOCK?ticks():ticks()-before;
        if(error)code=ZV_ERROR;
        hash_before=ticks();hash=zv_hash(zv_seed(session,seq),(const void *)(base+ZV_PIXELS),bytes);
        hash_ticks=ticks()-hash_before;
        for(i=0;i<64;i+=4)ad_put(base,ZV_PROFILE+i,0);
        ad_put(base,ZV_PROFILE,seq);ad_put(base,ZV_PROFILE+4,session);
        ad_put(base,ZV_PROFILE+8,(uint32_t)timing.decode);ad_put(base,ZV_PROFILE+12,(uint32_t)(timing.decode>>32));
        ad_put(base,ZV_PROFILE+16,(uint32_t)timing.colour);ad_put(base,ZV_PROFILE+20,(uint32_t)(timing.colour>>32));
        ad_put(base,ZV_PROFILE+24,(uint32_t)hash_ticks);ad_put(base,ZV_PROFILE+28,(uint32_t)(hash_ticks>>32));
        ad_put(base,ZV_RES+4,ZV_MAGIC);ad_put(base,ZV_RES+8,session);ad_put(base,ZV_RES+12,code);
        ad_put(base,ZV_RES+16,frame);ad_put(base,ZV_RES+20,info.width);ad_put(base,ZV_RES+24,info.height);
        ad_put(base,ZV_RES+28,info.rate_num);ad_put(base,ZV_RES+32,info.rate_den);ad_put(base,ZV_RES+36,bytes);
        ad_put(base,ZV_RES+40,(uint32_t)elapsed);ad_put(base,ZV_RES+44,(uint32_t)(elapsed>>32));
        ad_put(base,ZV_RES+48,(uint32_t)error);ad_put(base,ZV_RES+52,hash);
        ad_put(base,ZV_RES+56,zv_hash(zv_seed(session,seq),(const void *)(base+ZV_PROFILE),64));
        ad_put(base,ZV_RES+60,zv_hash(zv_seed(session,seq),(const void *)(base+ZV_RES+4),56));
        barrier(0);ad_put(base,ZV_RES,seq);barrier(0);
        watch[2]=frame;watch[3]=code;ad_enter(&core,2,watch,4);
    }
    ad_log(&core,"Video worker returning to firmware");ad_finish(&core);
}
