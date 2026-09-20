CUSTOM_MATRIX = lite
ANALOG_DRIVER_REQUIRED = yes
VPATH += keyboards/cipulot/common
SRC += matrix.c ec_board.c ec_switch_matrix.c


ifeq ($(strip $(VIA_ENABLE)), yes)
    SRC += via_ec.c
endif

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
            -fomit-frame-pointer -falign-functions=16 \
            -falign-loops=16 -falign-jumps=16 -fno-exceptions -fno-unwind-tables \
            -fcommon -DVIA_CUSTOM_MENU_ENABLE

