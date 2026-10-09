# ZZTemperature 1.0

Native AmigaOS monitor for the ZZ9000's **Zynq chip temperature**. Adds
**Tools → ZZ9000 Temperature…** to Workbench's top menu bar. The small window
shows Celsius, session minimum/maximum, and a valid-sample count; it refreshes
once a second. Closing the window or pressing Escape hides it. The menu
reopens it; `R` resets the statistics. Temperature is shown in the window,
not continuously in the top bar.

## Installed A4000TX

Executable: `SYS:WBStartup/ZZTemperature` (15,856 bytes).
The accompanying tool icon sets `DONOTWAIT`, `STARTPRI=-5`, and a 16 KiB stack.
Workbench launch registers the menu without opening a window. It runs locally
on the Amiga and does not require the Mac or the MCP connection.

The menu is already registered in the current session. At the next Workbench
startup, WBStartup loads it again. A cold reboot has not been performed for
this installation; Workbench launch through `C:WBLoad` was tested instead.

Shell commands (keywords are uppercase):

```text
SYS:WBStartup/ZZTemperature ONCE
SYS:WBStartup/ZZTemperature SHOW
SYS:WBStartup/ZZTemperature STOP
```

`ONCE` prints one reading and exits. `SHOW` opens the existing instance's
window; if no instance is running it starts the monitor in the foreground.
For a fresh background launch:

```text
Stack 16384
Run >NIL: SYS:WBStartup/ZZTemperature MENU
```

`STOP` removes the menu and exits. To uninstall, stop it first, then remove
`SYS:WBStartup/ZZTemperature` and `SYS:WBStartup/ZZTemperature.info`.
There are no settings files or startup-sequence edits to undo.

## Hardware interface and attribution

Register definitions and units were checked against MNT Research's official
[ZZTop 1.1 beta source](https://mntre.com/media/ZZ9000_info_md/zz9000-firmware-archive/zztop-1.1b.lha).
Their [firmware documentation](https://mntre.com/media/ZZ9000_info_md/zz9000-firmware-archive/zz9000-firmware-archive.html)
specifies firmware 1.7+ for the Zynq temperature facility. This application is
an original implementation using that documented interface.

- Discover the board with expansion.library: manufacturer `0x6d6e`, product
  4 (Zorro III), falling back to product 3 (Zorro II).
- Use the discovered board base; read a volatile 16-bit register at `+0xe0`.
  Its value is tenths of a degree Celsius.
- `ONCE` also reads firmware register `+0xc0`. The A4000TX returned `0x0113`;
  this register alone does not identify the XX19c suffix or XACP revision.
- Zero, all-ones, and values above 150.0 C are rejected as unavailable.
  This is a sanity check, not a thermal alarm or a calibration guarantee.
- No device-register writes, XACP commands, ARM payloads, memory mappings,
  cache changes, fan controls, or firmware updates are involved.

This measures the ZZ9000 Zynq die, **not the TF4060's 68060 or the case**.
Existing NewMeter and ToolsDaemon settings are untouched.

## Build

Requires an AmigaOS m68k GCC toolchain with the NDK and libnix/libamiga:

```sh
sh amiga/zztemperature/build.sh
```

Default outputs go to `.context/amiga/zztemperature/`; a first argument can
choose another directory. `AMIGA_CC` overrides the compiler executable.
The build targets 68020+, uses `-noixemul`, and enables
`-Wall -Wextra -Werror`. Tested compiler: see the acceptance record.
The normal AmigaOS libraries are sufficient; SDL, MUI, and a floating-point
library are not needed.

`MakeZZTemperatureIcon <path-to-executable>` creates the startup tool icon
using icon.library's default tool artwork. Do not run it on an existing icon
whose appearance or tooltypes need preserving.

The libamiga `sprintf` implementation uses RawDoFmt-style word arguments for
`%d`; GUI formatting deliberately uses `%ld` with explicit long arguments.
The visual hardware check caught this difference from libc `printf`.

`menu-check.c` is an optional hardware acceptance helper. It finds the real
Workbench menu label; `CLOSE` sends an IDCMP close to this application's
window, and `INVOKE` sends the discovered menu selection to Workbench.
It is not installed in WBStartup. Build with the same compiler flags and
`-lamiga`. Tests must run sequentially with a responsive Workbench.

## Verified on hardware, 2026-10-08

- Live readings approximately 53 C on the A4000TX/ZZ9000 with XX19c installed.
- Correct window formatting, changing readings, and session minimum/maximum.
- Closing hides the window; selecting the real Workbench menu reopens it.
  Selection was exercised using an injected IDCMP menu event, not a physical
  mouse click.
- Repeated `SHOW` calls retain one public port, one window, and one instance.
- `STOP` removes the public port, window, and menu.
- Installed executable's MD5 matches the Mac build; Workbench launch via
  `C:WBLoad` starts hidden and successfully registers the menu.
- SimCity 2000's screen stayed open throughout. No reboot was performed.

See `amiga/records/2026-10-08/zztemperature/acceptance.txt` for installed hashes.
