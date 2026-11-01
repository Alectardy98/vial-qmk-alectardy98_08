/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_rle565.h"
#include <string.h>
bool my2k_rle565_decode(const uint8_t *source, size_t bytes, uint8_t *out, size_t length) {
    if (!source || !out || !bytes || !length || (length & 1u) || length > 4096u)
        return false;
    size_t at = 0, wrote = 0;
    while (at < bytes) {
        unsigned tag = source[at++];
        size_t   n   = ((tag & 127u) + 1u) * 2u;
        if (n > length - wrote)
            return false;
        if (tag & 128u) {
            if (bytes - at < 2u)
                return false;
            uint8_t a = source[at++], b = source[at++];
            for (size_t i = 0; i < n; i += 2) {
                out[wrote + i]     = a;
                out[wrote + i + 1] = b;
            }
        } else {
            if (n > bytes - at)
                return false;
            memcpy(out + wrote, source + at, n);
            at += n;
        }
        wrote += n;
    }
    return wrote == length;
}
