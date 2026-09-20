

RGB_MATRIX_ENABLE = yes
RGB_MATRIX_DRIVER = ws2812

RGBLIGHT_ENABLE = yes


# Directories
ROOT_KB_DIR          = $(QMK_HOME)/keyboards/kindlestar
KB_DIR               = $(ROOT_KB_DIR)/n1

# Now your local “platforms/chibios”
CHIBIOS_DIR          = $(KB_DIR)/platforms/chibios

# Tell GCC to search your keyboard root,
# so that includes of "platforms/..." resolve here
CFLAGS              += -I$(KB_DIR)

# Your board code + custom sources
SRC += \
    $(KB_DIR)/n1.c \
    $(ROOT_KB_DIR)/kindlestar_output_auto_select.c \
    $(ROOT_KB_DIR)/usb_sender.c \
    $(ROOT_KB_DIR)/uart_sender.c \
    $(ROOT_KB_DIR)/kindlestar_custom_animation.c

# STM32 HAL under platforms/chibios
SRC += $(wildcard $(CHIBIOS_DIR)/drivers/STM32F1xx_HAL_Driver/Src/*.c)
VPATH += $(CHIBIOS_DIR)/drivers/STM32F1xx_HAL_Driver/Inc

# ChibiOS core + drivers
SRC += $(wildcard $(CHIBIOS_DIR)/*.c)
SRC += $(wildcard $(CHIBIOS_DIR)/drivers/*.c)
VPATH += $(CHIBIOS_DIR)
VPATH += $(CHIBIOS_DIR)/drivers

# Wear-leveling
SRC += $(wildcard $(CHIBIOS_DIR)/drivers/wear_leveling/*.c)
VPATH += $(CHIBIOS_DIR)/drivers/wear_leveling

# Compiler defs
OPT_DEFS += -DCORTEX_ENABLE_WFI_IDLE=TRUE
OPT_DEFS += -DSTM32F103xB
OPT_DEFS += -DSTM32_LSECLK=0





