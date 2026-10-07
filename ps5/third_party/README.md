# Third-party components of the PS5 build

Fetched by the setup scripts at pinned versions (SHA-256 checked); none is
included in this repository except the pad header in `sdl-pad/`.

- [ps5-payload-sdk v0.43](https://github.com/ps5-payload-dev/sdk/releases/tag/v0.43):
  x86-64 toolchain, CRT, libc++ and PS5 import libraries.
- [PacBrew ps5-payload-dev v0.40.2](https://github.com/ps5-payload-dev/pacbrew-repo/releases/tag/v0.40.2):
  static SDL2, FFmpeg, Vorbis and Ogg libraries and their dependencies.
- [SDL PS5](https://github.com/ps5-payload-dev/SDL/tree/release-2.30.x-ps5),
  commit `ee4c47dc0d617b3bc8f35108f9956baf228a1322`: pad ABI header copied
  unchanged into `sdl-pad/`, with its zlib licence.
- [PS5 OpenGL SDK 1.0.0](https://github.com/blackbearreloaded/ps5-opengl/releases/tag/v1.0.0),
  SHA-256 f93643c04c843d56143b00951df1f8042ea7706ae9f19e4e9abf158e1ead77c5:
  GL/EGL, Mesa and PSBC; PS5 integration GPL-3.0-or-later, other licences
  in the SDK.
- [PS5 native app boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate),
  commit `a93e1d677f55a9967de5608cdbe9c5ded6b66e81`, GPL-3.0-or-later: CRT,
  ELF/FSELF converter and libc of the native title.
- [LibProsperoPkg](https://github.com/SvenGDK/LibProsperoPKG), commit
  `748eabf`: package builder of the PS5 build
  (`tools/ps5_pkg`).
- [PS5 Vulkan](https://github.com/mihawk-99/PS5_Vulkan), commit
  `5b5e4fc2d80fdb67a4f61ba9e6a8a424026bb8a5`: RADV build/link recipes and
  native title tooling, GPL-3.0-or-later.
- [PS5 Mesa](https://github.com/mihawk-99/PS5_Mesa), commit
  `7b59ef27c1b09b9671bc4153c41940c3155c3af2`: Mesa 26.2.0 RADV/NIR/ACO with
  the PS5 winsys; licences in `docs/license.rst` and individual sources.
  `tools/patch_ps5_radv.sh` applies this port's fixes from `radv-patches/`
  (VideoOut keeps the 120 Hz mode it accepted) to the driver it builds.
- [PS5 Payload SDK fork](https://github.com/mihawk-99/PS5_PayloadSDK), commit
  `b83202be73e930050e00de0f1b4d9ec46c0391bf`: pinned SDK v0.42 and the PS5
  platform layer required by RADV; GPL-3.0-or-later and component licences.

`tools/setup_ps5_vulkan.sh` checks out these exact revisions outside the source
tree. `tools/build_ps5_vulkan.sh` uses the upstream RADV link recipe, avoiding
a second zlib implementation, and includes the native CRT/runtime and media
compatibility functions. Its delivery includes driver provenance and licences.

`tools/build_ps5_native.sh` adapts the boilerplate to this title: its
identity (PPSA99106), entry point and log, TLS and assert shims, the static
SDL / media / libc++ link group, AGC imports and allocator wrappers.
