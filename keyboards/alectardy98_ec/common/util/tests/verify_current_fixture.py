#!/usr/bin/env python3
"""Read-only current Vial identity/geometry/mask linkage check. No build or USB."""
from pathlib import Path
import json, re, sys
kb = Path(sys.argv[1])
fixture = Path(__file__).parent / 'fixtures/class0413_ec.vial.json'
actual = json.loads((kb / 'keymaps/vial_alec/vial.json').read_text())
assert actual == json.loads(fixture.read_text()), 'Update the desktop fixture for the current Vial definition'
assert actual['matrix'] == {'rows': 5, 'cols': 21}
assert [v['name'] for v in actual['customKeycodes'][:4]] == ['EC_NOIS', 'EC_BOTM', 'EC_PRNT', 'LK_SONG']
header = (kb / 'keymaps/vial_alec/config.h').read_text()
assert re.search(r'0x45,\s*0x46,\s*0x4C,\s*0x44,\s*0x49,\s*0x41,\s*0x47,\s*2', header)
config = (kb / 'config.h').read_text()
line = next(l for l in config.splitlines() if l.startswith('#define UNUSED_POSITIONS_LIST'))
mask = [tuple(map(int,m)) for m in re.findall(r'\{\s*(\d+),\s*(\d+)\s*\}', line)]
assert len(mask) == 14 and len(set(mask)) == 14
assert sorted(r*21+c for r,c in mask) == [37,58,62,78,83,89,90,91,93,94,95,96,100,101]
print('PASS current vial_alec definition/UID, original EC IDs/order, 5x21 matrix and 91/14 mask')
