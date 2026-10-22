// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
enum { CP_EARLY=1, CP_HAL_RETURN=2, CP_WL_ENTER=3, CP_BACKEND_ENTER=4,
 CP_GEOMETRY_OK=5, CP_PHYSICAL_ENTER=6, CP_PHYSICAL_RETURN=7, CP_READING=8,
 CP_WL_RETURN=9, CP_PRE_USB=10, CP_POST_INIT=11, CP_USB_ACTIVE=12 };
typedef struct {
 uint32_t magic, phase, milestones, error, wl_status, descriptor, flash_size,
 sectors, sector2_offset, sector2_size, physical_reads, last_read,
 program_ops, erase_ops, blocked_ops, flash_errors, backend_ok;
 uint32_t blank_checked, non_ff_bytes, first_non_ff, backend_offset, backend_sector_count, unlock_ops, option_bits, last_flash_status;
} diag_record_t;
extern volatile diag_record_t efl_diag_record;
void diag_mark(uint32_t phase);
bool diag_inspect_geometry(void);





extern bool s2_armed;
void s2_storage_init(void);
uint16_t s2_factory_kib(void);

uint32_t s2_option_bits(void);

bool ec_storage_ready(void);
