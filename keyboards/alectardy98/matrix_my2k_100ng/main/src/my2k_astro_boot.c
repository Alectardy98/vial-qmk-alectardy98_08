/* SPDX-License-Identifier: GPL-2.0-or-later
 * Independent ASTRO reentry implementation, compiled only by keymaps/release.
 * Original application 08028a08 -> 08029204 -> 08030690 / 080307e8:
 * enable backup writes, put ATKB in RTC backup register 0, delay, system reset.
 * Loader 08007130 consumes this marker. Entering ASTRO erases its settings
 * sector; the release profile deliberately accepts loss of those settings.
 * This code never programs flash or bypasses the original bootloader.
 */
#include "hal.h"
#include "wait.h"
#include <stdint.h>

static void astro_request(void) {
    /* PWR is already clocked by board initialization. Keep the dependency
     * explicit for callers reached after a future power-saving transition. */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    *(volatile uint32_t *)0x420e0020 = 1; /* observed DBP bit-band alias */
    (void)*(volatile uint32_t *)0x40007000;
    *(volatile uint32_t *)0x40002850 = 0x41544b42;
}

void bootloader_jump(void) {
    astro_request();
    wait_ms(11); /* stock request 10 ms plus its one-tick minimum */
    NVIC_SystemReset();
}

void mcu_reset(void) {
    NVIC_SystemReset();
}
