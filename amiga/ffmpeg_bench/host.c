#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include "bench.h"
struct Device *TimerBase;
static const char *input;
int zv_arguments(int argc,char **argv){if(argc!=2){puts("Usage: ZZFFmpegBench clip.m1v");return 1;}input=argv[1];return 0;}
static uint64_t get64(const volatile uint8_t *b,unsigned o){return (uint64_t)ad_get(b,o)|((uint64_t)ad_get(b,o+4)<<32);}
static uint64_t hostclock(void){struct EClockVal e;ReadEClock(&e);return ((uint64_t)e.ev_hi<<32)|e.ev_lo;}
static int request(volatile uint8_t *b,const struct ad_io *io,unsigned seq,unsigned size,unsigned mode,unsigned ih) {
 unsigned n,retries=0;
 ad_put(b,FB_REQUEST+4,size);ad_put(b,FB_REQUEST+8,mode);ad_put(b,FB_REQUEST+12,ih);
 io->push(b+FB_REQUEST,64,0);ad_put(b,FB_REQUEST,seq);io->push(b+FB_REQUEST,64,0);
 for(n=0;n<3000;n++){
  uint32_t count,hash;
  if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)return -1;
  io->pull(b+FB_RESULT,64,0);
  if(ad_get(b,FB_RESULT)==seq){
   count=ad_get(b,FB_RESULT+12);if(count>FB_MAX_FRAMES)return -2;
   io->pull(b+FB_HASHES,count*4,0);
   hash=fb_hash(ad_get(b,ZZ_CONTROL)^seq,(const void *)(b+FB_RESULT+4),44);
   hash=fb_hash(hash,(const void *)(b+FB_HASHES),count*4);
   if(hash==ad_get(b,FB_RESULT+48)&&ad_get(b,FB_RESULT+4)==FB_MAGIC&&ad_get(b,FB_RESULT+44)==mode){
    printf("RESULT seq=%u mode=%u rc=%ld frames=%lu retries=%u\n",seq,mode,(long)(int32_t)ad_get(b,FB_RESULT+8),(unsigned long)count,retries);return 0;
   }retries++;
  }
  Delay(1);
 }io->pull(b+FB_HASHES+512,8,0);printf("TIMEOUT stage=%lu frames=%lu\n",(unsigned long)ad_get(b,FB_HASHES+512),(unsigned long)ad_get(b,FB_HASHES+516));return -3;
}
int zv_window(volatile uint8_t *b,const struct ad_io *io,int connected){
 struct MsgPort *port=0;struct timerequest *tr=0;struct EClockVal ec;FILE *f=0;
 long n;uint32_t ih,seq=0,hz,mode,rep,i;uint64_t h0,h1,a0,a1;double armhz;int rc=20,opened=0;
 (void)connected;
 f=fopen(input,"rb");if(!f)goto done;fseek(f,0,SEEK_END);n=ftell(f);rewind(f);
 if(n<=0||n>(long)FB_INPUT_MAX-64)goto done;
 if(fread((void *)(b+FB_INPUT),1,(size_t)n,f)!=(size_t)n)goto done;
 fclose(f);f=0;memset((void *)(b+FB_INPUT+n),0,64);ih=fb_hash(2166136261u,(const void *)(b+FB_INPUT),(size_t)n);io->push(b+FB_INPUT,(size_t)n+64,0);
 port=CreateMsgPort();if(!port)goto done;tr=(struct timerequest *)CreateIORequest(port,sizeof(*tr));if(!tr)goto done;
 if(OpenDevice(TIMERNAME,UNIT_MICROHZ,(struct IORequest *)tr,0))goto done;
 opened=1;TimerBase=tr->tr_node.io_Device;hz=ReadEClock(&ec);
 if(request(b,io,++seq,0,2,0))goto done;
 h0=hostclock();a0=get64(b,FB_RESULT+16);
 Delay(50);
 if(request(b,io,++seq,0,2,0))goto done;
 h1=hostclock();a1=get64(b,FB_RESULT+16);
 if(!hz||h1<=h0||a1<=a0)goto done;
 armhz=(double)(a1-a0)*hz/(double)(h1-h0);
 printf("CLOCK estimated_hz=%.0f eclock_hz=%lu input_bytes=%ld input_hash=%08lx\n",armhz,(unsigned long)hz,n,(unsigned long)ih);fflush(stdout);
 for(rep=0;rep<3;rep++)for(mode=0;mode<2;mode++){
  uint32_t frames;double decode,parse;
  printf("START run=%u mode=%s\n",rep+1,mode?"neon":"c");fflush(stdout);
  if(request(b,io,++seq,(unsigned)n,mode,ih)||ad_get(b,FB_RESULT+8))goto done;
  frames=ad_get(b,FB_RESULT+12);if(frames!=25)goto done;
  decode=(double)get64(b,FB_RESULT+16)/armhz;parse=(double)get64(b,FB_RESULT+24)/armhz;
  printf("BENCH run=%u mode=%s frames=%lu decode_ms=%.3f decode_fps=%.3f parse_ms=%.3f total_worker_ms=%.3f heap_peak=%lu\n",rep+1,mode?"neon":"c",(unsigned long)frames,decode*1000.0/frames,frames/decode,parse*1000.0/frames,(double)get64(b,FB_RESULT+32)*1000.0/armhz,(unsigned long)ad_get(b,FB_RESULT+40));
  for(i=0;i<frames;i++)printf("HASH run=%u mode=%u frame=%u fnv=%08lx\n",rep+1,mode,i+1,(unsigned long)ad_get(b,FB_HASHES+i*4));
  fflush(stdout);
 }
 rc=0;
 done:if(f)fclose(f);if(opened)CloseDevice((struct IORequest *)tr);if(tr)DeleteIORequest((struct IORequest *)tr);if(port)DeleteMsgPort(port);printf("BENCHMARK exit=%d\n",rc);return rc;
}
