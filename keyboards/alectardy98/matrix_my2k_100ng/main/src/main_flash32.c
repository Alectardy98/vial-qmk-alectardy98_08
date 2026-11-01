/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_flash32.h"
#include "hal.h"
#include <string.h>
#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define FMC UINT32_C(0x40023c00)
#define END (MY2K_NV_BASE + 2u * MY2K_NV_BANK_BYTES)
bool my2k_flash32_read(uint32_t a, void *p, size_t n) {
    if (a < MY2K_NV_BASE || a > END || n > END - a || (!p && n))
        return false;
    memcpy(p, (const void *)(uintptr_t)a, n);
    return true;
}
static bool ready(void) {
    for (unsigned i = 0; i < 10000000u; i++)
        if (!(REG(FMC + 12) & 0x10000u))
            return true;
    return false;
}
/* Geehy RM V2.4 Table8/§3.7, DS V1.7 Table29. Executes from application
 * flash, preserving the existing cache/IRQ mitigation and avoiding the SRAM
 * unlock erratum. Only exact sectors10/11; never settings4/app5/loader/OPT. */
__attribute__((noipa, externally_visible)) bool my2k_flash32_operation(uint32_t address, uint32_t value, bool erase) {
    if ((!erase && (address < MY2K_NV_BASE || address > END - 4 || (address & 3))) ||
        (erase && address != MY2K_NV_BASE && address != MY2K_NV_BASE + MY2K_NV_BANK_BYTES))
        return false;
    /* Fail closed on a device which does not report the documented 1MiB array. */
    if (*(volatile const uint16_t *)(uintptr_t)UINT32_C(0x1fff7a22) != 0x0400u)
        return false;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    __DSB();
    __ISB();
    uint32_t access = REG(FMC);
    bool     ok     = ready();
    if (ok) {
        REG(FMC) = access & ~0x700u;
        REG(FMC) = (access & ~0x700u) | 0x1800u;
        REG(FMC) = access & ~0x700u;
        if (REG(FMC + 16) & 0x80000000u) {
            REG(FMC + 4) = 0x45670123u;
            REG(FMC + 4) = 0xcdef89abu;
        }
        ok = !(REG(FMC + 16) & 0x80000000u);
        if (ok) {
            REG(FMC + 12) = 0xf3u;
            if (erase) {
                uint32_t sector = address == MY2K_NV_BASE ? 10u : 11u;
                uint32_t ctrl   = 0x202u | (sector << 3);
                REG(FMC + 16)   = ctrl;
                REG(FMC + 16)   = ctrl | 0x10000u;
            } else {
                REG(FMC + 16) = 0x201u;
                REG(address)  = value;
            }
            __DSB();
            bool done = ready();
            ok        = done && !(REG(FMC + 12) & 0xf2u);
            if (done)
                REG(FMC + 16) = 0x80000000u;
        }
        REG(FMC) = (access & ~0x700u) | 0x1800u;
        REG(FMC) = access & ~0x700u;
        REG(FMC) = access;
    }
    __DSB();
    __ISB();
    __set_PRIMASK(mask);
    return ok;
}
bool my2k_flash32_word(uint32_t a, uint32_t v) {
    return my2k_flash32_operation(a, v, false);
}
bool my2k_flash32_erase(uint32_t a) {
    return my2k_flash32_operation(a, 0, true);
}
