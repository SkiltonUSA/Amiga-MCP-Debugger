#include <stddef.h>
void *memcpy(void *d,const void *s,size_t n)
{unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}
void *memmove(void *d,const void *s,size_t n)
{unsigned char *a=d;const unsigned char *b=s;if(a<b){while(n--)*a++=*b++;}else{a+=n;b+=n;while(n--)*--a=*--b;}return d;}
void *memset(void *d,int v,size_t n)
{unsigned char *a=d;while(n--)*a++=(unsigned char)v;return d;}
/* Compiler-generated aggregate copies on bare-metal ARM EABI. */
void __aeabi_memcpy(void *d,const void *s,size_t n) {memcpy(d,s,n);}
void __aeabi_memcpy4(void *d,const void *s,size_t n) {memcpy(d,s,n);}
void __aeabi_memcpy8(void *d,const void *s,size_t n) {memcpy(d,s,n);}
int abs(int n) {return n<0?-n:n;}
/* Cortex-A9 has no ARM-state integer divide instruction. Unsigned restoring
 * division uses no runtime or VFP state; caller's MPEG macroblock width > 0. */
int __aeabi_idiv(int a,int b)
{
    unsigned n=a<0?0u-(unsigned)a:(unsigned)a;
    unsigned d=b<0?0u-(unsigned)b:(unsigned)b,q=0,r=0;int i;
    if(!d)return 0;
    for(i=31;i>=0;i--) {
        unsigned carry=r>>31;r=(r<<1)|((n>>(unsigned)i)&1u);
        if(carry||r>=d){r-=d;q|=1u<<(unsigned)i;}
    }
    return (int)(((a<0)!=(b<0))?0u-q:q);
}
