# MCU and bootloader settings for RP2040
MCU = RP2040
BOOTLOADER = rp2040

# Use custom matrix scanning code
CUSTOM_MATRIX = yes

# Include our source files
SRC += matrix.c ibmpc.c 
