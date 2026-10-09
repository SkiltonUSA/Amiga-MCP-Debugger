#ifndef ZV_DECODER_H
#define ZV_DECODER_H
#include "media.h"
/* One Core1 decoder, caller owns input/heap/output for its entire lifetime. */
int zv_open(uint8_t *data,size_t n,void *heap,size_t heap_size,struct zv_info *info);
int zv_next(uint8_t *argb); /* 1 frame, 0 EOF, -1 decoder failure */
struct zv_timing {uint64_t decode,colour;};
int zv_next_timed(uint8_t *argb,uint64_t (*clock)(void),struct zv_timing *timing);
void zv_rewind(void);
size_t zv_heap_used(void);
#endif
