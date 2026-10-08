#include "relay.h"
static const char hex[]="0123456789abcdef";
static int fail(char *out,int cap,const char *msg) {
    int i=0;if(cap>0){while(msg[i]&&i<cap-1){out[i]=msg[i];i++;}out[i]=0;}return -1;
}
static int word(const char **s,uint32_t *out) {
    unsigned n=0,v;uint32_t x=0;
    while(**s==' ')(*s)++;
    while(**s && **s!=' '){
        char c=*(*s)++;
        if(c>='0'&&c<='9')v=(unsigned)(c-'0');
        else if(c>='a'&&c<='f')v=(unsigned)(c-'a'+10);
        else if(c>='A'&&c<='F')v=(unsigned)(c-'A'+10);
        else return 0;
        if(++n>8)return 0;
        x=(x<<4)|v;
    }
    *out=x;return n!=0;
}
static int tail(const char *s){while(*s==' ')s++;return *s==0;}
static void hexword(char *out,uint32_t x){unsigned i;for(i=0;i<8;i++)out[i]=hex[(x>>(28-4*i))&15];out[8]=0;}
int ad_relay_init(struct ad_relay *r,volatile void *page,const struct ad_io *io) {
    if(!r||!page||((uintptr_t)page&63)||!io||!io->pull||!io->push||!io->barrier)return -1;
    r->page=(volatile uint8_t *)page;r->io=*io;r->token=0;return 0;
}
int ad_relay_call(struct ad_relay *r,const char *args,char *out,int cap) {
    uint32_t a[6],seq,before,session,ack;unsigned i,tries;char op;
    if(!r||!args||!out||cap<10)return -1;
    op=*args++;if(*args && *args!=' ')return fail(out,cap,"BAD_REQUEST");
    if(op=='S'){
        if(!tail(args))return fail(out,cap,"BAD_REQUEST");
        for(tries=0;tries<4;tries++){
            r->io.pull(r->page,AD_PAGE_SIZE,r->io.user);r->io.barrier(r->io.user);
            before=ad_get(r->page,AD_O_SEQUENCE);if(before&1)continue;
            r->io.barrier(r->io.user);
            for(i=0;i<AD_PAGE_SIZE;i++)r->snapshot[i]=r->page[i];
            r->io.barrier(r->io.user);r->io.pull(r->page,64,r->io.user);
            if(before==ad_get(r->page,AD_O_SEQUENCE)){
                if(++r->token==0)++r->token;
                hexword(out,r->token);return 0;
            }
        }
        return fail(out,cap,"SNAPSHOT_BUSY");
    }
    if(op=='R'){
        for(i=0;i<3;i++)if(!word(&args,&a[i]))return fail(out,cap,"BAD_REQUEST");
        if(!tail(args))return fail(out,cap,"BAD_REQUEST");
        if(!r->token||a[0]!=r->token)return fail(out,cap,"STALE_SNAPSHOT");
        if(a[2]==0||a[2]>96||a[1]>AD_PAGE_SIZE||a[2]>AD_PAGE_SIZE-a[1]||
           a[2]*2+1>(uint32_t)cap)return fail(out,cap,"BAD_RANGE");
        for(i=0;i<a[2];i++){unsigned b=r->snapshot[a[1]+i];out[i*2]=hex[b>>4];out[i*2+1]=hex[b&15];}
        out[a[2]*2]=0;return 0;
    }
    if(op!='W')return fail(out,cap,"BAD_REQUEST");
    for(i=0;i<6;i++)if(!word(&args,&a[i]))return fail(out,cap,"BAD_REQUEST");
    if(!tail(args))return fail(out,cap,"BAD_REQUEST");
    r->io.pull(r->page,AD_MAILBOX+64,r->io.user);r->io.barrier(r->io.user);
    before=ad_get(r->page,AD_O_SEQUENCE);
    if(before&1)return fail(out,cap,"SNAPSHOT_BUSY");
    r->io.barrier(r->io.user);
    session=ad_get(r->page,AD_O_SESSION);ack=ad_get(r->page,AD_O_ACK);
    if(ad_get(r->page,AD_O_MAGIC)!=AD_MAGIC||ad_get(r->page,AD_O_ABI)!=AD_ABI)
        return fail(out,cap,"BAD_ABI");
    if(!session||a[0]!=session)return fail(out,cap,"BAD_SESSION");
    if(a[2]<AD_CMD_PAUSE||a[2]>AD_CMD_DETACH)return fail(out,cap,"BAD_COMMAND");
    seq=ad_get(r->page,AD_M_SEQUENCE);
    r->io.barrier(r->io.user);r->io.pull(r->page,64,r->io.user);
    if(before!=ad_get(r->page,AD_O_SEQUENCE))return fail(out,cap,"SNAPSHOT_BUSY");
    if(seq!=ack)return fail(out,cap,"COMMAND_PENDING");
    if(ack==UINT32_MAX||a[1]!=ack+1)return fail(out,cap,"BAD_SEQUENCE");
    ad_put(r->page,AD_M_SESSION,a[0]);ad_put(r->page,AD_M_OPCODE,a[2]);
    ad_put(r->page,AD_M_ARG0,a[3]);ad_put(r->page,AD_M_ARG1,a[4]);ad_put(r->page,AD_M_ARG2,a[5]);
    r->io.push(r->page+AD_MAILBOX,64,r->io.user);r->io.barrier(r->io.user);
    /* Sequence is the commit word, published only after the payload. */
    ad_put(r->page,AD_M_SEQUENCE,a[1]);
    r->io.push(r->page+AD_MAILBOX,64,r->io.user);r->io.barrier(r->io.user);
    out[0]='O';out[1]='K';out[2]=0;return 0;
}
