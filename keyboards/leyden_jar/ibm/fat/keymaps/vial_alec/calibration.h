#pragma once

/* Personal calibration for Alec's modified FAT PCB. The shared generated
 * FAT definition now contains the compiler-derived, physically checked row map
 * and populated-pad mask. Only calibration overrides belong in this keymap.
 */
/* Retain native calibration machinery and six sorted bins at +16 counts.
 * Custom bin 0 preserves the populated controller-pad (7,14) outlier.
 * Custom bin 1 isolates QMK (5,2), which maps to controller (2,2). Its
 * sustained held reading was only 0..2 counts above the old group trigger.
 * Use its own startup reference +12, without lowering the other keys' offset.
 * Previously recorded neighbouring responses rose by up to 11 counts; the
 * captured-signal regressions check that the new threshold still rejects them.
 * Coordinates below are controller coordinates, not QMK coordinates.
 * These are native DAC counts, not the old AVR controller's DAC units.
 */
#undef NB_CAL_BINS
#define NB_CAL_BINS 8
#define NB_CUSTOM_CAL_BINS 2
#define CUSTOM_CAL_BIN_KEYS {{0, 7, 14}, {1, 2, 2}}
#undef ACTIVATION_OFFSETS
#define ACTIVATION_OFFSETS {16, 12, 16, 16, 16, 16, 16, 16}
