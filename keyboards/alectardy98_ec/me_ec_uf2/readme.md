# Alectardy98 me_ec_uf2 EC framework port

Build: `make alectardy98_ec/me_ec_uf2:vial_alec`.
Also supports `vial`; see the root README for other copied keymaps.

Original hardware acquisition/pins/timing are retained; calibration/VA/RT/EC42
are shared from `../common/engine` and `../common/runtime`. New EC42 v2.9 records
require fresh manual incremental calibration. No legacy EC migration exists.

Read `../ARCHITECTURE.md` for the new EEPROM allocation and `../TESTING.md` before
programming. Artifacts: `firmware/alectardy98_ec/me_ec_uf2/<keymap>/` from repository root.
The owner reports this port working on hardware. Future changes still require
physical validation; current software checks do not prove analog/recovery behavior.
Original upstream copyright/license notices remain in reused source.

## Custom ASTRO bootloader

QK_BOOT, Vial and EC41 boot requests use the board-local RTC_BKP0R handoff
(0x41544B42) through QMK's standard bootloader_jump hook. UF2 packaging remains
the original QMK conversion: family 0x57755A57, base 0x08020000. Only sector 5
is permitted; sector 7 EEPROM and sectors 0..4/6 are excluded from updates.
No standard TinyUF2 SRAM handoff is used. See ../TESTING.md for recovery.

ASTRO handoff additionally accepts only an unselected or LSI RTC source, matching
the installed loader initialization. Unexpected sources are rejected without
resetting the backup domain or the MCU.
