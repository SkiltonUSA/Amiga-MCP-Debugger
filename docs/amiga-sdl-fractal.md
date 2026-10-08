# SDL ZZFractal 0.3

An interactive SDL2 frontend to the existing owned-memory XX19c Mandelbrot
worker. The 68k runs SDL and AmigaOS; the ZZ9000 Core1 computes bounded tiles.
A developer build exposes the existing cooperative debugger and an application
hook. The standalone release embeds the worker and needs no Mac or bridge.

## Controls and screens

A/C selects ARM/CPU and renders; click the image to center and zoom 2x;
right click zooms out; arrows pan by a quarter image; I/O changes the iteration
limit between 32, 64, 128 and 256. R resets, X/Escape cancels, Q/close quits.
M iconifies to a Workbench AppIcon; double-click restores. Rendering can
continue while iconified. The Q14 precision limit and bounded view center
remain unchanged from the original fractal demo.

When idle, F5 uses Workbench, F6 opens a temporary 16-bit RTG screen and F7
requests 32-bit pixel storage. The screen's colour depth can differ from its
storage width; the application checks CyberGraphX bytes per pixel. Unsupported
modes return to Workbench. Neither Workbench preferences nor the default
public screen are changed. The app closes its own screen on exit.

SDL's new `SDL_AMIGA_PUBLIC_SCREEN` hint selects an existing named RTG public
screen for window creation. Missing/incompatible explicit screens report an
error. An unset or empty hint selects the default public screen. A public-screen
lock spans native window creation. This supplements the published window
RastPort fix; it does not implement arbitrary fullscreen scaling.

## Reusable computation transport

`amiga/compute/xx19c.h` / `.c` contain a small SDL-independent client. It accepts
only launcher-owned memory, a cache/barrier interface and a monotonic clock.
`zc_submit` commits one immutable job; `zc_poll` checks its sequence,
generation, coordinates and response version before copying pixels.
`zc_cancel` advances the generation; the outstanding response must still be
drained before another request can reuse the shared buffer. Session identity
is checked by the worker. Invalid responses/timeouts latch an error; memory
remains owned until the launcher stops Core1. Sequence/generation wrap is
rejected. A pending job times out after ten seconds; long debugger pauses
while rendering must be cancelled or resumed within that interval.

The first typed operation is a Mandelbrot tile; the second is a read-only
clock sample. This is a reusable transport boundary, not a generic arbitrary
function-call/runtime service. The 128 KiB allocation, runtime mapping probe,
cache-off ARM contract and return-before-free teardown are unchanged.

The optional `FF_TIMING` worker extension uses the existing response's unused
words (20..44) for a 64-bit compute duration, publication timestamp, timer
control, `TIM3` version marker and FNV-1a integrity word. Request word 36 chooses
tile (0) or clock sample (1). The checksum covers session, sequence, generation,
coordinates, timing metadata and the big-endian iteration counts. Clock replies
validate the same header without pixels. The client keeps the request owned
and returns WAIT on a checksum mismatch, then rechecks on subsequent scheduler
passes until the existing ten-second deadline. It never displays that copy or
resends the job. The worker and client must be rebuilt together; the original
non-timing fractal protocol/build remains supported.

## Measurements

The ARM reads the Zynq global timer at `0xf8f00200` using upper/lower/upper
rollover protection; it never writes the timer or changes its configuration.
The map and read sequence follow the AMD/Xilinx standalone sources:
[xparameters_ps.h](https://github.com/Xilinx/embeddedsw/blob/master/lib/bsp/standalone/src/arm/cortexa9/xparameters_ps.h)
and [xtime_l.c](https://github.com/Xilinx/embeddedsw/blob/master/lib/bsp/standalone/src/arm/cortexa9/xtime_l.c).

Two timestamp exchanges about one second apart calibrate timer ticks against
SDL's Amiga E-clock. The record preserves the calibration span, exchange
uncertainty and timer control. A disabled/changed timer disables its estimate.
No nominal CPU frequency is assumed. ARM compute includes timing instrumentation
around each bounded kernel slice; paused/debugger/service time is excluded.
The 68k kernel is timed around its bounded 1024-step slices (8192 in the legacy diagnostic scheduler). Compiler settings, cache
policy and instrumentation differ: these are application measurements, not a
fair general-purpose CPU benchmark.

- `wall_us`: whole render, including UI and cooperative scheduling.
- `cpu_us`: time inside the 68k arithmetic slices.
- `arm_us_est`: timed ARM slices converted using the calibrated timer rate.
- `transfer_us`: 68k request/result synchronization, response polling and copy.
- `colour_us`: 68k iteration-to-colour mapping into the SDL surface.
- `draw_us`: SDL surface updates, including progress display during rendering.
- `roundtrip_us`: sum of ARM request-to-response intervals, overlapping compute
  and transfer. **Do not add all these columns together.**

Completed measurements freeze while the app is idle. Iconify/restore outside
rendering does not change them. Both paths use a two-millisecond ready-work
budget and a private timer.device request for a one-millisecond relative yield.
ARM polling stops at the first WAIT; it does not busy-spin. Idle windows and
failure to open the private timer fall back to Delay(1). OS calls, SDL drawing
and bridge hooks may exceed the work budget: this is cooperative scheduling,
not a real-time latency guarantee. ARM MMU/caches remain disabled. The kernel
uses Clang `-O2`, worker/debugger `-O1`, 68k GCC `-O2`, and the unchanged SDL2
0.2.0 library retains `-O0`. A shorter total render does not by itself establish
a faster arithmetic kernel.

The kernel keeps orbit state in locals across a slice, uses exact unsigned Q14
shifts for bounded nonnegative squares, and preserves signed division for xy.
An exact repeated integer state terminates a nonescaping orbit at its limit;
there is no approximate cardioid test. ARM slices allow up to 512 steps but stop
at each 32-pixel row boundary, preserving every row checkpoint and frequent
cancellation/debugger service. Host oracle tests cover several slice budgets,
edge views, fixed points and every row boundary.

Developer hooks separate `metrics`, `clock` and `scheduling` so each response fits
the bridge buffer. `scheduling` includes actual yield time, loop count, maximum
observed loop gap and integrity rechecks. `schedule legacy` / `schedule paced`
select the scheduler only while idle. The standalone build has no bridge hooks.
The timer follows [timer.device TR_ADDREQUEST](https://developer.amigaos3.net/autodocs/timer.device/TR_ADDREQUEST.html);
its request is completed before reuse or deletion.

## Build and test

Build the pinned SDL library in the public SDL2 repository first, then:

```sh
python3 scripts/build_zz9000_debug.py --sdl \
  --sdl-source .context/SDL2-AmigaOS3 \
  --container-command '["podman","--connection","nuflix-converter-root"]'
make test-amiga-sdl-fractal test-amiga-fractal test-amiga-arm test-amiga
```

The builder checks `amiga/sdl_fractal/sdl.json` against the actual static library.
Developer output: `.context/amiga/sdl-fractal/sdlzzfractal`, with build identity,
embedded ARM payload and matching checkpoint map. Use a 131072-byte Shell stack.
Adding `--release` omits the bridge. The public repository's
`scripts/build_fractal.py` builds its self-contained standalone source example.

The `sdlfractal` client exposes an `sdlfractal` hook with `status`, `metrics`,
`arm`, `cpu`, `cancel`, `reset`, `zoom x y`, `pan x y`, `iterations n`, `out`,
`screen 0/16/32`, `iconify`, `restore`, `cover`, `uncover`, `save` and `quit`.
`save` writes complete big-endian iteration counts to
`RAM:SixiesDev/sdl-fractal-counts.bin`. The directory must exist.

After staging and explicitly establishing that Core1 is idle, the physical
acceptance harness is `tests/amiga/sdl_fractal/live_sdl_fractal.py`. It compares
all 76,800 pixels against the independent Python oracle, exercises display
modes, cancellation, iconification and overlapping/moved windows, and stops
the application. The native transport tests use address/undefined-behavior
sanitizers; these host fixtures are separate from hardware evidence.

## 0.2 acceptance record

Physical acceptance on 2026-10-08 passed on the A4000TX / TF4060 / XX19c.
All 76,800 iteration counts matched the independent oracle in each of 14
completed renders: ARM/68k, default/zoom/pan, changed iterations, Workbench
and temporary 16-/32-bit screens. Six rapid cancel/zoom cycles, rendering
while iconified, restore and overlapping windows passed. The window-move
command was acknowledged; a later standalone test independently verified
the window position at (680,300). The harness now waits for asynchronous
Intuition movement before checking its position.
Idle timing stayed unchanged. Shutdown reported RET1 and released the owned
allocation; the bridge remained responsive with no registered client.

Native CGX readback sampled 475 pixels per true-colour mode: zero mismatches;
16-bit maximum channel difference 7 (tolerance 8), 32-bit difference 0.
Bridge screenshots report the P96 dummy 8-bit bitmap and lose true colour on
these screens; they are not the colour acceptance test. One bridge file-read
returned no response and succeeded on a bounded read-only retry. No commands
were blindly retried.

| Default view | Wall | Compute | Transfer | Colour mapping | SDL drawing |
| --- | ---: | ---: | ---: | ---: | ---: |
| ARM | 7.575 s | 2.894 s estimate | 94 ms | 38 ms | 745 ms |
| 68060 | 17.427 s | 2.233 s | 0 | 39 ms | 1,478 ms |

The ARM arithmetic slices are slower in this configuration despite the shorter
whole-render time. Scheduling granularity, compilers, instrumentation and
cache policy differ. At that stage kernel optimization and scheduling work were pending;
these measurements must not be presented as a raw processor speed ratio.
The read-only timer calibrated to 333,326,829 ticks/s over 1,014,070 us,
with a 485 us exchange uncertainty.

Local validation: 4 transport tests with sanitizers, 8 fractal/package tests,
22 debugger tests, the 138-tool MCP smoke suite, and ARM/68k builds passed.
The standalone build omits MCP/relay linkage and embeds its ARM payload.

Audio, texture-renderer compatibility, arbitrary resizing/scaling, other
hardware and cache-enabled ARM execution remain outside this milestone.

## 0.2 standalone distribution acceptance

The 1,153,524-byte standalone executable passed native LHA integrity testing
and fresh extraction. Every one of the LHA's 10 files matches the ZIP.
Extracted and installed executable MD5: `364ec823dea2615b6dc3fb5e91319177`.
Shell execution produced matching default CPU/ARM checksums `fb32f6c6`,
handled cancellation and discarded its old response. Ctrl-C shutdown returned
RET1, exited 0 and released its allocation.

The app is installed at `SD032G:Dev/SDLZZFractal-0.2/SDLZZFractal` with native
Workbench icons and a 131072-byte stack. Workbench launch through WBLoad,
the firmware-confirmation requester, default ARM rendering, an independently
verified move to (680,300), and Q quit passed. The owner port disappeared
and the bridge remained responsive after quit. No OS/firmware/preferences
or startup files changed. Close other Core1 apps before launching it.

![Installed Workbench application](../amiga/records/2026-10-08/sdl-fractal/standalone-workbench.png)

**Remaining input check:** native iconification passed, and developer-hook
restoration passed. Synthetic mouse input on the native Workbench AppIcon
stalled the bridge, consistent with its documented Intuition icon-drag
limitation. After recovery, Workbench ARexx `ICON ROOT "SDL ZZFractal" OPEN`
returned OK without restoring the window. Physical double-click restoration
therefore remains unverified; do not describe it as a completed test. Recovery
needed no reboot, and the standalone app subsequently shut down cleanly.
The Workbench ARexx command follows the
[AmigaOS documentation](https://wiki.amigaos.net/wiki/Workbench_ARexx_Port).

## 0.3 performance and shared-memory validation

On the same A4000TX on 2026-10-08, three default renders per mode gave these
medians. Both builds use the same SDL2 SDK 0.2.0 static library (SHA-256
`bf90c1f12536df97bfc85e838cc2fa0047fc23f37d5128dbb89bb38dddec386e`).

| Measurement | 0.2 baseline | 0.3 | Reduction |
| --- | ---: | ---: | ---: |
| ARM whole render | 7.609 s | 3.848 s | 49.4% |
| ARM compute estimate | 2.899 s | 1.472 s | 49.2% |
| 68060 whole render | 17.292 s | 4.328 s | 75.0% |
| 68060 compute | 2.235 s | 1.692 s | 24.3% |

These are developer builds with the same sequential MCP polling workload.
The 0.3 zoom runs took 5.665 s on ARM and 9.695 s on 68k (one run each).
The baseline harness initially mislabeled a CPU zoom; the preserved baseline
record corrects it. Only correctly identified repeated default runs enter
this table. Every measured frame matched the independent Q14 oracle; full
76,800-count captures were compared for the first run in each mode and each
zoom. Whole-render timings overlap compute, transfer and drawing.

Reducing the old one-tick wait initially exposed intermittent mixed old/new
results. A held diagnostic build was paused through the ARM debugger after a
failure: its complete 512-pixel local tile and shared wire tile agreed with
the oracle, while the host's captured tile differed. Whole-68060-data-cache
and CacheClearU diagnostic experiments did not eliminate it and were reverted.
The exact hardware/cache source of the transient visibility is not isolated.
The final transport therefore validates request identity, timing and pixels
together before consumption, with bounded read-only rechecks; it does not rely
on an arbitrary delay, global cache changes or re-executing jobs. Default ARM
benchmark runs recorded 16, 2 and 6 rechecks respectively across 150 tiles.
All final benchmark hashes passed. FNV-1a is a noncryptographic consistency
check, not a security boundary.

Kernel output, slice-budget invariance, every row checkpoint, exact fixed
points, stale-job/stale-timing rejection, transient visibility, persistent
corruption timeout, cancellation/draining and sequence exhaustion have host
coverage. Local validation passed 10 fractal/package tests, 8 transport tests,
22 cooperative debugger tests and the 138-tool MCP smoke suite. ARM/68k
component, native fractal, SDL developer and standalone builds passed.
The public flat example also carries 15 independently runnable kernel and
transport tests. Native/simulator tests are separate from physical acceptance.

Final 0.3 physical acceptance passed 14 complete pixel-by-pixel oracle checks
across ARM/CPU, zoom, pan, iteration changes, Workbench and both true-colour
modes, followed by 20 additional default ARM hash checks. The latter recorded
188 integrity rechecks across 3,000 tiles with no wrong final checksum. Native
CGX readback found zero out-of-tolerance samples in 475 samples per mode.
Six rapid cancel/zoom cycles and cancellation with Core1 paused through the
cooperative debugger passed. Iconified rendering, hook-driven restore,
overlapping windows and an independently observed window move passed. RET1,
exit 0, allocation release and subsequent bridge responsiveness were verified.
Physical Workbench AppIcon double-click restore remains an outstanding input
check from 0.2; hook-driven restore is not evidence for that gesture.

The 0.3 standalone Hunk is 1,156,000 bytes, SHA-256
`533b2ee03a8fcc07933202766ca345b7438c9d8386521b784407a39d4b965b87`,
MD5 `69257b060f68f6709eb8b7b0ba1470f7`. Native LHA testing and fresh extraction
passed; all ten LHA members match the ZIP, including executable and notices.
CLI execution verified default ARM and CPU hashes, cancel/discard, Q and clean
RET1 / exit 0. It is installed separately at
`SD032G:Dev/SDLZZFractal-0.3/SDLZZFractal`; version 0.2 remains available.
Workbench startup and its Start confirmation were exercised on the installed
copy. SDK 0.2.0 remains the dependency; this is an application-only release.

![Installed SDL ZZFractal 0.3](../amiga/records/2026-10-08/sdl-performance/standalone-workbench.png)
