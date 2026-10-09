#include "wire.h"
static void barrier(void) { __asm__ volatile("dsb sy" ::: "memory"); }
static uint64_t ticks(void)
{
    volatile uint32_t *t=(volatile uint32_t *)0xf8f00200u;
    uint32_t hi,lo;
    do {hi=t[1];lo=t[0];} while(hi!=t[1]);
    return ((uint64_t)hi<<32)|lo;
}
void zz_worker(volatile uint8_t *base)
{
    uint32_t sctlr,midr,mpidr,seq=0;
    uint32_t *source=(uint32_t *)(base+FB_SOURCE);
    __asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(sctlr));
    __asm__ volatile("mrc p15,0,%0,c0,c0,0":"=r"(midr));
    __asm__ volatile("mrc p15,0,%0,c0,c0,5":"=r"(mpidr));
    if((sctlr&0x1005u)||(mpidr&255u)!=1u)return;
    if(!(*(volatile uint32_t *)0xf8f00208u&1u))return;
    ad_put(base,ZZ_DIAG+4,sctlr);ad_put(base,ZZ_DIAG+8,midr);ad_put(base,ZZ_DIAG+12,mpidr);
    ad_put(base,ZZ_DIAG,ZZ_READY);barrier();
    while(!ad_get(base,ZZ_STOP)) {
        volatile uint8_t *cmd=base+FB_HOST,*reply=base+FB_REPLY;
        uint32_t next=ad_get(cmd,FB_SEQ),op,error=0,elapsed=0;
        uint64_t now;
        if(next==seq)continue;
        barrier();op=ad_get(cmd,FB_OP);
        if(op==FB_PREPARE) {
            uint32_t i;
            for(i=0;i<FB_MAX_WIDTH*FB_MAX_HEIGHT;i++)source[i]=ad_wire(fb_pattern(i));
        } else if(op==FB_COPY) {
            uint32_t addr=ad_get(cmd,FB_ADDR),pitch=ad_get(cmd,FB_PITCH);
            uint32_t w=ad_get(cmd,FB_W),h=ad_get(cmd,FB_H),nonce=ad_get(cmd,FB_NONCE),x,y;
            uint32_t *src=source;
            if(!fb_valid(addr,pitch,w,h))error=1;
            else {
                /* Prove this particular locked bitmap's mapping BEFORE writing. */
                uint32_t off[4]={0,4,(h-1)*pitch,(h-1)*pitch+(w-1)*4};
                for(x=0;x<4;x++)
                    if(ad_get((volatile uint8_t *)addr,off[x])!=(nonce^off[x]))error=2;
                if(!error) {
                    uint64_t start=ticks();
                    for(y=0;y<h;y++) {
                        volatile uint32_t *dst=(volatile uint32_t *)(addr+y*pitch);
                        if(ad_get(base,ZZ_STOP)){error=3;break;}
                        for(x=0;x<w;x+=8) {
                            dst[x]=src[x];dst[x+1]=src[x+1];dst[x+2]=src[x+2];dst[x+3]=src[x+3];
                            dst[x+4]=src[x+4];dst[x+5]=src[x+5];dst[x+6]=src[x+6];dst[x+7]=src[x+7];
                        }
                        src+=w;
                    }
                    barrier();elapsed=(uint32_t)(ticks()-start);
                }
            }
        } else if(op!=FB_CLOCK)error=4;
        barrier();now=ticks();
        ad_put(reply,FB_ERROR,error);ad_put(reply,FB_TICKS,elapsed);
        ad_put(reply,FB_CLOCK_LO,(uint32_t)now);ad_put(reply,FB_CLOCK_HI,(uint32_t)(now>>32));
        barrier();ad_put(reply,FB_ACK,next);barrier();seq=next;
    }
}
