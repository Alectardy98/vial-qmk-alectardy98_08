/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_TOUCH_H
#define MY2K_TOUCH_H
#include <stdbool.h>
#include <stdint.h>
#define MY2K_TOUCH_THRESHOLD 64u
#define MY2K_TOUCH_HOLD_MS 1000u
#define MY2K_CONSUMER_MUTE 0x00e2u
#define MY2K_CONSUMER_VOLUME_UP 0x00e9u
#define MY2K_CONSUMER_VOLUME_DOWN 0x00eau
typedef enum { MY2K_TOUCH_REPORT, MY2K_TOUCH_MUTE_TAP } my2k_touch_event;
/* REPORT is the complete consumer usage (0 releases). MUTE_TAP delegates tap
 * scheduling to the host/backend; no stock configurable tap duration invented. */
typedef void (*my2k_touch_emit_fn)(void *context, my2k_touch_event event, uint16_t usage);
typedef enum { MY2K_TOUCH_IDLE, MY2K_TOUCH_DOWN, MY2K_TOUCH_UP, MY2K_TOUCH_MIDDLE, MY2K_TOUCH_LEFT_PENDING, MY2K_TOUCH_RIGHT_PENDING } my2k_touch_state;
typedef struct {
    my2k_touch_state   state;
    uint8_t            raw[8];
    uint32_t           contact_ms;
    bool               hold_sent;
    uint16_t           consumer;
    my2k_touch_emit_fn emit;
    void              *context;
} my2k_touch;
void my2k_touch_init(my2k_touch *touch, my2k_touch_emit_fn emit, void *context);
/* Eight raw TC bytes; only first4 are gesture inputs in verified stock behavior.
 * NULL input performs a timer tick. Run in task context, not a UART ISR. */
void my2k_touch_update(my2k_touch *touch, const uint8_t raw[8], uint32_t now_ms);
/* Explicit disconnect/reset path releases held consumer state, clears cache.
 * No speculative inactivity timeout or calibration mutation. */
void my2k_touch_reset(my2k_touch *touch);
/* Highest channel strictly above64, first winner on equal values, else-1. */
int my2k_touch_peak(const uint8_t raw[8]);
#endif
