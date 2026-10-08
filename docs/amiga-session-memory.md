# A4000TX session memory

Updated 2026-10-08. Read this before further work on the real Amiga. This is
the durable handoff for the development setup and installations in this branch.
The user requested this GitHub backup. It preserves source, configuration and
evidence; it is not a backup of the Amiga's disks or installed commercial games.

## Current working state

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
| KickSmash32 | Installed, owner-confirmed 2026-10-08; board revision, firmware, selected ROM bank and USB connection unverified |
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
services on this machine. No KickSmash firmware, ROM bank or utility setup has
been inspected or changed in response to this hardware note.

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
