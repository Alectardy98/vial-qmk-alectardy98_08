/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/* The installed 75-key sweep has five released levels at 463-475 counts,
 * whose held levels remain above the old common trigger of 414. Give them
 * one native custom bin. The other seventy keys retain sorted calibration.
 * Coordinates below are controller sense/drive coordinates.
 *
 * Captured group bounds, including other keys being held:
 *   bin 0: held <=438, released >=460, reference 472, trigger 449
 *   bin 1: held <=414, released >=430, reference 444, trigger 422
 * Relative offsets follow each group's startup reference after reconnect.
 */
#define NB_CAL_BINS 2
#define NB_CUSTOM_CAL_BINS 1
#define CUSTOM_CAL_BIN_KEYS { \
    {.row = 0, .col = 18, .bin = 0}, \
    {.row = 1, .col = 18, .bin = 0}, \
    {.row = 2, .col = 18, .bin = 0}, \
    {.row = 3, .col = 18, .bin = 0}, \
    {.row = 3, .col = 0, .bin = 0} \
}
#define ACTIVATION_OFFSETS {-23, -22}
