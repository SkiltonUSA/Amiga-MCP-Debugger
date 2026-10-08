/* Native software fixture: NEVER evidence of ARM or Amiga hardware execution.
 * Uses the same core and relay as the target builds, in one host process. */
#include "protocol.h"
#include "relay.h"
static uint8_t page[AD_PAGE_SIZE] __attribute__((aligned(64)));
static struct ad_core core;
static struct ad_relay relay;
static uint8_t memory[128];
static uint32_t next_point;
static void range(volatile void *p,size_t n,void *u){(void)p;(void)n;(void)u;}
static void barrier(void *u){(void)u;__asm__ volatile("":::"memory");}
static const struct ad_io io={range,range,barrier,barrier,0};
int demo_reset(uint32_t session) {
    unsigned i;next_point=1;
    for(i=0;i<sizeof(memory);i++)memory[i]=(uint8_t)i;
    if(ad_init(&core,page,session,0x53495831u,AD_FEATURE_HOST_DEMO,&io))return -1;
    if(ad_add_region(&core,memory,sizeof(memory))!=0)return -1;
    ad_log(&core,"Host simulation ready; this is not ARM execution");
    return ad_relay_init(&relay,page,&io);
}
void demo_service(void){ad_service(&core);}
void demo_tick(void) {
    uint32_t values[2];ad_service(&core);
    if(core.state!=AD_RUNNING)return;
    values[0]=core.hits;values[1]=core.hits*7;
    ad_enter(&core,next_point,values,2);
    next_point=next_point==1?2:1;
}
int demo_hook(const char *args,char *out,int cap){return ad_relay_call(&relay,args,out,cap);}
void demo_log(const char *s){ad_log(&core,s);}
void demo_fault(void){ad_fault(&core,0xdabu,0x30001004u,0x30002000u,0x30001008u,0x600001d3u);}
void demo_finish(void){ad_finish(&core);}
/* Test-only fault injection, kept in this explicitly labelled host fixture. */
void demo_corrupt(unsigned offset,uint32_t value){if(offset<AD_PAGE_SIZE&&!(offset&3))ad_put(page,offset,value);}
