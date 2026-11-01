#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=$(mktemp -d);trap 'rm -rf "$out"' EXIT HUP INT TERM
flags='-std=c11 -Wall -Wextra -Werror -Wpedantic -DMY2K_STOCK_PARITY_PROFILE=1'
if [ "${SANITIZE:-0}" = 1 ];then flags="$flags -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer";fi
cc $flags -I "$base" -I "$base/vendor" "$base/my2k_main_fs.c" "$base/vendor/ff.c" "$base/vendor/ffunicode.c" "$base/tests/test_stock_write.c" -o "$out/test"
"$out/test"
