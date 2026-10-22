#!/usr/bin/env python3
"""Compile current EC42 source + actual board scan loops, replacing hardware only.

JSON templates contain host IO, fault injection and GPIO/ADC models, never a
frozen firmware implementation. Source is read anew for every build.
"""
from pathlib import Path
import argparse, json, os, subprocess
repo=Path(__file__).resolve().parents[2]
fw=repo/'keyboards/alectardy98_ec'
def clean(text):
    return '\n'.join(l for l in text.splitlines() if not l.lstrip().startswith(('#include','#pragma')))+'\n'
def build(board,output):
    m=json.loads((Path(__file__).parent/'tests'/f'{board}-mocks.json').read_text())
    def source(path):
        s=(fw/path).read_text()
        for name in ['record_storage.inc','actuation_control.inc']:
            s=s.replace(f'#include "{name}"',(fw/'common/engine'/name).read_text())
        return clean(s)
    header=(fw/'common/engine/ec_switch_matrix.h').read_text()
    types=header[header.index('typedef struct PACKED'):header.index('// Physical allocation')]
    config='\n'.join(l for l in (fw/board/'config.h').read_text().splitlines() if l.startswith(('#define DEFAULT_','#define NOISE_FLOOR_THRESHOLD','#define A98_','#define BOTTOMING_','#define WEAR_LEVELING_EFL_FIRST_SECTOR')))+'\n'
    s=m['pre']+source('common/interfaces/ec42_protocol.h')+config+types+m['stubs']+source('common/engine/calibration.h')+source('common/engine/calibration.c')+source('common/runtime/ec_board.c')+'\n#undef CELLS\n#undef RECORD_BYTES\n'+source('common/engine/ec_diagnostic.c')
    if board=='class0413_ec':
        s+='\nbool ec_actuators_blocked(void){return host_blocked;}\n'+source('common/engine/calibration_console.c')+source(board+'/class0413_ec.c')
    else:s+=source('common/runtime/integration.c')
    if board=='class0413_ec':
        native=(fw/board/'modules/native_actuators.c').read_text()
        s+=next(l for l in native.splitlines() if l.startswith('void suspend_power_down_user('))+'\n'
    s+=m['support']+source('common/runtime/decision.c')
    acquisition=(fw/board/('acquisition.c' if board=='ec_prox' else 'matrix.c')).read_text()
    if board=='me_ec_uf2':
        loop=acquisition[acquisition.index('bool matrix_scan_custom('):acquisition.index('eeprom_ec_config_t eeprom_ec_config;')]
        scan='matrix_scan_custom(host_matrix);'
    else:
        loop=acquisition[acquisition.index('bool ec_matrix_scan('):acquisition.index('// Read the capacitive sensor value')]
        scan='ec_matrix_scan(host_matrix);'
        if board=='class0413_ec':
            # Keep the board's actual integer scaling implementation too.
            start=acquisition.index('uint16_t rescale(')
            loop+=acquisition[start:]
    s+=clean(loop)+'\nstatic void scan(void){'+scan+'matrix_scan_kb();}\n'+m['main']
    output.parent.mkdir(parents=True,exist_ok=True)
    src=output.with_suffix('.c');src.write_text(s)
    flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if os.getenv('SANITIZE')=='1' else []
    subprocess.run(['cc','-std=gnu11','-Werror=implicit-function-declaration',*flags,str(src),'-o',str(output)],check=True)
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('board',choices=['class0413_ec','ec_prox','me_ec_uf2']);p.add_argument('output',type=Path);a=p.parse_args();build(a.board,a.output)
