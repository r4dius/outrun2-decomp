#!/usr/bin/env bash
# Pinned upstream homebrew SDK + PacBrew ports. Run inside Ubuntu WSL.
set -euo pipefail
base=${OR2_PS5_TOOL_ROOT:-"$HOME/.local/share/outrun-ps5"}
mkdir -p "$base/downloads" "$base/ps5-payload-sdk"
if [[ ${1:-} == --dependencies ]]; then
    deps=(build-essential clang lld llvm-dev cmake ninja-build unzip curl pkg-config libsdl2-dev libvorbis-dev libavformat-dev libavcodec-dev libswscale-dev libswresample-dev libavutil-dev)
    if [[ $EUID == 0 ]];then apt-get update;apt-get install -y "${deps[@]}";
    else sudo apt-get update;sudo apt-get install -y "${deps[@]}";fi
fi
fetch(){
    local name=$1 sha=$2 url=$3
    if [[ ! -f "$base/downloads/$name" ]];then curl -fL --retry 3 "$url" -o "$base/downloads/$name";fi
    printf '%s  %s\n' "$sha" "$base/downloads/$name" | sha256sum -c -
}
fetch ps5-payload-sdk-v0.43.zip a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb \
    https://github.com/ps5-payload-dev/sdk/releases/download/v0.43/ps5-payload-sdk.zip
fetch ps5-payload-dev-v0.40.2.tar.gz a85f65de418a8e6a898c6c3e3c870d50fff7618a200e4dd59ea9692af6ecec4d \
    https://github.com/ps5-payload-dev/pacbrew-repo/releases/download/v0.40.2/ps5-payload-dev.tar.gz
sdk="$base/ps5-payload-sdk"
unzip -oq "$base/downloads/ps5-payload-sdk-v0.43.zip" -d "$base"
tar -xzf "$base/downloads/ps5-payload-dev-v0.40.2.tar.gz" --strip-components=2 -C "$sdk"
# Restore the newer SDK wrappers/CRT; retain PacBrew static ports in homebrew/.
unzip -oq "$base/downloads/ps5-payload-sdk-v0.43.zip" -d "$base"
test -x "$sdk/bin/prospero-clang++"
test -f "$sdk/target/user/homebrew/lib/libSDL2.a"
"$sdk/bin/prospero-clang++" --version
printf 'PS5_PAYLOAD_SDK=%s\n' "$sdk"
