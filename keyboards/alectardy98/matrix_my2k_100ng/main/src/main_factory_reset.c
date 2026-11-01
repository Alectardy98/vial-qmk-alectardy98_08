/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_factory_reset.h"
#include "main_eeprom.h"
#include <string.h>
void clear_keyboard(void);
/* Deliberately separate from ordinary VIA keymap reset: full Main reset uses
 * exactly the same durable-before-restart lifecycle as the physical EE_CLR. */
__attribute__((noinline, noclone, externally_visible)) bool my2k_factory_reset_hid(uint8_t *p, uint8_t n,
                                                                                   bool allowed) {
    if (!p || n != 32 || p[0] != 253 || p[1] != 236)
        return false;
    uint8_t q[32];
    memcpy(q, p, 32);
    memset(p, 0, 32);
    p[0] = 253;
    p[1] = 236;
    p[2] = 85;
    p[3] = 1;
    p[4] = q[2];
    p[5] = q[4];
    if (q[3] != 1 || q[2] > 1) {
        p[6] = 1;
        return true;
    }
    if (!q[2]) {
        memcpy(p + 6, "CLR1", 4);
        p[10] = 32;
        p[11] = 7;
        p[12] = allowed && my2k_eeprom_durable();
        p[2]  = 170;
        return true;
    }
    if (!allowed) {
        p[6] = 2;
        return true;
    }
    if (!q[4] || memcmp(q + 5, "CLR1MAIN", 8)) {
        p[6] = 3;
        return true;
    }
    for (unsigned i = 13; i < 32; i++)
        if (q[i]) {
            p[6] = 3;
            return true;
        }
    if (!my2k_eeprom32_reset_request()) {
        p[6] = 4;
        return true;
    }
    clear_keyboard();
    p[2] = 170;
    p[6] = 1;
    return true;
}
