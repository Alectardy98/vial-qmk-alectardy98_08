/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "dev_build.h"

/* Keep this identity and the existing EEPROM layout stable across updates. */
#define VIAL_KEYBOARD_UID {0x4d, 0x59, 0x32, 0x4b, 0x45, 0x58, 0x50, 0x03}
#define VIAL_UNLOCK_COMBO_ROWS {0, 1}
#define VIAL_UNLOCK_COMBO_COLS {0, 13}
/* Deliberate user policy: this custom keyboard is always Vial-unlocked. */
#define VIAL_INSECURE
#define VIA_CUSTOM_LIGHTING_ENABLE

/* Enable the board runtime for this supported Release 1 keymap. */
#undef MY2K_UART_RUNTIME_ENABLED
#define MY2K_UART_RUNTIME_ENABLED 1
#undef MY2K_RGB_RUNTIME_ENABLED
#define MY2K_RGB_RUNTIME_ENABLED 1
#undef MY2K_USBH_RUNTIME_ENABLED
#define MY2K_USBH_RUNTIME_ENABLED 1

#undef EEPROM_SIZE
#define EEPROM_SIZE 32768
/* Last 256 bytes belong to board settings, beyond Vial's macro range. */
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 32511

#undef PRODUCT
#define PRODUCT "FAUKWAA - [MY-2K] 100NG Edition - Alectardy98 Custom"
#undef MANUFACTURER
#define MANUFACTURER "Matrix"
