#!/usr/bin/env python3
"""Verify six current ELF/image boundaries and report sizes, without USB access."""
from pathlib import Path
import argparse, hashlib, json, struct, subprocess
p=argparse.ArgumentParser();p.add_argument('build_tree',type=Path);p.add_argument('--baseline',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
def info(elf):
 sections=[]
 lines=subprocess.check_output(['arm-none-eabi-objdump','-h',str(elf)],text=True).splitlines()
 for i,l in enumerate(lines[:-1]):
  v=l.split()
  if len(v)==7 and v[0].isdigit():
   name,n,vma,lma,offset=v[1],*[int(x,16) for x in v[2:6]]
   if n and 'LOAD' in lines[i+1] and 0x08000000<=lma<0x08100000:sections.append((name,n,lma,offset))
 d=json.loads('{}');d['linked']=sum(x[1] for x in sections);d['end']=max(x[1]+x[2] for x in sections)
 sizes={l.split()[0]:int(l.split()[1]) for l in subprocess.check_output(['arm-none-eabi-size','-A',str(elf)],text=True).splitlines() if len(l.split())==3 and l.split()[0].startswith('.')}
 d['data']=sizes.get('.data',0);d['bss']=sizes.get('.bss',0);d['stacks']=sizes.get('.mstack',0)+sizes.get('.pstack',0);return d,sections

result=[]
for board,base,limit in [('class0413_ec',0x08010000,0x08020000),('ec_prox',0x08000000,0x08020000),('me_ec_uf2',0x08020000,0x08040000)]:
 for key in ['vial','vial_alec']:
  stem=f'alectardy98_ec_{board}_{key}';elf=a.build_tree/'.build'/f'{stem}.elf';d,sections=info(elf);raw=elf.read_bytes()
  assert all(base<=addr and addr+n<=limit for _,n,addr,_ in sections)
  d.update(board=board,keymap=key,span=d['end']-base,free=limit-d['end'])
  if board=='ec_prox':
   image=a.build_tree/f'firmware/alectardy98_ec/ec_prox/{key}/ec_prox.clean-install.bin';data=image.read_bytes()
   assert data[0x4000:0x8000]==b'\xff'*0x4000
   assert all(addr+n<=0x08004000 or addr>=0x08008000 for _,n,addr,_ in sections)
  else:
   image=a.build_tree/'.build'/f'{stem}.uf2';b=image.read_bytes();assert len(b)%512==0;payload=[]
   for i in range(len(b)//512):
    block=b[i*512:(i+1)*512];m0,m1,flags,addr,n,index,count,family=struct.unpack_from('<8I',block)
    assert (m0,m1,flags,n,index,count,family)==(0x0a324655,0x9e5d5157,0x2000,256,i,len(b)//512,0x57755a57)
    assert struct.unpack_from('<I',block,508)[0]==0x0ab16f30 and addr==base+256*i and addr+n<=limit and not any(block[288:508])
    payload.append(block[32:288])
   data=b''.join(payload)
   bin_path=a.build_tree/'.build'/f'{stem}.bin'
   if bin_path.exists():assert data==bin_path.read_bytes()
  assert all(data[addr-base:addr-base+n]==raw[off:off+n] for _,n,addr,off in sections)
  sp,reset=struct.unpack_from('<II',data);assert 0x20000000<sp<=0x20020000 and reset&1 and base<=(reset&~1)<limit
  d['image']=str(image);d['image_bytes']=image.stat().st_size;d['sha256']=hashlib.sha256(image.read_bytes()).hexdigest()
  if a.baseline:
   before,_=info(a.baseline/'.build'/f'{stem}.elf');d['before']=before;d['flash_saved']=before['end']-d['end'];d['ram_saved']=before['data']+before['bss']-d['data']-d['bss']
  result.append(d)
text=json.dumps(result,indent=2)+'\n'
if a.output:a.output.write_text(text)
print(text)
