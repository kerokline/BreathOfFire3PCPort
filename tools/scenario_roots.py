#!/usr/bin/env python
"""Enumerate the scenario banks from their chapter tables, and measure how much
of the exe they reach.

    python tools/scenario_roots.py                 # the report
    python tools/scenario_roots.py --chapter 16    # one chapter's roots and closure
    python tools/scenario_roots.py --json PATH     # also write the closure

The scenario engine reaches a chapter's code through three tables of 20
pointers each, indexed by the chapter byte Cond_ByteFA (s8 0x8034E0; PSX
0x8014686C), read off the exe here:

    0x662C80[c]  -> the chapter's vtable, 5 slots; the engine calls slot 0
                    every field frame (Field_ModeDispatch 0x56D690), 1 on an
                    object trigger (0x56D6D0), 2 and 3 as the step and arrive
                    hooks (0x56D700 / 0x56D750), 4 from 0x56D7A0.
                    PSX 0x801C944C.
    0x660B84[c]  -> the chapter's call table A (Scenario_CallA 0x5341A0,
                    jmp [[table] + n * 4]).  PSX 0x801CDC4C.
    0x660BD4[c]  -> call table B (0x5341C0).  PSX 0x801CDC9C.

The three are the only reads of Cond_ByteFA that index a table (a byte scan of
.text for `movsx eax, byte [0x8034E0]` followed by `mov reg, [eax*4 + imm]`
finds exactly these three immediates, at seven sites).  Everything a bank
does through the engine starts from one of those pointers, so the closure of
calls from them is the bank as the *scenario engine* sees it.

The walk is a recursive-descent decode of each function within its extent
(to the next start), following `call` / `jmp` rel32 to .text and every
`[imm + reg*4]` pointer table - in .data, .rdata or .text, since MSVC puts a
switch's table right after its function - reading dwords while they are
function starts.  A call target no start list records, or a table entry that
looks like a start (in .text, on a 16-byte boundary or after padding), is
added as a start and the walk re-run to a fixpoint, so the extents split
where the lists were short.

A function is expanded when its catalogue label is one the scenario family
could own (SCENA, COMMU, SHISU, SCE1xEF, PLP, a Scena* table, or Unlabelled),
or symbols.toml names it Scena* - ours or not.  Anything else it calls is
the frontier: engine code the banks lean on, counted by label, not walked.

The catalogue's SCENA label is the PSX side's word for where a twin lived,
and the PSX SCENA overlays held more than what the chapter tables reach:
world-map hooks, battle-side scripts, effects.  The report's last section
says, for every SCENA-labelled function the chapter walk misses, which other
table or subsystem does reach it.
"""
import argparse
import bisect
import collections
import csv
import json
import os
import struct
import sys
import tomllib

import capstone
from capstone import x86

sys.path.insert(0, os.path.dirname(__file__))
from event_scan import Image  # noqa: E402

CHAPTER_BYTE = 0x8034E0
VTABLES, CALL_A, CALL_B = 0x662C80, 0x660B84, 0x660BD4
CHAPTERS = 20
VTABLE_SLOTS = 5
SLOT_USE = ['frame (Field_ModeDispatch)', 'object trigger (0x56D6D0)',
            'step hook', 'arrive hook', 'slot 4 (0x56D7A0)']

FAMILY = ('Scenario event banks', 'Communication minigames', 'Master / apprentice',
          'Scenario effects', 'Party character sets', 'Table Scena')
SCENA = 'Scenario event banks'


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--exe', default='bof3/BOF3.exe')
    ap.add_argument('--funcs', default='analysis/pc_funcs.json')
    ap.add_argument('--hidden', default='analysis/pc_hidden.json')
    ap.add_argument('--catalog', default='analysis/remaining_catalog.tsv')
    ap.add_argument('--symbols', default='symbols.toml')
    ap.add_argument('--xref', default='analysis/pc_xref.json')
    ap.add_argument('--chapter', type=int)
    ap.add_argument('--json')
    a = ap.parse_args()

    im = Image(a.exe)
    text = next(s for s in im.secs if s[0] == '.text')
    text_lo, text_hi = text[1], text[1] + text[2]

    def in_text(x):
        return text_lo <= x < text_hi

    def in_image(x):
        return im.off(x) is not None

    pf = json.load(open(a.funcs))
    funcs = {f['entry']: f for f in pf['functions']}
    hidden = {h['entry']: h for h in json.load(open(a.hidden))}
    listed = set(funcs) | set(hidden)
    starts = sorted(listed)
    start_set = set(starts)
    added = set()               # starts this walk discovered

    def extent(e):
        i = bisect.bisect_right(starts, e)
        return (starts[i] if i < len(starts) else text_hi) - e

    def func_of(pc):
        i = bisect.bisect_right(starts, pc) - 1
        return starts[i] if i >= 0 else None

    label = {}
    with open(a.catalog, encoding='utf-8') as fh:
        for row in csv.DictReader(fh, delimiter='\t'):
            try:
                label[int(row['entry'], 16)] = row['label']
            except ValueError:
                pass
    sym = tomllib.load(open(a.symbols, 'rb'))
    named = {f['pc']: f for f in sym['func']}
    ours = {pc for pc, f in named.items() if 'impl' in f}
    data_names = sorted((d['pc'], d['name'], d.get('count') or 0) for d in sym['data'])
    data_pcs = [d[0] for d in data_names]

    def data_name(addr, near=0x800):
        i = bisect.bisect_right(data_pcs, addr) - 1
        if i >= 0 and addr - data_names[i][0] < near:
            return data_names[i][1]
        return None

    def data_near(addr):
        i = bisect.bisect_right(data_pcs, addr) - 1
        return data_names[i][1] if i >= 0 else f'0x{addr:X}'

    def name_of(e):
        return named.get(e, {}).get('name', '')

    def label_of(e):
        if e in ours:
            return 'ours: ' + name_of(e)
        if e in added:
            return 'new start'
        return label.get(e) or (name_of(e) or 'Unlabelled')

    def expandable(e):
        if name_of(e).startswith('Scena'):
            return True
        if e in ours:
            return False
        lb = label_of(e)
        return lb.startswith(FAMILY) or lb in ('Unlabelled', 'new start')

    def looks_like_start(x):
        if not in_text(x):
            return False
        if x in start_set:
            return True
        return x % 16 == 0 or im.byte(x - 1) in (0x90, 0xCC)

    # --- the roots -----------------------------------------------------
    def read_table(base, stop_at):
        out, x = [], base
        if not in_image(base):
            return out
        while x < stop_at and len(out) < 64:
            v = im.u32(x)
            if not in_text(v):
                break
            out.append(v)
            x += 4
        return out

    table_ptrs = [im.u32(t + 4 * c) for t in (VTABLES, CALL_A, CALL_B) for c in range(CHAPTERS)]
    bounds = sorted({x for x in table_ptrs if in_image(x)} | set(data_pcs))

    def bound_after(x):
        i = bisect.bisect_right(bounds, x)
        return bounds[i] if i < len(bounds) else x + 256

    roots = {}
    for c in range(CHAPTERS):
        vt, ta, tb = (im.u32(t + 4 * c) for t in (VTABLES, CALL_A, CALL_B))
        slots = [im.u32(vt + 4 * k) if in_image(vt) else 0 for k in range(VTABLE_SLOTS)]
        roots[c] = {'vtable': vt, 'slots': [s if in_text(s) else None for s in slots],
                    'callA_table': ta, 'callA': read_table(ta, bound_after(ta)),
                    'callB_table': tb, 'callB': read_table(tb, bound_after(tb))}
    all_seeds = set()
    for r in roots.values():
        all_seeds |= {s for s in r['slots'] if s} | set(r['callA']) | set(r['callB'])

    # --- the decode ----------------------------------------------------
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    edge_cache = {}
    decoded_bytes = {}
    END = {x86.X86_INS_RET, x86.X86_INS_RETF, x86.X86_INS_HLT, x86.X86_INS_INT3}

    def walk_table(base, lo, hi, blocks, targets):
        """dwords at base: in-function ones are blocks, other starts targets.

        A table in .text is a compiler switch: an entry outside the function
        is a start only if it looks like one (a shared tail is not).  A table
        in .data / .rdata is a hand-written pointer table: every entry is a
        function, aligned or not."""
        x, n = base, 0
        data_table = not in_text(base)
        while n < 512 and in_image(x):
            v = im.u32(x)
            if not in_text(v):
                break
            if v in start_set and not lo <= v < hi:
                targets.add(v)
            elif data_table:
                targets.add(v)
            else:
                blocks.add(v)      # a switch case of this function, wherever it lies
            x += 4
            n += 1
        return n

    def edges(e):
        if e in edge_cache:
            return edge_cache[e]
        lo, hi = e, e + extent(e)
        calls, tables, imms, newstarts = set(), set(), set(), set()
        seen_blocks, blocks = set(), {e}
        covered = set()
        table_bytes = set()
        while blocks:
            pc = blocks.pop()
            if pc in seen_blocks or not in_text(pc) or pc in table_bytes:
                continue
            seen_blocks.add(pc)
            code = im.bytes(pc, min(hi - pc if pc < hi else 0x2000, text_hi - pc))
            for ins in md.disasm(code, pc):
                if ins.address in table_bytes:
                    break
                covered.update(range(ins.address, ins.address + ins.size))
                if ins.id in END:
                    break
                if ins.id in (x86.X86_INS_CALL, x86.X86_INS_JMP) or ins.group(capstone.CS_GRP_JUMP):
                    op = ins.operands[0]
                    if op.type == x86.X86_OP_IMM:
                        t = op.imm
                        if not in_text(t):
                            pass
                        elif ins.id == x86.X86_INS_CALL:
                            if t not in start_set:
                                newstarts.add(t)      # a direct call is evidence of a start
                            calls.add(t)
                        elif t in start_set and not lo <= t < hi:
                            calls.add(t)              # a tail call
                        else:
                            blocks.add(t)             # a branch of this function, wherever it lies
                    elif op.type == x86.X86_OP_MEM and op.mem.scale == 4 and op.mem.base == 0 \
                            and in_image(op.mem.disp):
                        n = walk_table(op.mem.disp, lo, hi, blocks, tables)
                        if in_text(op.mem.disp):
                            table_bytes.update(range(op.mem.disp, op.mem.disp + 4 * n))
                    if ins.id == x86.X86_INS_JMP:
                        break
                    if ins.id == x86.X86_INS_CALL:
                        continue
                    continue
                for op in ins.operands:
                    if op.type == x86.X86_OP_MEM and op.mem.scale == 4 and op.mem.base == 0 \
                            and in_image(op.mem.disp):
                        n = walk_table(op.mem.disp, lo, hi, blocks, tables)
                        if in_text(op.mem.disp):
                            table_bytes.update(range(op.mem.disp, op.mem.disp + 4 * n))
                    elif op.type == x86.X86_OP_IMM and in_text(op.imm) and op.imm != e:
                        if op.imm in start_set:
                            imms.add(op.imm)
                        elif in_image(op.imm) and op.imm % 4 == 0:
                            # a table address in .text handed to a register
                            walk_table(op.imm, lo, hi, blocks, tables)
        newstarts |= {t for t in tables if t not in start_set}
        decoded_bytes[e] = covered
        edge_cache[e] = (calls, tables, imms, newstarts)
        return edge_cache[e]

    def closure(seeds, follow_imms):
        seen, frontier, queue, via, new = set(), collections.Counter(), list(seeds), {}, set()
        while queue:
            e = queue.pop()
            if e in seen:
                continue
            seen.add(e)
            if not expandable(e):
                frontier[label_of(e)] += 1
                continue
            calls, tables, imms, newstarts = edges(e)
            new |= newstarts
            for t in calls | tables | (imms if follow_imms else set()):
                via.setdefault(t, e)
                if t not in seen:
                    queue.append(t)
        return seen, frontier, via, new

    # fixpoint over the start list
    for it in range(8):
        seen, frontier, via, new = closure(all_seeds, True)
        new -= start_set
        if not new:
            break
        added |= new
        start_set |= new
        starts = sorted(start_set)
        edge_cache.clear()
    walked = {e for e in seen if expandable(e)}
    seen_c, frontier_c, via_c, _ = closure(all_seeds, False)
    walked_c = {e for e in seen_c if expandable(e)}
    per_chapter = {}
    for c, r in roots.items():
        seeds = {s for s in r['slots'] if s} | set(r['callA']) | set(r['callB'])
        s, f, _, _ = closure(seeds, True)
        per_chapter[c] = {'seeds': seeds, 'walked': {e for e in s if expandable(e)}, 'frontier': f}

    def bytes_of(s):
        return sum(extent(e) for e in s)

    def decoded_of(s):
        """bytes of .text the decode of these functions covered, as a union."""
        cov = set()
        for e in s:
            if e not in decoded_bytes and expandable(e):
                edges(e)
            cov |= decoded_bytes.get(e, set())
        return len(cov)

    def by_label(s):
        return collections.Counter(label_of(e) for e in s)

    # --- one chapter ---------------------------------------------------
    if a.chapter is not None:
        c, r = a.chapter, roots[a.chapter]
        print(f'chapter {c}: vtable 0x{r["vtable"]:X}')
        for k, s in enumerate(r['slots']):
            print(f'  slot {k} 0x{s:X}  {label_of(s)}  {name_of(s)}  - {SLOT_USE[k]}' if s else f'  slot {k} -')
        for kind in ('callA', 'callB'):
            print(f'  {kind} table 0x{r[kind + "_table"]:X}: {len(r[kind])} entries')
            for i, s in enumerate(r[kind]):
                print(f'    [{i}] 0x{s:X}  {label_of(s)}  {name_of(s)}')
        pc = per_chapter[c]
        print(f'  closure: {len(pc["walked"])} functions walked, {bytes_of(pc["walked"])} bytes; '
              f'frontier {sum(pc["frontier"].values())}')
        for e in sorted(pc['walked']):
            print(f'    0x{e:X}  {extent(e):5d} {decoded_of({e}):5d}  {label_of(e)}  {name_of(e)}')
        return

    # --- the report ----------------------------------------------------
    print(f'{len(all_seeds)} distinct roots from the three tables '
          f'({CHAPTERS} chapters x 5 vtable slots + call tables A and B); '
          f'{len(added)} starts added that no list had ({it} passes to the fixpoint)')
    print()
    print('chapter  vtable    slots  A   B  | closure (this chapter alone)  extent   covered')
    for c in range(CHAPTERS):
        r, pc = roots[c], per_chapter[c]
        ns = sum(1 for s in r['slots'] if s)
        print(f'{c:7d}  0x{r["vtable"]:X}  {ns:5d}  {len(r["callA"]):2d}  {len(r["callB"]):2d}  | '
              f'{len(pc["walked"]):4d} functions  {bytes_of(pc["walked"]):7d}  {decoded_of(pc["walked"]):7d}')
    print()
    print('union over all chapters:')
    print(f'  {len(walked)} functions walked, {bytes_of(walked)} bytes of extent, {decoded_of(walked)} of .text covered by the decode '
          f'({sum(1 for e in walked if e in hidden)} hidden starts, {sum(1 for e in walked if e in added)} new, '
          f'{sum(1 for e in walked if e in ours)} already ours)')
    for lb, n in by_label(walked).most_common():
        if not lb.startswith('ours'):
            print(f'    {n:5d}  {lb}')
    print(f'  frontier (reached, not walked): {sum(frontier.values())} functions')
    fr = collections.Counter()
    for lb, n in frontier.items():
        fr['ours (engine)' if lb.startswith('ours') else lb] += n
    for lb, n in fr.most_common(12):
        print(f'    {n:5d}  {lb}')
    print(f'  without pushed function pointers: {len(walked_c)} functions, {bytes_of(walked_c)} bytes')
    print()

    # coverage of the catalogue's scenario family
    fam = {e for e, lb in label.items() if lb.startswith(FAMILY)}
    fam_lab = collections.defaultdict(set)
    for e in fam:
        fam_lab[label[e]].add(e)
    print("the catalogue's scenario family, and how much of it the chapter tables reach:")
    for lb in sorted(fam_lab):
        s = fam_lab[lb]
        print(f'  {lb:45s} {len(s):4d}  reached {len(s & walked):4d}')
    hint_lo, hint_hi = 0x53D3B0, 0x56AD79
    hinted = {e for e in listed if hint_lo <= e <= hint_hi and label_of(e) == 'Unlabelled'}
    print(f'  unlabelled starts in the SCENA-shaped runs 0x{hint_lo:X}..0x{hint_hi:X}: {len(hinted)}, '
          f'reached {len(hinted & walked)}')
    unl = {e for e in walked if label_of(e) == 'Unlabelled'}
    print(f'  unlabelled functions the walk reaches anywhere: {len(unl)}, {len(unl - hinted)} outside those runs')
    print()

    # what reaches the SCENA-labelled functions the chapter tables do not
    scena_all = {e for e, lb in label.items() if lb.startswith(SCENA)}
    miss = sorted(e for e in scena_all if e not in walked)
    # direct callers, from a rel32 scan of .text
    seg = im.data[text[3]:text[3] + text[2]]
    callers = collections.defaultdict(set)
    miss_set = set(miss)
    for i in range(len(seg) - 4):
        b = seg[i]
        if b == 0xE8 or b == 0xE9:
            t = (text_lo + i + 5 + struct.unpack_from('<i', seg, i + 1)[0]) & 0xFFFFFFFF
            if t in miss_set:
                callers[t].add(func_of(text_lo + i))
    how = collections.Counter()
    detail = collections.defaultdict(list)
    for e in miss:
        keys = set()
        for _, addr in hidden.get(e, {}).get('refs', []):
            dn = data_name(addr)
            if dn:
                keys.add('table ' + dn)
            elif in_text(addr):
                f = func_of(addr)
                keys.add('jump table in ' + (label_of(f) if f not in miss_set else 'another missed SCENA function'))
            else:
                keys.add(f'unnamed data past {data_near(addr)}')
        for c in callers.get(e, ()):
            keys.add('called by ' + (label_of(c) if c not in miss_set else 'another missed SCENA function'))
        if not keys:
            keys.add('no reference found')
        k = ' / '.join(sorted(keys))
        how[k] += 1
        detail[k].append(e)
    print(f'{len(miss)} SCENA-labelled functions the chapter tables do not reach, by what does:')
    for k, n in how.most_common():
        eg = ', '.join(f'0x{e:X}' for e in detail[k][:3])
        print(f'  {n:4d}  {k}  (e.g. {eg})')

    if a.json:
        out = {'roots': {c: {'vtable': hex(r['vtable']),
                             'slots': [hex(s) if s else None for s in r['slots']],
                             'callA': [hex(x) for x in r['callA']],
                             'callB': [hex(x) for x in r['callB']]} for c, r in roots.items()},
               'added_starts': sorted(hex(e) for e in added),
               'walked': sorted(hex(e) for e in walked),
               'walked_calls_only': sorted(hex(e) for e in walked_c),
               'via': {hex(k): hex(v) for k, v in via.items()},
               'frontier': dict(frontier),
               'frontier_functions': {hex(e): label_of(e) for e in seen if not expandable(e)},
               'per_chapter': {c: sorted(hex(e) for e in pc['walked']) for c, pc in per_chapter.items()},
               'scena_missed': {hex(e): k for k, es in detail.items() for e in es}}
        json.dump(out, open(a.json, 'w'), indent=1)
        print(f'-> {a.json}')


if __name__ == '__main__':
    main()
