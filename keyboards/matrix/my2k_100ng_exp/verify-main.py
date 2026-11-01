#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Validate build-only artifacts and captured maps without opening any device."""
from pathlib import Path
import hashlib,json,re,struct,subprocess
root=Path(__file__).resolve().parents[3]
out=root/'development/my2k-100ng-experimental/build/main'
capture=bytes.fromhex((Path(__file__).parent/'captured-keymap-data.txt').read_text())
expected=struct.unpack('>512H',capture)
results=[]
for variant in ['stock_aux','replacement_aux','release']:
    stem='matrix_my2k_100ng_exp_'+variant
    elf=out/(stem+'.elf'); binary=out/(stem+'.bin'); data=binary.read_bytes()
    assert 0<len(data)<=131072
    sp,reset=struct.unpack_from('<II',data)
    assert 0x20000000<sp<=0x20020000 and reset&1 and 0x08020000<=(reset&~1)<0x08040000
    symbols=subprocess.check_output(['arm-none-eabi-nm','-S',str(elf)],text=True)
    match=re.search(r'^([0-9a-f]+)\s+([0-9a-f]+)\s+[rR]\s+keymaps$',symbols,re.M)
    assert match and int(match[2],16)==1024
    offset=int(match[1],16)-0x08020000
    keycodes=struct.unpack('<512H',data[offset:offset+1024])
    if variant=='release':
        assert keycodes[5*16]==0xe0  # real left Control, no captured tap dance
        assert all(k<0x7700 or k>=0x7800 for k in keycodes)
        assert keycodes[256:]==(1,)*256  # two clean transparent user layers
    else:
        assert keycodes==expected
    for forbidden in ['flash_lld_program','flash_lld_start_erase_sector','backing_store_write']:
        assert not re.search(r'\b'+forbidden+r'$',symbols,re.M),forbidden
    required=['my2k_usbh_start','my2k_usbh_capacity','my2k_usbh_submit_read','my2k_usbh_take_read','my2k_qmk_rgb_init','my2k_qmk_rgb_flush','my2k_aux_start','my2k_mcu_identity','my2k_aux_track','my2k_journal_init','my2k_journal_load','my2k_journal_save','my2k_usbh_scsi_init','my2k_scsi_start','my2k_scsi_poll','my2k_scsi_consume','my2k_scsi_cancel','my2k_usb_msc_task','my2k_usbh_file_cancel_reply','my2k_main_fs_reset','my2k_eeprom_task','my2k_eeprom_durable','storage_task']
    for name in required:assert re.search(r'\b'+name+r'$',symbols,re.M),name
    names={line.split()[-1]:int(line.split()[0],16) for line in symbols.splitlines() if len(line.split())>=3}
    assert names['_vectors']==0x08020000
    assert sp==names['__main_stack_end__'] and sp%8==0
    assert reset==names['Reset_Handler']|1
    assert struct.unpack_from('<I',elf.read_bytes(),24)[0]==reset
    vector_bytes=names['__vectors_end__']-names['__vectors_base__']
    assert vector_bytes>=16*4 and vector_bytes%4==0
    ram={'data_bytes':names['__data_end__']-names['__data_base__'],'static_bss_bytes':names['__bss_end__']-names['__bss_base__'],'irq_stack_bytes':names['__main_stack_end__']-names['__main_stack_base__'],'main_stack_bytes':names['__process_stack_end__']-names['__process_stack_base__'],'heap_bytes':names['__heap_end__']-names['__heap_base__']}
    assert ram['heap_bytes']>=32768 and ram['main_stack_bytes']>=2048
    sections=subprocess.check_output(['arm-none-eabi-objdump','-h',str(elf)],text=True).splitlines()
    loads=[]; code_ranges=[]
    for i,line in enumerate(sections[:-1]):
        parts=line.split()
        if len(parts)==7 and parts[0].isdigit() and 'LOAD' in sections[i+1]:
            size,address=int(parts[2],16),int(parts[4],16)
            if size:
                assert 0x08020000<=address and address+size<=0x08040000
                loads.append({'section':parts[1],'lma':hex(address),'bytes':size})
                if 'CODE' in sections[i+1] and parts[1]!='.vectors':code_ranges.append((address,address+size))
    # Inspect every nonzero handler, including IRQs beyond those enabled today.
    for index,target in enumerate(struct.unpack_from('<'+'I'*(vector_bytes//4),data)):
        if index and target:
            assert target&1 and any(lo<=target-1<hi for lo,hi in code_ranges),(index,hex(target))
    assert max(int(x['lma'],16)+x['bytes'] for x in loads)==0x08020000+len(data)
    results.append({'variant':variant,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'sp':hex(sp),'reset':hex(reset),'vector_entries_verified':vector_bytes//4,'captured_keymap_bytes_verified':0 if variant=='release' else 1024,'clean_release_defaults':variant=='release','load_sections':loads,'ram':ram,'persistent_flash_driver_symbols':False,'hardware_tested':False})
(out/'validation.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
