# Original IBM keyboards on Leyden Jar Rev 3

See [the matrix audit](MATRIX-AUDIT.md) for the corrected controller-family
translation, physical FAT results and regression gate.

Each board's README includes an ASCII Rev 3 controller reference, every J2/J3/J4
pin, and its own logical row/column assignments. Blocked ports show their missing
connections explicitly; these are not complete soldering guides. QMK coordinates
are not original IBM ribbon pin numbers. Follow the board links in the table below.

The separate Model F Labs reproduction targets have their own references:
[B104](../b104/README.md), [B122](../b122/README.md),
[F104](../f104/README.md), [F122](../f122/README.md),
[F62](../f62/README.md), [F77](../f77/README.md), and the legacy
[Beamspring Full](../brand_new_beamspring_full/README.md) /
[Beamspring SSK](../brand_new_beamspring_ssk/README.md) targets.
The legacy entries have unverified mappings and existing build issues.

**Connector capacity is not a blanket support verdict.** The populated-signal
[audit](DRIVE-CAPACITY.md) now counts actual layout positions and independent
column signals, rather than rejecting boards by their inherited column numbers.
The 17-column `3277_66key` and 18-column `3277_split_66key` have compact Rev 3
adapter maps and compile. Wider matrices need additional drive support; spare
I/O-expander outputs provide a possible route, not yet a validated scanner.
Displaywriter and TEST-controller definitions remain separate wiring families.

Refresh these documentation sections after changing mappings with
`python3 keyboards/leyden_jar/ibm/tools/update_pin_readmes.py`; use `--check` to
check for stale references without writing files. The tool does not build or flash firmware.

The [3276 port](3276_3278_75key/README.md) uses GPIO4-7 after
removal of U6. J4 remains available for the solenoid. The installed two-bin profile passed a full
75-key physical sweep.

These are **development candidates**, derived from the IBM layouts in
`keyboards/xwhatsit/ibm` and the native Leyden Jar PIO/DAC/IO-expander implementation.
They do not use the AVR scanner. Original IBM and Model F Labs reproduction
boards are separate targets; matching product names do not prove matching wiring.

## Build and compatibility

All Leyden Jar Vial variants now include `keyboards/leyden_jar/vial_features.mk`:
four dynamic layers and Vial's normal EEPROM-dependent feature capacities replace
the AVR-era layer/entry limits. QMK settings/Auto Shift, tap dance, combos, key
overrides, repeat/alternate repeat, Caps Word, layer lock, dynamic macros,
mouse keys, NKRO, Unicode and tri-layer support are compiled in. Features such
as Auto Shift still follow their normal settings rather than being forced active.
Console/command debugging and unsupported hardware drivers are not enabled.

**Export your Vial configuration before upgrading or rolling back.** Four layers,
expanded tables and the new keyboard data block change the EEPROM layout; QMK
may initialize its settings on the first boot. Restore the exported configuration
afterward. Never reinterpret an old EEPROM image as the new layout.

Protocol 0.10.0 adds persistent calibration offsets in a versioned, checksummed
48-byte keyboard data block within the existing 4096-byte logical EEPROM. The
underlying RP2040 wear-levelled flash allocation stays unchanged. Startup auto
calibration still measures references; saved offsets are relative to those
references and tied to the compiled wiring/bin configuration. Bad or mismatched
records fall back to defaults. GUI **Restore calibration defaults** clears only
the overrides; QMK EEPROM clear also restores calibration defaults in RAM.
The legacy native threshold command remains RAM-only for compatibility.

Host regressions: `python3 keyboards/leyden_jar/tests/test_calibration.py`.

The board overrides current QMK's weak `bootmagic_scan()` hook to keep the
native startup safeguard active. The former `bootmagic_lite()` override did
not disable the current upstream check. Software BOOT, mapped `QK_BOOT` and
physical RP2040 BOOTSEL recovery remain available. Regression:
`python3 keyboards/leyden_jar/tests/test_bootmagic.py`.

Alec's F122 `vial_alec` has a keymap-local +16 startup offset: a released
capture had a median of 379 and two pads near 390, above the prior +10 trigger
of 389. This leaves at least five counts of released margin on that capture
without adding scan passes or changing the connector map. Standard `vial`
keeps its original default. Regression with the recorded F122 fixture:
`python3 keyboards/leyden_jar/tests/test_f122_calibration.py`. Physical held-key
and chord margins still require acceptance testing on the installed assembly.

From the QMK repository root:

```sh
make leyden_jar/ibm/fat:vial
make leyden_jar/ibm/fat:vial_alec
```

Replace `fat` and the keymap with a supported row below. Normal QMK compilation
produces an RP2040 UF2. There are 29 imported board definitions and 38 retained
keymap variants. **20 boards / 29 variants build; 9 boards remain blocked by the current
scanner or missing direct-key wiring.** A successful build does not verify an IBM connector or calibration.

| Source board | Logical matrix | Keymaps | Rev 3 status |
|---|---:|---|---|
| [3101_3727_3278_87key](3101_3727_3278_87key/README.md) | 4×23 | `vial` | **Blocked by current support:** 23 populated drive signals exceed the Rev 3 eighteen-column scanner |
| [3178](3178/README.md) | 9×13 logical; 8×12 capacitive + switch | `vial`, `vial_alec` | Builds; GPIO27/J2.30 blue switch; physical acceptance required |
| [3276_3278_75key](3276_3278_75key/README.md) | 4×20 | `vial`, `vial_alec` | GPIO4-7 modified header after U6 removal. 75-key physical sweep passed on the installed modified header |
| [3277_66key](3277_66key/README.md) | 4×17 | `vial` | Builds; compact wiring adapter required; physical test pending |
| [3277_78key](3277_78key/README.md) | 4×20 | `vial` | **Blocked by current support:** 20 populated drive signals exceed the Rev 3 eighteen-column scanner |
| [3277_split_66key](3277_split_66key/README.md) | 4×18 | `vial` | Builds; compact wiring adapter required; physical test pending |
| [3278_3279_led_78key](3278_3279_led_78key/README.md) | 5×20 | `vial` | **Blocked by current support:** 20 populated drive signals exceed the Rev 3 eighteen-column scanner; AVR direct-key GPIO wiring has no verified Rev 3 equivalent |
| [4978](4978/README.md) | 8×22 | `vial` | **Blocked by current support:** 22 populated drive signals exceed the Rev 3 eighteen-column scanner |
| [5251](5251/README.md) | 4×23 | `vial` | **Blocked by current support:** 22 populated drive signals exceed the Rev 3 eighteen-column scanner |
| [5251_hebrew](5251_hebrew/README.md) | 4×23 | `vial` | **Blocked by current support:** 23 populated drive signals exceed the Rev 3 eighteen-column scanner |
| [beamal](beamal/README.md) | 8×12 | `vial`, `vial_alec` | Builds; physical validation required |
| [beamal_badpinout](beamal_badpinout/README.md) | 16×16 | `vial` | **Blocked by current support:** Logical rows exceed eight; extra direct keys or custom sense wiring need an adapter mapping |
| [bigfoot_fxt_type_1](bigfoot_fxt_type_1/README.md) | 4×23 | `vial` | **Blocked by current support:** 23 populated drive signals exceed the Rev 3 eighteen-column scanner |
| [displaywriter](displaywriter/README.md) | 8×12 | `vial` | Builds; physical validation required |
| [f104](f104/README.md) | 8×14 | `vial` | Builds; physical validation required |
| [f107](f107/README.md) | 8×15 | `vial` | Builds; physical validation required |
| [f122](f122/README.md) | 8×16 | `vial`, `vial_alec` | Builds; physical validation required |
| [f50](f50/README.md) | 8×7 | `vial` | Builds; physical validation required |
| [f62](f62/README.md) | 8×8 | `vial` | Builds; physical validation required |
| [fat](fat/README.md) | 8×16 | `vial`, `vial_alec`, `vial_finley` | Builds; physical validation required |
| [fext](fext/README.md) | 8×15 | `vial` | Builds; physical validation required |
| [fssk](fssk/README.md) | 8×12 | `vial` | Builds; physical validation required |
| [fssk_v1_0b](fssk_v1_0b/README.md) | 8×12 | `vial` | Builds; physical validation required |
| [fxt_type_2](fxt_type_2/README.md) | 8×12 | `vial` | Builds; physical validation required |
| [fxtal](fxtal/README.md) | 8×12 | `vial`, `vial_Karolus`, `vial_alec` | Builds; physical validation required |
| [sun_fssk](sun_fssk/README.md) | 8×16 | `vial`, `vial_alec` | Builds; physical validation required |
| [test](test/README.md) | 16×16 | `vial` | **Blocked by current support:** Logical rows exceed eight; extra direct keys or custom sense wiring need an adapter mapping |
| [test2](test2/README.md) | 8×12 | `vial` | Builds; physical validation required |
| [xtant](xtant/README.md) | 8×12 | `vial` | Builds; physical validation required |

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
