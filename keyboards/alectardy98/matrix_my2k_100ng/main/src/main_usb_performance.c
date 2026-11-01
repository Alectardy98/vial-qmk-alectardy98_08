/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_usb_performance.h"
#include <string.h>
bool my2k_input_usb_configuration(const uint8_t *base, size_t size, bool high, bool other, uint8_t keyboard_ep,
                                  uint8_t shared_ep, uint8_t *out, size_t cap, size_t *length) {
    if (length)
        *length = 0;
    if (!base || !out || !length || size < 9 || size > cap || base[0] != 9 || base[1] != 2 ||
        ((unsigned)base[2] | (unsigned)base[3] << 8) != size)
        return false;
    size_t p   = 0;
    bool   hid = false;
    while (p < size) {
        unsigned n = base[p];
        if (n < 2 || n > size - p)
            return false;
        if (base[p + 1] == 4) {
            if (n != 9)
                return false;
            hid = base[p + 5] == 3;
        }
        if (base[p + 1] == 5) {
            if (n != 7)
                return false;
            unsigned kind = base[p + 3] & 3, bytes = (unsigned)base[p + 4] | (unsigned)base[p + 5] << 8;
            if (high && kind == 2)
                return false; /* Never advertise HS bulk64. */
            if (hid && (kind != 3 || !bytes || bytes > 64 || !base[p + 6]))
                return false;
        }
        p += n;
    }
    memcpy(out, base, size);
    out[1] = other ? 7 : 2;
    hid    = false;
    for (p = 0; p < size; p += out[p]) {
        if (out[p + 1] == 4)
            hid = out[p + 5] == 3;
        if (out[p + 1] == 5 && hid) {
            uint8_t ep   = out[p + 2];
            bool    keys = ep == (uint8_t)(0x80 | keyboard_ep) || ep == (uint8_t)(0x80 | shared_ep);
            out[p + 6]   = high ? (keys ? 1 : 4) : 1;
        }
    }
    *length = size;
    return true;
}
