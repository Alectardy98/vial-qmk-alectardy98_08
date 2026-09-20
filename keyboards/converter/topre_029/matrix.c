#include "matrix.h"
#include "ps2.h"
#include "print.h"
#include "timer.h"

/*
 * Stage 3 mapper: ordinary Set-2, E0-prefixed keys, composite green
 * keys (F1, F3-F12, Caps), and the two unusual Shift keys.
 *
 * Compact virtual matrix: 6 physical rows x 17 left-to-right positions.
 * Only scan codes/signatures present on this keyboard are mapped.
 */
static matrix_row_t matrix[MATRIX_ROWS];

#define SPECIAL_BASE 0x200
#define SP_F1        (SPECIAL_BASE | 1)
#define SP_F3        (SPECIAL_BASE | 3)
#define SP_F4        (SPECIAL_BASE | 4)
#define SP_F5        (SPECIAL_BASE | 5)
#define SP_F6        (SPECIAL_BASE | 6)
#define SP_F7        (SPECIAL_BASE | 7)
#define SP_F8        (SPECIAL_BASE | 8)
#define SP_F9        (SPECIAL_BASE | 9)
#define SP_F10       (SPECIAL_BASE | 10)
#define SP_F11       (SPECIAL_BASE | 11)
#define SP_F12       (SPECIAL_BASE | 12)
#define SP_CAPS      (SPECIAL_BASE | 20)
#define SP_LSHIFT    (SPECIAL_BASE | 21)
#define SP_RSHIFT    (SPECIAL_BASE | 22)
#define SP_PGM       (SPECIAL_BASE | 23)
#define SP_RCD       (SPECIAL_BASE | 24)
#define SP_FLD       (SPECIAL_BASE | 25)
#define SP_KANA      (SPECIAL_BASE | 26)
#define SP_NUM       (SPECIAL_BASE | 27)
#define SP_ALPHA     (SPECIAL_BASE | 28)
#define SP_ALPHA2    (SPECIAL_BASE | 29)

__attribute__((weak)) void matrix_init_kb(void) { matrix_init_user(); }
__attribute__((weak)) void matrix_scan_kb(void) { matrix_scan_user(); }
__attribute__((weak)) void matrix_init_user(void) {}
__attribute__((weak)) void matrix_scan_user(void) {}

matrix_row_t matrix_get_row(uint8_t row) { return matrix[row]; }
void matrix_print(void) {}

static bool set_matrix_pos(uint8_t row, uint8_t col, bool pressed) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) return false;
    matrix_row_t mask = ((matrix_row_t)1) << col;
    bool old_state = (matrix[row] & mask) != 0;
    if (pressed) matrix[row] |= mask;
    else matrix[row] &= ~mask;
    return old_state != pressed;
}

/* Translate only scan codes/signatures that physically exist on this keyboard.
 * Matrix rows are the six physical keyboard rows; columns run left-to-right. */
static bool set_virtual_key(uint16_t value, bool pressed) {
    uint8_t row, col;
    switch (value) {
        /* physical row 0 */
        case 0x14: row=0; col=0; break; /* Esc */
        case SP_F1: row=0; col=3; break;
        case 0x09: row=0; col=4; break; /* F2 */
        case SP_F3: row=0; col=5; break;
        case SP_F4: row=0; col=6; break;
        case SP_F5: row=0; col=7; break;
        case SP_F6: row=0; col=8; break;
        case SP_F7: row=0; col=9; break;
        case SP_F8: row=0; col=10; break;
        case SP_F9: row=0; col=11; break;
        case SP_F10: row=0; col=12; break;
        case SP_F11: row=0; col=13; break;
        case SP_F12: row=0; col=14; break;

        /* physical row 1 */
        case 0x06: row=1; col=0; break;  case 0x05: row=1; col=1; break;
        case 0x16: row=1; col=2; break;  case 0x1E: row=1; col=3; break;
        case 0x26: row=1; col=4; break;  case 0x25: row=1; col=5; break;
        case 0x2E: row=1; col=6; break;  case 0x36: row=1; col=7; break;
        case 0x3D: row=1; col=8; break;  case 0x3E: row=1; col=9; break;
        case 0x46: row=1; col=10; break; case 0x45: row=1; col=11; break;
        case 0x4E: row=1; col=12; break; case 0x55: row=1; col=13; break;
        case 0x01: row=1; col=14; break; case 0x07: row=1; col=15; break;
        case 0x17D: row=1; col=16; break; /* F14 */

        /* physical row 2 */
        case SP_PGM: row=2; col=0; break; case SP_KANA: row=2; col=1; break;
        case 0x15: row=2; col=2; break; case 0x1D: row=2; col=3; break;
        case 0x24: row=2; col=4; break; case 0x2D: row=2; col=5; break;
        case 0x2C: row=2; col=6; break; case 0x35: row=2; col=7; break;
        case 0x3C: row=2; col=8; break; case 0x43: row=2; col=9; break;
        case 0x44: row=2; col=10; break; case 0x4D: row=2; col=11; break;
        case 0x54: row=2; col=12; break; case 0x5B: row=2; col=13; break;
        case 0x16B: row=2; col=14; break; case 0x174: row=2; col=15; break;
        case 0x0B: row=2; col=16; break;

        /* physical row 3 */
        case SP_RCD: row=3; col=0; break; case 0x170: row=3; col=1; break;
        case SP_CAPS: row=3; col=2; break; case 0x1C: row=3; col=3; break;
        case 0x1B: row=3; col=4; break; case 0x23: row=3; col=5; break;
        case 0x2B: row=3; col=6; break; case 0x34: row=3; col=7; break;
        case 0x33: row=3; col=8; break; case 0x3B: row=3; col=9; break;
        case 0x42: row=3; col=10; break; case 0x4B: row=3; col=11; break;
        case 0x4C: row=3; col=12; break; case 0x52: row=3; col=13; break;
        case 0x5D: row=3; col=14; break; case 0x78: row=3; col=15; break;
        case 0x83: row=3; col=16; break;

        /* physical row 4 */
        case SP_FLD: row=4; col=0; break; case SP_NUM: row=4; col=1; break;
        case 0x1A: row=4; col=2; break; case 0x22: row=4; col=3; break;
        case 0x21: row=4; col=4; break; case 0x2A: row=4; col=5; break;
        case 0x32: row=4; col=6; break; case 0x31: row=4; col=7; break;
        case 0x3A: row=4; col=8; break; case 0x41: row=4; col=9; break;
        case 0x49: row=4; col=10; break; case 0x4A: row=4; col=11; break;
        case 0x51: row=4; col=12; break; case SP_ALPHA2: row=4; col=13; break;
        case 0x0A: row=4; col=14; break;

        /* physical row 5 */
        case SP_ALPHA: row=5; col=0; break; case SP_LSHIFT: row=5; col=1; break;
        case 0x5A: row=5; col=2; break; case 0x29: row=5; col=3; break;
        case 0x0D: row=5; col=4; break; case SP_RSHIFT: row=5; col=5; break;
        case 0x169: row=5; col=6; break; case 0x17A: row=5; col=7; break;
        default: return false;
    }
    return set_matrix_pos(row, col, pressed);
}

static uint16_t green_key(uint8_t code) {
    switch (code) {
        case 0x3A: return SP_F1;
        case 0x33: return SP_F3;
        case 0x42: return SP_F4;
        case 0x1B: return SP_F5;
        case 0x2C: return SP_F6;
        case 0x22: return SP_F7;
        case 0x4B: return SP_F8;
        case 0x2D: return SP_F9;
        case 0x1C: return SP_F10;
        case 0x23: return SP_F11;
        case 0x34: return SP_F12;
        default:   return 0;
    }
}

void matrix_init(void) {
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) matrix[i] = 0;
    ps2_host_init();
    matrix_init_kb();
}

uint8_t matrix_scan(void) {
    enum { NORMAL, AFTER_E0, AFTER_F0, AFTER_E0_F0, AFTER_76, AFTER_F0_76,
           AFTER_F0_76_F0, AFTER_14, CAPS_HELD, CAPS_AFTER_F0 };
    static uint8_t state = NORMAL;
    bool changed = false;
    uint8_t code;
    /* Mystery keys emit a complete synthetic signature immediately before
     * the real key while held.  Decode that signature as a temporary Shift.
     * All six positions default to KC_LSFT in the mapper keymap. */
    enum { M_NONE, M_SIMPLE_F0, M_SIMPLE_END, M_E0_FIRST, M_E0_SECOND,
           M_E0_F0, M_E0_END, M_14_SECOND, M_ESC_END, M_14_F0_1, M_14_14,
           M_14_F0_2, M_14_END };
    static uint8_t mstate = M_NONE;
    static uint8_t mystery_code = 0;
    static bool mystery_active = false;
    static uint16_t esc_candidate_timer = 0;

    while ((code = ps2_host_recv()) != 0) {
        xprintf("%02X ", code);

        /* Recognize the six keyboard-internal modifier signatures and consume
         * them before the ordinary Set-2 parser can create phantom keys. */
        if (mstate != M_NONE) {
            if (mstate == M_SIMPLE_F0 && code == 0xF0) { mstate = M_SIMPLE_END; continue; }
            if (mstate == M_SIMPLE_END && code == mystery_code) {
                uint16_t sp = mystery_code == 0x04 ? SP_PGM : (mystery_code == 0x0C ? SP_RCD : SP_FLD);
                changed |= set_virtual_key(sp, true); mystery_active = true; mstate = M_NONE; continue;
            }
            if (mstate == M_E0_FIRST && (code == 0x75 || code == 0x72)) {
                mystery_code = code; mstate = M_E0_SECOND; continue;
            }
            if (mstate == M_E0_SECOND && code == 0xE0) { mstate = M_E0_F0; continue; }
            if (mstate == M_E0_F0 && code == 0xF0) { mstate = M_E0_END; continue; }
            if (mstate == M_E0_END && code == mystery_code) {
                changed |= set_virtual_key(mystery_code == 0x75 ? SP_KANA : SP_ALPHA2, true);
                mystery_active = true; mstate = M_NONE; continue;
            }
            if (mstate == M_14_SECOND && code == 0xF0) {
                changed |= set_virtual_key(0x14, true);
                mstate = M_ESC_END;
                continue;
            }
            if (mstate == M_ESC_END && code == 0x14) {
                changed |= set_virtual_key(0x14, false);
                mstate = M_NONE;
                xprintf("\n");
                continue;
            }
            if (mstate == M_14_SECOND && (code == 0x42 || code == 0x52)) {
                mystery_code = code; mstate = M_14_F0_1; continue;
            }
            if (mstate == M_14_SECOND && code == 0x51) {
                changed |= set_virtual_key(SP_CAPS, true);
                mstate = M_NONE; state = CAPS_HELD; continue;
            }
            if (mstate == M_14_F0_1 && code == 0xF0) { mstate = M_14_14; continue; }
            if (mstate == M_14_14 && code == 0x14) { mstate = M_14_F0_2; continue; }
            if (mstate == M_14_F0_2 && code == 0xF0) { mstate = M_14_END; continue; }
            if (mstate == M_14_END && code == mystery_code) {
                changed |= set_virtual_key(mystery_code == 0x42 ? SP_NUM : SP_ALPHA, true);
                mystery_active = true; mstate = M_NONE; continue;
            }
            /* Candidate did not complete. Preserve ordinary E0 keys. */
            if (mstate == M_E0_FIRST) {
                if (code == 0xF0) { state = AFTER_E0_F0; mstate = M_NONE; continue; }
                changed |= set_virtual_key(0x100 | code, true);
                mstate = M_NONE; continue;
            }
            mstate = M_NONE;
        }

        if (state == NORMAL && (code == 0x04 || code == 0x0C || code == 0x03)) {
            mystery_code = code; mstate = M_SIMPLE_F0; continue;
        }
        if (state == NORMAL && code == 0xE0) {
            mstate = M_E0_FIRST;
            continue;
        }
        if (state == NORMAL && code == 0x14) {
            mstate = M_14_SECOND;
            esc_candidate_timer = timer_read();
            continue;
        }

        switch (state) {
            case NORMAL:
                if (code == 0xE0) { state = AFTER_E0; break; }
                if (code == 0xF0) { state = AFTER_F0; break; }
                if (code == 0x76) { state = AFTER_76; break; }
                if (code == 0x14) { state = AFTER_14; break; }
                /* These Shift make bytes are only emitted when Shift was
                 * already held before another key.  Convert them to dedicated
                 * virtual Shift positions; their F0 releases are always sent. */
                if (code == 0x12) { changed |= set_virtual_key(SP_LSHIFT, true); break; }
                if (code == 0x59) { changed |= set_virtual_key(SP_RSHIFT, true); break; }
                changed |= set_virtual_key(code, true);
                break;

            case AFTER_E0:
                if (code == 0xF0) { state = AFTER_E0_F0; break; }
                changed |= set_virtual_key(0x100 | code, true);
                state = NORMAL;
                break;

            case AFTER_E0_F0:
                changed |= set_virtual_key(0x100 | code, false);
                if (mystery_active) {
                    changed |= set_virtual_key(SP_PGM, false);
                    changed |= set_virtual_key(SP_RCD, false);
                    changed |= set_virtual_key(SP_FLD, false);
                    changed |= set_virtual_key(SP_KANA, false);
                    changed |= set_virtual_key(SP_NUM, false);
                    changed |= set_virtual_key(SP_ALPHA, false);
                    changed |= set_virtual_key(SP_ALPHA2, false);
                    mystery_active = false;
                }
                state = NORMAL;
                xprintf("\n");
                break;

            case AFTER_F0:
                if (code == 0x76) { state = AFTER_F0_76; break; }
                if (code == 0x12) changed |= set_virtual_key(SP_LSHIFT, false);
                else if (code == 0x59) changed |= set_virtual_key(SP_RSHIFT, false);
                else changed |= set_virtual_key(code, false);
                if (mystery_active) {
                    changed |= set_virtual_key(SP_PGM, false);
                    changed |= set_virtual_key(SP_RCD, false);
                    changed |= set_virtual_key(SP_FLD, false);
                    changed |= set_virtual_key(SP_KANA, false);
                    changed |= set_virtual_key(SP_NUM, false);
                    changed |= set_virtual_key(SP_ALPHA, false);
                    changed |= set_virtual_key(SP_ALPHA2, false);
                    mystery_active = false;
                }
                state = NORMAL;
                xprintf("\n");
                break;

            case AFTER_76: {
                uint16_t special = green_key(code);
                if (special) {
                    changed |= set_virtual_key(special, true);
                } else {
                    /* Unexpected follower: preserve both ordinary bytes. */
                    changed |= set_virtual_key(0x76, true);
                    changed |= set_virtual_key(code, true);
                }
                state = NORMAL;
                break;
            }

            case AFTER_F0_76:
                if (code == 0xF0) {
                    state = AFTER_F0_76_F0;
                } else {
                    changed |= set_virtual_key(0x76, false);
                    changed |= set_virtual_key(code, true);
                    state = NORMAL;
                }
                break;

            case AFTER_F0_76_F0: {
                uint16_t special = green_key(code);
                if (special) changed |= set_virtual_key(special, false);
                else {
                    changed |= set_virtual_key(0x76, false);
                    changed |= set_virtual_key(code, false);
                }
                state = NORMAL;
                xprintf("\n");
                break;
            }

            case AFTER_14:
                if (code == 0x51) {
                    changed |= set_virtual_key(SP_CAPS, true);
                    state = CAPS_HELD;
                } else if (code == 0xF0) {
                    changed |= set_virtual_key(0x14, true);
                    state = AFTER_F0;
                } else {
                    changed |= set_virtual_key(0x14, true);
                    changed |= set_virtual_key(code, true);
                    state = NORMAL;
                }
                break;

            case CAPS_HELD:
                /* Caps make finishes with F0 14; suppress that synthetic 14 break. */
                if (code == 0xF0) state = CAPS_AFTER_F0;
                else if (code == 0x51) { changed |= set_virtual_key(SP_CAPS, false); state = NORMAL; xprintf("\n"); }
                break;

            case CAPS_AFTER_F0:
                if (code == 0x14) {
                    state = CAPS_HELD;
                    /* End of Caps make sequence. */
                    xprintf("\n");
                } else if (code == 0x51) {
                    changed |= set_virtual_key(SP_CAPS, false);
                    state = NORMAL;
                    xprintf("\n");
                } else {
                    state = CAPS_HELD;
                }
                break;
        }
    }

    /*
     * A lone 0x14 is ambiguous: it may be Esc, or the first byte of
     * Caps/Num/Alpha.  Their second byte arrives immediately.  If the input
     * queue is empty and no follower has arrived for 12 ms, commit the
     * candidate as a held Esc.  Its later F0 14 is handled normally.
     */
    if (mstate == M_14_SECOND && timer_elapsed(esc_candidate_timer) >= 12) {
        changed |= set_virtual_key(0x14, true);
        mstate = M_NONE;
    }

    matrix_scan_kb();
    return changed ? 1 : 0;
}
