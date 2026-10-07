#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
base=${OR2_PS5_TOOL_ROOT:-"$HOME/.local/share/outrun-ps5"}
sdk="$base/ps5-payload-sdk"
gpu="$base/ps5-opengl-sdk-1.0.0/sdk"
template="$base/reference-native"
reference="$base/reference-opengl"
export PS5_PAYLOAD_SDK="$sdk"
build_root=${OR2_PS5_BUILD_ROOT:-"$root"}
archive="$build_root/build-ps5-native-archive"
"$sdk/bin/prospero-cmake" -S "$root/ps5/title" -B "$archive" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DOR2_PS5_OPENGL=ON -DOR2_PS5_NATIVE_ARCHIVE=ON \
    -DPS5OpenGL_DIR="$gpu/lib/cmake/PS5OpenGL"
cmake --build "$archive" -j "${OR2_BUILD_JOBS:-8}"
app="$build_root/build-ps5-native-title"
mkdir -p "$app/src" "$app/vendor" "$app/.deps" "$app/build/native-imports"
cp "$template/Makefile" "$app/Makefile"
for directory in assets runtime sce_sys tooling tools;do
    mkdir -p "$app/$directory";cp -a "$template/$directory/." "$app/$directory/"
done
if [[ ! -d $app/.deps/native ]];then cp -a "$template/.deps/native" "$app/.deps/native";fi
# The title's own tile and backgrounds (tools/make_icons.ps1); no boilerplate selection sound.
cp "$root/ps5/sce_sys/icon0.png" "$root/ps5/sce_sys/pic0.dds" "$root/ps5/sce_sys/pic1.dds" "$app/sce_sys/"
rm -f "$app/sce_sys/snd0.at9" "$app/sce_sys/background-source.png" "$app/sce_sys/launch-background-source.png"
cp "$template/tooling/native/ps5-pie.ld" "$app/tooling/native/ps5-pie-base.ld"
cp "$reference/native-app/ps5-pie.ld" "$reference/native-app/app-symbols.map" "$app/tooling/native/"
# These SDK hooks belong to the payload ELF loader, absent from a native title.
# Resolve optional weak hooks to NULL, preserving their checked fallback paths.
cat >> "$app/tooling/native/ps5-pie.ld" <<'LD'
__dlopen = 0;
__dlsym = 0;
__dladdr = 0;
__dlclose = 0;
__dlerror = 0;
kernel_mprotect = 0;
LD
cp "$base/ps5-opengl-sdk-1.0.0/native-app/app_heap.c" "$app/src/app_heap.c"
# The game's heap: 1 GiB of direct memory (flexible memory is too small for the
# game, the GPU pool and the thread stacks together).
printf '#include <stddef.h>\nconst size_t ps5_opengl_heap_size = 1024u * 1024u * 1024u;\n' > "$app/src/app_heap_size.c"
cp "$root/ps5/source/native_entry.cpp" "$app/src/main.cpp"
cp "$root/ps5/source/native_memory.c" "$app/src/native_memory.c"
python3 - "$reference" "$app" <<'PY'
import json,sys
from pathlib import Path
reference,app=map(Path,sys.argv[1:])
# Keep upstream's compatibility shims; normal CRT exit and our own log remain active.
shims=(reference/'native-app/runtime_shims.c').read_text()
begin=shims.index('void ps5_opengl_glapi_tls_context_init')
end=shims.index('int mkstemps')
(app/'src/runtime_shims.c').write_text(shims[:shims.index('__attribute__((constructor))')]+shims[begin:end])
param=json.loads((reference/'native-app/param.json').read_text())
param.update(titleId='PPSA99106',conceptId='99106',contentId='UP9000-PPSA99106_00-OUTRUNPS5HB00001')
param['localizedParameters']['en-US']['titleName']='OutRun 2006 Coast 2 Coast'
(app/'sce_sys/param.json').write_text(json.dumps(param,indent=2)+'\n')
# mmap(MAP_ANON) kills a title on firmware 13.60 (SYSTEM_ILLEGAL_FUNCTION_CALL):
# the heap always comes from direct memory, never from the mmap fallback.
heap=app/'src/app_heap.c'
code=heap.read_text()
for old,new in (('  if (ps5_opengl_heap_size > (128u << 20)) {','  {'),
                ('  for (size_t size = ps5_opengl_heap_size; base == MAP_FAILED && size >= (128u << 20);\n       size /= 2) {\n    base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);\n    if (base != MAP_FAILED)\n      ps5_heap_size = size;\n  }\n','')):
    assert code.count(old)==1,old
    code=code.replace(old,new)
code += '\nint or2_ps5_native_heap_contains(const void *address) { return ps5_heap_owns(address); }\n'
heap.write_text(code)
writer=app/'tooling/native/sce_module_writer.cpp'
text=writer.read_text()
marker='write_u64(result.data, result.heap_size, std::numeric_limits<std::uint64_t>::max());'
assert text.count(marker)==1
writer.write_text(text.replace(marker,'write_u64(result.data, result.heap_size, 0x10000000ULL);'))
PY
app_sdk="$app/.deps/native/ps5-payload-sdk"
for name in agc_link_stub agc_driver_link_stub;do
    PS5_PAYLOAD_SDK="$app_sdk" sh "$app/tooling/prospero-clang18" -std=c11 -O2 -fPIC \
        -c "$base/ps5-opengl-sdk-1.0.0/native-app/$name.c" -o "$app/build/native-imports/$name.o"
done
"$app_sdk/bin/prospero-lld" --shared -soname libSceAgc.prx -o "$app_sdk/target/lib/libSceAgc.so" "$app/build/native-imports/agc_link_stub.o"
"$app_sdk/bin/prospero-lld" --shared -soname libSceAgcDriver.prx -o "$app_sdk/target/lib/libSceAgcDriver.so" "$app/build/native-imports/agc_driver_link_stub.o"
cp "$sdk/target/lib/libSceVideoOut.so" "$app_sdk/target/lib/libSceVideoOut.so"
group="$app/vendor/liboutrun_group.a"
{
    printf 'SEARCH_DIR("%s")\nSEARCH_DIR("%s")\nEXTERN(ps5_agc_gate2_run)\nGROUP (\n' "$sdk/target/lib" "$gpu/lib"
    printf '  "%s"\n' "$archive/libOutRun2006-PS5.a" "$archive/liboutrun_ps5_engine.a" "$gpu/lib/libPS5OpenGL.a"
    for library in SDL2 avformat avcodec swscale swresample avutil vorbisfile vorbis ogg z bz2 lzma iconv x264 ssl crypto samplerate;do
        printf '  "%s"\n' "$sdk/target/user/homebrew/lib/lib$library.a"
    done
    printf '  "%s"\n' "$sdk/target/lib/libunwind.a" "$sdk/target/lib/libc++abi.a" "$sdk/target/lib/libc++.a" "$sdk/target/lib/libc.a" "$(clang-18 --print-resource-dir)/lib/linux/libclang_rt.builtins-x86_64.a"
    printf ')\n'
} > "$group"
export APP_STATIC_ARCHIVES=vendor/liboutrun_group.a
export APP_WRAP_SYMBOLS='malloc calloc realloc free posix_memalign malloc_usable_size mmap munmap'
# No copyrighted game assets are distributed in the homebrew title.
export APP_ASSETS=''
export USE_CCACHE=0
(cd "$app" && bash tools/build.sh Folder)
out=${OR2_PS5_OUTPUT_ROOT:-"$root/deliver/ps5"}
mkdir -p "$out/native"
rm -rf "$out/native/PPSA99106"
cp -a "$app/dist/PPSA99106" "$out/native/"
cp "$app/dist/PPSA99106.zip" "$out/OutRun2006-PS5-native.zip"
cp "$root/ps5/README.md" "$out/README.md"
cp "$root/ps5/third_party/README.md" "$out/THIRD_PARTY.md"
mkdir -p "$out/licenses/ps5-opengl" "$out/licenses/native-app" "$out/licenses/sdl-pad"
cp -a "$base/ps5-opengl-sdk-1.0.0/LICENSES/." "$out/licenses/ps5-opengl/"
cp "$base/ps5-opengl-sdk-1.0.0/LICENSE" "$base/ps5-opengl-sdk-1.0.0/THIRD_PARTY_NOTICES.md" "$out/licenses/ps5-opengl/"
cp "$template/LICENSE" "$out/licenses/native-app/LICENSE"
cp -a "$root/ps5/third_party/sdl-pad/." "$out/licenses/sdl-pad/"
(cd "$out" && sha256sum OutRun2006-PS5-native.zip native/PPSA99106/eboot.bin > SHA256SUMS
 if [[ -f OutRun2006-PS5.elf ]];then sha256sum OutRun2006-PS5.elf >> SHA256SUMS;fi)
printf 'Native PS5 title: %s\n' "$out/OutRun2006-PS5-native.zip"
