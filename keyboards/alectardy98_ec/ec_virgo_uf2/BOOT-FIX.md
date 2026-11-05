# STM32F411CCU6 boot rejection correction — 2026-11-05

## Cause and evidence

The owner identified the physical MCU as **STM32F411CCU6**. ST's STM32F411CC
product page specifies 256 KiB flash; the original generic QMK processor selection
was STM32F411 with an F411xE linker default of 512 KiB. The initial Virgo TinyUF2
adapter incorrectly converted that generic build assumption into an exact
runtime requirement of **512 KiB**.

Both `board_app_valid()` and `virgo_write_valid()` rejected a 256 KiB factory
value. Thus an initial combined BIN could install the complete application, but
TinyUF2 would refuse to launch it or accept subsequent UF2 writes on the CC part.
The readback through CURRENT.UF2 confirmed that the installed application matched
the archived `vial` application byte for byte. Its stack/reset vectors were
`0x20000400` / `0x080102fd`, both valid for the intended application layout.

A native reproduction compiled the previous density gate and the corrected gate
against those captured vectors: the former rejected a simulated 256 KiB factory
value; the latter accepted it and continued rejecting the wrong 512 KiB value.
This establishes the code/model mismatch. The factory register was not directly
read from the old bootloader, and post-correction physical boot remains a separate
acceptance test. No debug probe was connected and no flash operation was issued.

## Correction

- Board TinyUF2 capacity is 256 KiB, six sectors, application end `0x08040000`.
- Startup and write gates require the identified 256 KiB value.
- Both EC42 Virgo linkers and package validators enforce the 256 KiB boundary.
- The non-UF2 adapter's EEPROM backend size declaration is also corrected.
- SRAM stays 128 KiB; mailbox `0x2001fffc` and settings placement are unchanged.
- The STM32F411xE CMSIS selector stays: its device-header source explicitly
  includes STM32F411CC among the supported devices. Changing that macro to an
  invented xC selector would break the supplied ST headers.
- Read-only boot information now includes factory KiB, vectors, reset request
  and raw RCC flags when the loader remains active. It does not consume/clear
  those values itself or bypass a failed check.

Only board-local files changed. Existing EC42 common code, Cipulot sources,
QMK/ChibiOS and the owner TinyUF2 checkout remain unchanged. The new bootloader
is built in a disposable pinned checkout with the already recorded hardening
patch. Earlier release directories remain untouched.

## Installation

Use the corrected **combined FIRST-INSTALL BIN** through the same ROM-DFU/SWD
method used initially. The old TinyUF2 loader cannot fix itself from an
application-only UF2 and has no enabled self-update route. The combined BIN is
for `0x08000000`, discards flash settings, and requires manual calibration.

After installing it, perform a complete unplug/reconnect with all keys released.
The board should start the EC42 application rather than remain at VIRGOBOOT.
Open VialAstro 1.56+, calibrate, Save and apply, and verify normal typing. Then
enter TinyUF2 deliberately and update the application by the matching Virgo UF2;
verify calibration survives a normal UF2 update and cold restart.

## Validation

The release evidence includes all four normal QMK builds (`vial`/`vial_alec`,
DFU/UF2), the TinyUF2 build receipt, native bootloader tests with UBSan, the 11
actual-source Virgo EC42 tests with ASan/UBSan, compiled EEPROM layout tests,
UF2/ELF agreement and first-install settings-gap checks. Broad TinyUF2 agent
checks still have six missing unrelated SDK errors; formatting/YAML/spelling
checks pass. This is not a claim of physical acceptance.

The local Calibre manufacturer/errata lookup was unavailable. Capacity is based
on the owner's exact marking and ST's primary product information, while retained
register sequences remain source-derived:
https://www.st.com/en/microcontrollers-microprocessors/stm32f411cc.html
