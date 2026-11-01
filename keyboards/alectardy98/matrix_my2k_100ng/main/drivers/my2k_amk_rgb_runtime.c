/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_amk_rgb_runtime.h"
#include <string.h>
#if MY2K_ACTIVE_DEV_PROFILE
#    include "my2k_render_checkpoint.h"
#endif
#ifndef MY2K_RENDER_CHECKPOINT
#    define MY2K_RENDER_CHECKPOINT() ((void)0)
#endif
#if MY2K_STOCK_PARITY_PROFILE
#    include "my2k_stock_effects.h"
#    include "my2k_stock_color.h"
#endif
#if !(MY2K_LOCK_LIGHTS_PROFILE && MY2K_EEPROM32_PROFILE)
static const uint8_t indicator_led[6]  = {125, 126, 112, 64, 65, 127};
static const uint8_t indicator_mask[6] = {2, 4, 2, 2, 1, 1};
#endif
static my2k_rgb_group *group(my2k_amk_rgb_runtime *r, uint8_t t, uint8_t i) {
    if (t == 3 && i == 0)
        return &r->groups[0];
    if (t == 1 && i < 7)
        return &r->groups[1 + i];
    return NULL;
}
static bool valid_led(uint8_t t, uint16_t i) {
    return (t == 3 && i < 64) || (t == 1 && i >= 64 && i < 125) || (t == 2 && i < 6);
}
static bool scalar(void *c, uint8_t cmd, uint8_t *v) {
    my2k_amk_rgb_runtime *r = c;
    return r->settings && r->settings->scalar && r->settings->scalar(r->settings->context, cmd, v);
}
static bool set_scalar(void *c, uint8_t cmd, uint8_t v) {
    my2k_amk_rgb_runtime *r = c;
    return r->settings && r->settings->ram_writes && r->settings->ram_writes->scalar &&
           r->settings->ram_writes->scalar(r->settings->context, cmd, v);
}
static bool count(void *c, uint8_t t, uint8_t *v) {
    (void)c;
    if (t != 1 && t != 3)
        return false;
    *v = t == 1 ? 7 : 1;
    return true;
}
static bool info(void *c, uint8_t t, uint8_t i, my2k_rgb_info *v) {
    my2k_rgb_group *g = group(c, t, i);
    if (!g)
        return false;
    *v = (my2k_rgb_info){.config  = 1,
                         .enabled = g->mode != 0,
                         .mode    = g->mode,
                         .custom  = MY2K_EFFECT_CUSTOM,
                         .rows    = t == 3 ? 4 : 0,
                         .cols    = t == 3 ? 16 : 0};
#if MY2K_STOCK_PARITY_PROFILE
    v->config = t == 3 ? 0 : 1 + i;
    v->custom = my2k_stock_effect_custom(t);
#endif
    return true;
}
static bool led(void *c, uint8_t t, uint16_t i, uint8_t v[4]) {
    my2k_amk_rgb_runtime *r = c;
    if (!valid_led(t, i))
        return false;
    my2k_rgb_config p = t == 2 ? r->indicators[i] : r->rgb.config[i];
    v[0]              = p.h;
    v[1]              = p.s;
    v[2]              = p.v;
    v[3]              = p.flags;
    return true;
}
static bool set_led(void *c, uint8_t t, uint16_t i, const uint8_t v[4]) {
    my2k_amk_rgb_runtime *r = c;
    if (!valid_led(t, i))
        return false;
    my2k_rgb_config p = {v[0], v[1], v[2], v[3]};
    if (t == 2)
        r->indicators[i] = p;
    else
        r->rgb.config[i] = p;
    r->dirty = true;
    r->configuration_revision++;
    return true;
}
static bool rendered(void *c, uint16_t i, uint8_t v[3]) {
    my2k_amk_rgb_runtime *r = c;
    if (i >= 128)
        return false;
    my2k_rgb_color p = r->rgb.rendered[i];
    v[0]             = p.r;
    v[1]             = p.g;
    v[2]             = p.b;
    return true;
}
static bool mode(void *c, uint8_t t, uint8_t i, uint8_t m) {
    my2k_amk_rgb_runtime *r = c;
    my2k_rgb_group       *g = group(r, t, i);
    if (!g)
        return false;
#if MY2K_STOCK_PARITY_PROFILE
    if (!my2k_stock_effect_valid(t, m))
        return false;
#else
    if (m >= MY2K_EFFECT_COUNT)
        return false;
#endif
    g->mode = m;
#if MY2K_DEV3_PROFILE
    if (m != MY2K_EFFECT_OFF)
        r->enabled = true;
#endif
    r->dirty = true;
    r->configuration_revision++;
    return true;
}
#if MY2K_DEV3_PROFILE && !MY2K_STOCK_PARITY_PROFILE
static void to_hsv(my2k_rgb_color c, uint8_t out[3]) {
    uint8_t hi = c.r > c.g ? c.r : c.g;
    if (c.b > hi)
        hi = c.b;
    uint8_t lo = c.r < c.g ? c.r : c.g;
    if (c.b < lo)
        lo = c.b;
    int delta = hi - lo, h = 0;
    if (delta) {
        if (hi == c.r)
            h = 43 * ((int)c.g - c.b) / delta;
        else if (hi == c.g)
            h = 85 + 43 * ((int)c.b - c.r) / delta;
        else
            h = 171 + 43 * ((int)c.r - c.g) / delta;
    }
    out[0] = (uint8_t)h;
    out[1] = hi ? (uint8_t)(delta * 255 / hi) : 0;
    out[2] = hi;
}
#endif
static bool param(void *c, uint8_t t, uint8_t p, uint8_t i, uint8_t v[3]) {
    my2k_rgb_group *g = group(c, t, i);
    if (!g)
        return false;
    if (p == 0) {
#if MY2K_STOCK_PARITY_PROFILE
        my2k_rgb_color color = my2k_stock_hsv_to_rgb(g->color.r, g->color.g, g->color.b);
        v[0]                 = color.r;
        v[1]                 = color.g;
        v[2]                 = color.b;
#else
        v[0] = g->color.r;
        v[1] = g->color.g;
        v[2] = g->color.b;
#endif
        return true;
    }
    if (p == 2) {
        v[0] = g->speed;
        return true;
    }
#if MY2K_DEV3_PROFILE
    my2k_amk_rgb_runtime *r    = c;
    unsigned              slot = (unsigned)(g - r->groups);
    if (p == 1) {
#    if MY2K_STOCK_PARITY_PROFILE
        v[0] = g->color.r;
        v[1] = g->color.g;
        v[2] = g->color.b;
#    else
        to_hsv(g->color, v);
#    endif
        return true;
    }
#    if MY2K_STOCK_PARITY_PROFILE
    if (slot == 0 && (p == 3 || p == 5))
        return false;
#    endif
    if (p == 3) {
        v[0] = r->sync[slot];
        return true;
    }
    if (p == 4) {
        v[0] = r->brightness[slot];
        return true;
    }
    if (p == 5) {
        v[0] = r->use_custom[slot];
        return true;
    }
#endif
    return false;
}
static bool set_param(void *c, uint8_t t, uint8_t p, uint8_t i, const uint8_t *v, size_t n) {
    my2k_amk_rgb_runtime *r = c;
    my2k_rgb_group       *g = group(r, t, i);
    if (!g)
        return false;
#if MY2K_STOCK_PARITY_PROFILE
    if (g == r->groups && (p == 3 || p == 5))
        return false;
#endif
    if (p == 0 && n == 3)
#if MY2K_STOCK_PARITY_PROFILE
        g->color = my2k_stock_rgb_to_hsv((my2k_rgb_color){v[0], v[1], v[2]});
#else
        g->color = (my2k_rgb_color){v[0], v[1], v[2]};
#endif
    else if (p == 2 && n == 1) {
        g->speed = v[0];
    }
#if MY2K_DEV3_PROFILE
    else if (p == 1 && n == 3) {
#    if MY2K_STOCK_PARITY_PROFILE
        g->color = (my2k_rgb_color){v[0], v[1], v[2]};
#    else
        g->color = my2k_rgb_hsv(v[0], v[1], v[2]);
#    endif
    } else if (p == 4 && n == 1)
        r->brightness[g - r->groups] = v[0];
    else if (p == 3 && n == 1 && v[0] <= 1)
        r->sync[g - r->groups] = v[0];
    else if (p == 5 && n == 1 && v[0] <= 1)
        r->use_custom[g - r->groups] = v[0];
#endif
    else
        return false;
    r->dirty = true;
    r->configuration_revision++;
    return true;
}
static const my2k_amk_ram_writes writes = {set_scalar, set_led, mode, set_param};
void                             my2k_amk_rgb_init(my2k_amk_rgb_runtime *r, const my2k_amk_backend *settings) {
    if (!r)
        return;
    memset(r, 0, sizeof(*r));
    r->settings               = settings;
    r->enabled                = true;
    r->dirty                  = true;
    r->configuration_revision = 1;
#if MY2K_STOCK_PARITY_PROFILE
    r->debounce_press = r->debounce_release = 5;
    r->aux_mode                             = 0;
#endif
    r->backend = (my2k_amk_backend){.context    = r,
                                    .total_leds = 128,
                                    .scalar     = scalar,
                                    .count      = count,
                                    .info       = info,
                                    .led        = led,
                                    .rendered   = rendered,
                                    .param      = param,
                                    .ram_writes = &writes};
    for (unsigned i = 0; i < 8; i++) {
        r->groups[i].mode  = MY2K_EFFECT_STATIC;
        r->groups[i].color = (my2k_rgb_color){16, 16, 16};
        r->brightness[i]   = 255;
#if MY2K_DEV3_PROFILE
        r->groups[i].color = (my2k_rgb_color){255, 255, 255};
        r->brightness[i]   = 128;
#endif
#if MY2K_STOCK_PARITY_PROFILE
        /* Factory-readable range restored; moderate initial output remains a
         * user color setting rather than a hidden physical PWM clamp. */
        r->groups[i].color = (my2k_rgb_color){0, 255, 200};
        r->brightness[i]   = 255;
        r->groups[i].speed = 127;
#endif
    }
    for (unsigned i = 0; i < 128; i++)
        r->rgb.config[i] = (my2k_rgb_config){0, 255, 32, 1};
#if MY2K_STOCK_PARITY_PROFILE
    for (unsigned i = 0; i < 128; i++)
        r->rgb.config[i] = (my2k_rgb_config){0, 255, 255, 0x81};
#endif
    for (unsigned i = 0; i < 6; i++)
        r->indicators[i] = (my2k_rgb_config){0, 0, 32, 1};
#if MY2K_STOCK_PARITY_PROFILE
    /* Captured stock FD33: all six indicators HSV0/0/255, flags81. */
    for (unsigned i = 0; i < 6; i++)
        r->indicators[i] = (my2k_rgb_config){0, 0, 255, 0x81};
#endif
    (void)my2k_amk_rgb_tick(r, 0, 0);
}
bool my2k_amk_rgb_tick(my2k_amk_rgb_runtime *r, uint32_t now, uint8_t locks) {
    if (!r)
        return false;
    if (r->started && !r->dirty && locks == r->host_locks && (uint32_t)(now - r->last_frame) < 20)
        return false;
    r->last_frame = now;
    r->host_locks = locks;
    r->started    = true;
    r->dirty      = false;
    my2k_rgb_render_custom(&r->rgb, now);
    for (unsigned j = 0; j < 8; j++) {
        unsigned source = j;
#if MY2K_DEV3_PROFILE
        if (j > 1 && r->sync[j])
            source = 1; /* optional strips mirror first strip's effect */
#endif
        my2k_rgb_group *g     = &r->groups[source];
        unsigned        start = j ? my2k_rgb_strips[j - 1].start : 0, n = j ? my2k_rgb_strips[j - 1].count : 64;
#if !MY2K_STOCK_PARITY_PROFILE
        uint32_t period = 4096u / (1u + g->speed / 16u), phase = now % period;
#endif
        for (unsigned k = 0; k < n; k++) {
            my2k_rgb_color color = g->color;
#if MY2K_STOCK_PARITY_PROFILE
            color = my2k_stock_hsv_to_rgb(color.r, color.g, color.b);
#endif
#if MY2K_DEV3_PROFILE
            if (r->use_custom[source])
                color = my2k_rgb_custom_color(r->rgb.config[start + k], now);
#endif
#if MY2K_STOCK_PARITY_PROFILE
            my2k_stock_effect effect = {.type   = j ? 1 : 3,
                                        .mode   = g->mode,
                                        .speed  = g->speed,
                                        .count  = n,
                                        .rows   = j ? 1 : 4,
                                        .cols   = j ? n : 16,
                                        .now_ms = now,
                                        .base   = color};
            my2k_rgb_color    custom = r->rgb.rendered[start + k];
            if (j && (g->mode == 17 || g->mode == 26)) {
                /* Stock strip Custom is governed by its group's value; grid
                 * Custom uses per-LED values. Both were observed live. */
                my2k_rgb_config config = r->rgb.config[start + k];
                config.v               = (uint16_t)config.v * g->color.b / 255u;
                custom                 = my2k_rgb_custom_color(config, now);
            }
            color = my2k_stock_effect_render(&effect, k, custom);
#else
            switch (g->mode) {
                case MY2K_EFFECT_CUSTOM:
#    if MY2K_DEV3_PROFILE
                    color = r->rgb.rendered[start + k];
                    break;
#    else
                    continue;
#    endif
                case MY2K_EFFECT_OFF:
                    color = (my2k_rgb_color){0, 0, 0};
                    break;
                case MY2K_EFFECT_RAINBOW:
                    color = my2k_rgb_hsv((uint8_t)(phase * 256u / period + k * 256u / n), 255, 64);
                    break;
                case MY2K_EFFECT_BREATHING: {
                    uint32_t ramp = phase < period / 2 ? phase : period - phase;
                    color.r       = (uint8_t)((uint32_t)color.r * ramp / (period / 2));
                    color.g       = (uint8_t)((uint32_t)color.g * ramp / (period / 2));
                    color.b       = (uint8_t)((uint32_t)color.b * ramp / (period / 2));
                    break;
                }
                default:
                    break;
            }
#endif
#if MY2K_DEV3_PROFILE
            color.r = (uint8_t)((uint16_t)color.r * r->brightness[source] / 255);
            color.g = (uint8_t)((uint16_t)color.g * r->brightness[source] / 255);
            color.b = (uint8_t)((uint16_t)color.b * r->brightness[source] / 255);
#endif
            r->rgb.rendered[start + k] = color;
            if ((k & 7u) == 7u)
                MY2K_RENDER_CHECKPOINT();
        }
    }
    /* New explicit policy: active configured lock indicators overlay effects.
     * Shared LEDs regain their base effect on release; dedicated ones go black. */
    for (unsigned i = 125; i < 128; i++)
        r->rgb.rendered[i] = (my2k_rgb_color){0, 0, 0};
#if !(MY2K_LOCK_LIGHTS_PROFILE && MY2K_EEPROM32_PROFILE)
    /* LKS2 supplies the complete conditional overlay after this base frame.
     * Do not let legacy indicator settings bypass its per-item enable flags. */
    for (unsigned i = 0; i < 6; i++)
        if ((locks & indicator_mask[i]) && (r->indicators[i].flags & 1u))
            r->rgb.rendered[indicator_led[i]] = my2k_rgb_custom_color(r->indicators[i], now);
#endif
#if MY2K_DEV3_PROFILE
    if (!r->enabled)
        memset(r->rgb.rendered, 0, sizeof r->rgb.rendered);
#endif
    r->frames++;
    return true;
}

/* Native VialRGB1 uses the repository's stable IDs:0Off,2Solid,6Breathing,14CycleLeftRight.
 * A global SET updates the same groups that AMK edits individually; no second engine. */
bool my2k_lighting_hid(my2k_amk_rgb_runtime *r, uint8_t p[32]) {
#if MY2K_DEV3_PROFILE && !MY2K_STOCK_PARITY_PROFILE
    if (!r || !p)
        return false;
    uint8_t command = p[0], field = p[1];
    if (command == 8) {
        if (field == 0x40) {
            memset(p + 2, 0, 30);
            p[2] = 1;
            p[4] = 255;
            return true;
        }
        if (field == 0x41) {
            uint8_t hsv[3];
            to_hsv(r->groups[0].color, hsv);
            uint8_t mode = r->groups[0].mode;
            memset(p + 2, 0, 30);
            p[2] = !r->enabled || mode == 0 ? 0 : mode == 2 ? 14 : mode == 3 ? 6 : 2;
            p[4] = r->groups[0].speed;
            p[5] = hsv[0];
            p[6] = hsv[1];
            p[7] = (uint8_t)((uint16_t)hsv[2] * r->brightness[0] / 255);
            return true;
        }
        if (field == 0x42) {
            uint16_t       greater = p[2] | (uint16_t)p[3] << 8;
            const uint16_t ids[]   = {2, 6, 14};
            memset(p + 2, 255, 30);
            unsigned off = 2;
            for (unsigned i = 0; i < 3; i++)
                if (ids[i] > greater) {
                    p[off++] = (uint8_t)ids[i];
                    p[off++] = 0;
                }
            return true;
        }
        return false;
    }
    if (command == 7 && field == 0x41) {
        uint16_t id = p[2] | (uint16_t)p[3] << 8;
        if (id != 0 && id != 2 && id != 6 && id != 14) {
            p[0] = 255;
            return true;
        }
        uint8_t        mode = id == 0 ? 0 : id == 6 ? 3 : id == 14 ? 2 : 1;
        my2k_rgb_color c    = my2k_rgb_hsv(p[5], p[6], 255);
        for (unsigned i = 0; i < 8; i++) {
            r->groups[i].mode  = mode;
            r->groups[i].speed = p[4];
            r->groups[i].color = c;
            r->brightness[i]   = p[7];
            r->sync[i] = r->use_custom[i] = 0;
        }
        r->enabled = id != 0;
        for (unsigned i = 0; i < 6; i++)
            r->indicators[i].v = p[7];
        r->dirty = true;
        r->configuration_revision++;
        return true;
    }
    if (command == 9)
        return true; /* Existing deferred EEPROM task already owns persistence. */
#else
    (void)r;
    (void)p;
#endif
    return false;
}
