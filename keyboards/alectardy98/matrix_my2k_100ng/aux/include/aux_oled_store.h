/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "my2k_oled_config.h"
bool            aux_oled_store_load(uint8_t[MY2K_OLED_BYTES]);
bool            aux_oled_store_save(const uint8_t[MY2K_OLED_BYTES]);
bool            aux_oled_store_writing(void);
bool            aux_oled_store_valid(void);
extern uint32_t aux_oled_store_error, aux_oled_store_saves;
