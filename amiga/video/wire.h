#ifndef ZV_WIRE_H
#define ZV_WIRE_H
#include "protocol.h"
#include "layout.h"
#include "media.h"
#define ZV_REQ (ZZ_CONTROL+0x800)
#define ZV_RES (ZZ_CONTROL+0x840)
#define ZV_PIXELS 0x20000u
#define ZV_INPUT 0x100000u
#define ZV_HEAP 0x600000u
#define ZV_MAGIC 0x5a565031u
#define ZV_OPEN 1u
#define ZV_NEXT 2u
#define ZV_REWIND 3u
#define ZV_CLOCK 4u
#define ZV_FRAME 1u
#define ZV_EOF 2u
#define ZV_OK 3u
#define ZV_ERROR 4u
#if ZZ_CONTROL != 0x10000 || ZZ_BLOCK_SIZE != 0x800000
#error Video layout requires its explicit build configuration
#endif
/* Result bytes: seq, magic, session, status, frame, width, height, rate_num,
 * rate_den, payload_size, ticks_low, ticks_high, error, data_hash, reserved,
 * header_hash. Hashes bind the session + sequence to the frame bytes. */
static inline uint32_t zv_seed(uint32_t session,uint32_t seq)
{uint8_t b[8];ad_put(b,0,session);ad_put(b,4,seq);return zv_hash(2166136261u,b,8);}
#endif
