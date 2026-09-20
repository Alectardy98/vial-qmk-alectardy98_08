#include QMK_KEYBOARD_H

#ifdef RGB_MATRIX_ENABLE
#    include "color.h"    // hsv_to_rgb()
#    include "eeconfig.h"
#    include "timer.h"
#    ifdef __AVR__
#        include <avr/pgmspace.h>
#    endif
#endif

enum _layer {
    _BASE,
    _FN
};

enum custom_keycodes {
    MODE = QK_KB_0,
    RSET,
    LOGO,
    SCRN,
    CHNA,
    CHNB,
    MIXD,
    PRVW,
    CWHT,
    CGRY,
    CRED,
    CMAG,
    CBLU,
    CCYN,
    CGRN,
    CYLW,
    NKBD,
    NCHA,
    NCHB,
    VDRT,                   //Desktop Right "set to move right a space on mac"
    VDLT,                   //Desktop Left "set to move left a space on mac"
    VDUP,                   //V-Desktop Up "set to mission controll on mac"
    VDDN,                   //Min All "set to show desktop on mac"
    EXPL,                   //New Explorer On Windows "set to show launchpad on mac"
    SNIP,                   //Snip Tool Windows "set to save picture of selected area on mac"
    COPY,                   //Copy All
    CUTT,                   //Cut All
    PAST,                   //Paste Over
    TASK,                   //Task Manager
    QUIT,                   //Close App
    //Discord Section
    THIS,                   //Macro for ":this:" emoji on discord
    OOF,                    //Macro for ":oof:" emoji on discord
    PIKA,                   //Macro for ":surprised:" emoji on discord
    SCAT,                   //Macro for ":smiley_cat2:" emoji on discord
    FCAT,                   //Macro for ":smiley_cat1:" emoji on discord
    HART,                   //Macro for ":heart:" emoji on discord
    DROL,                   //Macro for ":drooling_face:" emoji on discord
    MONY,                   //Macro for ":smiley_take:" emoji on discord
    FHAT,                   //Macro for ":smiley_fedora:" emoji on discord
    SPIT,                   //Macro for ":smiley_spit:" emoji on discord
    KING,                   //Macro for ":pepe_king:" emoji on discord
    FLEX,                   //Macro for ":muscle:" emoji on discord
    HAHA,                   //Macro for ":smiley_kekw:" emoji on discord
    LCRY,                   //Macro for ":joy:" emoji on discord
    NICE,                   //Macro for ":nice~2:" emoji on discord
    PPOG,                   //Macro for ":pepe_pog:" emoji on discord
    PRAY,                   //Macro for ":pray_tone2:" emoji on discord
    SWET,                   //Macro for ":sweat_smile:" emoji on discord
    COLD,                   //Macro for ":cold_face:" emoji on discord
    THUM,                   //Macro for ":thumbsup:" emoji on discord
};

// ---------------- Minimal keymaps (placeholders) ----------------
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
         KC_ESC,    TASK,   KC_F1,   KC_F2,    CWHT,    CGRY,    CRED,    CMAG,    CBLU,    CCYN,    CGRN,    CYLW,    LOGO,    SCRN,    CHNA,    CHNB,    MIXD,    PRVW,    MODE,      KC_INS, KC_HOME, KC_PGUP,  KC_F13,
        DM_PLY1, DM_PLY2,   KC_F3,   KC_F4,  KC_GRV,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINS,  KC_EQL, KC_BSPC, KC_BSPC,      KC_DEL,  KC_END, KC_PGDN,  KC_F14,
        _______, _______,   KC_F5,   KC_F6,  KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P, KC_LBRC, KC_RBRC, KC_BSLS,                         KC_UP,
           SNIP,    EXPL,   KC_F7,   KC_F8, KC_CAPS,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,  KC_ENT,  KC_ENT,              KC_LEFT, KC_DOWN, KC_RGHT,
           COPY,    PAST,   KC_F9,  KC_F10, KC_LSFT,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT, KC_RCTL,                                KC_DOWN,
           VDLT,    VDRT,  KC_F11,  KC_F12,          MO(_FN),                                     KC_SPC,                                      KC_RALT,                                   VDLT,    VDRT,    VDUP
    ),
    [_FN] = LAYOUT(
        QK_BOOT, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,    NKBD,    NCHA,    NCHB, _______, _______,    RSET,     _______, _______, _______, _______,
        DM_REC1, DM_REC2, _______, _______,  KC_ESC,   KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,  KC_F12,  KC_DEL,  KC_DEL,     _______, _______, _______, _______,
        _______, _______, _______, _______,    SNIP,    THIS,     OOF,    PIKA,    SCAT,    FCAT,    HART,    DROL,    MONY,    FHAT,    SPIT, _______, _______, _______,                          VDUP,
        _______, _______, _______, _______, KC_LCTL, C(KC_A), C(KC_S), C(KC_D), C(KC_F), _______, _______, _______, _______, _______, KC_VOLD, KC_VOLU, _______, _______,                 VDLT, _______,    VDRT,
        _______, _______, _______, _______, _______, C(KC_Z), C(KC_X), C(KC_C), C(KC_V), _______, _______, _______, KC_MPRV, KC_MNXT, KC_MPLY, _______, KC_LGUI,                                   VDDN,
        _______, _______, _______, _______,          _______,                                     _______,                                     _______,                                _______, _______, _______
    ),
};

#ifdef RGB_MATRIX_ENABLE

#ifndef SECTION_MODE_MAX_VAL
#    define SECTION_MODE_MAX_VAL 100
#endif

// ---- Display groups (0–29) ----
static const uint8_t PROGMEM LEDS_QB_LOGO[]    = { 0, 1, 2 };
static const uint8_t PROGMEM LEDS_SCREEN[]     = { 3, 4, 5 };
static const uint8_t PROGMEM LEDS_CH_A[]       = { 6, 7, 8, 9, 10, 11, 12, 13 };
static const uint8_t PROGMEM LEDS_CH_B[]       = { 14, 15, 16, 17, 18, 19, 20, 21 };
static const uint8_t PROGMEM LEDS_AB_MIX[]     = { 22, 23, 24 };
static const uint8_t PROGMEM LEDS_AB_PREVIEW[] = { 25, 26, 27, 28, 29 };

// ---- Number sequences (order = 5,3,1,2,4,6) ----
static const uint8_t PROGMEM SEQ_KBD[6] = { 30, 31, 32, 33, 34, 35 };
static const uint8_t PROGMEM SEQ_A[6]   = { 36, 37, 38, 39, 40, 41 };
static const uint8_t PROGMEM SEQ_B[6]   = { 42, 43, 44, 45, 46, 47 };

// --- Section bit positions ---
enum section_bits {
    BIT_QB_LOGO = 0,
    BIT_SCREEN,
    BIT_CH_A,
    BIT_CH_B,
    BIT_AB_MIX,
    BIT_AB_PREVIEW,
};

// Preset: logo+screen+mix+preview on; A/B off
#define PRESET_SECTION_MASK ((1u << BIT_QB_LOGO) | (1u << BIT_SCREEN) | (1u << BIT_AB_MIX) | (1u << BIT_AB_PREVIEW))

// ---- Color palette index ----
enum disp_color_idx {
    DISP_WHT = 0,
    DISP_GRY,
    DISP_RED,
    DISP_MAG,
    DISP_BLU,
    DISP_CYN,
    DISP_GRN,
    DISP_YLW,
};

// ---- Per-section color storage ----
enum disp_group {
    GRP_SCREEN = 0,
    GRP_CH_A,
    GRP_CH_B,
    GRP_AB_MIX,
    GRP_AB_PREVIEW,
    GRP_COUNT
};

static bool    section_mode = true; // start in custom mode
static uint8_t section_mask = 0;    // 6 bits

static uint8_t grp_idx[GRP_COUNT] = { DISP_RED, DISP_RED, DISP_RED, DISP_RED, DISP_RED };
static uint8_t grp_r[GRP_COUNT]   = { 255, 255, 255, 255, 255 };
static uint8_t grp_g[GRP_COUNT]   = {   0,   0,   0,   0,   0 };
static uint8_t grp_b[GRP_COUNT]   = {   0,   0,   0,   0,   0 };

// ---- Numbers animation enables (3 bits) ----
// bit0: KBD, bit1: A, bit2: B
#define ANIM_KBD (1u << 0)
#define ANIM_A   (1u << 1)
#define ANIM_B   (1u << 2)
static uint8_t anim_mask = (ANIM_KBD | ANIM_A | ANIM_B); // default ON for all

// Save/restore Vial/native RGB state when toggling modes
static bool    saved_enabled = false;
static uint8_t saved_mode    = 0;
static uint8_t saved_hue     = 0;
static uint8_t saved_sat     = 0;
static uint8_t saved_val     = 0;

// ---- Number animation state ----
#ifndef NUM_STEP_MS
#    define NUM_STEP_MS 250
#endif
static uint32_t num_timer = 0;
static uint8_t  num_step  = 0;

// ---- Persistent config in eeconfig_user() ----
#define USERCFG_SIG       0xA5u
#define USERCFG_SIG_SHIFT 24u

// ---- Debounce EEPROM writes to avoid freezes ----
#ifndef USERCFG_SAVE_DELAY_MS
#    define USERCFG_SAVE_DELAY_MS 500
#endif
static bool     usercfg_dirty = false;
static uint32_t usercfg_timer = 0;

// ---- FN override mask (render-time only, no EEPROM writes) ----
static inline uint8_t effective_section_mask(void) {
    uint8_t m = section_mask;
    if (layer_state_is(_FN)) {
        m |= (1u << BIT_CH_A);
        m |= (1u << BIT_CH_B);
    }
    return m;
}

// ---------- Color helpers ----------
static inline void idx_to_rgb(uint8_t idx, uint8_t *r, uint8_t *g, uint8_t *b) {
    switch (idx & 0x07) {
        case DISP_WHT: *r = 255; *g = 255; *b = 255; break;
        case DISP_GRY: *r = 128; *g = 128; *b = 128; break;
        case DISP_RED: *r = 255; *g =   0; *b =   0; break;
        case DISP_MAG: *r = 255; *g =   0; *b = 255; break;
        case DISP_BLU: *r =   0; *g =   0; *b = 255; break;
        case DISP_CYN: *r =   0; *g = 255; *b = 255; break;
        case DISP_GRN: *r =   0; *g = 255; *b =   0; break;
        case DISP_YLW: *r = 255; *g = 255; *b =   0; break;
        default:       *r = 255; *g =   0; *b =   0; break;
    }
}

static inline void set_group_idx(enum disp_group g, uint8_t idx) {
    grp_idx[g] = idx & 0x07;
    idx_to_rgb(grp_idx[g], &grp_r[g], &grp_g[g], &grp_b[g]);
}

// Apply a color to ONLY the sections that are currently lit
static inline void apply_color_to_currently_lit(uint8_t idx) {
    uint8_t m = effective_section_mask();
    if (m & (1u << BIT_SCREEN))     set_group_idx(GRP_SCREEN,     idx);
    if (m & (1u << BIT_CH_A))       set_group_idx(GRP_CH_A,       idx);
    if (m & (1u << BIT_CH_B))       set_group_idx(GRP_CH_B,       idx);
    if (m & (1u << BIT_AB_MIX))     set_group_idx(GRP_AB_MIX,     idx);
    if (m & (1u << BIT_AB_PREVIEW)) set_group_idx(GRP_AB_PREVIEW, idx);
}

static inline void toggle_anim(uint8_t bitmask) { anim_mask ^= bitmask; }
static inline bool anim_on(uint8_t bitmask) { return (anim_mask & bitmask) != 0; }

// ---------- EEPROM pack/unpack ----------
// Layout:
//   bits 0..5    section_mask
//   bits 6..8    screen idx
//   bits 9..11   ch_a idx
//   bits 12..14  ch_b idx
//   bits 15..17  ab_mix idx
//   bits 18..20  ab_prev idx
//   bits 21..23  anim_mask (3 bits)
//   bits 24..31  signature
static inline uint32_t pack_user_cfg(void) {
    uint32_t v = ((uint32_t)USERCFG_SIG << USERCFG_SIG_SHIFT);
    v |= (uint32_t)(section_mask & 0x3F);
    v |= ((uint32_t)(grp_idx[GRP_SCREEN]     & 0x07)) << 6;
    v |= ((uint32_t)(grp_idx[GRP_CH_A]       & 0x07)) << 9;
    v |= ((uint32_t)(grp_idx[GRP_CH_B]       & 0x07)) << 12;
    v |= ((uint32_t)(grp_idx[GRP_AB_MIX]     & 0x07)) << 15;
    v |= ((uint32_t)(grp_idx[GRP_AB_PREVIEW] & 0x07)) << 18;
    v |= ((uint32_t)(anim_mask              & 0x07)) << 21;
    return v;
}

static inline bool unpack_user_cfg(uint32_t v) {
    uint8_t sig = (uint8_t)(v >> USERCFG_SIG_SHIFT);
    if (sig != (uint8_t)USERCFG_SIG) return false;

    section_mask = (uint8_t)(v & 0x3F);

    set_group_idx(GRP_SCREEN,     (uint8_t)((v >> 6)  & 0x07));
    set_group_idx(GRP_CH_A,       (uint8_t)((v >> 9)  & 0x07));
    set_group_idx(GRP_CH_B,       (uint8_t)((v >> 12) & 0x07));
    set_group_idx(GRP_AB_MIX,     (uint8_t)((v >> 15) & 0x07));
    set_group_idx(GRP_AB_PREVIEW, (uint8_t)((v >> 18) & 0x07));

    anim_mask = (uint8_t)((v >> 21) & 0x07);
    if (anim_mask == 0) anim_mask = (ANIM_KBD | ANIM_A | ANIM_B); // sane default
    return true;
}

static inline void save_user_cfg_now(void) {
    eeconfig_update_user(pack_user_cfg());
    usercfg_dirty = false;
}

static inline void request_user_cfg_save(void) {
    usercfg_dirty = true;
    usercfg_timer = timer_read32();
}

void housekeeping_task_user(void) {
    if (usercfg_dirty && timer_elapsed32(usercfg_timer) >= USERCFG_SAVE_DELAY_MS) {
        save_user_cfg_now();
    }
}

// ---------- Preset ----------
static inline void apply_preset(void) {
    section_mask = (uint8_t)PRESET_SECTION_MASK;

    set_group_idx(GRP_SCREEN,     DISP_RED);
    set_group_idx(GRP_CH_A,       DISP_RED);
    set_group_idx(GRP_CH_B,       DISP_RED);
    set_group_idx(GRP_AB_MIX,     DISP_RED);
    set_group_idx(GRP_AB_PREVIEW, DISP_RED);

    anim_mask = (ANIM_KBD | ANIM_A | ANIM_B); // default: animated numbers ON

    num_timer = timer_read32();
    num_step  = 0;
}

static inline void enter_custom_mode(void) {
    saved_enabled = rgb_matrix_is_enabled();
    saved_mode    = rgb_matrix_get_mode();
    saved_hue     = rgb_matrix_get_hue();
    saved_sat     = rgb_matrix_get_sat();
    saved_val     = rgb_matrix_get_val();

    rgb_matrix_enable_noeeprom();
    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);

    uint8_t v = saved_val;
    if (v > SECTION_MODE_MAX_VAL) v = SECTION_MODE_MAX_VAL;
    rgb_matrix_sethsv_noeeprom(saved_hue, saved_sat, v);

    num_timer = timer_read32();
    num_step  = 0;
}

static inline void exit_custom_mode(void) {
    if (!saved_enabled) {
        rgb_matrix_disable_noeeprom();
        return;
    }
    rgb_matrix_mode_noeeprom(saved_mode);
    rgb_matrix_sethsv_noeeprom(saved_hue, saved_sat, saved_val);
}

void keyboard_post_init_user(void) {
    uint32_t u = eeconfig_read_user();
    if (!unpack_user_cfg(u)) {
        apply_preset();
        save_user_cfg_now();
    }
    section_mode = true;
    enter_custom_mode();
}

static inline void toggle_section_bit(uint8_t bit) { section_mask ^= (1u << bit); }

// ---- Small helpers (cheap on AVR) ----
static inline uint8_t scale8(uint8_t c, uint8_t v) { return (uint16_t)c * v / 255; }

static void paint_pgm_section_scaled(const uint8_t *arr_pgm, uint8_t len,
                                     uint8_t led_min, uint8_t led_max,
                                     uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t i = 0; i < len; i++) {
#ifdef __AVR__
        uint8_t idx = pgm_read_byte(&arr_pgm[i]);
#else
        uint8_t idx = arr_pgm[i];
#endif
        if (idx >= RGB_MATRIX_LED_COUNT) continue;
        if (idx >= led_min && idx < led_max) rgb_matrix_set_color(idx, r, g, b);
    }
}

static void paint_single_from_seq(const uint8_t *seq_pgm, uint8_t step,
                                  uint8_t led_min, uint8_t led_max,
                                  uint8_t r, uint8_t g, uint8_t b) {
#ifdef __AVR__
    uint8_t idx = pgm_read_byte(&seq_pgm[step]);
#else
    uint8_t idx = seq_pgm[step];
#endif
    if (idx >= RGB_MATRIX_LED_COUNT) return;
    if (idx >= led_min && idx < led_max) rgb_matrix_set_color(idx, r, g, b);
}

static void paint_all_from_seq(const uint8_t *seq_pgm, uint8_t len,
                               uint8_t led_min, uint8_t led_max,
                               uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t i = 0; i < len; i++) {
#ifdef __AVR__
        uint8_t idx = pgm_read_byte(&seq_pgm[i]);
#else
        uint8_t idx = seq_pgm[i];
#endif
        if (idx >= RGB_MATRIX_LED_COUNT) continue;
        if (idx >= led_min && idx < led_max) rgb_matrix_set_color(idx, r, g, b);
    }
}

// Rainbow only for the logo (3 LEDs)
static void paint_logo_rainbow(uint8_t led_min, uint8_t led_max, uint8_t v_cap) {
    uint8_t base_h = (uint8_t)((timer_read32() / 12) & 0xFF);

    for (uint8_t i = 0; i < (uint8_t)sizeof(LEDS_QB_LOGO); i++) {
#ifdef __AVR__
        uint8_t idx = pgm_read_byte(&LEDS_QB_LOGO[i]);
#else
        uint8_t idx = LEDS_QB_LOGO[i];
#endif
        if (idx >= RGB_MATRIX_LED_COUNT) continue;
        if (idx < led_min || idx >= led_max) continue;

        HSV hsv = { (uint8_t)(base_h + (i * 32)), 255, v_cap };
        RGB rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(idx, rgb.r, rgb.g, rgb.b);
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) return true;

    switch (keycode) {
        case MODE:
            section_mode = !section_mode;
            if (section_mode) enter_custom_mode();
            else              exit_custom_mode();
            return false;

        case RSET:
            apply_preset();
            save_user_cfg_now();
            return false;

        case LOGO: toggle_section_bit(BIT_QB_LOGO);    request_user_cfg_save(); return false;
        case SCRN: toggle_section_bit(BIT_SCREEN);     request_user_cfg_save(); return false;
        case CHNA: toggle_section_bit(BIT_CH_A);       request_user_cfg_save(); return false;
        case CHNB: toggle_section_bit(BIT_CH_B);       request_user_cfg_save(); return false;
        case MIXD: toggle_section_bit(BIT_AB_MIX);     request_user_cfg_save(); return false;
        case PRVW: toggle_section_bit(BIT_AB_PREVIEW); request_user_cfg_save(); return false;

        // recolor ONLY what's currently lit
        case CWHT: apply_color_to_currently_lit(DISP_WHT); request_user_cfg_save(); return false;
        case CGRY: apply_color_to_currently_lit(DISP_GRY); request_user_cfg_save(); return false;
        case CRED: apply_color_to_currently_lit(DISP_RED); request_user_cfg_save(); return false;
        case CMAG: apply_color_to_currently_lit(DISP_MAG); request_user_cfg_save(); return false;
        case CBLU: apply_color_to_currently_lit(DISP_BLU); request_user_cfg_save(); return false;
        case CCYN: apply_color_to_currently_lit(DISP_CYN); request_user_cfg_save(); return false;
        case CGRN: apply_color_to_currently_lit(DISP_GRN); request_user_cfg_save(); return false;
        case CYLW: apply_color_to_currently_lit(DISP_YLW); request_user_cfg_save(); return false;

        // number mode toggles per group
        case NKBD: toggle_anim(ANIM_KBD); request_user_cfg_save(); return false;
        case NCHA: toggle_anim(ANIM_A);   request_user_cfg_save(); return false;
        case NCHB: toggle_anim(ANIM_B);   request_user_cfg_save(); return false;
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
        case QUIT:
            SEND_STRING(SS_LALT(SS_TAP(X_F4)));     // Windows
            SEND_STRING(SS_LCTL(SS_TAP(X_W)));      // Mac
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
    return true;
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (!section_mode) return true;

    for (uint8_t i = led_min; i < led_max; i++) rgb_matrix_set_color(i, 0, 0, 0);

    if (timer_elapsed32(num_timer) >= NUM_STEP_MS) {
        num_timer = timer_read32();
        num_step = (num_step + 1) % 6;
    }

    uint8_t v = rgb_matrix_get_val();
    if (v > SECTION_MODE_MAX_VAL) v = SECTION_MODE_MAX_VAL;

    // white (for animated numbers)
    const uint8_t nwr = scale8(255, v);
    const uint8_t nwg = scale8(255, v);
    const uint8_t nwb = scale8(255, v);

    const uint8_t m = effective_section_mask();
#define BIT_ON(_bit) ((m & (1u << (_bit))) != 0)

    // logo rainbow
    if (BIT_ON(BIT_QB_LOGO)) paint_logo_rainbow(led_min, led_max, v);

    // per-section colors (scaled)
    const uint8_t scr_r = scale8(grp_r[GRP_SCREEN],     v);
    const uint8_t scr_g = scale8(grp_g[GRP_SCREEN],     v);
    const uint8_t scr_b = scale8(grp_b[GRP_SCREEN],     v);

    const uint8_t a_r   = scale8(grp_r[GRP_CH_A],       v);
    const uint8_t a_g   = scale8(grp_g[GRP_CH_A],       v);
    const uint8_t a_b   = scale8(grp_b[GRP_CH_A],       v);

    const uint8_t b_r   = scale8(grp_r[GRP_CH_B],       v);
    const uint8_t b_g   = scale8(grp_g[GRP_CH_B],       v);
    const uint8_t b_b   = scale8(grp_b[GRP_CH_B],       v);

    const uint8_t mx_r  = scale8(grp_r[GRP_AB_MIX],     v);
    const uint8_t mx_g  = scale8(grp_g[GRP_AB_MIX],     v);
    const uint8_t mx_b  = scale8(grp_b[GRP_AB_MIX],     v);

    const uint8_t pv_r  = scale8(grp_r[GRP_AB_PREVIEW], v);
    const uint8_t pv_g  = scale8(grp_g[GRP_AB_PREVIEW], v);
    const uint8_t pv_b  = scale8(grp_b[GRP_AB_PREVIEW], v);

    // sections
    if (BIT_ON(BIT_SCREEN))     paint_pgm_section_scaled(LEDS_SCREEN,     sizeof(LEDS_SCREEN),     led_min, led_max, scr_r, scr_g, scr_b);
    if (BIT_ON(BIT_CH_A))       paint_pgm_section_scaled(LEDS_CH_A,       sizeof(LEDS_CH_A),       led_min, led_max, a_r,   a_g,   a_b);
    if (BIT_ON(BIT_CH_B))       paint_pgm_section_scaled(LEDS_CH_B,       sizeof(LEDS_CH_B),       led_min, led_max, b_r,   b_g,   b_b);
    if (BIT_ON(BIT_AB_MIX))     paint_pgm_section_scaled(LEDS_AB_MIX,     sizeof(LEDS_AB_MIX),     led_min, led_max, mx_r,  mx_g,  mx_b);
    if (BIT_ON(BIT_AB_PREVIEW)) paint_pgm_section_scaled(LEDS_AB_PREVIEW, sizeof(LEDS_AB_PREVIEW), led_min, led_max, pv_r,  pv_g,  pv_b);

    // numbers:
    // if anim ON  -> one-at-a-time white
    // if anim OFF -> all 6 numbers in the group's section color
    if (BIT_ON(BIT_SCREEN)) {
        if (anim_on(ANIM_KBD)) paint_single_from_seq(SEQ_KBD, num_step, led_min, led_max, nwr, nwg, nwb);
        else                  paint_all_from_seq(SEQ_KBD, 6,       led_min, led_max, scr_r, scr_g, scr_b);
    }

    if (BIT_ON(BIT_CH_A)) {
        if (anim_on(ANIM_A))  paint_single_from_seq(SEQ_A, num_step, led_min, led_max, nwr, nwg, nwb);
        else                  paint_all_from_seq(SEQ_A, 6,       led_min, led_max, a_r, a_g, a_b);
    }

    if (BIT_ON(BIT_CH_B)) {
        if (anim_on(ANIM_B))  paint_single_from_seq(SEQ_B, num_step, led_min, led_max, nwr, nwg, nwb);
        else                  paint_all_from_seq(SEQ_B, 6,       led_min, led_max, b_r, b_g, b_b);
    }

    return false;
}

#endif // RGB_MATRIX_ENABLE
