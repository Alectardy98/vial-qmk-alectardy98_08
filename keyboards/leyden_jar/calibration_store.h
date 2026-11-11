#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LEYDEN_JAR_MAX_CAL_BINS 16

void leyden_jar_calibration_store_load(uint32_t signature, uint8_t bins, const int16_t *defaults);
int16_t leyden_jar_calibration_offset(uint8_t bin);
bool leyden_jar_calibration_is_saved(uint8_t bin);
bool leyden_jar_calibration_store_save(uint8_t bin, int16_t offset);
bool leyden_jar_calibration_store_reset(void);
