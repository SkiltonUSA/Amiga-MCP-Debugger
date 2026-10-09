#include <exec/memory.h>
#include <devices/timer.h>
#include <libraries/configvars.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/expansion.h>
#include <proto/intuition.h>
#include <proto/Picasso96.h>
#include <proto/timer.h>
#include <stdio.h>
#include <string.h>
#include "wire.h"
struct Device *TimerBase;
static struct Screen *screen;
static struct MsgPort *timerport;
static struct timerequest *timerio;
static struct RenderInfo ri;
static LONG lock;
static int locked,timeropen;
static uint8_t *cpu_frame;
static volatile uint8_t *shared;
static const struct ad_io *syncio;
static uint32_t sequence;
static ULONG us(void)
{ struct timeval tv;GetSysTime(&tv);return tv.tv_secs*1000000u+tv.tv_micro; }
static int request(uint32_t op,ULONG deadline)
{
    ULONG start=us();
    volatile uint8_t *cmd=shared+FB_HOST,*reply=shared+FB_REPLY;
    /* Publish payload before the commit word. Core1 can observe writebacks
     * while the 68060 is still flushing the rest of this cache line. */
    ad_put(cmd,FB_OP,op);syncio->push(cmd+4,60,NULL);
    syncio->barrier(NULL);ad_put(cmd,FB_SEQ,++sequence);
    syncio->push(cmd,4,NULL);
    do {
        syncio->pull(reply,64,NULL);
        if(ad_get(reply,FB_ACK)==sequence)return ad_get(reply,FB_ERROR)==0;
        if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)return 0;
        Delay(1); /* Wall timings include this 20 ms polling granularity. */
    } while((ULONG)(us()-start)<deadline);
    printf("TIMEOUT op=%u seq=%u\n",(ULONG)op,(ULONG)sequence);return 0;
}
static uint64_t clock_value(void)
{ return ((uint64_t)ad_get(shared+FB_REPLY,FB_CLOCK_HI)<<32)|ad_get(shared+FB_REPLY,FB_CLOCK_LO); }
/* Called by launcher only once Core1 is synchronously quiesced, including
 * failure paths. Never unlock/free a framebuffer that ARM might still use. */
void fb_cleanup(void)
{
    if(locked){p96UnlockBitMap(screen->RastPort.BitMap,lock);locked=0;}
    if(screen){p96CloseScreen(screen);screen=NULL;}
    if(cpu_frame){FreeVec(cpu_frame);cpu_frame=NULL;}
    if(timeropen){CloseDevice((struct IORequest *)timerio);timeropen=0;}
    if(timerio){DeleteIORequest((struct IORequest *)timerio);timerio=NULL;}
    if(timerport){DeleteMsgPort(timerport);timerport=NULL;}
}
int ff_window(volatile uint8_t *base,const struct ad_io *io,int connected)
{
    struct ConfigDev *card=FindConfigDev(NULL,0x6d6e,4),*ram=FindConfigDev(NULL,0x6d6e,5);
    ULONG board,err=0,t0,t1,width,height,n,pass,i,y,x;
    uint64_t clock0,clock1;
    ULONG rate;
    struct TagItem tags[]={
        {P96SA_Width,640},{P96SA_Height,480},{P96SA_Depth,32},
        {P96SA_RGBFormat,RGBFB_B8G8R8A8},{P96SA_Quiet,TRUE},
        {P96SA_ShowTitle,FALSE},{P96SA_NoSprite,TRUE},
        {P96SA_Title,(ULONG)"ZZFrameBench - direct ARM framebuffer"},
        {P96SA_ErrorCode,(ULONG)&err},{TAG_DONE,0}};
    (void)connected;shared=base;syncio=io;
    if(!card||!ram)return 20;
    board=(ULONG)card->cd_BoardAddr;
    timerport=CreateMsgPort();if(!timerport)return 20;
    timerio=(struct timerequest *)CreateIORequest(timerport,sizeof(*timerio));
    if(!timerio||OpenDevice(TIMERNAME,UNIT_MICROHZ,(struct IORequest *)timerio,0))return 20;
    TimerBase=timerio->tr_node.io_Device;timeropen=1;
    if(!request(FB_PREPARE,2000000))return 20;
    if(!request(FB_CLOCK,200000))return 20;
    clock0=clock_value();t0=us();Delay(100);
    if(!request(FB_CLOCK,200000))return 20;
    clock1=clock_value();t1=us();rate=(ULONG)((clock1-clock0)*1000000u/(t1-t0));
    printf("TIMER estimated_hz=%u calibration_us=%u\n",rate,t1-t0);
    if(rate<320000000u||rate>345000000u)return 20;
    cpu_frame=AllocVec(FB_MAX_WIDTH*FB_MAX_HEIGHT*4,MEMF_FAST|MEMF_PUBLIC);
    if(!cpu_frame)return 20;
    if((ULONG)cpu_frame>=(ULONG)ram->cd_BoardAddr &&
       (ULONG)cpu_frame<(ULONG)ram->cd_BoardAddr+ram->cd_BoardSize)return 20;
    for(i=0;i<FB_MAX_WIDTH*FB_MAX_HEIGHT;i++)ad_put(cpu_frame,i*4,fb_pattern(i));
    printf("CPU_SOURCE=%08x (outside ZZ9000 Fast RAM bank)\n",(ULONG)cpu_frame);
    screen=p96OpenScreenTagList(tags);
    if(!screen){printf("SCREEN failed code=%u\n",err);return 20;}
    ScreenToFront(screen);Delay(5);
    printf("SCREEN %ux%u bpp=%u format=%u\n",screen->Width,screen->Height,
        p96GetBitMapAttr(screen->RastPort.BitMap,P96BMA_BYTESPERPIXEL),
        p96GetBitMapAttr(screen->RastPort.BitMap,P96BMA_RGBFORMAT));fflush(stdout);
    for(pass=0;pass<2;pass++) {
        width=pass?640:320;height=pass?480:240;
        for(n=0;n<2;n++) {
            /* Three batches, 20 complete copies per batch, plus one verified
             * warm-up. No hashes, decoder or SDL inside timed copies. */
            for(i=0;i<61;i++) {
                ULONG start,elapsed,arm_ticks=0,nonce=(sequence+1)^ad_get(base,ZZ_CONTROL);
                ULONG offsets[4],addr,span;
                if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)return 20;
                memset(&ri,0,sizeof(ri));
                lock=p96LockBitMap(screen->RastPort.BitMap,(UBYTE *)&ri,sizeof(ri));locked=1;
                span=(height-1)*(ULONG)ri.BytesPerRow+width*4;
                if(!ri.Memory || (ULONG)ri.BytesPerRow<width*4 ||
                   !p96GetBitMapAttr(screen->RastPort.BitMap,P96BMA_ISONBOARD) ||
                   p96GetBitMapAttr(screen->RastPort.BitMap,P96BMA_BYTESPERPIXEL)!=4 ||
                   (ULONG)ri.Memory<board+0x10000 ||
                   (ULONG)ri.Memory>board+0x4000000-span)return 20;
                addr=(ULONG)ri.Memory-board+0x1f0000;
                if(!fb_valid(addr,(ULONG)ri.BytesPerRow,width,height))return 20;
                offsets[0]=0;offsets[1]=4;offsets[2]=(height-1)*ri.BytesPerRow;
                offsets[3]=offsets[2]+(width-1)*4;
                for(x=0;x<4;x++) {
                    ad_put(ri.Memory,offsets[x],nonce^offsets[x]);
                    io->push((uint8_t *)ri.Memory+offsets[x],4,NULL);
                }
                if(i==0)printf("BUFFER %ux%u amiga=%08x arm=%08x pitch=%u path=%s\n",
                    width,height,(ULONG)ri.Memory,addr,(ULONG)ri.BytesPerRow,n?"68k-Z3":"ARM-direct");
                start=us();
                if(!n) {
                    ad_put(base+FB_HOST,FB_ADDR,addr);ad_put(base+FB_HOST,FB_PITCH,ri.BytesPerRow);
                    ad_put(base+FB_HOST,FB_W,width);ad_put(base+FB_HOST,FB_H,height);
                    ad_put(base+FB_HOST,FB_NONCE,nonce);
                    if(!request(FB_COPY,400000)) {
                        printf("ARM COPY failed error=%u\n",(ULONG)ad_get(base+FB_REPLY,FB_ERROR));return 20;
                    }
                    arm_ticks=ad_get(base+FB_REPLY,FB_TICKS);
                } else {
                    for(y=0;y<height;y++)CopyMemQuick(cpu_frame+y*width*4,
                        (uint8_t *)ri.Memory+y*ri.BytesPerRow,width*4);
                    io->push(ri.Memory,span,NULL);
                }
                elapsed=us()-start;
                if(i==0 || i==60) {
                    ULONG bad=0;
                    io->pull(ri.Memory,span,NULL);
                    for(y=0;y<height;y++)for(x=0;x<width;x++)
                        if(ad_get(ri.Memory,y*ri.BytesPerRow+x*4)!=fb_pattern(y*width+x))bad++;
                    printf("VERIFY path=%s frame=%u mismatches=%u\n",n?"68k-Z3":"ARM-direct",i,bad);
                    if(bad)return 20;
                }
                p96UnlockBitMap(screen->RastPort.BitMap,lock);locked=0;
                if(i)printf("FRAME path=%s w=%u h=%u batch=%u n=%u arm_us=%u wall_us=%u\n",
                    n?"68k-Z3":"ARM-direct",width,height,(i-1)/20+1,i,
                    (ULONG)((uint64_t)arm_ticks*1000000u/rate),elapsed);
                fflush(stdout);
                Delay(1); /* Outside timing, releases the lock and yields. */
            }
        }
    }
    puts("FRAMEBUFFER_BENCH PASS; no decoding; no tear-free/vsync claim");fflush(stdout);
    Delay(100);return 0;
}
