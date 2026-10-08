import os
import ctypes as C
import hashlib,json,random,subprocess,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'scripts'))
from video_vendor import prepare
OUT=ROOT/'.context/amiga/video/tests'
class Info(C.Structure):
    _fields_=[(x,C.c_uint32) for x in ['width','height','rate_num','rate_den','pictures']]+[('bytes',C.c_size_t)]
class VideoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        prepare(OUT)
        common=['clang','-O2','-g','-fsanitize=address','-fno-omit-frame-pointer','-I'+str(ROOT/'amiga/video'),'-I'+str(OUT)]
        subprocess.run([*common,str(ROOT/'tests/amiga/video/decode_native.c'),str(ROOT/'amiga/video/decoder.c'),str(ROOT/'amiga/video/media.c'),'-o',str(OUT/'decode')],check=True)
        subprocess.run([*common,'-DORACLE',str(ROOT/'tests/amiga/video/decode_native.c'),str(ROOT/'amiga/video/media.c'),'-o',str(OUT/'oracle')],check=True)
        subprocess.run(['clang','-O2','-shared','-fPIC',str(ROOT/'amiga/video/media.c'),'-o',str(OUT/'media.dylib')],check=True)
        cls.lib=C.CDLL(str(OUT/'media.dylib'));cls.lib.zv_prepare.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(Info)]
        cls.clips=[]
        for name,size,rate,bframes in [('small','160x128','25',0),('bframes','320x240','25',2),('ntsc','160x128','30000/1001',2)]:
            file=OUT/(name+'.mpg')
            subprocess.run([os.environ.get('FFMPEG','ffmpeg'),'-hide_banner','-loglevel','error','-y','-f','lavfi','-i',f'testsrc2=size={size}:rate={rate}',
                '-frames:v','25','-c:v','mpeg1video','-q:v','5','-g','12','-bf',str(bframes),'-an','-f','mpeg',str(file)],check=True)
            cls.clips.append(file)
    def prepare_bytes(self,data):
        buf=C.create_string_buffer(data);info=Info();rc=self.lib.zv_prepare(buf,len(data),C.byref(info));return rc,info,buf.raw[:info.bytes]
    def test_shared_result_ownership_and_integrity(self):
        binary=OUT/"client"
        subprocess.run(["clang","-O2","-g","-fsanitize=address,undefined","-DZZ_BLOCK_SIZE=0x800000","-DZZ_CONTROL=0x10000",
            "-Iamiga/video","-Iamiga/arm_debug","-Iamiga/arm_debug/zz9000",
            "tests/amiga/video/test_client.c","amiga/video/client.c","amiga/video/media.c","-o",str(binary)],cwd=ROOT,check=True)
        subprocess.run([binary],check=True)
    def test_frames_match_unmodified_upstream(self):
        for clip in self.clips:
            with self.subTest(clip=clip.name):
                got=subprocess.run([OUT/'decode',clip,clip.with_suffix('.argb')],capture_output=True,text=True,check=True)
                want=subprocess.run([OUT/'oracle',clip],capture_output=True,text=True,check=True)
                actual=got.stdout.splitlines();baseline=want.stdout.splitlines()
                self.assertEqual(actual[:len(baseline)],baseline);self.assertEqual(len(actual),25)
                # Upstream omits its delayed reference frame when the last
                # coded picture is B. The port drains that final frame too.
                reference=clip.with_suffix('.ffmpeg.argb')
                subprocess.run([os.environ.get('FFMPEG','ffmpeg'),'-hide_banner','-loglevel','error','-y',
                    '-i',str(clip),'-pix_fmt','argb','-f','rawvideo',str(reference)],check=True)
                raw=clip.with_suffix('.argb').read_bytes();ref=reference.read_bytes()
                self.assertEqual(len(raw),len(ref))
                differences=[abs(a-b) for i,(a,b) in enumerate(zip(raw,ref)) if i%4]
                self.assertLessEqual(max(differences),16)
                self.assertLess(sum(differences)/len(differences),2)
                (clip.with_suffix('.comparison.json')).write_text(json.dumps(dict(frames=len(actual),
                    upstream_frames=len(baseline),max_channel_error=max(differences),
                    mean_channel_error=sum(differences)/len(differences)),indent=2)+'\n')
                (clip.with_suffix('.hashes')).write_text(got.stdout)
                self.assertIn('rewind='+want.stdout.splitlines()[0].split()[1],got.stderr)
    def test_elementary_stream_and_rational(self):
        data=self.clips[2].read_bytes();rc,info,es=self.prepare_bytes(data)
        self.assertEqual(rc,0);self.assertEqual((info.rate_num,info.rate_den),(30000,1001))
        rc2,info2,_=self.prepare_bytes(es);self.assertEqual(rc2,0);self.assertEqual(info2.pictures,25)
    def test_unsupported_and_bounds(self):
        self.assertLess(self.prepare_bytes(b'\0\0\0\x18ftypmp42')[0],0)
        _,_,es=self.prepare_bytes(self.clips[0].read_bytes())
        for w,h in [(322,240),(320,242),(319,120),(0,120),(160,120)]:
            d=bytearray(es);d[4]=w>>4;d[5]=((w&15)<<4)|(h>>8);d[6]=h&255
            self.assertEqual(self.prepare_bytes(bytes(d))[0],-4)
        self.assertEqual(self.prepare_bytes(es+b'\0\0\1\xb5\x10')[0],-3)
    def test_truncated_and_random_containers(self):
        data=self.clips[0].read_bytes()
        for n in [0,1,3,4,11,13,19,len(data)-1]:self.assertLess(self.prepare_bytes(data[:n])[0],0)
        rng=random.Random(9000)
        for _ in range(1000):
            d=b'\0\0\1\xba'+rng.randbytes(rng.randrange(0,256));self.assertLess(self.prepare_bytes(d)[0],0)
    def test_small_heap_fails_without_writes_outside(self):
        # Allocation failure paths run in the address-sanitized CLI as well as
        # this separately loaded decoder with a deliberately tiny arena.
        subprocess.run(['clang','-shared','-fPIC','-O2','-I'+str(ROOT/'amiga/video'),'-I'+str(OUT),
            str(ROOT/'amiga/video/decoder.c'),str(ROOT/'amiga/video/media.c'),'-o',str(OUT/'decoder.dylib')],check=True)
        lib=C.CDLL(str(OUT/'decoder.dylib'));lib.zv_open.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.POINTER(Info)]
        d=self.clips[0].read_bytes();buf=C.create_string_buffer(d);heap=C.create_string_buffer(64);info=Info()
        self.assertEqual(lib.zv_open(buf,len(d),heap,8,C.byref(info)),-6)
if __name__=='__main__':unittest.main()
