# Matrix MY-2K 100NG — Alectardy98 Custom

Manufacturer: **Matrix**  
Keyboard name: **FAUKWAA - [MY-2K] 100NG Edition - Alectardy98 Custom**

Board sources: `keyboards/alectardy98/matrix_my2k_100ng/`.

## Project origin

This is a clean-room reverse-engineering project developed by Alectardy98 with
Codex GPT 6.1, using REA (Reverse Engineer Anything) to analyze the original
ASTRO UF2 firmware binaries published on the
[ASTRO firmware download page](https://docs.iatkb.com/gao-su-di-yan-shi-pcb2.html),
including the MY2K 100NG Main and MY2K AUX images. The custom implementation
reconstructs hardware behavior and communication protocols from binary analysis
and physical testing.

The project also builds on QMK, Vial, ChibiOS and the separately licensed
open-source components credited below.

## Building

From the QMK repository root:

```sh
make alectardy98/matrix_my2k_100ng/main:vial
make alectardy98/matrix_my2k_100ng/aux:aux
```

Both commands build only. They never flash, reset, or access a keyboard.

```text
alectardy98/matrix_my2k_100ng/
├── main/
│   ├── src/          Main application and peripheral integration
│   ├── drivers/      RGB, USB host and storage transport drivers
│   ├── storage/      Filesystem, journal and private FatFS dependency
│   ├── chibios/      Required board-local USB patches and revision pins
│   ├── boards/       QMK/ChibiOS board support
│   ├── ld/           Main linker scripts
│   ├── keymaps/vial/ Vial definition and captured default keymap
│   ├── defaults/     Saved factory configuration
│   └── tests/        Offline regression tests
├── aux/
│   ├── src/          AUX application, displays and game integration
│   ├── include/      AUX headers and configuration
│   ├── vendor/       TinyUSB, FatFS, NES/GBC and Doom dependencies
│   └── tests/        AUX test fixtures
├── common/           Shared Main/AUX protocols and codecs
└── tools/            All Python build and packaging helpers
```

QMK configuration files, Main entrypoints and build rules remain at the Main
root where QMK expects them. AUX has its own `build.mk`, `rules.mk`, complete
compile manifest and linker script. It remains a standalone Cortex-M7
application rather than a QMK keymap. Shared QMK/ChibiOS source is unchanged.

Both `lib/chibios` and `lib/chibios-contrib` stay pristine. Main applies its
required USB adaptations only to private generated copies in
`.build/my2k-chibios/`. The build checks pinned source/patch hashes, and never
modifies either submodule. Keep `main/chibios/` as part of the board source.

The root `GNUmakefile` routes these two commands; other keyboards still use
QMK's original Makefile. The Python compatibility launcher supports Manjaro's
Python 3.14. Tool and source paths are relative to this checkout.

## Outputs

Only these files are emitted in the repository root by default:

- `alectardy98_matrix_my2k_100ng_main_vial.uf2`
- `alectardy98_matrix_my2k_100ng_aux_aux.uf2`

Each MCU's `rules.mk` contains:

```make
GENERATE_BIN ?= no
GENERATE_JSON ?= no
GENERATE_SHA256 ?= no
```

Set individual switches to `yes` to request extra final files. Disabled switches
remove only stale sidecars for that target. Intermediate BINs, ELFs, maps and
build caches stay in `.build/`. The verified ASTRO UF2 format and protected flash
boundaries are enforced even when optional reports are disabled.

## Defaults and validation

Your captured Main settings are the compiled factory defaults. A normal flash
preserves valid existing EEPROM. An erased EEPROM or Clear EEPROM restores the
captured defaults. OLED settings and assets on the 128MB storage are separate.

```sh
python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/run.py
SANITIZE=1 python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/run.py
python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/validate-artifacts.py
python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/test_package.py
python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/test_chibios.py
```

## Game and emulator credits

- **Doom:** [cnlohr/embeddedDOOM](https://github.com/cnlohr/embeddedDOOM), the
  memory-constrained Doom port used as the starting point for this keyboard's
  adaptation. Credit also goes to id Software for the original
  [Doom source](https://github.com/id-Software/DOOM).
- **NES:** [espressif/esp32-nesemu](https://github.com/espressif/esp32-nesemu),
  the source of the adapted Nofrendo emulator core. Credit to Matthew Conte for
  Nofrendo and to the ESP32 port's contributors.
- **Game Boy / Game Boy Color:**
  [PicoPlus-devel/pico-peanutGB](https://github.com/PicoPlus-devel/pico-peanutGB)
  by Frank Hoedemakers and contributors, used as the reference and source for
  the adapted core. Credit to Mahyar Koshkouei for
  [Peanut-GB](https://github.com/deltabeard/Peanut-GB), to tvecera and
  froggestspirit for the Game Boy Color work carried by the
  [gbc-rtc-fix branch](https://github.com/tvecera/Peanut-GB/tree/gbc-rtc-fix),
  and to Lior Halphon for the portions originating in
  [SameBoy](https://github.com/LIJI32/SameBoy).

Upstream copyright and license notices remain with the corresponding sources
under `aux/vendor/`. These credits supplement those notices.
