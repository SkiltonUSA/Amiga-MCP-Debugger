"""Exercise the real C transport against synthetic faults and a native worker.
Native timing is a host fixture; it does not prove physical ARM behavior.
"""
import pathlib
import subprocess
import tempfile
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[3]
HARNESS=r'''
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "xx19c.h"
void zz_worker(volatile uint8_t *);
static uint32_t clock_offset;
static uint32_t now(void *u){struct timespec t;(void)u;clock_gettime(CLOCK_MONOTONIC,&t);return (uint32_t)(t.tv_sec*1000000u+t.tv_nsec/1000)+clock_offset;}
static void barrier(void *u){(void)u;__sync_synchronize();}
static void range(volatile void *p,size_t n,void *u){(void)p;(void)n;barrier(u);}
static struct ad_io io={range,range,barrier,0,0};
static void *worker(void *p){zz_worker(p);return 0;}
static int wait_result(struct zc_client *c,struct zc_result *r){int code;uint32_t start=now(0);do{code=zc_poll(c,r);assert(now(0)-start<3000000u);if(!code)usleep(100);}while(!code);return code;}
int main(int argc,char **argv){
    struct zc_client c;struct zc_result r;struct ff_view view;
    uint8_t *mem;pthread_t thread;uint16_t expected[FF_PIXELS];struct ff_cursor cursor;
    assert(argc==2);assert(!posix_memalign((void **)&mem,64,ZZ_BLOCK_SIZE));memset(mem,0,ZZ_BLOCK_SIZE);ad_put(mem,ZZ_CONTROL,123);ff_default(&view);
    assert(zc_init(&c,mem,ZZ_BLOCK_SIZE-1,&io,now,0)==-1);
    assert(zc_init(&c,mem,ZZ_BLOCK_SIZE,&io,now,0)==0);
    if(!strcmp(argv[1],"worker")){
        assert(!pthread_create(&thread,0,worker,mem));
        assert(!zc_submit(&c,&view,128,112,0));assert(zc_submit(&c,&view,0,0,0)==-1);
        assert(wait_result(&c,&r)==ZC_TILE);assert(r.compute_ticks>0&&r.stamp>0&&r.control==1);
        ff_begin(&cursor,&view,128,112);while(!ff_step(&cursor,expected,8192)){}
        assert(!memcmp(expected,r.pixels,sizeof(expected)));
        assert(!zc_submit(&c,&view,160,112,0));assert(!zc_cancel(&c));assert(c.pending);
        assert(zc_submit(&c,&view,0,0,0)==-1);assert(wait_result(&c,&r)==ZC_DISCARDED);
        assert(!zc_submit(&c,&view,0,0,1));assert(wait_result(&c,&r)==ZC_CLOCK);assert(r.compute_ticks==0);
        ad_put(mem,ZZ_STOP,1);barrier(0);assert(!pthread_join(thread,0));
    }else if(!strcmp(argv[1],"malformed")){
        assert(!zc_submit(&c,&view,0,0,0));ad_put(mem,FF_RES,c.pending);
        ad_put(mem,FF_RES+4,FF_DONE);ad_put(mem,FF_RES+8,c.generation+1);
        ad_put(mem,FF_RES+40,0x54494d31u);assert(zc_poll(&c,&r)==ZC_ERROR);assert(c.failed);
        assert(zc_submit(&c,&view,0,0,0)==-1);
    }else if(!strcmp(argv[1],"timeout")){
        assert(!zc_submit(&c,&view,0,0,0));clock_offset=10000001;
        assert(zc_poll(&c,&r)==ZC_ERROR);assert(c.pending&&c.failed);
        assert(zc_submit(&c,&view,0,0,0)==-1);
    }else if(!strcmp(argv[1],"bounds")){
        view.limit=257;assert(zc_submit(&c,&view,0,0,0)==-1);view.limit=128;
        assert(zc_submit(&c,&view,1,0,0)==-1);c.sequence=0xffffffffu;
        assert(zc_submit(&c,&view,0,0,0)==-1);c.generation=0xffffffffu;
        assert(zc_cancel(&c)==-1&&c.failed);
    }else assert(0);
    free(mem);return 0;
}
'''
class ComputeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory();p=pathlib.Path(cls.tmp.name)
        (p/'harness.c').write_text(HARNESS);cls.exe=p/'compute-test'
        subprocess.run(['clang','-std=c99','-D_DEFAULT_SOURCE','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DFF_TIMING','-DFF_HOST_TEST','-DZZ_BUILD_ID=123',*[f'-I{ROOT/d}' for d in ['amiga/compute','amiga/fractal','amiga/arm_debug','amiga/arm_debug/zz9000']],str(p/'harness.c'),str(ROOT/'amiga/compute/xx19c.c'),str(ROOT/'amiga/fractal/worker.c'),str(ROOT/'amiga/fractal/fractal.c'),str(ROOT/'amiga/arm_debug/core.c'),'-pthread','-o',str(cls.exe)],check=True)
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def test_worker_transfer_cancel_drain_and_clock(self):subprocess.run([self.exe,'worker'],check=True)
    def test_mismatched_generation_is_fatal(self):subprocess.run([self.exe,'malformed'],check=True)
    def test_timeout_preserves_owned_request_until_shutdown(self):subprocess.run([self.exe,'timeout'],check=True)
    def test_bounds_and_sequence_exhaustion(self):subprocess.run([self.exe,'bounds'],check=True)
