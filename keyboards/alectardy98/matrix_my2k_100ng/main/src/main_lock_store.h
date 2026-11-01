/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "main_lock_rules.h"
/* Existing settings sector4 only. Seven legacy EEPROM slots are untouched. */
#define MY2K_LOCK_FLASH_BASE UINT32_C(0x0801e08c)
#define MY2K_LOCK_FLASH_END UINT32_C(0x08020000)
void     my2k_lock_store_init(void);
bool     my2k_lock_store_load(uint8_t *);
bool     my2k_lock_store_request(const uint8_t *);
void     my2k_lock_store_task(void);
void     my2k_lock_store_erased(void);
bool     my2k_lock_store_pending(void);
uint32_t my2k_lock_store_error(void);
uint16_t my2k_lock_store_remaining(void);
uint32_t my2k_lock_store_commits(void);
