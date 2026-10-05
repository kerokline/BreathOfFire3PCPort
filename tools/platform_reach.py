#!/usr/bin/env python
"""Which of BOF3.exe's remaining starts still run under ours, and who reaches them.

    python tools/platform_reach.py --catalog analysis/remaining_catalog.tsv \
        [--dll build/bof3x.dll] [--reach RUN.tsv ...] [--tsv OUT.tsv] [--all]

The first step of docs/platform-layers-plan.md section 4: every start that is
not ours and not a jump-table case (the catalogue's parts 0 and 1) with

  static   who refers to it in BOF3.exe: direct calls and jumps (an E8 / E9
           whose target is the start), split by whether the caller's function
           is ours (its body is dead while ours runs) or still Capcom's, and
           the places the start's address is stored (.rdata / .data cells,
           immediates in .text);
  dll      how often the start's address appears in our DLL, by section - a
           call by address from ours is an immediate there. A hit in the
           DLL's data alone is usually a fuzz row naming a callee;
  reach    the traced runs that entered it (bof3x.calltrace.tsv files of runs
           made with every such start armed, ours running), with the first
           caller - an address outside BOF3.exe's .text is ours or Windows.

A start is classed

  runs         a traced run entered it;
  held         nothing traced entered it, and something that can still run
               refers to it: our DLL's code, a data cell, or a function
               classed runs or held (to a fixed point);
  listed       only our DLL's data names it (a fuzz row, a table of ours);
  original     every reference is from a body of ours (dead) or from a start
               that is itself original: reached only by Capcom's code we have
               replaced. Gone at the cutover;
  unreferenced nothing refers to it at all.

The E8 / E9 scan is over bytes, not instructions: a false caller needs four
bytes after an E8 that happen to land on a listed start. The per-start output
is derived from the game's code and stays under analysis/ (CLAUDE.md rule 1).
"""
import argparse, bisect, collections, csv, json, os, struct, sys


def pe_sections(path):
    data = open(path, 'rb').read()
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    nsec, = struct.unpack_from('<H', data, pe + 6)
    optsz, = struct.unpack_from('<H', data, pe + 20)
    opt = pe + 24
    base, = struct.unpack_from('<I', data, opt + 28)
    entry, = struct.unpack_from('<I', data, opt + 16)
    secs = []
    for i in range(nsec):
        s = opt + optsz + i * 40
        name = data[s:s + 8].rstrip(b'\0').decode('latin1')
        vsz, va, rsz, raw = struct.unpack_from('<IIII', data, s + 8)
        secs.append((name, base + va, data[raw:raw + min(rsz, vsz) if vsz else rsz]))
    return base + entry, secs


def find_all(blob, pat):
    i = blob.find(pat)
    while i >= 0:
        yield i
        i = blob.find(pat, i + 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--catalog', default='analysis/remaining_catalog.tsv', help='tools/remaining_catalog.py --tsv')
    ap.add_argument('--exe', default='bof3/BOF3.exe')
    ap.add_argument('--funcs', default='analysis/pc_funcs.json')
    ap.add_argument('--hidden', default='analysis/pc_hidden.json')
    ap.add_argument('--dll', default=None, help='our DLL, the build the traced runs used')
    ap.add_argument('--reach', nargs='*', default=[], help='bof3x.calltrace.tsv files; the file name is the run')
    ap.add_argument('--tsv', default=None)
    ap.add_argument('--all', action='store_true', help='print every start, not only the layers outside the C runtime and the decoder')
    a = ap.parse_args()

    rows = list(csv.DictReader(open(a.catalog, encoding='utf-8'), delimiter='\t'))
    not_ours = {int(r['entry'], 16): r for r in rows}
    plat = {e: r for e, r in not_ours.items() if r['part'][0] in '01'}

    starts = sorted({f['entry'] for f in json.load(open(a.funcs))['functions']} |
                    {h['entry'] for h in json.load(open(a.hidden))} | set(not_ours))

    def owner(addr):
        """The start whose function holds addr, and whether that function is still Capcom's."""
        i = bisect.bisect_right(starts, addr) - 1
        if i < 0:
            return None, False
        s = starts[i]
        return s, s in plat   # a jump-table case inside a body of ours is as dead as the body

    entry, secs = pe_sections(a.exe)
    text = next(s for s in secs if s[0] == '.text')
    tlo, tblob = text[1], text[2]
    thi = tlo + len(tblob)

    # Direct calls and jumps onto a platform start.
    callers = collections.defaultdict(list)   # target -> [(site, caller start, caller is Capcom's)]
    for op in (0xE8, 0xE9):
        for i in find_all(tblob, bytes([op])):
            if i + 5 > len(tblob):
                break
            tgt = (tlo + i + 5 + struct.unpack_from('<i', tblob, i + 1)[0]) & 0xFFFFFFFF
            if tgt in plat:
                s, cap = owner(tlo + i)
                if s != tgt or op == 0xE8:
                    callers[tgt].append((tlo + i, s, cap))
    # The address stored: data cells, and immediates in .text (not the rel32 of a call).
    stored = collections.defaultdict(list)    # target -> [(section, address, holder start, holder is Capcom's)]
    for e in plat:
        pat = struct.pack('<I', e)
        for name, va, blob in secs:
            for i in find_all(blob, pat):
                if name == '.text':
                    s, cap = owner(va + i)
                    stored[e].append((name, va + i, s, cap))
                else:
                    stored[e].append((name, va + i, None, True))
    dll = collections.defaultdict(collections.Counter)
    if a.dll:
        _, dsecs = pe_sections(a.dll)
        for e in plat:
            pat = struct.pack('<I', e)
            for name, va, blob in dsecs:
                if name.startswith('/'):
                    continue          # debug sections
                n = sum(1 for _ in find_all(blob, pat))
                if n:
                    dll[e][name] += n
    reach = collections.defaultdict(dict)     # entry -> {run: first caller}
    for path in a.reach:
        run = os.path.splitext(os.path.basename(path))[0]
        for line in open(path):
            if line[0] == '#':
                continue
            p = line.split('\t')
            e = int(p[1], 16)
            if e in plat:
                reach[e].setdefault(run, int(p[2], 16))

    # Classes, to a fixed point.
    cls = {}
    for e in plat:
        if e in reach:
            cls[e] = 'runs'
    if entry in plat and entry not in cls:
        cls[entry] = 'runs' if not a.reach else 'held'
    changed = True
    while changed:
        changed = False
        for e in plat:
            if e in cls:
                continue
            live = dll[e]['.text'] > 0 or any(n != '.text' for n, *_ in stored[e])
            live = live or any(cap and cls.get(s) in ('runs', 'held') for _, s, cap in callers[e])
            live = live or any(n == '.text' and cap and cls.get(s) in ('runs', 'held') for n, _, s, cap in stored[e])
            if live:
                cls[e] = 'held'
                changed = True
    for e in plat:
        if e not in cls:
            cls[e] = 'listed' if dll.get(e) else 'original' if callers[e] or stored[e] else 'unreferenced'

    def name_of(s):
        r = not_ours.get(s)
        return (r['name'] or '0x%X' % s) if r else '0x%X (ours)' % s

    out = []
    for e in sorted(plat):
        r = plat[e]
        c_ours = sorted({s for _, s, cap in callers[e] if not cap and s is not None})
        c_cap = sorted({s for _, s, cap in callers[e] if cap})
        cells = ['%s:0x%X' % (n, ad) for n, ad, _, _ in stored[e] if n != '.text']
        imms = sorted({s for n, _, s, _ in stored[e] if n == '.text' and s is not None})
        out.append(dict(entry='0x%X' % e, size=r['size'], label=r['label'], name=r['name'], cls=cls[e],
                        runs=' '.join('%s<0x%X' % (k, v) for k, v in sorted(reach[e].items())),
                        callers_capcom=' '.join(name_of(s) for s in c_cap), callers_ours=len(c_ours),
                        callers_ours_list=' '.join('0x%X' % s for s in c_ours[:12]),
                        cells=' '.join(cells[:8]), imm_in=' '.join(name_of(s) for s in imms[:8]),
                        dll=' '.join('%s:%d' % kv for kv in sorted(dll[e].items()))))
    if a.tsv:
        with open(a.tsv, 'w', encoding='utf-8', newline='') as f:
            w = csv.DictWriter(f, fieldnames=list(out[0]), delimiter='\t')
            w.writeheader()
            w.writerows(out)

    by = collections.defaultdict(collections.Counter)
    for o in out:
        by[o['label']][o['cls']] += 1
    kinds = ('runs', 'held', 'listed', 'original', 'unreferenced')
    print('%-22s %5s  %s' % ('layer', 'all', '  '.join('%12s' % k for k in kinds)))
    for label in sorted(by):
        print('%-22s %5d  %s' % (label, sum(by[label].values()), '  '.join('%12d' % by[label][k] for k in kinds)))
    print()
    for o in out:
        if not a.all and o['label'] in ('MSVC CRT', 'MP3 decoder'):
            continue
        print('%-9s %5s %-18s %-12s %-24s runs[%s] capcom[%s] ours-bodies %d cells[%s] imm[%s] dll[%s]' % (
            o['entry'], o['size'], o['label'][:18], o['cls'], o['name'][:24], o['runs'][:70], o['callers_capcom'][:60],
            o['callers_ours'], o['cells'][:40], o['imm_in'][:40], o['dll']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
