/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_stock_effects.h"
static my2k_rgb_color black(void) {
    return (my2k_rgb_color){0, 0, 0};
}
static my2k_rgb_color scale(my2k_rgb_color c, unsigned level) {
    if (level > 255)
        level = 255;
    return (my2k_rgb_color){(uint8_t)((uint16_t)c.r * level / 255), (uint8_t)((uint16_t)c.g * level / 255),
                            (uint8_t)((uint16_t)c.b * level / 255)};
}
static uint8_t peak(my2k_rgb_color c) {
    uint8_t v = c.r > c.g ? c.r : c.g;
    return v > c.b ? v : c.b;
}
static uint8_t hue(my2k_rgb_color c) {
    uint8_t hi = peak(c), lo = c.r < c.g ? c.r : c.g;
    if (c.b < lo)
        lo = c.b;
    int d = hi - lo, h = 0;
    if (d) {
        if (hi == c.r)
            h = 43 * ((int)c.g - c.b) / d;
        else if (hi == c.g)
            h = 85 + 43 * ((int)c.b - c.r) / d;
        else
            h = 171 + 43 * ((int)c.r - c.g) / d;
    }
    return (uint8_t)h;
}
static unsigned triangle(unsigned phase) {
    return phase < 128 ? phase * 2 : (255 - phase) * 2;
}
static unsigned position(unsigned n, unsigned count) {
    return count > 1 ? n * 255u / (count - 1) : 0;
}
static unsigned distance(unsigned a, unsigned b) {
    return a > b ? a - b : b - a;
}
static unsigned beam(unsigned coordinate, unsigned center, unsigned width) {
    unsigned d = distance(coordinate, center);
    return d >= width ? 0 : 255 - d * 255 / width;
}
static uint32_t mix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}
/* Integer angular coordinate. This is a bounded approximation used for named
 * pinwheel/spiral behavior; no floating point/libm or dynamic allocations. */
static uint8_t angle(int x, int y) {
    if (!x && !y)
        return 0;
    int ay = y < 0 ? -y : y, a;
    if (x >= 0)
        a = 32 - 32 * (x - ay) / (x + ay);
    else
        a = 96 - 32 * (x + ay) / (ay - x);
    return (uint8_t)(y < 0 ? -a : a);
}
bool my2k_stock_effect_valid(uint8_t type, uint8_t mode) {
    return type == MY2K_STOCK_STRIP_TYPE
               ? (mode < MY2K_STOCK_STRIP_EFFECT_COUNT || mode == MY2K_STOCK_STRIP_CUSTOM_ALIAS)
               : type == MY2K_STOCK_GRID_TYPE && mode < MY2K_STOCK_GRID_EFFECT_COUNT;
}
uint8_t my2k_stock_effect_custom(uint8_t type) {
    return type == MY2K_STOCK_STRIP_TYPE ? 17 : type == MY2K_STOCK_GRID_TYPE ? 15 : 255;
}
uint16_t my2k_stock_effect_period(uint8_t speed) {
    return (uint16_t)(4096u / (1u + speed / 16u));
}
my2k_rgb_color my2k_stock_effect_render(const my2k_stock_effect *e, uint16_t index, my2k_rgb_color custom) {
    if (!e || !my2k_stock_effect_valid(e->type, e->mode) || !e->count || index >= e->count)
        return black();
    unsigned m = e->mode;
    if (m == my2k_stock_effect_custom(e->type) || (e->type == 1 && m == MY2K_STOCK_STRIP_CUSTOM_ALIAS))
        return custom;
    if (m == 0)
        return black();
    if (m == 1)
        return e->base;
    unsigned period = my2k_stock_effect_period(e->speed), phase = (e->now_ms % period) * 256u / period;
    unsigned p = position(index, e->count), v = peak(e->base), h = hue(e->base), breath = triangle(phase);
    if (e->type == MY2K_STOCK_STRIP_TYPE) {
        unsigned width = e->count > 1 ? 512u / e->count : 256;
        if (width < 16)
            width = 16;
        unsigned moving = beam(p, triangle(phase), width);
        switch (m) {
            case 2:
                return my2k_rgb_hsv((uint8_t)(h + p / 2), 255, v); /*Gradient*/
            case 3:
                return phase < 128 ? e->base : black(); /*Blink*/
            case 4:
                return my2k_rgb_hsv((uint8_t)(h + phase + p), 255, v); /*Rainbow*/
            case 5:
                return my2k_rgb_hsv((uint8_t)mix((e->now_ms / period) * 0x9e3779b9u + index + 1u), 255, v); /*Random*/
            case 6:
                return scale(e->base, breath); /*Breath*/
            case 7: {                          /*Circle: wrap an orbit, rather than bounce a scan.*/
                unsigned d = distance(p, phase);
                if (d > 128)
                    d = 256 - d;
                return scale(e->base, d >= width ? 0 : 255 - d * 255 / width);
            }
            case 8:
                return scale(e->base, moving);
            case 9:
                return scale(e->base, moving * breath / 255);
            case 10:
                return scale(my2k_rgb_hsv((uint8_t)(h + phase + p), 255, v), moving);
            case 11:
                return scale(e->base, 255 - moving);
            case 12:
                return scale(e->base, (255 - moving) * breath / 255);
            case 13:
                return scale(my2k_rgb_hsv((uint8_t)(h + phase + p), 255, v), 255 - moving);
            case 14:
                return p <= phase ? e->base : black();
            case 15:
                return p <= phase ? scale(e->base, breath) : black();
            case 16:
                return p <= phase ? my2k_rgb_hsv((uint8_t)(h + phase + p), 255, v) : black();
            default:
                return black();
        }
    }
    unsigned cols = e->cols ? e->cols : e->count, rows = e->rows ? e->rows : (e->count + cols - 1) / cols;
    unsigned column = index % cols, row = index / cols;
    if (row >= rows)
        return black();
    unsigned x = position(column, cols), y = position(row, rows);
    unsigned radius = (distance(x, 128) + distance(y, 128));
    if (radius > 255)
        radius = 255;
    unsigned width_x = cols > 1 ? 512u / cols : 256, width_y = rows > 1 ? 512u / rows : 256;
    if (width_x < 16)
        width_x = 16;
    if (width_y < 16)
        width_y = 16;
    switch (m) {
        case 2:
            return my2k_rgb_hsv((uint8_t)(h + x / 2), 255, v);
        case 3:
            return my2k_rgb_hsv((uint8_t)(h + y / 2), 255, v);
        case 4:
            return scale(e->base, breath);
        case 5:
            return scale(e->base, beam(x, triangle(phase), width_x));
        case 6:
            return scale(e->base, beam(y, triangle(phase), width_y));
        case 7:
            return x <= phase ? e->base : black();
        case 8:
            return y <= phase ? e->base : black();
        case 9:
            return my2k_rgb_hsv((uint8_t)(h + phase + x), 255, v);
        case 10:
            return my2k_rgb_hsv((uint8_t)(h + phase + y), 255, v);
        case 11:
            return my2k_rgb_hsv((uint8_t)(h + phase + radius), 255, v);
        case 12:
            return my2k_rgb_hsv((uint8_t)(h + phase + radius * 2), 255, v);
        case 13:
            return my2k_rgb_hsv((uint8_t)(h + phase + angle((int)x - 128, (int)y - 128)), 255, v);
        case 14:
            return my2k_rgb_hsv((uint8_t)(h + phase + angle((int)x - 128, (int)y - 128) + radius), 255, v);
        default:
            return black();
    }
}
