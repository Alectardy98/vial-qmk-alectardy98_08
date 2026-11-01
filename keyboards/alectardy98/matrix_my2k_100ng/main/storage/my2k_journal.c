/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_journal.h"
#include <string.h>
#define MAGIC UINT32_C(0x314a594d)
#define COMMIT UINT32_C(0x4d4f434b)
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t crc(uint32_t c, const uint8_t *p, size_t n) {
    while (n--) {
        c ^= *p++;
        for (unsigned i = 0; i < 8; i++)
            c = (c >> 1) ^ ((0u - (c & 1u)) & UINT32_C(0xedb88320));
    }
    return c;
}
bool my2k_journal_init(my2k_journal *j, my2k_journal_backend b, uint32_t max) {
    if (!j || !b.read || !b.program || !max || max > UINT32_MAX - 23u)
        return false;
    uint32_t slot = (max + 23u) & ~3u;
    if (b.bytes / slot < 2)
        return false;
    *j = (my2k_journal){b, slot, b.bytes / slot, max};
    return true;
}
static bool read_at(const my2k_journal *j, uint32_t p, void *d, size_t n) {
    return p <= j->backend.bytes && n <= j->backend.bytes - p && j->backend.read(j->backend.context, p, d, n);
}
static my2k_journal_result latest(const my2k_journal *j, uint32_t *slot, uint32_t *generation, uint32_t *length) {
    bool    found = false;
    uint8_t h[16], buf[32], end[4];
    for (uint32_t s = 0; s < j->slots; s++) {
        uint32_t base = s * j->slot_bytes;
        if (!read_at(j, base, h, 16) || !read_at(j, base + j->slot_bytes - 4, end, 4))
            return MY2K_JOURNAL_IO;
        uint32_t n = get32(h + 8);
        if (get32(h) != MAGIC || get32(end) != COMMIT || n > j->max_payload)
            continue;
        uint32_t c = crc(UINT32_MAX, h, 12);
        for (uint32_t off = 0; off < n;) {
            size_t k = n - off;
            if (k > sizeof buf)
                k = sizeof buf;
            if (!read_at(j, base + 16 + off, buf, k))
                return MY2K_JOURNAL_IO;
            c = crc(c, buf, k);
            off += (uint32_t)k;
        }
        if ((c ^ UINT32_MAX) != get32(h + 12))
            continue;
        uint32_t gen = get32(h + 4);
        if (!found || (uint32_t)(gen - *generation) < UINT32_C(0x80000000)) {
            *slot       = s;
            *generation = gen;
            *length     = n;
            found       = true;
        }
    }
    return found ? MY2K_JOURNAL_OK : MY2K_JOURNAL_EMPTY;
}
my2k_journal_result my2k_journal_load(const my2k_journal *j, void *out, size_t cap, size_t *len) {
    if (!j || !len || (!out && cap))
        return MY2K_JOURNAL_INVALID;
    *len                  = 0;
    uint32_t            s = 0, g = 0, n = 0;
    my2k_journal_result r = latest(j, &s, &g, &n);
    if (r != MY2K_JOURNAL_OK)
        return r;
    if (n > cap || (!out && n))
        return MY2K_JOURNAL_INVALID;
    if (!read_at(j, s * j->slot_bytes + 16, out, n))
        return MY2K_JOURNAL_IO;
    *len = n;
    return MY2K_JOURNAL_OK;
}
my2k_journal_result my2k_journal_save(const my2k_journal *j, const void *data, size_t n) {
    if (!j || n > j->max_payload || (!data && n))
        return MY2K_JOURNAL_INVALID;
    uint32_t            old = 0, g = 0, len = 0;
    my2k_journal_result r = latest(j, &old, &g, &len);
    if (r != MY2K_JOURNAL_OK && r != MY2K_JOURNAL_EMPTY)
        return r;
    uint8_t  b[32];
    uint32_t chosen = j->slots;
    for (uint32_t s = 0; s < j->slots; s++) {
        bool empty = true;
        for (uint32_t o = 0; o < j->slot_bytes;) {
            size_t k = j->slot_bytes - o;
            if (k > sizeof b)
                k = sizeof b;
            if (!read_at(j, s * j->slot_bytes + o, b, k))
                return MY2K_JOURNAL_IO;
            for (size_t i = 0; i < k; i++)
                if (b[i] != 255)
                    empty = false;
            o += (uint32_t)k;
        }
        if (empty) {
            chosen = s;
            break;
        }
    }
    if (chosen == j->slots)
        return MY2K_JOURNAL_FULL;
    uint8_t h[16];
    put32(h, MAGIC);
    put32(h + 4, r == MY2K_JOURNAL_EMPTY ? 0 : g + 1);
    put32(h + 8, (uint32_t)n);
    uint32_t c = crc(UINT32_MAX, h, 12);
    c          = crc(c, data, n);
    put32(h + 12, c ^ UINT32_MAX);
    uint32_t base = chosen * j->slot_bytes;
    if (!j->backend.program(j->backend.context, base, h, 16))
        return MY2K_JOURNAL_IO;
    /* Aligned 4-byte programming units; padding remains erased. */
    for (uint32_t o = 0; o < n; o += 4) {
        uint8_t word[4] = {255, 255, 255, 255};
        size_t  k       = n - o;
        if (k > 4)
            k = 4;
        memcpy(word, (const uint8_t *)data + o, k);
        if (!j->backend.program(j->backend.context, base + 16 + o, word, 4))
            return MY2K_JOURNAL_IO;
    }
    put32(b, COMMIT);
    if (!j->backend.program(j->backend.context, base + j->slot_bytes - 4, b, 4))
        return MY2K_JOURNAL_IO;
    return MY2K_JOURNAL_OK;
}
my2k_journal_result my2k_journal_rotate(const my2k_journal *j, uint32_t slots_per_unit, my2k_journal_erase_unit erase,
                                        void *context) {
    if (!j || !erase || !slots_per_unit || j->slots % slots_per_unit || j->slots / slots_per_unit < 2)
        return MY2K_JOURNAL_INVALID;
    uint32_t            slot = 0, generation = 0, length = 0;
    my2k_journal_result result = latest(j, &slot, &generation, &length);
    if (result != MY2K_JOURNAL_OK)
        return result;
    uint32_t units = j->slots / slots_per_unit;
    uint32_t next  = (slot / slots_per_unit + 1) % units;
    return erase(context, next) ? MY2K_JOURNAL_OK : MY2K_JOURNAL_IO;
}
