# Third-party notices and source provenance

This framework contains modified upstream work as well as later EC42 work.
File-level notices govern; this document does not relicense anyone else's code.
The full audit and file matrix are in `development/alectardy98_ec/licensing/`
from the QMK repository root. Missing/uncertain licenses remain unresolved,
not silently converted to GPL. Original keyboard directories are unchanged.

## Cipulot — electrocapacitive firmware foundation

**Cipulot's electrocapacitive keyboard implementation is a substantial foundation
of this framework.** Copyright notices identify **Copyright 2023 Cipulot**, and
some copied keymaps identify **Copyright 2025 Cipulot**. Those notices are retained.
The EC source files grant **GPL-3.0-or-later**, not the root QMK license by default.

Verifiable source:
- [QMK Cipulot common EC acquisition/decision code](https://github.com/qmk/qmk_firmware/blob/master/keyboards/cipulot/common/ec_switch_matrix.c)
- [EC data structures](https://github.com/qmk/qmk_firmware/blob/master/keyboards/cipulot/common/ec_switch_matrix.h)
- [Original VIA EC/calibration interface](https://github.com/qmk/qmk_firmware/blob/master/keyboards/cipulot/common/via_ec.c)
- [Shared-code refactoring, ed791972e1](https://github.com/qmk/qmk_firmware/commit/ed791972e1bf812c432655de31e3cf92f3bcf397)

Local ancestry is `keyboards/cipulot/common/` and `keyboards/cipulot/ec_prox/ansi_iso/`.
The shared decision path, EC runtime structures, baseline averaging and downward
baseline adjustment, rescaling, ADC/multiplexer acquisition and extremum-based
press/release logic derive from this work. They were adapted into
`common/runtime/{decision,ec_board}.c`, `common/engine/ec_switch_matrix.h`,
`ec_prox/{acquisition,matrix}.c`, and the earlier 0413 port/scanner. The shared
calibration/actuation modules retain adapted runtime/scaling concepts and code.

The explicit transactional record codec, incremental capture qualification,
uncalibrated-key mask, EC42 USB commands, telemetry, VA-first RT refinements,
Deepest safety limits and persistence guards are later EC42 framework work.
They must not all be represented as features originally supplied by Cipulot.
Neither this acknowledgment nor the use of upstream work implies endorsement
or collaboration by Cipulot.

Git also records Drashna Jaelre and Joel Challis as coauthors of the 2024 common
refactoring, Ryan/fauxpark for GPIO API updates, and subsequent local adaptations
by Alectardy98. Commit authorship is evidence of contributions, not a replacement
for source copyright notices or proof of exclusive ownership.

## QMK, TMK and Vial

QMK/TMK provide USB/HID, matrix plumbing, key processing, native audio/Clicky,
solenoid/haptics, lighting and persistent-storage primitives. Applicable
individual source notices must accompany corresponding source, including:

- `quantum/vial.c`: Copyright 2020 Ilya Zhuravlev, GPL-2.0-or-later.
- `quantum/process_keycode/process_music.c`: Copyright 2016 Jack Humbert,
  GPL-2.0-or-later; formerly included by an optional native_music wrapper (now retired and unlinked).
- `quantum/wear_leveling/wear_leveling.c`: Copyright 2022 Nick Brassel (@tzarc),
  GPL-2.0-or-later. Its own source credits Artur F., Yiancar, Ilya Zhuravlev,
  Don Kjer and Nick Brassel for preceding algorithms. The framework calls this
  implementation behind guards; it does not claim to have authored the journal.

Additional source notices include Jack Humbert (2016–2020) and JohSchneider
(2020) in native audio; mtdjr (2018), modified by ishtob, in the solenoid driver;
Drew Mills (2019) in the ChibiOS analog driver; and QMK (2024) in IS31FL3236.
The ChibiOS USB transport has a GPL-3.0-or-later OR Apache-2.0 notice and names
its contributors individually in usb_main.c, including Giovanni Di Sirio,
Fredizzimo, Drashna Jaelre, James Laird-Wah, George, Joel Challis, xyzz, a-chol,
Nick Brassel, Ryan and Stefan Kerkmann. The WS2812 bitbang driver itself credits
BigJosh's SimpleNeoPixelDemo as an adaptation. Those are upstream driver
contributions, not EC42 or Cipulot authorship. The detailed audit records the
original header excerpts and leaves all upstream notices unchanged.

See the full QMK/Vial source tree and submodule licenses. Redistributing only this
keyboard folder is not sufficient corresponding source for a compiled firmware.
Include the matching QMK revision/submodules, build configuration, and necessary
packaging/install scripts when satisfying source-distribution obligations.

## ChibiOS and STM32 support

Matrix `mcuconf.h` and `ld/me_ec_boot_STM32F411xE.ld` preserve **Giovanni Di Sirio**
notices (respectively 2006..2020 and 2006..2016) and **Apache-2.0** terms.
Do not label these files GPL merely because they are part of a QMK target.
ChibiOS/RT kernel headers in this checkout specify **GPL-3.0-only**; HAL files
have their own Apache-2.0 notices. The `time_math.h` specializations reimplement
ChibiOS rounding equations; the upstream source and its license are separately
identified in the file matrix. No broader license grant for copied ChibiOS
expression is asserted by the wrapper's existing SPDX header.

`lib/chibios`, `lib/chibios-contrib`, CMSIS/device headers, compiler runtimes and
newlib have component-specific licenses. Preserve the complete upstream trees
and their notices rather than treating these as EC42-owned code. GCC runtime
and newlib permissions must be checked with the exact shipped toolchain.

## Additional linked firmware dependencies

- `lib/printf`: MIT; its LICENSE names Copyright 2014 Marco Paland and Copyright
  2021 Eyal Rozenberg. Source additionally credits Martijn Jasperse for exponential
  specifiers. The complete MIT notice is copied as LICENSES/MIT-printf.txt.
- `lib/lib8tion`: Copyright 2013 FastLED, MIT; retained in MIT-lib8tion.txt.
- `lib/fnv/hash_32a.c` and companion hash sources: public-domain declaration and
  Landon Curt Noll warranty disclaimer, crediting Phong Vo and Glenn Fowler.
  The QMK type-validation adapter separately credits Nick Brassel and is
  GPL-2.0-or-later. See FNV-public-domain.txt and the full upstream source.
- ARM CMSIS headers in ChibiOS include Copyright 2009–2018 Arm Limited and
  Apache-2.0 terms (for example core_cm4.h); ST device files must retain their
  own individual notices too.
- The installed ARM toolchain is GCC 16.2.0 with newlib 4.6.0.20260123. GCC's
  runtime exception is copied verbatim, and aggregate newlib/libgloss notices
  are included. These are not blanket licenses for every toolchain object;
  source-file terms still govern. The exact linked objects are in release maps.

## Matrix source import — unresolved file-level grant

`me_ec_uf2/matrix.c`, LED map, most keymaps and some config/data files derive from
`keyboards/matrix/me_ec_uf2/`. That local source first appears in commit
`fe33059b22`, imported by Alectardy98. Several files contain no author/license
notice. The repository has a GPLv2 text, but that alone does not establish an
explicit GPLv2-or-later grant for these independently imported files. Their
original author and compatible distribution permission need confirmation.
No new SPDX or author has been invented for those files. This is a release
clearance issue, not something resolved by adding an acknowledgment.

The board-local ASTRO entry hook is a new register handoff implementation;
reading a magic value from a bootloader does not grant rights to redistribute
that bootloader. `DUMP/` contains physical firmware/bootloader dumps with no
verified redistribution permission. Keep them private or exclude them from any
public release until permission/source coverage is established. They are not
relicensed by the framework notices. Nothing has been deleted from the workspace.

## GUI — Cap-Util and system libraries

The application notices in `util/NOTICE.md` preserve **Copyright 2020 Purdea
Andrei** for adapted Cap-Util signal-color and window code. Original notices
are GPL-2.0-or-later; the derivative's existing GPL-3.0-or-later choice is retained.
The recorded 16 original-source hashes match `keyboards/xwhatsit/util/util/`.
The original recorded reverse-engineering path is historical, not a dependency.

GUI corresponding source also includes `common/interfaces/ec42_protocol.h`;
a util-only archive that omits that shared header is not a complete build.

The GUI dynamically links Qt Core/Gui/Widgets, HIDAPI-hidraw, liblzma and C/C++
runtimes. It does not bundle copies of those libraries or third-party icons/fonts.
Qt's open-source choices and component exceptions must be respected; HIDAPI
permits BSD-3-Clause/GPLv3/original choices, and installed liblzma uses 0BSD.
Copies of the relevant license texts are provided in `LICENSES/` and the GUI's
own `LICENSES/`; the existing GUI COPYING remains. A future AppImage or library
bundle requires its own dependency/plugin/license inventory and corresponding
source compliance; these notices are not certification of such a bundle.

KLE/Vial layout parsing is a public-format behavioral reference, not evidence
that Python source was copied into the C++ implementation. Existing notices link
the reference implementations. No MIT/BSD grant is inferred from those links.
The imported GUI screenshot has the same hash as the external application's
screenshot; its original image creator is not independently established.

## Distribution and compatibility

GPL-2.0-or-later portions can be combined under GPLv3 with GPL-3.0-or-later work;
GPL-2.0-only cannot simply be treated that way. Apache-2.0 is compatible with
GPLv3, not GPLv2-only. The ChibiOS kernel's v3-only terms must remain intact.
These compatibility statements do not resolve missing grants for Matrix imports
or permission for stock dumps. This audit does **not** certify a fully cleared
public release while those questions remain.

Preserve notices, modification notices/dates, license texts and disclaimers.
For binary redistribution provide the matching complete corresponding source
and any applicable installation information; provide required library notices
and source/relink rights when distributing library copies. Do not assert one
blanket license for every file, stock dump, toolchain or system dependency.
