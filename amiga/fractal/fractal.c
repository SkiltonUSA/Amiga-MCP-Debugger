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
/* Keep the current orbit in locals across a bounded slice. Shared DDR is
 * uncached on the ARM, so reloading cursor fields in the inner loop is costly.
 * The cursor and output tile are disjoint caller-owned objects.
 */
static int advance(struct ff_cursor *c,uint16_t *out,uint32_t budget,uint32_t stop)
{
    uint32_t pixel=c->pixel,n=c->iteration;
    int32_t x=c->x,y=c->y,cr=c->cr,ci=c->ci;
    const uint32_t limit=c->view.limit;
    while(budget-- && pixel<stop) {
        int32_t xx=0,yy=0;
        int escaped=x>2*FF_ONE||x< -2*FF_ONE||y>2*FF_ONE||y< -2*FF_ONE;
        if(!escaped) {
            /* Escape bounds make these products nonnegative and <= 2^30.
             * Unsigned shifts are exact Q14 division here; xy below remains
             * signed division to preserve truncation toward zero. */
            xx=(int32_t)((uint32_t)(x*x)>>14);
            yy=(int32_t)((uint32_t)(y*y)>>14);
            escaped=xx+yy>4*FF_ONE;
        }
        if(escaped||n>=limit) {
            out[pixel++]=(uint16_t)n;
            if(pixel<FF_PIXELS) {
                cr=c->view.cx+((int32_t)(c->tx+pixel%FF_TW)-(int32_t)FF_WIDTH/2)*c->view.step;
                ci=c->view.cy+((int32_t)(c->ty+pixel/FF_TW)-(int32_t)FF_HEIGHT/2)*c->view.step;
                x=y=0;n=0;
            }
        } else {
            int32_t nx=xx-yy+cr,ny=2*((x*y)/FF_ONE)+ci;
            ++n;
            /* Exact fixed-point recurrence: a repeated state can never escape.
             * This is an integer equality proof, not an approximate interior
             * test. It only shortcuts points that would return the limit. */
            if(nx==x&&ny==y)n=limit;
            x=nx;y=ny;
        }
    }
    c->pixel=pixel;c->iteration=n;c->x=x;c->y=y;c->cr=cr;c->ci=ci;
    return pixel==FF_PIXELS;
}
int ff_step(struct ff_cursor *c,uint16_t *out,uint32_t budget)
{return advance(c,out,budget,FF_PIXELS);}
int ff_step_row(struct ff_cursor *c,uint16_t *out,uint32_t budget)
{
    uint32_t stop=(c->pixel/FF_TW+1)*FF_TW;
    if(stop>FF_PIXELS)stop=FF_PIXELS;
    return advance(c,out,budget,stop);
}
/* Endian-independent FNV-1a of big-endian 16-bit iteration counts. */
uint32_t ff_hash(uint32_t h,uint16_t v)
{ h=(h^(v>>8))*16777619u;return (h^(v&255))*16777619u; }
