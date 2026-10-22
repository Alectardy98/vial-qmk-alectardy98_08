#include "calibration.h"
// SPDX-License-Identifier: GPL-2.0-or-later
/* Keyboard policy only: QMK owns audio, haptic pulses and persistent settings. */
#include "class0413_ec.h"
#include "audio.h"
#include "haptic.h"
#include "solenoid.h"
#include "usb_device_state.h"
#include "ec_switch_matrix.h"
#include "led.h"
#include "process_music.h"

static bool ec_music_active(void) {
#ifdef NO_MUSIC_MODE
    return false;
#else
    return is_music_on();
#endif
}

extern haptic_config_t haptic_config;
extern bool solenoid_on[];
static bool audio_ready;
static uint8_t lock_pending;
void ec_standard_audio_init(void);
void ec_qmk_solenoid_fire_handler(void);

bool ec_actuators_blocked(void) {
    return ec_config.bottoming_calibration || usb_device_state_get_configure_state() != USB_DEVICE_STATE_CONFIGURED;
}

/* File-local substitution in standard solenoid.c suppresses only its greeting.
 * Real USB state, native HF_* dispatch and normal key feedback are unchanged. */
__attribute__((noinline,noclone)) usb_configure_state_t ec_solenoid_setup_usb_state(void) {
    return USB_DEVICE_STATE_INIT;
}

void ec_read_haptic_config(haptic_config_t *config) {
    eeconfig_read_haptic(config);
    if (config->raw == 0 || config->raw == UINT32_MAX || config->dwell == 0) {
        config->raw = 0; /* First feature use: disabled, buzz off. */
        config->dwell = SOLENOID_DEFAULT_DWELL;
    } else if (config->dwell < SOLENOID_MIN_DWELL || config->dwell > SOLENOID_MAX_DWELL) {
        config->dwell = SOLENOID_DEFAULT_DWELL;
    }
    /* Standard haptic_init/solenoid_set_dwell persists this validated setting. */
}

void audio_init(void) {
    bool first = !audio_ready;
    ec_standard_audio_init();
    audio_ready = true;
    /* This fork starts PWM even while audio_driver_stopped is initially true.
     * Stop the idle timer through its standard API; preserve saved audio enable. */
    if (first) audio_driver_stop_impl();
}

void solenoid_fire_handler(void) {
    if (ec_actuators_blocked() || !haptic_config.enable) return;
    if (haptic_config.dwell < SOLENOID_MIN_DWELL || haptic_config.dwell > SOLENOID_MAX_DWELL)
        haptic_config.dwell = SOLENOID_DEFAULT_DWELL;
    gpio_write_pin_high(HAPTIC_ENABLE_PIN);
    ec_qmk_solenoid_fire_handler();
}

bool get_haptic_enabled_key(uint16_t keycode, keyrecord_t *record) {
    (void)record;
    /* process_haptic runs before audio_off(), which waits 100 ms in this fork.
     * Never start a pulse on a control that may block, save, calibrate or reset. */
    return !ec_actuators_blocked() &&
           !(keycode >= QK_AUDIO_ON && keycode <= QK_AUDIO_MAX) &&
           !(keycode >= QK_HAPTIC_ON && keycode <= QK_HAPTIC_DWELL_DOWN) &&
           !(keycode >= CLASS0413_EC_NOISE && keycode <= CLASS0413_LK_SONG) &&
           keycode != QK_BOOTLOADER && keycode != QK_REBOOT;
}

__attribute__((noinline,noclone)) void actuator_storage_quiet(void) {
    lock_pending = 0;
    gpio_write_pin_low(HAPTIC_ENABLE_PIN);
    /* Before setup solenoid_on is false and active-state storage is unset. */
    if (solenoid_on[0]) solenoid_stop(0);
    gpio_write_pin_low(SOLENOID_PIN);
    if (audio_ready) {
        audio_stop_all();
        audio_driver_stop_impl();
    }
    /* Do not change/persist enable choices or music mode when silencing outputs. */
}

void keyboard_pre_init_user(void) {
    gpio_write_pin_low(HAPTIC_ENABLE_PIN);
    gpio_set_pin_output(HAPTIC_ENABLE_PIN);
    gpio_write_pin_low(SOLENOID_PIN);
    gpio_set_pin_output(SOLENOID_PIN);
}

void matrix_scan_user(void) {
    if (ec_actuators_blocked()) actuator_storage_quiet();
    else gpio_write_pin(HAPTIC_ENABLE_PIN, haptic_config.enable);
}

/* Only a timely local lock press authorizes sound: other host updates may be
 * delayed initial synchronization. Native QMK still drives LEDs for every mask.
 * These windows are conservative UI policy, not recovered stock constants. */
#define LOCK_SOUND_SETTLE_MS 1000U
#define LOCK_SOUND_RESPONSE_MS 500U
static uint8_t lock_usb_state, lock_seen, lock_target;
static uint32_t lock_session_time, lock_press_time;
static float lock_caps_on[][2] = SONG(CAPS_LOCK_ON_SOUND);
static float lock_caps_off[][2] = SONG(CAPS_LOCK_OFF_SOUND);
static float lock_num_on[][2] = SONG(NUM_LOCK_ON_SOUND);
static float lock_num_off[][2] = SONG(NUM_LOCK_OFF_SOUND);
static float lock_scroll_on[][2] = SONG(SCROLL_LOCK_ON_SOUND);
static float lock_scroll_off[][2] = SONG(SCROLL_LOCK_OFF_SOUND);

/* Existing core user word (logical bytes19..22), no new datablock/layout.
 * Zero/erased is unclaimed; foreign nonzero values are never overwritten.
 * Read only at startup; persist solely on explicit user toggles. */
#define LOCK_SONG_SETTING_TAG 0x4C530100UL
static bool lock_songs_enabled = true;
void keyboard_post_init_user(void) {
    uint32_t saved = eeconfig_read_user();
    lock_songs_enabled = (saved & ~1UL) == LOCK_SONG_SETTING_TAG ? (saved & 1) : true;
}

void ec_lock_song_toggle(void) {
    lock_songs_enabled = !lock_songs_enabled;
    lock_pending = 0;
    uint32_t saved = eeconfig_read_user();
    if (saved == 0 || saved == UINT32_MAX || (saved & ~1UL) == LOCK_SONG_SETTING_TAG)
        eeconfig_update_user(LOCK_SONG_SETTING_TAG | (uint32_t)lock_songs_enabled);
    /* Foreign ownership: RAM-only toggle; preserve its complete original value. */
}

/* Observe after normal action processing; do not consume or alter key events. */
void post_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    post_process_record_user(keycode, record);
    if (!lock_songs_enabled || ec_actuators_blocked() || !is_audio_on() || ec_music_active()) { lock_pending = 0; return; }
    if (!record->event.pressed) return;
    uint8_t bit;
    switch (keycode) {
        case KC_CAPS_LOCK: bit = 2; break;
        case KC_NUM_LOCK: bit = 1; break;
        case KC_SCROLL_LOCK: bit = 4; break;
        default: return;
    }
    if (ec_actuators_blocked() || !is_audio_on() || ec_music_active() ||
        timer_elapsed32(lock_session_time) < LOCK_SOUND_SETTLE_MS) return;
    if (timer_elapsed32(lock_press_time) > LOCK_SOUND_RESPONSE_MS) lock_pending = 0;
    if (lock_pending & bit) {
        /* Two presses without an intervening response are ambiguous. */
        lock_pending &= ~bit;
        return;
    }
    lock_target = (lock_target & ~bit) | ((~usb_device_state_get_leds()) & bit);
    /* Do not extend an older pending press by adding another lock. */
    if (!lock_pending) lock_press_time = timer_read32();
    lock_pending |= bit;
}

bool led_update_kb(led_t state) {
    bool accepted = led_update_user(state);
    uint8_t mask = state.raw & 7;
    uint8_t changed = mask ^ lock_seen;
    lock_seen = mask; /* Track even muted, suspended, rejected, or in Music Mode. */
    uint8_t pending = lock_pending;
    lock_pending &= ~changed; /* No deferred replay of suppressed transitions. */
    if (accepted) led_update_ports(state); /* Preserve native LED pin handling. */
    if (!accepted || !changed || !lock_songs_enabled || ec_actuators_blocked() || !is_audio_on() || ec_music_active() ||
        timer_elapsed32(lock_session_time) < LOCK_SOUND_SETTLE_MS ||
        timer_elapsed32(lock_press_time) > LOCK_SOUND_RESPONSE_MS ||
        (changed & pending) != changed || ((mask ^ lock_target) & changed)) return accepted;
    /* Stock order is Caps, Num, Scroll; submit only the last changed song. */
    if (changed & 4) {
        if (mask & 4) PLAY_SONG(lock_scroll_on); else PLAY_SONG(lock_scroll_off);
    } else if (changed & 1) {
        if (mask & 1) PLAY_SONG(lock_num_on); else PLAY_SONG(lock_num_off);
    } else {
        if (mask & 2) PLAY_SONG(lock_caps_on); else PLAY_SONG(lock_caps_off);
    }
    return accepted;
}

void notify_usb_device_state_change_user(struct usb_device_state state) {
    if (lock_usb_state != state.configure_state) {
        lock_usb_state = state.configure_state;
        lock_seen = state.leds & 7;
        lock_pending = 0;
        lock_session_time = timer_read32();
    }
    if (state.configure_state != USB_DEVICE_STATE_CONFIGURED) { ec_cal_abort();ec_va_disconnected();actuator_storage_quiet(); }
}

void suspend_power_down_user(void) { ec_cal_abort();ec_va_disconnected();actuator_storage_quiet(); }
bool shutdown_user(bool jump_to_bootloader) {
    (void)jump_to_bootloader;
    actuator_storage_quiet();
    return true;
}

// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdlib.h>

/* This newlib's rand() allocates reentrancy storage and pulls in malloc/assert
 * and stdio. Use libc's allocation-free, explicit-state PRNG only for Clicky's
 * cosmetic pitch jitter. QMK's Clicky handler and melody remain unchanged. */
int ec_clicky_rand(void) {
    static unsigned int state = 1U;
    return rand_r(&state);
}
