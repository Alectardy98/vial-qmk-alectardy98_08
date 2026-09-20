# MCU name
MCU = STM32F411
# BOOTLOADER = custom  // used in the offical astro build, I lost mine
BOOTLOADER = stm32-dfu

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

# Settings if using the custom astro bootloader
# MCU_LDSCRIPT = me_ec_boot_STM32F411xE // needed for mapping the custom uf2 bootloader
# FIRMWARE_FORMAT = uf2  // compiles the code ready for the custom uf2 bootloader
# UF2_FIRMWARE_ADDR = 0x08020000 builds the uf2 to this starting address for the custom bootloader

