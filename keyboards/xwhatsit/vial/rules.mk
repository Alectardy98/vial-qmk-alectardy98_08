# Keep the extension local to the requested Vial keymaps. Other xwhatsit builds
# retain their original utility protocol and constant calibration path.
ifneq ($(filter vial vial_alec,$(KEYMAP)),)
XWHATSIT_RUNTIME_CALIBRATION ?= yes
ifeq ($(XWHATSIT_RUNTIME_CALIBRATION),yes)
OPT_DEFS += -DXWHATSIT_RUNTIME_CALIBRATION
SRC += keyboards/xwhatsit/calibration_config.c
endif
endif
