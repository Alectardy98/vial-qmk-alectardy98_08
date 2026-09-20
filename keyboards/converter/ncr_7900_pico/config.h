#pragma once

/* key matrix size */
#define MATRIX_ROWS 11
#define MATRIX_COLS 11


#define UART_DRIVER SIOD1
#define UART_RX_PIN GP21
#define UART_TX_PIN GP20



/* — NCR-7900 serial settings — */
#define SERIAL_UART_BAUD 1200  /* must match the keyboard’s 1200 baud */

/* — protocol constants — */
#define IDLE_CODE   0x00    // filler “00→5F” idle frame
//#define BREAK_DELTA 0xFF    // many keys send (make−0x10) as break
