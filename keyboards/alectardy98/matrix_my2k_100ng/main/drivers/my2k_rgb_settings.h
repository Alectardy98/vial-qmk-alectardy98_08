/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_RGB_SETTINGS_H
#define MY2K_RGB_SETTINGS_H
#include "my2k_amk_rgb_runtime.h"
#define MY2K_RGB_SETTINGS_PAYLOAD 576u
#define MY2K_RGB_SETTINGS_BYTES 592u
#define MY2K_RGB_SETTINGS_SIZE MY2K_RGB_SETTINGS_BYTES
/* Explicit LE header, byte payload, CRC32 IEEE. Never stores struct padding,
 * pointers, rendered frame, timer, scalar settings, or host lock state. */
bool my2k_rgb_settings_export(const my2k_amk_rgb_runtime *, uint8_t *data, size_t capacity);
/* Exact length required. Invalid header/CRC/mode leaves ALL runtime unchanged.
 * Success imports configuration, bumps configuration_revision and marks dirty. */
bool my2k_rgb_settings_import(my2k_amk_rgb_runtime *, const uint8_t *data, size_t size);
#endif
