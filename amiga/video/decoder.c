#include "decoder.h"
#include <string.h>
static uint8_t *arena;static size_t capacity,used;
static void *video_alloc(size_t n)
{
    void *p;if(n>capacity-used)return 0;
    n=(n+7)&~(size_t)7;if(n>capacity-used)return 0;
    p=arena+used;used+=n;memset(p,0,n);return p;
}
/* Only fixed-memory buffer + video decoder APIs are reachable. No growth/free. */
#define PLM_MALLOC(n) video_alloc(n)
#define PLM_REALLOC(p,n) ((void)(p),(void)(n),(void *)0)
#define PLM_FREE(p) ((void)(p))
#define PLM_NO_STDIO
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg_port.h"
static plm_video_t *decoder;
size_t zv_heap_used(void) {return used;}
int zv_open(uint8_t *data,size_t n,void *heap,size_t heap_size,struct zv_info *info)
{
    plm_buffer_t *buffer;int rc;
    decoder=0;arena=heap;capacity=heap_size;used=0;
    if(!heap||((uintptr_t)heap&7))return -6;
    rc=zv_prepare(data,n,info);if(rc)return rc;
    buffer=plm_buffer_create_with_memory(data,info->bytes,0);if(!buffer)return -6;
    decoder=plm_video_create_with_buffer(buffer,0);
    if(!decoder||!plm_video_has_header(decoder)){decoder=0;return -6;}
    return 0;
}
int zv_next(uint8_t *argb)
{return zv_next_timed(argb,0,0);}
int zv_next_timed(uint8_t *argb,uint64_t (*clock)(void),struct zv_timing *timing)
{
    plm_frame_t *f;uint64_t before=clock?clock():0,decoded;
    if(timing){timing->decode=0;timing->colour=0;}
    if(!decoder||!argb)return -1;
    f=plm_video_decode(decoder);
    decoded=clock?clock():0;
    if(timing)timing->decode=decoded-before;
    if(!f)return plm_video_has_ended(decoder)?0:-1;
    plm_frame_to_argb(f,argb,(int)f->width*4);
    if(timing&&clock)timing->colour=clock()-decoded;
    return 1;
}
void zv_rewind(void) {if(decoder)plm_video_rewind(decoder);}
