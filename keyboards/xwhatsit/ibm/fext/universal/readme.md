# FEXT — Universal controller

Build from the QMK repository root:

```sh
make xwhatsit/ibm/fext/universal:vial
```

This target uses the shared xwhatsit scan/utility protocol and the capability-checked XCAL live calibration settings. The build rule is local to this controller folder. `XWHATSIT_RUNTIME_CALIBRATION=no` selects the original constant-calibration path for the Vial keymaps.

Use the Universal controller wiring in `config.h`; this image is not for the older xwhatsit/WCASS controller revisions. No bootloader is included. Back up Vial settings before changing targets.

This Vial keymap provides **1 remappable layer(s)**. It omits Tap Dance, combos, key overrides and QMK settings to fit AVR flash/EEPROM; existing mouse/media/solenoid configuration is retained.

Compile/layout verification is recorded with Vi_al’s `dev` tools; physical acceptance remains board-specific.

Developer tests, port metadata and memory/build notes are maintained on Vi_al’s `dev` branch under `util/dev/xwhatsit_firmware/`; they are not needed to compile this keyboard.
