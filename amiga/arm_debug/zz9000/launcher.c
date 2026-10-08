/* XX19c-only proof launcher. AllocAbs reserves real Exec free memory in the
 * ZZ9000 Fast RAM bank. No memory-map gap is assumed to be free.
 * A sub-second P96 bootstrap first verifies the legacy address translation.
 * RUN is explicit: the caller must establish that no other Core1 app runs. */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <libraries/expansion.h>
#include <graphics/gfx.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/expansion.h>
#include <proto/intuition.h>
#include <proto/Picasso96.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "relay.h"
#include "layout.h"
#include "bridge_client.h"
#include "zz_payload.h"
#ifdef ZZ_FRACTAL
int ff_window(volatile uint8_t *,const struct ad_io *,int);
#endif
struct ExpansionBase *ExpansionBase;
struct IntuitionBase *IntuitionBase;
struct Library *P96Base;
static volatile UBYTE *board;
static void sync_range(volatile void *p,size_t n,void *u)
{ (void)u; CacheClearE((APTR)p,(ULONG)n,CACRF_ClearD); }
static void barrier(void *u) { (void)u; __asm__ volatile("nop" ::: "memory"); }
static void idle(void *u) { (void)u; Delay(1); }
static const struct ad_io io={sync_range,sync_range,barrier,idle,0};
static void regwrite(unsigned o,ULONG v) { *(volatile UWORD *)(board+o)=(UWORD)v; }
static void run_arm(ULONG pc,ULONG arg0,ULONG arg1)
{
    regwrite(0x94,2);
    regwrite(0x96,arg0>>16);regwrite(0x98,arg0);
    regwrite(0x9a,arg1>>16);regwrite(0x9c,arg1);
    regwrite(0x90,pc>>16);regwrite(0x92,pc);
    /* Firmware finishes arm_app_run before completing the Zorro command. */
    (void)*(volatile UWORD *)(board+0xc0);
}
static UBYTE *reserve(ULONG low,ULONG high)
{
    struct MemHeader *mh;struct MemChunk *mc;UBYTE *p=0;
    Forbid();
    for(mh=(struct MemHeader *)SysBase->MemList.lh_Head;
        mh->mh_Node.ln_Succ&&!p;mh=(struct MemHeader *)mh->mh_Node.ln_Succ) {
        if((ULONG)mh->mh_Lower<low || (ULONG)mh->mh_Upper>high)continue;
        for(mc=mh->mh_First;mc;mc=mc->mc_Next) {
            ULONG start=((ULONG)mc+63u)&~63u;
            if(start<(ULONG)mc || mc->mc_Bytes<start-(ULONG)mc ||
               mc->mc_Bytes-(start-(ULONG)mc)<ZZ_BLOCK_SIZE)continue;
            p=AllocAbs(ZZ_BLOCK_SIZE,(APTR)start);
            if(p)break;
        }
    }
    Permit();return p;
}
static void leput(UBYTE *p,ULONG n)
{p[0]=n;p[1]=n>>8;p[2]=n>>16;p[3]=n>>24;}
static ULONG leget(const UBYTE *p)
{return (ULONG)p[0]|((ULONG)p[1]<<8)|((ULONG)p[2]<<16)|((ULONG)p[3]<<24);}

static int mapping_probe(UBYTE *owned,ULONG candidate,ULONG nonce)
{
    struct Screen *screen=0;struct BitMap *bm=0;struct RenderInfo ri;
    LONG lock=0;int locked=0,ok=0;ULONG arm,i,expected[8],status=0;
    /* Flush old ARM cache lines before filling any reused graphics storage. */
    run_arm(0,0,0);
    screen=LockPubScreen(NULL);
    if(!screen)goto done;
    bm=p96AllocBitMap(512,8,8,BMF_DISPLAYABLE,screen->RastPort.BitMap,RGBFB_CLUT);
    UnlockPubScreen(NULL,screen);screen=0;
    if(!bm)goto done;
    lock=p96LockBitMap(bm,(UBYTE *)&ri,sizeof(ri));locked=1;
    /* A lock token is opaque: verify the returned location, not token bits. */
    if(!ri.Memory || ((ULONG)ri.Memory&3u) || ri.BytesPerRow<512 || !p96GetBitMapAttr(bm,P96BMA_ISONBOARD) ||
       (ULONG)ri.Memory<(ULONG)board+0x10000 ||
       (ULONG)ri.Memory+4096>(ULONG)board+0x4000000)goto unlock;
    arm=(ULONG)ri.Memory-(ULONG)board+0x1f0000;
    printf("BOOTSTRAP amiga=%08x arm=%08x (P96 allocated/locked)\n",(ULONG)ri.Memory,arm);
    memset(ri.Memory,0,4096);memcpy(ri.Memory,zz_mapping_probe,sizeof(zz_mapping_probe));
    for(i=0;i<8;i++){nonce=nonce*1664525u+1013904223u;expected[i]=nonce;ad_put(owned,i*4,nonce);}
    sync_range(owned,64,0);sync_range(ri.Memory,4096,0);
    run_arm(arm,arm,candidate);
    /* 20 ticks maximum, plus firmware reset/flush latency; no long P96 lock. */
    for(i=0;i<20;i++) {
        sync_range((UBYTE *)ri.Memory+1024,64,0);
        status=ad_get(ri.Memory,1024);
        if(status==0x4d415031u)break;
        Delay(1);
    }
    printf("MAP status=%08x SCTLR=%08x MIDR=%08x MPIDR=%08x\n",status,
           ad_get(ri.Memory,1028),ad_get(ri.Memory,1032),ad_get(ri.Memory,1036));
    ok=status==0x4d415031u && !(ad_get(ri.Memory,1028)&0x1005u) &&
        (ad_get(ri.Memory,1036)&255u)==1;
    for(i=0;i<8;i++)if(ad_get(ri.Memory,1040+i*4)!=expected[i])ok=0;
    run_arm(0,0,0); /* quiesce before P96 may move/free the code */
unlock:
    if(locked)p96UnlockBitMap(bm,lock);
done:
    if(bm)p96FreeBitMap(bm);
    if(screen)UnlockPubScreen(NULL,screen);
    printf("MAPPING %s (eight nonce-derived words)\n",ok?"PASS":"FAIL");fflush(stdout);
    return ok;
}

int main(int argc,char **argv)
{
    struct ConfigDev *gfx=0,*ram=0;struct MsgPort *owner=0;
    UBYTE *mem=0;ULONG arm=0,nonce,i,echoes=0;
#ifndef ZZ_FRACTAL
    ULONG tick,challenge=0;
#endif
    char *end;const char *mode="RUN";int rc=20,connected=0,bound=0,launched=0,stopping=0;
#ifdef ZZ_FRACTAL
    if(argc==1) {
        struct DateStamp stamp;
        DateStamp(&stamp);
        nonce=((ULONG)stamp.ds_Days*4320000u+(ULONG)stamp.ds_Minute*3000u+(ULONG)stamp.ds_Tick)^(ULONG)FindTask(NULL);
        if(!nonce)nonce=1;
    } else {
#endif
    if(argc!=3 || (strcmp(argv[2],"MAP")&&strcmp(argv[2],"RUN"))) {
        puts("Usage: zzarm-debug <fresh-hex-session> MAP|RUN (XX19c, Core1 idle required)");return 20;
    }
    errno=0;nonce=strtoul(argv[1],&end,16);
    if(errno||!nonce||*end||end==argv[1]||argv[1][0]=='-')return 20;
    mode=argv[2];
#ifdef ZZ_FRACTAL
    }
#endif
    ExpansionBase=(struct ExpansionBase *)OpenLibrary("expansion.library",0);
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",0);
    P96Base=OpenLibrary("Picasso96API.library",2);
    if(!ExpansionBase||!IntuitionBase||!P96Base)goto done;
    gfx=FindConfigDev(NULL,0x6d6e,4);ram=FindConfigDev(NULL,0x6d6e,5);
    if(!gfx||!ram||ram->cd_BoardSize!=0x10000000)goto done;
    board=gfx->cd_BoardAddr;
    if(*(volatile UWORD *)(board+0xc0)!=0x0113)goto done;
    owner=CreateMsgPort();if(!owner)goto done;
    owner->mp_Node.ln_Name="Sixies.ARM.Debug.Owner";
    Forbid();
    if(FindPort(owner->mp_Node.ln_Name)){Permit();DeleteMsgPort(owner);owner=0;goto done;}
    AddPort(owner);Permit();
    mem=reserve((ULONG)ram->cd_BoardAddr,(ULONG)ram->cd_BoardAddr+ram->cd_BoardSize);
    if(!mem)goto done;
    /* Legacy XX19c translation: RTL uses z3addr-z3_ram_low + ARM_MEMORY_START.
     * This is a CANDIDATE until the read-only nonce test proves it. */
    arm=(ULONG)mem-(ULONG)board+0x1f0000;
    if(arm<0x10000000 || arm+ZZ_BLOCK_SIZE>0x201f0000)goto done;
    printf("OWNED amiga=%08x arm_candidate=%08x size=%u nonce=%08x\n",(ULONG)mem,arm,(ULONG)ZZ_BLOCK_SIZE,nonce);
    if(!mapping_probe(mem,arm,nonce))goto done;
    if(!strcmp(mode,"MAP")){rc=0;goto done;}
    if(sizeof(zz_image)>ZZ_CONTROL)goto done;
    memset(mem,0,ZZ_BLOCK_SIZE);memcpy(mem,zz_image,sizeof(zz_image));
    for(i=0;i<sizeof(zz_relocations)/sizeof(zz_relocations[0]);i++) {
        ULONG off=zz_relocations[i];
        if(off+4>sizeof(zz_image))goto done;
        leput(mem+off,leget(mem+off)+arm);
    }
    ad_put(mem,ZZ_CONTROL,nonce);
    sync_range(mem,ZZ_BLOCK_SIZE,0);
    run_arm(arm+ZZ_ENTRY,arm,0);launched=1;
    for(i=0;i<100;i++) {
        sync_range(mem+ZZ_DIAG,64,0);
        if(ad_get(mem,ZZ_DIAG)==ZZ_READY)break;
        Delay(1);
    }
    printf("ARM ready=%08x SCTLR=%08x MIDR=%08x MPIDR=%08x\n",ad_get(mem,ZZ_DIAG),
           ad_get(mem,ZZ_DIAG+4),ad_get(mem,ZZ_DIAG+8),ad_get(mem,ZZ_DIAG+12));fflush(stdout);
    if(ad_get(mem,ZZ_DIAG)!=ZZ_READY)goto done;
#ifdef ZZ_FRACTAL
    if(!ab_init("zzfractal")) {
        connected=1;
        if(ad_bridge_bind(mem+ZZ_PAGE,&io))goto done;
        bound=1;
    }
    rc=ff_window(mem,&io,connected);
#else
    if(ab_init("zzarm-debug"))goto done;
    connected=1;
    if(ad_bridge_bind(mem+ZZ_PAGE,&io))goto done;
    bound=1;
    for(tick=0;tick<15000;tick++) {
        if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)break;
        ab_poll();
        if(tick%25==0) {
            sync_range(mem+ZZ_DIAG,64,0);
            if(challenge && ad_get(mem,ZZ_DIAG+16)!=(challenge^ZZ_XOR)) {
                puts("CACHE ECHO FAIL");goto done;
            }
            if(challenge)echoes++;
            challenge=nonce+tick+1;ad_put(mem,ZZ_CHALLENGE,challenge);
            sync_range(mem+ZZ_STOP,64,0);
        }
        if(tick%50==0)ab_heartbeat();
        Delay(1);
    }
    rc=0;
#endif
done:
    if(bound)ad_bridge_unbind();
    if(connected)ab_cleanup();
    if(launched) {
        ad_put(mem,ZZ_STOP,1);sync_range(mem+ZZ_STOP,64,0);
        for(i=0;i<100;i++) {
            sync_range(mem+ZZ_DIAG,64,0);
            if(ad_get(mem,ZZ_DIAG+24)==ZZ_RETURNED){stopping=1;break;}
            Delay(1);
        }
        printf("EXIT epilogue=%08x cache_echoes=%u rounds=%u\n",ad_get(mem,ZZ_DIAG+24),echoes,ad_get(mem,ZZ_DIAG+20));
        /* Synchronous XX19c reset-to-idle makes releasing shared code safe,
         * including timeout/failure. Does not change Core0 or firmware image. */
        run_arm(0,0,0);
        if(!stopping)rc=20;
    }
    if(mem)FreeMem(mem,ZZ_BLOCK_SIZE);
    if(owner){RemPort(owner);DeleteMsgPort(owner);}
    if(P96Base)CloseLibrary(P96Base);
    if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);
    if(ExpansionBase)CloseLibrary((struct Library *)ExpansionBase);
    printf("LAUNCHER exit=%d; owned memory released\n",rc);return rc;
}
