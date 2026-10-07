#!/bin/bash
# Runs the real switch/source/main.cpp (host_nro) with a scripted pad from a
# fresh save through license creation, the menus, car, transmission and music
# into START and requires mode 16 (GAME). Usage: host_nro_menu_to_game.sh HOST_NRO RETAIL_DIR
set -eu
host=$(realpath "$1");retail=$(realpath "$2")
# DrvFs (the retail tree host) is case-insensitive like the PC; keep the
# mirror on it so paths such as CARS/ resolve to Cars/.
output=$(realpath -m "${OR2_HOST_TEST_OUTPUT:-$PWD/host-menu-evidence}")
mkdir -p "$output"
work=$(mktemp -d -p "$output" menu.XXXXXX)
mkdir -p "$work/game/SaveGame"
for e in "$retail"/*;do
    name=$(basename "$e")
    case "${name,,}" in savegame|options.ini|outrunswitch.log|crash.txt) continue;; esac
    ln -s "$e" "$work/game/$name"
done
cd "$work"
set +e
OR2_HOST_TRACE=1 OR2_HOST_FRAMES=1200 \
OR2_HOST_SCRIPT='120:A,200:A,300:DOWN,302:DOWN,304:DOWN,310:A,500:A,540:A,580:A,620:A,640:A,740:A,780:A,860:A' \
  bash -c 'exec -a "$1" "$2"' _ "$work/game/OutRun2006-NX.nro" "$host" > trace.txt 2>&1
result=$?
set -e
printf 'host exit=%s; private evidence=%s\n' "$result" "$work"
if [ "$result" -ne 0 ] && [ "$result" -ne 3 ]; then exit "$result"; fi
if grep -E 'race end latched:|race end:.* FAULT ' trace.txt; then exit 1; fi
grep -q ' mode=13 ' trace.txt || { echo "START not reached";tail -5 trace.txt;exit 1; }
grep -q ' mode=16 ' trace.txt || { echo "GAME not reached";grep '^\[' trace.txt | tail -3;exit 1; }
echo "host_nro: menu -> START -> GAME reached (lifecycle only, no console validation)"
