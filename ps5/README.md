# OutRun 2006 Coast 2 Coast on PS5 (homebrew, experimental)

OpenGL and Vulkan boot and reach races on firmware 13.60; race performance is
being investigated. Native Vulkan explicitly dismisses the Shell launch image
after presenting its first frame.
This build installs and runs no exploit;
it needs a homebrew environment that installs packages and gives FTP access
(etaHEN, for example).

## What you need

- Your own OutRun 2006 Coast 2 Coast installation (Steam version), with its
  original `OR2006C2C.EXE`. At first launch the title extracts the supported
  PECompact build in memory, checks the ranges used by the game and saves
  `OR2006C2C.cache` in the game folder. No executable code from the PC file
  is run. An already decompressed copy is also accepted; other editions are
  rejected.
- `OutRun2006-PS5.pkg` (made by the build, below).

Nothing from the original game is included in the package: the port reads
your own files.

## Install

1. Install `OutRun2006-PS5.pkg` with your homebrew environment's package
   installer (for example from a USB drive).
2. Copy your Steam game folder, unchanged, to
   `/data/homebrew/PPSA99106/assets/game` by FTP.
3. Start the game. The first launch prepares `OR2006C2C.cache` (text screen
   with the steps); later launches start directly.

The game data cannot be inside the package: the PKG builder available for
PS5 homebrew (LibProsperoPkg) caps the image at about 569 MiB and the game
data is about 1 GB.

## Where the title looks for the game

`/app0/assets/game` (inside the title folder), then
`/data/homebrew/PPSA99106/assets/game`, then the older places
`/app0/OutRun2006`, `/mnt/usb0/OutRun2006`, `/mnt/usb1/OutRun2006`,
`/data/OutRun2006` and `/download0/OutRun2006`, or the path written on one
line in `/data/OutRunPS5/retail-root.txt`. The prepared EXE image is
`OR2006C2C.cache` in the game folder; without it, the first launch prepares
it from a readable EXE in the game folder (text screen with the steps) and
saves it there, or in `/data/OutRunPS5` when the game folder is read-only.
Later launches show nothing of this.

## Files the title writes

The user folder is `/data/OutRunPS5` (or `/download0/OutRunPS5` when the
title cannot write there): `options.ini`, `SaveGame/` and the
reports `OutRunPS5.log` and `crash.txt`. The native log is
`/download0/OutRunPS5/runtime.log`. Game data is only read.

## Display settings

Options > Settings adds ASPECT RATIO, RESOLUTION, ANTI-ALIASING and FRAME
RATE rows, saved in `options.ini`. Without a saved value the title starts in
16:9 at 2160p and 60 frames per second; FRAME RATE 120 applies at once on a
120 Hz screen. The game still ticks at 60 Hz: at 120 frames per second every other
frame draws the cars and the camera between the last two ticks
(`src/enhancements/frame_rate.hpp`, after OutRun2006Tweaks). The title opens
VideoOut at 120 Hz when the screen takes it and paces 60 or 120 frames per
second with the flip rate; without 120 Hz the row is not offered. The scanout is the smallest mode holding
the launch 3D resolution. The launcher plays no sound (no `snd0.at9`).

## Controls

- Cross: confirm; Circle: back.
- D-pad: navigate; left stick: steering.
- R2 / L2: analogue accelerator / brake.
- R1 / L1: shift up / down.
- Options: Start / pause.
- Touch pad: performance overlay.
- L1 + R1 + Options: quit.

## Credits

Inspired and using tweaks by [OutRun2006Tweaks](https://github.com/emoose/OutRun2006Tweaks)
by emoose. See `THIRD_PARTY_NOTICES.md` at the repository root.

OutRun is a trademark of Sega. This project is not affiliated with or
endorsed by Sega or Sumo Digital.

## Diagnostic payload

`OutRun2006-PS5.elf` is a separate payload for a compatible ELF loader. It
uses SDL/VideoOut and the reference CPU rasteriser, for engine diagnostics.

### Native OpenGL memory compatibility

On firmware 13.60, the payload SDK's anonymous `mmap` syscall terminates the
native title with `SYSTEM_ILLEGAL_FUNCTION_CALL`. Removing the heap's `mmap`
fallback alone is insufficient: Gallium also uses it for large texture uploads.
`native_memory.c`, linked by `tools/build_ps5_native.sh`, supplies private
read/write anonymous staging allocations from the title's direct-memory heap
and tracks complete unmaps. Pages are aligned to 16 KiB and zero-filled.
Unmaps of the driver's own direct/GPU allocations retain the native libkernel
release path; they must not be treated as staging allocations.
File, shared, executable and fixed mappings, and partial staging unmaps, return errors;
this adapter is not a general POSIX virtual-memory implementation. The console
has passed extraction and reached races with it. It does not change the Vulkan build.

### Native OpenGL performance counters

Frame-wide sample counting is disabled in the console build: the PS5 driver
allocates an extra occlusion buffer per draw while that diagnostic is active.
Desktop pixel tests can still request it. The native GL timer query uses CPU
wall-clock timestamps and drains submissions at both boundaries; it does not
separate GPU execution from CPU preparation. The console overlay therefore
shows GPU timing as unavailable, plus frame, render and swap wall times and the
actual resolution/antialiasing settings. A single log report every 120 frames
breaks down scene recording, composition and swap, with submitted draw counts.
Changes to race performance need hardware measurement after restarting.

## Building

Run `Build-PS5.ps1` (Windows, WSL Ubuntu-26.04; `-InstallDependencies` the
first time). The PS5 code is part of the single code base: `ps5/source` is
the platform layer around the shared runner (`src/runtime/game_host`), like
`mac/source` for macOS and `switch/source` for the Switch. The build then
packs the title into `deliver\ps5\OutRun2006-PS5.pkg`
(`tools/build_ps5_pkg.ps1`, .NET 10 SDK required).

### Vulkan renderer

`Build-PS5.ps1 -Renderer Vulkan -InstallDependencies` installs and builds the
pinned PS5 Mesa/RADV driver, then builds a separate native title. OpenGL remains
the default (`-Renderer OpenGL`). Each title uses one renderer; switching it
currently means building/installing the other title. Both use PPSA99106 and
the same game-data and save locations.

To build only the Vulkan native folder/ZIP, without making a package, run in WSL:

```sh
bash tools/setup_ps5_sdk.sh
bash tools/setup_ps5_vulkan.sh --dependencies
bash tools/build_ps5_vulkan.sh
```

Output: `work/deliver/ps5/OutRun2006-PS5-vulkan-native.zip` and
`work/deliver/ps5/vulkan/PPSA99106`. Build files stay in `work/ps5`; the
driver and toolchains stay in `~/.local/share/outrun-ps5/vulkan`.
`OR2_PS5_BUILD_ROOT`, `OR2_PS5_OUTPUT_ROOT` and `OR2_PS5_TOOL_ROOT` override these
locations. The ZIP contains the homebrew title and no original game data.
The title uses `ps5/sce_sys/icon0.png`, `pic0.dds` and `pic1.dds`, generated from
the shared repository artwork by `tools/make_icons.ps1`. Set
`OR2_PS5_TITLE_SUFFIX=' - Test'` to append that label to the launcher title.

The Vulkan adapter uses GPU rasterization and SPIR-V pixel shaders (fixed
function, PS 1.1 and PS 1.4), D3D texture/mipmap/cube resources, reflection
targets, depth/stencil, blending, occlusion queries, MSAA, FXAA and GPU
composition of UI/video. Indexed geometry uses persistent GPU buffers and
specialized SPIR-V vertex programs derived from the recovered Switch equations;
clipping runs on the GPU. A reusable CPU path handles unsupported declarations
and diagnostics (`OR2_PS5_CPU_VERTEX=1` selects it for comparison). Submission
still waits on one frame at a time. Console performance is being measured.

Retail particle point sprites use the same clip-space quad expansion as the
Metal port, including distance scaling, textures, alpha and depth state.
Vulkan pipelines share a driver cache, checkpointed every 120 displayed frames
and on exit to `/download0/OutRunPS5/vulkan-pipelines.cache`. Recorded pipeline
keys are prewarmed at the next launch. Driver UUID, shader fingerprint and file
checksum invalidate stale/corrupt caches automatically. First encounters can
still compile new materials; `[vk-pipeline]` logs their compilation time.
Host tests only persist a cache when `OR2_PS5_VK_CACHE` names a writable file.

To reduce first-encounter and MSAA pipeline compilation, pass a semicolon-separated
list of local runtime cache files in `OR2_PS5_VK_PROFILE_CACHES` (environment for
`build_ps5_vulkan.sh`, CMake cache variable for host builds). The shader generator
freezes the observed program constants and removes dead code with `spirv-opt`
during the build; it reads only program IDs, not driver binaries or game assets.
Keep profiles/generated catalogues in `work`. Unknown programs link direct
SPIR-V calls to the recovered blob functions, before driver optimization;
this avoids repeatedly inlining the full interpreter dispatch for each slot.
The generator validates the templates and patched call targets with `spirv-val`.
Logs distinguish `prepared=1/1` from `linked=1/1`. `OR2_PS5_VK_LINK_ONLY=1`
forces direct linking for differential host tests, including known programs.
Unchanged uniform uploads and descriptor bindings are reused only within one
command recording; submission/pool reset invalidates their caches.
Uniform descriptors use five dynamic offsets, so changed matrices/constants
reuse a descriptor set when buffers, ranges, textures and samplers match.
Interleaved materials also reuse sets from earlier draws in the same recording;
the cache is cleared at descriptor-pool reset and texture transitions still run
on every lookup. It is bounded to 4096 cached sets per recording.
Vulkan state commands are emitted only when their bindings change; raw command
buffer access invalidates that state cache. Index bounds are cached by buffer
and draw range, invalidated on Unlock/release, and current stream/base bounds
are still checked on every draw. Geometry is retained once per recording epoch.
Native frames submit scene and compositor together at present; explicit game
queries and host frame diagnostics still retire their requested results.
`[ps5-vk-cache]` reports avoided state commands, bounds scans and descriptors.
Native builds keep the CPU/GPU/frame/wait overlay active, but disable the fine
renderer timing zones by default to avoid thousands of clock reads per frame.
Create `/download0/OutRunPS5/profile-renderer.flag` before launch (or set
`OR2_PS5_RENDER_PROFILE`) to enable those development zones. A bounded startup
measurement logs the cost per `steady_clock` read under `[ps5-cpu-profile]`.
Per-draw vertex/command timings sample one native frame per 120-frame report
window (`sample-frames`); frame and overlay totals still measure every frame.
`OR2_PS5_MENU_TRACE=ON` adds menu ownership/resource counters without switching
vertex calculation to the CPU. One bounded menu capture records pose and the
first indexed geometry's clip coordinates and depth/blend/alpha states.

The native driver is statically linked RADV from the public PS5 Mesa fork;
it presents through `VK_KHR_display`/VideoOut, independently of SDL's video
backend. This path requires a native title, rather than the diagnostic ELF
loader. Vulkan 1.1 and the required attachment formats are checked at startup;
unsupported sampler features fail with an explicit error.

Host validation uses the same adapter and a real Vulkan device, with an
offscreen display instead of VideoOut:

```sh
cmake -S ps5/title -B work/ps5/build-vulkan-host -G Ninja \
  -DOR2_PS5_HOST_TEST=ON -DOR2_PS5_VULKAN=ON -DCMAKE_BUILD_TYPE=Release
cmake --build work/ps5/build-vulkan-host -j 4
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \
  ctest --test-dir work/ps5/build-vulkan-host --output-on-failure
```

Set `OR2_EXE` to your supported EXE/cache to include the recovered vertex and PS 1.1/1.4
bytecode tests. The tests compare geometry and pixels against the CPU reference and cover
reflection lifetimes, mip updates, display orientation and antialiasing.
Host checks and a successful native title build do not establish console
compatibility or complete gameplay; hardware testing is still pending.

This source code is be freely available for use, modification, and redistribution, subject to the licenses of the original projects it is based on.

Feel free to reuse any part of this project in your own work. Please give proper credit where due.

If you reuse our modifications, please keep an appropriate reference to this project and its contributors.

This project is developed and maintained in my free time.

If you enjoy the project and would like to support its development, donations are appreciated but entirely optional:

https://ko-fi.com/r4dius
