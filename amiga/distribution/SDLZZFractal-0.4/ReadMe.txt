SDL ZZFractal 0.4 Direct - A4000TX / ZZ9000 preview

The ARM computes and colours a complete 320x240 frame in owned ZZ9000 RAM.
It then copies the completed frame locally to a P96 bitmap. P96 presents
that bitmap to the SDL window; the statistics update after presentation.
The normal direct path does not return the pixel array to the 68060.
The previous completed image remains visible while rendering.

REQUIREMENTS
AmigaOS 3.2.x, 68030+ (tested TF4060/68060), ZZ9000 with Fast RAM enabled,
XX19c / XACP 1.7, Picasso96API.library and cybergraphics.library V40+.
A 640x480 mode with 32-bit BGRA storage is required for direct presentation.
The application opens its own temporary screen; Workbench is unchanged.
Allow 1 MiB ZZ9000 Fast RAM plus P96 screen/bitmap and application memory.
Only one Core1 application may run: close other ARM games/demos first.
The startup confirmation checks that you have selected the tested setup.

INSTALL / RUN
Double-click SDLZZFractal, or from its directory in a Shell:
  Protect SDLZZFractal +e
  Stack 131072
  SDLZZFractal
No Mac, network, MCP bridge or separate ARM payload is needed.
SDL2 SDK 0.2.0 is statically linked. No system library is replaced.

CONTROLS
A / ARM button: render on ARM. C / CPU button: render on 68060.
Left click image: zoom in; right click: zoom out; arrows: pan.
I / O: increase / decrease iterations. R: reset initial view.
X / Escape: cancel. M: iconify. Q / close gadget: quit.
F5: Workbench. F6: temporary 16-bit. F7: temporary 32-bit/direct.
Screen changes require an idle renderer. Workbench and 16-bit modes retain
traditional tiled ARM readback and SDL drawing for compatibility.
CPU mode remains the previous tiled reference implementation.
Q14 arithmetic bounds the zoom; no approximation was added to the kernel.

TIMING / LIMITS
Wall time includes compute, scheduling, colour, copy and presentation,
ending before the final statistics text update. ARM compute and colour
use a calibrated read-only hardware timer. Colour includes frame hashing.
Transfer measures small command/result synchronization in direct mode.
Draw includes the short bitmap-copy wait, P96 blit and initial UI work.
These overlapping counters must not be added to reconstruct wall time.
No per-tile progress/statistics drawing takes place during direct renders.
The copy is a completed-frame blit, not a vsync/page-flip guarantee.
P96/Layers still owns clipping and obscured-window refresh. OS UI work and
commands cross Zorro III; this is not a promise of zero bus activity.
The ARM caches/MMU and firmware remain unchanged. Very deep views are
bounded by a two-minute frame timeout (ten seconds per legacy tile).
Cancellation works while
cooperatively paused; this is not instruction stepping.

XACP platform by XANI / Xanxi:
https://github.com/Xanxi-Amiga/XACP-ZZ9000
Source and measurements: docs/amiga-sdl-fractal.md in
https://github.com/SkiltonUSA/Amiga-MCP-Debugger
This is an application pipeline change, not a new SDL2 acceleration API.
