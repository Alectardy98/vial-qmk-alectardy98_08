#include "debug.h"
#include "matrix.h"
#include "print.h"
#include "ps2.h"

/*
 * Discovery matrix
 * ----------------
 * A 9-bit/10-bit synthetic code is generated from the raw PS/2 stream:
 *   0x000-0x0FF : normal byte
 *   0x100-0x1FF : E0 + byte
 *   0x200-0x2FF : E1 + byte
 *
 * Splitting that synthetic code into 5 column bits gives a deterministic
 * 24x32 virtual matrix position for every possible code.
 */
static matrix_row_t matrix[MATRIX_ROWS];

#define ROW(code) ((uint8_t)((code) >> 5))
#define COL(code) ((uint8_t)((code) & 0x1F))

static void mapper_make(uint16_t code) {
    const uint8_t row = ROW(code);
    const uint8_t col = COL(code);
    if (row < MATRIX_ROWS && col < MATRIX_COLS) {
        matrix[row] |= ((matrix_row_t)1 << col);
    }
}

static void mapper_break(uint16_t code) {
    const uint8_t row = ROW(code);
    const uint8_t col = COL(code);
    if (row < MATRIX_ROWS && col < MATRIX_COLS) {
        matrix[row] &= ~((matrix_row_t)1 << col);
    }
}

static void mapper_clear(void) {
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        matrix[i] = 0;
    }
}

__attribute__((weak)) void matrix_init_kb(void) { matrix_init_user(); }
__attribute__((weak)) void matrix_scan_kb(void) { matrix_scan_user(); }
__attribute__((weak)) void matrix_init_user(void) {}
__attribute__((weak)) void matrix_scan_user(void) {}

matrix_row_t matrix_get_row(uint8_t row) {
    return matrix[row];
}

void matrix_print(void) {
    print("\nr/c 0123456789ABCDEF0123456789ABCDEF\n");
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        print_hex8(row);
        print(": ");
        print_bin_reverse32(matrix_get_row(row));
        print("\n");
    }
}

void matrix_init(void) {
    mapper_clear();
    ps2_host_init();
    print("\nPS2 MAPPER READY - raw Set-2 stream + virtual matrix coordinates\n");
    matrix_init_kb();
}

typedef enum {
    NORMAL,
    BREAK_F0,
    PREFIX_E0,
    PREFIX_E0_BREAK,
    PREFIX_E1,
    PREFIX_E1_BREAK
} mapper_state_t;

static void print_raw(uint8_t code) {
    xprintf("PS2:%02X ", code);
}

static void print_event(const char *kind, uint16_t code) {
    if (code <= 0xFF) {
        xprintf("| %s CODE:%02X -> R%02u C%02u\n", kind, (uint8_t)code, ROW(code), COL(code));
    } else if (code <= 0x1FF) {
        xprintf("| %s CODE:E0 %02X -> R%02u C%02u\n", kind, (uint8_t)code, ROW(code), COL(code));
    } else {
        xprintf("| %s CODE:E1 %02X -> R%02u C%02u\n", kind, (uint8_t)code, ROW(code), COL(code));
    }
}

uint8_t matrix_scan(void) {
    static mapper_state_t state = NORMAL;
    const uint8_t code = ps2_host_recv();

    if (!code) {
        matrix_scan_kb();
        return 0;
    }

    /* Always expose the byte actually received from the PS/2 keyboard. */
    print_raw(code);

    switch (state) {
        case NORMAL:
            if (code == 0xF0) {
                state = BREAK_F0;
            } else if (code == 0xE0) {
                state = PREFIX_E0;
            } else if (code == 0xE1) {
                state = PREFIX_E1;
            } else {
                mapper_make(code);
                print_event("MAKE ", code);
            }
            break;

        case BREAK_F0:
            mapper_break(code);
            print_event("BREAK", code);
            state = NORMAL;
            break;

        case PREFIX_E0:
            if (code == 0xF0) {
                state = PREFIX_E0_BREAK;
            } else {
                const uint16_t mapped = 0x100 | code;
                mapper_make(mapped);
                print_event("MAKE ", mapped);
                state = NORMAL;
            }
            break;

        case PREFIX_E0_BREAK: {
            const uint16_t mapped = 0x100 | code;
            mapper_break(mapped);
            print_event("BREAK", mapped);
            state = NORMAL;
            break;
        }

        case PREFIX_E1:
            if (code == 0xF0) {
                state = PREFIX_E1_BREAK;
            } else if (code == 0x14) {
                /* Keep E1 active for the common Pause sequence E1 14 77 ... */
            } else {
                const uint16_t mapped = 0x200 | code;
                mapper_make(mapped);
                print_event("MAKE ", mapped);
                state = NORMAL;
            }
            break;

        case PREFIX_E1_BREAK:
            if (code == 0x14) {
                state = PREFIX_E1;
            } else {
                const uint16_t mapped = 0x200 | code;
                mapper_break(mapped);
                print_event("BREAK", mapped);
                state = NORMAL;
            }
            break;
    }

    matrix_scan_kb();
    return 1;
}
