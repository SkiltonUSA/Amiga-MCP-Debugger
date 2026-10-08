#include "fractal.h"
void ff_default(struct ff_view *v)
{ v->cx=-FF_ONE*3/4;v->cy=0;v->step=FF_ONE*3/(int32_t)FF_WIDTH;v->limit=FF_LIMIT; }
int ff_valid(const struct ff_view *v,uint32_t tx,uint32_t ty)
{
    return v && v->cx>=-3*FF_ONE && v->cx<=3*FF_ONE &&
        v->cy>=-3*FF_ONE && v->cy<=3*FF_ONE && v->step>=2 &&
        v->step<=FF_ONE*3/(int32_t)FF_WIDTH && v->limit>=1 && v->limit<=256 &&
        tx<FF_WIDTH && ty<FF_HEIGHT && tx%FF_TW==0 && ty%FF_TH==0;
}
int ff_zoom(struct ff_view *v,uint32_t px,uint32_t py)
{
    struct ff_view n=*v;
    if(px>=FF_WIDTH||py>=FF_HEIGHT||v->step<4)return 0;
    n.cx+=((int32_t)px-(int32_t)FF_WIDTH/2)*v->step;
    n.cy+=((int32_t)py-(int32_t)FF_HEIGHT/2)*v->step;
    n.step/=2;
    if(!ff_valid(&n,0,0))return 0;
    *v=n;return 1;
}
static void point(struct ff_cursor *c)
{
    c->cr=c->view.cx+((int32_t)(c->tx+c->pixel%FF_TW)-(int32_t)FF_WIDTH/2)*c->view.step;
    c->ci=c->view.cy+((int32_t)(c->ty+c->pixel/FF_TW)-(int32_t)FF_HEIGHT/2)*c->view.step;
    c->x=c->y=0;c->iteration=0;
}
void ff_begin(struct ff_cursor *c,const struct ff_view *v,uint32_t tx,uint32_t ty)
{ c->view=*v;c->tx=tx;c->ty=ty;c->pixel=0;point(c); }
int ff_step(struct ff_cursor *c,uint16_t *out,uint32_t budget)
{
    while(budget-- && c->pixel<FF_PIXELS) {
        int32_t xx=0,yy=0;
        int escaped=c->x>2*FF_ONE||c->x< -2*FF_ONE||c->y>2*FF_ONE||c->y< -2*FF_ONE;
        if(!escaped) {
            xx=(c->x*c->x)/FF_ONE; yy=(c->y*c->y)/FF_ONE;
            escaped=xx+yy>4*FF_ONE;
        }
        if(escaped||c->iteration>=c->view.limit) {
            out[c->pixel++]=(uint16_t)c->iteration;
            if(c->pixel<FF_PIXELS)point(c);
        } else {
            c->y=2*((c->x*c->y)/FF_ONE)+c->ci;
            c->x=xx-yy+c->cr;c->iteration++;
        }
    }
    return c->pixel==FF_PIXELS;
}
/* Endian-independent FNV-1a of big-endian 16-bit iteration counts. */
uint32_t ff_hash(uint32_t h,uint16_t v)
{ h=(h^(v>>8))*16777619u;return (h^(v&255))*16777619u; }
