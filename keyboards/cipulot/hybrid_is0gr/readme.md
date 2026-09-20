# Hybrid is0GR

![Hybrid is0GR]()

Hybrid is0GR.

* Keyboard Maintainer: [cipulot](https://github.com/cipulot)
* Hardware Supported: Hybrid is0GR PCB
* Hardware Availability: TBD

Make example for this keyboard (after setting up your build environment):

    make cipulot/hybrid_is0gr:default

Flashing example for this keyboard:

    make cipulot/hybrid_is0gr:default:flash

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 2 ways:

* **Physical Button**: Long press the button on the top of the PCB
* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is available
