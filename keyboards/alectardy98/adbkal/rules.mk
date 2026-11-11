ENCODER_ENABLE = yes
ENCODER_MAP_ENABLE = yes
CUSTOM_MATRIX = lite
DEBOUNCE_TYPE = sym_defer_pk
SRC += matrix.c

# Sharp Memory LCD uses RP2040 SPI0; SCS is driven separately (active high).
SPI_DRIVER_REQUIRED = yes
SRC += display.c
WPM_ENABLE = yes
