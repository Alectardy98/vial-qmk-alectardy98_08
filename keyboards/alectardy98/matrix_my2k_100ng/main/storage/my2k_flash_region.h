/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_journal.h"
/* Explicitly reviewed upper-flash allocation only. This is a guarded address
 * mapper, not a register driver: no physical implementation is installed. */
typedef struct {
    void *context;
    bool (*read)(void *, uint32_t address, void *, size_t);
    bool (*program_word)(void *, uint32_t address, const uint8_t word[4]);
    bool (*erase_sector)(void *, uint8_t sector);
} my2k_flash_io;
typedef struct {
    my2k_flash_io io;
    uint8_t       first_sector, sector_count;
    uint32_t      slots_per_sector, slot_bytes, virtual_bytes;
} my2k_flash_region;
/* reviewed_allocation must remain false until recovery, readback and ownership
 * gates approve these EXACT sectors. Sector geometry still needs errata audit. */
bool my2k_flash_region_init(my2k_flash_region *, my2k_flash_io, uint8_t first_sector, uint8_t sector_count, uint32_t payload, bool reviewed_allocation, my2k_journal *);
bool my2k_flash_region_erase(void *region, uint32_t virtual_unit);
