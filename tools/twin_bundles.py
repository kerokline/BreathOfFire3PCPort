#!/usr/bin/env python
"""Write side-by-side bundles: our takeover of a PC function beside a US decompilation's matched C of its twin.

    python -I tools/twin_bundles.py --map analysis/region-twins/pc_map.tsv --decomp <decomp root> --repo <repo> \
        --out <scratch>/pilot --targets emi/battle/battle/03,emi/battle/battle/15

One .md per PC function; index.tsv lists them. Our function runs from its `// original 0xPC`
marker line to the line before the next marker (or 400 lines).
"""
import argparse, csv, glob, os, re

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--map', required=True); ap.add_argument('--decomp', required=True)
    ap.add_argument('--repo', required=True); ap.add_argument('--out', required=True)
    ap.add_argument('--targets', required=True)
    a = ap.parse_args()
    targets = set(a.targets.split(','))
    os.makedirs(a.out, exist_ok=True)
    # index our markers
    marker = re.compile(r'^// original 0x([0-9A-Fa-f]{6})\b')
    ours = {}
    for path in sorted(glob.glob(os.path.join(a.repo, 'src', '**', '*.cpp'), recursive=True)):
        lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
        starts = [(i, int(m.group(1), 16)) for i, l in enumerate(lines) if (m := marker.match(l))]
        for k, (i, pc) in enumerate(starts):
            end = starts[k + 1][0] if k + 1 < len(starts) else len(lines)
            end = min(end, i + 400)
            ours.setdefault(pc, (os.path.relpath(path, a.repo).replace('\\', '/'), i + 1, '\n'.join(lines[i:end])))
    rows = [r for r in csv.DictReader(open(a.map), delimiter='\t') if r['kind'] == 'c' and r['target'] in targets]
    by_pc = {}
    for r in rows:
        by_pc.setdefault(int(r['pc'], 16), []).append(r)
    idx = []
    n = 0
    for pc in sorted(by_pc):
        rs = by_pc[pc]
        if pc not in ours:
            idx.append((pc, rs[0]['jp'], rs[0]['us'], rs[0]['target'], '', 'no-marker'))
            continue
        f, ln, body = ours[pc]
        his = []
        for r in rs:
            p = os.path.join(a.decomp, r['name'])
            if os.path.exists(p):
                his.append((r, open(p, encoding='utf-8', errors='replace').read()))
        if not his:
            idx.append((pc, rs[0]['jp'], rs[0]['us'], rs[0]['target'], f'{f}:{ln}', 'no-file'))
            continue
        n += 1
        name = '%03d_0x%06X.md' % (n, pc)
        with open(os.path.join(a.out, name), 'w', encoding='utf-8') as o:
            o.write('# PC 0x%06X — ours at %s:%d\n\n' % (pc, f, ln))
            for r, txt in his:
                o.write('Twin: JP %s -> US %s (%s, %s; pairing %s, map status %s)\n' % (r['jp'], r['us'], r['target'], r['name'], r['how'], r['status']))
            o.write('\n## Ours (C++)\n\n```cpp\n%s\n```\n' % body)
            for r, txt in his:
                o.write('\n## US matched C (, %s)\n\n```c\n%s\n```\n' % (r['name'], txt))
        idx.append((pc, rs[0]['jp'], rs[0]['us'], rs[0]['target'], f'{f}:{ln}', name))
    with open(os.path.join(a.out, 'index.tsv'), 'w') as o:
        o.write('pc\tjp\tus\ttarget\tours\tbundle\n')
        for row in idx:
            o.write('0x%06X\t%s\t%s\t%s\t%s\t%s\n' % row)
    print(len(idx), 'pc functions;', n, 'bundles;', sum(1 for r in idx if r[5] == 'no-marker'), 'without our marker')

if __name__ == '__main__':
    main()
