# brand_new_beamspring_full

<!-- BEGIN LEYDEN JAR PIN REFERENCE -->
## Leyden Jar Rev 3 pin reference

**Logical diagram, not a solder-side view.** Match the numbered PCB pads; the drawing does not establish connector orientation. QMK rows/columns are zero-based firmware coordinates, **not original IBM ribbon or connector pin numbers**.

**Keyboard source declares:** 8 logical rows × 16 logical columns. **Rev 3 controller capacity:** 8 sense inputs and 18 drive outputs.

**Model F Labs reproduction/legacy target, not an original IBM pinout.** Its logical matrix cannot be substituted for the wiring of an original IBM beamspring.

**BLOCKED / UNVERIFIED PORT — this is not a complete soldering guide.**

- Legacy target has no matrix-format selection recognized by current common.c; build metadata/configuration also needs repair.

Any source relationships below describe only the declared signals. They do not supply a missing adapter, extra sense inputs, or unsupported drive outputs.

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
03    A_ROW0              Unverified; legacy port
04    A_ROW1              Unverified; legacy port
05    A_ROW2              Unverified; legacy port
06    A_ROW3              Unverified; legacy port
07    A_ROW4              Unverified; legacy port
08    A_ROW5              Unverified; legacy port
09    A_ROW6              Unverified; legacy port
10    A_ROW7              Unverified; legacy port
11    GND                 Matrix ground
12    GND                 Matrix ground
13    COL0                Unverified; legacy port
14    COL1                Unverified; legacy port
15    COL2                Unverified; legacy port
16    COL3                Unverified; legacy port
17    COL4                Unverified; legacy port
18    COL5                Unverified; legacy port
19    COL6                Unverified; legacy port
20    COL7                Unverified; legacy port
21    COL8                Unverified; legacy port
22    COL9                Unverified; legacy port
23    COL10               Unverified; legacy port
24    COL11               Unverified; legacy port
25    COL12               Unverified; legacy port
26    COL13               Unverified; legacy port
27    COL14               Unverified; legacy port
28    COL15               Unverified; legacy port
29    COL16               Unverified; legacy port
30    COL17               Unverified; legacy port
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

Mapping sources: [config.h](config.h) and [common.c](../common.c). Switch technology: **beamspring**. Only the FAT has the earlier full physical row/key sweep; F122 has firmware/idle checks. Other ports require physical matrix verification.

Controller wiring source: `mymakercorner/Leyden_Jar:pcb/leyden_jar_controller/Leyden_Jar.kicad_sch @ e01dea552d0a`, J2/J3/J4 and the A_ROW/COL/LED/SWD/I2C/expansion nets. [Rev 3 schematic](https://github.com/mymakercorner/Leyden_Jar/blob/e01dea552d0a/pcb/leyden_jar_controller/Leyden_Jar.kicad_sch).

This section is maintained by `ibm/tools/update_pin_readmes.py`; run it after changing a port mapping. <!-- END LEYDEN JAR PIN REFERENCE -->
