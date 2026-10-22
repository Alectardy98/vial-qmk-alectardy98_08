# MM Studio Class0413 EC — canonical EC42 v2.9

Build: `make alectardy98_ec/class0413_ec:vial_alec` (also vial). Native audio,
Clicky, lock sounds, solenoid and LEDs remain; Music Mode stays disabled. The
ADC/mux/timing loop is preserved; decision/capture/VA/RT/codec are shared.

Application-only TinyUF2:08010000..0801FFFF. Sector2 EEPROM and stock TinyUF2
are unchanged. USB/Vial UID/key assignments/pinned Vial ID remain unchanged.

Calibration record is now332 bytes, common method 0x42/magic A105. Old219-byte
records are rejected in RAM without automatic save; manually recalibrate. Vial
addresses stay fixed: EC native 0..218 maps 37..255; native 219..331 maps 3983..4095.
Macro capacity becomes 1691 (previously 1804); export/shorten macros before flashing.
The adapter blocks conflicting macro buffers instead of erasing/truncating them.
No physical flash/calibration operation is performed by build/test scripts.

Use updated util/build-universal/qmk-ec-util. Backups and rollback details, schema,
partial calibration and hardware testing are in ../README.md and ../TESTING.md.
Original mmstudio firmware and pre-upgrade artifacts remain recovery references.
