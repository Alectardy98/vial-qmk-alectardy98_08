# EC Keychain

![EC Keychain]()

EC Keychain for the SKME 2025.

* Keyboard Maintainer: [cipulot](https://github.com/cipulot)
* Hardware Supported: EC Keychain PCB
* Hardware Availability: SKME 2025

Make example for this keyboard (after setting up your build environment):

    make cipulot/ec_keychain:default

Flashing example for this keyboard:

    make cipulot/ec_keychain:default:flash

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 2 ways:

* **Physical Pins**: Long short the exposed pins on the  the PCB
* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is available
