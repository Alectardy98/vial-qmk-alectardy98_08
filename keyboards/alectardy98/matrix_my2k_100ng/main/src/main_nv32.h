/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MY2K_NV_BYTES 32768u
#define MY2K_NV_PAGE 256u
#define MY2K_NV_PAGES (MY2K_NV_BYTES / MY2K_NV_PAGE)
#define MY2K_NV_CACHE 16u
#define MY2K_NV_BASE UINT32_C(0x080c0000)
#define MY2K_NV_BANK_BYTES UINT32_C(0x20000)
#define MY2K_NV_HEADER 64u
#define MY2K_NV_RECORD 280u
#define MY2K_NV_SLOTS ((MY2K_NV_BANK_BYTES - MY2K_NV_HEADER) / MY2K_NV_RECORD)
typedef struct {
    bool (*read)(uint32_t, void *, size_t);
    bool (*word)(uint32_t, uint32_t);
    bool (*erase)(uint32_t);
} my2k_nv_io;
typedef struct {
    uint8_t  data[MY2K_NV_PAGE];
    uint16_t page;
    bool     valid, dirty;
    uint32_t version;
} my2k_nv_cache;
typedef struct {
    my2k_nv_io    io;
    uint32_t      map[MY2K_NV_PAGES], durable[MY2K_NV_PAGES];
    my2k_nv_cache cache[MY2K_NV_CACHE];
    uint8_t       record[MY2K_NV_RECORD];
    uint32_t      generation, txid, digest, count, commits, erases, words, error, evictions;
    uint32_t      record_address, record_cursor, page_version;
    uint16_t      slot, rotate_page, page;
    int8_t        bank, cache_index;
    uint8_t       action;
    bool          tx, reset, rotating, header_needed, checkpoint;
} my2k_nv32;

bool my2k_nv_init(my2k_nv32 *, my2k_nv_io);
bool my2k_nv_read(my2k_nv32 *, uint32_t, void *, size_t);
bool my2k_nv_write(my2k_nv32 *, uint32_t, const void *, size_t);
bool my2k_nv_clear(my2k_nv32 *);
bool my2k_nv_service(my2k_nv32 *, unsigned word_budget);
bool my2k_nv_pending(const my2k_nv32 *);
/* Explicit bounded maintenance helper for cache pressure and host tests.
 * Ordinary application persistence uses service(4), not this whole flush. */
bool my2k_nv_flush(my2k_nv32 *);
