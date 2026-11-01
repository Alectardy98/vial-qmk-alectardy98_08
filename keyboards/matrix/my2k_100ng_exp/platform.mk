# SPDX-License-Identifier: GPL-2.0-or-later
include $(CHIBIOS)/os/hal/ports/STM32/STM32F4xx/platform.mk
include $(CHIBIOS_CONTRIB)/os/hal/ports/STM32/LLD/USBHv1/driver.mk

ifeq ($(MY2K_ACTIVE_DEV_PROFILE),1)
MY2K_DEBUG_HOST_LLD = $(MY2K_EXPERIMENTAL)/generated/r1debug_host_lld.c
PLATFORMSRC_CONTRIB := $(filter-out $(CHIBIOS_CONTRIB)/os/hal/ports/STM32/LLD/USBHv1/hal_usbh_lld.c,$(PLATFORMSRC_CONTRIB))
PLATFORMSRC_CONTRIB += $(MY2K_DEBUG_HOST_LLD)
$(MY2K_DEBUG_HOST_LLD): $(MY2K_EXPERIMENTAL)/scripts/prepare-debug-host.py $(CHIBIOS_CONTRIB)/os/hal/ports/STM32/LLD/USBHv1/hal_usbh_lld.c
	python3 $(MY2K_EXPERIMENTAL)/scripts/prepare-debug-host.py
endif

ifeq ($(MY2K_DEV16_HS2),1)
MY2K_LOCAL_USB_LLD = $(MY2K_EXPERIMENTAL)/generated/dev16_usb_lld.c
PLATFORMSRC := $(filter-out $(CHIBIOS)/os/hal/ports/STM32/LLD/OTGv1/hal_usb_lld.c,$(PLATFORMSRC))
PLATFORMSRC += $(MY2K_LOCAL_USB_LLD)
$(MY2K_LOCAL_USB_LLD): $(MY2K_EXPERIMENTAL)/scripts/prepare-dev16-usb-lld.py $(CHIBIOS)/os/hal/ports/STM32/LLD/OTGv1/hal_usb_lld.c
	python3 $(MY2K_EXPERIMENTAL)/scripts/prepare-dev16-usb-lld.py --output $@
endif
