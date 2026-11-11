# Rev 3 populated-signal and expansion audit

The original blanket rejection was too restrictive. Source column indices are
not native channel capacity, and QMK logical dimensions are not a wiring diagram.
Two ports now use compact adapters and the existing native scanner. No hardware
was flashed during this audit. Wider-board expansion remains a design candidate.

## Four-row / twenty-two-column proposal

The requested connector order is `GND, GND, ROW0–3, COL0–3, GND, GND,
COL4–21`. It ends at **pin 30**, not 29: pins 3–6 are the four rows,
7–10 the first four columns, 13–30 the remaining eighteen columns.
This order is neither the existing Rev 3 connector nor the original IBM edge
connector.

Rev 3 J2.7–10 are **A_ROW4–7 analog comparator inputs**, not spare GPIO
outputs. For example, J2.7 goes to U6/LMV339 input pin 5; GPIO4 goes to U6
output pin 2 on a separate net. Changing a GPIO direction cannot drive a signal
backwards through the comparator to the connector. Implementing the proposed
pinout therefore requires hardware rerouting or a different controller/adapter
with additional drive outputs, followed by a matching scanner implementation.

The 3276 specifically needs **4×20**, using original drive signals 3–22.
The other original beamspring sources were checked individually; the table below
lists their populated drive counts, including 17/18-column 3277 variants,
22-column standard 5251, and 23-column 3101/Hebrew 5251. Displaywriter remains
an independent 8×12 adapter. Beamal uses an 8×12 TEST-family mapping and retains
beamspring sensing polarity; it is not converted to the four-row wiring.
The separately named `beamal_badpinout` definition is not equivalent to Beamal.

For the original IBM edge connector only, the archived Beamspring-USB-Rev4
installation guide (2014-07-08), page 2, Figure 1, shows pin 1 GND;
pins 2–24 column signals 0–22; pins 25–26 GND; pins 27–30 row signals
3, 2, 1, 0, viewed from the normal typing position. The figure explicitly
excludes Displaywriter from its compatibility statement. This is an original
keyboard connector reference, not a numbering translation to Rev 3 J2.

Revalidation: the host C compiler evaluated the original scanner macros for all
29 IBM source definitions; the mapping and active-pad checks passed. All 37
generated connector references also passed their consistency check. These checks
verify source relationships, not continuity of an individual keyboard. No flash,
EEPROM, driver, pin-direction or port-support changes were made for this review.

## What the existing layouts actually use

Counts below are the **union of all source layout options**, not counts of
nonzero keycodes. An unassigned key still has a physical sensing pad. Alternatives
can make the position count exceed the nameplate key count. A source matrix with
empty cells still requires a column wire if any populated key uses that column.

| Source | Declared QMK matrix | Populated positions | Used logical rows | Independent source columns | Current result |
|---|---:|---:|---:|---:|---|
| 3101_3727_3278_87key | 4×23 | 87 | 4 | 23 | current driver blocked |
| 3178 | 9×13 | 88 | 9 logical / 8 capacitive | 12 capacitive + direct switch | builds; GPIO27 switch, physical acceptance required |
| 3276_3278_75key | 4×20 | 75 | 4 | 20 | GPIO4-7 candidate after U6 removal |
| 3277_66key | 4×17 | 66 | 4 | 17 | compact adapter; builds |
| 3277_78key | 4×20 | 78 | 4 | 20 | current driver blocked |
| 3277_split_66key | 4×18 | 69 | 4 | 18 | compact adapter; builds |
| 3278_3279_led_78key | 5×20 | 78 | 5 | 20 | current driver blocked |
| 4978 | 8×22 | 171 | 8 | 22 | current driver blocked |
| 5251 | 4×23 | 83 | 4 | 22 | current driver blocked |
| 5251_hebrew | 4×23 | 85 | 4 | 23 | current driver blocked |
| beamal | 8×12 | 94 | 8 | 12 | existing adapter; builds |
| beamal_badpinout | 16×16 | 94 | 9 | 15 | current driver blocked |
| bigfoot_fxt_type_1 | 4×23 | 83 | 4 | 23 | current driver blocked |
| displaywriter | 8×12 | 84 | 8 | 12 | existing adapter; builds |
| f104 | 8×14 | 112 | 8 | 14 | existing adapter; builds |
| f107 | 8×15 | 109 | 8 | 15 | existing adapter; builds |
| f122 | 8×16 | 127 | 8 | 16 | existing adapter; builds |
| f50 | 8×7 | 50 | 8 | 7 | existing adapter; builds |
| f62 | 8×8 | 64 | 8 | 8 | existing adapter; builds |
| fat | 8×16 | 92 | 8 | 16 | existing adapter; builds |
| fext | 8×15 | 110 | 8 | 15 | existing adapter; builds |
| fssk | 8×12 | 88 | 8 | 12 | existing adapter; builds |
| fssk_v1_0b | 8×12 | 90 | 8 | 12 | existing adapter; builds |
| fxt_type_2 | 8×12 | 83 | 8 | 12 | existing adapter; builds |
| fxtal | 8×12 | 96 | 8 | 12 | existing adapter; builds |
| sun_fssk | 8×16 | 92 | 8 | 16 | existing adapter; builds |
| test | 16×16 | 256 | 16 | 16 | current driver blocked |
| test2 | 8×12 | 96 | 8 | 12 | existing adapter; builds |
| xtant | 8×12 | 85 | 8 | 12 | existing adapter; builds |

The 3101/3727/3278 87-key source uses all 23 columns. Standard 5251 leaves
source column 0 completely unused, but uses columns 1–22. Hebrew 5251 uses all
23. Their four-row pad cards have more than 72 populated positions, so combining
columns into an unchanged four-sense/18-drive grid cannot uniquely scan every
key. Unused sense inputs on the controller do not create extra row traces on
the IBM pad card.

For `3277_66key`, the source signals are 5–12 and 14–22 (17 total).
For `3277_split_66key`, they are 4–12 and 14–22 (18 total).
Each is mapped in ascending source-signal order to COL0 onward. Their board
READMEs list QMK column, original source signal, native column and J2 pin.
**These are proposed adapter connections, not verified original edge-contact
numbers.** The source-signal-to-edge-contact correspondence must be traced before
soldering. QMK/Vial key coordinates, sensing polarity, row mapping and calibration
defaults were preserved.

## Which pins are actually available

Rev 3 schematic source:
`mymakercorner/Leyden_Jar:pcb/leyden_jar_controller/Leyden_Jar.kicad_sch
@ e01dea552d0a`, U5/U8/J2/J3/J4 and column series resistors.
[Exact schematic](https://github.com/mymakercorner/Leyden_Jar/blob/e01dea552d0a/pcb/leyden_jar_controller/Leyden_Jar.kicad_sch).
Connectivity was read through KiCad export; this is schematic evidence, not a
continuity test of the user's individual controller.

| Resource | Existing connection | Expansion implication |
|---|---|---|
| RP2040 GPIO0–7 | Comparator outputs for A_ROW0–7 | Not free drive pins; driving them would contend with comparator outputs |
| GPIO8–15, 18–27 | 18 matrix drive signals through 1 kΩ resistors | Already used by native PIO scanner |
| GPIO16/17 | I²C SDA/SCL, DAC and PCA9557 | Available bus on J3.4/J3.6; not free GPIOs |
| GPIO28/29 | Solenoid enable/pulse on J4.2/J4.4 | Two possible drive outputs only if solenoid function is deliberately relinquished |
| U8 IO1 / pin 7 | No-connect | Candidate extra drive, requires solder access and validated series resistance/timing |
| U8 IO2 / pin 9 | No-connect | Same |
| U8 IO7 / pin 14 | No-connect | Same |
| U8 IO3 / pin 10 | PS2_ENABLE, J4.3 | Candidate fourth drive if unused PS/2 function is explicitly reserved for scanning |
| U8 IO0 / pin 6 | No-connect; **open-drain** | Cannot provide the native active-high drive edge directly; needs additional circuitry/timing validation |
| U8 IO4/5/6 | LED2/1/0 | Retain lock lights; not free outputs |

The [NXP PCA9557 datasheet, Rev. 7](https://www.nxp.com/docs/en/data-sheet/PCA9557.pdf),
page 1, Figures 2–3 (pages 3–4), and pin table (pages 5–6), distinguish the
open-drain IO0 from IO1–IO7. Thus four unconnected package pins do **not** mean
four interchangeable push-pull scan outputs. The Calibre documentation database
was unavailable; the manufacturer's PDF was read directly, as previously
authorized. No separate PCA9557 errata document was established in this audit.

## Feasible support paths, not yet hardware-validated

**3276 GPIO candidate:** after U6 removal, GPIO4-7 drive four columns alongside sixteen
native outputs using the native PIO timing. The solenoid retains J4. The previous
LED1 test showed a row-0 disturbance. The [3276 wiring and calibration notes](3276_3278_75key/README.md) describe the hardware modification and the passed 75-key physical sweep.

1. **17/18 columns:** compact wiring only, existing PIO/DAC implementation.
   Both 3277 candidates now compile, and every populated position survives the
   mapping/mask checks. Physical connector and press/release tests are still needed.
2. **20 columns:** two of IO1/IO2/IO7 could supply the extra drives if a suitably
   timed scan implementation is proven. This includes the 75-key 3276/3278 and
   78-key 3277. The 3278/3279 LED variant also needs its separate direct-key row
   implemented; columns alone do not complete that port.
3. **22 columns:** IO1/IO2/IO7 plus the unused PS2_ENABLE output give four
   candidate extra channels while retaining LEDs and solenoid pins. This could
   cover standard 5251 and 4978. Current firmware does not scan these pins.
4. **23 columns:** the above four plus a buffered/pull-up-assisted IO0, or one
   deliberately reassigned solenoid GPIO, are candidates. A small external
   driver/decoder adapter is another option. None is currently a validated
   plug-in wiring instruction for 3101/Hebrew 5251/Bigfoot.
5. **Extra direct keys / more sense rows:** handle separately. Direct buttons
   can be read as GPIO inputs through a suitable adapter, but genuinely additional
   capacitive sense rows need additional analog sensing hardware. The 3178 now reserves GPIO27/J2.30 for its direct switch while retaining
   all eight capacitive rows. `beamal_badpinout`/`test` definitions must not be treated as ordinary 4-row
   beamsprings merely because of their folder location.

An I²C expander is not a drop-in equivalent for the PIO outputs. The existing
PIO source configures a 10 MHz state-machine clock and samples shortly after
the drive edge. An expander scan must prove comparator sampling against the
actual edge, not merely wait until an I²C transaction completes. At configured
400 kHz, a two-byte expander register write consumes at least 27 clock pulses
(67.5 µs) before START/STOP/software overhead. Setting and clearing each of five
extra outputs therefore adds at least 675 µs per full scan **per calibration bin**,
plus edge/sampling overhead. This is a lower-bound model, not measured scan speed.

The existing LED refresh reconstructs the entire expander output byte using only
LED bits, so it would clear extra scan drives. Expansion needs one authoritative
output shadow, successful-write acknowledgement before shadow commit, serialized
DAC/expander access, fail-safe deassertion and explicit pin ownership. Current
`CONTROLLER_COLS`, mapping arrays, raw scan/level buffers, calibration loops,
protocol serialization and GUI diagnostics also need bounds reviewed for 22/23
channels. Removing a build error alone is not sufficient support.

## Validation before enabling wider ports

- Trace the exact IBM pad-card columns to source signal numbers; do not use a
  QMK coordinate as a numbered edge contact. Compare all layout alternatives.
- Check drive voltage, active polarity, resistance and comparator behavior against
  the actual hardware; scope native and proposed extra drive edges together.
- Test expander-only columns with PIO drives idle. Establish whether row pulses
  remain observable after the bus write, or require synchronized edge capture.
- Verify no overlap between drive channels and guaranteed idle-low on errors,
  startup, LED changes, calibration and bootloader entry.
- Measure full scan cadence, typing/chords and USB scheduling with every active
  bin. Report measured tradeoffs if I²C expansion is too slow.
- Test all populated keys, idle noise, repeated press/release and combinations;
  then test LED/solenoid operation and GUI matrix/live-sensor readback.

The wider ports remain build-blocked until those additional channels are actually
implemented and tested. This is a limitation of current driver/adapter support,
not a claim that the keyboards cannot be supported by Leyden Jar.

## Reproduction checks

```sh
python3 keyboards/leyden_jar/ibm/tools/port_ibm.py --maps-only
python3 keyboards/leyden_jar/ibm/tools/check_matrix_maps.py
python3 keyboards/leyden_jar/ibm/tools/update_pin_readmes.py --check
make leyden_jar/ibm/3277_66key:vial
make leyden_jar/ibm/3277_split_66key:vial
python3 keyboards/leyden_jar/ibm/tools/check_matrix_maps.py --compiled --boards 3277_66key 3277_split_66key
```

`--maps-only` preserves keymaps and personal calibration overrides. Compiled
checks read the mapping/mask bytes from the actual UF2 via ELF load addresses,
including tables initialized into RAM by LTO. Seven existing calibration,
persistence and bootmagic regressions also passed during this audit. No FAT/F122
firmware or connected controller settings were changed.
