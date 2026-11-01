# SPDX-License-Identifier: GPL-2.0-or-later
import struct,subprocess,sys
elf,binary=sys.argv[1:]
data=open(binary,'rb').read();sp,reset=struct.unpack_from('<II',data)
assert sp==0x20040000 and reset&1 and 0x08000200 <= reset < 0x08020000
assert len(data)<=128*1024
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm',elf],text=True).splitlines():
 fields=line.split()
 if len(fields)==3:symbols[fields[2]]=int(fields[0],16)
assert symbols['aux_vectors']==0x08000000
assert reset==symbols['Reset_Handler']|1
assert struct.unpack_from('<I',open(elf,'rb').read(),24)[0]==reset
assert symbols['__stack_top']==sp and sp%8==0
assert 0x20000000==symbols['__data_start']<=symbols['__data_end']<=symbols['__bss_start']<=symbols['__bss_end']<=sp-8192
sections=subprocess.check_output(['arm-none-eabi-objdump','-h',elf],text=True).splitlines()
loads=[];code=[]
for index,line in enumerate(sections[:-1]):
 fields=line.split()
 if len(fields)==7 and fields[0].isdigit() and 'LOAD' in sections[index+1]:
  size,address=int(fields[2],16),int(fields[4],16)
  if not size:continue
  assert 0x08000000<=address<address+size<=0x08020000,(fields[1],hex(address),size)
  loads.append((address,address+size))
  if 'CODE' in sections[index+1]:code.append((address,address+size))
assert min(x[0] for x in loads)==0x08000000 and max(x[1] for x in loads)==0x08000000+len(data)
for index,target in enumerate(struct.unpack_from('<128I',data)):
 if index and target:assert target&1 and any(lo<=target-1<hi for lo,hi in code),(index,hex(target))
assert struct.unpack_from('<I',data,(16+38)*4)[0]==symbols['USART2_Handler']|1
assert struct.unpack_from('<I',data,(16+77)*4)[0]==symbols['OTG_HS_Handler']|1
assert 'dcd_init' in symbols and 'tud_msc_read10_cb' in symbols
for forbidden in ('disk_write','f_write','f_mkfs','f_unlink','f_rename','HAL_FLASH_Program','HAL_FLASHEx_Erase'):
 assert forbidden not in symbols,forbidden
assert not subprocess.check_output(['arm-none-eabi-nm','-u',elf],text=True).strip()
attrs=subprocess.check_output(['arm-none-eabi-readelf','-A',elf],text=True)
assert 'Tag_CPU_arch: v7E-M' in attrs and 'Thumb-2' in attrs and 'FPv5/FP-D16' in attrs
policy='controlled media writes' if 'aux_sd_write' in symbols else 'read-only media'
print('AUX ELF/vector/size/undefined-symbol/no firmware-flash mutators ('+policy+'): PASS')
