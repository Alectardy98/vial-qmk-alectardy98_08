#!/usr/bin/env python3
"""Offline actual-source Virgo/EC42 regression tests; never opens HID."""
import argparse,hashlib,json,os,re,select,struct,subprocess,sys,tempfile,unittest,zlib
from pathlib import Path
from build_harness import BOARD,REPO,build
GUI=Path(os.environ.get('VIAL_ASTRO_SOURCE',REPO.parent/'vial-gui_astro'))
sys.path.insert(0,str(GUI/'src/main/python'))
from ec.protocol import Client,Profile,Op,ECError,PROFILES,decode_record,be16,u16
P=next(p for p in PROFILES if p.uid==bytes.fromhex('564952474f454301'))
HARN=Path(os.environ.get('VIRGO_HARNESS','/tmp/ec-virgo-host-harness'))
HOLES={(0,6),(0,14),(3,14),(3,15),(3,16),(3,17),(4,12),(4,15),(4,17),(5,1),(5,4),(5,8),(5,11),(5,12),(5,14)}

def function(text,signature):
 start=text.index(signature);i=text.index('{',start)+1;depth=1
 while depth:
  depth+=(text[i]=='{')-(text[i]=='}');i+=1
 return text[start:i]

class Peer:
 def __init__(self):
  self.p=subprocess.Popen([str(HARN)],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
  self.client=Client(self.call,P);self.client.identify()
 def call(self,q):
  self.p.stdin.write(q);self.p.stdin.flush()
  if not select.select([self.p.stdout],[],[],5)[0]:raise RuntimeError('Offline harness timed out')
  reply=self.p.stdout.read(32)
  if len(reply)!=32:raise RuntimeError('Offline harness exited '+str(self.p.poll()))
  return reply
 def cmd(self,op,index=0,value=0):
  q=bytearray(b'TEST'+bytes(28));q[4]=op;struct.pack_into('>HH',q,5,index,value);return self.call(q)
 def scan(self,n=32):self.cmd(2,n)
 def input(self,index,value):self.cmd(1,index,value)
 def all(self,value):
  for i in range(P.cells):self.input(i,value)
  self.scan()
 def writes(self):return int.from_bytes(self.cmd(5)[8:12],'big')
 def close(self):
  self.p.stdin.close();self.p.stdout.close();code=self.p.wait(timeout=5)
  if code:raise AssertionError('Native harness/sanitizer exited '+str(code))

class ProvenanceTests(unittest.TestCase):
 def test_common_and_original_source_unchanged(self):
  for name,digest in json.loads((BOARD/'tests/shared-source-baseline.json').read_text()).items():
   self.assertEqual(hashlib.sha256((REPO/name).read_bytes()).hexdigest(),digest,name)
 def test_hardware_acquisition_helpers_and_timing_are_stock(self):
  old=(REPO/'keyboards/cipulot/common/ec_switch_matrix.c').read_text();new=(BOARD/'acquisition.c').read_text()
  for signature in ('void init_row(', 'void init_amux(', 'void disable_unused_row(', 'void select_amux_channel(', 'void disable_unused_amux(', 'void discharge_capacitor(', 'void charge_capacitor(', 'int ec_init(', 'void ec_noise_floor(', 'uint16_t ec_readkey_raw('):
   self.assertEqual(function(old,signature),function(new,signature),signature)
  old=(REPO/'keyboards/cipulot/ec_virgo/config.h').read_text();new=(BOARD/'config.h').read_text()
  for name in ('MATRIX_ROW_PINS','AMUX_COUNT','AMUX_MAX_COLS_COUNT','AMUX_EN_PINS','AMUX_SEL_PINS','AMUX_COL_CHANNELS_SIZES','AMUX_0_COL_CHANNELS','AMUX_1_COL_CHANNELS','DISCHARGE_PIN','ANALOG_PORT','DISCHARGE_TIME','DEFAULT_NOISE_FLOOR_SAMPLING_COUNT'):
   pattern=r'#define '+name+r'\b[^\n]*(?:\\\n[^\n]*)*'
   self.assertEqual(re.search(pattern,old).group(),re.search(pattern,new).group(),name)
 def test_board_metadata_clocks_and_adc_configuration_preserved(self):
  original=REPO/'keyboards/cipulot/ec_virgo'
  for name in ('halconf.h','mcuconf.h'):
   self.assertEqual((original/name).read_bytes(),(BOARD/name).read_bytes())
  old=json.loads((original/'keyboard.json').read_text());old.pop('eeprom')
  self.assertEqual(old,json.loads((BOARD/'keyboard.json').read_text()))
 def test_vial_layout_and_custom_macros_preserved_without_boot_calibration_injection(self):
  for keymap in ('vial','vial_alec'):
   parent=REPO/'keyboards/cipulot/ec_virgo/keymaps'/keymap
   self.assertEqual((parent/'vial.json').read_bytes(),(BOARD/'keymaps'/keymap/'vial.json').read_bytes())
   old=(parent/'keymap.c').read_text().replace('#include "ec_calibration_defaults.h"\n','')
   old=re.sub(r'\nvoid keyboard_post_init_user\(void\) \{\s*ec_apply_calibration_defaults\(\);\s*\}\s*$','\n',old)
   self.assertEqual(old,(BOARD/'keymaps'/keymap/'keymap.c').read_text())

class EngineTests(unittest.TestCase):
 def setUp(self):self.peer=Peer();self.c=self.peer.client
 def tearDown(self):self.peer.close()
 def calibrate(self):
  with tempfile.TemporaryDirectory() as tmp:
   self.c.start(Path(tmp)/'backup.json')
  self.peer.all(100);self.peer.all(900);self.peer.all(100)
  review=self.c.review(fast=True);self.assertTrue(review['can_apply'])
  self.assertEqual(sum(r['status']==1 for r in review['rows']),93)
  saved=self.c.apply_review(review)
  self.assertTrue(decode_record(saved,P).packed)
  return saved
 def test_identity_dimensions_and_fail_closed_boot(self):
  self.assertEqual((P.rows,P.cols,P.record_size,P.magic),(6,18,341,0xa66c))
  self.assertTrue(self.c.capabilities&0x80)
  self.assertEqual(self.c.firmware,'A98-virgo-EC42-2.9g')
  self.assertEqual(self.peer.writes(),0)
  self.peer.all(900)
  samples=self.c.poll(full=True)
  self.assertEqual(sum(s['sampled'] for s in samples),93)
  self.assertFalse(any(s['active'] or s['enabled'] or s['debounced'] for s in samples))
  self.assertEqual(self.peer.writes(),0)
  for i,s in enumerate(samples):self.assertEqual(not s['sampled'],(i//18,i%18) in HOLES)
 def test_calibration_save_and_reboot_preserve_all_93_cells(self):
  saved=self.calibrate();self.peer.cmd(4)
  self.assertEqual(self.c.read_record(),saved)
  samples=self.c.poll(full=True);self.assertEqual(sum(s['enabled'] for s in samples),93)
 def test_global_per_key_deepest_and_real_scanner_thresholds(self):
  self.calibrate();self.c.set_actuation(0,70);self.c.set_actuation(1,30)
  self.c.set_global_actuation(60)
  self.assertFalse(any(self.c.record.overrides))
  self.c.set_actuation(0,99)
  s=self.c.poll(0,selected_only=True,metadata='selected')[0]
  self.assertEqual(s['press'],899)
  self.peer.input(0,899);self.peer.scan();self.assertFalse(self.c.poll(0,selected_only=True)[0]['active'])
  self.peer.input(0,900);self.peer.scan();self.assertTrue(self.c.poll(0,selected_only=True)[0]['active'])
  self.peer.input(0,100);self.peer.scan();self.assertFalse(self.c.poll(0,selected_only=True)[0]['active'])
  self.c.set_global_actuation(50);self.peer.cmd(3,2100);self.peer.scan(2)
  self.assertEqual(self.c.actuation()[4],0)
  record=self.c.read_record();self.peer.cmd(4);self.assertEqual(self.c.read_record(),record)
  self.assertFalse(any(self.c.active_record()[-P.cells:]))
 def test_rapid_trigger_and_released_key_guard(self):
  self.calibrate();self.c.set_global_actuation(50);self.c.set_rt(1,30,30)
  self.peer.input(0,800);self.peer.scan();self.assertTrue(self.c.poll(0,selected_only=True)[0]['active'])
  before=self.c.active_record()
  with self.assertRaises(ECError):self.c.set_global_actuation(60)
  self.assertEqual(self.c.active_record(),before)
  self.peer.input(0,740);self.peer.scan();self.assertFalse(self.c.poll(0,selected_only=True)[0]['active'])
  self.peer.input(0,780);self.peer.scan();self.assertTrue(self.c.poll(0,selected_only=True)[0]['active'])
  self.peer.input(0,100);self.peer.scan();self.assertFalse(self.c.poll(0,selected_only=True)[0]['active'])
 def test_capture_cancel_and_stale_packets_do_not_write(self):
  saved=self.calibrate();before=self.peer.writes()
  with tempfile.TemporaryDirectory() as tmp:self.c.start(Path(tmp)/'backup.json')
  self.peer.all(900);self.peer.all(100);self.c.control(4)
  self.assertEqual(self.c.read_record(),saved);self.assertEqual(self.peer.writes(),before)
  with self.assertRaises(ECError):self.c.request(Op.PAGE,be16(0)+be16(0)+b'\x01')
  with self.assertRaises(ECError):self.c.request(Op.GLOBAL_RESET,be16(0xffff)+b'\x32\xff\xff')
  self.assertEqual(self.c.read_record(),saved)
 def test_partial_restore_and_bad_crc_never_commit(self):
  saved=self.calibrate();before=self.peer.writes()
  token=self.c.request(Op.RESTORE_BEGIN,be16(P.record_size)+zlib.crc32(saved).to_bytes(4,'big'))
  self.c.request(Op.RESTORE_CHUNK,token+be16(0)+b'\x01'+saved[:1])
  with self.assertRaises(ECError):self.c.request(Op.RESTORE_COMMIT,token)
  self.assertEqual(self.peer.writes(),before);self.assertEqual(self.c.read_record(),saved)
 def test_foreign_board_identity_refused_before_mutation(self):
  before=self.peer.writes()
  foreign=Client(self.peer.call,PROFILES[0])
  with self.assertRaises(ECError):foreign.identify()
  self.assertEqual(self.peer.writes(),before)

if __name__=='__main__':unittest.main(verbosity=2)
