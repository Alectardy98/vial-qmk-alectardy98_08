"""F107 per-key debounce at its measured 9 ms scan cadence.

These synthetic inputs exercise noise isolation and temporal filtering; they
do not assert that an unobserved physical switch fault has been repaired.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

root = Path(__file__).resolve().parents[3]
stub='''#pragma once
#include <stdint.h>
#include <stdbool.h>
#define MATRIX_COLS 15
typedef uint16_t matrix_row_t;
void debounce_init(uint8_t);
void debounce_free(void);
bool debounce(matrix_row_t[],matrix_row_t[],uint8_t,bool);
'''
timer='''#pragma once
#include <stdint.h>
typedef uint16_t fast_timer_t;
fast_timer_t timer_read_fast(void);
#define timer_elapsed_fast(t) ((uint16_t)(timer_read_fast()-(t)))
#define TIMER_DIFF_FAST(a,b) ((uint16_t)((a)-(b)))
'''
source=r'''
#include "debounce.h"
#include "timer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t now;
fast_timer_t timer_read_fast(void){return now;}
#include "SOURCE"
int main(int argc,char **argv){
 assert(argc==2);int which=atoi(argv[1]);matrix_row_t raw[8]={0},cooked[8]={0},last[8]={0};unsigned downs[3]={0},ups[3]={0},first=0;
 debounce_init(8);
 for(unsigned t=0;t<=270;t+=9){
  now=t;memset(raw,0,sizeof(raw));
  if(which==0){ // L flickers each scan; a different key is intentionally pressed.
   if(t<180 && (t/9)%2)raw[5]|=1u<<7;
   if(t>=18 && t<81)raw[5]|=1u<<6;
  }else if(which==1){ // One sustained D hold containing two brief dropouts.
   if(t>=18 && t<189 && t!=63 && t!=108 && t!=117)raw[5]|=1u<<4;
  }else if(which==2){ // Spurious 9 ms and 18 ms L assertions, then a real press.
   if(t==18 || t==63 || t==72 || (t>=126 && t<180))raw[5]|=1u<<7;
  }else if(which==3){ // Two intentional quick taps, separated by a real release.
   if((t>=18 && t<63)||(t>=99 && t<144))raw[5]|=1u<<7;
  }else if(which==4){ // Two real simultaneous keys with a noisy third key.
   if(t<180 && (t/9)%2)raw[5]|=1u<<7;
   if(t>=18 && t<81)raw[5]|=(1u<<4)|(1u<<6);
  }
  bool changed=memcmp(raw,last,sizeof(raw))!=0;memcpy(last,raw,sizeof(raw));matrix_row_t before=cooked[5];
  bool result=debounce(raw,cooked,8,changed);assert(result==(before!=cooked[5]));
  unsigned cols[]={7,6,4};for(unsigned i=0;i<3;i++){unsigned mask=1u<<cols[i];if((before^cooked[5])&mask){if(cooked[5]&mask){downs[i]++;if(!first)first=t;}else ups[i]++;}}
 }
 printf("case %d L %u/%u K %u/%u D %u/%u first %u\n",which,downs[0],ups[0],downs[1],ups[1],downs[2],ups[2],first);
 if(EXPECT_PERKEY){
  if(which==0)assert(downs[1]==1 && ups[1]==1 && first<=36);
  if(which==1)assert(downs[2]==1 && ups[2]==1);
  if(which==2)assert(downs[0]==1 && ups[0]==1);
  if(which==3)assert(downs[0]==2 && ups[0]==2);
  if(which==4)assert(downs[1]==1 && ups[1]==1 && downs[2]==1 && ups[2]==1);
 }else{
  // Reproduce the old global filter's lost keys and split hold.
  if(which==0 || which==4)assert(downs[1]==0 && ups[1]==0);
  if(which==1)assert(downs[2]==2 && ups[2]==2);
  if(which==2)assert(downs[0]==2 && ups[0]==2);
 }
 for(unsigned r=0;r<8;r++)assert(cooked[r]==0);
 debounce_free();return 0;
}
'''
class F107DebounceTests(unittest.TestCase):
    def test_per_key_noise_isolation_and_short_dropouts(self):
        config = (root / 'keyboards/leyden_jar/ibm/f107/config.h').read_text()
        rules = (root / 'keyboards/leyden_jar/ibm/f107/rules.mk').read_text()
        self.assertIn('#define DEBOUNCE 12', config)
        self.assertIn('DEBOUNCE_TYPE = sym_defer_pk', rules)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path / 'debounce.h').write_text(stub)
            (path / 'timer.h').write_text(timer)
            for mode, milliseconds in [('sym_defer_g', 5), ('sym_defer_pk', 12)]:
                print(mode, milliseconds, flush=True)
                implementation = root / 'quantum/debounce' / f'{mode}.c'
                test_source = source.replace('SOURCE', str(implementation))
                test_source = test_source.replace('EXPECT_PERKEY', '1' if mode == 'sym_defer_pk' else '0')
                (path / 'test.c').write_text(test_source)
                subprocess.run([
                    'cc', '-std=c11', '-Wall', '-Wextra', '-Wno-unused-parameter',
                    '-fsanitize=address,undefined', f'-DDEBOUNCE={milliseconds}',
                    '-I' + tmp, str(path / 'test.c'), '-o', str(path / 'test'),
                ], check=True)
                for scenario in range(5):
                    subprocess.run([str(path / 'test'), str(scenario)], check=True)

if __name__ == '__main__':
    unittest.main(verbosity=2)
