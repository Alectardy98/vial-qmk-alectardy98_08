/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_JOURNAL_H
#define MY2K_JOURNAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Storage-independent append journal. No erase operation or physical address is
 * supplied. Backend program must reject forbidden 0->1 transitions. Main does
 * not install a physical backend until Geehy erase/program rules are audited. */
typedef struct {
    void    *context;
    uint32_t bytes;
    bool (*read)(void *, uint32_t offset, void *, size_t);
    bool (*program)(void *, uint32_t offset, const void *, size_t);
} my2k_journal_backend;
typedef struct {
    my2k_journal_backend backend;
    uint32_t             slot_bytes, slots, max_payload;
} my2k_journal;
typedef enum {
    MY2K_JOURNAL_OK,
    MY2K_JOURNAL_EMPTY,
    MY2K_JOURNAL_FULL,
    MY2K_JOURNAL_INVALID,
    MY2K_JOURNAL_IO
} my2k_journal_result;
bool                my2k_journal_init(my2k_journal *, my2k_journal_backend, uint32_t max_payload);
my2k_journal_result my2k_journal_load(const my2k_journal *, void *, size_t capacity, size_t *length);
my2k_journal_result my2k_journal_save(const my2k_journal *, const void *, size_t length);
/* Explicit reclamation only. Erase units contain an integral number of slots;
 * physical adapters must map any sector padding, not erase adjacent regions.
 * The newest valid record's unit is never erased. */
typedef bool (*my2k_journal_erase_unit)(void *context, uint32_t unit);
my2k_journal_result my2k_journal_rotate(const my2k_journal *, uint32_t slots_per_unit, my2k_journal_erase_unit,
                                        void *context);

#endif
