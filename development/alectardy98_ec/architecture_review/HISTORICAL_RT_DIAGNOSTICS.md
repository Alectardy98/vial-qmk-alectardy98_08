> Historical pre-v2.8 investigation fixture, not current scanner behavior. Current VA-first dynamic RT and wire format are documented in the framework README.md and EC42.md. Retained for diagnostic history.

# Rapid Trigger diagnostics

> Historical EC42 v2.5 behavior below. For the current v2.6 Variable Actuation interface, debounced automatic settings persistence, disabled Music Mode and rollback procedure, use [VARIABLE_ACTUATION.md](VARIABLE_ACTUATION.md). Manual calibration still uses explicit Apply & Save.

The checkbox, sensitivity slider and Advanced offsets are pending edits. They do not send settings. Enable RT separately, then explicitly use **Apply & Save RT / actuation settings**, approve the backup/save, and check firmware-reported mode and last verified settings. Closing/reopening the utility is not a keyboard power cycle.

The slider moves both independent offsets toward smaller ADC counts on its more-sensitive/right side. This affects release and reactivation movement above the initial RT point, not the initial actuation point. The latter is the Advanced normalized deadzone setting. APC ignores both RT offsets. The test firmware uses direct ADC counts for tagged manual records; production's inherited legacy affine/narrowing conversion is different. Do not copy test-firmware values to production blindly.

Offline tests use the actual test scanner: offsets 200/100/50 require movements 201/101/51 because comparisons are strict. This proves logical scanner behavior and protocol/runtime/reload plumbing, not physical travel sensitivity or USB HID timing. Actual GUI limits depend on every enabled calibrated key's measured travel/hysteresis and may differ from these test fixtures.

A saved valid method3 partial record is required to stage settings. Only enabled calibrated keys constrain the common safe range; unknown/disabled positions do not. At least one enabled key is needed. An unfinished capture does not enable settings writes. Partial calibration now fits the test target and is software-tested, but still requires physical validation. Firmware record/storage health must be ready before saving settings.
For physical diagnosis, export a backup, close Vial, read settings, record the reported mode and offsets, and compare partial-travel release/reactuation at three legal slider settings. Confirm each save/readback and release every key before writing. Keep the initial deadzone unchanged. Ordinary full-stroke typing may feel identical. Restore the backed-up settings afterward if desired. Do not invoke calibration just to test the slider.
