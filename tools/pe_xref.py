#!/usr/bin/env python
"""Global-address cross-reference for BOF3.exe.

Linear-sweeps .text once, indexes every absolute memory operand and every
immediate that looks like a .data address, then answers "who touches X".

    python tools/pe_xref.py 0x7dee6e 0x7dee50        # specific addresses
    python tools/pe_xref.py --range 0x7dee40:0x7dee80   # a whole block
    python tools/pe_xref.py --build                  # just build the index

Index is cached at analysis/pc_xref.json (game-derived; never committed).
"""
import sys, json, struct, bisect, os, argparse, collections
import capstone

IDX = 'analysis/pc_xref.json'

def sections():
    data = open('bof3/BOF3.exe','rb').read()
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    nsec, = struct.unpack_from('<H', data, pe+6); optsz, = struct.unpack_from('<H', data, pe+20)
    opt = pe+24; base, = struct.unpack_from('<I', data, opt+28)
    secs=[]
    for i in range(nsec):
        s = opt+optsz+i*40
        nm = data[s:s+8].rstrip(b'\0').decode('latin1')
        vsz,va,rsz,raw = struct.unpack_from('<IIII', data, s+8)
        secs.append(dict(name=nm, va=base+va, vsz=vsz, raw=raw, rsz=rsz))
    return data, base, secs

def build():
    data, base, secs = sections()
    t = next(s for s in secs if s['name']=='.text')
    code = data[t['raw']:t['raw']+t['rsz']]
    lo = t['va']
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail=True
    ents = sorted(f['entry'] for f in json.load(open('analysis/pc_funcs.json'))['functions'])
    hits = collections.defaultdict(list)
    pos = 0
    while pos < len(code):
        got = False
        for ins in md.disasm(code[pos:], lo+pos):
            got = True
            pos = ins.address - lo + ins.size
            for op in ins.operands:
                a = None
                if op.type == capstone.x86.X86_OP_MEM and op.mem.base==0 and op.mem.index==0:
                    a = op.mem.disp & 0xFFFFFFFF
                elif (op.type == capstone.x86.X86_OP_MEM and op.mem.base==0
                      and op.mem.index!=0):
                    a = op.mem.disp & 0xFFFFFFFF
                elif op.type == capstone.x86.X86_OP_IMM and ins.mnemonic in ('mov','push'):
                    a = op.imm & 0xFFFFFFFF
                if a and base <= a < 0x1000000:
                    i = bisect.bisect_right(ents, ins.address) - 1
                    hits[a].append([ins.address, ents[i] if i>=0 else 0,
                                    f'{ins.mnemonic} {ins.op_str}'])
        if not got:
            pos += 1
    json.dump({hex(k): v for k, v in hits.items()}, open(IDX,'w'))
    print(f'{len(hits)} distinct global addresses referenced -> {IDX}')

ap = argparse.ArgumentParser()
ap.add_argument('addrs', nargs='*')
ap.add_argument('--build', action='store_true')
ap.add_argument('--range')
ap.add_argument('--quiet', action='store_true')
a = ap.parse_args()
if a.build or not os.path.exists(IDX):
    build()
    if a.build and not a.addrs and not a.range: sys.exit()
idx = json.load(open(IDX))
targets = [int(x,16) for x in a.addrs]
if a.range:
    s,_,e = a.range.partition(':')
    targets += list(range(int(s,16), int(e,16)))
for t in targets:
    hs = idx.get(hex(t))
    if not hs:
        if not a.quiet: print(f'{t:#x}: (no references)')
        continue
    print(f'{t:#x}: {len(hs)} refs, funcs ' +
          ' '.join(sorted({hex(h[1]) for h in hs})))
    if not a.quiet:
        for at, fn, txt in hs: print(f'    {at:08x}  {fn:#x}  {txt}')
