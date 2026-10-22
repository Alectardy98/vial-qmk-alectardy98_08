# Source and history evidence

Paths are relative to the QMK root unless stated otherwise. The local source is
the authority for this checkout; public master links can change. Commit records
and hashes pin the evidence used. Git authors and source copyright holders are
recorded separately.

| Framework component | Source and evidence | Finding |
|---|---|---|
| `ec_prox/acquisition.c` | `keyboards/cipulot/common/ec_switch_matrix.c:60` (`init_row`), `:69` (`init_amux`), `:133` (`ec_init`), `:159` (`ec_noise_floor`), `:197` (`ec_matrix_scan`), `:232` (`ec_readkey_raw`) | Direct function extraction/adaptation: same bank/row selection, charge/discharge and averaging structure. EC42 capture/telemetry inserted; ADC hardware implementation not newly invented. |
| `common/runtime/decision.c` | Same original file `:256` (`ec_update_key`), Git blame `ed791972e1`, plus local threshold rescaling changes in `d440f6370b` | Original APC and extremum branches survive refactoring. New disabled-key guard/VA routing is distinguishable from copied legacy branches. Missing Cipulot notice restored. |
| `common/runtime/ec_board.c` | `keyboards/cipulot/common/ec_board.c`; original full 2023 GPLv3-or-later header retained | Keyboard hooks retained, persistence initialization substantially adapted. |
| `common/engine/ec_switch_matrix.h` | `keyboards/cipulot/common/ec_switch_matrix.h:25` onward | Named threshold/runtime structures retained and extended; explicit record encoding is later work. |
| `common/engine/calibration.c` | `keyboards/mmstudio/class0413_ec_test/modules/calibration.c`; original Cipulot rescale/runtime fields in common/ec_board.c and via_ec.c | New transaction/qualification engine with adapted scaling/runtime setup. Restored credit explicitly limited to adapted portions. |
| `common/engine/actuation_control.inc` | Local test port's same-named file; Cipulot `ec_update_key` high/low turning-point logic | Reimplemented/refined RT retains algorithmic lineage; VA-first activation, median filtering, revision control and debounce are EC42 additions. |
| `common/engine/record_storage.inc` | Later explicit portable serializer in universal-framework/canonical-v29 analysis; no direct Cipulot serializer body | New explicit format/CRC/enabled mask/overrides, not original Cipulot EEPROM format. Shared struct/API ancestry is not authorship of every serializer line. |
| `common/engine/ec_diagnostic.{c,h}`, interfaces | Earlier `class0413_ec_test/modules/ec_diagnostic.*` and subsequent local universal protocol work | EC41/EC42 wire transactions and capability/telemetry framing are project work. No original Cipulot EC42 implementation was found. |
| `class0413_ec/matrix.c` | Working `keyboards/mmstudio/class0413_ec/matrix.c`, itself a Cipulot EC port; original Cipulot common scanner | Confirms derivation even after hardware remapping and extraction of ec_update_key. MM Studio stock binary was hardware evidence, not proof of a vendor source-code license. |
| 0413 storage guards | Local production/test guard modules; `platforms/chibios/drivers/wear_leveling/wear_leveling_efl.c` and QMK journal remain separate dependencies | New policy wrappers call the original implementation. Do not assign QMK journal authorship to EC42 or Cipulot. |
| `class0413_ec/modules/native_music.c` | `quantum/process_keycode/process_music.c:1` | Includes original QMK code with Jack Humbert notice intact. Music remains disabled in the current firmware. |
| `class0413_ec/modules/time_math.h` | `lib/chibios/os/rt/include/chvt.h` TIME_* equations/type semantics, GPLv3-only header | Algebraic implementation correspondence, not proven literal code copy. Original wrapper SPDX preserved; licensing scope flagged for review rather than guessed. |
| Prox keymaps/config | `keyboards/cipulot/ec_prox/ansi_iso/` original path counterparts; `15cb7ded76`, `ed791972e1`, `2aa186873e`, local commits | Preserve 2023/2025 notices exactly as present. A header is evidence of claimed copyright; it is not proof that Cipulot wrote later user macro additions. |
| Matrix scanner/lighting/keymaps | `keyboards/matrix/me_ec_uf2/`, first imported in `fe33059b22` | Many substantial matching bodies, missing file-level notices. Importer/maintainer cannot be equated with original author. No invented replacement SPDX. |
| Matrix linker/mcuconf | Same original files with Giovanni Di Sirio Apache-2.0 headers, retained verbatim | Clear file-specific exception to any GPL-only description. Changed linker has a modification notice; identical config is kept unchanged. |
| ASTRO hook | `me_ec_uf2/astro_bootloader.c`, source explanation, previous `analysis/matrix_astro_handoff/` | New QMK hook for observed RTC handoff, not a copy of ASTRO bootloader code. A bootloader dump is not a redistribution permission. |
| GUI window/widgets | `keyboards/xwhatsit/util/util/mainwindow.{h,cpp}`, `signal_level.cpp`; all 16 recorded PROVENANCE.json hashes match | Confirms Purdea Andrei/Cap-Util lineage despite the historical reverseenginering path no longer being present. GPLv2-or-later notices preserved; derivative selection GPLv3-or-later already explicit. |
| GUI protocol/layout/model | External qmk-ec-util source import; C++ runtime implementations | EC42 codecs and runtime layouts are later app work. KLE/Vial Python is a behavioral reference; no copied Python implementation found. Do not import an unverified license merely because a reference URL exists. |
| GUI screenshot | Same SHA-256 `a3b67ddffe2d19410eac03bc744254ad65da6f32ffde742f1af827cab058dd4d` in external app; screenshot CLI in util/src/main.cpp | Imported app output, not a bundled Cap-Util icon. Exact image creator remains unasserted. |

## Original Cipulot history

- `15cb7ded76` (2023-03-23): Cipulot EC ProX PCB.
- `ed791972e1bf812c432655de31e3cf92f3bcf397` (2024-02-26): common refactoring;
  author Cipulot, coauthors Drashna Jaelre and Joel Challis. Preceding board-local
  EC implementations include ec_60, ec_pro2, ec_prox, ec_23u and related Cipulot
  boards. Those are ancestors, not evidence that they all contributed distinct
  independently licensed code to this port.
- `0f701c7dbe` (2024-02-26): Cipulot multiple-AMUX correction.
- `d09a06a1b3` (2024-05-03): Ryan/fauxpark GPIO API update.
- `2aa186873e` (2024-10-28): Cipulot updates.
- `0e20b59056`, `d440f6370b` and later local changes: Alectardy98 adaptations.

The old EC-Pro-X hardware URL in keyboard metadata returned 404 during this audit;
no replacement repository URL or license was invented. Public QMK source/commit
links in THIRD_PARTY_NOTICES.md are verifiable references.

## Upstream licenses consulted

- QMK root `LICENSE`: GPLv2 text; explicit individual-file later-version grants
  are separately retained, not inferred for unknown imports.
- Cipulot source headers: GPL-3.0-or-later.
- `quantum/vial.c`: Ilya Zhuravlev 2020, GPL-2.0-or-later.
- `quantum/wear_leveling/wear_leveling.c`: Nick Brassel 2022; predecessor credit list.
- `lib/printf/LICENSE`: MIT, Marco Paland and Eyal Rozenberg; source also credits
  Martijn Jasperse for exponential specifiers.
- `lib/lib8tion/LICENSE`: MIT, FastLED 2013.
- `lib/fnv/hash_32a.c`: public-domain statement and Landon Curt Noll disclaimer.
- ChibiOS `ch.h`/`chvt.h`: kernel GPL-3.0-only; Matrix mcuconf/linker Apache-2.0.
- Installed Qt/HIDAPI/xz license texts and package versions; full direct/transitive
  dynamic dependency inventory in GUI-DEPENDENCIES.json.
- ARM GCC runtime exception and newlib/libgloss notices copied from installed
  toolchain packages. Keep all original source notices with a matching release.

Official compatibility references:
[GNU license FAQ](https://www.gnu.org/licenses/gpl-faq.en.html#AllCompatibility),
[GNU GPLv3 compatibility guide](https://www.gnu.org/licenses/quick-guide-gplv3.html),
[Qt licensing](https://doc.qt.io/qt-6/licensing.html),
[HIDAPI BSD license](https://raw.githubusercontent.com/libusb/hidapi/master/LICENSE-bsd.txt).
These general rules do not establish missing source grants or certify a bundle.
