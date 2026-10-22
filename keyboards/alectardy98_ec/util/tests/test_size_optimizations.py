#!/usr/bin/env python3
"""Compare macro calls for all uint16 keycodes, on press AND release. Offline only."""
from pathlib import Path
import os, subprocess, tempfile
repo=next(p for p in Path(__file__).resolve().parents if (p/'quantum').is_dir())
k=repo/'keyboards/alectardy98_ec/class0413_ec/keymaps/vial_alec/keymap.c'
s=k.read_text();enum=s[s.index('enum custom_keycodes'):s.index('_Static_assert')]
new=s[s.index('// Same encoded send-string commands'):s.index('const uint16_t PROGMEM keymaps')]
old=Path(__file__).with_name('fixtures').joinpath('original_alec_macros.c').read_text()
pre=r'''#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define QK_KB_0 0x7e00
#include "send_string_keycodes.h"
#define CONSOLE_ENABLE
#define SEND_STRING(s) send_string_with_delay(s,0)
typedef struct{struct{bool pressed;struct{unsigned row,col;}key;unsigned time;}event;struct{unsigned interrupted,count;}tap;}keyrecord_t;
static unsigned logs,n;static unsigned char trace[1024];
#define uprintf(...) (++logs)
static void send_string_with_delay(const char*s,unsigned delay){size_t len=strlen(s);assert(n+len+2<sizeof(trace));trace[n++]=delay;trace[n++]=len;memcpy(trace+n,s,len);n+=len;}
'''
main=r'''int main(void){unsigned char expected[1024];for(unsigned k=0;k<65536;++k)for(unsigned press=0;press<2;++press){keyrecord_t r={0};r.event.pressed=press;logs=n=0;bool a=original(k,&r);unsigned en=n,el=logs;memcpy(expected,trace,n);logs=n=0;bool b=optimized(k,&r);assert(a==b&&en==n&&el==logs&&!memcmp(expected,trace,n));}puts("PASS 131072 macro press/release cases: encoded bytes, delay=0, call boundaries, hook return and console forwarding unchanged");}'''
with tempfile.TemporaryDirectory() as d:
 c=Path(d)/'macro.c';exe=Path(d)/'macro';c.write_text(pre+enum+old.replace('process_record_user','original')+new.replace('process_record_user','optimized')+main)
 subprocess.run(['cc','-std=c11','-I'+str(repo/'quantum/send_string'),'-fsanitize=address,undefined' if os.getenv('SANITIZE')=='1' else '-fsanitize=undefined',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)

with tempfile.TemporaryDirectory() as d:
 state=(repo/'keyboards/alectardy98_ec/class0413_ec/modules/storage_state.c').read_text()
 start=state.index('bool ec_storage_ready(void)');end=state.index('}',start)+1
 predicate=state[start:end]
 c=Path(d)/'health.c';exe=Path(d)/'health'
 c.write_text('#include <stdbool.h>\n#include <assert.h>\nbool s2_armed; struct {unsigned backend_ok,error;} efl_diag_record;\n'+predicate+'\nint main(void){for(unsigned a=0;a<2;++a)for(unsigned b=0;b<2;++b)for(unsigned e=0;e<12;++e){s2_armed=a;efl_diag_record.backend_ok=b;efl_diag_record.error=e;assert(ec_storage_ready()==(a&&b&&!e));}}')
 subprocess.run(['cc','-std=c11','-fsanitize=address,undefined' if os.getenv('SANITIZE')=='1' else '-fsanitize=undefined',str(c),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
 print('PASS actual storage-health predicate across armed/backend/error combinations')
