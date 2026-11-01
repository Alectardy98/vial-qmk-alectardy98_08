/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
typedef struct {uint32_t rom_bytes,ram_bytes;uint8_t type,mbc,color,battery,rtc;} my2k_gbc_rom_info;
int my2k_gbc_rom_parse(const uint8_t header[336],uint32_t bytes,my2k_gbc_rom_info *info);
