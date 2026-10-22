# EC Prox STM32F411CEU6 TinyUF2 port

## Result and validation scope

**Implemented and compiled; software-validated experimental installation set.**
The owner confirms the PCB is EC Prox and the chip is STM32F411CEU6 (UFQFPN-48,
512 KiB flash, device ID431). No EC60X configuration is used. No physical device
was opened, reset, flashed, erased or written during this work. Physical TinyUF2
operation has not been demonstrated; the checklist below remains mandatory.

Original Prox, 0413 and Matrix sources and existing original Prox release binaries
are preserved. Original release maps changed only by temporary LTO file paths
when rebuilding; their previous release copies were restored. A pre-build archive
and protected-source hashes are retained in `artifacts/`.

## Hardware configuration and evidence

The working `keyboards/alectardy98_ec/ec_prox` is authoritative for PCB wiring.
Its old metadata selects F401, but the confirmed physical chip requires the F411
CMSIS/startup target for this independent variant. Original firmware is unchanged.

| Item | Implemented source configuration |
|---|---|
| MCU | STM32F411/CMSIS STM32F411xE; confirmed CEU6 |
| HSE | 8 MHz, from original Discovery board.h, no HSE bypass |
| PLL | M4, N168, P4, Q7: 84 MHz CPU/AHB, 42 MHz APB1, 84 MHz APB2, 48 MHz USB |
| Matrix | 5×15, 62 eligible positions; original layouts/keymaps unchanged |
| Row pins | PA7, PB0, PA4, PA5, PA6 |
| ADC | ADC1 on PA1; original ADC configuration and sample routines |
| Mux enables | PB7, PB3 |
| Mux selects | PB4, PB5, PB6 |
| Channels | 0,3,1,2,5,7,6,4 and 0,3,1,2,5,7,6 |
| Discharge | PA2, original 10 us discharge timing |
| USB | OTG FS, PA11 D− / PA12 D+; no external VBUS sensing in TinyUF2 |
| Application RGB | PB12, 22 WS2812, same bitbang driver |
| Application RAM | Conservative first 64 KiB; extra device RAM unused |

The F411 generic QMK defaults would select 96 MHz and Nucleo GPIO defaults.
`mcuconf.h` restores the original 84 MHz clock tree. Local `board.h` reuses the
original Apache-2.0 Discovery GPIO header, removes its F401 device macro, and
selects F411. Original/generic board.c implementations are identical. Compilation
asserts F411-only selection and the 8/84/48 MHz clock constants. `acquisition.c`
and `matrix.c` are byte-identical to the working Prox files.

The crystal marking has not been independently measured. The working firmware's
8 MHz HSE configuration is stronger source evidence than interpreting a photo.
Source pin mappings and clock configuration are verified; electrical timing on
this new image still requires hardware testing.

The read-doc skill's primary MCU manual/errata lookup could not run because
`~/Documents/calibre-library/metadata.db` is absent. Independent manual/errata
review is **pending**, not passed. Flash geometry was cross-checked against the
ChibiOS F4 sector table (`lib/chibios/os/hal/ports/STM32/STM32F4xx/hal_efl_lld.c`,
lines72 onward), ST's F411 CMSIS/HAL sources, TinyUF2's F4 implementation and the
owner's confirmed density. This is not represented as a completed datasheet audit.

## Exact flash map, designed before implementation

The existing pre-implementation design remains deliberately restrictive even
with the physical 512 KiB chip now confirmed. Hardware configuration reports
512 KiB/eight sectors; application allocation uses only sectors4–5.

| Sector | Inclusive addresses | Erase size | Owner |
|---|---|---:|---|
| 0 | 08000000–08003FFF | 16 KiB | TinyUF2 |
| 1 | 08004000–08007FFF | 16 KiB | TinyUF2 reservation; replaces old EEPROM |
| 2 | 08008000–0800BFFF | 16 KiB | Guarded EEPROM journal |
| 3 | 0800C000–0800FFFF | 16 KiB | Reserved, no writes |
| 4 | 08010000–0801FFFF | 64 KiB | Application |
| 5 | 08020000–0803FFFF | 128 KiB | Application |
| 6 | 08040000–0805FFFF | 128 KiB | Reserved, no writes |
| 7 | 08060000–0807FFFF | 128 KiB | Reserved, no writes |

Only **08010000–0803FFFF** is UF2-writable. The bound is exclusive08040000,
independent of physical flash size. Every allowed erase unit is wholly owned
by the application. Firmware updates cannot erase sector2 as a side effect of
writing a nearby byte. There is no bootloader/application/EEPROM overlap.
STM32 factory ROM recovery is separate system memory, not user flash.

Bootloader code/data load images are restricted to the first31 KiB; the final
1 KiB of the 32 KiB reservation is CONFIG. Application vectors are at08010000.
The local linker asserts base/end/mailbox and image size; standard QMK TinyUF2
startup relocates VTOR. ELF section and UF2 payload verification is exhaustive.

## Persistent storage and first-boot behavior

QMK embedded-flash wear leveling remains in use: **8 KiB backing journal, 4 KiB
logical EEPROM**, inside the complete16 KiB sector2 erase unit. The unused half
of that physical sector is reserved, not available to other data. The geometry,
factory density, descriptor bounds, WRP, programming bounds, CRC/readback and
error latching from the working EC42 storage implementation remain enabled.
No safety check is bypassed to make calibration controls available.

Logical allocation, verified by compiling both original and UF2 configurations:

| Logical bytes | Use |
|---|---|
| 0–36 | QMK core settings, including existing lighting settings |
| 37–278 | 242-byte canonical EC42 v2.9 record |
| 279 onward | Existing Vial validity/layout fields |
| 283–2074 | Dynamic keymap/settings region, unchanged |
| 2075–4095 | Macro region, 2021 bytes, unchanged |

Record magic remainsA975; enabled-key flags, calibration cells, global/per-key
actuation, RT/Deepest, overrides and CRC representation remain unchanged. Vial
UID and build ID remain unchanged. Only the physical storage sector moves.
No migration reads original sector1 data. An empty/invalid EC record loads an
empty calibration state in RAM; uncalibrated keys stay disabled. Manual capture
and explicit Apply & Save enable keys incrementally. Resting baseline acquisition
is unchanged. There is no automatic bottom-out calibration or timed workflow.

Erased EEPROM is tested. On first normal application boot QMK/Vial may initialize
its defaults in the new logical EEPROM. Invalid journal or protected/mismatched
storage fails closed and may make GUI calibration read-only/NOT READY; no silent
sector erase or forced unprotect is performed. Recovery then requires a deliberate
sector2 reset/restore by the owner, after diagnosing the failure.

Interrupted writes cannot reach unrelated physical sectors. Existing QMK journal
and EC42 checks detect incomplete/corrupt records; tests exercise interrupted
appends and250 partial-save prefixes. **The retained single-sector QMK journal
has a power-loss data-loss window during consolidation.** This design does not
add redundant physical A/B storage or promise that every power cut preserves the
previous record. Corrupt calibration fails closed; keep off-device GUI backups.

First installation intentionally destroys original sector1 calibration/Vial
settings. Subsequent application UF2 updates exclude sector2 and preserve the
new calibration, Vial keymaps/macros and RGB settings under normal operation.

## TinyUF2 revision, board and necessary changes

TinyUF2 base: `5639f2d0b1d99d98c67f39f130662bd1263c216e`, with the recorded patch.
TinyUSB: `ea1d867ced5259d49679f481e5d4e21a36dc11b0`.
ST CMSIS F4: `2615e866fa48fe1ff1af9e31c348813f2b19e7ec`.
ST HAL F4: `04e99fbdabd00ab8f370f377c66b0a4570365b58`.
Full tool/dependency metadata is in the bootloader build log.

New board: `ports/stm32f4/boards/ec_prox/{board.h,board.cmake,boot.ld}`.
Adapted from the existing MIT-licensed STM32F411 BlackPill configuration, retaining
Ha Thach/Adafruit notices. HSE is changed from that board's25 MHz to Prox8 MHz;
UART/LED GPIO assignments are not borrowed. The loader drives no guessed
indicator, RGB or sampling pin. PA2 is not reassigned to a bootloader UART.

The CI-supported CMake backend and repository build-contract script are used.
No unvalidated make backend or self-updater is provided for this board.

Necessary TinyUF2 changes, all included in `artifacts/tinyuf2.patch`:

- Optional board-local boot linker override; existing boards retain their literal
  default link script. Helper tooling now reads that override instead of inferring
  the wrong memory map; a source-facts regression test covers Prox.
- No-op LED implementation for a board with no discrete LED pin defined.
- Prox-only application SP bounds/alignment, Thumb reset target and address checks.
- Prox-only write bounds/256-byte length/alignment checks before flash unlock;
  erase/program/wait errors propagate and programmed bytes are read back.
- Prox-only UF2 validation before flash or completion accounting; rejected or
  failed blocks are not counted as successful.
- CURRENT.UF2 exposes only the application allocation. A final audit caught
  the prototype conflating physical flash size with app length: its last64 KiB
  could export as zero/non-UF2 blocks. Physical size now remains512 KiB for INFO,
  while file block count and read bounds independently use the192 KiB app region.
  A full actual-GhostFAT native test exports and roundtrips all768 valid blocks,
  including the final address, under normal and ASan/UBSan builds.
- Board-local UF2 flash-size configuration matches512 KiB, bounding tracker RAM.
- Existing-board allowlists exclude Prox from bootloader-self-update, erase and
  demo applications; other boards retain their original apps.

The opt-in `BOARD_FLASH_APP_END` policy is used only by this board. Family ID is
established STM32F4 **57755A57**. Packet magic, family/flags, payload size, address
alignment, overflow and bounds are checked. Actual-source tests prove that erase
selection reaches only sectors4/5 and leaves loader, EEPROM, reserved and upper
flash canaries intact. A failed update keeps the loader from automatic completion
reset; reset/re-enter before retrying after a latched hardware error.

Automatic option-byte protection is disabled. There are no option-byte changes
at startup. Protection is software range enforcement; no claim of configured
hardware WRP is made. Existing WRP/RDP/PCROP conflicts must be inspected, not
silently removed. Bootloader/app updates are not authenticated or atomic whole-
image rollback; wrong-board files must not be installed merely because they
share the STM32F4 family ID.

The independent MIT linker script avoids copying the distribution-restricted Ac6
script bundled with the generic port. Bootloader SP is **2000FFF8** (8-byte
aligned), separately from reset mailbox **2000FFFC**. Application and bootloader
agree on that mailbox and native TinyUF2 magic **F01669EF**.

## QMK structure and preserved features

`keyboards/alectardy98_ec/ec_prox_uf2/` contains the hardware adapter, config/hal/
mcu/board headers, local `ld/ec_prox_uf2.ld`, post hooks, storage diagnostics,
original layout metadata and keymaps. Calibration, actuation, RT, persistence,
protocol and telemetry sources remain under `common/` and are not duplicated.
Both requested keymaps are exact copies of their working Prox counterparts.

Only MCU/clock-startup selection, TinyUF2 linking/entry, storage sector, firmware
identifier and packaging configuration differ. The split clean-install BIN
packager is removed from this variant. Normal make commands generate actual UF2
images; selecting TinyUF2 does **not** install the separate resident bootloader.

### RGB equivalence

- Both keymaps retain PB12,22 LEDs, bitbang driver and the same42 configured modes,
  brightness limit255 and lighting configuration/initialization behavior.
- `vial_alec` retains LED0 OFF/red/green/blue overlays, layer/default-layer hooks,
  override while underglow is off, and unchanged remaining LEDs1–21.
- Keymap.c, rules.mk, config.h and Vial JSON are byte-identical to original Prox.
  Driver sources and actual scan code are unchanged. No extra scan-loop polling,
  PWM/timer driver or RGB Matrix writer was introduced.
- Compiled tests compare LED count/pin, channel offsets, brightness, modes and
  layer capability. Actual QMK overlay rendering passes normal and sanitizer
  tests against the new variant's callbacks.
- Existing source tokens `WS2812_BYTE_ORDER_GBR` and `...RBG` are not enums defined
  by this QMK driver. Its preprocessor currently selects the RGB structure layout
  (r/g/b offsets0/1/2). The port deliberately preserves that **actual compiled
  behavior**, rather than correcting the spelling and changing tested colors.
- Standard QMK RGB EEPROM/VIA handlers remain; existing Vial definition/menu
  availability is unchanged. RGB Matrix/VialRGB was not restored or newly enabled.
  RGB persistence uses the same logical core bytes, now protected in sector2.

Physical colors, suspend/resume and scan timing with LEDs enabled remain on the
hardware checklist. Source equivalence and mocks do not prove those electrically.

## Identity and GUI

Application VID/PID6369:6B98, manufacturer/product, Vial UID4d2d00bcadf8b927 and
pinned Vial EEPROM build ID0036E2FF remain identical. Compiled DeviceDescriptor,
ConfigurationDescriptor, strings and compressed Vial definition match original
Prox exactly. FirmwareInfo reports `A98-prox-UF2-2.9`.

The loader deliberately has the same VID/PID but product `EC Pro X TinyUF2` and
volume `ECPROXBOOT`. Its compiled descriptors contain **MSC/SCSI/Bulk-Only only**,
no Raw HID; HID discovery therefore does not mistake it for an EC42 application.

No GUI runtime change or new board profile is necessary. Same UID,75 cells,
242-byte record and A975 magic match the Prox profile. The v2.9 descriptor accepts
sector2 offsets8000..C000 with density512 and verified health. Test transport now
runs the unchanged GUI against the actual UF2 board scanner/engine with those
values, including calibration, live states, settings, save/readback and restore.

Only standalone GUI CMake/test code changed: add the UF2 harness/transport case;
replace active `live_protocol_offline` (unsupported v2.8/219-byte fixture) with
current v2.9 `framework_prox_uf2_live`. Historical test source remains available;
no v2.8 runtime compatibility was added. The GUI executable rebuild is byte-
identical to the preserved known-good executable:
`bf62725b898db0bce8362c689d71a230bbd4c93fd97de7848e3a585cbcdafe3c`.

## Build and validation

```sh
# TinyUF2 repository:
python3 tools/get_deps.py stm32f4
python3 .claude/skills/build/scripts/check_build.py --board ec_prox --shared

# QMK, normal commands, system Python3.14.7 / ARM GCC16.2.0:
make alectardy98_ec/ec_prox_uf2:vial
make alectardy98_ec/ec_prox_uf2:vial_alec
make alectardy98_ec/ec_prox:vial
make alectardy98_ec/ec_prox:vial_alec

# Standalone GUI; runtime code unchanged:
cmake -S . -B build-universal
cmake --build build-universal -j4
QT_QPA_PLATFORM=offscreen ctest --test-dir build-universal --output-on-failure
cmake -S . -B build-universal-sanitize -DEC_ENABLE_SANITIZERS=ON
cmake --build build-universal-sanitize -j4
ASAN_OPTIONS=detect_leaks=0 QT_QPA_PLATFORM=offscreen ctest --test-dir build-universal-sanitize --output-on-failure
```

Both new targets were clean-built. Both originals rebuild; original release
BIN/ELF are unchanged. Root/.build output names are
`alectardy98_ec_ec_prox_uf2_vial.uf2` and `..._vial_alec.uf2`.

| Target | Linked flash bytes | Address span | Free app bytes | Data | BSS | Stacks |
|---|---:|---:|---:|---:|---:|---:|
| vial | 84,962 | 84,964 | 111,644 | 1,520 | 14,944 | 3,072 |
| vial_alec | 88,086 | 88,088 | 108,520 | 1,520 | 14,952 | 3,072 |

Bootloader BIN/address span: **15,492 bytes**, free in32 KiB reservation:
**17,276 bytes**. Data728/BSS6664, plus1536-byte linker heap/stack
reserve. Application span includes small alignment padding; UF2 payloads round
up to256-byte blocks. Berkeley `size` aggregates stack/heap reservation into BSS;
the table reports actual `.data`/`.bss` separately.

- TinyUF2 Prox build contract: **PASS**; all ELF flash sections match its BIN.
- UF2 parser: every512-byte block/header/family/address/payload matches ELF loads;
  all target bytes in sectors4/5. Native boot jump/magic, aligned SP and invalid
  vector rejection are tested with actual functions and mocked IO.
- New write/erase/entry tests: **PASS normal and ASan/UBSan**, including protected
  addresses, overflow/length/alignment, program/erase/wait failures and readback.
- Actual current EC42 scanner/engine: **PASS normal and ASan/UBSan** for Prox UF2
  and all three existing boards. Includes incremental calibration, disabled-key
  suppression, noise/spikes, long sessions/cancel, shallow/medium/deepest,
  overrides, RT4/16/32, telemetry, debounce, reboot, CRC and interrupted saves.
- Real QMK EFL/journal guards: **PASS normal and ASan/UBSan**, sector2 placement,
  Vial/core canaries, replay/consolidation, errors, protection and density checks.
- RGB hooks/rendering and compiled configuration/storage/USB equivalence: **PASS**.
- GUI: **14/14 normal;14/14 ASan/UBSan**, including current Prox UF2 endpoint and
  unchanged three existing board profiles. No physical HID test was run.

Broader TinyUF2 cross-platform validation was attempted, not hidden: representative
existing F4 boards and all seven ghostfat fixtures pass; overall build contract
cannot pass because unrelated MCU SDKs and Docker are absent. Tooling unit tests
also encounter missing NXP/F3 SDK files. The introduced linker-introspection
regression was corrected and Prox source-facts coverage added; remaining errors
are six missing NXP/F3 SDK prerequisites (one test skipped), not current Prox
code assertion failures. `pre-commit` is unavailable. Full raw outcomes are
retained in logs; these broader checks are **not** claimed passed.

Independent primary manual/errata review and all physical measurements remain
pending. This package is a controlled-test candidate, not hardware certification.

Focused offline commands from QMK root:

```sh
python3 development/alectardy98_ec/tinyuf2/validate_artifacts.py
python3 development/alectardy98_ec/tinyuf2/test_compiled_storage.py
python3 development/alectardy98_ec/tinyuf2/test_tinyuf2.py
python3 development/alectardy98_ec/tinyuf2/test_flash_erase.py
python3 development/alectardy98_ec/tinyuf2/test_boot_entry.py
python3 development/alectardy98_ec/tinyuf2/test_current_uf2.py
# Repeat the last4 with SANITIZE=1.
python3 development/alectardy98_ec/tests/test_port_storage.py
SANITIZE=1 python3 development/alectardy98_ec/tests/test_port_storage.py
EC_PROX_VARIANT=ec_prox_uf2 python3 development/alectardy98_ec/tests/test_prox_lighting.py
EC_PROX_VARIANT=ec_prox_uf2 SANITIZE=1 python3 development/alectardy98_ec/tests/test_prox_lighting.py
python3 development/alectardy98_ec/port-harness.py ec_prox_uf2 /tmp/prox-uf2-host
python3 development/alectardy98_ec/tests/test_ports.py ec_prox /tmp/prox-uf2-host
```

## Installation artifacts

Canonical directory: `development/alectardy98_ec/tinyuf2/artifacts/`.

- `ec_prox_tinyuf2.bin` — bootloader; **program at08000000 only during first install**.
- `ec_prox_uf2_vial.uf2`, `ec_prox_uf2_vial_alec.uf2` — application-only updates.
- Matching application BIN/ELF/map and bootloader HEX/ELF/map; `measurements.json`.
  Application BINs are debug/programmer images linked at08010000: **never program
  these at08000000 or confuse them with the original clean-install carrier**.
- SHA256SUMS, build/test logs, TinyUF2 patch and board/source snapshots, known-good
  GUI executable backup and original Prox artifact backup.

Do not install previous experimental files from the older firmware-analysis
snapshot merely because the names look similar. Use this directory/manifest.
No ordinary BIN is renamed as UF2.

| Primary artifact | SHA-256 |
|---|---|
| ec_prox_tinyuf2.bin | `3b92833d521bd2d809b273bebd98c0a6f009ce58b4a049f804e4a145e8a76a31` |
| ec_prox_uf2_vial.uf2 | `054981b432c2e9cfd4663f325eef2550f62fe11919ba6011a131005c08461a6f` |
| ec_prox_uf2_vial_alec.uf2 | `64fe3b5c4d1355d5b23f0e713ae1b3caeaeb4596bd254c5c4f62050c9828d386` |

## First installation — owner-operated, not executed

1. **Before erasing anything**, establish independent STM32 ROM DFU/BOOT0-reset or
   SWD/ST-Link recovery. Current QK_BOOT working in the old application is not
   proof of recovery when an application fails. Inspect the actual Prox PCB pads;
   the original README mentions exposed reset pins but does not establish every
   signal electrically. Do not short unknown pins or use Matrix ASTRO procedures.
   Resolve pending manual/errata review before treating this as fully approved.
2. Save a flash dump, Vial export and EC calibration backup off-device. Retain
   the original Prox clean-install BIN and its hash. First install loses old
   calibration/Vial/RGB settings; no automatic migration is supplied.
3. Connect CubeProgrammer through the independently verified ROM DFU or SWD
   route. Read device identity,512 KiB capacity and protections. Stop on WRP/RDP/
   PCROP issues. Do not change option bytes or select forced unprotect.
4. While halted/in programming mode, selectively erase **sectors0,1,2,4,5 only**.
   This removes old vectors, old sector1 settings, the new storage sector and the
   application target. Sector3 can retain inert old bytes: it is reserved and
   never executed by this layout. Sectors6/7 and system ROM remain untouched.
   **Full-chip erase is unnecessary.** An incomplete erase is not a clean install.
5. Program `ec_prox_tinyuf2.bin` at **0x08000000**, verify bytes. Keep any BOOT0
   recovery strap in the normal user-flash boot state before reconnect/reset.
   Erased application vectors should leave TinyUF2 in DFU. Expect mass-storage
   volume **ECPROXBOOT** and INFO_UF2.TXT identifying this board/application base.
   If it does not enumerate, stop and use the independent recovery route.
6. Copy `ec_prox_uf2_vial_alec.uf2` (or vial) to that drive. Wait for copy/flush and
   application USB re-enumeration. A bootloader BIN is not a drag-and-drop UF2.
7. Close Vial, open EC Utility, check EC42v2.9,5×15/62 eligible,242-byte record,
   sector2/storage ready and initial keys disabled. Manually calibrate/save one
   key; test it. Add keys incrementally; verify reboot persistence and save a GUI
   backup off-device before further testing.

A programmer interface is needed for the first resident-loader installation.
Use STM32 ROM USB DFU if accessible independently on this board; otherwise use
SWD/ST-Link. QMK's TinyUF2 selection alone does not install a bootloader.

## Subsequent updates and recovery

- QK_BOOT, Vial and existing GUI bootloader requests use native QMK TinyUF2 SRAM
  magic/reset. Both keymaps retain QK_BOOT. Raw HID bootloader requests work in
  host tests even when every key is uncalibrated/disabled.
- Copy only the correct application UF2 to ECPROXBOOT. The writer erases only
  sectors4/5 and excludes sector2. Check retained calibration, Vial and RGB after
  the first real update. Normal boot has TinyUF2's500 ms double-tap delay.
- If the app fails but the loader is intact, a physical NRST/reset route permits
  two resets inside that500 ms window. The actual accessible pads/button need
  owner verification; do not assume holding an unspecified key enters TinyUF2.
  Invalid vectors keep the loader in DFU. Valid-looking but broken code may
  still crash: independent reset/ROM/SWD recovery is essential.
- If TinyUF2 itself fails, use independently verified ROM DFU/BOOT0-reset or SWD.
  Reprogram/verify the loader, or restore the preserved original Prox clean-
  install BIN at08000000 using its verified clean-install procedure. A restore
  replacing the loader changes back to the original split layout/sector1 storage
  and loses new sector2 calibration. Selective erase of the old image's covered
  sectors is sufficient; no mass erase or option-byte operation is prescribed.
- After any detected flash error, reset/re-enter before retrying. A torn UF2
  update is not whole-image rollback. An interrupted EEPROM consolidation can
  require backup restore/recalibration; do not mistake fail-closed for data loss
  protection in every power-cut scenario.

## Physical validation before declaring stable

Verify independent recovery first, then bootloader MSD enumeration, normal app
handoff, QK_BOOT, Vial/GUI loader entry, both keymaps, initial empty storage,
one-key/incremental calibration, all physical keys, live ADC and actual logical
states, shallow/deepest and per-key actuation, RT off/three sensitivities,
partial-travel release/reactuation, unplug/replug persistence, Vial keymaps/macros,
RGB brightness/effects/layer colors/locked layers, suspend/resume and sustained
normal typing with lighting active. Finally perform a normal UF2 app update and
check that calibration/Vial/RGB settings survived. No destructive power-cut test
is authorized or claimed here.

## Licensing and scope

Cipulot's original notices and GPL terms remain in hardware/keymap files;
framework notices remain in common EC42 code. TinyUF2's MIT/Ha Thach/Adafruit
notices are retained, and ChibiOS's Apache notices remain in reused headers.
No original keyboard source or upstream QMK core changed. No EC60X port remains.

## Preserved original fallback and additional compiled evidence

Original `vial` linked flash85062 bytes; `vial_alec`88190 bytes. Their original
split-layout carrier spans are117352/120480 bytes. These are not comparable to
application-only UF2 file length; the new images use a different application base.
Data1520 and actual BSS14944/14952 are unchanged, with3072-byte stack reservations.

Preserved fallback: `firmware/alectardy98_ec/ec_prox/vial_alec/ec_prox.clean-install.bin`
SHA256 `0a12808075619cc4e1dedfd2e3ff4af6357b98b0f50655c9aa85ea3b8a6c3874`.
This is the original-layout image, not a TinyUF2 update. Pre-build release files
are also archived in `artifacts/pre-build-prox-release.tar.gz`.

Compiled bootloader handoff is retained in `logs/prox-final-handoff-disassembly.log`:
VTOR is set, followed by MSP/PSP writes and `bx r4`; no stack accesses occur after
the stack switch. Bootloader code has16252 bytes left in its31 KiB code budget,
plus the separate1024-byte CONFIG reservation (17276 bytes unoccupied overall).
No on-device handoff claim is made from this inspection.
