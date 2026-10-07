# Third-party notices

This repository contains no part of OutRun 2006 Coast 2 Coast: no executable
code, game data or original source files. The ported functions are
reconstructions written for this project from the behaviour of the PC
version; the player supplies their own copy of the game, and the port reads
the executable's tables from it at run time. Test fixtures hold synthetic
inputs and the values the original functions computed on them, not game
code or assets.

## OutRun2006Tweaks (MIT)

This project is inspired by [OutRun2006Tweaks](https://github.com/emoose/OutRun2006Tweaks).

Copyright (c) 2023 emoose

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Libraries used by the builds

Not included in this repository; fetched and built by the build scripts at
pinned versions, each under its own licence.

- macOS (`mac/tools/build-dependencies.sh`, versions and SHA-256 in
  `mac/tools/dependencies.lock`, licence texts in `mac/third_party/`):
  SDL2 2.32.10 (zlib), FFmpeg 8.0 (LGPL 2.1 or later, without GPL or
  non-free parts), Ogg 1.3.5 and Vorbis 1.3.7 (BSD), pkgconf 2.3.0.
- Nintendo Switch: devkitPro toolchain, libnx, deko3d and portlibs (zlib,
  libvorbis, libogg, FFmpeg), each under its own licence.
- Xbox (`xbox/tools/build-dependencies.ps1`, pinned versions with SHA-256),
  linked statically into the package: zlib 1.3.1 (zlib), Ogg 1.3.5 and
  Vorbis 1.3.7 (BSD), and FFmpeg 7.1.1 (LGPL 2.1 or later) built with only
  the Bink demuxer and decoders, swscale and swresample, without GPL or
  non-free parts. The port's source and that script let anyone rebuild the
  package with a modified FFmpeg, as the LGPL requires; FFmpeg's source is
  at https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz.
- PS5 homebrew: see `ps5/third_party/README.md`.
- General: zlib (zlib licence).
