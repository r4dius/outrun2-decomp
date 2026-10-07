#!/bin/bash
# The real switch/source/main.cpp (host_nro) without OR2_EXE: the EXE image comes
# from the first-launch preparation (the decompressed EXE next to the game) and
# then from its cache only. The context's copies of EXE tables at the first race
# frame (OR2_HOST_EXE_STATE) and every frame's draws must match the run with
# OR2_EXE set (where the image is installed before main runs), so nothing built
# before the image is installed can keep EXE tables that were still empty.
# Usage: host_nro_cache_only.sh HOST_NRO RETAIL_DIR   (OR2_EXE: the decompressed EXE)
set -eu
host=$(realpath "$1");retail=$(realpath "$2")
[ -n "${OR2_EXE:-}" ] || { echo "OR2_EXE (the decompressed OR2006C2C.EXE) is required"; exit 1; }
exe=$(realpath "$OR2_EXE")
# DrvFs (the retail tree host) is case-insensitive like the PC; keep the
# mirror on it so paths such as CARS/ resolve to Cars/.
output=$(realpath -m "${OR2_HOST_TEST_OUTPUT:-$PWD/host-cache-evidence}")
mkdir -p "$output"
work=$(mktemp -d -p "$output" cache.XXXXXX)
mkdir -p "$work/game"
for e in "$retail"/*;do
    name=$(basename "$e")
    case "${name,,}" in savegame|options.ini|outrunswitch.log|crash.txt|cache|.outrun-cache|.outrun-nx-cache|or2006c2c.cache) continue;; esac
    ln -s "$e" "$work/game/$name"
done
ln -s "$exe" "$work/game/OR2006C2C_decompressed.exe"
cd "$work"
run(){   # run NAME [env...]: from a fresh save through the menus into the race (host_nro_menu_to_game's pad)
    local name=$1;shift
    rm -rf "$work/game/SaveGame";mkdir -p "$work/game/SaveGame"
    env "$@" OR2_HOST_EXE_STATE=1 OR2_HOST_DRAWHASH=1 OR2_HOST_FIXEDCLOCK=1 OR2_HOST_TRACE=1 OR2_HOST_FRAMES=1300 \
        OR2_HOST_SCRIPT='120:A,200:A,300:DOWN,302:DOWN,304:DOWN,310:A,500:A,540:A,580:A,620:A,640:A,740:A,780:A,860:A' \
        bash -c 'exec -a "$1" "$2"' _ "$work/game/OutRun2006-NX.nro" "$host" > "$name.txt" 2>&1 || true
    grep -a '^drawhash\|^exe-state' "$name.txt" > "$name.hash" || true
    printf '%s: %s frames hashed, %s\n' "$name" "$(grep -c '^drawhash' "$name.hash" || true)" "$(grep '^exe-state' "$name.hash" || echo 'no race')"
}
run with-exe OR2_EXE="$exe"
grep -q '^exe-state' with-exe.hash || { echo "the reference run did not reach the race";tail -5 with-exe.txt;exit 1; }
run first-launch -u OR2_EXE
[ -f "$work/game/OR2006C2C.cache" ] && [ ! -e "$work/game/Cache" ] || { echo "the first launch wrote no OR2006C2C.cache next to the game";tail -5 first-launch.txt;exit 1; }
run cache -u OR2_EXE
for n in first-launch cache;do
    cmp -s with-exe.hash "$n.hash" || { echo "$n differs from the OR2_EXE run (evidence: $work)";diff with-exe.hash "$n.hash" | grep -v '^[<>] drawhash' | head -4;cmp with-exe.hash "$n.hash" | head -1;exit 1; }
done
echo "host_nro: identical EXE tables and draws with OR2_EXE, after the first-launch preparation and from the cache"
rm -rf "$work"
