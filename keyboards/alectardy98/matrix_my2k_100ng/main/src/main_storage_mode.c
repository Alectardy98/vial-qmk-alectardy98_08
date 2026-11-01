/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_storage_mode.h"
#if MY2K_RELEASE1_PROFILE
#    include "main_oled_config.h"
#endif
#define ASSET_MARKER 0x4d594153u
#if !MY2K_STORAGE_MODE_HOST
#    include "hal.h"
#    include "wait.h"
#    include "my2k_usb_bus_hooks.h"
#    include "main_eeprom.h"
#    include "main_persist.h"
#    include "my2k_usbh_chibios.h"
#    include "my2k_usb_msc.h"
#    include "aux_link.h"
_Static_assert(SERIAL_BUFFERS_SIZE <= 256, "Update the UART-drain bound for a larger serial queue");
static uint32_t my2k_storage_platform_marker(void) {
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    return *(volatile uint32_t *)0x40002854;
}
static void my2k_storage_platform_marker_write(uint32_t marker) {
    /* Same backup-domain access path as the working ASTRO reentry and original
     * application08022984. BKP1 is separate from ASTRO's BKP0 ATKB marker. */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    *(volatile uint32_t *)0x420e0020 = 1;
    (void)*(volatile uint32_t *)0x40007000;
    *(volatile uint32_t *)0x40002854 = marker;
}
static bool my2k_storage_platform_configured(void) {
    return usbGetDriverStateI(&USB_DRIVER) == USB_ACTIVE;
}
static bool my2k_storage_platform_idle(void) {
    return my2k_usbh_asset_idle();
}
static bool my2k_storage_platform_eeprom_idle(void) {
    return !my2k_eeprom_pending() && !my2k_persist_busy();
}
static bool my2k_storage_platform_ejected(void) {
    /* Release 1 must complete its write/flush barrier before leaving export.
     * Legacy read-only profiles only need the separate request-idle guard. */
#    if MY2K_RELEASE1_PROFILE
    return my2k_usb_msc_exit_safe();
#    else
    return true;
#    endif
}
static bool my2k_storage_platform_aux_power(bool on) {
    my2k_wire_message message = {.type = MY2K_SCR, .v = {MY2K_SCREEN_PWR, on}};
    return my2k_aux_send(&message);
}
static void my2k_storage_platform_reset(void) {
    usbDisconnectBus(&USB_DRIVER);
    wait_ms(11);
    NVIC_SystemReset();
}
#endif
static bool     initialized, exporting, pending, target, queued, quiet_applied;
static uint32_t requested_at, queued_at, requests, refused, resets;
#if MY2K_DEV15_PROFILE
/* Same existing BKP1 one-shot storage marker; ASTRO's BKP0 is untouched.
 * 0x4d15 identifies this format, then mode/7-bit index and CRC-8/ATM.
 * The marker is transient and consumed on boot, never written to EEPROM. */
#    define ASSET_V15_TAG 0x4d150000u
#    define ASSET_RELEASE_STABLE_MS 20u
static uint8_t  toggle_row, toggle_col;
static bool     toggle_valid, exit_armed, release_tracking, return_guard;
static uint32_t released_at;
static uint8_t  marker_crc(uint32_t value) {
    uint8_t crc = 0;
    for (unsigned shift = 24; shift >= 8; shift -= 8) {
        crc ^= (uint8_t)(value >> shift);
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (uint8_t)((crc & 0x80u) ? ((uint32_t)crc << 1) ^ 0x07u : (uint32_t)crc << 1);
    }
    return crc;
}
static uint32_t position_marker(uint8_t row, uint8_t col, bool export_mode) {
    if (row >= 8u || col >= 16u)
        return 0;
    uint32_t marker = ASSET_V15_TAG | ((uint32_t)((row << 4) | col | (export_mode ? 0u : 0x80u)) << 8);
    return marker | marker_crc(marker);
}
static bool marker_position(uint32_t marker) {
    if ((marker & 0xffff0000u) != ASSET_V15_TAG || (uint8_t)marker != marker_crc(marker & 0xffffff00u))
        return false;
    toggle_row = (uint8_t)((marker >> 12) & 7u);
    toggle_col = (uint8_t)((marker >> 8) & 15u);
    return true;
}
#endif
#if MY2K_DEV8_PROFILE
static uint32_t power_restores, power_restore_errors;
#endif
void my2k_asset_mode_init(void) {
    if (initialized)
        return;
    initialized = true;
#if MY2K_DEV15_PROFILE
    uint32_t marker = my2k_storage_platform_marker();
    toggle_valid    = marker_position(marker);
    exporting       = toggle_valid && !(marker & 0x8000u);
    return_guard    = toggle_valid && !exporting;
    /* Ignore a legacy marker rather than entering without a usable exit key.
     * Clear only our owned storage markers, not unrelated backup contents. */
    if ((marker & 0xffff0000u) == ASSET_V15_TAG || marker == ASSET_MARKER)
        my2k_storage_platform_marker_write(0);
#else
    exporting = my2k_storage_platform_marker() == ASSET_MARKER;
    if (exporting)
        my2k_storage_platform_marker_write(0);
#endif
}
bool my2k_asset_export_active(void) {
    return exporting;
}
bool my2k_asset_transition_pending(void) {
    return pending;
}
bool my2k_asset_file_access_allowed(void) {
    return !exporting && !pending;
}
bool my2k_asset_mode_request(bool pressed, uint32_t now) {
    if (!pressed)
        return true;
    requests++;
    /* Drain file operations and the export barrier before handing storage
     * ownership back. The deferred transition also waits for EEPROM/USB idle. */
    if (
#if MY2K_RELEASE1_PROFILE
        my2k_oled_main_busy() || my2k_usbh_animation_busy() ||
#endif
        pending || !my2k_storage_platform_configured() ||
#if !MY2K_DEV15_PROFILE
        !my2k_storage_platform_idle() ||
#else
        !toggle_valid ||
#endif
        (exporting && !my2k_storage_platform_ejected())) {
        refused++;
        return false;
    }
    pending      = true;
    target       = !exporting;
    queued       = false;
    requested_at = now;
    return true;
}
#if MY2K_DEV15_PROFILE
__attribute__((noipa, externally_visible)) bool my2k_asset_mode_request_key(uint8_t row, uint8_t col, bool pressed,
                                                                            uint32_t now) {
    if (!pressed)
        return true;
    if (exporting || pending || row >= 8u || col >= 16u || my2k_asset_mode_blocks_key(row, col)) {
        refused++;
        return false;
    }
    toggle_row       = row;
    toggle_col       = col;
    toggle_valid     = true;
    return_guard     = false;
    exit_armed       = false;
    release_tracking = false;
    return my2k_asset_mode_request(true, now);
}
__attribute__((noipa, externally_visible)) bool my2k_asset_mode_position(uint8_t *row, uint8_t *col) {
    if ((!exporting && !return_guard) || !toggle_valid || !row || !col)
        return false;
    *row = toggle_row;
    *col = toggle_col;
    return true;
}
__attribute__((noipa, externally_visible)) bool my2k_asset_mode_blocks_key(uint8_t row, uint8_t col) {
    return return_guard && toggle_valid && row == toggle_row && col == toggle_col;
}
__attribute__((noipa, externally_visible)) void my2k_asset_mode_observe(bool raw_pressed, bool debounced_pressed,
                                                                        uint32_t now) {
    if (!initialized || (!exporting && !return_guard) || !toggle_valid || pending)
        return;
    if (!exit_armed) {
        if (raw_pressed || debounced_pressed) {
            release_tracking = false;
            return;
        }
        if (!release_tracking) {
            release_tracking = true;
            released_at      = now;
            return;
        }
        if ((uint32_t)(now - released_at) >= ASSET_RELEASE_STABLE_MS) {
            if (return_guard) {
                return_guard     = false;
                release_tracking = false;
            } else
                exit_armed = true;
        }
        return;
    }
    if (raw_pressed && debounced_pressed) {
        /* One event only until another complete release window, including if
         * USB is not configured yet or the request is otherwise refused. */
        exit_armed       = false;
        release_tracking = false;
        (void)my2k_asset_mode_request(true, now);
    }
}
#endif
void my2k_asset_mode_task(uint32_t now) {
    if (!pending) {
        if (exporting && !quiet_applied)
            quiet_applied = my2k_storage_platform_aux_power(false);
        return;
    }
    if ((uint32_t)(now - requested_at) > 5000u) {
#if MY2K_DEV8_PROFILE
        /* A queued power command has already changed the AUX independently of
         * the USB mode. Undo it when this application-mode switch is cancelled.
         * Single enqueue attempt only; report failure rather than reset/retry. */
        if (queued) {
            if (my2k_storage_platform_aux_power(!exporting))
                power_restores++;
            else
                power_restore_errors++;
        }
        queued = false;
#endif
        pending = false;
        refused++;
        return;
    }
    if (!my2k_storage_platform_configured() || !my2k_storage_platform_idle() || !my2k_storage_platform_eeprom_idle())
        return;
    if (!queued) {
        if (!my2k_storage_platform_aux_power(!target))
            return;
        queued    = true;
        queued_at = now;
        return;
    }
    /* Service remains cooperative. At115200baud a512-byte software queue plus
     * the256-byte serial queue drains in<67ms;100ms exceeds that bound. No
     * storage or flash operation is started while this transition is pending. */
    if ((uint32_t)(now - queued_at) < 100u)
        return;
#if MY2K_DEV15_PROFILE
    my2k_storage_platform_marker_write(position_marker(toggle_row, toggle_col, target));
#else
    my2k_storage_platform_marker_write(target ? ASSET_MARKER : 0);
#endif
    pending = false;
    resets++;
    my2k_storage_platform_reset();
}
static void put32(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(value >> (8 * i));
}
void my2k_asset_mode_status(uint8_t report[32]) {
    report[4] = exporting;
    report[5] = pending;
    report[6] = queued;
    put32(report + 8, requests);
    put32(report + 12, refused);
    put32(report + 16, resets);
#if MY2K_DEV8_PROFILE
    put32(report + 20, power_restores);
    put32(report + 24, power_restore_errors);
#endif
#if MY2K_DEV15_PROFILE
    report[7]  = 2;
    report[28] = toggle_valid ? toggle_row : 255;
    report[29] = toggle_valid ? toggle_col : 255;
    report[30] = exit_armed;
    report[31] = (uint8_t)(release_tracking | (return_guard ? 2u : 0u));
#endif
}
