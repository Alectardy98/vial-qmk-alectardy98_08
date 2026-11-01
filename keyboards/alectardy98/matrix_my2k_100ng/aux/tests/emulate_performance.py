#!/usr/bin/env python3
"""Optimized AUX SPI/worker instruction model: fairness is not an idle wait.

No devices or real time. Same final ELF runs twice with synthetic immediate SPI
flags and either the old post-resume WFI policy or Dev4 runnable-aware policy.
Only avoidable scheduler sleeps are timed, not MCU instruction/SPI wire time.
"""
from pathlib import Path
import argparse,hashlib,json,sys
p=argparse.ArgumentParser();p.add_argument('elf',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args();elf=args.elf.resolve();out=args.output
harness=Path(__file__).with_name('emulate_worker.py');source=harness.read_text();argv=sys.argv[:];sys.argv=[str(harness),str(elf)]
g={'__file__':str(harness)};exec(compile(source[:source.index('rows=[]')],str(harness),'exec'),g);sys.argv=argv
Machine,s,ps=g['Machine'],g['s'],g['ps'];rows=[]
for policy in ['Dev3_unconditional_idle','Dev4_runnable_aware']:
 x=Machine();x.word(0x40013008,3);x.call('aux_work_init',ps['probe_spi_frame']|1)
 slices=sleeps=0;maximum=0
 while x.call('aux_work_active'):
  before=x.instructions;x.call('aux_work_resume');maximum=max(maximum,x.instructions-before);slices+=1
  runnable=x.call('aux_work_runnable')
  if x.call('aux_work_active') and (policy=='Dev3_unconditional_idle' or not runnable):
   sleeps+=1;x.time+=1;x.word(s['milliseconds'],x.time)
  assert slices<2000
 assert x.word(0x20030100)==1 and not x.word(s['aux_work_stack_fault'])
 assert x.word(s['aux_work_runnable_yields'])>500
 rows.append({'policy':policy,'SPI_payload_bytes':34176,'worker_resume_calls':slices,'avoidable_WFI_episodes':sleeps,
  'scheduler_sleep_model_ms':sleeps,'maximum_instructions_per_worker_resume':maximum,'total_emulated_instructions':x.instructions})
assert rows[0]['avoidable_WFI_episodes']>=534 and rows[1]['avoidable_WFI_episodes']==0
# A real idle yield must permit WFI and remain resumable. An uninitialized or
# finished worker must never make foreground spin. Context ownership unchanged.
x=Machine();assert not x.call('aux_work_runnable');x.call('aux_work_init',ps['probe_idle']|1);assert x.call('aux_work_runnable')
x.call('aux_work_resume');assert x.call('aux_work_active') and not x.call('aux_work_runnable')
assert x.word(s['aux_work_idle_yields'])==1
x.call('aux_work_resume');assert not x.call('aux_work_active') and not x.call('aux_work_runnable')
assert x.word(0x20030100)==1
result={'pass':True,'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'engine':'Unicorn '+g['uc'].__version__,
 'same_compiled_SPI_and_context_code_both_policies':True,'immediate_synthetic_SPI_flags':True,'cases':rows,
 'idle_yield_active_but_not_runnable_then_resumes_and_finishes':True,'UART_USB_foreground_between_each_slice':True,
 'hardware_accessed':False,'limits':'Instruction count is not cycle count. Old/main sleep policy modeled around actual current worker/SPI ELF;1ms synthetic idle represents SysTick-only wakes. USB/UART IRQ wakes, physical SPI/card/flash waits/CPU caches not modeled; this is avoidable wait evidence, not measured board FPS.'}
out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
