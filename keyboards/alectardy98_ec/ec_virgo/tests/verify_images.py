#!/usr/bin/env python3
"""Verify real QMK artifacts and compile EEPROM offsets from each build's flags."""
from pathlib import Path
import hashlib,json,lzma,re,shlex,struct,subprocess,tempfile
BOARD=Path(__file__).resolve().parents[1];ROOT=BOARD.parents[2]

def hex_bytes(path):
 result={};base=0
 for line in path.read_text().splitlines():
  data=bytes.fromhex(line[1:]);assert line.startswith(':') and not sum(data)%256
  n=data[0];offset=int.from_bytes(data[1:3],'big');kind=data[3];payload=data[4:4+n]
  if kind==0:
   for i,b in enumerate(payload):result[base+offset+i]=b
  elif kind==4:base=int.from_bytes(payload,'big')<<16
  elif kind==1:break
 return result

def verify(keymap):
 name='alectardy98_ec_ec_virgo_'+keymap;elf=ROOT/'.build'/(name+'.elf');image=(ROOT/(name+'.bin')).read_bytes()
 raw=elf.read_bytes();header=struct.unpack_from('<16sHHIIIIIHHHHHH',raw);assert header[2]==40
 segments=[]
 for i in range(header[10]):
  typ,offset,vma,lma,size,memsz,flags,align=struct.unpack_from('<8I',raw,header[5]+i*header[9])
  if typ!=1 or not size or not 0x08000000<=lma<0x08100000:continue
  assert lma+size<=0x08004000 or 0x08008000<=lma<lma+size<=0x08040000
  assert image[lma-0x08000000:lma-0x08000000+size]==raw[offset:offset+size]
  segments.append((lma,lma+size))
 assert image[0x4000:0x8000]==b'\xff'*0x4000
 sp,reset=struct.unpack_from('<II',image);assert 0x20000000<sp<=0x20020000 and reset&1 and 0x08008000<=reset&~1<0x08000000+len(image)
 assert b'A98-virgo-EC42-2.9g' in image
 decoded=hex_bytes(ROOT/'.build'/(name+'.hex'))
 assert not any(0x08004000<=a<0x08008000 for a in decoded)
 assert all(image[a-0x08000000]==b for a,b in decoded.items())
 generated=ROOT/'.build'/('obj_'+name)/'src/vial_generated_keyboard_definition.h'
 packed=bytes(int(v,16) for v in re.findall(r'0x([a-fA-F0-9]{2})',generated.read_text()))
 assert packed in image
 definition=json.loads(lzma.decompress(packed));assert definition==json.loads((BOARD/'keymaps'/keymap/'vial.json').read_text())
 nm=subprocess.check_output(['arm-none-eabi-nm','-n',str(elf)],text=True)
 assert 'ec_apply_calibration_defaults' not in nm
 for symbol in ('efl_lld_program','efl_lld_start_erase_sector','a98_hw_program'):
  assert symbol in nm
 fields=['record_bytes','ec_core_bytes','ec_total_bytes','keymap_address','keymap_layers','keymap_bytes','macro_address','macro_bytes','logical_bytes']
 with tempfile.TemporaryDirectory(prefix='virgo-layout-') as tmp:
  t=Path(tmp);source=t/'layout.c';obj=t/'layout.o';data=t/'layout.bin'
  source.write_text('''#include "quantum/nvm/eeprom/nvm_dynamic_keymap.c"
#include "ec_switch_matrix.h"
const uint32_t layout[] __attribute__((used,section(".ec42_layout")))={
sizeof(eeprom_ec_config_t),EECONFIG_BASE_SIZE,EECONFIG_SIZE,DYNAMIC_KEYMAP_EEPROM_ADDR,DYNAMIC_KEYMAP_LAYER_COUNT,
DYNAMIC_KEYMAP_LAYER_COUNT*MATRIX_ROWS*MATRIX_COLS*2,DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR,DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE,TOTAL_EEPROM_BYTE_COUNT};
''')
  flags=shlex.split((ROOT/'.build'/('obj_'+name)/'cflags.txt').read_text())
  subprocess.run(['arm-none-eabi-gcc',*flags,'-fno-lto','-c',str(source),'-o',str(obj)],cwd=ROOT,check=True)
  subprocess.run(['arm-none-eabi-objcopy','--dump-section','.ec42_layout='+str(data),str(obj)],check=True)
  layout=dict(zip(fields,struct.unpack('<9I',data.read_bytes())))
 assert layout['record_bytes']==341 and layout['logical_bytes']==4096
 assert layout['keymap_address']>=layout['ec_total_bytes']
 assert layout['macro_address']>=layout['keymap_address']+layout['keymap_bytes']
 assert layout['macro_bytes']>=100 and layout['macro_address']+layout['macro_bytes']==4096
 return dict(keymap=keymap,bin_bytes=len(image),bin_sha256=hashlib.sha256(image).hexdigest(),flash_load_ranges=[list(map(hex,s)) for s in segments],settings_gap='FF, verified',HEX='sparse; all load bytes match BIN; settings excluded',vial_definition='exact source match',EEPROM=layout)

if __name__=='__main__':print(json.dumps([verify(k) for k in ('vial','vial_alec')],indent=2))
