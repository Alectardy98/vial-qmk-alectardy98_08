# Alectardy98 EC Pro X ANSI/ISO

Hardware acquisition, ROM DFU boot method and split QMK linker are retained from
Cipulot. Shared EC42 v2.9 provides incremental manual calibration, calibrated-key
suppression, global/per-key Variable Actuation, Deepest, Dynamic RT and telemetry.

## Clean installation from a single BIN

Build and package in one command: `make alectardy98_ec/ec_prox:vial_alec`.
Use `firmware/alectardy98_ec/ec_prox/vial_alec/ec_prox.clean-install.bin` at
**0x08000000**, only after intentionally erasing old user flash/settings. This
carrier fills the EEPROM gap with FF; do not substitute `.bin.analysis-only`,
whose objcopy gap contains zeros. It is not a settings-preserving update image.

Before erase, establish independent BOOT0/STM32 ROM DFU or SWD recovery, confirm
MCU/density and read protection settings without changing option bytes. Stop if
protection prevents erasing/writing the selected sectors. Keep the keyboard in
programming mode until the complete BIN verifies; release BOOT0 and reset after
verification. The stock deployment has no resident user-flash bootloader to
preserve: STM32 ROM DFU remains in system memory. This procedure is Prox-only,
never for Matrix or 0413. Full erase removes all user calibration/keymaps/macros.

The logical linker remains vectors at08000000, EEPROM sector1 at08004000..7FFF,
code at08008000. Original QMK vendor EEPROM selection uses embedded_flash wear
leveling: **8192-byte backing journal, 4096 logical bytes**; the erase unit is the
entire16KiB sector1. The upper8KiB is not another storage allocation. Bounds,
geometry, WRP, checksums and error latching remain enforced. Missing calibration
starts disabled in RAM; Vial initializes defaults; calibration requires explicit
manual capture/save. No legacy migration or automatic bottoming calibration.

For later updates preserving settings, use the separate vectors/application BINs
with the original selective erase procedure in ../TESTING.md. No linker redesign
is necessary to offer a single clean-install BIN.

## Storage diagnostics and limits

Identity: `A98-prox-CLEAN-2.9`. The board-only console adapter reports after5s and
repeats while unhealthy. Run `/usr/bin/hid_listen` before connecting. Fault codes:
1 geometry/protection;2 backend;3 journal init;4 unlock;5 controller start;
6 program;7 erase;8 erase completion. P/E counts are attempts. No guard is bypassed.

The clean-install firmware has now passed physical EC Prox testing, including
manual calibration and persistent settings. The canonical `vial_alec` release
reproduced that tested ELF and BIN byte-for-byte before the approved lighting
cleanup. The owner reports the current RGBLIGHT-only implementation working.
Future lighting changes require a physical layer-indicator retest. Earlier storage NOT READY's
precise physical cause was not captured; do not infer a confirmed register fault.

The board-local `storage_version.h` pins Vial BUILD_ID to **0x0036E2FF**, recovered
from the tested ELF. This fork otherwise generates random IDs on every build,
which changes Vial EEPROM validity. Keep this ID for compatible rebuilds.

Canonical QMK build: `make alectardy98_ec/ec_prox:vial_alec`.
The board-local `post_rules.mk` automatically invokes `clean_install.py`, writes
the FF-filled BIN to the release directory above, and retains its ELF/map.
No format argument, separate packaging command or analysis-workspace script is required. Keep `storage_diagnostics.c`: it is a
permanent board-local observer of shared guarded storage, not a temporary driver.

The firmware still fits the minimum 128 KiB geometry; current sizes and hashes
are recorded with the build-warning validation report. The clean-install BIN
includes the deliberately erased EEPROM gap.
Standard single-sector QMK consolidation is fail-closed but not dual-bank atomic;
power loss can require recalibration and restoring exported Vial settings.

## Vial build configuration and lighting

This fork still selects Vial in `common_features.mk` with `VIAL_ENABLE`, while
its upstream metadata mappings reject that option in `rules.mk` and reject
`VIAL_KEYBOARD_UID` in `config.h`. Board `post_rules.mk` and `post_config.h`
therefore provide these fork-specific settings. UID and EEPROM identity are unchanged.

`VIAL_INSECURE = yes` is intentional and unchanged: Vial starts unlocked, so a
host with access to the Raw HID interface need not perform a physical unlock
sequence for operations otherwise protected by Vial (including bootloader entry).
Enabling security would require choosing and testing an unlock chord; it is not
part of this warning cleanup. The compiler's insecure-mode message is informational.

RGBLIGHT alone now owns all 22 WS2812 LEDs on PB12. RGB Matrix and VialRGB
were removed from both Vial keymaps with owner approval; their obsolete Vial
lighting advertisement was removed. In vial_alec, native RGBLIGHT overlays
reserve index 0 for the existing highest-layer indicator: base off, HHKB red,
Fn green, Fn2 blue. LEDs 1–21 retain underglow effects. The overlay is visible
even when underglow is toggled off, and follows persistent default-layer changes.
No second driver competes for the buffer. This lighting-only implementation
behavior is reported working by the owner; future changes need physical
validation of layer colors and underglow coexistence.

The board-local linker option `-flto=3` allows three LTO worker jobs using GCC's
existing partitions. LTO, optimization level and partitioning stay unchanged.
