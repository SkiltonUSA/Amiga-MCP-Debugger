/* SDL frontend for the owned-memory XX19c compute client. All OS/SDL work is 68k. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <workbench/workbench.h>
#include <workbench/startup.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/icon.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/wb.h>
#include "SDL.h"
#include "xx19c.h"
#ifndef ZZ_RELEASE
#include "bridge_client.h"
#else
#define ab_poll() ((void)0)
#define ab_heartbeat() ((void)0)
#define ab_register_hook(n,d,f) ((void)(f))
#define ab_unregister_hook(n) ((void)0)
#endif
struct Library *IconBase,*WorkbenchBase;
extern struct Library *CyberGfxBase;
extern struct GfxBase *GfxBase;
static struct {
    struct zc_client compute;struct zc_result result;
    struct ff_view view;struct ff_cursor cpu;
    SDL_Window *window,*cover;SDL_Surface *surface;
    struct Screen *test_screen;
    unsigned screen_depth;
    struct MsgPort *port;struct AppIcon *icon;struct DiskObject *disk;
    uint16_t *frame,tile[FF_PIXELS];Uint32 palette[257];
    uint32_t tx,ty,done,hash,start,elapsed,cpu_us,transfer_us,colour_us,draw_us,roundtrip_us;
    uint32_t rate,clock_control,status_time,calibration_span,calibration_uncertainty;
    uint64_t arm_ticks,clock_origin,clock_frequency;
    unsigned renders,cancels,discarded;
    int active,arm,cpu_busy,quit,error,connected,verifying;
} app;
static uint32_t now_us(void *u)
{(void)u;return (uint32_t)((SDL_GetPerformanceCounter()-app.clock_origin)*1000000u/app.clock_frequency);}
static uint32_t arm_us(void)
{return app.rate?(uint32_t)(app.arm_ticks*1000000u/app.rate):0;}
/* Original compact 5x7 glyphs. Rows are five-bit masks, left pixel at bit 4. */
static const char glyph_chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-./[]+% ";
static const unsigned char glyphs[][7]={
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},{0,4,4,0,4,4,0},{0,0,0,31,0,0,0},
 {0,0,0,0,0,6,6},{1,2,2,4,8,8,16},{14,8,8,8,8,8,14},
 {14,2,2,2,2,2,14},{0,4,4,31,4,4,0},{17,2,4,4,8,16,17},{0,0,0,0,0,0,0}
};
static void text_at(int x,int y,const char *text)
{
    Uint32 white=SDL_MapRGB(app.surface->format,230,235,245);
    while(*text&&x<394) {
        const char *g=strchr(glyph_chars,*text++);unsigned row,col;
        if(g)for(row=0;row<7;row++)for(col=0;col<5;col++)if(glyphs[g-glyph_chars][row]&(16>>col)) {
            Uint32 *p=(Uint32 *)((Uint8 *)app.surface->pixels+(y+row)*app.surface->pitch);p[x+col]=white;
        }
        x+=6;
    }
}
static void update(SDL_Rect *rect)
{
    uint32_t before;
    if(!app.window)return;
    before=now_us(0);
    if(SDL_UpdateWindowSurfaceRects(app.window,rect,1)<0){app.error=1;app.quit=1;}
    if(app.active)app.draw_us+=now_us(0)-before;
}
static void palette(void)
{
    unsigned i;
    for(i=0;i<=256;i++) {
        unsigned c=i>=app.view.limit?0:1+(i*3)%31;
        app.palette[i]=c?SDL_MapRGB(app.surface->format,c*7,c*3,255-c*7):SDL_MapRGB(app.surface->format,0,0,0);
    }
}
static void paint(unsigned tx,unsigned ty,unsigned w,unsigned h)
{
    unsigned x,y;uint32_t before;SDL_Rect r={(int)tx+40,(int)ty+44,(int)w,(int)h};
    if(!app.window)return;
    before=now_us(0);
    for(y=ty;y<ty+h;y++) {
        Uint32 *p=(Uint32 *)((Uint8 *)app.surface->pixels+(y+44)*app.surface->pitch)+40;
        for(x=tx;x<tx+w;x++)p[x]=app.palette[app.frame[y*FF_WIDTH+x]];
    }
    if(app.active)app.colour_us+=now_us(0)-before;
    update(&r);
}
static void controls(void)
{
    SDL_Rect r={0,0,400,36};unsigned i;
    const char *labels[]={"[A] ARM","[C] CPU","[X] CANCEL","[R] RESET"};
    SDL_FillRect(app.surface,&r,SDL_MapRGB(app.surface->format,28,38,60));
    for(i=0;i<4;i++) {
        SDL_Rect button={8+(int)i*98,6,90,24};
        SDL_FillRect(app.surface,&button,SDL_MapRGB(app.surface->format,45,65,92));
        text_at(button.x+8,15,labels[i]);
    }
    update(&r);
}
static void status(int force)
{
    char s[90];SDL_Rect r={0,292,400,62};uint32_t now=now_us(0);
    if(!app.window||(!force&&(!app.active||now-app.status_time<200000u)))return;
    app.status_time=now;SDL_FillRect(app.surface,&r,SDL_MapRGB(app.surface->format,28,38,60));
    snprintf(s,sizeof(s),"%s %3lu%% %lu MS ITER %lu STEP %ld %s",app.arm?"ARM":"CPU",
        (unsigned long)(app.done*100/FF_TILES),(unsigned long)((app.active?now-app.start:app.elapsed)/1000),
        (unsigned long)app.view.limit,(long)app.view.step,app.error?"ERROR":app.active?"RENDER":"IDLE");
    text_at(8,296,s);
    snprintf(s,sizeof(s),"COMPUTE %lu TRANSFER %lu DRAW %lu MS",
        (unsigned long)((app.arm?arm_us():app.cpu_us)/1000),(unsigned long)(app.transfer_us/1000),(unsigned long)(app.draw_us/1000));
    text_at(8,308,s);text_at(8,320,"CLICK ZOOM / RIGHT CLICK OUT / ARROWS PAN");
    text_at(8,332,"I/O ITERATIONS / M ICONIFY / Q QUIT");
    text_at(8,344,app.verifying?"AUTOMATED CHECK - Q/ESC TO STOP":app.rate?"ARM COMPUTE TIME USES CALIBRATED TIMER":"ARM COMPUTE TIMER UNAVAILABLE");update(&r);
}
static int open_window(void)
{
    app.window=SDL_CreateWindow("SDL ZZFractal 0.2",100,100,400,354,0);
    if(!app.window){printf("SDL window: %s\n",SDL_GetError());fflush(stdout);return 0;}
    app.surface=SDL_GetWindowSurface(app.window);
    if(!app.surface||app.surface->format->BytesPerPixel!=4){SDL_DestroyWindow(app.window);app.window=0;return 0;}
    SDL_FillRect(app.surface,0,SDL_MapRGB(app.surface->format,12,18,28));palette();
    {SDL_Rect r={0,0,400,354};update(&r);}controls();paint(0,0,FF_WIDTH,FF_HEIGHT);status(1);return 1;
}
static void remove_icon(void)
{
    struct Message *m;
    if(app.icon){RemoveAppIcon(app.icon);app.icon=0;}
    if(app.port)while((m=GetMsg(app.port)))ReplyMsg(m);
}
static int restore(void)
{
    if(app.window)return 1;
    if(!open_window())return 0;
    remove_icon();if(app.test_screen)ScreenToFront(app.test_screen);return 1;
}
static int iconify(void)
{
    if(!app.window)return 1;
    if(!app.port||!app.disk)return 0;
    app.icon=AddAppIconA(1,0,(UBYTE *)"SDL ZZFractal",app.port,0,app.disk,0);
    if(!app.icon)return 0;
    if(app.cover){SDL_DestroyWindow(app.cover);app.cover=0;}
    SDL_DestroyWindow(app.window);app.window=0;app.surface=0;
    if(app.test_screen)ScreenToBack(app.test_screen);return 1;
}
/* Explicit test screens. Never change Workbench/default public-screen settings. */
static int set_screen(unsigned depth)
{
    ULONG mode,actual_depth,mode_w,mode_h,error=0;
    if(app.active||app.compute.pending||app.icon)return 0;
    if(depth!=0&&depth!=16&&depth!=32)return 0;
    if(app.cover){SDL_DestroyWindow(app.cover);app.cover=0;}
    if(app.window){SDL_DestroyWindow(app.window);app.window=0;app.surface=0;}
    SDL_SetHint("SDL_AMIGA_PUBLIC_SCREEN","");
    if(app.test_screen){CloseScreen(app.test_screen);app.test_screen=0;}
    app.screen_depth=0;
    if(depth) {
        mode=BestCModeIDTags(CYBRBIDTG_NominalWidth,640,CYBRBIDTG_NominalHeight,480,CYBRBIDTG_Depth,depth,TAG_DONE);
        if(mode==(ULONG)INVALID_ID || GetCyberIDAttr(CYBRIDATTR_BPPIX,mode)!=depth/8) {
            ULONG id=INVALID_ID;mode=INVALID_ID;
            while((id=NextDisplayInfo(id))!=(ULONG)INVALID_ID) {
                if(IsCyberModeID(id) && GetCyberIDAttr(CYBRIDATTR_BPPIX,id)==depth/8 &&
                   GetCyberIDAttr(CYBRIDATTR_WIDTH,id)==640 && GetCyberIDAttr(CYBRIDATTR_HEIGHT,id)==480 &&
                   (depth==32||GetCyberIDAttr(CYBRIDATTR_DEPTH,id)==16)) {mode=id;break;}
            }
        }
        actual_depth=mode==(ULONG)INVALID_ID?0:GetCyberIDAttr(CYBRIDATTR_DEPTH,mode);
        mode_w=mode==(ULONG)INVALID_ID?640:GetCyberIDAttr(CYBRIDATTR_WIDTH,mode);
        mode_h=mode==(ULONG)INVALID_ID?480:GetCyberIDAttr(CYBRIDATTR_HEIGHT,mode);
        printf("SCREEN request_storage=%u mode=%08lx mode_depth=%lu width=%lu height=%lu\n",depth,(unsigned long)mode,(unsigned long)actual_depth,(unsigned long)mode_w,(unsigned long)mode_h);fflush(stdout);
        /* Intuition colour depth is at most 24; RTG storage may be 32 bits. */
        if(mode!=(ULONG)INVALID_ID)app.test_screen=OpenScreenTags(NULL,SA_DisplayID,mode,
            SA_Width,mode_w,SA_Height,mode_h,SA_Depth,actual_depth>24?24:actual_depth,SA_Type,PUBLICSCREEN,
            SA_ErrorCode,(ULONG)&error,
            SA_PubName,(ULONG)"SDLZZFractal.Test",SA_Title,(ULONG)"SDL ZZFractal temporary RTG test",
            SA_ShowTitle,TRUE,TAG_DONE);
        if(!app.test_screen){printf("SCREEN open failed error=%lu\n",(unsigned long)error);fflush(stdout);open_window();return 0;}
        printf("SCREEN bitmap_depth=%lu bytes_per_pixel=%lu format=%lu\n",
            (unsigned long)GetCyberMapAttr(app.test_screen->RastPort.BitMap,CYBRMATTR_DEPTH),
            (unsigned long)GetCyberMapAttr(app.test_screen->RastPort.BitMap,CYBRMATTR_BPPIX),
            (unsigned long)GetCyberMapAttr(app.test_screen->RastPort.BitMap,CYBRMATTR_PIXFMT));fflush(stdout);
        if(GetCyberMapAttr(app.test_screen->RastPort.BitMap,CYBRMATTR_BPPIX)!=depth/8) {
            CloseScreen(app.test_screen);app.test_screen=0;open_window();return 0;
        }
        PubScreenStatus(app.test_screen,0);ScreenToFront(app.test_screen);
        SDL_SetHint("SDL_AMIGA_PUBLIC_SCREEN","SDLZZFractal.Test");app.screen_depth=depth;
    }
    if(open_window())return 1;
    SDL_SetHint("SDL_AMIGA_PUBLIC_SCREEN","");
    if(app.test_screen){CloseScreen(app.test_screen);app.test_screen=0;}
    app.screen_depth=0;open_window();return 0;
}
static void cancel(void)
{
    if(zc_cancel(&app.compute)<0){app.error=1;app.quit=1;}
    if(app.active){app.elapsed=now_us(0)-app.start;app.cancels++;}
    app.active=app.cpu_busy=0;
}
static void start(int arm)
{
    unsigned i;cancel();if(app.quit)return;
    app.arm=arm;app.done=app.hash=app.tx=app.ty=0;
    app.cpu_us=app.transfer_us=app.colour_us=app.draw_us=app.roundtrip_us=0;app.arm_ticks=0;
    app.start=now_us(0);app.active=1;app.renders++;
    app.error=0;for(i=0;i<FF_WIDTH*FF_HEIGHT;i++)app.frame[i]=app.view.limit;
    if(app.window){palette();paint(0,0,FF_WIDTH,FF_HEIGHT);}
    status(1);
}
static int view_change(const char *kind,int x,int y)
{
    struct ff_view next=app.view;
    if(!strcmp(kind,"zoom")){if(x<0||y<0||!ff_zoom(&next,(unsigned)x,(unsigned)y))return 0;}
    else if(!strcmp(kind,"out")){next.step*=2;if(next.step>153)next.step=153;}
    else if(!strcmp(kind,"pan")) {
        if(x< -1||x>1||y< -1||y>1)return 0;
        next.cx+=x*80*next.step;next.cy+=y*60*next.step;
    } else if(!strcmp(kind,"iterations")) {
        if(x!=32&&x!=64&&x!=128&&x!=256)return 0;
        next.limit=x;
    } else return 0;
    if(!ff_valid(&next,0,0))return 0;
    app.view=next;start(app.arm);return 1;
}
static void completed(const uint16_t *pixels)
{
    unsigned x,y;
    for(y=0;y<FF_TH;y++)for(x=0;x<FF_TW;x++)app.frame[(app.ty+y)*FF_WIDTH+app.tx+x]=pixels[y*FF_TW+x];
    paint(app.tx,app.ty,FF_TW,FF_TH);app.done++;app.tx+=FF_TW;
    if(app.tx==FF_WIDTH){app.tx=0;app.ty+=FF_TH;}
    if(app.done==FF_TILES) {
        app.elapsed=now_us(0)-app.start;app.active=0;app.hash=2166136261u;
        for(x=0;x<FF_WIDTH*FF_HEIGHT;x++)app.hash=ff_hash(app.hash,app.frame[x]);
        printf("SDL_FRACTAL mode=%s hash=%08lx elapsed_us=%lu cpu_us=%lu arm_us_est=%lu transfer_us=%lu colour_us=%lu draw_us=%lu roundtrip_us=%lu rate=%lu cx=%ld cy=%ld step=%ld limit=%lu\n",
            app.arm?"ARM":"CPU",(unsigned long)app.hash,(unsigned long)app.elapsed,(unsigned long)app.cpu_us,
            (unsigned long)arm_us(),(unsigned long)app.transfer_us,(unsigned long)app.colour_us,
            (unsigned long)app.draw_us,(unsigned long)app.roundtrip_us,(unsigned long)app.rate,
            (long)app.view.cx,(long)app.view.cy,(long)app.view.step,(unsigned long)app.view.limit);fflush(stdout);status(1);
    }
}
static void progress(void)
{
    int result=zc_poll(&app.compute,&app.result);
    if(result==ZC_ERROR){app.error=1;app.quit=1;return;}
    if(result==ZC_DISCARDED)app.discarded++;
    if(result==ZC_TILE&&app.active&&app.arm) {
        if(app.result.control!=app.clock_control)app.rate=0;
        app.arm_ticks+=app.result.compute_ticks;app.transfer_us+=app.result.transfer_us;
        app.roundtrip_us+=app.result.roundtrip_us;completed(app.result.pixels);
    }
    if(!app.active)return;
    if(app.arm) {
        if(!app.compute.pending&&zc_submit(&app.compute,&app.view,app.tx,app.ty,0)<0){app.error=1;app.quit=1;}
    } else {
        uint32_t before;int done;
        if(!app.cpu_busy){ff_begin(&app.cpu,&app.view,app.tx,app.ty);app.cpu_busy=1;}
        before=now_us(0);done=ff_step(&app.cpu,app.tile,8192);app.cpu_us+=now_us(0)-before;
        if(done){app.cpu_busy=0;completed(app.tile);}
    }
}
/* Diagnostic readback of our native layered window. The bridge's generic
 * screenshots see P96 dummy 8-bit screen bitmaps on some true-colour modes. */
static int check_pixels(char *out,int cap)
{
    struct Screen *screen;struct Window *w;unsigned x,y,samples=0,mismatches=0,max_delta=0;
    unsigned char *rgb;unsigned tolerance=app.screen_depth==16?8:0;
    if(!app.window||!app.screen_depth||app.active||app.cover)return -1;
    screen=LockPubScreen("SDLZZFractal.Test");if(!screen)return -1;
    for(w=screen->FirstWindow;w;w=w->NextWindow)if(w->Title&&!strcmp((char *)w->Title,"SDL ZZFractal 0.2"))break;
    if(!w){UnlockPubScreen(NULL,screen);return -1;}
    rgb=calloc(FF_WIDTH*FF_HEIGHT,4);
    if(!rgb){UnlockPubScreen(NULL,screen);return -1;}
    if(ReadPixelArray(rgb,0,0,FF_WIDTH*4,w->RPort,40,44,FF_WIDTH,FF_HEIGHT,RECTFMT_ARGB)!=FF_WIDTH*FF_HEIGHT) {
        free(rgb);UnlockPubScreen(NULL,screen);snprintf(out,cap,"ok=0 error=readback_failed");return -1;
    }
    for(y=3;y<FF_HEIGHT;y+=13)for(x=3;x<FF_WIDTH;x+=13) {
        Uint8 r,g,b;unsigned c,want[3],bad=0;
        SDL_GetRGB(app.palette[app.frame[y*FF_WIDTH+x]],app.surface->format,&r,&g,&b);
        want[0]=r;want[1]=g;want[2]=b;
        for(c=0;c<3;c++) {
            unsigned got=rgb[(y*FF_WIDTH+x)*4+1+c];unsigned delta=got>want[c]?got-want[c]:want[c]-got;
            if(delta>max_delta)max_delta=delta;
            if(delta>tolerance)bad=1;
        }
        samples++;mismatches+=bad;
    }
    free(rgb);UnlockPubScreen(NULL,screen);
    snprintf(out,cap,"ok=%d samples=%u mismatches=%u max_delta=%u tolerance=%u storage=%u",
        mismatches==0,samples,mismatches,max_delta,tolerance,app.screen_depth);
    return mismatches?-1:0;
}
static int hook(const char *args,char *out,int cap)
{
    int x,y,ok=1;char extra;
    if(!strcmp(args,"automation on")){app.verifying=1;status(1);}
    else if(!strcmp(args,"automation off")){app.verifying=0;status(1);}
    else if(!strcmp(args,"pixels"))return check_pixels(out,cap);
    else if(!strcmp(args,"arm"))start(1);
    else if(!strcmp(args,"cpu"))start(0);
    else if(!strcmp(args,"cancel"))cancel();
    else if(!strcmp(args,"reset")){ff_default(&app.view);start(app.arm);}
    else if(!strcmp(args,"out"))ok=view_change("out",0,0);
    else if(!strcmp(args,"quit"))app.quit=1;
    else if(sscanf(args,"screen %d %c",&x,&extra)==1)ok=set_screen((unsigned)x);
    else if(!strcmp(args,"iconify"))ok=iconify();
    else if(!strcmp(args,"restore"))ok=restore();
    else if(!strcmp(args,"cover")) {
        if(!app.cover)app.cover=SDL_CreateWindow("SDL cover test",180,180,140,100,0);
        ok=app.cover!=0;
        if(ok){SDL_Surface *s=SDL_GetWindowSurface(app.cover);ok=s!=0;if(s){SDL_FillRect(s,0,SDL_MapRGB(s->format,220,60,80));ok=SDL_UpdateWindowSurface(app.cover)==0;}}
    } else if(!strcmp(args,"uncover")){if(app.cover)SDL_DestroyWindow(app.cover);app.cover=0;}
    else if(sscanf(args,"zoom %d %d %c",&x,&y,&extra)==2)ok=view_change("zoom",x,y);
    else if(sscanf(args,"pan %d %d %c",&x,&y,&extra)==2)ok=view_change("pan",x,y);
    else if(sscanf(args,"iterations %d %c",&x,&extra)==1)ok=view_change("iterations",x,0);
    else if(!strcmp(args,"save")) {
        FILE *f;if(app.active||app.done!=FF_TILES)ok=0;
        else if((f=fopen("RAM:SixiesDev/sdl-fractal-counts.bin","wb"))) {
            ok=fwrite(app.frame,2,FF_WIDTH*FF_HEIGHT,f)==FF_WIDTH*FF_HEIGHT;if(fclose(f))ok=0;
        } else ok=0;
    } else if(!strcmp(args,"metrics")) {
        snprintf(out,cap,"ok=1 mode=%s gen=%lu running=%d wall_us=%lu cpu_us=%lu arm_us_est=%lu transfer_us=%lu colour_us=%lu draw_us=%lu roundtrip_us=%lu rate=%lu calibration_span_us=%lu uncertainty_us=%lu control=%08lx",
            app.arm?"ARM":"CPU",(unsigned long)app.compute.generation,app.active,
            (unsigned long)(app.active?now_us(0)-app.start:app.elapsed),(unsigned long)app.cpu_us,(unsigned long)arm_us(),(unsigned long)app.transfer_us,
            (unsigned long)app.colour_us,(unsigned long)app.draw_us,(unsigned long)app.roundtrip_us,(unsigned long)app.rate,
            (unsigned long)app.calibration_span,(unsigned long)app.calibration_uncertainty,(unsigned long)app.clock_control);return 0;
    } else if(strcmp(args,"status"))ok=0;
    snprintf(out,cap,"ok=%d mode=%s running=%d gen=%lu tiles=%lu hash=%08lx ms=%lu pending=%lu renders=%u cancels=%u discarded=%u cx=%ld cy=%ld step=%ld limit=%lu hidden=%d depth=%u error=%d",
        ok,app.arm?"ARM":"CPU",app.active,(unsigned long)app.compute.generation,(unsigned long)app.done,(unsigned long)app.hash,
        (unsigned long)((app.active?now_us(0)-app.start:app.elapsed)/1000),(unsigned long)app.compute.pending,
        app.renders,app.cancels,app.discarded,(long)app.view.cx,(long)app.view.cy,(long)app.view.step,
        (unsigned long)app.view.limit,app.window==0,app.screen_depth,app.error);return ok?0:-1;
}
static void events(void)
{
    SDL_Event e;struct Message *m;
    if(app.port)while((m=GetMsg(app.port))) {ReplyMsg(m);if(!restore()){app.error=1;app.quit=1;}}
    while(SDL_PollEvent(&e)) {
        if(e.type==SDL_QUIT)app.quit=1;
        else if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_CLOSE) {
            if(app.cover&&e.window.windowID==SDL_GetWindowID(app.cover)){SDL_DestroyWindow(app.cover);app.cover=0;}
            else app.quit=1;
        } else if(e.type==SDL_KEYDOWN) {
            printf("SDL_INPUT key=%ld\n",(long)e.key.keysym.sym);fflush(stdout);
            if(app.verifying) {
                if(e.key.keysym.sym==SDLK_q||e.key.keysym.sym==SDLK_ESCAPE)app.quit=1;
                continue;
            }
            switch(e.key.keysym.sym) {
            case SDLK_a:start(1);break;case SDLK_c:start(0);break;
            case SDLK_x:case SDLK_ESCAPE:cancel();break;
            case SDLK_r:ff_default(&app.view);start(app.arm);break;
            case SDLK_F5:if(set_screen(0))start(app.arm);break;
            case SDLK_F6:if(set_screen(16))start(app.arm);break;
            case SDLK_F7:if(set_screen(32))start(app.arm);break;
            case SDLK_q:app.quit=1;break;case SDLK_m:iconify();break;
            case SDLK_LEFT:view_change("pan",-1,0);break;case SDLK_RIGHT:view_change("pan",1,0);break;
            case SDLK_UP:view_change("pan",0,-1);break;case SDLK_DOWN:view_change("pan",0,1);break;
            case SDLK_i:view_change("iterations",app.view.limit<256?app.view.limit*2:256,0);break;
            case SDLK_o:view_change("iterations",app.view.limit>32?app.view.limit/2:32,0);break;
            default:break;
            }
        } else if(e.type==SDL_MOUSEBUTTONDOWN&&app.window&&e.button.windowID==SDL_GetWindowID(app.window)) {
            int x=e.button.x,y=e.button.y;
            printf("SDL_INPUT button=%u x=%d y=%d\n",e.button.button,x,y);fflush(stdout);
            if(app.verifying)continue;
            if(e.button.button==SDL_BUTTON_RIGHT)view_change("out",0,0);
            else if(e.button.button==SDL_BUTTON_LEFT) {
                if(y>=6&&y<30&&x>=8&&x<392&&(x-8)%98<90) {
                    switch((x-8)/98) {
                    case 0:start(1);break;case 1:start(0);break;case 2:cancel();break;
                    case 3:ff_default(&app.view);start(app.arm);break;
                    }
                }
                else if(x>=40&&x<360&&y>=44&&y<284)view_change("zoom",x-40,y-44);
            }
        }
    }
}
static int sample_clock(uint64_t *stamp,uint32_t *mid,uint32_t *span)
{
    uint32_t begin=now_us(0);int r;
    if(zc_submit(&app.compute,&app.view,0,0,1)<0)return 0;
    do {r=zc_poll(&app.compute,&app.result);if(r==ZC_WAIT)Delay(0);} while(r==ZC_WAIT);
    if(r!=ZC_CLOCK)return 0;
    *stamp=app.result.stamp;*span=now_us(0)-begin;*mid=begin+*span/2;
    return 1;
}
static void calibrate(void)
{
    uint64_t a,b;uint32_t ma,mb,sa,sb,control;
    if(!sample_clock(&a,&ma,&sa))return;
    control=app.result.control;Delay(50);
    if(!sample_clock(&b,&mb,&sb)||control!=app.result.control||!(control&1)||b<=a||mb-ma<500000u)return;
    app.calibration_span=mb-ma;app.calibration_uncertainty=(sa+sb)/2;
    app.rate=(uint32_t)((b-a)*1000000u/(mb-ma));app.clock_control=control;
    if(app.rate<1000000u||app.rate>1000000000u)app.rate=0;
    printf("TIMER ticks_per_second=%lu span_us=%lu uncertainty_us=%lu control=%08lx read_only=1\n",
        (unsigned long)app.rate,(unsigned long)app.calibration_span,(unsigned long)app.calibration_uncertainty,(unsigned long)control);fflush(stdout);
}
int ff_window(volatile uint8_t *mem,const struct ad_io *io,int connected)
{
    unsigned tick=0;int rc=20;
    memset(&app,0,sizeof(app));app.connected=connected;app.arm=1;ff_default(&app.view);
    if(zc_init(&app.compute,mem,ZZ_BLOCK_SIZE,io,now_us,0)<0)return 20;
    if(SDL_Init(SDL_INIT_VIDEO)<0)goto done;
    app.clock_frequency=SDL_GetPerformanceFrequency();app.clock_origin=SDL_GetPerformanceCounter();
    if(!app.clock_frequency)goto done;
    app.frame=calloc(FF_WIDTH*FF_HEIGHT,sizeof(*app.frame));if(!app.frame)goto done;
    if(!open_window())goto done;
    IconBase=OpenLibrary("icon.library",44);WorkbenchBase=OpenLibrary("workbench.library",44);
    if(IconBase&&WorkbenchBase) {
        app.port=CreateMsgPort();app.disk=GetDiskObject("PROGDIR:SDLZZFractal");
        if(!app.disk)app.disk=GetDefDiskObject(WBTOOL);
    }
    calibrate();if(app.compute.failed)goto done;
    if(connected)ab_register_hook("sdlfractal","status metrics pixels arm cpu cancel reset zoom x y pan x y iterations n out screen 0/16/32 iconify restore cover uncover save quit",hook);
    start(1);
    while(!app.quit) {
        if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)app.quit=1;
        events();if(connected)ab_poll();if(app.quit)break;
        progress();status(0);if(connected&&tick++%100==0)ab_heartbeat();Delay(1);
    }
    cancel();rc=app.error?20:0;
    if(connected)ab_unregister_hook("sdlfractal");
done:
    if(rc)printf("SDL frontend error: %s\n",SDL_GetError());
    if(app.cover)SDL_DestroyWindow(app.cover);
    if(app.window)SDL_DestroyWindow(app.window);
    if(app.test_screen){CloseScreen(app.test_screen);app.test_screen=0;}
    SDL_SetHint("SDL_AMIGA_PUBLIC_SCREEN","");
    remove_icon();if(app.disk)FreeDiskObject(app.disk);if(app.port)DeleteMsgPort(app.port);
    if(WorkbenchBase)CloseLibrary(WorkbenchBase);if(IconBase)CloseLibrary(IconBase);
    free(app.frame);SDL_Quit();
    printf("SDL_UI exit=%d renders=%u cancels=%u discarded=%u\n",rc,app.renders,app.cancels,app.discarded);fflush(stdout);return rc;
}
