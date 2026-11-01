/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_rgb.h"
#if MY2K_ACTIVE_DEV_PROFILE
#    include "my2k_render_checkpoint.h"
#endif
#ifndef MY2K_RENDER_CHECKPOINT
#    define MY2K_RENDER_CHECKPOINT() ((void)0)
#endif
#include <string.h>
#if MY2K_STOCK_PARITY_PROFILE
#    include "my2k_stock_color.h"
#endif
/* Reconstructed hardware topology, not proprietary driver implementation. */
const my2k_rgb_driver  my2k_rgb_drivers[6]               = {{8, 0x6e, 0, 32}, {8, 0x68, 1, 34}, {3, 0xe8, 0, 19},
                                                            {3, 0xee, 1, 19}, {3, 0xea, 2, 21}, {5, 0x78, 0, 3}};
const my2k_rgb_channel my2k_rgb_channels[MY2K_RGB_COUNT] = {
#include "my2k_rgb_channels.inc"
};
/* Preserve definition order independently from global LED address order. */
const my2k_rgb_strip my2k_rgb_strips[7] = {{66, 19}, {85, 19}, {104, 8}, {112, 1}, {113, 8}, {121, 4}, {64, 2}};
void                 my2k_rgb_init(my2k_rgb_state *s) {
    if (s)
        memset(s, 0, sizeof(*s));
}
bool my2k_rgb_location(uint16_t led, const my2k_rgb_driver **driver, uint8_t *local) {
    if (!driver || !local || led >= MY2K_RGB_COUNT)
        return false;
    const my2k_rgb_channel *c = &my2k_rgb_channels[led];
    *driver                   = &my2k_rgb_drivers[c->driver];
    *local                    = c->local;
    return true;
}
bool my2k_rgb_set_config(my2k_rgb_state *s, uint16_t led, my2k_rgb_config c) {
    if (!s || led >= MY2K_RGB_COUNT)
        return false;
    s->config[led] = c;
    return true;
}
bool my2k_rgb_set_rendered(my2k_rgb_state *s, uint16_t start, const my2k_rgb_color *colors, size_t n) {
    if (!s || (!colors && n) || start > MY2K_RGB_COUNT || n > MY2K_RGB_COUNT - start)
        return false;
    if (n)
        memcpy(s->rendered + start, colors, n * sizeof(*colors));
    s->revision++;
    return true;
}
bool my2k_rgb_snapshot(const my2k_rgb_state *s, uint16_t start, size_t n, uint8_t *rgb, size_t cap) {
    if (!s || (!rgb && n) || start > MY2K_RGB_COUNT || n > MY2K_RGB_COUNT - start || n > cap / 3)
        return false;
    for (size_t i = 0; i < n; i++) {
        rgb[3 * i]     = s->rendered[start + i].r;
        rgb[3 * i + 1] = s->rendered[start + i].g;
        rgb[3 * i + 2] = s->rendered[start + i].b;
    }
    return true;
}
my2k_rgb_color my2k_rgb_hsv(uint8_t h, uint8_t s, uint8_t v) {
    if (s == 0)
        return (my2k_rgb_color){v, v, v};
    uint16_t scaled = (uint16_t)h * 6u;
    uint8_t  region = (uint8_t)(scaled >> 8), f = (uint8_t)scaled;
    uint8_t  p = (uint8_t)((uint16_t)v * (255u - s) / 255u);
    uint8_t  q = (uint8_t)((uint16_t)v * (255u - (uint16_t)s * f / 255u) / 255u);
    uint8_t  t = (uint8_t)((uint16_t)v * (255u - (uint16_t)s * (255u - f) / 255u) / 255u);
    switch (region) {
        case 0:
            return (my2k_rgb_color){v, t, p};
        case 1:
            return (my2k_rgb_color){q, v, p};
        case 2:
            return (my2k_rgb_color){p, v, t};
        case 3:
            return (my2k_rgb_color){p, q, v};
        case 4:
            return (my2k_rgb_color){t, p, v};
        default:
            return (my2k_rgb_color){v, p, q};
    }
}
my2k_rgb_color my2k_rgb_custom_color(my2k_rgb_config c, uint32_t now) {
    uint32_t period = 2048u / ((c.flags >> 4) + 1u), phase = now % period;
    uint8_t  v = c.v;
    if (!(c.flags & 1u) || ((c.flags & 4u) && phase >= period / 2u))
        v = 0;
    if (c.flags & 8u) {
        uint32_t ramp = phase < period / 2u ? phase : period - phase;
        v             = (uint8_t)((uint32_t)v * ramp / (period / 2u));
    }
    uint8_t h = c.h;
    if (c.flags & 2u)
        h = (uint8_t)(h + (uint64_t)now * 256u / period);
#if MY2K_STOCK_PARITY_PROFILE
    return my2k_stock_hsv_to_rgb(h, c.s, v);
#else
    return my2k_rgb_hsv(h, c.s, v);
#endif
}
void my2k_rgb_render_custom(my2k_rgb_state *s, uint32_t now) {
    if (!s)
        return;
    for (unsigned i = 0; i < MY2K_RGB_COUNT; i++) {
        s->rendered[i] = my2k_rgb_custom_color(s->config[i], now);
        if ((i & 7u) == 7u)
            MY2K_RENDER_CHECKPOINT();
    }
    s->revision++;
}
bool my2k_rgb_flush(const my2k_rgb_state *s, my2k_rgb_write_fn write, void *ctx, bool verified) {
    if (!s || !write || !verified)
        return false;
    for (uint16_t i = 0; i < MY2K_RGB_COUNT; i++) {
        const my2k_rgb_driver *d;
        uint8_t                n;
        if (!my2k_rgb_location(i, &d, &n) || !write(ctx, d, n, s->rendered[i]))
            return false;
    }
    return true;
}
