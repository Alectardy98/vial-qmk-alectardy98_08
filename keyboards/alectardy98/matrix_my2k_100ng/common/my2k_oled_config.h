/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MY2K_OLED_BYTES 480u
#define MY2K_OLED_VERSION 1u
#define MY2K_OLED_TYPING 1u
#define MY2K_OLED_HAPPY 2u
#define MY2K_OLED_CLOCK 8u
#define MY2K_OLED_IDLE_OFFSET 4u
#define MY2K_OLED_LABEL_OFFSET 64u
/* Explicit wire image: schema,flags,idle_length,label_length,idle[60],label[10],
 * font_size,font_name_length,font_name[40],page_bitmap[360],CRC32[4]. */
uint32_t my2k_oled_crc(const uint8_t *, size_t);
void     my2k_oled_seal(uint8_t[MY2K_OLED_BYTES]);
bool     my2k_oled_valid(const uint8_t[MY2K_OLED_BYTES]);
void     my2k_oled_defaults(uint8_t[MY2K_OLED_BYTES]);

#define MY2K_OLED_BITMAP_OFFSET 116u
#define MY2K_OLED_CRC_OFFSET 476u
#define MY2K_OLED_BITMAP 4u
