# OutRun 2006 Coast 2 Coast: Xbox port

The game on Xbox One and Xbox Series X|S in **Developer Mode**, as a UWP
application, and the same platform layer as a Windows program for testing
on a PC. Nothing from the original game is in the package: the port reads
your own Steam game folder.

Status: the PC test program and the unsigned package build. Not tested on a console yet.

## Layout

| Path | Content |
| --- | --- |
| `source/main.cpp` | entry flow (same as `ps5/source/main.cpp`): game folder, options, result log |
| `source/renderer.*`, `renderer_present.inc` | frontend, loading screens, title movie and PC scene composition (from the PS5 port) |
| `source/d3d11_d3d9.*` | Direct3D 11 back end of the D3D9 device (the PS5 GL device's split) |
| `source/d3d11_display.*` | swap chain and 2D composition |
| `source/audio.cpp` | XAudio2 output, 48 kHz stereo |
| `source/platform_api.cpp` | gamepad (Windows.Gaming.Input), game folder search |
| `source/window_uwp.cpp` | Xbox entry (CoreWindow) |
| `source/window_win32.cpp` | PC test entry (Win32 window) |
| `tools/generate_hlsl_shaders.py` | translates the pixel shaders in `switch/shaders` to HLSL at build time |
| `package/` | UWP manifest and package images (`tools/make_icons.ps1` at the repository root) |

The Direct3D 11 code targets feature level 10.1 (shader model 4.1), the
limit for UWP games on Xbox One.

## Build (Windows)

Visual Studio 2026 with *Desktop development with C++*, *Universal Windows
Platform development* and *C++ Clang tools*, Python 3, CMake and an MSYS2
bash with make. The libraries linked into the package (zlib, Ogg/Vorbis and
a minimal LGPL FFmpeg for the title movie) are built at pinned versions by:

    powershell -File xbox/tools/build-dependencies.ps1 -Out <deps>

`<deps-options>` below stands for the options it prints:
`-DZLIB_INCLUDE_DIR=<deps>/zlib/include -DZLIB_LIBRARY=<deps>/zlib/lib/zlibstatic.lib
-DOR2_XBOX_VORBIS_ROOT=<deps>/vorbis -DOR2_XBOX_FFMPEG_ROOT=<deps>/ffmpeg`.

The PC configuration (clang-cl) builds the engine, the Xbox platform code
and the PC test program:

    cmake -S . -B build-xbox-pc -G "Visual Studio 18 2026" -A x64 -T ClangCL -DOR2_XBOX=ON <deps-options>
    cmake --build build-xbox-pc --config Release --target OutRunXboxPC

The UWP configuration uses MSVC (Visual Studio has no clang-cl toolset for
UWP projects): it compiles only `source/window_uwp.cpp` and links the PC
configuration's libraries into the package:

    cmake -S . -B build-xbox -G "Visual Studio 18 2026" -A x64 -DOR2_XBOX=ON         -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0         -DOR2_XBOX_ENGINE_DIR=<path to build-xbox-pc> <deps-options>
    cmake --build build-xbox --config Release --target OutRunXbox

The package is written to `build-xbox/xbox/AppPackages/OutRunXbox/` (`.msix`,
unsigned, with `Dependencies/x64/Microsoft.VCLibs.x64.14.00.appx`).

## Game folder

The game folder may hold the original supported Steam `OR2006C2C.EXE`
(962,560 bytes). The shared `src/ports/pecompact_memory.cpp` decoder
extracts its data image in memory, verifies the expected ranges and saves
`OR2006C2C.cache` at first launch. A decompressed copy or an existing valid
cache remains supported. No PC executable code is run. Other EXE editions
are rejected.

The PC and UWP builds inherit the decoder from the engine libraries.
This integration is prepared for Xbox use; console file access and
first-launch behavior still require a real Xbox test. The current Xbox
platform layer requires a writable game folder. To prepare the cache on
a PC, run `or2setup <game folder>` before copying that folder to the Xbox.

The game folder is also where the prepared EXE image (`OR2006C2C.cache`, as
on the other platforms), saves, options and logs go.

- PC test build: put `OutRunXboxPC.exe` in your Steam game folder.
- Xbox: copy your Steam game folder to the root of a USB drive as
  `OutRun2006`. The application's `LocalState\OutRun2006` folder (filled
  through the Device Portal) is searched next.

## Install on the console

1. Switch the console to Developer Mode (Xbox Dev Mode Activation app).
2. In Dev Home, turn on Remote Access and note the console's address.
3. On a PC, open the Device Portal (`https://<console address>:11443`) and
   add the `.msix` with its `Microsoft.VCLibs` dependency.
4. In Dev Home, select the game, *View details*, and set its type to **Game**
   (a UWP *App* only gets part of the GPU and 1 GB of memory).
5. Plug in the USB drive with `OutRun2006` and start the game.

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
