# Alectardy98 EC42 v2.9 — canonical framework and porting reference

## Attribution and licensing

**Cipulot's electrocapacitive keyboard firmware is a substantial foundation of
this framework.** Acquisition and matrix scanning, resting-baseline handling,
EC runtime structures, calibrated threshold scaling, and extremum-based
press/release behavior were adapted from
[Cipulot's QMK EC implementation](https://github.com/qmk/qmk_firmware/blob/master/keyboards/cipulot/common/ec_switch_matrix.c)
and its [EC data structures](https://github.com/qmk/qmk_firmware/blob/master/keyboards/cipulot/common/ec_switch_matrix.h).
Original Cipulot copyright notices and GPL-3.0-or-later terms are preserved.
EC42 transactions, incremental qualification, explicit storage records and the
later VA-first/Deepest controls are subsequent framework work. This credit does
not imply endorsement or collaboration by Cipulot.

QMK/TMK/Vial, ChibiOS, the original board implementations, and Purdea Andrei's
Cap-Util also contributed code or interfaces. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md),
[licensing scope](LICENSE.md), and the repository's
[full provenance audit](../../development/alectardy98_ec/licensing/REPORT.md).
Unheaded Matrix imports and stock `DUMP/` redistribution permissions remain
unresolved; no new blanket license or public-release clearance is asserted.


This is the authoritative architecture for current firmware. All three targets
implement **EC42 major 2 / minor 9, storage format 1 / method 0x42** with one explicit
record schema and one calibration/settings/VA/RT engine. The owner reported the
previous firmware working; the new 0413 storage/engine integration is software
validated and requires physical retesting. No hardware was flashed by the agent.

## 1. Scope, version matrix and safety rules

| Target | MCU / matrix | Protocol | Record bytes | Record magic | Calibration reset |
|---|---|---|---:|---|---|
|class0413_ec|F401,5×21 /91 eligible|2.9|332|0xA105|Required from legacy 219-byte v2.8|
|ec_prox|F401,5×15 /62 eligible|2.9|242|0xA975|Existing canonical records preserved|
|me_ec_uf2|F411,6×16 /96 electrical|2.9|305|0xA996|Existing canonical records preserved|

The different record lengths are geometry, not different codecs. Sensor values
are rising-polarity 10-bit ADC counts on all current boards. The raw acquisition
and physical storage/bootloader differences are intentional and must remain local.
No automatic bottoming, timed calibration or legacy calibration migration exists.
No physical travel distance/voltage calibration is claimed.

**Rules for developers and AI agents**

- Preserve source, ELF/map/install image, matching GUI and SHA-256 hashes before
  editing. Original cipulot/matrix/mmstudio keyboard implementations are references.
- Inspect real hardware acquisition, installed bootloader and storage before
  changing shared code. Do not assume pins, ADC timing, polarity or flash geometry.
- Shared changes require builds/regressions for all three boards. Successful
  compilation and mocked tests never prove analog, lighting or power-loss behavior.
- Never bypass CRC, geometry, protection, released-key or revision checks. Never
  invent bottoms, silently truncate macros or erase unrelated settings.
- Never flash/reset hardware, write physical EEPROM, change option bytes or
  replace bootloaders without explicit owner authorization.
- Preserve known-good releases and independent recovery. Update tests/docs with
  behavior changes. Avoid speculative refactors and unnecessary protocol versions.

## 2. Source map and ownership

```text
keyboards/alectardy98_ec/
  README.md, ARCHITECTURE.md, TESTING.md
  DUMP/                            immutable hardware evidence (redistribution not cleared)
  common/interfaces/
    ec42_protocol.h               canonical version, command/status enums, sizes
  common/engine/
    ec_switch_matrix.h            canonical packed record and RAM scanner types
    calibration.h/.c              explicit capture state, CRC, validation, save
    record_storage.inc            ONE explicit per-key persistence codec
    actuation_control.inc         VA edits/debounce, filtered extremum-based RT
    ec_diagnostic.h/.c            command dispatch, cached telemetry, backup/restore
    calibration_console.c                      calibration console printing; old VIA writes rejected
  common/runtime/
    ec_board.c                    shared empty/init/load/post-init lifecycle
    decision.c                    ONE key decision function and portable sample adapter
    integration.c                 Prox/Matrix USB lifecycle and deferred boot request
    record_io.c                   optional split logical-record placement adapter
    storage.c, diag.h             guarded STM32F4 EFL/journal backend
    storage_version.h             QMK build-date helper; not a Vial ID pin
  class0413_ec/
    matrix.c                      preserved ADC/mux/scan and noinline rescale helper
    class0413_ec.c/.h              native feature hooks/keycodes/quiet USB integration
    config.h, rules.mk, post_rules.mk, keyboard.json, halconf.h, mcuconf.h
    modules/                      geometry, S2 guards/health, native actuators/time math
    ld/class0413_ec.ld             64 KiB app at08010000
    keymaps/{vial,vial_alec}/      keymap.c, config/rules, Vial JSON
    tests/verify_size_changes.py   current time-conversion and matrix-mask tests
  ec_prox/
    acquisition.c, matrix.c        preserved hardware acquisition; shared processing
    ec_prox.c                     native lighting/layer hooks
    config.h, post_config.h, rules.mk, post_rules.mk, keyboard.json, halconf.h, mcuconf.h
    storage_diagnostics.c          permanent observer of shared flash guards
    storage_version.h             pinned Vial identity
    clean_install.py              normal-make FF-gap BIN packaging
    keymaps/{default,60_hhkb,via,vial,vial_alec}/
  me_ec_uf2/
    matrix.c, me_ec_uf2.c          preserved sensing and native hooks
    astro_bootloader.c            RTC backup-register native QMK handoff
    is31fl3236_map.c               physical lighting-channel map
    config.h, rules.mk, post_rules.mk, keyboard.json, chconf.h, halconf.h, mcuconf.h
    storage_version.h             schema-specific pinned Vial IDs
    ld/me_ec_boot_STM32F411xE.ld    ASTRO sector5 app at08020000
    keymaps/{vial,vial_alec}/      supported releases
    keymaps/vial_alec_old/         historical buildable target; not release default
  util/
    CMakeLists.txt, src/, tests/, docs/, COPYING, NOTICE.md
    build-universal/qmk-ec-util    generated, ignored Linux executable
```

`development/alectardy98_ec/` owns `build-baselines.sh` (six standard QMK
builds), `test.sh` (normal and ASan/UBSan), `verify-builds.py` (all six images),
and `port-harness.py` (current-source host compilation). `tests/` holds hardware
models, scanner/transaction suites, flash-journal tests and ASTRO/Vial checks.
Host JSON templates contain only IO/fault-injection/acquisition models, never a
frozen engine. Missing current-source tooling is a CMake configuration error.
The real adapter is direct C functions/configuration; no runtime plugin registry
or callback abstraction is needed. GUI sources are never added to QMK SRC.
Historical source audit and cleanup evidence are under
`development/alectardy98_ec/architecture_review/`.

### Current cleanup validation

Only v2.9 firmware is admitted. No radix decoder, old telemetry parser, hidden
Apply & Save settings workflow or alternative RT engine is active. Reserved
wire opcodes remain reserved. The auxiliary EC41 boot envelope is retained only
because the current v2.9 GUI uses it for native recovery; it is not calibration
or earlier-version negotiation. Current record sizes and addresses are unchanged.
See the [cleanup report](../../development/alectardy98_ec/architecture_review/CLEANUP_REPORT.md)
for measured flash/RAM savings, artifact hashes, test commands and physical retests.

## 3. Execution and board adapter contract

1. ChibiOS/QMK initializes USB/clocks and guarded normal wear-level EEPROM.
   Factory density, trusted descriptors, application bounds, WRP and sector/line
   checks gate every destructive operation. No option-byte writes or mass erase.
2. matrix_init_custom initializes physical circuitry and measures30 resting
   samples/scans. All keys must be released at boot. Baselines are RAM-only.
3. common/runtime/ec_board.c initializes empty RAM, reads the canonical record,
   ec_va_load validates magic/method/CRC, then ec_cal_runtime installs thresholds.
   Missing/foreign/corrupt/legacy records leave all keys disabled. EC initialization
   never repairs EEPROM automatically; QMK/Vial/native defaults are separate.
4. Board acquisition loops obtain raw values once with original timing. Prox and
   Matrix call ec_process_sample;0413 retains its capture/quiet scan wrapper and
   calls the same shared ec_update_key and ec_diagnostic_sample. It retains its
   noinline rescale helper via EC_BOARD_RESCALE. Actual ADC loops are not replaced.
5. ec_update_key clears disabled row bits before any VA/RT decision. Raw capture
   continues. QMK handles matrix/debounce/HID. There is no GUI-only disabled mask.
6. ec_diagnostic_sample caches raw and actual logical state, tracks extrema and
   feeds ec_cal_sample. USB snapshots never initiate analog conversions.
7. ec_va_task applies the shared released-key persistence policy. Native0413
   hooks additionally quiet audio/solenoid during storage and USB suspend/reset.
8. raw_hid_receive_kb routes EC42 through the same Raw HID interface as Vial.
   Close Vial before the GUI. matrix_scan_kb manages disconnect rollback and
   delayed standard bootloader_jump; no command directly installs firmware.

A board supplies matrix dimensions/eligibility, eeprom_ec_config/ec_config,
raw acquisition/boot baseline, matrix init/scan wrappers, physical flash backend,
record magic/firmware identity and its native bootloader/features. Use common
lifecycle/decision/protocol/codec; duplicate none of these for a new board.

## 4. Canonical EC42 v2.9 wire protocol

Authority: `common/engine/ec_diagnostic.c`, `calibration.c`,
`actuation_control.inc`; GUI `util/src/protocol.{h,cpp}`, `device.cpp`,
`calibration.cpp`, `settings.cpp`. All HID reports are exactly 32 bytes.

| Byte | Meaning |
|---:|---|
|0|0xCA|
|1..4|ASCII EC42 (45 43 34 32)|
|5|major version 2|
|6|opcode|
|7..8|big-endian u16 request sequence, echoed|
|9|status (request must 0)|
|10|payload length 0..20|
|11|request 0, response 1|
|12..31|payload, unused request bytes must be zero|

Status: 0 OK, 1 malformed/wrong state, 2 unsupported, 3 range/safety/storage
rejection, 4 stale session/snapshot/revision/token. Field-specific state checks
can return malformed rather than a separate Busy code. No negotiated version
upgrade exists: HELLO plus capability checks and, Descriptor select the canonical record codec. Multibyte packet fields are big-endian. Packed native EEPROM
words are little-endian; record transfer accessors swap native u16 fields, leave
u8 fields/portable override tail unchanged. Do not memcpy host structs to USB.

All numbers in the following opcode column are hexadecimal. `u16`, `u32` are BE;
`index` is row*cols+col; `session` is the current nonzero capture ID.

|Op|Request payload|Response / behavior|
|---|---|---|
|10|empty|`QEC2`,u8 minor,u16 rows,u16 cols,u16 ADCmax,u8 polarity,u16 caps,u8 pageLimit,u8 metadataVersion (16 bytes)|
|11|empty|latch snapshot: u16 id,u32 timer_ms,u16 cellCount|
|12|u16 snapshot,u16 start,u8 count (1..5)|echo 5 bytes, then count × (u16 raw,u8 flags)|
|13|u16 snapshot,u16 index|echo 4 bytes + eight u16: baseline,bottom,press,release,min,max,peak,extremum|
|14|empty|u8 state,u16 session,u8 canonical method,u8 mode,u8 record+storageReady|
|15|u8 runtime (0/1)|9-byte settings header: mode,u16 pressQ,u16 releaseQ,u16 initialQ,u8 RTpress,u8 RTrelease|
|16|u16 offset,u8 count (1..17)|echo offset/count + raw saved record bytes; backup may be invalid|
|17|u16 recordBytes,u32 transferCRC|restore begin; returns u16 token|
|18|u16 token,u16 offset,u8 count,bytes (1..15)|stage restore bytes, no commit|
|19|u16 token|single-use validated complete restore commit; no blind retry|
|1B|empty|abort volatile calibration, not an EEPROM reset|
|1C|empty|firmware identity string|
|1D|u16 offset,u8 count (1..17)|canonical active RAM record bytes, not a saved backup|
|20|action byte (see below)|explicit capture control|
|22|u16 session|validate proposal; REVIEW -> READY|
|23|u16 session,u16 offset,u8 count (1..17)|echo offset/count + canonical proposal bytes|
|24|u16 session|activate validated/released proposal in RAM, verify installed thresholds|
|25|u16 session|explicit packed save with health/readback checks; return idle|
|27|u16 session,u16 index|u16 index,u16 qualifiedLow,u16 qualifiedHigh,u8 scanCount,u8 activity,u8 captureStatus|
|28|u16 index or FFFF global|20-byte VA/status structure below|
|29|u16 index (FFFF global),u8 value,u16 expectedRevision|immediate VA edit, returns new u16 revision|
|2A|u8 RTenable,u8 pressMovement,u8 releaseMovement,u16 expectedRevision|immediate RT edit, returns revision|
|2B|empty (all current boards)|20-byte hardware/storage descriptor below|

0x21/0x26 are reserved/unsupported; no auto-calibration upload, legacy migration,
generic arbitrary EEPROM write or reset command is provided by EC42. Vial's own
protocol is separate. All boards reject obsolete EC VIA menus; normal Vial keymap/lighting protocols remain separate.

0x20 actions: 1 start (`[1]` or `[1,mode0or1]`, returns session), 3 finish (no
save), 4 cancel, 5 retry (`[5,u16 index]`, capture only), 6 continue review.
States: 0 idle, 1 manual, 2 reserved, 3 review, 4 ready, 5 RAM applied. Start
clears draft evidence; Finish merges valid captures with untouched saved keys.
Continue preserves other captures; Retry clears selected evidence only.

Snapshot flags: bit0 seen, bit1 actual pre-debounce logical state, bit2 QMK
matrix state, bit3 calibrated/enabled, bits4..6 capture status, bit7 qualified.
Capture status 0 untouched,1 valid,2 insufficient sustained movement,3 invalid
thresholds/RT bounds,4 impossible. The scan count saturates at 255: it counts
scans, not presses. Activity bit1 records seeing ADC=1023; that alone is not a
qualification or an unconditional reject. Full-scan min/max are historical
extrema since startup, NOT extrema of only the latest USB observation window.

0x28 response: u16 RAM revision,u16 verified-saved revision,u8 status,u8 VA-ready,
u16 global pressQ,u16 releaseQ,u8 RTmode,u8 RTpress,u8 RTrelease,u8 selectedOverride,
u32 saved-record masked CRC,u16 echoed index. Status0 Saved,1 Applied/pending,
2 Saving,3 Error,4 Calibration required. Revisions are volatile, reset on reboot;
CRC/readback validates persistence separately. VA-ready is always1 in these
profiles; it is not a migration enable switch.

0x2B response: u16 magic,u16 recordBytes,u16 ADCmax,u16 eraseSector,u32
flash-relative storageStart,u32 storageEndExclusive,u16 factoryKiB,u8 storageReady,
u8 formatVersion(1). Capabilities 0x66 include telemetry(2), metadata(4), restore(32),
explicit controls(64). All three boards report 0x66; old custom VIA hooks are not advertised.

Example HELLO request, sequence1 (spaces are byte boundaries):
```
CA 45 43 34 32 02 10 00 01 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```
To start capture use opcode20,length 1,payload01 with a fresh sequence. To latch,
use opcode11,length 0, then pass returned snapshot ID to 12/13. Do not reuse a
stale snapshot or guess offsets for a different board.

Boot requests use a distinct retained EC41 envelope, **not an EC42 opcode**:
32 zero-initialized bytes with `CA EC41` at 0..4 and action7 at 5. 0413 also supports
EC41 status action0. Firmware defers boot at least 100 ms, aborts volatile state
and calls native `bootloader_jump`. It does not install firmware itself.

## 5. Canonical calibration, settings and persistence

### One schema

For N=rows*cols, the native record has exactly **17+3*N bytes**, packed without
padding. All three compile the same ec_switch_matrix.h and record_storage.inc.
Persistent method 0x42 and descriptor format 1 identify this schema. The board's
unique magic binds it to a hardware profile; geometry/length come from HELLO and
Descriptor. Changing this ABI requires explicit version/compatibility review.

| Native byte offset | Field |
|---:|---|
|0|u8 mode:0 fixed VA,1 dynamic RT|
|1,3,5|u16 global pressQ,releaseQ,initialQ|
|7,8|u8 RT press/release ADC movement|
|9..9+2N-1|N u16 bottom/valid/enabled/retry cells|
|9+2N|u16 board magic|
|11+2N|u16 method:0x42 saved,3 canonical active/proposal|
|13+2N,15+2N|u16 CRC32 low/high|
|17+2N..17+3N-1|N override bytes:0 inherit,1..98 percent,99 Deepest|

Native words are little-endian; wire transfer accessors send u16 fields
big-endian and leave byte fields/override tail unchanged. CRC32 uses polynomial
0xEDB88320, initialFFFFFFFF/final complement over native bytes with both CRC words
zeroed. Do not run transfer CRC directly as the stored masked CRC; restore also
uses a separate CRC of the transferred byte stream. Fields must pass range/flag
validation before active RAM or storage changes.

Cell low10bits=measured bottom,0x4000 qualified,0x8000 enabled,0x2000 retry.
Reserved bits0x1C00 are rejected for eligible cells. Enabled requires qualified,
nonzero bottom and no Retry. Impossible positions remain governed by the board
mask, not by installing fake bottom values. Baselines are NOT stored.

### Manual calibration

Start -> press/hold/release any desired keys -> Finish -> Review -> Apply & Save.
No timeout/automatic finish/automatic calibration save exists. One new valid key
is enough. Untouched saved valid keys remain enabled; uncalibrated keys remain
inactive; an invalid attempted recalibration becomes Retry/disabled rather than
retaining an unsafe activation. Cancel restores unsaved state without persistence.

Complete non-overlapping16-scan blocks qualify the minimum of block maxima as
rest and maximum of block minima as bottom. At least 100 ADC counts of observed
travel and safe boundaries are required. A plateau may need31 scans due to block
alignment. Counts saturate at 255 scans, not presses. Brief spikes cannot qualify;
sustained interference or clipped ADC rails still need hardware investigation.

### VA, Deepest and RT

manual_boundaries maps pressQ/releaseQ between each current baseline N and bottom
B. Global pressQ is a travel fraction /1023; release gap stays in Q units. Per-key
0 inherits,1..98 maps rounded percentage,99 uses Q1022/Deepest (B-1 in supported
integer spans). Release is capped at B-6 and must remain above N/below press.
The GUI calculates safe shallow minima from actual integer spans/gap.

Initial activation ALWAYS requires VA. With RT off, fixed press/release hysteresis
applies. With RT on, ec_rt_key median-filters3 samples, tracks extrema and releases/
reactivates on strict movement comparisons. Returning to/below fixed release
floor disarms the cycle. Safe global offsets are4..min(255,B-release-2) across
enabled keys;4 requires more than4 counts. No physical millimetre equivalence is
claimed. Existing downward-only noise-floor adaptation is not full drift/temperature
compensation. Never loosen qualification merely to make calibration appear valid.

### Persistence and recovery

Settings update active RAM after revision, release and bounds validation, then
wait at least 2 seconds with all enabled keys released. Applied/Saving/Saved/Error
are distinct; disconnect rolls pending edits back to stored state, not a save.
Manual capture requires explicit acceptance. store_raw quarantines CRC, writes
only the EC record, verifies readback, commits and verifies health/CRC again.

A single-sector QMK journal is NOT a dual-bank atomic backup. Power loss during
consolidation can lose a snapshot. CRC fails closed; recover from a matching disk
backup or recalibrate. USB RAM-cache readback is not power-cycle verification.
Save EC backups and Vial exports separately. No legacy calibration is migrated.

### Physical placement and 0413 conversion

All logical EEPROM capacities remain4096; physical erase sectors/backends stay
unchanged. Core 0..36 contains audio/haptics/user settings. Prox EC 37..278 (242)
and Matrix EC 37..341 (305) remain byte-for-byte compatible with previous v2.9.
Their Vial allocations are unchanged. Matrix vial_alec and vial have different
layout-options/feature allocations: retain distinct pinned compatibility IDs.

**0413 keeps every existing Vial address and ID.** The canonical332-byte record
is mapped by common/runtime/record_io.c:

| Region | Logical bytes (inclusive) |
|---|---|
|QMK/native core|0..36, unchanged|
|Canonical EC bytes 0..218|37..255 (old EC reservation)|
|Vial identity/options|256..259, unchanged|
|Four dynamic layers|260..1099, unchanged|
|QMK settings/tap dance/combos/overrides|1100..2291, unchanged|
|Vial macros|2292..3982 =1691 bytes (previously1804)|
|Canonical EC bytes 219..331|3983..4095 =113 bytes|

EECONFIG_KB_DATA_SIZE intentionally remains 219 for QMK address compatibility;
sizeof(eeprom_ec_config_t) is 332. Do not change that constant to 332 without a
new allocation plan: it would move/overwrite Vial. ec_record_read/write form one
logical332-byte record over two ranges, using the same canonical CRC/codec as all
boards. No scatter metadata, second calibration format or storage migration exists.

**Before flashing0413 export Vial keymaps/macros and shorten macros to fit1691
bytes.** The adapter checks QMK's final-zero upload sentinel and all macro
terminators inside the retained range. If they do not fit, EC saves/descriptor
readiness are blocked, not truncated or auto-erased. Keep the old firmware to
export/shorten an oversized buffer; the new Vial UI reports only the smaller
capacity. No raw bytes are erased automatically, but an oversized old macro is
not guaranteed playable by the smaller-buffer build. Never bypass this guard.

The first new boot rejects old 219-byte radix calibration, initializes empty RAM,
and disables ordinary typing. It does not overwrite calibration on startup.
Explicit manual Save replaces the old EC bytes and previously unused macro-tail
reservation. Vial maps/options/native core are preserved. After that, reverting
to v2.8 requires its matching EC backup/recalibration; do not expect either firmware
to decode the other's record. Raw new-format archival backup is not an old 219-byte
restore image. Keep pre-flash exports and matching fallback independently.
On rollback re-upload the Vial macro export as well: v2.8 checks the old final
macro byte, which the canonical record tail may have changed.

The split adapter is an allocation exception, not a sensor/algorithm exception.
A fresh fourth port should reserve its full record contiguously before Vial and
avoid a split unless preserving an established layout requires it.

## 6. Hardware-specific adapters and physical flash

### MM Studio Class0413 EC

STM32F401, HSE8 MHz, SYS84 MHz, USB48 MHz,128 KiB actual flash,64 KiB RAM.
5×21 electrical/91 eligible. Rows PB8/PB7/PB6/PB5/PB3; mux enables PA4/PA5/PA7;
select PB0/PB1/PB10; ADC PA2; PA3 open-drain discharge (do not drive push-pull
high). Preserve channel permutations and10 us discharge. Num PA15 and Caps PB15
are active-low. Beeper PA6/PB12 differential, TIM3 PWM/TIM4 GPT; TIM2 system timer.
Solenoid PB13 active-high, enable PB14; native dwell and suspend guards remain.
Audio, Clicky, lock songs/LK_SONG and haptics are enabled. Music Mode remains
explicitly disabled to fit VA; do not restore it during cleanup.

USB2024:0319, manufacturer Alectardy98, product
Sunxkui Design Studio - MM Studio 0413 EC, UID45464C4449414702.
TinyUF2 sector 0 at 08000000, protected sector 1 at 08004000, EEPROM sector 2
08008000..0800BFFF, factory settings sector 3 0800C000..0800FFFF, application sector 4
08010000..0801FFFF. Never assume a sixth sector on this128 KiB device. Guard
modules preserve this physical geometry despite generic MCU package settings.

### Cipulot EC Prox

**Configured MCU is STM32F401, not F411.** HSE8 MHz/SYS84 MHz/USB48 MHz,64 KiB RAM.
Build configuration declares256 KiB flash; guards validate the factory density
and current linked extent (including smaller supported density). Do not infer
actual chip density solely from processor name or memory dump size.
5×15/62 eligible; rows PA7/PB0/PA4/PA5/PA6; mux enable PB7/PB3;
select PB4/PB5/PB6; ADC PA1; discharge PA2,10 us. `acquisition.c` retains atomic
charge/read timing and30-sample baseline. PB12 WS2812 drives22 LEDs via RGBLIGHT;
vial_alec layer-lock indicator uses RGBLIGHT segments. RGB Matrix/VialRGB was
removed intentionally after conflict cleanup; do not resurrect competing writers.

USB6369:6B98, Cipulot / EC Pro X ANSI/ISO, UID4D2D00BCADF8B927.
No installed user-flash bootloader is required for clean-install mode; use the
established STM32 ROM DFU/SWD workflow. Vectors are in sector 0 at 08000000;
sector 1 08004000..08007FFF is persistent storage; text starts08008000.
Journal backing8192 bytes inside16 KiB erase unit, logical 4096. Never erase text
through a supposed contiguous32 KiB EEPROM hole. `clean_install.py` fills holes
withFF and checks ELFsections. A clean-install BIN at 08000000 is deliberately
not a calibration-preserving update: flashing tools may erase sector 1 despiteFF
payload. Preserve settings using verified selective sectors/split files instead.
ROM DFU survives erasing user flash; owner must establish physical entry first.

### Matrix ME EC UF2

STM32F411, HSE8 MHz/SYS96 MHz/USB48 MHz, configured512 KiB flash/128 KiB RAM.
6×16 electrical,86 unique default layout cells. Rows PB6/PB5/PB4/PA15/PB15/PB1;
left/right enable PB10/PB2; address PB14/PB13/PB12; opamp PA5; discharge PA6;
ADC PA2. Preserve original `sense_key`2 us discharge/charge sequencing and20 us
post-read wait; config's historical DISCHARGE_WAIT macros alone do not describe
the running scanner. Baseline averages30 real scans. IS31FL3236 on I2C1 PB8/PB9
and four WS2812 badge LEDs PB7 are separate hardware, not Prox's shared-pin issue.

USB4543:4D45, Matrix / Astro ME EC Kit, UID0EC173239F8F8F2C.
Installed **custom ASTRO**, not generic TinyUF2. `BOOTLOADER=tinyuf2` only selects
the existing QMK UF2 pipeline. Strong `astro_bootloader.c` replaces its entry:
PWR clock+DBP access, RTC_BKP0R=0x41544B42, readback/barriers, systemreset.
RTC source must be unselected/LSI; no destructive backup-domain reset. QK_BOOT,
Vial and EC41 boot requests reach it. Original QK_BOOT entry is owner-confirmed.
Fn+Escape is QK_BOOT; after empty calibration physical keys need calibration but
Vial/GUI softwareentry remains independent of enabledkey state while USB works.

|Sector|Range|Use|
|---|---|---|
|0/1|08000000..08007FFF|ASTRO loader; preserve|
|2/3|08008000..0800FFFF|reserved; preserve|
|4|08010000..0801FFFF|original settings; preserve|
|5|08020000..0803FFFF|new application,128 KiB updater region|
|6|08040000..0805FFFF|reserved; preserve|
|7|08060000..0807FFFF|new EC42 EEPROM128 KiB backing|

UF2 family57755A57, flags2000,512-byte blocks/256-byte payload, applicationorigin
08020000. Installed loader erases sector 5; all targets must remain below 08040000.
No generic TinyUF2 reset sequence is assumed. Dump-derived independent entry
samples fully pressed Escape at power-on (PB0 low is another loader gate); do
not short unidentified pads. Verify recovery physically before relying on it.
USB/UF2 format alone cannot attest factory flash density/current WRP; guards
fail closed if storage unavailable. User reports working board; that is not an
archived option-byte measurement. Preserve the dump and original stock UF2.

## 7. Builds, packaging and deterministic Vial IDs

Dependencies: this fork's QMK Python environment/submodules, make, Python3 and
arm-none-eabi GCC/binutils. Record toolchain/version with releases. From root:
```sh
make alectardy98_ec/class0413_ec:vial
make alectardy98_ec/class0413_ec:vial_alec
make alectardy98_ec/ec_prox:default
make alectardy98_ec/ec_prox:60_hhkb
make alectardy98_ec/ec_prox:via
make alectardy98_ec/ec_prox:vial
make alectardy98_ec/ec_prox:vial_alec
make alectardy98_ec/me_ec_uf2:vial
make alectardy98_ec/me_ec_uf2:vial_alec
```
Historical Matrix vial_alec_old is additionally build-tested, not the default.
Normal Prox make automatically produces FF-gap clean-install BIN, ELF and map.
Normal 0413/Matrix make produces application-only UF2. Release tools:
```sh
./development/alectardy98_ec/build-baselines.sh
python3 development/alectardy98_ec/package.py
sha256sum firmware/alectardy98_ec/class0413_ec/vial_alec/class0413_ec.uf2
```
Artifacts: firmware/alectardy98_ec/<board>/<keymap>/<board>.{elf,map,hex,bin,uf2}
where appropriate. Prox's flashable clean image is ec_prox.clean-install.bin at
08000000, not an ordinary zero-gap objcopy BIN or a UF2. It is destructive to
settings when installed with gap-spanning erase. Selective vector/application
BINs preserve sector 1 only with a verified selective erase policy.

Pinned Vial IDs remain 0413=175720,Prox=36E2FF,Matrix vial=9829ED/vial_alec=01198A.
They describe Vial allocation compatibility, not EC42 protocol. 0413's macro
capacity shrinks without moving data; do not bump its Vial ID and erase maps.
The fork still needs VIAL_ENABLE despite old metadata warnings; Prox uses
post_rules/post_config hooks. Insecure mode is intentional existing policy.
Never enable security, change unlock chords, suppress global warnings or alter
lighting merely for cleanup. ASTRO/tinyuf2 selector discrepancies are documented
hardware-specific packaging, not permission to replace the installed loader.

## 8. Canonical GUI and compatibility contract

The maintained application is util/. Original external qmk-ec-util and old
firmware are preserved fallbacks. Runtime uses the shared ec42_protocol.h;
CMake includes ../common/interfaces. Copy that header with the app if extracting
it from the framework. Do not duplicate its command/version definitions.

Qt6>=6.4 Core/Gui/Widgets/Test, C++17, CMake>=3.21, Ninja/Make, pkg-config,
hidapi-hidraw and liblzma are required; Python/QMK source supports firmware tests.
```sh
cmake -S keyboards/alectardy98_ec/util -B keyboards/alectardy98_ec/util/build-universal -G Ninja -DBUILD_TESTING=ON
cmake --build keyboards/alectardy98_ec/util/build-universal -j4
keyboards/alectardy98_ec/util/build-universal/qmk-ec-util
```
This dynamically linked Linux executable is not a portable AppImage. Close Vial;
both clients use Raw HID FF60/61 and Vial's serial marker. Do not run as root.
The worker verifies Vial identity, HELLO/capabilities and descriptor; live layout
comes from the embedded Vial definition. **A new canonical board needs no GUI
registry change**: geometry, record length/magic and format 1 are device-reported.
boardprofile.cpp contains optional offline presets/display expectations only.
Unknown/malformed descriptors, wrong active/restore magic and stale generations
are rejected before writes. VID/PID/product name alone never authorizes writes.

settings.cpp decodes canonical native-word CRC and per-key override bytes without
board-specific serialization. device.cpp controls transport/readback/backup,
protocol.cpp validates packets/snapshots; layout.cpp maps physical positions;
livepanel.cpp owns manual capture/VA/RT; widgets.cpp renders keyboard/graph;
mainwindow.cpp owns pages/session; telemetry.cpp caches samples/simulation.

Native sensor readings stay counts, not volts. Yellow/green uses actual firmware
logical released/pressed state; disabled keys are gray. Snapshot/meta polling is
bounded (normal 1 second,full metadata 5 seconds,faster selected-cell path), not an
oscilloscope. Do not increase USB sampling blindly or perform ADC in callbacks.
Settings apply immediately and auto-save after safe idle; manual captures never
auto-save. A verified saved revision/readback still requires power-cycle testing.

Only EC42 v2.9 is supported. Older protocol versions and record formats are
rejected before any write. There is no migration UI, radix codec or historical
firmware fixture in the active suite. Invalid/foreign/current-corrupt records
still fail closed; raw backups can be archived for investigation.
--offline, --board <preset> and --smoke-test never open HID. Backup/restore requires
matching device/record identity; unknown/invalid raw backups may be archived but
never forced into a device. Tests are offline only.

## 9. Fourth-keyboard implementation guide

1. Preserve original source, installable image/hash, settings and independent
   recovery. Inspect parent board files and deployed bootloader evidence.
2. Audit MCU/clocks, ADC range/polarity, mux/row ordering, discharge/settling,
   atomic sections, matrix holes/layout variants, lighting and peripheral timers.
3. Create a board directory with original acquisition/config/keymaps/linker.
   Retain licenses. Verify real flash density/geometry and updater erase scope.
4. Add common engine sources, runtime lifecycle/decision/integration and the
   common/interfaces include directory. Define a unique A98_RECORD_MAGIC and
   <=20-byte A98_FIRMWARE_ID. Do not create a new codec or protocol minor.
5. Define matrix dimensions, is_unused_position, EC globals and baseline; feed
   each raw sample through ec_process_sample or a justified native wrapper using
   ec_update_key + ec_diagnostic_sample. Preserve hardware timing exactly.
6. Reserve17+3*N logical bytes before Vial for a new design. Compile the actual
   QMK/Vial offsets and macro capacity; never borrow another board's addresses.
   Use normal record IO aliases for contiguous storage. Split record_io is only
   for preserving an existing layout. Confirm CRC/readback and fail-closed errors.
7. Provide a guarded persistent backend, native bootloader_jump and correct
   linker/VTOR/image packaging. F4 guards are not portable to unrelated MCUs.
   No option-byte changes or protected-sector programming may be introduced.
8. Preserve Vial UID/layout/custom keys and native lighting/features; pin its
   schema ID deliberately. A source enum is not automatically a Vial User label.
9. Supply HELLO geometry and format 1 descriptor. GUI live discovery/codec then
   works without a new model case. Add an optional offline layout preset and
   tests if desired. A different sensor scale/capability requires explicit verified
   normalization/protocol support; do not pretend arbitrary12-bit data is 10-bit.
10. Add acquisition fixtures and actual-scanner tests, shallow/medium/Deepest,
    RT on/off/three sensitivities, disabled HID, corrupt/torn records, runtime
    changes, save debounce, GUI/readback and image/erase boundary tests.
11. Build intended keymaps and inspect flash/RAM, ELF load regions and every
    install payload. Obtain owner approval only after a concrete safe candidate
    and recovery procedure exist; never flash automatically.
12. Physically test one-key calibration first, incremental additions, VA/RT/noise,
    reboot persistence, native features and USB/boot recovery. Archive source,
    toolchain, GUI, ELF/map/install image/hashes, tests and physical limitations.

## 10. Validation, troubleshooting and remaining limits

```sh
./development/alectardy98_ec/build-baselines.sh
./development/alectardy98_ec/test.sh
QT_QPA_PLATFORM=offscreen ctest --test-dir keyboards/alectardy98_ec/util/build-universal --output-on-failure
cmake -S keyboards/alectardy98_ec/util -B keyboards/alectardy98_ec/util/build-universal-sanitize -G Ninja -DEC_ENABLE_SANITIZERS=ON -DBUILD_TESTING=ON
cmake --build keyboards/alectardy98_ec/util/build-universal-sanitize -j4
ASAN_OPTIONS=detect_leaks=0 QT_QPA_PLATFORM=offscreen ctest --test-dir keyboards/alectardy98_ec/util/build-universal-sanitize --output-on-failure
```

Current actual scan loops and the shared v2.9 engine for ALL THREE boards are
compiled by `port-harness.py`; ADC/GPIO and host EEPROM are models. The suites
exercise qualified partial/incremental capture, 340/250/313 interrupted-save
prefixes, fixed/RT/Deepest, disabled HID, telemetry, restore and boot entry.
`test_record_io.py` executes the split adapter against Vial canaries and macro
conflicts. Real QMK NOR journal and guard tests cover replay/consolidation,
corruption and protected regions. Qt worker/UI tests use these same current
harnesses. No old firmware implementation is compiled by the active tests.
`verify-builds.py <build-tree>` requires all six artifacts and checks loadable
sections, UF2 family/bounds and Prox's FF storage gap; absence is an error.
`test_matrix_vial_id.py` additionally uses real ARM build flags to verify both
Matrix EEPROM schemas (`EC42_BUILD_TREE` selects an isolated build checkout).

Troubleshooting: no typing -> check Calibration required/record/eligibility;
NOT READY -> physical guard or0413 macro-tail conflict, never bypass it; gray
capture -> sustained hold/release and100countspan; red -> inspect qualified bounds,
RT limits and ADC noise/rail; lost saves -> check Applied/Saving/Saved and power
cycle; discovery -> close Vial and check permissions/descriptor; reset Vial ->
inspect pinned ID and actual allocation; wrong LED -> check physical hardware,
native callbacks/pin/count rather than swapping drivers blindly. Unsafe image
bounds or uncertain bootloader -> stop before flashing.

Remaining limits:10-bit rising sensor contract; limited downward baseline update;
sparse USB telemetry; one-sector consolidation power-loss window;0413 macro
capacity reduction and deliberate recalibration; archived historical keymap
warnings. Full current measurements, hashes and reports are under
[cleanup report](../../development/alectardy98_ec/architecture_review/CLEANUP_REPORT.md).


### Python 3.14 build compatibility

Matrix dimensions belong in each board's `keyboard.json` as `matrix_size`,
following the repository's ATmega32U4 converters (for example,
`converter/ibmpc_usb/32u4` and `converter/nissho/32u4`, commit `0e20b59056`).
0413 uses 5×21, EC Prox 5×15, and Matrix 6×16. QMK generates the matching
`MATRIX_ROWS` and `MATRIX_COLS` definitions in `info_config.h`.

Do not duplicate these dimensions in `config.h`: that makes this QMK revision
parse their C expressions with its obsolete `ast.Num`/`.n` arithmetic helper.
Other hardware pin/multiplexer definitions remain in board `config.h` files.
The normal make commands work with system Python 3.14 without a compatibility
shim, PYTHONPATH adjustment, virtual environment, or QMK core modifications.

The former `sitecustomize.py` shim and framework parent `rules.mk` override
have been removed. Python 3.13's lazy `ast.Num` lookup made the shim's
`hasattr(ast, 'Num')` probe itself emit a deprecation warning; moving dimensions
to metadata avoids both that probe and QMK's deprecated arithmetic path.
This does not modernize upstream QMK's expression parser for unrelated boards
that still declare matrix dimensions in C configuration.

Run the offline metadata/build-path regression check with:
`python3 keyboards/alectardy98_ec/common/build_tools/test_matrix_metadata.py`.
