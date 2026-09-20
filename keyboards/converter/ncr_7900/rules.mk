CUSTOM_MATRIX = yes
SRC = matrix.c
UART_DRIVER = hardware


# AVR-GCC compatibility: expose AVR register definitions to QMK sources
OPT_DEFS += -include avr/io.h

# AVR-GCC 16 compatibility
OPT_DEFS += -Wno-error=unused-but-set-variable
