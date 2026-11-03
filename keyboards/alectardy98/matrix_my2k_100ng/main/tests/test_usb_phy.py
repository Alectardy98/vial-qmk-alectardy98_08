#!/usr/bin/env python3
"""Run the generated USB startup with real register definitions/PHY helpers.

No hardware access. Clock/IRQ/reset boundaries are mocked, including bounded
reset failure, while the actual board-generated usb_lld_start is compiled.
"""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

MAIN = Path(__file__).resolve().parents[1]
REPO = next(p for p in MAIN.parents if (p / "quantum").is_dir())
spec = importlib.util.spec_from_file_location("lld", MAIN.parent / "tools/prepare-dev16-usb-lld.py")
lld = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lld)


def function(source, signature):
    start = source.index(signature)
    pos = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[start:pos]


PREAMBLE = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "stm32_otg.h"
static uint8_t shadow[40];
#define MY2K_HS2_SHADOW_WINDOW shadow
#include "my2k_hs2_usb.h"
#define STM32_USB_USE_OTG1 0
#define STM32_USB_USE_OTG2 1
#define STM32_OTG2_NUMBER 77
#define STM32_USB_OTG2_IRQ_PRIORITY 5
#define USB_STOP 0
#define TRDT_VALUE_FS 6
#define GCCFG_INIT_VALUE (GCCFG_NOVBUSSENS|GCCFG_PWRDWN)
typedef struct { void (*sof_cb)(void); } USBConfig;
typedef struct { stm32_otg_t *otg; unsigned state; USBConfig *config; } USBDriver;
static USBDriver USBD2;
static bool high, my2k_hs2_start_failed;
static unsigned faults, resets, fail_resets, clocks, ulpi_disabled, irqs, disabled;
static bool my2k_performance_hs_requested(void) { return high; }
static void my2k_performance_usb_fault(unsigned bits) {faults|=bits; if(bits&4)high=false;}
static bool otg_core_reset(USBDriver *d) {(void)d;return ++resets>fail_resets;}
static void rccEnableOTG_HS(bool on) { assert(on);clocks++; }
static void rccResetOTG_HS(void) {}
static void rccDisableOTG_HSULPI(void) {ulpi_disabled++;}
static void nvicEnableVector(unsigned n,unsigned p) {assert(n==77&&p==5);irqs++;}
static void otg_disable_ep(USBDriver *d) {(void)d;disabled++;}
'''

HARNESS = r'''
int main(void) {
    stm32_otg_t regs;
    USBConfig config={0};
    for(unsigned mode=0;mode<2;mode++)for(unsigned failures=0;failures<3;failures++) {
        memset(&regs,0,sizeof regs);memset(shadow,0xaa,sizeof shadow);
        USBD2=(USBDriver){&regs,USB_STOP,&config};high=mode;
        resets=faults=clocks=ulpi_disabled=irqs=disabled=0;fail_resets=failures;
        usb_lld_start(&USBD2);
        assert(clocks==1&&ulpi_disabled==1&&irqs==1);
        assert((shadow[4]&1)&&(shadow[16]&1)&&(shadow[36]&7)==5);
        if(!failures) {
            assert((shadow[0]&1)&&!(regs.GUSBCFG&GUSBCFG_PHYSEL));
            assert((regs.GUSBCFG&GUSBCFG_FDMOD)&&!(regs.GUSBCFG&GUSBCFG_FHMOD));
            assert((regs.GUSBCFG&GUSBCFG_TOCAL_MASK)==GUSBCFG_TOCAL(7));
            assert((regs.DCFG&DCFG_DSPD_MASK)==(mode?DCFG_DSPD_HS:DCFG_DSPD_HS_FS));
            assert((regs.DCFG&DCFG_NZLSOHSK)&&!faults&&!my2k_hs2_start_failed);
            assert(resets==1&&disabled==1&&(regs.GAHBCFG&GAHBCFG_GINTMSK));
        } else {
            assert(resets==2&&!(shadow[0]&1)&&(regs.GUSBCFG&GUSBCFG_PHYSEL));
            assert((regs.DCFG&DCFG_DSPD_MASK)==DCFG_DSPD_FS11);
            assert(faults==(failures==1?4u:12u));
            assert(my2k_hs2_start_failed==(failures==2));
            if(failures==2) assert((regs.DCTL&DCTL_SDIS)&&!disabled&&!regs.GAHBCFG);
            else assert(disabled==1&&(regs.GAHBCFG&GAHBCFG_GINTMSK));
        }
    }
    // Starting an already active controller must not select/reset either PHY.
    USBD2.state=1;resets=clocks=0;usb_lld_start(&USBD2);assert(!resets&&!clocks);
}
'''


class USBPHY(unittest.TestCase):
    def test_generated_startup(self):
        source = lld.generate(lld.UPSTREAM.read_bytes()).decode()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            c = root / "phy.c"
            c.write_text(PREAMBLE + function(source, "void usb_lld_start(") + HARNESS)
            flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-g", "-O1"]
            if os.environ.get("SANITIZE") == "1":
                flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie"]
            binary = root / "phy"
            subprocess.run(["cc", *flags, "-I", str(MAIN / "src"), "-I",
                            str(REPO / "lib/chibios/os/hal/ports/STM32/LLD/OTGv1"),
                            str(c), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
        print("PASS actual generated Standard/High startup, bounded legacy fallback, terminal failure and active no-op")


if __name__ == "__main__":
    unittest.main()
