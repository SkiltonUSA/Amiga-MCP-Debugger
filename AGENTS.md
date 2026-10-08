# Amiga MCP Debugger agent guide

Read docs/amiga-session-memory.md and docs/amiga-arm-debugging.md before hardware work.

Run make test-amiga-arm, make test-amiga, and the ARM/68k component build after changes. Keep tools in .tools and local settings/artifacts in .context. Do not commit firmware, ROMs, commercial game data or credentials.

Never invent a shared DDR allocation. Verify ownership, both processor addresses, MMU/cache attributes and lifetime. Only one Core1 app may run at a time. Preserve Core0 services and AmigaOS. Native and 68k relay results do not prove ARM execution or instruction stepping. No firmware flashing is required for cooperative checkpoint debugging.

Use the workspace launcher, not the upstream machine-global CLI. Do not open a second raw bridge connection. Record live evidence and distinguish pending work from verified results. Stop test applications cleanly and leave the bridge running.
