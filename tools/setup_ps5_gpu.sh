#!/usr/bin/env bash
set -euo pipefail
base=${OR2_PS5_TOOL_ROOT:-"$HOME/.local/share/outrun-ps5"}
mkdir -p "$base/downloads"
archive="$base/downloads/ps5-opengl-sdk-1.0.0.tar.gz"
if [[ ! -f $archive ]];then
    curl -fL --retry 3 https://github.com/blackbearreloaded/ps5-opengl/releases/download/v1.0.0/ps5-opengl-sdk-1.0.0.tar.gz -o "$archive"
fi
printf '%s  %s\n' f93643c04c843d56143b00951df1f8042ea7706ae9f19e4e9abf158e1ead77c5 "$archive" | sha256sum -c -
if [[ ! -f $base/ps5-opengl-sdk-1.0.0/sdk/lib/libPS5OpenGL.a ]];then tar -xzf "$archive" -C "$base";fi
checkout(){
    local path=$1 url=$2 revision=$3
    if [[ ! -d $path/.git ]];then git clone --no-checkout "$url" "$path";fi
    if [[ $(git -C "$path" rev-parse HEAD 2>/dev/null || true) != "$revision" ]];then
        git -C "$path" fetch --depth 1 origin "$revision"
        git -C "$path" checkout --detach "$revision"
    fi
    test "$(git -C "$path" rev-parse HEAD)" = "$revision"
}
checkout "$base/reference-native" https://github.com/blackbearreloaded/ps5-native-app-boilerplate a93e1d677f55a9967de5608cdbe9c5ded6b66e81
checkout "$base/reference-opengl" https://github.com/blackbearreloaded/ps5-opengl dce74910af7f4274c48d05e0ae001b3038462588
bash "$base/reference-native/tools/setup-native-dependencies.sh"
printf 'PS5 OpenGL SDK and native title tools ready; console execution is unverified.\n'
