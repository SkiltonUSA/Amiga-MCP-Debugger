"""Complete-frame protocol tests; physical bitmap copy requires live acceptance."""
import pathlib
import subprocess
import tempfile
import unittest
from test_compute import HARNESS, ROOT

CASES=r'''
    if(!strcmp(argv[1],"frame")||!strcmp(argv[1],"cancel_frame")){
        unsigned tx,ty,x,y;uint32_t hash=2166136261u;uint16_t *all=calloc(FF_WIDTH*FF_HEIGHT,2);
        assert(all);assert(!pthread_create(&thread,0,worker,mem));
        while(ad_get(mem,ZZ_DIAG)!=ZZ_READY)usleep(100);
        if(!strcmp(argv[1],"cancel_frame")) {
            /* Real checkpoint breakpoint, cancellation while paused, detach. */
            ad_put(mem,ZZ_PAGE+256,123);ad_put(mem,ZZ_PAGE+264,4);
            ad_put(mem,ZZ_PAGE+268,1);barrier(0);ad_put(mem,ZZ_PAGE+260,1);
            while(ad_get(mem,ZZ_PAGE+36)!=1)usleep(100);
            assert(!zc_submit(&c,&view,0,0,2));
            while(ad_get(mem,ZZ_PAGE+20)!=2)usleep(100);
            assert(!zc_cancel(&c));assert(wait_result(&c,&r)==ZC_DISCARDED);
            assert(ad_get(mem,ZZ_PAGE+20)==2);
            ad_put(mem,ZZ_PAGE+264,7);barrier(0);ad_put(mem,ZZ_PAGE+260,2);
            while(ad_get(mem,ZZ_PAGE+36)!=2)usleep(100);
        }
        assert(zc_submit(&c,&view,32,0,2)==-1);
        assert(!zc_submit(&c,&view,0,0,2));assert(wait_result(&c,&r)==ZC_FRAME);
        for(ty=0;ty<FF_HEIGHT;ty+=FF_TH)for(tx=0;tx<FF_WIDTH;tx+=FF_TW){
            ff_begin(&cursor,&view,tx,ty);while(!ff_step(&cursor,expected,8192)){}
            for(y=0;y<FF_TH;y++)for(x=0;x<FF_TW;x++)all[(ty+y)*FF_WIDTH+tx+x]=expected[y*FF_TW+x];
        }
        for(x=0;x<FF_WIDTH*FF_HEIGHT;x++){
            unsigned count=(mem[FF_FRAME_COUNTS+x*2]<<8)|mem[FF_FRAME_COUNTS+x*2+1];
            uint32_t colour;memcpy(&colour,mem+FF_FRAME_RGB+x*4,4);
            assert(count==all[x]);assert(colour==ff_rgb(count,view.limit));hash=ff_hash(hash,count);
        }
        assert(hash==r.frame_hash);assert(r.compute_ticks&&r.colour_ticks);assert(!r.copy_ticks);
        /* A later tile/clock request remains supported in the direct build. */
        assert(!zc_submit(&c,&view,0,0,1));assert(wait_result(&c,&r)==ZC_CLOCK);
        assert(!zc_submit(&c,&view,0,0,0));assert(wait_result(&c,&r)==ZC_TILE);
        ad_put(mem,ZZ_STOP,1);barrier(0);assert(!pthread_join(thread,0));free(all);
    }else if(!strcmp(argv[1],"frame_timeout")){
        assert(!zc_submit(&c,&view,0,0,2));clock_offset=10000001;
        assert(zc_poll(&c,&r)==ZC_WAIT);assert(c.pending&&!c.failed);
        clock_offset=120000001;assert(zc_poll(&c,&r)==ZC_ERROR);assert(c.failed&&c.pending);
    }else if(!strcmp(argv[1],"metadata")){
        uint32_t hash;assert(!zc_submit(&c,&view,0,0,2));
        hash=ff_timing_hash(ff_result_seed(c.session,c.pending,c.generation,0,0),0,0,0);
        hash=ff_direct_hash(hash,42,7,0);
        ad_put(mem,FF_RES+4,FF_FRAME_DONE);ad_put(mem,FF_RES+8,c.generation);
        ad_put(mem,FF_RES+40,0x54494d33u);ad_put(mem,FF_RES+44,hash);
        ad_put(mem,FF_RES+48,42);ad_put(mem,FF_RES+52,8);ad_put(mem,FF_RES,c.pending);
        assert(zc_poll(&c,&r)==ZC_WAIT);assert(c.pending);
        ad_put(mem,FF_RES+52,7);assert(zc_poll(&c,&r)==ZC_FRAME);
    }else
'''
class DirectTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory();p=pathlib.Path(cls.tmp.name);cls.exe=p/'direct-test'
        (p/'test.c').write_text(HARNESS.replace('    if(!strcmp(argv[1],"worker")){',CASES+' if(!strcmp(argv[1],"worker")){'))
        subprocess.run(['clang','-std=c99','-D_DEFAULT_SOURCE','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DFF_DIRECT','-DZZ_BLOCK_SIZE=0x100000','-DFF_TIMING','-DFF_HOST_TEST','-DZZ_BUILD_ID=123',*[f'-I{ROOT/d}' for d in ['amiga/compute','amiga/fractal','amiga/arm_debug','amiga/arm_debug/zz9000']],str(p/'test.c'),*[str(ROOT/f) for f in ['amiga/compute/xx19c.c','amiga/fractal/worker.c','amiga/fractal/fractal.c','amiga/arm_debug/core.c']],'-pthread','-o',str(cls.exe)],check=True)
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def test_full_frame_counts_colours_hash_and_legacy_commands(self):subprocess.run([self.exe,'frame'],check=True,timeout=15)
    def test_full_frame_cancel_while_paused_and_restart(self):subprocess.run([self.exe,'cancel_frame'],check=True,timeout=15)
    def test_direct_metadata_integrity(self):subprocess.run([self.exe,'metadata'],check=True,timeout=15)

    def test_full_frame_deadline_keeps_long_render_owned(self):subprocess.run([self.exe,'frame_timeout'],check=True,timeout=15)
