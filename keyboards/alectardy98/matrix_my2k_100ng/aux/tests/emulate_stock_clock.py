#!/usr/bin/env python3
"""Actual AUX calendar/control/mono font rendering, timer seam only; no device APIs."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,tempfile
import unicorn as uc
from unicorn import arm_const as a
p=argparse.ArgumentParser();p.add_argument('elf',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args();elf=args.elf.resolve();AUX=Path(__file__).resolve().parents[1]
sym={v[2]:int(v[0],16) for line in subprocess.check_output(['arm-none-eabi-nm',str(elf)],text=True).splitlines() if len(v:=line.split())==3};image=elf.with_suffix('.bin').read_bytes()
with tempfile.TemporaryDirectory() as directory:
 d=Path(directory);(d/'layout.c').write_text('''#include <stddef.h>
#include "aux_control.h"
const unsigned layout[] __attribute__((section(".layout")))={sizeof(my2k_wire_message),offsetof(my2k_wire_message,year),offsetof(my2k_wire_message,v),sizeof(aux_datetime),offsetof(aux_datetime,valid),sizeof(aux_panels),offsetof(aux_panels,mono_ready),offsetof(aux_panels,mono),offsetof(aux_panels,mono_overlay),offsetof(aux_panels,mono_overlay_enabled),offsetof(aux_panels,mono_dirty),offsetof(aux_panels,mono_page),offsetof(aux_panels,mono_enabled),sizeof(aux_control),offsetof(aux_control,clock),offsetof(aux_control,mono_mode),offsetof(aux_control,reply_pending)};
''')
 subprocess.run(['arm-none-eabi-gcc','-mcpu=cortex-m7','-mthumb','-DMY2K_STOCK_PARITY_PROFILE=1','-I'+str(AUX/'include'),'-I'+str(AUX/'../common'),'-c',str(d/'layout.c'),'-o',str(d/'layout.o')],check=True)
 subprocess.run(['arm-none-eabi-objcopy','-O','binary','--only-section=.layout',str(d/'layout.o'),str(d/'layout.bin')],check=True);layout=struct.unpack('<17I',(d/'layout.bin').read_bytes())
wire_size,wire_year,wire_v,clock_size,clock_valid,panel_size,panel_ready,panel_base,panel_overlay,panel_overlay_enable,panel_dirty,panel_page,panel_enabled,control_size,control_clock,control_mode,control_pending=layout
m=uc.Uc(uc.UC_ARCH_ARM,uc.UC_MODE_THUMB|uc.UC_MODE_MCLASS);m.mem_map(0x08000000,0x100000);m.mem_map(0x20000000,0x40000);m.mem_write(0x08000000,image)
m.mem_write(sym['__data_start'],image[sym['__data_load']-0x08000000:sym['__data_load']-0x08000000+sym['__data_end']-sym['__data_start']])
def timer(machine,address,size,ctx):machine.reg_write(a.UC_ARM_REG_R0,0);machine.reg_write(a.UC_ARM_REG_PC,machine.reg_read(a.UC_ARM_REG_LR))
m.hook_add(uc.UC_HOOK_CODE,timer,begin=sym['aux_millis'],end=sym['aux_millis'])
def run(name,*values):
 for i,v in enumerate(values):m.reg_write(getattr(a,'UC_ARM_REG_R'+str(i)),v)
 m.reg_write(a.UC_ARM_REG_SP,0x2003f000);m.reg_write(a.UC_ARM_REG_LR,0x080ff001);m.emu_start(sym[name]|1,0x080ff000,count=2000000);assert m.reg_read(a.UC_ARM_REG_PC)==0x080ff000;return m.reg_read(a.UC_ARM_REG_R0)
clock,wire,panel,control,reply=0x20010000,0x20011000,0x20012000,0x20013000,0x20014000
msg=bytearray(wire_size);struct.pack_into('<I',msg,0,12);struct.pack_into('<H',msg,wire_year,2026);msg[wire_v:wire_v+6]=bytes([12,31,4,23,59,59]);m.mem_write(wire,bytes(msg))
assert run('aux_datetime_set',clock,wire,0xfffffff0)==1;assert run('aux_datetime_step',clock,(0xfffffff0+4000)&0xffffffff)==1
assert bytes(m.mem_read(clock,8))==struct.pack('<H6B',2027,1,1,5,0,0,3)
m.mem_write(panel+panel_ready,b'\x01');m.mem_write(panel+panel_enabled,b'\x01');m.mem_write(panel+panel_base,b'\x5a'*360);run('aux_text_clock_render',clock,panel)
assert bytes(m.mem_read(panel+panel_base,360))==b'\x5a'*360;frame=bytes(m.mem_read(panel+panel_overlay,360));assert sum(bool(x) for x in frame)>20
assert bytes(m.mem_read(panel+panel_overlay_enable,1))==b'\x01' and bytes(m.mem_read(panel+panel_dirty,1))==b'\x01'
m.mem_write(control,struct.pack('<I',panel));assert run('aux_control_apply',control,wire,reply)==1
struct.pack_into('<I',msg,0,5);msg[wire_v:wire_v+2]=bytes([5,1]);m.mem_write(wire,bytes(msg));assert run('aux_control_apply',control,wire,reply)==1
run('aux_control_service',control,4000);assert bytes(m.mem_read(control+control_mode,1))==b'\x01'
assert bytes(m.mem_read(control+control_clock,8))==struct.pack('<H6B',2027,1,1,5,0,0,3)
assert run('aux_control_result',control)==0;m.mem_write(panel+panel_dirty,b'\0');assert run('aux_control_result',control)==1
msg[wire_v+1]=2;m.mem_write(wire,bytes(msg));assert run('aux_control_apply',control,wire,reply)==0
out={'pass':True,'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'engine':'Unicorn '+uc.__version__,'derived_ARM_layout':list(layout),'entries':{n:hex(sym[n]) for n in ['aux_datetime_set','aux_datetime_step','aux_text_clock_render','aux_control_apply','aux_control_service']},'calendar_year_day_weekday_timer_wrap':True,'real_font_mono_frame_and_asset_preservation':True,'clock_mode_ack_flush_barrier':True,'thermometer_mode_rejected':True,'hardware_accessed':False,'limits':'Actual calendar,validated wire decode,modecontrol andfont renderer instructions. Clock millis getter returns0;SPIflush completion is modeled by clearingdirty;actual peripheral transport covered elsewhere. NoRTC/panel electrical proof.'}
args.output.write_text(json.dumps(out,indent=2)+'\n');print('PASS: compiled AUX calendar,timer/year rollover,real mono glyphs,TM/AM1 routing,OLED ACK barrier andAM2rejection')
