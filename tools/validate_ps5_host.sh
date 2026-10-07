#!/usr/bin/env bash
# The actual PS5 application against desktop SDL. No writes to retail assets.
set -euo pipefail
app=$(realpath "$1")
retail=$(realpath "$2")
output=$(realpath -m "$3")
mkdir -p "$output"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
export OR2_HOST_WIDE=1 OR2_HOST_AREA=1 OR2_HOST_RACE=1 OR2_HOST_FRAMES=${OR2_HOST_FRAMES:-1600}
export OR2_HOST_HOLD=${OR2_HOST_HOLD:-950-9000:ZR}
export OR2_HOST_SCRIPT=${OR2_HOST_SCRIPT:-120:A,200:A,280:A,360:A,440:A,520:A,600:A,680:A,760:A,900:Y}
# Each skipped frame still runs the real scene callbacks and state changes.
# This optimization is absent from the console ELF.
export OR2_PS5_RENDER_EVERY=${OR2_PS5_RENDER_EVERY:-300}
# The EXE-owned tables come from the player's decompressed EXE (OR2_EXE) or a
# decompressed EXE / exe_image.bin cache in the data folder.
"$app" "--data=$retail" "--home=$output" > "$output/trace.log" 2>&1
grep -q "runtime rc=0" "$output/trace.log" || { echo "PS5 app: runtime failed";tail -20 "$output/trace.log";exit 1; }
grep -q "course_runtime=1" "$output/trace.log" || { echo "PS5 app: race course not reached";tail -20 "$output/trace.log";exit 1; }
grep -q "PS5 renderer .*failed=0" "$output/trace.log"
grep -Eq "PS5 raster: presented=[1-9][0-9]* .*pixels=[1-9][0-9]* .*errors=0" "$output/trace.log"
grep -Eq "license persistence saves=[0-9]+ deletes=0 error=0" "$output/trace.log"
echo "PS5 SDL application: native menu -> START -> race course, audio and sampled raster frames passed"
