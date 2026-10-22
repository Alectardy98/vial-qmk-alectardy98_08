# MM Studio Class0413 EC — stable EC42 v2.8

Promoted without firmware behavior changes from the hardware-tested v2.8 implementation. The production ELF, BIN and UF2 are byte-identical to the corresponding test builds. No hardware is accessed by building.

```sh
make mmstudio/class0413_ec:vial_alec FIRMWARE_FORMAT=bin
make mmstudio/class0413_ec:vial FIRMWARE_FORMAT=bin
```

EC42 v2.8 adds always-active calibrated-span Variable Actuation, global/default and91 per-key overrides, reset-to-global, separate filtered extremum-based Dynamic RT, immediate validated RAM updates and2-second idle/released-key automatic settings persistence. Only packed records are supported; the migration command is removed. Missing, legacy or invalid data creates an empty RAM state and shows Calibration required. GET/boot/connection never save calibration automatically. The existing unlimited,16-scan-qualified partial manual calibration still requires explicit Apply & Save. Disabled keys remain sensed but cannot generate APC/RT HID events. Incremental captures retain other enabled cells and overrides.

**Music Mode remains disabled, exactly as in the hardware-tested v2.8 firmware.** Native Audio, Clicky, lock songs and solenoid remain. Unused music wrappers/table sources remain as reference/equivalence fixtures but are not built. No shared QMK sources changed. Exact time conversions, solenoid startup bounds and flash policy wrappers remain.

Logical EC storage is still219 bytes at37..255; Vial starts256. New method40/41 packs measured bottoms, qualified/enabled/retry flags, per-key inheritance/1–98% overrides plus the Deepest endpoint and global VA/RT settings with CRC32. Runtime remains canonical method3. Only valid packed backups can be restored by this firmware. Old-format backups require the matching earlier firmware/GUI; alternatively recalibrate. Keep both firmware and matching EC/Vial exports. There is no dual persistent bank: CRC/quarantine fails closed after torn records, but QMK's single-sector erase/consolidation retains a power-loss window.

Flash remains128KiB: sector0 stock TinyUF2, sector1 protected, sector2 EEPROM08008000..0800bfff (16KiB backing/4KiB logical), sector3 stock settings, sector4 application08010000..0801ffff. USB identity, Vial UID, EEPROM compatibility ID, ADC/GPIO/timers and keymaps are unchanged. Runtime flash operations remain confined to sector2; BIN/UF2 installation payloads target sector4 only. No option-byte operations or bootloader image.

Native audio PA6/PB12 usesTIM3/TIM4, TIM2 is system timer; haptics PB13/PB14. RT median-of-three uses capture scratch only outside capture, never extra ADC reads; manual qualification is unchanged. Fixed release hysteresis and RT arithmetic limits are enforced for every enabled key. Updates reject held keys, stale revisions, unsafe spans and unhealthy storage. Pending settings reload on USB suspend/disconnect or boot entry, without saving. USB restore/TinyUF2 requests work even if ordinary keys are disabled.

Artifacts: `firmware/mmstudio/class0413_ec/class0413_ec_{vial,vial_alec}.{elf,bin,uf2}`. Detailed format, commands, application usage and hardware checklist: qmk-ec-util `docs/VARIABLE_ACTUATION.md`. The v2.8 implementation passed physical testing before promotion; an optional production-package smoke test is described in the stable release recovery instructions. Automated tests alone do not validate real sensor noise, audio timing or flash persistence.

Offline retained size-equivalence checks:

```sh
python3 keyboards/mmstudio/class0413_ec/tests/verify_size_changes.py
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 python3 keyboards/mmstudio/class0413_ec/tests/verify_size_changes.py
```

The music-table check covers retained source math, not an enabled Music Mode feature. Normal/ASan/UBSan GUI CTest additionally compiles the actual scanner/calibration/protocol against mocked ADC/NOR and checks incremental saves, disabled-key suppression, packed-codec corruption, interrupted writes and RT movement. Keep real hardware tests separate.

Measured final application load spans: `vial_alec`63,792/65,536 bytes (1,744 free), `vial`62,380/65,536 (3,156 free). Padded BINs64,000/62,464 bytes. Initialized RAM1,788/1,784 bytes respectively; ordinary `.bss`18,168 bytes in both. MSP1KiB and process stack2KiB are separate. The default `size` BSS sum includes reserved RAM address-space sections; use actual section values. No remaining headroom is promised for additional features.

## Reproducible production release

The default QMK targets also generate application-only UF2 files:

```sh
make mmstudio/class0413_ec:vial
make mmstudio/class0413_ec:vial_alec
# Maintain named ELF/BIN/UF2/map outputs and verify both images:
sh development/mmstudio/class0413_ec/build.sh all
# Complete offline regression + ASan/UBSan checks (requires adjacent qmk-ec-util):
sh development/mmstudio/class0413_ec/test.sh
```

The matching QMK EC Utility negotiates EC42 v2.8 by capabilities and Vial identity, not the source directory. Existing v2.8 packed calibration, enabled keys, VA/RT settings, Vial keymaps/macros and LK_SONG persistence remain compatible: no recalibration, migration or reset is requested by promotion. Keep the archived test target and stable release fallbacks until the production package has been physically verified. Do not mass erase, change option bytes or overwrite stock TinyUF2.
