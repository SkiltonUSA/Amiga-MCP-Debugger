#!/usr/bin/env python3
"""Build Cortex-A9 SDK objects, 68k relay and IPC probe; does not deploy or flash."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]
SDK=ROOT/"amiga/arm_debug"
OUT=ROOT/".context/amiga/arm-debug"

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--container-command",help='JSON argv override, e.g. ["podman","--connection","nuflix-converter-root"]')
    parser.add_argument("--arm-cc",default=os.environ.get("ARM_CC","clang"))
    args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    compiler=shutil.which(args.arm_cc)
    if not compiler:raise SystemExit("Install Clang with ARM target support or set ARM_CC=arm-none-eabi-gcc")
    flags=["--target=arm-none-eabi"] if "clang" in Path(compiler).name else []
    flags += ["-std=c99","-mcpu=cortex-a9","-marm","-mfloat-abi=soft","-ffreestanding","-fno-builtin",
              "-O0","-g","-fno-omit-frame-pointer","-Wall","-Wextra","-Werror","-I",str(SDK)]
    # A checkpoint map must change identity when its source or compiler changes.
    identity=hashlib.sha256()
    for source in (SDK/"protocol.h",SDK/"core.c",SDK/"examples/arm_worker.c"):
        identity.update(source.read_bytes())
    compiler_version=subprocess.check_output([compiler,"--version"],text=True).splitlines()[0]
    identity.update(compiler_version.encode())
    identity.update(" ".join(flags[:-2]).encode())
    build_id=int.from_bytes(identity.digest()[:4],"big") or 1
    for name,source in [("arm_debug_core",SDK/"core.c"),("arm_worker",SDK/"examples/arm_worker.c")]:
        obj=OUT/(name+".o")
        subprocess.run([compiler,*flags,f"-DARM_WORKER_BUILD_ID=0x{build_id:08x}u",
                        "-c",str(source),"-o",str(obj)],check=True)
        header=obj.read_bytes()[:20]
        if header[:6]!=b"\x7fELF\x01\x01" or struct.unpack_from("<H",header,18)[0]!=40:
            raise SystemExit(f"Expected little-endian 32-bit ARM ELF: {obj}")
    spec=importlib.util.spec_from_file_location("amiga_launcher",ROOT/"scripts/amiga.py")
    launcher=importlib.util.module_from_spec(spec);spec.loader.exec_module(launcher)
    command=json.loads(args.container_command) if args.container_command else launcher.container_command()
    if not isinstance(command,list) or not command or not all(isinstance(x,str) and x for x in command):
        raise SystemExit("--container-command must be a nonempty JSON argv array")
    if not (ROOT/".tools/amiga-devbench/amiga-bridge/include/bridge_client.h").exists():
        raise SystemExit("Run make setup-amiga before building the 68k adapter")
    prefix=[*command,"run","--rm","--platform","linux/amd64","-v",f"{ROOT}:/work","-w","/work",launcher.PIN["image"]]
    for name in ("relay","bridge_adapter"):
        subprocess.run([*prefix,"m68k-amigaos-gcc","-std=c99","-m68020","-O2","-g","-Wall","-Wextra","-Werror",
                        "-Iamiga/arm_debug","-I.tools/amiga-devbench/amiga-bridge/include","-c",
                        f"amiga/arm_debug/{name}.c","-o",f".context/amiga/arm-debug/{name}.o"],check=True)
    subprocess.run([*prefix,"m68k-amigaos-ar","rcs",".context/amiga/arm-debug/libarm_debug_relay.a",
                    ".context/amiga/arm-debug/relay.o",".context/amiga/arm-debug/bridge_adapter.o"],check=True)
    probe_identity=hashlib.sha256()
    for source in ("protocol.h","core.c","relay.h","relay.c","bridge_adapter.c","examples/amiga_relay_probe.c"):
        probe_identity.update((SDK/source).read_bytes())
    probe_build=int.from_bytes(probe_identity.digest()[:4],"big") or 1
    subprocess.run([*prefix,"m68k-amigaos-gcc","-std=c99","-noixemul","-m68020","-O2","-g",
                    "-Wall","-Wextra","-Werror",f"-DAD_RELAY_PROBE_BUILD_ID=0x{probe_build:08x}u",
                    "-Iamiga/arm_debug","-I.tools/amiga-devbench/amiga-bridge/include",
                    "amiga/arm_debug/examples/amiga_relay_probe.c","amiga/arm_debug/core.c",
                    ".context/amiga/arm-debug/libarm_debug_relay.a",
                    ".tools/amiga-devbench/amiga-bridge/client/bridge_client.c",
                    "-lamiga","-o",".context/amiga/arm-debug/amiga-relay-probe"],check=True)
    points=[]
    for number,line in enumerate((SDK/"examples/arm_worker.c").read_text().splitlines(),1):
        match=re.search(r"ad_checkpoint\(&debug,(\d+),.*point: (\w+)",line)
        if match:points.append({"id":int(match[1]),"name":match[2],"file":"amiga/arm_debug/examples/arm_worker.c","line":number})
    (OUT/"arm-worker-points.json").write_text(json.dumps({"build_id":build_id,"points":points},indent=2)+"\n")
    artifacts=[OUT/name for name in ("arm_debug_core.o","arm_worker.o","libarm_debug_relay.a","arm-worker-points.json","amiga-relay-probe")]
    (OUT/"build.json").write_text(json.dumps({"build_id":build_id,"source_identity_sha256":identity.hexdigest(),
        "relay_probe_build_id":probe_build,"relay_probe_execution":"68k_host_demo",
        "compiler":compiler_version,"arm_execution_verified":False,"files":{
            p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in artifacts}},indent=2)+"\n")
    print(f"Built Cortex-A9 ELF objects + 68k relay library in {OUT}")
    print("Includes a 68k software relay probe, not a standalone ARM loader. No deployment or firmware flash.")
if __name__=="__main__":main()
