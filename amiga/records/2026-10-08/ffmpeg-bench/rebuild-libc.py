from pathlib import Path
import re,subprocess,json
root=Path.cwd();out=root/'.context/amiga/ffmpeg-bench';src=out/'picolibc-1.8.12';objs=out/'libc-objects';objs.mkdir(exist_ok=True)
files={str(p.relative_to(src)).replace('/','_')+'.o':p for p in src.rglob('*.c')}
source=out/'link.map';members=sorted(set(re.findall(r'libc(?:-pic)?\.a\(([^)]+)\)',source.read_text())))
cmd=['podman','--connection','nuflix-converter-root','exec','zzffmpeg-toolchain']
flags=['-mcpu=cortex-a9','-marm','-mfpu=neon','-mfloat-abi=softfp','-O2','-D_LIBC','-D_GNU_SOURCE','-U_FORTIFY_SOURCE','-fPIC','-fno-builtin','-ffunction-sections','-fdata-sections','-include','/work/amiga/ffmpeg_bench/libc_config.h','-I/work/.context/amiga/ffmpeg-bench/picolibc-1.8.12/libc/tinystdio']
flags += ['-I/work/.context/amiga/ffmpeg-bench/picolibc-1.8.12/'+d for d in ['libc/locale','libc/stdio','libc/stdlib','libm/common','libm/math']]
for name in members:
 if name.endswith('.S.o'):continue
 p=files.get(name)
 if not p:
  if name=='memmove.c.o':p=src/'libc/string/memmove.c'
  else: print('Unknown member',name);continue
 obj=objs/name
 if obj.exists():continue
 print('PIC libc',name,flush=True)
 def inside(p):return '/work/'+str(p.relative_to(root))
 subprocess.run([*cmd,'arm-none-eabi-gcc',*flags,'-c',inside(p),'-o',inside(obj)],check=True)
 subprocess.run([*cmd,'arm-none-eabi-ar','r',inside(out/'libc-pic.a'),inside(obj)],check=True)
