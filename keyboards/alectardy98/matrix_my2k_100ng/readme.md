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

For the [Alec personal-macro keymap](main/keymaps/vial_alec/readme.md), build Main
with `make alectardy98/matrix_my2k_100ng/main:vial_alec`. It adds 39 macros to
Vial's User picker while retaining all 18 existing board controls. Its separate
output is `alectardy98_matrix_my2k_100ng_main_vial_alec.uf2`; AUX is unchanged.

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
├── build.mk          Board-local routing for the three supported build commands
├── main/
│   ├── src/          Main application and peripheral integration
│   ├── drivers/      RGB, USB host and storage transport drivers
│   ├── storage/      Filesystem, journal and private FatFS dependency
│   ├── chibios/      Required board-local USB patches and revision pins
│   ├── boards/       QMK/ChibiOS board support
│   ├── ld/           Main linker scripts
│   ├── keymaps/vial/ Vial definition and captured default keymap
│   ├── features.mk   Release 1 source selection and feature guards
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

This follows [QMK's keyboard layout conventions](https://docs.qmk.fm/hardware_keyboard_guidelines)
for Main: `keyboard.json`, `config.h`, `rules.mk`, `main.c`, `matrix.c` and
`keymaps/vial/` remain at the supported keyboard target. The maker directory is
organizational, not another keyboard target. AUX deliberately has no dummy
`keyboard.json`: its standalone build is the documented extension to QMK's
normal single-application build.

Both `lib/chibios` and `lib/chibios-contrib` stay pristine. Main applies its
required USB adaptations only to private generated copies in
`.build/my2k-chibios/`. The build checks pinned source/patch hashes, and never
modifies either submodule. Keep `main/chibios/` as part of the board source.

The board-local `build.mk` routes Main (`vial` and `vial_alec`) and AUX builds.
The root `GNUmakefile` is only a one-line include so the same commands work from
the repository root; GNU Make does not discover makefiles inside keyboard
folders automatically. Keep both files. Other keyboards still use QMK's original,
unchanged `Makefile`. The Python compatibility launcher supports Manjaro's
Python 3.14. Tool and source paths are relative to this checkout.

## Finding and extending the code

| Change | Start here |
| --- | --- |
| Physical layout, USB metadata | `main/keyboard.json`, `main/config.h` |
| Key scanning and debounce | `main/matrix.c`, `main/src/main_debounce.c` |
| QMK startup, key/layer callbacks, housekeeping | `main/main.c` |
| Enable a board feature or add a Main source | `main/features.mk`; hardware/build defaults are in `main/rules.mk` |
| Vial identity, defaults, custom key labels | `main/keymaps/vial/`; generated keymap comes from `tools/generate-defaults.py` |
| Custom keycode behavior | `main/src/main_keycodes.h`, `main/src/main_games.c`, `main/src/main_animation.c` |
| RGB, Lock Lights, touch, sleep | Matching `main/src/main_rgb_*`, `main_lock_*`, `main_touch_*`, `main_sleep.*` modules |
| Main flash settings and factory reset | `main/src/main_nv32.*`, `main_eeprom32.*`, `main_factory_*`, `main/defaults/` |
| UART framing and shared record formats | `common/`; Main transport is `main/src/aux_link.c` |
| USB host, storage ownership and export | `main/drivers/my2k_usbh_chibios.c`, `main/src/main_storage_mode.c`, `main/src/my2k_usb_msc.c` |
| AUX worker dispatch and peripheral startup | `aux/src/main.c`, `aux/src/work.c`, `aux/src/platform.c` |
| Color animation and display output | `aux/src/assets.c`, `animation.c`, `panels.c`, `panel_platform.c` |
| Small OLED content and settings | `aux/src/oled.c`, `text.c`, `clock.c`, `oled_store.c` |
| AUX card and filesystem access | `aux/src/sd_card.c`, `diskio.c`, `storage.c`, `msc_io.c` |
| Games and ROM libraries | `aux/src/nes.c`, `gbc.c`, `doom.c`, corresponding library/ROM modules; upstream cores are in `aux/vendor/` |

Main owns QMK input, external USB and RGB. AUX owns the displays, touch sensor,
asset filesystem and game execution. UART carries small control/status messages;
the internal USB connection carries block and file transfers. The AUX foreground
services USB/UART while its cooperative worker serializes display and filesystem
operations. Keep slow filesystem/display work out of Main's matrix path and out
of interrupt handlers. See the individual [Main](main/readme.md) and
[AUX](aux/readme.md) guides for entrypoints and build responsibilities.

Before changing an interface, check both endpoints and the GUI capability
negotiation. Append custom keycodes instead of reordering stored numeric values.
Treat EEPROM addresses, protocol fields and game RAM overlays as compatibility
boundaries. Settings migrations and capability/version changes need explicit
tests, not just a successful link.

Hand-maintained board C/headers use the root QMK formatting style with the local
`.clang-format` line-length setting. Leave generated defaults, captured keymaps,
vendor sources and pinned upstream patches out of blanket formatting. Historical
`DEV*` flags remain compatibility guards in the tested Release 1 build; their
names do not mean an old firmware image is selected. The AUX `DBG_SLEEP` setting
is required for reliable ASTRO programming and is not expendable debug logging.

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
SANITIZE=1 python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/test_usb_phy.py
SANITIZE=1 python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/test_vial_alec.py
python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/run_presentation.py
SANITIZE=1 python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/run_presentation.py
SANITIZE=1 python3 keyboards/alectardy98/matrix_my2k_100ng/main/tests/run_startup.py
sh keyboards/alectardy98/matrix_my2k_100ng/aux/tests/run-stock-write-tests.sh
SANITIZE=1 sh keyboards/alectardy98/matrix_my2k_100ng/aux/tests/run-stock-write-tests.sh
```

Build both images before running the artifact and packaging checks. Tests are
offline; the AUX write tests use simulated sectors in RAM and cannot write to
the keyboard. See [AUX test notes](aux/tests/readme.md) for the scope of the
historical fixtures. Passing these checks does not replace physical testing.

`qmk lint -kb alectardy98/matrix_my2k_100ng/main` can also check the conventional
keyboard files. This is a Vial fork, not an upstream-QMK submission: the stock
linter rejects the `vial` keymap, expects a `default` keymap, and flags the
intentional `defaults/factory.bin` test fixture. It also sees ignored local
`__pycache__` files, which are not committed. Keep those expected differences
separate from actual build/layout/test failures; do not delete required defaults
or change the supported keymap simply to silence upstream lint.

## Firmware updates

Use the ASTRO BOOT workflow, with a fresh BOOT session for each UF2. For a paired
update, install AUX first, wait for normal reconnection, then enter BOOT again
to install Main.
Do not copy both UF2s into one BOOT session. Keep known-working Main and AUX UF2s
outside the build output directory for independent rollback.

The custom application uses the embedded HS2 PHY for Standard and High
Performance USB. The unchanged ASTRO BOOT image can still fail enumeration
through some USB-A hub paths that the application now supports. If BOOT does
not appear, use the known-working USB-C/direct connection for the update;
changing the application does not update or repair the original bootloader.

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

`Release1-20261101-p2` starts animation discovery as soon as the AUX link is
responsive. An explicit AUX busy reply during panel/storage startup permits a
bounded 100ms retry; other errors retain the one-second backoff. A locally
deferred request does not restart that timer. This lets saved-slot restoration
finish before the separate OLED settings readback, while retaining the ANI5
first-frame gate and all panel initialization waits. This change is Main-only;
the paired AUX image remains unchanged.
Controlled Standard-mode software restarts measured about 3.60s before this
change and 2.11–2.13s afterward to the first observed completed color transfer.
These are host-polled transfer-counter measurements, not cold plug-in timing
or independently measured LCD scanout.

`Release1-20261101-p4` pairs with AUX build tag `aa8e2f79` for parallel startup.
Main completes the existing 11ms/2ms AUX reset in QMK's pre-init hook. AUX then
overlaps color initialization with OLED initialization and storage loading on
their independent peripheral buses. Each panel retains its command order and
minimum waits, including clearing the color window before display-on. Storage
starts during the color panel's 500ms sleep-out wait; slow/failed storage can
extend startup and is never hidden by fake readiness. The bounded control queue
may hold an animation capability probe during startup; settings and filesystem
commands remain deferred. The ANI5 saved-selection gate is unchanged.

Read-only diagnostic pages DQ26–28 expose boot timing (version 1): first completed
color transfer, panel readiness, storage loading and selection acknowledgement.
DBG1 operation 9 also reports native Main reset-release/current clock values;
its byte11 identifies pre-init timing before QMK rebases its timer. Six measured
restarts in Standard and High Performance modes estimated 0.888–0.891s from reset
to the first SPI-drained animation frame. The estimate removes the diagnostic
command's existing 100ms delay and QMK's 250ms shutdown delay; it is not an
optical cold plug-to-pixel guarantee. Confirm cold startup on physical hardware.
Neither ASTRO bootloader nor EEPROM/asset formats change.

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
