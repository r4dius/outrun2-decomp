#!/bin/sh
# Local, pinned ARM64 dependencies. No administrator rights or global install.
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
task_cmake=${OR2_CMAKE:-cmake}
task_prefix="$task_root/build/dependencies"
task_sources="$task_root/build/dependency-sources"
task_archives="$task_root/build/downloads"
mkdir -p "$task_archives" "$task_sources"
while IFS='|' read -r task_name task_url task_sha; do
    if [ ! -f "$task_archives/$task_name" ]; then
        curl --fail --location "$task_url" -o "$task_archives/$task_name"
    fi
    printf '%s  %s\n' "$task_sha" "$task_archives/$task_name" | shasum -a 256 -c -
    tar -xf "$task_archives/$task_name" -C "$task_sources"
done < "$task_root/mac/tools/dependencies.lock"
for task_pair in SDL2-2.32.10:sdl2 libogg-1.3.5:ogg libvorbis-1.3.7:vorbis; do
    task_source=${task_pair%:*}
    task_build=${task_pair#*:}
    "$task_cmake" -S "$task_sources/$task_source" -B "$task_root/build/dependency-build/$task_build" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$task_prefix" \
        -DCMAKE_PREFIX_PATH="$task_prefix" -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DBUILD_SHARED_LIBS=OFF -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF
    "$task_cmake" --build "$task_root/build/dependency-build/$task_build" -j 6
    "$task_cmake" --install "$task_root/build/dependency-build/$task_build"
done
(
    cd "$task_sources/pkgconf-2.3.0"
    ./configure --prefix="$task_prefix" --disable-shared --enable-static
    make -j 6
    make install
)
(
    cd "$task_sources/ffmpeg-8.0"
    ./configure --prefix="$task_prefix" --disable-shared --enable-static \
        --disable-programs --disable-doc --disable-everything --disable-autodetect \
        --enable-decoder=bink,binkaudio_dct,binkaudio_rdft,pcm_s16le,vorbis \
        --enable-demuxer=bink,ogg --enable-protocol=file --enable-swscale --enable-swresample \
        --extra-cflags=-mmacosx-version-min=14.0 --extra-ldflags=-mmacosx-version-min=14.0
    make -j 6
    make install
)
printf 'Dependencies installed in %s\n' "$task_prefix"
