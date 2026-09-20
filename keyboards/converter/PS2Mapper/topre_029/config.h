#pragma once

/*
 * IBM M50 / unusual PS/2 keyboard discovery firmware
 *
 * The virtual matrix is 24 x 32:
 *   rows  0-7  = unprefixed Set-2 byte 0x00-0xFF
 *   rows  8-15 = E0-prefixed byte      0x100-0x1FF
 *   rows 16-23 = E1-prefixed byte      0x200-0x2FF
 *
 * Matrix dimensions live in keyboard.json for compatibility with newer QMK
 * metadata parsing.
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
