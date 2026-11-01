#!/usr/bin/env python3
"""Build-local fail-stop DCD adaptation of the exact pinned TinyUSB source.

The upstream checkout and its MIT license remain unchanged. Every replacement
is checked against the source digest/count; unknown revisions fail closed.
Timeouts are independent firmware policy, not measured USB controller timing.
"""
from pathlib import Path
import hashlib, json, sys

source, out = map(Path, sys.argv[1:])
raw = source.read_bytes()
digest = hashlib.sha256(raw).hexdigest()
assert digest == '9f149cadd41ca4f9c0af66932b9d4ad07c34589f470480e133947f0286434b99', digest
text = raw.decode()
changes = []

def replace(old, new, count=1):
    global text
    assert text.count(old) == count, (old, text.count(old), count)
    text = text.replace(old, new)
    changes.append({'before': old, 'after': new, 'occurrences': count})

replace('#include "dwc2_common.h"', '''#include "dwc2_common.h"
#include "aux_msc.h"
/* Board policy: fail this USB instance permanently on a runtime timeout.
 * GINT/NVIC and reconnect stay disabled until a system reset. The UART and
 * foreground worker remain available; MSC cancellation runs in foreground. */
static bool aux_flush_tx(dwc2_regs_t* dwc2, uint8_t fifo) {
  dwc2->grstctl = GRSTCTL_TXFFLSH | ((uint32_t)fifo << GRSTCTL_TXFNUM_Pos);
  return aux_usb_runtime_wait_bits(&dwc2->grstctl, GRSTCTL_TXFFLSH_Msk, 0);
}
static bool aux_flush_rx(dwc2_regs_t* dwc2) {
  dwc2->grstctl = GRSTCTL_RXFFLSH;
  return aux_usb_runtime_wait_bits(&dwc2->grstctl, GRSTCTL_RXFFLSH_Msk, 0);
}''')
replace('static void edpt_disable(', 'static bool edpt_disable(')
for expression, mask in (
    ('dep->diepint', 'DIEPINT_INEPNE'),
    ('dep->diepint', 'DIEPINT_EPDISD_Msk'),
    ('dwc2->gintsts', 'GINTSTS_BOUTNAKEFF_Msk'),
    ('dep->doepint', 'DOEPINT_EPDISD_Msk'),
):
    replace(f'while (({expression} & {mask}) == 0) {{}}',
            f'if (!aux_usb_runtime_wait_bits(&{expression}, {mask}, {mask})) return false;')
replace('    dfifo_flush_tx(dwc2, epnum);\n    usbd_spin_unlock(is_isr);',
        '    bool flushed = aux_flush_tx(dwc2, epnum);\n    usbd_spin_unlock(is_isr);\n    if (!flushed) return false;')
replace('    dep->ctl &= ~EPCTL_USBAEP;\n  }\n}',
        '    dep->ctl &= ~EPCTL_USBAEP;\n  }\n  return true;\n}')
replace('  edpt_disable(rhport, p_endpoint_desc->bEndpointAddress, false, false);',
        '  if (!edpt_disable(rhport, p_endpoint_desc->bEndpointAddress, false, false)) return false;')
replace('  edpt_disable(rhport, ep_addr, true, false);',
        '  if (!edpt_disable(rhport, ep_addr, true, false)) return;')
replace('edpt_disable(rhport, 0x80, false, true);',
        'if (!edpt_disable(rhport, 0x80, false, true)) return;', 2)
replace('        edpt_disable(rhport, epnum | TUSB_DIR_IN_MASK, false, true);',
        '        if (!edpt_disable(rhport, epnum | TUSB_DIR_IN_MASK, false, true)) return;')
replace('  dfifo_flush_tx(dwc2, 0x10); // all tx fifo\n  dfifo_flush_rx(dwc2);\n  dfifo_device_init(rhport); // re-init dfifo',
        '''  if (!aux_flush_tx(dwc2, 0x10) || !aux_flush_rx(dwc2)) {
    usbd_spin_unlock(false);
    return;
  }
  dfifo_device_init(rhport); // re-init dfifo''')
replace('  dfifo_flush_tx(dwc2, 0x10); // all tx fifo\n  dfifo_flush_rx(dwc2);',
        '  if (!aux_flush_tx(dwc2, 0x10) || !aux_flush_rx(dwc2)) return;')
replace('    handle_bus_reset(rhport);\n    usbd_spin_unlock(true);',
        '    handle_bus_reset(rhport);\n    usbd_spin_unlock(true);\n    if (aux_usb_failed()) return;')
replace('''    do {
      handle_rxflvl_irq(rhport); // read all packets
    } while(dwc2->gintsts & GINTSTS_RXFLVL);''', '''    unsigned packets = 0;
    do {
      handle_rxflvl_irq(rhport);
      if (aux_usb_failed()) return;
      /* Bound even a permanently asserted RXFLVL with a stopped SysTick.
       * This is a fail-stop budget, not an assertion of FIFO occupancy. */
      if (++packets == 256 && (dwc2->gintsts & GINTSTS_RXFLVL)) {
        aux_usb_abort((uint32_t)(uintptr_t)&dwc2->gintsts);
        return;
      }
    } while(dwc2->gintsts & GINTSTS_RXFLVL);''')
# OSAL_NONE unlock can re-enable NVIC after a failure inside a critical section.
# All public operations that could re-arm the retired controller must fail too.
void_functions = ('dcd_int_enable', 'dcd_set_address', 'dcd_remote_wakeup',
                  'dcd_connect', 'dcd_sof_enable', 'dcd_edpt_close_all',
                  'dcd_edpt_stall', 'dcd_edpt_clear_stall', 'dcd_int_handler',
                  'dcd_enter_test_mode')
bool_functions = ('dcd_init', 'dcd_edpt_open', 'dcd_edpt_iso_alloc',
                  'dcd_edpt_iso_activate', 'dcd_edpt_xfer', 'dcd_edpt_xfer_fifo')
for name in void_functions + bool_functions:
    start = text.index(('void ' if name in void_functions else 'bool ') + name + '(')
    end = text.index('{', start) + 1
    signature = text[start:end]
    replace(signature, signature + '\n  if (aux_usb_failed()) return' + (';' if name in void_functions else ' false;'))
# No original unbounded DCD hardware poll/flush may silently survive.
assert 'dfifo_flush_' not in text
assert 'while ((' not in text
out.write_text(text)
out.with_suffix('.json').write_text(json.dumps({
    'upstream': str(source), 'upstream_sha256': digest,
    'generated_sha256': hashlib.sha256(text.encode()).hexdigest(),
    'changes': changes,
    'limits': 'Runtime NAK/disable/flush waits: 20 ms or 200000 polls; RX FIFO IRQ: 256 packets. Sticky USB abort; no automatic retry or hardware validation.'
}, indent=2) + '\n')
