#pragma once

/* The released FXTAL capture has one high-baseline pad: controller (3, 0),
 * QMK (4, 0), reads 392 while the rest of the matrix uses a 377 reference.
 * Isolate its reference instead of raising every key's activation offset.
 * Both Vial keymaps opt in; other boards and vial_Karolus stay unchanged.
 */
#undef NB_CAL_BINS
#define NB_CAL_BINS 2
#define NB_CUSTOM_CAL_BINS 1
#define CUSTOM_CAL_BIN_KEYS {{.row = 3, .col = 0, .bin = 0}}
#undef ACTIVATION_OFFSETS
#define ACTIVATION_OFFSETS {10, 10}
