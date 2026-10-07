# OutRun 2006 Coast 2 Coast on Nintendo Switch (homebrew)

## Install

1. Copy your OutRun 2006 Coast 2 Coast installation (Steam version) to a
   folder on the SD card, for example `sdmc:/switch/outrun2/`.
2. Put `OutRun2006-NX.nro` in that same folder.
3. Start it from the Homebrew Menu (full-memory mode: hold R while starting
   a game).

The first launch reads your `OR2006C2C.EXE`, checks it and saves the
prepared data there as `OR2006C2C.cache` (text screen with the steps).
Later launches start directly. The game files are only read.

## Build

Needs devkitPro with devkitA64, libnx, deko3d and the portlibs switch-zlib,
switch-libvorbis and switch-ffmpeg.

    make -C switch

The NRO is written to `switch/OutRun2006-NX.nro`. `source/` holds the
platform layer (libnx, deko3d, audio, network), `shaders/` the GLSL
shaders compiled into the NRO.

## Credits

Inspired and using tweaks by [OutRun2006Tweaks](https://github.com/emoose/OutRun2006Tweaks)
by emoose. See `THIRD_PARTY_NOTICES.md` at the repository root.

OutRun is a trademark of Sega. This project is not affiliated with or
endorsed by Sega or Sumo Digital.

This source code is be freely available for use, modification, and redistribution, subject to the licenses of the original projects it is based on.

Feel free to reuse any part of this project in your own work. Please give proper credit where due.

If you reuse our modifications, please keep an appropriate reference to this project and its contributors.

This project is developed and maintained in my free time.

If you enjoy the project and would like to support its development, donations are appreciated but entirely optional:

https://ko-fi.com/r4dius
