/* SDL2/ZZ9000 feasibility probe. All SDL/OS calls execute on the 68k.
 * ARM mode reuses the verified fractal worker and owned-memory launcher.
 * This is a diagnostic, not an OpenRCT2 port or a general SDL ARM proxy. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL.h"
#include "wire.h"

static int quit_requested;
static unsigned keys, clicks, moves;
static SDL_Window *window;
static SDL_Surface *surface;
static void events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) quit_requested = 1;
        if (e.type == SDL_KEYDOWN) {
            ++keys;
            printf("INPUT key=%ld\n", (long)e.key.keysym.sym);
            if (e.key.keysym.sym == SDLK_q || e.key.keysym.sym == SDLK_ESCAPE) quit_requested = 1;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN) {
            ++clicks;
            printf("INPUT click=%u x=%ld y=%ld\n", e.button.button, (long)e.button.x, (long)e.button.y);
        }
        if (e.type == SDL_MOUSEMOTION) ++moves;
    }
    fflush(stdout);
}
static void sync_in(volatile uint8_t *mem, const struct ad_io *io, unsigned off, unsigned size)
{ io->pull(mem+off,size,io->user); io->barrier(io->user); }
static void sync_out(volatile uint8_t *mem, const struct ad_io *io, unsigned off, unsigned size)
{ io->push(mem+off,size,io->user); io->barrier(io->user); }
static int arm_frame(volatile uint8_t *mem, const struct ad_io *io)
{
    struct ff_view v;
    unsigned tx,ty,x,y,i;
    uint32_t seq=0,hash=2166136261u;
    uint16_t *counts=malloc(FF_WIDTH*FF_HEIGHT*sizeof(*counts));
    Uint32 start=SDL_GetTicks();
    if (!counts) return 20;
    ff_default(&v);
    ad_put(mem,FF_CANCEL,1); sync_out(mem,io,FF_CANCEL,64);
    for (ty=0;ty<FF_HEIGHT;ty+=FF_TH) for (tx=0;tx<FF_WIDTH;tx+=FF_TW) {
        Uint32 wait_start;
        ad_put(mem,FF_REQ+4,ad_get(mem,ZZ_CONTROL));
        ad_put(mem,FF_REQ+8,1); ad_put(mem,FF_REQ+12,(uint32_t)v.cx);
        ad_put(mem,FF_REQ+16,(uint32_t)v.cy); ad_put(mem,FF_REQ+20,(uint32_t)v.step);
        ad_put(mem,FF_REQ+24,v.limit); ad_put(mem,FF_REQ+28,tx); ad_put(mem,FF_REQ+32,ty);
        sync_out(mem,io,FF_REQ,64);
        ad_put(mem,FF_REQ,++seq); sync_out(mem,io,FF_REQ,64);
        wait_start=SDL_GetTicks();
        for (;;) {
            sync_in(mem,io,FF_RES,64);
            if (ad_get(mem,FF_RES)==seq) break;
            events();
            if (quit_requested || SDL_GetTicks()-wait_start>5000) {
                ad_put(mem,FF_CANCEL,2); sync_out(mem,io,FF_CANCEL,64);
                free(counts); return quit_requested?0:20;
            }
            io->idle(io->user);
        }
        if (ad_get(mem,FF_RES+4)!=FF_DONE || ad_get(mem,FF_RES+8)!=1 ||
            ad_get(mem,FF_RES+12)!=tx || ad_get(mem,FF_RES+16)!=ty) {free(counts);return 20;}
        sync_in(mem,io,FF_DATA,FF_PIXELS*2);
        for(y=0;y<FF_TH;y++) for(x=0;x<FF_TW;x++) {
            unsigned n,colour; Uint32 *row;
            i=y*FF_TW+x; n=((unsigned)mem[FF_DATA+i*2]<<8)|mem[FF_DATA+i*2+1];
            counts[(ty+y)*FF_WIDTH+tx+x]=(uint16_t)n;
            colour=n>=v.limit?0:1+(n*3)%31;
            row=(Uint32 *)((Uint8 *)surface->pixels+(ty+y)*surface->pitch);
            row[tx+x]=colour?SDL_MapRGB(surface->format,colour*7,colour*3,255-colour*7):SDL_MapRGB(surface->format,0,0,0);
        }
        {SDL_Rect r={(int)tx,(int)ty,FF_TW,FF_TH};
         if(SDL_UpdateWindowSurfaceRects(window,&r,1)<0){free(counts);return 20;}}
        events();
    }
    for(i=0;i<FF_WIDTH*FF_HEIGHT;i++) hash=ff_hash(hash,counts[i]);
    free(counts);
    printf("ARM_FRAME tiles=%lu hash=%08lx expected=fb32f6c6 elapsed_ms=%lu\n",
        (unsigned long)seq,(unsigned long)hash,(unsigned long)(SDL_GetTicks()-start));
    fflush(stdout);
    return hash==0xfb32f6c6u?0:20;
}
static int benchmark(int dirty)
{
    unsigned i;
    Uint64 begin,end,freq=SDL_GetPerformanceFrequency();
    SDL_Rect r={0,0,32,16};
    begin=SDL_GetPerformanceCounter();
    for(i=0;i<30 && !quit_requested;i++) {
        int rc;
        r.x=(i*13)%(FF_WIDTH-32); r.y=(i*7)%(FF_HEIGHT-16);
        rc=dirty?SDL_UpdateWindowSurfaceRects(window,&r,1):SDL_UpdateWindowSurface(window);
        if(rc<0) return 20;
        events();
    }
    end=SDL_GetPerformanceCounter();
    printf("PRESENT path=%s frames=%u elapsed_us=%lu pixel_bytes_per_frame=%u\n",
        dirty?"surface_rect":"surface_full",i,(unsigned long)((end-begin)*1000000/freq),
        dirty?32*16*4:FF_WIDTH*FF_HEIGHT*4);
    fflush(stdout); return 0;
}
static int run_probe(volatile uint8_t *mem,const struct ad_io *io)
{
    SDL_version version;
    unsigned x,y;
    Uint32 wait_start;
    int rc=20;
    SDL_GetVersion(&version);
    printf("SDL_PROBE version=%u.%u.%u mode=%s library_O0=1\n",version.major,version.minor,version.patch,mem?"ARM":"68K");fflush(stdout);
    if(SDL_Init(SDL_INIT_VIDEO)<0) goto done;
    printf("INIT driver=%s\n",SDL_GetCurrentVideoDriver());fflush(stdout);
    window=SDL_CreateWindow(mem?"SDL2 ARM Probe":"SDL2 68K Probe",100,180,FF_WIDTH,FF_HEIGHT,0);
    if(!window) goto done;
    surface=SDL_GetWindowSurface(window);
    if(!surface || surface->format->BytesPerPixel!=4) goto done;
    printf("SURFACE w=%d h=%d pitch=%d format=%s\n",surface->w,surface->h,surface->pitch,SDL_GetPixelFormatName(surface->format->format));fflush(stdout);
    for(y=0;y<FF_HEIGHT;y++) for(x=0;x<FF_WIDTH;x++) {
        Uint32 *row=(Uint32 *)((Uint8 *)surface->pixels+y*surface->pitch);
        row[x]=SDL_MapRGB(surface->format,x<160?255:0,y<120?255:0,(x>=160&&y>=120)?255:0);
    }
    if(SDL_UpdateWindowSurface(window)<0) goto done;
    if(mem && arm_frame(mem,io)) goto done;
    if(!quit_requested && (benchmark(0)||benchmark(1))) goto done;
    printf("READY: click and press a key; Q/Escape/close quits; auto-exit in 60 seconds\n");fflush(stdout);
    wait_start=SDL_GetTicks();
    while(!quit_requested && SDL_GetTicks()-wait_start<60000) {events();SDL_Delay(20);}
    rc=0;
done:
    if(rc) printf("FAIL: %s\n",SDL_GetError());
    if(window) SDL_DestroyWindow(window);
    SDL_Quit();
    printf("SDL_EXIT rc=%d keys=%u clicks=%u moves=%u\n",rc,keys,clicks,moves);fflush(stdout);
    return rc;
}
#ifdef SDL_ARM_PROBE
int ff_window(volatile uint8_t *mem,const struct ad_io *io,int connected)
{(void)connected;return run_probe(mem,io);}
#else
int main(void) {return run_probe(NULL,NULL);}
#endif
