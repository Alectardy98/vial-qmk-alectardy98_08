# Standalone Cortex-M7 AUX build, integrated by the repository GNUmakefile.
# No QMK Main sources or dummy keyboard objects are compiled into AUX.
.DEFAULT_GOAL := all
AUX_DIR := keyboards/alectardy98/matrix_my2k_100ng/aux
include $(AUX_DIR)/rules.mk
MY2K_BUILD_TOOLS := keyboards/alectardy98/matrix_my2k_100ng/tools
AUX_TARGET := alectardy98_matrix_my2k_100ng_aux_aux
AUX_ELF := .build/$(AUX_TARGET).elf
.PHONY: all
all:
	python3 $(MY2K_BUILD_TOOLS)/build-aux.py --elf $(AUX_ELF)
	python3 $(MY2K_BUILD_TOOLS)/package.py --mcu aux --input .build/$(AUX_TARGET).bin --elf $(AUX_ELF) --output $(AUX_TARGET) --generate-bin $(GENERATE_BIN) --generate-json $(GENERATE_JSON) --generate-sha256 $(GENERATE_SHA256)
