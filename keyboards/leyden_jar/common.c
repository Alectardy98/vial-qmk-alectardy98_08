/* Copyright 2022 Eric Becourt (Rico https://mymakercorner.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include "quantum.h"
#include "haptic.h"
#include "common.h"
#include "dac.h"
#include "io_expander.h"
#include "pio_matrix_scan.h"
#include "calibration_store.h"

typedef struct {
    uint8_t bin;
    uint8_t row;
    uint8_t col;
} key_info_t;

typedef struct {
    uint16_t level;
    key_info_t key_info;
} key_with_level_info_t;

#define UNCONNECTED_LEVEL 380

#ifndef LEYDEN_JAR_LEVEL_SCAN_MAX
#define LEYDEN_JAR_LEVEL_SCAN_MAX 500
#endif
_Static_assert(LEYDEN_JAR_LEVEL_SCAN_MAX > 350 && LEYDEN_JAR_LEVEL_SCAN_MAX <= 1024,
               "Level sweep must fit the ten-bit DAC range");

#ifndef NB_CAL_BINS
#define NB_CAL_BINS 1
#endif

#ifndef NB_CUSTOM_CAL_BINS
#define NB_CUSTOM_CAL_BINS 0
#endif


static uint16_t s_matrix_levels[CONTROLLER_COLS][CONTROLLER_ROWS];
static key_with_level_info_t s_sorted_levels[CONTROLLER_COLS * CONTROLLER_ROWS];
static uint8_t s_bin_map[CONTROLLER_COLS][CONTROLLER_ROWS];
static uint16_t s_dac_thresholds[NB_CAL_BINS];
static uint16_t s_dac_ref_level[NB_CAL_BINS];
static size_t s_sorted_level_count;
static bool s_calibration_ready;

/* IBM ports exclude unwired pads from both calibration and key reporting.
 * Existing Model F Labs boards retain their original full-matrix behaviour. */
#ifdef LEYDEN_JAR_ACTIVE_PADS
static const uint8_t s_active_pads[CONTROLLER_COLS] = LEYDEN_JAR_ACTIVE_PADS;
#endif

static bool leyden_jar_pad_is_active(uint8_t row, uint8_t col) {
    if (row >= CONTROLLER_ROWS || col >= CONTROLLER_COLS) {
        return false;
    }
#ifdef LEYDEN_JAR_ACTIVE_PADS
    return (s_active_pads[col] & (1u << row)) != 0;
#else
    return true;
#endif
}
static bool s_is_keyboard_enabled;
static uint8_t s_RawMergedBinsMatrixScanValues[CONTROLLER_COLS];
static matrix_row_t s_logical_matrix_scan[MATRIX_ROWS];

static const int16_t s_bin_activation_offsets[] = ACTIVATION_OFFSETS;
_Static_assert(NB_CAL_BINS <= LEYDEN_JAR_MAX_CAL_BINS, "Too many calibration bins");
_Static_assert(sizeof(s_bin_activation_offsets) / sizeof(s_bin_activation_offsets[0]) == NB_CAL_BINS,
               "Every calibration bin needs a default offset");

#ifdef CUSTOM_CAL_BIN_KEYS
    static key_info_t s_custom_cal_bin_key_elems[] = CUSTOM_CAL_BIN_KEYS;
#else
    static key_info_t s_custom_cal_bin_key_elems[] = {};
#endif

#if defined(LEYDEN_JAR_MATRIX_ROWS) && defined(LEYDEN_JAR_MATRIX_COLS)
    /* Explicit connector translation; no AVR GPIO names enter these ports. */
    static const uint8_t s_matrixToControllerCol[(CONTROLLER_COLS > 18) ? CONTROLLER_COLS : 18] = LEYDEN_JAR_MATRIX_COLS;
    static const uint8_t s_matrixToControllerRow[(MATRIX_ROWS > 8) ? MATRIX_ROWS : 8] = LEYDEN_JAR_MATRIX_ROWS;
    static const uint8_t s_matrixLayout = MATRIX_LAYOUT_IS_XWHATSIT;
#ifdef LEYDEN_JAR_DIRECT_KEY_PIN
    /* The ninth logical row is a switch, never a comparator/sense channel. */
    _Static_assert(MATRIX_ROWS == 9 && LEYDEN_JAR_DIRECT_KEY_ROW == 8,
                   "Only the explicit extra direct-switch row may exceed eight");
    _Static_assert(LEYDEN_JAR_DIRECT_KEY_COL < MATRIX_COLS, "Direct switch column out of bounds");
    _Static_assert(LEYDEN_JAR_DIRECT_KEY_PIN == 27 && CONTROLLER_COLS == 16,
                   "Reserve GPIO27 by disabling the extra PIO column pair");
#else
    _Static_assert(MATRIX_ROWS <= 8, "Leyden Jar Rev 3 has eight sense inputs");
#endif
    _Static_assert(MATRIX_COLS <= CONTROLLER_COLS, "Logical columns exceed the configured scanner");
#ifndef LEYDEN_JAR_GPIO_COLUMNS
    _Static_assert(CONTROLLER_COLS <= 18, "Additional columns need an explicit scanner implementation");
#endif
#elif defined(MATRIX_FORMAT_XWHATSIT)

    #if defined(BOARD_MODEL_IS_F77) || defined(BOARD_MODEL_IS_F62)

    static const uint8_t s_matrixToControllerCol[18] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 255, 255, 255, 255, 255, 255, 255 };
    static const uint8_t s_matrixToControllerRow[8] = { 7, 6, 5, 4, 2, 0, 1, 3 };
    static const uint8_t s_matrixLayout = MATRIX_LAYOUT_IS_XWHATSIT;

    #endif

#elif defined(MATRIX_FORMAT_WCASS)

    #if defined(BOARD_MODEL_IS_F77) || defined(BOARD_MODEL_IS_F62)
        static const uint8_t s_matrixToControllerCol[18] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 255, 255, 255, 255, 255, 255, 255 };
    #else
        static const uint8_t s_matrixToControllerCol[18] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 255, 255 };
    #endif

    static const uint8_t s_matrixToControllerRow[8] = { 2, 1, 3, 0, 6, 5, 7, 4 };
    static const uint8_t s_matrixLayout = MATRIX_LAYOUT_IS_WCASS;

#elif defined(MATRIX_FORMAT_LEYDEN_JAR)

    static const uint8_t s_matrixToControllerCol[18] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };
    static const uint8_t s_matrixToControllerRow[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    static const uint8_t s_matrixLayout = MATRIX_LAYOUT_IS_LEYDEN_JAR;

#endif

#define MATRIX_TO_CONTROLLER_COL(col) s_matrixToControllerCol[col]
#define MATRIX_TO_CONTROLLER_ROW(row) s_matrixToControllerRow[row]

static const uint8_t *leyden_jar_acquire_scan(void) {
    pio_raw_scan();
#ifdef LEYDEN_JAR_GPIO_COLUMNS
    if (!pio_gpio_columns_ready()) {
        s_is_keyboard_enabled = false;
        s_calibration_ready = false;
        return NULL;
    }
#endif
    return pio_get_scan_vals();
}

static bool leyden_jar_detect_levels(void) {
    for (int col = 0; col < CONTROLLER_COLS; col++) {
        for (int row=0; row<CONTROLLER_ROWS; row++) {
            s_matrix_levels[col][row] = 0;
        }
    }

#ifdef LEYDEN_JAR_GPIO_COLUMNS
    /* A full-range linear sweep takes too long at startup.
     * A scan at one DAC value compares every active pad at once. Maintain a
     * true/false interval per pad and share each successive comparison across
     * all intervals. This searches the full ten-bit range without a clipped
     * calibration or a heap allocation; ordinary native boards keep their
     * original sweep below. -1/1024 are the out-of-range sentinels. */
    int16_t low[CONTROLLER_COLS][CONTROLLER_ROWS];
    int16_t high[CONTROLLER_COLS][CONTROLLER_ROWS];
    for (unsigned col = 0; col < CONTROLLER_COLS; ++col) {
        for (unsigned row = 0; row < CONTROLLER_ROWS; ++row) {
            low[col][row] = -1;
            high[col][row] = 1024;
        }
    }
    for (unsigned comparisons = 0; comparisons < 1024; ++comparisons) {
        int width = 1, probe = 0;
        for (unsigned col = 0; col < CONTROLLER_COLS; ++col) {
            for (unsigned row = 0; row < CONTROLLER_ROWS; ++row) {
                int span = high[col][row] - low[col][row];
                if (leyden_jar_pad_is_active(row, col) && span > width) {
                    width = span;
                    probe = low[col][row] + span / 2;
                }
            }
        }
        if (width == 1) {
            for (unsigned col = 0; col < CONTROLLER_COLS; ++col) {
                for (unsigned row = 0; row < CONTROLLER_ROWS; ++row) {
                    s_matrix_levels[col][row] = low[col][row] < 0 ? 0 : low[col][row];
                }
            }
            return true;
        }
        if (!dac_write_val(probe)) {
            s_is_keyboard_enabled = false;
            s_calibration_ready = false;
            return false;
        }
        wait_us(100);
        const uint8_t *raw = leyden_jar_acquire_scan();
        if (!raw) {
            return false;
        }
        for (unsigned col = 0; col < CONTROLLER_COLS; ++col) {
            for (unsigned row = 0; row < CONTROLLER_ROWS; ++row) {
                if (probe > low[col][row] && probe < high[col][row]) {
                    if (raw[col] & (1u << row)) {
                        low[col][row] = probe;
                    } else {
                        high[col][row] = probe;
                    }
                }
            }
        }
    }
    s_is_keyboard_enabled = false;
    s_calibration_ready = false;
    return false;
#else
    for (uint16_t dac_val=350; dac_val < LEYDEN_JAR_LEVEL_SCAN_MAX; dac_val++) {
        dac_write_val(dac_val);
        wait_us(100);
        const uint8_t* p_raw_vals = leyden_jar_acquire_scan();
        if (!p_raw_vals) {
            return false;
        }

        for (int col = 0; col < CONTROLLER_COLS; col++) {
            for (int row = 0; row < CONTROLLER_ROWS; row++) {
                if (p_raw_vals[col] & (uint8_t)(1<<row)) {
                    s_matrix_levels[col][row] = dac_val;
                }
            }
        }
    }
    return true;
#endif
}

static size_t leyden_jar_get_nb_custom_cal_bin_key_elems(void) {
    return sizeof(s_custom_cal_bin_key_elems) / sizeof(key_info_t);
}

static uint8_t leyden_jar_get_initial_bin(uint8_t row, uint8_t col) {
    size_t custom_cal_bin_key_elems = leyden_jar_get_nb_custom_cal_bin_key_elems();

    for (size_t i = 0; i < custom_cal_bin_key_elems; i++) {
        if (s_custom_cal_bin_key_elems[i].row == row && s_custom_cal_bin_key_elems[i].col == col) {
            return s_custom_cal_bin_key_elems[i].bin;
        }
    }

    return 0xFF;
}

static int leyden_jar_compare_vals(const void* pVal1, const void* pVal2) {
    if (((key_with_level_info_t*)pVal1)->key_info.bin < ((key_with_level_info_t*)pVal2)->key_info.bin) {
        return -1;
    }
    else if (((key_with_level_info_t*)pVal1)->key_info.bin > ((key_with_level_info_t*)pVal2)->key_info.bin) {
        return 1;
    }

    if (((key_with_level_info_t*)pVal1)->level < ((key_with_level_info_t*)pVal2)->level) {
        return -1;
    }
    else if (((key_with_level_info_t*)pVal1)->level > ((key_with_level_info_t*)pVal2)->level) {
        return 1;
    }
    else {
        return 0;
    }
}

static void leyden_jar_sort_level_values(void) {
    s_sorted_level_count = 0;
    memset(s_bin_map, 0xFF, sizeof(s_bin_map));
    for (int col = 0; col < CONTROLLER_COLS; col++) {
        for (int row = 0; row < CONTROLLER_ROWS; row++) {
            if (!leyden_jar_pad_is_active(row, col)) {
                continue;
            }
            key_with_level_info_t *key = &s_sorted_levels[s_sorted_level_count++];
            key->level = s_matrix_levels[col][row];
            key->key_info.bin = leyden_jar_get_initial_bin(row, col);
            key->key_info.row = row;
            key->key_info.col = col;
        }
    }

    qsort((void*)s_sorted_levels, s_sorted_level_count, sizeof(key_with_level_info_t), leyden_jar_compare_vals);
}

/* We check if the controller has been soldered to the keyboard PCB.
 *
 * For that we take the maximum level value of all the key matrix and compare it with UNCONNECTED_LEVEL (value is 380).
 * If the maximum level is below this reference value then we know that the Leyden Jar is unsoldered.
 * NOTE:
 *    Previously this detection whas done by taking the maximum level for each bin.
 *    This was problematic with the F122 keyboard where some bins had all their unpressed keys with a level below the
 *    UNCONNECTED_LEVEL reference value.
 *    Using the absolute maximum level of all bins combined solves this issue.
 */

static bool leyden_jar_is_controller_soldered(void) {
    if (s_sorted_level_count == 0 || s_sorted_levels[s_sorted_level_count - 1].level <= UNCONNECTED_LEVEL) {
        return false;
    }
    else {
        return true;
    }
}

/* We compute the threshold to were we consider that a key has been pressed.
 *
 * First step:
 * We detect the maximum level for all keys in the switch matrix and we sort those values in ascending order.
 * This will later be useful to retrieve the median and maximum values.
 *
 * Scenario 1:
 *   We try to manage the case were the controller is powered on while no columns/rows are soldered to a key matrix.
 *   This happens when you want to pre-flash the controller firmware before soldering the controller PCB
 *   to the key matrix.
 *   If we do nothing the controller can spam fake keypresses to the computer.
 *   We prevent this by taking the maximum detected level and comparing it to a low reference value.
 *   If the maximum detected level is equal or lower than this reference value then we know that no soldering to a
 *   key matrix have been done.
 *   We then configure the dac threshold with a value that will never trigger a key press:
 *     - 1023 for Model F keyboards.
 *     - 300 for Beamspring keyboards.
 *
 * Scenario  2:
 *   We detected that the controller PCB was properly soldered.
 *   The dac threshold value is then the median value of all detected levels with a fixed offset added to it.
 *   The offset value is defined by the ACTIVATION_OFFSET macro defined outside this file.
 *   The offset is positive for Model F keyboards and negative for Beamspring keyboards.
 *   Using the median value of all detected values has several advantages:
 *     - It gives close to an average value of levels.
 *     - It allows to ignore uninstalled flippers in the key matrix (lowest values).
 *     - It allows to ignore calibration pads in the key matrix (lowest or highest values).
 *     - It allows to ignore several pressed keys during boot up (does not work if all keys are pressed).
 *     - Consequently allows to use QMK BOOT_MAGIC lite feature. */

static void leyden_jar_compute_dac_thresholds(int bin_number, int16_t activation_offset, int first_elem_offset, int last_elem_offset) {
    uint16_t median_val = s_sorted_levels[first_elem_offset + ((last_elem_offset + 1) - first_elem_offset) / 2].level;
    bool is_controller_soldered = leyden_jar_is_controller_soldered();

    s_dac_ref_level[bin_number] = median_val;

    if (is_controller_soldered == false) {
        if (activation_offset > 0) {
            s_dac_thresholds[bin_number] = 1023;
        } else {
            s_dac_thresholds[bin_number] = 300;
        }

    } else {
        s_dac_thresholds[bin_number] = (uint16_t)((int16_t)median_val + activation_offset);
    }
}

/* Tie preferences to the compiled wiring and bin configuration. Different
 * keymaps/firmware must not silently inherit an incompatible calibration.
 * Sorted bins still regroup by released level on every startup, as before.
 */
static uint32_t calibration_signature(void) {
    uint32_t value = 2166136261u;
#define HASH_CONFIG(array) do { \
    const uint8_t *bytes = (const uint8_t *)(array); \
    for (size_t i = 0; i < sizeof(array); ++i) value = (value ^ bytes[i]) * 16777619u; \
} while (0)
    const uint8_t dimensions[] = {MATRIX_ROWS, MATRIX_COLS, CONTROLLER_ROWS, CONTROLLER_COLS,
                                 NB_CAL_BINS, NB_CUSTOM_CAL_BINS, leyden_jar_is_beamspring()};
    HASH_CONFIG(dimensions);
    HASH_CONFIG(s_matrixToControllerRow);
    HASH_CONFIG(s_matrixToControllerCol);
    HASH_CONFIG(s_bin_activation_offsets);
    HASH_CONFIG(s_custom_cal_bin_key_elems);
#ifdef LEYDEN_JAR_GPIO_COLUMNS
    const uint8_t drive_pins[] = {4, 5, 6, 7};
    HASH_CONFIG(drive_pins);
#endif
#ifdef LEYDEN_JAR_ACTIVE_PADS
    HASH_CONFIG(s_active_pads);
#endif
#undef HASH_CONFIG
    return value;
}

static bool leyden_jar_raw_matrix_scan(void) {

    memset(s_RawMergedBinsMatrixScanValues, 0, sizeof(s_RawMergedBinsMatrixScanValues));

    for (int bin_number = 0; bin_number < NB_CAL_BINS; bin_number++) {
        dac_write_val(s_dac_thresholds[bin_number]);
        wait_us(100);

        const uint8_t* p_raw_vals = leyden_jar_acquire_scan();
        if (!p_raw_vals) {
            /* Never report an I2C/capture failure as all beamspring keys down. */
            memset(s_RawMergedBinsMatrixScanValues, leyden_jar_is_beamspring() ? 0xff : 0,
                   sizeof(s_RawMergedBinsMatrixScanValues));
            return false;
        }

        for (int col = 0; col < CONTROLLER_COLS; col++) {
            for (int row = 0; row < CONTROLLER_ROWS; row++) {
                if (s_bin_map[col][row] == bin_number) {
                    s_RawMergedBinsMatrixScanValues[col] |= p_raw_vals[col] & (uint8_t)(1<<row);
                }
            }
        }
    }
    return true;
}

static const uint8_t* leyden_jar_get_scan_vals(void) {
    return s_RawMergedBinsMatrixScanValues;
}

void leyden_jar_init(void) {
    dac_init();
    io_expander_init();
    pio_matrix_scan_init(CONTROLLER_COLS >= 18);
#ifdef LEYDEN_JAR_DIRECT_KEY_PIN
    /* GPIO27 is not claimed by either of the two 16-column PIO programs.
     * J2.30/COL17 reaches it through R47 (1k); the button closes to GND.
     */
    setPinInputHigh(LEYDEN_JAR_DIRECT_KEY_PIN);
#endif

    s_is_keyboard_enabled = true;
}

void leyden_jar_calibrate(void) {
    if (!leyden_jar_detect_levels()) {
        return;
    }
    leyden_jar_sort_level_values();
    leyden_jar_calibration_store_load(calibration_signature(), NB_CAL_BINS, s_bin_activation_offsets);

    size_t nb_custom_cal_bin_key_elems = leyden_jar_get_nb_custom_cal_bin_key_elems();

    size_t bin_size_array[NB_CAL_BINS];
    size_t bin_size_index = 0;
    size_t start_offset = 0;
    size_t end_offset = s_sorted_level_count;

    if (end_offset < NB_CAL_BINS) {
        /* Invalid population/bins must never underflow a median index. */
        s_is_keyboard_enabled = false;
        return;
    }

    while (bin_size_index < NB_CUSTOM_CAL_BINS) {
        size_t nb_bin_elems = 0;
        for (size_t i = 0; i < nb_custom_cal_bin_key_elems; i++) {
            if (s_custom_cal_bin_key_elems[i].bin == bin_size_index) {
               nb_bin_elems++;
            }
        }
        bin_size_array[bin_size_index] = nb_bin_elems;
        bin_size_index++;
        start_offset += nb_bin_elems;
    }

    int nb_standard_bins = (NB_CAL_BINS - NB_CUSTOM_CAL_BINS);
    if (nb_standard_bins > 0) {
        int standard_bin_size = (end_offset - start_offset) / nb_standard_bins;

        while (bin_size_index < NB_CAL_BINS) {
            if (bin_size_index == NB_CAL_BINS - 1) {
                bin_size_array[bin_size_index] = end_offset - start_offset;
            }
            else {
                bin_size_array[bin_size_index] = standard_bin_size;
            }
            start_offset += bin_size_array[bin_size_index];
            bin_size_index++;
        }
    }

    size_t first_elem_offset = 0;
    for (size_t bin_number = 0; bin_number < NB_CAL_BINS; bin_number++) {
        size_t last_elem_offset = first_elem_offset + bin_size_array[bin_number] - 1;

        int16_t offset = leyden_jar_calibration_offset(bin_number);
        uint16_t reference = s_sorted_levels[first_elem_offset + (last_elem_offset + 1 - first_elem_offset) / 2].level;
        int32_t trigger = (int32_t)reference + offset;
        if (trigger < 0 || trigger > 1023 ||
            (leyden_jar_is_beamspring() ? offset >= 0 : offset <= 0)) {
            offset = s_bin_activation_offsets[bin_number];
        }
        leyden_jar_compute_dac_thresholds(bin_number, offset, first_elem_offset, last_elem_offset);

        for (size_t bin_elem = first_elem_offset; bin_elem <= last_elem_offset; bin_elem++) {
            s_bin_map[s_sorted_levels[bin_elem].key_info.col][s_sorted_levels[bin_elem].key_info.row] = bin_number;
        }

        first_elem_offset += bin_size_array[bin_number];
    }
    s_calibration_ready = true;
}

void leyden_jar_update(void) {
    io_expander_update_state();
}

void leyden_jar_enable(bool enable) {
#ifdef LEYDEN_JAR_GPIO_COLUMNS
    if (enable && (!s_calibration_ready || !pio_gpio_columns_ready())) {
        return;
    }
#endif
    s_is_keyboard_enabled = enable;
}

bool leyden_jar_is_enabled() {
    return s_is_keyboard_enabled;
}

void leyden_jar_logical_matrix_scan(matrix_row_t current_matrix[]) {
    for (int row = 0; row < MATRIX_ROWS; row++) {
        current_matrix[row] = 0;
    }
    if (!leyden_jar_raw_matrix_scan()) {
        return;
    }
    const uint8_t* p_raw_vals = leyden_jar_get_scan_vals();

    for (int col = 0; col < MATRIX_COLS; col++) {
        int physCol = (int)MATRIX_TO_CONTROLLER_COL(col);
        for (int row = 0; row < MATRIX_ROWS; row++) {
            int physicalRow = (int)MATRIX_TO_CONTROLLER_ROW(row);
            if (!leyden_jar_pad_is_active(physicalRow, physCol)) {
                continue;
            }
            matrix_row_t rowVal = (matrix_row_t)((p_raw_vals[physCol] >> physicalRow) & 1);
            #ifdef BEAMSPRING_KEYBOARD
                rowVal = (~rowVal) & 1;
            #endif
            rowVal = rowVal << col;
            current_matrix[row] |= rowVal;
        }
    }
#ifdef LEYDEN_JAR_DIRECT_KEY_PIN
    if (!readPin(LEYDEN_JAR_DIRECT_KEY_PIN)) {
        current_matrix[LEYDEN_JAR_DIRECT_KEY_ROW] |= (matrix_row_t)1u << LEYDEN_JAR_DIRECT_KEY_COL;
    }
#endif
}

bool leyden_jar_get_column_bin_map(uint16_t col_index, uint8_t* bin_map_ptr, uint16_t bin_map_buffer_size) {
    if (col_index >= CONTROLLER_COLS || bin_map_buffer_size < CONTROLLER_ROWS) {
        return false;
    }

    for (int i=0; i<CONTROLLER_ROWS; i++) {
        bin_map_ptr[i] = s_bin_map[col_index][i];
    }

    return true;
}

bool leyden_jar_set_detect_levels() {
    return leyden_jar_detect_levels();
}

bool leyden_jar_get_column_levels(uint16_t col_index, uint16_t* level_buffer_ptr, uint16_t level_buffer_size) {
    if (col_index >= CONTROLLER_COLS || level_buffer_size < CONTROLLER_ROWS) {
        return false;
    }

    for (int i=0; i<CONTROLLER_ROWS; i++) {
        level_buffer_ptr[i] = s_matrix_levels[col_index][i];
    }

    return true;
}

bool leyden_jar_get_dac_ref_level(uint16_t* ref_level_ptr, uint8_t bin_number_ptr) {
    if (ref_level_ptr == NULL || bin_number_ptr >= NB_CAL_BINS) {
        return false;
    }
    *ref_level_ptr = s_dac_ref_level[bin_number_ptr];
    return true;
}

bool leyden_jar_get_dac_threshold(uint16_t* dac_threshold_ptr, uint8_t bin_number_ptr) {
    if (dac_threshold_ptr == NULL || bin_number_ptr >= NB_CAL_BINS) {
        return false;
    }
    *dac_threshold_ptr = s_dac_thresholds[bin_number_ptr];
    return true;
}

bool leyden_jar_set_dac_threshold(uint16_t dac_threshold, uint8_t bin_number) {
    if (dac_threshold > 1023 || bin_number >= NB_CAL_BINS) {
        return false;
    }

    /* A rejected DAC transfer must not become an acknowledged RAM setting. */
    if (dac_write_val(dac_threshold) != 1) {
        return false;
    }
    s_dac_thresholds[bin_number] = dac_threshold;
    wait_us(100);

    return true;
}

bool leyden_jar_get_calibration_preference(uint8_t bin, int16_t *offset, int16_t *default_offset, bool *saved) {
    if (bin >= NB_CAL_BINS) {
        return false;
    }
    *offset = leyden_jar_calibration_offset(bin);
    *default_offset = s_bin_activation_offsets[bin];
    *saved = leyden_jar_calibration_is_saved(bin);
    return true;
}

bool leyden_jar_save_calibration_offset(uint8_t bin, int16_t offset) {
    if (!s_calibration_ready || !leyden_jar_is_controller_soldered() || bin >= NB_CAL_BINS ||
        (leyden_jar_is_beamspring() ? offset >= 0 : offset <= 0)) {
        return false;
    }
    int32_t trigger = (int32_t)s_dac_ref_level[bin] + offset;
    if (trigger < 0 || trigger > 1023) {
        return false;
    }
    uint16_t previous = s_dac_thresholds[bin];
    if (!leyden_jar_set_dac_threshold(trigger, bin)) {
        return false;
    }
    if (!leyden_jar_calibration_store_save(bin, offset)) {
        s_dac_thresholds[bin] = previous;
        return false;
    }
    return true;
}

bool leyden_jar_restore_calibration_defaults(void) {
    if (!leyden_jar_calibration_store_reset()) {
        return false;
    }
    /* Restore only calibration preferences, using the existing references.
     * Never reboot/recalibrate with a held key or erase QMK/Vial settings.
     */
    for (uint8_t bin = 0; bin < NB_CAL_BINS; ++bin) {
        int32_t trigger = (int32_t)s_dac_ref_level[bin] + s_bin_activation_offsets[bin];
        if (!leyden_jar_is_controller_soldered()) {
            s_dac_thresholds[bin] = leyden_jar_is_beamspring() ? 300 : 1023;
        } else if (trigger >= 0 && trigger <= 1023) {
            s_dac_thresholds[bin] = trigger;
        }
    }
    return true;
}

void eeconfig_init_kb(void) {
    /* QMK's EEPROM clear has already erased the block. Refresh RAM too, so
     * the old offsets cannot survive until the next restart. This hook also
     * runs before matrix initialization on a first boot, when refs are unset.
     */
    if (s_calibration_ready) {
        leyden_jar_calibration_store_load(calibration_signature(), NB_CAL_BINS, s_bin_activation_offsets);
        leyden_jar_restore_calibration_defaults();
    }
    eeconfig_init_user();
}

bool leyden_jar_set_enable_keyboard(uint8_t enable) {
    leyden_jar_enable(enable != 0);
    return leyden_jar_is_enabled() == (enable != 0);
}

bool leyden_jar_get_enable_keyboard(uint8_t* is_enabled) {
    *is_enabled = ( leyden_jar_is_enabled() == true ) ? 1 : 0;

    return true;
}

bool leyden_jar_set_scan_logical_matrix() {
    leyden_jar_logical_matrix_scan(s_logical_matrix_scan);
#ifdef LEYDEN_JAR_GPIO_COLUMNS
    return pio_gpio_columns_ready();
#else
    return true;
#endif
}

bool leyden_jar_set_scan_physical_matrix(void) {
    return leyden_jar_raw_matrix_scan();
}

bool leyden_jar_get_logical_matrix_row(uint32_t* logical_row_ptr, uint8_t row_index) {
    if (row_index >= MATRIX_ROWS) {
        return false;
    }

    *logical_row_ptr = (uint32_t)s_logical_matrix_scan[row_index];

    return true;
}

bool leyden_jar_get_physical_matrix_values(uint8_t* scan_ptr, uint16_t scan_buffer_size) {
    if (scan_buffer_size < CONTROLLER_COLS) {
        return false;
    }

    const uint8_t* p_raw_vals = leyden_jar_get_scan_vals();
    memcpy(scan_ptr, p_raw_vals, CONTROLLER_COLS);

    return true;
}

bool leyden_jar_get_matrix_to_controller_cols(uint8_t* matrix_to_controller_cols, uint8_t buffer_size)
{
    if (buffer_size < CONTROLLER_COLS) {
        return false;
    }

    memcpy (matrix_to_controller_cols, s_matrixToControllerCol, CONTROLLER_COLS);

    return true;
}

bool leyden_jar_get_matrix_to_controller_rows(uint8_t* matrix_to_controller_rows, uint8_t buffer_size)
{
    if (buffer_size < CONTROLLER_ROWS) {
        return false;
    }

    memcpy (matrix_to_controller_rows, s_matrixToControllerRow, CONTROLLER_ROWS);

    return true;
}

uint8_t leyden_jar_get_matrix_to_controller_type(void)
{
    return s_matrixLayout;
}

bool leyden_jar_is_beamspring()
{
    #ifdef BEAMSPRING_KEYBOARD
        return true;
    #else
        return false;
    #endif
}

#ifdef VIA_ENABLE

void via_init_kb(void) {
    if (!via_eeprom_is_valid()) {
        eeconfig_disable();
    }
}

#endif
