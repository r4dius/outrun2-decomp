#!/usr/bin/env bash
# Fetches LibProsperoPkg (PS5 package builder, .NET 10) into build-ps5-pkg-tools/,
# pinned to the revision tools/ps5_pkg was written against.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
dir="$root/build-ps5-pkg-tools/LibProsperoPKG"
rev=748eabf
if [[ ! -d $dir/.git ]];then
    mkdir -p "$(dirname "$dir")"
    git clone --quiet https://github.com/SvenGDK/LibProsperoPKG.git "$dir"
fi
git -C "$dir" fetch --quiet origin || true
git -C "$dir" checkout --quiet "$rev"
printf 'LibProsperoPkg %s in %s\n' "$rev" "$dir"
