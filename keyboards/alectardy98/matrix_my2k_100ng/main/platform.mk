# SPDX-License-Identifier: GPL-2.0-or-later
# Build private copies before QMK includes the Contrib HAL makefiles. Redirect
# only this board's Contrib sources/headers; never patch a submodule in place.
MY2K_CHIBIOS_PREPARE_STATUS := $(shell python3 keyboards/alectardy98/matrix_my2k_100ng/tools/prepare-chibios.py --output $(MY2K_CHIBIOS_OVERLAY) >/dev/null; echo $$?)
ifneq ($(MY2K_CHIBIOS_PREPARE_STATUS),0)
$(error MY-2K private ChibiOS generation failed; see the error above)
endif
CHIBIOS_CONTRIB := $(MY2K_LOCAL_CONTRIB)

include $(CHIBIOS)/os/hal/ports/STM32/STM32F4xx/platform.mk
include $(CHIBIOS_CONTRIB)/os/hal/ports/STM32/LLD/USBHv1/driver.mk

ifeq ($(MY2K_ACTIVE_DEV_PROFILE),1)
MY2K_DEBUG_HOST_LLD = $(INTERMEDIATE_OUTPUT)/my2k-generated/r1debug_host_lld.c
PLATFORMSRC_CONTRIB := $(filter-out $(CHIBIOS_CONTRIB)/os/hal/ports/STM32/LLD/USBHv1/hal_usbh_lld.c,$(PLATFORMSRC_CONTRIB))
PLATFORMSRC_CONTRIB += $(MY2K_DEBUG_HOST_LLD)
$(MY2K_DEBUG_HOST_LLD): keyboards/alectardy98/matrix_my2k_100ng/tools/prepare-debug-host.py $(CHIBIOS_CONTRIB)/os/hal/ports/STM32/LLD/USBHv1/hal_usbh_lld.c
	python3 keyboards/alectardy98/matrix_my2k_100ng/tools/prepare-debug-host.py --source $(CHIBIOS_CONTRIB)/os/hal/ports/STM32/LLD/USBHv1/hal_usbh_lld.c --output $@
endif

ifeq ($(MY2K_DEV16_HS2),1)
MY2K_LOCAL_USB_LLD = $(INTERMEDIATE_OUTPUT)/my2k-generated/dev16_usb_lld.c
PLATFORMSRC := $(filter-out $(CHIBIOS)/os/hal/ports/STM32/LLD/OTGv1/hal_usb_lld.c,$(PLATFORMSRC))
PLATFORMSRC += $(MY2K_LOCAL_USB_LLD)
MY2K_GUARDED_USB_LLD = $(MY2K_CHIBIOS_OVERLAY)/chibios/os/hal/ports/STM32/LLD/OTGv1/hal_usb_lld.c
$(MY2K_LOCAL_USB_LLD): keyboards/alectardy98/matrix_my2k_100ng/tools/prepare-dev16-usb-lld.py $(MY2K_GUARDED_USB_LLD)
	python3 keyboards/alectardy98/matrix_my2k_100ng/tools/prepare-dev16-usb-lld.py --source $(MY2K_GUARDED_USB_LLD) --output $@
endif
