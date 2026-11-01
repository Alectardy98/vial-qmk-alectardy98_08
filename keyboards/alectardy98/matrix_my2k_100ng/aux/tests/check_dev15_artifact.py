#!/usr/bin/env python3
"""Execute final AUX queue/backend/inquiry instructions with synthetic SD reads.

No board/peripherals are emulated. This independently verifies final optimized
profile bounds, read-only identity, private buffers and eight-sector ordering.
"""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess
import unicorn as uc
from unicorn import arm_const as a
p=argparse.ArgumentParser();p.add_argument('elf',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
elf=args.elf.resolve();blob=elf.with_suffix('.bin').read_bytes()
syms={};sizes={}
for line in subprocess.check_output(['arm-none-eabi-nm','-S',str(elf)],text=True).splitlines():
    f=line.split()
    if len(f)==4:syms[f[3]]=int(f[0],16);sizes[f[3]]=int(f[1],16)
    elif len(f)==3:syms[f[2]]=int(f[0],16)
for name in ('completed','read_staging','_mscd_epbuf'):assert sizes[name]==4096,(name,sizes.get(name))
for name in ('aux_sd_write','disk_write','f_write','f_mkfs','HAL_FLASH_Program','HAL_FLASHEx_Erase'):assert name not in syms,name
m=uc.Uc(uc.UC_ARCH_ARM,uc.UC_MODE_THUMB|uc.UC_MODE_MCLASS)
m.mem_map(0x08000000,0x20000);m.mem_write(0x08000000,blob);m.mem_map(0x20000000,0x40000)
data_start,data_end=syms['__data_start'],syms['__data_end'];load=syms['__data_load']-0x08000000
m.mem_write(data_start,blob[load:load+data_end-data_start])
def word(addr,value=None):
    if value is not None:m.mem_write(addr,struct.pack('<I',value&0xffffffff))
    return struct.unpack('<I',m.mem_read(addr,4))[0]
STOP=0x0801fff0;calls=[]
def code(machine,addr,size,unused):
    if addr==syms['aux_sd_read']:
        lba=machine.reg_read(a.UC_ARM_REG_R0);dst=machine.reg_read(a.UC_ARM_REG_R1);calls.append(lba)
        assert lba<32;machine.mem_write(dst,bytes((lba*37+i*3)&255 for i in range(512)))
        machine.reg_write(a.UC_ARM_REG_R0,1);machine.reg_write(a.UC_ARM_REG_PC,machine.reg_read(a.UC_ARM_REG_LR))
m.hook_add(uc.UC_HOOK_CODE,code)
def call(name,*values):
    stack=0x2003f000;m.reg_write(a.UC_ARM_REG_SP,stack);m.reg_write(a.UC_ARM_REG_LR,STOP|1)
    for reg,value in zip((a.UC_ARM_REG_R0,a.UC_ARM_REG_R1,a.UC_ARM_REG_R2,a.UC_ARM_REG_R3),values):m.reg_write(reg,value)
    for i,value in enumerate(values[4:]):word(stack+i*4,value)
    m.emu_start(syms[name]|1,STOP,count=200000)
    assert m.reg_read(a.UC_ARM_REG_PC)==STOP,name
    return m.reg_read(a.UC_ARM_REG_R0)
word(syms['sector_count'],32)
output=0x20030000;result=0x20033000;m.mem_write(output,b'\xa5'*4096)
assert call('aux_msc_queue',0,2,0,output,4096)==0xfffffffe
assert call('aux_msc_queue',0,0,0,output,512)==0xffffffff
call('aux_msc_worker_service');assert calls==list(range(2,10))
assert bytes(m.mem_read(output,4096))==b'\xa5'*4096
assert call('aux_msc_take_completion',result)==1 and word(result)==4096
assert bytes(m.mem_read(output,4096))==bytes(((2+i//512)*37+(i%512)*3)&255 for i in range(4096))
for values in ((0,0,0,output,4097),(0,25,0,output,4096),(0,24,1,output,4096),(1,0,0,output,1),(0,0,512,output,1)):
    assert call('aux_msc_queue',*values)==0xffffffff
vendor,product,revision=0x20034000,0x20034020,0x20034040
call('tud_msc_inquiry_cb',0,vendor,product,revision)
assert bytes(m.mem_read(vendor,8))==b'MY2K-EXP' and bytes(m.mem_read(product,16))==b'Read-only assets' and bytes(m.mem_read(revision,4))==b'D015'
assert call('tud_msc_is_writable_cb',0)==0
assert call('tud_msc_write10_cb',0,0,0,output,4096)==0xffffffff
assert 'aux_msc_queue_write' not in syms
if 'aux_msc_write' in syms:assert call('aux_msc_write',0,0,0,output,4096)==0xffffffff
assert calls==list(range(2,10))
# Queue and cancel without copying a completion into the borrowed destination.
assert call('aux_msc_queue',0,0,0,output,4096)==0xfffffffe
call('aux_msc_cancel');assert call('aux_msc_take_completion',result)==0
r={'pass':True,'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'engine':'Unicorn '+uc.__version__,'buffer_bytes':{k:sizes[k]for k in ('completed','read_staging','_mscd_epbuf')},'actual_inquiry_revision':'D015','actual_queue_max_bytes':4096,'actual_backend_sector_order':calls,'actual_write_rejection':True,'actual_bounds_rejection':True,'actual_cancel_no_completion':True,'private_buffer_before_completion':True,'BSS_end':hex(syms['__bss_end']),'stack_top':hex(syms['__stack_top']),'hardware_accessed':False,'limits':'Synthetic successful SD sector function replaces actual SD/MMC; no USB host, IRQ, DCD, flash, timing, DMA/cache or display emulation.'}
args.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
