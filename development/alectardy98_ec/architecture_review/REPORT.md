# EC42 architecture review against Cipulot

Analysis date: 2026-10-21. Scope: current `keyboards/alectardy98_ec/`, original `keyboards/cipulot/common/`, current GUI and licensing evidence. **Analysis only: no firmware, GUI, build configuration, records or release artifacts changed; no firmware rebuilt or hardware accessed.** The owner reports all three ports physically working. This review does not extend that validation to proposed changes.

## Recommendation

Keep the existing compile-time shared engine and board-specific acquisition. EC42 is not over-abstracted at runtime: most complexity buys functionality absent from the original. The best simplifications are restoring reproducible current-source tests, removing proven obsolete runtime branches/state, and making the existing transaction/serialization code readable without changing its semantics. Do not replace storage guards, calibration transactions or board drivers with Cipulot's shorter originals.

The first three improvements I would implement, in order:

1. **Restore the missing current-source validation tooling and make missing coverage visible.** The documented commands do not exist in this checkout, and CMake silently omits the current-board integration suite. This is the prerequisite for safe simplification.
2. **Remove the unreachable pre-EC42 RT fallback and audit its runtime-only fields.** The canonical implementation always uses the filtered VA-first engine. Retain every persisted/wire field and independently preserve diagnostic semantics. This can reduce real RAM and source complexity without changing hardware acquisition.
3. **Expand and name the existing calibration/record helpers and states in place.** Replace dense multi-operation lines and duplicated byte helpers with clear internal boundaries, protected by golden wire/record tests. Keep the single translation unit initially; do not introduce a plugin framework or rewrite persistence.

These are proposals, not changes made in this pass. No measured flash saving is claimed for an unimplemented proposal.

## Evidence and method

Source paths below are repository-relative, with line references to the audited checkout. Key references:

- Original `keyboards/cipulot/common/ec_switch_matrix.c:133,159,197,232,256,352`: peripheral initialization, startup baseline, scan, raw conversion, decision and scaling.
- Original `keyboards/cipulot/common/via_ec.c:47` onward: direct VIA menu setters and calibration/save helpers; `common_cipulot.mk:1` onward: three common C files plus optional VIA handler.
- EC42 `common/engine/calibration.c:25–277`, `record_storage.inc:1–47`, `actuation_control.inc:20–139`.
- EC42 `common/runtime/decision.c:23–119`, `ec_board.c:29–52`, `storage.c:7–67`, `record_io.c:8–40`, `integration.c:8–23`.
- EC42 `common/engine/ec_diagnostic.c:12–159`, `common/interfaces/ec42_protocol.h` and `ec_hardware.h`.
- Provenance: `development/alectardy98_ec/licensing/{REPORT.md,PROVENANCE.md,EVIDENCE.md,FIRMWARE-COMPARISON.json}`. Cipulot's original GPL notices and later-work attribution must remain in any refactoring.

Method: source/call-site searches, invariant/reachability tracing, original-versus-adapted comparison, existing ARM ELF symbol inspection with `arm-none-eabi-nm -S --size-sort`, existing release comparison data, and offline GUI CTest. No cycle counts were measured. Source-level calls may be inlined or removed by LTO; source line counts and largest symbol sizes are not recoverable-byte estimates.

## 1. What Cipulot's architecture gets right

Cipulot has a small compile-time integration surface: board pin/mux definitions, direct acquisition helpers, a common scan loop and one decision function. Selection, charging, sampling and discharge are visible in order. The charge/read critical region is explicit; there is no virtual dispatch or asynchronous ADC abstraction concealing timing. Runtime thresholds are computed outside the normal comparison path, apart from a baseline adjustment. Startup baseline acquisition is separate from bottom capture. Runtime `ec_config` is separated from persisted `eeprom_ec_config`. QMK hooks and VIA menu handling are separate from the electrical sampling loop.

The structure is easy to follow because each helper describes a hardware operation and `ec_update_key()` is a linear decision. EC42 should retain these characteristics. However, the original also couples peripheral acquisition and generic decisions in one file, captures simple raw maxima, uses sentinel/fallback values instead of explicit per-key validity, and exposes direct configuration writes. Its simplicity does not supply EC42's transactional review, partial calibration, qualified capture, universal telemetry, backup/restore or verified automatic persistence.

Original baseline calibration averages startup readings with 5 ms waits between sweeps. The scan selects mux/row, charges and reads under `ATOMIC_BLOCK_FORCEON`, discharges, waits `DISCHARGE_TIME`, then updates capture or state. The original bottoming path initializes each key from its first sample and thereafter retains the maximum; it does not prove a physical press. Its 1023 fallback is not an independently measured bottom. Copying this mechanism back would reintroduce the spike/untouched-key problems EC42 deliberately solved. The original VIA start/stop path is not universally a timed GUI workflow; distinguish that source from the historical 0413 timed implementation.

## 2. Subsystem comparison

| Subsystem | Cipulot | Current EC42 | Why different / recommendation / risk |
|---|---|---|---|
| Acquisition | Common mux/row helpers and ADC loop driven by config | Prox adapts that driver; 0413 retains its own electrical driver; Matrix retains its opamp/mux sequence | Necessary electrical differences. Retain direct board code; sharing textual resemblance could change settling or interrupt behavior. High risk to unify. |
| Hardware boundary | Compile-time config and direct functions | Direct `ec_process_sample()` for Prox/Matrix; 0413 explicit capture path; descriptive callback structs unused by firmware | Actual boundary is appropriately simple. Do not activate a callback registry. Clarify or remove the unused descriptive header after documentation review. |
| Startup | Initialize pins, average resting baseline, load/configure thresholds | Board baseline plus shared record validation/load; invalid record gives disabled keys in RAM | Necessary empty-calibration safety. Preserve startup ordering and no automatic EC record replacement. |
| Baseline drift | Lower-than-baseline update in `ec_update_key()` | Same downward-only concept, gated by active valid calibration and idle capture state | Not general bidirectional temperature tracking. Retain behavior; speculative drift redesign is not cleanup. |
| Bottom capture | First sample then maximum, starter flags | Sustained 16-scan blocks, qualified low/high, explicit review and valid/enabled/retry bits | Additional complexity essential. Preserve exact qualification and unlimited session behavior. |
| Partial calibration | No equivalent validity/enabled transaction | Candidate merges valid captures with existing keys; disabled keys suppressed before state decision | Essential. Do not equate absence, untouched and invalid. |
| Fixed actuation | Precomputed per-key thresholds from global scaled levels | Same basic principle plus per-key percentage override and Deepest bound | Necessary feature. Keep integer rounding, strict comparisons and six-count release headroom. |
| RT | Extremum-based movement within initial active zone | Initial VA crossing, median-of-three, turning points, fixed floor reset, global independent offsets | Necessary behavioral improvement. Original fallback is obsolete; original algorithm is not an equivalent replacement. |
| Configuration | `mode_0`/`mode_1` fields and arrays | Same names persist through canonical explicit record and live cache | Stored names/layout must remain; internal runtime naming can improve without changing bytes. |
| Persistence | Direct QMK field/block updates and defaults | CRC, quarantine, readback, staged apply, error status, QMK wear leveling and guarded EFL | Necessary safety. Shortening this to original direct setters loses guarantees. |
| Protocol | VIA custom menu IDs | Shared 32-byte EC42 packet parser, paged frozen snapshot, revisions, backup/restore | Necessary GUI features. Fixed packet layout and errors remain stable. |
| QMK integration | Shared matrix/board/VIA hooks | Shared integration for two boards; native 0413 hooks additionally quiet actuators, handle lock songs and legacy status | Some duplication is justified. Factor only identical routing with explicit board hooks after tests. |
| Error handling | Mostly direct state/config manipulation | Range/session/version/revision/storage/released guards | Do not classify repeated boundary checks as redundant solely by count. Independent entry points require validation. |
| GUI | VIA menus | Qt/HIDAPI device, protocol, settings/calibration controllers and telemetry UI | Necessary additional layer. Board layout/UID profiles are real metadata; do not replace with model-name checks or generic geometry guesses. |

## 3. Current canonical invariants and hardware exceptions

All three use the same common EC42 v2.9 handler and explicit record serialization: **17 + 3 × matrix cells**. The RAM working record uses method 3; persisted method/tag is 0x42. Conversion is current internal encoding, not legacy migration. Preserve the distinction.

| Board | Geometry | Native record / magic | Physical persistence | Application / packaging |
|---|---:|---:|---|---|
| 0413 | 5×21 | 332 bytes / A105 | Sector 2; logical 4096; record split 219+113 to preserve Vial bases | 08010000–0801FFFF; TinyUF2 |
| Prox | 5×15 | 242 / A975 | Sector 1; 8 KiB backing / 4096 logical | Vectors 08000000, text 08008000; FF-gap clean-install BIN |
| Matrix | 6×16 | 305 / A996 | Sector 7; 128 KiB backing / 4096 logical | ASTRO sector-5 app 08020000–0803FFFF; custom-compatible UF2 |

0413's tail is logical 3983–4095; the macro boundary/terminator guard is deliberate compatibility protection. Prox/Matrix use contiguous keyboard records. These adapters must not be collapsed into identical addresses. Device UID, pinned Vial build IDs, keymap-dependent allocations and packaging remain board-specific. The original boards and upstream QMK are outside refactoring scope.

## 4. Concrete findings and priorities

Each entry supplies current/reference/proposal, expected value, risk, feature contract, complexity and recommendation.

### P0 — Make current-source validation reproducible (low firmware risk)

**Component:** tests/build documentation. **Current:** README:112–113,477,579–588 and TESTING.md:11–18 advertise `development/alectardy98_ec/build-baselines.sh`, `test.sh`, `test-ports.sh`, `port-harness.py` and packaging tools; that directory currently contains only licensing materials (plus this report). `util/CMakeLists.txt` conditionally adds `framework_port_live` only if `port-harness.py` exists. The legacy harness compiles a frozen v2.8 C fixture, not the current engine, despite dependencies on current paths. **Cipulot reference:** small direct source build; no claim of equivalent historical fixtures.

**Proposal:** recover/review a matching current-source generator, make the current-board suite explicitly required or visibly skipped, distinguish frozen compatibility tests from current scanner tests, and correct executable commands. Archived older tools exist but are not automatically compatible; do not blindly restore them. **Benefit:** reliable evidence for every later cleanup. **Risk:** test infrastructure may expose real failures; avoid changing firmware to accommodate obsolete harness assumptions. **Feature impact:** none intended. **Complexity:** medium. **Recommendation:** implement first; require fresh normal/sanitizer all-board tests before runtime changes.

### P1 — Retire proven legacy decision fallback (low/medium risk)

**Component:** decision/runtime caches. **Current:** `record_storage.inc:4` defines `static const bool va_active=true`; `actuation_control.inc:109` returns it from `ec_va_dynamic()`. `decision.c:40` returns through `ec_rt_key()` for mode 1, making the subsequent original mode-1 branch unreachable. `ec_diagnostic.c:60` has the same unreachable fallback. Canonical load/import/update paths permit modes 0/1; invalid records disable keys before this decision. `EC_CAL_AUTO` is never returned by current `method()` (3 or 255), so its current-engine checks are obsolete, not support for automatic calibration.

**Cipulot reference:** that fallback was the original RT implementation; its precomputed per-key movement arrays were used there. **Proposal:** remove fallback branches and obsolete method checks, then audit runtime-only `rescaled_mode_1_*` arrays. Keep native record header fields, serializer offsets, descriptor and diagnostic outputs unchanged. Do not remove EC41 boot entry or frozen fixtures just because their names are old. **Benefit:** simpler single RT path; potential RAM savings below. **Risk:** an overlooked consumer or future out-of-tree integration could rely on internal struct fields; test all framework consumers, not original external keyboards. **Feature impact:** same VA-first/filter/floor behavior and wire representation. **Complexity:** medium. **Recommendation:** implement after P0, with trace equivalence and artifact measurement. LTO may already remove unreachable instructions; do not promise flash gains.

### P2 — Readable transaction/record code, not a new abstraction (low/medium risk)

**Component:** `calibration.c`, `.inc` helpers. **Current:** densely packed validation, protocol dispatch, state assignments and offset arithmetic in one translation unit; code/raw record tag distinction hidden behind names `packed`, `cell`, `seal`. CRC32 bit loops occur in both record checksum and transport CRC with different byte-selection semantics. **Cipulot reference:** named focused helpers and straightforward control flow.

**Proposal:** expand formatting first; name statuses and header offsets; add narrowly scoped helpers for repeated LE-native/BE-wire access and explicit transition intent. Keep `.inc` arrangement while the compiler-size tradeoff is measured. A shared CRC update primitive is a separate measured experiment: record checksum skips CRC fields while transport checksum covers the complete wire payload, so they are not interchangeable calls. **Benefit:** maintainability/reviewability, less accidental state/byte-order confusion. **Risk:** changing evaluation order, errors, CRC domains or optimizer decisions. **Feature impact:** byte-identical records/packets and transitions. **Complexity:** low for formatting, medium for helpers. **Recommendation:** implement incrementally with golden vectors; do not combine with storage redesign.

A related documentation correction belongs here: `ec_switch_matrix.h` still describes actuation mode 2 and offset semantics from the older implementation, although current validation accepts only 0/1. Correct comments without changing stored values.

### P3 — Clarify/remove descriptive-only interface and no-op declarations (low risk)

**Component:** `common/interfaces/ec_hardware.h`, `ec_diagnostic.h`. **Current:** whole-framework source search finds no includes/users of `a98_ec_hardware_t`, `a98_ec_storage_t` or normalization helper outside that header. README already calls them descriptive, not a registry. `ec_calibration_request_reset()` is defined twice across alternative board integrations but has no current caller; `ec_cal_sample(...,bool logical)` does not use its final argument. **Cipulot reference:** actual direct-function/config contract.

**Proposal:** document the real direct acquisition boundary in one place; either remove unused conceptual types or clearly move them to design notes. Remove no-op API after checking test fixtures; remove unused internal argument separately. **Benefit:** less misleading API surface. **Risk:** fixtures/out-of-tree source API, no protocol risk. **Feature impact:** preserve diagnostic logical-state argument to `ec_diagnostic_sample()`—that one is used. **Complexity:** low. **Recommendation:** implement after tests. No runtime size benefit promised for unused headers/no-op functions already dead-stripped.

### P4 — Audit redundant runtime arrays (medium risk)

**Current/reference:** Cipulot's runtime struct mirrored both modes and capture scratch. EC42 retained those fields while adding its own qualified scratch. Current runtime writes per-key mode1 offsets but only unreachable fallback reads them; `rescaled_mode_1_initial_deadzone_offset` also needs fallback/diagnostic reference review. `bottoming_calibration_starter` is written but no longer drives acquisition qualification. Runtime `bottoming_reading` still feeds print/telemetry peak: not dead. **Proposal:** remove only proven runtime-only unused fields; retain persisted `bottoming_reading` and every actual consumer. **Benefit:** RAM/cache initialization work, smaller mental model. **Risk:** confusing persisted and runtime structs or removing a diagnostic value. **Feature contract:** no record layout change. **Complexity:** medium. **Recommendation:** investigate/measure with P1, not blanket struct replacement.

### P5 — Preserve capture semantics before sharing sample wrapper (medium/high risk)

**Current:** Prox/Matrix call `ec_process_sample()` which suppresses HID during capture and reports logical false. 0413 `matrix.c:248–274` has a separate `capture_matrix` and runs `ec_update_key()` on it, publishing its logical bit while clearing HID rows. The calibration collector ignores logical activity for qualification, but telemetry does not. **Reference:** original scanner captured maxima or updated keys in mutually exclusive branches.

**Proposal:** retain current difference initially. Establish whether capture-time logical indication is a contractual GUI/native behavior; if unifying later, use an explicit static policy/helper with identical state reset and drift behavior. **Benefit:** less orchestration duplication only if semantics intentionally reconciled. **Risk:** capture telemetry change, baseline adjustment during capture, key release sequencing. **Complexity:** medium. **Recommendation:** investigate further; not an equivalent low-risk cleanup. No callback-per-sample design.

### P6 — Consolidate framing constants, not compatibility features (medium risk)

**Current:** opcodes 0x20–0x2A, statuses and limits are partly literals in calibration/actuation handlers despite `ec42_protocol.h`; two BE16 helper implementations; 0413/shared QMK wrappers implement EC41 boot routing differently. **Reference:** original VIA enum centralization. **Proposal:** centralize names without altering values/validation order; preserve EC41 status/boot behavior and board-specific actuator quieting. **Benefit:** protocol maintenance and fourth-board clarity. **Risk:** accidentally changing malformed packet handling or 0413 native safety. **Complexity:** medium. **Recommendation:** implement named constants after golden packet tests; retain separate board policies.

### P7 — Foreground task repeated work (medium risk, performance hypothesis)

**Current:** `ec_va_task()` copies `eeprom_ec_config` into candidate on every pending scan before the 2000 ms gate; `va_released()` scans enabled keys and recomputes boundaries before persistence. 0413 readiness scans the macro buffer for safe separation. **Reference:** original direct saves have no deferred-safe state machine.

**Proposal:** measure whether candidate copy can move below elapsed-time check, preserving all candidate consumers/session guards; retain actual released-state and storage checks at commit. **Benefit:** avoid repeated 242–332-byte copies during debounce, possible lower CPU use. **Risk:** candidate is shared mutable scratch; validate command sequencing before moving writes. **Complexity:** medium. **Recommendation:** investigate after source tests; do not cache storage health or macro readiness across writes without invalidation proof. No measured cycle saving.

### P8 — Acquisition loop micro-optimizations (high risk / low priority)

**Current:** Prox/0413 recompute mux-column prefix sums; unused-position checks are linear lists; Matrix retains commented debug fragments and compile-time `SCAN_ONE` branch. **Reference:** much of this is inherited Cipulot/Matrix simplicity. **Proposal:** remove demonstrably inert commented experiments as documentation cleanup; consider prefix constants only after disassembly/timing. **Benefit:** readability; CPU benefit unproven (compiler may fold constants). **Risk:** altered selection/order or scan cadence. **Complexity:** low comments, medium/high runtime. **Recommendation:** retain working acquisition; do not chase loop shortening before proving a bottleneck.

## 5. Performance and size evidence

Existing final licensing-validation artifacts were inspected, not rebuilt:
`/home/alec/firmware-analysis/analysis/ec42_licensing/20261021/build-tree/.build/`.

| Target | Application span bytes | Linked section bytes | Free application bytes | Data / BSS bytes |
|---|---:|---:|---:|---:|
| 0413 vial | 62,320 | 62,318 | 3,216 | 1,784 / 18,748 |
| 0413 vial_alec | 63,720 | 63,718 | 1,816 | 1,788 / 18,748 |
| Prox vial | 117,428 | 85,138 | 13,644* | 1,520 / 15,320 |
| Prox vial_alec | 120,588 | 88,298 | 10,484* | 1,520 / 15,332 |
| Matrix vial | 81,548 | 81,546 | 49,524 | 1,612 / 18,656 |
| Matrix vial_alec | 83,348 | 83,346 | 47,724 | 1,612 / 16,552 |

*Prox span includes vector-to-text storage hole; free here uses conservative 08020000 end. Do not compare gap-spanning file length as executable content. Section totals exclude two alignment bytes where applicable. Linker stacks total 3072 bytes separately; data+BSS alone is not total reserved RAM.

Measured `vial_alec` ELF symbols:

| Symbol | 0413 | Prox | Matrix |
|---|---:|---:|---:|
| `ec_config` runtime object | 1586 | 1136 | 1452 |
| `candidate` / `previous`, each | 332 | 242 | 305 |
| `frozen` snapshot | 2100 | 1500 | 1920 |
| raw/min/max, each | 210 | 150 | 192 |
| RT/capture window low/high, each | 210 | 150 | 192 |
| overrides | 105 | 75 | 96 |

Two old per-key uint8 RT offset arrays represent **2N = 210/150/192 bytes**; old per-key uint16 initial threshold another **2N**. Removing all three, if full consumer audit confirms redundancy, would eliminate **4N = 420/300/384 bytes of fields**, before structure alignment. This is a source storage estimate, not measured post-change BSS. Starter bool array is another N candidate only after proving all consumers. Do not count active capture/median scratch twice: it is deliberately reused across mutually exclusive modes.

0413 `ec_update_key` is 640 bytes; Matrix 376. `ec_diagnostic_command` is 4316 bytes (0413), 5548 (Prox), 1872 (Matrix). These are **not equivalent standalone component sizes**: inlining/LTO absorbed calibration/protocol work differently. Prox's `_crt0_entry` symbol spans other code and is not an 80 KiB removable startup function. Largest 0413 symbols include `process_record` 7616 and `main` 6560; likewise not savings targets wholesale.

Hot path is acquisition → enabled check → optional downward drift update → fixed comparison or median/extremum decision → diagnostic sample/cache/extrema → calibration collector. Full CRC/record validation occurs on control/load paths, not every key sample. Threshold division is normally precomputed; it reappears on drift/settings/load, not every stable fixed decision. Frozen snapshot assembly occurs on LATCH, not every scan. Continuous minima/maxima adds comparisons per sample but supports existing telemetry; removing it loses a feature. Physical ADC waits/atomic regions remain the primary timing-sensitive boundary; no measured claim about relative CPU share is justified here.

## 6. State and persistence complexity to retain

Keep Idle → Manual → Review → Ready → Applied and explicit save/cancel. They distinguish untrusted evidence, validated proposal, active RAM and durable record; collapsing Review/Ready/Applied would make rollback/error handling ambiguous. Saturating counts and bounded 16-scan windows protect unlimited sessions. `previous` is rollback, candidate is staged capture/settings; deleting either requires a separate lifecycle proof.

`store_raw()` quarantines the record, verifies invalidation, writes payload, verifies bytes, then verifies the final record. This is corruption/torn-write detection, **not a second durable copy guaranteeing the last calibration survives any power loss**. A torn record may require recalibration or host backup recovery. Preserve QMK journal behavior and host backup; do not promise atomic old-record retention that this layer does not supply. CRC-native and transfer CRC cover different representations. The explicit record tag and RAM tag are not obsolete backward compatibility.

Keep per-call EFL address/alignment/sector/WRP checks. They protect actual flash operations and latch storage failure. Prox's original descriptor/geometry workaround was required by real hardware; it is not disposable diagnostic clutter. Keep Matrix ASTRO RTC magic/clock/source policy, unchanged UF2 pipeline, and 0413 TinyUF2 path separate.

## 7. GUI and build technical debt

The GUI already separates device transport, protocol, calibration/settings and presentation. `util/src/boardprofile.cpp:4–14` intentionally maps UIDs to board layout, dimensions and record magic; current fourth-board onboarding still needs a profile/layout entry. This is modest explicit metadata, not a generic capability-driven arbitrary-layout UI. Improving that later is distinct from scanner cleanup; do not advertise zero GUI edits today.

Keep legacy GUI fixture coverage as a historical compatibility oracle, clearly labeled. Do not delete it as dead firmware. Do not restore old automatic calibration or timed modes. CMake source dependencies do not turn the frozen v2.8 harness into a current v2.9 test. Missing optional current harness must be visible.

Normal Python 3.14 support uses JSON `matrix_size` metadata to avoid QMK's deprecated AST conversion path. Preserve this existing solution; no sitecustomize, wrappers or upstream `lib/` changes are needed. Prox packaging and deterministic keymap-specific Vial IDs are justified build complexity. Removing them would lose safe FF-gap images or stable EEPROM identities. Unlinked historical music/equivalence helpers should be inventoried separately against tests before deletion; no savings can be claimed for unlinked code.

## 8. Staged approval/validation plan

| Stage | Change scope | Required regression evidence | Physical validation |
|---|---|---|---|
| 0 | Recover current-source tooling and coverage reporting | Fresh actual scanner compilation for all boards; normal+ASan/UBSan; packet/record golden vectors; packaging verifier | None for test-only work; hardware not accessed by tests |
| 1 | Proven dead branches/no-op internal APIs; runtime fields | All modes, invalid startup, partial saves/retries, disabled-key suppression; compare serialized bytes and telemetry; six builds and sections/maps | Typing, calibrated/disabled keys, RT off/three sensitivities, native diagnostics |
| 2 | Formatting/names/helper extraction in existing TU | Identical opcode/error/CRC/record vectors; stale session/revision, restore replay/timeouts; held-key writes denied; disconnect rollback | Calibration cancel/retry, partial/incremental save, suspend/reconnect, persistence after owner power cycle |
| 3 | Deferred-copy optimization and optional orchestration sharing | Command interleavings, pending-save capture blocked, released checks; compare per-sample state traces incl capture logical bits; size/timing evidence | Key-held drag, no stuck HID, actuator quieting, selected-key graph under capture, scan latency/jitter |
| 4 | Only if measured need: acquisition simplification | Board-specific order/timing mocks and actual scanner tests; all firmware/GUI regressions | Logic-analyzer GPIO/charge/discharge timing, ADC noise distributions and rails; USB/load/lighting/audio concurrent; long typing |

For every firmware stage: build vial and vial_alec on all three with standard make/system Python; verify same IDs/layouts/record bytes/addresses; check 0413 64 KiB, Matrix sector5-only UF2 and ASTRO handoff, Prox FF-gap/load sections; compare symbol maps without assuming byte identity after source refactoring. Preserve known-good images before experiments. Do not flash without explicit authorization. No intentional protocol/storage migration belongs to this plan.

Essential traces: shallow/mid/Deepest with varied baseline/span; equality boundaries and minimum RT spans; initial VA with RT enabled, partial release/reactuation/floor reset at three sensitivities; isolated spikes versus sustained 16-scan capture; untouched/invalid/retry/incremental sessions; calibration longer than 30 seconds; settings while held; timer/revision rollover; disconnect pending save; erased/corrupt/incompatible record; dropped/partial writes at each quarantine/readback phase; macro-tail conflict on 0413; Vial keymap/macro preservation; boot requests. Simulated analog traces do not validate physical settling or drift behavior.

## 9. Areas to retain as appropriate

- Direct compile-time board acquisition and MCU-specific timing, not a generic ADC plugin.
- Shared VA/RT/capture engine; per-key threshold cache and median scratch reuse.
- Single translation-unit `.inc` organization until a measured alternative fits 0413.
- Strict disabled-key gate before HID, while sampling continues.
- Explicit transaction/session/revision and persistence statuses; no automatic capture save.
- Canonical record bytes and board-specific placement; strict storage guards and backup checks.
- Frozen paged telemetry and actual logical states; no threshold-derived GUI substitute.
- Separate ASTRO/TinyUF2/ROM-DFU packaging and recovery paths.
- Native actuator, lighting, identity and Vial behavior; original attribution/licensing.

## 10. Validation performed and limitations

Static searches establish the current unused conceptual interface and no-op API, constant-true VA invariant, unused logical collector parameter, fallback dependence and missing tool paths. Existing matching six-target ELF files were inspected; no new firmware-size claims or binaries were generated. Prior licensing comparison recorded unchanged executable firmware after header-only work. Offline GUI CTest was rerun against that analysis build: **13/13 passed, 29.80 seconds**. The existing ASan/UBSan analysis build also passed **13/13, 39.22 seconds**. Commands: `ctest --test-dir /home/alec/firmware-analysis/analysis/ec42_licensing/20261021/gui-build --output-on-failure` and the equivalent `gui-sanitize` directory. These suites include frozen v2.8 tests and offline board/UI tests; it does not restore the missing fresh current-board generator or prove physical equivalence.

The review cannot prove absence of analog/timing regressions for proposed code, nor guarantee optimizer size changes. It also does not certify licensing distribution clearance; the licensing report's unresolved Matrix/imported-material questions remain intact. No existing tests or historical fixtures were removed. Next action is approval of Stage 0, followed by narrowly scoped Stage 1—not a framework rewrite.
