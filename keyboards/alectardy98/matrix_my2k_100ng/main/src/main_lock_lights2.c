/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_lock_lights.h"
#include "main_lock_store.h"
#include <string.h>
static uint8_t        active[128], staged[128], token, received, phase, error, selected, host_locks;
static uint32_t       revision, stage_time, selected_at, selection_changes, frames, layers_seen, on_at[5];
static bool           upload;
static my2k_rgb_color output;
static const uint8_t  fixed_led[5] = {126, 125, 127, 64, 65}, fixed_mask[5] = {4, 2, 1, 2, 1};
static uint32_t       get(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
__attribute__((noinline, noclone, externally_visible)) void my2k_lock_init(bool preserve_legacy) {
    (void)preserve_legacy;
    my2k_lock_defaults(active);
    my2k_lock_store_init();
    (void)my2k_lock_store_load(active);
    upload = false;
    token = received = phase = error = host_locks = 0;
    selected                                      = 255;
    revision                                      = 1;
    stage_time = selected_at = selection_changes = frames = 0;
    layers_seen                                           = UINT32_MAX;
    memset(on_at, 0, sizeof on_at);
    output = (my2k_rgb_color){0, 0, 0};
}
__attribute__((noinline, noclone, externally_visible)) void my2k_lock_service(uint32_t now, bool allow_flash) {
    (void)allow_flash;
    if (upload && (uint32_t)(now - stage_time) > 5000) {
        upload   = false;
        received = 0;
    }
    my2k_lock_store_task();
    if (phase == 1 && !my2k_lock_store_pending()) {
        uint8_t saved[128];
        if (my2k_lock_store_error() || !my2k_lock_store_load(saved) || memcmp(saved, active, 128)) {
            phase = 3;
            error = 7;
        } else {
            phase = 2;
            error = 0;
        }
    }
    if (phase == 1 && my2k_lock_store_error()) {
        phase = 3;
        error = 7;
    }
}
__attribute__((noinline, noclone, externally_visible)) bool my2k_lock_hid(uint8_t *p, uint8_t length, uint32_t now,
                                                                          bool allow_writes) {
    if (!p || length != 32 || p[0] != 253 || p[1] != 233)
        return false;
    uint8_t req[32];
    memcpy(req, p, 32);
    memset(p, 0, 32);
    uint8_t op = req[2], t = req[4];
    p[0] = 253;
    p[1] = 233;
    p[2] = 0x55;
    p[3] = 2;
    p[4] = op;
    p[5] = t;
#define FAIL(code)     \
    do {               \
        p[6] = (code); \
        return true;   \
    } while (0)
    if (req[3] != 2 || op > 6)
        FAIL(1);
    if (upload && (uint32_t)(now - stage_time) > 5000)
        upload = false;
    if (op >= 2 && op <= 4 && !allow_writes)
        FAIL(2);
    if (op == 0) {
        memcpy(p + 6, "LKS2", 4);
        p[10] = 128;
        p[11] = 8;
        p[12] = 5;
        p[13] = 0x0e;
        p[14] = 5;
        p[15] = 1;
        put(p + 16, revision);
        p[20] = 255;
        p[21] = 255;
        p[22] = my2k_lock_store_error() != 0;
        p[23] = 1;
    } else if (op == 1) {
        unsigned at = req[5], n = req[6];
        if (!n || n > 24 || at >= 128 || n > 128 - at)
            FAIL(3);
        p[6] = at;
        p[7] = n;
        memcpy(p + 8, active + at, n);
    } else if (op == 2) {
        if (upload)
            FAIL(4);
        if (!t || get(req + 5) != revision)
            FAIL(5);
        if (my2k_lock_store_error())
            FAIL(7);
        token      = t;
        received   = 0;
        upload     = true;
        stage_time = now;
        memset(staged, 0, 128);
    } else if (op == 3) {
        unsigned at = req[5], n = req[6];
        if (!upload || t != token || !n || n > 24 || at != received || at >= 128 || n > 128 - at)
            FAIL(3);
        memcpy(staged + at, req + 7, n);
        received += (uint8_t)n;
        stage_time = now;
    } else if (op == 4) {
        if (!upload || t != token || received != 128)
            FAIL(3);
        upload = false;
        if (!my2k_lock_validate(staged))
            FAIL(6);
        if (!my2k_lock_store_request(staged))
            FAIL(7);
        /* Apply live immediately; durable status is separate and never fabricated. */
        memcpy(active, staged, 128);
        revision++;
        phase       = my2k_lock_store_pending() ? 1 : 2;
        error       = 0;
        layers_seen = UINT32_MAX;
        selected    = 255;
    } else if (op == 5) {
        p[6] = phase;
        p[7] = error;
        p[8] = my2k_lock_store_pending();
        p[9] = my2k_lock_store_error() != 0;
        put(p + 10, revision);
        memcpy(p + 14, active + 124, 4);
        put(p + 18, my2k_lock_store_commits());
    } else if (op == 6) {
        p[6] = selected;
        p[8] = host_locks;
        put(p + 9, layers_seen);
        p[13] = output.r;
        p[14] = output.g;
        p[15] = output.b;
        put(p + 16, frames);
        put(p + 20, selection_changes);
        p[24] = 112;
        p[25] = my2k_rgb_channels[112].driver;
        p[26] = my2k_lock_store_pending();
        p[27] = my2k_lock_store_error() != 0;
        for (unsigned i = 0; i < 5; i++)
            if (active[72 + i * 8])
                p[28] |= 1u << i;
    }
    p[2] = 0xaa;
    return true;
#undef FAIL
}
__attribute__((noinline, noclone, externally_visible)) bool my2k_lock_frame(my2k_amk_rgb_runtime *r, uint32_t now,
                                                                            uint8_t locks, uint32_t layers) {
    if (layers != layers_seen) {
        r->dirty    = true;
        layers_seen = layers;
    }
    if (!my2k_amk_rgb_tick(r, now, locks))
        return false;
    uint8_t match;
    output = my2k_lock_evaluate(active, locks, layers, now - selected_at, &match);
    if (match != selected) {
        selected    = match;
        selected_at = now;
        selection_changes++;
        output = my2k_lock_evaluate(active, locks, layers, 0, &match);
    }
    for (unsigned i = 0; i < 6; i++) {
        my2k_rgb_color color;
        if (i == 5) {
            /* None/no match inherits the freshly rendered strip. An explicit fallback
             * (even black) or matching rule owns the pixel until the condition clears. */
            if (match == 255 && !active[2]) {
                output = r->rgb.rendered[112];
                continue;
            }
            color = output;
        } else {
            const uint8_t *f = active + 72 + i * 8;
            if ((locks & fixed_mask[i]) && !(host_locks & fixed_mask[i]))
                on_at[i] = now;
            bool enabled = f[0] && (locks & fixed_mask[i]);
            /* Shared100NG pixels retain strip7 when no override applies. Dedicated
             * Under Cap pixels are already black in the base renderer. */
            if (!enabled)
                continue;
            color = my2k_lock_effect((my2k_rgb_color){f[3], f[4], f[5]}, f[1], f[2], now - on_at[i]);
        }
        color.r = (uint16_t)color.r * r->brightness[4] / 255u;
        color.g = (uint16_t)color.g * r->brightness[4] / 255u;
        color.b = (uint16_t)color.b * r->brightness[4] / 255u;
        if (!r->enabled)
            color = (my2k_rgb_color){0, 0, 0};
        r->rgb.rendered[i == 5 ? 112 : fixed_led[i]] = color;
        if (i == 5)
            output = color;
    }
    host_locks = locks;
    frames++;
    return true;
}
