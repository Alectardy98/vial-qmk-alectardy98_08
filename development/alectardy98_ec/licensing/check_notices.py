#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check established notices; report unknowns without inventing license grants."""
from pathlib import Path
import argparse,json,re,sys
repo=Path(__file__).resolve().parents[3]
fw=repo/'keyboards/alectardy98_ec'
p=argparse.ArgumentParser();p.add_argument('--strict',action='store_true',help='fail on known release-clearance questions');a=p.parse_args()
expected={
 'common/runtime/decision.c':'Copyright 2023 Cipulot',
 'common/engine/calibration.c':'Copyright 2023 Cipulot',
 'common/engine/actuation_control.inc':'Copyright 2023 Cipulot',
 'common/engine/calibration_console.c':'Copyright 2023 Cipulot',
 'common/runtime/ec_board.c':'Copyright 2023 Cipulot',
 'common/engine/ec_switch_matrix.h':'Copyright 2023 Cipulot',
 'ec_prox/acquisition.c':'Copyright 2023 Cipulot',
 'ec_prox/matrix.c':'Copyright 2023 Cipulot',
 'class0413_ec/matrix.c':'Copyright 2023 Cipulot',
 'util/src/widgets.cpp':'Copyright 2020 Purdea Andrei',
 'util/src/mainwindow.cpp':'Copyright 2020 Purdea Andrei',
 'util/src/mainwindow.h':'Copyright 2020 Purdea Andrei',
 'me_ec_uf2/mcuconf.h':'2006..2020 Giovanni Di Sirio',
 'me_ec_uf2/ld/me_ec_boot_STM32F411xE.ld':'2006..2016 Giovanni Di Sirio'}
for f,notice in expected.items():assert notice in (fw/f).read_text(),f
allowed={'GPL-2.0-or-later','GPL-3.0-or-later','GPL-3.0-only','Apache-2.0'}
identifiers={};unknown=[]
for file in fw.rglob('*'):
 if not file.is_file() or any(x in file.parts for x in ['build-universal','build-universal-sanitize','__pycache__','DUMP','LICENSES']):continue
 if file.suffix not in ['.c','.cpp','.h','.inc','.py','.mk','.ld','.license']:continue
 s=file.read_text();ids=[x.rstrip(' */') for x in re.findall(r'SPDX-License-Identifier:\s*([^\r\n]+)',s)]
 for license in ids:assert license in allowed,(file,license)
 for license in set(ids):identifiers[license]=identifiers.get(license,0)+1
 has_notice=bool(ids) or any(x in s for x in ['version 3 of the License, or','version 2 of the License, or','Licensed under the Apache License, Version 2.0'])
 if not has_notice:unknown.append(str(file.relative_to(fw)))
for name in ['GPL-2.0-or-later.txt','GPL-3.0-or-later.txt','GPL-3.0-only.txt','Apache-2.0.txt','LGPL-3.0-only.txt','BSD-3-Clause-HIDAPI.txt','0BSD.txt']:
 assert (fw/'LICENSES'/name).stat().st_size>500,name
for name in ['GPL-2.0-or-later.txt','LGPL-3.0-only.txt','BSD-3-Clause-HIDAPI.txt','0BSD.txt']:
 assert (fw/'util/LICENSES'/name).read_bytes()==(fw/'LICENSES'/name).read_bytes(),name
assert (fw/'util/COPYING').stat().st_size>30000
assert 'does not imply endorsement' in (fw/'README.md').read_text() or 'does\nnot imply endorsement' in (fw/'README.md').read_text()
assert (fw/'util/tests/fixtures/scanner_harness_templates.json.license').exists()
issues=json.loads(Path(__file__).with_name('OPEN_ISSUES.json').read_text())
blockers=[x['id'] for x in issues if x['status']!='resolved' and x['severity']=='release-blocker']
summary={'required_notices':'PASS','SPDX_identifiers_checked':identifiers,'unassigned_code_or_build_files':unknown,'release_clearance':blockers or 'No recorded release blockers; independent review still required'}
print(json.dumps(summary,indent=2))
if a.strict and blockers:sys.exit(2)
