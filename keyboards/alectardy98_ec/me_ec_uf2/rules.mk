# MCU name
MCU = STM32F411
# UF2 build selector only; astro_bootloader.c overrides its weak entry hook.
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


EXTRALDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref

CUSTOM_MATRIX = lite
ANALOG_DRIVER_REQUIRED = yes
EEPROM_DRIVER = wear_leveling
WEAR_LEVELING_DRIVER = embedded_flash
EXTRAINCDIRS += keyboards/alectardy98_ec/common/engine keyboards/alectardy98_ec/common/runtime
SRC += keyboards/alectardy98_ec/common/engine/calibration.c keyboards/alectardy98_ec/common/engine/ec_diagnostic.c
SRC += keyboards/alectardy98_ec/common/runtime/decision.c keyboards/alectardy98_ec/common/runtime/ec_board.c keyboards/alectardy98_ec/common/runtime/integration.c keyboards/alectardy98_ec/common/runtime/storage.c

SRC += astro_bootloader.c

EXTRAINCDIRS += keyboards/alectardy98_ec/common/interfaces
