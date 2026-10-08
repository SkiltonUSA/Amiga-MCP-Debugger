ZZFractal 0.1 - Experimental ZZ9000 ARM fractal demo
8 October 2026

INSTALL AND RUN
Extract the complete archive into any drawer on your Amiga.
Open the ZZFractal drawer and double-click the ZZFractal icon.
The icon sets a 65536-byte stack. Check the firmware requirement in the
launch requester and choose Start. Keep ZZFractal.info with the executable.
No installer, assigns, startup edits or extra payload files are required.
You can move the whole drawer to another disk or rename its parent drawer.

Shell alternative:
  Stack 65536
  ZZFractal

REQUIREMENTS
- Classic Amiga with a 68020 or newer CPU; tested on TF4060/68060 A4000TX.
- Zorro III ZZ9000 with 256 MB Amiga-visible Fast RAM enabled and enough
  free memory for a contiguous, aligned 128 KB reservation.
- XX19c / XACP 1.7 firmware. Other firmware is NOT established compatible.
  The legacy firmware register cannot identify XX19c by itself; the
  Workbench requester asks the owner to confirm this configuration.
- AmigaOS 3.x and Picasso96API.library v2 or later. Tested on AmigaOS 3.2.3
  with an 8-bit RTG Workbench; broader hardware/OS compatibility is untested.
- A public Workbench screen large enough for the 408 x 339 window.
- Allow at least 1 MB free general RAM for the application and OS resources.

No AmigaBridge, MCP server, Mac, Ethernet, ARexx, or ixemul.library is needed.
The release executable does not link the bridge client or register with it.
The ARM program is embedded in the Amiga executable.
Firmware, P96, ROMs and operating-system files are not included.

IMPORTANT: Close other ZZ9000 ARM applications before starting. Quit this
application before launching ZZQuake, ZZDoom, ZZDarkForces or another ARM app.
CPU comparison mode still reserves the ARM worker. Our own duplicate-instance
check cannot coordinate unrelated ARM applications.

CONTROLS
A / ARM       Render the current view on ARM Core1.
C / CPU       Render the current view on the Amiga CPU.
Click image   Center on that point and zoom 2x (six levels maximum).
X / Escape    Cancel rendering.
R / Reset     Restore the initial view and render again.
Q / close     Quit and release the ARM and graphics resources.

The image is 320 x 240 with 128 iterations and a shared Workbench palette.
The display updates in tiles, so the interface remains usable during a render.
Render time includes scheduling, data exchange and drawing, and should not
be interpreted as an isolated CPU benchmark.

THIS FIRST RELEASE
This is an experimental preview for the tested setup, not a broad compatibility
release. ARM caches/MMU remain disabled. Deep zoom, Julia sets, image export,
ARexx commands and an application acceleration library are future work.
The separate developer build supports cooperative MCP checkpoints; that
interface is deliberately absent from this distribution executable.

TROUBLESHOOTING
If startup fails, read its requester (Workbench) or output (Shell).
Use the supplied icon or Shell Stack 65536. Do not work around a failed
memory-mapping check by changing addresses or firmware files.
The program installs no device drivers and makes no persistent system changes.
Delete the extracted drawer and its drawer icon to uninstall.

DISTRIBUTION
Keep this preview's ReadMe, executable, icons and third-party notices together
when sharing it. No blanket open-source license is asserted over the project.
Third-party runtime terms are described in ThirdParty.txt and accompanying
license files. Software is supplied as an experimental preview without a
promise of fitness for a particular purpose.
