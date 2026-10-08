#ifndef ZZ_FRACTAL_H
#define ZZ_FRACTAL_H
#include <stdint.h>
#define FF_WIDTH 320u
#define FF_HEIGHT 240u
#define FF_TW 32u
#define FF_TH 16u
#define FF_PIXELS (FF_TW*FF_TH)
#define FF_TILES ((FF_WIDTH/FF_TW)*(FF_HEIGHT/FF_TH))
#define FF_ONE 16384
#define FF_LIMIT 128u
/* Q14. Every signed division truncates toward zero. No negative shifts.
 * Component escape checks bound each signed 32-bit multiplication. */
struct ff_view { int32_t cx, cy, step; uint32_t limit; };
struct ff_cursor {
    struct ff_view view;
    uint32_t tx,ty,pixel,iteration;
    int32_t x,y,cr,ci;
};
void ff_default(struct ff_view *);
int ff_valid(const struct ff_view *,uint32_t,uint32_t);
int ff_zoom(struct ff_view *,uint32_t,uint32_t);
void ff_begin(struct ff_cursor *,const struct ff_view *,uint32_t,uint32_t);
/* At most budget orbit steps/escape checks; returns 1 when tile complete. */
int ff_step(struct ff_cursor *,uint16_t *,uint32_t budget);
uint32_t ff_hash(uint32_t,uint16_t);
#endif
