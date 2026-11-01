#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT HUP INT TERM
flags='-std=c11 -Wall -Wextra -Werror -Wpedantic'
if [ "${SANITIZE:-0}" = 1 ]; then flags="$flags -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"; fi
${CC:-cc} $flags -I "$base" -I "$base/vendor" "$base/my2k_main_fs.c" "$base/vendor/ff.c" "$base/vendor/ffunicode.c" "$base/tests/test_main_fs.c" -o "$out/test"
"$out/test"
