#!/usr/bin/env python3
"""Build the XX19c runtime-verified shared RAM launcher. No deployment."""
import argparse
import hashlib
import importlib.util
import json
import re
from pathlib import Path
import shutil
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SDK=ROOT/"amiga/arm_debug"
OUT=ROOT/".context/amiga/arm-debug/zz9000"


def unpack_elf(data):
    """Accept one bounded ARM load segment and only local RELATIVE relocations."""
    if data[:6]!=b"\x7fELF\x01\x01" or struct.unpack_from("<H",data,18)[0]!=40:
        raise ValueError("Expected ELF32 little-endian ARM")
    h=struct.unpack_from("<HHIIIIIHHHHHH",data,16)
    entry,phoff,shoff,phsize,phnum,shsize,shnum=h[3],h[4],h[5],h[8],h[9],h[10],h[11]
    segments=[]
    for i in range(phnum):
        p=struct.unpack_from("<8I",data,phoff+i*phsize)
        if p[0]==1:segments.append(p)
    if len(segments)!=1:raise ValueError("Exactly one owned image segment required")
    _,off,addr,_,filesz,memsz,_,_=segments[0]
    if addr or not 0<memsz<0x8000 or filesz>memsz or off+filesz>len(data) or entry>=memsz:
        raise ValueError("Invalid image bounds")
    image=bytearray(data[off:off+filesz])+bytearray(memsz-filesz)
    rel=[]
    for i in range(shnum):
        s=struct.unpack_from("<10I",data,shoff+i*shsize)
        if s[1]==4:raise ValueError("RELA unsupported")
        if s[1]!=9:continue
        if s[9]!=8 or s[5]%8:raise ValueError("Invalid relocation table")
        for pos in range(s[4],s[4]+s[5],8):
            at,info=struct.unpack_from("<II",data,pos)
            if info!=23 or at%4 or at+4>memsz or at in rel:
                raise ValueError(f"Unsupported ARM relocation {info} at {at:x}")
            if struct.unpack_from("<I",image,at)[0]>=memsz:
                raise ValueError("Relocation escapes the application allocation")
            rel.append(at)
    return image,rel,entry


def main():
    global OUT
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--fractal",action="store_true",help="Build the Workbench Mandelbrot application")
    p.add_argument("--sdl",action="store_true",help="Build the interactive SDL2 frontend and timed worker (implies --fractal)")
    p.add_argument("--sdl-source",type=Path,help="Pinned SDL2-AmigaOS3 SDK source checkout")
    p.add_argument("--release",action="store_true",help="Standalone distribution build (requires --fractal)")
    p.add_argument("--container-command",help="JSON container argv; defaults to workspace settings")
    p.add_argument("--clang",default="clang")
    p.add_argument("--ld",default=shutil.which("ld.lld") or "/opt/homebrew/opt/lld/bin/ld.lld")
    a=p.parse_args()
    if a.sdl:a.fractal=True
    if a.release and not a.fractal:p.error("--release requires --fractal")
    if a.fractal:OUT=ROOT/".context/amiga/fractal"
    if a.release:OUT=ROOT/".context/amiga/fractal-release"
    if a.sdl:OUT=ROOT/(".context/amiga/sdl-fractal-release" if a.release else ".context/amiga/sdl-fractal")
    OUT.mkdir(parents=True,exist_ok=True)
    cc=shutil.which(a.clang);ld=shutil.which(a.ld)
    if not cc or not ld:raise SystemExit("Clang ARM target and LLVM ld.lld required")
    flags=["--target=arm-none-eabi","-mcpu=cortex-a9","-marm","-mfloat-abi=soft","-ffreestanding",
           "-fno-builtin","-fPIC","-fvisibility=hidden","-fno-stack-protector","-O1","-g",
           "-Wall","-Wextra","-Werror","-I",str(SDK),"-I",str(SDK/"zz9000"),
           "-I",str(ROOT/"amiga/fractal")]
    if a.sdl:flags += ["-DFF_TIMING"]
    sources=[SDK/"core.c",SDK/"protocol.h",*sorted((SDK/"zz9000").glob("*"))]
    if a.fractal:sources+=sorted((ROOT/"amiga/fractal").glob("*"))
    if a.sdl:sources += [*sorted((ROOT/"amiga/sdl_fractal").glob("*.c")),*sorted((ROOT/"amiga/compute").glob("*.[ch]"))]
    identity=hashlib.sha256()
    for path in sources:identity.update(path.read_bytes())
    for tool in (cc,ld):identity.update(subprocess.check_output([tool,"--version"]))
    identity.update(" ".join(flags[:flags.index("-I")]+(["-DFF_TIMING"] if a.sdl else [])).encode()) # Exclude workspace include paths.
    if a.release:identity.update(b"standalone-release-v1")
    if a.sdl:identity.update(b"sdl-fractal-timing-v1")
    build=int.from_bytes(identity.digest()[:4],"big") or 1
    objects=[]
    arm_sources=[SDK/"core.c",SDK/"zz9000/entry.S"]
    arm_sources += [ROOT/"amiga/fractal/worker.c",ROOT/"amiga/fractal/fractal.c"] if a.fractal else [SDK/"zz9000/worker.c"]
    for source in arm_sources:
        obj=OUT/(source.stem+".o");objects.append(obj)
        subprocess.run([cc,*flags,f"-DZZ_BUILD_ID=0x{build:08x}u","-c",source,"-o",obj],check=True)
    elf=OUT/"zzarm.elf"
    subprocess.run([ld,"-shared","-Bsymbolic","--no-undefined","-T",SDK/"zz9000/payload.ld",
                    *objects,"-o",elf],check=True)
    image,rel,entry=unpack_elf(elf.read_bytes())
    subprocess.run([cc,*flags,"-c",SDK/"zz9000/mapping_probe.S","-o",OUT/"mapping.o"],check=True)
    subprocess.run([ld,"-shared","-Bsymbolic","--no-undefined","-T",SDK/"zz9000/payload.ld",
                    OUT/"mapping.o","-o",OUT/"mapping.elf"],check=True)
    probe,probe_rel,probe_entry=unpack_elf((OUT/"mapping.elf").read_bytes())
    if probe_rel or probe_entry or len(probe)>1024:raise ValueError("Bootstrap must fit before scratch")
    def array(name,buf):
        return f"static const unsigned char {name}[]={{\n"+"\n".join(
            ",".join(f"0x{x:02x}" for x in buf[i:i+16])+"," for i in range(0,len(buf),16))+"\n};\n"
    (OUT/"zz_payload.h").write_text(f"#define ZZ_ENTRY {entry}u\n"+array("zz_image",image)+
        array("zz_mapping_probe",probe)+"static const unsigned long zz_relocations[]={"+
        ",".join(str(x) for x in rel)+"};\n")
    spec=importlib.util.spec_from_file_location("amiga_launcher",ROOT/"scripts/amiga.py")
    launcher=importlib.util.module_from_spec(spec);spec.loader.exec_module(launcher)
    command=json.loads(a.container_command) if a.container_command else launcher.container_command()
    if not isinstance(command,list) or not command or not all(isinstance(x,str) and x for x in command):
        raise ValueError("Nonempty container argv required")
    prefix=[*command,"run","--rm","--platform","linux/amd64","-v",f"{ROOT}:/work","-w","/work",launcher.PIN["image"]]
    out_rel=OUT.relative_to(ROOT)
    name="zzfractal" if a.fractal else "zzarm-debug"
    if a.release:name="ZZFractal"
    extra=["-DZZ_FRACTAL","-Iamiga/fractal","amiga/fractal/window.c","amiga/fractal/fractal.c"] if a.fractal else []
    if a.release:extra += ["-DZZ_RELEASE","-s"]
    bridge_sources=[] if a.release else ["amiga/arm_debug/relay.c","amiga/arm_debug/bridge_adapter.c",
        ".tools/amiga-devbench/amiga-bridge/client/bridge_client.c"]
    sdl_record=None
    if a.sdl:
        if not a.sdl_source:raise ValueError("--sdl-source must identify the SDL2 0.1.0 source checkout with built library")
        sdl=a.sdl_source.resolve();sdl_rel=sdl.relative_to(ROOT)
        library=sdl/"libSDL2.a"
        digest=hashlib.sha256(library.read_bytes()).hexdigest()
        sdl_record=json.loads((ROOT/"amiga/sdl_fractal/sdl.json").read_text())
        if digest!=sdl_record["library_sha256"]:
            raise ValueError("SDL2 library does not match the published clean SDK")
        name="SDLZZFractal" if a.release else "sdlzzfractal"
        common=[*prefix,"m68k-amigaos-gcc","-std=c99","-noixemul","-m68030","-O2",
            "-Wall","-Wextra","-Werror","-D__AMIGAOS3__","-DZZ_FRACTAL",
            '-DZZ_APP_NAME="SDLZZFractal"','-DZZ_APP_VERSION="0.2"','-DZZ_CLIENT_NAME="sdlfractal"',
            "-DZZ_MIN_STACK=65536","-Iamiga/arm_debug","-Iamiga/arm_debug/zz9000",
            "-Iamiga/fractal","-Iamiga/compute","-I"+str(sdl_rel/"include"),
            "-I"+str(out_rel),"-I.tools/amiga-devbench/amiga-bridge/include"]
        if a.release:common += ["-DZZ_RELEASE"]
        subprocess.run([*common,"-DIntuitionBase=ZZIntuitionBase","-c",
            "amiga/arm_debug/zz9000/launcher.c","-o",str(out_rel/"launcher.o")],check=True)
        subprocess.run([*common,"amiga/sdl_fractal/app.c","amiga/compute/xx19c.c","amiga/fractal/fractal.c",
            str(out_rel/"launcher.o"),*bridge_sources,str(sdl_rel/"libSDL2.a"),"-lm","-lamiga",
            *(["-s"] if a.release else []),"-o",str(out_rel/name)],check=True)
    else:
        subprocess.run([*prefix,"m68k-amigaos-gcc","-std=c99","-noixemul","-m68020","-O2","-g",*extra,
            "-Wall","-Wextra","-Werror","-Iamiga/arm_debug","-Iamiga/arm_debug/zz9000",
            "-I"+str(out_rel),"-I.tools/amiga-devbench/amiga-bridge/include",
            "amiga/arm_debug/zz9000/launcher.c",*bridge_sources,"-lamiga","-o",
            str(out_rel/name)],check=True)
    binary=OUT/name
    if binary.read_bytes()[:4]!=b"\0\0\x03\xf3":raise ValueError("Expected Amiga Hunk")
    record={"build_id":build,"source_identity_sha256":identity.hexdigest(),"image_bytes":len(image),
            "application":"ZZFractal" if a.fractal else "zzarm-debug","standalone_release":a.release,
            "relocations":rel,"entry":entry,"arm_execution_verified":False,
            "files":{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (elf,binary,OUT/"zz_payload.h")}}
    if a.sdl:record.update(application="SDLZZFractal",sdl=sdl_record)
    if a.fractal:
        names={1:"tile_dispatch",2:"row_complete",3:"tile_ready",4:"idle"}
        points=[]
        for line,text in enumerate((ROOT/"amiga/fractal/worker.c").read_text().splitlines(),1):
            match=re.search(r"ad_enter\(&core,(\d+),",text)
            if match:
                point=int(match[1])
                points.append({"id":point,"name":names[point],"file":"amiga/fractal/worker.c","line":line})
        manifest=OUT/"fractal-points.json"
        manifest.write_text(json.dumps({"build_id":build,"points":points},indent=2)+"\n")
        record["files"][manifest.name]=hashlib.sha256(manifest.read_bytes()).hexdigest()
    (OUT/"build.json").write_text(json.dumps(record,indent=2)+"\n")
    print(json.dumps(record,indent=2))


if __name__=="__main__":main()
