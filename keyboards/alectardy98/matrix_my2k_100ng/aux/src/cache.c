/* SPDX-License-Identifier: GPL-2.0-or-later */
/* ST AN4667/PM0253: AXI Flash execution uses the Cortex-M7 instruction cache.
 * CMSIS invalidates before enabling and supplies DSB/ISB ordering. Data cache
 * remains untouched; no DMA/cache ownership rules are changed. */
#include "stm32f722xx.h"
void aux_instruction_cache_enable(void) {
    if(!(SCB->CCR & SCB_CCR_IC_Msk)) SCB_EnableICache();
}
