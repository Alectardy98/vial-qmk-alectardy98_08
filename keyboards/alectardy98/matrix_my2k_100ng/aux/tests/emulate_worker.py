#!/usr/bin/env python3
"""Execute release ELF instructions, with explicitly modeled peripheral responses.

This is not board emulation: clocks/USB enumeration/IRQs are not modeled. Real
worker/context, SD command/data loops, panel initializer, parser and dispatcher
execute. Test fixtures substitute time and UART/USB transport at function seams.
"""
from pathlib import Path
import argparse, hashlib, json, struct, subprocess
import unicorn as uc
from unicorn import arm_const as a
P=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('elf',type=Path);args=ap.parse_args()
elf=args.elf.resolve();build=elf.parent;binary=elf.with_suffix('.bin').read_bytes()
def syms(path):
    return {f[2]:int(f[0],16) for l in subprocess.check_output(['arm-none-eabi-nm',str(path)],text=True).splitlines() if len(f:=l.split())==3}
s=syms(elf)
probe=build/'worker-probe.elf'
subprocess.run(['arm-none-eabi-gcc',*(['-DAUX_DEV4_PROBE'] if 'aux_work_idle' in s else []),'-mcpu=cortex-m7','-mthumb','-mfpu=fpv5-sp-d16','-mfloat-abi=softfp','-nostdlib','-Wl,-Ttext=0x08100000','-Wl,-e,probe_worker','-Wl,--just-symbols='+str(elf),str(P/'tests/worker_probe.S'),'-o',str(probe)],check=True)
subprocess.run(['arm-none-eabi-objcopy','-O','binary',str(probe),str(probe.with_suffix('.bin'))],check=True)
ps=syms(probe)
STOP=0x0810f000
class Machine:
    def __init__(self):
        self.m=uc.Uc(uc.UC_ARCH_ARM,uc.UC_MODE_THUMB|uc.UC_MODE_MCLASS)
        self.m.mem_map(0x08000000,0x120000);self.m.mem_write(0x08000000,binary)
        self.m.mem_write(0x08100000,probe.with_suffix('.bin').read_bytes())
        self.m.mem_map(0x20000000,0x40000);self.m.mem_map(0x40000000,0x50000);self.m.mem_map(0xe0000000,0x100000)
        self.m.mem_write(s['__data_start'],binary[s['__data_load']-0x08000000:s['__data_load']-0x08000000+s['__data_end']-s['__data_start']])
        self.m.reg_write(a.UC_ARM_REG_C1_C0_2,0xf<<20);self.m.reg_write(a.UC_ARM_REG_FPEXC,1<<30)
        self.time=0;self.hooks={};self.instructions=0;self.checkpoints=0;self.sd_mode='timeout';self.commands=[];self.fifo=0
        self.periodic_tick_instructions=0;self.stop_at_time=None
        self.m.hook_add(uc.UC_HOOK_CODE,self.code)
        self.m.hook_add(uc.UC_HOOK_MEM_WRITE,self.write,begin=0x40012c00,end=0x40012cff)
        self.m.hook_add(uc.UC_HOOK_MEM_READ,self.read,begin=0x40012c00,end=0x40012cff)
        self.hooks[s['aux_millis']]=lambda:self.ret(self.time)
    def word(self,addr,value=None):
        if value is not None:self.m.mem_write(addr,struct.pack('<I',value&0xffffffff))
        return struct.unpack('<I',self.m.mem_read(addr,4))[0]
    def ret(self,result=0):
        self.m.reg_write(a.UC_ARM_REG_R0,result);self.m.reg_write(a.UC_ARM_REG_PC,self.m.reg_read(a.UC_ARM_REG_LR))
    def code(self,m,addr,size,unused):
        self.instructions+=1
        # Optional synthetic independent SysTick for full main-loop scheduling.
        # Dev4 runnable work intentionally does not WFI after every fairness
        # yield, so time must advance while instructions execute as on hardware.
        if self.periodic_tick_instructions and self.instructions%self.periodic_tick_instructions==0:
            self.time+=1;self.word(s['milliseconds'],self.time)
            if self.stop_at_time is not None and self.time>=self.stop_at_time:self.m.emu_stop();return
        if addr==s['aux_work_checkpoint']:self.checkpoints+=1
        if addr in self.hooks:self.hooks[addr]()
    def write(self,m,access,addr,size,value,unused):
        if addr==0x40012c0c:
            cmd=value&63;self.commands.append(cmd)
            status=0;reply=0
            if self.sd_mode!='timeout':
                status=1<<7 if cmd==0 else 1<<6
                reply={8:0x1aa,41:0xc0ff8000,3:0x12340000,13:4<<9}.get(cmd,0)
                if cmd==9:
                    for i,v in enumerate((0x40000000,0,0x001f0000,0)):self.word(0x40012c14+4*i,v)
                else:self.word(0x40012c14,reply)
                if cmd==17:self.fifo=0
            self.word(0x40012c34,status)
    def read(self,m,access,addr,size,value,unused):
        if addr==0x40012c34 and self.commands and self.commands[-1]==17 and self.sd_mode!='timeout':
            status=1<<6
            if self.sd_mode=='data':
                status|=(1<<21) if self.fifo<128 else (1<<8)
            elif self.sd_mode=='error':status|=1<<1
            self.word(addr,status)
        if addr==0x40012c80:
            self.word(addr,sum(((self.fifo*4+b)&255)<<(8*b) for b in range(4)));self.fifo+=1
    def call(self,name,*args,limit=3000000):
        address=s.get(name,ps.get(name)) if isinstance(name,str) else name
        self.m.reg_write(a.UC_ARM_REG_SP,0x2003f000);self.m.reg_write(a.UC_ARM_REG_LR,STOP|1)
        for reg,value in zip((a.UC_ARM_REG_R0,a.UC_ARM_REG_R1,a.UC_ARM_REG_R2,a.UC_ARM_REG_R3),args):self.m.reg_write(reg,value)
        for i,value in enumerate(args[4:]):self.word(0x2003f000+4*i,value)
        self.m.emu_start(address|1,STOP,count=limit)
        assert self.m.reg_read(a.UC_ARM_REG_PC)==STOP,(name,hex(self.m.reg_read(a.UC_ARM_REG_PC)))
        return self.m.reg_read(a.UC_ARM_REG_R0)
    def run_worker(self,entry,max_slices=1000,advance_time=True):
        self.call('aux_work_init',ps[entry]|1);slices=0;max_instructions=0
        while self.call('aux_work_active'):
            before=self.instructions;self.call('aux_work_resume');max_instructions=max(max_instructions,self.instructions-before)
            slices+=1;self.time+=int(advance_time);self.word(s['milliseconds'],self.time)
            assert slices<=max_slices,(entry,self.time,self.commands,self.word(s['finished']),self.word(s['slice_start']))
        assert not self.word(s['aux_work_stack_fault'])
        return slices,max_instructions
rows=[]
for pattern in (0x5a,0xa5):
    boot=Machine();boot.m.mem_write(0x20000000,bytes([pattern])*0x40000)
    boot.m.reg_write(a.UC_ARM_REG_SP,0x20040000)
    boot.m.emu_start(s['Reset_Handler']|1,s['aux_main'],count=200000)
    assert boot.m.reg_read(a.UC_ARM_REG_PC)==s['aux_main']
    assert bytes(boot.m.mem_read(s['__bss_start'],s['__bss_end']-s['__bss_start']))==bytes(s['__bss_end']-s['__bss_start'])
    data=binary[s['__data_load']-0x08000000:s['__data_load']-0x08000000+s['__data_end']-s['__data_start']]
    assert bytes(boot.m.mem_read(s['__data_start'],len(data)))==data
    assert boot.m.reg_read(a.UC_ARM_REG_SP)==0x20040000
    rows.append({'case':'actual reset dirty RAM','status':'PASS','pattern':hex(pattern),'bss_bytes':s['__bss_end']-s['__bss_start'],'data_bytes':len(data),'MSP':'0x20040000'})
x=Machine();x.call('aux_work_init',ps['probe_worker']|1)
values={a.UC_ARM_REG_R4:0xdead0044,a.UC_ARM_REG_R5:0xdead0055,a.UC_ARM_REG_D8:0x1234000056780000,a.UC_ARM_REG_D15:0x9876000054320000}
for r,v in values.items():x.m.reg_write(r,v)
for _ in range(2):
    x.call('aux_work_resume')
    for r,v in values.items():assert x.m.reg_read(r)==v,(r,hex(x.m.reg_read(r)),hex(v))
assert struct.unpack('<6I',x.m.mem_read(0x20030000,24))==(0x12345678,0x9abcdef0,0x12345678,0x9abcdef0,0x9abcdef0,0x12345678)
assert x.word(0x20030018)%8==0 and not x.call('aux_work_active')
rows.append({'case':'actual MSP/context/FPU callee save','status':'PASS','worker_stack_alignment':8})
for mode in ('timeout','data','stall','error'):
    x=Machine();x.sd_mode=mode
    if mode=='timeout':entry='probe_init'
    else:
        x.word(s['sector_count'],4096);x.m.mem_write(s['high_capacity'],b'\x01');x.m.mem_write(s['card_rca'],b'\x34\x12');entry='probe_read'
    slices,largest=x.run_worker(entry)
    result=x.word(0x20030100)
    assert result==(mode=='data'),(mode,result,x.commands,x.fifo,x.word(0x40012c34),slices)
    if mode=='data':assert bytes(x.m.mem_read(0x20030200,512))==bytes(range(256))*2
    if mode in ('timeout','stall'):assert slices>=200
    assert largest<16000,(mode,largest)
    assert set(x.commands)<=set((0,2,3,6,7,8,9,13,16,17,41,55))
    rows.append({'case':'real SDMMC '+mode,'status':'PASS','foreground_service_slices':slices,'maximum_instructions_per_worker_slice':largest,'commands':x.commands})
x=Machine();x.sd_mode='data';slices,largest=x.run_worker('probe_init')
assert x.word(0x20030100)==1 and x.word(0x40012c04)==0x4903
rows.append({'case':'real SD init command sequence and recovered HWFC','status':'PASS','CLKCR':'0x00004903','commands':x.commands,'worker_slices':slices})
for case,entry in (('SD-command','probe_read'),('SD-data','probe_read'),
                   ('I2C-touch','probe_touch'),('SPI-panel','probe_spi'),('reset-delay','probe_delay')):
    x=Machine();x.word(s['sector_count'],4096)
    x.m.mem_write(s['high_capacity'],b'\x01');x.m.mem_write(s['card_rca'],b'\x34\x12')
    if case=='SD-data':x.sd_mode='stall'
    slices,largest=x.run_worker(entry,max_slices=5000,advance_time=False)
    assert x.word(0x20030100)==0 and x.time==0,(case,x.time,x.commands)
    assert 1500<slices<4000 and largest<16000,(case,slices,largest)
    if case=='reset-delay':assert x.word(s['aux_fault_code'])==10
    if case=='I2C-touch':assert x.word(0x40005804)&(1<<14)
    if case=='SD-data':assert x.word(0x40012c2c)==0
    rows.append({'case':'actual peripheral stopped-SysTick '+case,'status':'PASS',
                 'returned_success':False,'foreground_service_slices':slices,
                 'maximum_instructions_per_worker_slice':largest})
# Real TinyUSB init, adapted core wait, and abort path. Model only DWC2
# readiness/clear-on-write flags and elapsed time, not any host bus transaction.
for mode in ('ready','AHBIDL-low','CSRST-held','DONE-low','TXFLUSH-held','RXFLUSH-held','stopped-SysTick'):
    usb=Machine();ticks=[0];reset_writes=[];mmio_writes=[]
    usb.word(0x40040040,0x4f54420a if mode=='DONE-low' else 0x4f542000)
    usb.word(0x40040048,(1<<8)|(5<<10));usb.word(0x4004004c,1024<<16)
    usb.word(0x40040010,0x80000000 if mode not in ('AHBIDL-low','stopped-SysTick') else 0)
    def usb_time():
        ticks[0]+=1;usb.time=0 if mode=='stopped-SysTick' else ticks[0]//128;usb.ret(usb.time)
    usb.hooks[s['aux_millis']]=usb_time
    def usb_read(m,access,addr,size,value,unused):
        if addr!=0x40040010:return
        v=usb.word(addr)
        if mode not in ('AHBIDL-low','stopped-SysTick'):v|=1<<31
        if mode not in ('CSRST-held','DONE-low'):v&=~1
        if mode!='TXFLUSH-held':v&=~(1<<5)
        if mode!='RXFLUSH-held':v&=~(1<<4)
        usb.word(addr,v)
    def usb_write(m,access,addr,size,value,unused):
        mmio_writes.append((addr,value))
        if addr==0x40040010:reset_writes.append(value)
    usb.m.hook_add(uc.UC_HOOK_MEM_READ,usb_read,begin=0x40040010,end=0x40040010)
    usb.m.hook_add(uc.UC_HOOK_MEM_WRITE,usb_write,begin=0x40000000,end=0x4004ffff)
    usb.m.hook_add(uc.UC_HOOK_MEM_WRITE,usb_write,begin=0xe000e000,end=0xe000efff)
    before=usb.instructions;result=usb.call('aux_usb_init',limit=10000000)
    assert result==(mode=='ready'),(mode,result,reset_writes)
    if mode!='ready':
        assert not usb.word(0x40040008)&1 and usb.word(0x40040018)==0 and usb.word(0x40040804)&2
        assert usb.word(0xe000e188)&(1<<13)
        assert usb.call('aux_usb_failed') and usb.word(s['aux_usb_fault_register'])==0
        count=len(mmio_writes);epoch=usb.call('aux_msc_generation')
        assert usb.call('aux_usb_init')==0 and usb.call('aux_usb_init')==0
        assert usb.call('aux_msc_generation')==epoch
        usb.call('dcd_int_enable',1)
        usb.call('aux_usb_task') # false readiness must skip the half-initialized TinyUSB task
        assert len(mmio_writes)==count,(mode,'failed initialization rearmed hardware')
        assert not usb.word(0x40040008)&1 and not usb.word(0x40040018) and usb.word(0x40040804)&2
        assert usb.time<=20 or mode=='stopped-SysTick'
    else:
        assert usb.word(0x40040008)&1 and not usb.word(0x40040804)&2
        assert usb.word(0xe000e108)&(1<<13)
    rows.append({'case':'actual TinyUSB init '+mode,'status':'PASS','returned_success':bool(result),'simulated_ms':usb.time,'instructions':usb.instructions-before,'reset_writes':[hex(v) for v in reset_writes],'repeated_failed_init_rejected_without_MMIO_or_epoch_change':True if mode!='ready' else None})
# Execute the actual generated DCD and foreground cleanup against explicitly
# injected MMIO failures. SysTick is frozen throughout: ISR timeouts cannot
# depend on a lower/equal-priority timer interrupt preempting them.
runtime_cases = ('reset-ready','stall-IN-ready','stall-OUT-ready','RX-drain-ready',
                 'reset-TX-held','reset-RX-held','close-TX-held','close-RX-held',
                 'IN-NAK-low','IN-disable-low','IN-flush-held','OUT-NAK-low',
                 'OUT-disable-low','RXFLVL-held','SETUP-IN-NAK-low')
for mode in runtime_cases:
    usb=Machine();hold=[0];writes=[];pops=[0];remaining=[0]
    usb.word(0x40040040,0x4f542000)
    usb.word(0x40040048,(1<<8)|(5<<10));usb.word(0x4004004c,1024<<16)
    usb.word(0x40040010,1<<31)
    def runtime_read(m,access,addr,size,value,unused):
        if addr==0x40040010:
            usb.word(addr,(usb.word(addr)|(1<<31))&~((1|(1<<4)|(1<<5))&~hold[0]))
        elif addr==0x40040020:
            pops[0]+=1
            if remaining[0]:
                remaining[0]-=1
                if not remaining[0]:usb.word(0x40040014,0)
    def runtime_write(m,access,addr,size,value,unused):writes.append((addr,value))
    usb.m.hook_add(uc.UC_HOOK_MEM_READ,runtime_read,begin=0x40040000,end=0x40040fff)
    usb.m.hook_add(uc.UC_HOOK_MEM_WRITE,runtime_write,begin=0x40040000,end=0x40040fff)
    usb.m.hook_add(uc.UC_HOOK_MEM_WRITE,runtime_write,begin=0xe000e100,end=0xe000e2ff)
    assert usb.call('aux_usb_init')==1
    # A pending read must be retired in foreground, never from the DCD ISR.
    usb.word(s['sector_count'],4096)
    assert usb.call('aux_msc_queue',0,2,0,0x20031000,512)==0xfffffffe
    epoch=usb.call('aux_msc_generation');before=usb.instructions
    if 'TX-held' in mode or mode=='IN-flush-held':hold[0]=1<<5
    if 'RX-held' in mode:hold[0]=1<<4
    if mode.startswith('reset'):
        usb.word(0x40040014,1<<12);usb.word(0x40040018,1<<12)
        usb.call('dcd_int_handler',1,limit=10000000)
    elif mode.startswith('close'):
        usb.call('dcd_edpt_close_all',1,limit=10000000)
    elif mode.startswith('RX') or mode=='SETUP-IN-NAK-low':
        usb.word(0x40040014,1<<4);usb.word(0x40040018,1<<4)
        usb.word(0x40040020,(4 if mode=='SETUP-IN-NAK-low' else 1)<<17)
        if mode=='SETUP-IN-NAK-low':usb.word(0x40040900,1<<31)
        if mode=='RX-drain-ready':remaining[0]=3
        usb.call('dcd_int_handler',1,limit=10000000)
    else:
        incoming=mode.startswith('IN') or mode=='stall-IN-ready'
        ep=0x40040920 if incoming else 0x40040b20
        usb.word(ep,1<<31)
        usb.word(ep+8,0 if mode=='IN-NAK-low' else (1<<6)|(0 if mode in ('IN-disable-low','OUT-disable-low') else 2))
        usb.word(0x40040014,0 if mode=='OUT-NAK-low' else 1<<7)
        usb.call('dcd_edpt_stall',1,0x81 if incoming else 1,limit=10000000)
    fault=not mode.endswith('ready')
    assert bool(usb.call('aux_usb_failed'))==fault,mode
    assert usb.call('aux_msc_generation')==epoch,(mode,'ISR must defer cancellation')
    if fault:
        assert not usb.word(0x40040008)&1 and not usb.word(0x40040018) and usb.word(0x40040804)&2,mode
        assert usb.word(0xe000e188)&(1<<13) and usb.word(s['aux_fault_code'])==9
        last_disable=max(i for i,(addr,_) in enumerate(writes) if addr==0xe000e188)
        assert not any(addr==0xe000e108 for addr,_ in writes[last_disable+1:]),(mode,'OSAL unlock reenabled failed USB IRQ')
        count=len(writes)
        usb.call('dcd_int_enable',1);usb.call('dcd_connect',1)
        assert usb.call('dcd_edpt_xfer',1,0x81,0x20031000,64,False)==0
        usb.call('OTG_HS_Handler')
        assert len(writes)==count,(mode,'retired USB controller rearmed')
        usb.call('aux_usb_task');usb.call('aux_usb_task')
        assert usb.call('aux_msc_generation')==epoch+1
        assert usb.call('aux_msc_take_completion',0x20032000)==0
        assert usb.call('aux_usb_init')==0 # explicit system reset required
    else:
        assert usb.word(0x40040008)&1 and not usb.word(0x40040804)&2
        if mode=='RX-drain-ready':assert pops[0]==3
    rows.append({'case':'actual DCD runtime '+mode,'status':'PASS','stopped_SysTick':True,
                 'instructions':usb.instructions-before,'RX_status_pops':pops[0],
                 'USB_fault_latched':fault,'fault_register':hex(usb.word(s['aux_usb_fault_register'])),
                 'foreground_cancel_only':True,'automatic_retry':False})
# Main loop with real parser/state, panels, GPIO/SPI wait loops and absent SD.
# Clock setup, UART transport, USB task/IRQ and time are explicit substitutes.
for startup_mode in ('substituted-ready','AHBIDL-low','runtime-reset-TX-held'):
    x=Machine();acks=[];sent_tick=[];usb_ticks=[];rx=bytearray(b'PWR:OFF;\n');last_inject=-1;power_off=[]
    x.periodic_tick_instructions=4096;x.stop_at_time=4500
    x.word(0x40013008,3);x.word(0x40003c08,3)
    def gpio_write(m,access,addr,size,value,unused):
        if addr==0x40020018 and value==1<<10:power_off.append(x.time)
    x.m.hook_add(uc.UC_HOOK_MEM_WRITE,gpio_write,begin=0x40020018,end=0x40020018)
    x.hooks[s['aux_clock_init']]=lambda:x.ret(1)
    x.hooks[s['aux_uart_init']]=lambda:x.ret()
    if startup_mode=='substituted-ready':x.hooks[s['aux_usb_init']]=lambda:x.ret(1)
    else:x.word(0x40040040,0x4f542000)
    runtime_injected=[False]
    if startup_mode=='runtime-reset-TX-held':
        x.word(0x40040048,(1<<8)|(5<<10));x.word(0x4004004c,1024<<16);x.word(0x40040010,1<<31)
        def startup_usb_read(m,access,addr,size,value,unused):
            value=x.word(addr)|(1<<31)
            value&=~(1|(1<<4)|(0 if runtime_injected[0] else 1<<5))
            x.word(addr,value)
        x.m.hook_add(uc.UC_HOOK_MEM_READ,startup_usb_read,begin=0x40040010,end=0x40040010)
    def usb():
        usb_ticks.append(x.time)
        if startup_mode=='substituted-ready':x.ret()
        elif startup_mode=='runtime-reset-TX-held' and x.time==100 and not runtime_injected[0]:
            runtime_injected[0]=True
            x.word(0x40040014,1<<12);x.word(0x40040018,1<<12)
            # Inject the actual handler at a foreground function boundary.
            # Exception stacking/priority are not emulated; time is frozen.
            x.m.reg_write(a.UC_ARM_REG_R0,1)
            x.m.reg_write(a.UC_ARM_REG_PC,s['dcd_int_handler']|1)
    x.hooks[s['aux_usb_task']]=usb
    x.hooks[s['aux_uart_rx_pending']]=lambda:x.ret(bool(rx))
    def get():
        global last_inject
        if x.time%10==0 and last_inject!=x.time:
            rx.extend(b'LED:3;\nLYR:2;\n');last_inject=x.time;sent_tick.append(x.time)
        if rx:x.m.mem_write(x.m.reg_read(a.UC_ARM_REG_R0),bytes([rx.pop(0)]));x.ret(1)
        else:x.ret(0)
    def send():
        data=bytes(x.m.mem_read(x.m.reg_read(a.UC_ARM_REG_R0),x.m.reg_read(a.UC_ARM_REG_R1)));acks.append((x.time,data));x.ret(1)
    def idle():
        x.time+=1;x.word(s['milliseconds'],x.time)
        if x.time>=4500:x.m.emu_stop()
        else:x.ret()
    x.hooks[s['aux_uart_get']]=get;x.hooks[s['aux_uart_send']]=send;x.hooks[s['aux_idle']]=idle
    x.m.reg_write(a.UC_ARM_REG_SP,0x2003f000);x.m.emu_start(s['aux_main']|1,STOP,count=30000000)
    assert x.time==4500,(x.time,hex(x.m.reg_read(a.UC_ARM_REG_PC)))
    assert len(acks)==len(sent_tick)*2 and all(frame==b'STS:OK;\n' for _,frame in acks)
    ack_delays=[acks[i*2+j][0]-tick for i,tick in enumerate(sent_tick) for j in range(2)]
    assert all(0<=delay<=2 for delay in ack_delays),max(ack_delays)
    assert x.word(s['aux_peripherals_initialized'])&0xff and x.word(s['aux_storage_initialized'])&0xff
    assert x.word(s['aux_fault_code'])==2 and not x.word(s['aux_work_stack_fault'])
    max_usb_gap=max(b-a for a,b in zip(usb_ticks,usb_ticks[1:]))
    assert usb_ticks and max_usb_gap<=(22 if startup_mode=='runtime-reset-TX-held' else 3),(startup_mode,max_usb_gap)
    if startup_mode=='AHBIDL-low':assert not x.m.mem_read(s['usb_ready'],1)[0]
    if startup_mode=='runtime-reset-TX-held':
        assert runtime_injected[0] and not x.m.mem_read(s['usb_ready'],1)[0]
        assert x.word(s['aux_usb_fault_register'])==0x40040010
        assert not x.word(0x40040008)&1 and not x.word(0x40040018) and x.word(0x40040804)&2
    assert power_off,power_off
    rows.append({'case':'release main/parser/panels/absent SD startup '+startup_mode,'status':'PASS','simulated_ms':4500,'USB_service_calls':len(usb_ticks),'maximum_USB_service_gap_simulated_ms':max_usb_gap,'LED_LYR_ACKs':len(acks),'maximum_ACK_delay_simulated_ms':max(ack_delays),'storage_failure_reported':True,'early_unacknowledged_PWR_OFF_applied_at_ms':power_off[-1], 'runtime_USB_fault_injected':runtime_injected[0],'time_model':'1ms per4096instructions plus WFI1ms, not cycle-accurate'})
# Execute the original SD handle initializer with HAL/media/log calls replaced,
# then execute the original final register writer without substituted callees.
# This independently anchors the repaired bit to manufacturer instructions.
r=P.parent.parent/'my2k-100ng-investigation/full-flash-analysis'
original=(r/'working/ST_STM32F722RET6_dump.bin').read_bytes()
assert hashlib.sha256(original).hexdigest()=='41d396aca30efeb48383253a3fb08886e1ccd31c58bd0b1527db4993c619caa9'
x=Machine();x.m.mem_write(0x08000000,original);x.hooks={};captured=[]
for address in (0x0800ecda,0x0800f43c,0x0800c350,0x08005f1c):x.hooks[address]=lambda:x.ret(0)
def widebus():
    handle=x.m.reg_read(a.UC_ARM_REG_R0)
    fields=struct.unpack('<7I',x.m.mem_read(handle,28));captured.append(fields)
    assert fields==(0x40012c00,0,0,0,0,0x4000,3),fields
    assert x.m.reg_read(a.UC_ARM_REG_R1)==0x800
    x.ret(0)
x.hooks[0x0800f498]=widebus
assert x.call(0x08005fa0)==1 and len(captured)==1
x.word(0x40012c04,0x100)
x.m.mem_write(0x2003f000,struct.pack('<3I',0x800,0x4000,3))
assert x.call(0x0800f9d4,0x40012c00,0,0,0)==0
assert x.word(0x40012c04)==0x4903
rows.append({'case':'original08005FA0 handle + original0800F9D4 register writer','status':'PASS','CLKCR':'0x00004903','substituted':'HAL/card/log calls in handle initializer; no substitutions in final writer','original_sha256':hashlib.sha256(original).hexdigest()})
report={'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'unicorn':uc.__version__,'limitations':'Synthetic peripheral flags/time; no IRQ, USB bus, card electrical timing or physical validation. Manufacturer behavior addressed by separate original-instruction fixtures.','cases':rows}
out=build/'worker-validation.json';out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
