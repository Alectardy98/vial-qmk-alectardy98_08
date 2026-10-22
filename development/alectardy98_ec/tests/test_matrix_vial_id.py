#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Matrix schema IDs, real EEPROM offsets and actual VIA reboot/reset paths.

Offline only. Target objects are compiled but never executed; VIA functions run
against a host RAM EEPROM. No device is opened or physical EEPROM written.
"""
from pathlib import Path
import os,re,shlex,struct,subprocess,tempfile
repo=Path(__file__).resolve().parents[3]
build=Path(os.environ.get('EC42_BUILD_TREE',repo))
board=repo/'keyboards/alectardy98_ec/me_ec_uf2'
post=(board/'post_rules.mk').read_text()
ids={'vial':0x9829ED,'vial_alec':0x01198A}
assert len(set(ids.values()))==2
via=(repo/'quantum/via.c').read_text()
def function(signature):
 start=via.index(signature+' {');i=via.index('{',start)+1;depth=1
 while depth:depth+=(via[i]=='{')-(via[i]=='}');i+=1
 return via[start:i]
functions='\n'.join(function(s) for s in ['bool via_eeprom_is_valid(void)','void via_eeprom_set_valid(bool valid)','void eeconfig_init_via(void)','void via_init(void)'])
for key,expected in ids.items():
 match=re.search(r'(?:else )?ifeq \(\$\(KEYMAP\),'+key+r'\)\s*\nMATRIX_VIAL_STORAGE_ID := (0x[0-9A-Fa-f]+)',post)
 assert match and int(match[1],16)==expected
 assert '-DA98_MATRIX_VIAL_BUILD_ID=$(MATRIX_VIAL_STORAGE_ID)' in post
 assert '-include keyboards/alectardy98_ec/me_ec_uf2/storage_version.h' in post
 flags=shlex.split((build/f'.build/obj_alectardy98_ec_me_ec_uf2_{key}/cflags.txt').read_text())
 with tempfile.TemporaryDirectory(prefix='matrix-vial-id-') as temp:
  p=Path(temp)
  # A compatibility ID cannot silently cover a changed EEPROM ABI.
  expressions=['EECONFIG_BASE_SIZE','EECONFIG_KB_DATA_SIZE','VIA_EEPROM_MAGIC_ADDR','DYNAMIC_KEYMAP_EEPROM_ADDR','DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR','DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE']
  (p/'layout.c').write_text('#include "'+str(repo/'quantum/nvm/eeprom/nvm_dynamic_keymap.c')+'"\nconst uint32_t layout[] __attribute__((section(".layout"),used))={'+','.join('(uintptr_t)('+s+')' for s in expressions)+'};\n')
  subprocess.run(['arm-none-eabi-gcc',*flags,'-fno-lto','-c',str(p/'layout.c'),'-o',str(p/'layout.o')],cwd=build,check=True,capture_output=True)
  subprocess.run(['arm-none-eabi-objcopy','-O','binary','--only-section=.layout',str(p/'layout.o'),str(p/'layout.bin')],check=True)
  layout=struct.unpack('<6I',(p/'layout.bin').read_bytes())
  assert layout==((37,305,342,346,2306,1790) if key=='vial' else (37,305,342,349,1469,2627)),(key,layout)
  core,ec,magic,keymap,macro,macro_bytes=layout
  header_code='#include <stdint.h>\n#include "'+str(board/'storage_version.h')+'"\n_Static_assert(BUILD_ID=='+hex(expected)+',"wrong stable identity");\n'
  code=header_code+'''#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#define VIAL_ENABLE
#define VIA_EEPROM_LAYOUT_OPTIONS_DEFAULT 0
#define VIA_EEPROM_MAGIC_ADDR '''+str(magic)+'''
static uint8_t mem[4096];static unsigned writes,resets,macro_resets;
static uint8_t eeprom_read_byte(const void*p){return mem[(uintptr_t)p];}
static void eeprom_update_byte(void*p,uint8_t v){mem[(uintptr_t)p]=v;writes++;}
static void via_init_kb(void){}
static void via_set_layout_options_kb(uint32_t v){(void)v;}
static uint32_t via_get_layout_options(void){return 0;}
static void via_set_layout_options(uint32_t v){(void)v;}
static void dynamic_keymap_reset(void){resets++;memset(mem+'''+str(keymap)+''',0,768);}
static void dynamic_keymap_macro_reset(void){macro_resets++;memset(mem+'''+str(macro)+''',0,'''+str(macro_bytes)+''');}
'''+functions+'''
int main(void){
 memset(mem,0x56,sizeof mem);uint8_t protected[342];memcpy(protected,mem,sizeof protected);
 eeconfig_init_via();assert(via_eeprom_is_valid());
 mem['''+str(keymap)+''']=0xA7;mem['''+str(macro)+''']=0xB8;
 unsigned before=writes;for(unsigned i=0;i<10;i++)via_init();
 assert(writes==before&&resets==1&&macro_resets==1);
 assert(mem['''+str(keymap)+''']==0xA7&&mem['''+str(macro)+''']==0xB8);
 mem[VIA_EEPROM_MAGIC_ADDR]^=1;assert(!via_eeprom_is_valid());via_init();
 assert(resets==2&&macro_resets==2&&writes>before&&via_eeprom_is_valid());
 assert(!memcmp(protected,mem,sizeof protected));return 0;
}
'''
  (p/'test.c').write_text(code)
  san=['-fsanitize=address,undefined','-fno-pie','-no-pie'] if os.environ.get('SANITIZE')=='1' else ['-fsanitize=undefined']
  for generated in [0x123456,0xABCDEF]:
   (p/'version.h').write_text('#pragma once\n#define BUILD_ID '+hex(generated)+'\n#define QMK_BUILDDATE "random build date"\n')
   subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',*san,'-I'+str(p),'-DA98_MATRIX_VIAL_BUILD_ID='+hex(expected),str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
  print('PASS',key,hex(expected),'distinct stable ID despite random generated header; exact schema; repeated boot preserves keymap/macros without writes; incompatible ID resets only VIA, EC/core canaries unchanged')
