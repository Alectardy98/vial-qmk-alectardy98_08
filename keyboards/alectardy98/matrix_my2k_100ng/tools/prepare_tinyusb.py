#!/usr/bin/env python3
"""Generate a local bounded startup adaptation; never modify TinyUSB checkout.

MIT upstream copyright/license retained. Exact source digest and replacement
counts fail closed when the pinned dependency changes. This only modifies the
AUX build's common core initializer, not DCD endpoint/IRQ runtime semantics.
"""
from pathlib import Path
import hashlib,json,sys
source,out=map(Path,sys.argv[1:])
raw=source.read_bytes();digest=hashlib.sha256(raw).hexdigest()
assert digest=='86110d78f398aaaacfb5f1fd54fbe9855190f18ba099d6e23e0698dff28f0a5c',digest
text=raw.decode();changes=[]
def replace(old,new,count=1):
 global text
 assert text.count(old)==count,(old,text.count(old),count)
 text=text.replace(old,new);changes.append({'before':old,'after':new,'occurrences':count})
replace('#include "dwc2_common.h"','#include "dwc2_common.h"\n#include "aux_msc.h"\n/* AUX board adaptation: reset/FIFO waits fail after20ms or200000 polls. */')
replace('static void reset_core(', 'static bool reset_core(')
replace('while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) {\n  }','if (!aux_usb_wait_bits(&dwc2->grstctl, GRSTCTL_AHBIDL, GRSTCTL_AHBIDL)) return false;')
replace('while (dwc2->grstctl & GRSTCTL_CSRST) {}','if (!aux_usb_wait_bits(&dwc2->grstctl, GRSTCTL_CSRST, 0)) return false;')
replace('while (!(dwc2->grstctl & GRSTCTL_CSRST_DONE)) {}','if (!aux_usb_wait_bits(&dwc2->grstctl, GRSTCTL_CSRST_DONE, GRSTCTL_CSRST_DONE)) return false;')
replace('while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) {} // wait for AHB master IDLE','if (!aux_usb_wait_bits(&dwc2->grstctl, GRSTCTL_AHBIDL, GRSTCTL_AHBIDL)) return false;\n  return true;')
replace('static void phy_fs_init(', 'static bool phy_fs_init(')
replace('static void phy_hs_init(', 'static bool phy_hs_init(')
replace('  // Reset core after selecting PHY\n  reset_core(dwc2);','  // Reset core after selecting PHY\n  TU_ASSERT(reset_core(dwc2));',2)
replace('dwc2_phy_update(dwc2, GHWCFG2_HSPHY_NOT_SUPPORTED);','dwc2_phy_update(dwc2, GHWCFG2_HSPHY_NOT_SUPPORTED);\n  return true;')
replace('dwc2_phy_update(dwc2, ghwcfg2.hs_phy_type);','dwc2_phy_update(dwc2, ghwcfg2.hs_phy_type);\n  return true;')
replace('    phy_hs_init(dwc2);','    TU_ASSERT(phy_hs_init(dwc2));')
replace('    phy_fs_init(dwc2);','    TU_ASSERT(phy_fs_init(dwc2));')
replace('  dfifo_flush_tx(dwc2, 0x10); // all tx fifo\n  dfifo_flush_rx(dwc2);','''  dwc2->grstctl = GRSTCTL_TXFFLSH | (0x10u << GRSTCTL_TXFNUM_Pos);
  TU_ASSERT(aux_usb_wait_bits(&dwc2->grstctl, GRSTCTL_TXFFLSH_Msk, 0));
  dwc2->grstctl = GRSTCTL_RXFFLSH;
  TU_ASSERT(aux_usb_wait_bits(&dwc2->grstctl, GRSTCTL_RXFFLSH_Msk, 0));''')
out.write_text(text)
out.with_suffix('.json').write_text(json.dumps({'upstream':str(source),'upstream_sha256':digest,'generated_sha256':hashlib.sha256(text.encode()).hexdigest(),'changes':changes,'limits':'Startup/reset and initial FIFO flush only; endpoint runtime/IRQ waits unchanged.'},indent=2)+'\n')
