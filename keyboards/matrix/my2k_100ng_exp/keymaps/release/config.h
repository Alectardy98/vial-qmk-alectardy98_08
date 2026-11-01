/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#define VIAL_KEYBOARD_UID {0x4d,0x59,0x32,0x4b,0x45,0x58,0x50,0x03}
#define VIAL_UNLOCK_COMBO_ROWS {0, 1}
#define VIAL_UNLOCK_COMBO_COLS {0, 13}
#undef MY2K_UART_RUNTIME_ENABLED
#define MY2K_UART_RUNTIME_ENABLED 1
/* Host storage and physical RGB are not prerequisites for keyboard/HID service.
 * Keep unrelated outputs off until their own initialization audit is complete. */
#undef PRODUCT
#define PRODUCT "MY-2K 100NG QMK/Vial"
