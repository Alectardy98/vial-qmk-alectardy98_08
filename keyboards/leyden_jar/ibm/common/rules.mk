# Reuse Leyden Jar's native scanner, peripherals and diagnostics.
CUSTOM_MATRIX = lite
HAPTIC_ENABLE = yes
HAPTIC_DRIVER = solenoid
EEPROM_DRIVER = wear_leveling
WEAR_LEVELING_DRIVER = rp2040_flash
SRC += keyboards/leyden_jar/matrix.c keyboards/leyden_jar/dac.c
SRC += keyboards/leyden_jar/common.c keyboards/leyden_jar/pio_matrix_scan.c
SRC += keyboards/leyden_jar/io_expander.c keyboards/leyden_jar/leds.c
SRC += keyboards/leyden_jar/util_protocol.c
QUANTUM_LIB_SRC += i2c_master.c
