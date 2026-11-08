# Original IBM keyboards on Leyden Jar Rev 3

See [the matrix audit](MATRIX-AUDIT.md) for the corrected controller-family
translation, physical FAT results and regression gate.

These are **development candidates**, derived from the IBM layouts in
`keyboards/xwhatsit/ibm` and the native Leyden Jar PIO/DAC/IO-expander implementation.
They do not use the AVR scanner. Original IBM and Model F Labs reproduction
boards are separate targets; matching product names do not prove matching wiring.

## Build and compatibility

From the QMK repository root:

```sh
make leyden_jar/ibm/fat:vial
make leyden_jar/ibm/fat:vial_alec
```

Replace `fat` and the keymap with a supported row below. Normal QMK compilation
produces an RP2040 UF2. There are 29 imported board definitions and 38 retained
keymap variants. **16 boards / 23 variants build; 13 boards are deliberately
blocked.** A successful build does not verify an IBM connector or calibration.

| Source board | Logical matrix | Keymaps | Rev 3 status |
|---|---:|---|---|
| `3101_3727_3278_87key` | 4×23 | `vial` | **Blocked:** Required drive columns exceed the Rev 3 eighteen-column connector |
| `3178` | 9×13 | `vial`, `vial_alec` | **Blocked:** Logical rows exceed eight; extra direct keys or custom sense wiring need an adapter mapping; AVR direct-key GPIO wiring has no verified Rev 3 equivalent |
| `3276_3278_75key` | 4×20 | `vial`, `vial_alec` | **Blocked:** Required drive columns exceed the Rev 3 eighteen-column connector |
| `3277_66key` | 4×17 | `vial` | **Blocked:** Source connector column numbering needs a verified compact Rev 3 adapter mapping |
| `3277_78key` | 4×20 | `vial` | **Blocked:** Required drive columns exceed the Rev 3 eighteen-column connector |
| `3277_split_66key` | 4×18 | `vial` | **Blocked:** Source connector column numbering needs a verified compact Rev 3 adapter mapping |
| `3278_3279_led_78key` | 5×20 | `vial` | **Blocked:** Required drive columns exceed the Rev 3 eighteen-column connector; AVR direct-key GPIO wiring has no verified Rev 3 equivalent |
| `4978` | 8×22 | `vial` | **Blocked:** Required drive columns exceed the Rev 3 eighteen-column connector |
| `5251` | 4×23 | `vial` | **Blocked:** Required drive columns exceed the Rev 3 eighteen-column connector |
| `5251_hebrew` | 4×23 | `vial` | **Blocked:** Required drive columns exceed the Rev 3 eighteen-column connector |
| `beamal` | 8×12 | `vial`, `vial_alec` | Candidate; physical validation needed |
| `beamal_badpinout` | 16×16 | `vial` | **Blocked:** Logical rows exceed eight; extra direct keys or custom sense wiring need an adapter mapping |
| `bigfoot_fxt_type_1` | 4×23 | `vial` | **Blocked:** Required drive columns exceed the Rev 3 eighteen-column connector |
| `displaywriter` | 8×12 | `vial` | Candidate; physical validation needed |
| `f104` | 8×14 | `vial` | Candidate; physical validation needed |
| `f107` | 8×15 | `vial` | Candidate; physical validation needed |
| `f122` | 8×16 | `vial`, `vial_alec` | Candidate; physical validation needed |
| `f50` | 8×7 | `vial` | Candidate; physical validation needed |
| `f62` | 8×8 | `vial` | Candidate; physical validation needed |
| `fat` | 8×16 | `vial`, `vial_alec`, `vial_finley` | Candidate; physical validation needed |
| `fext` | 8×15 | `vial` | Candidate; physical validation needed |
| `fssk` | 8×12 | `vial` | Candidate; physical validation needed |
| `fssk_v1_0b` | 8×12 | `vial` | Candidate; physical validation needed |
| `fxt_type_2` | 8×12 | `vial` | Candidate; physical validation needed |
| `fxtal` | 8×12 | `vial`, `vial_Karolus`, `vial_alec` | Candidate; physical validation needed |
| `sun_fssk` | 8×16 | `vial`, `vial_alec` | Candidate; physical validation needed |
| `test` | 16×16 | `vial` | **Blocked:** Logical rows exceed eight; extra direct keys or custom sense wiring need an adapter mapping |
| `test2` | 8×12 | `vial` | Candidate; physical validation needed |
| `xtant` | 8×12 | `vial` | Candidate; physical validation needed |

Blocked targets fail in `rules.mk`, before a misleading partial firmware can be
produced. Direct keys need a verified hardware input; matrices exceeding eight
sense lines or eighteen drive lines need a suitable adapter/controller design.
The 3277 66-key variants have 17/18 distinct column lines and could fit with a
verified compact adapter mapping; their inherited connector indices exceed 17.
Their current block is a mapping limitation, not proof they need a larger
controller. No spare MCU pins or external multiplexer wiring are assumed.

## Architecture and mapping

- Native `leyden_jar/{matrix,common,pio_matrix_scan,dac,io_expander,leds,util_protocol}`
  implement scanning, automatic startup calibration, diagnostics, LEDs and DAC.
- Every port declares explicit logical-to-controller row/column maps. Columns are
  obtained by compiling the original board's actual mapping macro. Rows are evaluated from the actual source `CAPSENSE_KEYMAP_ROW_TO_GPIO_ROW`
  macro after selecting its controller and capacitive-row count. Eight-row
  universal definitions reverse 7–0, four-row definitions reverse 3–0, and
  TEST definitions use their source identity mapping. Direct-key rows are
  not treated as capacitive inputs. The F77/F62-specific legacy permutation
  is not used for these universal-controller definitions. **The FAT translation has eight-row physical evidence; other boards still
  require connector verification. Compiler checks are not continuity measurements.**
- Only pads present in the source QMK layout union participate in new-port
  calibration/reporting. Alternative physical layouts are retained. Unused pads
  have bin 255, and unmapped columns cannot index controller arrays.
- Standard new targets use one native calibration bin, with native Model F +10 or
  beamspring −30 DAC-count offsets. Timing and DAC sweep stay native. These
  initial offsets require physical released/held testing on each keyboard;
  they are not imported AVR 12-bit thresholds. The FAT `vial_alec` keymap has a
  separate seven-bin calibration override for Alec’s modified PCB; see its calibration header
  and FAT README. This does not change standard `vial` defaults.
- Native existing boards without the populated-pad mask retain their original
  calibration population. FAT custom keymap LED calls use the IO expander;
  no AVR pin identifier is used to configure an RP2040 peripheral.
- Source Vial geometry, keycodes, layer counts and macros are retained. USB uses
  native Leyden Jar VID/PID 1209:4704, with distinct per-board Leyden Jar Vial UIDs.
  The AVR xwhatsit add-on therefore cannot mistake these for AVR controllers.
- Persistence uses existing RP2040 wear-levelled QMK EEPROM. No new EEPROM
  record or persistent DAC override is introduced. Startup calibration remains
  automatic; diagnostic threshold overrides last until restart.

`ports.json` records source paths, Vial hashes, mappings, technology, UIDs and
blockers. `python3 keyboards/leyden_jar/ibm/tools/port_ibm.py` regenerates the
imported definitions from the current AVR source; it overwrites generated files.
Review source changes before regenerating.

## Rev 3 ownership and evidence

The public Rev 3 KiCad schematic, commit `e01dea552d0a` of
[mymakercorner/Leyden_Jar](https://github.com/mymakercorner/Leyden_Jar), and the
existing local native scanner agree on this allocation:

| Function | Native ownership / connector |
|---|---|
| Eight comparator outputs | RP2040 GP0–GP7, PIO0 inputs |
| Columns 0–7 | GP8–GP15, PIO0 |
| Columns 8–15 | GP18–GP25, PIO0 |
| Columns 16–17 | GP26–GP27, PIO0 |
| DAC and LED IO expander | I2C0, GP16 SDA / GP17 SCL, configured at 400 kHz |
| Solenoid | GP28 enable, GP29 pulse; native haptic implementation |
| J2 pins 1–2, 11–12 | Ground |
| J2 pins 3–10 | Analog sense A_ROW0–A_ROW7 through LMV339 comparators |
| J2 pins 13–30 | Columns 0–17 through 1 kΩ series resistors |
| J2 pins 31–33 | LED0–LED2 |

This establishes controller-side topology, not the ribbon ordering of every
original IBM assembly. Do not wire by product name alone. The suggested
`brand_new_beamspring_ssk` equivalence has not been established: its old native
configuration and the AVR reproduction definition have different matrix sizes.
No equivalence-based mapping was used here.

Manufacturer RP2040 base/errata verification was **unavailable**: the `read-doc`
lookup for RP2040 returned missing
`/home/alec/Documents/calibre-library/metadata.db` (lookup `767000ebf3e6`).
No clock, PIO program, DMA, USB peripheral or external-flash driver was redesigned.
Peripheral electrical/timing acceptance remains pending hardware tests.

## Vi_al diagnostics

Install the separately packaged `leyden_jar` ZIP using **Add-ons → Install from
ZIP**, then restart Vi_al. It exposes General, Calibration, Live Sensors and
Keypress Monitor for known Leyden Jar UIDs. Existing native 0.9.1 controllers
can use it without reflashing. See the add-on README for protocol coverage.

Full signal measurement invokes the existing native 150-step DAC sweep
(350 through 499, with a 100 µs wait per step plus bus/scan overhead).
It pauses normal scanning for that operation. Continuous sweeps occur only
while Live Sensors is open, with a 400 ms gap between completed samples;
leave that page for normal keyboard use. No claimed latency improvement is
inferred from host-side threading. Threshold editing is explicit and read back from RAM. A failed DAC write now
rejects the threshold command instead of acknowledging an unapplied change;
readback is not an electrical voltage measurement.

## User-controlled installation and recovery

1. Confirm the exact IBM assembly, Rev 3 wiring and connector orientation.
   Save the currently working UF2 and export the existing Vial keymap.
2. Install the add-on first if desired; it never flashes firmware automatically.
3. Use the controller's existing BOOTSEL/reset procedure to enter the RP2040
   ROM UF2 loader. Copy only the matching candidate UF2 to its volume.
4. Release every key during startup calibration. Check every physical key in
   Matrix Tester, including optional split keys, modifiers and keypad keys.
5. In Calibration, check populated bins and signal values; test released/held
   margins, simultaneous keys, key release, Num/Caps/Scroll LEDs and solenoid
   only if fitted. Verify Layout options update both diagnostic views.
6. Check remapping persistence through reconnect, held-key release when output
   is disabled, and prompt resumption after leaving Live Sensors. Test normal
   use without continuous sweeps, reboot and bootloader re-entry.
7. To roll back, re-enter the same ROM BOOTSEL loader and copy the preserved
   working Leyden Jar UF2. If settings are incompatible, explicitly clear QMK
   EEPROM and restore the exported map; AVR EEPROM contents are not migrated.

The FAT `vial_alec` candidate has now been flashed and tested on Alec’s modified
Rev 3 assembly. Its eight sampled rows exposed an incorrect imported controller-family map;
see the FAT README for the recorded evidence and remaining tests. Other targets
remain unflashed candidates. No EEPROM was erased or keymap changed in testing.
Full typing acceptance, unmodified-board wiring and scan latency remain
unverified. Build and host tests do not substitute for those checks.
