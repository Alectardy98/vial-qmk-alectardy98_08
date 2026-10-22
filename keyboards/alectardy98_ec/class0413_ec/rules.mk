# Standard QMK EEPROM and wear-leveling backend, with keyboard-local guards.
CUSTOM_MATRIX = lite
EEPROM_DRIVER = wear_leveling
WEAR_LEVELING_DRIVER = embedded_flash
MCU_LDSCRIPT = class0413_ec
OPT = s
VIA_ENABLE = yes
ANALOG_DRIVER_REQUIRED = yes
KEYBOARD_SHARED_EP = yes
SRC += matrix.c keyboards/alectardy98_ec/common/runtime/ec_board.c keyboards/alectardy98_ec/common/engine/calibration_console.c keyboards/alectardy98_ec/common/engine/ec_diagnostic.c
SRC += modules/storage_state.c modules/geometry.c modules/storage_guards.c

# Native QMK features; the test targets do not inherit these make flags.
OPT_DEFS += -DEC_NATIVE_ACTUATORS
AUDIO_ENABLE = yes
AUDIO_DRIVER = pwm_software
HAPTIC_ENABLE = yes
HAPTIC_DRIVER = solenoid
SRC += modules/native_actuators.c
# Music Mode is disabled in this experimental target to fund Variable Actuation.
OPT_DEFS += -DNO_MUSIC_MODE

# Native Audio Clicky is enabled with #define AUDIO_CLICKY in config.h.

# Stock TinyUF2 family; both default UF2 and explicit BIN builds are application-only.
UF2_FAMILY = 0x57755a57

# Headers shared by the root matrix/keyboard hooks and custom modules.
EXTRAINCDIRS += keyboards/alectardy98_ec/class0413_ec/modules

SRC += keyboards/alectardy98_ec/common/engine/calibration.c

EXTRALDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref



EXTRAINCDIRS += keyboards/alectardy98_ec/common/engine

SRC += keyboards/alectardy98_ec/common/runtime/record_io.c

EXTRAINCDIRS += keyboards/alectardy98_ec/common/interfaces

SRC += keyboards/alectardy98_ec/common/runtime/decision.c
