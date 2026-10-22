#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate ELF flash LMAs, UF2 bytes and Prox TinyUF2 reserved regions offline."""
from pathlib import Path
import json, struct, subprocess, sys
repo=Path(__file__).resolve().parents[3]
tiny=Path(sys.argv[1]) if len(sys.argv)>1 else repo.parent/'tinyuf2'
def sections(elf):
 raw=elf.read_bytes(); headers=subprocess.check_output(['arm-none-eabi-objdump','-h',str(elf)],text=True).splitlines(); result=[]
 for i,line in enumerate(headers[:-1]):
  f=line.split()
  if len(f)!=7 or not f[0].isdigit() or 'CONTENTS' not in headers[i+1] or 'LOAD' not in headers[i+1]:continue
  n,addr,off=int(f[2],16),int(f[4],16),int(f[5],16)
  if n and 0x08000000<=addr<0x08100000:result.append((addr,raw[off:off+n]))
 return result
def symbol_bytes(elf,name):
 raw=elf.read_bytes()
 rows=subprocess.check_output(['arm-none-eabi-nm','-S',str(elf)],text=True).splitlines()
 row=next(x.split() for x in rows if x.split()[-1]==name)
 address,size=int(row[0],16),int(row[1],16)
 headers=subprocess.check_output(['arm-none-eabi-objdump','-h',str(elf)],text=True).splitlines()
 for line in headers:
  f=line.split()
  if len(f)==7 and f[0].isdigit():
   n,vma,off=int(f[2],16),int(f[3],16),int(f[5],16)
   if vma<=address and address+size<=vma+n:return raw[off+address-vma:off+address-vma+size]
 raise AssertionError('Symbol missing from file: '+name)
results={}
for keymap in ['vial','vial_alec']:
 elf=repo/f'.build/alectardy98_ec_ec_prox_uf2_{keymap}.elf';uf2=elf.with_suffix('.uf2').read_bytes(); assert len(uf2)%512==0
 payload={}; blocks=len(uf2)//512
 for i in range(blocks):
  b=uf2[i*512:(i+1)*512];m0,m1,flags,addr,size,no,total,family=struct.unpack_from('<8I',b)
  assert (m0,m1,struct.unpack_from('<I',b,508)[0])==(0x0a324655,0x9e5d5157,0x0ab16f30)
  assert flags==0x2000 and family==0x57755a57 and size==256 and total==blocks and no==i
  assert 0x08010000<=addr<=0x08040000-256 and addr%256==0
  for j,v in enumerate(b[32:288]):assert addr+j not in payload;payload[addr+j]=v
 for name in ['DeviceDescriptor','ConfigurationDescriptor','ManufacturerString','ProductString','SerialNumberString','keyboard_definition']:
  assert symbol_bytes(elf,name)==symbol_bytes(repo/f'.build/alectardy98_ec_ec_prox_{keymap}.elf',name),name
 ss=sections(elf)
 for addr,data in ss:
  assert 0x08010000<=addr and addr+len(data)<=0x08040000
  assert bytes(payload[addr+j] for j in range(len(data)))==data
 vector=bytes(payload[0x08010000+j] for j in range(8));sp,reset=struct.unpack('<II',vector)
 assert 0x20000000<sp<=0x2000fffc and reset&1 and 0x08010000<=reset<0x08040000
 nm=subprocess.check_output(['arm-none-eabi-nm',str(elf)],text=True)
 assert any(l.startswith('2000fffc ') and l.endswith(' _board_dfu_dbl_tap') for l in nm.splitlines())
 dis=subprocess.check_output(['arm-none-eabi-objdump','-d',str(elf)],text=True)
 assert 'f01669ef' in dis and '2000fffc' in dis # LTO inlines bootloader_jump
 assert '2000fffc A __heap_end__' in nm
 size_a=subprocess.check_output(['arm-none-eabi-size','-A',str(elf)],text=True)
 sizes={f[0]:int(f[1]) for l in size_a.splitlines() if len(f:=l.split())==3 and f[1].isdigit()}
 span=max(a+len(d) for a,d in ss)-0x08010000
 results[keymap]={'linked_flash':sum(len(d) for _,d in ss),'application_span':span,'free_application':196608-span,'uf2_payload_bytes':len(payload),'data':sizes['.data'],'bss':sizes['.bss'],'stacks':sizes['.mstack']+sizes['.pstack'],'vector_sp':hex(sp),'reset':hex(reset)}
boot=tiny/'cmake-build/cmake-build-ec_prox/tinyuf2.elf'; ss=sections(boot)
raw=(tiny/'cmake-build/cmake-build-ec_prox/_bin/tinyuf2.bin').read_bytes()
for addr,data in ss:
 assert 0x08000000<=addr and addr+len(data)<=0x08008000
 assert raw[addr-0x08000000:addr-0x08000000+len(data)]==data
sp,reset=struct.unpack_from('<II',raw);assert sp==0x2000fff8 and reset&1 and 0x08000000<=reset<0x08007c00
device=symbol_bytes(boot,'desc_device');config=symbol_bytes(boot,'desc_fs_configuration')
assert device[0]==18 and device[1]==1 and device[7]==64
assert struct.unpack_from('<HH',device,8)==(0x6369,0x6b98)
interfaces=[];offset=0
while offset<len(config):
 length=config[offset];assert length>=2
 if config[offset+1]==4:interfaces.append(tuple(config[offset+5:offset+8]))
 offset+=length
assert interfaces==[(8,6,0x50)],interfaces # MSC/SCSI/Bulk-Only; no competing Raw HID.
results['bootloader']={'bin_bytes':len(raw),'flash_span':max(a+len(d) for a,d in ss)-0x08000000,'free_in_32KiB':32768-len(raw),'vector_sp':hex(sp),'reset':hex(reset)}
print(json.dumps(results,indent=2))
