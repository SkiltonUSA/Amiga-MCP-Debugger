/* Video-only preview. 68k owns file I/O, window, timing; ARM decodes/colors. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <devices/timer.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include "SDL.h"
#include "client.h"
#ifndef ZZ_RELEASE
#include "bridge_client.h"
#else
#define ab_poll() ((void)0)
#define ab_heartbeat() ((void)0)
#define ab_register_hook(n,d,f) ((void)(f))
#define ab_unregister_hook(n) ((void)0)
#endif
struct Library *AslBase;
static struct {
    struct zv_client client;struct zv_result result;
    SDL_Window *window;SDL_Surface *surface,*frame_surface;
    struct MsgPort *timer_port;struct timerequest *timer;int timer_open;
    uint8_t *pixels;char path[1024],message[96];
    uint32_t command,width,height,rate_num,rate_den,frames,next_time,heartbeat,start,draw_ms,pace_remainder;
    uint64_t ticks,clock_first;uint32_t clock_at,clock_hz;
    int quit,error,playing,loaded,eof,connected,verify,rewind,choose,stop;
} app;
static uint32_t now(void) {return SDL_GetTicks();}
int zv_arguments(int argc,char **argv)
{
    int i;for(i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--verify"))app.verify=1;
        else if(!app.path[0]&&strlen(argv[i])<sizeof(app.path))strcpy(app.path,argv[i]);
        else {puts("Usage: ZZVideo [clip.mpg] [--verify]");return -1;}
    }
    return 0;
}
static void wait_tick(void)
{
    if(app.timer_open) {
        app.timer->tr_node.io_Command=TR_ADDREQUEST;
        app.timer->tr_time.tv_secs=0;app.timer->tr_time.tv_micro=2000;
        if(DoIO((struct IORequest *)app.timer)){app.error=1;app.quit=1;}
    } else Delay(1);
}
static void status(const char *s)
{
    if(s!=app.message)snprintf(app.message,sizeof(app.message),"%s",s);
    if(app.window)SDL_SetWindowTitle(app.window,app.message);
}
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
static void chrome(void)
{
    SDL_Rect top={0,0,400,32},bottom={0,276,400,36};
    SDL_FillRect(app.surface,&top,SDL_MapRGB(app.surface->format,28,38,60));
    SDL_FillRect(app.surface,&bottom,SDL_MapRGB(app.surface->format,28,38,60));
    text_at(8,12,"[O] OPEN  [SPACE] PLAY/PAUSE  [Q] QUIT");
    text_at(8,289,"[R] REPLAY  [S] STOP    MPEG-1 / ARM");
    SDL_UpdateWindowSurface(app.window);
}
static int submit(unsigned cmd,unsigned size,unsigned hash)
{
    if(zv_submit(&app.client,cmd,size,hash)){app.error=app.quit=1;return -1;}
    app.command=cmd;return 0;
}
static int select_file(void)
{
    struct FileRequester *r;int ok=0;
    if(!AslBase)AslBase=OpenLibrary("asl.library",38);
    if(!AslBase)return 0;
    r=AllocAslRequestTags(ASL_FileRequest,ASLFR_TitleText,(ULONG)"Open MPEG-1 video (up to 4 MiB)",
        ASLFR_DoPatterns,TRUE,ASLFR_InitialPattern,(ULONG)"#?.(mpg|mpeg|m1v)",TAG_DONE);
    if(!r)return 0;
    if(AslRequestTags(r,TAG_DONE)) {
        if(strlen((char *)r->fr_Drawer)+strlen((char *)r->fr_File)+2<sizeof(app.path)) {
            strcpy(app.path,(char *)r->fr_Drawer);ok=AddPart(app.path,r->fr_File,sizeof(app.path));
        }
    }
    FreeAslRequest(r);return ok;
}
static int load(void)
{
    FILE *f;long size;size_t got=0;uint32_t hash= zv_seed(app.client.session,app.client.seq+1);
    status("ZZVideo: loading MPEG-1 video");
    f=fopen(app.path,"rb");if(!f){status("ZZVideo: cannot open file - O to choose another");return -1;}
    if(fseek(f,0,SEEK_END)||((size=ftell(f))<=0)||size>(long)ZV_MAX_INPUT||fseek(f,0,SEEK_SET)) {
        fclose(f);status("ZZVideo: file must be between 1 byte and 4 MiB");return -1;
    }
    app.loaded=app.playing=app.eof=0;app.frames=0;app.pace_remainder=0;app.ticks=0;app.draw_ms=0;
    while(got<(size_t)size) {
        uint8_t chunk[4096];size_t n=(size_t)size-got,i;if(n>sizeof(chunk))n=sizeof(chunk);
        if(fread(chunk,1,n,f)!=n){fclose(f);status("ZZVideo: file read failed");return -1;}
        hash=zv_hash(hash,chunk,n);
        for(i=0;i<n;i++)app.client.mem[ZV_INPUT+got+i]=chunk[i];
        app.client.io->push(app.client.mem+ZV_INPUT+got,n,app.client.io->user);got+=n;
        if(app.connected)ab_poll();
        if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C){fclose(f);app.quit=1;return -1;}
    }
    fclose(f);printf("INPUT path=%s bytes=%ld hash=%08lx\n",app.path,size,(unsigned long)hash);fflush(stdout);
    return submit(ZV_OPEN,(unsigned)size,hash);
}
static void paint(void)
{
    SDL_Rect dest={40+(320-(int)app.width)/2,34+(240-(int)app.height)/2,0,0};uint32_t before=now();
    if(!app.frame_surface)app.frame_surface=SDL_CreateRGBSurfaceFrom(app.pixels,app.width,app.height,32,app.width*4,
        0x00ff0000,0x0000ff00,0x000000ff,0);
    if(!app.frame_surface||SDL_BlitSurface(app.frame_surface,0,app.surface,&dest)<0||
       SDL_UpdateWindowSurfaceRects(app.window,&dest,1)<0){app.error=app.quit=1;return;}
    app.draw_ms+=now()-before;
}
static void action(SDL_Keycode key)
{
    if(key==SDLK_q||key==SDLK_ESCAPE)app.quit=1;
    else if(key==SDLK_SPACE&&app.loaded) {
        if(app.eof)app.rewind=1;
        else {app.playing=!app.playing;app.next_time=now();status(app.playing?"ZZVideo: playing on ARM Core1":"ZZVideo: paused");}
    } else if(key==SDLK_r&&app.loaded)app.rewind=1;
    else if(key==SDLK_s&&app.loaded){app.playing=0;app.stop=1;status("ZZVideo: stopped - R to replay");}
    else if(key==SDLK_o){app.playing=0;app.choose=1;}
}
static int hook(const char *args,char *out,int cap)
{
    if(!strcmp(args,"quit"))app.quit=1;
    else if(!strcmp(args,"pause"))app.playing=0;
    else if(!strcmp(args,"play")){app.playing=1;app.next_time=now();}
    else if(!strcmp(args,"replay"))app.rewind=1;
    else if(strcmp(args,"status"))return -1;
    snprintf(out,cap,"ok=1 loaded=%d playing=%d eof=%d frame=%lu pending=%lu retries=%lu error=%d hash=%08lx",
        app.loaded,app.playing,app.eof,(unsigned long)app.frames,(unsigned long)app.client.pending,
        (unsigned long)app.client.retries,app.error,(unsigned long)app.result.hash);return 0;
}
static void result(void)
{
    struct zv_result *r=&app.result;
    if(r->code==ZV_ERROR) {
        printf("DECODE ERROR %d: %s\n",r->error,zv_error(r->error));fflush(stdout);
        snprintf(app.message,sizeof(app.message),"ZZVideo: %s",zv_error(r->error));status(app.message);
        app.playing=app.loaded=0;if(app.verify)app.error=app.quit=1;return;
    }
    if(app.command==ZV_CLOCK)return;
    if(app.command==ZV_OPEN) {
        app.width=r->width;app.height=r->height;app.rate_num=r->rate_num;app.rate_den=r->rate_den;
        if(!app.width||!app.height||!app.rate_num||!app.rate_den){app.error=app.quit=1;return;}
        if(app.frame_surface){SDL_FreeSurface(app.frame_surface);app.frame_surface=0;}
        SDL_FillRect(app.surface,0,0);chrome();app.loaded=app.playing=1;app.next_time=app.start=now();
        printf("OPEN width=%lu height=%lu fps=%lu/%lu\n",(unsigned long)app.width,(unsigned long)app.height,
            (unsigned long)app.rate_num,(unsigned long)app.rate_den);
        status("ZZVideo: playing on ARM Core1");
    } else if(app.command==ZV_REWIND) {
        app.frames=0;app.pace_remainder=0;app.ticks=0;app.eof=0;app.playing=!app.stop;app.stop=0;app.next_time=app.start=now();
        status(app.playing?"ZZVideo: playing on ARM Core1":"ZZVideo: stopped - R to replay");
    } else if(r->code==ZV_FRAME) {
        app.frames=r->frame;app.ticks+=r->ticks;
        if(app.verify)printf("FRAME %lu hash=%08lx ticks=%lu:%08lx retries=%lu\n",(unsigned long)r->frame,
            (unsigned long)r->hash,(unsigned long)(r->ticks>>32),(unsigned long)r->ticks,(unsigned long)app.client.retries);
        if(!app.choose&&!app.stop&&!app.rewind)paint();
        /* Never skip decoded reference frames. If late, slow down gracefully. */
        app.pace_remainder+=1000u*app.rate_den;
        app.next_time+=app.pace_remainder/app.rate_num;app.pace_remainder%=app.rate_num;
        if((int32_t)(now()-app.next_time)>200)app.next_time=now();
    } else if(r->code==ZV_EOF) {
        app.eof=1;app.playing=0;status("ZZVideo: finished - R to replay, O to open");
        printf("EOF frames=%lu elapsed_ms=%lu decode_ticks=%lu:%08lx draw_ms=%lu retries=%lu clock_hz=%lu\n",
            (unsigned long)app.frames,(unsigned long)(now()-app.start),(unsigned long)(app.ticks>>32),
            (unsigned long)app.ticks,(unsigned long)app.draw_ms,(unsigned long)app.client.retries,(unsigned long)app.clock_hz);
        if(app.verify)app.quit=1;
    }
    fflush(stdout);
}
int zv_window(volatile uint8_t *mem,const struct ad_io *io,int connected)
{
    SDL_Event e;int poll;unsigned phase=0;uint32_t phase_at;
    app.connected=connected;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0)return 20;
    app.window=SDL_CreateWindow("ZZVideo 0.1 - MPEG-1 / ARM Core1",100,100,400,312,0);
    if(!app.window||(app.surface=SDL_GetWindowSurface(app.window))==0||app.surface->format->BytesPerPixel!=4)goto fail;
    app.pixels=calloc(ZV_MAX_WIDTH*ZV_MAX_HEIGHT,4);if(!app.pixels)goto fail;
    if(zv_client_init(&app.client,mem,io,now))goto fail;
    app.timer_port=CreateMsgPort();
    if(app.timer_port)app.timer=(struct timerequest *)CreateIORequest(app.timer_port,sizeof(*app.timer));
    if(app.timer&&!OpenDevice(TIMERNAME,UNIT_MICROHZ,(struct IORequest *)app.timer,0))app.timer_open=1;
    if(connected)ab_register_hook("video","status pause play replay quit",hook);
    chrome();phase_at=now();submit(ZV_CLOCK,0,0);
    while(!app.quit) {
        while(SDL_PollEvent(&e)) {
            if(e.type==SDL_QUIT)app.quit=1;
            else if(e.type==SDL_KEYDOWN)action(e.key.keysym.sym);
            else if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT) {
                if(e.button.y<32)action(e.button.x<75?SDLK_o:e.button.x<290?SDLK_SPACE:SDLK_q);
                else if(e.button.y>=276)action(e.button.x<100?SDLK_r:SDLK_s);
            } else if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_EXPOSED)SDL_UpdateWindowSurface(app.window);
        }
        if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)app.quit=1;
        if(connected){ab_poll();if(now()-app.heartbeat>1000){ab_heartbeat();app.heartbeat=now();}}
        if(app.quit)break;
        poll=zv_poll(&app.client,&app.result,app.pixels,ZV_MAX_WIDTH*ZV_MAX_HEIGHT*4);
        if(poll<0){status("ZZVideo: ARM transfer timeout or invalid result");app.error=app.quit=1;break;}
        if(poll>0) {
            if(phase==0){app.clock_first=app.result.ticks;app.clock_at=now();phase=1;}
            else if(phase==2) {
                uint32_t span=now()-app.clock_at;
                if(span)app.clock_hz=(uint32_t)((app.result.ticks-app.clock_first)*1000u/span);
                printf("CLOCK estimated_hz=%lu interval_ms=%lu\n",(unsigned long)app.clock_hz,(unsigned long)span);
                phase=3;
                if(!app.path[0]&&!select_file())app.quit=1;
                else if(load()&&app.verify)app.error=app.quit=1;
            } else result();
        }
        if(phase==1&&now()-app.clock_at>=200){phase=2;submit(ZV_CLOCK,0,0);}
        if(phase<3&&now()-phase_at>30000){app.error=app.quit=1;}
        if(phase==3&&!app.client.pending&&!app.quit) {
            if(app.choose) {app.choose=0;if(select_file()&&load()&&app.verify)app.error=app.quit=1;}
            else if(app.rewind||app.stop) {app.rewind=0;submit(ZV_REWIND,0,0);}
            else if(app.loaded&&app.playing&&(int32_t)(now()-app.next_time)>=0)submit(ZV_NEXT,0,0);
        }
        wait_tick();
    }
    goto done;
fail:app.error=1;
done:
    if(connected)ab_unregister_hook("video");
    if(app.timer_open)CloseDevice((struct IORequest *)app.timer);
    if(app.timer)DeleteIORequest((struct IORequest *)app.timer);
    if(app.timer_port)DeleteMsgPort(app.timer_port);
    if(app.frame_surface)SDL_FreeSurface(app.frame_surface);
    free(app.pixels);if(app.window)SDL_DestroyWindow(app.window);SDL_Quit();
    if(AslBase)CloseLibrary(AslBase);
    printf("VIDEO exit=%d frames=%lu\n",app.error?20:0,(unsigned long)app.frames);fflush(stdout);
    return app.error?20:0;
}
