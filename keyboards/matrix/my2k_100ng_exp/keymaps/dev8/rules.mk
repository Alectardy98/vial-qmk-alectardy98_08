# SPDX-License-Identifier: GPL-2.0-or-later
VIA_ENABLE = yes
VIAL_ENABLE = yes
OPT_DEFS += -DMY2K_AUX_REPLACEMENT_AUX=1 -DMY2K_RELEASE_PROFILE=1
OPT_DEFS += -DMY2K_AUX_LINK_MANAGED=1 -DMY2K_AUX_STARTUP_CONTROL_ENABLED=1
OPT_DEFS += -DMY2K_DEV_PROFILE=1 -DMY2K_DEV_PERSISTENCE=1 -DMY2K_DEV2_PROFILE=1 -DMY2K_DEV3_PROFILE=1
MY2K_GEEHY_FS_USB_ENABLE = 1
SRC += my2k_astro_boot.c main_diag.c main_persist.c main_rgb_bus.c

OPT_DEFS += -DMY2K_DEV4_PROFILE=1
SRC += main_perf.c $(MY2K_EXPERIMENTAL)/drivers/my2k_rgb_refresh.c

OPT_DEFS += -DMY2K_STOCK_PARITY_PROFILE=1
DEBOUNCE_TYPE = custom
SRC += main_debounce.c $(MY2K_EXPERIMENTAL)/drivers/my2k_stock_effects.c $(MY2K_EXPERIMENTAL)/common/my2k_aux_ui.c
OPT_DEFS += -DUSE_CIE1931_CURVE
SRC += $(MY2K_EXPERIMENTAL)/drivers/my2k_stock_color.c quantum/led_tables.c

OPT_DEFS += -DMY2K_DEV6_PROFILE=1
SRC += main_aux_telemetry.c

OPT_DEFS += -DMY2K_DEV7_PROFILE=1 -DMY2K_DEV7_WRITABLE_MSC=0
SRC += main_storage_mode.c

OPT_DEFS += -DMY2K_DEV8_PROFILE=1
SRC += main_host_mode.c
