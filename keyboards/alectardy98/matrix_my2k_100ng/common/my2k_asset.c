/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_asset.h"
static uint16_t le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8);
}
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static bool read(const my2k_reader *r, uint32_t offset, void *dst, size_t n) {
    return r && r->read_at && dst && offset <= r->size && n <= r->size - offset &&
           r->read_at(r->context, offset, dst, n);
}
bool my2k_asset_open(const my2k_reader *r, my2k_asset *out) {
    uint8_t h[20];
    if (!out || !read(r, 0, h, sizeof h))
        return false;
    my2k_asset a;
    a.abit_size_quirk = false;
    a.reader          = *r;
    if (h[0] == 'A' && h[1] == 'N' && h[2] == 'I' && h[3] == 'M')
        a.kind = MY2K_ASSET_ANIM;
    else if (h[0] == 'A' && h[1] == 'B' && h[2] == 'I' && h[3] == 'T')
        a.kind = MY2K_ASSET_ABIT;
    else
        return false;
    a.width  = le16(h + 12);
    a.height = le16(h + 14);
    a.frames = le16(h + 18);
    a.offset = le16(h + 6);
    if (le16(h + 4) != 20 || le16(h + 16) != 2 || !a.width || a.width > 512 || !a.height || a.height > 512 ||
        !a.frames || a.frames > 512 || a.offset < 20u + 2u * a.frames)
        return false;
    uint32_t pixels = (uint32_t)a.width * a.height;
    a.frame_bytes   = a.kind == MY2K_ASSET_ANIM ? pixels * 2u : (pixels + 7u) / 8u;
    if (a.kind == MY2K_ASSET_ABIT && a.frames != 1)
        return false;
    /* Explicit division guards multiplication/address overflow before calculating lengths. */
    if (a.frame_bytes > (UINT32_MAX - a.offset) / a.frames)
        return false;
    uint32_t actual = a.offset + a.frame_bytes * a.frames;
    if (actual != r->size)
        return false;
    uint32_t declared = le32(h + 8);
    if (a.kind == MY2K_ASSET_ANIM) {
        if (declared != actual)
            return false;
    } else if (declared != actual) {
        if (declared != a.offset + pixels * 2u)
            return false;
        a.abit_size_quirk = true;
    }
    *out = a;
    return true;
}
bool my2k_asset_delay(const my2k_asset *a, uint16_t frame, uint16_t *ms) {
    uint8_t b[2];
    if (!a || !ms || frame >= a->frames || !read(&a->reader, 20u + 2u * frame, b, 2))
        return false;
    *ms = le16(b);
    return true;
}
bool my2k_asset_read_frame(const my2k_asset *a, uint16_t frame, uint32_t off, void *dst, size_t n) {
    if (!a || frame >= a->frames || off > a->frame_bytes || n > a->frame_bytes - off)
        return false;
    return read(&a->reader, a->offset + (uint32_t)frame * a->frame_bytes + off, dst, n);
}
bool my2k_asset_pixels(const my2k_asset *a, uint16_t frame, uint32_t first, size_t n, uint16_t *out) {
#if MY2K_ASSET_BATCH128
    uint8_t b[256];
#else
    uint8_t b[64];
#endif
    if (!a || !out || frame >= a->frames)
        return false;
    uint32_t pixels = (uint32_t)a->width * a->height;
    if (first > pixels || n > pixels - first)
        return false;
    while (n) {
#if MY2K_ASSET_BATCH128
        size_t limit = a->kind == MY2K_ASSET_ANIM ? 128 : 32;
        size_t take  = n > limit ? limit : n;
#else
        size_t take = n > 32 ? 32 : n;
#endif
        if (a->kind == MY2K_ASSET_ANIM) {
            if (!my2k_asset_read_frame(a, frame, first * 2u, b, take * 2u))
                return false;
            for (size_t i = 0; i < take; i++)
                out[i] = (uint16_t)((uint16_t)b[i * 2] << 8 | b[i * 2 + 1]);
        } else {
            unsigned shift = first & 7u;
            size_t   bytes = (shift + take + 7u) / 8u;
            if (!my2k_asset_read_frame(a, frame, first / 8u, b, bytes))
                return false;
            for (size_t i = 0; i < take; i++)
                out[i] = (b[(shift + i) / 8u] & (1u << ((shift + i) & 7u))) ? UINT16_MAX : 0;
        }
        first += (uint32_t)take;
        out += take;
        n -= take;
    }
    return true;
}
bool my2k_asset_frame_interval(const my2k_asset *a, uint16_t frame, uint16_t *ms) {
    if (!a || !ms || frame >= a->frames)
        return false;
    if (a->frames == 1) {
        *ms = 1000;
        return true;
    }
    return my2k_asset_delay(a, frame, ms);
}
bool my2k_asset_due(uint32_t now, uint32_t last, uint16_t ms) {
    return (uint32_t)(now - last) >= ms;
}
