#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
cd "$repo"
qmk=${MY2K_QMK_BIN:-$repo/development/my2k-100ng-experimental/.venv-qmk/bin/qmk}
out=$repo/development/my2k-100ng-experimental/build/main
if [[ ! -x "$qmk" ]]; then
    echo "Missing repository-local QMK CLI. Create Python 3.11 venv development/my2k-100ng-experimental/.venv-qmk and install qmk==1.2.0 plus requirements.txt." >&2
    exit 1
fi
mkdir -p "$out"
rebuild_args=()
if [[ ${MY2K_REBUILD_ALL:-0} == 1 ]]; then
    rebuild_args=(-B)
fi
for variant in stock_aux replacement_aux release; do
    # Pinned QMK's version generator otherwise embeds wall-clock time. Its
    # supported skip-all mode uses stable placeholders; manifest.json retains
    # the real revision and source hashes. This changes no upstream source.
    make "${rebuild_args[@]}" "matrix/my2k_100ng_exp:$variant" "QMK_BIN=$qmk" SKIP_VERSION=yes "-j${JOBS:-4}" > "$out/$variant.log" 2>&1
    name=matrix_my2k_100ng_exp_$variant
    for ext in elf bin hex map; do
        cp -- ".build/$name.$ext" "$out/$name.$ext"
    done
    arm-none-eabi-size -A ".build/$name.elf" > "$out/$variant.sections.txt"
done
"$(dirname "$qmk")/python" -m pip freeze > "$out/python-requirements.lock"
python3 keyboards/matrix/my2k_100ng_exp/verify-main.py
