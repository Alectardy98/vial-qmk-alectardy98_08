CUSTOM_MATRIX = yes
PS2_DRIVER = usart
SRC += matrix.c ibmpc.c

# AVR-GCC 16 compatibility
OPT_DEFS += -Wno-error=unused-but-set-variable
