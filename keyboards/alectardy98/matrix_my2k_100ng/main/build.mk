# Main-only QMK packaging. Never opens or flashes a device.
MY2K_BUILD_TOOLS := keyboards/alectardy98/matrix_my2k_100ng/tools

.PHONY: my2k-package-force
# Override QMK's generic UF2 conversion with the verified ASTRO layout. QMK
# copies only this UF2 to the root. Always repackage so output switches apply
# even when the compiled firmware has not changed.
$(BUILD_DIR)/$(TARGET).uf2: $(BUILD_DIR)/$(TARGET).bin my2k-package-force $(MY2K_BUILD_TOOLS)/package.py $(MY2K_BUILD_TOOLS)/astro_uf2.py
	python3 $(MY2K_BUILD_TOOLS)/package.py --mcu main --input $(BUILD_DIR)/$(TARGET).bin --elf $(BUILD_DIR)/$(TARGET).elf --output $(TARGET) --uf2-output $@ --generate-bin $(GENERATE_BIN) --generate-json $(GENERATE_JSON) --generate-sha256 $(GENERATE_SHA256)
