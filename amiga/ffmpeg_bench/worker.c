/* Bounded FFmpeg MPEG-1 decode-only experiment; no display or OS calls. */
#include "bench.h"
#include "libavcodec/avcodec.h"
#include "libavutil/cpu.h"
#include "libavutil/log.h"
#include <string.h>
void fb_heap_init(void *,size_t);
size_t fb_heap_peak(void);
static volatile uint8_t *shared;
static uint64_t ticks(void) {
#ifdef FB_NATIVE
 return 0;
#else
 volatile uint32_t *t=(volatile uint32_t *)0xf8f00200u;uint32_t h,l;
 do{h=t[1];l=t[0];}while(h!=t[1]);return ((uint64_t)h<<32)|l;
#endif
}
static void put64(unsigned off,uint64_t x){ad_put(shared,off,(uint32_t)x);ad_put(shared,off+4,(uint32_t)(x>>32));}
static int frame_hash(AVFrame *f,unsigned number) {
 unsigned p,y;uint32_t h=2166136261u;
 if(number>=FB_MAX_FRAMES||f->width!=320||f->height!=240||f->format!=AV_PIX_FMT_YUV420P)return -1001;
 for(p=0;p<3;p++)for(y=0;y<(unsigned)(f->height>>(p!=0));y++)
  h=fb_hash(h,f->data[p]+y*f->linesize[p],(size_t)(f->width>>(p!=0)));
 ad_put(shared,FB_HASHES+number*4,h);return 0;
}
static void progress(unsigned stage,unsigned frames){ad_put(shared,FB_HASHES+512,stage);ad_put(shared,FB_HASHES+516,frames);
#ifndef FB_NATIVE
 __asm__ volatile("dsb sy":::"memory");
#endif
}
static int decode(unsigned size,int neon,unsigned *frames,uint64_t *decode_time,uint64_t *parse_time) {
 const AVCodec *codec;AVCodecParserContext *parser=0;AVCodecContext *ctx=0;AVFrame *f=0;AVPacket *pkt=0;
 uint8_t *in=(uint8_t *)shared+FB_INPUT;unsigned left=size;int rc=-1000,drained=0;uint64_t t,start=ticks();
 av_force_cpu_flags(neon?(AV_CPU_FLAG_ARMV5TE|AV_CPU_FLAG_ARMV6|AV_CPU_FLAG_ARMV6T2|AV_CPU_FLAG_VFP|AV_CPU_FLAG_VFPV3|AV_CPU_FLAG_NEON):0);
 #ifdef FB_NATIVE
 av_log_set_level(AV_LOG_DEBUG);
#else
 av_log_set_level(AV_LOG_QUIET);
#endif
 progress(1,0);codec=avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
 if(!codec)goto done;
 progress(2,0);parser=av_parser_init(AV_CODEC_ID_MPEG1VIDEO);progress(3,0);ctx=avcodec_alloc_context3(codec);progress(4,0);f=av_frame_alloc();progress(5,0);pkt=av_packet_alloc();
 if(!parser||!ctx||!f||!pkt)goto done;
 ctx->thread_count=1;ctx->idct_algo=FF_IDCT_SIMPLEAUTO;
 progress(6,0);if((rc=avcodec_open2(ctx,codec,0))<0)goto done;
 for(;;) {
  int got;
  if(ad_get(shared,ZZ_STOP)||ticks()-start>20000000000ULL){rc=-1002;goto done;}
  progress(7,*frames);t=ticks();rc=avcodec_receive_frame(ctx,f);*decode_time+=ticks()-t;
  if(rc==0){if((rc=frame_hash(f,*frames)))goto done;(*frames)++;av_frame_unref(f);continue;}
  if(rc==AVERROR_EOF){rc=0;break;}
  if(rc!=AVERROR(EAGAIN))goto done;
  if(drained){rc=-1003;goto done;}
  progress(8,*frames);t=ticks();got=av_parser_parse2(parser,ctx,&pkt->data,&pkt->size,in,(int)left,AV_NOPTS_VALUE,AV_NOPTS_VALUE,0);*parse_time+=ticks()-t;
  if(got<0){rc=got;goto done;}
  in+=got;left-=got;
  if(!pkt->size&&left){if(!got){rc=-1004;goto done;}continue;}
  progress(9,*frames);t=ticks();rc=avcodec_send_packet(ctx,pkt->size?pkt:0);*decode_time+=ticks()-t;
  if(!pkt->size)drained=1;
  if(rc<0)goto done;
 }
 done:
 progress(10,*frames);if(parser)av_parser_close(parser);
 av_packet_free(&pkt);av_frame_free(&f);avcodec_free_context(&ctx);return rc;
}
#ifndef FB_NATIVE
void zz_worker(volatile uint8_t *base) {
 uint32_t sctlr,midr,mpidr,last=0;shared=base;
 __asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(sctlr));
 __asm__ volatile("mrc p15,0,%0,c0,c0,0":"=r"(midr));
 __asm__ volatile("mrc p15,0,%0,c0,c0,5":"=r"(mpidr));
 ad_put(base,ZZ_DIAG+4,sctlr);ad_put(base,ZZ_DIAG+8,midr);ad_put(base,ZZ_DIAG+12,mpidr);
 if((sctlr&0x1005u)!=0x1000u||(mpidr&255u)!=1)return;
 ad_put(base,ZZ_DIAG,ZZ_READY);__asm__ volatile("dsb sy":::"memory");
 while(!ad_get(base,ZZ_STOP)) {
  uint32_t seq=ad_get(base,FB_REQUEST),size,mode,hash;unsigned count=0;uint64_t dt=0,pt=0,start;int rc=0;
  if(!seq||seq==last)continue;
  __asm__ volatile("dsb sy":::"memory");last=seq;size=ad_get(base,FB_REQUEST+4);mode=ad_get(base,FB_REQUEST+8);start=ticks();
  if(mode==2){put64(FB_RESULT+16,start);}
  else {
   if(!size||size>FB_INPUT_MAX-AV_INPUT_BUFFER_PADDING_SIZE)rc=-1005;
   else if(fb_hash(2166136261u,(const void *)(base+FB_INPUT),size)!=ad_get(base,FB_REQUEST+12))rc=-1006;
   else {fb_heap_init((void *)(base+FB_HEAP),FB_HEAP_SIZE);rc=decode(size,(int)mode,&count,&dt,&pt);}
   put64(FB_RESULT+16,dt);
  }
  put64(FB_RESULT+24,pt);put64(FB_RESULT+32,ticks()-start);
  ad_put(base,FB_RESULT+4,FB_MAGIC);ad_put(base,FB_RESULT+8,(uint32_t)rc);ad_put(base,FB_RESULT+12,count);
  ad_put(base,FB_RESULT+40,(uint32_t)fb_heap_peak());ad_put(base,FB_RESULT+44,mode);
  hash=fb_hash(ad_get(base,ZZ_CONTROL)^seq,(const void *)(base+FB_RESULT+4),44);
  hash=fb_hash(hash,(const void *)(base+FB_HASHES),count*4);
  ad_put(base,FB_RESULT+48,hash);__asm__ volatile("dsb sy":::"memory");ad_put(base,FB_RESULT,seq);__asm__ volatile("dsb sy":::"memory");
 }
}

#endif
