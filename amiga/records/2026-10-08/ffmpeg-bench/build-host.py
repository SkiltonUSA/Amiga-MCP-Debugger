from pathlib import Path
import sys,subprocess,hashlib,json
ROOT=Path.cwd();sys.path.insert(0,str(ROOT/'scripts'))
from build_zz9000_debug import unpack_elf
out=ROOT/'.context/amiga/ffmpeg-bench'
image,rel,entry=unpack_elf((out/'ffmpeg.elf').read_bytes(),0x180000)
print('Validated ELF',len(image),'bytes,',len(rel),'relative relocations')
# Reuse the byte-identical checked bootstrap from the previous accepted build.
probe,pr,pe=unpack_elf((ROOT/'.context/amiga/video-neon/neon/mapping.elf').read_bytes())
assert not pr and not pe and len(probe)<1024
def array(n,b):return 'static const unsigned char '+n+'[]={'+','.join(map(str,b))+'};\n'
(out/'zz_payload.h').write_text('#define ZZ_ENTRY '+str(entry)+'u\n'+array('zz_image',image)+array('zz_mapping_probe',probe)+'static const unsigned long zz_relocations[]={'+','.join(map(str,rel))+'};\n')
cmd=['podman','--connection','nuflix-converter-root','exec','-w','/work','zzffmpeg-toolchain','m68k-amigaos-gcc','-std=c99','-noixemul','-m68030','-O2','-Wall','-Wextra','-Werror','-D__AMIGAOS3__','-DZZ_VIDEO','-DZZ_VIDEO_ICACHE','-DZZ_VIDEO_NEON','-DZZ_CONTROL=0x180000','-DZZ_BLOCK_SIZE=0x1000000','-DZZ_RELEASE','-DZZ_MIN_STACK=131072','-DZZ_APP_NAME="ZZFFmpegBench"','-DZZ_APP_VERSION="0.1"','-DZZ_CLIENT_NAME="zzffmpeg"','-Iamiga/ffmpeg_bench','-Iamiga/arm_debug','-Iamiga/arm_debug/zz9000','-I.context/amiga/ffmpeg-bench']
subprocess.run([*cmd,'amiga/arm_debug/zz9000/launcher.c','amiga/ffmpeg_bench/host.c','-lamiga','-lm','-s','-o','.context/amiga/ffmpeg-bench/ZZFFmpegBench'],check=True)
record={'ffmpeg_version':'9.0.2','arm_image_bytes':len(image),'relative_relocations':len(rel),'allocation_bytes':0x1000000,'control_offset':0x180000,'hardware_verified':False,'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [out/'ffmpeg.elf',out/'ZZFFmpegBench',out/'bframes.m1v']}}
(out/'build.json').write_text(json.dumps(record,indent=2)+'\n')
