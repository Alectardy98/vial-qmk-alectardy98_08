/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_panels.h"
#include "aux_animation.h"
#include "aux_perf21.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t now, origin, reset_high, reset_low, swreset, sleepout, mono_low, mono_high;
static bool     dc[2], configured, broken, saw_on;
static uint8_t  color_command;
static unsigned clear_bytes;
uint32_t        aux_millis(void) {
    return now;
}
static bool configure(void *ctx) {
    (void)ctx;
    return configured;
}
static bool signal(void *ctx, aux_panel_id id, unsigned line, bool high) {
    (void)ctx;
    if (line == AUX_PANEL_DC)
        dc[id] = high;
    if (line == AUX_PANEL_RESET) {
        if (id == AUX_PANEL_COLOR) {
            if (high) {
                reset_high = now;
                assert((uint32_t)(now - reset_low) >= 5);
            } else
                reset_low = now;
        } else {
            if (high) {
                mono_high = now;
                assert((uint32_t)(now - mono_low) >= 100);
            } else
                mono_low = now;
        }
    }
    return true;
}
static bool send(void *ctx, aux_panel_id id, const uint8_t *data, size_t n) {
    (void)ctx;
    if (id == AUX_PANEL_MONO) {
        if (!dc[id] && data[0] == 0xae)
            assert((uint32_t)(now - mono_high) >= 100);
        return true;
    }
    if (!dc[id]) {
        assert(n == 1);
        color_command = data[0];
        if (color_command == 1) {
            assert((uint32_t)(now - reset_high) < 10);
            swreset = now;
        } else if (color_command == 0x11) {
            assert((uint32_t)(now - swreset) >= 150);
            sleepout = now;
        } else {
            assert((uint32_t)(now - sleepout) >= 500);
        }
        if (color_command == 0xb1 && broken)
            return false;
        if (color_command == 0x29) {
            assert(clear_bytes == 32768);
            saw_on = true;
        }
    } else if (color_command == 0x2c && !saw_on) {
        for (unsigned i = 0; i < n; i++)
            assert(!data[i]);
        clear_bytes += (unsigned)n;
    }
    return true;
}
static bool delay(void *ctx, uint32_t ms) {
    (void)ctx;
    (void)ms;
    assert(!"boot must not call blocking delay");
    return false;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    my2k_wire_message q = {.type = MY2K_AC, .v = {8, 1, 1, 5}};
    assert(aux_animation_startup_probe(&q) && !aux_animation_startup_probe(NULL));
    for (unsigned op = 0; op < 256; op++) {
        q.v[0] = (uint8_t)op;
        assert(aux_animation_startup_probe(&q) == (op == 8));
    }
    q.v[0] = 8;
    for (unsigned i = 4; i < 8; i++) {
        q.v[i] = 1;
        assert(!aux_animation_startup_probe(&q));
        q.v[i] = 0;
    }
    q.v[1] = 0;
    assert(!aux_animation_startup_probe(&q));
    q.v[1] = 1;
    q.v[3] = 0;
    assert(aux_animation_startup_probe(&q));
    q.v[3] = 4;
    assert(!aux_animation_startup_probe(&q));
    configured = strcmp(argv[1], "configure-fail") != 0;
    broken     = !strcmp(argv[1], "transfer-fail");
    now = origin = !strcmp(argv[1], "wrap") ? UINT32_MAX - 200u : 0;
    aux_panels    p;
    aux_panel_bus bus  = {0, configure, signal, send, delay};
    bool          okay = aux_panels_begin(&p, &bus, now);
    if (!configured) {
        assert(!okay && !aux_panels_starting(&p));
        return 0;
    }
    assert(okay && !p.color_ready && !p.mono_ready);
    bool card = false, mono_first = false;
    for (unsigned budget = 0; aux_panels_starting(&p); budget++) {
        assert(budget < 3000);
        aux_panels_service(&p, now);
        if (p.mono_ready && !p.color_ready)
            mono_first = true;
        if (!card && p.color_setting >= 2) {
            now += 200;
            card = true;
        }
        if (p.color_waiting && (int32_t)(now - p.color_deadline) < 0)
            now++;
        else if (p.color_fault && p.mono_startup)
            now++;
    }
    assert(p.mono_ready && mono_first && card);
    if (broken) {
        assert(p.color_fault && !p.color_ready && !saw_on);
        return 0;
    }
    assert(p.color_ready && saw_on && clear_bytes == 32768);
    assert((uint32_t)(now - origin) >= 766 && (uint32_t)(now - origin) < 800);
    /* First completed frame is immutable boot telemetry, not a loop counter. */
    uint16_t pixels[128] = {0};
    now                  = origin + 900;
    for (unsigned i = 0; i < 128; i++)
        assert(aux_panel_color_pixels(&p, 128, 128, 128 * i, pixels, 128));
    assert(aux_startup.first_frame_ms == now && aux_display_perf.frames == 1);
    now += 100;
    for (unsigned i = 0; i < 128; i++)
        assert(aux_panel_color_pixels(&p, 128, 128, 128 * i, pixels, 128));
    assert(aux_startup.first_frame_ms == origin + 900 && aux_display_perf.frames == 2);
    puts("PASS overlapping boot, unchanged waits/clear/order, storage delay, wrap and first-frame latch");
}
