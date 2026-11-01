/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
bool my2k_main_scalar(void *, uint8_t command, uint8_t *value);
bool my2k_main_scalar_set(void *, uint8_t command, uint8_t value);
uint8_t my2k_main_debounce(bool pressed);
void my2k_main_settings_bind(void *context);
