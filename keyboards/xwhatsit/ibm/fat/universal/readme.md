# xwhatsit/ibm/fat/universal

![xwhatsit/ibm/fat/universal](https://i.imgur.com/5YjgiXWh.jpeg)

A short description of the keyboard/project

* Keyboard Maintainer: [Purdea Andrei](https://github.com/purdeaandrei)
* Hardware Supported: Supports the IBM Model F AT keyboards, using one of the xwhatsit controller PCBs supported by the 'universal' driver: the TH_XWhatsIt controller, or one of the SMD Model F controllers.

  The open hardware controllers supported by this firmware are published here:
    https://github.com/listofoptions/TH-XWhatsIt
    https://github.com/purdeaandrei/SMDModelFController
    https://github.com/purdeaandrei/SMD4704KishsaverClassModelFController
    https://github.com/purdeaandrei/SMDModelFController/tree/extra_columns
  And they have been discussed in the following threads:
    TH: https://deskthority.net/viewtopic.php?f=7&t=23406
    SMD Model F: https://deskthority.net/viewtopic.php?f=7&t=24597

* Hardware Availability: Rare collector's item

Make example for this keyboard (after setting up your build environment):

    make xwhatsit/ibm/fat/universal:vial

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Vi_al calibration

Build from the QMK repository root:

```sh
make xwhatsit/ibm/fat/universal:vial
```

The existing personal keymap is also available as `xwhatsit/ibm/fat/universal:vial_alec`.

This target uses the shared xwhatsit scan/utility protocol and the capability-checked XCAL live calibration settings. The build rule is local to this controller folder. `XWHATSIT_RUNTIME_CALIBRATION=no` selects the original constant-calibration path for the Vial keymaps.

Use the Universal controller wiring in `config.h`; this image is not for the older xwhatsit/WCASS controller revisions. No bootloader is included. Back up Vial settings before changing targets.

Developer tests, port metadata and memory/build notes are maintained on Vi_al’s `dev` branch under `util/dev/xwhatsit_firmware/`; they are not needed to compile this keyboard.
