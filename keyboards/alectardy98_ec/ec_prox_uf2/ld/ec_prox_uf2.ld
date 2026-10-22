/* SPDX-License-Identifier: GPL-2.0-or-later
 * Conservative 256 KiB flash window; upper flash remains untouched.
 * Sectors 0-1: TinyUF2; sector 2: EEPROM; sector 3: reserved.
 */
f4xx_flash_size = 256k;
f4xx_ram1_size = 64k;
f4xx_ram2_size = 0;
f4xx_ram4_size = 0;
f4xx_ram5_size = 0;
INCLUDE platforms/chibios/boards/common/ld/stm32f4xx_tinyuf2_common.ld
ASSERT(__flash0_base__ == 0x08010000, "Incorrect TinyUF2 app base")
ASSERT(__flash0_end__ == 0x08040000, "Application boundary mismatch")
ASSERT(__flash0_free__ <= 0x08040000, "Application overflow")
ASSERT(_board_dfu_dbl_tap == 0x2000FFFC, "TinyUF2 mailbox mismatch")
/* Keep the shared reset mailbox out of QMK's allocator. */
__heap_end__ = _board_dfu_dbl_tap;
ASSERT(__bss_end__ <= _board_dfu_dbl_tap, "RAM overlaps reset mailbox")
