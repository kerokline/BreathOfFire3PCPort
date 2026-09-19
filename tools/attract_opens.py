#!/usr/bin/env python
"""Summarise the file opens bof3x.dll logged during a run, as load bursts.

    python tools/attract_opens.py analysis/attract/<run>.log

Our File_Open (src/game/file_io.cpp) writes one `open  t=<GetTickCount> ...`
line per call. Opens less than a second apart are one burst - in practice one
scene load. File names are the game's own; output stays under analysis/.
"""
import re, sys

BS = chr(92)
ops = []
failed = 0
for line in open(sys.argv[1], encoding='utf-8', errors='replace'):
    m = re.match(r'open\s+t=(\d+)\s+(slot \d+|FAILED)\s+(.*)', line.strip())
    if m:
        ops.append((int(m.group(1)), m.group(3).replace(BS, '/')))
        failed += m.group(2) == 'FAILED'
if not ops:
    sys.exit('no open lines')
print(f'{len(ops)} opens, {failed} failed')
t0 = ops[0][0]
bursts = []
for t, p in ops:
    if not bursts or t - bursts[-1][-1][0] > 1000:
        bursts.append([])
    bursts[-1].append((t, p))
for b in bursts:
    names = [p for _, p in b]
    print(f'+{(b[0][0] - t0) / 1000:7.1f}s  {len(b):3d} opens  ' + ' '.join(names)[:170])
