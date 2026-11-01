/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_wire.h"
#include <stdbool.h>
bool my2k_oled_hid(uint8_t *, unsigned);
void my2k_oled_main_init(void);
void my2k_oled_main_service(uint32_t);
void my2k_oled_main_receive(const my2k_wire_message *);

bool my2k_oled_main_busy(void);
bool my2k_oled_main_ready(void);

bool my2k_oled_export_owned(void);
