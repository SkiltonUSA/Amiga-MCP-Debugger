# Direct ZZ9000 framebuffer bandwidth experiment

This is a display-only benchmark, not a video player. It compares ARM-local
frame copies into an active ZZ9000 display buffer against 68060 copies from
Amiga Fast RAM outside the ZZ9000 bank. No decoding, colour conversion, SDL,
audio, firmware changes, or cache/MMU enablement are involved.

## Physical result, 2026-10-09

Tested on the A4000TX/TF4060, XX19c/XACP 1.7, using a dedicated Picasso96
640x480 screen with four bytes per pixel. The smaller test updates a 320x240
rectangle on that same screen. Each case has 60 timed copies in three batches
of 20. Values below are medians of all 60 copies.

| Frame size | ARM-local copy | ARM payload rate | 68060 → ZZ9000 copy | Zorro payload rate |
| --- | ---: | ---: | ---: | ---: |
| 320x240, 32-bit | 13.723 ms | 22.39 MB/s | 47.527 ms | 6.46 MB/s |
| 640x480, 32-bit | 59.316 ms | 20.72 MB/s | 190.933 ms | 6.44 MB/s |

All **240 timed copies** completed. Eight full-frame comparisons (first and
last copy of each case) found **zero mismatched pixels**. Each ARM copy first
checked nonce-derived sentinels in its particular locked display buffer.
The program returned through its ARM epilogue, synchronously reset Core1,
released its resources and restored Workbench, with launcher exit 0.

This validates direct ARM writes into an active P96 framebuffer without the
68060 copying the full pixels in the timed ARM path. At 320x240, the pixel-copy
stage fits within the 40 ms budget for 25 fps; at 640x480 it does not in this
cache-off configuration. **This does not demonstrate 25 fps video playback.**

## Timing boundaries

ARM timing covers reading a pre-generated frame in its Exec-owned ZZ9000
allocation, writing it to the display framebuffer, and the completion memory
barrier. Its read-only global timer was calibrated against Amiga timer.device
over approximately two seconds (estimated 339,064,983 Hz in this run).

The host also measured ARM submission-to-completion observation: median
33.685 ms at 320x240, 67.379 ms at 640x480. These include the deliberately
coarse 20 ms mailbox polling. They exclude bitmap locking, nonce sentinel
setup, logging and a deliberate inter-frame yield. Do not convert them into
a claim of sustained or tear-free displayed frame rate.

The 68060 case times row-wise `CopyMemQuick` plus destination cache
synchronization. Its source allocation was outside ZZ9000 Fast RAM. These
are the achieved payload rates of these routines, not peak bus specifications.
Each ARM copy also reads its source in DDR; the reported payload rate counts
the output bytes once, not all internal memory transactions.

Pixel verification reads the framebuffer through Zorro, but occurs outside
the timed copies. There is no page flipping or vsync pacing. The display
pattern is synthetic, not decoded media. No physical-monitor observation or
photograph is claimed; the active P96 buffer was validated by readback.

## Ownership and shutdown

Reuses the existing XX19c launcher's Exec-owned allocation and runtime
mapping probe. The private allocation is 4 MiB; source pixels start at its
offset 0x20000 and remain separated from code, control records and stack.
No global DDR address is treated as free memory.

The display is opened through P96. Every copy locks its bitmap, confirms
on-card placement and four-byte pixels, checks bounds, and exchanges a
fresh mapping challenge before ARM writes. The lock is released immediately
after completion. ARM commands time out after 400 ms; on failure the bitmap
remains locked until the launcher's synchronous Core1 reset makes cleanup
safe. A benchmark-specific cleanup callback runs after that reset and before
the shared allocation or libraries are released.

The first development run rejected an ARM command with error 1 and exited
cleanly. The host was flushing payload and commit together, allowing ARM to
observe the commit too early. Publication now flushes the payload first,
then the sequence commit. The completed run passed all mapping/data checks.

## Build and repeat

Current Mac workspace prerequisites are the same pinned Amiga cross-compiler,
Clang ARM target and LLVM linker used by the debugger. The build script uses
the working `nuflix-converter-root` Podman connection.

```sh
python3 scripts/build_framebuffer_bench.py
```

Output is `.context/amiga/framebuffer-bench/ZZFrameBench`, with build hashes.
Before physical use, establish that **no other Core1 application is running**;
the launcher's ownership port cannot detect unrelated XACP games. Transfer
and verify its hash, then run from Amiga Shell:

```text
Protect ZZFrameBench +e
Stack 65536
ZZFrameBench >RAM:framebuffer-bench.log
```

The temporary test screen closes automatically. Ctrl-C requests shutdown.
Do not open or move other screens during the short bitmap locks.

Analyze the captured log on the Mac:

```sh
python3 scripts/analyze_framebuffer_bench.py framebuffer-bench.log --output summary.json
```

Records: `amiga/records/2026-10-09/framebuffer-bench/`. The ARM/MCP regression
tests and ARM/68k/default-launcher builds passed after adding the cleanup
hook. Native bounds tests use AddressSanitizer and UndefinedBehaviorSanitizer.

The interface follows the existing launcher mapping contract and the P96
SDK's bitmap lock/unlock requirements. XANI's
[ZZQuake framebuffer implementation](https://github.com/Xanxi-Amiga/XACP-ZZ9000/blob/main/applications/games/ZZQuake/source/arm/zz9000/zzquake_platform.c)
and [ZZ-MPEG documentation](https://github.com/Xanxi-Amiga/XACP-ZZ9000/tree/main/applications/mpegplayer#playback-modes)
provide the reference for the direct-display approach. This experiment does
not change or replace their applications.
