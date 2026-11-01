"""Local host LLD: retain the clock during WFI; one passive channel-event hook."""
from pathlib import Path
import hashlib,json
import argparse
E=Path(__file__).resolve().parents[1];R=next(p for p in E.parents if (p/"quantum").is_dir())
parser=argparse.ArgumentParser();parser.add_argument("--output",type=Path,required=True);parser.add_argument("--source",type=Path,default=R/'.build/my2k-chibios/chibios-contrib/os/hal/ports/STM32/LLD/USBHv1/hal_usbh_lld.c');args=parser.parse_args()
source=args.source
assert hashlib.sha256(source.read_bytes()).hexdigest()=='899a4521ebea8913673bff3c2626fffaa1579a27d81fb363dc2985982542de52','Unexpected private host LLD'
s=source.read_text();old='\thcint &= hc->HCINTMSK;\n\thc->HCINT = hcint;'
assert s.count(old)==1
new='\thcint &= hc->HCINTMSK;\n\tmy2k_debug_host_irq(host, chn, hcint);\n\thc->HCINT = hcint;'
s=s.replace(old,new)
clock='\t\trccEnableOTG1(FALSE);'
assert s.count(clock)==1
s=s.replace(clock,'\t\trccEnableOTG1(TRUE); /* MY2K: QMK idle uses WFI; host SOF/transactions must continue. */')
s='/* Generated board-local diagnostic copy; original driver unchanged. */\n#include "main_debug.h"\n'+s
dest=args.output;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(s)
dest.with_suffix('.json').write_text(json.dumps(dict(source=str(source),source_SHA256=hashlib.sha256(source.read_bytes()).hexdigest(),generated_SHA256=hashlib.sha256(dest.read_bytes()).hexdigest(),single_readonly_hook=True,host_sleep_clock_retained=True,USB_error_handling_unchanged=True),indent=2)+'\n')
