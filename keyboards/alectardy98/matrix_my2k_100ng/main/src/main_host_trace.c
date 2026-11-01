/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "hal.h"
#include "usbh/internal.h"
#include "timer.h"
#include "main_host_trace.h"
#include <string.h>

#define TRACE_PAGES 8u
#define TRACE_CHANNELS 8u
#define TRACE_PERIOD_MS 20u
static uint8_t  pages[TRACE_PAGES][28], channels[TRACE_CHANNELS][28];
static uint8_t  reply_pages[TRACE_PAGES][28], reply_channels[TRACE_CHANNELS][28];
static bool     reply_valid;
static uint32_t sampled_ms, samples, enters, returns, last_enter, last_return;
static uint32_t previous_hprt, hprt_changes, seen_hprt, seen_gintsts, last_change;
static bool     in_loop;
static void     put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (i * 8));
}
static void put16(uint8_t *p, uint32_t v) {
    if (v > 65535u)
        v = 65535u;
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void words(uint8_t *p, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f) {
    put32(p + 4, a);
    put32(p + 8, b);
    put32(p + 12, c);
    put32(p + 16, d);
    put32(p + 20, e);
    put32(p + 24, f);
}
void my2k_host_trace_loop(bool entering) {
    uint32_t now = timer_read32();
    osalSysLock();
    in_loop = entering;
    if (entering) {
        enters++;
        last_enter = now;
    } else {
        returns++;
        last_return = now;
    }
    osalSysUnlock();
}
/* Stable channel/URB fields are borrowed only while scheduler/host IRQs are
 * masked. Never retain pointers, walk arbitrary queues, or cancel a transfer.
 * Unassigned channels have no URB even if hardware registers retain old values. */
__attribute__((noinline, noclone, externally_visible)) void my2k_host_trace_service(uint32_t now_ms) {
    if (samples && (uint32_t)(now_ms - sampled_ms) < TRACE_PERIOD_MS)
        return;
    osalSysLock();
    bool     clock    = (RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN) != 0;
    bool     readable = clock && USBHD1.otg != NULL;
    uint32_t hprt     = readable ? USBHD1.otg->HPRT : 0;
    uint32_t gint     = readable ? USBHD1.otg->GINTSTS : 0;
    sampled_ms        = now_ms;
    samples++;
    if (samples == 1 || previous_hprt != hprt) {
        hprt_changes++;
        last_change   = now_ms;
        previous_hprt = hprt;
    }
    seen_hprt |= hprt;
    seen_gintsts |= gint;
    uint8_t flags =
        1u | (clock ? 2u : 0u) | ((hprt & HPRT_PCSTS) ? 4u : 0u) | (in_loop ? 8u : 0u) | (readable ? 16u : 0u);
    for (unsigned i = 0; i < TRACE_PAGES; i++) {
        memset(pages[i], 0, 28);
        pages[i][0] = 1;
        pages[i][1] = (uint8_t)i;
        pages[i][2] = flags;
        pages[i][3] = (uint8_t)USBHD1.rootport.device.status;
    }
    words(pages[0], sampled_ms, samples, enters, returns, last_enter, last_return);
    if (readable) {
        words(pages[1], hprt, gint, USBHD1.otg->GINTMSK, USBHD1.otg->GAHBCFG, USBHD1.otg->HAINT, USBHD1.otg->HAINTMSK);
        words(pages[2], USBHD1.otg->GUSBCFG, USBHD1.otg->GCCFG, USBHD1.otg->PCGCCTL, USBHD1.otg->GRSTCTL,
              USBHD1.otg->HCFG, USBHD1.otg->HFIR);
    }
    words(pages[3], GPIOA->MODER, GPIOA->OTYPER, GPIOA->OSPEEDR, GPIOA->PUPDR, GPIOA->AFRH, GPIOA->IDR);
    unsigned irq      = STM32_OTG1_NUMBER;
    uint32_t irqstate = ((NVIC->ISER[irq / 32] >> (irq % 32)) & 1u) |
                        (((NVIC->ISPR[irq / 32] >> (irq % 32)) & 1u) << 1) |
                        (((NVIC->IABR[irq / 32] >> (irq % 32)) & 1u) << 2) | ((uint32_t)NVIC->IP[irq] << 8);
    words(pages[4], RCC->AHB1ENR, RCC->AHB2ENR, RCC->AHB1RSTR, RCC->AHB2RSTR, RCC->APB2ENR, irqstate);
    usbh_port_t   *port = &USBHD1.rootport;
    usbh_device_t *dev  = &port->device;
    words(pages[5], (uint32_t)port->lld_status | ((uint32_t)port->lld_c_status << 16),
          (uint32_t)port->status | ((uint32_t)port->c_status << 16),
          (uint32_t)dev->status | ((uint32_t)USBHD1.status << 8) | ((uint32_t)dev->address << 16) |
              ((uint32_t)dev->bConfiguration << 24),
          (uint32_t)dev->devDesc.idVendor | ((uint32_t)dev->devDesc.idProduct << 16),
          (uint32_t)dev->ctrl.status | ((uint32_t)dev->ctrl.wMaxPacketSize << 16),
          (uint32_t)dev->devDesc.bDeviceClass | ((uint32_t)dev->devDesc.bDeviceSubClass << 8) |
              ((uint32_t)dev->devDesc.bDeviceProtocol << 16) | ((uint32_t)dev->devDesc.bNumConfigurations << 24));
    words(pages[6], hprt_changes, seen_hprt, seen_gintsts, last_change, readable ? USBHD1.otg->HFNUM : 0,
          usbh_lld_fault(&USBHD1));
    words(pages[7], GPIOB->MODER, GPIOB->OTYPER, GPIOB->OSPEEDR, GPIOB->PUPDR, GPIOB->AFRH, GPIOB->ODR);
    for (unsigned i = 0; i < TRACE_CHANNELS; i++) {
        uint8_t *p = channels[i];
        memset(p, 0, 28);
        p[0]  = 1;
        p[1]  = (uint8_t)i;
        p[20] = p[21] = p[22] = p[23] = 255;
        if (i >= USBHD1.channels_number)
            continue;
        p[2] = 1;
        if (readable) {
            put32(p + 4, USBHD1.otg->hc[i].HCCHAR);
            put32(p + 8, USBHD1.otg->hc[i].HCTSIZ);
            put32(p + 12, USBHD1.otg->hc[i].HCINT);
            put32(p + 16, USBHD1.otg->hc[i].HCINTMSK);
        }
        stm32_hc_management_t *hcm = &USBHD1.channels[i];
        p[3]                       = (uint8_t)hcm->halt_reason;
        usbh_ep_t *ep              = hcm->ep;
        if (!ep)
            continue;
        p[2] |= 2;
        p[20] = (uint8_t)ep->type;
        p[21] = (uint8_t)ep->status;
        p[23] = ep->type == USBH_EPTYPE_CTRL ? (uint8_t)ep->xfer.u.ctrl_phase : 255;
        /* List heads are initialized before an endpoint is assigned to a channel.
         * Exclude a not-yet-linked/empty head; hold no reference after unlock. */
        if (ep->urb_list.next == NULL || list_empty(&ep->urb_list))
            continue;
        usbh_urb_t *urb = list_first_entry(&ep->urb_list, usbh_urb_t, node);
        p[2] |= 4;
        if (urb->waitingThread)
            p[2] |= 8;
        if (urb->abortingThread)
            p[2] |= 16;
        p[22] = (uint8_t)urb->status;
        put16(p + 24, urb->requestedLength);
        put16(p + 26, urb->actualLength);
    }
    osalSysUnlock();
}
/* Page19/index0 freezes one complete RAM sample for subsequent pages/channels.
 * This is only a diagnostic reply cache, never a peripheral/config operation. */
static void latch_reply(void) {
    memcpy(reply_pages, pages, sizeof pages);
    memcpy(reply_channels, channels, sizeof channels);
    reply_valid = true;
}
__attribute__((noinline, noclone, externally_visible)) bool my2k_host_trace_page(uint8_t index, uint8_t report[32]) {
    if (index >= TRACE_PAGES)
        return false;
    osalSysLock();
    if (!index || !reply_valid)
        latch_reply();
    memcpy(report + 4, reply_pages[index], 28);
    osalSysUnlock();
    return true;
}
__attribute__((noinline, noclone, externally_visible)) bool my2k_host_channel_page(uint8_t index, uint8_t report[32]) {
    if (index >= TRACE_CHANNELS)
        return false;
    osalSysLock();
    if (!reply_valid)
        latch_reply();
    memcpy(report + 4, reply_channels[index], 28);
    osalSysUnlock();
    return true;
}
