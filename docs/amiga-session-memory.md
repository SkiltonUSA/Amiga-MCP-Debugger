# A4000TX session memory

Updated 2026-10-09. Read this before further work on the real Amiga. This is
the durable handoff for the development setup and installations in this branch.
The user requested this GitHub backup. It preserves source, configuration and
evidence; it is not a backup of the Amiga's disks or installed commercial games.

## GitHub backup (2026-10-09)

Backed up current Amiga development work to the dedicated public repository
`SkiltonUSA/Amiga-MCP-Debugger`, including ZZTemperature source, the exact
installed executable/icon with SHA-256 checksums, screenshot, installation
records, and this session memory. The distribution is under
`amiga/distribution/ZZTemperature-1.0/`. Earlier unpushed ARM/video source,
parked experiment records, and application installation metadata were also
preserved. No commercial game files, installer archives, firmware or ROMs
are included. This is a development backup, not an Amiga disk image.

Backup verification passed 23 ARM tests, the MCP simulator smoke test,
10 fractal tests, 8 SDL-fractal tests, and 6 video tests. ARM SDK/68k components
and the default ZZ9000 launcher rebuilt in the dedicated repository. The
system FFmpeg initially failed because Homebrew's x265 dylib was missing;
video tests passed using the already-available standalone FFmpeg 7.1 binary.
No hardware programs were launched or Amiga files changed during backup.
Logs are in `amiga/records/2026-10-09/github-backup/`.

## ZZ9000 temperature monitor (2026-10-08)

Created and installed **ZZTemperature 1.0** at `SYS:WBStartup/ZZTemperature`
with a tool icon (`DONOTWAIT`, `STARTPRI=-5`, 16 KiB stack). Workbench launch
starts hidden and adds **Tools → ZZ9000 Temperature…** to the top menu bar.
The window refreshes every second, showing Celsius and session min/max.
Close/Escape hides; the menu reopens it; `R` resets statistics. Repeated
launches reuse the existing `ZZTemperature.1` instance. Shell `ONCE` prints
one reading, `SHOW` opens, `STOP` exits and removes its menu.

This reads the **ZZ9000 Zynq** sensor, not the TF4060/68060 or motherboard.
MNT's ZZTop source documents the 16-bit register at discovered board base
`+0xe0`, in tenths Celsius (firmware 1.7+). Physical readings were around
53–54 C. Raw firmware register returned `0x0113`; it does not independently
establish the XX19c suffix. No XACP/Core1 commands or MMIO writes are used.
NewMeter and ToolsDaemon configuration were left intact.

Verified the installed binary hash, correct live GUI formatting/updates,
menu registration and selection through a synthetic Workbench IDCMP event,
close/reopen, single-instance handling, and clean STOP/menu removal.
`C:WBLoad` verified the hidden Workbench startup path; **cold-boot automatic
startup has not yet been tested**. The monitor is left running with its
window open behind the still-running SimCity 2000 screen. No reboot was done.

Source/build/instructions: `amiga/zztemperature/`; installation hashes and
acceptance: `amiga/records/2026-10-08/zztemperature/`. Executable 15,856 bytes,
MD5 `8a28c7fe5dda0764592ea0e92b74677b`. To uninstall, STOP and remove only its
executable and `.info` from WBStartup; no startup-sequence edits were made.

## SimCity 2000 installation and RTG patch (2026-10-08)

Installed the user's three ADFs in `HDD50Gig:Games/SimCity2000/`, with a
Workbench drawer icon, original game icon, sample cities and local fonts.
The supplied installer joins sc1/sc2/sc3 and runs decrunchall; that same
process was reproduced with validated inputs and temporary RAM extraction.
All 52 copied game/font/icon files matched the decompressor output by MD5.
No earlier SimCity2000 installation existed; SimCityCDTV was left untouched.

At the user's request, applied Tobias "MastaTabs" Seiler's SC2000RTG 1.5 from
https://aminet.net/package/game/patch/SC2000RTG . The patcher recognised the
English 0.031 executable (27 Oct 1994), applied 20 patches and produced
375,160 bytes. Native and independently host-built patcher output matched:
MD5 `aa2f2c7024f591a71eb29d53061ee400`. This is now the main `SimCity2000`
executable; its original MD5 was `534a6f9b1a093839a8e70c0746221716`.
Original executable/icon/res1 are preserved under `Original-AGA/` inside the
game drawer. `RTG-Patch/` contains the patcher and its documentation. No MIDI
patch/player was installed. No firmware, OS libraries or startup scripts changed.

Workbench launch while holding Shift opened the screen requester. Selected
**ZZ9000 1024x768 8-bit**; Maxis/title artwork and the first-run owner-name
form displayed correctly. The user subsequently confirmed the game is working.
There is no independent city-view screenshot or save/load acceptance test.
The game was left running while the icons were updated. The unpatched build exited silently before opening a screen;
its precise cause was not established. The game stores display settings in
`res1`; hold Shift during startup to choose another mode.

Installation metadata and hashes, excluding commercial game data, are under
`amiga/records/2026-10-08/simcity2000/`. Local screenshots and supplied-image
extraction remain under `.context/amiga/simcity2000/`.
The final checksum command completed after first-run setup progressed.
Temporary installation and icon staging were removed; Mac HTTP staging and
the helper build container were stopped. Do not dismiss the user's game
without checking current state.

Added Christian Rosentreter (tokai)'s NewIcons from
https://aminet.net/package/pix/nicon/tk_sc2k_icons : drawer/application,
five sample city and five scenario icons (12 total). The originals are in
`HDD50Gig:Games/SimCity2000/Icons-Before-NewIcons/`. An icon.library helper
copied artwork and read back each result, verifying default tool, stack size,
icon type, position, tool window and non-image ToolTypes remained unchanged.
In particular TYPE=CITY/TYPE=SCENARIO and SimCity2000 default tools were
preserved. Source attribution is `NewIcons-Readme.txt` in the game drawer.
The running game was not restarted for the icon change. Metadata, checksums
and helper source (not third-party artwork) are in the same install records.

## WHDLoad update (2026-10-08)

Installed the user's WHDLoad 20.0 archive, matching the latest stable release
listed on whdload.de at installation. `C:WHDLoad` is build 7051 and
`C:WHDLoadCD32` build 7052 (27 March 2026), replacing 19.1 builds 6907/6908.
All seven supplied C commands are installed, including WArc 1.0 and
WHDLoad.VFS / ArchiveFS 1.4. Documentation is in `SYS:Locale/Help/WHDLoad`.
The three existing `S:WHDLoad` preferences/startup/cleanup files exactly match
the supplied defaults and were left unchanged. Keys, game files, save data,
Kickstart images, network scripts and system startup files were not modified.

Rollback: `SYS:Storage/WHDLoad-before-20.0-20261008/` contains the previous
five commands, three settings files and documentation. The eight command/
settings backup checksums match their originals. Native LhA tested/extracted
all 562 archive members; 558 installed command/document/default files matched
the attached archive by MD5. WHDLoad 20.0 command help executed successfully;
no gameplay test or reboot was performed. Temporary RAM staging was removed
and the Mac HTTP transfer server stopped; bridge postflight passed. Records:
`amiga/records/2026-10-08/whdload/`.

The old EyeOfTheBeholder2 WHDLoad launch was waiting at its splash/update
requester and held the executable open; its pending launch was cancelled with
Escape to complete replacement. Start it again to use 20.0. During inspection,
an incorrect Assign command briefly removed C: and S:; both were restored to
SYS:C and SYS:S and verified, and the resulting C: volume requester dismissed.
No persistent assign/startup configuration was changed.

## Final Writer installation (2026-10-08)

Final Writer 7.3 from the user's `FinalWriter7.3-m68k-amigaos.lha` is installed
at **`HDD50Gig:APPS/FinalWriter7.3/FinalWriter`**, with the supplied application
and drawer icons. The old `HDD50Gig:APPS/FinalWriter` was preserved. The supplied
icon specifies stack 131072; CLI startup with that stack was verified. The
editor is left open on the existing 1920x800 Workbench with an empty Untitled
document, Vera Sans selected, ARexx ports **FW.1** and **FINALW.1**. Do not close it in a later
session without checking whether the user has entered unsaved work.

Added previously absent `LIBS:freetype2.library` 1.3, Vera TrueType fonts and
font descriptors under FONTS:, and `SYS:System/FTManager` with its icon, from
Aminet's freetype2_lib package. Added `LIBS:popupmenu.library` 10.8.6 (68060)
from pmlib060. An application-local popupmenu copy alone failed on this OS3
build; adding the system copy resolved the startup error. A duplicate remains
in the application's Libs drawer. Existing codesets.library 6.22 and
muimaster.library 19.35 were retained. No startup script or firmware changes.

The source archive checksum, native extraction integrity and 178 installed
application/dependency files were verified; the additional system popupmenu
copy was independently MD5-verified (179 files total). Editor startup was
visually checked. Document saving, PDF export and printer output remain
untested. GhostScript was not installed; its optional printing features need
separate setup. The native PDF export does not require it per Requirements.
Commercial application bytes are excluded from the repository. Installation
metadata/checksums and logs: `amiga/records/2026-10-08/finalwriter/`.
Temporary RAM archives/extraction were removed and the Mac HTTP server stopped.

## Latest video application (2026-10-08)

**Parked at the user's request (2026-10-08).** This includes the YouTube/video
project, FFmpeg runtime debugging and benchmarking, and further ZZ-MPEG/direct
framebuffer experiments. Preserve the sources and evidence; do not resume
this work unless the user asks. The last FFmpeg attempt produced no valid fps
result. All test processes and the temporary Mac staging server are stopped.


**Wider YouTube project remains parked.** The user subsequently explicitly
requested a limited NEON decoder measurement; it is now complete. At 320x240,
three matched runs per mode gave scalar/NEON decode **78.257/62.225 ms per
frame** (20.49% less decode time), and verified playback **2.125/2.194 fps**
(+3.26%). Both use -O3 for the decoder, instruction cache on, data cache/MMU
off. NEON build also enables VFP via softfp; this does not isolate SIMD alone.
All 225 frames matched, with six integrity rechecks and clean returns. Every
NEON run verified CPACR/FPEXC/MVFR access, vector witness 42 and preservation
of all D0-D31/FPSCR plus unchanged CPACR/FPEXC. Details and limits:
[ARM investigation](amiga-video-arm-execution.md), evidence
`amiga/records/2026-10-08/video-neon/`. FFmpeg has relevant 32-bit ARM NEON
kernels. A subsequent minimal FFmpeg 9.0.2 port was built and attempted on
Core1, but stalled inside `avcodec_alloc_context3` before decoding any frame.
**There is no valid FFmpeg throughput result.** Both attempts exited 20 with
no RET1/cache/SIMD restoration proof; the launcher's synchronous XX19c reset
preceded memory release. Bridge and Final Writer remained responsive/present.
The mapping test, Core1 identity, NEON witness and timer calibration did pass.
Failed-run evidence and build metadata: `amiga/records/2026-10-08/ffmpeg-bench/`.
Experimental sources: `amiga/ffmpeg_bench/`; temporary files remain in
`RAM:ZZFFmpegBench`. No firmware/driver/startup/installed-app changes.
XANI's ZZ-MPEG player was reviewed, not installed or run: PL_MPEG with direct
ARM framebuffer writes, older XX16c baseline, fixed historical rings and no
published source. Its documented 25 fps target is not a measured result here.
The bounded cooperative pause/read/quit check also passed with context/cache
restoration and RET1/exit 0. All test applications are stopped, no owner/client
remains and the bridge is responsive. Staged files remain in RAM:ZZVideoNEON;
the temporary Mac HTTP server is stopped. The installed ZZVideo 0.1 MD5 remains
`770252ca2ff01ff1489ee33825afee1d`.
The 25 fps gate still fails. These results do not establish the hardware's
maximum or an unavoidable Zorro III limit. Cacheable private ARM data and
direct card-local presentation remain untested.

**Performance requirement:** the user rejected 2 fps and requires 25 fps.
Treat sustained 25 fps at the initial 320x240 benchmark resolution as the gate
before further YouTube feature work. ZZVideo 0.1 passes correctness checks but
fails this performance gate. Existing logs show about 474 ms/frame overall,
approximately 132–145 ms for ARM decode/colour and 88 ms for SDL blit/update;
the target total budget is 40 ms. ARM times use an estimated timer frequency.
See the video development document for the proposed acceptance run and limits
of these measurements. No hardware or cache-policy changes were made during
this assessment.

Follow-up [ARM execution investigation](amiga-video-arm-execution.md): the
instruction-cache-only experiment is now physically verified. Median 320x240
playback across three runs per mode: **2.112 -> 2.152 fps (+1.86%)**. It does
not meet 25 fps. Cached ARM decode/colour/hash: about 78/54/34 ms per frame;
host copy/checks/display about 112/88/88 ms. All 225 frames in nine runs matched
native references; two integrity rechecks occurred. Every benchmark restored
SCTLR `08c50878` after running at `08c51878`, reported RET1/exit 0 and released
its allocation. Data cache and MMU stayed off; Core0/PL310 policy unchanged.
Temporary binaries/scripts/logs are in `RAM:ZZVideoPerf`; installed 0.1 remains
unchanged. All experimental apps are stopped, and the bridge is responsive.

A bounded automated pause/read/quit test passed with cache restoration and
exit 0. An earlier manual pause hit the existing 30-second transfer timeout,
exited 20 and still restored cache state/released memory. Long debugger pauses
remain limited by that deadline. Evidence is retained, not counted as a pass.
ZVP2 adds integrity-bound timing records; baseline and experiment must use
their bundled matching host/worker. The default build remains cache-off.
Six video tests, 23 ARM tests, 138-tool MCP smoke and ARM/68k component / loader
builds passed. Evidence: `amiga/records/2026-10-08/video-performance/`.

NEON auto-vectorises IDCT/motion compensation but not colour conversion;
the later physical comparison is recorded at the top of this section. No
private-data-cache variant has run. A minimal FFmpeg MPEG-1 decoder is a
candidate comparison, alongside private ARM data and card-local presentation.
Further YouTube feature work remains parked.

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
- Dedicated public backup/development repo (visibility verified 2026-10-08):
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
| Final Writer 7.3 | `HDD50Gig:APPS/FinalWriter7.3` | Editor startup verified on Workbench; stack 131072, Vera fonts, ARexx FW.1; printing untested |
| ZZDarkForcesNEXT 1.0 | `HDD50Gig:Games/ZZDarkForcesNEXT` | 640x480, SC55, stack 65536; user confirmed display/game works |
| ZZDoom 1.1 | `HDD50Gig:Games/ZZDoom` | Full three-episode `DOOM.WAD` copied from owner’s Mac; both icons select it, `NOMUSIC`, stack 65536; shareware retained |
| ZZQuake 1.0 | `HDD50Gig:Games/ZZQuake` | Original 320x240 version; registered data now installed (1.06 pak0 + CD pak1); earlier shareware display confirmed |
| ZZQuake HighRes 1.0 | Same ZZQuake drawer | 640/800/1024 launchers and corresponding blobs installed and checksum-verified; not launch-tested |
| ScummVM AGA 060 2.5.1.40 | `HDD50Gig:Games/ScummVM` | CAMD/native MT-32, serial `out.0`; launcher display confirmed, external MIDI untested |
| ZZBenchGUI 1.3 | `SD032G:Drivers/XACP/ZZBench` | ARM Core1 execution verified; ARM DDR bandwidth subtest reported 0.0 MB/s and remains unresolved |

ZZDoom data updated 2026-10-08 from the user's Mac `Downloads/doom/DOOM.WAD`:
11,159,840 bytes, MD5 `1cd63c5ddff1bf8ce844237f580e9cf3`. IWAD directory
bounds and all 27 maps across episodes 1–3 were checked locally; native Amiga
MD5 matches. Both ZZDoom icons now select `WAD=DOOM.WAD`, retaining NOMUSIC,
stack 65536 and other icon settings. Previous icons and Setup-Notes.txt are in
`HDD50Gig:Games/ZZDoom/backup-before-full-doom/`. Original shareware DOOM1.WAD
is unchanged (MD5 `f0cefca49926d00903cf57551d901abe`). No new game launch was
performed; the earlier title-screen test was with shareware. Firmware, game
executables, saves and Final Writer were untouched. Records contain hashes,
configuration evidence and helper source, not commercial WAD bytes:
`amiga/records/2026-10-08/doom-full/`. Temporary Mac transfer server stopped.

ZZQuake registered data added 2026-10-08 from the user's `Downloads/Quake
(USA)` CD image. MODE1/2352 data sectors were extracted, the split QUAKE101
1.01 installer reassembled and its LHA payload unpacked. Both PAK directory
bounds were checked. The older CD pak0 was not installed: the existing 1.06
pak0 remains (18,689,235 bytes, MD5 `5906e5998fc3d896ddaf5e6a62e03abb`), matching
the user's `Downloads/QUAKE/ID1/PAK0.PAK`. Added `id1/pak1.pak` (34,257,856
bytes, MD5 `d76b3e5678f0b64ac74ce5e340e6a685`) with episodes 2–4, ending,
deathmatch maps and registration data. Native MD5 matched before renaming the
incoming file into place. All four installed ZZQuake launchers use this id1
folder and automatically detect registered data. Icons, configuration, saves,
engine and firmware were unchanged; no new game launch was performed. CD
soundtrack tracks were not converted or installed, and Quake II files were
not used. Details: `HDD50Gig:Games/ZZQuake/Registered-Data.txt`; metadata and
verification only (no commercial game bytes):
`amiga/records/2026-10-08/quake-full/`. Temporary Mac HTTP server stopped.

The earlier Games search found only original SimCity CDTV; SimCity 2000 was subsequently installed as recorded above.

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

## Debugger repository release update (2026-10-08)

Amiga-MCP-Debugger is public. Commit `1a25d01` refreshes its main README
with debugger scope, SDL2/ARM responsibilities and the verified 0.2-to-0.3
performance table, removing unrelated project references. Release
https://github.com/SkiltonUSA/Amiga-MCP-Debugger/releases/tag/fractal-v0.3.0
now includes the hardware-verified standalone SDLZZFractal executable,
LHA/ZIP packages, instructions and checksums. All six GitHub asset digests
were checked against local files. The executable matches the prior physical
acceptance SHA-256 `533b2ee03a8fcc07933202766ca345b7438c9d8386521b784407a39d4b965b87`;
this is a mirror of the existing 0.3 build, not a rebuild or a new hardware test.
The release remains a prerelease. No Amiga files were changed.
