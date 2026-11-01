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

Build prerequisites are GNU Make, Python 3 with QMK's Python dependencies and
the `qmk_cli` package, and ARM GNU GCC/binutils (`arm-none-eabi-*`) on `PATH`.
The offline C tests also require a native `cc` compiler. Initialize the pinned
library submodules before the first build:

```sh
git submodule update --init lib/chibios lib/chibios-contrib
```

The AUX vendor sources and Main factory defaults are included in this tree.
Building does not require another local firmware checkout, a connected keyboard,
stock firmware dumps, or the archived development directory.

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
python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/run_presentation.py
SANITIZE=1 python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/run_presentation.py
sh keyboards/alectardy98/matrix_my2k_100ng/aux/tests/run-stock-write-tests.sh
SANITIZE=1 sh keyboards/alectardy98/matrix_my2k_100ng/aux/tests/run-stock-write-tests.sh
```

Build both images before running the artifact and packaging checks. Tests are
offline; the AUX write tests use simulated sectors in RAM and cannot write to
the keyboard. See [AUX test notes](aux/tests/readme.md) for the scope of the
historical fixtures. Passing these checks does not replace physical testing.

## Firmware updates

Use the ASTRO BOOT workflow, with a fresh BOOT session for each UF2. For a paired
update, install AUX first, wait for normal reconnection, then enter BOOT again
to install Main.
Do not copy both UF2s into one BOOT session. Keep known-working Main and AUX UF2s
outside the build output directory for independent rollback.

In the controlled stock/custom flash cycle, the custom AUX UF2 took about
28 seconds and Main about 4.3 seconds. AUX's 1 MiB UF2 carries a 512 KiB flash
image; the other half is UF2 framing and padding. A copy progress bar is not
proof that programming finished. Wait for completion and reconnection before
starting another update. A transfer that stops progressing with USB timeouts
is a separate fault, not the expected 28-second programming time.

The current AUX startup sets `DBGMCU_CR.DBG_SLEEP` before its first idle wait.
Without this, Cortex-M7 Sleep can gate HCLK and prevent ASTRO's onboard SWD
programmer from reliably attaching to the running AUX. The observed failure
stalled the first UF2 data write until Linux's 30-second USB recovery. Setting
this documented bit keeps debug memory access available while AUX uses `WFI`;
it does not replace idle waits with busy polling or change Main's bootloader.
See ST RM0431 sections 34.16.1–34.16.4 for the clock and reset semantics.

Four consecutive software-BOOT AUX updates passed with this change, including
a cold reconnect with the Pico disconnected. Each took about 28 seconds. A
full 512KiB AUX readback after the third matched the UF2 exactly; the final
update also completed every data command successfully. Older AUX images lack
this startup setting: upgrading from them may still require a fresh physical
BOOT entry or probe recovery for that initial installation. Keep recovery
available until the updated pair has been verified on your keyboard.

BOOT automatically reboots when programming finishes. A later synthetic-FAT
metadata flush can therefore report an I/O error even after every UF2 data
command was acknowledged, as observed in the High Performance entry test.
This differs from a stalled first data command. Check the returned firmware
identity before interpreting a late filesystem error as a failed image.

## Animation startup and game transitions

`Release1-20261101-p1` waits for Main's saved animation-slot restore before AUX
presents its first animation frame. The internal ANI5 handshake keeps a legacy
root CRS from flashing onscreen first. With no saved slot, Main explicitly
accepts the discovered default. An invalid saved slot stays unpresented until
the user chooses an available slot; it does not silently display a different GIF.
The small OLED, storage and input services continue during this handshake.

Game status advertises an additive atomic-handoff capability in byte6 bit1.
Main then requests the next game directly, and AUX stops/starts the two owners
in one filesystem-worker dispatch, at a completed display-transfer boundary.
Animation remains held during loading and failed handoffs; an explicit normal
return or animation selection releases it. Older peers retain their existing
command format. Install both new images for the complete fix; no EEPROM layout,
VialAstro protocol, RGB driver or asset-file format changes are required.

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
The Doom port's upstream license text is retained in
[aux/vendor/doom/LICENSE.md](aux/vendor/doom/LICENSE.md). NES and Game Boy ROMs
are supplied separately on the keyboard's storage and are not required to
compile the firmware. The Doom port includes specialized level and sprite
tables; its upstream `support/doom1.wad` is only a Git LFS pointer, not a complete
WAD. The original Doom source-license notices and game-data provenance need to
be considered separately from the QMK and Vial licenses when distributing it.
