#!/usr/bin/env python3
"""Generate the MY-2K-only embedded HS2 adaptation; never edit shared ChibiOS.

The pinned input includes the board-local resume safety patch.
Upstream ISR/endpoint/FIFO code is retained byte-for-byte except ENUMDNE's
RAM-only speed notification. Hardware waits added here are bounded.
"""
from pathlib import Path
import argparse
import hashlib
import json

REPO = next(p for p in Path(__file__).resolve().parents if (p / "quantum").is_dir())
UPSTREAM = REPO / ".build/my2k-chibios/chibios/os/hal/ports/STM32/LLD/OTGv1/hal_usb_lld.c"
EXPECTED_SHA256 = "54bcfcd95be589bb7acd828c3ada9411658aff648ca7ce32685fbaa60d5fcf07"
DEFAULT_OUTPUT = REPO / ".build/my2k-generated/dev16_usb_lld.c"

def replace_once(text, before, after):
    if text.count(before) != 1:
        raise ValueError("LLD template no longer matches exactly: " + before[:80])
    return text.replace(before, after, 1)

def generate(original):
    digest = hashlib.sha256(original).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError("Shared LLD SHA256 mismatch: " + digest)
    text = original.decode()
    text = replace_once(text, '#include "hal.h"', '''#include "hal.h"
#include "my2k_usb_lld_hooks.h"
#include "main_performance_mode.h"
#include "my2k_hs2_usb.h"
#if !MY2K_DEV16_HS2
#error "Generated Dev16 USB driver requires its board-local runtime HS2 hooks"
#endif
static bool my2k_hs2_start_failed;
bool my2k_hs2_usb_start_failed(void) {
  return my2k_hs2_start_failed;
}''')
    start = text.index("static void otg_core_reset(USBDriver *usbp) {")
    end = text.index("\nstatic void otg_disable_ep", start)
    text = text[:start] + '''static bool otg_core_reset(USBDriver *usbp) {
  stm32_otg_t *otgp = usbp->otg;
  if (!my2k_hs2_wait_bits(&otgp->GRSTCTL, GRSTCTL_AHBIDL,
                          GRSTCTL_AHBIDL)) {
    my2k_performance_usb_fault(MY2K_HS2_FAULT_AHB_IDLE);
    return false;
  }
  otgp->GRSTCTL = GRSTCTL_CSRST;
  osalSysPolledDelayX(12);
  if (!my2k_hs2_wait_bits(&otgp->GRSTCTL, GRSTCTL_CSRST, 0u)) {
    my2k_performance_usb_fault(MY2K_HS2_FAULT_CORE_RESET);
    return false;
  }
  osalSysPolledDelayX(18);
  if (!my2k_hs2_wait_bits(&otgp->GRSTCTL, GRSTCTL_AHBIDL,
                          GRSTCTL_AHBIDL)) {
    my2k_performance_usb_fault(MY2K_HS2_FAULT_AHB_IDLE);
    return false;
  }
  return true;
}
''' + text[end:]
    text = replace_once(text, '''void usb_lld_start(USBDriver *usbp) {
  stm32_otg_t *otgp = usbp->otg;

  if (usbp->state == USB_STOP) {''', '''void usb_lld_start(USBDriver *usbp) {
  stm32_otg_t *otgp = usbp->otg;
  bool my2k_hs2 = false;

  if (usbp->state == USB_STOP) {
    my2k_hs2_start_failed = false;''')
    # Keep all common clock/vector/endpoint logic. Replace only the OTG2 PHY
    # selection block; OTG1 is not a USB device on this board.
    before = '''      /* - Forced device mode.
         - USB turn-around time = TRDT_VALUE_HS or TRDT_VALUE_FS.*/
#if defined(BOARD_OTG2_USES_ULPI)
      /* High speed ULPI PHY.*/
      otgp->GUSBCFG = GUSBCFG_FDMOD | GUSBCFG_TRDT(TRDT_VALUE_HS) |
                      GUSBCFG_SRPCAP | GUSBCFG_HNPCAP;
#else
      otgp->GUSBCFG = GUSBCFG_FDMOD | GUSBCFG_TRDT(TRDT_VALUE_FS) |
                      GUSBCFG_PHYSEL;
#endif

#if defined(BOARD_OTG2_USES_ULPI)
#if STM32_USE_USB_OTG2_HS
      /* USB 2.0 High Speed PHY in HS mode.*/
      otgp->DCFG = 0x02200000 | DCFG_DSPD_HS | BOARD_OTG2_ULPI_CHIRP_DELAY_MASK;
#else
      /* USB 2.0 High Speed PHY in FS mode.*/
      otgp->DCFG = 0x02200000 | DCFG_DSPD_HS_FS;
#endif
#else
      /* 48MHz 1.1 PHY.*/
      otgp->DCFG = 0x02200000 | DCFG_DSPD_FS11;
#endif'''
    after = '''      /* Embedded Geehy HS2 shares the HS controller/pins/IRQ; it is not
       * an external ULPI PHY and must not enable the ULPI clock. */
      /* The legacy serial FS PHY fails control transfers behind tested
       * USB-A hubs. Use the proven embedded HS2 PHY in both profiles;
       * Standard still forces 12 Mb/s and 1 ms HID polling. */
      my2k_hs2 = true;
      if (my2k_hs2) {
        otgp->DCTL |= DCTL_SDIS;
        my2k_hs2_phy_prepare(MY2K_HS2_SHADOW_WINDOW);
        /* Stock selects the embedded HS PHY before resetting the core. */
        otgp->GUSBCFG &= ~GUSBCFG_PHYSEL;
      }
      else {
        /* Dev15's known-working standard full-speed path. */
        my2k_hs2_phy_deselect(MY2K_HS2_SHADOW_WINDOW);
        otgp->GUSBCFG = GUSBCFG_FDMOD | GUSBCFG_TRDT(TRDT_VALUE_FS) |
                        GUSBCFG_PHYSEL;
        otgp->DCFG = 0x02200000 | DCFG_DSPD_FS11;
      }'''
    text = replace_once(text, before, after)
    text = replace_once(text, '''    /* Soft core reset.*/
    otg_core_reset(usbp);''', '''    /* Bounded reset. Recover once to standard FS after an HS-start
     * failure; never reset the MCU, touch OTG1, or retry indefinitely. */
    if (!otg_core_reset(usbp)) {
      if (my2k_hs2) {
        my2k_performance_usb_fault(MY2K_HS2_FAULT_FS_FALLBACK);
        my2k_hs2 = false;
        rccResetOTG_HS();
        /* Return the embedded-PHY selector to its reset/FS selection. */
        my2k_hs2_phy_deselect(MY2K_HS2_SHADOW_WINDOW);
        otgp->DCTL |= DCTL_SDIS;
        otgp->GUSBCFG = GUSBCFG_FDMOD | GUSBCFG_TRDT(TRDT_VALUE_FS) |
                        GUSBCFG_PHYSEL;
        otgp->DCFG = 0x02200000 | DCFG_DSPD_FS11;
        otgp->PCGCCTL = 0;
        otgp->GOTGCTL = GOTGCTL_BVALOEN | GOTGCTL_BVALOVAL;
        otgp->GCCFG = GCCFG_INIT_VALUE;
        if (!otg_core_reset(usbp)) {
          my2k_hs2_start_failed = true;
          my2k_performance_usb_fault(MY2K_HS2_FAULT_START_FAILED);
          otgp->DCTL |= DCTL_SDIS;
          return;
        }
      }
      else {
        my2k_hs2_start_failed = true;
        my2k_performance_usb_fault(MY2K_HS2_FAULT_START_FAILED);
        otgp->DCTL |= DCTL_SDIS;
        return;
      }
    }
    if (my2k_hs2) {
      /* Actual stock PHY-mode values, independently expressed. Keep the
       * existing ChibiOS periodic-frame scheduling bits and EP machinery. */
      otgp->GUSBCFG = (otgp->GUSBCFG &
                      ~(GUSBCFG_PHYSEL | GUSBCFG_TRDT_MASK |
                        GUSBCFG_TOCAL_MASK | GUSBCFG_FHMOD)) |
                      GUSBCFG_FDMOD | (9u << 10) | GUSBCFG_TOCAL(7u);
      otgp->DCFG = 0x02200000 | DCFG_NZLSOHSK |
                   (my2k_performance_hs_requested() ? DCFG_DSPD_HS
                                                     : DCFG_DSPD_HS_FS);
    }''')
    text = replace_once(text, '''  if (sts & GINTSTS_ENUMDNE) {
    /* Full or High speed timing selection.*/''', '''  if (sts & GINTSTS_ENUMDNE) {
    /* IRQ-only RAM cache; never persists settings or calls USB synchronously. */
    my2k_performance_usb_enumerated(
        (uint8_t)((otgp->DSTS & DSTS_ENUMSPD_MASK) >> 1));
    /* Full or High speed timing selection.*/''')
    return text.encode()

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    ap.add_argument("--source", type=Path, default=UPSTREAM)
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    original = args.source.read_bytes()
    output = generate(original)
    if args.check:
        if args.output.read_bytes() != output:
            raise SystemExit("Generated LLD differs; run generator first")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        if not args.output.exists() or args.output.read_bytes() != output:
            args.output.write_bytes(output)
    print(json.dumps({"upstream_sha256": hashlib.sha256(original).hexdigest(),
                      "generated_sha256": hashlib.sha256(output).hexdigest(),
                      "output": str(args.output), "shared_source_modified": False,
                      "checked": args.check}, sort_keys=True))

if __name__ == "__main__":
    main()

