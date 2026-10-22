# EC42 framework license, attribution and provenance audit

Audit date: 2026-10-21. Repository HEAD: cce96857f5. Framework:
`keyboards/alectardy98_ec/`. Source audit complete; **public-release clearance
is not established** for the specifically unresolved imports/dumps below.
No unknown code was relicensed or assigned a speculative author.

## 1. Outcome and scope

Audited **188 framework files**, including the initial 167 files and added
license/notice artifacts. Build caches are excluded; stock dump files, screenshot,
fixtures, keymaps, scripts and configuration data are included. The file-by-file
matrix is [PROVENANCE.md](PROVENANCE.md), with machine-readable details and hashes
in [PROVENANCE.json](PROVENANCE.json). Function-level evidence/history is in
[EVIDENCE.md](EVIDENCE.md). Similarity scores support identification; they are
not treated as ownership proof or the sole derivation test.

Confirmed Cipulot notices were restored where extraction lost them; modified
work notices/dates were added to adapted source. Existing original notices,
years and SPDX choices remain. No code, configuration values, serialized data,
protocol, EEPROM layout, keymap, scanner, GPIO or hardware behavior changed.

Outstanding release blockers: unheaded Matrix source's compatible grant and
stock dump redistribution permission. Further review items include untagged
project sources, screenshot authorship and the algebraic ChibiOS time wrapper.
See [OPEN_ISSUES.json](OPEN_ISSUES.json). Safe corrections were completed while
speculative licensing changes were deliberately stopped for these cases.

## 2. Projects, people and file-specific licenses

| Source | Confirmed authors/notice | Applicable evidence/terms |
|---|---|---|
| Cipulot EC source | Copyright 2023 Cipulot; copied keymaps also state 2025 Cipulot | Individual full notices: GPL-3.0-or-later. Explicit source notices take precedence over a generic root-license assumption. |
| Cipulot refactoring contributors | Cipulot; Drashna Jaelre and Joel Challis coauthors; Ryan/fauxpark GPIO updates; local Alectardy98 changes | Commit ed791972e1 and related history, separately from copyright declarations. Do not equate commit authorship with exclusive ownership. |
| QMK/TMK/Vial | Vial: Ilya Zhuravlev 2020; music: Jack Humbert 2016; audio: Jack Humbert 2016–2020 and JohSchneider 2020 | GPL-2.0-or-later source notices. Upstream source and copyright text are retained unchanged. |
| QMK wear leveling | Nick Brassel 2022; source credits Artur F., Yiancar, Ilya Zhuravlev, Don Kjer, Nick Brassel | GPL-2.0-or-later; framework adds wrappers, not authorship of the journal. |
| QMK hardware drivers | Solenoid: mtdjr 2018, modified by ishtob; analog: Drew Mills 2019; IS31FL3236: QMK 2024 | GPL-2.0-or-later individual headers. ChibiOS USB has a GPL-3.0-or-later OR Apache-2.0 choice and its own contributor list. Header excerpts in UPSTREAM-DEPENDENCY-NOTICES.json. |
| ChibiOS | Giovanni Di Sirio, exact source year ranges retained | Matrix linker/config Apache-2.0; RT kernel source GPL-3.0-only. Do not call all ChibiOS GPLv2 or automatically or-later. |
| ARM CMSIS | Arm Limited, 2009–2018 in core_cm4.h | Apache-2.0; preserve actual vendor header notices. |
| printf | Marco Paland 2014; Eyal Rozenberg 2021 in LICENSE; source also credits Martijn Jasperse | MIT, full copied license notice. |
| lib8tion | FastLED 2013 | MIT, full copied notice. |
| FNV | Landon Curt Noll, Phong Vo and Glenn Fowler | Public-domain declaration with Landon Curt Noll warranty disclaimer; QMK type adapter is separately GPL-2.0-or-later. No new copyright asserted over public-domain hash code. |
| Cap-Util | Purdea Andrei 2020 | Original GPL-2.0-or-later notices preserved in GUI widgets/window files; derivative's existing GPL-3.0-or-later selection retained. |
| Qt | The Qt Company and other contributors | Installed Qt 6.11.2; component-specific LGPL/GPL/commercial options. Current GUI links Core/Gui/Widgets and system dependencies dynamically. Full bundle clearance not inferred. |
| HIDAPI | Alan Ott / Signal 11 Software 2010 in BSD text | Installed 0.15.0 allows BSD/GPLv3/original alternatives. Full BSD notice copied, without removing the original library's choices. |
| liblzma | Current installed xz 5.8.3 package notices | liblzma 0BSD (not a blanket claim about all xz programs or historical versions). |
| ARM runtime/newlib | GCC runtime exception; newlib's many contributor notices | GCC 16.2.0/newlib 4.6.0.20260123; exact aggregate notices copied. File-specific runtime terms still govern. |
| Original Matrix source | Original author not established for unheaded imported files | First local import fe33059b22 by Alectardy98; maintainer/importer status is not proof of original authorship. Root GPLv2 text does not independently establish a later-version grant for these imports. |
| MM Studio stock image | Vendor authorship/source grant not established from the image | Used as hardware evidence; the local working QMK implementation is a Cipulot-derived port, not presumed manufacturer QMK source. |
| Stock dumps | Device images only | No blanket distribution grant; original ASTRO loader is not relicensed by a new handoff implementation. |

Original source links and pinned history:
[Cipulot common EC source](https://github.com/qmk/qmk_firmware/blob/master/keyboards/cipulot/common/ec_switch_matrix.c),
[original data structures](https://github.com/qmk/qmk_firmware/blob/master/keyboards/cipulot/common/ec_switch_matrix.h),
[via_ec.c](https://github.com/qmk/qmk_firmware/blob/master/keyboards/cipulot/common/via_ec.c),
[2024 shared-code refactoring](https://github.com/qmk/qmk_firmware/commit/ed791972e1bf812c432655de31e3cf92f3bcf397).
The EC-Pro-X hardware URL in the old metadata returned 404; no substitute URL
or license was invented. The local QMK fork and original source headers supply
the evidence regardless of the missing hardware repository page.

## 3. Derivation findings

### Copied and adapted Cipulot code

The multiplexer/row setup, bank selection, discharge/charge/sample path, resting
averaging and EC scan loop in ec_prox/acquisition.c are function extractions from
cipulot/common/ec_switch_matrix.c. 0413's scanner is an earlier hardware-adapted
Cipulot port with its own drive/discharge/channel configuration and ADC setup.
The original copyright block survived in these files.

The APC, downward-baseline update and extremum branches of ec_update_key were
moved into common/runtime/decision.c. Their structure, comparisons and updates
survive renaming/refactoring; the previous SPDX line alone omitted Cipulot's
copyright notice. That notice is now restored. Runtime structures/hooks in
common/engine/ec_switch_matrix.h and common/runtime/ec_board.c have clear
original common-source counterparts and already retained the full notices.

Calibration/actuation modules adapt scaling and runtime-field setup and extend
the extremum algorithm. Attribution now explicitly limits Cipulot's notice to
those adapted portions. The manual transactions, 16-scan qualification,
enabled-key mask, explicit CRC record format, USB revisions, debounce, VA-first
initial activation, median filtering and Deepest safeguards are later EC42 work,
not all original Cipulot features.

The simple console view retains Cipulot's original interface and notice, but
its output implementation was rewritten. The original full GPL notice was
restored without claiming that the old VIA protocol remains active.

### Other origins and independent work

Matrix acquisition/lighting/keymaps match its original local source, not a
wholesale Cipulot acquisition copy. Shared processing does not make Matrix's
hardware driver Cipulot-authored. Its original unheaded files remain unheaded
rather than being assigned a guessed copyright or SPDX.

The ASTRO RTC handoff is a new QMK hook implementing observed hardware facts;
its copyright/notice is already explicit. No original bootloader body is copied.
The 0413 guards and portable storage wrappers are project policy around QMK /
ChibiOS, not rewrites claiming authorship of the underlying journal or HAL.

The GUI has a confirmed Cap-Util lineage: all **16** hashes recorded in its
PROVENANCE.json match the still-present xwhatsit/util/util sources. The older
reverse-engineering path is historical. Existing Purdea Andrei headers remain.
C++ EC42 codecs, runtime layouts and settings models are later application work;
KLE/Vial Python is a format/behavior reference, not evidence of source copying.
No Cap-Util icon, AppImage, font or third-party DLL was imported into the source.
The screenshot is identical to the external app's screenshot, but its precise
image creator has not been independently established.

The framework itself is currently untracked in this repository, so there is no
complete per-file Git authorship chain for new EC42 work. Development snapshots,
source matches and recorded reports are used with explicitly stated confidence;
no exclusive ownership is inferred from a maintainer name or an AI-generated
comment. The original Cipulot tracked history provides much stronger lineage.

## 4. Missing notices and corrections

Fifteen original files received comments/notices/documentation changes:
- common/runtime/decision.c: restored Cipulot full notice and scoped modification note.
- common/engine/calibration.c and actuation_control.inc: original notice for adapted
  scaling/state-machine portions, with later EC42 work distinguished.
- common/engine/calibration_console.c: restored full original GPL/copyright block.
- common/runtime/ec_board.c and common/engine/ec_switch_matrix.h: dated modified-work notices.
- ec_prox/acquisition.c, config.h and vial_alec/keymap.c: dated modified-work notices.
- class0413_ec/matrix.c: dated modified-work notice.
- class0413_ec/modules/native_music.c: explicit reference to included Jack Humbert source.
- me_ec_uf2/ld/me_ec_boot_STM32F411xE.ld: changed-file notice preserving Apache terms.
- util/tests/fixtures/legacy_v28_harness.c: modified-work notice preserving embedded notices.
- README.md and util/NOTICE.md: prominent and component-specific attribution.

New artifacts include LICENSE.md scope, THIRD_PARTY_NOTICES.md, full license texts,
GUI dependency notices/texts, and a REUSE-style sidecar for the generated JSON
scanner template's adapted EC type declarations. Original JSON data is unchanged.
License-text sources and hashes are in LICENSE-TEXT-SOURCES.json. GNU license
texts are copied verbatim; their filenames do not themselves grant a license
to unheaded program files.

No original author was replaced with Alectardy98, no copyright year invented,
and no upstream source or original keyboard directory was changed. Existing
GPL-2.0-or-later wrappers were not gratuitously retagged GPLv3. GUI SPDX v3 with
retained Cap-Util v2-or-later notices is an allowed later-version combination,
not evidence that the original v2 permission should be deleted.

## 5. Compatibility and unresolved questions

Known GPL-2.0-or-later contributions can select GPLv3 when combined with
GPL-3.0-or-later work. GPL-2.0-only cannot simply be upgraded this way. Apache-2.0
is compatible with GPLv3, and original Apache notices/modified-file notices must
remain. The ChibiOS RT kernel's v3-only terms constrain a combined artifact's
license version; they are not silently converted to an or-later grant.

Sources: [GNU license compatibility FAQ](https://www.gnu.org/licenses/gpl-faq.en.html#AllCompatibility),
[GNU compatibility guide](https://www.gnu.org/licenses/quick-guide-gplv3.html),
[Qt licensing](https://doc.qt.io/qt-6/licensing.html),
[HIDAPI BSD option](https://raw.githubusercontent.com/libusb/hidapi/master/LICENSE-bsd.txt).
Those general rules do not establish missing grants or ownership.

**Release blockers:**
1. Obtain source provenance/compatible license confirmation for meaningful unheaded
   Matrix imports. A Git importer or hardware vendor string is not sufficient.
2. Exclude stock firmware/bootloader dumps from public redistribution, or obtain
   permission and any required matching source. Private recovery evidence is
   preserved, not erased. Git ignores these binaries by default, but a directory
   tarball can still accidentally include them.

**Review items:**
- time_math.h algebraically reproduces ChibiOS rounding semantics. Determine
  whether any protectable copied expression is present before asserting a
  broader file license; existing SPDX was preserved, not guessed away.
- **42** code/build files lack individual license notices; many are project-written
  scripts/config stubs, not necessarily third-party copies. **91** matrix rows
  have NOASSERTION including data/docs. Missing individual notices are not proof
  of infringement, but no new license or ownership grant was invented for them.
  Confirm original owner/license intent before blanket SPDX normalization.
- Exact screenshot creator/grant remains unasserted.
- A future bundled AppImage/shared-library package needs a packaging-specific
  inventory of Qt plugins, fonts, system libraries, their source and obligations.
  GUI-DEPENDENCIES.json records **47** actual dynamic dependencies/package metadata;
  package-wide license lists are not treated as file-specific grants.

Redistributed firmware needs matching complete corresponding source, QMK and
submodule revisions, relevant build/install/packaging scripts and any applicable
installation information. A keyboard-folder-only source archive is insufficient.
GUI corresponding source includes the shared common/interfaces/ec42_protocol.h;
a util-only copy omitting it is also incomplete. Runtime/newlib aggregate texts
and compiler exceptions do not substitute for original per-file runtime terms.
This was not a new legal audit of every upstream QMK vendor file.

## 6. License/SPDX validation

`python3 development/alectardy98_ec/licensing/check_notices.py` passes established
notice preservation, supported SPDX identifiers, copied-text presence and GUI
license-copy consistency. Results are in LICENSE-CHECK.json. This is a targeted
source checker, not a claim of complete REUSE certification for every file.

`.../check_notices.py --strict` intentionally exits **2** because recorded release
blockers remain. OPEN_ISSUES.json drives that status. No safety or license check
was bypassed to produce a green clearance label.

COMMENT-ONLY-CHECK.json proves identical non-comment/preprocessing tokens for
all changed runtime source and no changed original serialized/test data. This
is stronger than relying only on a successful build after header editing.

## 7. Builds and regression results

System Python 3.14; standard make commands; no core AST patch or host shim.
All six primary targets passed final clean builds in an isolated source copy.

| Target | Application span bytes | Free flash | ELF and install image |
|---|---:|---:|---|
| class0413_ec_vial | 62,320 | 3,216 | Identical |
| class0413_ec_vial_alec | 63,720 | 1,816 | Identical |
| ec_prox_vial | 117,428 | 13,644 | Identical |
| ec_prox_vial_alec | 120,588 | 10,484 | Identical |
| me_ec_uf2_vial | 81,548 | 49,524 | Identical |
| me_ec_uf2_vial_alec | 83,348 | 47,724 | Identical |

Prox span includes its EEPROM hole; every Prox gap remains FF. UF2 family
0x57755A57 and existing application ranges were rechecked; no bootloader/storage
payload appeared. Full ELF/install-byte identity means flash/RAM use, settings
compatibility, scanner/calibration/RT, native audio/lighting/haptics and Vial
behavior are unchanged. No physical retest is needed merely for these notice
edits; no hardware verification is newly claimed.

GUI rebuilt from the actual framework source with the matching Debug configuration.
Fresh normal tests: **13/13**; fresh ASan/UBSan tests: **13/13**. The current CMake
conditionally omits framework_port_live when the matching canonical development
port-harness.py is absent; no test or CMake logic was weakened. The retained
validated suite including that test also passed **14/14 normal and 14/14 sanitizer**.
The new GUI passes --smoke-test offline. The missing canonical generator is
pre-existing development tooling debt, not repaired during this licensing task.

The GUI full ELF differs only in `.note.gnu.build-id`, `.debug_info`, `.debug_line`
and `.debug_line_str`. Every allocated section except the build-ID note has
identical bytes/addresses/sizes, including .text, .rodata, data and relocations.
The cause is new build-directory DWARF paths and the resulting GNU build ID.
This is not a GUI behavior change. GUI-SECTION-COMPARISON.json records the evidence.

New validation GUI SHA-256: `ed2a4f8376b2e1ecd5035b6d7ca340a5c073f487bc58b467d6900654fdb48326`.
Original known-good GUI remains untouched; its SHA-256 is
`49c9f02b7d07aa4c3522c7a00785a3905f3c11c2fc583167168047975cfd82ab`.

Firmware hashes and equivalence: FIRMWARE-COMPARISON.json and SHA256SUMS.
Working artifacts were not overwritten. Full build/test logs, source backup,
GUI validation executable and compiled firmware are under:
`/home/alec/firmware-analysis/analysis/ec42_licensing/20261021/`.
GUI: `gui-build/qmk-ec-util`; firmware: `build-tree/.build/` and the normal
isolated Prox clean-install output folder. No flash/erase/HID/EEPROM operation ran.

## 8. Final README acknowledgment (verbatim)

```markdown
## Attribution and licensing

**Cipulot's electrocapacitive keyboard firmware is a substantial foundation of
this framework.** Acquisition and matrix scanning, resting-baseline handling,
EC runtime structures, calibrated threshold scaling, and extremum-based
press/release behavior were adapted from
[Cipulot's QMK EC implementation](https://github.com/qmk/qmk_firmware/blob/master/keyboards/cipulot/common/ec_switch_matrix.c)
and its [EC data structures](https://github.com/qmk/qmk_firmware/blob/master/keyboards/cipulot/common/ec_switch_matrix.h).
Original Cipulot copyright notices and GPL-3.0-or-later terms are preserved.
EC42 transactions, incremental qualification, explicit storage records and the
later VA-first/Deepest controls are subsequent framework work. This credit does
not imply endorsement or collaboration by Cipulot.

QMK/TMK/Vial, ChibiOS, the original board implementations, and Purdea Andrei's
Cap-Util also contributed code or interfaces. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md),
[licensing scope](LICENSE.md), and the repository's
[full provenance audit](../../development/alectardy98_ec/licensing/REPORT.md).
Unheaded Matrix imports and stock `DUMP/` redistribution permissions remain
unresolved; no new blanket license or public-release clearance is asserted.
```

## 9. Handoff

The evidence-backed attribution corrections are complete and technically neutral.
Do not describe the entire tree as legally cleared while OPEN_ISSUES.json contains
unresolved grants. Resolve those source/permission questions with the relevant
rights holders, keep original notices, and update the matrix and strict audit
status based on evidence rather than introducing a blanket replacement license.
