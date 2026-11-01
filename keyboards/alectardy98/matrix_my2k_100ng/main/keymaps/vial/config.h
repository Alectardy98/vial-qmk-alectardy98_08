#include "dev_build.h"
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

/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#undef MY2K_RGB_RUNTIME_ENABLED
#define MY2K_RGB_RUNTIME_ENABLED 1
#undef PRODUCT
#define PRODUCT "MY-2K " MY2K_DEV_BUILD

#define VIA_CUSTOM_LIGHTING_ENABLE
/* Explicit user decision: development keyboard is always Vial-unlocked. */
#define VIAL_INSECURE

/* Stock GUI profile: no generic Lighting page or false high-speed claim. */
#undef MY2K_USBH_RUNTIME_ENABLED
#define MY2K_USBH_RUNTIME_ENABLED 1

/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#undef EEPROM_SIZE
#define EEPROM_SIZE 32768
/* Last256 bytes belong to board settings; VIA macros cannot alias them. */
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 32511

#undef PRODUCT
#define PRODUCT "FAUKWAA - [MY-2K] 100NG Edition - Alectardy98 Custom"
#undef MANUFACTURER
#define MANUFACTURER "Matrix"
