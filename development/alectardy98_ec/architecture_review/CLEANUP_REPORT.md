# EC42 v2.9 cleanup report

Date: 2026-10-21. Scope: the three `keyboards/alectardy98_ec/` ports and the canonical GUI in `util/`. **No hardware was opened, flashed, reset or written. Original Cipulot, Matrix and MM Studio source directories and QMK core remain unchanged.**

## Outcome

One supported EC42 version (2.9), one current shared decision/calibration/storage engine, one explicit record format and one current-source firmware/GUI test harness. Removed the obsolete alternative RT path and redundant runtime arrays, old GUI codecs/telemetry/settings workflow and frozen historical engine fixture. All six primary clean firmware builds and the current normal/sanitizer suites pass.

Valid v2.9 records and settings remain compatible; this release does **not** require recalibration or an EEPROM reset. Invalid, incompatible or interrupted records still fail closed. Hardware-dependent validation of these new binaries remains necessary before designating them physically stable.

## 1. Controlled stages and baseline

1. Preserved the full starting framework/development tree, including the existing working GUI executable, in `source-before.tar.gz` under the analysis directory below. Existing known-good firmware/ELF/map and GUI analysis releases were not overwritten.
2. Established current v2.9 baseline tests **before firmware edits** using rebuilt host programs containing the actual shared C engine and each board's scan loop. Qualified partial/incremental capture, VA/Deepest, RT, persistence, boot recovery and torn saves passed for all three.
3. Verified that the firmware source/configuration files in Git HEAD at task start matched the six-target licensing build snapshot (`baseline-source-check.json`: no mismatches). Used its exact ELF files for before measurements; did not substitute padded image length for linked content.
4. Removed dead RT/runtime code and measured six clean builds. Intermediate results are `stage-rt-sizes.json`: 0413 saved 68/60 bytes of span; Prox 76/108; Matrix 280/272 (vial/vial_alec). RAM reductions were already final.
5. Removed old GUI codecs and hidden obsolete controls, then ported worker/UI tests to current firmware. Retired the frozen engine and old conversion-only tests. Normal/sanitizer tests passed after updates.
6. Removed the unused old 0413 EC41 status query, retained the current GUI's native boot request, and performed the final six clean builds and complete regression runs. This saved a further 20 bytes in both 0413 targets.

The first audit `REPORT.md` is intentionally retained as the **pre-cleanup** assessment; this report supersedes its current-code/test-path descriptions.

## 2. Measured firmware results

Application span is highest flash load end minus application/vector base. Prox spans include the EEPROM address hole; its meaningful linked bytes are reported separately. BSS is the actual `.bss` section, **not** GNU size's aggregate that includes the linker heap reservation. Initialized `.data` is unchanged. Stacks reserve another 3,072 bytes in each target.

| Target | Span before → after (bytes) | Flash recovered | Free after | Data before → after | BSS before → after | RAM recovered |
|---|---:|---:|---:|---:|---:|---:|
| class0413_ec:vial | 62,320 → 62,232 | 88 | 3,304 | 1,784 → 1,784 | 18,748 → 18,224 | 524 |
| class0413_ec:vial_alec | 63,720 → 63,640 | 80 | 1,896 | 1,788 → 1,788 | 18,748 → 18,224 | 524 |
| ec_prox:vial | 117,428 → 117,352 | 76 | 13,720 | 1,520 → 1,520 | 15,320 → 14,944 | 376 |
| ec_prox:vial_alec | 120,588 → 120,480 | 108 | 10,592 | 1,520 → 1,520 | 15,332 → 14,952 | 380 |
| me_ec_uf2:vial | 81,548 → 81,268 | 280 | 49,804 | 1,612 → 1,612 | 18,656 → 18,176 | 480 |
| me_ec_uf2:vial_alec | 83,348 → 83,076 | 272 | 47,996 | 1,612 → 1,612 | 16,552 → 16,072 | 480 |

Final linked flash section totals (excluding address holes/alignment):

| Board | vial | vial_alec |
|---|---:|---:|
| class0413_ec | 62,230 | 63,638 |
| ec_prox | 85,062 | 88,190 |
| me_ec_uf2 | 81,266 | 83,074 |

The physical checks passed: 0413 payload only `0x08010000–0x0801FFFF`; Matrix payload only sector 5 `0x08020000–0x0803FFFF`; both UF2s use family `0x57755A57`, correct block numbering and 256-byte payloads. Prox vectors/text remain at `0x08000000`/`0x08008000`, and the complete `0x4000–0x7FFF` BIN gap is FF. Every ELF flash-load section matches its flashable image. Initial SP/reset vectors were checked.

Flash improvements are modest because LTO already removed much of the unreachable alternative RT implementation. The primary measurable benefit is runtime RAM, plus reduced source/GUI maintenance. No large unmeasured saving is claimed. Binary hashes change; address relocations and runtime object layout changes mean these are not byte-identical releases. Wire/record/decision equivalence is tested separately below.

## 3. Removed firmware code and state

Common paths are relative to `keyboards/alectardy98_ec/`.

- `common/runtime/decision.c`: removed original pre-EC42 mode-1 RT fallback. The preceding mode-1 branch always dispatched to the v2.9 filtered `ec_rt_key()` because `ec_va_dynamic()` returned a constant true. Mode 0 fixed actuation remains intact.
- `common/engine/ec_diagnostic.c`: removed the corresponding unreachable old RT-boundary calculation. Snapshots still publish the actual v2.9 boundaries and logical states.
- `common/engine/ec_switch_matrix.h`: removed runtime arrays `rescaled_mode_1_initial_deadzone_offset`, `rescaled_mode_1_actuation_offset`, `rescaled_mode_1_release_offset` and `bottoming_calibration_starter`, plus their writes in calibration/load helpers. Their old readers were unreachable or absent. **The persisted struct is unchanged.** Nominal removed fields total 5 bytes per matrix position; measured BSS includes alignment/LTO effects.
- Removed `ec_va_dynamic()`, constant `va_active` and constant-only conditions. Actuation readback still returns active=1; invalid/uncalibrated keys are still disabled separately.
- Removed `EC_CAL_MANUAL`/`EC_CAL_AUTO` old method constants and comparisons against impossible current auto method. Current working method 3 and persisted tag 0x42 remain.
- Removed unused `ec_calibration_request_reset()` declarations/no-op definitions and the unused `logical` argument to `ec_cal_sample()`. The real logical state passed to diagnostics is retained.
- Removed unused descriptive-only `common/interfaces/ec_hardware.h`. No live source included it; adapters remain direct functions/configuration.
- Removed the unused EC41 status query from 0413. **Retained EC41 boot envelope command 7** because the current v2.9 GUI still sends it for native recovery. This is a current operational command, not admission of older firmware, calibration compatibility or a migration engine. Changing its envelope would change current USB behavior, outside this cleanup.
- Removed unlinked `class0413_ec/modules/native_music.c` and `music_compact.c`; neither was listed in active SRC nor referenced by firmware. Music Mode remains disabled; native Audio, Clicky, lock songs and solenoid code remain untouched. Removed their obsolete core-LUT-dependent frequency test portion; preserved actual time-conversion and impossible-position equivalence tests. Copyright/license evidence remains in the before snapshot and historical licensing report; original headers of retained derived files were not replaced.

**Acquisition, GPIO sequencing, waits/atomic regions, baseline averaging, qualification algorithm, thresholds, RT median/extremum/floor behavior and storage write algorithms were not redesigned.** 0413's capture-time logical matrix behavior remains distinct from the other boards; it was not silently unified.

## 4. Removed GUI legacy support

- `src/settings.cpp`: removed the 219-byte 0413 radix decoder, embedded absent-cell metadata decoding and earlier calibration/RT conversion modes. Only explicit `17+3*N` records with valid CRC/flags/settings are interpreted. Empty local record cache is allowed while initializing; malformed nonempty records are rejected.
- `src/protocol.cpp/.h`: HELLO accepts minor 9 only; descriptor is mandatory. Removed minor 0/5/8 telemetry sizes/flag interpretations, older record-length fallback and old capability hook helper. The simulator now emits the v2.9 telemetry shape with explicit simulation/no-write capabilities. Read-only descriptor requests remain allowed without a write capability.
- `src/device.cpp`: removed 0413-v2.8 admission exception, old backup/tag/CRC path, old readback/partial-merge branches and the obsolete `settings` action targeting reserved `0x26`.
- `src/livepanel.cpp`, `mainwindow.cpp/.h`: removed hidden old APC/mode/deadzone controls, hidden Apply & Save settings button, old slider mapping and old RT explanation, unused pending settings cache and old-version branches. Existing visible VA controls, RT checkbox/sensitivity and **independent advanced press/release movements** remain immediate, read back and debounced-saved. Manual calibration still has explicit Review/Apply & Save, separate from settings.
- Retired unused old settings-range/validation helper APIs and their historical conversion-only tests. Actual current GUI controls and firmware validation are exercised by current worker/scanner tests instead.
- The GUI rejects earlier/future unsupported minor versions before descriptor/write commands. Unsupported firmware is a clear error; it is not silently interpreted as v2.9.

The GUI remains compatible with the same current UID/layout profiles and dynamic geometry. No new sensor assumptions, protocol version or storage layout were introduced.

## 5. Current-source test infrastructure

Restored reviewed acquisition/guard/boot/Vial tests from existing archives and adapted paths to the present checkout. Recreated the missing source generator around the actual current modules. There is **one** `development/alectardy98_ec/port-harness.py`, consumed by both Python scanner tests and Qt subprocess/worker tests. The three JSON models contain host IO, ADC/mux injection and fault hooks, not frozen copies of firmware.

The harness reads current headers, `.inc` code, calibration/diagnostic/runtime code and actual board scan loops each time it builds. 0413 also compiles the real suspend hook body and board integration. ADC conversion timing, physical GPIO and flash controller are modeled, not executed. Separate acquisition tests compare the original Prox/Matrix driver and exercise actual startup averaging. Separate tests execute real QMK wear leveling with host NOR and current sector guards. 0413 split record IO is tested independently against a 4 KiB host store and Vial canaries; the general scanner's record store is a host abstraction, not a claim to exercise flash physics.

### Retired active files

- `util/tests/fixtures/legacy_v28_harness.c`
- `util/tests/fixtures/scanner_harness_templates.json` and its license sidecar (history retained externally)
- `util/tests/firmware_harness.py`, `packed_record.py`
- `util/tests/test_variable_actuation.py`, `test_explicit_calibration.py` (old codec/geometry expectations; current behavior covered by replacement suite)
- Historical-only slots in `test_settings.cpp`; other current map/capture/polling tests retained.

`test_live.cpp` was **ported, not discarded**: current firmware codec, corruption, backup, discovery, disconnect/reconnect, generation checks, manual transactions, failed readback, partial sessions, GUI sliders and boot recovery remain active. `test_ports_live.cpp` now runs all three freshly compiled boards, not stale generated binaries.

### Added/restored active coverage

- `tests/test_ports.py`: all three actual scan loops; one-key/incremental calibration, long sessions, retries/cancel/disconnect, noise spikes, shallow/middle/Deepest, global/per-key/reset, RT 4/16/32, strict equality crossings, smaller span/baseline changes, disabled keys, telemetry, CRC, restore, reboot/debounced saves, recovery with disabled keys.
- 340/250/313 interrupted-save write prefixes (0413/Prox/Matrix): valid old/new or fail-closed invalid state, never unchecked activation.
- `test_record_io.py`: every valid read range in the split 219+113 adapter; macro conflict, bad bounds/storage health; unchanged Vial/core canaries.
- `test_port_storage.py`, `test_0413_storage.py`: real QMK NOR replay/consolidation with modeled flash, geometry/WRP/bounds/mass-erase rejection, health/error latching, protected canaries.
- `test_acquisition.py`: original acquisition equivalence and startup averages; `test_astro_bootloader.py`: actual Matrix hook against registers; `test_prox_lighting.py`: current layer overlay ownership/behavior.
- `test_matrix_vial_id.py`: actual compiled schema offsets for both keymaps; pinned IDs unaffected by generated build dates; Vial keymap/macro canaries and reset paths.
- Qt `test_boards.cpp`: explicit rejection of unsupported minor versions and old record shape before writes; current descriptor identity/health behavior retained.
- Current time/matrix-mask equivalence and system Python matrix metadata tests.
- `verify-builds.py`: mandatory six-image existence, load-section equivalence, vectors, protected boundaries, UF2 metadata and FF-gap validation. This replaces old checkers that could silently do nothing if artifact glob paths were empty.

CMake now errors if current harness tooling is absent, instead of silently dropping the all-board suite. The GUI's `CMakeLists.txt` had been hidden by QMK's global ignore pattern; a local exception makes it visible to Git. Test interpreter selection defaults to the environment's `python3`; final runs explicitly selected `/usr/bin/python3` (3.14.7). An initial CMake auto-selection of local 3.13 lacked jsonschema; this was an environment selection failure, fixed without QMK core changes or a compatibility shim.

## 6. Results and reproducible commands

- Six **clean** normal make builds: success, system Python **3.14.7**, no warning filters/PYTHONPATH patches. Commands are the requested `make alectardy98_ec/<board>:vial` and `:vial_alec`.
- Final normal CTest: **20/20 passed, 36.33 s**.
- Final ASan/UBSan CTest: **20/20 passed, 48.95 s** (Qt leak checks disabled; memory/UB checks enabled).
- Full pre/post v2.9 trace comparison: all scanner-suite request/reply bytes, including record CRC/backup data and interrupted-save outcomes, matched the pre-cleanup host binaries across all three boards. The removed unused EC41 status query is deliberately excluded from equivalence; boot command remains tested.
- Both Matrix compiled Vial layouts/IDs: passed after final builds.
- All six payload/ELF/bounds checks: passed.
- `git diff --check`: clean. Tracked upstream math/audio/send-string match HEAD; no upstream/core/original board changes.

Repository entry points:

```sh
./development/alectardy98_ec/build-baselines.sh
./development/alectardy98_ec/test.sh
```

These are software-only; build script cleans generated firmware outputs, so preserve release artifacts first. `test.sh` builds/tests current GUI and scanner programs normally and with ASan/UBSan, then checks Matrix layouts and firmware packaging. To verify isolated firmware builds, set `EC42_BUILD_TREE=/path/to/build-tree` for the final artifact/layout checks. Direct commands used for this task:

```sh
ctest --test-dir /home/alec/firmware-analysis/analysis/ec42_cleanup/20261021/gui-build --output-on-failure
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /home/alec/firmware-analysis/analysis/ec42_cleanup/20261021/gui-sanitize --output-on-failure
python3 development/alectardy98_ec/verify-builds.py /home/alec/firmware-analysis/analysis/ec42_cleanup/20261021/build-tree
```

Remaining build notices: intentional Vial insecure mode; Matrix's existing ChibiOS `~` applied to boolean `palWriteLine` argument warning. No AST deprecation warning appeared. Neither notice was suppressed or "fixed" by changing hardware behavior; changing Matrix acquisition expressions is outside this safe cleanup.

## 7. Record, storage and identity compatibility

| Board | Canonical size / magic | Placement preserved |
|---|---|---|
| 0413 | 332 / A105 | 219 bytes at logical 37–255, 113 at 3983–4095; sector 2 backend |
| Prox | 242 / A975 | Native keyboard record, sector 1; 8 KiB backing / 4 KiB logical |
| Matrix | 305 / A996 | Native keyboard record, sector 7; 128 KiB backing / 4 KiB logical |

`eeprom_ec_config_t`, tag 0x42, CRC domain, metadata placement, per-key enabled/qualified/retry flags and override bytes are unchanged. Runtime tag 3 remains required for working proposals and active RAM; it is **not** a legacy record migration. The LE-native/BE-wire conversion is still necessary. Pinned Vial build IDs, USB names/VID/PID, UIDs, keymap/macro addresses, bootloaders and flash layout were not changed.

CRC/readback/quarantine and QMK wear leveling remain mandatory. A torn record is rejected and may require host-backup restoration/recalibration: this is not a claim that one logical record guarantees a durable old copy through every power cut. All recovery checks remain intact. No startup EC record overwrite or physical EEPROM operation was introduced.

## 8. Code deliberately retained

- Explicit capture/review/ready/applied states, candidate and rollback record: distinguish evidence, approved proposal, active RAM and verified persistence.
- Saturating counts and 16-scan block extrema: prevent isolated spikes/unlimited-session overflow, not historical complexity.
- Median RT scratch arrays shared with capture: valid reuse of mutually exclusive modes; no extra acquisition or changed timing.
- Full frozen telemetry snapshot and raw/low/high cache: coherent paginated reads and real logical state. Latest/candidate data serve release and transaction checks; merging them needs a lifecycle proof not available here.
- Independent packet/session/revision/range/released checks at external and persistence boundaries.
- Exact v2.9 persisted header fields, even names inherited from mode-based code: changing/removing bytes would violate current records.
- Split 0413 IO and macro-tail scan; Prox/Matrix geometry/protection guards; current NOR health interposition; native actuator quieting.
- Direct board acquisition, 0413 capture logical state, Matrix opamp/settling/ASTRO, Prox FF packaging, pinned Vial IDs and Python JSON matrix metadata.
- Single-TU `.inc` structure: avoids an unmeasured 0413 size regression; no runtime callback framework added.
- Archived alternate keymap layouts remain source references/current-engine targets, not earlier EC42 implementations. Only six primary release targets were clean-built here; no historical protocol engine was retained to support them.

Deferred technical debt: dense transaction dispatch formatting, CRC helper duplication with distinct domains, pending-task candidate copying, and possible smaller telemetry/state caches. None was speculatively rewritten. Acquisition loop micro-optimizations need actual timing evidence. Flash savings alone are not sufficient reason to weaken recovery or strip native features.

## 9. Documentation and provenance

README/TESTING/GUI protocol instructions now describe current-source v2.9-only testing and no old settings path. Historical source audit, old GUI validation JSON and RT investigation moved to this development report directory with `HISTORICAL_` names. Historical licensing/import manifests remain historical records, not current build manifests. Cipulot's GPL notices and other authors' retained headers/required license texts are preserved. No new ownership or licensing grant was invented.

See `CHANGED_FILES.txt` for the file inventory and `TRACKED_SOURCE_DIFF.patch` for tracked edits; new tooling/CMake source is present directly in the checkout. `sizes.json` and `SHA256SUMS` accompany this report.

## 10. Artifacts, backups and physical retest

Analysis/build/log directory:
`/home/alec/firmware-analysis/analysis/ec42_cleanup/20261021/`

- `artifacts/<board>/<keymap>/`: final ELF/map plus flashable UF2 (0413/Matrix) or `ec_prox.clean-install.bin`; UF2 payload BIN copies also included.
- New GUI: `artifacts/gui/qmk-ec-util` (matching Debug build in `gui-build/qmk-ec-util`).
- `SHA256SUMS`: all packaged artifact hashes; this report directory holds a copy.
- `source-before.tar.gz`: complete before snapshot; original known-good GUI remains at `keyboards/alectardy98_ec/util/build-universal/qmk-ec-util`, verified unchanged SHA-256 `49c9f02b7d07aa4c3522c7a00785a3905f3c11c2fc583167168047975cfd82ab`.
- Matching unchanged baseline firmware remains in `/home/alec/firmware-analysis/analysis/ec42_licensing/20261021/build-tree/`.

Owner-performed physical validation (not executed):

1. Export current Vial settings and a valid v2.9 EC backup; keep the matching pre-cleanup firmware and independent native boot recovery. Use a second keyboard/mouse.
2. Use only the board's established update method and matching artifact. **Prox clean-install BIN at 08000000 can erase/reset settings depending on the programmer's erase policy**; it is not a calibration-preserving update merely because its gap is FF. Use the separately documented selective method when preserving physical sector 1. Matrix ASTRO and 0413 TinyUF2 payloads exclude their storage regions. Do not apply Prox erase procedures to other boards.
3. On boot confirm saved calibrated keys, overrides, RT and keymaps load without migration/reset; normal typing, disabled keys and native lighting/audio/haptics are unchanged.
4. Test shallow/middle/Deepest, per-key reset and override, RT off/three sensitivities, initial crossing and partial-travel cycles; release completely and check for stuck/repeated events. Check live logical colors/graph against typing.
5. With explicit calibration approval, add/recalibrate a single key, cancel/retry another, save and power-cycle. Confirm unrelated keys/keymaps/macros remain intact. Check automatic setting persistence and reconnect/suspend behavior.
6. Confirm current GUI/QK_BOOT reaches native recovery. Matrix must retain ASTRO; no generic TinyUF2 entry assumptions.

Compilation/host traces do not validate analog timing, physical power-loss behavior or lighting. No physical recalibration, flash or EEPROM write was performed for this cleanup.
