#!/usr/bin/env bash
# Build a pinned, public PS5 RADV driver outside the source tree.
set -euo pipefail
base=${OR2_PS5_TOOL_ROOT:-"$HOME/.local/share/outrun-ps5"}
cache="$base/vulkan"
mkdir -p "$cache"
if [[ ${1:-} == --dependencies ]];then
    deps=(clang clang-18 llvm-21-dev libclang-21-dev libclang-cpp21-dev libclc-21 libclc-21-dev
        libllvmspirvlib-21-dev meson ninja-build rsync python3-mako python3-yaml bison flex
        spirv-tools spirv-tools-dev spirv-tools-headers glslang-tools libvulkan-dev vulkan-validationlayers
        libdrm-dev libudev-dev libexpat1-dev zlib1g-dev python3-packaging)
    if [[ $EUID == 0 ]];then apt-get update;apt-get install -y "${deps[@]}";
    else sudo apt-get update;sudo apt-get install -y "${deps[@]}";fi
fi
checkout(){
    local name=$1 revision=$2 path="$cache/$1"
    if [[ ! -d $path/.git ]];then git clone --no-checkout "https://github.com/mihawk-99/$name.git" "$path";fi
    if [[ $(git -C "$path" rev-parse HEAD 2>/dev/null || true) != "$revision" ]];then
        git -C "$path" fetch --depth 1 origin "$revision";git -C "$path" checkout --detach "$revision"
    fi
    test "$(git -C "$path" rev-parse HEAD)" = "$revision"
}
checkout PS5_Vulkan 5b5e4fc2d80fdb67a4f61ba9e6a8a424026bb8a5
checkout PS5_Mesa 7b59ef27c1b09b9671bc4153c41940c3155c3af2
checkout PS5_PayloadSDK b83202be73e930050e00de0f1b4d9ec46c0391bf
# Bound memory use of Mesa's large compiler translation units.
printf '#!/usr/bin/env bash\nexec ninja -j "${OR2_BUILD_JOBS:-4}" "$@"\n' > "$cache/ninja-outrun"
chmod +x "$cache/ninja-outrun"
export NINJA="$cache/ninja-outrun"
bash "$cache/PS5_Vulkan/tools/setup-native-dependencies.sh"
bash "$cache/PS5_Vulkan/tools/build-radv.sh" release
printf 'PS5 RADV ready: %s\n' "$cache/PS5_Vulkan/.deps/native/radv-release"
bash "$(dirname "$0")/patch_ps5_radv.sh"   # fixes of this port over the pinned RADV
