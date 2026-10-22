/* Reported 128 KiB flash; sector 4 application, sector 2 guarded EEPROM. */
f4xx_flash_size = 128k;
f4xx_ram1_size = 64k;
f4xx_ram2_size = 0;
f4xx_ram4_size = 0;
f4xx_ram5_size = 0;
INCLUDE platforms/chibios/boards/common/ld/stm32f4xx_tinyuf2_common.ld

/* Match the installed bootloader and keep boot/settings/EEPROM out of images. */
ASSERT(__flash0_base__ == 0x08010000, "Wrong application base")
ASSERT(__flash0_end__ == 0x08020000, "Application overlaps EEPROM sector")
ASSERT(__flash0_free__ <= 0x08020000, "Application exceeds sector 4")
ASSERT(_board_dfu_dbl_tap == 0x2000FFFC, "Wrong TinyUF2 reset mailbox")
