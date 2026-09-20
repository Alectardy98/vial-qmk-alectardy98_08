# 3x3 Macro Keyboard

![3_3_macro](https://i.imgur.com/tz3Q74J.png)

3x3 Macropad badge for the Geonworks User Networking Meetup 2025.

* Keyboard Maintainer: [cipulot](https://github.com/cipulot)
* Hardware Supported: 3X3 Macro
* Hardware Availability: [GeonWorks](https://geon.works) User Networking Meetup 2025

Make example for this keyboard (after setting up your build environment):

    make cipulot/3_3_macro:default

Flashing example for this keyboard:

    make cipulot/3_3_macro:default:flash

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 3 ways:

* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is configured.
* **Physical reset pins**: Long short the reset pins on the PCB.
* **Bootmagic reset**: Hold down the top left key and plug in the controller.
