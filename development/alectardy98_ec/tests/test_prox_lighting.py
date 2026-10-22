#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise Prox's actual layer hooks and QMK's actual overlay renderer offline."""
from pathlib import Path
import os, subprocess, tempfile
repo = Path(__file__).resolve().parents[3]
variant = os.environ.get('EC_PROX_VARIANT','ec_prox')
assert variant in ('ec_prox','ec_prox_uf2')
keymap = (repo/f'keyboards/alectardy98_ec/{variant}/keymaps/vial_alec/keymap.c').read_text()
source = (repo/'quantum/rgblight/rgblight.c').read_text()
header = (repo/'quantum/rgblight/rgblight.h').read_text()
def function(text, name):
    start = text.index('static void '+name+'(void) {')
    brace = text.index('{', start); end = brace + 1; depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}'); end += 1
    return text[start:end]
types = header[header.index('typedef struct {', header.index('#ifdef RGBLIGHT_LAYERS')):header.index('// Get/set enabled rgblight layers')]
colors = '\n'.join(line for line in (repo/'quantum/color.h').read_text().splitlines() if line.startswith('#define HSV_'))
code = '''#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#define PROGMEM
#define RGBLIGHT_LED_COUNT 22
#define MIN(a,b) ((a)<(b)?(a):(b))
#define pgm_read_ptr(p) (*(p))
#define memcpy_P memcpy
'''+colors+'\n'+types+'''
typedef uint32_t layer_state_t;
enum {_BASE,_HHKB,_FN,_FN2};
static layer_state_t layer_state, default_layer_state;
static unsigned mask;
static uint8_t leds[22][3];
static const rgblight_segment_t *const *rgblight_layers;
static uint8_t get_highest_layer(uint32_t state){uint8_t n=0;while(state>>=1)++n;return n;}
static void rgblight_set_layer_state(uint8_t i,bool enabled){if(enabled)mask|=1u<<i;else mask&=~(1u<<i);}
static bool rgblight_get_layer_state(uint8_t i){return !!(mask&(1u<<i));}
static void sethsv(uint8_t h,uint8_t s,uint8_t v,int i){assert(i>=0&&i<22);leds[i][0]=h;leds[i][1]=s;leds[i][2]=v;}
'''+keymap[keymap.index('// RGBLIGHT owns'):keymap.index('// Defines the keycodes')]+function(source,'rgblight_layers_write')+'''
static void verify(unsigned state,unsigned defaults,unsigned expected){
 layer_state=state;default_layer_state=defaults;
 assert(layer_state_set_user(state)==state);
 assert(default_layer_state_set_user(defaults)==defaults);
 memset(leds,19,sizeof leds);rgblight_layers_write();
 assert(mask==(1u<<expected));
 const rgblight_segment_t *s=indicator_layers[expected];
 assert(leds[0][0]==s->hue&&leds[0][1]==s->sat&&leds[0][2]==s->val);
 for(unsigned i=1;i<22;i++)for(unsigned j=0;j<3;j++)assert(leds[i][j]==19);
}
int main(void){
 keyboard_post_init_user();assert(rgblight_layers==indicator_layers);
 for(unsigned i=0;i<4;i++){verify(1u<<i,1,i);verify(0,1u<<i,i);}
 verify(15,1,3);verify(7,1,2);verify(0,0,0);verify(1u<<7,1,0);
 for(unsigned i=0;i<100;i++){verify(8,1,3);verify(1,1,0);}
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/'test.c').write_text(code)
    flags=['-fsanitize=address,undefined','-fno-pie','-no-pie'] if os.environ.get('SANITIZE')=='1' else ['-fsanitize=undefined']
    subprocess.run(['cc','-std=c11','-Wall','-Werror',*flags,str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS actual Prox layer/default hooks and native QMK overlay renderer: OFF/red/green/blue, priority, repeated changes, LEDs1..21 untouched')
