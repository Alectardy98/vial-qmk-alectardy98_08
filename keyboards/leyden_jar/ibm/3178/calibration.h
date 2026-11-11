#pragma once

/* Released capture on the installed 3178 spans 414..506 DAC counts, exceeding
 * the legacy sweep's 499 ceiling and a single shared +10 trigger. Separate
 * the two highest-baseline pads, then retain native sorted calibration for
 * the remaining keys. These are controller coordinates, not QMK rows.
 */
#undef NB_CAL_BINS
#define NB_CAL_BINS 8
#define NB_CUSTOM_CAL_BINS 2
#define CUSTOM_CAL_BIN_KEYS { \
    {.row = 6, .col = 2, .bin = 0}, \
    {.row = 6, .col = 9, .bin = 1} \
}
#undef ACTIVATION_OFFSETS
/* Holding Y/U/L raises inactive same-column pads by up to nine counts.
 * A +10 median offset left their triggers below those inactive levels.
 * Add eight counts of activation margin; group layout and scan cost stay
 * unchanged. The captured targets still have over 35 counts of margin.
 */
#define ACTIVATION_OFFSETS {18, 18, 18, 18, 18, 18, 18, 25}
