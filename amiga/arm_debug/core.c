#include "protocol.h"
static void begin(struct ad_core *c) {
    ad_put(c->page, AD_O_SEQUENCE, ++c->sequence);
    c->io.push(c->page+AD_O_SEQUENCE,4,c->io.user);
    c->io.barrier(c->io.user);
}
static void end(struct ad_core *c) {
    unsigned i;
    ad_put(c->page,AD_O_STATE,c->state); ad_put(c->page,AD_O_REASON,c->reason);
    ad_put(c->page,AD_O_POINT,c->point); ad_put(c->page,AD_O_HITS,c->hits);
    ad_put(c->page,AD_O_ACK,c->ack); ad_put(c->page,AD_O_RESULT,c->result);
    ad_put(c->page,AD_O_VALUE_COUNT,c->value_count);
    ad_put(c->page,AD_O_BP_COUNT,c->bp_count);
    ad_put(c->page,AD_O_REGION_COUNT,c->region_count);
    ad_put(c->page,AD_O_LOG_COUNT,c->logs);
    ad_put(c->page,AD_O_PAUSE_PENDING,c->pause_pending);
    for(i=0;i<AD_MAX_VALUES;i++) ad_put(c->page,AD_O_VALUES+i*4,c->values[i]);
    for(i=0;i<AD_MAX_BREAKPOINTS;i++) ad_put(c->page,AD_O_BREAKPOINTS+i*4,c->bp[i]);
    /* Never publish the host-owned mailbox cache line. */
    c->io.push(c->page,AD_STATUS_SIZE,c->io.user);
    c->io.push(c->page+AD_LOG_BASE,AD_PAGE_SIZE-AD_LOG_BASE,c->io.user);
    c->io.barrier(c->io.user);
    ad_put(c->page,AD_O_SEQUENCE,++c->sequence);
    c->io.push(c->page+AD_O_SEQUENCE,4,c->io.user);
    c->io.barrier(c->io.user);
}
int ad_init(struct ad_core *c,volatile void *page,uint32_t session,
            uint32_t build_id,uint32_t features,const struct ad_io *io) {
    unsigned i; uint8_t *bytes=(uint8_t *)c;
    if(!c||!page||((uintptr_t)page&63)||!session||!io||!io->pull||
       !io->push||!io->barrier||!io->idle) return -1;
    for(i=0;i<sizeof(*c);i++) bytes[i]=0;
    c->page=(volatile uint8_t *)page;c->io=*io;c->session=session;
    c->state=AD_RUNNING;c->features=features|AD_FEATURE_COOPERATIVE;
    /* Init is only legal before the host relay is exposed. */
    for(i=0;i<AD_PAGE_SIZE;i++)c->page[i]=0;
    ad_put(c->page,AD_O_MAGIC,AD_MAGIC);ad_put(c->page,AD_O_ABI,AD_ABI);
    ad_put(c->page,AD_O_SESSION,session);ad_put(c->page,AD_O_BUILD,build_id);
    ad_put(c->page,AD_O_FEATURES,c->features);
    c->io.push(c->page+AD_MAILBOX,64,c->io.user);
    begin(c);end(c);return 0;
}
int ad_add_region(struct ad_core *c,const void *data,uint32_t size) {
    unsigned n=c->region_count;
    if(!data||!size||n>=AD_MAX_REGIONS||c->hits||c->ack)return -1;
    c->regions[n].data=(const uint8_t *)data;c->regions[n].size=size;
    begin(c);c->region_count++;end(c);return (int)n;
}
void ad_service(struct ad_core *c) {
    uint32_t seq,session,cmd,a,b,n;unsigned i;
    c->io.pull(c->page+AD_MAILBOX,64,c->io.user);c->io.barrier(c->io.user);
    seq=ad_get(c->page,AD_M_SEQUENCE);
    if(seq==c->ack)return;
    /* Acquire the published commit before reading its command payload. */
    c->io.barrier(c->io.user);
    session=ad_get(c->page,AD_M_SESSION);cmd=ad_get(c->page,AD_M_OPCODE);
    a=ad_get(c->page,AD_M_ARG0);b=ad_get(c->page,AD_M_ARG1);n=ad_get(c->page,AD_M_ARG2);
    c->io.barrier(c->io.user);
    /* A stale/incomplete producer must not acknowledge or execute anything. */
    if(seq!=ad_get(c->page,AD_M_SEQUENCE)||session!=c->session||
       c->ack==UINT32_MAX||seq!=c->ack+1)return;
    begin(c);c->result=AD_OK;ad_put(c->page,AD_O_READ_SIZE,0);
    if(c->state==AD_FINISHED || (c->state==AD_FAULTED && cmd!=AD_CMD_READ))
        c->result=AD_BAD_STATE;
    else switch(cmd) {
    case AD_CMD_PAUSE:
        if(c->state==AD_RUNNING)c->pause_pending=1;
        break;
    case AD_CMD_CONTINUE:
        c->pause_pending=0;c->step_pending=0;c->state=AD_RUNNING;break;
    case AD_CMD_STEP:
        if(c->state!=AD_PAUSED){c->result=AD_BAD_STATE;break;}
        c->pause_pending=0;c->step_pending=1;c->state=AD_RUNNING;break;
    case AD_CMD_SET_BP:
        if(!a){c->result=AD_BAD_RANGE;break;}
        for(i=0;i<c->bp_count && c->bp[i]!=a;i++){}
        if(i==c->bp_count){
            if(i==AD_MAX_BREAKPOINTS)c->result=AD_FULL;
            else c->bp[c->bp_count++]=a;
        } break;
    case AD_CMD_CLEAR_BP:
        if(!a){for(i=0;i<AD_MAX_BREAKPOINTS;i++)c->bp[i]=0;c->bp_count=0;}
        else {
            for(i=0;i<c->bp_count;i++)if(c->bp[i]==a){
                c->bp[i]=c->bp[--c->bp_count];c->bp[c->bp_count]=0;break;
            }
        }
        break;
    case AD_CMD_READ:
        if(c->state!=AD_PAUSED&&c->state!=AD_FAULTED){c->result=AD_BAD_STATE;break;}
        if(a>=c->region_count||!n||n>AD_MAX_READ||b>c->regions[a].size||
           n>c->regions[a].size-b){c->result=AD_BAD_RANGE;break;}
        for(i=0;i<n;i++)c->page[AD_O_READ_DATA+i]=c->regions[a].data[b+i];
        ad_put(c->page,AD_O_READ_SIZE,n);break;
    case AD_CMD_DETACH:
        for(i=0;i<AD_MAX_BREAKPOINTS;i++)c->bp[i]=0;
        c->bp_count=0;c->pause_pending=0;c->step_pending=0;c->state=AD_RUNNING;break;
    default:c->result=AD_BAD_COMMAND;break;
    }
    c->ack=seq;end(c);
}
void ad_enter(struct ad_core *c,uint32_t point,const uint32_t *values,unsigned count) {
    unsigned i;ad_service(c);
    if(c->state!=AD_RUNNING||!point||count>AD_MAX_VALUES||(count&&!values))return;
    begin(c);c->point=point;c->hits++;c->value_count=count;
    for(i=0;i<AD_MAX_VALUES;i++)c->values[i]=i<count?values[i]:0;
    if(c->pause_pending||c->step_pending){
        c->state=AD_PAUSED;c->reason=c->step_pending?AD_STEP:AD_PAUSE;
        c->pause_pending=0;c->step_pending=0;
    } else for(i=0;i<c->bp_count;i++)if(c->bp[i]==point){
        c->state=AD_PAUSED;c->reason=AD_BREAKPOINT;break;
    }
    end(c);
}
void ad_checkpoint(struct ad_core *c,uint32_t point,const uint32_t *v,unsigned n) {
    ad_enter(c,point,v,n);
    while(c->state==AD_PAUSED){ad_service(c);c->io.idle(c->io.user);}
}
void ad_log(struct ad_core *c,const char *text) {
    unsigned i,o;if(!text||c->logs==UINT32_MAX)return;
    begin(c);o=AD_LOG_BASE+(c->logs%AD_LOG_SLOTS)*AD_LOG_SIZE;
    for(i=0;i<AD_LOG_TEXT && text[i];i++)c->page[o+8+i]=(uint8_t)text[i];
    ad_put(c->page,o+4,i);ad_put(c->page,o,++c->logs);end(c);
}
void ad_fault(struct ad_core *c,uint32_t code,uint32_t pc,uint32_t sp,uint32_t lr,uint32_t cpsr) {
    begin(c);c->state=AD_FAULTED;c->reason=AD_FAULT;
    ad_put(c->page,AD_O_FAULT,code);ad_put(c->page,AD_O_PC,pc);
    ad_put(c->page,AD_O_SP,sp);ad_put(c->page,AD_O_LR,lr);ad_put(c->page,AD_O_CPSR,cpsr);
    end(c);
}
void ad_finish(struct ad_core *c) {begin(c);c->state=AD_FINISHED;end(c);}
