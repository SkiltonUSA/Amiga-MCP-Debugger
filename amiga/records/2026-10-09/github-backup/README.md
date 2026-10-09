# Development backup, 2026-10-09

Destination: https://github.com/SkiltonUSA/Amiga-MCP-Debugger (main).

Includes current Amiga source, development and installation records, session
memory, and the hardware-tested ZZTemperature 1.0 executable/icon and screenshot.
No commercial game content, installer archives, ROMs, firmware, credentials,
generated toolchains or full-disk copies are included. Video work remains parked.

Validation:

- `tests.log`: 23 ARM tests, MCP simulator smoke (138 tools), 10 fractal tests,
  and 8 SDL-fractal tests passed in the source workspace. The copied sources
  are byte-identical. The first video attempt hit a pre-existing broken
  Homebrew FFmpeg dependency (missing libx265.215.dylib).
- `video-tests.log`: 6 video tests passed in the dedicated repository with
  the locally available standalone FFmpeg 7.1 selected through `FFMPEG`.
- `components.log`: ARM SDK objects, 68k relay and software probe built in
  the dedicated repository.
- `launcher.log`: default ZZ9000 ARM/68k launcher build passed there.
- ZZTemperature package SHA-256 checksums verified against the exact binary
  installed on the Amiga on 2026-10-08. No rebuild or new hardware test claimed.

These backup checks do not resume the parked video project or replace physical
hardware acceptance. No Amiga files were changed during this backup.
