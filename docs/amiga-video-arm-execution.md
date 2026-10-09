# ZZVideo ARM execution investigation

2026-10-08. Source review followed by physical profiling and an instruction-cache
experiment for the user's 25 fps requirement. Temporary developer builds ran
from `RAM:ZZVideoPerf`; the installed ZZVideo 0.1 was preserved. No firmware,
startup or driver changes were made. Core1's original SCTLR was restored on exit.

The first experiment, **instruction caching on Core1 with data caching and the
MMU still off**, is now implemented and physically verified. It improved median
playback only **1.86%**, from 2.112 to 2.152 fps. The existing
player remains the correctness reference. The approximately 132–145 ms ARM
decode/colour cost must fall below the 40 ms total budget, and the separate
88 ms display cost also needs work. ARM-only acceleration cannot by itself
meet the playback target.

## What the current code actually executes

| Component | Source / observed behaviour | Implication |
| --- | --- | --- |
| Entry | `amiga/arm_debug/zz9000/entry.S` requires SCTLR bits `0x1005` clear on entry, saves firmware registers/stack, uses an application stack and returns through RET1 | Default remains uncached; explicit video experiment enables only I and restores SCTLR |
| Worker | `amiga/video/worker.c` checks Core1, processes one request at a time, decodes and converts before hashing/publishing | No overlapping frame production and presentation |
| Decoder | `amiga/video/decoder.c`, pinned PL_MPEG | Integer MPEG-1 decode, then scalar YCbCr-to-ARGB conversion |
| Compiler | `scripts/build_zzvideo.py`: Cortex-A9, ARM state, `-O2`, soft-float | Optimisation is already enabled; no NEON/VFP instructions in the examined payload |
| Runtime | `amiga/video/runtime.c` | Byte loops for memory helpers; fixed 32-iteration software integer division |
| Host | `amiga/video/client.c` | Reads the full frame into 68k memory and hashes it twice before SDL display |

The accepted hardware register sample is SCTLR `0x08c50878`: I=0, C=0, M=0,
but Z=1. Branch prediction is already enabled; proposing to enable it again
does not address the missing caches. This is from the previous acceptance
logs, not a fresh register read.

The inspected XX19c `ZZ9000OS/src/core2.c` enables CP10/CP11 and FPEXC on
Core1, and uses L1-only maintenance before entering the application. Its
comments explicitly prohibit Core1 from operating the shared PL310 controller.
This is useful firmware evidence, but is not a live NEON capability/state
test. Core0 remains assigned to firmware/services.

Arm documents independently controlled instruction/data caches, and the
SCTLR I/C/M/Z controls in the [Cortex-A9 TRM, sections 4.3.9 and 7.1](https://documentation-service.arm.com/static/5f0377b2cafe527e86f5c247).
AMD's [Cortex-A9 cache implementation](https://github.com/Xilinx/embeddedsw/blob/master/lib/bsp/standalone/src/arm/cortexa9/xil_cache.c)
distinguishes `Xil_L1ICacheEnable` from the generic `Xil_ICacheEnable`, which
can also enable shared L2. Neither should be copied blindly into this loader.

## Generated-code comparison

This table records the earlier compile-only review. The later NEON hardware
measurements below supersede its untested status.

All variants compile the same current sources and generated vendor header,
using the same link layout and garbage collection. The experimental build ID
is 1; these are not distributable builds. Sizes below are emitted function
bytes, **not time measurements**. Source/compiler hashes and flags are in
`amiga/records/2026-10-08/video-arm-review/codegen.json`.

| Variant | Function text total | Colour conversion | IDCT | Motion compensation | Link result |
| --- | ---: | ---: | ---: | ---: | --- |
| `-O2`, scalar baseline | 20,808 | 584 | 632 | 1,080 | Pass |
| `-O3`, scalar | 20,836 | 592 | 632 | 1,080 | Pass |
| `-Os`, scalar | — | — | — | — | Missing `__aeabi_memclr8` helper |
| `-O3 -mfpu=neon -mfloat-abi=softfp` | 23,408 | 584 | 1,280 | 2,636 | Pass, not run |

The NEON candidate contains vector integer operations in IDCT and motion
compensation, including 168 and 92 static VFP/NEON-family instructions
respectively (including register saves/restores). It still has **zero vector
instructions in colour conversion**. A compiler flag alone therefore does not
vectorise the complete pipeline. More instructions or larger code does not
predict whether a function is faster.

The baseline disassembly confirms:

- Colour conversion handles 2x2 groups with scalar integer arithmetic,
  saturating `USAT`, repeated loads and three byte stores per output pixel.
  A specialised aligned packed-store or NEON implementation is worth timing.
  It must preserve byte order, rounding and the existing fourth-byte contents.
- IDCT and motion compensation are scalar. Their vector variants need actual
  output comparisons and hardware timing, not just successful compilation.
- Macroblock row calculation calls `__aeabi_idiv` at two emitted call sites.
  The helper always performs 32 iterations. An exact faster divider or avoiding
  repeated division is a secondary candidate; its timing share is unknown.
- The out-of-line byte memory helpers exist, but their observed direct call
  sites are mainly initialisation/header work. Do not assume replacing them
  fixes steady-state frame time without profiling.

Local disassemblies and the reproduction script are under
`.context/amiga/video-arm-review/`. They are analysis artifacts, not launchable
Amiga packages. The `-Os` failure is recorded, not patched into production.

## Bounded experimental sequence

Steps 1 and 2 below have been performed. The later user-requested NEON
measurement also exercises compiler-vectorised kernels from step 4; see the
separate NEON results below. Private data caching and card-local presentation
remain pending.

1. **Measure stages without changing execution policy.** Split MPEG decode,
   colour conversion and ARM integrity hashing into separate timings. Measure
   host transfer/checking separately from drawing. Keep raw timer ticks and a
   longer frequency calibration; existing ARM timings use a short estimate.
   Use identical I/P/B-frame clips and matching reference hashes.
2. **Test instruction caching alone.** Implement an explicit experimental
   launch mode. Check initial CPU/state, publish and synchronise immutable
   relocated code before entry, perform only appropriate local instruction
   maintenance with barriers, save/restore the original SCTLR, and verify
   I=1/C=0/M=0 during the run and restored state before RET1. Preserve the
   existing data visibility protocol and synchronous idle/reset teardown.
   Audit firmware/shared-L2 visibility and privilege requirements before live
   use. The current entry and worker reject I=1: relaxing only one check is
   not an implementation.
3. **Introduce cacheable private data if needed.** Use application-owned,
   suitably aligned pages for decoder state, reference frames and stack, with
   explicit uncached shared control/output mappings. Define inner and outer
   attributes and handoff maintenance. Current AllocAbs ownership is only
   64-byte aligned; do not map neighbouring owners' bytes as private cacheable
   pages. No guessed DDR allocation or global PL310 operation.
4. **Test specialised integer/NEON kernels.** Establish live feature access,
   memory alignment/type suitability and firmware register preservation.
   Start with the stage measured as most expensive. Keep scalar output as an
   oracle, including B-frame drain and replay. Compilation success alone does
   not establish safe SIMD execution or speed on this memory mapping.
5. **Integrate card-local presentation.** Obtain an owned, supported framebuffer
   or overlay mapping; preserve Workbench layering and buffer lifetime. Then
   consider double buffering to overlap work. Pipeline throughput remains
   bounded by its slowest stage.

Every live variant must retain frame verification, repeated launch/quit,
paused-debugger shutdown, RET1 and memory release checks. Compare repeated
runs with the baseline and record mode, clip, build identity and all stage
timings. A decoder-only benchmark must be labelled as such. The final gate
remains sustained 320x240 at 25 fps with correct displayed output; none of the
compile-only results here demonstrates that rate.

## Physical instruction-cache results

Three alternating 25-frame runs per mode at 320x240 used identical generated
MPEG-1 I/P/B-frame input. Each accepted frame matched the native decoder hash.
Median stage times are milliseconds per frame; ARM values use a two-second
global-timer frequency estimate (about 333 MHz), not a measured CPU frequency.

| Stage | Baseline | Instruction cache |
| --- | ---: | ---: |
| MPEG decode | 85.53 | 78.16 |
| YCbCr-to-ARGB | 54.94 | 53.91 |
| ARM integrity hash | 33.95 | 33.90 |
| Host frame pull/copy | 112.92 | 112.12 |
| Host integrity/reference hashes | 87.20 | 87.68 |
| SDL blit/update | 86.08 | 87.88 |
| **Playback fps** | **2.112** | **2.152** |

These stage medians are not additive whole-playback measurements; other costs
include request scheduling, publication, logging and EOF. Host copy includes
CacheClearE and shared-memory reads; it is not an isolated Zorro bandwidth test.
Integrity rechecks are included. There was one recheck in a baseline 320x240
run and one in the additional cached 160x128 NTSC run; no incorrect frame was
accepted. All **225 frames in nine runs** matched (six 320x240 runs, two 160x128
25 fps runs and one 160x128 29.97 fps run).

The active experiment reads SCTLR `08c51878`; baseline and restored state read
`08c50878`. Every completed run reported RET1, launcher exit 0 and allocation
release. A separate automated test passed a stable cooperative pause, 64-byte
debug read and quit while paused, also restoring SCTLR and releasing ownership.

A first manual pause exceeded the existing 30-second request deadline. The
frontend exited with code 20, restored SCTLR and released memory; subsequent
debug calls correctly found no client. That is retained as timeout evidence,
not a successful quit test. Long debugger pauses are still limited by the
frontend's request timeout. The successful automated pause/quit stayed below it.

The result rules out instruction caching alone as the needed improvement.
Even excluding ARM hashing, cached decode plus colour is about 132 ms/frame.
Host copy, checks and drawing also independently block 25 fps. Next work must
address private ARM data-memory access and card-local presentation. NEON may
then help the measured kernels; the subsequent NEON comparison is recorded
below separately from this instruction-cache experiment.

### Implementation and reproduction

`zv_next_timed` splits decode and colour timing without changing their output.
The worker separately times hashing. Internal protocol **ZVP2** binds a
64-byte timing record to the session, request sequence and result-header hash;
the host rejects stale/corrupt timing records before accepting a frame. Host
copy and hash totals are measured separately. This is an internal bundled
host/worker change; the installed ZVP1 executable is unchanged.

The experimental entry checks Core1 before cache changes, performs local
ICIALLU/BPIALL maintenance with DSB/ISB, enables only SCTLR.I, then restores
the saved SCTLR and reads it back before publishing RET1. The worker verifies
the expected active mode, and the launcher rejects a restore mismatch. The
mapping probe still requires the original uncached state. Shared data uses
the existing protocol and host CacheClearE; no global PL310 operation is added.

```sh
python3 scripts/build_zzvideo.py --developer \
  --output .context/amiga/video-perf/baseline \
  --container-command '["podman","--connection","nuflix-converter-root"]'
python3 scripts/build_zzvideo.py --developer --experimental-icache \
  --output .context/amiga/video-perf/icache \
  --container-command '["podman","--connection","nuflix-converter-root"]'
```

Build options participate in payload identity. Separate output directories
preserve the packaged 0.1 artifacts. For live acceptance, first close other
Core1 applications, stage each executable as `RAM:ZZVideoPerf/video-<mode>`
with execute protection and copy the generated test clips there. A script
named `run-<mode>-<clip>-<run>` must set stack 131072, execute that binary with
the clip and `--verify`, and redirect to the same basename with `.log` in the
RAM drawer. Then use the workspace venv to run
`tests/amiga/video/live_profile.py <mode> <clip> <run>` from the Git root.
The harness never runs as part of the unit suite.

Evidence, frame references, build identities, entry disassembly, per-run
measurements and shutdown results:
`amiga/records/2026-10-08/video-performance/`. Local validation passed six video
tests (including timed output and corrupt timing-record rejection), 23 ARM
tests, MCP smoke with 138 tools, ARM/68k component and existing launcher builds.


## Physical NEON decoder results

The user explicitly requested NEON measurements after parking the wider
YouTube project. This limited experiment is now physically verified on Core1.
It does not reopen networking/UI/audio work or establish the 25 fps gate.

Three alternating 25-frame runs per mode used the same 320x240 I/P/B clip.
Both variants compile **only decoder.c at -O3**, with the remaining C units at
-O2; both enable instruction caching, with data cache and MMU off. The NEON
variant additionally compiles decoder.c and entry.S with
`-mfpu=neon -mfloat-abi=softfp`. It therefore enables VFP arithmetic as well as
SIMD; the measurements do not isolate NEON from hardware floating-point use.
No fast-math option is used. SDL2 and the shared-frame verification protocol
are identical. This is a matched new comparison, not a comparison with the
previous -O2 instruction-cache experiment.

| Median stage, ms/frame | Scalar -O3 | NEON-enabled -O3 |
| --- | ---: | ---: |
| MPEG decode | 78.257 | 62.225 |
| YCbCr-to-ARGB | 59.335 | 59.125 |
| ARM integrity hash | 33.957 | 34.031 |
| Host pull/copy | 112.520 | 113.240 |
| Host integrity/reference hashes | 88.080 | 86.480 |
| SDL blit/update | 86.400 | 87.280 |
| **Complete verified playback, fps** | **2.125** | **2.194** |

Decode time fell **20.49%** (1.258x decoder throughput); playback improved
**3.26%**. Even decode alone is above the 40 ms budget. Decode plus colour is
about 121 ms/frame. No 25 fps claim is supported. These short generated clips
are regression benchmarks, not a representative movie workload, and all ARM
times use the estimated global-timer frequency. The uncached data-memory
configuration also limits what this says about an optimally configured A9.

All **225 frames in nine runs** matched the scalar native reference: six
320x240 runs, two 160x128 runs and an additional 160x128 29.97 fps NEON clip.
Six shared-data integrity rechecks occurred, with no incorrect frame accepted.
Every run reported mapping PASS, Core1 identity, RET1, exit 0, restored SCTLR
and allocation release. Every NEON run passed the SIMD witness and full
register/context comparison. Small-clip single-run decode was 32.301 versus
27.601 ms/frame; playback 6.985 versus 7.213 fps.

The linked NEON payload contains 168 VFP/NEON-family instructions in IDCT and
92 in motion compensation (including saves/restores), and zero in colour
conversion. Colour conversion is still scalar. Counts show emitted code, not
per-frame instruction counts or an isolated timing share.

### SIMD state and reproduction

Entry checks CP10/CP11 full access, ASEDIS/D32DIS clear, FPEXC.EN, and MVFR0/1
SIMD capability fields before compiled decoder code. It saves all D0-D31 and
FPSCR in the owned control area, executes a vector integer witness (21+21=42),
then restores the registers and FPSCR before returning to firmware. A second
snapshot verifies every vector byte and CPACR/FPEXC/FPSCR equality. CPACR and
FPEXC are never changed by the experiment. Actual hardware values:

- CPACR `00f00000`, FPEXC `40000000`, FPSCR `00000000`.
- MVFR0 `10110222`, MVFR1 `01111111`; witness 42, context_equal 1.
- SCTLR `08c50878` -> `08c51878` -> `08c50878`.

No shared PL310 maintenance, private-data caching, MMU changes or guessed DDR
reservations were introduced. The default build still disables both optional
experiments. This tested firmware has SIMD already enabled; other firmware
may fail the checks, and is not covered by these results.

```sh
python3 scripts/build_zzvideo.py --developer --experimental-icache \
  --decoder-opt 3 --output .context/amiga/video-neon/scalar \
  --container-command '["podman","--connection","nuflix-converter-root"]'
python3 scripts/build_zzvideo.py --developer --experimental-icache \
  --experimental-neon --decoder-opt 3 --output .context/amiga/video-neon/neon \
  --container-command '["podman","--connection","nuflix-converter-root"]'
```

Stage these as video-scalar/video-neon with execute protection under
`RAM:ZZVideoNEON`, with the generated clips and matching run scripts. The live
harness accepts `scalar|neon <clip> <index> --drawer RAM:ZZVideoNEON --output
.context/amiga/video-neon`. Never run it concurrently with another hardware
controller. It verifies frame hashes, register diagnostics, cache restoration
and clean release before accepting timings. The bundled host/worker protocol
remains ZVP2. Build manifests deliberately retain physical_execution_verified
false until supported by the separate acceptance records.

A separate bounded cooperative pause, stable checkpoint, 64-byte memory read
and quit-while-paused check passed, including SIMD context and SCTLR restoration,
RET1, exit 0 and allocation release. Postflight found no video window, bridge
client or owner port. The installed ZZVideo 0.1 executable's MD5 is unchanged;
all experimental files remain temporary in RAM:ZZVideoNEON. The Mac staging
server is stopped.

Local validation passed six video tests, 23 ARM tests, the 138-tool MCP smoke
check, ARM/68k components and the existing launcher build. Evidence is under
`amiga/records/2026-10-08/video-neon/`, including matched build identities,
inputs, frame references, per-run raw logs/timings and the source review.

## FFmpeg and gen2brain/mpeg review

The user supplied these repositories during the NEON measurement. FFmpeg has
**32-bit ARM** NEON code relevant to Cortex-A9, including
[IDCT dispatch](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/arm/idctdsp_init_arm.c),
[handwritten IDCT assembly](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/arm/simple_idct_neon.S)
and [half-pixel motion compensation](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/arm/hpeldsp_init_neon.c).
These are concrete candidates for a subsequent decoder comparison. The source
review is not an FFmpeg port or a performance result on the ZZ9000.

Our proposed integration would be a bounded MPEG-1 decoder subset in the ARM
worker, with the Amiga host providing input and presentation. A normal Linux
FFmpeg executable cannot run directly in the current bare-metal XACP worker.
The subset needs allocator/runtime adaptation and CPU-feature configuration.
IDCT coefficient layout, scaling, rounding and alignment must match the chosen
decoder; its assembly is not a drop-in replacement for PL_MPEG's IDCT. Keep the
existing ownership, SIMD state restoration and shared-frame integrity checks.
The reviewed routines carry LGPL-2.1-or-later notices; retain attribution and
review the selected build's distribution requirements before shipping.

[gen2brain/mpeg](https://github.com/gen2brain/mpeg) is a Go MPEG-1/MP2 decoder.
Its [release notes](https://github.com/gen2brain/mpeg/releases) discuss ARM64
NEON motion compensation and audio synthesis. Those published ARM64 results
are not measurements of this 32-bit Cortex-A9, and the reported audio speedup
must not be presented as an MPEG video speedup. Its algorithms may inform a C
port, but neither its Go runtime nor ARM64 assembly is a direct XACP payload.

Neither library supplies evidence that this complete Amiga pipeline can reach
25 fps. The next useful decoder experiment would compare a minimal FFmpeg
MPEG-1 build against the measured PL_MPEG baseline, without treating decoder
throughput as playback throughput. Cacheable private data and presentation
remain independent work.


## XANI ZZ-MPEG review (2026-10-08)

The current [mpegplayer documentation](https://github.com/Xanxi-Amiga/XACP-ZZ9000/tree/main/applications/mpegplayer)
describes ZZ-MPEG 1.0 Advanced Beta: PL_MPEG video decoding on Core1,
MP2 audio through XACP/AHI, direct buffered and disk-streaming variants.
It is not an FFmpeg decoder; FFmpeg is used in its host-side encoding recipe.

Its most relevant design choice is **Core1 writing directly to the Picasso96
framebuffer**, either at a window offset or on a dedicated 320x240x32 screen.
That could avoid our measured 68k pixel-copy and SDL presentation costs; this
is an architectural inference, not a measured improvement on our machine.
The documented 25 fps pacing/target is not accompanied by timed benchmark
results. The recommended MPEG-1 profile also disables B-frames (`-bf 0`),
unlike our I/P/B decoder benchmark.

The documented baseline is XX16c and historical XACP v1/v1.5, whereas this
A4000TX runs XX19c. The README specifies fixed framebuffer-relative shared
rings at +0x04002000, +0x04100000, +0x04200000 and +0x05000000. These are not
proof of available memory in our current OS allocation. Compatibility and
ownership must be established before running the supplied binaries here.
No installation, firmware change or execution was performed for this review.

The directory publishes executables and embedded ARM blob headers, not the
player source. Its proprietary freeware terms permit unmodified distribution
with notices; modification/repackaging requires permission. PL_MPEG itself
remains MIT licensed. Audio/video synchronization is still being finalized,
and seeking and automatic frame-rate detection are not implemented.


## FFmpeg hardware benchmark attempt (2026-10-08)

**Attempted on the physical Cortex-A9, but no valid performance result.**
A minimal FFmpeg 9.0.2 MPEG-1 decoder/parser was cross-built for Core1, with
avcodec/avutil only. The experimental host, worker and bounded allocator are
in `amiga/ffmpeg_bench/`. Build/configuration metadata, reference hashes,
allocator tests and both hardware attempts are retained in
`amiga/records/2026-10-08/ffmpeg-bench/`. This is an unfinished port, not a
released FFmpeg application or a playback benchmark.

The intended measurement is three alternating C and ARM/NEON runs of the
same 25-frame 320x240 I/P/B MPEG-1 clip. Decode timing covers packet submission
and frame reception; parsing and YUV hashing are outside that timing. There
is no RGB conversion, display, audio or file streaming in the ARM worker.
C auto-vectorisation is disabled; explicit runtime feature flags select the
C baseline versus ARM/NEON kernels. Instruction cache only is enabled, with
data cache and MMU left off.

The Mac reference returned all 25 frames and matched an independent FFmpeg
7.1 decode using simple IDCT. An MPEG sequence-end marker was appended to the
elementary stream to obtain the complete final frame. The bounded allocator
passed ASan/UBSan checks for alignment, zeroing, growth, coalescing, overflow
and exhaustion. These native checks do not prove ARM decoder execution.

The final ARM ELF passed the strict loader checks: 558,217 image bytes and
3,138 relative relocations. A private 16 MiB Exec-owned ZZ9000 allocation,
the existing runtime mapping proof and the existing launcher were reused.
On hardware, the mapping nonce test, Core1 identity (MIDR 413fc090,
MPIDR 80000001), NEON witness 42 and timer calibration (~333.334 MHz) passed.
The timer frequency is not the CPU clock frequency.

Both attempts timed out before decoding a frame and exited 20. Inspection
found GCC had transformed the initial custom calloc into a self-call;
`-ffreestanding` fixes that generated-code defect. The instrumented retry
still stopped at stage 3, inside `avcodec_alloc_context3`, after parser
allocation and before frame allocation. Its cause remains unresolved.
There was no cooperative RET1, no verified SCTLR restoration and no verified
SIMD-context restoration in either failed run. The existing synchronous
XX19c Core1 reset-to-idle path ran before releasing owned memory. Subsequent
bridge ping and window enumeration succeeded; Final Writer remained open.
Do not describe these failures as clean cooperative returns or publish fps
from them. The C-versus-NEON performance comparison remains pending.

Runtime adaptations disable logging/OS feature discovery, supply single-worker
memory allocation and global errno, and rebuild the selected picolibc objects
as position-independent code. FFmpeg subtitle iconv support was disabled.
A GCC/picolibc type-compatibility fix changes the local MPEG start-code
variable from `int = -1` to `uint32_t = UINT32_MAX` in mpeg12dec.c; the decoder
algorithm was not changed. The saved build scripts/config are experimental
reproduction notes, not a turnkey SDK installer. Third-party source/toolchains
and binaries remain local under `.context/amiga/ffmpeg-bench/`.

No firmware, drivers, startup files or installed video application were changed.
The temporary test files remain in `RAM:ZZFFmpegBench`; no benchmark process
remains. Further work should isolate decoder-context/default-option startup
before attempting another timed decode or changing cache/MMU policy.
