ZZVideo 0.1 - standalone ARM MPEG-1 video preview
A4000TX / ZZ9000 XX19c / XACP 1.7

Close other ARM Core1 games/apps before starting, including ZZFractal.
Double-click ZZVideo, accept Start, then choose demo.mpg (or your own clip).
From Shell:
  Stack 131072
  ZZVideo demo.mpg

O / Open       Choose a file
Space          Play / pause
R / Replay     Return to the beginning and play
S / Stop       Stop and return to the beginning
Q / Esc        Quit
Controls can also be clicked. The last frame stays visible at end of file.

This first milestone decodes video on the ZZ9000 ARM Core1. The Amiga handles
file I/O, timing, controls and display. No Mac, bridge, network or server is
needed during playback. Firmware, drivers and startup files are not changed.

Supports MPEG-1 Program Stream (.mpg) or elementary video (.m1v), up to 4 MiB.
Dimensions must be multiples of 16, at most 320x240, constant size/frame rate.
Recommended: MPEG-1 320x240, 25 fps, moderate bitrate. MP2 audio is ignored.
The whole compressed file is loaded before playing. No streaming, audio,
YouTube search/URL resolution, MPEG-2, H.264/MP4, scaling or seek bar yet.
The included demo.mpg is a generated test pattern, not a YouTube download.

Measured developer-preview playback on the A4000TX: about 6.8 fps at 160x128,
2.1 fps at 320x240. Playback slows when decoding/transfer/display cannot keep up.
These are single test-clip measurements, not performance guarantees. Integrity
checks and the current cache-off ARM/display path are deliberately retained.

Requires AmigaOS 3.x, Picasso96, tested XX19c firmware, 8 MiB free contiguous
ZZ9000 Fast RAM plus frontend storage, and Shell/icon stack 131072.
The supplied icon sets this stack. Do not run concurrently with a Core1 game.
This is a development preview tested with generated clips, not a hardened
player for arbitrary corrupt/untrusted files.

YouTube integration is the next stage; see docs/amiga-video-client.md in the
source workspace. AmiTube is a candidate frontend/server interface. H.264
and MP4 support are separate milestones and are not implied by this build.
