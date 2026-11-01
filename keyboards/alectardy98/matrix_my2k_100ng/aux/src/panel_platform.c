/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Independent SPI PIO implementation. Wiring facts recovered from AUX GPIO/MSP
 * and initial-data tables; electrically unverified. Disabled by default. */
#if MY2K_DEV22_PROFILE
#    include "aux_perf22.h"
#endif
#include "aux_panels.h"
#include "aux_platform.h"
#include "aux_work.h"
#if MY2K_DEV21_PROFILE
#    include "aux_perf21.h"
#endif
#ifndef AUX_ENABLE_RECOVERED_PANELS
#    define AUX_ENABLE_RECOVERED_PANELS 0
#endif
#define R32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define R8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define R16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define RCC 0x40023800u
static void output(uint32_t base, unsigned pin, bool high) {
    R32(base + 0x18) = 1u << (pin + (high ? 0 : 16));
    R32(base)        = (R32(base) & ~(3u << (pin * 2))) | (1u << (pin * 2));
    R32(base + 4) &= ~(1u << pin);
    R32(base + 12) &= ~(3u << (pin * 2));
}
static void alternate(uint32_t base, unsigned pin, unsigned af) {
    R32(base) = (R32(base) & ~(3u << (pin * 2))) | (2u << (pin * 2));
    R32(base + 4) &= ~(1u << pin);
    R32(base + 8) |= 3u << (pin * 2);
    R32(base + 12) &= ~(3u << (pin * 2));
    uint32_t reg = base + 0x20 + (pin / 8) * 4, shift = (pin % 8) * 4;
    R32(reg) = (R32(reg) & ~(15u << shift)) | (af << shift);
}
static bool configure(void *context) {
    (void)context;
    if (!AUX_ENABLE_RECOVERED_PANELS)
        return false;
    R32(RCC + 0x30) |= 3u;
    R32(RCC + 0x44) |= 1u << 12;
    R32(RCC + 0x40) |= 1u << 15;
    (void)R32(RCC + 0x40);
    /* Color SPI1 PA5/6/7 AF5; mono SPI3 PB3/4/5 AF6. */
    for (unsigned p = 5; p <= 7; p++)
        alternate(0x40020000u, p, 5);
    for (unsigned p = 3; p <= 5; p++)
        alternate(0x40020400u, p, 6);
    output(0x40020000u, 4, true);
    output(0x40020000u, 8, true);
    output(0x40020000u, 9, true);
    output(0x40020000u, 10, true);
    output(0x40020400u, 6, true);
    output(0x40020400u, 7, true);
    output(0x40020400u, 8, true);
    R32(0x40013000u) = 0;
    R32(0x40013004u) = 0x1700u;
    R32(0x40013000u) = 0x347u; /* Master,mode3,/2,SSM/SSI,SPE. */
    R32(0x40003c00u) = 0;
    R32(0x40003c04u) = 0x1700u;
    R32(0x40003c00u) = 0x364u; /* Master,mode0,/32,SSM/SSI,SPE. */
    return true;
}
static bool signal(void *context, aux_panel_id panel, unsigned line, bool high) {
    (void)context;
    if (!AUX_ENABLE_RECOVERED_PANELS || line > AUX_PANEL_POWER)
        return false;
    unsigned pin;
    uint32_t base;
    if (panel == AUX_PANEL_COLOR) {
        static const unsigned pins[] = {4, 9, 8, 10};
        pin                          = pins[line];
        base                         = 0x40020000u;
    } else if (panel == AUX_PANEL_MONO && line != AUX_PANEL_POWER) {
        static const unsigned pins[] = {6, 8, 7};
        pin                          = pins[line];
        base                         = 0x40020400u;
    } else
        return false;
    R32(base + 0x18) = 1u << (pin + (high ? 0 : 16));
    return true;
}
static bool wait(uint32_t spi, uint32_t mask, uint32_t value) {
#if MY2K_DEV22_PROFILE && MY2K_SPI_READY_FAST
    /* No time read or worker checkpoint is needed when the requested condition
     * already holds. All non-ready paths below retain bounded timeout/fairness. */
    if ((R32(spi + 8) & mask) == value)
        return true;
#endif
    uint32_t start = aux_millis();
    unsigned polls = 0;
    do {
#if MY2K_DEV21_PROFILE && !MY2K_D015_PIO_CONTROL
        /* Ready hardware must not pay a worker checkpoint on every byte. The
         * send loop retains an unconditional checkpoint every32bytes. Slow/error
         * paths retain the original bounded cooperative waits. No clock/AF/DMA change. */
        if ((R32(spi + 8) & mask) == value)
            return true;
        aux_display_perf.checkpoints++;
#endif
#if MY2K_DEV22_PROFILE && MY2K_D015_PIO_CONTROL
        aux_display_perf.checkpoints++;
#endif
        aux_work_checkpoint();
        if ((R32(spi + 8) & mask) == value)
            return true;
    } while (++polls < 200000u && (uint32_t)(aux_millis() - start) < 10u);
    return false;
}
static bool send_impl(void *context, aux_panel_id panel, const uint8_t *data, size_t n) {
    (void)context;
    if (!AUX_ENABLE_RECOVERED_PANELS || !data || panel > AUX_PANEL_MONO)
        return false;
    uint32_t spi = panel == AUX_PANEL_COLOR ? 0x40013000u : 0x40003c00u;
#if MY2K_DEV21_PROFILE
    if (panel == AUX_PANEL_COLOR)
        aux_display_perf.sends++;
#endif
#if AUX_SPI_PIPELINE
    if (panel == AUX_PANEL_COLOR) {
        /* Existing8-bit format and halfword packing. Bound all unconsumed receive
         * bytes to the4-byte FIFO capacity, even when USB pauses the worker. */
        size_t   sent = 0, received = 0;
        unsigned fairness = 0;
        while (received < n) {
            bool     progress = false;
            uint32_t status   = R32(spi + 8);
            if (sent > received && (status & (1u << 10)) && sent - received >= 2u) {
                (void)R16(spi + 12);
                received += 2u;
                progress = true;
            } else if (sent > received && (status & 1u)) {
                (void)R8(spi + 12);
                received++;
                progress = true;
            }
            status = R32(spi + 8);
            if (sent < n && sent - received <= 2u && (status & 2u)) {
                size_t count = n - sent >= 2u ? 2u : 1u;
                if (count == 2u)
                    R16(spi + 12) = (uint16_t)data[sent] | (uint16_t)data[sent + 1u] << 8;
                else
                    R8(spi + 12) = data[sent];
                sent += count;
                progress = true;
#    if MY2K_DEV21_PROFILE
                aux_display_perf.bytes += (uint32_t)count;
#    endif
            }
            if (progress) {
                if (++fairness == 16u) {
                    fairness = 0;
                    aux_work_checkpoint();
                }
                continue;
            }
            if (!wait(spi, sent > received ? 1u : 2u, sent > received ? 1u : 2u)) {
#    if MY2K_DEV21_PROFILE
                aux_display_perf.failed_sends++;
#    endif
                return false;
            }
        }
        bool complete = wait(spi, 1u << 7, 0);
#    if MY2K_DEV21_PROFILE
        if (!complete)
            aux_display_perf.failed_sends++;
#    endif
        return complete;
    }
#endif
    for (size_t i = 0; i < n;) {
#if MY2K_DEV21_PROFILE && !MY2K_D015_PIO_CONTROL
        if ((i & 31u) == 0) {
            aux_display_perf.checkpoints++;
            aux_work_checkpoint();
        }
#endif
        if (!wait(spi, 2u, 2u)) {
#if MY2K_DEV21_PROFILE
            if (panel == AUX_PANEL_COLOR)
                aux_display_perf.failed_sends++;
#endif
            return false;
        }
        /* RM0431 SPI data packing: a16-bit DR access queues two8-bit frames,
         * LSB first. TXE guarantees room for two bytes. FRLVL bit10 is set
         * only once at least two received bytes may be drained as a halfword.
         * Keep DS=8 and FRXTH=8 unchanged for odd tails and all OLED traffic. */
        bool pair = panel == AUX_PANEL_COLOR && n - i >= 2u;
        if (pair)
            R16(spi + 12) = (uint16_t)data[i] | (uint16_t)data[i + 1u] << 8;
        else
            R8(spi + 12) = data[i];
        if (!wait(spi, pair ? (1u << 10) : 1u, pair ? (1u << 10) : 1u)) {
#if MY2K_DEV21_PROFILE
            if (panel == AUX_PANEL_COLOR)
                aux_display_perf.failed_sends++;
#endif
            return false;
        }
        if (pair)
            (void)R16(spi + 12);
        else
            (void)R8(spi + 12);
        size_t count = pair ? 2u : 1u;
        i += count;
#if MY2K_DEV21_PROFILE
        if (panel == AUX_PANEL_COLOR)
            aux_display_perf.bytes += (uint32_t)count;
#endif
    }
    bool complete = wait(spi, 1u << 7, 0);
#if MY2K_DEV21_PROFILE
    if (!complete && panel == AUX_PANEL_COLOR)
        aux_display_perf.failed_sends++;
#endif
    return complete;
}
static bool send(void *context, aux_panel_id panel, const uint8_t *data, size_t n) {
#if MY2K_DEV22_PROFILE
    bool scope = false;
    if (panel == AUX_PANEL_COLOR) {
        aux_perf22_count(PERF22_SEND_CALL);
        scope = aux_perf22_enter(PERF22_SPI);
    }
#endif
    bool ok = send_impl(context, panel, data, n);
#if MY2K_DEV22_PROFILE
    aux_perf22_leave(scope);
#endif
    return ok;
}
static bool delay(void *context, uint32_t ms) {
    (void)context;
    return aux_delay(ms);
}
const aux_panel_bus *aux_panel_physical_bus(void) {
    static const aux_panel_bus bus = {0, configure, signal, send, delay};
    return &bus;
}
