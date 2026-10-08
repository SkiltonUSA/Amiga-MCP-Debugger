"""Independent Q14 oracle and actual C worker protocol tests (host only)."""
import ctypes as C
import pathlib
import random
import struct
import subprocess
import tempfile
import threading
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
ONE = 16384
REQ, CANCEL, RES, DATA, PAGE = 0x8800, 0x8840, 0x8880, 0x8900, 0x8100

class View(C.Structure):
    _fields_ = [('cx', C.c_int32), ('cy', C.c_int32), ('step', C.c_int32), ('limit', C.c_uint32)]
class Cursor(C.Structure):
    _fields_ = [('view', View)] + [(x, C.c_uint32) for x in ('tx','ty','pixel','iteration')] + [(x, C.c_int32) for x in ('x','y','cr','ci')]

def trunc(x):
    return x // ONE if x >= 0 else -((-x) // ONE)

def reference(v, px, py):
    cr, ci = v.cx + (px-160)*v.step, v.cy + (py-120)*v.step
    x = y = n = 0
    while True:
        if abs(x)>2*ONE or abs(y)>2*ONE: return n
        xx, yy = trunc(x*x), trunc(y*y)
        if xx+yy>4*ONE or n>=v.limit: return n
        x, y = xx-yy+cr, 2*trunc(x*y)+ci
        n += 1

def frame_reference(v):
    return b''.join(struct.pack('>H',reference(v,x,y)) for y in range(240) for x in range(320))

def fnv(data):
    h=2166136261
    for b in data:h=((h^b)*16777619)&0xffffffff
    return h

class FractalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory()
        wrapper=pathlib.Path(cls.tmp.name)/'wrap.c'
        wrapper.write_text('''#include <stdlib.h>
#include <string.h>
#include "wire.h"
void *ff_alloc(void) {void *p=0;if(posix_memalign(&p,64,ZZ_BLOCK_SIZE))return 0;memset(p,0,ZZ_BLOCK_SIZE);return p;}
void ff_free(void *p){free(p);}
void ff_put(void *p,unsigned o,uint32_t v){ad_put(p,o,v);__sync_synchronize();}
uint32_t ff_get(void *p,unsigned o){__sync_synchronize();return ad_get(p,o);}
''')
        lib=pathlib.Path(cls.tmp.name)/'fractal.dylib'
        subprocess.run(['clang','-shared','-fPIC','-O2','-g','-Wall','-Wextra','-Werror',
            '-DFF_HOST_TEST','-DZZ_BUILD_ID=123', '-I'+str(ROOT/'amiga/fractal'),
            '-I'+str(ROOT/'amiga/arm_debug'),'-I'+str(ROOT/'amiga/arm_debug/zz9000'),
            str(ROOT/'amiga/fractal/fractal.c'),str(ROOT/'amiga/fractal/worker.c'),
            str(ROOT/'amiga/arm_debug/core.c'),str(wrapper),'-o',str(lib)],check=True)
        cls.lib=C.CDLL(str(lib))
        for name,args,result in [('ff_begin',[C.POINTER(Cursor),C.POINTER(View),C.c_uint32,C.c_uint32],None),
             ('ff_step',[C.POINTER(Cursor),C.POINTER(C.c_uint16),C.c_uint32],C.c_int),
             ('ff_valid',[C.POINTER(View),C.c_uint32,C.c_uint32],C.c_int),
             ('ff_zoom',[C.POINTER(View),C.c_uint32,C.c_uint32],C.c_int),
             ('ff_alloc',[],C.c_void_p),('ff_free',[C.c_void_p],None),
             ('ff_put',[C.c_void_p,C.c_uint,C.c_uint32],None),
             ('ff_get',[C.c_void_p,C.c_uint],C.c_uint32),('zz_worker',[C.c_void_p],None)]:
            f=getattr(cls.lib,name);f.argtypes=args;f.restype=result
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def tile(self,v,tx,ty,budget):
        c=Cursor();out=(C.c_uint16*512)();self.lib.ff_begin(C.byref(c),C.byref(v),tx,ty)
        while not self.lib.ff_step(C.byref(c),out,budget):pass
        return list(out)
    def test_reference_views_and_extreme_coordinates(self):
        rng=random.Random(20261008)
        views=[View(-12288,0,153,128),View(0,0,2,256),View(-3*ONE,3*ONE,153,1),View(3*ONE,-3*ONE,153,256)]
        views += [View(rng.randrange(-3*ONE,3*ONE),rng.randrange(-3*ONE,3*ONE),rng.randrange(2,154),rng.randrange(1,257)) for _ in range(10)]
        for v in views:
            tx,ty=rng.randrange(10)*32,rng.randrange(15)*16
            want=[reference(v,tx+x,ty+y) for y in range(16) for x in range(32)]
            self.assertEqual(self.tile(v,tx,ty,64),want)
            self.assertEqual(self.tile(v,tx,ty,8192),want)
    def test_zero_budget_and_finite_precision_zoom(self):
        v=View(-12288,0,153,128);c=Cursor();out=(C.c_uint16*512)()
        self.lib.ff_begin(C.byref(c),C.byref(v),0,0)
        self.assertEqual(self.lib.ff_step(C.byref(c),out,0),0);self.assertEqual(c.pixel,0)
        for step in (76,38,19,9,4,2):
            self.assertEqual(self.lib.ff_zoom(C.byref(v),160,120),1);self.assertEqual(v.step,step)
        before=bytes(v);self.assertEqual(self.lib.ff_zoom(C.byref(v),160,120),0);self.assertEqual(bytes(v),before)
        self.assertEqual(self.lib.ff_zoom(C.byref(v),320,240),0)
    def test_reject_invalid_jobs(self):
        for v in (View(0,0,0,128),View(0,0,154,128),View(0,0,2,0),View(0,0,2,257),View(3*ONE+1,0,2,128)):
            self.assertEqual(self.lib.ff_valid(C.byref(v),0,0),0)
        v=View(-12288,0,153,128)
        for x,y in ((1,0),(0,1),(320,0),(0,240),(0xffffffff,0)):
            self.assertEqual(self.lib.ff_valid(C.byref(v),x,y),0)
    def test_valid_extremes_under_undefined_behavior_sanitizer(self):
        source=pathlib.Path(self.tmp.name)/'sanitize.c'
        source.write_text('''#include "fractal.h"
int main(void) {
    struct ff_view v;struct ff_cursor c;uint16_t out[FF_PIXELS];
    unsigned seed=17,i;
    for(i=0;i<100;i++) {
        seed=seed*1664525u+1013904223u;v.cx=(int)(seed%(6*FF_ONE+1))-3*FF_ONE;
        seed=seed*1664525u+1013904223u;v.cy=(int)(seed%(6*FF_ONE+1))-3*FF_ONE;
        v.step=(i&1)?2:153;v.limit=256;
        ff_begin(&c,&v,(i%10)*32,(i%15)*16);
        while(!ff_step(&c,out,64)){}
    }
    return 0;
}
''')
        exe=pathlib.Path(self.tmp.name)/'sanitize'
        subprocess.run(['clang','-O2','-fsanitize=undefined','-fno-sanitize-recover=all',
            '-I'+str(ROOT/'amiga/fractal'),str(source),str(ROOT/'amiga/fractal/fractal.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    def wait(self,condition):
        end=time.monotonic()+3
        while time.monotonic()<end:
            if condition():return
            time.sleep(.001)
        self.fail('Worker timed out')
    def test_worker_wire_cancel_while_paused_and_relaunch(self):
        for launch in range(2):
            p=self.lib.ff_alloc();self.assertTrue(p)
            put=lambda off,val:self.lib.ff_put(p,off,val)
            get=lambda off:self.lib.ff_get(p,off)
            put(0x8000,1234+launch)
            t=threading.Thread(target=self.lib.zz_worker,args=(p,));t.start()
            try:
                self.wait(lambda:get(0x8040)==0x41524d31)
                self.wait(lambda:get(PAGE+28)==4)
                sequence=get(PAGE+16)
                time.sleep(.01)
                self.assertEqual(get(PAGE+16),sequence,'Idle must leave a stable debugger snapshot')
                # Breakpoint at dispatch, using the real debug mailbox.
                for off,v in [(0,1234+launch),(8,4),(12,1)]:put(PAGE+256+off,v)
                put(PAGE+260,1);self.wait(lambda:get(PAGE+36)==1)
                def job(seq,gen,tx=128):
                    for off,v in [(4,1234+launch),(8,gen),(12,-12288),(16,0),(20,153),(24,128),(28,tx),(32,112)]:put(REQ+off,v)
                    put(CANCEL,gen);put(REQ,seq)
                job(1,1)
                self.wait(lambda:get(PAGE+20)==2)
                self.assertEqual(get(RES),0)
                put(CANCEL,2)
                self.wait(lambda:get(RES)==1)
                self.assertEqual(get(RES+4),2);self.assertEqual(get(PAGE+20),2)
                # Detach clears breakpoint/resumes, then a fresh tile completes.
                put(PAGE+264,7);put(PAGE+260,2)
                self.wait(lambda:get(PAGE+36)==2)
                job(2,3)
                self.wait(lambda:get(RES)==2)
                self.assertEqual(get(RES+4),1);self.assertEqual(get(RES+8),3)
                pixels=struct.unpack('>512H',C.string_at(p+DATA,1024))
                v=View(-12288,0,153,128)
                self.assertEqual(list(pixels),[reference(v,128+x,112+y) for y in range(16) for x in range(32)])
                saved=C.string_at(p+DATA,1024)
                job(3,4,320);self.wait(lambda:get(RES)==3)
                self.assertEqual(get(RES+4),3);self.assertEqual(C.string_at(p+DATA,1024),saved)
            finally:
                put(0x8080,1);t.join(3)
                if not t.is_alive():self.lib.ff_free(p)
            self.assertFalse(t.is_alive(),'Paused worker must stop')

if __name__=='__main__':unittest.main()
