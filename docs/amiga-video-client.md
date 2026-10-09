# ZZVideo / minimal YouTube client development

The first milestone is a standalone **video-only MPEG-1 player**, ZZVideo 0.1,
for the A4000TX's ZZ9000 XX19c / XACP 1.7. It is the playback foundation for
our minimal YouTube client, not yet a YouTube URL resolver or streaming service.
The user selected standalone Amiga playback first, with YouTube access afterward.
No comments, transcript, translation, account or social UI is planned.

## Architecture and boundaries

- 68k: AmigaDOS file access, ASL requester, SDL2 window, playback pacing and controls.
- ARM Core1: MPEG-1 PS demultiplexing, integer video decoding and YCbCr-to-RGB conversion.
- Core0: existing firmware/services, untouched. No firmware/cache/MMU changes.
- Input: MPEG-1 Program Stream (`.mpg`) or elementary video (`.m1v`), at most
  4 MiB. Constant dimensions/rate, both dimensions multiples of 16, maximum
  320x240. MPEG-2 extensions and MP4/H.264 are rejected. MP2 packets are skipped;
  this milestone produces **no audio**. Pixel-aspect correction is not implemented.
- The complete compressed file is read into owned RAM before decoding. This is
  local playback, **not disk/network streaming**. Mac tools build and stage the
  program and generated fixtures; the standalone executable needs no Mac to play.
- Basic validation bounds packet lengths, dimensions, input size and allocation.
  PL_MPEG is not a hardened decoder for arbitrary hostile files. Initial hardware
  acceptance uses generated test clips, not downloaded YouTube content.

An 8 MiB block is reserved with Exec AllocAbs from an actual free ZZ9000 RAM
chunk. The existing P96 bootstrap verifies the address translation at runtime.
Code is below offset 0x10000; control/debug state starts there; output at 0x20000;
compressed input at 0x100000 (4 MiB maximum); decoder arena at 0x600000 (512 KiB);
private stack at the end. These are offsets inside the allocation, never global
DDR reservations. Decoder code is soft-float; no VFP/NEON registers are used.

One command is in flight. Shared request/result headers and frame bytes carry
session/sequence-bound integrity checks. The host rereads a mismatched result,
without displaying it or recycling its buffer, up to a 30-second deadline.
The launcher signals shutdown, checks RET1, synchronously returns Core1 to idle,
then frees the allocation. The shared owner port excludes our other applications;
foreign XACP games still need closing explicitly before launch.

## Building and testing

Requires the same pinned SDL2 static library as SDLZZFractal 0.3, local Clang/LLD
and the pinned Amiga GCC container. Generated files remain in `.context/amiga/video/`.

```sh
python3 scripts/build_zzvideo.py \
  --container-command '["podman","--connection","nuflix-converter-root"]'
# Add --developer for the MCP-integrated zzvideo-debug variant.
make test-amiga-video
make test-amiga-arm test-amiga
```

Tests require FFmpeg on PATH, or `FFMPEG=/path/to/ffmpeg`. The development Mac's
Homebrew FFmpeg has a missing x265 dylib; the existing HeavyM-bundled FFmpeg was
used for fixtures without modifying either installation.

Six video tests cover decoder/reference comparisons with address sanitizer,
replay, MPEG-PS/elementary inputs, 25/29.97 fps, B-frame drain, small allocation
failure, malformed/truncated containers, unsupported formats/dimensions and
shared-buffer ownership/integrity with address/undefined-behavior sanitizers.
Three generated clips contain 25 frames each. All ordinary frames are byte
identical to unmodified upstream PL_MPEG. Its omitted final B-sequence reference
frame is additionally tested against FFmpeg. All 75 frames match FFmpeg's frame
count; integer IDCT/colour rounding differs (maximum channel error 15, mean less
than 0.74 on the test clips; acceptance bounds 16 and mean <2).

An initial 160x120 NTSC B-frame fixture exposed a localized edge-block artifact
in upstream PL_MPEG too (49 channel samples differed by >20; maximum 255).
The preview therefore rejects dimensions not aligned to complete macroblocks.
An aligned 160x128 fixture passed; this is a restriction, not a claim that the
upstream motion/edge handling has been fixed.

## PL_MPEG provenance and small port changes

`amiga/video/vendor/upstream.json` pins the upstream commit and SHA-256.
The original MIT header is kept unchanged, including its SPDX license identifier; the full MIT notice accompanies it. The build
creates a patched copy through `scripts/video_vendor.py`:

1. Fixed-memory buffers stay immutable when the video decoder asks to discard
   consumed bytes. This preserves EOF detection and replay.
2. Drain the delayed reference frame when the last coded picture is a B-frame.
3. Check allocation failures in buffer/video creation and reference-frame storage.
4. Omit unused floating-point presentation timestamps. 68k pacing uses the exact
   sequence-header rate numerator/denominator and accumulates fractional milliseconds.
5. Keep the unused pixel-aspect field in the upstream table's float format,
   avoiding a soft-double conversion. Pixel decoding/IDCT remain unchanged.

A bounded monotonic arena holds decoder state and three YUV reference frames.
Replay reuses it; opening a new file resets it. No reachable decoder operation
reallocates. A small integer ARM division helper supplies the Cortex-A9 EABI
without a new runtime dependency.

## Controls

```text
Stack 131072
ZZVideo path:to/clip.mpg
```

O opens a file requester; Space plays/pauses; R replays; S stops; Q/Esc quits.
The controls can also be clicked. End-of-file retains the last picture until
Replay, Open or Quit. `--verify` logs per-frame hashes/timer ticks and exits at EOF.
Close other Core1 applications first, including standalone fractal windows that
have no MCP client. The debug build registers `video` (status/pause/play/replay/quit)
and the existing cooperative ARM debugger on client `zzvideo`; checkpoints are
1 request, 2 published frame, 3 idle. It cannot instruction-step inside a picture.

## Projects reviewed

| Project | Use in this work |
| --- | --- |
| [AmiTube](https://github.com/alb42/AmiTube) | CC0 Amiga search/download frontend; external server converts YouTube to MPEG-1/CDXL, then launches a configurable player. Candidate integration after local playback. No Mac required if another server supplies conversion; still not wholly on-Amiga YouTube processing. |
| [XACP ZZ-MPEG](https://github.com/Xanxi-Amiga/XACP-ZZ9000/tree/main/applications/mpegplayer) | Existing ARM MPEG-1/MP2 player demonstrates feasibility. Closed-source freeware, source unavailable, README baseline XX16c. No code/blob copied or firmware downgraded. It credits MIT PL_MPEG. |
| [PL_MPEG](https://github.com/phoboslab/pl_mpeg) | Actual MPEG-1 decoder source, pinned and built into our Core1 payload. |
| [SDL2_sound 68k](https://github.com/SteffenHaeuser/SDL2_sound_AmigaOS3.1_68k) | Audio file decoding for a later 68k fallback. Does not automatically offload or decode video. |
| [SDL3_68k](https://github.com/SteffenHaeuser/SDL3_68k) | Has AmigaOS3 video/audio backends; framebuffer uses layered WritePixelArray. Separate frontend evaluation later; current acceptance keeps tested SDL2 fixed. |
| [mpega.library 2.4](https://aminet.net/package/util/libs/mpega_library) | 68k MPEG audio layers I/II/III to PCM (also a distinct PowerUP PPC version). Audio candidate, not video or ARM acceleration. |

## Next milestones

1. Meet the user's **sustained 25 fps** requirement before further YouTube
   feature work. Use 320x240 MPEG-1 as the initial benchmark resolution; the
   current 2.1 fps player fails this gate. Keep 0.1 as a correctness reference.
   Establish a safe faster ARM execution path and card-local presentation,
   measuring each independently before integrating them.
2. Disk ring buffering and longer clips, then direct MPEG HTTP input via Roadshow.
3. Minimal video URL/search list, integrated with AmiTube's retrieval/conversion
   model where suitable. Disclose server dependence and server availability.
4. ARM MP2 audio, AHI output and one audio/video clock; evaluate Core0 XACP audio
   versus Core1 workload before allocating either core.
5. H.264 decoder benchmark and MP4 demuxing. Do not promise HD or current YouTube
   formats based on MPEG-1 results. Network/TLS/URL resolution are separate work.

Physical acceptance results and installation details belong in the session memory
and dated evidence, not inferred from these native/build tests.

## Physical acceptance and preview package

Installed: `SD032G:Dev/ZZVideo-0.1/ZZVideo`. Double-click its icon, click Start,
then select `demo.mpg`. The Workbench and Shell paths both passed on the real
A4000TX. Test apps are closed after acceptance; the bridge stays available.

| Test profile | Verified frames | Observed playback |
| --- | ---: | ---: |
| 160x128, 25 fps source | 25 | 6.78 fps |
| 160x128, 29.97 fps, B-frames | 25 | 6.79 fps |
| 320x240, 25 fps, B-frames | 25 | 2.12 fps |
| Installed standalone 320x240 | 25 | Approximately 2.1 fps |

Single runs, including integrity checking and display; not a claim of realtime
playback. All 100 accepted frame hashes match the native reference. ARM identity,
mapping, cooperative debugging, RET1 and allocation release were verified.
Playback pause/replay/stop/resume and Workbench file selection passed. A synthetic
activation click reopened ASL; Cancel then Q closed it. Full logs, build hashes,
FFmpeg comparisons and screenshots: `amiga/records/2026-10-08/video/`.

The immediate performance work is to separate frame transfer/integrity costs
from colour conversion/display, then design and validate a faster ARM/cache or
card-local presentation path. Do not remove integrity checks or enable caches
based only on these timings. The current decoder remains intentionally bounded.

The [ARM execution investigation](amiga-video-arm-execution.md) now includes
physical profiling: instruction caching improved median 320x240 playback from
2.112 to 2.152 fps (1.86%), with 225 reference-correct frames across nine runs.
The 25 fps gate is still unmet. Private data caching, NEON execution and
card-local presentation remain pending.

### Required performance gate (2026-10-08)

The user rejected 2 fps as useful playback and requires 25 fps. The initial
320x240 target allows **40 ms per frame**, compared with about 474 ms now:
roughly a 12x overall improvement is needed.

Existing 25-frame logs give the following averages, including startup/EOF work
in the whole-playback measurement:

| 320x240 run | Whole playback, ms/frame | ARM decode + colour, estimated ms/frame | SDL blit + update, ms/frame |
| --- | ---: | ---: | ---: |
| Developer (`bframes-live.log`) | 472.20 | 144.61 | 87.52 |
| Installed standalone (`release-live.log`) | 473.76 | 131.64 | 87.56 |

The ARM timer frequency is estimated over a short host interval, so those
converted times are approximate. ARM frame hashing is outside the decode
timer. Unclassified time includes ARM hashing, shared-memory transfer, host
hashing, scheduling, protocol and logging; it is not a measured bus-only cost.
Both measured stages independently exceed 40 ms. Removing checks or changing
SDL alone does not establish a route to 25 fps.

The current ARGB frame is 307,200 bytes and travels from card memory into a
68k buffer before display. At 25 fps that is 7.68 MB/s for one full-frame pass,
or 15.36 MB/s for a card-to-host-to-card round trip, before other accesses.
Investigate supported card-local framebuffer/overlay presentation and a
validated ARM cache/coherency contract. Do not infer free DDR, toggle global
cache state, or change firmware to obtain a speculative speedup.

The proposed acceptance run is at least 60 seconds of continuous 320x240,
25 fps video, without a Mac processing the video or concealed frame dropping,
with correct output, responsive controls and clean Core1 shutdown. This is
not yet achieved; longer input handling will be needed for that run. Existing
XACP MPEG documentation targets 25 fps, but does not prove that rate on this
XX19c configuration. Establish compatibility before using it as a comparison.

```sh
python3 scripts/package_zzvideo.py --demo /path/to/generated/demo.mpg
```

Produces `.context/amiga/video/dist/ZZVideo-0.1-XX19c.zip`, with native icons,
demo, notices and SHA256 manifest. It is a local preview, not a published release.
