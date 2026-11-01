/* SPDX-License-Identifier: GPL-2.0-or-later */
#if MY2K_DEV12_PROFILE
#    include "main_host_events.h"
#    if MY2K_DEV14_PROFILE
#        include "my2k_usb_msc.h"
#    endif
#endif
#if MY2K_DEV11_PROFILE
#    include "main_scheduler.h"
#endif
#if MY2K_DEV10_PROFILE
#    include "main_host_trace.h"
#endif
#include QMK_KEYBOARD_H
#include "main_diag.h"
#if MY2K_DEV21_PROFILE
#    include "main_rgb_first.h"
#endif
#if MY2K_DEV16_PROFILE
#    include "main_input_performance.h"
#    include "main_input_scheduler.h"
#    if MY2K_DEV17_PROFILE
#        include "main_rgb_observer.h"
#        if MY2K_DEV18_PROFILE
#            include "main_rgb_trace.h"
#        endif
#    endif
#endif
#if MY2K_DEV8_PROFILE
#    include "main_button.h"
#    include "main_host_mode.h"
#endif
#include "main_eeprom.h"
#include "main_persist.h"
#include "my2k_qmk_rgb.h"
#include "aux_link.h"
#include "dynamic_keymap.h"
#include "vial.h"
#if MY2K_DEV2_PROFILE
#    include "main_rgb_bus.h"
#endif
#include <string.h>
#if MY2K_DEV7_PROFILE
#    include "main_storage_mode.h"
#endif
#if MY2K_DEV6_PROFILE
#    include "main_aux_telemetry.h"
#endif
#if MY2K_STOCK_PARITY_PROFILE
#    include "my2k_usbh_chibios.h"
#endif
#if MY2K_DEV4_PROFILE
#    include "main_perf.h"
#    include "my2k_rgb_refresh.h"
#endif
_Static_assert(sizeof(MY2K_DEV_BUILD) <= 25, "Diagnostic build identifier must fit its page");
static uint32_t attempts, accepted, events;
static uint8_t  set_layer, set_row, set_col, key_row, key_col, key_down;
static uint16_t wanted, actual, key_code;
static void     put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (i * 8));
}
void my2k_diag_set_attempt(const uint8_t *p) {
    attempts++;
    set_layer = p[1];
    set_row   = p[2];
    set_col   = p[3];
    wanted    = ((uint16_t)p[4] << 8) | p[5];
}
void my2k_diag_set_complete(void) {
    actual = dynamic_keymap_get_keycode(set_layer, set_row, set_col);
    if (actual == wanted)
        accepted++;
}
void my2k_diag_key(uint8_t row, uint8_t col, uint16_t code, bool pressed) {
    events++;
    key_row  = row;
    key_col  = col;
    key_code = code;
    key_down = pressed;
}
bool my2k_diag_reply(uint8_t *p) {
#if MY2K_DEV21_PROFILE
    if (p[0] == 0xfd && p[1] == 0xf4) {
        bool ok = p[2] == 1 && p[3] == 1;
        for (unsigned i = 4; i < 32; i++)
            if (p[i])
                ok = false;
        if (ok)
            my2k_first_rearm();
        memset(p, 0, 32);
        p[0] = 0xfd;
        p[1] = 0xf4;
        p[2] = 1;
        p[3] = ok ? 1 : 255;
        return true;
    }
#endif
#if MY2K_DEV3_PROFILE
    if (p[0] == 0xfd && p[1] == 0xf1) {
        uint8_t  action = p[2], group = p[3], color = p[4];
        uint16_t duration = p[5] | (uint16_t)p[6] << 8;
        bool     ok       = action == 0;
        if (action == 1)
            ok = my2k_qmk_rgb_test_start(group, color, duration, timer_read32());
        if (action == 2) {
            my2k_qmk_rgb_test_cancel();
            ok = true;
        }
        memset(p, 0, 32);
        p[0] = 0xfd;
        p[1] = 0xf1;
        p[2] = action;
        p[3] = ok ? 1 : 0xff;
        p[4] = my2k_qmk_rgb_test_group();
        return true;
    }
#endif
    if (p[0] != 0xfd || p[1] != 0xf0)
        return false;
    uint8_t page = p[2];
#if MY2K_DEV3_PROFILE
    uint8_t index = p[3];
#endif
    memset(p, 0, 32);
    p[0] = 0xfd;
    p[1] = 0xf0;
    p[2] = page;
    p[3] = 1;
    switch (page) {
#if MY2K_DEV21_PROFILE
        case 45:
        case 46:
        case 47:
        case 48:
        case 49:
        case 50:
            if (!my2k_first_page(page, index, p))
                p[3] = 255;
            break;
#endif
#if MY2K_DEV19_PROFILE
        case 43:
        case 44:
            if (!my2k_rgb_bus_hw_page(page, index, p))
                p[3] = 255;
            break;
#endif
#if MY2K_DEV17_PROFILE
#    if MY2K_DEV18_PROFILE
        case 41:
        case 42:
            if (!my2k_rgb_trace_page(page, index, p))
                p[3] = 255;
            break;
        case 36:
        case 37:
        case 38:
        case 39:
        case 40:
#    endif
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
            if (!my2k_rgb_observer_page(page, index, p))
                p[3] = 255;
            break;
#endif
        case 0:
            memcpy(p + 4, MY2K_DEV_BUILD, sizeof(MY2K_DEV_BUILD) - 1);
            p[28] = my2k_eeprom_durable();
            p[29] = my2k_eeprom_pending();
            p[30] = my2k_aux_responsive();
            p[31] = 1;
            break;
        case 1:
            put32(p + 4, attempts);
            put32(p + 8, accepted);
            put32(p + 12, events);
            p[16] = set_layer;
            p[17] = set_row;
            p[18] = set_col;
            p[19] = vial_unlock_in_progress;
            p[20] = wanted >> 8;
            p[21] = wanted;
            p[22] = actual >> 8;
            p[23] = actual;
            p[24] = key_row;
            p[25] = key_col;
            p[26] = key_code >> 8;
            p[27] = key_code;
            p[28] = key_down;
            p[29] = (uint8_t)layer_state;
            p[30] = (uint8_t)default_layer_state;
            p[31] = vial_unlocked;
            break;
        case 2:
#if MY2K_DEV_PERSISTENCE
            put32(p + 4, my2k_persist_commits());
            put32(p + 8, my2k_persist_erases());
            put32(p + 12, my2k_persist_error());
#    if MY2K_EEPROM32_PROFILE
            put32(p + 16, 0x080c0000);
            put32(p + 20, 0x40000);
#    else
            put32(p + 16, 0x08010000);
            put32(p + 20, 0x10000);
#    endif
            p[24] = my2k_eeprom_pending();
            p[25] = my2k_persist_busy();
            p[26] = my2k_eeprom_last_result();
            p[27] = 1;
            p[28] = 7; /* MYJ1 format; seven append snapshots per sector */
#else
            /* Zero capacity/base explicitly identifies volatile storage. */
            p[24] = my2k_eeprom_pending();
            p[26] = my2k_eeprom_last_result();
#endif
            break;
        case 3: {
            my2k_aux_diagnostics d = my2k_aux_get_diagnostics();
            p[4]                   = my2k_qmk_rgb_present();
            p[5]                   = my2k_qmk_rgb_families();
            p[6]                   = 16;
            p[7]                   = 8;
#if MY2K_DEV3_PROFILE
            p[7] = 32;
#endif
#if MY2K_STOCK_PARITY_PROFILE
            p[6] = 255;
            p[7] = 255;
#endif
            put32(p + 8, my2k_qmk_rgb_errors());
            put32(p + 12, d.rx_frames);
            put32(p + 16, d.link_responses);
            put32(p + 20, d.response_timeouts);
            put32(p + 24, d.rx_invalid);
            p[28] = my2k_aux_responsive();
            break;
        }
        case 4: {
            my2k_rgb_diagnostics d = my2k_qmk_rgb_diagnostics();
            put32(p + 4, d.writes);
            put32(p + 8, d.acknowledged);
            put32(p + 12, d.frames);
            p[16] = (uint8_t)d.nonzero_channels;
            p[17] = d.nonzero_channels >> 8;
            p[18] = d.last_address;
            p[19] = d.last_register;
            p[20] = (uint8_t)d.last_status;
            p[21] = d.startup_test;
#if MY2K_DEV2_PROFILE
            p[22] = my2k_rgb_bus_lines();
            p[23] = my2k_rgb_bus_fault();
            put32(p + 24, my2k_rgb_bus_recoveries());
            p[28] =
#    if MY2K_DEV19_PROFILE
                0;
#    else
                1;
#    endif
#endif
            break;
        }
#if MY2K_DEV3_PROFILE
        case 5: {
            if (index >= 6) {
                p[3] = 255;
                break;
            }
            my2k_rgb_controller_diagnostics d = my2k_qmk_rgb_controller(index);
            p[4]                              = index;
            p[5]                              = my2k_rgb_drivers[index].address_byte >> 1;
            p[6]                              = my2k_rgb_drivers[index].type;
            p[7]                              = d.initialized;
            put32(p + 8, d.writes);
            put32(p + 12, d.acknowledged);
            put32(p + 16, d.nacks);
            put32(p + 20, d.timeouts);
            put32(p + 24, d.pwm_updates);
            p[28] = d.nonzero_channels;
            p[29] = d.nonzero_channels >> 8;
            p[30] = d.last_register;
            p[31] = (uint8_t)d.last_status;
            break;
        }
        case 6: {
            if (index >= 6) {
                p[3] = 255;
                break;
            }
            my2k_rgb_controller_diagnostics d = my2k_qmk_rgb_controller(index);
            p[4]                              = index;
            p[5]                              = my2k_rgb_drivers[index].address_byte >> 1;
            p[6]                              = my2k_rgb_drivers[index].type;
            p[7]                              = (uint8_t)d.read_status;
            memcpy(p + 8, d.pre, 4);
            memcpy(p + 12, d.post, 4);
            memcpy(p + 16, d.sample, 3);
            break;
        }
        case 7:
            if (index > 8)
                p[3] = 255;
            else
                my2k_main_rgb_diag(index, p);
            break;
#    if MY2K_DEV4_PROFILE
        case 8: {
            if (index >= MY2K_PERF_COUNT) {
                p[3] = 255;
                break;
            }
            my2k_perf_metric d = my2k_perf_get(index);
            p[4]               = index;
            put32(p + 8, d.count);
            put32(p + 12, d.total_us);
            put32(p + 16, d.max_us);
            put32(p + 20, d.last_us);
            put32(p + 24, d.max_interval_us);
            put32(p + 28, d.last_interval_us);
            break;
        }
        case 9: {
            if (index) {
                p[3] = 255;
                break;
            }
            my2k_refresh_stats d = my2k_refresh_diagnostics();
            p[4]                 = my2k_refresh_pending();
            p[5]                 = 1; /* one transaction/pass, boot-only readback cache */
            put32(p + 8, d.transactions);
            put32(p + 12, d.bytes);
            put32(p + 16, d.failures);
            put32(p + 20, d.completed);
            put32(p + 24, d.staged);
            put32(p + 28, d.coalesced);
            break;
        }
#    endif
#endif
#if MY2K_STOCK_PARITY_PROFILE
        case 10:
            if (index) {
                p[3] = 255;
                break;
            }
            put32(p + 4, my2k_usbh_fault());
            p[8] = my2k_usbh_write_profile_ready();
            p[9] = 1; /* Exact INQUIRY identity gate, not just media responsiveness. */
            break;
#    if MY2K_DEV6_PROFILE
        case 11: {
            if (index) {
                p[3] = 255;
                break;
            }
            my2k_usbh_diagnostics d;
            my2k_usbh_get_diagnostics(&d);
            p[4] = d.flags;
            p[5] = d.host_state;
            p[6] = d.lun_state;
            p[7] = d.worker_phase;
            put32(p + 8, d.blocks);
            put32(p + 12, d.generation);
            put32(p + 16, d.connect_attempts);
            put32(p + 20, d.connect_returns);
            put32(p + 24, d.connect_failures);
            p[28] = d.read_state;
            p[29] = d.file_state;
            p[30] = d.last_connect_result;
            p[31] = d.file_command;
            break;
        }
#    endif
#endif
#if MY2K_DEV6_PROFILE
        case 12:
            my2k_aux_telemetry_page(index, p, timer_read32());
            break;
#endif
#if MY2K_DEV7_PROFILE
        case 13: {
            if (index > 1) {
                p[3] = 255;
                break;
            }
            my2k_usbh_identity d;
            my2k_usbh_get_identity(&d);
            p[4] = index;
            p[5] = (d.device_valid ? 1 : 0) | (d.inquiry_valid ? 2 : 0);
            p[6] = d.vendor_id;
            p[7] = d.vendor_id >> 8;
            p[8] = d.product_id;
            p[9] = d.product_id >> 8;
            if (!index)
                memcpy(p + 10, d.inquiry, 22);
            else
                memcpy(p + 10, d.inquiry + 22, 14);
            break;
        }
        case 14:
            if (index)
                p[3] = 255;
            else
                my2k_asset_mode_status(p);
            break;
        case 15:
            if (index)
                p[3] = 255;
            else
                my2k_aux_telemetry_counters(p);
            break;
#endif
#if MY2K_DEV8_PROFILE
        case 16:
            if (index)
                p[3] = 255;
            else
                my2k_button_status(p);
            break;
        case 17:
            if (index)
                p[3] = 255;
            else
                my2k_host_mode_status(p);
            break;
#endif
#if MY2K_DEV9_PROFILE
        case 18:
            if (index)
                p[3] = 255;
            else
                my2k_host_port_status(p);
            break;
#endif
#if MY2K_DEV10_PROFILE
        case 19:
            if (!my2k_host_trace_page(index, p))
                p[3] = 255;
            break;
        case 20:
            if (!my2k_host_channel_page(index, p))
                p[3] = 255;
            break;
        case 21: {
            if (index >= 12) {
                p[3] = 255;
                break;
            }
            my2k_rgb_bus_failure_diagnostics d;
            if (!my2k_rgb_bus_failure(index / 2, &d)) {
                p[3] = 255;
                break;
            }
            bool last = index & 1;
            p[4]      = 1;
            p[5]      = index / 2;
            p[6]      = last;
            p[7]      = last ? d.last_phase : d.first_phase;
            put32(p + 8, d.attempts);
            put32(p + 12, d.failures);
            put32(p + 16, last ? d.last_ms : d.first_ms);
            uint16_t n = last ? d.last_data_index : d.first_data_index;
            p[20]      = (uint8_t)n;
            p[21]      = (uint8_t)(n >> 8);
            p[22]      = last ? d.last_fault : d.first_fault;
            p[23]      = (uint8_t)(last ? d.last_status : d.first_status);
            p[24]      = last ? d.last_register : d.first_register;
            p[25]      = my2k_rgb_drivers[index / 2].address_byte >> 1;
            break;
        }
#endif
#if MY2K_DEV11_PROFILE
        case 22:
            if (index)
                p[3] = 255;
            else
                my2k_scheduler_status(p);
            break;
#endif
#if MY2K_DEV12_PROFILE
        case 23:
            if (index)
                p[3] = 255;
            else
                my2k_host_events_summary(p);
            break;
        case 24:
            if (!my2k_host_events_page(index, p))
                p[3] = 255;
            break;
#    if MY2K_DEV14_PROFILE
        case 25:
        case 26:
            if (!my2k_usb_msc_diag(page, index, p))
                p[3] = 255;
            break;
#        if MY2K_DEV15_PROFILE
        case 27:
            if (!my2k_usbh_performance_page(index, p))
                p[3] = 255;
            break;
#            if MY2K_DEV16_PROFILE
        case 28:
            if (!my2k_input_status(index, p))
                p[3] = 255;
            break;
        case 29:
            if (!my2k_input_histogram(index, p))
                p[3] = 255;
            break;
        case 30:
            if (!my2k_input_scheduler_status(index, p))
                p[3] = 255;
            break;
#            endif
#        endif
#    endif
#endif
        default:
            p[3] = 0xff;
            break;
    }
    return true;
}
