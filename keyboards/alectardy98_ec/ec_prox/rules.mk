OPT = 2

EXTRALDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref

CUSTOM_MATRIX = lite
ANALOG_DRIVER_REQUIRED = yes
EEPROM_DRIVER = vendor # original STM32/QMK selection
WEAR_LEVELING_DRIVER = embedded_flash
EXTRAINCDIRS += keyboards/alectardy98_ec/common/engine keyboards/alectardy98_ec/common/runtime
SRC += keyboards/alectardy98_ec/common/engine/calibration.c keyboards/alectardy98_ec/common/engine/ec_diagnostic.c
SRC += keyboards/alectardy98_ec/common/runtime/decision.c keyboards/alectardy98_ec/common/runtime/ec_board.c keyboards/alectardy98_ec/common/runtime/integration.c storage_diagnostics.c
SRC += acquisition.c
SRC += matrix.c

# Every keymap needs the EC configuration channel before keys are calibrated.
VIA_ENABLE = yes
VIAL_INSECURE = yes

EXTRAINCDIRS += keyboards/alectardy98_ec/common/interfaces
