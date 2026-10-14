#!/usr/bin/env python
"""Function inventory for the Chinese PC port's BOF3.exe.

Linear-sweeps `.text` with capstone, recovers function entries from direct call
targets, and emits one record per function with architecture-neutral features
for the PSX<->PC matcher (docs/PLAN.md section 3).

    python tools/pe_funcs.py --exe bof3/BOF3.exe --out analysis/pc_funcs.json

Output is derived from copyrighted game code: it lives under analysis/ and is
never committed (CLAUDE.md rule 1).
"""
import argparse, bisect, json, struct, sys, collections
import capstone

from pe_read import parse_pe   # the project's PE parser, capstone-free

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
    pos = 0
    # capstone's disasm ends, without a word, at the first byte it cannot
    # decode. The sweep starts again one byte on, and every such byte is
    # recorded by address (docs/hidden-start-scan.md section 1).
    stray_at = []
    while pos < len(code):
        got = False
        for ins in md.disasm(code[pos:], lo + pos):
            insns[ins.address] = ins
            pos = ins.address - lo + ins.size
            got = True
        if not got:
            stray_at.append(lo + pos)
            pos += 1
    stray = len(stray_at)

    # --- direct call targets, re-anchored ---
    # A linear sweep that falls out of step (after an inline jump table) can
    # decode ACROSS a call target, which then is no instruction boundary and
    # was dropped from the entries - Item_UseFlags 0x591810, 18 callers
    # (hidden-start-scan.md 4.1). Each such target is decoded from itself
    # until the new instructions meet a boundary of the sweep again; the
    # sweep's instructions that overlap them are replaced. New call targets
    # in the re-read code go round again.
    #
    # Only a credible call re-anchors: the sweep also decodes `call`s out of
    # data, and one such (0x420324, inside an inline table) aims at
    # 0x42452B, the middle of a real instruction; re-anchoring there would
    # break good code. A target is taken when it is 16-aligned (where this
    # compiler starts every function of the game's code), or when control flow
    # from the entry before one of its calls (jumps and branches followed, a
    # ret / indirect jmp / int3 ending a path) reaches that call - the
    # runtime's hand-written code is not aligned. Flow alone is not enough: a
    # call in a case of a jump table is reached by no direct flow. The rest
    # are listed, not taken.
    def call_targets():
        out, sites = collections.Counter(), collections.defaultdict(list)
        for ins in insns.values():
            if ins.mnemonic == 'call' and len(ins.operands) == 1:
                op = ins.operands[0]
                if op.type == capstone.x86.X86_OP_IMM and lo <= op.imm < hi:
                    out[op.imm] += 1
                    sites[op.imm].append(ins.address)
        return out, sites

    def flow_reaches(start, stop, site):
        """True when control flow from `start`, kept inside [start, stop),
        decodes an instruction at `site`."""
        seen, work = set(), [start]
        while work:
            p = work.pop()
            while start <= p < stop and p not in seen:
                seen.add(p)
                ins = next(md.disasm(code[p - lo:p - lo + 16], p), None)
                if ins is None:
                    break
                if p == site:
                    return True
                m, ops = ins.mnemonic, ins.operands
                if m in ('ret', 'retf', 'int3', 'hlt'):
                    break
                if m in BRANCHY and m != 'call':
                    if ops and ops[0].type == capstone.x86.X86_OP_IMM:
                        work.append(ops[0].imm)
                    if m == 'jmp':
                        break
                p += ins.size
        return False

    reanchored, unanchored, incredible = [], [], []
    while True:
        calltargets, sites = call_targets()
        todo = sorted(t for t in set(calltargets) | {entry}
                      if t not in insns and t not in unanchored and t not in {r['target'] for r in incredible})
        if not todo:
            break
        known = sorted(e for e in set(calltargets) | {entry} if e in insns)
        for t in todo:
            if t in insns:                       # an earlier re-anchor this round reached it
                continue
            if t != entry and t % 16:
                ok = False
                for s in sites[t]:
                    i = bisect.bisect_right(known, s) - 1
                    if i >= 0 and flow_reaches(known[i], known[i + 1] if i + 1 < len(known) else hi, s):
                        ok = True
                        break
                if not ok:
                    incredible.append(dict(target=t, sites=sites[t]))
                    continue
            new = []
            for ins in md.disasm(code[t - lo:], t):
                if new and ins.address in insns:
                    break                         # back in step with the sweep
                new.append(ins)
            if not new:
                unanchored.append(t)
                continue
            end = new[-1].address + new[-1].size
            # the sweep's instructions overlapping [t, end), one that starts
            # before t and runs into it included (x86: at most 15 bytes)
            dropped = [ad for ad in range(max(lo, t - 15), end) if ad in insns and ad + insns[ad].size > t]
            for ad in dropped:
                del insns[ad]
            for ins in new:
                insns[ins.address] = ins
            reanchored.append(dict(target=t, resync=end, insns=len(new), replaced=len(dropped),
                                   callers=calltargets.get(t, 0)))
    order = sorted(insns)

    # --- entries: direct call targets, plus the PE entry point ---
    entries = set(calltargets) | {entry}
    entries = sorted(e for e in entries if e in insns)

    # --- bodies: [entry, next entry), trailing int3/nop padding trimmed ---
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
               stray_at=[hex(s) for s in stray_at], reanchored=reanchored,
               unanchored=[hex(t) for t in unanchored],
               not_reanchored=[dict(target=hex(r['target']), sites=[hex(s) for s in r['sites']]) for r in incredible],
               functions=funcs)
    json.dump(out, open(a.out, 'w'))
    print(f"{len(order)} insns, {stray} stray bytes, "
          f"{len(calltargets)} distinct call targets, {len(funcs)} functions")
    runs = []
    for s in stray_at:
        if runs and runs[-1][1] == s:
            runs[-1][1] = s + 1
        else:
            runs.append([s, s + 1])
    print(f"undecodable: {stray} bytes in {len(runs)} runs, the sweep resumed past each: " +
          ', '.join(f'0x{s:X}' + (f'+{e - s}' if e - s > 1 else '') for s, e in runs[:12]) +
          (f', ... ({len(runs) - 12} more in the JSON\'s stray_at)' if len(runs) > 12 else ''))
    print(f"re-anchored: {len(reanchored)} call targets the sweep had decoded across: " +
          ', '.join(f"0x{r['target']:X} ({r['callers']} calls, in step at 0x{r['resync']:X})" for r in reanchored[:12]) +
          (f', ... ({len(reanchored) - 12} more)' if len(reanchored) > 12 else ''))
    if incredible:
        print(f"not re-anchored, not 16-aligned and no flow from the entry before reaches a call: " +
              ', '.join(f"0x{r['target']:X} (from {', '.join(f'0x{s:X}' for s in r['sites'])})" for r in incredible))
    if unanchored:
        print(f"NOT DECODABLE at a call target, so no entry: {', '.join(f'0x{t:X}' for t in unanchored)}")
    sizes = sorted(f['insns'] for f in funcs)
    print(f"insns/func: min {sizes[0]} med {sizes[len(sizes)//2]} "
          f"p90 {sizes[int(len(sizes)*.9)]} max {sizes[-1]}")
    print('->', a.out)

main()
