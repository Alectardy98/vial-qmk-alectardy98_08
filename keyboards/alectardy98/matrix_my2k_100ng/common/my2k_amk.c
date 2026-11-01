/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_amk.h"
static uint16_t index_at(const uint8_t *r, bool v2) {
    return (uint16_t)(r[2] | (v2 ? (uint16_t)r[3] << 8 : 0));
}
bool my2k_amk_dispatch(const my2k_amk_backend *b, uint8_t *r, size_t n) {
    if (!b || !r || n != MY2K_AMK_REPORT_SIZE || r[0] != 0xfd)
        return false;
    uint8_t cmd = r[1], request[30];
    for (unsigned i = 0; i < 30; i++)
        request[i] = r[2 + i];
    for (unsigned i = 2; i < MY2K_AMK_REPORT_SIZE; i++)
        r[i] = 0;
    r[2]                = 0x55;
    bool          v2    = b->total_leds > 254;
    uint8_t       value = 0, type = 0;
    my2k_rgb_info info = {0};
    switch (cmd) {
        case 0:
            r[3] = 1;
            break;
        case 7:
        case 9:
        case 11:
        case 13:
        case 58:
            if (!b->scalar || !b->scalar(b->context, cmd, &value))
                return true;
            r[3] = value;
            break;
        case 27:
        case 65:
            type = cmd == 27 ? 1 : 3;
            if (!b->count || !b->count(b->context, type, &value))
                return true;
            r[3] = value;
            break;
        case 28:
        case 66:
            type = cmd == 28 ? 1 : 3;
            if (!b->info || !b->info(b->context, type, request[0], &info))
                return true;
            r[3] = request[0];
            r[4] = info.config;
            r[5] = info.enabled;
            r[6] = info.mode;
            r[7] = info.custom;
            if (type == 3) {
                r[8] = info.rows;
                r[9] = info.cols;
            }
            break;
        case 31:
        case 67:
            type = cmd == 31 ? 1 : 3;
            if (!b->info || !b->info(b->context, type, request[0], &info))
                return true;
            r[3] = request[0];
            r[4] = info.mode;
            break;
        case 29:
        case 33:
        case 69: {
            type         = cmd == 29 ? 1 : cmd == 33 ? 2 : 3;
            uint16_t idx = (uint16_t)(request[0] | (v2 ? (uint16_t)request[1] << 8 : 0));
            if (type != 2 && idx >= b->total_leds)
                return true;
            uint8_t led[4] = {0};
            if (!b->led || !b->led(b->context, type, idx, led))
                return true;
            r[3]         = (uint8_t)idx;
            unsigned off = 4;
            if (v2) {
                r[4] = (uint8_t)(idx >> 8);
                off  = 5;
            }
            for (unsigned i = 0; i < 4; i++)
                r[off + i] = led[i];
            break;
        }
        case 60: {
            uint8_t  input[4] = {0xfd, cmd, request[0], request[1]};
            uint16_t start    = index_at(input, v2);
            uint8_t  count    = request[v2 ? 2 : 1];
            unsigned limit    = v2 ? 8 : 9;
            if (!count || count > limit || start >= b->total_leds || count > b->total_leds - start || !b->rendered)
                return true;
            uint8_t colors[27];
            for (unsigned i = 0; i < count; i++)
                if (!b->rendered(b->context, (uint16_t)(start + i), colors + i * 3))
                    return true;
            r[3]         = (uint8_t)start;
            unsigned off = 5;
            if (v2) {
                r[4] = (uint8_t)(start >> 8);
                r[5] = count;
                off  = 6;
            } else
                r[4] = count;
            for (unsigned i = 0; i < (unsigned)count * 3; i++)
                r[off + i] = colors[i];
            break;
        }
        case 61: {
            type      = request[0];
            uint8_t p = request[1], index = request[2], out[3] = {0};
            if (type > 3 || p > 5 || !b->param || !b->param(b->context, type, p, index, out))
                return true;
            r[3]           = p;
            unsigned count = p <= 1 ? 3 : 1;
            for (unsigned i = 0; i < count; i++)
                r[4 + i] = out[i];
            break;
        }
        case 8:
        case 10:
        case 12:
        case 14:
        case 59:
            if (!b->ram_writes || !b->ram_writes->scalar || (cmd == 14 && request[0] > 1) ||
                !b->ram_writes->scalar(b->context, cmd, request[0]))
                return true;
            break;
        case 30:
        case 34:
        case 70: {
            type           = cmd == 30 ? 1 : cmd == 34 ? 2 : 3;
            uint16_t index = (uint16_t)(request[0] | (v2 ? (uint16_t)request[1] << 8 : 0));
            if ((type != 2 && index >= b->total_leds) || !b->ram_writes || !b->ram_writes->led ||
                !b->ram_writes->led(b->context, type, index, request + (v2 ? 2 : 1)))
                return true;
            break;
        }
        case 32:
        case 68:
            type = cmd == 32 ? 1 : 3;
            if (!b->ram_writes || !b->ram_writes->mode ||
                !b->ram_writes->mode(b->context, type, request[0], request[1]))
                return true;
            break;
        case 62: {
            type          = request[0];
            uint8_t p     = request[1];
            size_t  count = p <= 1 ? 3 : 1;
            if (type > 3 || p > 5 || !b->ram_writes || !b->ram_writes->param ||
                !b->ram_writes->param(b->context, type, p, request[2], request + 3, count))
                return true;
            break;
        }
        default:
            return true;
    }
    r[2] = 0xaa;
    return true;
}
