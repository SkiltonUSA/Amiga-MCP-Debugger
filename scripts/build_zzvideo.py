#!/usr/bin/env python3
"""Build the standalone XX19c ARM MPEG-1 preview. Does not deploy or run it."""
import argparse,hashlib,json,subprocess,shutil
from pathlib import Path
from video_vendor import prepare
from build_zz9000_debug import unpack_elf
import amiga as launcher
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'.context/amiga/video'
DEFS=['-DZZ_VIDEO','-DZZ_BLOCK_SIZE=0x800000','-DZZ_CONTROL=0x10000']
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--container-command');p.add_argument('--sdl-source',type=Path,default=ROOT/'.context/SDL2-AmigaOS3')
    p.add_argument('--developer',action='store_true');a=p.parse_args()
    prepare(OUT)
    cc=shutil.which('clang');ld=shutil.which('ld.lld') or '/opt/homebrew/opt/lld/bin/ld.lld'
    src=[ROOT/f for f in ['amiga/arm_debug/zz9000/entry.S','amiga/arm_debug/core.c','amiga/video/worker.c',
         'amiga/video/decoder.c','amiga/video/media.c','amiga/video/runtime.c']]
    identity=hashlib.sha256()
    for path in sorted((ROOT/'amiga/video').rglob('*')):
        if path.is_file() and (path.suffix in ('.c','.h','.S') or path.name=='upstream.json') and path.name!='make-icons.c':
            identity.update(path.read_bytes())
    for path in [*src[:2],ROOT/"amiga/arm_debug/protocol.h",ROOT/"amiga/arm_debug/zz9000/layout.h",
                 ROOT/"amiga/arm_debug/zz9000/launcher.c",ROOT/"amiga/arm_debug/zz9000/payload.ld",
                 ROOT/"scripts/video_vendor.py",Path(__file__),OUT/"pl_mpeg_port.h"]:
        identity.update(path.read_bytes())
    identity.update(subprocess.check_output([cc,'--version']));build=int.from_bytes(identity.digest()[:4],'big') or 1
    flags=['--target=arm-none-eabi','-mcpu=cortex-a9','-marm','-mfloat-abi=soft','-ffreestanding','-fno-builtin',
        '-fPIC','-fvisibility=hidden','-fno-stack-protector','-ffunction-sections','-fdata-sections','-O2','-g',
        '-Wall','-Wextra',*DEFS,f'-DZZ_BUILD_ID=0x{build:08x}u',
        '-Iamiga/video/freestanding','-Iamiga/video','-Iamiga/arm_debug','-Iamiga/arm_debug/zz9000','-I'+str(OUT)]
    objects=[]
    for s in src:
        obj=OUT/(s.stem+'.o');objects.append(obj)
        subprocess.run([cc,*flags,'-c',str(s),'-o',str(obj)],cwd=ROOT,check=True)
    elf=OUT/'zzvideo.elf'
    subprocess.run([ld,'-shared','-Bsymbolic','--gc-sections','--no-undefined','--defsym=ZZ_IMAGE_LIMIT=0x10000',
        '-T','amiga/arm_debug/zz9000/payload.ld',*objects,'-o',elf],cwd=ROOT,check=True)
    image,rel,entry=unpack_elf(elf.read_bytes(),0x10000)
    subprocess.run([cc,*flags,'-c','amiga/arm_debug/zz9000/mapping_probe.S','-o',OUT/'mapping.o'],cwd=ROOT,check=True)
    subprocess.run([ld,'-shared','-Bsymbolic','--no-undefined','-T','amiga/arm_debug/zz9000/payload.ld',
        OUT/'mapping.o','-o',OUT/'mapping.elf'],cwd=ROOT,check=True)
    probe,pr,pe=unpack_elf((OUT/'mapping.elf').read_bytes())
    if pr or pe or len(probe)>1024:raise ValueError('Invalid bootstrap')
    def array(name,data):return 'static const unsigned char '+name+'[]={'+','.join(str(b) for b in data)+'};\n'
    (OUT/'zz_payload.h').write_text(f'#define ZZ_ENTRY {entry}u\n'+array('zz_image',image)+array('zz_mapping_probe',probe)+
        'static const unsigned long zz_relocations[]={'+','.join(str(x) for x in rel)+'};\n')
    command=json.loads(a.container_command) if a.container_command else launcher.container_command()
    sdl=a.sdl_source.resolve();library=sdl/'libSDL2.a'
    pin=json.loads((ROOT/'amiga/sdl_fractal/sdl.json').read_text())
    if hashlib.sha256(library.read_bytes()).hexdigest()!=pin['library_sha256']:raise ValueError('SDL library pin mismatch')
    prefix=[*command,'run','--rm','--platform','linux/amd64','-v',f'{ROOT}:/work','-w','/work',launcher.PIN['image']]
    out=OUT.relative_to(ROOT);sdl=sdl.relative_to(ROOT)
    common=[*prefix,'m68k-amigaos-gcc','-std=c99','-noixemul','-m68030','-O2','-Wall','-Wextra','-Werror',
        '-D__AMIGAOS3__',*DEFS,'-DZZ_APP_NAME="ZZVideo"','-DZZ_APP_VERSION="0.1"','-DZZ_CLIENT_NAME="zzvideo"',
        '-DZZ_MIN_STACK=131072','-Iamiga/video','-Iamiga/arm_debug','-Iamiga/arm_debug/zz9000','-I'+str(out),
        '-I'+str(sdl/'include'),'-I.tools/amiga-devbench/amiga-bridge/include']
    if not a.developer:common+=['-DZZ_RELEASE']
    subprocess.run([*common,'-DIntuitionBase=ZZIntuitionBase','-c','amiga/arm_debug/zz9000/launcher.c','-o',str(out/'launcher.o')],check=True)
    bridge=['amiga/arm_debug/relay.c','amiga/arm_debug/bridge_adapter.c','.tools/amiga-devbench/amiga-bridge/client/bridge_client.c'] if a.developer else []
    name='zzvideo-debug' if a.developer else 'ZZVideo'
    subprocess.run([*common,'amiga/video/app.c','amiga/video/client.c','amiga/video/media.c',str(out/'launcher.o'),
        *bridge,str(sdl/'libSDL2.a'),'-lm','-lamiga','-s','-o',str(out/name)],check=True)
    record=dict(application='ZZVideo',version='0.1',build_id=build,source_sha256=identity.hexdigest(),image_bytes=len(image),
        block_bytes=0x800000,relocations=len(rel),developer=a.developer,physical_execution_verified=False,sdl=pin,
        files={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (elf,OUT/name)})
    (OUT/(name+'-build.json')).write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2))
if __name__=='__main__':main()
