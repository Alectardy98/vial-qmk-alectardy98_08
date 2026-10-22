# EC Prox TinyUF2 variant — build and safety report

Status: experimental bootloader/application built and validated offline; **not
physically validated or approved for unattended installation**. Nothing was
flashed, erased or written to physical EEPROM.

## Hardware evidence and assumptions

The user's CubeProgrammer output identifies **STM32F411xC/E, device ID 0x431**.
The original working QMK metadata says STM32F401; it is source configuration,
not a physical chip identification. The original implementation remains unchanged.
The new variant selects QMK STM32F411/CMSIS STM32F411xE. The package marking is
unknown. `512 KB - Default` alone is not a measured flash-size guarantee.

An earlier actual EC42 descriptor read, retained in
`/home/alec/firmware-analysis/analysis/ec_prox_discovery/REPORT.md`, was
`a97500f203ff0001000040000000800002000001`, including a 512 KiB factory-density
reading. Nevertheless this design uses **only the first 256 KiB flash and first
64 KiB RAM**, and storage tests cover both 256/512 KiB factory geometries.

Both the working F401 source and this variant use an 8 MHz HSE configuration,
PLL M=4/N=168/P=4/Q=7, 84 MHz CPU and 48 MHz USB. The F411 generic QMK default
would have changed the CPU to 96 MHz; the variant explicitly overrides that.
A local board-header adapter reuses the original Discovery GPIO defaults,
selects the F411 device macro, and removes HSE bypass. The generic F411 and
original Discovery board.c files are identical. The actual acquisition and
matrix.c files are byte-identical to the original framework Prox.

USB is the F4 OTG FS peripheral on PA11/PA12. The bootloader disables VBUS sensing
as the existing TinyUF2 F411 board does; **physical enumeration still needs a
hardware test**. No guessed discrete LED, UART or button pin is driven by the
bootloader. In particular PA2 remains reserved for the Prox discharge circuit.

Required local reference-manual/errata lookup was attempted using the TinyUF2
read-doc skill. The Calibre library was absent (exit 3). Independent register-
level/errata validation is therefore **pending**; source reuse and builds are not
a substitute. This is a remaining pre-install review item.

## Flash map and persistence

| Region | Range (inclusive) | Treatment |
|---|---|---|
| TinyUF2 sectors 0–1 | 08000000–08007FFF | 32 KiB reserved; code capped at 31 KiB, final 1 KiB CONFIG |
| EEPROM sector 2 | 08008000–0800BFFF | 16 KiB erase unit; 8 KiB QMK wear journal, 4 KiB logical |
| Reserved sector 3 | 0800C000–0800FFFF | No loader/application/storage writes |
| Application sectors 4–5 | 08010000–0803FFFF | 192 KiB application window |
| Higher user flash, if fitted | 08040000 onward | Unused and untouched |
| STM32 system ROM | Separate system memory | Existing factory recovery; never part of these images |

Bootloader vectors start at 08000000. Application vectors start at 08010000;
QMK relocates VTOR using the normal TinyUF2 linker contract. Both sides share
SRAM reset mailbox **2000FFFC**, magic **F01669EF**. The loader reserves the word;
the application explicitly caps its heap there. QK_BOOT and normal Vial entry
use QMK's native TinyUF2 reset path. Both Vial keymaps contain QK_BOOT.

The EC42 record remains **242 bytes** (17+3×75), magic A975, v2.9. Existing enabled
mask, calibration, overrides, RT/Deepest and CRC validation remain shared and
unchanged. Sector-1 records are not read or migrated. First installation requires
fresh Vial setup and manual incremental calibration; uncalibrated keys remain
disabled. Normal first-boot QMK initialization may write defaults to the new
logical EEPROM. No automatic migration or fabricated measurements were added.

The standard QMK journal and current EC42 guards/checksums/readback/error latching
are retained. Interrupted appends and EC42 saves are tested. **QMK's single-sector
journal still has its existing interrupted-consolidation data-loss window**:
corrupt state fails closed and may need restore/recalibration. This does not
provide a redundant physical A/B storage bank or guarantee the previous record
survives every possible power cut. Keep GUI calibration backups off-device.

## TinyUF2 source and safety changes

Checkout `/home/alec/Documents/GitHub/tinyuf2`, base
`5639f2d0b1d99d98c67f39f130662bd1263c216e`.
Pinned dependencies: CMSIS device F4 `2615e866fa48fe1ff1af9e31c348813f2b19e7ec`,
HAL F4 `04e99fbdabd00ab8f370f377c66b0a4570365b58`. Full submodule pins are in
`logs/ec-prox-tinyuf2-build.log`.

New board: `ports/stm32f4/boards/ec_prox/{board.h,board.mk,board.cmake,boot.ld}`.
Adapted from the existing MIT-licensed F411 board; original notices retained.
The board uses the established STM32F4 UF2 family **57755A57**, not a new family.
A separately authored MIT linker script avoids copying the bundled Ac6 linker
script, whose header prohibits distributing modified source.

Small necessary TinyUF2 source changes (none in QMK core):

- `ports/stm32f4/CMakeLists.txt`: optional board linker override; existing boards
  retain their link script. Explicit existing-board allowlists keep self-update,
  erase and demo apps out of the Prox package; existing boards retain those apps.
- `ports/stm32f4/boards.c`: no-op LED when no LED pin is supplied; Prox-only strict
  application reset/SP bounds, and stay in loader after a latched flash error.
- `ports/stm32f4/board_flash.c`: opt-in application-end guard; validate address,
  alignment and 256-byte write length before unlock. Propagate erase/program/wait
  errors and verify programmed bytes. CURRENT.UF2 exposes only the app window.
- `src/ghostfat.c`: same opt-in range/payload checks before flash or completion
  accounting; reject failed writes rather than counting them as successful.

`BOARD_FLASH_APP_END` enables these checks only for this new board. Allowed writes
are 256-byte aligned blocks wholly inside sectors 4–5. Wrong family, malformed
size/flags, arithmetic overflow, loader/storage/reserved targets are rejected.
There are no distributed bootloader-self-update UF2 files. Automatic option-byte
write protection is **disabled**, so startup never changes option bytes. Protection
here is software range enforcement, not a claim that hardware WRP is enabled.

Exact tracked TinyUF2 patch and new board snapshot are included in the artifacts.

## QMK source and GUI integration

New independent `keyboards/alectardy98_ec/ec_prox_uf2/` copies the original hardware
adapter/keymaps and references the same common EC42 engine. Changes are confined
to the variant's MCU/bootloader, local linker/clock/GPIO adapter, sector-2 storage
constants, and UF2 build configuration. The split clean-install packager is
removed from this variant. No QMK quantum/lib/platform source was changed.

Application VID/PID **6369:6B98**, product/manufacturer, Vial UID
**4d2d00bcadf8b927**, matrix 5×15/62 eligible positions, layouts, Vial JSON and pinned
EEPROM build ID **0036E2FF** remain the same intentionally: it is the same physical
keyboard and protocol profile. Firmware ID distinguishes `A98-prox-UF2-2.9`.
The existing standalone GUI profile `src/boardprofile.cpp` already matches these
values. No GUI code change or rebuild is needed; normal and sanitizer tests pass.
TinyUF2 itself is a mass-storage bootloader, not an application EC42 endpoint.

Development changes outside the new board are limited to the existing host
harness accepting the new name and sector-2 storage test cases, plus new tests
under `development/alectardy98_ec/tinyuf2/`.

## Build commands

```sh
# TinyUF2, in its own checkout; dependencies downloaded at their pinned revisions:
python3 tools/get_deps.py stm32f4
python3 .claude/skills/build/scripts/check_build.py --board ec_prox --shared

# QMK, system Python 3.14, no extra packaging command:
make alectardy98_ec/ec_prox_uf2:vial
make alectardy98_ec/ec_prox_uf2:vial_alec

# Original targets rebuilt successfully:
make alectardy98_ec/ec_prox:vial
make alectardy98_ec/ec_prox:vial_alec
```

Normal QMK commands create `.build/alectardy98_ec_ec_prox_uf2_<keymap>.uf2` and the
same named root UF2. Release copies have the requested shorter names below.
Bootloader native build output: `tinyuf2/cmake-build/cmake-build-ec_prox/_bin/`.

## Measurements and artifacts

Artifact directory:
`firmware/alectardy98_ec/ec_prox_uf2/20261022_tinyuf2/`

| Image | Flash span | Remaining window | Data | BSS | Separate stack reserve |
|---|---:|---:|---:|---:|---:|
| vial | 84,964 | 111,644 | 1,520 | 14,944 | 3,072 |
| vial_alec | 88,088 | 108,520 | 1,520 | 14,952 | 3,072 |

`size` Berkeley BSS includes the linker-reserved heap and stacks; the table above
uses actual `.data`/`.bss` sections and reports stacks separately. Conservative
RAM capacity is 65,536 bytes. Exact section LMAs, UF2 spans and vectors are in
`measurements.json` and the artifact validation log. Bootloader BIN span: **15,504 bytes**, leaving **17,264 bytes** in its 32 KiB
reservation. Bootloader data: **728 bytes**, BSS: **8,456 bytes**; its linker
additionally reserves 1,536 bytes for stack/heap.

Files: `ec_prox_tinyuf2.bin`, `.hex`, `.elf`, `.map`; `ec_prox_uf2_vial.uf2` and
`ec_prox_uf2_vial_alec.uf2`, matching BIN/ELF/maps; SHA256SUMS; source snapshots;
TinyUF2 patch; logs; original Prox artifact backup; protected-source hash list.
Use UF2 for subsequent drag-and-drop updates, **never the bootloader BIN**.

## Validation results and limits

- Both new targets clean-built and then rebuilt after final linker updates.
- Both original targets build. Their BIN/ELF bytes match preserved originals.
  One regenerated original map differed only in temporary LTO object paths;
  its previous release copy was restored. Diff is retained in logs.
- All 117 protected source files hash-identical (original Prox, original Cipulot,
  Matrix and 0413 framework source snapshots). QMK core diff is empty.
- Actual new Prox scanner/engine: normal and ASan/UBSan pass incremental/single-key
  calibration, spike rejection, cancellation, deepest/shallow/medium actuation,
  RT movement 4/16/32, disabled HID suppression, CRC/restore/reboot/debounce and
  250 interrupted-save prefixes. All three existing board scanner suites also
  pass normal and sanitizer runs.
- Real QMK EFL/journal/storage tests pass normal/sanitizer for original Prox
  256/512 KiB, Matrix, and new sector-2 Prox 256/512 KiB. Includes Vial canaries,
  sector isolation, geometry/WRP rejection, failure latching and replay.
- New `test_tinyuf2.py` executes actual UF2, flash_write and write-guard functions
  with mocked hardware. Normal/sanitizer pass valid/end-boundary writes, rejected
  protected addresses, overflow/alignment/payload/family/flags, erase/program/wait
  failure propagation, readback, and protected-region canaries. A native 64-bit
  compilation warning from upstream's 32-bit pointer cast is expected in this
  host extraction; ARM firmware builds do not report it.
- `validate_artifacts.py` verifies every flash load section against UF2 data,
  block numbers/family/metadata, all target bounds, reset/SP vectors and mailbox.
- Standalone GUI: **14/14 normal, 14/14 sanitizer** tests pass. Same app identity
  and copied layouts/UID are explicitly checked for the variant.
- Existing acquisition, ASTRO, Prox lighting, record-I/O and 0413 storage tests
  pass normal/sanitizer. Matrix Vial-ID script could not run: existing Matrix
  `.build/.../cflags.txt` was absent. No Matrix firmware was rebuilt or modified.
- TinyUF2 new-board build contract passes. Broader contract also passes two
  representative existing STM32F4 boards and all seven ghostfat fixtures. The
  overall cross-platform run is **incomplete/fails** because unrelated MCU SDKs
  and Docker are unavailable. `pre-commit` is not installed. These checks are
  not represented as successful. Optional make board.mk is not CI validated.
- Local TinyUF2-required agentrc skills were installed by its documented skill
  installer. No hardware skill or flashing workflow was run.

Actual compiled EEPROM ABI for **both** keymaps is identical to original Prox:
core 0..36; EC record 37..278; Vial magic 279; dynamic keymaps start 283; macros
start 2075 with 2021 bytes remaining. Backing remains 8192, logical 4096. The
compiled-config test additionally proves F411-only macro selection and 8/84/48 MHz
HSE/CPU/USB clocks. Only the physical erase sector changes from 1 to 2.

Commands for rerunning the focused offline checks:

```sh
python3 development/alectardy98_ec/tinyuf2/test_tinyuf2.py
SANITIZE=1 python3 development/alectardy98_ec/tinyuf2/test_tinyuf2.py
python3 development/alectardy98_ec/tinyuf2/validate_artifacts.py
python3 development/alectardy98_ec/tinyuf2/test_compiled_storage.py
python3 development/alectardy98_ec/tests/test_port_storage.py
SANITIZE=1 python3 development/alectardy98_ec/tests/test_port_storage.py
python3 development/alectardy98_ec/port-harness.py ec_prox_uf2 /tmp/prox-uf2-host
python3 development/alectardy98_ec/tests/test_ports.py ec_prox /tmp/prox-uf2-host
# Repeat with SANITIZE=1 when compiling the harness.
QT_QPA_PLATFORM=offscreen ctest --test-dir /home/alec/Documents/GitHub/qmk-ec-util/build-universal --output-on-failure
ASAN_OPTIONS=detect_leaks=0 QT_QPA_PLATFORM=offscreen ctest --test-dir /home/alec/Documents/GitHub/qmk-ec-util/build-universal-sanitize --output-on-failure
```

## First installation — future manual procedure, not performed

1. Resolve pending manual/errata review and establish a tested **independent**
   ROM DFU/BOOT0-reset or ST-Link/SWD recovery path. Pin/button availability is
   not established by compilation. Do not use Matrix ASTRO instructions.
2. Back up current flash and GUI calibration/Vial configuration. Old sector-1
   settings will not migrate. Keep the original Prox clean-install fallback.
3. Connect CubeProgrammer through the established ROM DFU or SWD route. Read
   device ID, density and protections. Stop on WRP/RDP/PCROP conflicts; no option
   byte changes or unprotect operations are part of this procedure.
4. With the application halted/in programming mode, selectively erase **sectors
   0, 1, 2, 4 and 5** only. This intentionally destroys the old application and
   settings in those sectors. Sector 3 may retain old inert bytes, but it is
   reserved and never executed or touched by this new layout. No mass erase is
   required. Upper flash and system ROM are not involved.
5. Program `ec_prox_tinyuf2.bin` at **08000000** and verify every programmed byte.
   Reset/reconnect into TinyUF2; erased app sectors should keep it in DFU. Expect
   the mass-storage volume **ECPROXBOOT**. Do not assume success without observing
   enumeration/INFO_UF2.TXT and the expected app start/family.
6. Copy `ec_prox_uf2_vial_alec.uf2` (or vial) onto that volume. Wait for completion
   and application USB re-enumeration. Never copy a bootloader binary as an app.
7. Close Vial, connect EC Utility, verify storage ready/empty calibration, manually
   calibrate one key, Apply & Save, then confirm that only that key works.
8. Add more keys, test actuation/RT/Deepest/lighting/Vial, unplug/replug and check
   saved calibration/settings. Save a GUI backup off-device.

This initial procedure requires a programmer interface; a normal application
UF2 alone cannot install a resident bootloader. Whether the owner's current USB
connection exposes ROM DFU must be confirmed; otherwise ST-Link/SWD is required.
No suggested sector erase has been executed by this task.

## Subsequent updates and recovery

Use QK_BOOT or the existing Vial/GUI bootloader request to write the TinyUF2 SRAM
magic and reset. Copy only this variant's UF2 to ECPROXBOOT. UF2 targets/erase
units are sectors 4–5, so calibration/Vial storage in sector 2 is excluded.
Check calibration persistence after the first real update. Normal TinyUF2
startup includes its double-tap wait before handing off to QMK.

If application USB/key events fail, two resets within TinyUF2's 500 ms double-tap
window are the source-supported loader entry, **provided a physical NRST/reset
route is available**. Its accessibility on this PCB has not been established.
Invalid app vectors keep the bootloader in DFU; a superficially valid but broken
app is not guaranteed to be caught. Reset/re-enter the loader after a flash error;
write errors latch and prevent automatic completion reset. A torn application
update is not an atomic whole-image rollback. Keep a verified UF2 fallback.

If TinyUF2 itself does not start, use the independently established factory ROM
DFU/BOOT0-reset route or SWD. Restore a previously verified full Prox clean-install
BIN at 08000000 using its original selective/clean-install procedure; this
removes TinyUF2 and loses new sector-2 settings. Do not write STM32 option bytes
or assume a software boot key works while every key is uncalibrated. Raw HID
bootloader requests are separately regression-tested with all keys disabled.

## Required physical validation

Confirm bootloader USB/MSD enumeration, application handoff/vector relocation,
QK_BOOT and Raw HID entry (including uncalibrated state), accessible independent
reset/recovery, first initialization/storage health, one-key then incremental
calibration, reboot persistence, both Vial layouts, actuation/RT/Deepest, live
ADC, and RGBLIGHT/layer indicator behavior. Perform an ordinary UF2 update and
verify saved Vial/calibration survive. Do not infer hardware success from the
host tests above; no destructive power-cut test is authorized here.

## Primary artifact SHA-256

- `ec_prox_tinyuf2.bin`: `ad0d04e98c8ef06ffffa2f59db34b6e91b0f2a001dbc34a0ebbc73e948812314`
- `ec_prox_uf2_vial.uf2`: `2c491351911baf1717437e8abd9a545c95176a4fc12226a8907bdc8882fd0738`
- `ec_prox_uf2_vial_alec.uf2`: `e2bffb70c477295fa80f32f65e66ca8ac32eb09fc8b147caa42bc88f071515a9`
