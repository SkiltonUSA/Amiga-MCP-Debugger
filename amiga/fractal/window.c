/* AmigaOS frontend. Only this 68k task touches Intuition, DOS and graphics. */
#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wire.h"
#ifndef ZZ_RELEASE
#include "bridge_client.h"
#else
#define ab_poll() ((void)0)
#define ab_heartbeat() ((void)0)
#define ab_register_hook(name,description,fn) ((void)(fn))
#define ab_unregister_hook(name) ((void)0)
#endif
struct GfxBase *GfxBase;
static struct {
    volatile uint8_t *mem;const struct ad_io *io;
    struct Window *w;struct Screen *screen;struct TextFont *font;
    struct ff_view view;struct ff_cursor cpu;
    uint16_t *frame,tile[FF_PIXELS];LONG pens[32];
    uint32_t gen,seq,pending,pending_gen,tx,ty,done,hash,start,elapsed,cancel_start,cancel_ms;
    uint32_t renders,cancels,discarded,depth;
    int active,arm,cpu_busy,quit,error,connected,cancelling;
    char status[100];
} app;
static uint32_t millis(void) { ULONG s,u;CurrentTime(&s,&u);return s*1000u+u/1000u; }
static void pull(unsigned off,unsigned n) {app.io->pull(app.mem+off,n,app.io->user);app.io->barrier(app.io->user);}
static void push(unsigned off,unsigned n) {app.io->push(app.mem+off,n,app.io->user);app.io->barrier(app.io->user);}
static int left(void) {return app.w->BorderLeft;}
static int top(void) {return app.w->BorderTop;}
static void text_at(int x,int y,const char *s)
{SetAPen(app.w->RPort,1);SetBPen(app.w->RPort,0);SetDrMd(app.w->RPort,JAM2);Move(app.w->RPort,left()+x,top()+y);Text(app.w->RPort,(STRPTR)s,strlen(s));}
static unsigned colour(uint16_t n) {return n>=app.view.limit?0:1+(n*3u)%31u;}
static void paint(unsigned tx,unsigned ty,unsigned width,unsigned height)
{
    unsigned y,x,end,p;
    for(y=ty;y<ty+height;y++)for(x=tx;x<tx+width;x=end) {
        p=colour(app.frame[y*FF_WIDTH+x]);end=x+1;
        while(end<tx+width&&colour(app.frame[y*FF_WIDTH+end])==p)end++;
        SetAPen(app.w->RPort,app.pens[p]);
        RectFill(app.w->RPort,left()+40+x,top()+44+y,left()+39+end,top()+44+y);
    }
}
static void controls(void)
{
    unsigned i;const char *labels[]={"[A] ARM","[C] CPU","[X] Cancel","[R] Reset"};
    SetAPen(app.w->RPort,0);RectFill(app.w->RPort,left(),top(),left()+399,top()+35);
    for(i=0;i<4;i++)text_at(8+i*98,18,labels[i]);
    text_at(40,300,"Click image: zoom 2x | Q: quit");
}
static void status(void)
{
    char s[100];uint32_t ms=app.active?millis()-app.start:app.elapsed;
    snprintf(s,sizeof(s),"%s %3u%% %u.%03us zoom:%u %s",app.arm?"ARM":"CPU",
        (ULONG)(app.done*100/FF_TILES),
        (ULONG)(ms/1000),(ULONG)(ms%1000),(ULONG)app.depth,
        app.error?"ERROR":app.active?"rendering":app.done==FF_TILES?"done":"stopped");
    if(strcmp(app.status,s)) {
        strcpy(app.status,s);SetAPen(app.w->RPort,0);
        RectFill(app.w->RPort,left()+4,top()+306,left()+395,top()+322);text_at(8,318,s);
    }
}
static void cancel(void)
{
    app.gen++;if(!app.gen){app.error=1;app.quit=1;return;}
    ad_put(app.mem,FF_CANCEL,app.gen);push(FF_CANCEL,64);
    if(app.active){app.elapsed=millis()-app.start;app.cancels++;}
    app.active=0;app.cpu_busy=0;
    if(app.pending&&!app.cancelling){app.cancel_start=millis();app.cancelling=1;}
    else if(!app.pending)app.cancel_ms=0;
}
static void start(int arm)
{
    unsigned i;cancel();if(app.quit)return;
    app.arm=arm;app.done=app.hash=app.tx=app.ty=0;app.error=0;
    for(i=0;i<FF_WIDTH*FF_HEIGHT;i++)app.frame[i]=(uint16_t)app.view.limit;
    paint(0,0,FF_WIDTH,FF_HEIGHT);
    app.start=millis();app.active=1;app.renders++;app.status[0]=0;
}
static void reset(void) {ff_default(&app.view);app.depth=0;start(app.arm);}
static int zoom(unsigned x,unsigned y)
{if(!ff_zoom(&app.view,x,y))return 0;app.depth++;start(app.arm);return 1;}
static void completed_tile(void)
{
    unsigned x,y;uint32_t h;
    for(y=0;y<FF_TH;y++)for(x=0;x<FF_TW;x++)
        app.frame[(app.ty+y)*FF_WIDTH+app.tx+x]=app.tile[y*FF_TW+x];
    paint(app.tx,app.ty,FF_TW,FF_TH);app.done++;
    app.tx+=FF_TW;if(app.tx==FF_WIDTH){app.tx=0;app.ty+=FF_TH;}
    if(app.ty==FF_HEIGHT) {
        app.elapsed=millis()-app.start;app.active=0;h=2166136261u;
        for(x=0;x<FF_WIDTH*FF_HEIGHT;x++)h=ff_hash(h,app.frame[x]);
        app.hash=h;
        printf("FRACTAL mode=%s gen=%u tiles=%u hash=%08x elapsed_ms=%u cx=%d cy=%d step=%d\n",
            app.arm?"ARM":"CPU",(ULONG)app.gen,(ULONG)app.done,(ULONG)h,(ULONG)app.elapsed,
            (LONG)app.view.cx,(LONG)app.view.cy,(LONG)app.view.step);fflush(stdout);
    }
}
static void progress(void)
{
    unsigned i;
    if(app.pending) {
        pull(FF_RES,64);
        if(ad_get(app.mem,FF_RES)==app.pending) {
            uint32_t result=ad_get(app.mem,FF_RES+4),gen=ad_get(app.mem,FF_RES+8);
            if(app.cancelling){app.cancel_ms=millis()-app.cancel_start;app.cancelling=0;}
            if(app.active&&app.arm&&gen==app.gen&&gen==app.pending_gen) {
                if(result!=FF_DONE||ad_get(app.mem,FF_RES+12)!=app.tx||ad_get(app.mem,FF_RES+16)!=app.ty) {
                    app.error=1;app.active=0;
                } else {
                    pull(FF_DATA,FF_PIXELS*2);
                    for(i=0;i<FF_PIXELS;i++)app.tile[i]=((unsigned)app.mem[FF_DATA+i*2]<<8)|app.mem[FF_DATA+i*2+1];
                    completed_tile();
                }
            } else app.discarded++;
            app.pending=0;
        }
    }
    if(!app.active)return;
    if(app.arm) {
        if(app.pending)return;
        if(app.seq==0xffffffffu){app.error=1;app.active=0;return;}
        ad_put(app.mem,FF_REQ+4,ad_get(app.mem,ZZ_CONTROL));
        ad_put(app.mem,FF_REQ+8,app.gen);ad_put(app.mem,FF_REQ+12,(uint32_t)app.view.cx);
        ad_put(app.mem,FF_REQ+16,(uint32_t)app.view.cy);ad_put(app.mem,FF_REQ+20,(uint32_t)app.view.step);
        ad_put(app.mem,FF_REQ+24,app.view.limit);ad_put(app.mem,FF_REQ+28,app.tx);ad_put(app.mem,FF_REQ+32,app.ty);
        push(FF_REQ,64);app.pending=++app.seq;app.pending_gen=app.gen;
        ad_put(app.mem,FF_REQ,app.seq);push(FF_REQ,64);
    } else {
        if(!app.cpu_busy){ff_begin(&app.cpu,&app.view,app.tx,app.ty);app.cpu_busy=1;}
        if(ff_step(&app.cpu,app.tile,8192)){app.cpu_busy=0;completed_tile();}
    }
}
static int hook(const char *args,char *out,int cap)
{
    unsigned x,y;char extra;int ok=1;
    if(!strcmp(args,"arm"))start(1);
    else if(!strcmp(args,"cpu"))start(0);
    else if(!strcmp(args,"cancel"))cancel();
    else if(!strcmp(args,"reset"))reset();
    else if(!strcmp(args,"quit"))app.quit=1;
    else if(sscanf(args,"zoom %u %u %c",&x,&y,&extra)==2)ok=zoom(x,y);
    else if(!strcmp(args,"save")) {
        FILE *f;if(app.active||app.done!=FF_TILES)ok=0;
        else if((f=fopen("RAM:SixiesDev/fractal-counts.bin","wb"))) {
            ok=fwrite(app.frame,2,FF_WIDTH*FF_HEIGHT,f)==FF_WIDTH*FF_HEIGHT;
            if(fclose(f))ok=0;
        } else ok=0;
    } else if(strcmp(args,"status"))ok=0;
    snprintf(out,cap,"ok=%d mode=%s running=%d gen=%u tiles=%u hash=%08x ms=%u pending=%u cancel_ms=%u renders=%u cancels=%u discarded=%u depth=%u error=%d",
        ok,app.arm?"ARM":"CPU",app.active,(ULONG)app.gen,(ULONG)app.done,(ULONG)app.hash,
        (ULONG)(app.active?millis()-app.start:app.elapsed),(ULONG)app.pending,(ULONG)app.cancel_ms,
        (ULONG)app.renders,(ULONG)app.cancels,(ULONG)app.discarded,(ULONG)app.depth,app.error);
    return ok?0:-1;
}
static void events(void)
{
    struct IntuiMessage *msg;
    while((msg=(struct IntuiMessage *)GetMsg(app.w->UserPort))) {
        ULONG cl=msg->Class;UWORD code=msg->Code;int x=msg->MouseX-left(),y=msg->MouseY-top();
        ReplyMsg((struct Message *)msg);
        if(cl==IDCMP_CLOSEWINDOW)app.quit=1;
        else if(cl==IDCMP_REFRESHWINDOW) {
            BeginRefresh(app.w);controls();paint(0,0,FF_WIDTH,FF_HEIGHT);app.status[0]=0;status();EndRefresh(app.w,TRUE);
        } else if(cl==IDCMP_VANILLAKEY) {
            if(code=='a'||code=='A')start(1);
            else if(code=='c'||code=='C')start(0);
            else if(code=='x'||code=='X'||code==27)cancel();
            else if(code=='r'||code=='R')reset();
            else if(code=='q'||code=='Q')app.quit=1;
        } else if(cl==IDCMP_MOUSEBUTTONS&&code==SELECTDOWN) {
            if(y>=4&&y<30&&x>=8&&x<400) {
                switch((x-8)/98){case 0:start(1);break;case 1:start(0);break;case 2:cancel();break;case 3:reset();break;}
            } else if(x>=40&&x<360&&y>=44&&y<284)zoom(x-40,y-44);
        }
    }
}
int ff_window(volatile uint8_t *mem,const struct ad_io *io,int connected)
{
    unsigned i,tick=0;int rc=20;struct TextAttr ta={(STRPTR)"topaz.font",8,0,0};
    memset(&app,0,sizeof(app));for(i=0;i<32;i++)app.pens[i]=-1;
    app.mem=mem;app.io=io;app.connected=connected;app.arm=1;ff_default(&app.view);
    GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",39);if(!GfxBase)goto done;
    app.font=OpenFont(&ta);if(!app.font)goto done;
    app.screen=LockPubScreen(NULL);if(!app.screen)goto done;
    app.frame=AllocVec(FF_WIDTH*FF_HEIGHT*2,MEMF_PUBLIC|MEMF_CLEAR);if(!app.frame)goto done;
    for(i=0;i<32;i++) {
        ULONG r=i?(i<16?i*16:255):0,g=i?(i<16?i*5:(i-16)*16):0,b=i?(255-i*7):0;
        app.pens[i]=ObtainBestPen(app.screen->ViewPort.ColorMap,r*0x01010101u,g*0x01010101u,b*0x01010101u,TAG_DONE);
        if(app.pens[i]<0)goto done;
    }
    app.w=OpenWindowTags(NULL,WA_Title,(ULONG)"ZZ9000 Fractal - Mandelbrot",WA_PubScreen,(ULONG)app.screen,
        WA_Left,80,WA_Top,70,WA_InnerWidth,400,WA_InnerHeight,326,
        WA_Flags,WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_ACTIVATE|WFLG_SIMPLE_REFRESH,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_VANILLAKEY|IDCMP_MOUSEBUTTONS|IDCMP_REFRESHWINDOW,TAG_DONE);
    if(!app.w)goto done;
    SetFont(app.w->RPort,app.font);controls();
    if(connected)ab_register_hook("fractal","status arm cpu cancel reset zoom x y save quit",hook);
    start(1);
    while(!app.quit) {
        if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)app.quit=1;
        events();if(connected)ab_poll();
        if(app.quit)break;
        progress();status();
        if(connected&&tick++%100==0)ab_heartbeat();
        Delay(1);
    }
    cancel();rc=app.error?20:0;
    if(connected)ab_unregister_hook("fractal");
    printf("FRACTAL UI exit=%d renders=%u cancels=%u discarded=%u\n",rc,(ULONG)app.renders,(ULONG)app.cancels,(ULONG)app.discarded);fflush(stdout);
done:
    if(app.w)CloseWindow(app.w);
    for(i=0;i<32;i++)if(app.pens[i]>=0)ReleasePen(app.screen->ViewPort.ColorMap,app.pens[i]);
    if(app.frame)FreeVec(app.frame);
    if(app.screen)UnlockPubScreen(NULL,app.screen);
    if(app.font)CloseFont(app.font);
    if(GfxBase)CloseLibrary((struct Library *)GfxBase);
    return rc;
}
