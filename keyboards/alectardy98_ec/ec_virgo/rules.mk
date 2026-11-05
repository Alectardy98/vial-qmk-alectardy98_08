# Board-local wiring/packaging; all decisions and protocol use the common engine.
OPT = 3
CUSTOM_MATRIX = lite
ANALOG_DRIVER_REQUIRED = yes
EEPROM_DRIVER = wear_leveling
WEAR_LEVELING_DRIVER = embedded_flash
VIA_ENABLE = yes
VIAL_INSECURE = yes
MCU_LDSCRIPT = ec_virgo_ec42
EXTRAINCDIRS += keyboards/alectardy98_ec/common/engine
EXTRAINCDIRS += keyboards/alectardy98_ec/common/runtime
EXTRAINCDIRS += keyboards/alectardy98_ec/common/interfaces
SRC += matrix.c acquisition.c
SRC += keyboards/alectardy98_ec/common/engine/calibration.c
SRC += keyboards/alectardy98_ec/common/engine/ec_diagnostic.c
SRC += keyboards/alectardy98_ec/common/runtime/decision.c
SRC += keyboards/alectardy98_ec/common/runtime/ec_board.c
SRC += keyboards/alectardy98_ec/common/runtime/integration.c
SRC += keyboards/alectardy98_ec/common/runtime/storage.c
EXTRALDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref
