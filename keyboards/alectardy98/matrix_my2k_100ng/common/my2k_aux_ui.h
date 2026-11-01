/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "my2k_wire.h"
/* Callbacks return acceptance, not a claim that a panel has visibly refreshed.
 * apply_mode atomically owns queueing SCR:AM and committing its RAM/NVM mode.
 * Never advertise the GUI aux_display flag until clock+file upload work too. */
typedef struct {
    void *context;
    bool (*send)(void *, const my2k_wire_message *);
    bool (*get_mode)(void *, uint8_t *);
    bool (*apply_mode)(void *, uint8_t);
} my2k_aux_ui_backend;
bool my2k_aux_ui_hid(const my2k_aux_ui_backend *, uint8_t report[32], size_t length);
