/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_usb_descriptor.h"
#include <string.h>
bool my2k_usb_append_msc(const uint8_t *b, size_t n, uint8_t maxep, uint8_t *out, size_t cap, size_t *length, uint8_t *iface, uint8_t *ep) {
    if (!length) return false;
    *length = 0;
    if (!b || !out || !iface || !ep || n < 9 || n > 65535 - 23 || n + 23 > cap || b[0] != 9 || b[1] != 2 || ((size_t)b[2] | (size_t)b[3] << 8) != n || b[4] == 255) return false;
    uint8_t max = 0, count = 0;
    bool    interfaces[256] = {false};
    for (size_t offset = 9; offset < n;) {
        if (n - offset < 2 || b[offset] < 2 || b[offset] > n - offset) return false;
        if (b[offset + 1] == 4) {
            if (b[offset] < 9 || b[offset + 2] >= b[4]) return false;
            if (!interfaces[b[offset + 2]]) {
                interfaces[b[offset + 2]] = true;
                count++;
            }
        }
        if (b[offset + 1] == 5) {
            if (b[offset] < 7 || (b[offset + 2] & 0x70)) return false;
            uint8_t e = b[offset + 2] & 15;
            if (!e) return false;
            if (e > max) max = e;
        }
        offset += b[offset];
    }
    if (count != b[4] || max >= maxep || max >= 15) return false;
    *iface = b[4];
    *ep    = max + 1;
    memcpy(out, b, n);
    out[2] = (uint8_t)(n + 23);
    out[3] = (uint8_t)((n + 23) >> 8);
    out[4]++;
    uint8_t extra[23] = {9, 4, *iface, 0, 2, 8, 6, 0x50, 0, 7, 5, (uint8_t)(0x80 | *ep), 2, 64, 0, 0, 7, 5, *ep, 2, 64, 0, 0};
    memcpy(out + n, extra, sizeof extra);
    *length = n + sizeof extra;
    return true;
}
