/* SPDX-License-Identifier: GPL-3.0-or-later
 * Preserve the STM32F411CCU6 split vector/EEPROM/application layout. */
f4xx_flash_size = 256k;
f4xx_ram1_size = 128k;
f4xx_ram2_size = 0;
f4xx_ram4_size = 0;
f4xx_ram5_size = 0;
INCLUDE platforms/chibios/boards/common/ld/stm32f4xx_common.ld
ASSERT(__flash0_base__ == 0x08000000, "Virgo vector base mismatch")
ASSERT(__flash1_base__ == 0x08004000, "Virgo EEPROM base mismatch")
ASSERT(__flash1_end__ == 0x08008000, "Virgo EEPROM boundary mismatch")
ASSERT(__flash2_base__ == 0x08008000, "Virgo code base mismatch")
ASSERT(__flash2_free__ <= 0x08040000, "Virgo code exceeds STM32F411CCU6 flash")
