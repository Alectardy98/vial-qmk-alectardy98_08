# MCU name
MCU = STM32F411
BOOTLOADER = tinyuf2
# BOOTLOADER = stm32-dfu

# RGB Matrix for top left side is31fl3236
RGB_MATRIX_ENABLE = yes
RGB_MATRIX_DRIVER = is31fl3236
I2C_DRIVER_REQUIRED = yes

# RGB Light for Heart WS2812
RGBLIGHT_ENABLE = yes

# Enter lower-power sleep mode when on the ChibiOS idle thread
OPT_DEFS += -DCORTEX_ENABLE_WFI_IDLE=TRUE

CUSTOM_MATRIX = lite
SRC += matrix.c
ALLOW_WARNINGS = yes
SRC += analog.c
SRC += is31fl3236_map.c

MCU_LDSCRIPT = me_ec_boot_STM32F411xE
FIRMWARE_FORMAT = uf2
UF2_FIRMWARE_ADDR = 0x08020000

