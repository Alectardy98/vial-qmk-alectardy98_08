VIA_ENABLE = yes
VIAL_ENABLE = yes
VIAL_INSECURE = yes

LTO_ENABLE = yes
QMK_SETTINGS = no
CAPS_WORD_ENABLE = no
LAYER_LOCK_ENABLE = no
REPEAT_KEY_ENABLE = no

OPT_DEFS += -include avr/io.h
OPT_DEFS += -Wno-error=unused-but-set-variable
