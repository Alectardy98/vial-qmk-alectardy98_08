/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_usb_abort.h"
#include <string.h>
/* Independent bounded adaptation of the DWC2 endpoint-disable requirements
 * exhibited by existing ChibiOS OTGv1 and TinyUSB DWC2 implementations. Geehy
 * behavior/errata remains unverified. Never spins with interrupts disabled. */
void my2k_usb_abort_begin(my2k_usb_abort *a, uint32_t now) {
    *a = (my2k_usb_abort){.started = now};
}
bool my2k_usb_abort_pollI(my2k_usb_abort *a, USBDriver *d, usbep_t ep, uint32_t now) {
    stm32_otg_t *o = d->otg;
    if (a->failed)
        return false;
    if ((uint32_t)(now - a->started) > 100) {
        o->DCTL |= DCTL_CGONAK;
        a->failed = true;
        return false;
    }
    switch (a->stage) {
        case 0:
            o->DIEPEMPMSK &= ~(1u << ep);
            if (o->ie[ep].DIEPCTL & DIEPCTL_EPENA) {
                o->ie[ep].DIEPCTL |= DIEPCTL_SNAK;
                a->stage = 1;
            } else {
                o->ie[ep].DIEPCTL |= DIEPCTL_STALL;
                a->stage = 3;
            }
            break;
        case 1:
            if (o->ie[ep].DIEPINT & DIEPINT_INEPNE) {
                o->ie[ep].DIEPCTL |= DIEPCTL_EPDIS | DIEPCTL_STALL;
                a->stage = 2;
            }
            break;
        case 2:
            if (o->ie[ep].DIEPINT & DIEPINT_EPDISD) {
                o->ie[ep].DIEPINT = DIEPINT_EPDISD;
                a->stage          = 3;
            }
            break;
        case 3:
            if (!(o->GRSTCTL & GRSTCTL_TXFFLSH)) {
                o->GRSTCTL = GRSTCTL_TXFNUM(ep) | GRSTCTL_TXFFLSH;
                a->stage   = 4;
            }
            break;
        case 4:
            if (!(o->GRSTCTL & GRSTCTL_TXFFLSH)) {
                if (o->oe[ep].DOEPCTL & DOEPCTL_EPENA) {
                    o->DCTL |= DCTL_SGONAK;
                    a->stage = 5;
                } else {
                    o->oe[ep].DOEPCTL |= DOEPCTL_STALL;
                    a->stage = 7;
                }
            }
            break;
        case 5:
            if (o->GINTSTS & GINTSTS_GONAKEFF) {
                o->oe[ep].DOEPCTL |= DOEPCTL_EPDIS | DOEPCTL_STALL;
                a->stage = 6;
            }
            break;
        case 6:
            if (o->oe[ep].DOEPINT & DOEPINT_EPDISD) {
                o->oe[ep].DOEPINT = DOEPINT_EPDISD;
                o->DCTL |= DCTL_CGONAK;
                a->stage = 7;
            }
            break;
        case 7:
            /* Endpoint-specific stale completion flags and software busy ownership only.
             * The shared RX FIFO and HID endpoints are deliberately left intact. */
            o->ie[ep].DIEPINT = 0xffffffffu;
            o->oe[ep].DOEPINT = 0xffffffffu;
            d->transmitting &= ~(1u << ep);
            d->receiving &= ~(1u << ep);
            memset(d->epc[ep]->in_state, 0, sizeof(USBInEndpointState));
            memset(d->epc[ep]->out_state, 0, sizeof(USBOutEndpointState));
            a->stage = 8;
            return true;
        case 8:
            return true;
        default:
            a->failed = true;
            break;
    }
    return false;
}
