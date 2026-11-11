# BEAMal - Leyden Jar Rev 3

Uses the native eight-row comparator circuit, PIO scanner, DAC and solenoid
driver. Its twelve connected drive channels carry source columns 11 down to 0.
The physical mapping follows the ordered 2026-11-11 key capture; the original
AVR TEST transform alone does not establish the connector order.

```sh
make leyden_jar/ibm/beamal:vial
make leyden_jar/ibm/beamal:vial_alec
```

`vial_alec` matches Alec's 88-switch assembly: shorter side clusters and all
five spacebar-row keys. Its [population mask](keymaps/vial_alec/population.h)
excludes only the six optional bottom side-cluster pads. Standard `vial`
supports the complete 94-position source layout union. Changing layout options
in the GUI does not install those extra switches or change the personal mask.

The [captured regression fixture](../../tests/fixtures/beamal_sweep_levels.json)
covers all 88 physical positions. The host test replays their measured
press/release values through the native C scanner, including Alt and D, which
the old reversed pad mask excluded. Calibration retains its single group and
−30 offset. Vial coordinates, custom keycode values and saved keymaps retain
their existing meanings.

<!-- BEGIN LEYDEN JAR PIN REFERENCE -->
## Leyden Jar Rev 3 pin reference

**Logical diagram, not a solder-side view.** Match the numbered PCB pads; the drawing does not establish connector orientation. QMK rows/columns are zero-based firmware coordinates, **not original IBM ribbon or connector pin numbers**.

**Keyboard source declares:** 8 logical rows × 12 logical columns. **Rev 3 controller capacity:** 8 sense inputs and 18 drive outputs.

The original/native scanner declares **8 capacitive sense rows**. Any extra QMK logical rows may represent direct keys rather than additional capacitive rows. All eight A_ROW pins below belong to the controller; unassigned ones do not imply extra rows on the keyboard.

**TEST-controller source definition:** this target uses the source `CONTROLLER_IS_TEST` mapping. It is not a verified standard IBM beamspring connector reference; do not generalize its dimensions or identity mapping to a 327x/5251 keyboard.

**BEAMal physical sweep:** the Rev 3 connector carries source columns 11 down to 0 on COL0 through COL11; rows retain their source order. The 2026-11-11 capture matched all 88 shorter-layout keys to the original layout. `vial_alec` excludes six unpopulated bottom side-cluster positions for the shorter side layout; standard `vial` keeps all 94 source positions. All five spacebar-row switches, including right Ctrl, remain enabled.

**Populated source-layout union:** 94 positions, 12 distinct drive signals. Empty intersections do not free a column unless that entire column is unused.

**Compact adapter mapping:** the firmware now uses the mapping below. Wire the corresponding source signals through an adapter; this is not a drop-in match for their old column numbers. Physical connector contact numbers/orientation still require continuity verification.

| QMK column | Source column signal | Rev 3 column | J2 pin |
|---:|---:|---:|---:|
| 0 | 0 | COL11 | 24 |
| 1 | 1 | COL10 | 23 |
| 2 | 2 | COL9 | 22 |
| 3 | 3 | COL8 | 21 |
| 4 | 4 | COL7 | 20 |
| 5 | 5 | COL6 | 19 |
| 6 | 6 | COL5 | 18 |
| 7 | 7 | COL4 | 17 |
| 8 | 8 | COL3 | 16 |
| 9 | 9 | COL2 | 15 |
| 10 | 10 | COL1 | 14 |
| 11 | 11 | COL0 | 13 |

The row mapping and QMK/Vial key coordinates are unchanged.

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
03    A_ROW0              QMK row 0
04    A_ROW1              QMK row 1
05    A_ROW2              QMK row 2
06    A_ROW3              QMK row 3
07    A_ROW4              QMK row 4
08    A_ROW5              QMK row 5
09    A_ROW6              QMK row 6
10    A_ROW7              QMK row 7
11    GND                 Matrix ground
12    GND                 Matrix ground
13    COL0                QMK column 11
14    COL1                QMK column 10
15    COL2                QMK column 9
16    COL3                QMK column 8
17    COL4                QMK column 7
18    COL5                QMK column 6
19    COL6                QMK column 5
20    COL7                QMK column 4
21    COL8                QMK column 3
22    COL9                QMK column 2
23    COL10               QMK column 1
24    COL11               QMK column 0
25    COL12               No assigned column
26    COL13               No assigned column
27    COL14               No assigned column
28    COL15               No assigned column
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

Original controller-family source: [AVR configuration](../../../xwhatsit/ibm/beamal/universal/config.h) and [port inventory](../ports.json). Blocked references retain source relationships even where a complete native adapter mapping is unavailable.

Mapping sources: [config.h](config.h) and [common.c](../../common.c). Switch technology: **beamspring**. All 88 keys in the installed shorter-side layout were observed in the physical sweep and replayed through the corrected native matrix code. The six optional extra positions require a fully populated assembly to test.

Controller wiring source: `mymakercorner/Leyden_Jar:pcb/leyden_jar_controller/Leyden_Jar.kicad_sch @ e01dea552d0a`, J2/J3/J4 and the A_ROW/COL/LED/SWD/I2C/expansion nets. [Rev 3 schematic](https://github.com/mymakercorner/Leyden_Jar/blob/e01dea552d0a/pcb/leyden_jar_controller/Leyden_Jar.kicad_sch).

This section is maintained by `ibm/tools/update_pin_readmes.py`; run it after changing a port mapping. <!-- END LEYDEN JAR PIN REFERENCE -->
