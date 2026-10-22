// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <stdbool.h>
bool ec_diagnostic_command(uint8_t *data, uint8_t length);
void ec_diagnostic_sample(uint8_t row, uint8_t col, uint16_t raw, bool logical);
void ec_calibration_cancel(void);
void ec_calibration_request_reset(void);
