#pragma once


#define VENDOR_ID       0xFEED
#define PRODUCT_ID      0x6060
#define DEVICE_VER      0x0001
#define MANUFACTURER    "IBMPC_QMK"
#define PRODUCT         "IBM PC Converter (RP2040)"
#define DESCRIPTION     "IBM PC XT/AT keyboard to USB converter (RP2040)"

#define DEBOUNCE    5
#define FORCE_NKRO

#define LOCKING_SUPPORT_ENABLE
#define LOCKING_RESYNC_ENABLE

#define IBMPC_CLOCK_PIN GP1
#define IBMPC_DATA_PIN  GP0
#define IBMPC_RESET_PIN GP2
