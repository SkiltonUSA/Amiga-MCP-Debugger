#include <string.h>
#include "xx19c.h"
static void pull(struct zc_client *c,unsigned off,unsigned size)
{c->io->pull(c->mem+off,size,c->io->user);c->io->barrier(c->io->user);}
static void push(struct zc_client *c,unsigned off,unsigned size)
{c->io->push(c->mem+off,size,c->io->user);c->io->barrier(c->io->user);}
int zc_init(struct zc_client *c,volatile uint8_t *mem,size_t bytes,const struct ad_io *io,
            uint32_t (*now_us)(void *),void *user)
{
    if(!c||!mem||bytes<ZZ_BLOCK_SIZE||!io||!io->pull||!io->push||!io->barrier||!now_us)return -1;
    memset(c,0,sizeof(*c));c->mem=mem;c->io=io;c->now_us=now_us;c->clock_user=user;
    pull(c,ZZ_CONTROL,64);c->session=ad_get(mem,ZZ_CONTROL);
    if(!c->session)return -1;
    return zc_cancel(c);
}
int zc_cancel(struct zc_client *c)
{
    if(c->failed||c->generation==0xffffffffu){c->failed=1;return -1;}
    ++c->generation;ad_put(c->mem,FF_CANCEL,c->generation);push(c,FF_CANCEL,64);return 0;
}
int zc_submit(struct zc_client *c,const struct ff_view *v,uint32_t tx,uint32_t ty,int clock_only)
{
    uint32_t begin;
    if(c->failed||c->pending||!ff_valid(v,tx,ty)||c->sequence==0xffffffffu)return -1;
    begin=c->now_us(c->clock_user);
    ad_put(c->mem,FF_REQ+4,c->session);ad_put(c->mem,FF_REQ+8,c->generation);
    ad_put(c->mem,FF_REQ+12,(uint32_t)v->cx);ad_put(c->mem,FF_REQ+16,(uint32_t)v->cy);
    ad_put(c->mem,FF_REQ+20,(uint32_t)v->step);ad_put(c->mem,FF_REQ+24,v->limit);
    ad_put(c->mem,FF_REQ+28,tx);ad_put(c->mem,FF_REQ+32,ty);
    ad_put(c->mem,FF_REQ+36,clock_only?1:0);push(c,FF_REQ,64);
    c->pending=++c->sequence;c->pending_generation=c->generation;c->tx=tx;c->ty=ty;
    c->kind=clock_only?1:0;c->started=begin;
    ad_put(c->mem,FF_REQ,c->pending);push(c,FF_REQ,64);
    c->transfer_us=c->now_us(c->clock_user)-begin;return 0;
}
int zc_poll(struct zc_client *c,struct zc_result *r)
{
    uint32_t begin,code,gen;unsigned i;
    if(!r||c->failed)return ZC_ERROR;
    if(!c->pending)return ZC_WAIT;
    begin=c->now_us(c->clock_user);pull(c,FF_RES,64);
    if(ad_get(c->mem,FF_RES)!=c->pending) {
        c->transfer_us+=c->now_us(c->clock_user)-begin;
        if(begin-c->started>10000000u){c->failed=1;return ZC_ERROR;}
        return ZC_WAIT;
    }
    code=ad_get(c->mem,FF_RES+4);gen=ad_get(c->mem,FF_RES+8);
    if(gen!=c->pending_generation||ad_get(c->mem,FF_RES+12)!=c->tx||
       ad_get(c->mem,FF_RES+16)!=c->ty||ad_get(c->mem,FF_RES+40)!=0x54494d31u) {
        c->failed=1;return ZC_ERROR;
    }
    c->pending=0;
    if(gen!=c->generation)return ZC_DISCARDED;
    if((!c->kind&&code!=FF_DONE)||(c->kind&&code!=4)){c->failed=1;return ZC_ERROR;}
    r->compute_ticks=((uint64_t)ad_get(c->mem,FF_RES+24)<<32)|ad_get(c->mem,FF_RES+20);
    r->stamp=((uint64_t)ad_get(c->mem,FF_RES+32)<<32)|ad_get(c->mem,FF_RES+28);
    r->control=ad_get(c->mem,FF_RES+36);
    if(!c->kind) {
        pull(c,FF_DATA,FF_PIXELS*2);
        for(i=0;i<FF_PIXELS;i++)r->pixels[i]=((unsigned)c->mem[FF_DATA+i*2]<<8)|c->mem[FF_DATA+i*2+1];
    }
    r->transfer_us=c->transfer_us+c->now_us(c->clock_user)-begin;
    r->roundtrip_us=c->now_us(c->clock_user)-c->started;
    return c->kind?ZC_CLOCK:ZC_TILE;
}
