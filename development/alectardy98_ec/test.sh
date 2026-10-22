#!/bin/sh
# One current-source harness shared by firmware and GUI tests; no HID access.
set -eu
cd "$(dirname "$0")/../.."
for mode in normal sanitize; do
 enabled=OFF
 if [ "$mode" = sanitize ]; then enabled=ON; fi
 build="keyboards/alectardy98_ec/util/build-current-$mode"
 cmake -S keyboards/alectardy98_ec/util -B "$build" -DBUILD_TESTING=ON -DEC_ENABLE_SANITIZERS="$enabled"
 cmake --build "$build" -j4
 ASAN_OPTIONS=detect_leaks=0 QT_QPA_PLATFORM=offscreen ctest --test-dir "$build" --output-on-failure
done
# Needs the two completed Matrix firmware builds; source-only tests above do not.
python3 development/alectardy98_ec/tests/test_matrix_vial_id.py
python3 development/alectardy98_ec/verify-builds.py "${EC42_BUILD_TREE:-.}"
