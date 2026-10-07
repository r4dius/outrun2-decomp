#!/usr/bin/env bash
# Separate native Vulkan title; the OpenGL build and Switch/Mac sources stay intact.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
base=${OR2_PS5_TOOL_ROOT:-"$HOME/.local/share/outrun-ps5"}
driver="$base/vulkan/PS5_Vulkan"
sdk="$driver/.deps/native/ps5-payload-sdk"
radv="$driver/.deps/native/radv-release"
media="$base/ps5-payload-sdk/target/user/homebrew"
build_root=${OR2_PS5_BUILD_ROOT:-"$root/work/ps5"}
out=${OR2_PS5_OUTPUT_ROOT:-"$root/work/deliver/ps5"}
archive="$build_root/build-ps5-vulkan-archive"
app="$build_root/build-ps5-vulkan-title"
for file in "$radv/lib/libvulkan_radeon.ps5.a" "$sdk/target/lib/libps5platform.a" "$media/lib/libSDL2.a";do
    [[ -f $file ]] || { echo "Missing $file; run setup_ps5_sdk.sh and setup_ps5_vulkan.sh" >&2;exit 2; }
done
# The pinned RADV SDK supplies the platform/CRT; PacBrew supplies SDL/media.
export PS5_PAYLOAD_SDK="$sdk"
"$sdk/bin/prospero-cmake" -S "$root/ps5/title" -B "$archive" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DOR2_PS5_OPENGL=OFF -DOR2_PS5_VULKAN=ON \
    -DOR2_PS5_NATIVE_ARCHIVE=ON -DOR2_PS5_RADV_ROOT="$radv" -DOR2_PS5_MEDIA_ROOT="$media" \
    -DOR2_PS5_VK_PROFILE_CACHES="${OR2_PS5_VK_PROFILE_CACHES:-}" \
    -DOR2_PS5_MENU_TRACE="${OR2_PS5_MENU_TRACE:-OFF}"
cmake --build "$archive" -j "${OR2_BUILD_JOBS:-4}"
mkdir -p "$app/obj" "$app/stubs" "$app/tooling/native" "$app/native/PPSA99106/sce_sys" "$app/native/PPSA99106/sce_module"
cp "$driver/tooling/native/ps5-pie.ld" "$app/tooling/native/ps5-pie.ld"
cp "$driver/tooling/psbc/ps5-pie-unwind.ld" "$app/tooling/native/ps5-pie-unwind.ld"
# Optional ELF-payload loader hooks have checked fallbacks in native titles.
cat >> "$app/tooling/native/ps5-pie-unwind.ld" <<'LD'
__dlopen = 0; __dlsym = 0; __dladdr = 0; __dlclose = 0; __dlerror = 0; kernel_mprotect = 0;
LD
cc(){ PS5_PAYLOAD_SDK="$sdk" sh "$driver/tooling/prospero-clang18" "$@"; }
cc -std=c++20 -O2 -DOR2_PS5_VULKAN=1 -ffunction-sections -fdata-sections -c "$root/ps5/source/native_entry.cpp" -o "$app/obj/main.o"
cc -std=c++20 -O2 -fno-exceptions -fno-rtti -c "$driver/tooling/native/app_crt.cpp" -o "$app/obj/app_crt.o"
cc -std=c++20 -O2 -fno-exceptions -fno-rtti -c "$driver/tooling/native/app_cpp_runtime.cpp" -o "$app/obj/app_cpp_runtime.o"
cc -std=c11 -O2 -c "$root/ps5/source/vk_native_media.c" -o "$app/obj/media.o"
for spec in 'libSceAgc agc_canary_link_stub' 'libSceAgcDriver agc_driver_canary_link_stub';do
    read -r library source <<< "$spec"
    cc -std=c11 -O2 -fPIC -c "$driver/vendor/ps5/sdk/stubs/$source.c" -o "$app/obj/$library.o"
    "$sdk/bin/prospero-lld" --shared -soname "$library.prx" -o "$app/stubs/$library.so" "$app/obj/$library.o"
done
source "$driver/tools/radv-link.sh"
radv_link_recipe "$driver" "$sdk" "$radv/lib/libvulkan_radeon.ps5.a"
radv_link_flags+=(--wrap=sceVideoOutSubmitFlip)
radv_link_flags+=(--defsym=_setjmp=or2_ps5_setjmp --defsym=_longjmp=or2_ps5_longjmp
    --defsym=___mb_cur_max=or2_ps5_mb_cur_max --defsym=sendmmsg=or2_ps5_sendmmsg --defsym=recvmmsg=or2_ps5_recvmmsg)
# The converter refuses title exports, including these compatibility aliases.
printf '{ local: _setjmp; _longjmp; ___mb_cur_max; sendmmsg; recvmmsg; };\n' > "$app/media-local.map"
radv_link_flags+=(--version-script "$app/media-local.map")
libraries=()
# RADV's whole archive already contains its pinned zlib implementation.
for library in SDL2 avformat avcodec swscale swresample avutil vorbisfile vorbis ogg bz2 lzma iconv x264 ssl crypto samplerate;do
    libraries+=("$media/lib/lib$library.a")
done
"$sdk/bin/prospero-lld" -T "$app/tooling/native/ps5-pie-unwind.ld" -L "$app/tooling/native" --eh-frame-hdr \
    "${radv_link_flags[@]}" --version-script "$driver/tooling/native/app-symbols.map" --exclude-libs=ALL \
    -e _start -o "$app/llvm-pie.elf" "$app/obj/app_crt.o" "$app/obj/app_cpp_runtime.o" "$app/obj/main.o" "$app/obj/media.o" \
    "$app/stubs/libSceAgc.so" "$app/stubs/libSceAgcDriver.so" \
    --start-group "$archive/libOutRun2006-PS5.a" "$archive/liboutrun_ps5_engine.a" "${libraries[@]}" --end-group \
    "${radv_link_inputs[@]}" --as-needed "$sdk"/target/lib/*.so
# Build the public ELF/FSELF converter with the pinned native-title metadata.
tool="$app/ps5-native-tool"
native="$driver/tooling/native"
clang++ -std=c++20 -O2 -I "$driver/.deps/native/zlib/root/usr/include" \
    "$native/native_app_builder.cpp" "$native/self_container.cpp" "$native/elf_object.cpp" "$native/sce_module_writer.cpp" \
    "$driver/.deps/native/zlib/root/usr/lib/libz.a" -o "$tool"
"$tool" link --in "$app/llvm-pie.elf" --out "$app/eboot.elf" --stub-dir "$sdk/target/lib" \
    --stub "$app/stubs/libSceAgc.so" --stub "$app/stubs/libSceAgcDriver.so" \
    --module-sdk 0x02000009 --companion-sdk 0x08050001 --file-name eboot.elf
title="$app/native/PPSA99106"
"$tool" self --sign --in "$app/eboot.elf" --out "$title/eboot.bin" --magic 0x1D3D154F
bash "$driver/tools/rebuild-libc.sh"
cp "$driver/runtime/libc.prx" "$title/sce_module/"
python3 - "$driver/sce_sys/param-radv.json" "$title/sce_sys/param.json" <<'PY'
import json,os,sys
param=json.load(open(sys.argv[1]))
param.update(titleId='PPSA99106',conceptId='99106',contentId='UP9000-PPSA99106_00-OUTRUNPS5HB00001')
# 120 Hz output (FRAME RATE 120): 0x40 asks VideoOut for its high-frame-rate
# mode, 0x40000 keeps the system's VRR as retail 120 Hz titles do.
param['attribute3']=param.get('attribute3',0)|0x40|0x40000
param['localizedParameters']['en-US']['titleName']='OutRun 2006 Coast 2 Coast'+os.environ.get('OR2_PS5_TITLE_SUFFIX','')
with open(sys.argv[2],'w') as f:json.dump(param,f,indent=2);f.write('\n')
PY
# No snd0.at9: the driver's sample sound is not ours, so the launcher stays silent.
rm -f "$title/sce_sys/snd0.at9"
for asset in icon0.png pic0.dds pic1.dds;do
    if [[ -f $root/ps5/sce_sys/$asset ]];then
        cp "$root/ps5/sce_sys/$asset" "$title/sce_sys/"
    else
        echo "Missing PS5 title artwork: $root/ps5/sce_sys/$asset (run tools/make_icons.ps1)" >&2;exit 2
    fi
done
"$tool" self --inspect --file "$title/eboot.bin" > "$app/inspect.txt"
"$tool" self --inspect --file "$title/sce_module/libc.prx" >> "$app/inspect.txt"
mkdir -p "$out/vulkan/licenses"
cp -a "$title" "$out/vulkan/"
cp "$root/ps5/README.md" "$out/vulkan/"
cp "$driver/LICENSE" "$out/vulkan/licenses/PS5_Vulkan-LICENSE"
cp "$base/vulkan/PS5_Mesa/docs/license.rst" "$out/vulkan/licenses/Mesa-license.rst"
cp "$base/vulkan/PS5_PayloadSDK/LICENSE" "$out/vulkan/licenses/PS5-platform-LICENSE"
cp "$root/ps5/third_party/README.md" "$out/vulkan/THIRD_PARTY.md"
# Licences accompany the binary inside the distributable.
cp -a "$out/vulkan/licenses" "$out/vulkan/PPSA99106/"
rm -f "$out/vulkan/PPSA99106/README.md" "$out/vulkan/PPSA99106/THIRD_PARTY.md"   # the .md files stay at the release root
python3 - "$out/vulkan/PPSA99106" "$out/OutRun2006-PS5-vulkan-native.zip" <<'PY'
import shutil,sys
shutil.make_archive(sys.argv[2][:-4],'zip',root_dir=sys.argv[1]+'/..',base_dir='PPSA99106')
PY
(cd "$out/vulkan" && sha256sum ../OutRun2006-PS5-vulkan-native.zip PPSA99106/eboot.bin PPSA99106/sce_module/libc.prx > SHA256SUMS)
printf 'PS5 Vulkan native title: %s\n' "$out/OutRun2006-PS5-vulkan-native.zip"
