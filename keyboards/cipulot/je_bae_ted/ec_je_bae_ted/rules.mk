CUSTOM_MATRIX = lite
ANALOG_DRIVER_REQUIRED = yes
SRC += matrix.c ec_switch_matrix.c

MCUFLAGS += -march=armv7e-m \
            -mcpu=cortex-m4 \
            -mfloat-abi=hard \
            -mfpu=fpv4-sp-d16 \
            -mabi=aapcs \
            -mcmodel=small \
            -mstrict-align \
            -mthumb

OPT_DEFS += -Ofast -ffast-math -funroll-loops \
            -fno-tree-vectorize -fno-signed-zeros -fno-math-errno \
            -fno-common -fomit-frame-pointer -falign-functions=16 \
            -falign-loops=16 -falign-jumps=16 -fno-exceptions -fno-unwind-tables
