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

The lock handler uses firmware LED0 for Num Lock, LED1 for Caps Lock, and LED2
for Scroll/FN. In the current driver those reach **PCB LED2/J2.33**,
**PCB LED1/J2.32**, and **PCB LED0/J2.31**, respectively. The personal
keymap's existing Num/FN behaviour remains in its `led_update_user` implementation.
Check the actual LED header wiring and polarity when assembling the controller.

Release all keys during startup automatic calibration. Standard `vial` now uses the corrected compiler-derived mapping and retains
its single-bin +10 calibration default; it has not been
physically validated on an unmodified FAT. Alec's modified PCB uses a separate
`vial_alec/calibration.h` calibration override:

- The board-level map was checked on all eight physical rows: QMK row → controller row is
  `{7,6,5,4,3,2,1,0}`. Columns matched on the eight sampled keys.
- The populated-pad mask uses that mapping, excluding unused pad `(0,10)`.
- Eight native bins include two explicitly assigned groups and six sorted
  groups. Custom group 0 retains controller pad `(7,14)` at startup reference
  +16. Custom group 1 contains only QMK `(5,2)` / controller `(2,2)`, using its
  own startup reference +12: the physical Live Sensors trace showed a sustained
  reading around 404–406 against the old shared trigger of 404. The six sorted
  groups retain +16 offsets. This keymap-only change leaves the scanner and
  GPIO implementation unchanged; one additional bin adds one scan pass.
- The native code decoded 81 recorded held-key frames to their eight expected
  single keys; simulated individual press/release checks cover all 92 pads.
- The updated UF2 was flashed through software BOOT. Connected tests recorded
  187 empty idle snapshots, eight sweeps with at least 12 counts of released
  margin, successful threshold readback/error rejection, and unchanged keymaps.
  The original four compiled GUI diagnostic pages were tested on this physical
  controller; the later GUI add-on removes the redundant Keypress Monitor tab.

The eight-bin sensitivity candidate passes all 92 simulated populated-pad
press/release paths and all 81 previously recorded held-key frames using the
native scanner under AddressSanitizer/UndefinedBehaviorSanitizer. The recorded
released capture produces a trigger of 398 for QMK `(5,2)`, instead of 404.
Actual references change at each startup calibration. A fresh held-key check
and neighbouring-key/chord testing on the physical assembly are still required;
the recorded data cannot establish margins for every possible key combination.

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

## Persistent sensitivity and full Vial features

Native protocol 0.10.0 and Leyden Jar GUI add-on 1.1.0 add **Apply and save**:
the selected bin's offset from its startup reference is stored in the existing
QMK EEPROM. Automatic startup calibration remains active. **Restore calibration
defaults** restores the offsets compiled into `calibration.h` without clearing
keymaps/macros or restarting. Group 1's compiled default is +12 for QMK `(5,2)`.

All Vial variants now use four dynamic layers and normal Vial entry capacities.
This FAT reports 32 tap dances, 32 combos, 32 key overrides, 32 alternate-repeat
entries and 1788 macro bytes. Its former three-layer map and macros were exported
before flashing, restored into the new layout, and checked through subsequent
restarts. Export before future upgrade/rollback: the old and new EEPROM addresses
are different. Persistence and Restore defaults were both verified through
software restarts; cold power-loss and full typing/chord acceptance remain
physical checks.

<!-- BEGIN LEYDEN JAR PIN REFERENCE -->
## Leyden Jar Rev 3 pin reference

**Logical diagram, not a solder-side view.** Match the numbered PCB pads; the drawing does not establish connector orientation. QMK rows/columns are zero-based firmware coordinates, **not original IBM ribbon or connector pin numbers**.

**Keyboard source declares:** 8 logical rows × 16 logical columns. **Rev 3 controller capacity:** 8 sense inputs and 18 drive outputs.

The original/native scanner declares **8 capacitive sense rows**. Any extra QMK logical rows may represent direct keys rather than additional capacitive rows. All eight A_ROW pins below belong to the controller; unassigned ones do not imply extra rows on the keyboard.

**Populated source-layout union:** 92 positions, 16 distinct drive signals. Empty intersections do not free a column unless that entire column is unused.

This table follows this target's source mapping. It does not establish the original keyboard connector orientation, wire colours, supply requirements, or that every row/column intersection has a physical key.

```text
                 +--------------------------+
USB-C -----------|     LEYDEN JAR REV 3      |
                 |          RP2040          |
                 | J2  Matrix / LED pads    |
                 | J3  Debug / I2C header    |
                 | J4  Expansion header     |
                 +--------------------------+

J2 - MAIN CONNECTOR
Pin   Controller signal   Logical keyboard signal
---   -----------------   -----------------------
01    GND                 Matrix ground
02    GND                 Matrix ground
03    A_ROW0              QMK row 7
04    A_ROW1              QMK row 6
05    A_ROW2              QMK row 5
06    A_ROW3              QMK row 4
07    A_ROW4              QMK row 3
08    A_ROW5              QMK row 2
09    A_ROW6              QMK row 1
10    A_ROW7              QMK row 0
11    GND                 Matrix ground
12    GND                 Matrix ground
13    COL0                QMK column 0
14    COL1                QMK column 1
15    COL2                QMK column 2
16    COL3                QMK column 3
17    COL4                QMK column 4
18    COL5                QMK column 5
19    COL6                QMK column 6
20    COL7                QMK column 7
21    COL8                QMK column 8
22    COL9                QMK column 9
23    COL10               QMK column 10
24    COL11               QMK column 11
25    COL12               QMK column 12
26    COL13               QMK column 13
27    COL14               QMK column 14
28    COL15               QMK column 15
29    COL16               No assigned column
30    COL17               No assigned column
31    LED0                I/O-expander LED output; see note below
32    LED1                I/O-expander LED output; see note below
33    LED2                I/O-expander LED output; see note below

J3 - DEBUG / I2C HEADER
Pin   Signal
---   ------
01    GND
02    3.3V
03    SWCLK
04    SDA (GP16)
05    SWDIO (schematic: SWIO)
06    SCL (GP17)

J4 - EXPANSION / SOLENOID DRIVER HEADER
Pin   Signal
---   ------
01    +5V
02    Solenoid enable (GP28)
03    PS2_ENABLE
04    Solenoid pulse (GP29)
05    3.3V
06    GND
```

J2 sense signals are analog inputs to the controller circuitry; do not bypass the comparators by wiring the keyboard directly to RP2040 GPIOs. J4 solenoid enable/pulse are driver-control signals, not bare-coil connections.

**LED labels versus firmware names:** the current shared driver assigns firmware LED0 to IO4, while the Rev 3 schematic labels that output LED2; firmware LED2 reaches the PCB pad LED0. Firmware LED1 matches PCB LED1. The default lock handler therefore sends Num Lock to J2.33, Caps Lock to J2.32 and Scroll Lock to J2.31. A keymap's `led_update_user()` may override those functions or add layer indications. No original keyboard light-board voltage/pinout is assumed.

For the current FAT `vial_alec` lock-light signal mapping:

```text
AT A1 (Caps Lock)   -> J2.32 / PCB LED1
AT A2 (Scroll Lock) -> J2.31 / PCB LED0
AT A3              -> not connected
AT B1 (Num Lock)    -> J2.33 / PCB LED2
AT B2 (GND)         -> GND
AT B3 (VCC)         -> supply connection not electrically verified
```

Trace the wires from the AT light-board labels: its cable rearranges the connector order. See the [original AT installation manual, pages 13–14](https://static.wongcornall.com/ibm-capsense-usb/installation_model_f.pdf).

Original controller-family source: [AVR configuration](../../../xwhatsit/ibm/fat/universal/config.h) and [port inventory](../ports.json). Blocked references retain source relationships even where a complete native adapter mapping is unavailable.

Mapping sources: [config.h](config.h) and [common.c](../../common.c). Switch technology: **model f**. Only the FAT has the earlier full physical row/key sweep; F122 has firmware/idle checks. Other ports require physical matrix verification.

Controller wiring source: `mymakercorner/Leyden_Jar:pcb/leyden_jar_controller/Leyden_Jar.kicad_sch @ e01dea552d0a`, J2/J3/J4 and the A_ROW/COL/LED/SWD/I2C/expansion nets. [Rev 3 schematic](https://github.com/mymakercorner/Leyden_Jar/blob/e01dea552d0a/pcb/leyden_jar_controller/Leyden_Jar.kicad_sch).

This section is maintained by `ibm/tools/update_pin_readmes.py`; run it after changing a port mapping. <!-- END LEYDEN JAR PIN REFERENCE -->
