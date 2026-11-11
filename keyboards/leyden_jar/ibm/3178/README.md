# IBM Model F 3178 - Leyden Jar Rev 3

## Native 3178 support

`vial` and `vial_alec` retain the source's 9×13 **logical** layout: capacitive
keys occupy rows 0–7, columns 0–11; the separate blue switch occupies `(8,12)`.
This does not establish the pad-card edge-contact numbering or prove that its
physical row labels match QMK coordinates. Source row 7 includes Space/Ctrl and
other keys; physical wiring still needs press/release validation.

The blue button closes **J2.30 / COL17 to GND**. That signal reaches GPIO27
through R47 (1 kΩ). This port uses 16 PIO drive channels, leaving GPIO26/27
unclaimed by PIO, and configures GPIO27 as a pulled-up input. It does not use
LED0/1/2 or solenoid GPIOs for the switch. Capacitive calibration excludes the
switch, and QMK's normal matrix debounce processes its raw state.

Protocol 0.10.1 adds an optional descriptor in the otherwise reserved portion
of the Details reply: byte 6 = 1, bytes 7–9 = logical row, column, GPIO.
The legacy capacitive mapping remains eight sense entries. Leyden Jar add-on
1.1.1 recognizes the direct row without inventing a capacitive signal value;
older add-ons reject the nine-row target. Existing eight-row controllers remain
compatible. General, calibration, live-sensor and native BOOT commands remain
available with reports disabled.

The installed assembly's released readings spanned about 414–506, wider than
the old 350–499 sweep. This target extends that sweep to 350–899. Its shared
[calibration.h](calibration.h) separates controller pads `(6,2)` and `(6,9)`
and sorts the other pads into six groups. The original eight-group trial used +10 offsets except +17 for the upper sorted
group. The current defaults are +18 and +25 respectively, following the
held-key coupling investigation below. Eight groups add scan passes compared with a single
shared threshold; end-to-end input latency has not been measured.

The first one-group test reproduced 23–24 false active positions with reports
suppressed. The eight-group hardware trial had 40 clear idle snapshots and
five sweeps with positive released margins. A controlled physical capture verified A at `(5,9)`, Space at `(7,7)` and the
blue switch at `(8,12)`, each releasing correctly. Signal increases were about
57 counts for A and 65 for Space. Full physical matrix mapping and chords still
require validation. The final enabled image passed three software-start checks
with clear idle states and at least four counts of released margin. One initial
warm UF2 handoff required a host USB configuration retry; a later observed eight-second USB disconnect/reconnect configured normally
and returned with typing enabled and a clear idle matrix. The initial warm
handoff failure remains unexplained; no bootloader defect or fix is claimed.

### Same-column false activations

Idle-only validation did not catch interaction between keys. In the controlled
87-key sweep, Y also activated F, U activated 8, and L activated M; other short
same-column pulses appeared on twelve physical holds. These were raw matrix
activations, with corresponding unwanted USB presses, rather than just Linux
held-key autorepeat. All 87 dominant physical positions matched the source
layout in the user's declared top-left-to-bottom-right order.

Signal sweeps with QMK output disabled (therefore no newly generated haptic
pulses) measured F at 453 during Y, above its 451 trigger; 8 at 473 during U,
above its 472 trigger; and M at 454–455 during L, above its 451 trigger. The
intended targets were 512–516. This establishes insufficient trigger margin
against measured same-column signal coupling. It does not establish the
physical origin of every coupled signal or eliminate possible wiring defects.

The correction adds eight counts to every group offset: +18 for groups 0–6,
+25 for group 7. The grouping, driver, GPIO mapping, default debounce and scan
schedule are unchanged. A RAM-only preview repeated all 87 positions in order
with zero neighboring-key activations. One brief Fn pulse preceded its main
press by about 0.4 seconds; the user was unsure whether it was a separate touch,
so it remains unconfirmed rather than being counted as resolved chatter.

The new [coupling fixture](../../tests/fixtures/3178_coupling.json) retains the
measured boot's groups and thresholds, since a later released sweep can sort
near-equal keys into different groups. Regressions reproduce the old ghosts
and reject them with the corrected margins using both recorded groups and a
fresh calibration. All five released captures still pass for both keymaps.

Tests:

```sh
make leyden_jar/ibm/3178:vial
make leyden_jar/ibm/3178:vial_alec
python3 keyboards/leyden_jar/tests/test_3178_direct.py
python3 keyboards/leyden_jar/tests/test_3178_calibration.py
python3 keyboards/leyden_jar/tests/test_3178_coupling.py
```

The calibration regression replays all five released captures, reproduces the
previous single-bin failure, and checks simulated press/release for all 87
source-defined capacitive positions plus the switch and persistent offsets.

<!-- BEGIN LEYDEN JAR PIN REFERENCE -->
## Leyden Jar Rev 3 pin reference

**Logical diagram, not a solder-side view.** Match the numbered PCB pads; the drawing does not establish connector orientation. QMK rows/columns are zero-based firmware coordinates, **not original IBM ribbon or connector pin numbers**.

**Keyboard source declares:** 9 logical rows × 13 logical columns. **Rev 3 controller capacity:** 8 sense inputs and 18 drive outputs.

The original/native scanner declares **8 capacitive sense rows**. Any extra QMK logical rows may represent direct keys rather than additional capacitive rows. All eight A_ROW pins below belong to the controller; unassigned ones do not imply extra rows on the keyboard.

**Populated source-layout union:** 88 positions, 12 distinct drive signals. Empty intersections do not free a column unless that entire column is unused.

**Blue button:** connect one terminal to J2.30 / COL17 and the other to GND. This target reserves GPIO27 as a pulled-up input and scans only the first 16 PIO columns. The button reports QMK (8,12); it is not a ninth capacitive row. LED0/1/2 and solenoid signals remain available.

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
25    COL12               No capacitive pads; blue button is a separate switch
26    COL13               No assigned column
27    COL14               No assigned column
28    COL15               No assigned column
29    COL16               Not scanned by this target
30    COL17               Blue button to GND (GPIO27 input; excluded from PIO)
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

Original controller-family source: [AVR configuration](../../../xwhatsit/ibm/3178/universal/config.h) and [port inventory](../ports.json). Blocked references retain source relationships even where a complete native adapter mapping is unavailable.

Mapping sources: [config.h](config.h) and [common.c](../../common.c). Switch technology: **model f**. Only the FAT has the earlier full physical row/key sweep; F122 has firmware/idle checks. Other ports require physical matrix verification.

Controller wiring source: `mymakercorner/Leyden_Jar:pcb/leyden_jar_controller/Leyden_Jar.kicad_sch @ e01dea552d0a`, J2/J3/J4 and the A_ROW/COL/LED/SWD/I2C/expansion nets. [Rev 3 schematic](https://github.com/mymakercorner/Leyden_Jar/blob/e01dea552d0a/pcb/leyden_jar_controller/Leyden_Jar.kicad_sch).

This section is maintained by `ibm/tools/update_pin_readmes.py`; run it after changing a port mapping. <!-- END LEYDEN JAR PIN REFERENCE -->
