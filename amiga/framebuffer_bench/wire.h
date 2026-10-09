#ifndef FB_BENCH_WIRE_H
#define FB_BENCH_WIRE_H
#include "protocol.h"
#include "layout.h"
#define FB_HOST (ZZ_CONTROL+0x800)
#define FB_REPLY (ZZ_CONTROL+0x900)
#define FB_SOURCE 0x20000u
#define FB_MAX_WIDTH 640u
#define FB_MAX_HEIGHT 480u
enum { FB_SEQ=0, FB_OP=4, FB_ADDR=8, FB_PITCH=12, FB_W=16, FB_H=20, FB_NONCE=24 };
enum { FB_ACK=0, FB_ERROR=4, FB_TICKS=8, FB_CLOCK_LO=12, FB_CLOCK_HI=16 };
enum { FB_CLOCK=1, FB_PREPARE=2, FB_COPY=3 };
static inline uint32_t fb_pattern(uint32_t i)
{ return ((i&255u)<<16) | (((i>>8)&255u)<<8) | 0x60u; }
static inline int fb_valid(uint32_t addr,uint32_t pitch,uint32_t w,uint32_t h)
{
    return w && h && w<=FB_MAX_WIDTH && h<=FB_MAX_HEIGHT && !(w&7u) &&
        !(addr&3u) && !(pitch&3u) && pitch>=w*4u && pitch<=8192 &&
        addr>=0x200000u && addr<=0x41f0000u-h*pitch;
}
#endif
