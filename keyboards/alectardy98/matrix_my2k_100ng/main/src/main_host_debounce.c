/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "hal.h"
#include "usbh/internal.h"
#include "main_host_debounce.h"

__attribute__((noinline,noclone,externally_visible))
bool my2k_host_debounce_rearm(void) {
    bool rearm = false;
    osalSysLock();
    usbh_port_t *port = &USBHD1.rootport;
    const uint16_t level_mask = USBH_PORTSTATUS_CONNECTION | USBH_PORTSTATUS_POWER |
                                USBH_PORTSTATUS_OVERCURRENT | USBH_PORTSTATUS_SUSPEND |
                                USBH_PORTSTATUS_RESET;
    const uint16_t connected = USBH_PORTSTATUS_CONNECTION | USBH_PORTSTATUS_POWER;
    if (USBHD1.status == USBH_STATUS_STARTED && USBHD1.otg &&
        (RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN) && !usbh_lld_fault(&USBHD1) &&
        port->device.status == USBH_DEVSTATUS_ATTACHED &&
        (port->lld_c_status & USBH_PORTSTATUS_C_CONNECTION) &&
        !(port->c_status & USBH_PORTSTATUS_C_CONNECTION) &&
        (port->lld_status & level_mask) == connected &&
        (port->status & level_mask) == connected) {
        uint32_t hprt = USBHD1.otg->HPRT;
        if ((hprt & (HPRT_PCSTS | HPRT_PPWR | HPRT_POCA | HPRT_PSUSP | HPRT_PRST)) ==
            (HPRT_PCSTS | HPRT_PPWR)) {
            /* Same RAM acknowledgement as root ClearPortFeature(C_CONNECTION).
             * Keep C_ENABLE and every other change. A subsequent IRQ can set
             * C_CONNECTION again; the unchanged upper path will then abort. */
            port->lld_c_status &= ~USBH_PORTSTATUS_C_CONNECTION;
            rearm = true;
        }
    }
    osalSysUnlock();
    return rearm;
}
