/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_rgb.h"
#if MY2K_EEPROM32_PROFILE
#    define MY2K_LOCK_BYTES 128u
#else
#    define MY2K_LOCK_BYTES 76u
#endif
#define MY2K_LOCK_MAX_RULES 8u
#define MY2K_LOCK_LAYER_COUNT 4u
/* Canonical wire/storage bytes: version,count,fallback,reserved,RGB,pad;
 * eight {trigger,enabled,layer,effect,speed,R,G,B}; CRC32 over bytes0..71. */
enum { MY2K_LOCK_NONE, MY2K_LOCK_CAPS, MY2K_LOCK_NUM, MY2K_LOCK_SCROLL, MY2K_LOCK_LAYER };
enum { MY2K_LOCK_SOLID, MY2K_LOCK_BLINK, MY2K_LOCK_BREATH, MY2K_LOCK_PULSE, MY2K_LOCK_RAINBOW };
uint32_t       my2k_lock_crc(const uint8_t *, size_t);
bool           my2k_lock_validate(const uint8_t *);
void           my2k_lock_defaults(uint8_t *);
my2k_rgb_color my2k_lock_evaluate(const uint8_t *, uint8_t locks, uint32_t layers, uint32_t elapsed, uint8_t *selected);
#if MY2K_EEPROM32_PROFILE
my2k_rgb_color my2k_lock_effect(my2k_rgb_color, uint8_t effect, uint8_t speed, uint32_t elapsed);
#endif
