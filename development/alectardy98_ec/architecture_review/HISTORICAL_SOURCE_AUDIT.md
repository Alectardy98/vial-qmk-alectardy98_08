> Historical pre-port audit. Current implementation and hardware status are documented in README.md. Do not use old deployment assumptions as current instructions.

> Historical pre-port source audit. The storage blockers recorded below were
> resolved for the new v2.9 ports using the authorized record/Vial reset. Current
> implementation, maps and test procedure are in ARCHITECTURE.md and TESTING.md.

# Source audit and board comparison

Sources inspected: complete original Class0413 tree; EC Prox `ansi_iso` and all
its keymaps plus `cipulot/common`; Matrix ME EC UF2 root and all three keymaps.
No original source was changed. Before/after per-file SHA-256 evidence and source
archives are in the analysis run referenced by the final report.

| Property | MM Studio Class0413 | Cipulot EC Pro X ANSI/ISO | Matrix Astro ME EC |
|---|---|---|---|
| MCU | STM32F401, reported 128 KiB | STM32F401, configured 256 KiB | STM32F411, configured 512 KiB |
| Clock source/config | 8 MHz HSE, PLL 4/168/4/7 | inherited generic F401 same PLL | 8 MHz HSE, PLL 8/192/2/4 |
| SYSCLK / USB | 84 / 48 MHz | 84 / 48 MHz | 96 / 48 MHz |
| Bootloader | stock TinyUF2, verified existing family | STM32 ROM DFU | rules select TinyUF2 despite JSON stm32-dfu |
| Application | 08010000..0801ffff | vectors 08000000; text 08008000 onward | 08020000..0807ffff configured |
| Matrix | 5x21, 91 sampled | 5x15, 62 sampled | 6x16 scan, no physical exclusion mask |
| Sensor input | PA2 | PA1 | PA2 |
| Rows | PB8 PB7 PB6 PB5 PB3 | PA7 PB0 PA4 PA5 PA6 | PB6 PB5 PB4 PA15 PB15 PB1 |
| Mux enable | PA4 PA5 PA7 | PB7 PB3 | PB10 PB2 |
| Mux selects | PB0 PB1 PB10 | PB4 PB5 PB6 | PB14 PB13 PB12 |
| Discharge | PA3 | PA2 | PA6; opamp PA5 |
| Active polarity | rising ADC on press | rising ADC on press | rising ADC on press |
| Acquisition | existing Cipulot-derived atomic charge/read; 10 us discharge | atomic charge/read, open-drain discharge, 10 us wait | discharge 2 us; drive row/read; clear row; wait 20 us |
| Returned ADC | 0..1023 | 0..1023 | 0..1023 under current shared analog driver |
| Logical EEPROM / requested backing | 4096 / 16384 | 4096 / 8192 | 4096 / 8192 |
| Actual selected erase sector | 2, 16 KiB | 1, 16 KiB | 3, 16 KiB |
| Calibration profile bytes | 219 packed + CRC | 159 legacy, no equivalent transaction CRC | 0 dedicated keyboard bytes |
| GUI EC42 | v2.8 full | absent | absent; raw diagnostic handler commented out |

Evidence locations (original relative paths):
* `mmstudio/class0413_ec/config.h`: pins, constants and sector-2 geometry;
  `matrix.c`: acquisition/eligibility; `modules/calibration.c` and the two
  `actuation_*.inc`: actual capture, scanner decisions, packed record and saves;
  `modules/storage_guards.c` and `geometry.c`: all physical write/erase gates;
  `ld/class0413_ec.ld`: application assertions; `post_rules.mk`: safety wrappers.
* `cipulot/ec_prox/ansi_iso/config.h`: 13 impossible coordinates and mux order;
  `common_cipulot.mk`/inherited post rules: actual common sources;
  `cipulot/common/ec_switch_matrix.c`: `ec_readkey_raw`, `ec_noise_floor`,
  `ec_scan_matrix`, `ec_update_key`; `ec_board.c`: defaults and runtime load;
  `via_ec.c`: EC41-style custom VIA values 1..11 and immediate/explicit writes.
* `matrix/me_ec_uf2/matrix.c`: `sense_key`, `ecsm_update_key`, `matrix_scan_custom`;
  `ec_matrix.h` and `me_ec.c`: calibration/stroke structs and compiled tables;
  the active scanner does not use these tables for key decisions. It compares
  directly with SENSE_HIGH=500 and SENSE_LOW=300. Do not advertise its dormant
  EC_AUTO/EC_CALIBRATE enums as implemented manual calibration or Rapid Trigger.
* Shared `platforms/chibios/drivers/analog.c`: ADCv2 samples at 12 bits and shifts
  by `(12-ADC_RESOLUTION)`; default returned resolution is 10 bits. Matrix's
  compiled 1500..2200 bottom values belong to the inactive alternative model;
  they are not verified measurements usable by a new 10-bit calibration codec.
* `builddefs/common_features.mk`: vendor STM32 EEPROM selects wear_leveling/EFL.
  Generic F401/F411 board configs choose sector 1 without TinyUF2, sector 3 with
  TinyUF2. Thus Matrix FEE_PAGE_BASE_ADDRESS=08060000 is inactive in this build.
* `platforms/chibios/boards/common/ld/stm32f4xx_common.ld`: EC Prox vectors in
  sector 0, excluded sector 1, rest in sector 2 onward. A flat BIN destroys the
  sparse distinction; even a sparse HEX needs an erase-preserving installer.

## Behavior and features

Class0413 preserves indefinite explicit manual Start/Finish/Cancel/Review/Save,
16-scan qualification, incremental enabled-key mask, uncalibrated HID suppression,
per-key/global VA, optional median-filtered extremum RT, immediate RAM updates,
2-second idle persistence, EC41 compatibility/EC42, audio/Clicky/lock sounds,
solenoid and Num/Caps indicators. Music Mode remains disabled as in stable v2.8.
No normalization or timing was changed in its port.

Cipulot legacy calibration averages 30 startup noise-floor samples; its downward
baseline correction and unqualified running-max bottom capture differ from the
0413 sustained-block capture. VIA bottoming stop writes the entire legacy record
and substitutes 1023 for unmeasured/insufficient keys. Old fixed/RT modes are
mutually selected; this is not the new VA-initial-activation armed-cycle engine.
The `vial_alec` post-init hook reapplies personal compiled calibration constants,
which must not silently override future incremental calibration.
RGB lighting is retained, including the existing Vial one-LED RGB matrix config
alongside the root 22-LED rgblight config; no attempt was made to correct physical
LED mapping without evidence.

Matrix active scanner has fixed hysteresis and 10 ms debounce. It does not
implement persistence of its dormant per-key `ec_matrix` values, true RT, or a
live EC42 adapter. It retains IS31FL3236 RGB matrix via I2C1 PB8/PB9 and WS2812
PB7 lighting. JSON indicator key `cqpw_lock` is misspelled; not silently fixed.
The copied bootloader helper contains a BKPT path and differing mailbox address;
its actual reachability/installed bootloader contract need verification. It was
not reused as a universal recovery method.

## USB, layouts and identities

Native QMK/ChibiOS HID and Vial are preserved; no custom USB stack.
* 0413 VID:PID 2024:0319, UID 45464c4449414702.
* EC Prox VID:PID 6369:6b98, UID 4d2d00bcadf8b927.
* Matrix VID:PID 4543:4d45, UID 0ec173239f8f8f2c.
Vial compressed layouts, options and existing keymaps/macros remain unchanged.
GUI layout rendering is runtime Vial/KLE, not a copied 0413 physical arrangement.

## Measured EEPROM map

`firmware/alectardy98_ec/eeprom-map.json` is compiled with actual target flags
(LTO disabled only for the constant-offset probe, not for firmware builds).

| Region start / size | 0413 | Prox | Matrix |
|---|---:|---:|---:|
| Core | 0 / 37 | 0 / 37 | 0 / 37 |
| Keyboard record | 37 / 219 | 37 / 159 | 37 / 0 |
| VIA | 256 / 4 | 196 / 4 | 37 / 4 |
| Dynamic keymaps | 260 / 840 | 200 / 600 | 41 / 768 |
| Encoders | 1100 / 0 | 800 / 0 | 809 / 0 |
| QMK settings | 1100 / 40 | 800 / 40 | 809 / 40 |
| Tap dance | 1140 / 320 | 840 / 320 | 849 / 320 |
| Combos | 1460 / 320 | 1160 / 320 | 1169 / 320 |
| Key overrides | 1780 / 512 | 1480 / 512 | 1489 / 512 |
| Macros | 2292 / 1804 | 1992 / 2104 | 2001 / 2095 |
| Logical end exclusive | 4096 | 4096 | 4096 |

These are `vial` maps. Other keymaps have their own flags; never infer identical
optional-region sizes. Backing request 8192 still requires one whole 16384-byte
F4 erase sector. No physical erase/program was executed.

## Build limitations and release gates

Eight intended baseline keymaps build. Two inherited historical/non-Vial variants
fail independently of the new framework: EC Prox `via` references undeclared
VIAL_MACRO_EXT_* and vial_keycode_* from shared dynamic_keymap.c; Matrix
`vial_alec_old` sets invalid driver IS31FL3731. Their logs are retained. They are
not silently corrected by enabling features or editing shared QMK.

Configured capacities on the additional boards are source assumptions, not new
physical density readings. No new calibration/storage writes or UF2 installation
contract is certified for them. Matrix's missing safe persistent-record placement
and EC Prox's unimplemented 159-byte transaction profile prevent calling this a
finished three-board universal firmware.
