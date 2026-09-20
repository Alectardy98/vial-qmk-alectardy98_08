/* Copyright 2022 Alectardy98
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "timer.h"
#include "config.h"

// Use QMK's audio engine (PWM on RP2040) instead of manually toggling GPIO
#include "audio.h"

// MAX7219 SPI
#include "quantum.h"
#include "spi_master.h"

/* ─────────────────────────────────────────────
 * Layers
 * ───────────────────────────────────────────── */
enum _layer {
    _BASE,
    _FN,
    _FN2,
};

/* ─────────────────────────────────────────────
 * Custom keycodes (Vial-friendly)
 * ─────────────────────────────────────────────
 *
 * NOTE:
 * - Keep LEDT = QK_KB_0 as your existing “toggle DEFAULT <-> TEST”
 * - Everything after that auto-increments (unique keycodes, no conflicts)
 */
enum custom_keycode {
    // Display / CW controls
    LEDT = QK_KB_0, // toggle DEFAULT <-> TEST
    CWMD,           // toggle DEFAULT <-> CW
    WPMT,           // enter WPM edit (CW mode only)
    TONE,           // MOMENTARY: hold = tone on, release = off (uses CW pitch)
    TNUP,           // pitch up
    TNDN,           // pitch down

    // Your custom “user macros”
    TEST,
    VDRT,                   // Desktop Right
    VDLT,                   // Desktop Left
    VDUP,                   // Mission Control (mac)
    VDDN,                   // Show Desktop (mac)
    EXPL,                   // Explorer (win) / Launchpad (mac)
    SNIP,                   // Snip tool / screenshot selection
    COPY,                   // Copy All
    CUTT,                   // Cut All
    PAST,                   // Paste Over
    TASK,                   // Task Manager
    GOGL,                   // Open Chrome (Automator: Ctrl+Alt+G on mac)
    CMD,                    // Open Terminal (Automator: Ctrl+Alt+T on mac)
    QUIT,                   // Close App
    DZRO,                   // Double Zero
    ATAB,                   // Alt+Tab
    LPRC,                   // "("
    RPRC,                   // ")"
    NUM,                    // Num Lock toggle

    // Discord Section
    THIS,                   // ":this:"
    OOF,                    // ":oof:"
    PIKA,                   // ":surprised:"
    SCAT,                   // ":smiley_cat2:"
    FCAT,                   // ":smiley_cat1:"
    HART,                   // ":heart:"
    DROL,                   // ":drooling_face:"
    MONY,                   // ":smiley_take:"
    FHAT,                   // ":smiley_fedora:"
    SPIT,                   // ":smiley_spit:"
    KING,                   // ":pepe_king:"
    FLEX,                   // ":muscle:"
    HAHA,                   // ":smiley_kekw:"
    LCRY,                   // ":joy:"
    NICE,                   // ":nice~2:"
    PPOG,                   // ":pepe_pog:"
    PRAY,                   // ":pray_tone2:"
    SWET,                   // ":sweat_smile:"
    COLD,                   // ":cold_face:"
    THUM,                   // ":thumbsup:"
};

/* ─────────────────────────────────────────────
 * Display modes
 * ───────────────────────────────────────────── */
typedef enum {
    MODE_DEFAULT = 0, // BOTH displays OFF (except overlays/counter)
    MODE_TEST    = 1, // Bar + MAX7219 test patterns
    MODE_CW      = 2, // CW mode: WPM display + audio sidetone
} display_mode_t;

static display_mode_t g_mode = MODE_DEFAULT;

/* ─────────────────────────────────────────────
 * 74HC595 helpers
 * ───────────────────────────────────────────── */
static inline void sr_pulse(pin_t pin) {
    writePinHigh(pin);
    writePinLow(pin);
}

static void bar_led_write(uint8_t bits) {
    writePinLow(BAR_RCLK_PIN);

    for (int8_t i = 7; i >= 0; i--) {
        if (bits & (1 << i)) writePinHigh(BAR_SER_PIN);
        else                 writePinLow(BAR_SER_PIN);
        sr_pulse(BAR_SRCLK_PIN);
    }

    sr_pulse(BAR_RCLK_PIN);
}

/* ─────────────────────────────────────────────
 * Bar segments (ACTIVE-LOW)
 * ───────────────────────────────────────────── */
// Seg 1  -> GP5 (ON=LOW)
// Seg 2  -> GP4 (ON=LOW)
// Seg 3..10 -> 74HC595 QA..QH (bit0..bit7), ON=0
static uint8_t sr_state = 0xFF; // 1=OFF, 0=ON

static inline void sr_commit(void) { bar_led_write(sr_state); }

static inline void sr_set_bit(uint8_t bit, bool on) {
    if (on) sr_state &= ~(1u << bit);   // ON  -> 0
    else    sr_state |=  (1u << bit);   // OFF -> 1
    sr_commit();
}

static inline void segments_all_off(void) {
    writePinHigh(GP5); // seg1 OFF
    writePinHigh(GP4); // seg2 OFF
    sr_state = 0xFF;
    sr_commit();
}

static inline void segments_all_on(void) {
    writePinLow(GP5);  // seg1 ON
    writePinLow(GP4);  // seg2 ON
    sr_state = 0x00;   // QA..QH ON
    sr_commit();
}

static inline void segment_set(uint8_t seg, bool on) {
    switch (seg) {
        case 1:  if (on) writePinLow(GP5); else writePinHigh(GP5); break;
        case 2:  if (on) writePinLow(GP4); else writePinHigh(GP4); break;
        case 3:  sr_set_bit(0, on); break; // QA
        case 4:  sr_set_bit(1, on); break; // QB
        case 5:  sr_set_bit(2, on); break; // QC
        case 6:  sr_set_bit(3, on); break; // QD
        case 7:  sr_set_bit(4, on); break; // QE
        case 8:  sr_set_bit(5, on); break; // QF
        case 9:  sr_set_bit(6, on); break; // QG
        case 10: sr_set_bit(7, on); break; // QH
        default: break;
    }
}

/* ─────────────────────────────────────────────
 * MAX7219 (SPI)
 * ───────────────────────────────────────────── */
#ifndef MAX7219_CS_PIN
#    define MAX7219_CS_PIN GP8
#endif

#ifndef MAX7219_NUM_DIGITS
#    define MAX7219_NUM_DIGITS 4
#endif

#ifndef MAX7219_INTENSITY_DEFAULT
#    define MAX7219_INTENSITY_DEFAULT 0x0F
#endif

#define REG_DIGIT0    0x01
#define REG_DECODE    0x09
#define REG_INTENSITY 0x0A
#define REG_SCANLIM   0x0B
#define REG_SHUTDOWN  0x0C
#define REG_TEST      0x0F

static inline void max7219_tx(uint8_t reg, uint8_t data) {
    spi_start(MAX7219_CS_PIN, /*lsb_first=*/false, /*mode=*/0, /*divisor=*/128);
    spi_write(reg);
    spi_write(data);
    spi_stop();
}

static inline void max7219_write_digit(uint8_t digit, uint8_t val) {
    if (digit >= MAX7219_NUM_DIGITS) return;
    max7219_tx((uint8_t)(REG_DIGIT0 + digit), val);
}

/* ─────────────────────────────────────────────
 * MAX7219 decode-mode switching (digits vs raw segments)
 * ───────────────────────────────────────────── */
#define SEG_A  0x40
#define SEG_B  0x20
#define SEG_C  0x10
#define SEG_D  0x08
#define SEG_E  0x04
#define SEG_F  0x02
#define SEG_G  0x01
#define SEG_DP 0x80

static uint8_t g_decode_mask = 0;
static bool    g_decode_on   = true;

static inline void max7219_set_decode(bool enable) {
    if (g_decode_on == enable) return;
    g_decode_on = enable;
    max7219_tx(REG_DECODE, enable ? g_decode_mask : 0x00);
}

static inline void max7219_force_on(void) {
    max7219_tx(REG_SHUTDOWN, 0x01);
    max7219_tx(REG_TEST, 0x00);
}

static inline void max7219_blank_all(void) {
    max7219_set_decode(true);
    for (uint8_t d = 0; d < MAX7219_NUM_DIGITS; d++) {
        max7219_write_digit(d, 0x0F); // blank in Code-B decode
    }
}

void max7219_init(void) {
    spi_init();

    max7219_tx(REG_TEST, 0x01);
    wait_ms(150);
    max7219_tx(REG_TEST, 0x00);

    max7219_tx(REG_SHUTDOWN, 0x01);
    max7219_tx(REG_SCANLIM, (uint8_t)(MAX7219_NUM_DIGITS - 1));

    g_decode_mask = (MAX7219_NUM_DIGITS >= 8) ? 0xFF : (uint8_t)((1u << MAX7219_NUM_DIGITS) - 1u);
    max7219_tx(REG_DECODE, g_decode_mask);
    g_decode_on = true;

    max7219_tx(REG_INTENSITY, (MAX7219_INTENSITY_DEFAULT & 0x0F));
    max7219_blank_all();
}

/* ─────────────────────────────────────────────
 * DEFAULT MODE: keypress counter + responsive bar + FN/CAPS overlays
 * ───────────────────────────────────────────── */

#ifndef BAR_ACTIVITY_MAX
#    define BAR_ACTIVITY_MAX 255u
#endif
#ifndef BAR_ACTIVITY_BOOST
#    define BAR_ACTIVITY_BOOST 38u
#endif
#ifndef BAR_ACTIVITY_DECAY_MS
#    define BAR_ACTIVITY_DECAY_MS 18u
#endif
#ifndef BAR_ACTIVITY_DECAY_STEP
#    define BAR_ACTIVITY_DECAY_STEP 7u
#endif
#ifndef BAR_RENDER_MS
#    define BAR_RENDER_MS 14u
#endif

static uint8_t  g_bar_activity   = 0;
static uint32_t g_bar_last_decay = 0;
static uint32_t g_bar_last_render = 0;

// 4-digit keypress counter (default mode)
static uint16_t g_keypress_count = 0;

static inline void bar_activity_boost(void) {
    uint16_t v = (uint16_t)g_bar_activity + (uint16_t)BAR_ACTIVITY_BOOST;
    g_bar_activity = (v > BAR_ACTIVITY_MAX) ? (uint8_t)BAR_ACTIVITY_MAX : (uint8_t)v;
}

static inline uint8_t bar_level_from_activity(uint8_t a) {
    if (a == 0) return 0;
    uint16_t level = (uint16_t)((a * 10u + 254u) / 255u); // ceil
    if (level > 10) level = 10;
    if (level < 1) level = 1;
    return (uint8_t)level;
}

static void default_bar_task(void) {
    uint32_t now = timer_read32();

    if (TIMER_DIFF_32(now, g_bar_last_decay) >= BAR_ACTIVITY_DECAY_MS) {
        g_bar_last_decay = now;

        if (g_bar_activity > 0) {
            uint8_t dec = BAR_ACTIVITY_DECAY_STEP;
            g_bar_activity = (g_bar_activity > dec) ? (g_bar_activity - dec) : 0;
        }
    }

    if (TIMER_DIFF_32(now, g_bar_last_render) < BAR_RENDER_MS) return;
    g_bar_last_render = now;

    uint8_t level = bar_level_from_activity(g_bar_activity);

    segments_all_off();
    for (uint8_t s = 1; s <= level; s++) {
        segment_set(s, true);
    }
}

static void max7219_show_count_4(uint16_t v) {
    // 0000..9999 in Code-B decode
    max7219_force_on();
    max7219_set_decode(true);

    uint16_t x = (v % 10000u);

    uint8_t d0 = (uint8_t)(x % 10u); x /= 10u;
    uint8_t d1 = (uint8_t)(x % 10u); x /= 10u;
    uint8_t d2 = (uint8_t)(x % 10u); x /= 10u;
    uint8_t d3 = (uint8_t)(x % 10u);

    max7219_write_digit(0, d3);
    if (MAX7219_NUM_DIGITS > 1) max7219_write_digit(1, d2);
    if (MAX7219_NUM_DIGITS > 2) max7219_write_digit(2, d1);
    if (MAX7219_NUM_DIGITS > 3) max7219_write_digit(3, d0);
}

static void max7219_show_FN_custom(void) {
    max7219_force_on();
    max7219_set_decode(false);

    uint8_t Fmask = (SEG_A | SEG_F | SEG_E | SEG_G);                 // B,C,D OFF
    uint8_t Nmask = (SEG_F | SEG_B | SEG_A | SEG_E | SEG_C);         // your custom N

    max7219_write_digit(0, Fmask);
    if (MAX7219_NUM_DIGITS > 1) max7219_write_digit(1, Nmask);
    if (MAX7219_NUM_DIGITS > 2) max7219_write_digit(2, 0x00);
    if (MAX7219_NUM_DIGITS > 3) max7219_write_digit(3, 0x00);
}

static void max7219_show_CAPS_custom(void) {
    max7219_force_on();
    max7219_set_decode(false);

    uint8_t Cmask = (SEG_A | SEG_F | SEG_E | SEG_D);
    uint8_t Amask = (SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G);
    uint8_t Pmask = (SEG_A | SEG_B | SEG_E | SEG_F | SEG_G);
    uint8_t Smask = (SEG_A | SEG_F | SEG_G | SEG_C | SEG_D);

    max7219_write_digit(0, Cmask);
    if (MAX7219_NUM_DIGITS > 1) max7219_write_digit(1, Amask);
    if (MAX7219_NUM_DIGITS > 2) max7219_write_digit(2, Pmask);
    if (MAX7219_NUM_DIGITS > 3) max7219_write_digit(3, Smask);
}

/* ─────────────────────────────────────────────
 * FN2 overlay: show "FN 2" + bar all on
 * ───────────────────────────────────────────── */
static void max7219_show_FN2_custom(void) {
    max7219_force_on();
    max7219_set_decode(false);

    uint8_t Fmask = (SEG_A | SEG_F | SEG_E | SEG_G);                 // F
    uint8_t Nmask = (SEG_F | SEG_B | SEG_A | SEG_E | SEG_C);         // your custom N
    uint8_t Twom  = (SEG_A | SEG_B | SEG_G | SEG_E | SEG_D);         // "2" in raw segments

    max7219_write_digit(0, Fmask);
    if (MAX7219_NUM_DIGITS > 1) max7219_write_digit(1, Nmask);
    if (MAX7219_NUM_DIGITS > 2) max7219_write_digit(2, 0x00);
    if (MAX7219_NUM_DIGITS > 3) max7219_write_digit(3, Twom);
}

/* ─────────────────────────────────────────────
 * FN bar animation (LOOPING): 1→10→9→1 forever while FN is held
 * ───────────────────────────────────────────── */
#define FN_BAR_STEP_MS   74u
#define FN_BAR_STEPS     19u
#define FN_TOTAL_MS      (FN_BAR_STEP_MS * FN_BAR_STEPS)

static void fn_loop_bar_task(void) {
    uint32_t now = timer_read32();

    // Loop forever
    uint32_t elapsed = now % FN_TOTAL_MS;

    uint32_t step = elapsed / FN_BAR_STEP_MS;
    if (step >= FN_BAR_STEPS) step = FN_BAR_STEPS - 1;

    // step 0..9 -> seg 1..10, step 10..18 -> seg 9..1
    uint8_t seg = (step <= 9) ? (uint8_t)(1u + step) : (uint8_t)(19u - step);

    segments_all_off();
    segment_set(seg, true);
}

static void default_display_task(void) {
    bool fn2_active = layer_state_is(_FN2);
    bool fn_active  = layer_state_is(_FN);
    led_t leds = host_keyboard_led_state();
    bool caps_on = leds.caps_lock;

    // Priority: FN2 > FN > CAPS > counter
    if (fn2_active) {
        max7219_show_FN2_custom();
        segments_all_on();      // ALL segments ON in FN2
        return;                 // skip other behavior
    }

    if (fn_active) {
        // FN: show "FN" and run the looping sweep animation regardless of typing
        max7219_show_FN_custom();
        fn_loop_bar_task();
        return;
    }

    if (caps_on) {
        max7219_show_CAPS_custom();
    } else {
        max7219_show_count_4(g_keypress_count);
    }

    default_bar_task();
}

/* ─────────────────────────────────────────────
 * CW Audio + WPM + Morse scheduler
 * ───────────────────────────────────────────── */

#ifndef CW_DEFAULT_WPM
#    define CW_DEFAULT_WPM 33u
#endif

static uint16_t cw_wpm = CW_DEFAULT_WPM;

// WPM entry state (CW mode only)
static bool     cw_edit_active = false;
static char     cw_edit_buf[4] = {0};
static uint8_t  cw_edit_len    = 0;

static inline uint16_t cw_dit_ms(void) {
    if (cw_wpm < 1) return 1200;
    return (uint16_t)(1200u / cw_wpm);
}

#ifndef CW_DOT_MIN_MS
#    define CW_DOT_MIN_MS 30u
#endif

static inline bool is_digit_kc(uint16_t kc) {
    return (kc == KC_0) || (kc >= KC_1 && kc <= KC_9);
}
static inline char digit_from_kc(uint16_t kc) {
    return (kc == KC_0) ? '0' : (char)('0' + (kc - KC_1 + 1));
}

static float cw_tone_hz = 600.0f;
static bool  cw_tone_running = false;
static bool  cw_tone_preview = false;

#ifndef CW_TONE_STEP_HZ
#    define CW_TONE_STEP_HZ 20.0f
#endif
#ifndef CW_TONE_MIN_HZ
#    define CW_TONE_MIN_HZ  200.0f
#endif
#ifndef CW_TONE_MAX_HZ
#    define CW_TONE_MAX_HZ  2000.0f
#endif

static inline float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline void cw_tone_engine_start(void) {
    if (!is_audio_on()) return;
    if (cw_tone_running) return;
    cw_tone_running = true;
    play_note(cw_tone_hz, 1);
}

static inline void cw_tone_engine_stop(void) {
    cw_tone_running = false;
    stop_all_notes();
}

static inline void cw_tone_on(void)  { cw_tone_engine_start(); }
static inline void cw_tone_off(void) { cw_tone_engine_stop();  }
static inline void cw_audio_hard_stop(void) { cw_tone_engine_stop(); }

static inline void cw_tone_restart_if_running(void) {
    if (!cw_tone_running) return;
    stop_all_notes();
    cw_tone_running = false;
    cw_tone_engine_start();
}

static const char *cw_morse_for(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');

    switch (c) {
        case 'A': return ".-";
        case 'B': return "-...";
        case 'C': return "-.-.";
        case 'D': return "-..";
        case 'E': return ".";
        case 'F': return "..-.";
        case 'G': return "--.";
        case 'H': return "....";
        case 'I': return "..";
        case 'J': return ".---";
        case 'K': return "-.-";
        case 'L': return ".-..";
        case 'M': return "--";
        case 'N': return "-.";
        case 'O': return "---";
        case 'P': return ".--.";
        case 'Q': return "--.-";
        case 'R': return ".-.";
        case 'S': return "...";
        case 'T': return "-";
        case 'U': return "..-";
        case 'V': return "...-";
        case 'W': return ".--";
        case 'X': return "-..-";
        case 'Y': return "-.--";
        case 'Z': return "--..";

        case '0': return "-----";
        case '1': return ".----";
        case '2': return "..---";
        case '3': return "...--";
        case '4': return "....-";
        case '5': return ".....";
        case '6': return "-....";
        case '7': return "--...";
        case '8': return "---..";
        case '9': return "----.";

        default:  return NULL;
    }
}

static void cw_display_wpm(uint16_t wpm) {
    max7219_force_on();
    max7219_set_decode(true);

    if (wpm > 99) wpm = 99;

    uint8_t tens = (uint8_t)(wpm / 10u);
    uint8_t ones = (uint8_t)(wpm % 10u);

    max7219_write_digit(0, 0x0F);
    max7219_write_digit(1, 0x0F);

    if (MAX7219_NUM_DIGITS > 2) max7219_write_digit(2, tens);
    if (MAX7219_NUM_DIGITS > 3) max7219_write_digit(3, ones);
}

/* ─────────────────────────────────────────────
 * Type-ahead queue + CW bargraph (unchanged)
 * ───────────────────────────────────────────── */
typedef enum {
    CW_IDLE = 0,
    CW_TONE_ON,
    CW_GAP,
} cw_play_state_t;

#define CW_QSIZE 512u
#define CW_QMASK (CW_QSIZE - 1u)

static char     cw_q[CW_QSIZE];
static uint16_t cw_q_r = 0, cw_q_w = 0;
static uint32_t cw_queue_dits_total = 0;

static inline uint16_t cw_q_len(void) { return (uint16_t)((cw_q_w - cw_q_r) & CW_QMASK); }
static inline bool cw_q_empty(void)   { return cw_q_r == cw_q_w; }
static inline bool cw_q_full(void)    { return cw_q_len() >= (CW_QSIZE - 1u); }

#ifndef CW_BAR_FULL_MS
#    define CW_BAR_FULL_MS 30000u
#endif

static inline uint16_t cw_symbol_dits(char sym) { return (sym == '-') ? 3u : 1u; }

static uint16_t cw_char_dits(char c) {
    if (c == ' ') return 7u;

    const char *p = cw_morse_for(c);
    if (!p) return 3u;

    uint16_t dits = 0;
    uint16_t n = 0;
    while (*p) {
        dits += cw_symbol_dits(*p);
        n++;
        p++;
    }
    if (n > 1) dits += (uint16_t)(n - 1u);
    dits += 3u;
    return dits;
}

static inline void cw_q_push(char c) {
    if (cw_q_full()) return;
    cw_q[cw_q_w] = c;
    cw_q_w = (cw_q_w + 1u) & CW_QMASK;
    cw_queue_dits_total += cw_char_dits(c);
}

static inline char cw_q_pop(void) {
    char c = cw_q[cw_q_r];
    cw_q_r = (cw_q_r + 1u) & CW_QMASK;
    return c;
}

static inline bool cw_q_pop_last(void) {
    if (cw_q_empty()) return false;
    uint16_t last = (cw_q_w - 1u) & CW_QMASK;
    char c = cw_q[last];
    cw_q_w = last;

    uint16_t d = cw_char_dits(c);
    cw_queue_dits_total = (cw_queue_dits_total >= d) ? (cw_queue_dits_total - d) : 0;
    return true;
}

static inline uint32_t cw_estimate_queue_ms(void) {
    return cw_queue_dits_total * (uint32_t)cw_dit_ms();
}

static bool     cw_queue_locked = false;
static uint32_t cw_err_flash_until = 0;

#ifndef CW_ERR_FLASH_MS
#    define CW_ERR_FLASH_MS 200u
#endif

static inline void cw_error_beep_and_flash(void) {
    cw_queue_locked = true;
    cw_err_flash_until = timer_read32() + CW_ERR_FLASH_MS;

    if (!is_audio_on()) return;
    play_note(200.0f, 1);
    wait_ms(60);
    stop_all_notes();
}

#ifndef CW_BAR_UPDATE_MS
#    define CW_BAR_UPDATE_MS 40u
#endif

static uint32_t cw_bar_last_update = 0;
static uint8_t  cw_bar_cached_level = 0;

static uint8_t cw_bar_level_from_ms(uint32_t ms) {
    uint8_t level;
    if (ms == 0) level = 0;
    else if (ms >= (uint32_t)CW_BAR_FULL_MS) level = 10;
    else {
        uint32_t bumped = ms + (CW_BAR_FULL_MS / 20u);
        level = (uint8_t)((bumped * 10u) / (uint32_t)CW_BAR_FULL_MS);
        if (level > 10) level = 10;
    }
    return level;
}

static void cw_bar_apply_level(uint8_t level, bool force) {
    if (!force && level == cw_bar_cached_level) return;
    cw_bar_cached_level = level;

    segments_all_off();
    for (uint8_t s = 1; s <= level; s++) segment_set(s, true);
}

static void cw_bar_update_time_remaining(void) {
    uint32_t now = timer_read32();

    bool flash = (cw_err_flash_until != 0) && ((int32_t)(now - cw_err_flash_until) < 0);
    bool force = flash;

    if (!force && TIMER_DIFF_32(now, cw_bar_last_update) < CW_BAR_UPDATE_MS) return;
    cw_bar_last_update = now;

    uint8_t level = cw_bar_level_from_ms(cw_estimate_queue_ms());
    cw_bar_apply_level(level, force);

    if (flash) {
        segment_set(10, true);
    } else if (cw_err_flash_until != 0 && (int32_t)(now - cw_err_flash_until) >= 0) {
        cw_err_flash_until = 0;
    }
}

/* ─────────────────────────────────────────────
 * Playback working state (unchanged)
 * ───────────────────────────────────────────── */
static cw_play_state_t cw_state = CW_IDLE;
static uint32_t cw_next_event_ms = 0;

static const char *cw_pat = NULL;
static uint8_t cw_pat_idx = 0;
static bool cw_intra_symbol_gap = false;

static uint16_t cw_last_on_extra_ms = 0;
static uint32_t cw_gap_debt_ms      = 0;

static inline uint16_t cw_elem_base_ms(char sym) {
    uint16_t dit = cw_dit_ms();
    return (sym == '-') ? (uint16_t)(3u * dit) : dit;
}

static inline uint16_t cw_elem_play_ms(char sym, uint16_t *extra_out) {
    uint16_t base = cw_elem_base_ms(sym);

    if (sym == '.' && base < CW_DOT_MIN_MS) {
        uint16_t extra = (uint16_t)(CW_DOT_MIN_MS - base);
        *extra_out = extra;
        return CW_DOT_MIN_MS;
    }

    *extra_out = 0;
    return base;
}

static inline uint32_t cw_apply_gap_debt(uint32_t gap_ms) {
    if (cw_gap_debt_ms == 0) return gap_ms;
    uint32_t use = (cw_gap_debt_ms < gap_ms) ? cw_gap_debt_ms : gap_ms;
    cw_gap_debt_ms -= use;
    return gap_ms - use;
}

static void cw_start_next_char(void) {
    if (cw_q_empty()) {
        cw_state = CW_IDLE;
        cw_tone_off();
        return;
    }

    cw_queue_locked = false;

    char c = cw_q_pop();

    uint16_t d = cw_char_dits(c);
    cw_queue_dits_total = (cw_queue_dits_total >= d) ? (cw_queue_dits_total - d) : 0;

    uint16_t dit = cw_dit_ms();

    if (c == ' ') {
        cw_state = CW_GAP;
        cw_tone_off();
        uint32_t gap = cw_apply_gap_debt((uint32_t)(7u * dit));
        cw_next_event_ms = timer_read32() + gap;
        return;
    }

    cw_pat = cw_morse_for(c);
    if (!cw_pat) {
        cw_state = CW_GAP;
        cw_tone_off();
        uint32_t gap = cw_apply_gap_debt((uint32_t)(3u * dit));
        cw_next_event_ms = timer_read32() + gap;
        return;
    }

    cw_pat_idx = 0;
    cw_intra_symbol_gap = false;

    cw_state = CW_TONE_ON;
    cw_tone_on();

    cw_last_on_extra_ms = 0;
    uint16_t dur = cw_elem_play_ms(cw_pat[cw_pat_idx], &cw_last_on_extra_ms);
    cw_next_event_ms = timer_read32() + dur;
}

static void cw_task(void) {
    if (g_mode != MODE_CW) {
        cw_state = CW_IDLE;
        cw_audio_hard_stop();
        return;
    }

    if (!is_audio_on() && cw_tone_running) cw_audio_hard_stop();

    max7219_force_on();

    if (cw_edit_active) {
        uint16_t tmp = 0;
        for (uint8_t i = 0; i < cw_edit_len; i++) tmp = (uint16_t)(tmp * 10u + (uint16_t)(cw_edit_buf[i] - '0'));
        cw_display_wpm(tmp);
    } else {
        cw_display_wpm(cw_wpm);
    }

    if (cw_tone_preview) {
        cw_tone_on();
        return;
    }

    uint32_t now = timer_read32();

    if (cw_state == CW_IDLE) {
        if (!cw_q_empty()) cw_start_next_char();
        return;
    }

    if ((int32_t)(now - cw_next_event_ms) < 0) return;

    uint16_t dit = cw_dit_ms();

    if (cw_state == CW_TONE_ON) {
        cw_tone_off();
        cw_state = CW_GAP;

        cw_gap_debt_ms += (uint32_t)cw_last_on_extra_ms;
        cw_last_on_extra_ms = 0;

        uint32_t gap = cw_apply_gap_debt((uint32_t)dit);
        cw_next_event_ms = now + gap;

        cw_intra_symbol_gap = true;
        return;
    }

    if (cw_intra_symbol_gap && cw_pat) {
        cw_pat_idx++;
        cw_intra_symbol_gap = false;

        if (cw_pat[cw_pat_idx] != '\0') {
            cw_state = CW_TONE_ON;
            cw_tone_on();

            cw_last_on_extra_ms = 0;
            uint16_t dur = cw_elem_play_ms(cw_pat[cw_pat_idx], &cw_last_on_extra_ms);
            cw_next_event_ms = now + dur;
            return;
        }

        cw_pat = NULL;
        cw_state = CW_GAP;

        uint32_t gap = cw_apply_gap_debt((uint32_t)(2u * dit));
        cw_next_event_ms = now + gap;
        return;
    }

    cw_start_next_char();
}

/* ─────────────────────────────────────────────
 * Startup animation (unchanged)
 * ───────────────────────────────────────────── */
#define START_BAR_STEP_MS   74u
#define START_BAR_STEPS     19u
#define START_TOTAL_MS      (START_BAR_STEP_MS * START_BAR_STEPS)

#define START_STAGE0_END    (START_TOTAL_MS / 4u)
#define START_STAGE1_END    (2u * (START_TOTAL_MS / 4u))
#define START_STAGE2_END    (3u * (START_TOTAL_MS / 4u))
#define START_STAGE3_END    (START_TOTAL_MS)

static bool     g_startup_running = false;
static uint32_t g_startup_t0      = 0;

/* ─────────────────────────────────────────────
 * Mode switching helper
 * ───────────────────────────────────────────── */
static inline void set_display_mode(display_mode_t mode) {
    g_mode = mode;

    if (g_mode == MODE_DEFAULT) {
        segments_all_off();
        max7219_blank_all();

        g_bar_activity = 0;
        g_bar_last_decay = timer_read32();
        g_bar_last_render = 0;

        // counter keeps running; do not reset unless you want it reset here

        cw_edit_active = false;
        cw_edit_len = 0;
        cw_edit_buf[0] = '\0';

        cw_q_r = cw_q_w = 0;
        cw_queue_dits_total = 0;

        cw_state = CW_IDLE;
        cw_queue_locked = false;
        cw_err_flash_until = 0;
        cw_bar_last_update = 0;
        cw_bar_cached_level = 0;

        cw_tone_preview = false;
        cw_gap_debt_ms = 0;
        cw_last_on_extra_ms = 0;

        cw_audio_hard_stop();

    } else if (g_mode == MODE_CW) {
        segments_all_off();
        max7219_blank_all();
        max7219_force_on();

        cw_wpm = CW_DEFAULT_WPM;

        cw_edit_active = false;
        cw_edit_len = 0;
        cw_edit_buf[0] = '\0';

        cw_q_r = cw_q_w = 0;
        cw_queue_dits_total = 0;

        cw_state = CW_IDLE;
        cw_queue_locked = false;
        cw_err_flash_until = 0;
        cw_bar_last_update = timer_read32();
        cw_bar_cached_level = 0;

        cw_tone_preview = false;
        cw_gap_debt_ms = 0;
        cw_last_on_extra_ms = 0;

        cw_audio_hard_stop();

        cw_display_wpm(cw_wpm);
    }
}

/* ─────────────────────────────────────────────
 * Init hooks
 * ───────────────────────────────────────────── */
void keyboard_pre_init_user(void) {
    setPinOutput(BAR_SER_PIN);
    setPinOutput(BAR_SRCLK_PIN);
    setPinOutput(BAR_RCLK_PIN);

    setPinOutput(GP4); // seg 2
    setPinOutput(GP5); // seg 1

    cw_audio_hard_stop();

    segments_all_off();
    g_mode = MODE_DEFAULT;

    g_bar_activity = 0;
    g_bar_last_decay = timer_read32();
    g_bar_last_render = 0;
}

void keyboard_post_init_user(void) {
    setPinOutput(MAX7219_CS_PIN);
    writePinHigh(MAX7219_CS_PIN);

    max7219_init();

    g_startup_running = true;
    g_startup_t0 = timer_read32();

    segments_all_off();
    max7219_blank_all();
    max7219_force_on();

    g_mode = MODE_DEFAULT;
}

/* ─────────────────────────────────────────────
 * Startup animation tasks (unchanged)
 * ───────────────────────────────────────────── */
static void startup_bar_task(void) {
    uint32_t now = timer_read32();
    uint32_t elapsed = TIMER_DIFF_32(now, g_startup_t0);

    if (elapsed >= START_TOTAL_MS) {
        segments_all_off();
        return;
    }

    uint32_t step = elapsed / START_BAR_STEP_MS;
    if (step >= START_BAR_STEPS) step = START_BAR_STEPS - 1;

    uint8_t seg = (step <= 9) ? (uint8_t)(1u + step) : (uint8_t)(19u - step);

    segments_all_off();
    segment_set(seg, true);
}

static void startup_max_task(void) {
    uint32_t now = timer_read32();
    uint32_t elapsed = TIMER_DIFF_32(now, g_startup_t0);

    if (elapsed >= START_TOTAL_MS) {
        max7219_blank_all();
        return;
    }

    max7219_force_on();
    max7219_set_decode(true); // startup uses digits

    uint8_t stage = 0;
    if (elapsed < START_STAGE0_END) stage = 0;
    else if (elapsed < START_STAGE1_END) stage = 1;
    else if (elapsed < START_STAGE2_END) stage = 2;
    else stage = 3;

    uint8_t out[4] = {0x0F, 0x0F, 0x0F, 0x0F};
    out[0] = 3;
    if (stage >= 1) out[1] = 2;
    if (stage >= 2) out[2] = 1;
    if (stage >= 3) out[3] = 0;

    for (uint8_t d = 0; d < MAX7219_NUM_DIGITS && d < 4; d++) max7219_write_digit(d, out[d]);
}

/* ─────────────────────────────────────────────
 * Tasks
 * ───────────────────────────────────────────── */
void housekeeping_task_user(void) {
    if (g_startup_running) {
        startup_bar_task();
        return;
    }

    if (g_mode == MODE_CW) {
        cw_bar_update_time_remaining();
        return;
    }

    if (g_mode == MODE_DEFAULT) {
        // NOTE: FN's bar animation is handled in default_display_task()
        default_bar_task();
        return;
    }

    if (g_mode != MODE_TEST) return;

    static uint32_t last_step = 0;
    static uint8_t seg = 1;
    static bool invert_pass = false;

    if (timer_elapsed(last_step) < 333) return;
    last_step = timer_read();

    if (!invert_pass) {
        segments_all_off();
        segment_set(seg, true);
    } else {
        segments_all_on();
        segment_set(seg, false);
    }

    seg++;
    if (seg > 10) {
        seg = 1;
        invert_pass = !invert_pass;
    }
}

void matrix_scan_user(void) {
    if (g_startup_running) {
        startup_max_task();

        uint32_t now = timer_read32();
        uint32_t elapsed = TIMER_DIFF_32(now, g_startup_t0);
        if (elapsed >= START_TOTAL_MS) {
            g_startup_running = false;
            segments_all_off();
            max7219_blank_all();
            set_display_mode(MODE_DEFAULT);
        }
        return;
    }

    if (g_mode == MODE_DEFAULT) {
        default_display_task();
        return;
    }

    if (g_mode == MODE_CW) {
        cw_task();
        return;
    }

    if (g_mode != MODE_TEST) return;

    #define MAX7219_STEP_MS 200

    static uint32_t last = 0;
    static uint8_t phase = 0;
    static uint8_t val = 0;

    uint32_t now = timer_read32();
    if (TIMER_DIFF_32(now, last) < MAX7219_STEP_MS) return;
    last = now;

    max7219_force_on();
    max7219_set_decode(true);

    uint8_t out[4] = {0x0F, 0x0F, 0x0F, 0x0F};

    if (phase < 4) {
        if (phase < MAX7219_NUM_DIGITS) out[phase] = val;
    } else {
        for (uint8_t d = 0; d < MAX7219_NUM_DIGITS && d < 4; d++) out[d] = val;
    }

    for (uint8_t d = 0; d < MAX7219_NUM_DIGITS && d < 4; d++) max7219_write_digit(d, out[d]);

    val++;
    if (val > 9) {
        val = 0;
        phase++;
        if (phase > 4) phase = 0;
    }
}

/* ─────────────────────────────────────────────
 * WPM commit helper
 * ───────────────────────────────────────────── */
static void cw_commit_wpm_from_buf(void) {
    if (cw_edit_len != 2) return;

    uint16_t v = (uint16_t)((cw_edit_buf[0] - '0') * 10u + (uint16_t)(cw_edit_buf[1] - '0'));
    if (v < 1) v = 1;
    if (v > 99) v = 99;

    cw_wpm = v;

    cw_edit_active = false;
    cw_edit_len = 0;
    cw_edit_buf[0] = '\0';

    cw_display_wpm(cw_wpm);
}

/* ─────────────────────────────────────────────
 * Macro handler
 * ───────────────────────────────────────────── */
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // DEFAULT mode: every keypress boosts bar + increments counter
    if (record->event.pressed && !g_startup_running && g_mode == MODE_DEFAULT) {
        bar_activity_boost();
        g_keypress_count++;
        if (g_keypress_count >= 10000u) g_keypress_count = 0;
    }

    // MOMENTARY TONE: handle on press + release
    if (keycode == TONE) {
        if (g_mode != MODE_CW) return false;

        if (record->event.pressed) {
            cw_tone_preview = true;
            cw_state = CW_IDLE;
            cw_pat = NULL;
            cw_intra_symbol_gap = false;
            cw_tone_on();
        } else {
            cw_tone_preview = false;
            cw_tone_off();
        }
        return false;
    }

    // If it's a release event (and not TONE), let it pass
    if (!record->event.pressed) return true;

    // CW edit capture (CW mode only)
    if (g_mode == MODE_CW && cw_edit_active) {
        if (keycode == WPMT) {
            cw_edit_active = false;
            cw_edit_len = 0;
            cw_edit_buf[0] = '\0';
            cw_display_wpm(cw_wpm);
            return false;
        } else if (is_digit_kc(keycode)) {
            if (cw_edit_len < 2) {
                cw_edit_buf[cw_edit_len++] = digit_from_kc(keycode);
                cw_edit_buf[cw_edit_len] = '\0';
            }
            if (cw_edit_len == 2) {
                cw_commit_wpm_from_buf();
            }
            return false;
        } else if (keycode == KC_BSPC) {
            if (cw_edit_len > 0) {
                cw_edit_len--;
                cw_edit_buf[cw_edit_len] = '\0';
            }
            return false;
        } else {
            return false;
        }
    }

    // Your display/CW control keys (existing behavior)
    switch (keycode) {
        case LEDT:
            if (g_startup_running) return false;
            set_display_mode((g_mode == MODE_TEST) ? MODE_DEFAULT : MODE_TEST);
            return false;

        case CWMD:
            if (g_startup_running) return false;
            set_display_mode((g_mode == MODE_CW) ? MODE_DEFAULT : MODE_CW);
            return false;

        case WPMT:
            if (g_mode != MODE_CW) return false;

            cw_edit_active = true;
            cw_edit_len = 0;
            cw_edit_buf[0] = '\0';

            cw_display_wpm(0);
            return false;

        case TNUP:
            if (g_mode != MODE_CW) return false;
            cw_tone_hz = clampf(cw_tone_hz + CW_TONE_STEP_HZ, CW_TONE_MIN_HZ, CW_TONE_MAX_HZ);
            cw_tone_restart_if_running();
            return false;

        case TNDN:
            if (g_mode != MODE_CW) return false;
            cw_tone_hz = clampf(cw_tone_hz - CW_TONE_STEP_HZ, CW_TONE_MIN_HZ, CW_TONE_MAX_HZ);
            cw_tone_restart_if_running();
            return false;
    }

    // Your custom “user macros”
    switch (keycode) {
        case TEST:
            SEND_STRING("TEST");
            break;
        case VDRT:
            SEND_STRING(SS_LGUI(SS_DOWN(X_LCTL) SS_TAP(X_RIGHT) SS_UP(X_LCTL)));
            break;
        case VDLT:
            SEND_STRING(SS_LGUI(SS_DOWN(X_LCTL) SS_TAP(X_LEFT) SS_UP(X_LCTL)));
            break;
        case VDUP:
            SEND_STRING(SS_LGUI(SS_TAP(X_TAB) SS_TAP(X_T)));
            break;
        case VDDN:
            SEND_STRING(SS_LGUI("d"));
            break;
        case EXPL:
            SEND_STRING(SS_LGUI("e"));
            break;
        case SNIP:
            SEND_STRING(SS_LGUI(SS_LSFT("s")));
            break;
        case COPY:
            SEND_STRING(SS_TAP(X_F2) SS_LCTL(SS_TAP(X_A) SS_TAP(X_C)));
            break;
        case CUTT:
            SEND_STRING(SS_TAP(X_F2) SS_LCTL(SS_TAP(X_A) SS_TAP(X_X)));
            break;
        case PAST:
            SEND_STRING(SS_TAP(X_F2) SS_LCTL(SS_TAP(X_V)));
            break;
        case TASK:
            SEND_STRING(SS_LCTL(SS_DOWN(X_RSFT) SS_TAP(X_ESCAPE) SS_UP(X_RSFT)));  // Windows
            SEND_STRING(SS_LCTL(SS_DOWN(X_LALT) SS_TAP(X_ESCAPE) SS_UP(X_LALT)));    // Mac
            break;
        case GOGL:
            SEND_STRING(SS_LCTL(SS_LALT(SS_TAP(X_G))));
            break;
        case CMD:
            SEND_STRING(SS_LCTL(SS_LALT(SS_TAP(X_T))));
            break;
        case QUIT:
            SEND_STRING(SS_LALT(SS_TAP(X_F4)));     // Windows
            SEND_STRING(SS_LCTL(SS_TAP(X_W)));      // Mac
            break;
        case DZRO:
            SEND_STRING("00");
            break;
        case ATAB:
            SEND_STRING(SS_LALT(SS_TAP(X_TAB)));
            break;
        case LPRC:
            SEND_STRING("(");
            break;
        case RPRC:
            SEND_STRING(")");
            break;
        case NUM:
            SEND_STRING(SS_TAP(X_NUM));         // Toggles layer change and presses the num lock button, to allow for Num lock toggle to occur for both Mac OS and Windows
            return true;
            break;
        // Discord Section
        case THIS:
            SEND_STRING(":this:" SS_TAP(X_ENTER));
            break;
        case OOF:
            SEND_STRING(":oof:" SS_TAP(X_ENTER));
            break;
        case PIKA:
            SEND_STRING(":surprised:" SS_TAP(X_ENTER));
            break;
        case SCAT:
            SEND_STRING(":smiley_cat2:" SS_TAP(X_ENTER));
            break;
        case FCAT:
            SEND_STRING(":smiley_cat1:" SS_TAP(X_ENTER));
            break;
        case HART:
            SEND_STRING(":heart:" SS_TAP(X_ENTER));
            break;
        case DROL:
            SEND_STRING(":drooling_face:" SS_TAP(X_ENTER));
            break;
        case MONY:
            SEND_STRING(":smiley_take:" SS_TAP(X_ENTER));
            break;
        case FHAT:
            SEND_STRING(":smiley_fedora:" SS_TAP(X_ENTER));
            break;
        case SPIT:
            SEND_STRING(":smiley_spit:" SS_TAP(X_ENTER));
            break;
        case KING:
            SEND_STRING(":pepe_king:" SS_TAP(X_ENTER));
            break;
        case FLEX:
            SEND_STRING(":muscle:" SS_TAP(X_ENTER));
            break;
        case HAHA:
            SEND_STRING(":smiley_kekw:" SS_TAP(X_ENTER));
            break;
        case LCRY:
            SEND_STRING(":joy:" SS_TAP(X_ENTER));
            break;
        case NICE:
            SEND_STRING(":nice~2:" SS_TAP(X_ENTER));
            break;
        case PPOG:
            SEND_STRING(":pepe_pog:" SS_TAP(X_ENTER));
            break;
        case PRAY:
            SEND_STRING(":pray_tone2:" SS_TAP(X_ENTER));
            break;
        case SWET:
            SEND_STRING(":sweat_smile:" SS_TAP(X_ENTER));
            break;
        case COLD:
            SEND_STRING(":cold_face:" SS_TAP(X_ENTER));
            break;
        case THUM:
            SEND_STRING(":thumbsup:" SS_TAP(X_ENTER));
            break;
    }

    // CW typing queue behavior (unchanged)
    if (g_mode == MODE_CW) {
        if (cw_tone_preview) return true;

        if (keycode == KC_BSPC) {
            cw_q_pop_last();
            return false;
        }

        if (cw_queue_locked) {
            if ((keycode >= KC_A && keycode <= KC_Z) || is_digit_kc(keycode) || keycode == KC_SPC) {
                cw_error_beep_and_flash();
                return false;
            }
            return true;
        }

        if ((keycode >= KC_A && keycode <= KC_Z) || is_digit_kc(keycode) || keycode == KC_SPC) {
            if (cw_q_full()) {
                cw_error_beep_and_flash();
                return false;
            }
        }

        if (keycode >= KC_A && keycode <= KC_Z) {
            cw_q_push((char)('A' + (keycode - KC_A)));
        } else if (is_digit_kc(keycode)) {
            cw_q_push(digit_from_kc(keycode));
        } else if (keycode == KC_SPC) {
            cw_q_push(' ');
        }

        return true;
    }

#ifdef CONSOLE_ENABLE // Console Debug
    uprintf("KL: kc: 0x%04X, col: %u, row: %u, pressed: %1u, time: %u, interrupt: %1u, count: %u\n",
            keycode, record->event.key.col, record->event.key.row, record->event.pressed, record->event.time,
            record->tap.interrupted, record->tap.count);
#endif

    return true;
}

/* ─────────────────────────────────────────────
 * Keymaps
 * ───────────────────────────────────────────── */
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

[_BASE] = LAYOUT(
     KC_ESC,     KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSPC, KC_BSPC,   CWMD,
     KC_TAB,  KC_GRV,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC, KC_BSLS, CK_TOGG,
    KC_LCTL, KC_CAPS,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,    TONE,  KC_ENT, MU_TOGG,
    MO(_FN), KC_LSFT,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT, KC_RALT, KC_RGUI,
                                                           KC_SPC
),

[_FN] = LAYOUT(
    QK_BOOT,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,  KC_F12,  KC_DEL,  KC_DEL,    WPMT,
       SNIP, _______, _______, _______, _______, _______, _______, _______, _______, KC_PAUS, KC_SCRL, KC_PSCR,   CK_UP,    COPY,    PAST, _______,
   MO(_FN2), _______, C(KC_A), C(KC_S), C(KC_D), C(KC_F), _______, _______, _______, KC_HOME, KC_PGUP, KC_LEFT, KC_RGHT, _______, _______, _______,
    _______, _______, C(KC_Z), C(KC_X), C(KC_C), C(KC_V), _______, _______, _______,  KC_END, KC_PGDN, KC_DOWN, _______,    TNDN,    TNUP,
                                                             LEDT
),

[_FN2] = LAYOUT(
    QK_BOOT, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_VOLD, KC_VOLU, _______, _______,    VDUP,
    _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_MPRV, KC_MNXT, KC_MPLY, _______,    VDLT,    VDRT,
                                                          _______
),
};

const uint8_t music_map[MATRIX_ROWS][MATRIX_COLS] = LAYOUT(
    45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,  0,  0,  0,
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
                              0
);
