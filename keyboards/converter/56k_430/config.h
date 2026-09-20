#pragma once

/* key matrix size */
#define MATRIX_ROWS 10
#define MATRIX_COLS 11

/* — serial settings — */
#define SERIAL_UART_BAUD 4800  /* must match the keyboard’s 1200 baud */


// Active‑LOW inhibit → hold PD0 (D3) low so keyboard is always on
#define WALT_ENABLE_PIN D3
// W line (command) → PD3 (TX1) on Pro Micro D1
#define WALT_CMD_PIN    D1
