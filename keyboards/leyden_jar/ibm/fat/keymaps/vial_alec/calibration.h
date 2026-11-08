#pragma once

/* Personal calibration for Alec's modified FAT PCB. The shared generated
 * FAT definition now contains the compiler-derived, physically checked row map
 * and populated-pad mask. Only calibration overrides belong in this keymap.
 */
/* Retain native calibration machinery: one custom bin for the populated
 * (7,14) outlier, six sorted bins for other populated pads. Captured held
 * signals rise by 48..108 counts; one neighbour rises by up to 11 counts.
 * A +16 offset keeps those measured neighbour responses below threshold.
 * These are native DAC counts, not the old AVR controller's DAC units.
 */
#undef NB_CAL_BINS
#define NB_CAL_BINS 7
#define NB_CUSTOM_CAL_BINS 1
#define CUSTOM_CAL_BIN_KEYS {{0, 7, 14}}
#undef ACTIVATION_OFFSETS
#define ACTIVATION_OFFSETS {16, 16, 16, 16, 16, 16, 16}
