#include "media.h"
#include <string.h>
uint32_t zv_hash(uint32_t h,const void *data,size_t n)
{const uint8_t *p=data;while(n--)h=(h^*p++)*16777619u;return h;}
static int prefix(const uint8_t *p) {return !p[0]&&!p[1]&&p[2]==1;}
static int timestamp(const uint8_t *p,unsigned tag)
{return (p[0]>>4)==tag&&(p[0]&1)&&(p[2]&1)&&(p[4]&1);}
const char *zv_error(int c)
{
    switch(c) {
    case 0:return "OK";
    case -1:return "Input must be MPEG-1 PS or elementary video, 1 byte to 4 MiB";
    case -2:return "Truncated or malformed MPEG program stream";
    case -3:return "MPEG-2, MP4 and H.264 are not supported in this preview";
    case -4:return "Video must have 16-pixel-aligned dimensions, at most 320x240, constant size/rate";
    case -5:return "Missing MPEG-1 sequence or complete picture headers";
    default:return "Decoder allocation or sequence-header failure";
    }
}
int zv_prepare(uint8_t *d,size_t n,struct zv_info *v)
{
    size_t p=0,out=0,i;unsigned have=0,rate=0;
    static const uint32_t rates[]={0,24000,24,25,30000,30,50,60000,60};
    memset(v,0,sizeof(*v));
    if(!d||!n||n>ZV_MAX_INPUT||n<4||!prefix(d))return -1;
    if(d[3]==0xba) {
        while(p<n) {
            unsigned id;size_t end,q,len;
            if(n-p<4||!prefix(d+p))return -2;
            id=d[p+3];p+=4;
            if(id==0xb9) {if(p!=n)return -2;break;}
            if(id==0xba) {
                if(n-p<8)return -2;
                if((d[p]&0xf0)!=0x20)return -3;
                if(!(d[p]&1)||!(d[p+2]&1)||!(d[p+4]&1)||!(d[p+7]&1))return -2;
                p+=8;continue;
            }
            if(id!=0xbb&&id<0xbc)return -2;
            if(n-p<2)return -2;
            len=((size_t)d[p]<<8)|d[p+1];p+=2;
            if(!len||len>n-p)return -2;
            end=p+len;
            if(id>=0xe0&&id<=0xef) {
                if(id!=0xe0)return -3; /* one video stream in this milestone */
                q=p;
                while(q<end&&d[q]==0xff)q++;
                if(q==end)return -2;
                if((d[q]&0xc0)==0x40) {if(end-q<2)return -2;q+=2;}
                if(q==end)return -2;
                if((d[q]&0xf0)==0x20) {
                    if(end-q<5||!timestamp(d+q,2))return -2;
                    q+=5;
                } else if((d[q]&0xf0)==0x30) {
                    if(end-q<10||!timestamp(d+q,3)||!timestamp(d+q+5,1))return -2;
                    q+=10;
                } else if(d[q]==0x0f)q++;
                else return -3;
                memmove(d+out,d+q,end-q);out+=end-q;
            }
            p=end;
        }
        n=out;
    } else if(d[3]!=0xb3)return -1;
    for(i=0;i+4<=n;i++)if(prefix(d+i)) {
        unsigned id=d[i+3];
        if(id==0xb5)return -3; /* MPEG-2 extension */
        if(id==0xb3) {
            unsigned w,h,r;
            if(n-i<12)return -5;
            w=((unsigned)d[i+4]<<4)|(d[i+5]>>4);
            h=((unsigned)(d[i+5]&15)<<8)|d[i+6];r=d[i+7]&15;
            if(!w||!h||(w&15)||(h&15)||w>ZV_MAX_WIDTH||h>ZV_MAX_HEIGHT||!r||r>8)return -4;
            if(have&&(v->width!=w||v->height!=h||rate!=r))return -4;
            have=1;rate=r;v->width=w;v->height=h;
            v->rate_num=rates[r];v->rate_den=(r==1||r==4||r==7)?1001:1;
        } else if(id==0) {
            unsigned type;
            if(!have||n-i<8)return -5;
            type=(d[i+5]>>3)&7;if(type<1||type>3)return -5;
            v->pictures++;
        }
    }
    if(!have||!v->pictures)return -5;
    v->bytes=n;return 0;
}
