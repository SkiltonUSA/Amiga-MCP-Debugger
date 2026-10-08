#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"
static uint32_t clock_ms;
static uint32_t now(void){return clock_ms;}
static void range(volatile void *p,size_t n,void *u){(void)p;(void)n;(void)u;}
static void barrier(void *u){(void)u;}
static void result(uint8_t *m,struct zv_client *c,uint8_t *data,unsigned bytes)
{
    unsigned i;uint32_t seed=zv_seed(c->session,c->pending);
    memset(m+ZV_RES,0,64);
    ad_put(m,ZV_RES,c->pending);ad_put(m,ZV_RES+4,ZV_MAGIC);ad_put(m,ZV_RES+8,c->session);
    ad_put(m,ZV_RES+12,ZV_FRAME);ad_put(m,ZV_RES+16,1);ad_put(m,ZV_RES+20,2);ad_put(m,ZV_RES+24,2);
    ad_put(m,ZV_RES+28,25);ad_put(m,ZV_RES+32,1);ad_put(m,ZV_RES+36,bytes);
    ad_put(m,ZV_RES+52,zv_hash(seed,data,bytes));
    ad_put(m,ZV_RES+60,zv_hash(seed,m+ZV_RES+4,56));
    for(i=0;i<bytes;i++)m[ZV_PIXELS+i]=data[i];
}
int main(void)
{
    uint8_t *m=calloc(1,ZZ_BLOCK_SIZE),pixels[16]={0},data[16]={0,1,2,3};
    struct ad_io io={range,range,barrier,barrier,0};struct zv_client c;struct zv_result r;
    assert(m);ad_put(m,ZZ_CONTROL,0x12345678);assert(!zv_client_init(&c,m,&io,now));
    assert(!zv_submit(&c,ZV_NEXT,0,0));assert(zv_submit(&c,ZV_NEXT,0,0)<0);
    assert(zv_poll(&c,&r,pixels,16)==0);
    result(m,&c,data,16);m[ZV_PIXELS+2]^=1;
    assert(zv_poll(&c,&r,pixels,16)==0&&c.pending&&c.retries==1);
    result(m,&c,data,16);m[ZV_RES+24]^=0x80; /* corrupt size must never reach a copy */
    assert(zv_poll(&c,&r,pixels,16)==0&&c.pending);
    result(m,&c,data,16);assert(zv_poll(&c,&r,pixels,16)==1&&!c.pending&&!memcmp(pixels,data,16));
    assert(!zv_submit(&c,ZV_NEXT,0,0));assert(zv_poll(&c,&r,pixels,16)==0); /* stale sequence */
    result(m,&c,data,16);ad_put(m,ZV_RES+8,c.session+1);
    assert(zv_poll(&c,&r,pixels,16)==0); /* stale session */
    result(m,&c,data,16);assert(zv_poll(&c,&r,pixels,8)==-1); /* bounded destination */
    assert(!zv_client_init(&c,m,&io,now));assert(!zv_submit(&c,ZV_NEXT,0,0));
    clock_ms=30001;assert(zv_poll(&c,&r,pixels,16)==-1&&c.failed);
    free(m);return 0;
}
