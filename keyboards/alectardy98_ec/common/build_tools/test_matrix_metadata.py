#!/usr/bin/env python3
"""Check native QMK matrix metadata without compatibility shims or hardware."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[4]
boards = {'alectardy98_ec/class0413_ec': (5, 21),
          'alectardy98_ec/ec_prox': (5, 15),
          'alectardy98_ec/me_ec_uf2': (6, 16),
          'converter/ibmpc_usb/32u4': (8, 16),
          'converter/nissho/32u4': (8, 16)}
# Fail if any board regresses to the deprecated expression parser. Replacing
# this test-local function is an assertion probe, not runtime compatibility.
probe = '''
import json, os, sys
os.environ['ORIG_CWD'] = os.getcwd()
sys.path.insert(0, 'lib/python')
import qmk.info as info

def forbidden(*args):
    raise AssertionError('Legacy matrix expression parser invoked: ' + repr(args))

info.compute = forbidden
for board, dims in json.loads(sys.argv[1]).items():
    actual = info.info_json(board)['matrix_size']
    assert (actual['rows'], actual['cols']) == tuple(dims), (board, actual)
print('PASS all five boards use metadata directly, without QMK arithmetic evaluation')
'''
env = dict(os.environ)
# No PYTHONPATH shim or warning filter participates in this test. The
# forbidden parser assertion above detects use even when Python would hide
# library DeprecationWarnings by default.
env.pop('PYTHONPATH', None)
env.pop('PYTHONWARNINGS', None)
subprocess.run([sys.executable, '-c', probe,
                json.dumps(boards)], cwd=repo, env=env, check=True)
with tempfile.TemporaryDirectory() as tmp:
    for board, (rows, cols) in boards.items():
        base = Path(tmp)/board.replace('/', '_')
        for command, suffix in [('generate-keyboard-h', '.h'), ('generate-config-h', '_config.h')]:
            output = Path(str(base)+suffix)
            p = subprocess.run(['qmk', command, '-kb', board, '-o', str(output)],
                               cwd=repo, env=env, text=True, capture_output=True, check=True)
            assert 'DeprecationWarning' not in p.stderr and 'ast.Num' not in p.stderr, p.stderr
            if command == 'generate-config-h':
                text = output.read_text()
                for name, expected in [('ROWS', rows), ('COLS', cols)]:
                    assert re.search(r'#\s*define\s+MATRIX_'+name+r'\s+'+str(expected)+r'\b', text), board
print('PASS real QMK header/config generation and exact matrix dimensions for all five boards')
print('Python:', sys.version.split()[0])
