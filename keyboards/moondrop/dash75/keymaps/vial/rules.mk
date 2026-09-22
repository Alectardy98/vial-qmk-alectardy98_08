# Vial options
VIA_ENABLE = yes
VIAL_ENABLE = yes

LTO_ENABLE = yes
CAPS_WORD_ENABLE = no 
LAYER_LOCK_ENABLE = no 
REPEAT_KEY_ENABLE = no

OPT_DEFS += -include avr/io.h
OPT_DEFS += -Wno-error=unused-but-set-variable
