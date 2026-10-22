# Variable Actuation and Dynamic Rapid Trigger

All current firmware is canonical EC42 v2.9. VA always determines initial activation
for calibrated enabled keys. Global/per-key1..98 percent plus99 Deepest use each
key's baseline/bottom; override0 inherits. Dynamic RT is a separate checkbox and
movement sensitivity; it releases/reactivates after initial activation and resets
below the fixed release floor. It does not replace initial VA.

Edits reach RAM immediately with revision/release checks. Settings auto-save after
two seconds of released-key idle; Applied/Saving/Saved/Error differ. Capture never
auto-saves: Start -> hold/release selected keys -> Finish -> Review -> Apply & Save.
Uncalibrated keys stay disabled but continue sensing. No timed or automatic bottoming.

The shared explicit record has17+3*N bytes and CRC; Prox/Matrix records are unchanged.
0413 now uses 332 bytes and needs fresh calibration. Export Vial first: its macro
capacity is 1691 bytes, with EC extension in the last113 EEPROM bytes. Overlong old
macros block saves rather than being overwritten. See framework README section5
for exact allocation, backup/rollback and the single-sector power-loss limitation.
