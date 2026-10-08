#ifndef ZV_CLIENT_H
#define ZV_CLIENT_H
#include "wire.h"
struct zv_result {uint32_t code,frame,width,height,rate_num,rate_den,bytes,hash;int error;uint64_t ticks;};
struct zv_client {
    volatile uint8_t *mem;const struct ad_io *io;uint32_t session,seq,pending,started,retries;
    uint32_t (*now)(void);int failed;
};
int zv_client_init(struct zv_client *,volatile uint8_t *,const struct ad_io *,uint32_t (*now)(void));
int zv_submit(struct zv_client *,uint32_t command,uint32_t size,uint32_t input_hash);
/* 0 waiting, 1 validated result, -1 failure. Frame storage is caller owned. */
int zv_poll(struct zv_client *,struct zv_result *,uint8_t *pixels,size_t capacity);
#endif
