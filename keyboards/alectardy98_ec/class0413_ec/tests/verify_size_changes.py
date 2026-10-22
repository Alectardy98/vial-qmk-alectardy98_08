#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline equivalence tests. No USB/device operations. SANITIZE=1 adds ASan."""
from pathlib import Path
import os,subprocess,tempfile,re,json
kb=Path(__file__).resolve().parents[1];repo=kb.parents[2]
sanitize='address,undefined' if os.getenv('SANITIZE')=='1' else 'undefined'
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp)
 def run(name,source,extra=[]):
  c=d/(name+'.c');exe=d/name;c.write_text(source)
  subprocess.run(['cc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-fsanitize='+sanitize,'-I'+str(d),'-I'+str(kb/'modules'),str(c),*extra,'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
 d.joinpath('ch.h').write_text('#pragma once\n#include <stdint.h>\n#define CH_CFG_ST_FREQUENCY 100000\n#define CH_CFG_INTERVALS_SIZE 32\n#define CH_CFG_TIME_TYPES_SIZE 32\ntypedef uint64_t time_conv_t;typedef uint32_t sysinterval_t,time_msecs_t,time_usecs_t,time_secs_t;\n')
 run("time_math",'#include <assert.h>\n#include <stdio.h>\n#include <limits.h>\n#include "time_math.h"\nstatic uint64_t state=413;\nstatic uint64_t rand64(void){state^=state<<13;state^=state>>7;state^=state<<17;return state;}\nstatic void test(uint64_t x){\n assert(TIME_MS2I(x)==(uint32_t)((x*100000ULL+999)/1000));\n assert(TIME_US2I(x)==(uint32_t)((x*100000ULL+999999)/1000000));\n assert(TIME_I2S(x)==(uint32_t)((x+99999)/100000));\n assert(TIME_I2MS(x)==(uint32_t)((x*1000ULL+99999)/100000));\n assert(TIME_I2US(x)==(uint32_t)((x*1000000ULL+99999)/100000));\n uint32_t u=x;int32_t i=(int32_t)u;\n assert(TIME_MS2I(i)==(uint32_t)(((uint64_t)i*100000ULL+999)/1000));\n assert(TIME_MS2I(u)==(uint32_t)(((uint64_t)u*100000ULL+999)/1000));\n assert(chTimeI2MS(u)==(uint32_t)(((uint64_t)u*1000ULL+99999)/100000));\n}\nint main(void){\n for(uint64_t i=0;i<1000000;i++){test(i);test(rand64());test((uint32_t)rand64());}\n uint64_t b[]={0,1,9,10,11,99,100,101,999,1000,1001,INT32_MAX,(uint64_t)INT32_MAX+1,UINT32_MAX,(uint64_t)UINT32_MAX+1,UINT64_MAX,UINT64_MAX/100000,UINT64_MAX/1000,UINT64_MAX/1000000};\n for(unsigned j=0;j<sizeof(b)/sizeof(*b);j++)for(int offset=-3;offset<=3;offset++)test(b[j]+offset);\n int j=1;assert(TIME_MS2I(j++)==100&&j==2);j=1;assert(TIME_US2I(j++)==1&&j==2);\n puts("PASS 3,000,133 inputs x 8 exact conversions, signed/unsigned edge cases, full uint64 overflow fallback and single evaluation");\n}\n')
 # Exhaustive equivalence for the new absent-position implementation.
 matrix=(kb/'matrix.c').read_text();start=matrix.index('bool is_unused_position(');end=matrix.index('\n}',start)+2
 fn=matrix[start:end];config=(kb/'config.h').read_text();line=next(x for x in config.splitlines() if x.startswith('#define UNUSED_POSITIONS_LIST'))
 source='#include <stdint.h>\n#include <stdbool.h>\n#include <assert.h>\n#include <stdio.h>\n#define MATRIX_ROWS 5\n#define MATRIX_COLS 21\n'+line+'\nstatic const uint8_t old[][2]=UNUSED_POSITIONS_LIST;\n'+fn
 source+='\nint main(void){for(unsigned r=0;r<256;r++)for(unsigned c=0;c<256;c++){bool expected=false;for(unsigned i=0;i<sizeof(old)/sizeof(*old);i++)if(old[i][0]==r&&old[i][1]==c)expected=true;assert(is_unused_position(r,c)==expected);}puts("PASS all 65536 row/column pairs match original impossible-position list");}\n'
 run('mapping',source)
