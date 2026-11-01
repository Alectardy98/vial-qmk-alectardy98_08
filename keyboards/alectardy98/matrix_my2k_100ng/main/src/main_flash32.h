/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "main_nv32.h"
bool my2k_flash32_read(uint32_t,void *,size_t);
bool my2k_flash32_word(uint32_t,uint32_t);
bool my2k_flash32_erase(uint32_t);
