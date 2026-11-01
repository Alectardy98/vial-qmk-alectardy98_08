/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "my2k_rgb.h"
/* GUI-visible stock ordinals: strips0..17, grid0..15. Stock current firmware
 * reports stripcustom26 although its JSON/GUI select17;26 is an explicit alias.
 * Independently authored named effects, NOT extracted manufacturer code or a
 * claim of bit/timing-identical mathematical output. Hardware current/brightness
 * limiting and per-LED custom-flag evaluation remain with the caller. */
#define MY2K_STOCK_STRIP_TYPE 1u
#define MY2K_STOCK_GRID_TYPE 3u
#define MY2K_STOCK_STRIP_EFFECT_COUNT 18u
#define MY2K_STOCK_GRID_EFFECT_COUNT 16u
#define MY2K_STOCK_STRIP_CUSTOM_ALIAS 26u

typedef struct {
    uint8_t type, mode, speed;
    uint16_t count, rows, cols;
    uint32_t now_ms;
    my2k_rgb_color base;
} my2k_stock_effect;
bool my2k_stock_effect_valid(uint8_t type,uint8_t mode);
uint8_t my2k_stock_effect_custom(uint8_t type);
uint16_t my2k_stock_effect_period(uint8_t speed);
my2k_rgb_color my2k_stock_effect_render(const my2k_stock_effect *effect,uint16_t index,my2k_rgb_color custom_color);
