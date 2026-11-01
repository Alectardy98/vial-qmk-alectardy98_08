/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_nv32.h"
#include <string.h>

#define BANK_MAGIC UINT32_C(0x3242564d)
#define RECORD_MAGIC UINT32_C(0x3252564d)
#define COMMIT UINT32_C(0x54494d43)
enum { BEGIN = 1, PAGE = 2, END = 3, HEADER = 4 };
static uint32_t get(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t crc(const void *data, size_t n) {
    const uint8_t *p = data;
    uint32_t       c = ~0u;
    while (n--) {
        c ^= *p++;
        for (unsigned i = 0; i < 8; i++)
            c = (c >> 1) ^ ((0u - (c & 1u)) & UINT32_C(0xedb88320));
    }
    return ~c;
}
static uint32_t bank_base(int bank) {
    return MY2K_NV_BASE + (uint32_t)bank * MY2K_NV_BANK_BYTES;
}
static bool erased(const uint8_t *p, size_t n) {
    while (n--)
        if (*p++ != 255)
            return false;
    return true;
}
static uint32_t fold(uint32_t d, uint32_t c) {
    return (d << 5 | d >> 27) ^ c;
}
static bool rd(my2k_nv32 *s, uint32_t a, void *p, size_t n) {
    if (a < MY2K_NV_BASE || a > MY2K_NV_BASE + 2 * MY2K_NV_BANK_BYTES ||
        n > MY2K_NV_BASE + 2 * MY2K_NV_BANK_BYTES - a || !s->io.read(a, p, n)) {
        s->error = 1;
        return false;
    }
    return true;
}
static bool header(my2k_nv32 *s, int b, uint32_t *g) {
    if (!rd(s, bank_base(b), s->record, 64))
        return false;
    uint8_t *p = s->record;
    if (get(p) != BANK_MAGIC || get(p + 60) != crc(p, 60))
        return false;
    if (get(p) == BANK_MAGIC && (get(p + 4) != 1 || get(p + 8) != MY2K_NV_BYTES || get(p + 12) != MY2K_NV_PAGE)) {
        s->error = 6;
        return false;
    }
    if (get(p) != BANK_MAGIC || get(p + 4) != 1 || get(p + 8) != MY2K_NV_BYTES || get(p + 12) != MY2K_NV_PAGE ||
        get(p + 60) != crc(p, 60))
        return false;
    for (unsigned i = 20; i < 60; i++)
        if (p[i])
            return false;
    *g = get(p + 16);
    return true;
}
static bool scan(my2k_nv32 *s, int b) {
    memset(s->map, 0, sizeof s->map);
    memset(s->durable, 0, sizeof s->durable);
    bool     valid = false, in_tx = false;
    uint32_t id = 0, digest = 0, count = 0;
    unsigned cursor = 0;
    for (unsigned k = 0; k < MY2K_NV_SLOTS; k++) {
        uint32_t a = bank_base(b) + 64 + k * MY2K_NV_RECORD;
        if (!rd(s, a, s->record, MY2K_NV_RECORD))
            return false;
        uint8_t *p = s->record;
        if (erased(p, MY2K_NV_RECORD))
            continue;
        cursor = k + 1;
        if (get(p) != RECORD_MAGIC || get(p + 276) != COMMIT || get(p + 272) != crc(p, 272)) {
            in_tx = false;
            continue;
        }
        uint32_t kind = get(p + 8), index = get(p + 12), tid = get(p + 4);
        if (kind == BEGIN) {
            in_tx = tid == k + 1 && index == 0 && p[16] <= 1 && (valid || p[16] == 1);
            for (unsigned j = 17; j < 272; j++)
                if (p[j])
                    in_tx = false;
            if (!in_tx)
                continue;
            if (p[16])
                memset(s->map, 0, sizeof s->map);
            else
                memcpy(s->map, s->durable, sizeof s->map);
            id     = tid;
            digest = 0;
            count  = 0;
        } else if (in_tx && tid == id && kind == PAGE && index < MY2K_NV_PAGES) {
            s->map[index] = a + 16;
            digest        = fold(digest, get(p + 272));
            count++;
        } else if (in_tx && tid == id && kind == END && index == 0 && get(p + 16) == count && get(p + 20) == digest) {
            bool canonical = true;
            for (unsigned j = 24; j < 272; j++)
                if (p[j])
                    canonical = false;
            if (canonical) {
                memcpy(s->durable, s->map, sizeof s->map);
                valid = true;
            }
            in_tx = false;
        } else
            in_tx = false;
    }
    memcpy(s->map, s->durable, sizeof s->map);
    s->slot       = (uint16_t)cursor;
    s->checkpoint = valid;
    return valid;
}
bool my2k_nv_init(my2k_nv32 *s, my2k_nv_io io) {
    if (!s || !io.read || !io.word || !io.erase)
        return false;
    memset(s, 0, sizeof *s);
    s->io          = io;
    s->bank        = -1;
    s->cache_index = -1;
    uint32_t a = 0, b = 0;
    bool     av = header(s, 0, &a), bv = header(s, 1, &b);
    if (s->error)
        return false;
    int first = bv && (!av || (b != a && (uint32_t)(b - a) < UINT32_C(0x80000000))) ? 1 : 0;
    for (unsigned i = 0; i < 2; i++) {
        int bank = first ^ (int)i;
        if ((bank && !bv) || (!bank && !av))
            continue;
        if (scan(s, bank)) {
            s->bank       = (int8_t)bank;
            s->generation = bank ? b : a;
            return true;
        }
        if (s->error)
            return false;
    }
    memset(s->map, 0, sizeof s->map);
    memset(s->durable, 0, sizeof s->durable);
    s->slot  = 0;
    s->reset = true;
    return true;
}
static int cached(my2k_nv32 *s, unsigned page) {
    for (unsigned i = 0; i < MY2K_NV_CACHE; i++)
        if (s->cache[i].valid && s->cache[i].page == page)
            return (int)i;
    return -1;
}
static bool page_read(my2k_nv32 *s, unsigned page, void *out) {
    int c = cached(s, page);
    if (c >= 0) {
        memcpy(out, s->cache[c].data, 256);
        return true;
    }
    if (!s->map[page]) {
        memset(out, 0, 256);
        return true;
    }
    return rd(s, s->map[page], out, 256);
}
bool my2k_nv_read(my2k_nv32 *s, uint32_t at, void *out, size_t n) {
    if (!s || (!out && n) || at > MY2K_NV_BYTES || n > MY2K_NV_BYTES - at)
        return false;
    uint8_t *p = out;
    while (n) {
        unsigned page = at / 256, off = at % 256;
        size_t   k = 256 - off;
        if (k > n)
            k = n;
        int c = cached(s, page);
        if (c >= 0)
            memcpy(p, s->cache[c].data + off, k);
        else if (!s->map[page])
            memset(p, 0, k);
        else if (!rd(s, s->map[page] + off, p, k))
            return false;
        n -= k;
        p += k;
        at += (uint32_t)k;
    }
    return true;
}
static bool dirty(const my2k_nv32 *s) {
    for (unsigned i = 0; i < MY2K_NV_CACHE; i++)
        if (s->cache[i].valid && s->cache[i].dirty)
            return true;
    return false;
}
bool my2k_nv_pending(const my2k_nv32 *s) {
    return s && (s->action || s->tx || s->reset || s->rotating || dirty(s));
}
static void record_start(my2k_nv32 *s, unsigned kind, unsigned page, const uint8_t *data) {
    memset(s->record, 0, sizeof s->record);
    put(s->record, RECORD_MAGIC);
    put(s->record + 4, s->txid);
    put(s->record + 8, kind);
    put(s->record + 12, page);
    if (data)
        memcpy(s->record + 16, data, 256);
    put(s->record + 272, crc(s->record, 272));
    put(s->record + 276, COMMIT);
    s->record_address = bank_base(s->bank) + 64 + s->slot * MY2K_NV_RECORD;
    s->slot++;
    s->record_cursor = 0;
    s->action        = (uint8_t)kind;
}
static bool start_rotation(my2k_nv32 *s) {
    int next = s->bank < 0 ? 0 : s->bank ^ 1;
    if (!s->io.erase(bank_base(next))) {
        s->error = 3;
        return false;
    }
    s->erases++;
    s->bank = (int8_t)next;
    s->generation++;
    s->slot          = 0;
    s->rotating      = true;
    s->rotate_page   = 0;
    s->tx            = false;
    s->header_needed = false;
    memset(s->record, 0, 64);
    put(s->record, BANK_MAGIC);
    put(s->record + 4, 1);
    put(s->record + 8, MY2K_NV_BYTES);
    put(s->record + 12, 256);
    put(s->record + 16, s->generation);
    put(s->record + 60, crc(s->record, 60));
    s->record_address = bank_base(next);
    s->record_cursor  = 0;
    s->action         = HEADER;
    return true;
}
static bool schedule(my2k_nv32 *s) {
    if (s->bank < 0 || (!s->rotating && (unsigned)s->slot + 3u > MY2K_NV_SLOTS))
        return start_rotation(s);
    if (!s->tx) {
        if (!s->rotating && !s->reset && !dirty(s))
            return true;
        uint8_t begin[256] = {0};
        begin[0]           = s->rotating || s->reset;
        s->reset           = false;
        s->txid            = s->slot + 1;
        s->digest = s->count = 0;
        s->tx                = true;
        record_start(s, BEGIN, 0, begin);
        return true;
    }
    if (s->rotating) {
        /* One logical page per pass, even when it contains only zeroes. */
        if (s->rotate_page < MY2K_NV_PAGES) {
            unsigned page = s->rotate_page++;
            uint8_t  data[256];
            if (!page_read(s, page, data))
                return false;
            bool zero = true;
            for (unsigned j = 0; j < 256; j++)
                if (data[j])
                    zero = false;
            int c = cached(s, page);
            if (zero) {
                s->map[page] = 0;
                if (c >= 0)
                    s->cache[c].dirty = false;
                return true;
            }
            s->page         = (uint16_t)page;
            s->cache_index  = (int8_t)c;
            s->page_version = c >= 0 ? s->cache[c].version : 0;
            record_start(s, PAGE, page, data);
            return true;
        }
    } else {
        for (unsigned i = 0; i < MY2K_NV_CACHE; i++)
            if (s->cache[i].valid && s->cache[i].dirty) {
                s->page         = s->cache[i].page;
                s->cache_index  = (int8_t)i;
                s->page_version = s->cache[i].version;
                record_start(s, PAGE, s->page, s->cache[i].data);
                return true;
            }
    }
    uint8_t end[256] = {0};
    put(end, s->count);
    put(end + 4, s->digest);
    record_start(s, END, 0, end);
    return true;
}
bool my2k_nv_service(my2k_nv32 *s, unsigned budget) {
    if (!s || s->error)
        return false;
    if (!s->action) {
        if (!my2k_nv_pending(s))
            return true;
        if (!schedule(s))
            return false;
    }
    if (!s->action)
        return true;
    unsigned bytes = s->action == HEADER ? 64 : MY2K_NV_RECORD;
    while (budget-- && s->record_cursor < bytes) {
        uint32_t a = s->record_address + s->record_cursor, v = get(s->record + s->record_cursor), check;
        if (!s->io.word(a, v)) {
            s->error = 2;
            return false;
        }
        s->words++;
        if (!rd(s, a, &check, 4) || check != v) {
            s->error = 4;
            return false;
        }
        s->record_cursor += 4;
    }
    if (s->record_cursor != bytes)
        return true;
    uint8_t action = s->action;
    s->action      = 0;
    if (action == PAGE) {
        s->map[s->page] = s->record_address + 16;
        s->digest       = fold(s->digest, get(s->record + 272));
        s->count++;
        int c = s->cache_index;
        if (c >= 0 && s->cache[c].valid && s->cache[c].page == s->page && s->cache[c].version == s->page_version)
            s->cache[c].dirty = false;
    } else if (action == END) {
        memcpy(s->durable, s->map, sizeof s->map);
        s->tx         = false;
        s->rotating   = false;
        s->checkpoint = true;
        s->commits++;
    }
    return true;
}
bool my2k_nv_flush(my2k_nv32 *s) {
    for (unsigned i = 0; i < 40000; i++) {
        if (s->error)
            return false;
        if (!my2k_nv_pending(s))
            return true;
        if (!my2k_nv_service(s, 16))
            return false;
    }
    s->error = 5;
    return false;
}
bool my2k_nv_write(my2k_nv32 *s, uint32_t at, const void *in, size_t n) {
    if (!s || s->error || (!in && n) || at > MY2K_NV_BYTES || n > MY2K_NV_BYTES - at)
        return false;
    /* Rotation sources must stay stable. Only configuration writes can enter
     * this bounded slow path; keyboard input never writes EEPROM. */
    if (s->rotating && !my2k_nv_flush(s))
        return false;
    const uint8_t *p = in;
    while (n) {
        unsigned page = at / 256, off = at % 256;
        size_t   k = 256 - off;
        if (k > n)
            k = n;
        int c = cached(s, page);
        if (c < 0) {
            for (unsigned i = 0; i < MY2K_NV_CACHE; i++)
                if (!s->cache[i].valid || (!s->cache[i].dirty && !(s->action == PAGE && s->cache_index == (int)i))) {
                    c = (int)i;
                    break;
                }
            if (c < 0) {
                /* Cache pressure is rare outside bulk configuration/default reset. A
                 * bounded durable flush avoids silently dropping accepted VIA writes. */
                s->evictions++;
                if (!my2k_nv_flush(s))
                    return false;
                c = 0;
            }
            if (!page_read(s, page, s->cache[c].data))
                return false;
            s->cache[c].page    = (uint16_t)page;
            s->cache[c].valid   = true;
            s->cache[c].dirty   = false;
            s->cache[c].version = 0;
        }
        if (memcmp(s->cache[c].data + off, p, k)) {
            memcpy(s->cache[c].data + off, p, k);
            s->cache[c].dirty = true;
            s->cache[c].version++;
        }
        p += k;
        n -= k;
        at += (uint32_t)k;
    }
    return true;
}
bool my2k_nv_clear(my2k_nv32 *s) {
    if (!s || s->error)
        return false;
    /* Complete only an in-flight record, then supersede any uncommitted batch
     * with an explicit reset transaction. Old durable state remains recoverable. */
    for (unsigned i = 0; s->action && i < 128; i++)
        if (!my2k_nv_service(s, 16))
            return false;
    if (s->action) {
        s->error = 5;
        return false;
    }
    s->tx       = false;
    s->rotating = false;
    s->reset    = true;
    memset(s->map, 0, sizeof s->map);
    memset(s->cache, 0, sizeof s->cache);
    return true;
}
