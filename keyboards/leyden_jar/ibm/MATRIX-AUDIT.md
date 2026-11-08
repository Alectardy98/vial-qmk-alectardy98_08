# IBM matrix audit · 2026-11-08

## Mistake

The first generator treated `{7,6,5,4,2,0,1,3}` as a generic xwhatsit-to-Leyden Jar
row map. In native `common.c` that permutation belongs specifically to the
F77/F62 legacy **CONTROLLER_IS_XWHATSIT_MODEL_F** compatibility case. The original
IBM definitions being imported select **UNIVERSAL** controllers or **TEST**,
which have different logical-row transforms in `keyboards/xwhatsit/capsense.h`.
Reversing the same legacy permutation for TEST was also incorrect.

The error affected all 29 generated row tables and their dependent pad masks.
The Vial JSON geometry and keycodes were preserved correctly; those files do not
specify electrical connector order. Compilation and bounds tests passed because
a wrong permutation still contains valid array indices. The initial simulated
scan fixture used the assumed map on both sides and did not independently
validate that assumption against source-controller macros or physical keys.

## Evidence and correction

A controlled physical FAT capture established:

| Physical key | Source QMK position | Measured controller position |
|---|---|---|
| Numpad minus | (0,15) | (7,15) |
| Top-row Backspace | (1,12) | (6,12) |
| 1 | (2,2) | (5,2) |
| Q | (3,2) | (4,2) |
| A | (4,3) | (3,3) |
| Main Enter | (5,11) | (2,11) |
| Left Shift | (6,2) | (1,2) |
| Left Ctrl | (7,2) | (0,2) |

The corrected FAT row map is `{7,6,5,4,3,2,1,0}`. Regenerating its pad mask also
excludes controller `(0,10)`, an unused high-signal pad that the wrong mapping
had treated as a key. All eight controller sense channels were covered by these
physical captures. This validates the channel translation for the FAT assembly;
it does not establish every other IBM connector's wiring.

The generator now compiles each original board's `config.h`, `universal/config.h`
and actual `capsense.h`, evaluates `CAPSENSE_KEYMAP_ROW_TO_GPIO_ROW`, and records
`MATRIX_CAPSENSE_ROWS`, controller family, physical columns and shift-register
columns. The correct family is selected before interpreting row numbers:

| Source case | Evaluated logical rows | Port result |
|---|---|---|
| Eight capacitive rows, UNIVERSAL | 7–0 | Native channels 7–0 |
| Four capacitive rows, UNIVERSAL | 3–0 | Native channels 3–0; remaining entries unused |
| Eight rows, TEST | 0–7 | Native channels 0–7 |
| Extra direct-key row | Not capacitive | No capacitive mapping; target remains blocked |
| Sixteen sense rows | Exceeds native connector | Target remains blocked |

All 29 generated row tables and their populated-pad masks were corrected.
Re-evaluating the actual column macros found no column-order changes. The native
F77/F62 compatibility code itself was not changed. The 275 archived-source IBM
files under `keyboards/xwhatsit/ibm` remain byte-for-byte unchanged.

## Regression gate

```sh
python3 keyboards/leyden_jar/ibm/tools/port_ibm.py
python3 keyboards/leyden_jar/ibm/tools/check_matrix_maps.py
# Build the compatible targets, then inspect their actual firmware tables:
python3 keyboards/leyden_jar/ibm/tools/check_matrix_maps.py --compiled
```

The checker evaluates source transforms, compiles the generated native config,
checks the mask against original populated positions, and optionally extracts
row/column/mask arrays from every compatible UF2 using its ELF symbols. A FAT
anchor records the independently measured eight-row result. Unsupported
controller families require an explicit translation instead of a fallback.

Results: 29 source/config checks passed, all 23 compatible firmware variants
rebuilt, all 23 compiled map tables checked, all 38 Vial geometries preserved,
and all 13 unsupported target guards still reject their builds explicitly.

## FAT hardware results

Personal calibration remains confined to FAT `vial_alec/calibration.h`; it does
not enter standard `vial` or other boards. The original single-bin threshold
392 intersected released signals spanning 377–401 on Alec's modified PCB. Native
seven-bin calibration and a +16 offset address the measured spread and smaller
neighbour responses without replacing the scanner or changing GPIO timing.

- 81 recorded held-key frames replayed through native C: each produced its
  expected single key. ASan/UBSan tests also exercised all 92 declared pad paths.
- Full physical sweep: 89 distinct matrix presses, each with its corresponding
  release; no duplicate presses, overlaps or keys left down.
- Linux capture: 102 key-down and 102 key-up events, including macros. The 717
  value-2 auto-repeat events occurred while keys were held and are distinct
  from new matrix presses. No unmatched host release or stuck host key occurred.
- Declared all-options positions not observed: `(2,11)`, `(4,10)`, `(6,10)`.
  Their presence depends on the actual assembly/layout; this is not proof that
  a fitted key failed.
- 187 idle snapshots were empty; eight native sweeps had at least 12 DAC counts
  of released margin. All seven thresholds acknowledged/read back; invalid
  threshold 1024 was rejected. Output disable/enable and software reboot passed.
- Software BOOT → UF2 copy → normal enumeration passed. The final source build
  was flashed and 12 additional idle checks were empty. Keymap and layout-option
  readback matched the pre-flash values. No explicit EEPROM erase was used.
- Compiled Vi_al 1.0.11 exercised all four add-on pages on the connected FAT,
  including seven bin rows and 144 physical signal values, with persistent
  writes blocked during that GUI check.

Other controller assemblies, alternate physical layouts, chords, LEDs/solenoid
acceptance and a full cold-power-cycle campaign remain unverified. A ~19 ms
physical snapshot query and ~82 ms multi-command diagnostic snapshot are host
operation durations, not measured matrix scan or input latency. Full native
DAC sweeps block normal scanning while they execute.
