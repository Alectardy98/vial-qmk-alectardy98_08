/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
void my2k_mbc7_init(uint8_t eeprom[256]);
void my2k_mbc7_buttons(uint8_t buttons);
uint8_t my2k_mbc7_read(uint16_t address);
void my2k_mbc7_write(uint16_t address,uint8_t value);
bool my2k_mbc7_dirty(void);
