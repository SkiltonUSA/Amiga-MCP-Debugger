#ifndef ZZ_FRACTAL_WIRE_H
#define ZZ_FRACTAL_WIRE_H
#include "layout.h"
#include "protocol.h"
#include "fractal.h"
/* All offsets are INSIDE the launcher-owned 128 KiB block. Distinct 64-byte
 * writer lines; one immutable outstanding request and one response buffer.
 * Request: seq(commit), session, generation, cx, cy, step, limit, tx, ty.
 * Cancel: current generation. Response: seq(commit), result, generation, tx,ty.
 * Result pixels are big-endian uint16; published before response commit. */
#define FF_REQ 0x8800u
#define FF_CANCEL 0x8840u
#define FF_RES 0x8880u
#define FF_DATA 0x8900u
#define FF_DONE 1u
#define FF_CANCELLED 2u
#define FF_INVALID 3u
#ifdef FF_DIRECT
/* Full-frame storage INSIDE the enlarged Exec-owned allocation. */
#define FF_FRAME_COUNTS 0x20000u
#define FF_FRAME_RGB 0x50000u
#define FF_FRAME_DONE 5u
#define FF_PRESENT_DONE 6u
#if FF_FRAME_COUNTS + FF_WIDTH*FF_HEIGHT*2 > FF_FRAME_RGB || FF_FRAME_RGB + FF_WIDTH*FF_HEIGHT*4 > ZZ_STACK_TOP - 16384
#error Direct frame overlaps private allocation regions
#endif
static inline uint32_t ff_rgb(uint16_t count,uint32_t limit)
{
    uint32_t c=count>=limit?0:1+(count*3)%31;
    return c?((c*7)<<16)|((c*3)<<8)|(255-c*7):0;
}
static inline uint32_t ff_direct_hash(uint32_t h,uint32_t frame,uint32_t colour,uint32_t copy)
{
    h=ff_hash(ff_hash(h,(uint16_t)(frame>>16)),(uint16_t)frame);
    h=ff_hash(ff_hash(h,(uint16_t)(colour>>16)),(uint16_t)colour);
    return ff_hash(ff_hash(h,(uint16_t)(copy>>16)),(uint16_t)copy);
}
#endif
/* Bind integrity to the request as well as pixels; stale data and its old
 * checksum must never validate as a new tile. Each word is hashed big-endian. */
static inline uint32_t ff_result_seed(uint32_t session,uint32_t seq,uint32_t gen,uint32_t tx,uint32_t ty)
{
    uint32_t words[5]={session,seq,gen,tx,ty},h=2166136261u;unsigned i;
    for(i=0;i<5;i++){h=ff_hash(h,(uint16_t)(words[i]>>16));h=ff_hash(h,(uint16_t)words[i]);}
    return h;
}
/* Cover timing metadata too: accepting a new tile with old counters would
 * produce misleading benchmarks even if the displayed pixels were correct. */
static inline uint32_t ff_timing_hash(uint32_t h,uint64_t ticks,uint64_t stamp,uint32_t control)
{
    uint32_t words[5]={(uint32_t)(ticks>>32),(uint32_t)ticks,
        (uint32_t)(stamp>>32),(uint32_t)stamp,control};unsigned i;
    for(i=0;i<5;i++){h=ff_hash(h,(uint16_t)(words[i]>>16));h=ff_hash(h,(uint16_t)words[i]);}
    return h;
}
#if ZZ_PAGE + AD_PAGE_SIZE > FF_REQ || FF_DATA + FF_PIXELS * 2 > ZZ_STACK_TOP - 16384
#error Fractal layout overlaps debugger or stack reserve
#endif
#endif
