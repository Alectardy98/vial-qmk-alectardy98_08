#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile actual QMK configs to compare logical EEPROM ABI and clock selection."""
from pathlib import Path
import shlex, subprocess, tempfile, struct
repo=Path(__file__).resolve().parents[3]
results={}
rgb={}
for board in ['ec_prox','ec_prox_uf2']:
 for keymap in ['vial','vial_alec']:
  flags=shlex.split((repo/f'.build/obj_alectardy98_ec_{board}_{keymap}/cflags.txt').read_text())
  with tempfile.TemporaryDirectory() as temp:
   p=Path(temp)
   code='#include "'+str(repo/'quantum/nvm/eeprom/nvm_dynamic_keymap.c')+'"\n'
   code+='const uint32_t layout[] __attribute__((section(".layout"),used))={EECONFIG_BASE_SIZE,EECONFIG_KB_DATA_SIZE,VIA_EEPROM_MAGIC_ADDR,DYNAMIC_KEYMAP_EEPROM_ADDR,DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR,DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE,WEAR_LEVELING_LOGICAL_SIZE,WEAR_LEVELING_BACKING_SIZE,WEAR_LEVELING_EFL_FIRST_SECTOR,A98_STORAGE_START,A98_STORAGE_END};\n'
   (p/'layout.c').write_text(code)
   subprocess.run(['arm-none-eabi-gcc',*flags,'-fno-lto','-c',str(p/'layout.c'),'-o',str(p/'layout.o')],cwd=repo,check=True)
   subprocess.run(['arm-none-eabi-objcopy','-O','binary','--only-section=.layout',str(p/'layout.o'),str(p/'layout.bin')],check=True)
   results[board,keymap]=struct.unpack('<11I',(p/'layout.bin').read_bytes())
   rgbcode='#include "quantum.h"\n#include "rgblight.h"\n#include "ws2812.h"\n'
   rgbcode+='#ifdef RGBLIGHT_LAYERS\n#define TEST_LAYERS 1\n#else\n#define TEST_LAYERS 0\n#endif\n'
   rgbcode+='const uint32_t values[] __attribute__((section(".rgb_cfg"),used))={RGBLIGHT_LED_COUNT,WS2812_DI_PIN,offsetof(ws2812_led_t,r),offsetof(ws2812_led_t,g),offsetof(ws2812_led_t,b),RGBLIGHT_LIMIT_VAL,RGBLIGHT_MODES,TEST_LAYERS};\n'
   (p/'rgb.c').write_text(rgbcode)
   subprocess.run(['arm-none-eabi-gcc',*flags,'-fno-lto','-c',str(p/'rgb.c'),'-o',str(p/'rgb.o')],cwd=repo,check=True)
   subprocess.run(['arm-none-eabi-objcopy','-O','binary','--only-section=.rgb_cfg',str(p/'rgb.o'),str(p/'rgb.bin')],check=True)
   rgb[board,keymap]=struct.unpack('<8I',(p/'rgb.bin').read_bytes())

   if board=='ec_prox_uf2':
    (p/'clock.c').write_text('''#include "hal.h"
#ifndef STM32F411xE
#error Wrong F411 device selection
#endif
#ifdef STM32F401xC
#error F401 device macro leaked from original GPIO header
#endif
_Static_assert(STM32_HSECLK==8000000U,"HSE changed");
_Static_assert(STM32_SYSCLK==84000000U,"CPU timing changed");
_Static_assert(STM32_PLL48CLK==48000000U,"USB clock changed");
''')
    subprocess.run(['arm-none-eabi-gcc',*flags,'-fno-lto','-c',str(p/'clock.c'),'-o',str(p/'clock.o')],cwd=repo,check=True)
for k in ['vial','vial_alec']:
 assert results['ec_prox',k][:8]==results['ec_prox_uf2',k][:8]==(37,242,279,283,2075,2021,4096,8192)
 assert results['ec_prox_uf2',k][8:]==(2,0x8000,0xc000)
print(results)
print('PASS actual compiled EEPROM/Vial ABI unchanged; only physical sector moves. F411 selection and 8/84/48 MHz clocks verified.')

for keymap in ['vial','vial_alec']:
 assert rgb['ec_prox',keymap]==rgb['ec_prox_uf2',keymap]
 assert rgb['ec_prox_uf2',keymap][0]==22
 for name in ['keymap.c','rules.mk','config.h','vial.json']:
  assert (repo/f'keyboards/alectardy98_ec/ec_prox/keymaps/{keymap}/{name}').read_bytes()==(repo/f'keyboards/alectardy98_ec/ec_prox_uf2/keymaps/{keymap}/{name}').read_bytes()
print('PASS compiled RGB LED count/pin/order/brightness/effects/overlays and exact keymap hooks:',rgb)
