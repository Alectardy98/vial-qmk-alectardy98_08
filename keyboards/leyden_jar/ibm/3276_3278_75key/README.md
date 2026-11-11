# IBM Beamspring 3276 / 3278 75-key — Leyden Jar Rev 3

This target uses a modified Leyden Jar Rev 3 with four sense inputs and twenty
drive outputs. Both `vial` and `vial_alec` share the scanner, wiring and
calibration profile. `vial_alec` adds Alec's personal keymap and macros.

## Hardware modification

Disconnect USB before soldering. Use the Rev 3 PCB pad numbers and the U6
pin-1 marking to identify the footprint orientation.

1. Remove **U6**, the LMV339 serving the four unused sense inputs.
2. Remove **R27, R35, R39 and R37**, the four 47 kΩ resistors connecting those
   inputs to the shared sense-bias rail.
3. Install the following links at the empty U6 footprint, with **one 1 kΩ
   resistor in series with each link**.

| New drive | J2 pin | RP2040 GPIO | U6 pad link through 1 kΩ |
|---|---:|---:|---|
| 0 | 7 | 4 | Pad 2 → pad 5 |
| 1 | 8 | 5 | Pad 1 → pad 7 |
| 2 | 9 | 6 | Pad 14 → pad 9 |
| 3 | 10 | 7 | Pad 13 → pad 11 |

The existing 10 kΩ output pull-ups **R25, R33, R38 and R36** can stay.
J2 pins 3–6 retain the four sense inputs through their original comparators.

## Keyboard connector

| J2 pins | Connection |
|---|---|
| 1–2 | Ground |
| 3–6 | Sense inputs 0–3 |
| 7–10 | Drive outputs 0–3 through the U6 modifications |
| 11–12 | Ground |
| 13–28 | Drive outputs 4–19 through the first sixteen native outputs |

The installed matrix ends at **J2 pin 28**. J2 pins 29–30 are unused by this
scanner. J2 LED0/LED1 pins 31–32 remain high-impedance.

The source's QMK columns are reversed relative to this connector's drive order.
The complete mapping appears in the generated reference below and in
[config.h](config.h).

## Solenoid

J4 retains the original solenoid-driver connections. Enable is **J4 pin 2 /
GPIO28** and pulse is **J4 pin 4 / GPIO29**. Use the solenoid driver board for
the coil. The added matrix outputs use GPIO4–7.

## Calibration

[calibration.h](calibration.h) defines two native calibration bins. The first
contains controller column 18, all four sense rows, plus controller position
(row 3, column 0). The second contains the other seventy keys. Their default
relative offsets are **−23** and **−22**.

Startup calibration measures the released reference for each bin. Typical
references of 473–474 and 444 produce triggers of 450–451 and 422. Beamspring
presses reduce the sensed level, so the triggers sit below the released levels.
Vi_al can save relative threshold adjustments and restore these defaults.

## Build and recovery

From the QMK repository root, build either keymap with

```sh
make leyden_jar/ibm/3276_3278_75key:vial
make leyden_jar/ibm/3276_3278_75key:vial_alec
```

The UF2 files appear in the repository root. Enter RP2040 BOOTSEL and copy the
chosen UF2 to `RPI-RP2`. Normal startup enables typing after calibration.
Vi_al's Bootloader action and the physical BOOTSEL recovery path are available.

## Verification

The installed two-bin build passed a complete 75-key physical sweep in layout
order, with one press/release run per key and no simultaneous false detections.
Idle checks passed across two software restarts. Captured-data regression tests
cover held/released bounds, cross-bin chords, persistence, protocol bounds and
scan-failure handling. Cold power cycling, extended typing and the solenoid's
physical operation remain user acceptance checks.

The active firmware keeps the native peripheral and timing architecture.
Command `0x95` provides read-only scan timing and error counts. Archived LED
transport experiments and the temporary `0x96` pin-control command have been
retired from this build.

<!-- BEGIN LEYDEN JAR PIN REFERENCE -->
## Leyden Jar Rev 3 pin reference

**Logical diagram, not a solder-side view.** Match the numbered PCB pads; the drawing does not establish connector orientation. QMK rows/columns are zero-based firmware coordinates, **not original IBM ribbon or connector pin numbers**.

**Keyboard source declares:** 4 logical rows × 20 logical columns. **Rev 3 controller capacity:** 8 sense inputs and 18 drive outputs.

The original/native scanner declares **4 capacitive sense rows**. Any extra QMK logical rows may represent direct keys rather than additional capacitive rows. All eight A_ROW pins below belong to the controller; unassigned ones do not imply extra rows on the keyboard.

The [original Beamspring-USB installation guide, page 2](https://static.wongcornall.com/ibm-capsense-usb/installation_beamspring.pdf) describes the standard 4-row / 23-column interface and treats Displaywriter separately. This keyboard source may use only a subset of those columns. The stock J2 connector has 18 drive outputs: ports needing more signals require additional verified hardware/driver support. The populated-signal audit is in [DRIVE-CAPACITY.md](../DRIVE-CAPACITY.md).

**3276 modified-header candidate.** J2.3-6 are the four sense inputs. J2.7-10 drive columns 0-3 through GPIO4-7 after U6 removal. Install four 1 kΩ series links at U6, pads 2-to-5, 1-to-7, 14-to-9 and 13-to-11. Remove R27, R35, R39 and R37 to disconnect these former sense inputs from the shared bias rail. J2.13-28 provide the remaining sixteen drives. The installed physical sweep verified all 75 key positions with two calibration bins.

**Populated source-layout union:** 75 positions, 20 distinct drive signals. Empty intersections do not free a column unless that entire column is unused.

**Compact adapter mapping:** the firmware now uses the mapping below. Wire the corresponding source signals through an adapter; this is not a drop-in match for their old column numbers. Physical connector contact numbers/orientation still require continuity verification.

| QMK column | Source column signal | Rev 3 column | Controller connection |
|---:|---:|---:|---:|
| 0 | 3 | COL19 | J2.28 |
| 1 | 4 | COL18 | J2.27 |
| 2 | 5 | COL17 | J2.26 |
| 3 | 6 | COL16 | J2.25 |
| 4 | 7 | COL15 | J2.24 |
| 5 | 8 | COL14 | J2.23 |
| 6 | 9 | COL13 | J2.22 |
| 7 | 10 | COL12 | J2.21 |
| 8 | 11 | COL11 | J2.20 |
| 9 | 12 | COL10 | J2.19 |
| 10 | 13 | COL9 | J2.18 |
| 11 | 14 | COL8 | J2.17 |
| 12 | 15 | COL7 | J2.16 |
| 13 | 16 | COL6 | J2.15 |
| 14 | 17 | COL5 | J2.14 |
| 15 | 18 | COL4 | J2.13 |
| 16 | 19 | COL3 | J2.10 |
| 17 | 20 | COL2 | J2.9 |
| 18 | 21 | COL1 | J2.8 |
| 19 | 22 | COL0 | J2.7 |

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
03    A_ROW0              QMK row 3
04    A_ROW1              QMK row 2
05    A_ROW2              QMK row 1
06    A_ROW3              QMK row 0
07    A_ROW4              Digital drive COL0 after U6 modification
08    A_ROW5              Digital drive COL1 after U6 modification
09    A_ROW6              Digital drive COL2 after U6 modification
10    A_ROW7              Digital drive COL3 after U6 modification
11    GND                 Matrix ground
12    GND                 Matrix ground
13    COL0                QMK column 15
14    COL1                QMK column 14
15    COL2                QMK column 13
16    COL3                QMK column 12
17    COL4                QMK column 11
18    COL5                QMK column 10
19    COL6                QMK column 9
20    COL7                QMK column 8
21    COL8                QMK column 7
22    COL9                QMK column 6
23    COL10               QMK column 5
24    COL11               QMK column 4
25    COL12               QMK column 3
26    COL13               QMK column 2
27    COL14               QMK column 1
28    COL15               QMK column 0
29    COL16               Unused in this adapter
30    COL17               Unused in this adapter
31    LED0                High-impedance input in this candidate
32    LED1                High-impedance input in this candidate
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

On this modified target, J2.31/32 remain high-impedance. J2.33 retains the native Num Lock output.

Original controller-family source: [AVR configuration](../../../xwhatsit/ibm/3276_3278_75key/universal/config.h) and [port inventory](../ports.json). Blocked references retain source relationships even where a complete native adapter mapping is unavailable.

Mapping sources: [config.h](config.h) and [common.c](../../common.c). Switch technology: **beamspring**. The installed modified 3276 passed a complete 75-key physical sweep. Other controller assemblies require their own physical verification.

Controller wiring source: `mymakercorner/Leyden_Jar:pcb/leyden_jar_controller/Leyden_Jar.kicad_sch @ e01dea552d0a`, J2/J3/J4 and the A_ROW/COL/LED/SWD/I2C/expansion nets. [Rev 3 schematic](https://github.com/mymakercorner/Leyden_Jar/blob/e01dea552d0a/pcb/leyden_jar_controller/Leyden_Jar.kicad_sch).

This section is maintained by `ibm/tools/update_pin_readmes.py`; run it after changing a port mapping. <!-- END LEYDEN JAR PIN REFERENCE -->
