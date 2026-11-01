#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
repo=$(CDPATH= cd -- "$base/../../.." && pwd)
tusb=/home/alec/Documents/GitHub/tinyuf2/lib/tinyusb/src
out=$(mktemp -d);trap 'rm -rf "$out"' EXIT HUP INT TERM
flags='-std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -DMY2K_STOCK_PARITY_PROFILE=1'
if [ "${SANITIZE:-0}" = 1 ];then flags="$flags -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer";fi
cc $flags -I "$tusb" -I "$base/include" -I "$base/../common" \
 "$base/src/msc_io.c" "$base/src/msc_backend.c" "$base/src/usb_msc.c" "$base/tests/test_msc_write.c" -Wl,--gc-sections -o "$out/test"
"$out/test"
