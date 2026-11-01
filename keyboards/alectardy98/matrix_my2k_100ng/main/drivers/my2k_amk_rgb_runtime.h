/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_AMK_RGB_RUNTIME_H
#define MY2K_AMK_RGB_RUNTIME_H
#include "my2k_amk.h"
#include "my2k_rgb.h"
/* This independent effect ordering must match the supplied GUI definition. */
enum { MY2K_EFFECT_OFF, MY2K_EFFECT_STATIC, MY2K_EFFECT_RAINBOW, MY2K_EFFECT_BREATHING, MY2K_EFFECT_CUSTOM, MY2K_EFFECT_COUNT };
typedef struct {
    uint8_t        mode, speed;
    my2k_rgb_color color;
} my2k_rgb_group;
typedef struct {
    my2k_rgb_state          rgb;
    my2k_rgb_group          groups[8]; /* grid, then definition-order strips */
    uint8_t                brightness[8], sync[8], use_custom[8];
#if MY2K_STOCK_PARITY_PROFILE
    uint8_t                debounce_press,debounce_release,aux_mode;
#endif
    my2k_rgb_config         indicators[6];
    my2k_amk_backend        backend;
    const my2k_amk_backend *settings;
    uint32_t                last_frame, frames, configuration_revision;
    uint8_t                 host_locks;
    bool                    dirty, started,enabled;
} my2k_amk_rgb_runtime;
void my2k_amk_rgb_init(my2k_amk_rgb_runtime *, const my2k_amk_backend *settings);
/* Cached preview only: no I2C, USB or flash. 20ms frame cadence, wrap-safe.
 * Returns true only when a new complete rendered frame is committed. */
bool my2k_amk_rgb_tick(my2k_amk_rgb_runtime *, uint32_t now_ms, uint8_t host_lock_bits);
/* Native VialRGB1 global controls share this renderer and saved state. */
bool my2k_lighting_hid(my2k_amk_rgb_runtime *, uint8_t report[32]);
#endif
