#!/usr/bin/env python3
"""Build SDL2 68k and ZZ9000 display probes; never deploy or start hardware.

The upstream library is pinned and patched locally for clipped window drawing.
The ARM payload/loader are the existing verified fractal implementation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
REV = "1eefa8f35c5ad4b63fa835e251e801a9315dff5c"
URL = "https://github.com/bdgscotland/libSDL2-amigaos3.git"
OUT = ROOT / ".context/amiga/sdl-probe"
PATCH = ROOT / "amiga/sdl_probe/sdl-window-safety.patch"
FLAGS = "-std=gnu99 -O0 -m68030 -noixemul -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -I./include -I./src -D__AMIGAOS3__"


def run(args, **kwargs):
    subprocess.run([str(x) for x in args], check=True, **kwargs)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", type=Path, default=ROOT / ".tools/amiga-sdl2")
    p.add_argument("--container-command", required=True, help="JSON container argv")
    a = p.parse_args()
    source = a.source.resolve()
    # Bind only workspace-owned source trees into the compiler container.
    rel = source.relative_to(ROOT)
    command = json.loads(a.container_command)
    if not isinstance(command, list) or not command or not all(isinstance(x, str) and x for x in command):
        p.error("container-command must be a nonempty array of strings")
    OUT.mkdir(parents=True, exist_ok=True)
    if not source.exists():
        run(["git", "clone", URL, source])
        run(["git", "-C", source, "checkout", "--detach", REV])
    if subprocess.check_output(["git", "-C", source, "rev-parse", "HEAD"], text=True).strip() != REV:
        raise SystemExit("SDL source is not at the reviewed pinned revision")
    changed = subprocess.check_output(["git", "-C", source, "diff", "--binary", "HEAD"], text=True)
    if not changed:
        run(["git", "-C", source, "apply", "--check", PATCH])
        run(["git", "-C", source, "apply", PATCH])
        changed = subprocess.check_output(["git", "-C", source, "diff", "--binary", "HEAD"], text=True)
    if changed != PATCH.read_text():
        raise SystemExit("SDL source has unexpected tracked edits; preserve and inspect them")
    image = json.loads((ROOT / "amiga/upstream.json").read_text())["image"]
    prefix = [*command, "run", "--rm", "--platform", "linux/amd64", "-v", f"{ROOT}:/work", "-w", "/work", image]
    # Stamp includes patch/compiler/flags. -B avoids stale objects from other builds.
    stamp = hashlib.sha256((REV + digest(PATCH) + image + FLAGS).encode()).hexdigest()
    stamp_path = source / ".sixies-build-stamp"
    if not stamp_path.exists() or stamp_path.read_text() != stamp or not (source / "libSDL2.a").exists():
        with (OUT / "sdl-build.log").open("w") as log:
            run([*prefix, "make", "-C", rel, "-B", "-j8", "native-build", "CFLAGS=" + FLAGS], stdout=log, stderr=subprocess.STDOUT)
        stamp_path.write_text(stamp)
    # This also validates/rebuilds the existing ARM image and its relocations.
    run([sys.executable, ROOT / "scripts/build_zz9000_debug.py", "--fractal", "--release",
         "--container-command", a.container_command], cwd=ROOT)
    payload = ROOT / ".context/amiga/fractal-release"
    payload_record = json.loads((payload / "build.json").read_text())
    if digest(payload / "zz_payload.h") != payload_record["files"]["zz_payload.h"]:
        raise SystemExit("ARM payload does not match its build record")
    out = OUT.relative_to(ROOT)
    includes = ["-I" + str(rel / "include"), "-Iamiga/arm_debug", "-Iamiga/arm_debug/zz9000", "-Iamiga/fractal"]
    cc = [*prefix, "m68k-amigaos-gcc", "-std=c99", "-O2", "-m68030", "-noixemul", "-Wall", "-Wextra", "-Werror"]
    run([*cc, "-DZZ_FRACTAL", "-DZZ_RELEASE", "-DIntuitionBase=ZZIntuitionBase",
         "-I" + str(payload.relative_to(ROOT)), *includes,
         "-c", "amiga/arm_debug/zz9000/launcher.c", "-o", out / "launcher.o"])
    for arm in (False, True):
        name = "sdl-arm-probe" if arm else "sdl-probe"
        extra = ["-DSDL_ARM_PROBE"] if arm else []
        objects = [out / "launcher.o"] if arm else []
        run([*cc, "-D__AMIGAOS3__", *extra, *includes,
             "amiga/sdl_probe/probe.c", "amiga/fractal/fractal.c", *objects,
             rel / "libSDL2.a", "-lm", "-lamiga", "-o", out / name])
        if (OUT / name).read_bytes()[:4] != b"\0\0\x03\xf3":
            raise SystemExit("Expected Amiga Hunk executable")
    record = {
        "sdl_repository": URL, "sdl_revision": REV, "sdl_flags": FLAGS,
        "compiler_image": image, "patch_sha256": digest(PATCH),
        "probe_source_sha256": digest(ROOT / "amiga/sdl_probe/probe.c"),
        "sdl_library_sha256": digest(source / "libSDL2.a"),
        "arm_payload_build": payload_record,
        "files": {name: {"bytes": (OUT/name).stat().st_size, "sha256": digest(OUT/name)}
                  for name in ("sdl-probe", "sdl-arm-probe")},
        "hardware_execution_verified_by_build": False,
    }
    (OUT / "build.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
