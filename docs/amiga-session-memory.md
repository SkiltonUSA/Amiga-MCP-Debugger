# A4000TX session memory

Updated 2026-10-08. Read this before further work on the real Amiga. This is
the durable handoff for the development setup and installations in this branch.
The user requested this GitHub backup. It preserves source, configuration and
evidence; it is not a backup of the Amiga's disks or installed commercial games.

## Latest video application (2026-10-08)

ZZVideo 0.1 is installed in `SD032G:Dev/ZZVideo-0.1/` with a native Workbench
icon (stack 131072), `demo.mpg` and readme/licenses. Executable: 1,168,048 bytes,
MD5 `770252ca2ff01ff1489ee33825afee1d`, SHA-256
`b9736ffee80a4b47a1b7322314093e6fe61b6c302fab8dad3abe5bb59d0542b3`.
**All video test applications and the previously open fractal have been closed.**
Re-enumerate windows/owner port before another Core1 launch.

This is the user's standalone-first decoding milestone for a minimal YouTube
client: ARM Core1 MPEG-1 video decoding and colour conversion, 68k SDL2 UI.
It needs no Mac or bridge for local playback. It does not yet retrieve YouTube,
stream, play audio or decode H.264/MP4. Files must be <=4 MiB, fixed dimensions
aligned to 16 pixels and <=320x240. Whole-file buffering and an Exec-owned 8 MiB
ZZ9000 allocation use the same runtime mapping and safe return proof as fractal.
Core0, firmware, drivers, startup, MMU and cache policy were unchanged.

Physical acceptance: 75 developer frames and 25 installed standalone frames
matched the native decoder hashes, all with Core1 identity, mapping PASS, RET1
and exit0/free verified. A separate initial standalone run is retained as
historical evidence. Single-run playback was about 6.8 fps at 160x128 and 2.1 fps
at 320x240; these are a baseline, not realtime playback. ASL file selection,
Workbench launch, EOF, replay, pause, S stop, Space resume, Q and quitting a
paused ARM debugger passed. A screenshot confirms visible colour video.
A synthetic activation click reopened ASL unexpectedly; Cancel then Q closed
cleanly. Prefer keyboard controls on an already focused SDL window.

Local tests: six video tests (ASan decoder/FFmpeg comparison, container bounds,
heap failure, replay and stale shared-data rejection), 23 ARM tests, existing
fractal/compute tests and 138-tool MCP smoke; ARM/68k and existing launcher /
native-fractal builds passed. Decoder source and port changes are in
`amiga/video/` and `scripts/video_vendor.py`; details and reviewed repositories
(AmiTube, XACP MPEG, SDL2_sound, SDL3, mpega.library) are in
[video development](amiga-video-client.md). Evidence:
`amiga/records/2026-10-08/video/`. The unchanged SDL2 SDK 0.2.0 is statically linked.

Bridge postflight was healthy with no video/fractal window, client or owner
port. The temporary Mac HTTP staging server on 55932 is stopped. RAM:ZZVideo
contains the temporary test inputs/scripts/logs and can be discarded after review.

A ZIP preview is generated under `.context/amiga/video/dist/`; it has not been
published as a GitHub release. `scripts/package_zzvideo.py` recreates it from
the standalone build, native icons and an explicit generated demo clip.

## Latest SDL application (2026-10-08)

**SDL ZZFractal 0.3** is installed in `SD032G:Dev/SDLZZFractal-0.3/`, alongside
0.2, with native icons and stack 131072. The 1,156,000-byte executable has
MD5 `69257b060f68f6709eb8b7b0ba1470f7` and SHA-256
`533b2ee03a8fcc07933202766ca345b7438c9d8386521b784407a39d4b965b87`.
It was left running from Workbench at (680,300) with a complete default ARM image,
and was subsequently closed for the video work recorded above.
It registers **no bridge client**: zero clients does not establish Core1 idle.
Check window `SDL ZZFractal 0.3`, the app task and `Sixies.ARM.Debug.Owner`;
quit with Q before another ARM app. Re-enumerate live state on the next task.

The exact Q14 kernel keeps orbit state in locals and uses Clang -O2; the
worker allows 512 steps but stops at each row checkpoint. Both UI modes use
a 2 ms ready-work target and a private 1 ms timer yield, with one-tick idle
and timer-open fallback. OS/SDL/bridge calls can exceed that work target.
No firmware, OS, startup, driver or cache-policy changes were made. Core1
MMU/caches remain off, and host CacheClearE still synchronizes shared ranges.
The SDL2 static library is unchanged from SDK 0.2.0, SHA-256
`bf90c1f12536df97bfc85e838cc2fa0047fc23f37d5128dbb89bb38dddec386e`.

Default medians of three developer runs per mode, 0.2 -> 0.3:
ARM render 7.609 -> 3.848 s; ARM compute estimate 2.899 -> 1.472 s;
68060 render 17.292 -> 4.328 s; 68060 compute 2.235 -> 1.692 s.
These are application measurements, not a general CPU/SDL speed ratio.
The installed Workbench copy showed ARM 3.819 s, compute 1.471 s.

Shorter waits initially exposed mixed old/new host tile reads. A complete
ARM-side diagnostic capture matched the oracle while the host copy differed.
Full-cache experiments did not eliminate the fault and were reverted. The
precise hardware/cache source remains unisolated. The final TIM3 protocol
validates session, sequence, generation, coordinates, timing and pixels as a
unit, keeping the request owned during read-only rechecks up to the existing
10 s deadline. Do not restore pixel-only checksums, accept unchecked data,
resend work, globally operate PL310, or infer a fixed DDR reservation.

Physical acceptance passed 14 full 76,800-count oracle comparisons and
20 additional default ARM hashes; the latter recorded 188 rechecks across
3,000 tiles. Native 16-/32-bit CGX sampling, cancellation while the ARM debugger
was paused, six rapid cancel/zoom cycles, iconified rendering, hook restore,
overlapping/moved windows and RET1 / exit 0 / allocation release passed.
Standalone CLI ARM/CPU hashes, cancel/discard and Q exit passed. Native LHA
integrity/fresh extraction passed and all 10 members match the ZIP. Installed
Workbench launch and its Start requester passed; bridge postflight was healthy.

Native AppIcon double-click restore remains unverified from 0.2. Injected
Workbench icon mouse input stalled Intuition/bridge, recovering without a
reboot; ARexx ICON OPEN did not restore it. Avoid synthetic Workbench icon
clicks. Developer-hook restore is not proof of the physical gesture.

Public source commit `e73eee2`; application-only prerelease:
https://github.com/SkiltonUSA/SDL2-AmigaOS3/releases/tag/v0.3.0
The SDK remains at v0.2.0. Source: `amiga/compute/`, `amiga/sdl_fractal/`,
`amiga/fractal/`; docs: `docs/amiga-sdl-fractal.md`; accepted evidence and the
rejected visibility capture: `amiga/records/2026-10-08/sdl-performance/`.
Use sequential hardware calls. Temporary extraction/scripts/logs remain in
RAM:; Mac HTTP staging is stopped at the end of this task.

## Current working state

Chronological background follows; the latest active application is recorded above.

- Target: A4000TX, TF4060, 68060 revision 5, AGA, AmigaOS 3.2.3 as identified
  by the owner. CPU MHz and individual chip package/date codes are unverified.
- Hardware audit found **2 MiB Chip and 624 MiB Fast RAM**, replacing the
  initial owner estimate of 284 MB Fast. Banks: 256 MiB at `$08000000`,
  112 MiB at `$01000000`, 256 MiB ZZ9000 RAM at `$50000000`.
- Ethernet: X-Surf 100, Roadshow 1.15 / bsdsocket.library 4.364,
  `x-surf-100` interface, last known Amiga address **10.0.0.40**.
- Bridge: **AmigaBridge 1.20**, TCP **2345**, installed in
  `SD032G:amiga-bridge/`. Its source is geekychris/amiga_mcp, pinned in
  `amiga/upstream.json`, with the local socket compatibility patch. It is
  not an independently written bridge.
- Mac MCP/dashboard: `http://127.0.0.1:55010/mcp` and
  `http://127.0.0.1:55010/`. Run `make amiga-hardware`. Use Mac FS-UAE for
  emulation, not WinUAE. Emulator boot remains unverified.
- 2026-10-08: live bridge connection restored and verified. Workbench was the
  only screen; no game was running. The new ARM-debugger **68k relay probe**
  passed all nine tools over Ethernet and was stopped cleanly. Its binary and
  log remain in `RAM:SixiesDev` until reboot. This was not ARM execution.
- Later 2026-10-08: **physical ZZ9000 Core1 debugging passed** with the new
  `zzarm-debug` launcher. All nine MCP tools worked on the ARM worker. It uses
  a 128 KiB Exec allocation in ZZ9000 Fast RAM and verifies its ARM translation
  with a short read-only mapping challenge. Observed mapping: Amiga
  `0x50000040` -> ARM `0x101f0040`; never treat these as fixed reservations.
  CP15 evidence: MIDR `413fc090`, MPIDR `80000001`, SCTLR `08c50878`.
  ARM MMU and caches stay off; the 68k uses CacheClearE on shared ranges.
  First run: 67 visibility echoes, RET1 epilogue, exit 0 and memory released.
  Second run: all nine tools passed again, plus paused Ctrl-C shutdown,
  218 echoes, RET1 and exit 0. Both instances are stopped; Workbench/bridge
  are healthy. Temporary launcher/scripts/logs remain in `RAM:SixiesDev`.
  Details and reproduction: `docs/amiga-arm-debugging.md`.
- Latest 2026-10-08: **Workbench Mandelbrot demo installed** in
  `SD032G:Dev/ZZFractal/`; launch with `Execute SD032G:Dev/ZZFractal/Start-Fractal`.
  Client `zzfractal`, 320×240, click-to-zoom, A=ARM, C=CPU, X/Escape=cancel,
  R=reset, Q=quit. ARM and 68060 exactly matched all 76,800 pixels at default
  and 2× zoom. Default timings: ARM 7.523 s, CPU 15.081 s; interactive-demo
  measurements including scheduling/drawing, not kernel benchmarks.
  Twenty cancel/zoom cycles and ten paused launch/quit cycles passed. Shared
  memory and cache-off contract remain unchanged. No startup/firmware changes.
  The installed demo was opened for the user after the acceptance suite;
  **check for `zzfractal` and close it before starting any other Core1 app**.
  The script chooses a session ID automatically and logs to
  `RAM:SixiesDev/fractal.log`. Source/build/tests: `docs/amiga-fractal-demo.md`;
  evidence: `amiga/records/2026-10-08/fractal/`.
- Distribution preview 0.1 is installed separately at
  `SD032G:Dev/ZZFractal-0.1/ZZFractal`, with Workbench tool/drawer icons.
  Double-click and choose Start after checking the XX19c firmware requirement.
  This 26,112-byte standalone build does **not** register with AmigaBridge;
  zero clients does not establish Core1 idle. Check the `ZZFractal` task and
  `ZZ9000 Fractal - Mandelbrot` window, then quit before launching another
  ARM application. LHA/ZIP archives passed native extraction, renamed-drawer
  Workbench launch and Shell rendering/clean exit. The old developer copy
  remains available for MCP. Evidence: `amiga/records/2026-10-08/fractal-release/`.
- Dedicated private backup/development repo:
  https://github.com/SkiltonUSA/Amiga-MCP-Debugger (initial baseline `4224fd7`).
  It contains the Amiga tooling, debugger, session memory and records, without
  Sixies game source, firmware/ROM binaries or commercial game data.
- SDL2 feasibility test (2026-10-08): bdgscotland SDL2 revision `1eefa8f`,
  locally patched for clipped window drawing and correct CGX library opening,
  passed 68k colour/input and physical ARM-generated Mandelbrot display tests.
  ARM checksum `fb32f6c6`, about 7.3 s including tile exchange/drawing; full
  320x240 surface redraws about 50-53 ms on the current 8-bit Workbench.
  Small 32x16 updates about 0.7 ms. These are redraw timings, not game FPS.
  Q, Escape during rendering, close gadget and timed exits were exercised.
  ARM return and allocation release passed. No audio/fullscreen/OpenRCT2
  execution was tested. Both probes/logs are in `RAM:` only; no OS setup
  changes. The previously open standalone ZZFractal was closed for testing.
  Reproduction: `docs/amiga-sdl2-probe.md`; source: `amiga/sdl_probe/`;
  evidence: `amiga/records/2026-10-08/sdl-probe/`.
- SDL2 now has a dedicated public source/SDK repository:
  https://github.com/SkiltonUSA/SDL2-AmigaOS3 and preview release
  https://github.com/SkiltonUSA/SDL2-AmigaOS3/releases/tag/v0.1.0.
  It contains the pinned upstream library source, exact two-file patch,
  build wrapper, SDK headers/static libraries, sample and validation evidence.
  Clean SDK archive SHA-256 is
  `eb805aac436378a29a20fb50270e7c21cb8dbe61fdf6f317ff62418ba16be000`.
  Three duplicate C2P members from repeated earlier builds were removed;
  all 149 rebuilt objects match and both relinked hardware probes are
  byte-identical to the programs already tested on the A4000TX.
  Uploaded ZIP/tar.gz/checksum assets were verified. This remains a 68k SDL
  build with application-owned ARM work; no Amiga files or settings changed.
- **ZZDarkForcesNEXT 1.0 works on the physical monitor at 640x480.** The user
  confirmed it after the loading delay. Its icon now enables `640x480` and
  `SC55`, stack 65536. `SC55` means the internal SoundFont synth.
- 320x240 showed a picture in the top half, repeated/flashing in the bottom
  half. Exact cause is unknown. Do not restore that mode as the default.
- Each Dark Forces launch stages about **72 MB** into ZZ9000 memory. A blank
  screen during loading is normal for this installation; allow loading to
  finish before treating it as a failure. It was restarted after saving the
  icon. The machine was unreachable at the later backup attempt, so its
  current running state is unknown.

## Hardware, OS and storage

The [full inventory](../amiga/records/2026-10-07/inventory/A4000TX-System-Report.html)
and [PDF](../amiga/records/2026-10-07/inventory/A4000TX-System-Report.pdf) include
77 loaded libraries/classes/audio components, 28 devices, 494 installed
component-version queries and 2,763 system-file entries. Raw evidence and CSVs
are beside the reports. This audit predates the Doom, Quake HighRes and Dark
Forces installations later that day.

| Component | Recorded state |
| --- | --- |
| KickSmash32 | Live detection 2026-10-08: firmware 1.8, built 2025-07-11 12:32:17; 32-bit mode, name `A4000-TX 2025`; active/power-on bank 3 (`47.115 3.2x`); board revision unverified |
| AGA | PAL Alice R2, Lisa; exact physical chip markings unverified |
| ZZ9000 | Zorro III, Zynq XC7Z020 dual Cortex-A9, nominal 666 MHz; 1 GB onboard DDR3 per specification |
| ZZ9000 firmware | **XX19c / XACP 1.7**, activated after power cycle, handshake and subsequent ARM application execution verified |
| ZZ9000.card | 1.13 (04.10.2022), 11,916 bytes, CRC32 `816CA650`; loaded header differs from embedded version |
| Picasso96 | rtg.library 43.538, Picasso96API.library 2.455 |
| Workbench screen | 1920x800, 8-bit during audit |
| Buddha IDE | buddha.device / 2nd.buddha.device 52.110 |
| Main storage | 64 GB SDCFXS-064G CompactFlash; `System:` / BDH0 and `HDD50Gig:` / BDH1 |
| SD storage | Approximately 32 GB SD through FC-1307 SD-to-CF adapter; `SD032G:` / SDH0 |
| Filesystems | Registered PFS3 19.2 and FFS 47.4; see report for per-volume types |
| Freeway Triton Lite | Poseidon 4.5, supplied freewaytritonz3usb.device 1.01; three-port root hub reported |
| AHI | ahi.device 4.180 (68060), paula.audio 4.23; ZZ9000AX driver installed |
| MIDI | CAMD 37.14, internal serial enabled, `out.0`; intended Roland MT-32 disconnected |

KickSmash32 was reported by the owner after the 2026-10-07 inventory; the
historical HTML/PDF audit does not include that confirmation. Upstream reference:
https://github.com/cdhooper/kicksmash32. It replaces the Kickstart ROMs and
supports in-system programming with `smash`, host programming over USB-C with
`hostsmash` (including macOS), up to eight flash banks, and optional host file
access through `smashfs`/`smashftp`. These are upstream capabilities, not verified
services on this machine. Subsequent live detection used the official release
2.0 Amiga `smash` utility temporarily in `RAM:`, with only `identify` and
`bank show` queries; the utility was then deleted. The board replied with
firmware 1.8, USB VID:PID `1209:1610`, serial `6d8050PP3GY17C` and the identity
shown above. This USB identity came from the board's Amiga-side response;
no KickSmash USB device was visible on the Mac during the check.
Bank labels: 0 `Switcher`, 1 `Diagrom V2a`, 2 `Logica`, 3 `47.115 3.2x`,
4 `45.66 3.x`, 5 `40.70 A4000T`, 6/7 `-`. Long-reset sequence: banks 0 then 1.
Bank labels were queried; ROM contents were not read or validated. No firmware,
bank settings or startup files were changed. Evidence:
`amiga/records/2026-10-08/kicksmash/detection.txt`.

AHI saved units 0-3 select Paula Fast 8-bit mono at 8000 Hz. Music Unit 255
selects ZZ9000AX 16-bit stereo at 32000 Hz. These preferences do not establish
the physical presence of an AX daughterboard or an application's mixer format.
Do not send further MIDI test notes unless the user requests testing and
connects the Roland. Internal game SoundFonts need no external instrument.

Firmware SHA256:
`2b6a4224d4645694ef4d186baae14a86c100f4ce9e15033262a814cc239e7305`.
The existing card matches the reference used with XX19/XX19b/XX19c. Do not
downgrade firmware or replace the card driver to resolve a speculative display
issue. Only one ARM Core1 application should run at a time.

## Network and USB startup

Roadie 1.3.9 calls AddNetInterface directly. Our `C:bridge-netwatch` helper
waits for Roadshow and a nonzero IPv4 address on `x-surf-100`, then runs
`SD032G:amiga-bridge/Start-Amiga-Bridge`. `S:User-Startup` invokes
`S:Start-Amiga-Bridge-Auto`. The watcher runs at priority -5 and avoids duplicate
watchers/bridges. It recreates `RAM:SixiesDev` through the launch script.

Sources: `amiga/tools/bridge-netwatch/main.c` and `amiga/scripts/`.
Stop only the watcher with `Execute S:Stop-Amiga-Bridge-Auto`; that leaves the
bridge running. Start it with `Execute S:Start-Amiga-Bridge-Auto`.
Backups on the machine include `S:User-Startup.before-netwatch` and
`SD032G:amiga-bridge/backup-netwatch/Start-Amiga-Bridge`.

Poseidon was installed from the user's Freeway-specific archive and driver.
Trident saved the hardware configuration to `ENVARC:PsdStackloader`, which
User-Startup invokes. `L:fat95` was installed (26,868 bytes, CRC32 `0CD1BD9C`).
USB mass storage mounted successfully and the user confirmed it worked. fat95
is loaded when needed by the filesystem setup; it needs no separate startup
program. Driver/archive identities and installation checksums are backed up.
The DrawBridge floppy device is not established as an AmigaOS floppy solution;
do not treat it as an ordinary USB mass-storage drive.

## Installed applications

| Application | Location | Working configuration / limits |
| --- | --- | --- |
| ZZDarkForcesNEXT 1.0 | `HDD50Gig:Games/ZZDarkForcesNEXT` | 640x480, SC55, stack 65536; user confirmed display/game works |
| ZZDoom 1.1 | `HDD50Gig:Games/ZZDoom` | Doom 1.9 shareware `DOOM1.WAD`, `NOMUSIC`, stack 65536; title screen captured |
| ZZQuake 1.0 | `HDD50Gig:Games/ZZQuake` | Original 320x240 version and Quake shareware pak0; physical display confirmed later than its initial install record |
| ZZQuake HighRes 1.0 | Same ZZQuake drawer | 640/800/1024 launchers and corresponding blobs installed and checksum-verified; not launch-tested |
| ScummVM AGA 060 2.5.1.40 | `HDD50Gig:Games/ScummVM` | CAMD/native MT-32, serial `out.0`; launcher display confirmed, external MIDI untested |
| ZZBenchGUI 1.3 | `SD032G:Drivers/XACP/ZZBench` | ARM Core1 execution verified; ARM DDR bandwidth subtest reported 0.0 MB/s and remains unresolved |

The Games search found original SimCity CDTV, not SimCity 2000.

Dark Forces data came from the user's `StarWarsDarkForces.iso` in Mac Downloads.
All 54 DARK/ files were extracted and structurally checked; 51 game-data files
(68,390,561 bytes, excluding three DOS executables) were installed and verified
with native Amiga MD5Sum. Keep the original `DATA/LFD` layout and supplied TFE
JSON files. The ISO's missing tail affects unrelated Full Throttle demo files,
not any extracted Dark Forces file. ISO/data hashes are in the records; the
commercial data itself is excluded from Git.

The successful 640x480 diagnostic log records preflight PASS, 4/4 GOBs,
SoundFont load OK, 312 acknowledged displayed frames, no PAN ACK timeouts,
zero AHI underruns and a clean Core1 exit marker. These logs do not prove
audible output or save/load gameplay. Settings/pilot writeback succeeded.
The icon's original backup is `ZZDarkForces.info.before-640x480` in its drawer.
The backed-up `set-video-mode.c` documents the actual icon change through
icon.library; it is not an automatic startup utility.

## Tool behavior and validation boundaries

- A cooperative ARM debug framework now exists in `amiga/arm_debug/`, with
  nine MCP tools registered by the workspace launcher. Native C/MCP tests and
  Cortex-A9/68k compilation and physical Core1 acceptance passed.
  See `docs/amiga-arm-debugging.md`. It requires an instrumented application
  and a launcher-owned shared channel; it cannot attach to unmodified games
  or single-step ARM instructions. `make amiga-arm-demo` is a software demo
  only, on a separate localhost endpoint. A separate 68k probe subsequently
  passed live AmigaOS IPC/Ethernet acceptance (2026-10-08), including stable
  pause, exactly one checkpoint step, breakpoint hit, 64-byte memory read and
  clean detach/process exit. See `amiga/records/2026-10-08/arm-debug/`.
  The later standalone XX19c launcher passed physical ARM acceptance using
  Exec-reserved ZZ9000 Fast RAM. The SDK still has no generic shared-channel
  allocator; do not infer free DDR from gaps. Do not enable ARM caches/MMU
  without implementing and validating a different coherency contract.

- Real hardware Ethernet, DOS commands, ARexx `SIXIES_AREXX_OK`, probe client
  registration and changing `ticks` were verified. Simulator checks are
  separate evidence and never prove AmigaOS execution.
- Source debugging is unfinished: the compiler emits DWARF2, while the pinned
  upstream reader expects STABS. Symbols load; source-line/type mapping and
  debugger stepping remain unvalidated. ARM source debugging is unvalidated.
- Some RTG screenshots are black despite a working physical display. Dummy
  Intuition screens can report 1bpp while P96 uses a 32-bit bitmap. Ask about
  the physical monitor and inspect logs before declaring failure.
- Run remote DOS scripts sequentially: command IDs can collide. Avoid a nested
  `Quit` that prevents the outer script's completion marker from being written.
- Fetch files in **2048-byte chunks** with `inventory/fetch.py`. The bridge's
  4096-byte read path has shown truncation/metadata quirks. Check returned size.
- Large `amiga_checksum` requests can exceed its roughly 10-second timeout.
  Use native `C:MD5Sum` with output redirected to RAM, then fetch the result.
- Large transfers used temporary Mac HTTP servers and Roadshow `C:wget`.
  Verify destination checksums and stop the temporary server afterward. All
  installation transfer servers were stopped.
- Do not open a second raw TCP connection to 2345 to test availability; the
  bridge may replace the MCP client. Use its current MCP connection.
- To stop Dark Forces, first identify its CLI with `Status FULL`, then send
  `Break <current-cli-number> C`. Never assume a previously seen CLI number.
  Confirm the screen closes and the log records cooperative Core1 return.

## Known audit findings and next work

The Amiga clock reported October 2025 while the Mac reported October 2026;
file dates are therefore not reliable installation dates. Kickstart 47.115 /
Exec 47.13 differ in age from Workbench 47.33 / disk version 47.2. Startup
references missing StikyRMB/VisualPrefs were recommendations only; no cleanup
was performed. Hardware MHz, physical chip revisions and AX presence remain
unverified. Cold-boot and full Roadie offline/online acceptance remain separate
from the tested watcher restart behavior.

For rebuilding, use the compiler image digest in `amiga/upstream.json`.
The Mac's `nuflix-converter-root` Podman connection worked during this backup;
the captured local settings name `podman-machine-default-root`, which may need
selecting/changing on the next machine. Do not change global Podman defaults.

See [recovery instructions](amiga-backup-and-restore.md). The historical game
backup attempt could not reach 10.0.0.40 (`Host is down`); later 2026-10-08
debugger work reconnected and captured the live evidence above. Neither backup
is an Amiga disk image or a capture of live game progress.
