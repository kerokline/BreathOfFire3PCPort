#!/usr/bin/env python
"""Every instruction of BOF3.exe that touches the field's view ring (docs/wide-ring-read.md).

    python tools/ring_census.py [--exe bof3/BOF3.exe] [--funcs analysis/pc_funcs.json] [--symbols symbols.toml]
    python tools/ring_census.py --self-test

A linear sweep of .text with capstone, as tools/pe_xref.py does, but keeping
every operand - a memory operand's displacement with or without a base or
index register, and an immediate of any instruction (pe_xref keeps mov / push
only) - that lands inside one of the ring's arrays (a displacement aimed at a
field, +2 of a cell or the pair at +0x20 of a layer, is inside) or on one of
its scalars. No margin round the arrays: every neighbour of the three is
another named variable (Frame_Counter 0x937F94 below MapView_CellItems,
Game_AreaNumber 0x904EFC below MapView_Cells), and a margin of 0x40 caught
1,363 functions through them. A raw 4-byte search of the whole image for each
array's base and end catches pointers held in .data or .rdata.

For each function that touches the ring it then prints the bound and stride
immediates the function holds - 27 / 28 / 55 / 56 and their forms (0x1B,
0x1C, 0x36, 0x37, 0x38, 0x39, 0x70 the CellItems row in bytes, 0x310 and
0x620 the dword / word counts, 0x1880 the byte count) - and the functions
that hold both a row bound and a column bound without touching any ring
address (a copy of the grid code reached through a pointer).

Prints addresses, mnemonics and our names only - no game bytes. The function
list is analysis/pc_funcs.json (tools/pe_funcs.py) joined with symbols.toml's
functions; a function is the entry at or below an instruction.
"""
import argparse
import bisect
import collections
import json
import struct
import sys
import tomllib

# The ring and its satellites: (name, start, bytes). symbols.toml has each.
REGIONS = [
    ('MapView_CellItems', 0x937FA0, 0x38 * 0x1C * 4),
    ('MapView_Cells', 0x904F20, 0x38 * 0x1C * 2),
    ('DrawLayers', 0x8022A0, 0x38 * 0x30),
    ('MapView_Column', 0x929F20, 2),
    ('MapView_Row', 0x929F24, 2),
    ('MapView_Inset', 0x905D80, 1),
    ('MapView_Origin', 0x7E0688, 4),
]
MARGIN = 0             # in-array only: every neighbour of the three arrays is another named variable (symbols.toml)
ROW_BOUNDS = {0x36, 0x37, 0x38, 0x39}
COL_BOUNDS = {0x1B, 0x1C}
OTHER = {0x70: 'row bytes', 0x310: 'dwords', 0x620: 'words', 0x1880: 'bytes', 0xC40: 'word bytes', 0xA80: 'layer bytes'}


def region_of(a):
    for i, (name, lo, n) in enumerate(REGIONS):
        m = MARGIN if i < 3 else 0          # the arrays only; a scalar's neighbours are other variables
        if lo - m <= a < lo + n + m:
            if a < lo:
                return f'{name}-{lo - a:#x}'
            if a >= lo + n:
                return f'{name}+end+{a - lo - n:#x}'
            return f'{name}+{a - lo:#x}' if a != lo else name
    return None


def sections(data):
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    nsec, = struct.unpack_from('<H', data, pe + 6)
    optsz, = struct.unpack_from('<H', data, pe + 20)
    opt = pe + 24
    base, = struct.unpack_from('<I', data, opt + 28)
    out = []
    for i in range(nsec):
        s = opt + optsz + i * 40
        nm = data[s:s + 8].rstrip(b'\0').decode('latin1')
        vsz, va, rsz, raw = struct.unpack_from('<IIII', data, s + 8)
        out.append(dict(name=nm, va=base + va, vsz=vsz, raw=raw, rsz=rsz))
    return base, out


def sweep(data, base, secs):
    import capstone
    t = next(s for s in secs if s['name'] == '.text')
    code = data[t['raw']:t['raw'] + t['rsz']]
    lo = t['va']
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    pos = 0
    while pos < len(code):
        got = False
        for ins in md.disasm(code[pos:], lo + pos):
            got = True
            pos = ins.address - lo + ins.size
            yield ins
        if not got:
            pos += 1


def census(exe, funcs_path, symbols_path):
    data = open(exe, 'rb').read()
    base, secs = sections(data)
    names = {}
    impls = {}
    with open(symbols_path, 'rb') as f:
        sym = tomllib.load(f)
    for x in sym.get('func', []):
        names[x['pc']] = x['name']
        if 'impl' in x:
            impls[x['pc']] = x['impl']
    # pc_funcs.json's 3,004 entries are the call-anchored ones; symbols.toml's
    # functions add the hidden ones (handlers reached through tables), which
    # pc_funcs folds into the function before them.
    ents = sorted({f['entry'] for f in json.load(open(funcs_path))['functions']} | set(names))

    def owner(a):
        i = bisect.bisect_right(ents, a) - 1
        return ents[i] if i >= 0 else 0

    import capstone
    refs = collections.defaultdict(list)       # function -> [(addr, region, text)]
    consts = collections.defaultdict(set)      # function -> {(value, addr)}
    for ins in sweep(data, base, secs):
        fn = owner(ins.address)
        txt = f'{ins.mnemonic} {ins.op_str}'
        for op in ins.operands:
            vals = []
            if op.type == capstone.x86.X86_OP_MEM:
                vals.append(op.mem.disp & 0xFFFFFFFF)
            elif op.type == capstone.x86.X86_OP_IMM:
                v = op.imm & 0xFFFFFFFF
                vals.append(v)
                if v in ROW_BOUNDS | COL_BOUNDS or v in OTHER:
                    consts[fn].add((v, ins.address))
            for v in vals:
                r = region_of(v)
                if r:
                    refs[fn].append((ins.address, r, txt))
    # Pointers held as data: each array's base and end, anywhere in the image.
    pointers = []
    for name, lo, n in REGIONS[:3]:
        for v in (lo, lo + n):
            pat = struct.pack('<I', v)
            at = data.find(pat)
            while at >= 0:
                va = None
                for s in secs:
                    if s['raw'] <= at < s['raw'] + s['rsz']:
                        va = s['va'] + at - s['raw']
                        sec = s['name']
                if va is not None and sec != '.text':
                    pointers.append((name, v, sec, va))
                at = data.find(pat, at + 1)
    copies = []
    for fn, cs in consts.items():
        vs = {v for v, _ in cs}
        if fn not in refs and vs & {0x37, 0x38} and vs & {0x1B, 0x1C}:
            copies.append(fn)
    return refs, consts, pointers, sorted(copies), names, impls


def report(refs, consts, pointers, copies, names, impls, out=sys.stdout):
    p = lambda *a: print(*a, file=out)
    p(f'# ring census: {len(refs)} functions touch the ring or its heads')
    for fn in sorted(refs):
        regions = sorted({r.split('+')[0].split('-')[0] for _, r, _ in refs[fn]})
        cs = sorted(consts.get(fn, ()))
        cstr = ' '.join(f'{v:#x}@{a:#x}' for v, a in cs)
        p(f'{fn:#x} {names.get(fn, "?")} [{impls.get(fn, "-")}] regions={",".join(regions)}')
        for a, r, t in refs[fn]:
            p(f'    {a:#x} {r:28s} {t}')
        if cs:
            p(f'    bounds: {cstr}')
    p(f'# pointers to an array base or end outside .text: {len(pointers)}')
    for name, v, sec, va in pointers:
        p(f'    {name} {v:#x} held at {va:#x} ({sec})')
    p(f'# functions with a row bound and a column bound but no ring address: {len(copies)}')
    for fn in copies:
        cs = sorted(consts[fn])
        p(f'    {fn:#x} {names.get(fn, "?")} [{impls.get(fn, "-")}] ' + ' '.join(f'{v:#x}@{a:#x}' for v, a in cs))


def self_test():
    assert region_of(0x937FA0) == 'MapView_CellItems'
    assert region_of(0x937FA2) == 'MapView_CellItems+0x2'
    assert region_of(0x937FA0 + 0x1880) is None
    assert region_of(0x937F94) is None     # Frame_Counter, the array's neighbour
    assert region_of(0x8022C0) == 'DrawLayers+0x20'
    assert region_of(0x929F24) is not None
    assert region_of(0x500000) is None
    print('ring_census self-test: ok')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default='bof3/BOF3.exe')
    ap.add_argument('--funcs', default='analysis/pc_funcs.json')
    ap.add_argument('--symbols', default='symbols.toml')
    ap.add_argument('--self-test', action='store_true')
    a = ap.parse_args()
    if a.self_test:
        self_test()
        return
    report(*census(a.exe, a.funcs, a.symbols))


if __name__ == '__main__':
    main()
