/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_ASSET_H
#define MY2K_ASSET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef bool (*my2k_read_at_fn)(void *context, uint32_t offset, void *dst, size_t size);
typedef struct {
    void           *context;
    uint32_t        size;
    my2k_read_at_fn read_at;
} my2k_reader;
typedef enum { MY2K_ASSET_ANIM = 1, MY2K_ASSET_ABIT = 2 } my2k_asset_kind;
typedef struct {
    my2k_reader     reader;
    my2k_asset_kind kind;
    uint16_t        width, height, frames;
    uint32_t        offset, frame_bytes;
    bool            abit_size_quirk;
} my2k_asset;
/* Reads only. ANIM=BE RGB565; ABIT=continuous row-major LSB bits, not row padded. */
bool my2k_asset_open(const my2k_reader *reader, my2k_asset *out);
bool my2k_asset_delay(const my2k_asset *asset, uint16_t frame, uint16_t *milliseconds);
bool my2k_asset_read_frame(const my2k_asset *asset, uint16_t frame, uint32_t byte_offset, void *dst, size_t size);
bool my2k_asset_pixels(const my2k_asset *asset, uint16_t frame, uint32_t first, size_t count, uint16_t *rgb565);
/* Matches observed single-frame1000ms scheduling; otherwise encoded LE16 delay. */
bool my2k_asset_frame_interval(const my2k_asset *asset, uint16_t frame, uint16_t *milliseconds);
/* Wrap-safe elapsed check: intervals are16bit, timestamps are32bit monotonic ms. */
bool my2k_asset_due(uint32_t now, uint32_t last, uint16_t milliseconds);
#endif
