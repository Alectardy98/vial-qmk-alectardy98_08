// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include "my2k_wire.h"
#ifndef MY2K_AUX_LINK_MANAGED
#    define MY2K_AUX_LINK_MANAGED 0
#endif
#ifndef MY2K_AUX_STARTUP_CONTROL_ENABLED
#    define MY2K_AUX_STARTUP_CONTROL_ENABLED 0
#endif
bool my2k_aux_start(void);
/* Pre-init only: bounded 11ms/2ms reset, before QMK rebases its clock. */
void my2k_aux_prepare(void);
/* Physical startup reset must finish before the internal host enumerates AUX. */
bool my2k_aux_startup_ready(void);
void my2k_aux_startup_status(uint8_t report[32]);
bool my2k_aux_send(const my2k_wire_message *message);
void my2k_aux_poll(void);
/* Release service: finite GPIO startup, status probing and UART work. Time is
 * caller supplied, including across uint32 wrap. Never waits for AUX readiness.
 * Use this (rather than poll alone) when MY2K_AUX_LINK_MANAGED is enabled. */
void my2k_aux_service(uint32_t now_ms);
/* A recent STS response establishes control-path responsiveness only: there is
 * no transaction ID, storage-ready guarantee or display-ready guarantee. */
bool my2k_aux_responsive(void);
void my2k_aux_on_message(const my2k_wire_message *message);

/* Coalesced current status; accepted queue entries are retried only when needed.
 * No ACK or delivery confirmation is inferred from UART enqueue success. */
void my2k_aux_forward_status(uint8_t host_leds, uint8_t highest_layer);
/* Latest requested display power is retried on bounded polls until queued.
 * Stock PWR has no ACK: queue success never establishes AUX delivery. */
void my2k_aux_request_power(bool on);
/* Stock typing overlay: basic HID key press -> character, OFF after >5000 ms
 * input inactivity. Full-queue character events are dropped, never replayed;
 * OFF is retried until queued. These calls never start the UART. */
void my2k_aux_forward_key(uint16_t keycode, bool pressed, uint8_t mods, uint8_t host_leds);
void my2k_aux_typing_idle(uint32_t elapsed_ms);
/* Caller invokes on independently observed disconnect; drops stale queued data
 * and forces both current status values to be resent on subsequent polling. */
void my2k_aux_forget_state(void);
typedef struct {
    uint32_t tx_overflow, tx_invalid, rx_invalid, rx_frames;
    uint32_t tx_unresponsive, response_timeouts, response_failures, link_responses;
} my2k_aux_diagnostics;
my2k_aux_diagnostics my2k_aux_get_diagnostics(void);
