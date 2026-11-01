/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_oled_config.h"
#include <string.h>
uint32_t my2k_oled_crc(const uint8_t *p, size_t n) {
    uint32_t c = ~0u;
    while (n--) {
        c ^= *p++;
        for (unsigned i = 0; i < 8; i++)
            c = (c >> 1) ^ ((0u - (c & 1u)) & 0xedb88320u);
    }
    return ~c;
}
void my2k_oled_seal(uint8_t p[MY2K_OLED_BYTES]) {
    uint32_t c = my2k_oled_crc(p, MY2K_OLED_CRC_OFFSET);
    for (unsigned i = 0; i < 4; i++)
        p[MY2K_OLED_CRC_OFFSET + i] = (uint8_t)(c >> (i * 8));
}
bool my2k_oled_valid(const uint8_t p[MY2K_OLED_BYTES]) {
    if (!p || p[0] != 1 || (p[1] & ~15u) || ((p[1] & MY2K_OLED_HAPPY) && (p[1] & MY2K_OLED_CLOCK)) || p[2] > 59 || p[3] > 10 || p[74] < 8 || p[74] > 38 || p[75] > 40) return false;
    uint32_t c = my2k_oled_crc(p, MY2K_OLED_CRC_OFFSET);
    for (unsigned i = 0; i < 4; i++)
        if (p[MY2K_OLED_CRC_OFFSET + i] != (uint8_t)(c >> (i * 8))) return false;
    for (unsigned i = 0; i < p[2]; i++)
        if (p[4 + i] != '\n' && (p[4 + i] < 32 || p[4 + i] > 126)) return false;
    for (unsigned i = 0; i < p[75]; i++)
        if (p[76 + i] < 32 || p[76 + i] > 126) return false;
    for (unsigned i = p[75]; i < 40; i++)
        if (p[76 + i]) return false;
    for (unsigned i = p[2]; i < 60; i++)
        if (p[4 + i]) return false;
    for (unsigned i = 0; i < p[3]; i++)
        if (p[64 + i] < 32 || p[64 + i] > 126) return false;
    for (unsigned i = p[3]; i < 10; i++)
        if (p[64 + i]) return false;
    return true;
}
void my2k_oled_defaults(uint8_t p[MY2K_OLED_BYTES]) {
    memset(p, 0, MY2K_OLED_BYTES);
    p[0] = 1;
    p[1] = MY2K_OLED_TYPING;
    p[2] = 12;
    p[3] = 4;
    memcpy(p + 4, "Hello\nMatrix", 12);
    memcpy(p + 64, "MY2K", 4);
    p[74] = 8;
    p[75] = 15;
    memcpy(p + 76, "DejaVu Sans Mono", 15);
    my2k_oled_seal(p);
}
