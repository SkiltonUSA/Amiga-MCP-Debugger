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
#ifdef FF_DIRECT
    if(clock_only<0||clock_only>3||(clock_only>=2&&(tx||ty)))return -1;
#else
    if(clock_only<0||clock_only>1)return -1;
#endif
    if(c->failed||c->pending||!ff_valid(v,tx,ty)||c->sequence==0xffffffffu)return -1;
    begin=c->now_us(c->clock_user);
    ad_put(c->mem,FF_REQ+4,c->session);ad_put(c->mem,FF_REQ+8,c->generation);
    ad_put(c->mem,FF_REQ+12,(uint32_t)v->cx);ad_put(c->mem,FF_REQ+16,(uint32_t)v->cy);
    ad_put(c->mem,FF_REQ+20,(uint32_t)v->step);ad_put(c->mem,FF_REQ+24,v->limit);
    ad_put(c->mem,FF_REQ+28,tx);ad_put(c->mem,FF_REQ+32,ty);
    ad_put(c->mem,FF_REQ+36,(uint32_t)clock_only);push(c,FF_REQ,64);
    c->pending=++c->sequence;c->pending_generation=c->generation;c->tx=tx;c->ty=ty;
    c->kind=(uint32_t)clock_only;c->started=begin;c->checksum_retries=0;
    ad_put(c->mem,FF_REQ,c->pending);push(c,FF_REQ,64);
    c->transfer_us=c->now_us(c->clock_user)-begin;return 0;
}
static uint32_t deadline(const struct zc_client *c)
{
#ifdef FF_DIRECT
    /* One full frame may take much longer than a single legacy tile.
     * Cooperative cancellation and UI service remain live throughout. */
    if(c->kind==2)return 120000000u;
#else
    (void)c;
#endif
    return 10000000u;
}
int zc_poll(struct zc_client *c,struct zc_result *r)
{
    uint32_t begin,code,gen,hash;unsigned i;
    if(!r||c->failed)return ZC_ERROR;
    if(!c->pending)return ZC_WAIT;
    begin=c->now_us(c->clock_user);pull(c,FF_RES,64);
    if(ad_get(c->mem,FF_RES)!=c->pending) {
        c->transfer_us+=c->now_us(c->clock_user)-begin;
        if(begin-c->started>deadline(c)){c->failed=1;return ZC_ERROR;}
        return ZC_WAIT;
    }
    code=ad_get(c->mem,FF_RES+4);gen=ad_get(c->mem,FF_RES+8);
    if(gen!=c->pending_generation||ad_get(c->mem,FF_RES+12)!=c->tx||
       ad_get(c->mem,FF_RES+16)!=c->ty||ad_get(c->mem,FF_RES+40)!=0x54494d33u) {
        c->failed=1;return ZC_ERROR;
    }
    if(gen!=c->generation){c->pending=0;return ZC_DISCARDED;}
    if((!c->kind&&code!=FF_DONE)||(c->kind==1&&code!=4)
#ifdef FF_DIRECT
       ||(c->kind==2&&code!=FF_FRAME_DONE)||(c->kind==3&&code!=FF_PRESENT_DONE)
#endif
       ){c->failed=1;return ZC_ERROR;}
    r->compute_ticks=((uint64_t)ad_get(c->mem,FF_RES+24)<<32)|ad_get(c->mem,FF_RES+20);
    r->stamp=((uint64_t)ad_get(c->mem,FF_RES+32)<<32)|ad_get(c->mem,FF_RES+28);
    r->control=ad_get(c->mem,FF_RES+36);
    hash=ff_timing_hash(ff_result_seed(c->session,c->pending,c->pending_generation,c->tx,c->ty),
        r->compute_ticks,r->stamp,r->control);
#ifdef FF_DIRECT
    if(c->kind>=2) {
        r->frame_hash=ad_get(c->mem,FF_RES+48);r->colour_ticks=ad_get(c->mem,FF_RES+52);
        r->copy_ticks=ad_get(c->mem,FF_RES+56);
        hash=ff_direct_hash(hash,r->frame_hash,r->colour_ticks,r->copy_ticks);
    }
#endif
    if(!c->kind) {
        pull(c,FF_DATA,FF_PIXELS*2);
        for(i=0;i<FF_PIXELS;i++) {
            r->pixels[i]=((unsigned)c->mem[FF_DATA+i*2]<<8)|c->mem[FF_DATA+i*2+1];
            hash=ff_hash(hash,r->pixels[i]);
        }
    }
    /* Keep ownership until the published tile is visible and validated.
     * A mismatch is never displayed or followed by another request. Recheck
     * with the normal scheduler until the existing ten-second job deadline;
     * neither resend work nor add a fixed visibility delay. Persistent damage
     * fails closed and the launcher retains safe teardown. */
    c->observed_hash=hash;c->expected_hash=ad_get(c->mem,FF_RES+44);
    if(hash!=c->expected_hash) {
        c->transfer_us+=c->now_us(c->clock_user)-begin;
        ++c->checksum_retries;
        if(begin-c->started>deadline(c)){c->failed=1;return ZC_ERROR;}
        return ZC_WAIT;
    }
    c->pending=0;
    r->transfer_us=c->transfer_us+c->now_us(c->clock_user)-begin;
    r->roundtrip_us=c->now_us(c->clock_user)-c->started;
    return c->kind==3?ZC_PRESENT:c->kind==2?ZC_FRAME:c->kind?ZC_CLOCK:ZC_TILE;
}
