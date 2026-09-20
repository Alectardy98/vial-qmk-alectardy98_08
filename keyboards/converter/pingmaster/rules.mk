CUSTOM_MATRIX = yes

# project specific files
SRC += matrix.c
#SRC += ibm4707.c
OPT_DEFS += -DIBM4704_ALPS

# AVR-GCC 16 compatibility
OPT_DEFS += -Wno-error=array-bounds

# AVR-GCC compatibility: expose AVR register definitions to QMK sources
OPT_DEFS += -include avr/io.h

# AVR-GCC 16 compatibility
OPT_DEFS += -Wno-error=unused-but-set-variable
