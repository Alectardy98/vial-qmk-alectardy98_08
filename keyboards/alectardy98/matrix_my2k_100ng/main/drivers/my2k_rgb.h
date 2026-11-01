/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_RGB_H
#define MY2K_RGB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MY2K_RGB_COUNT 128u
#define MY2K_RGB_STRIPS 7u
typedef struct {
    uint8_t r, g, b;
} my2k_rgb_color;
typedef struct {
    uint8_t h, s, v, flags;
} my2k_rgb_config;
typedef struct {
    uint8_t type, address_byte, chip, count;
} my2k_rgb_driver;
typedef struct {
    uint8_t start, count;
} my2k_rgb_strip;
typedef struct {
    uint8_t driver, local, family_index, r, g, b;
} my2k_rgb_channel;
extern const my2k_rgb_channel my2k_rgb_channels[MY2K_RGB_COUNT];
extern const my2k_rgb_driver  my2k_rgb_drivers[6];
extern const my2k_rgb_strip   my2k_rgb_strips[MY2K_RGB_STRIPS];
typedef struct {
    my2k_rgb_config config[MY2K_RGB_COUNT];
    my2k_rgb_color  rendered[MY2K_RGB_COUNT];
    uint32_t        revision;
} my2k_rgb_state;
/* Hardware facts: address_byte is the original 8-bit HAL address; callers of
 * QMK's 7-bit API must convert explicitly. Channel maps/current limits are NOT
 * invented here. Physical flush requires an independently verified backend. */
typedef bool (*my2k_rgb_write_fn)(void *ctx, const my2k_rgb_driver *driver, uint8_t local, my2k_rgb_color color);
void my2k_rgb_init(my2k_rgb_state *state);
bool my2k_rgb_location(uint16_t led, const my2k_rgb_driver **driver, uint8_t *local);
bool my2k_rgb_set_config(my2k_rgb_state *state, uint16_t led, my2k_rgb_config config);
bool my2k_rgb_set_rendered(my2k_rgb_state *state, uint16_t start, const my2k_rgb_color *colors, size_t count);
bool my2k_rgb_snapshot(const my2k_rgb_state *state, uint16_t start, size_t count, uint8_t *rgb, size_t capacity);
/* Independently written per-LED custom HSV/blink/breath preview renderer.
 * This is not a claim of exact stock named-effect parity. flags: on bit0,
 * dynamic1,blink2,breath3,speed highnibble. Read/preview never persists. */
my2k_rgb_color my2k_rgb_hsv(uint8_t h, uint8_t s, uint8_t v);
my2k_rgb_color my2k_rgb_custom_color(my2k_rgb_config config, uint32_t now_ms);
void           my2k_rgb_render_custom(my2k_rgb_state *state, uint32_t now_ms);
bool my2k_rgb_flush(const my2k_rgb_state *state, my2k_rgb_write_fn write, void *ctx, bool channel_map_verified);
#endif
