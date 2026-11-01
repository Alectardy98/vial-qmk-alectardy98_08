/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_gbc_rom.h"
#include <string.h>
int my2k_gbc_rom_parse(const uint8_t h[336], uint32_t size, my2k_gbc_rom_info *out) {
    if (!h || !out || size < 32768)
        return 2;
    uint8_t sum = 0;
    for (unsigned i = 0x134; i <= 0x14c; i++)
        sum = (uint8_t)(sum - h[i] - 1u);
    if (sum != h[0x14d])
        return 2;
    unsigned          t = h[0x147], r = h[0x149];
    my2k_gbc_rom_info v = {0};
    v.type              = (uint8_t)t;
    v.color             = !!(h[0x143] & 128u);
    switch (t) {
        case 0:
        case 8:
        case 9:
            v.mbc = 0;
            break;
        case 1:
        case 2:
        case 3:
            v.mbc = 1;
            break;
        case 5:
        case 6:
            v.mbc = 2;
            break;
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
            v.mbc = 3;
            break;
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
            v.mbc = 5;
            break;
        case 34:
            v.mbc = 7;
            break;
        default:
            return 4;
    }
    if (h[0x148] > 8 || r > 5)
        return 5;
    v.rom_bytes = 32768u << h[0x148];
    if (size != v.rom_bytes)
        return 5;
    static const uint32_t ram[6] = {0, 2048, 8192, 32768, 131072, 65536};
    v.ram_bytes                  = v.mbc == 2 ? 512u : v.mbc == 7 ? 256u : ram[r];
    if (v.ram_bytes > 65536 || (v.ram_bytes > 32768 && v.mbc != 3))
        return 9;
    if (v.mbc == 7 && r)
        return 6;
    if ((!v.mbc && size != 32768) || (v.mbc == 1 && size > 2097152u) || (v.mbc == 2 && size > 262144u) ||
        (v.mbc == 3 && size > 4194304u) || (v.mbc == 7 && size > 2097152u))
        return 6;
    if ((t == 0 || t == 1 || t == 17 || t == 25 || t == 28 || t == 15) && v.ram_bytes)
        return 6;
    v.battery = t == 3 || t == 6 || t == 9 || t == 15 || t == 16 || t == 19 || t == 27 || t == 30 || t == 34;
    v.rtc     = t == 15 || t == 16;
    *out      = v;
    return 0;
}
