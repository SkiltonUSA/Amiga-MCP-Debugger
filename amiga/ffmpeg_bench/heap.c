/* Single-worker bounded allocator, 64-byte aligned. No firmware allocator use. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
struct block {size_t size;struct block *next;unsigned free;};
static struct block *first;static size_t live,peak;
void fb_heap_init(void *p,size_t n){first=p;first->size=n-64;first->next=0;first->free=1;live=peak=0;}
size_t fb_heap_peak(void){return peak;}
void *malloc(size_t n){struct block *b;size_t rounded;if(n>SIZE_MAX-63)return 0;rounded=(n+63)&~(size_t)63;if(!rounded)rounded=64;
 for(b=first;b;b=b->next)if(b->free&&b->size>=rounded){
  if(b->size>=rounded+128){struct block *next=(void *)((uint8_t *)b+64+rounded);next->size=b->size-rounded-64;next->free=1;next->next=b->next;b->next=next;b->size=rounded;}
  b->free=0;live+=b->size;if(live>peak)peak=live;return (uint8_t *)b+64;
 }return 0;}
void free(void *p){struct block *b;if(!p)return;b=(void *)((uint8_t *)p-64);if(b->free)return;b->free=1;live-=b->size;
 for(b=first;b&&b->next;)if(b->free&&b->next->free){b->size+=64+b->next->size;b->next=b->next->next;}else b=b->next;}
void *calloc(size_t n,size_t s){void *p;if(s&&n>SIZE_MAX/s)return 0;p=malloc(n*s);if(p)memset(p,0,n*s);return p;}
void *realloc(void *p,size_t n){void *q;struct block *b;if(!p)return malloc(n);if(!n){free(p);return 0;}b=(void *)((uint8_t *)p-64);if(n<=b->size)return p;q=malloc(n);if(q){memcpy(q,p,b->size);free(p);}return q;}
int posix_memalign(void **p,size_t a,size_t n){if(a>64||!a||(a&(a-1)))return 22;*p=malloc(n);return *p?0:12;}
void *aligned_alloc(size_t a,size_t n){void *p=0;return posix_memalign(&p,a,n)?0:p;}
