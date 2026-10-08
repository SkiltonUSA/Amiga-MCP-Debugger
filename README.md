# Amiga MCP Debugger

Dedicated development and backup repository for the AmigaOS/ZZ9000 cooperative debugger and its Mac MCP host. Extracted from the Sixies development workspace; contains no C64 game source, ROMs, commercial game data or firmware binaries.

Nine MCP tools provide instrumented checkpoint debugging. Native C tests and live A4000TX **68060 relay** acceptance passed. Physical ZZ9000 ARM launcher/cache integration is in development; this is not an instruction-step debugger.

## Develop

Install `uv`, Clang and Docker or Podman, then:

```sh
make setup-amiga
make test-amiga-arm
make test-amiga
make amiga-arm-build
make amiga-arm-demo
```

Setup pins upstream [Amiga DevBench](https://github.com/geekychris/amiga_mcp) and applies the included compatibility patch. Edit `.context/amiga/settings.json` for your container command and hardware IP. Use `make amiga-hardware` for the real bridge. Do not run two hardware servers against the same Amiga. Local settings, tools and build outputs are ignored by Git.

Read [debugger architecture and acceptance](docs/amiga-arm-debugging.md), [hardware session memory](docs/amiga-session-memory.md), and [Amiga development setup](docs/development-amiga.md). Historical paths or Sixies names in these records retain their original context.

This repository preserves project-owned source, dependency pins, build instructions and selected system evidence. Vendor components retain their own licenses and are fetched separately. No blanket license is asserted over third-party records or components.
