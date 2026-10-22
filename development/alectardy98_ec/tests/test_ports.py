#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Wire transactions against actual shared engine and actual board scan loops."""
import subprocess,struct,zlib,argparse
p=argparse.ArgumentParser();p.add_argument('board');p.add_argument('harness');p.add_argument('--reference');a=p.parse_args();rows,cols={'class0413_ec':(5,21),'ec_prox':(5,15),'me_ec_uf2':(6,16)}[a.board];cells=rows*cols;size=17+3*cells
class Device:
 def __init__(self):self.p=subprocess.Popen([a.harness],stdin=subprocess.PIPE,stdout=subprocess.PIPE);self.seq=0;self.reference=subprocess.Popen([a.reference],stdin=subprocess.PIPE,stdout=subprocess.PIPE) if a.reference else None
 def exchange(self,b):
  self.p.stdin.write(b.ljust(32,b'\0'));self.p.stdin.flush();r=self.p.stdout.read(32);assert len(r)==32
  if self.reference:
   self.reference.stdin.write(b.ljust(32,b'\0'));self.reference.stdin.flush();expected=self.reference.stdout.read(32);assert r==expected,('Current v2.9 trace differs',b.hex(),r.hex(),expected.hex())
  return r
 def ec(self,op,p=b'',status=0):
  self.seq+=1;r=self.exchange(b'\xcaEC42\x02'+bytes([op])+struct.pack('>H',self.seq)+bytes([0,len(p),0])+p);assert (r[9]==status if status is not None else r[9] in (0,3)),(op,r.hex());return r[12:12+r[10]]
 def tool(self,kind,value=0,index=0):
  b=bytearray(32);b[:4]=b'TEST';b[4]=kind;b[5:7]=struct.pack('>H',index)
  if kind==0:b[8:12]=struct.pack('>I',value)
  else:b[7:9]=struct.pack('>H',value)
  return self.exchange(bytes(b))
 def hold(self,i,v,n=40):
  self.tool(1,v,i)
  for _ in range(n):self.tool(2)
 def backup(self):return b''.join(self.ec(0x16,struct.pack('>HB',o,min(17,size-o)))[3:] for o in range(0,size,17))
 def flags(self,i=0):
  s=self.ec(0x11);return self.ec(0x12,s[:2]+struct.pack('>HB',i,1))[7]
 def cfg(self,i=65535):return self.ec(0x28,struct.pack('>H',i))
 def writes(self):return struct.unpack_from('>I',self.tool(2),8)[0]
 def capture(self,keys):
  t=self.ec(0x20,b'\x01\x00');self.tool(0,60000) # indefinite, no 30 s timeout
  for i in keys:self.hold(i,100);self.hold(i,900);self.hold(i,100)
  self.ec(0x20,b'\x03');return t
 def save(self,t):
  for op in [0x22,0x24,0x25]:self.ec(op,t)
 def va(self,i,v):self.ec(0x29,struct.pack('>HB',i,v)+self.cfg()[:2])
 def rt(self,on,move):self.ec(0x2a,bytes([on,move,move])+self.cfg()[:2])
 def flush(self,time=64000):self.tool(0,time);self.tool(2)
 def close(self):
  self.p.stdin.close();self.p.wait(timeout=5);assert self.p.returncode==0
  if self.reference:self.reference.stdin.close();self.reference.wait(timeout=5);assert self.reference.returncode==0

def initialized():
 d=Device();hello=d.ec(0x10);assert hello[4]==9 and struct.unpack_from('>HH',hello,5)==(cells//cols,cols)
 assert d.cfg()[4]==4 and d.writes()==0
 for i in [0,1,cells-1]:d.hold(i,1023);assert not d.flags(i)&6;d.hold(i,100)
 t=d.capture([0]);d.save(t);return d

d=initialized()
try:
 first=d.backup();assert len(first)==size and struct.unpack_from('>H',first,9)[0]==0xc000|900
 assert d.flags(0)&8 and not d.flags(1)&8
 t=d.capture([1]);d.save(t);assert d.flags(0)&8 and d.flags(1)&8
 # Noise spike never qualifies a third key. Cancellation leaves saved data.
 saved=d.backup();t=d.ec(0x20,b'\x01\x00');d.hold(2,1023,1);d.hold(2,100,40);assert not d.flags(2)&8;d.ec(0x20,b'\x04');assert d.backup()==saved
 # Disconnect and retry discard only volatile capture; saved keys/values survive.
 saved=d.backup();w=d.writes();t=d.ec(0x20,b'\x01\x00');d.hold(4,900);d.hold(4,100)
 d.tool(14,1);assert d.ec(0x14)[0]==0 and d.backup()==saved and d.writes()==w
 d.tool(14,0)
 for retry in range(3):d.ec(0x20,b'\x01\x00');d.ec(0x20,b'\x04')
 for pct in [10,50,99]:
  d.va(65535,pct);d.flush(65000+pct*5000)
  threshold=struct.unpack_from('>H',d.cfg(),6)[0];press=100+threshold*800//1023
  d.hold(0,100);d.hold(0,press);assert not d.flags()&2
  d.hold(0,press+1);assert d.flags()&2
  d.hold(0,100);assert not d.flags()&2
 d.va(0,50);d.va(65535,99);assert d.cfg(0)[13]==50
 active=b''.join(d.ec(0x1d,struct.pack('>HB',o,min(17,size-o)))[3:] for o in range(0,size,17));assert active[17+2*cells]==50
 d.va(0,0);assert d.cfg(0)[13]==0
 d.va(65535,50)
 for move in [4,16,32]:
  d.rt(1,move)
  d.hold(0,100);d.hold(0,480);assert not d.flags()&2
  d.hold(0,700);assert d.flags()&2
  for noisy in [699,701,698,700,702]:d.hold(0,noisy);assert d.flags()&2
  d.hold(0,710)
  d.hold(2,900);assert not d.flags(2)&2;d.hold(2,100)
  d.hold(0,710-move);assert d.flags()&2
  d.hold(0,709-move);assert not d.flags()&2
  d.hold(0,710);assert d.flags()&2
  d.hold(0,100);assert not d.flags()&2
 d.rt(0,16);d.flush(900000);saved=d.backup();w=d.writes();d.tool(8);assert d.backup()==saved and d.writes()==w
 # A second baseline/span exercises deepest VA and the minimum safe RT window.
 d.tool(3,200);t=d.ec(0x20,b'\x01\x00');d.hold(3,200);d.hold(3,320);d.hold(3,200);d.ec(0x20,b'\x03');d.save(t)
 d.va(65535,99);d.rt(1,4);d.hold(3,318);assert not d.flags(3)&2
 d.hold(3,320);assert d.flags(3)&2;d.hold(3,315);assert not d.flags(3)&2
 d.hold(3,320);assert d.flags(3)&2;d.hold(3,314);assert not d.flags(3)&2
 d.hold(3,318);assert not d.flags(3)&2;d.hold(3,200)
 d.rt(0,4);d.flush(1200000);saved=d.backup();w=d.writes();d.tool(8);assert d.writes()==w and d.backup()==saved
 # Corruption is fail closed without startup repair writes.
 d.tool(15,0,index=9);assert d.cfg()[4]==4 and not d.flags()&8
 # Restore packet validation and exact readback, including all per-key bytes.
 token=d.ec(0x17,struct.pack('>HI',size,zlib.crc32(saved)))
 for o in range(0,size,15):part=saved[o:o+15];d.ec(0x18,token+struct.pack('>HB',o,len(part))+part)
 d.ec(0x19,token);assert d.backup()==saved and d.flags()&8
 print('PASS',a.board,'single/incremental capture, >30s, spike/cancel, shallow/medium/deepest, overrides, actual scanner RT4/16/32, disabled HID, CRC, restore, reboot and debounce')
finally:d.close()
# Every write prefix must leave exact old/new or a fail-closed invalid record.
for cut in range(size+8):
 d=initialized()
 try:
  old=d.backup();t=d.capture([1]);d.ec(0x22,t);d.ec(0x24,t);d.tool(6,cut);d.ec(0x25,t,status=None);d.tool(7)
  raw=d.backup()
  if d.cfg()[4]!=4:assert raw==old or (d.flags(0)&8 and d.flags(1)&8)
 finally:d.close()
print('PASS',a.board,size+8,'interrupted-save prefixes; fail-closed or valid old/new calibration')

d=Device()
try:
 w=d.writes();q=bytearray(32);q[:5]=b'\xcaEC41';q[5]=7;assert d.exchange(bytes(q))[8]==0
 d.tool(0,200);assert d.tool(11)[12] and d.writes()==w
 print('PASS',a.board,'bootloader request with all keys disabled; no EEPROM writes')
finally:d.close()
