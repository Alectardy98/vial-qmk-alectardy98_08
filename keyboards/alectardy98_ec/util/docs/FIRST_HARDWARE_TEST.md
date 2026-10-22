# Owner-performed EC42 hardware checklist

Builds and offline tests do not perform these steps. Do not flash or change
physical calibration without explicit owner approval. Keep a second keyboard,
matching fallback firmware/GUI, Vial export and EC backup. Use the selected
board's documented bootloader and installation method; Matrix ASTRO, Prox ROM
DFU and 0413 TinyUF2 are not interchangeable.

1. Launch the canonical utility after closing Vial. Confirm the correct board,
   UID, EC42 minor (0413=8, Prox/Matrix=9), dimensions and storage READY. Uncalibrated
   keys must not type. If existing valid calibration is present, it must load
   without a migration prompt or reset.
2. Check idle readings with hands released. Preserve a disk backup. Start manual
   capture; fully press/hold/release one key. It becomes NEW only from firmware
   qualification, not a logical press or raw spike. Untouched keys stay inactive.
3. Finish/review and explicitly Apply & Save. Confirm the saved key types and
   uncalibrated keys do not. Repeat with additional keys and verify retained keys.
   Check retry, continue, cancel and reconnect without unwanted saves.
4. Test global shallow/medium/Deepest, per-key overrides and reset-to-global.
   Release all keys before editing. Changes should reach the scanner immediately
   and save after two seconds of released-key idle. Check Applied/Saving/Saved.
5. Test RT disabled and three sensitivities. VA must still determine initial
   activation; RT changes partial-travel release/reactivation; returning below
   the fixed floor resets the cycle. Check no chatter or stuck keys.
6. Check ADC graph and firmware-reported logical state, knowing sparse USB
   samples can miss brief movement. No voltage or millimetre calibration is claimed.
7. Power-cycle deliberately and verify EC settings, Vial keymaps/macros and
   native lighting/actuators. Live readback alone is not persistence proof.
8. Only with recovery established, test native bootloader entry. Matrix must
   enter ASTRO via RTC handoff; do not assume generic TinyUF2 double-reset.
   Software entry should work while ordinary keys are uncalibrated through Vial
   or the utility. This action resets the keyboard but does not install firmware.
9. Record what actually passed physically, the exact image hash, GUI/source
   version and remaining limitations. Never reset/erase calibration to diagnose
   discovery or bypass storage health checks.

Full flash boundaries, records and recovery procedures: framework README.md and
TESTING.md. Restore only a matching board/codec backup. Single-sector wear-level
consolidation is not a dual-bank power-loss guarantee.
