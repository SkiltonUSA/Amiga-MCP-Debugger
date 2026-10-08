#include "client.h"
#include <string.h>
static void pull(struct zv_client *c,unsigned off,size_t n)
{c->io->pull(c->mem+off,n,c->io->user);c->io->barrier(c->io->user);}
static void push(struct zv_client *c,unsigned off,size_t n)
{c->io->push(c->mem+off,n,c->io->user);c->io->barrier(c->io->user);}
int zv_client_init(struct zv_client *c,volatile uint8_t *mem,const struct ad_io *io,uint32_t (*now)(void))
{
    if(!c||!mem||!io||!io->pull||!io->push||!io->barrier||!now)return -1;
    memset(c,0,sizeof(*c));c->mem=mem;c->io=io;c->now=now;
    pull(c,ZZ_CONTROL,64);c->session=ad_get(mem,ZZ_CONTROL);return c->session?0:-1;
}
int zv_submit(struct zv_client *c,uint32_t cmd,uint32_t size,uint32_t hash)
{
    if(c->failed||c->pending||c->seq==0xffffffffu||cmd<ZV_OPEN||cmd>ZV_CLOCK||size>ZV_MAX_INPUT)return -1;
    c->pending=++c->seq;c->started=c->now();
    ad_put(c->mem,ZV_REQ+4,c->session);ad_put(c->mem,ZV_REQ+8,cmd);
    ad_put(c->mem,ZV_REQ+12,size);ad_put(c->mem,ZV_REQ+16,hash);
    ad_put(c->mem,ZV_REQ+60,zv_hash(zv_seed(c->session,c->pending),(const void *)(c->mem+ZV_REQ+4),56));
    push(c,ZV_REQ,64);ad_put(c->mem,ZV_REQ,c->pending);push(c,ZV_REQ,64);return 0;
}
int zv_poll(struct zv_client *c,struct zv_result *r,uint8_t *pixels,size_t capacity)
{
    uint8_t header[64];uint32_t seed,hash;size_t i;
    if(c->failed||!r)return -1;
    if(!c->pending)return 0;
    if(c->now()-c->started>30000u){c->failed=1;return -1;}
    pull(c,ZV_RES,64);if(ad_get(c->mem,ZV_RES)!=c->pending)return 0;
    for(i=0;i<64;i++)header[i]=c->mem[ZV_RES+i];
    seed=zv_seed(c->session,c->pending);
    if(ad_get(header,0)!=c->pending||ad_get(header,4)!=ZV_MAGIC||ad_get(header,8)!=c->session||
       zv_hash(seed,header+4,56)!=ad_get(header,60)){c->retries++;return 0;}
    r->code=ad_get(header,12);r->frame=ad_get(header,16);r->width=ad_get(header,20);r->height=ad_get(header,24);
    r->rate_num=ad_get(header,28);r->rate_den=ad_get(header,32);r->bytes=ad_get(header,36);
    r->ticks=((uint64_t)ad_get(header,44)<<32)|ad_get(header,40);r->error=(int32_t)ad_get(header,48);
    if(r->code<ZV_FRAME||r->code>ZV_ERROR||r->bytes>capacity||
       (r->code==ZV_FRAME&&(!pixels||!r->width||!r->height||r->width>ZV_MAX_WIDTH||r->height>ZV_MAX_HEIGHT||
        r->bytes!=r->width*r->height*4))||(r->code!=ZV_FRAME&&r->bytes)) {c->failed=1;return -1;}
    hash=seed;
    if(r->bytes) {
        pull(c,ZV_PIXELS,r->bytes);
        for(i=0;i<r->bytes;i++)pixels[i]=c->mem[ZV_PIXELS+i];
        hash=zv_hash(seed,pixels,r->bytes);
    }
    if(hash!=ad_get(header,52)){c->retries++;return 0;}
    r->hash=zv_hash(2166136261u,pixels,r->bytes);c->pending=0;return 1;
}
