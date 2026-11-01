/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_aux_ui.h"
#include <string.h>
bool my2k_aux_ui_hid(const my2k_aux_ui_backend *b, uint8_t r[32], size_t n) {
    if (!b || !r || n != 32 || r[0] != 0xfd)
        return false;
    uint8_t command = r[1];
    if (command != 42 && command != 54 && command != 55 && command != 58 && command != 59)
        return false;
    uint8_t request[30];
    memcpy(request, r + 2, 30);
    memset(r + 2, 0, 30);
    r[2] = 0x55;
    if (command == 54)
        return true; /* Stock08023DBC explicitly rejects GET_DATETIME. */
    if (command == 42) {
        if (request[0] <= 1)
            r[2] = 0xaa;
        return true;
    } /* stock play/pause hooks are empty */
    if (command == 58) {
        uint8_t mode;
        if (b->get_mode && b->get_mode(b->context, &mode) && mode <= 3) {
            r[3] = mode;
            r[2] = 0xaa;
        }
        return true;
    }
    if (command == 59) {
        if (request[0] <= 3 && b->apply_mode && b->apply_mode(b->context, request[0]))
            r[2] = 0xaa;
        return true;
    }
    my2k_wire_message message = {.type = MY2K_TM, .year = (uint16_t)((uint16_t)request[0] << 8 | request[1])};
    for (unsigned i = 0; i < 6; i++)
        message.v[i] = request[2 + i];
    char validation[MY2K_WIRE_MAX];
    if (message.year > 9999 || my2k_wire_encode(&message, validation, sizeof validation) < 0)
        return true;
    if (b->send && b->send(b->context, &message))
        r[2] = 0xaa;
    return true;
}
