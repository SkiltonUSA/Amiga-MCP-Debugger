/* No operating system on Core1. Logging is disabled for this benchmark. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "libavutil/log.h"
void av_log(void *p,int level,const char *fmt,...){(void)p;(void)level;(void)fmt;}
void av_vlog(void *p,int level,const char *fmt,va_list ap){(void)p;(void)level;(void)fmt;(void)ap;}
void av_log_once(void *p,int initial,int subsequent,int *state,const char *fmt,...){(void)p;(void)initial;(void)subsequent;(void)fmt;*state=1;}
void av_log_set_level(int level){(void)level;}
int av_log_get_level(void){return AV_LOG_QUIET;}
const char *av_default_item_name(void *p){const AVClass *c=*(const AVClass **)p;return c?c->class_name:"FFmpeg";}
AVClassCategory av_default_get_category(void *p){const AVClass *c=*(const AVClass **)p;return c?c->category:AV_CLASS_CATEGORY_NA;}
unsigned long getauxval(unsigned long n){(void)n;return 0;}
long sysconf(int n){(void)n;return 1;}
int av_log_get_flags(void){return 0;}
static int forced_flags;
int av_get_cpu_flags(void){return forced_flags;}
void av_force_cpu_flags(int flags){forced_flags=flags;}
int av_cpu_count(void){return 1;}
size_t av_cpu_max_align(void){return 64;}
int errno;
void avpriv_report_missing_feature(void *p,const char *fmt,...){(void)p;(void)fmt;}
void avpriv_request_sample(void *p,const char *fmt,...){(void)p;(void)fmt;}
/* No entropy device on this bounded decoder worker; not a security RNG. */
uint32_t av_get_random_seed(void){return *(volatile uint32_t *)0xf8f00200u;}
/* Fatal libc errors cannot unwind into firmware; the host deadline resets
 * Core1 before releasing the allocation and rejects that run. */
volatile int fb_abort_code;
__attribute__((noreturn)) void _exit(int status){fb_abort_code=status;for(;;)__asm__ volatile("nop");}
char *_user_strerror(int n,int internal,int *errptr){(void)n;(void)internal;(void)errptr;return 0;}
