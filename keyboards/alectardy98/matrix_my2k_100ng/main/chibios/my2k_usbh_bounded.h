/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MY2K_USBH_BOUNDED_H
#define MY2K_USBH_BOUNDED_H
#include <stdbool.h>
#include <stdint.h>
/* A finite read budget works even with IRQs locked and a stopped system tick.
 * No delays, RTOS calls, retries or unrelated peripheral accesses occur here. */
#define MY2K_USBH_POLL_LIMIT 32768u
#ifndef MY2K_USBH_POLL_READ
#define MY2K_USBH_POLL_READ(reg) (*(reg))
#endif
static inline bool my2k_usbh_wait_bits(volatile uint32_t *reg,uint32_t mask,uint32_t wanted) {
    for(unsigned remaining=MY2K_USBH_POLL_LIMIT;remaining;--remaining)
        if((MY2K_USBH_POLL_READ(reg)&mask)==wanted)return true;
    return false;
}
#if MY2K_DEV7_PROFILE
#ifndef MY2K_USBH_CLOCK_READ
#define MY2K_USBH_CLOCK_READ() chSysGetRealtimeCounterX()
#endif
/* Existing ChibiOS realtime counter (DWT on this target), with an independent
 * finite read cap if it stops. No SysTick/RTOS sleep or clock-enable write. */
static inline bool my2k_usbh_delay_cycles(uint32_t cycles) {
    uint32_t start=MY2K_USBH_CLOCK_READ();
    for(unsigned polls=0;polls<1048576u;polls++)
        if((uint32_t)(MY2K_USBH_CLOCK_READ()-start)>=cycles)return true;
    return false;
}
#endif

#endif
