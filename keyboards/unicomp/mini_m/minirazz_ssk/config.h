#pragma once

/*
 * IBM Topre 029 PS/2 converter.
 * Compact virtual matrix: 6 physical rows x 17 columns.
 * Matrix dimensions live in keyboard.json.
 */

#ifdef PS2_DRIVER_USART
#    error The UART is needed for the mouse; use PS2_DRIVER=interrupt.
#endif

#ifdef PS2_DRIVER_INTERRUPT
#    define PS2_CLOCK_PIN D1
#    define PS2_DATA_PIN  D0

#    define PS2_INT_INIT() do { \
        EICRA |= ((1 << ISC11) | (0 << ISC10)); \
    } while (0)
#    define PS2_INT_ON() do { EIMSK |= (1 << INT1); } while (0)
#    define PS2_INT_OFF() do { EIMSK &= ~(1 << INT1); } while (0)
#    define PS2_INT_VECT INT1_vect
#endif

#ifdef PS2_DRIVER_BUSYWAIT
#    define PS2_CLOCK_PIN D1
#    define PS2_DATA_PIN  D0
#endif

/* Secondary IBM PS/2 channel - local mapper implementation (receive-only).
 * Red = DATA2 -> PD2 / D2
 * Blue = CLOCK2 -> PD3 / D3 (INT3)
 */
#define PS2_SECONDARY_DATA_PIN D2
#define PS2_SECONDARY_CLOCK_PIN D3
