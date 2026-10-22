# EC42 Utility — canonical universal GUI

This is the maintained application source for the Alectardy98 framework.
The original external qmk-ec-util repository/executable are preserved fallbacks.
All three current boards use canonical EC42 v2.9 and the same explicit per-key
record codec. Live admission uses Vial identity, HELLO and the storage descriptor;
new canonical boards reuse the codec; add a profile/layout for preset visualization.
Only v2.9 firmware is supported; unsupported versions are rejected before writes.
Close Vial before connecting: both use the same Raw HID FF60/61 interface.

## Build from the QMK root

Dependencies: Qt6 >=6.4 Core/Gui/Widgets/Test, C++17 compiler, CMake >=3.21,
Ninja or Make, pkg-config, HIDAPI hidraw and liblzma/xz. Python3 plus the
containing QMK sources/toolchain are needed for firmware integration tests.
The app and firmware share ../common/interfaces/ec42_protocol.h; retain that
header when extracting the GUI from the framework.

```sh
cmake -S keyboards/alectardy98_ec/util -B keyboards/alectardy98_ec/util/build-universal -G Ninja -DBUILD_TESTING=ON
cmake --build keyboards/alectardy98_ec/util/build-universal -j4
keyboards/alectardy98_ec/util/build-universal/qmk-ec-util
QT_QPA_PLATFORM=offscreen ctest --test-dir keyboards/alectardy98_ec/util/build-universal --output-on-failure
cmake -S keyboards/alectardy98_ec/util -B keyboards/alectardy98_ec/util/build-universal-sanitize -G Ninja -DEC_ENABLE_SANITIZERS=ON -DBUILD_TESTING=ON
cmake --build keyboards/alectardy98_ec/util/build-universal-sanitize -j4
ASAN_OPTIONS=detect_leaks=0 QT_QPA_PLATFORM=offscreen ctest --test-dir keyboards/alectardy98_ec/util/build-universal-sanitize --output-on-failure
```

The executable dynamically links system Qt/HIDAPI/liblzma; do not copy it alone
to a machine without those dependencies. Sources/resources/licenses/tests are
included; build caches and executables are ignored by Git. BUILD_TESTING=OFF
builds just the application. Actual-firmware integration tests require the QMK
checkout; EC_FIRMWARE_DIR and EC_FRAMEWORK_DIR can select another matching checkout.

## Use

Connect with all keys released. Confirm the board name, EC42 version, dimensions
and storage READY. No typing/calibration is needed for discovery. If discovery
fails, close other HID clients and inspect permissions/logs; do not reset EEPROM.
Missing/corrupt records show Calibration required with inactive keys.

Start Manual Calibration -> press/hold/release one or more keys -> Finish ->
Review -> Apply & Save. No timed or automatic bottoming calibration exists.
Blue NEW is firmware-qualified unsaved capture; Blue Saved is retained enabled
calibration; Gray is uncalibrated/inactive; Red Retry needs another capture;
dark gray is an impossible position. Later sessions preserve untouched valid keys.
The firmware uses 16-scan sustained qualification and at least 100 ADC counts of
travel. Sparse live USB readings cannot establish that every analog sample is clean.
Keep the automatic disk backup and explicit EC/Vial exports before changes.

Variable Actuation controls initial activation, globally or per key. Override0
inherits; endpoint99 is Deepest. Dynamic RT separately controls movement-based
release/reactivation after initial activation. Controls immediately update RAM;
firmware saves after two seconds of released-key idle. Applied/Saving/Saved/Error
are distinct. Verify power-cycle persistence separately from live readback.
No GUI calibration write occurs merely from connecting or viewing telemetry.

`--offline`, `--board class0413_ec|ec_prox|me_ec_uf2` and `--smoke-test` disable USB.
`--layout path` selects a runtime layout; live devices load their Vial definition.
Build/test smoke commands do not flash or connect to physical keyboards.

## Source map and protocol

See [the framework reference](../README.md) for complete wire structures,
records, storage boundaries, porting instructions, hardware and troubleshooting.
`src/boardprofile.cpp` is the board registry; `device.cpp` handles HID and worker
operations; `protocol.cpp` packets; `calibration.cpp` record codecs; `settings.cpp`
validation; `layout.cpp` layouts; `livepanel.cpp` workflows and controls;
`widgets.cpp` keyboard/graph; `mainwindow.cpp` pages; `telemetry.cpp` data/model.

[Protocol](docs/EC42.md), [VA/RT](docs/VARIABLE_ACTUATION.md),
[hardware checklist](docs/FIRST_HARDWARE_TEST.md), [import provenance](docs/IMPORT.md).
`docs/PROVENANCE.json` retains original Cap-Util lineage. GPL and attribution
notices remain in COPYING/NOTICE.md. tests/fixtures includes historical inputs
needed by regression tests; do not remove them as obsolete application assets.

## 0413 upgrade safety

The new 0413 uses 332-byte canonical records and requires recalibration. Vial data
addresses and compatibility ID are unchanged, but113 bytes of macro capacity
are reserved for the record extension:1691 macro bytes remain. Export Vial/EC
backups before flashing and shorten long macros using the old firmware first.
A macro buffer crossing the new boundary blocks EC saves rather than being
silently truncated. No startup EC save/erase occurs. For rollback restore the
matching v2.8 EC backup AND re-upload the Vial macro export; its old end-of-buffer
sentinel may otherwise contain canonical record bytes. See framework README.
