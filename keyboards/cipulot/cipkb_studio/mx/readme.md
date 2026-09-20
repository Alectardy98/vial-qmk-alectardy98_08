# CipKB Studio MX

![CipKB Studio MX](https://i.imgur.com/SRkj6g4h.png)

CipKB Studio MX is a port of the HHKB Studio to QMK.

* Keyboard Maintainer: [cipulot](https://github.com/cipulot)
* Hardware Supported: CipKB Studio MX
* Hardware Availability: [HHKB Studio](https://hhkeyboard.us/hhkb-studio/)

Make example for this keyboard (after setting up your build environment):

    make cipulot/cipkb_studio/mx:default

Flashing example for this keyboard:

    make cipulot/cipkb_studio/mx:default:flash

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 2 ways:

* **Physical Boot0 pins**: Short the Boot0 pins on the back of the PCB while plugging in the keyboard
* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is available
