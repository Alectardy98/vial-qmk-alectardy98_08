/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    void *context;
    bool (*media)(void *, uint32_t *sector_count, uint32_t *generation);
    bool (*read_sector)(void *, uint32_t lba, uint8_t data[512]);
#if MY2K_STOCK_PARITY_PROFILE
    bool (*write_sector)(void *, uint32_t lba, const uint8_t data[512]);
    bool (*sync)(void *);
#endif
} my2k_main_fs_backend;
/* Singleton FatFs service. ALL APIs run on one storage worker, never matrix/HID
 * interrupt paths. Backend is synchronous. Legacy profiles remain read-only;
 * stock-parity writes require both real write_sector and sync callbacks. */
void my2k_main_fs_init(const my2k_main_fs_backend *);
void my2k_main_fs_reset(void);
bool my2k_main_fs_process(uint8_t report[32], size_t size);

/* Cached handle state only; query when the single worker is idle. */
bool my2k_main_fs_has_open_writers(void);
