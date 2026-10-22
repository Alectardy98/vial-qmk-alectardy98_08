// Copyright 2026 Alectardy98
// SPDX-License-Identifier: GPL-2.0-or-later
#include "hal.h"
#include "bootloader.h"

/* Installed ASTRO loader: dump main 0x08004e54 reads RTC_BKP0R and tests
 * 0x41544b42. This strong QMK hook replaces the weak TinyUF2 SRAM handoff;
 * the build selector is retained solely for the existing UF2 build pipeline.
 */
void bootloader_jump(void) {
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR; // Complete the peripheral clock enable before PWR access.
    PWR->CR |= PWR_CR_DBP;
    for (unsigned retry = 0; retry < 1024; ++retry) {
        if (PWR->CR & PWR_CR_DBP) {
            /* Backup registers use APB, not RTC calendar shadow registers:
             * no oscillator/source change, WPR unlock or RSF wait is needed.
             * Never reset the backup domain to request a boot.
             */
            /* Loader RCC setup (dump 0x08002af4) preserves BKP only when
             * RTCSEL is unselected or already LSI. An unexpected source would
             * make it reset the domain and lose this request: fail safely.
             */
            uint32_t source = RCC->BDCR & RCC_BDCR_RTCSEL;
            if (source != 0 && source != RCC_BDCR_RTCSEL_1) return;
            RTC->BKP0R = 0x41544b42U;
            __DSB();
            if (RTC->BKP0R == 0x41544b42U) NVIC_SystemReset();
            return; // Fail without reset if the handoff could not be verified.
        }
    }
}
