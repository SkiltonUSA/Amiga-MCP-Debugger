#ifndef ZZ_FFMPEG_BENCH_H
#define ZZ_FFMPEG_BENCH_H
#include "protocol.h"
#include "layout.h"
#define FB_REQUEST (ZZ_CONTROL+0x1000u)
#define FB_RESULT (ZZ_CONTROL+0x1100u)
#define FB_HASHES (ZZ_CONTROL+0x1200u)
#define FB_INPUT 0x200000u
#define FB_INPUT_MAX 0x40000u
#define FB_HEAP 0x300000u
#define FB_HEAP_SIZE 0xb00000u
#define FB_MAX_FRAMES 100u
#define FB_MAGIC 0x46464231u
static inline uint32_t fb_hash(uint32_t h,const void *p,size_t n) {
 const uint8_t *b=p;while(n--){h^=*b++;h*=16777619u;}return h;
}
#endif
