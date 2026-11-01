// SPDX-License-Identifier: GPL-2.0-or-later
#include "hal.h"
#include "aux_link.h"
#if MY2K_DEV21_PROFILE
#include "main_rgb_first.h"
#endif
#include <string.h>
#define QUEUE_SIZE 512u
static bool                 active;
static uint8_t              tx[QUEUE_SIZE];
static uint16_t             head, tail;
static my2k_wire_parser     parser;
static bool                 leds_valid, layer_valid;
static uint8_t              sent_leds, sent_layer;
static bool                 power_requested, power_desired, power_valid, sent_power;
static bool                 typing_active;
static my2k_aux_diagnostics diagnostics;
#if MY2K_AUX_LINK_MANAGED
#    define RESPONSE_MS 250u
#    define RETRY_BACKOFF_MS 1000u
#    define HEARTBEAT_MS 2000u
static bool responding, waiting, backoff;
static uint8_t desired_leds, desired_layer, pending_type, pending_value, attempts;
static uint32_t service_now, requested_at, responded_at;
#endif
#if MY2K_AUX_STARTUP_CONTROL_ENABLED
static uint8_t control_stage;
static uint32_t control_at;
#endif
void                        my2k_aux_forget_state(void) {
    leds_valid = layer_valid = false;
    power_valid = false;
    typing_active = false;
    head = tail = 0;
    my2k_wire_init(&parser);
#if MY2K_AUX_LINK_MANAGED
    responding = waiting = backoff = false;
    attempts = 0;
#endif
}
my2k_aux_diagnostics my2k_aux_get_diagnostics(void) {
    return diagnostics;
}

static const SerialConfig  configuration = {MY2K_UART_BAUD, 0, 0, 0};
__attribute__((weak)) void my2k_aux_on_message(const my2k_wire_message *message) {
    (void)message;
}
bool my2k_aux_start(void) {
    if (active) return true;
    /* Recovered USART1 PA9/PA10 AF7; no reset or peripheral power pin guessed. */
    palSetPadMode(GPIOA, 9, PAL_MODE_ALTERNATE(7));
    palSetPadMode(GPIOA, 10, PAL_MODE_ALTERNATE(7));
    sdStart(&SD1, &configuration);
    my2k_aux_forget_state();
    active = true;
    return true;
}
__attribute__((noinline,noclone,externally_visible)) bool my2k_aux_startup_ready(void) {
#if MY2K_AUX_STARTUP_CONTROL_ENABLED
    return active && control_stage == 3;
#else
    return active;
#endif
}
void my2k_aux_startup_status(uint8_t report[32]) {
    report[8] = active;
#if MY2K_AUX_STARTUP_CONTROL_ENABLED
    report[9] = control_stage;
    for(unsigned i=0;i<4;i++)report[12+i]=(uint8_t)(control_at>>(8*i));
#endif
    report[10] = my2k_aux_startup_ready();
}
static bool queue_message(const my2k_wire_message *message) {
#if MY2K_DEV21_PROFILE
    my2k_first_event(FIRST_AUX,0,0,(uint8_t)message->type,8,active,0,0,message->v);
#endif
    if (!active) return false;
    char frame[MY2K_WIRE_MAX];
    int  length = my2k_wire_encode(message, frame, sizeof frame);
    if (length <= 0) {
        diagnostics.tx_invalid++;
        return false;
    }
    unsigned free_bytes = (tail + QUEUE_SIZE - head - 1u) % QUEUE_SIZE;
    if ((unsigned)length > free_bytes) {
        diagnostics.tx_overflow++;
        return false;
    }
    for (int i = 0; i < length; ++i) {
        tx[head] = (uint8_t)frame[i];
        head     = (head + 1u) % QUEUE_SIZE;
    }
    return true;
}
bool my2k_aux_responsive(void) {
#if MY2K_AUX_LINK_MANAGED
    return active && responding;
#else
    return active;
#endif
}
bool my2k_aux_send(const my2k_wire_message *message) {
#if MY2K_AUX_LINK_MANAGED
    /* Transient key/overlay traffic must not accumulate during AUX startup or
     * failure. Current LED/layer/power state has separate coalesced storage. */
    if (!my2k_aux_responsive()) {
        diagnostics.tx_unresponsive++;
        return false;
    }
#endif
    return queue_message(message);
}
void my2k_aux_request_power(bool on) {
    power_requested = true;
    power_desired = on;
}
void my2k_aux_forward_key(uint16_t keycode, bool pressed, uint8_t mods, uint8_t host_leds) {
    if (!active || !pressed) return;
    /* Independent range implementation of observed main 08022a48/table
     * 0803b5ae. This is the stock overlay's mapping, not host-layout text:
     * Caps Lock XOR Shift selects either column for every mapped key; the
     * upper column intentionally leaves digits 5..0 unchanged. */
    bool upper = ((host_leds & 2u) != 0) ^ ((mods & 0x22u) != 0);
    uint8_t character = 0;
    if (keycode >= 4 && keycode <= 29)
        character = (uint8_t)((upper ? 'A' : 'a') + keycode - 4);
    else if (keycode >= 30 && keycode <= 39)
        character = (uint8_t)(upper ? "!@#$567890" : "1234567890")[keycode - 30];
    else if (keycode >= 0x2d && keycode <= 0x38)
        character = (uint8_t)(upper ? "_+{}|~:\"~<>?" : "-=[]\\#;'`,./")[keycode - 0x2d];
    else if (keycode >= 0x54 && keycode <= 0x57)
        character = (uint8_t)"/*-+"[keycode - 0x54];
    else if (keycode >= 0x59 && keycode <= 0x61)
        character = (uint8_t)('1' + keycode - 0x59);
    else if (keycode == 0x62) character = '0';
    else if (keycode == 0x63) character = '.';
    if (character) {
        my2k_wire_message message = {.type = MY2K_TX, .v = {character}};
        if (my2k_aux_send(&message)) typing_active = true;
    }
}
void my2k_aux_typing_idle(uint32_t elapsed_ms) {
    if (!typing_active || elapsed_ms <= 5000) return;
    my2k_wire_message message = {.type = MY2K_TX};
    if (my2k_aux_send(&message)) typing_active = false;
}
void my2k_aux_poll(void) {
    if (!active) return;
#if MY2K_AUX_STARTUP_CONTROL_ENABLED
    if (control_stage != 3) return;
#endif
    /* Bounded, immediate operations keep the QMK matrix path non-blocking. */
    for (unsigned i = 0; i < 16; ++i) {
        msg_t value = sdGetTimeout(&SD1, TIME_IMMEDIATE);
        if (value < MSG_OK) break;
        my2k_wire_message message;
        int               result = my2k_wire_feed(&parser, (uint8_t)value, &message);
        if (result == 1) {
            diagnostics.rx_frames++;
#if MY2K_AUX_LINK_MANAGED
            if (message.type == MY2K_STS && waiting) {
                if (message.v[0]) {
                    responding = true;
                    waiting = backoff = false;
                    attempts = 0;
                    responded_at = service_now;
                    diagnostics.link_responses++;
                    if (pending_type == MY2K_LED) {
                        leds_valid = true;
                        sent_leds = pending_value;
                    } else {
                        layer_valid = true;
                        sent_layer = pending_value;
                    }
                } else {
                    /* Let the common timeout path perform bounded recovery. */
                    diagnostics.response_failures++;
                    requested_at = service_now - RESPONSE_MS;
                }
            }
#endif
            my2k_aux_on_message(&message);
        } else if (result < 0)
            diagnostics.rx_invalid++;
    }
    if (my2k_aux_responsive() && power_requested && (!power_valid || sent_power != power_desired)) {
        my2k_wire_message message = {.type = MY2K_PWR, .v = {power_desired}};
        if (my2k_aux_send(&message)) {
            sent_power = power_desired;
            power_valid = true;
        }
    }
    for (unsigned i = 0; i < 16 && head != tail; ++i) {
        if (sdPutTimeout(&SD1, tx[tail], TIME_IMMEDIATE) != MSG_OK) break;
        tail = (tail + 1u) % QUEUE_SIZE;
    }
}

void my2k_aux_service(uint32_t now_ms) {
    if (!active) return;
#if MY2K_AUX_STARTUP_CONTROL_ENABLED
    /* Main 08022dd4: PA8 output/high, delay 10, PB4 output/low,
     * delay 1, PB4 input. Stock HAL delay adds its 1ms tick quantum: use
     * 11ms/2ms elapsed thresholds so scheduling cannot shorten either wait.
     * PB4 is also the loader's active-low target reset;
     * PA8's physical net remains unidentified. Repeat neither on UART timeout.
     * Preserve the observed GPIO order/speed without blocking USB or scanning. */
    if (control_stage == 0) {
        palSetPadMode(GPIOA, 8, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
        /* writepad uses the same 32-bit BSRR access as stock 0802916e. */
        palWritePad(GPIOA, 8, 1);
        control_at = now_ms;
        control_stage = 1;
        return;
    }
    if (control_stage == 1) {
        if ((uint32_t)(now_ms - control_at) < 11u) return;
        palSetPadMode(GPIOB, 4, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_MID2);
        palWritePad(GPIOB, 4, 0);
        control_at = now_ms;
        control_stage = 2;
        return;
    }
    if (control_stage == 2) {
        if ((uint32_t)(now_ms - control_at) < 2u) return;
        palSetPadMode(GPIOB, 4, PAL_MODE_INPUT | PAL_STM32_OSPEED_MID2);
        control_stage = 3;
        my2k_wire_init(&parser);
    }
#endif
#if MY2K_AUX_LINK_MANAGED
    service_now = now_ms;
#else
    (void)now_ms;
#endif
    my2k_aux_poll();
#if MY2K_AUX_LINK_MANAGED
    if (waiting && (uint32_t)(now_ms - requested_at) >= RESPONSE_MS) {
        diagnostics.response_timeouts++;
        responding = waiting = false;
        leds_valid = layer_valid = power_valid = false;
        typing_active = false;
        /* Drop stale events; a newline terminates any partially transmitted
         * frame before retry. UART hardware bytes already queued are finite. */
        head = tail = 0;
        tx[head++] = '\n';
        if (attempts >= 3) {
            backoff = true;
            requested_at = now_ms;
        }
    }
    if (waiting) return;
    if (backoff) {
        if ((uint32_t)(now_ms - requested_at) < RETRY_BACKOFF_MS) return;
        backoff = false;
        attempts = 0;
    }
    my2k_wire_message message = {0};
    bool refresh = (uint32_t)(now_ms - responded_at) >= HEARTBEAT_MS;
    if (!leds_valid || sent_leds != desired_leds || refresh) {
        message.type = MY2K_LED;
        message.v[0] = desired_leds;
    } else if (!layer_valid || sent_layer != desired_layer) {
        message.type = MY2K_LYR;
        message.v[0] = desired_layer;
    } else return;
    if (queue_message(&message)) {
        /* A fast AUX reboot can occur between probes without timing out.
         * Reconcile both idempotent status values, not only the LED heartbeat.
         * Do not periodically replay PWR: stock ON also resets mono AM mode. */
        if (refresh) layer_valid = false;
        pending_type = message.type;
        pending_value = message.v[0];
        requested_at = now_ms;
        waiting = true;
        attempts++;
    }
#endif
}

void my2k_aux_forward_status(uint8_t leds, uint8_t layer) {
    if (!active) return;
#if MY2K_AUX_LINK_MANAGED
    desired_leds = leds;
    desired_layer = layer;
#else
    if (!leds_valid || sent_leds != leds) {
        my2k_wire_message message = {.type = MY2K_LED, .v = {leds}};
        if (my2k_aux_send(&message)) {
            sent_leds  = leds;
            leds_valid = true;
        }
    }
    if (!layer_valid || sent_layer != layer) {
        my2k_wire_message message = {.type = MY2K_LYR, .v = {layer}};
        if (my2k_aux_send(&message)) {
            sent_layer  = layer;
            layer_valid = true;
        }
    }
#endif
}
