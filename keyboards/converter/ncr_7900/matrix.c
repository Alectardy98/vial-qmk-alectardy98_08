#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include "quantum.h"
#include "print.h"    // for xprintf()
#include "config.h"   // for SERIAL_UART_BAUD

#define GHOST_PREFIX    0xFF
#define GHOST_CODE      0x41
#define NOISE_CODE      0x10
#define MAX_ACTIVE      16
#define GHOST_THRESHOLD 1

// — scan→pos map (0xFF = ignore) —
static const uint8_t sc_to_pos[256] = {
    [0 ... 255] = 0xFF,
    [0x39] = 0x00,
    [0x32] = 0x10,  // 0xB2 Release
    [0x33] = 0x01,  // 0xB3 Release
    [0x34] = 0x11,  // 0xB4 Release
    [0x3A] = 0x02,
    [0x3B] = 0x12,
    [0x3C] = 0x03,
    [0x3D] = 0x13,
    [0x3E] = 0x04,
    [0x3F] = 0x14,
    [0x40] = 0x05,
    [0x41] = 0x15,
    [0x42] = 0x06,
    [0x43] = 0x16,
    [0x44] = 0x07,
    [0x45] = 0x17,
    [0x46] = 0x08,
    [0x47] = 0x18,
    [0x48] = 0x09,
    [0x49] = 0x19,
    [0x4A] = 0x0A,
    [0x4B] = 0x1A,
    [0x63] = 0x20,
    [0x0D] = 0x30,
    [0x01] = 0x21,
    [0x02] = 0x31,
    [0x03] = 0x22,
    [0x04] = 0x32,
    [0x05] = 0x23,
    [0x06] = 0x33,
    [0x07] = 0x24,
    [0x08] = 0x34,
    [0x09] = 0x25,
    [0x0A] = 0x35,
    [0x0B] = 0x26,
    [0x0C] = 0x36,
    [0x66] = 0x27,
    [0x5B] = 0x37,
    [0x5C] = 0x28,
    [0x57] = 0x38,
    [0x58] = 0x29,
    [0x59] = 0x39,
    [0x4E] = 0x2A,
    [0x64] = 0x40,
    [0x67] = 0x50,
    [0x0E] = 0x41,
    [0x0F] = 0x51,
    [0x1D] = 0x42,
    [0x1E] = 0x52,
    [0x1F] = 0x43,
    [0x10] = 0x53,
    [0x20] = 0x44,
    [0x21] = 0x54,
    [0x22] = 0x45,
    [0x23] = 0x55,
    [0x11] = 0x46,
    [0x12] = 0x56,
    [0x68] = 0x47,
    [0x5D] = 0x57,
    [0x5E] = 0x48,
    [0x54] = 0x58,
    [0x55] = 0x49,
    [0x56] = 0x59,
    [0x4F] = 0x4A,
    [0x35] = 0x60,  // 0xB5 Release
    [0x36] = 0x70,  // 0xB6 Release
    [0x13] = 0x61,
    [0x24] = 0x71,
    [0x25] = 0x62,
    [0x26] = 0x72,
    [0x27] = 0x63,
    [0x28] = 0x73,
    [0x29] = 0x64,
    [0x2A] = 0x74,
    [0x2B] = 0x65,
    [0x14] = 0x75,
    [0x15] = 0x66,
    [0x16] = 0x76,
    [0x5F] = 0x77,
    [0x60] = 0x68,
    [0x51] = 0x78,
    [0x52] = 0x69,
    [0x53] = 0x79,
    [0x5A] = 0x6A,
    [0x65] = 0x80,
    [0x37] = 0x90,  // 0xB7 Release
    [0x17] = 0x81,
    [0x18] = 0x91,
    [0x2C] = 0x82,
    [0x2D] = 0x92,
    [0x2E] = 0x83,
    [0x2F] = 0x93,
    [0x30] = 0x84,
    [0x19] = 0x94,
    [0x1A] = 0x85,
    [0x1B] = 0x95,
    [0x1C] = 0x86,
    [0x38] = 0x96,  // 0xB8 Release
    [0x69] = 0x87,
    [0x61] = 0x97,
    [0x62] = 0x88,
    [0x50] = 0x98,
    [0x4C] = 0x89,
    [0x4D] = 0x99,
    [0x6A] = 0xA1,
    [0x31] = 0xA3,
    [0x6B] = 0xA6,
    [0x6C] = 0xA7,
};

static matrix_row_t matrix[MATRIX_ROWS];
static uint8_t      active_codes[MAX_ACTIVE];
static uint8_t      active_ghosts[MAX_ACTIVE];
static uint8_t      active_count = 0;

void uart_init(uint32_t baud) {
    uint16_t ubrr = (F_CPU / (16UL * baud)) - 1;
    UBRR1L = (uint8_t)ubrr;
    UBRR1H = (uint8_t)(ubrr >> 8);
    UCSR1B = _BV(RXEN1);
    UCSR1C = _BV(UCSZ11) | _BV(UCSZ10);
}

bool uart_available(void) {
    return (UCSR1A & _BV(RXC1)) != 0;
}

uint8_t uart_read(void) {
    return UDR1;
}

void matrix_init(void) {
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        matrix[r] = 0;
    }
    active_count = 0;
    uart_init(SERIAL_UART_BAUD);
}

uint8_t matrix_scan(void) {
    static bool saw_prefix = false;

    while (uart_available()) {
        uint8_t code = uart_read();

        // debug: incoming scan code
       // xprintf("SCODE:%02X\n", code);

        // noise filter: ignore NOISE_CODE if already two or more keys pressed
        if (code == NOISE_CODE) {
            uint8_t total = 0;
            for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
                total += __builtin_popcount(matrix[r]);
            }
            if (total >= 2) {
               // xprintf("SC:%02X ->IGNORE NOISE\n", code);
                continue;
            }
        }

        // handle ghost prefix
        if (saw_prefix) {
            saw_prefix = false;
            if (code == GHOST_CODE) {
                //xprintf("GHOST_EVENT\n");
                // increment ghost counters
                for (uint8_t i = 0; i < active_count; i++) {
                    active_ghosts[i]++;
                }
                // break keys exceeding threshold
                for (uint8_t i = 0; i < active_count; ) {
                    if (active_ghosts[i] >= GHOST_THRESHOLD) {
                        uint8_t gc = active_codes[i];
                        uint8_t pos = sc_to_pos[gc];
                        if (pos != 0xFF) {
                            matrix[pos>>4] &= ~(1u << (pos&0x0F));
                           // xprintf("GB:%02X ->GHOST-BREAK r%u,c%u\n", gc, pos>>4, pos&0x0F);
                        }
                        // remove entry
                        for (uint8_t j = i; j < active_count - 1; j++) {
                            active_codes[j]  = active_codes[j+1];
                            active_ghosts[j] = active_ghosts[j+1];
                        }
                        active_count--;
                    } else {
                        i++;
                    }
                }
                continue;
            }
            // stray code after prefix resets state
        }
        if (code == GHOST_PREFIX) {
            saw_prefix = true;
            continue;
        }

        // explicit break codes Bx for 3x make codes
        if ((code & 0xF0) == 0xB0) {
           // xprintf("BR:%02X\n", code);
            uint8_t make = 0x30 | (code & 0x0F);
            uint8_t pos = sc_to_pos[make];
            if (pos != 0xFF) {
                matrix[pos>>4] &= ~(1u << (pos&0x0F));
              //  xprintf("->BREAK r%u,c%u\n", pos>>4, pos&0x0F);
            }
            // remove from active list
            for (uint8_t i = 0; i < active_count; i++) {
                if (active_codes[i] == make) {
                    for (uint8_t j = i; j < active_count - 1; j++) {
                        active_codes[j]  = active_codes[j+1];
                        active_ghosts[j] = active_ghosts[j+1];
                    }
                    active_count--;
                    break;
                }
            }
            continue;
        }

        // make codes
      // xprintf("MAKE_CODE:%02X\n", code);
        uint8_t pos = sc_to_pos[code];
        if (pos == 0xFF) {
          //  xprintf("->IGNORE\n");
            continue;
        }
        uint8_t row = pos >> 4;
        uint8_t col = pos & 0x0F;
        matrix[row] |= (1u << col);
       // xprintf("->MAKE r%u,c%u\n", row, col);

        // ghost-tracking exception for release codes 0x32-0x38
        if (code < 0x32 || code > 0x38) {
            bool found = false;
            for (uint8_t i = 0; i < active_count; i++) {
                if (active_codes[i] == code) {
                    active_ghosts[i] = 0;
                    found = true;
                    break;
                }
            }
            if (!found && active_count < MAX_ACTIVE) {
                active_codes[active_count]  = code;
                active_ghosts[active_count] = 0;
                active_count++;
            }
        }
    }
    return 0;
}

matrix_row_t matrix_get_row(uint8_t row) {
    return matrix[row];
}

void matrix_print(void) { }
