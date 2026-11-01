/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_touch.h"
#define MY2K_TOUCH_CONFIG_BYTES 16u
#define MY2K_TOUCH_QUEUE 16u
typedef void (*my2k_touch_key_fn)(void *, uint16_t, bool);
typedef struct {
    my2k_touch        hold;
    my2k_touch_key_fn emit;
    void             *context;
    uint16_t          keys[3], queue[MY2K_TOUCH_QUEUE], output;
    uint32_t          edge_at, contact_at, samples, pulses, dropped, last_sample, candidate_at;
    uint32_t          taps, swipes, rejected;
    int16_t           position, anchor;
    uint8_t           mode, sensitivity, head, count, raw[4], tap_jitter;
    int8_t            candidate_direction;
    bool              contact, moved, pulse_down;
} my2k_touch_engine;
bool     my2k_touch_key_valid(uint16_t);
uint32_t my2k_touch_crc(const uint8_t *, unsigned);
void     my2k_touch_defaults(uint8_t *);
bool     my2k_touch_config_valid(const uint8_t *);
/* Upgrade a valid TCH1 record in RAM; never writes/changes the old EEPROM slot. */
bool my2k_touch_config_upgrade(uint8_t *);
void my2k_touch_engine_init(my2k_touch_engine *, my2k_touch_key_fn, void *);
void my2k_touch_engine_cancel(my2k_touch_engine *);
void my2k_touch_engine_configure(my2k_touch_engine *, const uint8_t *);
void my2k_touch_engine_sample(my2k_touch_engine *, const uint8_t *, uint32_t);
void my2k_touch_engine_task(my2k_touch_engine *, uint32_t);
