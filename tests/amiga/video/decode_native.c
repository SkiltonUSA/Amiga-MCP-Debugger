#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decoder.h"
#ifdef ORACLE
#define PL_MPEG_IMPLEMENTATION
#include "vendor/pl_mpeg.h"
#endif
int main(int argc,char **argv)
{
    unsigned frame=0,w,h;int rc;FILE *f;uint8_t *data,*pixels;long n;
    if(argc!=2&&argc!=3)return 2;
    FILE *raw=argc==3?fopen(argv[2],"wb"):0;
    f=fopen(argv[1],"rb");if(!f)return 2;
    fseek(f,0,SEEK_END);n=ftell(f);rewind(f);if(n<1||n>ZV_MAX_INPUT)return 2;
    data=malloc(n);pixels=calloc(ZV_MAX_WIDTH*ZV_MAX_HEIGHT,4);
    if(!data||!pixels||fread(data,1,n,f)!=(size_t)n)return 2;
    fclose(f);
#ifdef ORACLE
    plm_t *plm=plm_create_with_memory(data,n,0);plm_set_audio_enabled(plm,0);
    w=plm_get_width(plm);h=plm_get_height(plm);
    plm_frame_t *decoded;
    while((decoded=plm_decode_video(plm))) {
        plm_frame_to_argb(decoded,pixels,w*4);printf("%u %08x\n",++frame,zv_hash(2166136261u,pixels,w*h*4));
    }
    plm_destroy(plm);rc=0;
#else
    void *heap=malloc(ZV_HEAP_SIZE);struct zv_info info;
    rc=zv_open(data,n,heap,ZV_HEAP_SIZE,&info);
    if(rc){fprintf(stderr,"%d %s\n",rc,zv_error(rc));free(data);free(pixels);free(heap);return 1;}
    w=info.width;h=info.height;
    fprintf(stderr,"%ux%u %u/%u pictures=%u heap=%zu\n",w,h,info.rate_num,info.rate_den,info.pictures,zv_heap_used());
    while((rc=zv_next(pixels))==1) {
        printf("%u %08x\n",++frame,zv_hash(2166136261u,pixels,w*h*4));
        if(raw)fwrite(pixels,1,w*h*4,raw);
    }
    /* Rewind must reproduce the same first frame without heap growth. */
    size_t used=zv_heap_used();zv_rewind();if(zv_next(pixels)!=1||zv_heap_used()!=used)return 3;
    fprintf(stderr,"rewind=%08x frames=%u\n",zv_hash(2166136261u,pixels,w*h*4),frame);
    free(heap);
#endif
    if(raw)fclose(raw);
    free(data);free(pixels);return rc<0?1:0;
}
