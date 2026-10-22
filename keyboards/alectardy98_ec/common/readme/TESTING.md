# Validation and recovery

Current status: the owner reports all three ports working on physical hardware.
The checks below remain required after future changes. Physical success does not
turn mocked tests into hardware tests. Use the canonical GUI in `util/`.
See README.md for full protocol, architecture, build and storage specifications.

No build/test script opens HID, flashes a device or writes physical EEPROM.

```sh
./development/alectardy98_ec/build-baselines.sh
./development/alectardy98_ec/test.sh
```

Only current v2.9 source is tested. The GUI CTest suite compiles the three actual
scan loops and shared engine, runs current Qt worker/UI integration, and executes
host storage/acquisition/boot checks normally and with ASan/UBSan. No HID is opened.
The final verifier requires all six images and checks flashable payloads against
ELF load sections. Historical fixtures are retired; archived reports are evidence,
not a second supported engine. See `architecture_review/CLEANUP_REPORT.md` under
the development directory for measured changes and limitations.

## 0413 canonical v2.9 upgrade

Back up Vial keymaps/macros and the old EC record before flashing. New calibration
uses 332 bytes; legacy 219-byte data is not migrated. Macro capacity shrinks from
1804 to 1691 bytes without moving Vial addresses. Shorten overlong macros using
the old firmware before installing; conflicts block EC saves, not erase data.
After flashing expect Calibration required and inactive keys; calibrate one key
and save, then add keys incrementally. Use the updated universal GUI.
Rollback needs matching old EC backup/recalibration and re-upload of Vial macros
to restore the old end-of-buffer sentinel after the new record uses that tail.
The original stable mmstudio source/firmware remains untouched.

## Before any owner-performed flash

1. Back up the *physical board's* application/settings and export Vial keymaps and
   macros. Keep a second keyboard/mouse: new firmware types nothing until manually
   calibrated. The source snapshot alone is not a backup of your device settings.
2. Confirm MCU/density and original boot method. EC Prox is configured STM32F401; Matrix STM32F411. Never change option bytes or WRP.
   Prox needs writable sector 1; Matrix needs a 512 KiB F411 and writable sector 7.
   A failed runtime guard disables persistence instead of bypassing protection.
3. Check SHA256SUMS and use only that board's artifacts. Do not mass erase.

## EC Prox: optional destructive clean installation

When the owner explicitly discards all settings, use `ec_prox.clean-install.bin`
at 08000000 after an intentional user-flash erase. Its EEPROM hole is FF and all
ELF bytes are verified; ordinary zero-filled `.bin.analysis-only` is not a clean
EEPROM image. Establish independent ROM DFU/SWD recovery before erase. Verify
actual density/protection without changing option bytes, and remain in programmer
mode until the image verifies. ROM DFU is outside user flash. This is **Prox only**;
do not apply a full-chip erase to Matrix or0413. See ec_prox/readme.md.

## EC Prox: safest selective programming procedure

Target is STM32 ROM DFU, not TinyUF2. Its vectors are at `0x08000000` and text at
`0x08008000`, with EEPROM between them. **Never download the gap-spanning flat
`.bin.analysis-only` at zero.** A sparse HEX contains no sector-1 payload, but a
programmer's erase policy can still erase address holes.

Use the board's established ROM DFU method or SWD/STM32CubeProgrammer. Selectively
erase application sectors **0, 2, 3, 4 only** for these checked <=128 KiB candidates.
Program `ec_prox.vectors.bin` at **0x08000000** and
`ec_prox.application.bin` at **0x08008000**, as separate ranges. Verify each.
Never select sector 1 or mass erase. If your hardware uses a different flash
bootloader/layout than the source target, stop instead of installing this image.

## Matrix: installed custom ASTRO UF2 loader

Use `firmware/alectardy98_ec/me_ec_uf2/vial_alec/me_ec_uf2.uf2` after entering
its established bootloader mode. Confirm ASTRO mass storage (USB 4D58:4266), copy
this UF2, flush the copy and wait for completion. The installed loader accepts
family **0x57755A57** and jumps through the vector table at **0x08020000**.
The original HEX-to-UF2 recipe is reproduced byte-for-byte by our packaging.

All payloads must stay in **sector 5, 0x08020000..0x0803FFFF**. This loader always
erases sector 5 at an aligned destination: images extending into sector 6 are
unsafe. The linker and package checks enforce this stricter limit. Preserve
sectors 0..4, 6 and 7 during installation. Sector 4 contains original settings;
sector 7 is the new runtime EEPROM. Runtime defaults/saves can change sector 7.

The Matrix-only `astro_bootloader.c` strong QMK hook enables PWR access, sets DBP,
writes **0x41544B42 to RTC_BKP0R**, verifies it and resets. It neither resets the
backup domain nor changes RTC clock sources. QK_BOOT, Vial's bootloader command,
and the supported GUI EC41 boot request reach this same hook. The QMK `tinyuf2`
build selector remains only to preserve its UF2 pipeline; its SRAM handoff is
not linked into Matrix's entry hook. Do not use generic TinyUF2 double-tap advice.

For initial testing retain an independently verified ASTRO entry/recovery method.
The loader dump includes Escape-held-at-power-on entry; confirm it physically
before relying on it. Normal original QK_BOOT entry is owner-confirmed. A selective BIN
alternative erases **sector 5 only** and downloads `me_ec_uf2.bin` at
**0x08020000**, then verifies before reset. Never mass erase or change option
bytes. Before use verify actual 512 KiB density and writable sector 7.

## Recalibration and functionality checks

1. Boot with all keys released. Check normal USB enumeration and Vial identity;
   close Vial before opening `keyboards/alectardy98_ec/util/build-universal/qmk-ec-util`.
2. Confirm EC42 v2.9, correct board layout/matrix and Calibration required. Baseline
   and live ADC should be plausible. No key should type before calibration.
3. Start Manual Calibration. Leave untouched positions gray. Fully press/hold and
   release one key for about a second; it should turn blue only from qualified
   firmware capture. A quick spike must not turn it blue. Finish, review, Apply &
   Save. Only that key should type. There is no 30-second deadline or auto-save.
4. Add several keys in a second session. Confirm the first key remains enabled,
   retries/cancel work, and uncalibrated keys remain inactive. Save a disk backup.
5. Test shallow/medium/Deepest globally, per-key override, reset override, RT off
   and three sensitivities. Check initial VA crossing and partial-travel RT cycles.
   Verify graph/raw counts and yellow/green state match actual firmware events.
6. Release keys and wait for Applied → Saving → Saved. Unplug/reconnect; verify
   calibration, settings, keymaps/macros and native lighting. Readback before a
   reboot is not proof of power-cycle persistence. Test suspend/resume and normal
   typing. Then calibrate the remaining real switches incrementally.
7. Test GUI/QK_BOOT recovery only with a known independent hardware fallback. For
   Matrix, confirm UF2 reinstallation does not erase sector 7 before relying on it.

Expected risks: analog settling/noise, startup-held keys, minimum 100-count capture
span, RT's software 4-count floor, USB pauses during large-sector consolidation,
and installed Matrix bootloader behavior require physical validation. Do not loosen
thresholds or write protection to conceal a failure. Startup baselines are RAM
measurements; release keys when powering on.

## Rollback

No new calibration record is compatible with legacy firmware. EC Prox reuses and
changes sector 1, so preserve your original sector-1 dump/export before testing.
Restore the matching original application plus its original settings backup, or
use its documented factory reset and reconfigure/recalibrate; do not expect new
records to work in old firmware. New GUI backups restore only to matching v2.9
profiles and UIDs. Never restore another board's calibration.

Matrix keeps the original dumped sector-4 store intact with these procedures.
Restore the exact original application with its established programming method;
verify old settings before assuming recovery. Use the original dump analysis
for selective recovery ranges; never install JustBootloader.bin wholesale.

0413: current canonical firmware differs intentionally from the old v2.8 release.
Keep the old firmware/GUI/EC and Vial exports separately; no radix migration is
provided. See the upgrade section above and the main README.

ASTRO handoff additionally accepts only an unselected or LSI RTC source, matching
the installed loader initialization. Unexpected sources are rejected without
resetting the backup domain or the MCU.
