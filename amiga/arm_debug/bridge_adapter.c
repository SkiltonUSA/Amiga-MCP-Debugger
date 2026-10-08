#include "relay.h"
#include "bridge_client.h"
static struct ad_relay relay;
static int bound;
static int hook(const char *args,char *out,int cap) {
    if(!bound)return -1;
    return ad_relay_call(&relay,args,out,cap);
}
int ad_bridge_bind(volatile void *page,const struct ad_io *io) {
    if(bound||!ab_is_connected()||ad_relay_init(&relay,page,io))return -1;
    bound=1;
    ab_register_hook("arm_debug","Cooperative ARM debug ABI 1; bounded app-owned channel",hook);
    return 0;
}
void ad_bridge_unbind(void) {
    if(bound)ab_unregister_hook("arm_debug");
    bound=0;relay.page=0;
}
