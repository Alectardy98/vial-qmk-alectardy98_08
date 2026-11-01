/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_touch_config.h"
#include "main_touch_engine.h"
#include "main_eeprom.h"
#include "main_persist.h"
#include "eeprom_driver.h"
#include <string.h>
/* Standard QMK output ABI; gesture/config logic has no quantum internals. */
void register_code16(uint16_t);
void unregister_code16(uint16_t);
#define LEGACY_ADDRESS ((void *)(uintptr_t)32640u)
#define ADDRESS ((void *)(uintptr_t)32656u)
static my2k_touch_engine engine;
static uint8_t           active[16];
static uint32_t          revision;
static bool              allowed;
static uint32_t          get(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static void emit(void *ctx, uint16_t code, bool down) {
    (void)ctx;
    if (down)
        register_code16(code);
    else
        unregister_code16(code);
}
__attribute__((noinline, noclone, externally_visible)) void my2k_touchbar_init(void) {
    my2k_touch_defaults(active);
    uint8_t saved[16];
    eeprom_read_block(saved, ADDRESS, 16);
    if (my2k_touch_config_valid(saved))
        memcpy(active, saved, 16);
    else {
        eeprom_read_block(saved, LEGACY_ADDRESS, 16);
        if (my2k_touch_config_upgrade(saved))
            memcpy(active, saved, 16);
    }
    my2k_touch_engine_init(&engine, emit, NULL);
    my2k_touch_engine_configure(&engine, active);
    revision = 1;
    allowed  = true;
}
void my2k_touchbar_cancel(void) {
    my2k_touch_engine_cancel(&engine);
    allowed = false;
}
__attribute__((noinline, noclone, externally_visible)) void my2k_touchbar_sample(const uint8_t *raw, uint32_t now) {
    if (allowed)
        my2k_touch_engine_sample(&engine, raw, now);
}
__attribute__((noinline, noclone, externally_visible)) void my2k_touchbar_service(uint32_t now, bool run) {
    if (!run) {
        if (allowed)
            my2k_touchbar_cancel();
        return;
    }
    allowed = true;
    my2k_touch_engine_task(&engine, now);
}
__attribute__((noinline, noclone, externally_visible)) bool my2k_touchbar_hid(uint8_t *p, uint8_t n, bool writes) {
    if (!p || n != 32 || p[0] != 253 || p[1] != 235)
        return false;
    uint8_t q[32];
    memcpy(q, p, 32);
    memset(p, 0, 32);
    p[0] = 253;
    p[1] = 235;
    p[2] = 85;
    p[3] = 2;
    p[4] = q[2];
    p[5] = q[4];
#define FAIL(e)      \
    do {             \
        p[6] = (e);  \
        return true; \
    } while (0)
    unsigned op = q[2];
    if (q[3] != 2 || op > 5)
        FAIL(1);
    if (op == 0) {
        memcpy(p + 6, "TCH2", 4);
        p[10] = 16;
        p[11] = 3;
        p[12] = 1;
        p[13] = 100;
        p[14] = 50;
        p[15] = 1;
        put(p + 16, revision);
        put(p + 20, my2k_persist_error());
        p[24] = 0;
        p[25] = 100;
        p[26] = 50;
        p[27] = 1;
        p[28] = 1; /* Hold-mode jitter supported; additive TCH2 capability. */
    }
    if (op == 1) {
        memcpy(p + 8, active, 16);
        put(p + 24, revision);
        p[28] = my2k_eeprom_pending();
        p[29] = my2k_persist_error() != 0;
    }
    if (op == 2) {
        if (!writes)
            FAIL(2);
        if (!q[4] || get(q + 5) != revision)
            FAIL(3);
        if (!my2k_touch_config_valid(q + 9))
            FAIL(4);
        if (!my2k_eeprom_durable() || my2k_persist_error())
            FAIL(5);
        eeprom_write_block(q + 9, ADDRESS, 16);
        uint8_t saved[16];
        eeprom_read_block(saved, ADDRESS, 16);
        if (my2k_persist_error() || memcmp(saved, q + 9, 16))
            FAIL(5);
        my2k_touch_engine_configure(&engine, q + 9);
        memcpy(active, q + 9, 16);
        revision++;
    }
    if (op == 2 || op == 3) {
        put(p + 8, revision);
        memcpy(p + 12, active + 12, 4);
        p[16] = my2k_eeprom_pending();
        put(p + 20, my2k_persist_error());
    }
    if (op == 4) {
        p[6] = engine.mode;
        p[7] = engine.count;
        memcpy(p + 8, engine.raw, 4);
        put(p + 12, engine.samples);
        put(p + 16, engine.pulses);
        put(p + 20, engine.dropped);
        put(p + 24, engine.last_sample);
        p[28] = engine.contact;
        p[29] = allowed;
        p[30] = (uint8_t)engine.position;
        p[31] = (uint8_t)(engine.position >> 8);
    }
    if (op == 5) {
        p[6] = !engine.contact ? 0 : engine.moved ? 2 : engine.candidate_direction ? 3 : 1;
        p[7] = engine.tap_jitter;
        put(p + 8, engine.taps);
        put(p + 12, engine.swipes);
        put(p + 16, engine.rejected);
        put(p + 20, engine.candidate_at);
        p[24] = (uint8_t)((unsigned)engine.tap_jitter * 384u / 100u);
        p[25] = (uint8_t)(((unsigned)engine.tap_jitter * 384u / 100u) >> 8);
    }
    p[2] = 170;
    return true;
#undef FAIL
}

bool my2k_touchbar_preserve_legacy(void) {
    uint8_t saved[16], verify[16];
    eeprom_read_block(saved, ADDRESS, 16);
    if (my2k_touch_config_valid(saved))
        return true;
    eeprom_read_block(saved, LEGACY_ADDRESS, 16);
    if (!my2k_touch_config_upgrade(saved))
        return true;
    eeprom_write_block(saved, ADDRESS, 16);
    eeprom_read_block(verify, ADDRESS, 16);
    return !my2k_persist_error() && !memcmp(saved, verify, 16);
}
