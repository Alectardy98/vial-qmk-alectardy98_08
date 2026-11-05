# Virgo EC — EC42 board adapter

Port of `keyboards/cipulot/ec_virgo` to the existing Alectardy98 EC42 v2.9g
framework. The original Cipulot implementation is unchanged. Original copyright
and GPL-3.0-or-later notices are retained.

```sh
make alectardy98_ec/ec_virgo:vial
make alectardy98_ec/ec_virgo:vial_alec
```

The normal commands produce a BIN in the repository root and an addressed HEX
in `.build/`.
This retains the stock STM32 ROM-DFU bootloader selection; it does not install
TinyUF2 or change option bytes. No hardware was flashed during development.

## Board-local responsibilities

- `config.h`, `keyboard.json`, `halconf.h`, `mcuconf.h`: stock MCU selection,
  clocks, ADC, GPIO, mux mapping, 6×18 matrix, 93 populated positions, USB IDs and
  physical layout. The sensing helpers, discharge/settling delays, ADC atomic
  section and 30-scan startup baseline are preserved exactly.
- `acquisition.c`: original acquisition helpers and scan order; each raw sample
  is handed to the common `ec_process_sample()` adapter.
- `matrix.c`: original QMK custom-matrix lifecycle.
- `rules.mk`, `post_rules.mk`: compile the common engine, decision, lifecycle,
  protocol and guarded storage directly. No copies of their algorithms live here.
- `ld/ec_virgo_ec42.ld`, `clean_install.py`: retain the stock split flash layout
  and package the reserved EEPROM gap with FF rather than objcopy's zero fill.
- `post_config.h`, `storage_version.h`: preserve Virgo's supplied Vial UID and
  pin a new schema identity for compatible rebuilds.
- `keymaps/vial`, `keymaps/vial_alec`: preserve the supplied layouts/macros and
  custom-keycode values. The legacy `ec_apply_calibration_defaults()` boot hook
  and its precomputed sensor tables are deliberately not compiled into EC42.
- `tests`: actual-source scanner/protocol tests and compiled-image verification.

The `common/engine`, `common/runtime` and `common/interfaces` sources are
unchanged. The calibration capture qualification is the existing EC42 sustained
100-count rule, rather than the legacy Virgo single-peak/50-count behavior.
This is a calibration policy change, not an ADC timing change.

## Identity and persistence

| Property | Value |
|---|---|
| USB VID:PID | `6369:6bc0` (preserved) |
| Vial UID | `564952474f454301` (preserved) |
| EC42 identity | `A98-virgo-EC42-2.9g` |
| EC record magic | `0xa66c` |
| Canonical EC record | 341 bytes, `17 + 3 × 108` |
| Logical EEPROM | 4096 bytes |
| Journal backing | 8192 bytes within the 16 KiB settings erase sector |
| Pinned Vial BUILD_ID | `0x0066ec42` |

The actual compiled EEPROM layout in both keymaps is:

| Region | Logical byte addresses |
|---|---|
| QMK core | 0–36 |
| Canonical EC record | 37–377 |
| VIA validity/layout options | 378–381 |
| Four-layer dynamic keymap | 382–1245 (864 bytes) |
| Existing Vial/QMK feature tables | 1246–2437 |
| Dynamic macros | 2438–4095 (1658 bytes) |

The old Virgo record is 225 bytes and is not compatible with this layout.
Legacy calibration and Vial settings are not automatically migrated. Export
backups before first installation. Fresh/foreign/corrupt calibration stays
unconfigured in RAM; ordinary typing is suppressed until explicit calibration
and Save and apply. Startup does not write fabricated calibration into EEPROM.

The common storage guard checks the factory density against the ChibiOS flash
descriptor, application extent, sector geometry, protection state and write
bounds. A mismatch disables settings writes; it is not bypassed for this port.
The existing single-sector journal is not a dual-bank atomic updater: interrupted
consolidation can require recovery/recalibration.

## Flash layout and installation

These are the **source/linker limits** corrected for the owner-identified STM32F411CCU6
(256 KiB flash, 128 KiB RAM):

| Region | Address range |
|---|---|
| Vectors, sector 0 | `0x08000000–0x08003fff` |
| Settings, sector 1 | `0x08004000–0x08007fff` |
| Code/data load allocation | `0x08008000–0x0803ffff` |
| SRAM allocation | `0x20000000–0x2001ffff` |

The owner identified the chip marking as STM32F411CCU6. The original generic
F411xE linker assumption was 512 KiB and has been corrected to 256 KiB. The CMSIS
F411xE header explicitly covers F411CC and is intentionally retained. Establish
the original ROM-DFU or SWD recovery path before first installation. Keep a full-flash/settings backup. The
manufacturer documentation recheck was unavailable because the configured Calibre
`metadata.db` is missing (lookup `cabb6fd7af0e`); register/errata semantics remain unverified against that local library.
The chip capacity is supported by the owner identification and ST product data:
https://www.st.com/en/microcontrollers-microprocessors/stm32f411cc.html.
No debug probe or post-fix physical validation has been performed.

The BIN is a **clean-install carrier** at **`0x08000000`**. It includes FF bytes
through the settings hole and is not an EEPROM-preserving update. With only this
Virgo in its known STM32 ROM DFU mode, a manual installation is:

```sh
sudo dfu-util -l
sudo dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave \
  -D alectardy98_ec_ec_virgo_vial_alec.bin
```

Do not use this layout on a board with a different installed user-flash bootloader.
Do not unprotect or mass-erase a device merely because a command fails. The HEX
is sparse and excludes sector 1, but its new logical EEPROM schema still requires
an intentional first-install reset/migration; this port does not claim to preserve
legacy settings. Restore the full-flash backup to roll back to the original layout.

Use VialAstro GUI 1.56 or newer for Virgo's EC tabs. Its sole protocol-module
change for Virgo is a UID/dimensions/magic registry entry; the existing HELLO,
version, descriptor and record checks remain mandatory. The standalone EC42
Utility's existing three offline presets were not modified.

## Validation

Run from the QMK repository root:

```sh
python3 keyboards/alectardy98_ec/ec_virgo/tests/build_harness.py \
  /tmp/ec-virgo-host-harness --sanitize
VIRGO_HARNESS=/tmp/ec-virgo-host-harness \
  python3 keyboards/alectardy98_ec/ec_virgo/tests/test_port.py
python3 keyboards/alectardy98_ec/ec_virgo/tests/verify_images.py
```

The host harness compiles the **actual current common engine/runtime files and
this board's actual acquisition code**, not a frozen firmware model. Only GPIO,
ADC input, time, EEPROM and host I/O are mocked. The test compile permits the
common engine's existing signed-comparison style; AddressSanitizer and UBSan remain
enabled. Tests cover 93/108 scanning, all 15 holes, uncalibrated suppression,
manual capture/save/reboot, per-key/Global/Deepest, real decision thresholds,
Rapid Trigger, keys-held rejection, cancellation, malformed/partial/stale requests,
foreign identity, source provenance and absence of legacy boot calibration writes.

Image checks compare actual ELF load segments with BIN/HEX, verify erased settings
gaps and vectors, decode the compiled Vial definition, check storage guard symbols,
and compile an EEPROM-layout probe using each real build's flags.

Software validation does **not** establish ADC quality, scan latency, flash erase
reliability, power-loss behavior, USB recovery or physical key mapping. Acceptance
on the board is still required:

1. Confirm identity, geometry and independent bootloader recovery.
2. Start manual calibration with all keys released; fully press/release every
   installed key and check the selected physical layout, including split options.
3. Save and apply; verify all calibrated keys, modifiers, layers and macros.
4. Test Global, per-key overrides, Deepest and RT; reconnect and verify persistence.
5. Check Vial remapping/layout persistence and subsequent normal firmware updates.
6. Confirm QK_BOOT/GUI bootloader entry and rollback before relying on the port.

## Porting another board

Keep sensing, GPIO/clock setup, holes, linker and flash policy in its adapter.
Add the unchanged common sources in its own `rules.mk`; provide a unique EC magic,
firmware identity, record allocation and Vial identity metadata. The board config
and descriptor determine record dimensions; no new opcode or special codec is
needed. Reuse the shared storage guard only where its documented hardware contract
matches, and keep new GUI admission entries as identity data rather than board
branches in protocol/algorithm code.
