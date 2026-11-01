/* SPDX-License-Identifier: GPL-2.0-or-later */
#if MY2K_LOCK_LIGHTS_PROFILE
#    include "main_lock_lights.h"
#endif
#if MY2K_RELEASE1_PROFILE
#    include "main_oled_config.h"
#    if MY2K_ANIMATION_PROFILE
#        include "main_animation.h"
#    endif
#endif
#if MY2K_DEV21_PROFILE
#    include "main_rgb_first.h"
#endif
#if MY2K_DEV11_PROFILE
#    include "main_scheduler.h"
#endif
#if MY2K_DEV10_PROFILE
#    include "main_host_trace.h"
#endif
#include QMK_KEYBOARD_H
#include "my2k_amk.h"
#include "my2k_amk_rgb_runtime.h"
#include "main_rgb_settings.h"
#include "main_eeprom.h"
#if MY2K_ACTIVE_DEV_PROFILE
#    include "main_sleep.h"
#endif
#if MY2K_TOUCH_CONFIG_PROFILE
#    include "main_touch_config.h"
#endif
#if MY2K_DEV16_PROFILE
#    include "main_performance_mode.h"
#    include "main_input_performance.h"
#    include "main_input_scheduler.h"
#    if MY2K_DEV17_PROFILE
#        include "main_rgb_observer.h"
#    endif
#endif
#include "my2k_usb_msc.h"
#include "my2k_usbh_chibios.h"
#include "aux_link.h"
#include "my2k_qmk_rgb.h"
#include "my2k_touch.h"
#include "host.h"
#include "vial.h"
#if MY2K_DEV4_PROFILE
#    include "main_perf.h"
#endif
#if MY2K_DEV_PROFILE
#    include "main_diag.h"
#endif
#if MY2K_DEV6_PROFILE
#    include "main_aux_telemetry.h"
#endif
#if MY2K_DEV7_PROFILE
#    include "main_storage_mode.h"
#endif
#if MY2K_DEV15_PROFILE
#    include "main_keycodes.h"
#    if MY2K_GAMES_PROFILE
#        include "main_games.h"
#    endif
/* CUSTOM_MATRIX=lite exports the most recent raw scan. Observe both raw and
 * cooked state so initial debounce cannot turn a held entry key into an exit. */
extern matrix_row_t raw_matrix[MATRIX_ROWS];
void                matrix_scan_kb(void) {
    uint8_t row, col;
    if (my2k_asset_mode_position(&row, &col))
        my2k_asset_mode_observe((raw_matrix[row] & ((matrix_row_t)1 << col)) != 0, matrix_is_on(row, col),
                                timer_read32());
    matrix_scan_user();
}
bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    /* This runs before tap-hold, combos and key actions, including layers and
     * macros. The exit key is sampled independently from its cooked position. */
    if (my2k_asset_export_active() || my2k_asset_transition_pending()
#    if MY2K_EEPROM32_PROFILE
        || my2k_eeprom32_reset_pending()
#    endif
    )
        return false;
    if (my2k_asset_mode_blocks_key(record->event.key.row, record->event.key.col))
        return false;
    return pre_process_record_user(keycode, record);
}
#endif
#if !MY2K_TOUCH_CONFIG_PROFILE
static my2k_touch touch;
static void       touch_emit(void *context, my2k_touch_event event, uint16_t usage) {
    (void)context;
#    if MY2K_DEV15_PROFILE
    if (my2k_asset_export_active() || my2k_asset_transition_pending()
#        if MY2K_EEPROM32_PROFILE
        || my2k_eeprom32_reset_pending()
#        endif
    )
        return;
#    endif
    if (event == MY2K_TOUCH_REPORT)
        host_consumer_send(usage);
    else if (event == MY2K_TOUCH_MUTE_TAP)
        tap_code16(KC_MUTE);
}
#endif
void my2k_aux_on_message(const my2k_wire_message *message) {
#if MY2K_GAMES_PROFILE
    my2k_games_receive(message);
#endif
#if MY2K_ACTIVE_DEV_PROFILE
    my2k_sleep_receive(message);
#endif
#if MY2K_RELEASE1_PROFILE
    my2k_oled_main_receive(message);
#    if MY2K_ANIMATION_PROFILE
    my2k_animation_receive(message);
#    endif
#endif
    if (message && message->type == MY2K_TC) {
#if MY2K_TOUCH_CONFIG_PROFILE
        if (!my2k_asset_export_active() && !my2k_asset_transition_pending() && !my2k_eeprom32_reset_pending()
#    if MY2K_GAMES_PROFILE
            && !my2k_games_running()
#    endif
        )
            my2k_touchbar_sample(message->v, timer_read32());
#else
        my2k_touch_update(&touch, message->v, timer_read32());
#endif
    }
#if MY2K_DEV6_PROFILE
    my2k_aux_telemetry_receive(message, timer_read32());
#endif
}
void my2k_aux_connection_lost(void) {
#if MY2K_TOUCH_CONFIG_PROFILE
    my2k_touchbar_cancel();
#else
    my2k_touch_reset(&touch);
#endif
    my2k_aux_forget_state();
}
#include "main_settings.h"
static my2k_amk_rgb_runtime      rgb_runtime;
static const my2k_amk_ram_writes ram_writes       = {.scalar = my2k_main_scalar_set};
static const my2k_amk_backend    settings_backend = {.context    = &rgb_runtime,
                                                     .scalar     = my2k_main_scalar,
                                                     .ram_writes = &ram_writes};
static my2k_main_rgb_settings    rgb_settings;
#if MY2K_STOCK_PARITY_PROFILE
#    include "my2k_aux_ui.h"
#    include "my2k_stock_color.h"
static bool aux_mode_forwarded;
static bool aux_ui_send(void *ctx, const my2k_wire_message *message) {
    (void)ctx;
    return my2k_aux_send(message);
}
static bool aux_ui_mode_get(void *ctx, uint8_t *mode) {
    *mode = ((my2k_amk_rgb_runtime *)ctx)->aux_mode;
    return true;
}
static bool aux_ui_mode_set(void *ctx, uint8_t mode) {
    if (mode > 3 || mode == 2)
        return false;
#    if MY2K_DEV7_PROFILE
    if (mode == 0 && !my2k_asset_file_access_allowed())
        return false;
#    endif
    my2k_wire_message message = {.type = MY2K_SCR, .v = {MY2K_SCREEN_AM, mode}};
    if (!my2k_aux_send(&message))
        return false;
    my2k_amk_rgb_runtime *r = ctx;
    r->aux_mode             = mode;
    r->dirty                = true;
    r->configuration_revision++;
    aux_mode_forwarded = true;
    return true;
}
static const my2k_aux_ui_backend aux_ui = {.context    = &rgb_runtime,
                                           .send       = aux_ui_send,
                                           .get_mode   = aux_ui_mode_get,
                                           .apply_mode = aux_ui_mode_set};
#endif
#if MY2K_DEV3_PROFILE
static uint32_t lighting_requests, lighting_rejected;
static uint8_t  last_lighting_command;
/* Retain the real subsystem boundaries for compiled packet/persistence tests. */
__attribute__((noinline, noclone, externally_visible)) void my2k_main_rgb_initialize(void) {
    my2k_amk_rgb_init(&rgb_runtime, &settings_backend);
    my2k_main_rgb_restore(&rgb_settings, &rgb_runtime);
}
__attribute__((noinline, noclone, externally_visible)) bool my2k_main_rgb_frame(uint32_t now, uint8_t locks) {
#    if MY2K_LOCK_LIGHTS_PROFILE
    return my2k_lock_frame(&rgb_runtime, now, locks, layer_state | default_layer_state);
#    else
    return my2k_amk_rgb_tick(&rgb_runtime, now, locks);
#    endif
}
__attribute__((noinline, noclone, externally_visible)) void my2k_main_rgb_commit_task(void) {
    my2k_main_rgb_save_task(&rgb_settings, &rgb_runtime);
}
static void lighting_put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
void my2k_main_rgb_diag(unsigned group, uint8_t p[32]) {
    p[4]  = group;
    p[15] = my2k_qmk_rgb_test_group();
    if (group < 8) {
        my2k_rgb_group *g    = &rgb_runtime.groups[group];
        p[5]                 = g->mode;
        p[6]                 = rgb_runtime.brightness[group];
        p[7]                 = rgb_runtime.sync[group];
        p[8]                 = rgb_runtime.use_custom[group];
        my2k_rgb_color color = g->color;
#    if MY2K_STOCK_PARITY_PROFILE
        color = my2k_stock_hsv_to_rgb(color.r, color.g, color.b);
#    endif
        p[9]  = color.r;
        p[10] = color.g;
        p[11] = color.b;
        p[12] = group ? my2k_rgb_strips[group - 1].start : 0;
        p[13] = group ? my2k_rgb_strips[group - 1].count : 64;
        for (unsigned i = p[12]; i < (unsigned)p[12] + p[13]; i++)
            p[14] |= 1u << my2k_rgb_channels[i].driver;
    } else {
        const uint8_t indicators[] = {125, 126, 112, 64, 65, 127};
        p[13]                      = 6;
        for (unsigned i = 0; i < 6; i++)
            p[14] |= 1u << my2k_rgb_channels[indicators[i]].driver;
    }
    lighting_put32(p + 16, lighting_requests);
    lighting_put32(p + 20, lighting_rejected);
    lighting_put32(p + 24, rgb_runtime.configuration_revision);
    p[28] = last_lighting_command;
    p[29] = host_keyboard_leds();
    p[30] = vial_unlocked;
    p[31] = rgb_runtime.enabled;
}
#endif
void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
#if MY2K_GAMES_PROFILE
    if (my2k_games_hid(data, length))
        return;
#endif
#if MY2K_ACTIVE_DEV_PROFILE
    if (my2k_sleep_hid(data, length,
                       vial_unlocked && !my2k_asset_export_active() && !my2k_asset_transition_pending() &&
                           !my2k_eeprom32_reset_pending()))
        return;
#endif
#if MY2K_TOUCH_CONFIG_PROFILE
    if (my2k_touchbar_hid(data, length,
                          vial_unlocked && !my2k_asset_export_active() && !my2k_asset_transition_pending() &&
                              !my2k_eeprom32_reset_pending())) {
#    if MY2K_ACTIVE_DEV_PROFILE
        if (data[2] == 170 && data[4] == 2)
            my2k_sleep_activity(timer_read32());
#    endif
        return;
    }
#endif
#if MY2K_LOCK_LIGHTS_PROFILE
    if (my2k_lock_hid(data, length, timer_read32(),
                      vial_unlocked && !my2k_asset_export_active() && !my2k_asset_transition_pending())) {
#    if MY2K_ACTIVE_DEV_PROFILE
        if (data[2] == 170 && data[4] == 4)
            my2k_sleep_activity(timer_read32());
#    endif
        return;
    }
#    if MY2K_EEPROM32_PROFILE
    if (my2k_eeprom32_hid(data, length))
        return;
#    endif
#endif
#if MY2K_RELEASE1_PROFILE
    if (my2k_oled_hid(data, length)) {
#    if MY2K_ACTIVE_DEV_PROFILE
        /* Explicit updates/sync wake; GET/refresh/telemetry never refresh idle. */
        if (data[2] == 170 && (data[4] == 2 || data[4] == 7))
            my2k_sleep_activity(timer_read32());
#    endif
        return;
    }
#endif
#if MY2K_DEV16_PROFILE
    if (length == 32 && my2k_performance_reply(data))
        return;
#endif
#if MY2K_STOCK_PARITY_PROFILE
    if (my2k_aux_ui_hid(&aux_ui, data, length))
        return;
#endif
#if MY2K_DEV3_PROFILE
    bool standard_set = length == 32 && data[0] == 7;
    if (length == 32 && my2k_lighting_hid(&rgb_runtime, data)) {
#    if MY2K_ACTIVE_DEV_PROFILE
        if (standard_set && data[0] != 255)
            my2k_sleep_activity(timer_read32());
#    endif
        if (standard_set) {
            lighting_requests++;
            last_lighting_command = data[1];
            if (data[0] == 255)
                lighting_rejected++;
        }
        return;
    }
#endif
    /* Configuration RAM writes follow the same unlock policy as Vial keymaps.
     * Read-only rendered-color polling remains available while locked. */
#if !MY2K_DEV3_PROFILE
    if (length == 32 && data[0] == 0xfd && !vial_unlocked &&
        (data[1] == 30 || data[1] == 32 || data[1] == 34 || data[1] == 62 || data[1] == 68 || data[1] == 70)) {
        data[2] = 0x55;
        return;
    }
#endif
    if (!my2k_amk_dispatch(&rgb_runtime.backend, data, length))
        data[0] = 0xff;
#if MY2K_DEV3_PROFILE
    if (length == 32 && data[0] == 0xfd &&
        (data[1] == 30 || data[1] == 32 || data[1] == 34 || data[1] == 62 || data[1] == 68 || data[1] == 70)) {
        lighting_requests++;
        last_lighting_command = data[1];
        if (data[2] != 0xaa)
            lighting_rejected++;
#    if MY2K_ACTIVE_DEV_PROFILE
        else
            my2k_sleep_activity(timer_read32());
#    endif
    }
#endif
    /* VIA sends exactly one reply after this extension returns. */
}

void keyboard_pre_init_kb(void) {
#if MY2K_UART_RUNTIME_ENABLED
    /* AUX panel waits overlap external USB, EEPROM and RGB initialization.
     * Internal USB host startup remains after reset and host-driver binding. */
    my2k_aux_prepare();
#endif
    keyboard_pre_init_user();
}

void keyboard_post_init_kb(void) {
#if MY2K_ACTIVE_DEV_PROFILE
    my2k_sleep_init(timer_read32());
#endif
#if MY2K_RELEASE1_PROFILE
    my2k_oled_main_init();
#    if MY2K_ANIMATION_PROFILE
    my2k_animation_init();
#    endif
#endif
#if MY2K_TOUCH_CONFIG_PROFILE
    my2k_touchbar_init();
#else
    my2k_touch_init(&touch, touch_emit, NULL);
#endif
#if MY2K_DEV3_PROFILE
    my2k_main_rgb_initialize();
#else
    my2k_amk_rgb_init(&rgb_runtime, &settings_backend);
    my2k_main_rgb_restore(&rgb_settings, &rgb_runtime);
#endif
#if MY2K_STOCK_PARITY_PROFILE
    my2k_main_settings_bind(&rgb_runtime);
#    if MY2K_LOCK_LIGHTS_PROFILE
    my2k_lock_init(rgb_settings.restored);
#    endif
#endif
#if MY2K_UART_RUNTIME_ENABLED
    my2k_aux_start();
#endif
#if MY2K_RGB_RUNTIME_ENABLED
#    if MY2K_DEV15_PROFILE
    if (!my2k_asset_export_active())
#    endif
        (void)my2k_qmk_rgb_init(true);
#endif
    keyboard_post_init_user();
}

void my2k_hid_file_task(uint32_t now_ms);
void housekeeping_task_kb(void) {
#if MY2K_DEV16_PROFILE
    my2k_input_event_end();
    /* This first housekeeping pass is after protocol_post_init/host binding.
     * Checkpoints are only at task boundaries with no GPIO bus held. */
    my2k_input_scheduler_enter(my2k_performance_high_cpu(), true,
                               my2k_asset_export_active() || my2k_asset_transition_pending());
    (void)my2k_input_scheduler_checkpoint(true);
#endif
#if MY2K_DEV15_PROFILE
    bool storage_only = my2k_asset_export_active();
#endif
#if MY2K_ACTIVE_DEV_PROFILE
    my2k_sleep_service(timer_read32(),
                       storage_only || my2k_asset_transition_pending() || my2k_eeprom32_reset_pending());
#endif
#if MY2K_DEV10_PROFILE
    my2k_host_trace_service(timer_read32());
#endif
#if MY2K_DEV4_PROFILE
    uint32_t work_started = my2k_perf_clock(), part_started;
#endif
    my2k_hid_file_task(timer_read32());
    if (my2k_usbh_take_disconnect())
        my2k_aux_connection_lost();
#if MY2K_DEV15_PROFILE
    if (!storage_only) {
#endif
        my2k_aux_forward_status(host_keyboard_leds(), get_highest_layer(layer_state | default_layer_state));
        my2k_aux_typing_idle(last_input_activity_elapsed());
#if MY2K_DEV15_PROFILE
    }
#endif
#if MY2K_DEV4_PROFILE
    part_started = my2k_perf_clock();
#endif
    my2k_aux_service(timer_read32());
#if MY2K_USBH_RUNTIME_ENABLED
    /* The old post-init host start could enumerate an already-running AUX
     * during RGB init, then housekeeping reset that same USB device. Complete
     * the existing nonblocking PB4 reset sequence before starting enumeration.
     * my2k_usbh_start remains one-shot and retains its failure guard. */
    if (my2k_aux_startup_ready())
        my2k_usbh_start();
#endif
#if MY2K_RELEASE1_PROFILE
    my2k_oled_main_service(timer_read32());
#    if MY2K_ANIMATION_PROFILE
    my2k_animation_service(timer_read32());
#    endif
#endif
#if MY2K_DEV6_PROFILE
    my2k_aux_telemetry_service(timer_read32());
#endif
#if MY2K_STOCK_PARITY_PROFILE
#    if MY2K_DEV15_PROFILE
    if (!storage_only) {
#    endif
        if (!my2k_aux_responsive())
            aux_mode_forwarded = false;
        if (
#    if MY2K_RELEASE1_PROFILE
            false &&
#    endif
            !aux_mode_forwarded && my2k_aux_responsive())
            (void)aux_ui_mode_set(&rgb_runtime, rgb_runtime.aux_mode);
#    if MY2K_DEV15_PROFILE
    }
#    endif
#endif
#if MY2K_DEV4_PROFILE
    my2k_perf_record(MY2K_PERF_AUX, part_started);
    part_started = my2k_perf_clock();
#endif
#if MY2K_DEV15_PROFILE
    /* UART servicing/telemetry continues. The AUX is already told to pause
     * displays by the existing mode transition; no graphics/lighting state is
     * changed or persisted merely because storage mode is active. */
    if (!storage_only) {
#endif
#if !MY2K_TOUCH_CONFIG_PROFILE
        my2k_touch_update(&touch, NULL, timer_read32());
#endif
        bool frame_ready;
#if MY2K_DEV3_PROFILE
        frame_ready =
#    if MY2K_ACTIVE_DEV_PROFILE
            !my2k_sleep_blank() &&
#    endif
            my2k_main_rgb_frame(timer_read32(), host_keyboard_leds());
#else
    frame_ready = my2k_amk_rgb_tick(&rgb_runtime, timer_read32(), host_keyboard_leds());
#endif
#if MY2K_DEV4_PROFILE
        if (frame_ready) {
            my2k_perf_record(MY2K_PERF_RENDER, part_started);
        }
        part_started = my2k_perf_clock();
#endif
#if MY2K_DEV16_PROFILE
        (void)my2k_input_scheduler_checkpoint(true);
#endif
#if MY2K_RGB_RUNTIME_ENABLED
#    if MY2K_DEV4_PROFILE
        (void)frame_ready;
#        if MY2K_ACTIVE_DEV_PROFILE
        /* Physical blank only; authoritative configuration and effects untouched. */
        my2k_qmk_rgb_visual_blank(my2k_sleep_blank());
        (void)my2k_qmk_rgb_service(&rgb_runtime.rgb, timer_read32(), true);
#        else
        (void)my2k_qmk_rgb_service(&rgb_runtime.rgb, timer_read32(), true);
#        endif
#    else
        if (frame_ready)
            (void)my2k_qmk_rgb_service(&rgb_runtime.rgb, timer_read32(), true);
#    endif
#else
    (void)frame_ready;
#endif
#if MY2K_DEV4_PROFILE
        my2k_perf_record(MY2K_PERF_RGB, part_started);
#    if MY2K_DEV17_PROFILE
        my2k_rgb_observer_service(&rgb_runtime.rgb, timer_read32());
#    endif
#endif
#if MY2K_DEV16_PROFILE
        (void)my2k_input_scheduler_checkpoint(true);
#endif
#if MY2K_DEV3_PROFILE
#    if MY2K_EEPROM32_PROFILE
        if (!my2k_eeprom32_reset_pending())
#    endif
            my2k_main_rgb_commit_task();
#else
    my2k_main_rgb_save_task(&rgb_settings, &rgb_runtime);
#endif
#if MY2K_DEV15_PROFILE
    }
#endif
#if MY2K_DEV4_PROFILE
    part_started = my2k_perf_clock();
#endif
#if MY2K_TOUCH_CONFIG_PROFILE
    my2k_touchbar_service(
        timer_read32(), !storage_only && !my2k_asset_transition_pending() && !my2k_eeprom32_reset_pending()
#    if MY2K_GAMES_PROFILE
                            && !my2k_games_running()
#    endif
    );
#endif
    my2k_eeprom_task(timer_read32());
#if MY2K_EEPROM32_PROFILE
    /* Called only after the clear transaction's verified END/commit is durable.
     * This is a normal application restart, not ASTRO entry. */
    if (my2k_eeprom32_reset_ready())
        soft_reset_keyboard();
#endif
#if MY2K_LOCK_LIGHTS_PROFILE
    my2k_lock_service(
        timer_read32(), !storage_only && !my2k_asset_transition_pending() && last_input_activity_elapsed() >= 1000
#    if MY2K_EEPROM32_PROFILE
                            && !my2k_eeprom32_reset_pending()
#    endif
    );
#endif
#if MY2K_DEV4_PROFILE
    my2k_perf_record(MY2K_PERF_EEPROM, part_started);
#endif
    my2k_usb_msc_task(timer_read32());
#if MY2K_DEV7_PROFILE
#    if MY2K_EEPROM32_PROFILE
    if (!my2k_eeprom32_reset_pending())
#    endif
        my2k_asset_mode_task(timer_read32());
#endif
#if MY2K_DEV16_PROFILE
#    if MY2K_EEPROM32_PROFILE
    if (!my2k_eeprom32_reset_pending())
#    endif
        my2k_performance_task(timer_read32());
    my2k_input_scheduler_leave();
#endif
#if MY2K_GAMES_PROFILE
    my2k_games_service(timer_read32());
#endif
    housekeeping_task_user();
#if MY2K_DEV4_PROFILE
    my2k_perf_record(MY2K_PERF_HOUSEKEEPING, work_started);
#endif
#if MY2K_DEV11_PROFILE
    my2k_main_cooperate();
#endif
}
#if MY2K_DEV7_PROFILE
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
#    if MY2K_GAMES_PROFILE
    if (!my2k_games_key(keycode, record))
        return false;
#    endif
#    if MY2K_DEV21_PROFILE
    my2k_first_key(record->event.key.row, record->event.key.col, keycode, record->event.pressed, layer_state,
                   get_mods(), get_weak_mods(), get_highest_layer(default_layer_state));
#    endif
#    if MY2K_DEV16_PROFILE
    bool keyboard_event = (keycode >= KC_A && keycode <= KC_RGUI);
    my2k_input_event_begin(record->event.key.row, record->event.key.col, record->event.pressed, keyboard_event,
                           my2k_perf_clock());
#    endif
#    if MY2K_DEV15_PROFILE
#        if MY2K_EEPROM32_PROFILE
    if (my2k_eeprom32_reset_pending()) {
#            if MY2K_DEV16_PROFILE
        my2k_input_event_end();
#            endif
        return false;
    }
#        endif
#        if MY2K_ANIMATION_PROFILE
    if (keycode >= MY2K_ANIMATION_NEXT && keycode <= MY2K_ANIMATION_10) {
        if (record->event.pressed && !my2k_asset_export_active() && !my2k_asset_transition_pending())
            my2k_animation_key(keycode == MY2K_ANIMATION_NEXT       ? 255
                               : keycode == MY2K_ANIMATION_PREVIOUS ? 254
                                                                    : keycode - MY2K_ANIMATION_01 + 1);
#            if MY2K_DEV16_PROFILE
        my2k_input_event_end();
#            endif
        return false;
    }
#        endif
    if (keycode == MY2K_STORAGE_TOGGLE) {
        if (my2k_asset_mode_request_key(record->event.key.row, record->event.key.col, record->event.pressed,
                                        timer_read32()) &&
            record->event.pressed)
            clear_keyboard();
        return false;
    }
    if (my2k_asset_export_active() || my2k_asset_transition_pending())
        return false;
#    else
    if (keycode == KC_F24) {
        (void)my2k_asset_mode_request(record->event.pressed, timer_read32());
        return false;
    }
#    endif
#    if MY2K_EEPROM32_PROFILE
    if (keycode == QK_CLEAR_EEPROM) {
        if (record->event.pressed && my2k_eeprom32_reset_request())
            clear_keyboard();
#        if MY2K_DEV16_PROFILE
        my2k_input_event_end();
#        endif
        return false;
    }
#    endif
    bool accepted = process_record_user(keycode, record);
#    if MY2K_DEV16_PROFILE
    if (!accepted)
        my2k_input_event_end();
#    endif
    return accepted;
}
#endif
void post_process_record_kb(uint16_t keycode, keyrecord_t *record) {
#if MY2K_DEV16_PROFILE
    my2k_input_event_end();
#endif
#if MY2K_DEV15_PROFILE
    if (my2k_asset_export_active() || my2k_asset_transition_pending())
        return;
#endif
#if MY2K_DEV_PROFILE
    my2k_diag_key(record->event.key.row, record->event.key.col, keycode, record->event.pressed);
#    if MY2K_DEV17_PROFILE
    my2k_rgb_observer_key(record->event.key.row, record->event.key.col, keycode, record->event.pressed);
#    endif
#endif
    my2k_wire_message message = {.type = MY2K_KH,
                                 .v    = {record->event.key.row, record->event.key.col, record->event.pressed ? 1 : 0}};
    (void)my2k_aux_send(&message);
    my2k_aux_forward_key(keycode, record->event.pressed, get_mods() | get_weak_mods(), host_keyboard_leds());
    post_process_record_user(keycode, record);
}

void suspend_power_down_kb(void) {
#if MY2K_DEV21_PROFILE
    my2k_first_event(FIRST_MODE, 0, 0, 0, 0, 0, 0, 0, 0);
#endif
#if MY2K_TOUCH_CONFIG_PROFILE
    my2k_touchbar_cancel();
#else
    my2k_touch_reset(&touch);
#endif
    my2k_aux_request_power(false);
    my2k_aux_service(timer_read32());
#if MY2K_RELEASE1_PROFILE
    my2k_oled_main_service(timer_read32());
#    if MY2K_ANIMATION_PROFILE
    my2k_animation_service(timer_read32());
#    endif
#endif
#if MY2K_DEV6_PROFILE
    my2k_aux_telemetry_service(timer_read32());
#endif
    suspend_power_down_user();
}
void suspend_wakeup_init_kb(void) {
#if MY2K_DEV21_PROFILE
    my2k_first_event(FIRST_MODE, 0, 0, 1, 0, 0, 0, 1, 0);
#endif
    my2k_aux_request_power(true);
    my2k_aux_service(timer_read32());
#if MY2K_RELEASE1_PROFILE
    my2k_oled_main_service(timer_read32());
#    if MY2K_ANIMATION_PROFILE
    my2k_animation_service(timer_read32());
#    endif
#endif
#if MY2K_DEV6_PROFILE
    my2k_aux_telemetry_service(timer_read32());
#endif
    suspend_wakeup_init_user();
}

const char  my2k_mcu_identity[] = "Geehy APM32F405RGT6 experimental register compatibility";
const char *my2k_aux_track(void) {
#if defined(MY2K_AUX_STOCK_AUX)
    return "stock-aux";
#else
    return "replacement-aux";
#endif
}
