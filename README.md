# Amiga MCP Debugger

Develop and debug AmigaOS applications from a Mac, including applications that offload computation to the ZZ9000's ARM Cortex-A9. This repository extends [Amiga DevBench](https://github.com/geekychris/amiga_mcp) with cooperative ARM debugging, a verified shared-memory compute interface, and a fractal demo tested on a physical A4000TX.

The Mac runs the MCP server. The existing **AmigaBridge** runs on the 68060 and connects over Ethernet through Roadshow, providing AmigaOS inspection, files, Shell commands and ARexx. Our additional 68k launcher and ARM worker provide the ZZ9000 debugging and computation path.

## XACP firmware and the debugging protocol

**The physical ARM debugger depends on the tested XX19c / XACP 1.7 firmware setup.** We acknowledge **XANI's [XACP-ZZ9000 project](https://github.com/Xanxi-Amiga/XACP-ZZ9000)** and its firmware, application examples and developer documentation as the foundation for this integration.

The debugger uses the firmware's existing ARM launch/reset interface to start and stop its Core1 worker. Debugging commands then travel through our own shared-memory protocol inside the instrumented application:

| Connection | Mechanism |
| --- | --- |
| Mac MCP server → Amiga | Existing AmigaBridge Ethernet protocol |
| 68060 launcher → ZZ9000 firmware | Existing ARM launch/reset interface on the tested XX19c / XACP setup |
| 68060 launcher ↔ ARM worker | Our shared-memory protocol for checkpoints, watched values, memory inspection and logs; a separate application compute protocol carries jobs and results |

We have not added debugging commands to XACP or modified its firmware. The debugger builds on the existing ARM execution facilities; its checkpoint protocol is our application-level extension, not an XACP debugging standard. Ordinary MCP operations—files, Shell commands, system inspection and ARexx—use AmigaBridge and do **not** require XACP or a ZZ9000.

See XANI's [XACP 1.7 developer notes](https://github.com/Xanxi-Amiga/XACP-ZZ9000/blob/main/docs/XACP_V1_7_DEVELOPER_NOTES.md) and our [launcher and memory contract](docs/amiga-arm-debugging.md#memory-ownership-and-cache-contract) for the integration details.

## Download the executable demo

**[SDL ZZFractal 0.3.0 — release and downloads](https://github.com/SkiltonUSA/Amiga-MCP-Debugger/releases/tag/fractal-v0.3.0)**

- **[Amiga LHA package](https://github.com/SkiltonUSA/Amiga-MCP-Debugger/releases/download/fractal-v0.3.0/SDLZZFractal-0.3-XX19c.lha)** — recommended; includes the executable, Workbench icons, instructions, licences and build records.
- **[ZIP package](https://github.com/SkiltonUSA/Amiga-MCP-Debugger/releases/download/fractal-v0.3.0/SDLZZFractal-0.3-XX19c.zip)** — the same distribution in ZIP format.
- **[Standalone Amiga executable](https://github.com/SkiltonUSA/Amiga-MCP-Debugger/releases/download/fractal-v0.3.0/SDLZZFractal)** — the native 68k Hunk executable, with its ARM worker and SDL2 linked in.
- **[SHA-256 checksums](https://github.com/SkiltonUSA/Amiga-MCP-Debugger/releases/download/fractal-v0.3.0/SHA256SUMS.txt)**.

Extract the package and double-click `SDLZZFractal`. It needs **no Mac, MCP server, network connection or separate ARM payload**. This is an application preview, version 0.3.0; it uses SDL2 SDK 0.2.0. The standalone release omits the debugger connection; build the developer variant below to use MCP debugging.

Tested on **A4000TX / TF4060 (68060), AmigaOS 3.2.3, ZZ9000 with XX19c / XACP 1.7, ZZ9000 Fast RAM enabled, Picasso96 and compatible cybergraphics.library**. The application requires cybergraphics.library V40+; the tested version is 42.7. Close other ZZ9000 Core1 applications before launching, including when selecting CPU mode. The release does not install or change firmware, drivers or startup files.

For the bare executable, copy it to an Amiga directory and run from Shell:

```text
Protect SDLZZFractal +e
Stack 131072
SDLZZFractal
```

Press **A** for ARM rendering or **C** for 68060 rendering. Click to zoom, use arrows to pan, **R** to reset, **X/Escape** to cancel and **Q** to quit. F5/F6/F7 select Workbench or temporary 16-/32-bit screens while idle. See the [demo guide](docs/amiga-sdl-fractal.md) for all controls and requirements.

## ZZ9000 temperature monitor

[ZZTemperature 1.0](amiga/zztemperature/README.md) is a small native AmigaOS
utility that adds **Tools → ZZ9000 Temperature…** to Workbench. Its window
shows the live Zynq temperature and session minimum/maximum, updating every
second. It runs independently of the Mac and MCP server.

The [tested executable, Workbench icon and checksums](amiga/distribution/ZZTemperature-1.0/)
are backed up here alongside the source. On the A4000TX it reported roughly
53–54°C. Window updates, menu selection, close/reopen, single-instance handling
and Workbench launch were verified; cold-boot startup remains untested.
It uses MNT's existing read-only sensor register, with no XACP commands or
firmware changes. See the [hardware acceptance record](amiga/records/2026-10-08/zztemperature/acceptance.txt).

## Archived video experiments

The [ARM execution measurements](docs/amiga-video-arm-execution.md) and
[FFmpeg benchmark attempt](amiga/records/2026-10-08/ffmpeg-bench/experiment.json)
are preserved for reference. The video project is **parked**: the experiments
did not meet the 25 fps target, and the FFmpeg attempt produced no valid ARM
decode timing. These records do not establish a working YouTube client.

## What the debugger adds

Nine MCP tools connect to an instrumented ARM application through the existing bridge's `CALLHOOK` interface:

| Tools | Capability |
| --- | --- |
| `amiga_arm_attach`, `amiga_arm_detach` | Connect to a debugging session and release it |
| `amiga_arm_status` | Inspect checkpoints, watched values, breakpoints and application-reported fault context |
| `amiga_arm_pause`, `amiga_arm_continue` | Pause at a cooperative checkpoint or resume |
| `amiga_arm_step_checkpoint`, `amiga_arm_breakpoint` | Advance to the next checkpoint or set checkpoint breakpoints |
| `amiga_arm_read_memory` | Read registered application memory while paused or faulted |
| `amiga_arm_logs` | Retrieve application log records |

**This is cooperative checkpoint debugging, not instruction-level stepping.** Applications must include the debugging SDK. It cannot attach to arbitrary unmodified ARM programs, and it does not provide a full live register capture or GDB/DWARF source stepping.

Physical ZZ9000 Core1 acceptance exercised all nine tools in two sessions, 285 shared-memory visibility echoes, relaunch, and shutdown while paused. The launcher owns its shared allocation, verifies the Amiga-to-ARM address mapping at runtime, and returns Core1 before freeing memory. ARM MMU/caches remain disabled under the tested contract; the 68k performs shared-range cache synchronization. No fixed shared DDR address is assumed.

The reusable compute client adds job/session identity, cancellation and draining, bounded timeouts, and validation of result pixels and timing metadata. Incomplete shared-memory reads are rechecked before display; stale or corrupt results are not accepted. See [debugger architecture and acceptance](docs/amiga-arm-debugging.md) and [compute/visibility validation](docs/amiga-sdl-fractal.md#03-performance-and-shared-memory-validation).

## SDL2 and ARM: separate responsibilities

**SDL2 runs on the 68k. ARM acceleration is provided by our separate compute helper and application-specific ARM worker, not by new ARM functions inside SDL2.** Linking an existing application against this SDL2 library does not automatically offload its work.

The [SDL2 AmigaOS3 SDK](https://github.com/SkiltonUSA/SDL2-AmigaOS3) contains our changes to the existing AmigaOS backend:

- Correct graphics-library handling instead of treating Picasso96API as a CyberGraphX library base.
- Clipped window drawing that respects movement, overlapping windows and partial update rectangles.
- Named public-screen selection, used by the demo for Workbench and temporary true-colour screens.

The application sends bounded Mandelbrot jobs to the Cortex-A9. The 68k receives validated results, maps them to colours and uses SDL2 to display them. The standalone demo embeds both processor programs; the developer build also exposes MCP hooks and checkpoints.

## Measured improvements: demo 0.2 to 0.3

Default **320×240** Mandelbrot view on the same A4000TX, median of three developer-build runs per mode:

| Measurement | Demo 0.2 | Demo 0.3 | Time reduction |
| --- | ---: | ---: | ---: |
| ARM complete render | 7.609 s | **3.848 s** | **49.4%** |
| ARM calculation time, estimated | 2.899 s | **1.472 s** | **49.2%** |
| 68060 complete render | 17.292 s | **4.328 s** | **75.0%** |
| 68060 calculation time | 2.235 s | **1.692 s** | **24.3%** |

**Both versions use the same SDL2 SDK 0.2.0 library.** These gains come from the fractal kernel and cooperative scheduling: keeping orbit state local, exact fixed-point optimisations, bounded row checkpoints, and shorter timer-based yields instead of unnecessary one-tick waits. They are not an SDL2 rendering acceleration claim.

Complete-render time includes computation, scheduling, transfer and display. ARM calculation time is estimated using a calibrated hardware timer. CPU and ARM compiler settings, instrumentation and cache policies differ, so these results are application measurements rather than a general processor speed comparison. The ARM cache policy and firmware were unchanged.

Validation included 14 complete 76,800-pixel comparisons against an independent reference, 20 additional ARM render checksum checks, native 16-/32-bit colour readback, cancellation while the ARM debugger was paused, repeated cancellation, overlapping/moved windows and clean shutdown. The standalone executable separately passed ARM/CPU rendering, cancellation and clean exit. Physical AppIcon double-click restoration remains unverified.

[Detailed measurements](docs/amiga-sdl-fractal.md#03-performance-and-shared-memory-validation) · [Raw benchmark and acceptance records](amiga/records/2026-10-08/sdl-performance)

## Develop

Install `uv`, Clang, LLVM `ld.lld` and Docker or Podman, then:

```sh
make setup-amiga
make test-amiga-arm
make test-amiga
make amiga-arm-build
make amiga-zz9000-build
```

Setup pins upstream Amiga DevBench and applies the included socket compatibility patch. Configure the container command and hardware IP in `.context/amiga/settings.json`, start AmigaBridge in TCP mode on the Amiga, and run `make amiga-hardware` on the Mac. Keep a single hardware server connected to the bridge. `make amiga-arm-demo` runs the host software demo; it is not evidence of physical ARM execution.

For the SDL fractal developer build, build the pinned SDK in the [SDL2 repository](https://github.com/SkiltonUSA/SDL2-AmigaOS3), then supply its local checkout:

```sh
make amiga-sdl-fractal-build SDL2_SOURCE=/path/to/SDL2-AmigaOS3
make test-amiga-sdl-fractal
make test-amiga-fractal
```

See [Amiga development setup](docs/development-amiga.md), [hardware session memory](docs/amiga-session-memory.md), [fractal build instructions](docs/amiga-sdl-fractal.md#build-and-test) and the [ARM development roadmap](docs/amiga-arm-roadmap.md). Local settings, tools and build outputs are ignored by Git.

## Attribution and scope

The original MCP server and AmigaBridge come from **[geekychris/amiga_mcp](https://github.com/geekychris/amiga_mcp)**. This repository adds the Mac workspace integration, bridge startup helpers, cooperative ARM debugger, ZZ9000 launcher, compute interface and demonstration applications. SDL2 is based on **[bdgscotland/libSDL2-amigaos3](https://github.com/bdgscotland/libSDL2-amigaos3)**, with our fixes maintained separately.

**[XANI / Xanxi-Amiga's XACP-ZZ9000](https://github.com/Xanxi-Amiga/XACP-ZZ9000)** provides the XACP firmware environment and reference material used by the tested ARM integration. Credit for XACP remains with its authors; our MCP debugger and shared-memory protocols are separate additions.

Vendor components retain their own licences and notices. Release packages include their applicable licence files. No blanket licence is asserted over third-party records or components; firmware and ROM binaries are not distributed here.
