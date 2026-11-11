# Full software feature set for the RP2040; Vial supplies normal entry counts
# and the four-layer default. No AVR memory-reduction overrides belong here.
VIA_ENABLE = yes
VIAL_ENABLE = yes
VIAL_INSECURE = yes
LTO_ENABLE = yes
QMK_SETTINGS = yes
TAP_DANCE_ENABLE = yes
COMBO_ENABLE = yes
KEY_OVERRIDE_ENABLE = yes
REPEAT_KEY_ENABLE = yes
CAPS_WORD_ENABLE = yes
LAYER_LOCK_ENABLE = yes
DYNAMIC_MACRO_ENABLE = yes
MOUSEKEY_ENABLE = yes
EXTRAKEY_ENABLE = yes
NKRO_ENABLE = yes
UNICODE_ENABLE = yes
TRI_LAYER_ENABLE = yes

# Preserve the native safeguard against booting from an active/noisy pad.
# Console/command are developer facilities, not Vial configuration features.
BOOTMAGIC_ENABLE = no
CONSOLE_ENABLE = no
COMMAND_ENABLE = no
