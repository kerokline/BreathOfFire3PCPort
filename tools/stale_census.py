#!/usr/bin/env python
"""Static census of overlay-born .data: every range of BOF3.exe's initialised
.data whose PlayStation twin lived in an overlay the PlayStation reloaded
(an area, a battle, a boss, a spell, a chapter, a swap-slot screen), and every
.text instruction of BOF3.exe that writes it (docs/stale-overlay-state.md).

    python tools/stale_census.py                     # summary + analysis/stale_census.tsv
    python tools/stale_census.py --build psx-us      # another build's exe_maps/
    python tools/stale_census.py --show 0x65EF00     # the range holding an address, with every writer

The class of defect (D226, the Volt row-done byte): the PlayStation reloaded an
overlay's file image with each area or battle, so a table the overlay's code
wrote came back to its disc bytes the next time; the PC holds the same table
once in its image, so a write survives into the next showing in one process.
A range is a candidate only if some instruction of .text writes it.

Where the ranges come from: exe_maps/<build>.tsv (tools/exe_twins.py map), the
PC range [pc, pc_end) found byte for byte in a disc file. Segments of one file
and section are merged across gaps of at most 64 bytes with no other file's
segment between. BOOT (the PlayStation's boot EXE, resident) is left out.

Writers: a linear sweep of .text with capstone (as tools/pe_xref.py) - but
every memory operand whose displacement lies in .data, with or without a base
or index register (pe_xref.py indexes only base-less operands, so it misses
`mov [eax+0x65EF0C], cl`, D226's own writes). An operand capstone marks
written is a write; `lea`, and `mov`/`push` of an immediate in the range, take
its address (a writer through a pointer may follow; read those by hand). An
indexed operand is assigned by its displacement, so a write `[0x65EF00+eax]`
counts against the range holding 0x65EF00 even when the index reaches the next.

The PC's .bss (0x676000..) is not here: a PlayStation overlay's zero bytes
match nothing by bytes, so .bss twins are found by the dynamic check
(tools/revisit_diff.py) or by reading (the Volt byte was one).

Output: addresses, symbol names and counts only (CLAUDE.md rule 1).
"""
import argparse, bisect, collections, os, struct, sys

import capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bof3x_home  # noqa: E402

DATA_LO, DATA_HI = 0x5DA000, 0x93E000          # .data with its .bss tail (BOF3.exe's section table)
INIT_HI = 0x676000                             # end of the initialised part


def reload_class(path):
    """When the PlayStation brought this file's section back from disc."""
    top, _, name = path.partition('/')
    if path == 'BOOT':
        return 'resident'
    if top.startswith('WORLD'):
        return 'area'
    if top == 'BATTLE':
        return 'battle'
    if top == 'BOSS':
        return 'boss'
    if top == 'BMAGIC':
        return 'spell'
    if top == 'SCENARIO':
        return 'chapter'
    if top == 'ETC':
        stem = name.split('.')[0]
        return 'game' if stem == 'GAME' else stem.lower()
    return top.lower()


def exe_path(arg):
    if arg:
        return arg
    for g in (bof3x_home.default_game(os.path.join(ROOT, 'bof3')),
              os.path.join(ROOT, 'bof3'), os.path.join(ROOT, '..', '..', '..', 'bof3')):
        p = os.path.join(g, 'BOF3.exe')
        if os.path.isfile(p):
            return p
    sys.exit('BOF3.exe not found: pass --exe')


def text_section(path):
    data = open(path, 'rb').read()
    pe, = struct.unpack_from('<I', data, 0x3C)
    n, = struct.unpack_from('<H', data, pe + 6)
    opt, = struct.unpack_from('<H', data, pe + 20)
    base, = struct.unpack_from('<I', data, pe + 24 + 28)
    for i in range(n):
        s = pe + 24 + opt + i * 40
        if data[s:s + 8].rstrip(b'\0') == b'.text':
            vsz, va, rsz, raw = struct.unpack_from('<IIII', data, s + 8)
            return base + va, data[raw:raw + min(vsz, rsz)]
    sys.exit('no .text')


def refs(path):
    """[(insn, disp, kind)] kind w (written), r (read), a (address taken)."""
    lo, code = text_section(path)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    out, pos = [], 0
    X = capstone.x86
    while pos < len(code):
        got = False
        for ins in md.disasm(code[pos:], lo + pos):
            got = True
            pos = ins.address - lo + ins.size
            for op in ins.operands:
                if op.type == X.X86_OP_MEM:
                    d = op.mem.disp & 0xFFFFFFFF
                    if DATA_LO <= d < DATA_HI:
                        if ins.mnemonic == 'lea':
                            k = 'a'
                        elif op.access & capstone.CS_AC_WRITE:
                            k = 'w'
                        else:
                            k = 'r'
                        out.append((ins.address, d, k))
                elif op.type == X.X86_OP_IMM and ins.mnemonic in ('mov', 'push'):
                    d = op.imm & 0xFFFFFFFF
                    if DATA_LO <= d < DATA_HI:
                        out.append((ins.address, d, 'a'))
        if not got:
            pos += 1
    return out


def symbols():
    import tomllib
    with open(os.path.join(ROOT, 'symbols.toml'), 'rb') as f:
        t = tomllib.load(f)
    funcs = sorted((e['pc'], e['name']) for e in t['func'] if isinstance(e.get('pc'), int))
    data = sorted((e['pc'], e['name']) for e in t['data'] if isinstance(e.get('pc'), int))
    return funcs, data


def ranges(build):
    path = os.path.join(ROOT, 'exe_maps', build + '.tsv')
    rows = []
    for line in open(path, encoding='utf-8'):
        if line.startswith('#') or line.startswith('pc\t'):
            continue
        f = line.rstrip('\n').split('\t')
        rows.append((int(f[0], 16), int(f[1], 16), f[2], f[3]))
    rows.sort()
    merged = []
    for lo, hi, fil, sec in rows:
        if merged and merged[-1][2] == fil and merged[-1][3] == sec and lo - merged[-1][1] <= 64:
            merged[-1][1] = max(merged[-1][1], hi)
        else:
            merged.append([lo, hi, fil, sec])
    return [r for r in merged if r[2] != 'BOOT']


def name_at(table, addr):
    keys = [a for a, _ in table]
    i = bisect.bisect_right(keys, addr) - 1
    if i < 0:
        return '?'
    a, n = table[i]
    return n if a == addr else f'{n}+0x{addr - a:X}'


def census(a):
    funcs, data = symbols()
    rs = ranges(a.build)
    starts = [r[0] for r in rs]
    hits = collections.defaultdict(lambda: {'w': [], 'r': [], 'a': []})
    for insn, d, k in refs(exe_path(a.exe)):
        i = bisect.bisect_right(starts, d) - 1
        if i >= 0 and d < rs[i][1]:
            hits[i][k].append((insn, d))
    dkeys = [x for x, _ in data]
    rows = []
    for i, (lo, hi, fil, sec) in enumerate(rs):
        h = hits[i]
        wf = sorted({name_at(funcs, x) .split('+')[0] for x, _ in h['w']})
        af = sorted({name_at(funcs, x).split('+')[0] for x, _ in h['a']})
        if h['w']:
            cls = 'written'
        elif h['a']:
            cls = 'address'
        else:
            cls = 'read-only' if h['r'] else 'unreferenced'
        j, k = bisect.bisect_left(dkeys, lo), bisect.bisect_left(dkeys, hi)
        syms = [data[m][1] for m in range(j, k)] or [name_at(data, lo)]
        rows.append(dict(lo=lo, hi=hi, file=fil, section=sec, reload=reload_class(fil), cls=cls,
                         writes=len(h['w']), reads=len(h['r']), addrs=len(h['a']),
                         writers=wf, takers=af, syms=syms, h=h))
    return rows, funcs


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--build', default='psx-jp', help='exe_maps/<build>.tsv (default psx-jp, the PC\'s parent)')
    ap.add_argument('--exe', help='BOF3.exe (default: the checkout\'s or the release home\'s bof3/)')
    ap.add_argument('--out', default=os.path.join(ROOT, 'analysis', 'stale_census.tsv'))
    ap.add_argument('--show', help='an address: print the range holding it with every reference')
    a = ap.parse_args()
    rows, funcs = census(a)
    if a.show:
        x = int(a.show, 0)
        for r in rows:
            if r['lo'] <= x < r['hi']:
                print(f"0x{r['lo']:06X}..0x{r['hi']:06X} {r['file']}#{r['section']} ({r['reload']}) {r['cls']}")
                print('  data: ' + ' '.join(r['syms'][:20]))
                for k in 'wa':
                    for insn, d in sorted(r['h'][k]):
                        print(f'  {k} 0x{insn:06X} {name_at(funcs, insn):40s} -> 0x{d:06X}')
                return
        sys.exit('no overlay-born range holds 0x%X' % x)
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, 'w', encoding='utf-8') as f:
        f.write('# tools/stale_census.py: overlay-born .data ranges of BOF3.exe and their .text writers '
                f'(exe_maps/{a.build}.tsv). Addresses, names, counts only.\n')
        f.write('lo\thi\tbytes\tfile\tsection\treload\tclass\twrites\treads\taddrs\twriters\ttakers\tdata\n')
        for r in rows:
            f.write(f"0x{r['lo']:06X}\t0x{r['hi']:06X}\t{r['hi'] - r['lo']}\t{r['file']}\t{r['section']}\t"
                    f"{r['reload']}\t{r['cls']}\t{r['writes']}\t{r['reads']}\t{r['addrs']}\t"
                    f"{' '.join(r['writers'])}\t{' '.join(r['takers'])}\t{' '.join(r['syms'][:8])}\n")
    tot = collections.Counter()
    cnt = collections.Counter()
    for r in rows:
        tot[(r['reload'], r['cls'])] += r['hi'] - r['lo']
        cnt[(r['reload'], r['cls'])] += 1
    print(f'{len(rows)} overlay-born ranges, {sum(r["hi"] - r["lo"] for r in rows)} bytes -> {a.out}')
    for key in sorted(cnt):
        print(f'  {key[0]:10s} {key[1]:12s} {cnt[key]:5d} ranges {tot[key]:8d} bytes')


if __name__ == '__main__':
    main()
