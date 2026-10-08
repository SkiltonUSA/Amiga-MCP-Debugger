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
#if ZZ_PAGE + AD_PAGE_SIZE > FF_REQ || FF_DATA + FF_PIXELS * 2 > ZZ_STACK_TOP - 16384
#error Fractal layout overlaps debugger or stack reserve
#endif
#endif
