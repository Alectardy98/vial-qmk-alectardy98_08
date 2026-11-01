# SPDX-License-Identifier: GPL-2.0-or-later
VIA_ENABLE = yes
VIAL_ENABLE = yes
OPT_DEFS += -DMY2K_AUX_REPLACEMENT_AUX=1 -DMY2K_RELEASE_PROFILE=1
OPT_DEFS += -DMY2K_AUX_LINK_MANAGED=1 -DMY2K_AUX_STARTUP_CONTROL_ENABLED=1
OPT_DEFS += -DMY2K_DEV_PROFILE=1 -DMY2K_DEV_PERSISTENCE=1 -DMY2K_DEV2_PROFILE=1 -DMY2K_DEV3_PROFILE=1
MY2K_GEEHY_FS_USB_ENABLE = 1
SRC += $(MY2K_MAIN)/src/my2k_astro_boot.c $(MY2K_MAIN)/src/main_diag.c $(MY2K_MAIN)/src/main_rgb_bus_hw.c

OPT_DEFS += -DMY2K_DEV4_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_perf.c $(MY2K_MAIN)/drivers/my2k_rgb_refresh.c

OPT_DEFS += -DMY2K_STOCK_PARITY_PROFILE=1
DEBOUNCE_TYPE = custom
SRC += $(MY2K_MAIN)/src/main_debounce.c $(MY2K_MAIN)/drivers/my2k_stock_effects.c keyboards/alectardy98/matrix_my2k_100ng/common/my2k_aux_ui.c
OPT_DEFS += -DUSE_CIE1931_CURVE
SRC += $(MY2K_MAIN)/drivers/my2k_stock_color.c quantum/led_tables.c

OPT_DEFS += -DMY2K_DEV6_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_aux_telemetry.c

OPT_DEFS += -DMY2K_DEV7_PROFILE=1 -DMY2K_DEV7_WRITABLE_MSC=1
SRC += $(MY2K_MAIN)/src/main_storage_mode.c

OPT_DEFS += -DMY2K_DEV8_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_host_mode.c

OPT_DEFS += -DMY2K_DEV9_PROFILE=1

OPT_DEFS += -DMY2K_DEV10_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_host_trace.c

OPT_DEFS += -DMY2K_DEV11_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_scheduler.c

OPT_DEFS += -DMY2K_DEV12_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_host_events.c
MY2K_HOST_HIGH_OBJECT = $(INTERMEDIATE_OUTPUT)/$(MY2K_LOCAL_CONTRIB)/os/hal/src/hal_usbh.o
$(MY2K_HOST_HIGH_OBJECT): FILE_SPECIFIC_CFLAGS += -include $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/main_host_events_hooks.h
$(MY2K_HOST_HIGH_OBJECT): $(TOP_DIR)/keyboards/alectardy98/matrix_my2k_100ng/main/src/main_host_events_hooks.h

OPT_DEFS += -DMY2K_DEV13_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_host_debounce.c

OPT_DEFS += -DMY2K_DEV14_PROFILE=1

# Dedicated read-only storage mode and remappable key identity.
OPT_DEFS += -DMY2K_DEV15_PROFILE=1
SRC += $(MY2K_MAIN)/drivers/my2k_storage_caps.c

OPT_DEFS += -DMY2K_DEV16_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_performance_mode.c $(MY2K_MAIN)/src/main_input_performance.c $(MY2K_MAIN)/src/main_input_scheduler.c

MY2K_DEV16_HS2 = 1
OPT_DEFS += -DMY2K_DEV16_HS2=1

SRC += $(MY2K_MAIN)/src/main_usb_performance.c

OPT_DEFS += -DMY2K_DEV17_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_rgb_first.c

OPT_DEFS += -DMY2K_DEV18_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_rgb_trace.c

# Stock-supported hardware I2C1, board-local transport.
OPT_DEFS += -DMY2K_DEV19_PROFILE=1

OPT_DEFS += -DMY2K_DEV21_PROFILE=1

OPT_DEFS += -DMY2K_RELEASE1_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_oled_config.c keyboards/alectardy98/matrix_my2k_100ng/common/my2k_oled_config.c

# Release 1 Lock Lights; existing RGB transport/refresh is unchanged.
OPT_DEFS += -DMY2K_LOCK_LIGHTS_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_lock_rules2.c $(MY2K_MAIN)/src/main_lock_store32.c $(MY2K_MAIN)/src/main_lock_lights2.c

OPT_DEFS += -DMY2K_EEPROM32_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_nv32.c $(MY2K_MAIN)/src/main_flash32.c

# Release1 touch configuration consumes16 bytes of the existing reserved tail.
OPT_DEFS += -DMY2K_TOUCH_CONFIG_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_touch_engine.c $(MY2K_MAIN)/src/main_touch_config.c
SRC += $(MY2K_MAIN)/src/main_factory_reset.c
OPT_DEFS += -DMY2K_ANIMATION_PROFILE=1
SRC += $(MY2K_MAIN)/src/main_animation.c
