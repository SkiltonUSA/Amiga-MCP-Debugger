# Amiga MCP Debugger agent guide

Read docs/amiga-session-memory.md and docs/amiga-arm-debugging.md before hardware work.

Run make test-amiga-arm, make test-amiga, and the ARM/68k component build after changes. Also run make amiga-zz9000-build for launcher changes. Keep tools in .tools and local settings/artifacts in .context. Do not commit firmware, ROMs, commercial game data or credentials.

Never invent a shared DDR allocation. Verify ownership, both processor addresses, MMU/cache attributes and lifetime. Only one Core1 app may run at a time. Preserve Core0 services and AmigaOS. Physical XX19c Core1 acceptance is recorded in amiga/records/2026-10-08/arm-debug/zz9000. Its Exec-owned allocation and runtime mapping proof are mandatory; observed addresses are not fixed reservations. ARM MMU/caches remain off. Native and 68k relay results alone do not prove ARM execution or instruction stepping. No firmware flashing is required for cooperative checkpoint debugging.

Use the workspace launcher, not the upstream machine-global CLI. Do not open a second raw bridge connection. Record live evidence and distinguish pending work from verified results. Stop test applications cleanly and leave the bridge running.

The Workbench fractal demo lives in amiga/fractal/. Read docs/amiga-fractal-demo.md, run make test-amiga-fractal and make amiga-fractal-build for changes. Its request/result buffers remain inside the verified launcher allocation; preserve cancellation while paused.
