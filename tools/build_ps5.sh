#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
sdk=${PS5_PAYLOAD_SDK:-"${OR2_PS5_TOOL_ROOT:-$HOME/.local/share/outrun-ps5}/ps5-payload-sdk"}
test -x "$sdk/bin/prospero-cmake" || { printf 'Install the SDK with tools/setup_ps5_sdk.sh\n' >&2;exit 1; }
export PS5_PAYLOAD_SDK="$sdk"
build="${OR2_PS5_BUILD_ROOT:-$root}/build-ps5-payload"
"$sdk/bin/prospero-cmake" -S "$root/ps5/title" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_VERBOSE_MAKEFILE=OFF
cmake --build "$build" -j "${OR2_BUILD_JOBS:-8}"
out=${OR2_PS5_OUTPUT_ROOT:-"$root/deliver/ps5"}
mkdir -p "$out"
cp "$build/OutRun2006-PS5.elf" "$out/OutRun2006-PS5.elf"
# Strip debug info only. The payload loader still needs its dynamic metadata.
llvm-strip --strip-debug "$out/OutRun2006-PS5.elf"
cp "$root/ps5/README.md" "$out/README.md"
cp "$root/ps5/third_party/README.md" "$out/THIRD_PARTY.md"
(cd "$out" && sha256sum OutRun2006-PS5.elf > SHA256SUMS)
printf 'PS5 ELF: %s\n' "$out/OutRun2006-PS5.elf"
