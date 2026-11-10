#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then echo "Usage: $0 cartridge.lnx" >&2; exit 2; fi
rom=$(realpath "$1")
cd "$(dirname "$0")/.."
A=build/lynx-039-crc-test
mkdir -p "$A"
cp tests/native-crc.inc "$A/native-crc.inc"
P=firmware
common=(-O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie)
for mode in 2; do
 O="$A/math-$mode";mkdir -p "$O"
 gcc -std=c11 "${common[@]}" -DR8_LYNX_CRC_TABLE=1 -DLCRC_MODE=2 -I"$P" -c "$P/lynx_crc.c" -o "$O/table.o"
 gcc -std=c11 "${common[@]}" -DR8_LYNX_CRC_MERGE=1 -DLCM_MODE="$mode" -I"$P" -c "$P/lynx_crcmerge.c" -o "$O/merge.o"
 g++ -std=c++17 "${common[@]}" -no-pie -DR8_LYNX_CRC_MERGE=1 -DR8_LYNX_CRC_TABLE=1 -I"$P" -I"$A" tests/lynx-crcmerge-math-20261105.cpp "$O/table.o" "$O/merge.o" -o "$O/math"
 ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$O/math" "$rom"
done
