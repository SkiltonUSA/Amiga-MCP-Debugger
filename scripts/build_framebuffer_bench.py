#!/usr/bin/env python3
"""Build a display-only, cache-off XX19c framebuffer bandwidth experiment."""
import hashlib
import json
from pathlib import Path
import subprocess
from build_zz9000_debug import unpack_elf
from amiga import PIN

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'.context/amiga/framebuffer-bench'
OUT.mkdir(parents=True,exist_ok=True)
flags=['--target=arm-none-eabi','-mcpu=cortex-a9','-marm','-mfloat-abi=soft',
       '-ffreestanding','-fno-builtin','-fPIC','-fvisibility=hidden','-O2','-g',
       '-Wall','-Wextra','-Werror','-DZZ_BLOCK_SIZE=0x400000','-DZZ_CONTROL=0x10000',
       '-Iamiga/arm_debug','-Iamiga/arm_debug/zz9000','-Iamiga/framebuffer_bench']
objects=[]
for source in ['amiga/arm_debug/zz9000/entry.S','amiga/framebuffer_bench/worker.c']:
    obj=OUT/(Path(source).stem+'.o');objects.append(obj)
    subprocess.run(['clang',*flags,'-c',source,'-o',obj],cwd=ROOT,check=True)
elf=OUT/'worker.elf'
subprocess.run(['/opt/homebrew/opt/lld/bin/ld.lld','-shared','-Bsymbolic','--no-undefined',
                '-T','amiga/arm_debug/zz9000/payload.ld',*objects,'-o',elf],cwd=ROOT,check=True)
payload,relocations,entry=unpack_elf(elf.read_bytes(),0x10000)
subprocess.run(['clang',*flags,'-c','amiga/arm_debug/zz9000/mapping_probe.S','-o',OUT/'mapping.o'],cwd=ROOT,check=True)
subprocess.run(['/opt/homebrew/opt/lld/bin/ld.lld','-shared','-Bsymbolic','--no-undefined',
                '-T','amiga/arm_debug/zz9000/payload.ld',OUT/'mapping.o','-o',OUT/'mapping.elf'],cwd=ROOT,check=True)
mapping,rel,point=unpack_elf((OUT/'mapping.elf').read_bytes())
assert not rel and not point and len(mapping)<=1024
def array(name,data):
    return 'static const unsigned char '+name+'[]={'+','.join(str(x) for x in data)+'};\n'
(OUT/'zz_payload.h').write_text(f'#define ZZ_ENTRY {entry}u\n'+array('zz_image',payload)+
    array('zz_mapping_probe',mapping)+'static const unsigned long zz_relocations[]={'+
    ','.join(str(x) for x in relocations)+'};\n')
relative=OUT.relative_to(ROOT)
cmd=['podman','--connection','nuflix-converter-root','run','--rm','--platform','linux/amd64',
     '-v',f'{ROOT}:/work','-w','/work',PIN['image'],'m68k-amigaos-gcc',
     '-std=c99','-noixemul','-m68020','-O2','-g','-Wall','-Wextra','-Werror',
     '-DZZ_FB_BENCH','-DZZ_RELEASE','-DZZ_BLOCK_SIZE=0x400000','-DZZ_CONTROL=0x10000',
     '-DZZ_APP_NAME="ZZFrameBench"','-DZZ_APP_VERSION="0.1"','-DZZ_MIN_STACK=65536',
     '-Iamiga/arm_debug','-Iamiga/arm_debug/zz9000','-Iamiga/framebuffer_bench',
     '-I'+str(relative),'amiga/arm_debug/zz9000/launcher.c','amiga/framebuffer_bench/host.c',
     '-lamiga','-o',str(relative/'ZZFrameBench')]
subprocess.run(cmd,cwd=ROOT,check=True)
binary=OUT/'ZZFrameBench';assert binary.read_bytes()[:4]==b'\x00\x00\x03\xf3'
sources=['amiga/framebuffer_bench/worker.c','amiga/framebuffer_bench/host.c',
         'amiga/framebuffer_bench/wire.h','amiga/arm_debug/zz9000/launcher.c',
         'amiga/arm_debug/zz9000/entry.S','scripts/build_framebuffer_bench.py']
(OUT/'build.json').write_text(json.dumps(dict(cache_policy='ARM MMU/I-cache/D-cache off',
    sources={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources},
    executable_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
    executable_md5=hashlib.md5(binary.read_bytes()).hexdigest()),indent=2)+'\n')
print((OUT/'build.json').read_text())
