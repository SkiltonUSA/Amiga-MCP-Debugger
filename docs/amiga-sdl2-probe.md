# SDL2 and ZZ9000 feasibility probe

The 2026-10-08 A4000TX test successfully displayed a ZZ9000 Core1-generated
Mandelbrot image through bdgscotland's SDL2 AmigaOS backend, accepted native
mouse/keyboard events, and returned the ARM worker before releasing its memory.
This establishes a small graphics/input integration, not an OpenRCT2 port or
a general ARM implementation of SDL2. Audio and fullscreen are untested here.

## Source and build

Editable probe: `amiga/sdl_probe/probe.c`.
Upstream: https://github.com/bdgscotland/libSDL2-amigaos3,
revision `1eefa8f35c5ad4b63fa835e251e801a9315dff5c` (SDL headers 2.33.0).
The repository's zlib licence and source notices remain applicable.
Downloads, upstream binaries and build outputs are not committed.

```sh
python3 scripts/build_sdl_probe.py \
  --container-command '["podman","--connection","nuflix-converter-root"]'
```

The default source checkout is `.tools/amiga-sdl2`. For an existing checkout,
pass `--source` with a path inside this workspace. The builder requires the
pinned revision and exactly the recorded patch; it refuses unexpected edits.
It uses the pinned compiler image from `amiga/upstream.json`. SDL is built
with upstream's `-O0 -m68030 -noixemul`, without `SDL_OS3_DEBUG`; the probe
and existing launcher use `-O2`. No machine-global tools are installed.

The build also invokes the existing standalone fractal builder to regenerate
and validate the ARM image. Outputs under `.context/amiga/sdl-probe/`:

- `sdl-probe`: 68k colour-quadrant, presentation and input test.
- `sdl-arm-probe`: same frontend, with the existing embedded ARM fractal worker.
- `build.json`: upstream revision, patch/source/library/binary hashes and
  the embedded ARM build record. A successful build alone proves no hardware
  execution; acceptance logs are separate.

The SDL library and ARM launcher each own their Intuition library reference.
Only the launcher's compilation renames its library-base symbol to
`ZZIntuitionBase`, avoiding a symbol/lifetime collision with SDL. The launcher's
runtime mapping check, cache-off contract, allocation and teardown are unchanged.
No general SDL calls execute on ARM. ARM returns 32x16 tiles of big-endian
iteration counts; the 68k converts them to colours and calls SDL.

## Recorded local changes to SDL

`amiga/sdl_probe/sdl-window-safety.patch` contains two changes:

1. Remove the fallback which opened `Picasso96API.library` as a CyberGraphX
   library base. Their library vector tables are not interchangeable. Our
   machine has the compatible `cybergraphics.library 42.7` installed.
2. Replace the RTG update branch with clipped `WritePixelArray` calls through
   the window's layered RastPort, preserving individual update rectangles.
   Upstream's direct bitmap-base writes do not account for window position
   or occlusion, and its scaling decision compares outer window dimensions
   with the client surface. This test intentionally uses no scaling.
   The windows use `WA_GimmeZeroZero`, so drawing coordinates are already
   relative to the client area and must not add border offsets again.

File-debug logging is disabled at compile time to avoid references to the
upstream developer's `WORK:` volume and remove file I/O from measurements.
An initial colour test revealed an extra border offset in our first patch;
the final patch and final screenshots correct it.

## Run on the Amiga

Copy both executables to `RAM:` using the existing transfer tools. In Shell:

```text
Protect RAM:sdl-probe +e
Protect RAM:sdl-arm-probe +e
Stack 131072
Run >NIL: RAM:sdl-probe >RAM:sdl-probe.log
```

Click inside the window and press A to exercise input. Q, Escape or the close
gadget exits. The input phase exits automatically after 60 seconds. After
exit the log is readable; libnix holds its output file exclusively while open.

Before the ARM variant, close every other ZZ9000 Core1 application, including
standalone ZZFractal (which does not register with AmigaBridge). Then:

```text
Stack 131072
Run >NIL: RAM:sdl-arm-probe >RAM:sdl-arm-probe.log
```

Use the verified XX19c/XACP 1.7 hardware setup described in
`docs/amiga-arm-debugging.md`. This reuses the 128 KiB Exec-owned allocation;
no high ARM arena, new cache policy or firmware changes are introduced.

## Physical acceptance and limits

Acceptance evidence: `amiga/records/2026-10-08/sdl-probe/`.
Screenshots remain under `.context/amiga/sdl-probe/` in the development workspace.

- The final 320x240 window surface is ARGB8888, pitch 1280, on the existing
  1920x800 8-bit Workbench. Colours are therefore subject to the Workbench
  palette and RTG conversion; this is not a 32-bit screen benchmark.
- Native colour-quadrant display, click, A key and close gadget passed.
- ARM output completed 150 tiles with the established `fb32f6c6` iteration
  checksum; mouse, A/Q input and normal shutdown passed.
- Escape during an incomplete ARM render returned cleanly with `RET1` and
  the owned allocation released. Repeat launch was also checked.
- First complete ARM run: 7.284 seconds including polling, tile transfer,
  colour mapping and drawing. This is not isolated ARM compute throughput.
- First complete ARM run presentation: 30 full updates in 1,509,283 us
  (50.31 ms/update); 30 updates of 32x16 pixels in 20,530 us
  (0.684 ms/update). Input polling is included. The pixel buffer is already
  populated during these loops: these are redraw timings, not animation FPS.
- Existing regression checks: 8 fractal/package tests, 22 debugger tests,
  plus the MCP smoke test (138 tools). These supplement physical evidence.

This probe uses `SDL_GetWindowSurface`/`SDL_UpdateWindowSurfaceRects`.
The SDL texture renderer, AHI/Paula audio, arbitrary scaling, fullscreen,
32-bit screen performance and a broad SDL compatibility suite have not been
validated on this machine. OpenRCT2's ARM C++ runtime, dependencies, filesystem
service requests and full SDL behaviour still need separate work.

The current result supports continuing with this pinned, patched SDL baseline.
Retain bounded tile/rectangle transfers and measure a larger scene before
making game-performance claims. This test makes no persistent AmigaOS setup
changes; RAM-resident probes and logs disappear on reboot.

## Dedicated SDL2 source and SDK release

The patched library now lives in
[SDL2-AmigaOS3](https://github.com/SkiltonUSA/SDL2-AmigaOS3), a separate private
repository containing a pinned upstream library-source snapshot, the exact
patch, attribution, build wrapper, documentation and acceptance evidence.
[Preview v0.1.0](https://github.com/SkiltonUSA/SDL2-AmigaOS3/releases/tag/v0.1.0)
provides ZIP/tar.gz SDKs with headers, `libSDL2.a`, `libSDL2_test.a`, a small
example and checksums. Distribution version 0.1.0, SDL headers 2.33.0 and
upstream port version 0.7.0 are separate version numbers.

Clean packaging exposed three duplicate C2P assembly members left by repeated
builds in the earlier archive. The new wrapper deletes old archives/objects
before building. All 149 object files match the original build; both original
hardware probes relinked against the clean SDK are byte-for-byte identical to
the tested executables. The clean library hash is
`eb805aac436378a29a20fb50270e7c21cb8dbe61fdf6f317ff62418ba16be000`.
The separate small SDK example is compile/link checked only. Hardware
acceptance remains bounded by the tests above. No Amiga deployment or system
configuration change was needed to publish the SDK.

Release verification: `amiga/records/2026-10-08/sdl-probe/sdk-release.json`.
