/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AUX_STATE_H
#define AUX_STATE_H
#include "my2k_wire.h"
typedef struct {
    uint8_t  leds, layer, temperature, humidity;
    uint16_t year;
    uint8_t  datetime[6];
    uint32_t key_events;
    bool     panel_ready;
} aux_state;
/* RAM-only data dispatch. main.c routes SCR/PWR to aux_control separately;
 * passing screen commands here still fails closed. */
bool aux_state_apply(aux_state *state, const my2k_wire_message *message, bool *reply);
#endif
