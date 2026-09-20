#include "quantum.h"
#include "print.h"                   // for xprintf()
#include <hardware/pio.h>             // PIO
#include <hardware/clocks.h>          // clock for PIO
#include "config.h"                  // SERIAL_UART_BAUD, IDLE_CODE, MATRIX_ROWS

// — scan→pos map (0xFF = ignore) —
static const uint8_t sc_to_pos[256] = {
    [0x63] = 0x00,
    [0x33] = 0x10,
    [0x66] = 0x01,
    [0x19] = 0x11,
    // ... continue mapping your codes ...
};

static matrix_row_t matrix[MATRIX_ROWS];
static uint8_t last_sc = 0xFF;

// PIO program for UART RX with inversion
static const uint16_t uart_rx_program_instructions[] = {
    // wait for start bit (inverted: high->low)
    0x2020, // wait 0 pin 0
    // inverts are done by reading bit, xors 1
    // shift in 8 data bits
    0xa0c1, // set x, 1
    0x2021, 0xf001, // sideset 0, in pins, 1
    // stub: actual PIO UART program omitted for brevity
};
static const struct pio_program uart_rx_program = {
    .instructions = uart_rx_program_instructions,
    .length = sizeof(uart_rx_program_instructions)/sizeof(uart_rx_program_instructions[0]),
    .origin = -1
};

static PIO pio = pio0;
static int sm;

static void init_pio_uart(void) {
    uint offset = pio_add_program(pio, &uart_rx_program);
    // Claim a free state machine
    sm = pio_claim_unused_sm(pio, true);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_in_pins(&c, PICO_DEFAULT_UART_RX_PIN);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    sm_config_set_in_shift(&c, false, true, 8);
    pio_gpio_init(pio, PICO_DEFAULT_UART_RX_PIN);
    pio_sm_set_consecutive_pindirs(pio, sm, PICO_DEFAULT_UART_RX_PIN, 1, false);
    float div = (float)clock_get_hz(clk_sys) / (8 * SERIAL_UART_BAUD);
    sm_config_set_clkdiv(&c, div);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

void matrix_init(void) {
    init_pio_uart();
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        matrix[r] = 0;
    }
    last_sc = 0xFF;
}

uint8_t matrix_scan(void) {
    if (pio_sm_get_rx_fifo_level(pio, sm) == 0) {
        return 0;
    }
    uint32_t raw32 = pio_sm_get(pio, sm);
    uint8_t raw = raw32 & 0xFF;

    // heartbeat detection
    static bool hb_first = false;
    static uint32_t hb_count = 0;
    if (!hb_first && raw == 0xFF) { hb_first = true; return 0; }
    if (hb_first) {
        hb_first = false;
        if (raw == IDLE_CODE) { hb_count++; return 0; }
    }

    // logging
    xprintf("RAW:%02X SC:%02X ", raw, raw);

    uint8_t pos = sc_to_pos[raw];
    if (pos == 0xFF) {
        xprintf("→IGNORE\n");
        return 0;
    }
    uint8_t row = pos >> 4;
    uint8_t col = pos & 0x0F;
    if (last_sc != 0xFF && last_sc != raw) {
        uint8_t old = sc_to_pos[last_sc];
        matrix[old>>4] &= ~(1u << (old&0x0F));
    }
    matrix[row] |= (1u << col);
    last_sc = raw;
    xprintf("→MAKE row%u,col%u\n", row, col);
    return 0;
}

matrix_row_t matrix_get_row(uint8_t row) {
    return matrix[row];
}

void matrix_print(void) { }
