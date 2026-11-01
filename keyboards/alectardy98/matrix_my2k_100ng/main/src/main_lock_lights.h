/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_amk_rgb_runtime.h"
void my2k_lock_init(bool preserve_legacy);
bool my2k_lock_hid(uint8_t *,uint8_t length,uint32_t now,bool allow_writes);
void my2k_lock_service(uint32_t now,bool allow_flash);
bool my2k_lock_frame(my2k_amk_rgb_runtime *,uint32_t now,uint8_t locks,uint32_t layers);
