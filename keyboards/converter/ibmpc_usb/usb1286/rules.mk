CUSTOM_MATRIX = yes
SRC += matrix.c ibmpc.c

# AVR-GCC 16 compatibility
OPT_DEFS += -Wno-error=unused-but-set-variable
