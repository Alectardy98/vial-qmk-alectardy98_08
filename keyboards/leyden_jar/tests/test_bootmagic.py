#!/usr/bin/env python3
"""Link the actual QMK implementation against Leyden Jar's startup hook."""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[3]
QUANTUM='''#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef uint16_t matrix_row_t;
#define MATRIX_ROWS 8
#define MATRIX_COLS 16
'''
MAIN='''#include "quantum.h"
static unsigned scans,jumps,erases;
void leyden_jar_init(void){}
void leyden_jar_calibrate(void){}
void leyden_jar_update(void){}
bool leyden_jar_is_enabled(void){return true;}
void leyden_jar_logical_matrix_scan(matrix_row_t *m){for(int r=0;r<8;r++)m[r]=0;}
uint8_t matrix_scan(void){scans++;return 1;}
matrix_row_t matrix_get_row(uint8_t r){(void)r;return 1;}
void wait_ms(uint16_t ms){(void)ms;}
void eeconfig_disable(void){erases++;}
void bootloader_jump(void){jumps++;}
void bootmagic(void);
int main(void){bootmagic();return scans || jumps || erases;}
'''
class BootmagicTests(unittest.TestCase):
 def run_variant(self, legacy):
  source=(ROOT/'keyboards/leyden_jar/matrix.c').read_text()
  if legacy:source=source.replace('void bootmagic_scan(void) {}','void bootmagic_lite(void) {}')
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);(p/'quantum.h').write_text(QUANTUM)
   headers={'matrix.h':'#include "quantum.h"\nuint8_t matrix_scan(void);\nmatrix_row_t matrix_get_row(uint8_t row);\n',
            'keyboard.h':'#include "quantum.h"\n','wait.h':'#include "quantum.h"\nvoid wait_ms(uint16_t);\n',
            'eeconfig.h':'void eeconfig_disable(void);\n','bootloader.h':'void bootloader_jump(void);\n'}
   for name,text in headers.items():(p/name).write_text(text)
   (p/'board.c').write_text(source);(p/'main.c').write_text(MAIN)
   subprocess.run(['cc','-std=gnu11','-O2','-flto','-I'+tmp,'-I'+str(ROOT/'keyboards/leyden_jar'),
                   '-DBOOTMAGIC_ROW=0','-DBOOTMAGIC_COLUMN=0',str(p/'board.c'),str(p/'main.c'),
                   str(ROOT/'quantum/bootmagic/bootmagic.c'),'-o',str(p/'test')],check=True)
   return subprocess.run([str(p/'test')]).returncode
 def test_current_hook_prevents_unintended_startup_boot(self):self.assertEqual(self.run_variant(False),0)
 def test_old_hook_does_not_override_current_qmk(self):self.assertNotEqual(self.run_variant(True),0)
if __name__=='__main__':unittest.main(verbosity=2)
