#!/usr/bin/env bash
# Applies ps5/third_party/radv-patches to the RADV source that
# tools/setup_ps5_vulkan.sh built, rebuilds the archive and records the
# patches in its PROVENANCE.txt. Patches already applied are skipped.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
base=${OR2_PS5_TOOL_ROOT:-"$HOME/.local/share/outrun-ps5"}
driver="$base/vulkan/PS5_Vulkan"
source_tree="$driver/.deps/work/radv-src"
build="$driver/.deps/work/radv-build-ps5-release"
install="$driver/.deps/native/radv-release"
[[ -f $build/build.ninja && -f $install/PROVENANCE.txt ]] || { echo "Run tools/setup_ps5_vulkan.sh first" >&2;exit 2; }
ninja=${NINJA:-$(command -v ninja || echo "$HOME/.local/bin/ninja")}
export PATH="$driver/.deps/work/radv-clc-bin:$PATH"
changed=0
for patch in "$root"/ps5/third_party/radv-patches/*.patch;do
    if patch -d "$source_tree" -p1 -R --dry-run -s -f < "$patch" > /dev/null 2>&1;then continue;fi
    patch -d "$source_tree" -p1 -s < "$patch"
    printf 'patch: %s\n' "$(basename "$patch")" >> "$install/PROVENANCE.txt"
    changed=1
done
if [[ $changed == 1 ]];then
    "$ninja" -C "$build" src/amd/vulkan/libvulkan_radeon.a > "$build.log" 2>&1 ||
        { grep -E "error|FAILED" "$build.log" | head -20 >&2; exit 1; }
    cp "$build/src/amd/vulkan/libvulkan_radeon.a" "$install/lib/libvulkan_radeon.ps5.a"
    sed -i "s/^archive sha256: .*/archive sha256: $(sha256sum "$install/lib/libvulkan_radeon.ps5.a" | cut -d' ' -f1)/" "$install/PROVENANCE.txt"
fi
printf 'PS5 RADV patches applied: %s\n' "$(grep -c '^patch: ' "$install/PROVENANCE.txt" || true)"
