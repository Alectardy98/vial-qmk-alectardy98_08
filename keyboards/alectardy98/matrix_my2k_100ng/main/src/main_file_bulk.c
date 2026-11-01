/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_file_bulk.h"
#include <string.h>
/* Scoped to the unique media worker. The existing LUN semaphore and complete
 * BOT/CSW/recovery implementation remain authoritative. A normal WRITE10 is
 * never redirected outside this explicit negotiated vendor operation. */
static bool     armed;
static uint8_t  command[16];
static uint16_t wire_bytes;
static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
void my2k_file_bulk_cbw(usbh_ep_t *ep, void *data, uint32_t n) {
    if (!armed || ep->in || !data || n != 31)
        return;
    uint8_t *p       = data;
    bool     reading = command[0] == 0xc2 || command[0] == 0xc3;
    if (word(p) != 0x43425355u || word(p + 8) != wire_bytes || p[12] != (reading ? 128 : 0) || p[13] || p[14] != 10 ||
        p[15] != (reading ? 0x28 : 0x2a) || word(p + 17) || p[22] || p[23] != (wire_bytes / 512u))
        return;
    p[14] = 16;
    memcpy(p + 15, command, 16);
}
bool my2k_file_bulk_write(USBHMassStorageLUNDriver *lun, uint8_t *data, unsigned n, unsigned logical, uint8_t codec,
                          uint8_t slot, uint8_t token, uint32_t at) {
    if (armed || !lun || !data || !n || n > 4096 || !logical || logical > 4096 || codec > 1 || n > logical ||
        (!codec && n != logical) || !slot || slot > 10 || !token || !usbhmsdLUNWriteProfile(lun))
        return false;
    wire_bytes = (uint16_t)((n + 511u) & ~511u);
    memset(data + n, 0, wire_bytes - n);
    memset(command, 0, 16);
    command[0] = 0xc1;
    memcpy(command + 1, "AFB", 3);
    command[4] = 1;
    command[5] = slot;
    command[6] = token;
    command[7] = codec;
    for (unsigned i = 0; i < 4; i++) {
        command[8 + i] = (uint8_t)(at >> (8 * i));
    }
    command[12] = (uint8_t)logical;
    command[13] = (uint8_t)(logical >> 8);
    if (codec) {
        command[14] = (uint8_t)n;
        command[15] = (uint8_t)(n >> 8);
    }
    armed     = true;
    bool okay = usbhmsdLUNWrite(lun, 0, data, wire_bytes / 512u) == HAL_SUCCESS;
    armed     = false;
    return okay;
}

bool my2k_file_bulk_read(USBHMassStorageLUNDriver *lun, uint8_t *data, unsigned n, uint8_t slot, uint8_t token,
                         uint32_t at) {
    if (armed || !lun || !data || !n || n > 4064 || slot > 10 || !token || !usbhmsdLUNWriteProfile(lun))
        return false;
    wire_bytes = (uint16_t)((32u + n + 511u) & ~511u);
    memset(command, 0, 16);
    command[0] = 0xc2;
    memcpy(command + 1, "AFR", 3);
    command[4] = 1;
    command[5] = slot;
    command[6] = token;
    for (unsigned i = 0; i < 4; i++) {
        command[8 + i] = (uint8_t)(at >> (8 * i));
    }
    command[12] = (uint8_t)n;
    command[13] = (uint8_t)(n >> 8);
    armed       = true;
    bool okay   = usbhmsdLUNRead(lun, 0, data, wire_bytes / 512u) == HAL_SUCCESS;
    armed       = false;
    return okay;
}

/* One negotiated BOT transaction instead of prepare/poll/prepared-read. */
bool my2k_file_direct_read(USBHMassStorageLUNDriver *lun, uint8_t *data, unsigned n, uint8_t slot, uint8_t token,
                           uint32_t at) {
    if (armed || !lun || !data || !n || n > 4096 || slot > 10 || !token || !usbhmsdLUNWriteProfile(lun))
        return false;
    wire_bytes = (uint16_t)((n + 511u) & ~511u);
    memset(command, 0, 16);
    command[0] = 0xc3;
    memcpy(command + 1, "AFD", 3);
    command[4] = 1;
    command[5] = slot;
    command[6] = token;
    for (unsigned i = 0; i < 4; i++)
        command[8 + i] = (uint8_t)(at >> (8 * i));
    command[12] = (uint8_t)n;
    command[13] = (uint8_t)(n >> 8);
    armed       = true;
    bool okay   = usbhmsdLUNRead(lun, 0, data, wire_bytes / 512u) == HAL_SUCCESS;
    armed       = false;
    return okay;
}
