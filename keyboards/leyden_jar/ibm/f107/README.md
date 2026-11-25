# IBM Model F 4704 F107 - Leyden Jar Rev 3

## Calibration

This F107 profile uses ten native scan groups. References are measured again
at startup; no absolute threshold or keymap is hard-coded by calibration.
Keep all keys released during startup.

| Group | Controller positions (row, column) | Reference and offset |
| --- | --- | --- |
| 0 | (7,11), normally Page Up | Own released level +12 |
| 1 | (5,2), (6,6), (5,11), normally Tab / 7 / Page Down | Median +32 |
| 2 | (5,4), (6,4), normally 3 / R | Median +35 |
| 3 | (5,13), (6,13), normally numpad 8 / 9 | Median +35 |
| 4 | (2,7), (0,10), normally L / Left Arrow | Median released level +8 |
| 5–9 | Remaining active positions | Highest released level in the group +16 |

The automatic groups minimise the widest released-signal range, rather than
putting the same number of keys in each group. This keeps high-baseline pads
from triggering above a group's median and prevents a distant outlier from
raising the threshold of many other keys. The fixed five-group budget bounds
normal scanning work. The startup measurement searches the full DAC range, 0–1023;
readings above 499 are no longer clipped. Thresholds saturate at 1023 rather
than wrapping the DAC. An actually held or faulty pad cannot be distinguished
from a high released baseline using startup calibration alone.

`LEYDEN_JAR_RANGE_CALIBRATION` enables this method only for F107. Other boards
retain their existing calibration. All 109 declared positions, including the
custom-layout positions, remain available. Calibration follows controller
coordinates even when a key is remapped. No position is disabled and no keymap,
layout option or macro is changed. Existing calibration controls and Restore
Defaults remain available. The preference record and EEPROM allocation are
unchanged; incompatible calibration preferences fall back to the compiled
offsets without resetting other settings.

The ten groups require four more native scan passes than the previous six:
400 microseconds of additional DAC settling plus scan/transfer work per matrix
scan. The range search runs only at calibration, uses the existing sorted-key
array, and adds no per-key runtime thresholds or heap allocation.

### Per-key debounce candidate

F107 selects QMK's unmodified `sym_defer_pk` algorithm with `DEBOUNCE 12`.
The previous global 5 ms filter restarted its wait whenever any pad changed.
Per-key filtering prevents a noisy pad from postponing another key's report.
At the observed approximately 9 ms scan interval, a stable change normally
settles after two intervals (about 18 ms after its first observation), rather
than one. This adds roughly 9 ms of settling time and filters short assertions
or release gaps. Deliberate taps shorter than the settling window can also be
filtered; sustained false signals and mechanical dropouts remain sensing issues.

This setting is local to F107 and applies to both its keymaps. It changes no
calibration thresholds, EEPROM layout, key assignments or shared QMK code.
QMK allocates one debounce counter per matrix position (120 bytes plus allocator
and algorithm state). The native USB polling interval remains 1 ms.

The regression runs the actual old and new QMK algorithms under ASan/UBSan at
the measured cadence. It reproduces unrelated keys being suppressed by a noisy
pad, a hold split by brief dropouts, short false assertions, two separate taps,
and a simultaneous chord. The candidate passes those synthetic cases; they do
not establish that every physical source of unwanted L/D presses is fixed.

The candidate was flashed through ROM BOOTSEL on November 25, 2026; all 27
Leyden Jar regression tests passed. Before flashing, a passive host recording
showed ten clean taps each of K/L/D and continuous L/D holds. The user chose
normal typing instead of repeating that controlled sequence after flashing,
and reported no unexpected behavior so far. The subsequent passive recording
contained matched press/release events for K/L/D, but has no known physical
press count, so it is not a missed-key acceptance test. Everyday use remains
the next check. Typing is enabled and the recorder has been stopped.

**Rebuild settings caveat:** this repository's Vial build generates a new
`BUILD_ID`, which `via_eeprom_is_valid()` uses to invalidate saved Vial settings
and restore compiled keymap/macro defaults. The immediate before/after keymap
check failed; three later readbacks matched the compiled defaults and earlier
session fingerprint. The user confirmed no recent keymap changes. No explicit
EEPROM-clear command was sent, but rebuilding must not be described as preserving
custom Vial settings. Calibration preferences use a separate record.

### Live sensor measurements

The F107 also uses the range search for native live-level requests. Between
measurement probes it services QMK's regular `keyboard_task()`, including
scanning, debounce, key actions, USB reports and solenoid timing. The Raw HID
handler runs outside that task in the main loop, so this does not recursively
process USB commands. Startup calibration does not service key events.
The command still returns one acknowledgement after the complete measurement;
the GUI and packet format are unchanged. There are 576 bytes of additional
static search workspace, kept off QMK's small task stack. No calibration or
EEPROM writes occur during a live read.

This corrects a measured problem in the first nine-group build: each live
request occupied about 601 ms without normal scanning. The cooperative search
adds one measurement probe between regular keyboard scans; it is not a claim
of zero overhead or a verified 1 kHz matrix scan rate. Other Leyden Jar boards
retain their existing measurement path unless they opt into range calibration.

### Evidence and validation

The six-group build passed the November 23 targeted host tests but failed
again after changes to the physical assembly. On November 24 the shared 3/R
and numpad 8/9 threshold reached 485 while the sweep measured 3/R peaks of
479/481. Separating those pairs avoids that missed-key mechanism. The normal
equal-population groups also left high released pads above their thresholds;
full-range comparisons measured a pad at 527–528, beyond the old 499 ceiling.
The reason the physical baselines changed is not established by these tests.

Build: `make leyden_jar/ibm/f107:vial`.

Tests: `python -m unittest discover -s keyboards/leyden_jar/tests`.

The F107 sanitizer tests run the real calibration and scanner code against
released captures, measured held/coupled signals, changed-baseline sessions,
103 observed sweep responses, simultaneous-key paths, all 109 matrix
positions, equal-level grouping, DAC saturation, persistence and defaults.
Sweep readings above 499 are explicitly treated as lower bounds, not precise
measurements. Synthetic/replayed tests do not establish keyboard feel, host
latency or cold-start stability. Physical validation is still in progress.

Hardware check (2026-11-24): the nine-group UF2 was flashed through ROM
BOOTSEL. Readback confirmed nine groups, all 109 positions, and an unchanged
four-layer keymap fingerprint. A 12-second host-only idle recording had no
key events or held keys. After a software restart, the user deliberately
pressed P and O; both produced balanced press/release reports. The subsequent host-only physical sweep recorded
97 presses and 97 releases across all 96 assigned host keycodes, no autorepeat
and no held keys at completion. Q/W overlapped intentionally, as confirmed by
the user. Up produced two brief press/release pairs in that sweep; a subsequent isolated
check recorded five clean taps and one uninterrupted 1.718-second hold, with
exactly six presses/releases, no other keys, and nothing held afterward. The
43 autorepeat events during the hold were Linux repeats, not extra firmware
press/release transitions. This did not verify the requested five-second hold
duration. Normal typing was restored and read back as enabled. Cold-start
acceptance remains pending. No EEPROM reset was performed.

A subsequent L-only host test with Vi_al closed recorded exactly ten presses
and ten releases (81–122 ms taps), no extra keys, and no held keys afterward.
It did not include the requested sustained hold. L’s released readings were
410–411 against a threshold of 428. The live-measurement correction is covered
by sanitizer tests that interleave press/release scans, verify all 144 level
readbacks, preserve calibration/EEPROM, reject recursion and handle DAC errors.
The corrected UF2 was flashed and its unchanged keymap fingerprint verified.
With output disabled, full-level reads took 69–73 ms instead of about 601 ms.
The typing-enabled path additionally services QMK between probes; its total
reply time must not be confused with a scan blackout. The first physical live
test still received only four of ten L taps. A diagnostic build then received
ten of ten, so that pass alone did not establish a fix.

A subsequent held-L capture with live queries measured 426–428 against the
427 threshold, while released readings were 409–411. Temporary scanner counters
recorded 64 raw transitions across three live captures during that hold. QMK
debounce masked them as one continuous 3.708-second host press; 102 Linux repeat
events were normal host autorepeat. This demonstrates inadequate signal margin
at the old threshold, even when the host happens to report a clean hold. The
ten-group profile isolates that controller position at its own released +8;
other groups retain their offsets. The signal replay reproduces the missed
press at 427 and checks the new threshold across the measured held range,
including cooperative live acquisition. Temporary counters and their protocol
command are removed from the final candidate. The cause of L's small physical
signal range has not been established.

The ten-group candidate was subsequently flashed through ROM BOOTSEL. Native
readback confirmed L alone in group 4, released reference 411 and threshold
419. All 109 positions and the existing four-layer keymap fingerprint were
preserved. The 26-test Leyden Jar suite passed. No EEPROM reset or layout write
was performed. A 247-second host recording with continuous live sensor polling
then received exactly ten L presses and ten releases (79–131 ms), no other key
events and nothing held afterward. No long hold was recorded in that test, so
sustained-hold acceptance of this final build remains unverified. Normal typing
was restored and read back as enabled, with L's threshold still 419.

A later full-key sweep with live polling stopped did **not** pass: number-row
8 produced no host event, Left Arrow produced two press/release pairs, and
Print Screen released briefly then asserted again and remained held. The user
confirmed each key was pressed once. There were 97 presses, 96 releases and no
host autorepeat; D also had a short 28 ms reported press, which needs a targeted
check rather than being classified as a missed press. Post-sweep native reads
found Print Screen (controller 3,10) consistently at 477–478 against threshold
437, while released 8/Left were around 409–410 and D around 414–415. Typing was
disabled for investigation. This build is not accepted as a stable final F107
release; the calibration and physical-return checks remain open.

Targeted signals then measured Left Arrow held at 425–427 with threshold 428.
The next source candidate places Left Arrow in L's group: both released levels
are around 409–411, so they can share +8 without another scan pass. Signal
replays cover both weak held ranges and their released ranges. This candidate
must not be startup-calibrated while the Print Screen pad remains asserted.
The requested Print Screen press in the first targeted test actually changed
the position currently mapped to Insert (controller 7,10); the persistent high
signal at controller 3,10 therefore still requires a specific return check.
Number-row 8 held steadily at 455–457 in the targeted test, while D showed two
brief signal bursts instead of a continuous hold. Neither result justifies
changing their thresholds without further evidence.

Repeated presses at the actual Print Screen position then changed its signal
from the persistent 477–478 back to 417–419; it stayed released for more than
30 seconds without any firmware or threshold change. This supports checking
the physical return/contact, but does not establish a specific mechanical
fault. Only after native reads confirmed all positions released was the
L/Left Arrow candidate flashed through ROM BOOTSEL. Readback confirmed both
positions in group 4 at reference 410 / threshold 418, all 109 positions and an
unchanged keymap fingerprint. The full 26-test suite passed. Post-flash targeted
host testing is in progress; 8/D intermittency and cold-start acceptance remain
unresolved.

The next five-taps-per-key host test passed Left Arrow (5/5) but failed overall:
8 registered 3/5; D registered five, with three reports only about 18 ms long;
Print Screen generated seven press/release pairs. Nothing remained held. The
user reports a **beeper on the solenoid outputs**, not a solenoid. A temporary
diagnostic build provided a bounded raw/debounced/output-state trace and
a volatile beeper mute (native command 0x95, gated by marker 0xf1). It did not
change calibration or EEPROM. The mute restored the saved RAM enable state
explicitly or after ten minutes; a reset reloaded the existing saved setting.
This instrumentation has since been removed. Mute/restore/timeout and trace bounds
passed a temporary ASan/UBSan harness.

The beeper-muted recording on 2026-11-24 captured 324,868 scans with no trace
overflow and a longest scan interval of 10 ms. Both beeper control outputs were
inactive throughout. Print Screen still produced 268 raw transitions, of which
86 passed debounce; all 86 reached USB. D produced 144 raw transitions and 126
debounced/USB transitions. The user confirmed additional keys were deliberately
tested, so their reports are not classified as phantom keys. The test was not a
counted physical-press sequence and cannot establish a missed-press rate.
It establishes that beeper activity is not required for the observed raw
chatter; it does not yet distinguish comparator/scan effects from physical
switch behavior. Original beeper state and RAM thresholds were restored.

The temporary diagnostic also supported one-pad full-range level measurement
while typing is disabled (GET 0x95, marker 0xf1, action 2, controller row/column).
It used at most 11 comparisons, restored the last normal scan's DAC value, and
did not change calibration or cached full-matrix levels. Tests covered all 1024
levels, rejected live/invalid queries, DAC failures, and restoration. This was
investigation instrumentation, not a production feature or a confirmed fix.

The one-pad Print Screen capture measured 4,706 samples at about 91 Hz. It
recorded three distinct holds (2.79, 2.96 and 2.60 seconds) with complete returns
to the released range of 415–420. The third hold weakened to 441–453 for most
of its duration, compared with roughly 478 in the first two. All three stayed
above the current 438 threshold during this test; this does not reproduce the
earlier raw chatter, but establishes that the held margin varies substantially.
No calibration settings were changed. A proposed uniform reduction of automatic
bin offsets from +16 to +10 was rejected offline: an existing single-key capture
would falsely activate its neighbour at controller (5,6). The hardware retains
the original offsets pending a targeted correction and verification.

A following two-pad capture recorded 7,375 samples each for D and number-row 8
at about 45 Hz per pad, with a maximum interval below 24 ms and no acquisition
errors. Number-row 8 showed five sustained responses lasting 0.84–0.97 seconds,
mostly 458–465 versus a threshold of 428; it returned to 410–412 afterward.
D showed two sustained responses lasting 1.03/1.10 seconds, mostly 462–466
versus a threshold of 438, plus several isolated 22–44 ms responses. Its idle
range before and after the sequence was 414–417. The user subsequently confirmed
that D's presses were intended holds, not quick taps. Several failed responses
therefore returned to the released signal range during an intended hold; a
threshold-only correction was not established, prompting a native-scanner
comparison to distinguish measurement effects from the input itself.
Thresholds remained unchanged and typing was disabled
throughout the signal measurement. This is not a host-report acceptance test.

The user then adjusted D's spring. In a confirmed continuous hold afterward,
the normal native scanner at its unchanged threshold of 438 remained asserted
for 25.43 seconds with no intermediate release. The lower comparison threshold
of 426 agreed. Away from the press/release edges, 618 consecutive measurements
were 462–467, followed by a clean return to 412–418 with both scanners reporting
released. The original threshold was restored after the comparison. This
strongly supports a mechanical contribution to D's dropout and demonstrates
improvement after the spring adjustment, without claiming that every F107 key
or future cold start is now verified. No blanket sensitivity increase was
applied. Temporary diagnostic source, protocol hooks and build flags were
removed before rebuilding the normal firmware.

The normal build passed all 26 Leyden Jar regression tests and was sent through
ROM BOOTSEL. Its first reconnect failed USB configuration and returned a malformed
product string; a subsequent reconnect enumerated correctly without another
firmware write. The cause of that first enumeration failure is unresolved.
Application readback confirmed ten groups, all 109 positions, unchanged saved
keymap fingerprint, and rejection of the removed diagnostic command. A 12-second
host-only idle check had no key events and no held keys. Normal typing was then
enabled and read back successfully. No EEPROM reset or keymap write occurred.
UF2 SHA256: `f9b6790f554500a4f4310fccc5f79a37595ada34946968d01676336e94ff6fff`.

<!-- BEGIN LEYDEN JAR PIN REFERENCE -->
## Leyden Jar Rev 3 pin reference

**Logical diagram, not a solder-side view.** Match the numbered PCB pads; the drawing does not establish connector orientation. QMK rows/columns are zero-based firmware coordinates, **not original IBM ribbon or connector pin numbers**.

**Keyboard source declares:** 8 logical rows × 15 logical columns. **Rev 3 controller capacity:** 8 sense inputs and 18 drive outputs.

The original/native scanner declares **8 capacitive sense rows**. Any extra QMK logical rows may represent direct keys rather than additional capacitive rows. All eight A_ROW pins below belong to the controller; unassigned ones do not imply extra rows on the keyboard.

**Populated source-layout union:** 109 positions, 15 distinct drive signals. Empty intersections do not free a column unless that entire column is unused.

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
03    A_ROW0              QMK row 7
04    A_ROW1              QMK row 6
05    A_ROW2              QMK row 5
06    A_ROW3              QMK row 4
07    A_ROW4              QMK row 3
08    A_ROW5              QMK row 2
09    A_ROW6              QMK row 1
10    A_ROW7              QMK row 0
11    GND                 Matrix ground
12    GND                 Matrix ground
13    COL0                QMK column 0
14    COL1                QMK column 1
15    COL2                QMK column 2
16    COL3                QMK column 3
17    COL4                QMK column 4
18    COL5                QMK column 5
19    COL6                QMK column 6
20    COL7                QMK column 7
21    COL8                QMK column 8
22    COL9                QMK column 9
23    COL10               QMK column 10
24    COL11               QMK column 11
25    COL12               QMK column 12
26    COL13               QMK column 13
27    COL14               QMK column 14
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

Original controller-family source: [AVR configuration](../../../xwhatsit/ibm/f107/universal/config.h) and [port inventory](../ports.json). Blocked references retain source relationships even where a complete native adapter mapping is unavailable.

Mapping sources: [config.h](config.h) and [common.c](../../common.c). Switch technology: **model f**. Only the FAT has the earlier full physical row/key sweep; F122 has firmware/idle checks. Other ports require physical matrix verification.

Controller wiring source: `mymakercorner/Leyden_Jar:pcb/leyden_jar_controller/Leyden_Jar.kicad_sch @ e01dea552d0a`, J2/J3/J4 and the A_ROW/COL/LED/SWD/I2C/expansion nets. [Rev 3 schematic](https://github.com/mymakercorner/Leyden_Jar/blob/e01dea552d0a/pcb/leyden_jar_controller/Leyden_Jar.kicad_sch).

This section is maintained by `ibm/tools/update_pin_readmes.py`; run it after changing a port mapping. <!-- END LEYDEN JAR PIN REFERENCE -->
