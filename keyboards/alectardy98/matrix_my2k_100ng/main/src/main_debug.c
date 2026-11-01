/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_debug.h"
#include "main_file_bulk.h"
#include QMK_KEYBOARD_H
#include "hal.h"
#include "main_eeprom.h"
#include "main_storage_mode.h"
#include "main_oled_config.h"
#include "my2k_usbh_chibios.h"
#include "main_keycodes.h"
#include "dynamic_keymap.h"
#include "aux_link.h"
#include "timer.h"
#include <string.h>
#define API __attribute__((noinline, noclone, externally_visible))
#define RECORDS 16u
static uint8_t  trace[RECORDS][32], latched[32];
static uint8_t  used, head, latched_index, pending, last_cdb;
static bool     frozen, latched_valid;
static uint32_t transfers, sequence, command_failures, requested_at, trace_epoch;
static uint8_t  irq_snapshot[96];
static bool     irq_captured;
static uint32_t irq_errors;
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(n >> (8 * i));
}
static void put16(uint8_t *p, uint32_t n) {
    if (n > 65535)
        n = 65535;
    p[0] = n;
    p[1] = n >> 8;
}
/* Called before the LLD acknowledges host-channel status. No peripheral write,
 * FIFO read, recovery or ISR logging transport. Preserve the first hard error. */
API void my2k_debug_host_irq(void *instance, unsigned channel, uint32_t bits) {
    USBHDriver *host = instance;
    if (!(bits & (HCINTMSK_AHBERRM | HCINTMSK_TRERRM | HCINTMSK_BBERRM | HCINTMSK_FRMORM | HCINTMSK_DTERRM)))
        return;
    irq_errors++;
    if (irq_captured || channel >= host->channels_number)
        return;
    stm32_hc_management_t *hcm = &host->channels[channel];
    stm32_otg_host_chn_t  *hc  = hcm->hc;
    usbh_ep_t             *ep  = hcm->ep;
    uint8_t               *b   = irq_snapshot;
    memset(b, 0, 96);
    b[0] = 1;
    b[1] = channel;
    put32(b + 4, timer_read32());
    put32(b + 8, bits);
    put32(b + 12, hc->HCCHAR);
    put32(b + 16, hc->HCTSIZ);
    put32(b + 20, hc->HCINTMSK);
    put32(b + 24, host->otg->HPRT);
    put32(b + 28, host->otg->GINTSTS);
    put32(b + 32, host->otg->GINTMSK);
    put32(b + 72, RCC->AHB2LPENR);
    put32(b + 76, RCC->AHB2ENR);
    if (ep) {
        b[2]  = 1;
        b[3]  = ep->type;
        b[36] = ep->status;
        b[37] = ep->address;
        b[38] = ep->in;
        b[39] = ep->type == USBH_EPTYPE_CTRL ? ep->xfer.u.ctrl_phase : 255;
        put32(b + 64, ep->dt_mask);
        put32(b + 68, ep->xfer.error_count);
        if (ep->urb_list.next && !list_empty(&ep->urb_list)) {
            usbh_urb_t *urb = list_first_entry(&ep->urb_list, usbh_urb_t, node);
            b[2] |= 2;
            put32(b + 40, urb->requestedLength);
            put32(b + 44, urb->actualLength);
            if (urb->setup_buff)
                memcpy(b + 48, urb->setup_buff, 8);
            if (urb->buff && urb->requestedLength >= 8)
                memcpy(b + 56, urb->buff, 8);
        }
    }
    irq_captured = true;
}
/* Start records are visible while a transfer sleeps. Commit is protected by a
 * short scheduler lock; no lock spans a USB transfer. First transport/phase
 * error freezes the ring; ordinary SCSI CHECK CONDITION remains observable. */
API usbh_urbstatus_t my2k_debug_transfer(usbh_ep_t *ep, void *data, uint32_t n, uint32_t *actual, systime_t timeout) {
    my2k_file_bulk_cbw(ep, data, n);
    uint8_t  r[32] = {0};
    uint32_t began = timer_read32(), got = 0;
    uint8_t *p   = data;
    bool     cbw = !ep->in && n == 31 && p && get32(p) == 0x43425355u;
    if (cbw) {
        last_cdb = p[15];
        put32(r + 16, get32(p + 4));
        put32(r + 24, get32(p + 8));
    }
    r[20] = last_cdb;
    r[21] = cbw ? 1 : (ep->in && n == 13 ? 3 : 2);
    r[22] = USBH_URBSTATUS_PENDING;
    r[23] = ep->in ? 1 : 0;
    put32(r + 4, began);
    put16(r + 12, n);
    chSysLock();
    transfers++;
    uint8_t  slot   = head;
    bool     record = !frozen;
    uint32_t epoch  = trace_epoch;
    if (record) {
        put32(r, ++sequence);
        memcpy(trace[slot], r, 32);
        head = (head + 1u) % RECORDS;
        if (used < RECORDS)
            used++;
    }
    chSysUnlock();
    usbh_urbstatus_t result = usbhSynchronousTransfer(ep, data, n, &got, timeout);
    if (actual)
        *actual = got;
    put32(r + 8, timer_read32() - began);
    put16(r + 14, got);
    r[22]    = (uint8_t)result;
    bool csw = ep->in && n == 13 && got == 13 && p && get32(p) == 0x53425355u;
    if (csw) {
        put32(r + 16, get32(p + 4));
        put32(r + 28, get32(p + 8));
        r[23] |= 2u | (p[12] << 4);
        if (p[12])
            command_failures++;
    }
    chSysLock();
    if (record && epoch == trace_epoch) {
        memcpy(trace[slot], r, 32);
        if (result != USBH_URBSTATUS_OK || (csw && p[12] >= 2))
            frozen = true;
    }
    chSysUnlock();
    return result;
}
static bool idle(void) {
    return !my2k_eeprom32_reset_pending() && !my2k_eeprom_pending() && my2k_eeprom_durable() &&
           !my2k_asset_transition_pending() && !my2k_oled_main_busy() && my2k_usbh_asset_idle();
}
static bool action(unsigned op, uint32_t now) {
    if (pending || my2k_asset_export_active() || !idle())
        return false;
    pending      = op;
    requested_at = now;
    return true;
}
API void my2k_debug_task(uint32_t now) {
    if (!pending)
        return;
    if ((uint32_t)(now - requested_at) > 5000u) {
        pending = 0;
        return;
    }
    if ((uint32_t)(now - requested_at) < 100u || !idle())
        return;
    unsigned op = pending;
    pending     = 0;
    if (op == 4)
        reset_keyboard();
    else
        soft_reset_keyboard();
}
static bool set_storage(bool export_mode, uint32_t now) {
    if (pending || !idle())
        return false;
    if (my2k_asset_export_active() == export_mode)
        return true;
    if (!export_mode)
        return my2k_asset_mode_request(true, now);
    uint32_t blocks;
    uint16_t size;
    if (!my2k_usbh_capacity(&blocks, &size) || !blocks || size != 512)
        return false;
    for (unsigned layer = 0; layer < DYNAMIC_KEYMAP_LAYER_COUNT; layer++)
        for (unsigned row = 0; row < MATRIX_ROWS; row++)
            for (unsigned col = 0; col < MATRIX_COLS; col++)
                if (dynamic_keymap_get_keycode(layer, row, col) == MY2K_STORAGE_TOGGLE)
                    return my2k_asset_mode_request_key(row, col, true, now);
    return false;
}
API bool my2k_debug_hid(uint8_t *r, unsigned n, bool unlocked) {
    if (!r || n != 32)
        return false;
    /* Restore the existing GUI's standard VIA bootloader action for this explicit
     * development profile only. Secure/other keyboard profiles are untouched. */
    if (r[0] == 11) {
        bool valid = unlocked;
        for (unsigned i = 1; i < 32; i++)
            if (r[i])
                valid = false;
        if (!valid || !action(4, timer_read32()))
            r[0] = 255;
        return true;
    }
    if (r[0] != 253 || r[1] != 239)
        return false;
    uint8_t q[32];
    memcpy(q, r, 32);
    memset(r, 0, 32);
    r[0]        = 253;
    r[1]        = 239;
    r[2]        = 85;
    r[3]        = 1;
    r[4]        = q[2];
    r[5]        = q[4];
    unsigned op = q[2];
    if (q[3] != 1 || op > 11 || op == 5)
        return true;
    bool     mutation = op == 3 || op == 4 || op == 6 || op == 7;
    unsigned reserved = mutation || op == 11 ? 10 : (op == 2 ? 7 : op == 8 || op == 10 ? 6 : 5);
    for (unsigned i = reserved; i < 32; i++)
        if (q[i])
            return true;
    if (mutation) {
        if (!unlocked || !q[4] || memcmp(q + 5, "DBG1", 4) || (op != 6 && op != 7 && q[9]))
            return true;
    }
    r[2] = 170;
    r[6] = 2;
    if (!op) {
        memcpy(r + 8, "DBG1", 4);
        put32(r + 12, 0x7ffu);
        r[16] = RECORDS;
        r[17] = 32;
        r[18] = pending;
        r[19] = frozen;
        put32(r + 20, transfers);
        put32(r + 24, command_failures);
        put32(r + 28, sequence);
        return true;
    }
    if (op == 11) {
        /* Development readback only: fixed Main boot/application flash span.
         * No SRAM, peripheral registers, EEPROM, reset, erase or program API. */
        uint32_t       offset = get32(q + 5);
        unsigned       count  = q[9];
        const uint32_t limit =
#if MY2K_GAMES_PROFILE
            0x60000u;
#else
            0x40000u;
#endif
        if (!count || count > 20u || offset > limit - count) {
            r[2] = 85;
            r[6] = 3;
            return true;
        }
        r[7] = (uint8_t)count;
        put32(r + 8, offset);
        const volatile uint8_t *source = (const volatile uint8_t *)(uintptr_t)(0x08000000u + offset);
        for (unsigned i = 0; i < count; i++)
            r[12 + i] = source[i];
        return true;
    }
    if (op == 1) {
        my2k_usbh_diagnostics d;
        my2k_usbh_get_diagnostics(&d);
        r[7]  = d.flags;
        r[8]  = d.host_state;
        r[9]  = d.lun_state;
        r[10] = d.worker_phase;
        r[11] = d.read_state;
        r[12] = d.file_state;
        r[13] = d.last_connect_result;
        r[14] = d.file_command;
        r[15] = (my2k_asset_export_active() ? 1 : 0) | (my2k_asset_transition_pending() ? 2 : 0) |
                (my2k_oled_main_busy() ? 4 : 0) | (my2k_eeprom_pending() ? 8 : 0) | (my2k_aux_responsive() ? 16 : 0);
        put32(r + 16, d.blocks);
        put32(r + 20, my2k_usbh_fault());
        put32(r + 24, d.connect_attempts);
        put32(r + 28, d.connect_failures);
        return true;
    }
    if (op == 2) {
        unsigned index = q[5], part = q[6];
        chSysLock();
        if (index >= used || part > 1) {
            r[6] = 3;
            chSysUnlock();
            return true;
        }
        if (!part || !latched_valid || latched_index != index) {
            unsigned oldest = (head + RECORDS - used) % RECORDS;
            memcpy(latched, trace[(oldest + index) % RECORDS], 32);
            latched_index = index;
            latched_valid = true;
        }
        r[8]  = used;
        r[9]  = RECORDS;
        r[10] = frozen;
        r[11] = part;
        put32(r + 12, get32(latched));
        memcpy(r + 16, latched + part * 16u, 16);
        chSysUnlock();
        return true;
    }
    if (op == 8) {
        if (q[5] >= MY2K_DIAG_PAGES || !my2k_aux_responsive()) {
            r[6] = 4;
            return true;
        }
        my2k_wire_message message = {.type = MY2K_DQ, .v = {q[5]}};
        if (!my2k_aux_send(&message))
            r[6] = 4;
        r[8] = q[5];
        return true; /* queued GET, not completion */
    }
    if (op == 9) {
        my2k_aux_startup_status(r);
        return true;
    }
    if (op == 10) {
        if (q[5] >= 4) {
            r[6] = 3;
            return true;
        }
        chSysLock();
        if (irq_captured)
            memcpy(r + 8, irq_snapshot + q[5] * 24u, 24);
        else
            r[6] = 4;
        chSysUnlock();
        return true;
    }
    if (op == 3 || op == 4) {
        if (!action(op, timer_read32()))
            r[6] = 4;
        return true;
    }
    if (op == 6) {
        if (q[9] > 1 || !my2k_usbh_asset_idle()) {
            r[6] = 4;
            return true;
        }
        chSysLock();
        if (q[9])
            frozen = true;
        else {
            trace_epoch++; /* An older in-flight completion cannot repopulate a cleared ring. */
            irq_captured = false;
            irq_errors   = 0;
            used = head   = 0;
            sequence      = 0;
            frozen        = false;
            latched_valid = false;
        }
        chSysUnlock();
        return true;
    }
    if (op == 7) {
        if (q[9] > 1 || !set_storage(q[9] != 0, timer_read32()))
            r[6] = 4;
        return true;
    }
    return true;
}
