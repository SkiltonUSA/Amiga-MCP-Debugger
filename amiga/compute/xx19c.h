#ifndef ZZ_COMPUTE_XX19C_H
#define ZZ_COMPUTE_XX19C_H
#include "wire.h"
/* Cooperative, one-job transport over the launcher's OWNED memory.
 * No allocation, hardware registers, SDL, or OS calls. Caller owns lifetime.
 * The first typed workload is a Mandelbrot tile; this is not arbitrary RPC. */
enum zc_status { ZC_ERROR=-1, ZC_WAIT=0, ZC_TILE=1, ZC_DISCARDED=2, ZC_CLOCK=3 };
struct zc_result {
    uint16_t pixels[FF_PIXELS];
    uint64_t compute_ticks, stamp;
    uint32_t control, roundtrip_us, transfer_us;
};
struct zc_client {
    volatile uint8_t *mem;
    const struct ad_io *io;
    uint32_t (*now_us)(void *);
    void *clock_user;
    uint32_t session, generation, sequence, pending, pending_generation;
    uint32_t tx, ty, kind, started, transfer_us;
    int failed;
};
int zc_init(struct zc_client *,volatile uint8_t *,size_t,const struct ad_io *,uint32_t (*)(void *),void *);
int zc_cancel(struct zc_client *); /* advance generation; still drain outstanding job */
int zc_submit(struct zc_client *,const struct ff_view *,uint32_t,uint32_t,int clock_only);
int zc_poll(struct zc_client *,struct zc_result *);
#endif
