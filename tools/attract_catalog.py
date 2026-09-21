#!/usr/bin/env python
"""What the attract sequence still runs of Capcom's code, as markdown tables.

    python tools/attract_catalog.py analysis/calltrace/hidden_b/bof3x.callcounts.tsv \
        --also analysis/calltrace/all_a/bof3x.callcounts.tsv,analysis/calltrace/all_b/bof3x.callcounts.tsv \
        --out analysis/attract_catalog.md

COUNTS is an all-calls run (BOF3X_CALLTRACE_MODE=all) over
entries_plus_hidden.txt (tools/pe_hidden.py plus); --also adds the reach, not
the counts, of earlier runs. A function is listed when some run reached it and
symbols.toml gives it no impl. Groups are address ranges; what each one is,
and the evidence for it, is docs/attract-remaining.md section 4.
"""
import argparse, bisect, collections, json, tomllib

OWNED_CALLER = 0xFFFFFFFF  # a call made from inside an owned function (calltrace.cpp)
TASK_RUNALL = 0x5A98A0     # counted by the tracer as the frame, not as a call
DIV0010_COPIES = {0x5A2300, 0x5A2520, 0x5A2710}  # Capcom's handlers, re-aimed; unarmed

# (section, title, test) - first match wins. docs/attract-remaining.md section 4.
SHELL = {0x4FCB00, 0x4FC6A0, 0x4FC6F0, 0x4FCAC0, 0x4FD030, 0x4FD110, 0x4FD200, 0x4FD290,
         0x5A9700, 0x5A9880, 0x5A9907, 0x5A72C0}
GROUPS = [
    ('4.1', 'Windows shell', lambda e: e in SHELL),
    ('4.2', 'Task system', lambda e: 0x5A98A0 <= e < 0x5A9A00),
    ('4.13', 'MSVC CRT', lambda e: e >= 0x5B9380),
    ('4.13', 'MP3 decoder', lambda e: 0x5AB000 <= e < 0x5B9380),
    ('4.12', 'Sound', lambda e: 0x5A69C0 <= e < 0x5A72C0 or 0x587740 <= e <= 0x587CD0),
    ('4.10', 'Renderer', lambda e: 0x59E000 <= e < 0x5A6000 or 0x5A9A00 <= e < 0x5AB000),
    ('4.11', 'PSX library layer', lambda e: 0x5A6000 <= e < 0x5A9600),
    ('4.4', 'Text and windows', lambda e: 0x497000 <= e < 0x498800 or 0x516B00 <= e < 0x516E00
                                          or 0x594E00 <= e < 0x596300),
    ('4.3', 'Top-level modes', lambda e: 0x462000 <= e < 0x462B00 or 0x494000 <= e < 0x497000),
    ('4.7', 'Map and draw layers', lambda e: 0x56E6C0 <= e < 0x572B00),
    ('4.8', 'Sprite draw', lambda e: 0x57BAE0 <= e < 0x57C0A0 or 0x5935B0 <= e < 0x594000),
    ('4.6', 'Event script', lambda e: 0x52D000 <= e < 0x537000 or 0x56AD00 <= e < 0x56DA00),
    ('4.5', 'Field objects', lambda e: 0x517200 <= e < 0x519900 or 0x573080 <= e < 0x57BAE0
                                       or 0x57C0A0 <= e < 0x57CE00 or 0x588F00 <= e < 0x589900
                                       or 0x592F00 <= e < 0x593000),
    ('4.9', 'Miscellaneous game code', lambda e: True),
]
LISTED = {'MSVC CRT', 'MP3 decoder'}  # one line each, not a table


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('counts')
    ap.add_argument('--also', default='')
    ap.add_argument('--funcs', default='analysis/pc_funcs.json')
    ap.add_argument('--symbols', default='symbols.toml')
    ap.add_argument('--entries', default='analysis/calltrace/entries_plus_hidden.txt')
    ap.add_argument('--reached', default='analysis/pc_hidden_reached.json')
    ap.add_argument('--out', default='analysis/attract_catalog.md')
    a = ap.parse_args()

    funcs = {f['entry']: f for f in json.load(open(a.funcs))['functions']}
    named = {f['pc']: f for f in tomllib.load(open(a.symbols, 'rb'))['func']}
    ours = {pc for pc, f in named.items() if 'impl' in f}
    size = {}
    for line in open(a.entries):
        if line[0] != '#' and line.strip():
            e, s = line.split()
            size[int(e, 16)] = int(s, 16)
    hidden = {r['entry']: r for r in json.load(open(a.reached))}
    starts = sorted(set(funcs) | set(hidden))

    def owner(addr):
        return starts[bisect.bisect_right(starts, addr) - 1]

    total, callers, frames = {}, collections.defaultdict(collections.Counter), 0
    for line in open(a.counts):
        if line[0] == '#':
            if 'frame' in line:
                frames = int(line.split('frame')[1].split()[0].rstrip(';'))
            continue
        e, c, n = (int(x, 16) if i < 2 else int(x) for i, x in enumerate(line.split()))
        if c == 0:
            total[e] = total.get(e, 0) + n
        elif c == OWNED_CALLER:
            callers[e]['ours'] += n
        elif c in range(0x401000, 0x5C5000):
            callers[e][f'`0x{owner(c):06X}`'] += n
        else:
            callers[e]['outside exe'] += n
    reach = set(total) | set(hidden) | {TASK_RUNALL}
    for path in filter(None, a.also.split(',')):
        for line in open(path):
            if line[0] != '#' and int(line.split()[1], 16) == 0:
                reach.add(int(line.split()[0], 16))

    groups = collections.defaultdict(list)
    for e in sorted(reach - ours):
        sec, title, _ = next(g for g in GROUPS if g[2](e))
        f = funcs.get(e)
        n = min(size.get(e, 1 << 30), f['size'] if f else 1 << 30)
        groups[(sec, title)].append(dict(
            entry=e, name=named.get(e, {}).get('name', ''), calls=total.get(e, 0),
            size=n if n < 1 << 30 else hidden[e]['size'],
            how=hidden[e]['how'] if e in hidden else '',
            callers=', '.join(c for c, _ in callers[e].most_common(3))))

    order = sorted(groups, key=lambda k: [int(x) for x in k[0].split('.')] + [k[1] in LISTED])
    out = ['| Group | Functions | Of them hidden | Calls | § |', '|---|--:|--:|--:|---|']
    for k in order:
        rs = groups[k]
        out.append(f'| {k[1]} | {len(rs)} | {sum(1 for r in rs if r["how"])} | '
                   f'{sum(r["calls"] for r in rs):,} | {k[0]} |')
    n = sum(len(v) for v in groups.values())
    out += [f'| **Total** | **{n}** | **{sum(1 for v in groups.values() for r in v if r["how"])}** | | |', '']
    for k in order:
        rs = groups[k]
        out.append(f'<!-- {k[0]} {k[1]}: {len(rs)} functions -->')
        if k[1] in LISTED:
            out += [', '.join(f'`0x{r["entry"]:06X}`' + (f' {r["name"]}' if r['name'] else '')
                              + f' ({r["calls"]:,})' for r in rs), '']
            continue
        out += ['| Entry | Name | Calls | Bytes | Found as | Main callers |', '|---|---|--:|--:|---|---|']
        for r in rs:
            calls = f'{r["calls"]:,}'
            name = f'`{r["name"]}`' if r['name'] else ''
            if r['entry'] == TASK_RUNALL:
                calls = f'{frames:,} (frames)'
            if r['entry'] in DIV0010_COPIES:
                calls, name = 'unarmed', name + ' (DIV-0010 copy)'
            found = f'hidden, {r["how"]}' if r['how'] else ''
            out.append(f'| `0x{r["entry"]:06X}` | {name} | {calls} | {r["size"]} | {found} | {r["callers"]} |')
        out.append('')
    open(a.out, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    print(f'{n} reached and not ours ({len(reach & ours)} reached and ours) -> {a.out}')


if __name__ == '__main__':
    main()
