#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
void fb_heap_init(void *,size_t);size_t fb_heap_peak(void);
void *fb_malloc(size_t);void fb_free(void *);void *fb_realloc(void *,size_t);void *fb_calloc(size_t,size_t);
static _Alignas(64) uint8_t arena[131072];
int main(void){void *p[80];unsigned round,i;
 for(round=0;round<20;round++){
 fb_heap_init(arena,sizeof(arena));
 {unsigned char *z=fb_calloc(37,3);assert(z);for(i=0;i<111;i++)assert(z[i]==0);fb_free(z);}
 for(i=0;i<80;i++){p[i]=fb_malloc(13+i*7);assert(p[i]&&((uintptr_t)p[i]&63)==0);memset(p[i],i,13+i*7);}
 for(i=0;i<80;i+=2){uint8_t *q=fb_realloc(p[i],1000+i);assert(q);assert(q[0]==i&&q[12+i*7]==i);p[i]=q;}
 assert(!fb_calloc(SIZE_MAX,4));assert(!fb_malloc(SIZE_MAX));
 for(i=0;i<80;i++)fb_free(p[i]);
 p[0]=fb_malloc(sizeof(arena)-64);assert(p[0]);assert(!fb_malloc(1));fb_free(p[0]);
 assert(fb_heap_peak()==sizeof(arena)-64);
 }
 puts("Bounded allocator: alignment, growth, coalescing and overflow checks passed.");
}
