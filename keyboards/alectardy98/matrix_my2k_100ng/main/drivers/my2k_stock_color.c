/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_stock_color.h"
#include "led_tables.h"
#ifndef USE_CIE1931_CURVE
#    error "Stock color conversion requires QMK's USE_CIE1931_CURVE and quantum/led_tables.c"
#endif

/* Independently expressed integer conversion. The curve comes from QMK's GPL
 * led_tables.c, not a copied manufacturer table. All 256 bytes were compared
 * with the newest manufacturer's firmware and the conversion is checked
 * against original ARM instructions by the offline stock-color tests. */
my2k_rgb_color my2k_stock_hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v) {
    const uint8_t value = CIE1931_CURVE[v];
    if (!s) return (my2k_rgb_color){value, value, value};
    const unsigned f = (h % 43u) * 6u;
    const uint8_t p = ((unsigned)value * (255u - s)) >> 8;
    const uint8_t q = ((unsigned)value * (255u - (((unsigned)s * f) >> 8))) >> 8;
    const uint8_t t = ((unsigned)value * (255u - (((unsigned)s * (255u - f)) >> 8))) >> 8;
    switch (h / 43u) {
        case 0: return (my2k_rgb_color){value, t, p};
        case 1: return (my2k_rgb_color){q, value, p};
        case 2: return (my2k_rgb_color){p, value, t};
        case 3: return (my2k_rgb_color){p, q, value};
        case 4: return (my2k_rgb_color){t, p, value};
        default: return (my2k_rgb_color){value, p, q};
    }
}

my2k_rgb_color my2k_stock_rgb_to_hsv(my2k_rgb_color rgb) {
    uint8_t maximum = rgb.r, minimum = rgb.r;
    if (rgb.g > maximum) maximum = rgb.g;
    if (rgb.b > maximum) maximum = rgb.b;
    if (rgb.g < minimum) minimum = rgb.g;
    if (rgb.b < minimum) minimum = rgb.b;
    if (!maximum) return (my2k_rgb_color){0, 0, 0};
    const unsigned delta = maximum - minimum;
    const uint8_t saturation = delta * 255u / maximum;
    int hue = 0;
    if (saturation) {
        if (rgb.r == maximum) hue = ((int)rgb.g - rgb.b) * 43 / (int)delta;
        else if (rgb.g == maximum) hue = ((int)rgb.b - rgb.r) * 43 / (int)delta + 85;
        else hue = ((int)rgb.r - rgb.g) * 43 / (int)delta - 85;
    }
    return (my2k_rgb_color){(uint8_t)hue, saturation, maximum};
}
