/* Copyright 2020 Purdea Andrei
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

#include "quantum.h"
#include "raw_hid.h"
#include "util_comm.h"
#include "matrix_manipulate.h"
#include "calibration_config.h"
#include <string.h>
#include "eeprom.h"
#include <progmem.h>

#if defined(KEYBOARD_SHARED_EP) && defined(RAW_ENABLE)
#error "Enabling the KEYBOARD_SHARED_EP will make the util be unable to communicate with the firmware, because due to hidapi limiations, the util can't figure out which interface to talk to, so it hardcodes interface zero."
#endif

#ifndef RAW_EPSIZE
#define RAW_EPSIZE 32
#endif

#define min(x, y) (((x) < (y))?(x):(y))

extern const char PROGMEM KEYBOARD_FILENAME[]; // This must be defined in keyboard_name.c to equal the filename. This is sent back to the PC-side software for it to determine which keyboard we are using.

static const uint8_t magic[] = UTIL_COMM_MAGIC;

void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
    if (length != RAW_EPSIZE || 0 != memcmp(data, magic, sizeof(magic))) {
        return;
    }
    uint8_t response[RAW_EPSIZE];
    memset(response, 0, sizeof(response));
    memcpy(response, magic, sizeof(magic));
    response[2] = UTIL_COMM_RESPONSE_ERROR;
    switch (data[2])
    {
        case UTIL_COMM_GET_VERSION:
            response[2] = UTIL_COMM_RESPONSE_OK;
            response[3] = UTIL_COMM_VERSION_MAJOR;
            response[4] = UTIL_COMM_VERSION_MID;
            response[5] = (UTIL_COMM_VERSION_MINOR >> 8) & 0xff;
            response[6] = (UTIL_COMM_VERSION_MINOR >> 0) & 0xff;
            break;
        case UTIL_COMM_DISABLE_KEYBOARD:
            response[2] = UTIL_COMM_RESPONSE_OK;
            response[3] = (uint8_t) keyboard_scan_enabled;
            keyboard_scan_enabled = 0;
            break;
        case UTIL_COMM_ENABLE_KEYBOARD:
            response[2] = UTIL_COMM_RESPONSE_OK;
            response[3] = (uint8_t) keyboard_scan_enabled;
            keyboard_scan_enabled = 1;
            break;
        case UTIL_COMM_ENTER_BOOTLOADER:
            keyboard_scan_enabled = 0;
            wait_ms(10);
            bootloader_jump();
            break;
        case UTIL_COMM_GET_KEYSTATE:
            response[2] = UTIL_COMM_RESPONSE_OK;
            {
                matrix_row_t current_matrix[MATRIX_ROWS];
                matrix_scan_raw(current_matrix);
                char *current_matrix_ptr = (char *)current_matrix;
                int offset = 0;
                if (sizeof(current_matrix) > 32-3)
                {
                    offset = data[3];
                    if (offset >= sizeof(current_matrix)) {
                        response[2] = UTIL_COMM_RESPONSE_ERROR;
                        break;
                    }
                    current_matrix_ptr += offset;
                }
                memcpy(&response[3], current_matrix_ptr, min(32-3, sizeof(current_matrix)-offset));
            }
            break;
        case UTIL_COMM_GET_THRESHOLDS:
            response[2] = UTIL_COMM_RESPONSE_OK;
            #if CAPSENSE_CAL_ENABLED
            response[3] = XCAL_BINS;
            {
                const uint8_t cal_bin = data[3];
                if (cal_bin >= XCAL_BINS || (sizeof(assigned_to_threshold[cal_bin]) > 26 && data[4] >= sizeof(assigned_to_threshold[cal_bin]))) {
                    response[2] = UTIL_COMM_RESPONSE_ERROR;
                    break;
                }
                response[4] = cal_thresholds[cal_bin] & 0xff;
                response[5] = (cal_thresholds[cal_bin] >> 8) & 0xff;
                char *assigned_to_threshold_ptr = (char *)assigned_to_threshold[cal_bin];
                int offset = 0;
                if (sizeof(assigned_to_threshold[cal_bin]) > 32-6)
                {
                    offset = data[4];
                    assigned_to_threshold_ptr += offset;
                }
                memcpy(&response[6], assigned_to_threshold_ptr, min(32-6, sizeof(assigned_to_threshold[cal_bin]) - offset));
            }
            #else
            response[3] = 0;
            response[4] = (CAPSENSE_HARDCODED_THRESHOLD) & 0xff;
            response[5] = ((CAPSENSE_HARDCODED_THRESHOLD) >> 8) & 0xff;
            #endif
            break;
#ifndef XWHATSIT_RUNTIME_CALIBRATION
        case UTIL_COMM_GET_KEYBOARD_FILENAME:
            {
                int string_length = strlen_P(KEYBOARD_FILENAME) + 1;
                const uint8_t offset = data[3];
                response[2] = UTIL_COMM_RESPONSE_OK;
                if (offset >= string_length) {
                    response[3] = 0;
                } else {
                    const char *substring = KEYBOARD_FILENAME + offset;
                    string_length -= offset;
                    if (string_length > RAW_EPSIZE - 3) {
                        string_length = RAW_EPSIZE - 3;
                    }
                    memcpy_P(&response[3], substring, string_length);
                }
                break;
            }
#endif
        case UTIL_COMM_ERASE_EEPROM:
            {
                response[2] = UTIL_COMM_RESPONSE_OK;
                uint16_t addr;
                for (addr=0; addr<=E2END; addr += 1)
                {
                    eeprom_update_byte((uint8_t *)(uintptr_t)addr, 0xff);
                }
                break;
            }
        case UTIL_COMM_GET_SIGNAL_VALUE:
            {
                response[2] = UTIL_COMM_RESPONSE_OK;
                uint8_t col = data[3];
                uint8_t row = data[4];
                uint8_t count = data[5];
                if (col >= MATRIX_COLS || row >= MATRIX_CAPSENSE_ROWS || count == 0 || count > 14) {
                    response[2] = UTIL_COMM_RESPONSE_ERROR;
                    break;
                }
                int i;
                for (i=0;i<count;i++)
                {
                    uint16_t value = measure_middle_keymap_coords(col, row, XCAL_SAMPLE, 8);
                    response[3+i*2] = value & 0xff;
                    response[3+i*2+1] = (value >> 8) & 0xff;
                    col += 1;
                    if (col >= MATRIX_COLS) {
                        col -= MATRIX_COLS;
                        row += 1;
                    }
                    if (row >= MATRIX_CAPSENSE_ROWS)
                    {
                        break;
                    }
                }
                break;
            }
        case UTIL_COMM_GET_KEYBOARD_DETAILS:
            {
                response[2] = UTIL_COMM_RESPONSE_OK;
                response[3] = MATRIX_COLS;
                response[4] = MATRIX_ROWS;
                #if defined(CONTROLLER_IS_XWHATSIT_BEAMSPRING_REV_4)
                response[5] = 1;
                #elif defined(CONTROLLER_IS_XWHATSIT_MODEL_F_OR_WCASS_MODEL_F)
                response[5] = 2;
                #elif defined(CONTROLLER_IS_THROUGH_HOLE_BEAMSPRING)
                response[5] = 3;
                #elif defined(CONTROLLER_IS_THROUGH_HOLE_MODEL_F)
                response[5] = 4;
                #else
                response[5] = 0;
                #endif
                response[6] = CAPSENSE_KEYBOARD_SETTLE_TIME_US;
                response[7] = XCAL_SETTLE;
                response[8] = XCAL_SAMPLE;
                response[9] = CAPSENSE_CAL_ENABLED;
                response[10] = CAPSENSE_DAC_MAX & 0xFF;
                response[11] = (CAPSENSE_DAC_MAX >> 8) & 0xFF;
                response[12] = MATRIX_CAPSENSE_ROWS;
                response[13] = 0; // reserved for future
                response[14] = 0; // reserved for future
                response[15] = 0; // reserved for future
                break;
            }
#ifdef XWHATSIT_RUNTIME_CALIBRATION
        case UTIL_COMM_CAL_CAPS:
            response[2] = UTIL_COMM_RESPONSE_OK;
            response[3] = 'X'; response[4] = 'C'; response[5] = 'A'; response[6] = 'L';
            response[7] = 1;
            response[8] = CAPSENSE_CAL_BINS;
            response[9] = CAPSENSE_CAL_ENABLED | (CAPSENSE_CAL_DEBUG << 1);
            response[10] = CAPSENSE_DAC_MAX & 255;
            response[11] = CAPSENSE_DAC_MAX >> 8;
            xcal_get_defaults(response + 12);
            break;
        case UTIL_COMM_CAL_SETTINGS:
            goto settings_reply;
        case UTIL_COMM_CAL_APPLY_SAVE:
        case UTIL_COMM_CAL_DEFAULTS:
        case UTIL_COMM_CAL_RUN:
            // A held key must never become the released calibration reference.
            response[2] = 0x24;
            if (data[2] != UTIL_COMM_CAL_DEFAULTS) {
                for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
                    if (matrix_get_row(row)) goto calibration_reply;
                }
            }
            response[2] = UTIL_COMM_RESPONSE_ERROR;
            if (data[2] == UTIL_COMM_CAL_APPLY_SAVE) {
                if (!xcal_valid(data + 3)) break;
                memcpy(xcal_settings, data + 3, 8);
            } else if (data[2] == UTIL_COMM_CAL_DEFAULTS) xcal_defaults();
            calibration();
            if (data[2] != UTIL_COMM_CAL_RUN) xcal_save();
        settings_reply:
            response[2] = UTIL_COMM_RESPONSE_OK;
            memcpy(response + 3, xcal_settings, 8);
        calibration_reply:
            break;
#else
        case UTIL_COMM_SHIFT_DATA:
        case UTIL_COMM_SHIFT_DATA_EXT:
            {
                response[2] = UTIL_COMM_RESPONSE_OK;
                uint32_t shdata = (((uint32_t)(data[3])) << 0) |
                                (((uint32_t)(data[4])) << 8) |
                                (((uint32_t)(data[5])) << 16) |
                                (((uint32_t)(data[6])) << 24);
                int data_idle = 0;
                int shcp_idle = 0;
                int stcp_idle = 0;
                if (data[2] == UTIL_COMM_SHIFT_DATA_EXT)
                {
                    data_idle = data[7];
                    shcp_idle = data[8];
                    stcp_idle = data[9];
                }
                shift_data(shdata, data_idle, shcp_idle, stcp_idle);
                response[3] = readPin(CAPSENSE_SHIFT_DIN);
                response[4] = readPin(CAPSENSE_SHIFT_SHCP);
                response[5] = readPin(CAPSENSE_SHIFT_STCP);
                break;
            }
        case UTIL_COMM_SET_DAC_VALUE:
            {
                response[2] = UTIL_COMM_RESPONSE_OK;
                uint16_t value = data[3] | (((uint16_t)data[4]) << 8);
                dac_write_threshold(value);
                break;
            }
        case UTIL_COMM_GET_ROW_STATE:
            {
                response[2] = UTIL_COMM_RESPONSE_OK;
                response[3] = test_single(255, 0, NULL);
                break;
            }
#endif
        default:
            break;
    }
    memcpy(data, response, sizeof(response));
}
