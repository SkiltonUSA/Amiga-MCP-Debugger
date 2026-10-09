#define FB_NATIVE
#include "worker.c"
#include "../video/media.h"
#include <stdio.h>
void fb_heap_init(void *p,size_t n){(void)p;(void)n;}
size_t fb_heap_peak(void){return 0;}
int main(int argc,char **argv){
 FILE *f;long n;struct zv_info info;unsigned count=0,i;uint64_t dt=0,pt=0;int rc;
 if(argc!=2)return 2;shared=calloc(1,ZZ_BLOCK_SIZE);if(!shared)return 2;
 f=fopen(argv[1],"rb");if(!f)return 2;fseek(f,0,SEEK_END);n=ftell(f);rewind(f);
 if(n<=0||n>FB_INPUT_MAX-64)return 2;
 if(fread((void *)(shared+FB_INPUT),1,n,f)!=(size_t)n)return 2;fclose(f);
 rc=zv_prepare((uint8_t *)shared+FB_INPUT,n,&info);if(rc)return 3;
 shared[FB_INPUT+info.bytes]=0;shared[FB_INPUT+info.bytes+1]=0;shared[FB_INPUT+info.bytes+2]=1;shared[FB_INPUT+info.bytes+3]=0xb7;info.bytes+=4;
 f=fopen("bframes.m1v","wb");if(!f)return 2;fwrite((const void *)(shared+FB_INPUT),1,info.bytes,f);fclose(f);
 memset((void *)(shared+FB_INPUT+info.bytes),0,64);
 rc=decode(info.bytes,0,&count,&dt,&pt);
 fprintf(stderr,"Native FFmpeg rc=%d frames=%u\n",rc,count);
 for(i=0;i<count;i++)printf("%08x\n",ad_get(shared,FB_HASHES+i*4));
 free((void *)shared);return rc||count!=25;
}
