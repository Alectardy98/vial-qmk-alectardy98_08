Virgo EC - automatic 60-second calibration firmware

Install this folder at:
  keyboards/cipulot/ec_virgo/keymaps/calabration

Compile:
  make clean && make cipulot/ec_virgo:calabration

Then flash the resulting firmware and run:
  qmk console

The firmware automatically enters bottoming calibration for 60 seconds.
During that minute, press every physical key fully to bottom-out.
After 60 seconds it exits calibration, saves using Cipulot's native
ec_save_bottoming_reading(), and prints the calibration arrays.

VIA_ENABLE is enabled because Cipulot's ec_save_bottoming_reading()
implementation is provided by via_ec.c.

No Vial. No LTO.
