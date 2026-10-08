#ifndef ZV_MEDIA_H
#define ZV_MEDIA_H
#include <stddef.h>
#include <stdint.h>
#define ZV_MAX_WIDTH 320u
#define ZV_MAX_HEIGHT 240u
#define ZV_MAX_INPUT (4u*1024u*1024u)
#define ZV_HEAP_SIZE (512u*1024u)
struct zv_info { uint32_t width,height,rate_num,rate_den,pictures; size_t bytes; };
/* In-place MPEG-1 PS -> elementary video. No floating point, audio discarded.
 * Also accepts MPEG-1 elementary streams. Validates the preview's limits. */
int zv_prepare(uint8_t *data,size_t size,struct zv_info *info);
const char *zv_error(int code);
uint32_t zv_hash(uint32_t seed,const void *data,size_t size);
#endif
