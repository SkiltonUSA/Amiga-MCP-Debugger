# ZZ9000 Workbench fractal demo

`zzfractal` renders a 320×240 Mandelbrot image in a normal Workbench window.
The 68060 owns the interface and drawing; the ZZ9000's Cortex-A9 Core1 can
calculate the image in 32×16 tiles. A CPU mode runs the same C arithmetic on
the 68060. This is the first application from the
[ARM acceleration roadmap](amiga-arm-roadmap.md).

## Controls

| Control | Action |
| --- | --- |
| A or ARM button | Render the current view on ARM |
| C or CPU button | Render the current view on the 68060 |
| Click inside the image | Center there and zoom 2× |
| X, Escape or Cancel button | Cancel the current render |
| R or Reset button | Restore the full view and render with the selected processor |
| Q or close gadget | Quit, including while ARM is checkpoint-paused |

The status line shows the selected processor, progress, elapsed time and zoom
level. Zoom stops after six levels because this first version uses bounded
Q14 fixed-point arithmetic. The iteration limit is 128. Rendering uses the
Workbench palette through allocated shared pens; palette appearance can vary
with the screen's available colors. No new screen mode is selected.

Only one Core1 application can run at a time. Close this demo before launching
ZZQuake, ZZDoom, ZZDarkForces or another ARM application. The shared owner port
prevents duplicate launches of our debugger/demo, but unrelated games do not
observe it. Even CPU comparison mode keeps this application's ARM worker alive.

## Build

```sh
make test-amiga-fractal
make amiga-fractal-build
```

To override the workspace's container connection, invoke the builder directly:

```sh
python3 scripts/build_zz9000_debug.py --fractal \
  --container-command '["podman","--connection","nuflix-converter-root"]'
```

`make amiga-fractal-build` without an override uses the workspace's configured
container. The compiler image is pinned in `amiga/upstream.json`. Clang and
LLVM `ld.lld` build the embedded ARM payload. Outputs under
`.context/amiga/fractal/` include the Amiga Hunk executable `zzfractal`,
`zzarm.elf`, `fractal-points.json` and `build.json`. The build record describes
artifacts; physical acceptance is recorded separately.

Launch from an Amiga Shell with a 64 KiB stack:

```text
Stack 65536
RAM:SixiesDev/zzfractal
```

The no-argument form creates a session identifier from the Amiga clock and
task identity. Automated tests supply a fresh random nonzero hexadecimal
session explicitly: `zzfractal <session> RUN`. This identifier prevents
accidental stale-session reuse; it is not a security credential.

The frontend works without a connected Mac. If AmigaBridge is present, it
registers as `zzfractal` and exposes debugger and application hooks. The tested
XX19c/XACP 1.7 setup, P96 and the ZZ9000 are required, including for the CPU
comparison. This executable is not an emulator/AGA-only fallback.

## Computation and memory contract

The implementation reuses the verified launcher in
`amiga/arm_debug/zz9000/launcher.c`, its allocation probe and assembly entry.
It reserves 128 KiB through Exec in real free ZZ9000 Fast RAM, validates the
Amiga-to-ARM translation using nonce-derived words, and releases the short
P96 mapping-probe lock before opening the application window. It does not
claim unallocated DDR, alter the firmware or enable ARM MMU/caches.

The ARM payload remains below offset `0x8000`. The existing debug page ends at
`0x8540`. Fractal requests start at `0x8800`, cancellation at `0x8840`, responses
at `0x8880`, and the 1 KiB pixel exchange buffer at `0x8900`. These are offsets
inside that allocation, never fixed board addresses. Compile-time checks keep
the layout clear of the debugger and at least 16 KiB below the stack top.

There is one outstanding tile at a time. Each request contains the session,
generation, view, iteration limit and tile coordinates. Request/response
sequence words commit the data after publication barriers. Host and ARM write
separate cache lines. ARM uses barriers with caches disabled; the 68k uses
`CacheClearE` on exchanged ranges. The 153,600-byte complete iteration image
belongs to the 68k and lives outside the shared worker block.

Cancel, reset and zoom change the generation. The worker checks cancellation
between at most 64 orbit steps/escape checks, including while checkpoint-paused.
The frontend waits for the outstanding response before reusing its request and
pixel exchange buffer, and rejects results from older generations. CPU work
is also incremental, bounded to 8192 orbit steps/escape checks per event-loop
iteration. All graphics and OS calls stay on the 68k.

Q14 coordinates use signed division with truncation toward zero. Component
escape checks bound multiplications before they occur. Counts are 16-bit;
wire pixels are big-endian, while the debugger's registered ARM tile memory
is native little-endian. `fractal.c` is shared by the native reference tests,
68k and ARM builds. An independent Python integer implementation provides the
oracle. Frame hashes use FNV-1a over row-major big-endian counts.

Quit requests worker shutdown even when paused. After the ARM epilogue's
`RET1` marker, the host synchronously resets Core1 to idle before freeing shared
memory, preserving the verified launcher's teardown contract.

## Debugging and automation

Attach with `amiga_arm_attach(client="zzfractal", points_path=".context/amiga/fractal/fractal-points.json")`.
This checks the map against the running build ID. The four checkpoints are:

| ID | Meaning | Watched values |
| --- | --- | --- |
| 1 | Tile dispatch | Generation, tile x/y, iteration limit, completed pixels, request sequence |
| 2 | Row progress | Same, with updated pixel count |
| 3 | Tile ready, before publication | Same, with 512 completed pixels |
| 4 | Idle, awaiting a request | No watched values |

Pause, checkpoint stepping, breakpoint control, bounded tile-memory reads,
logs, continue and detach use the existing ARM MCP tools. Idle is a real
checkpoint: stepping after tile publication can reach idle before the next
dispatch. This is cooperative debugging, not ARM instruction stepping.

`amiga_call_hook(client="zzfractal", hook="fractal", args="status")` reports
progress, generation, outstanding request, checksum, elapsed time and measured
cancellation acknowledgement time. Other bounded commands are `arm`, `cpu`,
`cancel`, `reset`, `zoom x y`, `save` and `quit`. `save` requires a complete
image and writes raw big-endian iteration counts to
`RAM:SixiesDev/fractal-counts.bin`; it does not yet export a user-facing image
format. Ensure that directory exists for this test/export operation.

Explicit hardware acceptance, after staging the binary and confirming Core1
is idle:

```sh
.tools/amiga-venv/bin/python tests/amiga/fractal/live_fractal.py
```

The script compares every pixel for default and zoomed CPU/ARM renders,
exercises twenty render/zoom/cancel cycles, reads a completed ARM tile through
MCP, steps exactly one checkpoint, cancels while paused and performs ten
paused launch/quit cycles. It records results and leaves the application
stopped. Log downloads use 2048-byte reads because larger bridge reads and
long DOS-tool output have known limits.

## Performance scope

Displayed times include cooperative scheduling, tile exchange and Workbench
drawing. The frontend yields one OS tick each loop; CPU mode additionally
splits computation across bounded work slices. Debug checks and IPC add
unequal overhead to the two paths. These are interactive-demo measurements,
not isolated kernel benchmarks or general 68060-versus-ARM performance claims.

Cache-enabled execution, floating point/NEON, deeper zoom, Julia sets,
palette controls, image export, a reusable acceleration service and ARexx
application commands remain later work. The current Shell launcher and MCP
hook are not an ARexx host. Milestone 2 will separate computation, exchange,
display and scheduling costs before drawing broader performance conclusions.

## A4000TX installation and acceptance

Installed on 8 October 2026 in `SD032G:Dev/ZZFractal/`. From an Amiga Shell:

```text
Execute SD032G:Dev/ZZFractal/Start-Fractal
```

The script has execute/script protection, sets a 65536-byte stack, creates
`RAM:SixiesDev` when needed and redirects diagnostics to
`RAM:SixiesDev/fractal.log`. It is not added to startup. Its source is
[Start-Fractal](../amiga/scripts/Start-Fractal).

The final physical acceptance build is `3653742807` (`0xd9c7b0d7`).
All 76,800 iteration counts matched the independent reference for each of
four renders:

| View | ARM time | 68060 time | Exact frame hash |
| --- | --- | --- | --- |
| Default | 7.523 s | 15.081 s | `fb32f6c6` |
| Centered 2× zoom | 11.133 s | 30.094 s | `a0004522` |

These are individual runs, not the repeated, isolated benchmarks planned for
milestone 2. Twenty render/zoom/cancel cycles passed, including rejection of
old results. Debugger tile-memory reads matched the expected little-endian
counts; checkpoint stepping advanced exactly once. Cancellation while paused
and ten paused launch/quit cycles passed. Every exit recorded the ARM epilogue
and safe allocation release. Free Chip RAM was unchanged across the suite;
free Fast RAM increased by 3280 bytes, so no net free-memory loss was observed.
This is bounded stress evidence, not proof against all long-term leaks.
The installed SD launcher, CPU/ARM keyboard selection, Escape cancellation,
window movement while paused and the build-matched checkpoint map were also
verified. A captured window screenshot shows the completed fractal.

The Mac suite has 27 tests (22 debugger/loader and five fractal), including
undefined-behavior sanitizer checks and a regression against excessive idle
status publication starving the 68k snapshot reader. The MCP smoke test
passed with 138 tools. A separate checkout reproduced the executable, embedded
ARM image and checkpoint map byte for byte; ELF debug source paths differ.

[Hardware acceptance and raw calls](../amiga/records/2026-10-08/fractal/acceptance.json)
include the build identity, timings, launch/cancel results and pre/postflight
state. No firmware, OS startup or cache configuration was changed.

## Standalone distribution 0.1

A separate release build is packaged as `ZZFractal-0.1-XX19c.lha` and
`ZZFractal-0.1-XX19c.zip`. It is a 26,112-byte Amiga Hunk executable with the
ARM program embedded, plus classic Workbench tool/drawer icons, documentation,
third-party runtime notices and file checksums. The tool icon sets a 65536-byte
stack. Extract anywhere, open the drawer and double-click `ZZFractal`.
Workbench startup displays the XX19c/XACP requirement and asks the user to
close other ARM applications before choosing Start. The firmware register
alone cannot reliably identify this variant.

This build omits the bridge client and relay. It needs no Mac, MCP server,
network, ARexx or separate ARM payload. It still needs the same ZZ9000/P96 and
firmware configuration as the developer build. It is an experimental preview
for the verified hardware configuration, not a claim of general Amiga support.
The runtime rejects an inadequate Shell stack; Shell users should set
`Stack 65536` before running it. Its `$VER:` string is `ZZFractal 0.1`.

```sh
python3 scripts/build_zz9000_debug.py --fractal --release \
  --container-command '["podman","--connection","nuflix-converter-root"]'
python3 scripts/package_fractal.py
```

`make amiga-fractal-release` uses the configured container and performs both
steps. The packager validates the executable against its release build record,
checks the icon type/stack, and includes only an explicit file list. The ZIP is
deterministic and preserves executable permission. Outputs stay in
`.context/amiga/fractal-release/dist/`. Archives are release assets rather than
committed build products.

Icon sources are `amiga/distribution/make-fractal-icons.c`; the generated
classic `.info` files are checked in under `amiga/distribution/`. The helper
uses its own mathematical two-bitplane artwork, not OS default color-icon
artwork. Rebuild it with the pinned 68k compiler and run it on an AmigaOS 3.2
packaging machine with the package drawer as its argument. It round-trips the
icons through icon.library and reports their type, dimensions and stack.

The LHA was created with native LhA 2.15, with the package root as current
folder, then integrity-tested and extracted into a fresh RAM drawer:

```text
lha -r a RAM:ZZFractal-0.1-XX19c.lha ZZFractal ZZFractal.info SHA256SUMS.txt
lha t RAM:ZZFractal-0.1-XX19c.lha
lha x RAM:ZZFractal-0.1-XX19c.lha RAM:FractalFresh/
```

All eight extracted files matched the ZIP manifest. The extracted drawer was
renamed to `PortableDemo`; native Workbench startup and rendering still worked,
with zero bridge clients registered. Shell launch of the same binary produced
the reference frame hash `fb32f6c6`, normal ARM epilogue and exit 0. The
independent repository checkout reproduced the executable and ZIP byte for
byte. Thirty native tests and the MCP smoke suite passed.

[Distribution acceptance](../amiga/records/2026-10-08/fractal-release/acceptance.json)
records archive checksums, raw launch evidence and the validation boundary.
The standalone copy is installed at `SD032G:Dev/ZZFractal-0.1/ZZFractal`; its
sibling `.info` and drawer icon enable normal Workbench launch. The earlier
developer copy remains in `SD032G:Dev/ZZFractal/` for MCP debugging.
**Zero bridge clients no longer implies that Core1 is idle:** check tasks,
windows and the owner port, and close either edition before another ARM app.
