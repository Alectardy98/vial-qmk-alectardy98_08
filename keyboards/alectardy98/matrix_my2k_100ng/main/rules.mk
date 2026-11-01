MY2K_MAIN := keyboards/alectardy98/matrix_my2k_100ng/main
MY2K_CHIBIOS_OVERLAY := $(BUILD_DIR)/my2k-chibios
MY2K_LOCAL_CONTRIB := $(MY2K_CHIBIOS_OVERLAY)/chibios-contrib
# SPDX-License-Identifier: GPL-2.0-or-later
MCU = cortex-m4
ARMV = 7
MCU_FAMILY = STM32
MCU_SERIES = STM32F4xx
MCU_STARTUP = stm32f4xx
MCU_LDSCRIPT = my2k_apm32f405_128k
BOARD = MY2K_APM32F405_COMPAT
USE_FPU = yes
BOOTLOADER = custom
FIRMWARE_FORMAT = uf2
# Final files in the repository root. Internal BIN/ELF stay in .build/.
GENERATE_BIN ?= no
GENERATE_JSON ?= no
GENERATE_SHA256 ?= no
EEPROM_DRIVER = custom
CUSTOM_MATRIX = lite
DEBOUNCE_TYPE = sym_defer_pk
BOOTMAGIC_ENABLE = no
CONSOLE_ENABLE = no
COMMAND_ENABLE = no
MOUSEKEY_ENABLE = yes
EXTRAKEY_ENABLE = yes
NKRO_ENABLE = yes
LTO_ENABLE = yes
EXTRAINCDIRS += keyboards/alectardy98/matrix_my2k_100ng/main/src
SRC += matrix.c
OPT_DEFS += -DMY2K_MCU_APM32F405RGT6=1
EXTRALDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref
# Explicit LTO worker selection avoids GCC's serial-LTRANS warning. Uses the
# make jobserver when available, otherwise the host's detected CPU parallelism.
EXTRALDFLAGS += -flto=auto

# Independently authored, bounded protocol/cache modules (no physical writes).
EXTRAINCDIRS += keyboards/alectardy98/matrix_my2k_100ng/common $(MY2K_MAIN)/drivers
SRC += keyboards/alectardy98/matrix_my2k_100ng/common/my2k_amk.c
SRC += keyboards/alectardy98/matrix_my2k_100ng/common/my2k_wire.c
SRC += keyboards/alectardy98/matrix_my2k_100ng/common/my2k_asset.c
SRC += $(MY2K_MAIN)/drivers/my2k_rgb.c
SRC += $(MY2K_MAIN)/drivers/my2k_block.c

USE_CHIBIOS_CONTRIB = yes
PLATFORM_MK = keyboards/alectardy98/matrix_my2k_100ng/main/platform.mk
I2C_DRIVER_REQUIRED = yes

SRC += $(MY2K_MAIN)/drivers/my2k_usbh_chibios.c
# Retain the explicit worker/backend entrypoints while runtime activation is gated.
EXTRALDFLAGS += -Wl,--undefined=my2k_usbh_start,--undefined=my2k_usbh_capacity,--undefined=my2k_usbh_submit_read,--undefined=my2k_usbh_take_read

SRC += $(MY2K_MAIN)/src/aux_link.c
EXTRALDFLAGS += -Wl,--undefined=my2k_aux_start,--undefined=my2k_aux_send,--undefined=my2k_aux_poll

SRC += $(MY2K_MAIN)/drivers/my2k_qmk_rgb.c
SRC += $(MY2K_MAIN)/drivers/my2k_qmk_rgb_3729.c
SRC += $(MY2K_MAIN)/drivers/my2k_qmk_rgb_3731.c
SRC += $(MY2K_MAIN)/drivers/my2k_qmk_rgb_3236.c
EXTRALDFLAGS += -Wl,--undefined=my2k_qmk_rgb_init,--undefined=my2k_qmk_rgb_flush

SRC += keyboards/alectardy98/matrix_my2k_100ng/common/my2k_touch.c

EXTRALDFLAGS += -Wl,--undefined=my2k_aux_track,--undefined=my2k_mcu_identity

EXTRAINCDIRS += $(MY2K_MAIN)/storage
SRC += $(MY2K_MAIN)/storage/my2k_journal.c
EXTRALDFLAGS += -Wl,--undefined=my2k_journal_init,--undefined=my2k_journal_load,--undefined=my2k_journal_save

SRC += $(MY2K_MAIN)/src/main_settings.c
SRC += $(MY2K_MAIN)/drivers/my2k_scsi.c
EXTRALDFLAGS += -Wl,--undefined=my2k_usbh_scsi_init,--undefined=my2k_scsi_start,--undefined=my2k_scsi_poll,--undefined=my2k_scsi_consume,--undefined=my2k_scsi_cancel

# Board-local middleware; QMK HID code is unchanged.
SRC += $(MY2K_MAIN)/src/main_eeprom32.c $(MY2K_MAIN)/src/main_factory_defaults.c
SRC += $(MY2K_MAIN)/src/my2k_usb_msc.c $(MY2K_MAIN)/src/my2k_usb_abort.c
SRC += $(MY2K_MAIN)/storage/my2k_flash_region.c
SRC += $(MY2K_MAIN)/drivers/my2k_amk_rgb_runtime.c
SRC += $(MY2K_MAIN)/drivers/my2k_bot.c
SRC += $(MY2K_MAIN)/drivers/my2k_usb_descriptor.c
EXTRALDFLAGS += -Wl,--undefined=my2k_eeprom_task,--undefined=my2k_eeprom_durable,--undefined=my2k_journal_rotate,--undefined=my2k_flash_region_init,--undefined=my2k_flash_region_erase
$(INTERMEDIATE_OUTPUT)/protocol/chibios/usb_main.o: FILE_SPECIFIC_CFLAGS += -include $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_usb_hooks.h

SRC += $(MY2K_MAIN)/drivers/my2k_rgb_settings.c

SRC += $(MY2K_MAIN)/src/main_rgb_settings.c

SRC += $(MY2K_MAIN)/src/my2k_hid_guard.c
$(INTERMEDIATE_OUTPUT)/quantum/via.o: FILE_SPECIFIC_CFLAGS += -include $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_via_hooks.h
$(INTERMEDIATE_OUTPUT)/protocol/chibios/usb_main.o: $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_usb_hooks.h $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/rules.mk
$(INTERMEDIATE_OUTPUT)/protocol/chibios/usb_main.o: $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_usb_bus_hooks.h
$(INTERMEDIATE_OUTPUT)/quantum/via.o: $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_via_hooks.h $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/rules.mk
$(INTERMEDIATE_OUTPUT)/quantum/via.o: $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_storage_version.h

EXTRAINCDIRS += $(MY2K_MAIN)/storage $(MY2K_MAIN)/storage/vendor
SRC += $(MY2K_MAIN)/storage/my2k_main_fs.c
SRC += $(MY2K_MAIN)/storage/vendor/ff.c $(MY2K_MAIN)/storage/vendor/ffunicode.c
EXTRAFLAGS += -fstack-usage

# Recovered byte-width Geehy shadow PHY preparation; compile-review only.
MY2K_RECOVERED_USB_PHY_ENABLE ?= 0
OPT_DEFS += -DMY2K_RECOVERED_USB_PHY_ENABLE=$(MY2K_RECOVERED_USB_PHY_ENABLE)
MY2K_GEEHY_FS_USB_ENABLE ?= 0
OPT_DEFS += -DMY2K_GEEHY_FS_USB_ENABLE=$(MY2K_GEEHY_FS_USB_ENABLE)

# Both normal QMK restart and bootloader-entry disconnect must use DCTL when
# the recovered no-VBUS-sensing FS configuration is selected.
$(INTERMEDIATE_OUTPUT)/protocol/chibios/usb_util.o: FILE_SPECIFIC_CFLAGS += -include $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_usb_bus_hooks.h
$(INTERMEDIATE_OUTPUT)/protocol/chibios/usb_util.o: $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_usb_bus_hooks.h

# Apply the board hook after the actual RCC reset, for this USB driver TU only.
MY2K_OTG_OBJECT = $(INTERMEDIATE_OUTPUT)/lib/chibios/os/hal/ports/STM32/LLD/OTGv1/hal_usb_lld.o
$(MY2K_OTG_OBJECT): FILE_SPECIFIC_CFLAGS += -include $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_usb_lld_hooks.h
$(MY2K_OTG_OBJECT): $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_usb_lld_hooks.h $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/rules.mk

# Explicit board-local active-development profile, default off.
MY2K_ACTIVE_DEV_PROFILE ?= 1
MY2K_GAMES_PROFILE ?= 1
OPT_DEFS += -DMY2K_ACTIVE_DEV_PROFILE=$(MY2K_ACTIVE_DEV_PROFILE)
ifeq ($(MY2K_ACTIVE_DEV_PROFILE),1)
SRC += $(MY2K_MAIN)/src/main_input_io.c $(MY2K_MAIN)/src/main_sleep.c
SRC += $(MY2K_MAIN)/src/main_debug.c $(MY2K_MAIN)/src/main_file_bulk.c
MY2K_MSD_DEBUG_OBJECT = $(INTERMEDIATE_OUTPUT)/$(MY2K_LOCAL_CONTRIB)/os/hal/src/usbh/hal_usbh_msd.o
$(MY2K_MSD_DEBUG_OBJECT): FILE_SPECIFIC_CFLAGS += -include $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_msd_debug_hooks.h
$(MY2K_MSD_DEBUG_OBJECT): $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/my2k_msd_debug_hooks.h $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/rules.mk
endif

# Explicit game-development profile; vectors/reset stay in ASTRO sector5.
ifeq ($(MY2K_GAMES_PROFILE),1)
MCU_LDSCRIPT = my2k_apm32f405_games256k
OPT_DEFS += -DMY2K_GAMES_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_games.c
endif

# Both user-facing targets package ASTRO-compatible UF2 locally.
include keyboards/alectardy98/matrix_my2k_100ng/main/build.mk
