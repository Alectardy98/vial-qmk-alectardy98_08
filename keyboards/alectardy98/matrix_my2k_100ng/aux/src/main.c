/* SPDX-License-Identifier: GPL-2.0-or-later */
#if MY2K_DEV22_PROFILE
#    include "aux_perf22.h"
#endif
#include "aux_platform.h"
#if MY2K_NES_PROFILE
#    include "aux_nes.h"
#endif
#if MY2K_ANIMATION_PROFILE
#    include "aux_animation.h"
#    include "aux_animation_files.h"
#endif
#include <string.h>
#include "aux_assets.h"
#include "aux_state.h"
#include "aux_msc.h"
#include "aux_panels.h"
#include "aux_control.h"
#if AUX_VISUAL_SLEEP
#    include "aux_visual_sleep.h"
#endif
#if MY2K_RELEASE1_PROFILE
#    include "aux_oled_store.h"
#endif
#include "aux_work.h"
#if MY2K_DEV21_PROFILE
#    include "aux_perf21.h"
#endif
#if AUX_STOCK_PARITY
#    include "aux_storage.h"
#endif
#if MY2K_DEV6_PROFILE
#    include "aux_diagnostics.h"
#endif
#include "my2k_wire.h"
aux_state         aux_live_state;
aux_asset_status  aux_live_assets;
volatile uint32_t aux_rx_valid, aux_rx_invalid, aux_rx_unsupported;
/* Separate states: UART ACK means parser responsive, not panels/storage ready. */
volatile bool           aux_peripherals_initialized, aux_storage_initialized;
static aux_panels       panels;
static aux_render_sinks render_sinks;
static aux_control      control = {.panels = &panels};
/* Only the worker mutates panel/text/control state or touches SPI/I2C/FatFs.
 * Foreground enqueues complete messages; no bus/FS re-entry across a yield. */
static my2k_wire_message controls[8];
static unsigned          control_head, control_tail;
/* PWR has no stock ACK. Keep the newest intent even during panel startup;
 * otherwise an early suspend would be silently lost until a later transition. */
static uint8_t pending_power = 2;
static void    apply_pending_power(void) {
    if (pending_power > 1)
        return;
    my2k_wire_message request = {.type = MY2K_PWR, .v = {pending_power}};
    pending_power             = 2;
    bool reply;
    if (!aux_control_apply(&control, &request, &reply))
        aux_rx_unsupported++;
}
static void send_status(bool ok) {
    my2k_wire_message response = {.type = MY2K_STS, .v = {ok}};
    char              bytes[32];
    int               count = my2k_wire_encode(&response, bytes, sizeof bytes);
    if (count < 0 || !aux_uart_send(bytes, (size_t)count))
        aux_fault_code = 4;
}
static bool enqueue_control(const my2k_wire_message *request) {
    if (request->type == MY2K_PWR) {
        if (request->v[0] > 1)
            return false;
        pending_power = request->v[0];
        return true;
    }
    unsigned next          = (control_head + 1u) % 8u;
    bool     startup_probe = false;
#if MY2K_ANIMATION_PROFILE && MY2K_RELEASE1_PROFILE
    startup_probe = aux_animation_startup_probe(request);
#endif
    if ((!aux_peripherals_initialized && !startup_probe) || next == control_tail)
        return false;
    controls[control_head] = *request;
    control_head           = next;
    return true;
}
static void storage_start(void) {
#if MY2K_DEV21_PROFILE
    aux_startup.storage_begin_ms = aux_millis();
#endif
#if AUX_STOCK_PARITY
    aux_storage_start();
    aux_storage_service(aux_millis());
    if (aux_storage_live.stage == AUX_STORAGE_SD_FAILED)
        aux_fault_code = 2;
    else if (aux_storage_live.stage == AUX_STORAGE_MOUNT_FAILED)
        aux_fault_code = 3;
#else
    if (!aux_sd_init())
        aux_fault_code = 2;
    else if (!aux_assets_init())
        aux_fault_code = 3;
#endif
#if MY2K_RELEASE1_PROFILE
    (void)aux_oled_store_load(control.oled.config);
    control.text.typing_enabled = (control.oled.config[1] & 1) != 0;
    memset(control.text.label, 0, sizeof control.text.label);
    memcpy(control.text.label, control.oled.config + 64, control.oled.config[3]);
    control.oled.dirty = true;
#endif
    aux_storage_initialized = true; /* Attempt finished; readiness is aux_sd_sectors. */
#if MY2K_DEV21_PROFILE
    aux_startup.storage_ready_ms = aux_millis();
#endif
}
static void io_worker(void) {
    aux_touch_init();
#if MY2K_DEV21_PROFILE
    aux_startup.panels_begin_ms = aux_millis();
#endif
    (void)aux_panels_begin(&panels, aux_panel_physical_bus(), aux_millis());
#if MY2K_RELEASE1_PROFILE
    aux_oled_init(&control.oled, &control.text);
#endif
    bool storage_attempted = false;
    do {
        uint32_t now = aux_millis();
        aux_panels_service(&panels, now);
        /* SDMMC/FatFs and OLED SPI3 can run during color SPI1's 500ms
         * sleep-out wait. Keep a single worker/bus owner; no nested dispatch
         * or shorter controller delays. A slow card may extend, never shorten,
         * the color wait. Attempt storage even if color initialization fails. */
        if (!storage_attempted && (panels.color_setting >= 2 || panels.color_fault || !panels.color_capable)) {
            storage_start();
            storage_attempted = true;
        }
        if (aux_panels_starting(&panels)) {
            if (panels.color_waiting && (int32_t)(aux_millis() - panels.color_deadline) < 0)
                aux_work_idle();
            else
                aux_work_yield();
        }
    } while (aux_panels_starting(&panels));
    if (!storage_attempted)
        storage_start();
#if MY2K_DEV21_PROFILE
    aux_startup.panels_ready_ms = aux_millis();
#endif
    render_sinks                = (aux_render_sinks){panels.color_ready ? aux_panel_color_pixels : 0,
                                                     panels.mono_ready ? aux_panel_mono_pixels : 0, &panels};
    aux_live_state.panel_ready  = panels.color_ready && panels.mono_ready;
    aux_peripherals_initialized = true;
    apply_pending_power();
    uint32_t last_touch  = aux_millis();
    uint8_t  previous[8] = {0};
    bool     touch_seen  = false;
    for (;;) {
        aux_msc_worker_service();
#if MY2K_ANIMATION_PROFILE
        aux_animation_files_service();
#endif
#if AUX_STOCK_PARITY
        (void)aux_assets_media_service();
        aux_storage_service(aux_millis());
#endif
        apply_pending_power();
        if (control_tail != control_head) {
            my2k_wire_message request = controls[control_tail];
            control_tail              = (control_tail + 1u) % 8u;
#if MY2K_RELEASE1_PROFILE
            if (request.type == MY2K_AC) {
                my2k_wire_message response;
                int               result;
#    if MY2K_ANIMATION_PROFILE
#        if AUX_VISUAL_SLEEP
                if (aux_visual_sleep_request(&request, &response))
                    result = response.type ? 1 : 0;
                else
#        endif
#        if MY2K_NES_PROFILE
                    if (aux_nes_request(&request, &response))
                    result = response.type ? 1 : 0;
                else
#        endif
                    if (aux_animation_request(&request, &response))
                    result = 1;
                else
#    endif
                    result = aux_oled_request(&control.oled, &control.text, &panels, &request, &response, aux_millis());
                if (result) {
                    char bytes[128];
                    int  n = my2k_wire_encode(&response, bytes, sizeof bytes);
                    if (n < 0 || !aux_uart_send(bytes, (size_t)n))
                        aux_fault_code = 4;
                }
            } else
#endif
            {
                bool reply;
                bool ok = aux_control_apply(&control, &request, &reply);
#if AUX_STOCK_PARITY
                if (ok && request.type == MY2K_TM) {
                    bool state_reply;
                    (void)aux_state_apply(&aux_live_state, &request, &state_reply);
                }
#endif
                if (!ok)
                    aux_rx_unsupported++;
                if (reply)
                    send_status(ok);
            }
        }
        uint32_t now = aux_millis();
#if AUX_VISUAL_SLEEP
        my2k_wire_message sleep_reply;
        if (aux_visual_sleep_service(&panels, now, &sleep_reply)) {
            char bytes[128];
            int  n = my2k_wire_encode(&sleep_reply, bytes, sizeof bytes);
            if (n < 0 || !aux_uart_send(bytes, (size_t)n))
                aux_fault_code = 4;
        }
#endif
#if !MY2K_RELEASE1_PROFILE
        aux_text_service(&control.text, &panels);
#endif
#if AUX_STOCK_PARITY
        aux_control_service(&control, now);
#endif
        aux_panels_service(&panels, now);
#if MY2K_RELEASE1_PROFILE
        my2k_wire_message oled_reply;
        if (aux_oled_ack(&control.oled, &control.text, &panels, &control.clock, &oled_reply)) {
            char bytes[128];
            int  n = my2k_wire_encode(&oled_reply, bytes, sizeof bytes);
            if (n < 0 || !aux_uart_send(bytes, (size_t)n))
                aux_fault_code = 4;
        }
#endif
        int completed = aux_control_result(&control);
        if (completed)
            send_status(completed > 0);
        aux_live_state.panel_ready = panels.color_ready && panels.mono_ready;
#if MY2K_NES_PROFILE
        aux_nes_service(&panels, now);
#endif
        aux_assets_set_outputs(
#if MY2K_NES_PROFILE
            !aux_nes_active() &&
#endif
                panels.color_ready && !panels.color_blanked && !aux_panel_color_busy(&panels),
            panels.mono_ready && !panels.mono_blanked && panels.mono_enabled, now);
        if ((uint32_t)(now - last_touch) >= 20u) {
            last_touch              = now;
            my2k_wire_message touch = {.type = MY2K_TC};
            if (aux_touch_read(touch.v)) {
                bool changed = !touch_seen;
                for (unsigned i = 0; i < 8; i++)
                    if (touch.v[i] != previous[i])
                        changed = true;
                if (changed) {
                    char bytes[128];
                    int  n = my2k_wire_encode(&touch, bytes, sizeof bytes);
                    if (n < 0 || !aux_uart_send(bytes, (size_t)n))
                        aux_fault_code = 5;
                    else {
                        for (unsigned i = 0; i < 8; i++)
                            previous[i] = touch.v[i];
                        touch_seen = true;
                    }
                }
            }
        }
        aux_assets_step(now, &render_sinks);
        aux_assets_status(&aux_live_assets);
        /* Fairness yields keep the worker runnable. Do not turn each128poll SPI/SD
         * slice or128pixel chunk into an involuntary1ms sleep. Only a genuinely idle
         * pass waits for UART/USB/SysTick. A timed power transition may sleep until
         * its deadline; a dirty mono page or queued control must run immediately. */
        bool panel_runnable = (panels.mono_dirty && !panels.mono_blanked) ||
                              (aux_panel_color_busy(&panels) &&
                               (!panels.color_waiting || (int32_t)(aux_millis() - panels.color_deadline) >= 0));
        if (
#if MY2K_ANIMATION_PROFILE
            aux_animation_files_pending() ||
#endif
#if MY2K_NES_PROFILE
            aux_nes_runnable(aux_millis()) ||
#endif
            aux_assets_busy() || panel_runnable || control_tail != control_head || pending_power <= 1)
            aux_work_yield();
        else
            aux_work_idle();
    }
}
void aux_main(void) {
    if (!aux_clock_init()) {
        aux_fault_code = 1;
        for (;;)
            aux_idle();
    }
#if AUX_ICACHE_ENABLE
    extern void aux_instruction_cache_enable(void);
    aux_instruction_cache_enable();
#endif
#if MY2K_DEV22_PROFILE
    aux_perf22_init();
#endif
    aux_uart_init();
    my2k_wire_parser parser;
    my2k_wire_init(&parser);
    /* USB and the parser run before the first panel delay or SD card command. */
    if (!aux_usb_init())
        aux_fault_code = 6;
    aux_work_init(io_worker);
    for (;;) {
        aux_usb_task();
        uint8_t byte;
        for (unsigned budget = 0; budget < 128u && aux_uart_get(&byte); budget++) {
            my2k_wire_message request;
            int               result = my2k_wire_feed(&parser, byte, &request);
            if (result < 0) {
                aux_rx_invalid++;
                continue;
            }
            if (!result)
                continue;
            aux_rx_valid++;
            bool reply = false, ok;
#if MY2K_DEV6_PROFILE
            if (request.type == MY2K_DQ) {
                my2k_wire_message response = {.type = MY2K_DR};
                ok                         = aux_diagnostics_snapshot(request.v[0], &panels, response.v);
                if (ok) {
                    char bytes[128];
                    int  n = my2k_wire_encode(&response, bytes, sizeof bytes);
                    ok     = n >= 0 && aux_uart_send(bytes, (size_t)n);
                    if (!ok)
                        aux_fault_code = 4;
                }
            } else
#endif
#if MY2K_RELEASE1_PROFILE
                if (request.type == MY2K_AC) {
                ok = enqueue_control(&request);
                if (!ok) {
                    my2k_wire_message response = {.type = MY2K_AR, .v = {request.v[0], request.v[1], request.v[2], 2}};
                    char              bytes[128];
                    int               n = my2k_wire_encode(&response, bytes, sizeof bytes);
                    if (n > 0)
                        (void)aux_uart_send(bytes, (size_t)n);
                }
            } else
#endif
                if (request.type == MY2K_SCR || request.type == MY2K_PWR || request.type == MY2K_TX
#if AUX_STOCK_PARITY
                    || request.type == MY2K_TM
#endif
                ) {
                ok    = enqueue_control(&request);
                reply = !ok && request.type == MY2K_SCR;
            } else
                ok = aux_state_apply(&aux_live_state, &request, &reply);
            if (!ok)
                aux_rx_unsupported++;
            if (reply)
                send_status(ok);
        }
        aux_work_resume();
        if (!aux_uart_rx_pending() && !aux_work_runnable())
            aux_idle();
    }
}
