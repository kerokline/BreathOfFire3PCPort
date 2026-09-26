#!/usr/bin/env python
"""Every row of the effect table Magic_Rows, and the PC extent of the overlay
behind it.

    python tools/magic_rows.py                 -> analysis/magic_rows.tsv
                                                  analysis/magic_funcs.tsv
                                                  and a summary on stdout
    python tools/magic_rows.py --unit MAGIC070 -> one overlay's functions
    python tools/magic_rows.py --unit MAGIC070 --clones
                                               -> its clone table for the spell
                                                  harness (docs/magic_harness.md)

Magic_Rows (0x64C2B8, docs/battle_fx_tasks.md section 2) is 151 rows of a u16
DAT file id and a code pointer, the kind-2 battle task's handler. The five rows
with file 0xFFFF are engine-side; the rest are the PSX BMAGIC overlays compiled
into the exe (the PC's file id is the PSX's plus 0x105, docs/cut-content.md
section 2). For every row this prints the file, the overlay's name (the
sibling's analysis/file_ids.json), the PC entry, the abilities that load the
row (the sibling's names/magic.toml) with their names **read one id down**
(cut-content section 2: the name of ability id N is magic.toml's label for
N - 1), and the overlay's PC extent.

**The extent method** (docs/takeover-queue-round9-spells.md section 1 has the
measurements):

1. The function starts are pe_funcs.py's (analysis/pc_funcs.json) and
   pe_hidden.py's (analysis/pc_hidden.json) inside the magic band
   0x498FE0..0x4FC6A0 - after the engine's level-up routine 0x498DE0 and
   before Input_Latch 0x4FC6A0 - corrected by a recursive descent of each
   one: a start that is a case of another function's jump table is not a
   start (three are), and code no start covers - after a jump table, where
   pe_hidden.py's rule wants a ret, or reached only by a tail jmp - is a
   function of its own (42 are).
2. Each overlay's first function is its lowest entry: in 126 of 127
   overlays the lowest Magic_Rows entry of the file is the first function of
   its code (the one exception, MAGIC019, sits inside MAGIC018 and the two are
   one unit here). An extent runs from that entry to the next overlay's.
3. The check: every function is reached from its file's entries (calls, jmp,
   code immediates - the stack tables -, .data tables read in place, and
   BattleTask_Create(1 or 3, parameter) through the kinds' stack tables),
   and a function reached from one file only must sit in that file's extent.
   The exceptions are printed (one: the bare ret every empty function was
   folded into). The effect library at 0x4FAFF0, after MAGIC226/227, is a
   unit of its own (0x4FAF90 before it is MAGIC226/227's pool allocator,
   docs/magic_lib.md section 1); code reached from no entry whose address a table of
   another subsystem holds (MapCell_Handlers') is not counted.
4. A file whose lowest entry lies inside another file's extent and is
   reached from that file too (MAGIC016 inside MAGIC015; MAGIC005, 029,
   049, 133..136, 156, 157 on MAGIC004's entry) has no code of its own: the
   linker folded it whole into the other.

Sizes are a function's own bytes - its recursive descent, jump tables
included, to its last instruction - not the padding after it.

Reads bof3/BOF3.exe (--exe), analysis/ (--analysis, the main checkout's when
run from a worktree), symbols.toml, and the sibling checkout (--sibling). The
output lists addresses and sizes only, but it is derived from copyrighted game
code: it lives under analysis/ and is never committed (CLAUDE.md rule 1).
"""
import argparse, bisect, collections, json, os, struct, sys, tomllib

import capstone
from capstone import x86

ROWS_AT = 0x64C2B8
ROW_COUNT = 151
ENGINE_FILE = 0xFFFF
PSX_FILE_DELTA = 0x105
BAND_LO = 0x498FE0          # MAGIC001's entry; 0x498DE0 before it is the engine's level-up
BAND_HI = 0x4FC6A0          # Input_Latch, the engine again
TASK_CREATE = 0x435180      # BattleTask_Create(kind, parameter)
# BattleTask_RunAll's kinds with a stack table by parameter: kind 1 is
# BattleMagicFx_Dispatch (110 entries), kind 3 is 0x4357D0 (8 entries).
KIND_DISPATCH = {1: (0x435350, 0x476), 3: (0x4357D0, 0x56)}
LIBRARY_LO = 0x4FAFF0       # the effect library after MAGIC226/227 (docs/magic_lib.md section 1: 0x4FAF90 is theirs)
LIBRARY_NAME = 'LIBRARY'


class Image:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        d = self.data
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        nsec, = struct.unpack_from('<H', d, pe + 6)
        optsz, = struct.unpack_from('<H', d, pe + 20)
        opt = pe + 24
        self.base, = struct.unpack_from('<I', d, opt + 28)
        self.secs = []
        for i in range(nsec):
            s = opt + optsz + i * 40
            name = d[s:s + 8].rstrip(b'\0').decode('latin1')
            vsz, va, rsz, raw = struct.unpack_from('<IIII', d, s + 8)
            self.secs.append((name, self.base + va, max(vsz, rsz), raw, rsz))
        t = next(s for s in self.secs if s[0] == '.text')
        self.text_lo, self.text_hi = t[1], t[1] + t[4]

    def off(self, va):
        for _, v, _, raw, rsz in self.secs:
            if v <= va < v + rsz:
                return raw + va - v
        return None

    def u32(self, va):
        o = self.off(va)
        return None if o is None else struct.unpack_from('<I', self.data, o)[0]

    def u8(self, va):
        o = self.off(va)
        return None if o is None else self.data[o]

    def in_text(self, va):
        return self.text_lo <= va < self.text_hi


MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
MD.detail = True


def decode(img, va, limit=64):
    o = img.off(va)
    return next(MD.disasm(img.data[o:o + 16], va), None) if o is not None else None


def descend(img, start, known):
    """Recursive descent of one function. Returns (end, targets, false_starts,
    code_refs, data_refs, creates): end is one past its last byte (jump tables
    included); false_starts are known starts reached as internal targets."""
    seen = set()
    work = [start]
    end = start
    false_starts = set()
    out = set()        # code addresses it names: calls, tail jmps, immediates
    drefs = set()      # .data displacements it reads or writes
    tables = set()     # .data tables it indexes (read in place)
    creates = []       # BattleTask_Create(1, parameter) immediates
    tail_starts = set()
    unknown_creates = 0
    while work:
        pc = work.pop()
        pushes = []
        prev = []
        while pc not in seen:
            ins = decode(img, pc)
            if ins is None:
                break
            m = ins.mnemonic
            if m in ('nop', 'int3'):
                break   # padding is not the function's
            seen.add(pc)
            end = max(end, pc + ins.size)
            ops = ins.operands
            for op in ops:
                if op.type == x86.X86_OP_MEM:
                    d = op.mem.disp & 0xFFFFFFFF
                    if img.off(d) is not None and not img.in_text(d):
                        drefs.add(d)
                        if op.mem.index != 0:
                            tables.add(d)
                elif op.type == x86.X86_OP_IMM and m not in ('call', 'jmp') and not m.startswith('j'):
                    v = op.imm & 0xFFFFFFFF
                    if img.in_text(v):
                        out.add(v)
                    elif img.off(v) is not None and v >= 0x5C0000:
                        tables.add(v)   # a .data table's address taken into a register
            if m == 'push':
                pushes.append(ops[0].imm & 0xFFFFFFFF if ops[0].type == x86.X86_OP_IMM else None)
            if m == 'call':
                if ops[0].type == x86.X86_OP_IMM:
                    t = ops[0].imm & 0xFFFFFFFF
                    out.add(t)
                    if t == TASK_CREATE and len(pushes) >= 2 and pushes[-1] in KIND_DISPATCH:
                        if pushes[-2] is None:
                            unknown_creates += 1
                        else:
                            creates.append((pushes[-1], pushes[-2]))
                pushes = []
            elif m in ('ret', 'retf', 'hlt'):
                break
            elif m == 'jmp':
                op = ops[0]
                if op.type == x86.X86_OP_IMM:
                    t = op.imm & 0xFFFFFFFF
                    if t != start and t % 16 == 0 and t not in known and BAND_LO <= t < BAND_HI                             and (img.u8(t - 1) == 0xC3 or (img.u8(t - 1) in (0x90, 0xCC)
                                                             and img.u8(t - 2) in (0x90, 0xCC, 0xC3))):
                        # a tail jmp to a function no list has: on a boundary, after padding or a ret
                        out.add(t)
                        tail_starts.add(t)
                    elif t == start or (t > start and t not in known and t < start + 0x4000 and t < BAND_HI):
                        work.append(t)
                    elif start < t < start + 0x4000 and t in known and _inside(t, start, pc):
                        # a known start branched to from before it and inside our run: a case
                        work.append(t)
                        false_starts.add(t)
                    else:
                        out.add(t)
                    break
                if op.type == x86.X86_OP_MEM and op.mem.index != 0 and img.in_text(op.mem.disp & 0xFFFFFFFF):
                    t = op.mem.disp & 0xFFFFFFFF
                    n = 0
                    while True:
                        w = img.u32(t + 4 * n)
                        if w is None or not (start <= w < t):
                            break
                        work.append(w)
                        if w in known and w != start:
                            false_starts.add(w)
                        n += 1
                    end = max(end, t + 4 * n)
                    # MSVC's two-level switch: movzx r, byte [r + T2] before it
                    for p in prev[-3:]:
                        if p.mnemonic in ('mov', 'movzx') and len(p.operands) == 2                                 and p.operands[1].type == x86.X86_OP_MEM and p.operands[1].size == 1:
                            t2 = p.operands[1].mem.disp & 0xFFFFFFFF
                            if img.in_text(t2):
                                # the byte table runs to the bound its cmp tests
                                bound = _cmp_bound(prev)
                                k = bound + 1 if bound is not None and bound < 0x400 else 0
                                end = max(end, t2 + k)
                    break
                out.add(('indirect', pc))
                break
            elif m.startswith('j') or m.startswith('loop'):
                t = ops[0].imm & 0xFFFFFFFF
                work.append(t)
                if t in known and t != start:
                    false_starts.add(t)
            prev.append(ins)
            prev = prev[-6:]
            pc += ins.size
            if pc in known and pc != start and pc not in seen:
                # fell through into a known start: it is part of this one
                false_starts.add(pc)
    for t in tables | drefs:
        # the code pointers stored there, while the dwords are .text addresses
        for i in range(256):
            w = img.u32(t + 4 * i)
            if w is None or not img.in_text(w):
                break
            if BAND_LO <= w < BAND_HI:
                out.add(w)
    return dict(end=end, out=out, drefs=drefs, creates=creates, unknown_creates=unknown_creates,
                false_starts=false_starts, tail_starts=tail_starts)


def _inside(t, start, pc):
    return start < t <= pc


def _cmp_bound(prev):
    for p in reversed(prev):
        if p.mnemonic == 'cmp' and len(p.operands) == 2 and p.operands[1].type == x86.X86_OP_IMM:
            return p.operands[1].imm
    return None


def padding_end(img, va, hi):
    while va < hi and img.u8(va) in (0x90, 0xCC):
        va += 1
    return va


def discover(img, starts):
    """Descend every start in the band; drop the false ones; add the code a
    start does not cover (after a jump table, or a function reached only
    by a tail jmp). Returns {start: descent}."""
    found_by_tail = set()
    known = set(starts)
    funcs = {}
    todo = sorted(known)
    while todo:
        a = todo.pop(0)
        if a not in known or a in funcs:
            continue
        d = descend(img, a, known)
        funcs[a] = d
        for f in d['false_starts']:
            if f in known:
                known.discard(f)
                funcs.pop(f, None)
                d2 = descend(img, a, known)   # again, with the case inside
                funcs[a] = d = d2
        for t in d['tail_starts']:
            if t not in known:
                known.add(t)
                todo.append(t)
                found_by_tail.add(t)
    for t in found_by_tail:
        if t in funcs:
            funcs[t]['found'] = True
    # uncovered code after a function's end and before the next start
    changed = True
    while changed:
        changed = False
        ordered = sorted(funcs)
        for i, a in enumerate(ordered):
            nxt = ordered[i + 1] if i + 1 < len(ordered) else BAND_HI
            p = padding_end(img, funcs[a]['end'], nxt)
            # MSVC aligns functions to 16: code resumes on the boundary
            if p < nxt and p >= funcs[a]['end']:
                q = (funcs[a]['end'] + 15) & ~15
                if q < nxt and q not in funcs and img.u8(q) not in (0x90, 0xCC, 0x00):
                    known.add(q)
                    funcs[q] = descend(img, q, known)
                    funcs[q]['found'] = True
                    changed = True
    return funcs


def clone_sites(img, base, end):
    """The transfers a byte-copy of [base, end) must have re-aimed: every E8 /
    E9 rel32 that leaves it, every stack-table immediate (mov dword [esp + k],
    imm32 with a .text value), every jump table inside it, and anything the
    harness cannot move (a jcc rel32 leaving it)."""
    calls, imms, tables, refused = [], [], [], []
    covered = set()
    work = [base]
    while work:
        pc = work.pop()
        while base <= pc < end and pc not in covered:
            ins = decode(img, pc)
            if ins is None:
                break
            covered.add(pc)
            m, ops = ins.mnemonic, ins.operands
            if m in ('call', 'jmp') and ops[0].type == x86.X86_OP_IMM:
                t = ops[0].imm & 0xFFFFFFFF
                if not base <= t < end:
                    if img.data[img.off(pc)] in (0xE8, 0xE9):
                        calls.append((pc - base, t))
                    else:
                        refused.append((pc - base, 'short jmp out to %#x' % t))
                    if m == 'jmp':
                        break
                elif m == 'jmp':
                    pc = t
                    continue
            elif m.startswith('j') and ops[0].type == x86.X86_OP_IMM:
                t = ops[0].imm & 0xFFFFFFFF
                if base <= t < end:
                    work.append(t)
                else:
                    refused.append((pc - base, 'conditional jump out to %#x' % t))
            elif m == 'jmp' and ops[0].type == x86.X86_OP_MEM:
                t = ops[0].mem.disp & 0xFFFFFFFF
                if base <= t < end and ops[0].mem.index != 0:
                    n = 0
                    while img.u32(t + 4 * n) is not None and base <= img.u32(t + 4 * n) < t:
                        work.append(img.u32(t + 4 * n))
                        n += 1
                    tables.append((pc - base + ins.size - 4, t - base, n))
                elif not img.in_text(t):
                    refused.append((pc - base, 'note: jmp through .data %#x, %d code entries (a data_tables entry)' % (t, _code_run(img, t))))
                else:
                    refused.append((pc - base, 'indirect jmp through .text %#x' % t))
                break
            elif m == 'call' and ops[0].type == x86.X86_OP_MEM and ops[0].mem.base != x86.X86_REG_ESP:
                t = ops[0].mem.disp & 0xFFFFFFFF
                if t and not img.in_text(t):
                    refused.append((pc - base, 'note: call through .data %#x, %d code entries (a data_tables entry)' % (t, _code_run(img, t))))
            elif m == 'mov' and len(ops) == 2 and ops[0].type == x86.X86_OP_MEM and ops[0].mem.base == x86.X86_REG_ESP \
                    and ops[1].type == x86.X86_OP_IMM and img.in_text(ops[1].imm & 0xFFFFFFFF):
                imms.append((pc - base + ins.size - 4, ops[1].imm & 0xFFFFFFFF))
            if m in ('ret', 'retf', 'int3', 'hlt'):
                break
            pc += ins.size
    return sorted(set(calls)), sorted(set(imms)), tables, refused


def _code_run(img, t):
    n = 0
    while img.u32(t + 4 * n) is not None and img.in_text(img.u32(t + 4 * n)):
        n += 1
    return n


def print_clones(img, addrs, funcs, named):
    """C++ for a group's clone table (magic_harness.h), by capstone."""
    lines = []
    for x in addrs:
        end = funcs[x]['end']
        calls, imms, tables, refused = clone_sites(img, x, end)
        tag = '%X' % x
        name = named.get(x, 'Fn_' + tag)
        print('// 0x%X: 0x%X bytes%s' % (x, end - x, ''.join(
            ('; +0x%X %s' if r[1].startswith('note') else '; REFUSED +0x%X %s') % r for r in refused)))
        if calls:
            print('constexpr magic_harness::CallSite kCalls%s[] = {%s};' % (
                tag, ', '.join('{0x%X, 0x%X}' % c for c in calls)))
        if imms:
            print('constexpr magic_harness::Imm kImms%s[] = {%s};' % (tag, ', '.join('{0x%X, 0x%X}' % i for i in imms)))
        if tables:
            print('constexpr magic_harness::JumpTable kTables%s[] = {%s};' % (
                tag, ', '.join('{0x%X, 0x%X, %d}' % t for t in tables)))
        cell = lambda v, k: ('k%s%s, MH_N(k%s%s)' % (k, tag, k, tag)) if v else 'nullptr, 0'
        lines.append('    {"%s", 0x%X, 0x%X, %s, %s, %s, reinterpret_cast<const void*>(&::%s)},' % (
            name, x, end - x, cell(calls, 'Calls'), cell(imms, 'Imms'), cell(tables, 'Tables'), name))
    print('#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])')
    print('const magic_harness::Clone kClones[] = {')
    for line in lines:
        print(line)
    print('};')


def load_toml(path):
    with open(path, 'rb') as f:
        return tomllib.load(f)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--exe', default=os.path.join(repo, 'bof3', 'BOF3.exe'))
    ap.add_argument('--analysis', default=os.path.join(repo, 'analysis'))
    ap.add_argument('--sibling', default=os.path.join(os.path.dirname(repo), 'BreathOfFire3Recomp'))
    ap.add_argument('--symbols', default=os.path.join(repo, 'symbols.toml'))
    ap.add_argument('--unit', help='print one unit (overlay name, e.g. MAGIC070) function by function')
    ap.add_argument('--clones', action='store_true',
                    help="with --unit: print the unit's functions not yet ours as magic_harness clone tables (C++)")
    ap.add_argument('--quiet', action='store_true', help='write the TSVs, print only the totals')
    a = ap.parse_args()

    img = Image(a.exe)
    rows = []
    for r in range(ROW_COUNT):
        o = img.off(ROWS_AT + 8 * r)
        fid, _, ptr = struct.unpack_from('<HHI', img.data, o)
        rows.append((fid, ptr))

    file_names = {}
    fid_path = os.path.join(a.sibling, 'analysis', 'file_ids.json')
    for x in json.load(open(fid_path)):
        file_names[int(x['id'], 16)] = os.path.splitext(os.path.basename(x['path']))[0]
    def fname(fid):
        return 'engine' if fid == ENGINE_FILE else file_names.get(fid - PSX_FILE_DELTA, '?%X' % fid)

    abilities = load_toml(os.path.join(a.sibling, 'names', 'magic.toml'))['ability']
    label = {ab['id']: ab['en'] for ab in abilities}
    by_row = collections.defaultdict(list)
    for ab in abilities:
        by_row[ab['row']].append(ab['id'])
    def shifted(i):
        n = label.get(i - 1)
        return n if n else ('(id 0)' if i == 0 else '(no label)')

    psx = {}
    cat = json.load(open(os.path.join(a.sibling, 'analysis', 'overlay_catalog.json')))
    for ov in cat['overlays']:
        if ov.get('family') == 'BMAGIC':
            psx[ov['name']] = (ov.get('function_count'), ov.get('size'))

    sym = load_toml(a.symbols)
    ours = {f['pc']: f['name'] for f in sym.get('func', []) if 'impl' in f}
    named = {f['pc']: f['name'] for f in sym.get('func', [])}

    pf = json.load(open(os.path.join(a.analysis, 'pc_funcs.json')))
    recorded = {f['entry'] for f in pf['functions']}
    hidden = {h['entry'] for h in json.load(open(os.path.join(a.analysis, 'pc_hidden.json')))}
    band = sorted(s for s in recorded | hidden if BAND_LO <= s < BAND_HI)

    funcs = discover(img, band)
    starts = sorted(funcs)
    sset = set(starts)

    # the task kinds' stack tables, (kind, parameter) -> handler
    kinds = {}
    for kind, (at, size) in KIND_DISPATCH.items():
        o = img.off(at)
        for ins in MD.disasm(img.data[o:o + size], at):
            ops = ins.operands
            if ins.mnemonic == 'mov' and len(ops) == 2 and ops[0].type == x86.X86_OP_MEM \
                    and ops[0].mem.base == x86.X86_REG_ESP and ops[1].type == x86.X86_OP_IMM:
                kinds[(kind, ops[0].mem.disp // 4)] = ops[1].imm & 0xFFFFFFFF
    k1_of = {v: '%d.%d' % k for k, v in kinds.items() if v in funcs}

    # Code in the band that is not a spell's: reached from no entry, and its
    # address stored in a table symbols.toml names for another subsystem
    # (MapCell_Handlers' four between MAGIC102 and MAGIC103).
    tables = [(d['pc'], d['pc'] + 4 * d.get('count', 1), d['name']) for d in sym.get('data', [])
              if d.get('ctype') in ('unsigned long', 'void *', 'const void *') and d['name'] != 'Magic_Rows']
    def stored_in(x):
        b = struct.pack('<I', x)
        names = set()
        i = img.data.find(b)
        while i >= 0:
            for nm, v, _, raw, rsz in img.secs:
                if raw <= i < raw + rsz and nm != '.text':
                    va = v + i - raw
                    for lo, hi, name in tables:
                        if lo <= va < hi:
                            names.add(name)
            i = img.data.find(b, i + 1)
        return names

    edges = {}
    for s in starts:
        e = {t for t in funcs[s]['out'] if not isinstance(t, tuple) and t in sset}
        for kp in funcs[s]['creates']:
            if kinds.get(kp) in sset:
                e.add(kinds[kp])
        edges[s] = e

    files = collections.defaultdict(list)
    for r, (fid, ptr) in enumerate(rows):
        if fid != ENGINE_FILE:
            files[fid].append(ptr)
    reach = {}
    for fid, ents in files.items():
        seen, st = set(), [x for x in ents if x in sset]
        while st:
            x = st.pop()
            if x not in seen:
                seen.add(x)
                st.extend(edges[x])
        reach[fid] = seen
    reached_by = collections.defaultdict(set)
    for fid, s in reach.items():
        for x in s:
            reached_by[x].add(fid)
    # foreign: its table, and everything only it reaches
    foreign = {}
    for x in starts:
        if not reached_by.get(x) and x < LIBRARY_LO:
            t = stored_in(x)
            if t:
                foreign[x] = '/'.join(sorted(t))
    grew = True
    while grew:
        grew = False
        for x in list(foreign):
            for y in edges[x]:
                if y not in foreign and not reached_by.get(y):
                    foreign[y] = foreign[x]
                    grew = True

    # Units: each file's lowest entry starts its code, unless it lies inside
    # an earlier file's reach (folded whole), or inside another unit's run of
    # functions (interleaved: one unit).
    first = {fid: min(e for e in ents if e in sset) for fid, ents in files.items()}
    folded_into = {}
    unit_start = {}
    for fid in sorted(first, key=lambda f: (first[f], f)):
        e = first[fid]
        owners = [g for g in reached_by[e] if g != fid and first[g] < e]
        if owners:
            folded_into[fid] = min(owners, key=lambda g: first[g])
        else:
            unit_start.setdefault(e, []).append(fid)
    us = sorted(unit_start)
    units = [dict(lo=u, files=sorted(unit_start[u])) for u in us]
    units.append(dict(lo=LIBRARY_LO, files=[], library=True))
    units.sort(key=lambda u: u['lo'])
    for i, u in enumerate(units):
        u['hi'] = units[i + 1]['lo'] if i + 1 < len(units) else BAND_HI
    ulos = [u['lo'] for u in units]
    def unit_of(x):
        return units[bisect.bisect_right(ulos, x) - 1]
    # interleaving: a file reached only by itself whose functions sit in another unit
    merged = True
    while merged:
        merged = False
        for x in starts:
            u = unit_of(x)
            rb = reached_by.get(x, set())
            if len(rb) == 1 and u['files'] and not (rb & set(u['files'])):
                (f,) = rb
                v = next((w for w in units if f in w['files']), None)
                if v is not None and v is not u and abs(units.index(v) - units.index(u)) == 1 \
                        and len(v['files']) == 1 and all(len(reached_by.get(y, ())) != 1 or reached_by[y] == {f}
                                                         for y in starts if v['lo'] <= y < v['hi']):
                    lo_u, hi_u = (u, v) if u['lo'] < v['lo'] else (v, u)
                    lo_u['files'] = sorted(set(lo_u['files']) | set(hi_u['files']))
                    lo_u['hi'] = hi_u['hi']
                    lo_u.setdefault('merged', True)
                    units.remove(hi_u)
                    ulos = [w['lo'] for w in units]
                    merged = True
                    break
    for u in units:
        u['name'] = LIBRARY_NAME if u.get('library') else '/'.join(fname(f) for f in u['files'])
        u['funcs'] = [s for s in starts if u['lo'] <= s < u['hi'] and s not in foreign]
        u['bytes'] = sum(funcs[s]['end'] - s for s in u['funcs'])
        u['ours'] = [s for s in u['funcs'] if s in ours]
        u['rows'] = sorted(r for r, (fid, _) in enumerate(rows) if fid in u['files'] or folded_into.get(fid) in u['files'])

    # the check (step 3): reached from one file only, and in another's extent
    exceptions = []
    for x in starts:
        u = unit_of(x)
        rb = reached_by.get(x, set())
        if rb and not (rb & set(u['files'])) and not u.get('library'):
            exceptions.append((x, u['name'], sorted(rb)))

    def funcs_cell(u):
        return ','.join('%X:%X%s' % (s, funcs[s]['end'] - s, '*' if s in ours else '') for s in u['funcs'])

    os.makedirs(a.analysis, exist_ok=True)
    rows_tsv = os.path.join(a.analysis, 'magic_rows.tsv')
    with open(rows_tsv, 'w', encoding='utf-8') as f:
        f.write('row\tfile_id\toverlay\tentry\tabilities (id: name, shifted)\tunit\tfirst\tlast\tfunctions\tbytes\tours\tpsx_functions\tpsx_size\tfunction_starts (addr:size, * ours)\n')
        for r, (fid, ptr) in enumerate(rows):
            abil = '; '.join('%#x: %s' % (i, shifted(i)) for i in sorted(by_row.get(r, [])))
            if fid == ENGINE_FILE:
                f.write('%d\t0xFFFF\tengine\t%#x\t%s\t-\t-\t-\t-\t-\t-\t-\t-\t-\n' % (r, ptr, abil))
                continue
            host = folded_into.get(fid, fid)
            u = next(w for w in units if host in w['files'])
            pc, ps = psx.get(fname(fid), (None, None))
            f.write('%d\t%#x\t%s%s\t%#x\t%s\t%s\t%#x\t%#x\t%d\t%d\t%d\t%s\t%s\t%s\n' % (
                r, fid, fname(fid), (' (folded into %s)' % fname(host)) if fid in folded_into else '', ptr, abil,
                u['name'], u['funcs'][0], u['funcs'][-1], len(u['funcs']), u['bytes'], len(u['ours']),
                pc if pc is not None else '', ps if ps is not None else '', funcs_cell(u)))
    funcs_tsv = os.path.join(a.analysis, 'magic_funcs.tsv')
    with open(funcs_tsv, 'w', encoding='utf-8') as f:
        f.write('start\tsize\tunit\tsource\tours\treached_by\ttask_kind.parameter\tentry_rows\tforeign\n')
        for x in starts:
            u = unit_of(x)
            src = ('R' if x in recorded else '') + ('H' if x in hidden else '') + ('S' if funcs[x].get('found') else '')
            ents = [r for r, (_, p) in enumerate(rows) if p == x]
            rb = reached_by.get(x, set())
            f.write('%#x\t%#x\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' % (
                x, funcs[x]['end'] - x, u['name'], src, ours.get(x, named.get(x, '') and '(%s)' % named[x]),
                ','.join(fname(g) for g in sorted(rb)) if len(rb) <= 6 else '%d files' % len(rb),
                k1_of.get(x, ''), ','.join(map(str, ents)), foreign.get(x, '')))

    if a.unit:
        u = next((w for w in units if a.unit in w['name'].split('/') or a.unit == w['name']), None)
        if u is None:
            sys.exit('no unit %s' % a.unit)
        if a.clones:
            print_clones(img, [x for x in u['funcs'] if x not in ours], funcs, named)
            return
        print('%s  %#x..%#x  %d functions, %#x bytes, rows %s' % (u['name'], u['lo'], u['hi'], len(u['funcs']), u['bytes'], u['rows']))
        for x in u['funcs']:
            rb = reached_by.get(x, set())
            ents = [r for r, (_, p) in enumerate(rows) if p == x]
            print('  %#x %5x %-28s %s%s%s' % (x, funcs[x]['end'] - x, ours.get(x, named.get(x, '')),
                                              ('rows %s ' % ents) if ents else '',
                                              ('task kind.parameter %s ' % k1_of[x]) if x in k1_of else '',
                                              ('reached by ' + ','.join(fname(g) for g in sorted(rb))) if 0 < len(rb) <= 4
                                              else ('reached by %d files' % len(rb) if rb else 'reached by none')))
        return

    dropped = sorted(set(band) - sset)
    overlaps = [(x, y) for x, y in zip(starts, starts[1:]) if funcs[x]['end'] > y]
    found = sorted(s for s in starts if funcs[s].get('found'))
    total = sum(len(u['funcs']) for u in units)
    tbytes = sum(u['bytes'] for u in units)
    tours = sum(len(u['ours']) for u in units)
    print('band %#x..%#x: %d starts recorded or hidden; %d dropped (a jump-table case or fallen into): %s; %d found by the descent (after a jump table, or code no start covers): %s'
          % (BAND_LO, BAND_HI, len(band), len(dropped), ' '.join('%#x' % x for x in dropped), len(found),
             ' '.join('%#x' % x for x in found)))
    print('extents overlapping the next start: %d %s' % (len(overlaps), ' '.join('%#x>%#x' % o for o in overlaps)))
    print('%d units (%d overlays with code, the library), %d functions, %#x (%d) bytes, %d ours; not ours %d, %d bytes'
          % (len(units), len(units) - 1, total, tbytes, tbytes, tours, total - tours,
             tbytes - sum(funcs[x]['end'] - x for u in units for x in u['ours'])))
    print('folded whole: %s' % ', '.join('%s -> %s' % (fname(f), fname(g)) for f, g in sorted(folded_into.items())))
    print('interleaved (one unit): %s' % ', '.join(u['name'] for u in units if u.get('merged')) or 'none')
    order = [min(u['files']) for u in units if u['files']]
    inv = [(fname(order[i]), fname(order[i + 1])) for i in range(len(order) - 1) if order[i + 1] < order[i]]
    print('out of file order: %s' % ', '.join('%s before %s' % p for p in inv))
    print('reached from another overlay only (shared bodies): %d' % len(exceptions))
    for x, un, rb in exceptions:
        print('  %#x in %s, reached by %s' % (x, un, ','.join(fname(g) for g in rb)))
    unreached = [x for x in starts if not reached_by.get(x) and not unit_of(x).get('library') and x not in foreign]
    print('reached from no entry (a task the engine starts, or dead code): %d %s' % (
        len(unreached), ' '.join('%#x%s' % (x, '(kind %s)' % k1_of[x] if x in k1_of else '') for x in unreached)))
    print('not spell code (in a table of another subsystem, and what only it reaches): %d %s' % (
        len(foreign), ' '.join('%#x(%s)' % (x, foreign[x]) for x in sorted(foreign))))
    if not a.quiet:
        print()
        print('%-34s %-9s %-9s %4s %7s %4s %5s %7s  %s' % ('unit', 'first', 'last', 'fns', 'bytes', 'ours', 'psx', 'psxsz', 'rows'))
        for u in units:
            pc = [psx.get(fname(f), (None, None)) for f in u['files']]
            pcs = '/'.join(str(p[0]) for p in pc if p[0] is not None)
            pss = '/'.join(str(p[1]) for p in pc if p[1] is not None)
            print('%-34s %#-9x %#-9x %4d %7d %4d %5s %7s  %s' % (u['name'][:34], u['funcs'][0], u['funcs'][-1], len(u['funcs']),
                                                             u['bytes'], len(u['ours']), pcs[:5], pss[:7],
                                                             ','.join(map(str, u['rows']))))
    print('\nwrote %s, %s' % (rows_tsv, funcs_tsv))


if __name__ == '__main__':
    main()
