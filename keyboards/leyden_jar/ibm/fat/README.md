# Original IBM Model F AT · Leyden Jar Rev 3

Requested personal target:

```sh
make leyden_jar/ibm/fat:vial_alec
```

This preserves the AVR FAT `vial_alec` keymap, custom keycodes and Vial layout
options, using the native Leyden Jar RP2040 scanner and IO-expander LEDs.
`vial` and `vial_finley` are also available. This personal keymap has now been flashed and checked on Alec’s modified
assembly, including an 89-key sweep. Other assemblies remain unverified.

## Declared controller translation

This table describes **the firmware's expected logical-row connection**. Verify
it against the actual IBM ribbon/adapter before soldering; QMK row numbers are
not physical keyboard rows. J2 numbering comes from the Rev 3 schematic, not a
photograph of your board. Confirm connector orientation separately.

| QMK row | Controller comparator row | Rev 3 J2 analog input pin | RP2040 comparator-output GPIO |
|---:|---:|---:|---:|
| 0 | 7 | 10 | GP7 |
| 1 | 6 | 9 | GP6 |
| 2 | 5 | 8 | GP5 |
| 3 | 4 | 7 | GP4 |
| 4 | 3 | 6 | GP3 |
| 5 | 2 | 5 | GP2 |
| 6 | 1 | 4 | GP1 |
| 7 | 0 | 3 | GP0 |

QMK columns 0–15 map to controller columns 0–15 and J2 pins 13–28 respectively.
Controller columns 16–17 are scanned by the native implementation but have no
populated pads in this target. J2 pins 1–2 and 11–12 are ground; pins 31–33 are
LED0/LED1/LED2. Do not connect analog sense pads directly to RP2040 inputs:
the controller comparators are part of the signal path.

Native LEDs retain Num Lock→LED0, Caps Lock→LED1, Scroll/FN→LED2. The personal
keymap's existing Num/FN behaviour remains in its `led_update_user` implementation.
Check the actual LED header wiring and polarity when assembling the controller.

Release all keys during startup automatic calibration. Standard `vial` now uses the corrected compiler-derived mapping and retains
its single-bin +10 calibration default; it has not been
physically validated on an unmodified FAT. Alec's modified PCB uses a separate
`vial_alec/calibration.h` calibration override:

- The board-level map was checked on all eight physical rows: QMK row → controller row is
  `{7,6,5,4,3,2,1,0}`. Columns matched on the eight sampled keys.
- The populated-pad mask uses that mapping, excluding unused pad `(0,10)`.
- Seven native bins isolate populated outlier `(7,14)` and sort the remaining
  pads into six bins. Their +16 offset rejects the smaller neighbouring signal
  seen in the held-key recordings. This does not change the scanner or GPIOs.
- The native code decoded 81 recorded held-key frames to their eight expected
  single keys; simulated individual press/release checks cover all 92 pads.
- The updated UF2 was flashed through software BOOT. Connected tests recorded
  187 empty idle snapshots, eight sweeps with at least 12 counts of released
  margin, successful threshold readback/error rejection, and unchanged keymaps.
  All four compiled GUI diagnostic pages were tested on this physical controller.

The full sweep captured 89 distinct presses/releases with no duplicate or
overlapping matrix events. Linux reported 102 matching key-down/key-up pairs
(including macros) and no keys left down. Three all-options matrix positions
were not pressed: (2,11), (4,10), (6,10). Other layout variants, chords,
LED/solenoid acceptance and cold-power-cycle coverage remain pending. Do not infer input latency from a native
multi-request diagnostic snapshot (measured around 82 ms for the whole query).

The corrected board-level connector table applies to all FAT keymaps.
QMK rows 0–7 connect to controller rows 7–0 and J2 analog input pins 10–3.
Personal calibration remains confined to `vial_alec`. Verify orientation against the
actual assembly. See the [parent report](../README.md) for general recovery.
