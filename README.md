# Amiga MCP Debugger

Dedicated development and backup repository for the AmigaOS/ZZ9000 cooperative debugger and its Mac MCP host. Extracted from the Sixies development workspace; contains no C64 game source, ROMs, commercial game data or firmware binaries.

Nine MCP tools provide instrumented checkpoint debugging. Native C tests, live A4000TX 68060 relay acceptance, and **physical ZZ9000 Core1 debugging** passed. Two ARM sessions exercised all nine tools, 285 shared-memory echoes, relaunch and shutdown while paused. This is cooperative checkpoint debugging; it does not single-step instructions.

The [ARM acceleration roadmap](docs/amiga-arm-roadmap.md) starts with a Workbench fractal explorer, followed by runtime optimization, a reusable compute interface and image processing tools. The first [fractal demo](docs/amiga-fractal-demo.md) now renders on the physical A4000TX with matching 68060 and ARM results; later milestones remain planned.

Standalone Workbench distribution: [ZZFractal 0.1 packaging and requirements](docs/amiga-fractal-demo.md#standalone-distribution-01). LHA/ZIP assets are backed up with the release; the standalone executable needs no bridge or Mac.

The [SDL2 feasibility probe](docs/amiga-sdl2-probe.md) now displays ARM-generated
fractal tiles through a patched SDL2 AmigaOS backend on the physical A4000TX.
Windowed display, input, redraw timings and normal/early shutdown are verified;
audio, fullscreen and OpenRCT2 execution remain untested.
The library source, fix and [SDK preview release](https://github.com/SkiltonUSA/SDL2-AmigaOS3/releases/tag/v0.1.0)
are maintained separately in [SDL2-AmigaOS3](https://github.com/SkiltonUSA/SDL2-AmigaOS3).

The follow-on [interactive SDL fractal](docs/amiga-sdl-fractal.md) adds a reusable
cooperative tile client, distinct timing categories and true-colour screens.
Fourteen full-frame comparisons and 16-/32-bit native readback passed. The
standalone package is installed and passed Shell/Workbench rendering and clean
shutdown. Native mouse double-click restoration remains unverified because of
the bridge input limitation. Source, SDK and application packages are in the
[public v0.2.0 preview](https://github.com/SkiltonUSA/SDL2-AmigaOS3/releases/tag/v0.2.0).

## Develop

Install `uv`, Clang, LLVM `ld.lld` and Docker or Podman, then:

```sh
make setup-amiga
make test-amiga-arm
make test-amiga
make amiga-arm-build
make amiga-zz9000-build
make test-amiga-fractal
make amiga-fractal-build
make amiga-arm-demo
```

Setup pins upstream [Amiga DevBench](https://github.com/geekychris/amiga_mcp) and applies the included compatibility patch. Edit `.context/amiga/settings.json` for your container command and hardware IP. Use `make amiga-hardware` for the real bridge. Do not run two hardware servers against the same Amiga. Local settings, tools and build outputs are ignored by Git.

Read [debugger architecture and acceptance](docs/amiga-arm-debugging.md), [hardware session memory](docs/amiga-session-memory.md), and [Amiga development setup](docs/development-amiga.md). Historical paths or Sixies names in these records retain their original context.

This repository preserves project-owned source, dependency pins, build instructions and selected system evidence. Vendor components retain their own licenses and are fetched separately. No blanket license is asserted over third-party records or components.


## Standalone ARM video preview

ZZVideo 0.1 decodes MPEG-1 video on physical ZZ9000 Core1, with a minimal
68k SDL2 interface. The installed A4000TX copy and 100 reference frame hashes
passed. Current playback is about 6.8 fps at 160x128 and 2.1 fps at 320x240;
this is a decoding foundation, with YouTube retrieval, streaming, audio and
H.264 still pending. [Development and build guide](docs/amiga-video-client.md).
