#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Preserve board-specific acquisition exactly; never execute GPIO on a device."""
from pathlib import Path
repo=Path(__file__).resolve().parents[3];fw=repo/'keyboards/alectardy98_ec'
def function(text,name):
 end=text.index('(',text.index(name+'('));start=text.rfind('\n',0,text.index(name+'('))+1;br=text.index('{',end);depth=1;i=br+1
 while depth:depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]
old=(repo/'keyboards/cipulot/common/ec_switch_matrix.c').read_text();new=(fw/'ec_prox/acquisition.c').read_text()
for name in ['ec_init','init_row','init_amux','disable_unused_row','select_amux_channel','disable_unused_amux','discharge_capacitor','charge_capacitor','ec_readkey_raw']:
 assert function(old,name)==function(new,name),name
old=(repo/'keyboards/matrix/me_ec_uf2/matrix.c').read_text();new=(fw/'me_ec_uf2/matrix.c').read_text()
for name in ['sense_key','matrix_adc_read']:
 assert function(old,name)==function(new,name),name
# Physical pin maps/channel order stay identical to the working sources.
for token in ['static uint32_t custom_row_pins[]','static uint8_t custom_col_pins[]']:
 a=old[old.index(token):old.index(';',old.index(token))+1];b=new[new.index(token):new.index(';',new.index(token))+1];assert a==b
assert 'wait_us(2)' in function(new,'sense_key') and 'wait_us(20)' in function(new,'sense_key')
print('PASS exact original EC Prox mux/row/charge/ADC/discharge implementation; exact Matrix acquisition timing/pins/channel order')
# Execute the actual startup averaging routines with deterministic ADC models.
import tempfile,subprocess,os,re
for board,rows,cols in [('ec_prox',5,15),('me_ec_uf2',6,16)]:
 text=(fw/board/('acquisition.c' if board=='ec_prox' else 'matrix.c')).read_text()
 prefix='#include <stdint.h>\n#include <stdbool.h>\n#include <assert.h>\n#include <string.h>\n'+f'#define MATRIX_ROWS {rows}\n#define MATRIX_COLS {cols}\n#define DEFAULT_NOISE_FLOOR_SAMPLING_COUNT 30\n'
 prefix+='typedef uint32_t matrix_row_t;static struct {uint16_t noise_floor[MATRIX_ROWS][MATRIX_COLS];} ec_config;static unsigned calls,waited;static void wait_ms(unsigned n){waited+=n;}\n'
 if board=='me_ec_uf2':
  begin=text.rindex('static void baseline(void){');body=text[begin:]
  prefix+='static uint32_t readings[MATRIX_ROWS][MATRIX_COLS];static bool matrix_scan_custom(matrix_row_t *r){(void)r;for(unsigned i=0;i<MATRIX_ROWS;i++)for(unsigned j=0;j<MATRIX_COLS;j++)readings[i][j]=100+i*MATRIX_COLS+j+(calls%3)*3;calls++;return false;}\n'
  expected='103+i*MATRIX_COLS+j';call='baseline()';count=60
 else:
  body=function(text,'ec_noise_floor');prefix+='''#define AMUX_COUNT 2
#define UNUSED_POSITIONS_LIST 1
static const unsigned amux_n_col_sizes[]={8,7};
static bool is_unused_position(unsigned r,unsigned c){const unsigned absent[]={29,44,57,60,63,64,65,67,68,71,72,73,74};for(unsigned n=0;n<13;n++)if(r*15+c==absent[n])return true;return false;}
static void disable_unused_amux(unsigned a){(void)a;}static void disable_unused_row(unsigned r){(void)r;}
static uint16_t ec_readkey_raw(unsigned a,unsigned r,unsigned c){unsigned sample=calls++/62;return 100+r*15+c+(a?8:0)+(sample%3)*3;}
''';expected='is_unused_position(i,j)?0:103+i*MATRIX_COLS+j';call='ec_noise_floor()';count=3720
 code=prefix+body+f'\nint main(void){{memset(&ec_config,0xff,sizeof ec_config);for(unsigned n=0;n<2;n++){{{call};for(unsigned i=0;i<MATRIX_ROWS;i++)for(unsigned j=0;j<MATRIX_COLS;j++)assert(ec_config.noise_floor[i][j]==({expected}));}}assert(calls=={count}&&waited==300);return 0;}}'
 with tempfile.TemporaryDirectory() as temp:
  p=Path(temp);(p/'test.c').write_text(code);flags=['-fsanitize=address,undefined','-fno-pie','-no-pie'] if os.environ.get('SANITIZE')=='1' else ['-fsanitize=undefined']
  subprocess.run(['cc','-std=c11',*flags,str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
 print('PASS',board,'actual 30-scan startup baseline averaging, eligibility and repeat initialization')
