#!/usr/bin/env python
"""Function inventory for the Chinese PC port's BOF3.exe.

Linear-sweeps `.text` with capstone, recovers function entries from direct call
targets, and emits one record per function with architecture-neutral features
for the PSX<->PC matcher (docs/PLAN.md section 3).

    python tools/pe_funcs.py --exe bof3/BOF3.exe --out analysis/pc_funcs.json

Output is derived from copyrighted game code: it lives under analysis/ and is
never committed (CLAUDE.md rule 1).
"""
import argparse, json, struct, sys, collections
import capstone

def parse_pe(data):
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    assert data[pe:pe+4] == b'PE\0\0', 'not a PE'
    nsec, = struct.unpack_from('<H', data, pe + 6)
    optsz, = struct.unpack_from('<H', data, pe + 20)
    opt = pe + 24
    entry, = struct.unpack_from('<I', data, opt + 16)
    base, = struct.unpack_from('<I', data, opt + 28)
    secs = []
    off = opt + optsz
    for i in range(nsec):
        s = off + i * 40
        name = data[s:s+8].rstrip(b'\0').decode('latin1')
        vsz, va, rsz, raw = struct.unpack_from('<IIII', data, s + 8)
        secs.append(dict(name=name, va=base + va, vsz=vsz, raw=raw, rsz=rsz))
    return base, base + entry, secs

# Instructions whose immediate is a code address, not a value.
BRANCHY = {'call', 'jmp'} | {'j' + c for c in
    ('o','no','b','ae','e','ne','be','a','s','ns','p','np','l','ge','le','g',
     'z','nz','c','nc','cxz','ecxz')}
X87 = tuple('f')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default='bof3/BOF3.exe')
    ap.add_argument('--out', default='analysis/pc_funcs.json')
    a = ap.parse_args()

    data = open(a.exe, 'rb').read()
    base, entry, secs = parse_pe(data)
    text = next(s for s in secs if s['name'] == '.text')
    lo, hi = text['va'], text['va'] + text['vsz']
    code = data[text['raw']:text['raw'] + text['rsz']]

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    insns = {}
    order = []
    pos = 0
    stray = 0
    while pos < len(code):
        got = False
        for ins in md.disasm(code[pos:], lo + pos):
            insns[ins.address] = ins
            order.append(ins.address)
            pos = ins.address - lo + ins.size
            got = True
        if not got:
            stray += 1
            pos += 1
    order.sort()

    # --- entries: direct call targets, plus the PE entry point ---
    entries = set([entry])
    calltargets = collections.Counter()
    for addr in order:
        ins = insns[addr]
        if ins.mnemonic == 'call' and len(ins.operands) == 1:
            op = ins.operands[0]
            if op.type == capstone.x86.X86_OP_IMM and lo <= op.imm < hi:
                entries.add(op.imm)
                calltargets[op.imm] += 1
    entries = sorted(e for e in entries if e in insns)

    # --- bodies: [entry, next entry), trailing int3/nop padding trimmed ---
    import bisect
    funcs = []
    for i, e in enumerate(entries):
        end = entries[i+1] if i + 1 < len(entries) else hi
        lo_i = bisect.bisect_left(order, e)
        hi_i = bisect.bisect_left(order, end)
        body = order[lo_i:hi_i]
        while body and insns[body[-1]].mnemonic in ('int3', 'nop'):
            body.pop()
        if not body:
            continue
        f = dict(entry=e, size=(body[-1] + insns[body[-1]].size - e),
                 insns=len(body), callees=[], imms=[], offs=[], globals_=[],
                 loads=0, stores=0, fp=0, icall=0, ijmp=0, rets=0,
                 callers=calltargets.get(e, 0))
        cal = set()
        for ad in body:
            ins = insns[ad]
            m = ins.mnemonic
            if m.startswith('f') and m not in ('fs',):
                f['fp'] += 1
            if m == 'ret' or m == 'retf':
                f['rets'] += 1
            ops = ins.operands
            if m == 'call':
                if ops and ops[0].type == capstone.x86.X86_OP_IMM:
                    t = ops[0].imm
                    if lo <= t < hi:
                        cal.add(t)
                else:
                    f['icall'] += 1
            elif m == 'jmp' and ops and ops[0].type != capstone.x86.X86_OP_IMM:
                f['ijmp'] += 1
            for op in ops:
                if op.type == capstone.x86.X86_OP_IMM and m not in BRANCHY:
                    f['imms'].append(op.imm & 0xFFFFFFFF)
                elif op.type == capstone.x86.X86_OP_MEM:
                    d = op.mem.disp
                    if op.mem.base == 0 and op.mem.index == 0:
                        f['globals_'].append(d & 0xFFFFFFFF)
                    elif d:
                        f['offs'].append(d)
                    if ins.mnemonic in ('mov','movzx','movsx','lea'):
                        pass
            # crude load/store tally: memory operand read vs written
            if ops and any(o.type == capstone.x86.X86_OP_MEM for o in ops):
                if ops[0].type == capstone.x86.X86_OP_MEM and m not in ('cmp','test','push'):
                    f['stores'] += 1
                else:
                    f['loads'] += 1
        f['callees'] = sorted(cal)
        funcs.append(f)

    out = dict(exe=a.exe, image_base=hex(base), entry=hex(entry),
               text=dict(va=hex(lo), size=text['vsz']),
               total_insns=len(order), stray_bytes=stray,
               call_targets=len(calltargets), function_count=len(funcs),
               functions=funcs)
    json.dump(out, open(a.out, 'w'))
    print(f"{len(order)} insns, {stray} stray bytes, "
          f"{len(calltargets)} distinct call targets, {len(funcs)} functions")
    sizes = sorted(f['insns'] for f in funcs)
    print(f"insns/func: min {sizes[0]} med {sizes[len(sizes)//2]} "
          f"p90 {sizes[int(len(sizes)*.9)]} max {sizes[-1]}")
    print('->', a.out)

main()
