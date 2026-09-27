#!/usr/bin/env python
"""Every area of the field engine, the code the engine reaches it through,
and the PC extent of that code.

    python tools/area_rows.py                  -> analysis/area_rows.tsv
                                                  analysis/area_funcs.tsv
                                                  and a summary on stdout
    python tools/area_rows.py --unit AREA033   -> one area's block, function
                                                  by function
    python tools/area_rows.py --unit AREA033 --clones
                                               -> its clone table for the area
                                                  harness (docs/area_harness.md)
    python tools/area_rows.py --groups         -> the proposed groups, whole
                                                  areas in address order

The PSX loaded one AREA overlay per area; the PC compiled all of them into the
exe, in area order, as one band of .text at its very start (0x401000 up to the
battle code's BATE / BATTLE functions). docs/area-rows.md has the measurement
this prints; docs/takeover-queue-areas.md is the plan it serves.

**The roots.** The engine reaches an area's code through seven tables, all
read off the exe here, and through the area's own data:

1. Area_Descriptors (0x667590, 200 pointers) -> a 0x44-byte descriptor per
   area (0x5DB318 .. 0x64AD68, in area order). Three of its fields are code:
   +0x34 the choice handlers (MsgBox_ChoiceCommit, [+0x34][id]),
   +0x3C the handler array (MoveScript ops 03 / DE, [+0x3C][n]),
   +0x40 the init (Area_Enter, once per entry).
2. Area_StepHook 0x56E050 / Area_ArriveHook 0x56E4E0: switches over the area
   number, read from their byte index and jump tables (docs/event-ops.md 6).
3. Field_ModeTailKinds 0x662CE8, 64 slots: Field_ModeTailRun jumps through it
   by the byte 0x9039F3, which the areas' own code sets. A slot is attributed
   to the area whose code stores its number into that byte.
4. Area_CellHooks 0x662F28: 28 (area byte, pad, function) records the cell
   hook 0x56E670 searches by Game_AreaNumber (its loop stops at 0x663008).
5. WorldMap_Records 0x653910 (eleven 0x1C-byte records, five code fields and
   the area byte +0x18) and WorldMap_FieldHooks 0x662DF0 (one per record,
   the twelfth for "no world map").
5a. Field_ObjectTriggers 0x662E20: 0x56E020, called by 0x56D6B0 for an
   object with +0x89 bit 6, calls [0x662E1C + object[+0x86] * 4](object,
   story flags) - ids 1..64 up to the null before Area_CellHooks. Not in the
   plan's seven; found by this tool's scan 7. Area-less: an id is set by
   whatever placed the object.
6. The area's data block: every dword of its .data that points at a band
   function, beyond the descriptor's own tables (the state tables its frame
   functions dispatch through, handler lists its scripts name). The block is
   the span from the previous descriptor's end to this one's, corrected by
   the walk: a dword in a table (the nearest start at or below it that code
   or a descriptor names) that one area alone reads - its descriptor or its
   exclusive code - is that area's, wherever it lies.
7. For completeness, not roots of any area: rel32 calls into the band from
   outside it, and .data dwords outside the descriptor region that hold a
   band start. The tool prints them; an area harness must know they exist.

**The walk** is a recursive descent of every start (pe_funcs.py's and
pe_hidden.py's in the band), with the three lessons of
docs/scenario-roots.md section 3: a switch's jump table lives in .text right
after its function and every known start before the table is that function's
(a case, not a function); a conditional jump into a neighbour's start is a
shared tail (an edge, not a merge; the clone table REFUSES it); functions
already ours are walked through like any other. Code no start covers - after
a jump table, reached by a direct call or a tail jmp only, or named by a
.data table - is a start of its own; the discovery runs to a fixpoint.
An area's closure follows calls, tail and shared jumps, code immediates,
the .data tables its code indexes or loads (a run of code pointers, stopped
at the next table any code or symbols.toml names), and the tail kinds it
arms. Code outside the band is the frontier: counted, not walked.

**The block rule** (the spell round's, docs/takeover-queue-round9-spells.md
section 1): a function reached by one area only is that area's; each area's
block runs from its lowest exclusive function to the next area's lowest.
Areas whose exclusive functions interleave are one unit. Shared bodies
(reached by two or more) and unreached starts (the gaps) belong to the block
they lie in - keyed by address, as the spell round's 781 were. The starts
past the last function any area reaches are not area code (BATE, BATTLE).

**The groups** are whole units in address order, one world at a time
(the world is the PSX directory of the area's file, BIN/WORLDnn, from the
sibling's analysis/file_ids.json), cut into about --group-size functions
not yet ours each.

Reads bof3/BOF3.exe (--exe), analysis/ (--analysis: pc_funcs.json,
pc_hidden.json, remaining_catalog.tsv, the hidden_reached_*.json route
reaches, area_pairs*.json for PSX twins), symbols.toml and the sibling
checkout (--sibling). The output lists addresses and sizes only, but it is
derived from copyrighted game code: it lives under analysis/ and is never
committed (CLAUDE.md rule 1).
"""
import argparse
import bisect
import collections
import csv
import glob
import json
import os
import re
import statistics
import struct
import sys
import tomllib

from capstone import x86

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import magic_rows as mr  # noqa: E402  Image, decode, clone_sites: the spell harness's clone sites as they are

BAND_LO = 0x401000              # the start of .text: area 0's first function
BAND_HI = 0x430000              # BattleAction_End 0x430010 is battle code
AREA_TABLE = 0x667590           # Area_Descriptors
AREAS = 200
DESC_SIZE = 0x44                # g_descriptor[0x44] in every fuzz; the PSX's 0x44 (area_records.toml)
CHOICE, HANDLERS, INIT = 0x34, 0x3C, 0x40
STEP_HOOK, ARRIVE_HOOK = 0x56E050, 0x56E4E0
TAIL_KINDS, TAIL_KIND_COUNT, TAIL_KIND_BYTE = 0x662CE8, 64, 0x9039F3
CELL_HOOKS, CELL_HOOK_COUNT = 0x662F28, 28
WM_RECORDS, WM_RECORD_SIZE, WM_RECORD_COUNT, WM_CODE_FIELDS, WM_AREA = 0x653910, 0x1C, 11, 5, 0x18
WM_HOOKS, WM_HOOK_COUNT = 0x662DF0, 12
OBJ_TRIGGERS = 0x662E1C          # 0x56E020: call [0x662E1C + object[+0x86] * 4](object, 0x904030); ids from 1
BARE_RET = 0x437CC0             # every empty handler slot's target
PADDING = (0x90, 0xCC)


# --------------------------------------------------------------------------
# The descent

class Descent:
    """One function by recursive descent. `absorbed` are known starts that
    turned out to be inside it (a case, fallen into, before its jump table)."""

    def __init__(self, img, start, starts, absorbed=frozenset()):
        self.start = start
        self.end = start
        self.calls, self.tails, self.shared = set(), set(), set()
        self.imms, self.drefs, self.tables = set(), set(), set()
        self.tail_kinds = set()
        self.new_starts = set()
        self.absorb = set()          # known starts found inside: the caller re-descends
        self.jtables = []            # (.text table, entries)
        self.indirect = []           # (pc, text): calls and jumps through registers or .data
        self._run(img, starts, absorbed)

    def _limit(self, starts, absorbed):
        i = bisect.bisect_right(starts, self.start)
        while i < len(starts) and starts[i] in absorbed:
            i += 1
        return starts[i] if i < len(starts) else BAND_HI

    def _run(self, img, starts, absorbed):
        known = set(starts)
        limit = self._limit(starts, absorbed)
        inside = lambda t: self.start <= t < limit or t in absorbed or \
            any(a <= t < _next(starts, a, absorbed) for a in absorbed)
        seen = set()
        work = [self.start]
        while work:
            pc = work.pop()
            prev = []
            while pc not in seen:
                ins = mr.decode(img, pc)
                if ins is None or ins.mnemonic in ('nop', 'int3'):
                    break
                seen.add(pc)
                self.end = max(self.end, pc + ins.size)
                m, ops = ins.mnemonic, ins.operands
                for op in ops:
                    if op.type == x86.X86_OP_MEM:
                        d = op.mem.disp & 0xFFFFFFFF
                        if img.off(d) is not None and not img.in_text(d):
                            self.drefs.add(d)
                            if op.mem.index != 0:
                                self.tables.add(d)
                    elif op.type == x86.X86_OP_IMM and m != 'call' and not m.startswith('j'):
                        v = op.imm & 0xFFFFFFFF
                        if img.in_text(v):
                            self.imms.add(v)
                        elif img.off(v) is not None and m != 'cmp' and v >= 0x5B0000:
                            self.tables.add(v)      # a .data table's address taken
                # the area arms a tail kind: mov byte [0x9039F3], imm8
                if m == 'mov' and len(ops) == 2 and ops[0].type == x86.X86_OP_MEM and ops[0].size == 1 \
                        and ops[0].mem.base == 0 and ops[0].mem.index == 0 \
                        and ops[0].mem.disp & 0xFFFFFFFF == TAIL_KIND_BYTE and ops[1].type == x86.X86_OP_IMM:
                    self.tail_kinds.add(ops[1].imm & 0xFF)
                if m == 'call':
                    if ops[0].type == x86.X86_OP_IMM:
                        t = ops[0].imm & 0xFFFFFFFF
                        if img.in_text(t):
                            self.calls.add(t)
                            if BAND_LO <= t < BAND_HI and t not in known:
                                self.new_starts.add(t)      # a direct call is evidence of a start
                    else:
                        self.indirect.append((pc, 'call ' + ins.op_str))
                elif m in ('ret', 'retf', 'hlt'):
                    break
                elif m == 'jmp' and ops[0].type == x86.X86_OP_MEM:
                    t = ops[0].mem.disp & 0xFFFFFFFF
                    if ops[0].mem.index != 0 and img.in_text(t) and t > self.start:
                        n = self._switch(img, t, prev, known, work)
                        self.jtables.append((t, n))
                    else:
                        self.indirect.append((pc, 'jmp ' + ins.op_str))
                    break
                elif m == 'jmp' or m.startswith('j') or m.startswith('loop'):
                    if ops[0].type != x86.X86_OP_IMM:
                        self.indirect.append((pc, m + ' ' + ins.op_str))
                        break
                    t = ops[0].imm & 0xFFFFFFFF
                    if inside(t):
                        work.append(t)
                    elif t in known:
                        (self.tails if m == 'jmp' else self.shared).add(t)
                    elif m == 'jmp' and BAND_LO <= t < BAND_HI and _looks_like_start(img, t):
                        self.tails.add(t)
                        self.new_starts.add(t)      # a tail jmp to code no list has
                    elif img.in_text(t):
                        self.shared.add(t)          # into a neighbour's body: a shared tail
                    if m == 'jmp':
                        break
                prev.append(ins)
                prev = prev[-6:]
                pc += ins.size
                if pc >= limit and pc in known and pc not in absorbed and pc not in seen:
                    self.absorb.add(pc)             # fell through into a known start
                    break

    def _switch(self, img, t, prev, known, work):
        """jmp [r*4 + T], T in .text after the function: the cases, capped by
        the cmp / ja before it; every known start before T is this function's
        (docs/scenario-roots.md section 3, the first lesson)."""
        cap = mr._jump_cap(img, prev)
        n = 0
        while n < cap:
            w = img.u32(t + 4 * n)
            if w is None or not (self.start <= w < t):
                break
            work.append(w)
            n += 1
        self.end = max(self.end, t + 4 * n)
        if mr._byte_table(img, prev):
            for p in prev[-3:]:
                if p.mnemonic in ('mov', 'movzx') and len(p.operands) == 2 \
                        and p.operands[1].type == x86.X86_OP_MEM and p.operands[1].size == 1:
                    t2 = p.operands[1].mem.disp & 0xFFFFFFFF
                    bound = mr._cmp_bound(prev)
                    if img.in_text(t2) and bound is not None and bound < 0x400:
                        self.end = max(self.end, t2 + bound + 1)
        if t - self.start < 0x4000:
            self.absorb.update(s for s in known if self.start < s < t)
        return n


def _next(starts, a, absorbed):
    i = bisect.bisect_right(starts, a)
    while i < len(starts) and starts[i] in absorbed:
        i += 1
    return starts[i] if i < len(starts) else BAND_HI


def _looks_like_start(img, t):
    """On a 16-byte boundary, after padding or a ret, and not padding itself."""
    return t % 16 == 0 and img.u8(t) not in PADDING + (0,) and img.u8(t - 1) in PADDING + (0xC3,)


def discover(img, band_starts, extra=()):
    """Descend every start; absorb the false ones; add what the descents and
    `extra` name; repeat to a fixpoint. Returns ({start: Descent}, dropped,
    found)."""
    listed = set(band_starts)
    known = set(band_starts) | set(extra)
    dropped = set()
    while True:
        starts = sorted(known)
        funcs = {}
        absorbed_by = {}
        grew = False
        for s in starts:
            if s in dropped:
                continue
            absorbed = frozenset()
            while True:
                d = Descent(img, s, starts, absorbed)
                if not d.absorb - absorbed:
                    break
                absorbed = absorbed | d.absorb
            funcs[s] = d
            for a in absorbed:
                absorbed_by[a] = s
        # a start absorbed by another is not a function
        for a, s in absorbed_by.items():
            if a in funcs and a != s:
                del funcs[a]
                if a not in dropped:
                    dropped.add(a)
                    grew = True
        for d in funcs.values():
            for t in d.new_starts:
                if t not in known and t not in dropped:
                    known.add(t)
                    grew = True
        # code no start covers: the next 16-byte boundary after a function's end
        ordered = sorted(funcs)
        for i, a in enumerate(ordered):
            nxt = ordered[i + 1] if i + 1 < len(ordered) else BAND_HI
            q = (funcs[a].end + 15) & ~15
            p = mr.padding_end(img, funcs[a].end, nxt)
            if p < nxt and q < nxt and q not in known and img.u8(q) not in PADDING + (0,) \
                    and all(img.u8(x) in PADDING for x in range(funcs[a].end, q)):
                known.add(q)
                grew = True
        known -= dropped
        if not grew:
            found = sorted(set(funcs) - listed)
            return funcs, sorted(dropped & listed), found


# --------------------------------------------------------------------------
# The tables

def code_run(img, t, table_starts, limit=256):
    """The code pointers of the .data table at t: while the dwords are .text
    addresses, up to the next table's start."""
    ws = []
    while len(ws) < limit:
        a = t + 4 * len(ws)
        w = img.u32(a)
        if w is None or not img.in_text(w) or (ws and a in table_starts):
            break
        ws.append(w)
    return ws


def read_switch(img, fn):
    """Area_StepHook / Area_ArriveHook: add eax, -bias; cmp eax, n; ja default;
    mov cl, [eax + index]; jmp [ecx*4 + table]. Returns {area: handler}."""
    bias = n = default = index = table = None
    pc = fn
    for _ in range(64):
        ins = mr.decode(img, pc)
        ops = ins.operands
        if ins.mnemonic == 'add' and ops[0].type == x86.X86_OP_REG and ops[1].type == x86.X86_OP_IMM:
            bias = -ops[1].imm
        elif ins.mnemonic == 'cmp' and ops[1].type == x86.X86_OP_IMM:
            n = ops[1].imm
        elif ins.mnemonic == 'ja':
            default = ops[0].imm
        elif ins.mnemonic == 'mov' and ops[1].type == x86.X86_OP_MEM and ops[1].size == 1:
            index = ops[1].mem.disp
        elif ins.mnemonic == 'jmp' and ops[0].type == x86.X86_OP_MEM:
            table = ops[0].mem.disp
            break
        pc += ins.size
    out = {}
    for i in range(n + 1):
        case = img.u32(table + 4 * img.u8(index + i))
        if case == default:
            continue
        pc = case
        for _ in range(12):
            ins = mr.decode(img, pc)
            if ins.mnemonic == 'call' and ins.operands[0].type == x86.X86_OP_IMM:
                out[i + bias] = ins.operands[0].imm & 0xFFFFFFFF
                break
            if ins.mnemonic in ('ret', 'jmp'):
                break
            pc += ins.size
    return out


def load_toml(path):
    with open(path, 'rb') as f:
        return tomllib.load(f)


def median(xs):
    return statistics.median(xs) if xs else 0


# --------------------------------------------------------------------------

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--exe', default=os.path.join(repo, 'bof3', 'BOF3.exe'))
    ap.add_argument('--analysis', default=os.path.join(repo, 'analysis'))
    ap.add_argument('--sibling', default=os.path.join(os.path.dirname(repo), 'BreathOfFire3Recomp'))
    ap.add_argument('--symbols', default=os.path.join(repo, 'symbols.toml'))
    ap.add_argument('--unit', help='print one unit (AREA033, or 33) function by function')
    ap.add_argument('--clones', action='store_true',
                    help="with --unit: print the unit's functions not yet ours as area_harness clone tables (C++)")
    ap.add_argument('--groups', action='store_true', help='print the proposed groups')
    ap.add_argument('--group-size', type=int, default=50, help='functions not yet ours a group aims at (default 50)')
    ap.add_argument('--quiet', action='store_true', help='write the TSVs, print only the totals')
    a = ap.parse_args()

    img = mr.Image(a.exe)
    sym = load_toml(a.symbols)
    ours = {f['pc']: f['name'] for f in sym.get('func', []) if 'impl' in f}
    named = {f['pc']: f['name'] for f in sym.get('func', [])}
    data_named = {d['pc']: d['name'] for d in sym.get('data', [])}

    pf = json.load(open(os.path.join(a.analysis, 'pc_funcs.json')))
    recorded = {f['entry'] for f in pf['functions']}
    hidden = {h['entry'] for h in json.load(open(os.path.join(a.analysis, 'pc_hidden.json')))}
    band = sorted(s for s in recorded | hidden if BAND_LO <= s < BAND_HI)
    label = {}
    with open(os.path.join(a.analysis, 'remaining_catalog.tsv'), encoding='utf-8') as fh:
        for row in csv.DictReader(fh, delimiter='\t'):
            label[int(row['entry'], 16)] = row['label']
    world_of = {}
    for x in json.load(open(os.path.join(a.sibling, 'analysis', 'file_ids.json'))):
        r = re.search(r'WORLD(\d+)/AREA(\d+)', x['path'])
        if r:
            world_of[int(r.group(2))] = int(r.group(1))

    # ---- the descriptors and the fixed tables ----------------------------
    descs = [img.u32(AREA_TABLE + 4 * k) for k in range(AREAS)]
    fields = {k: [img.u32(d + o) for o in range(0, DESC_SIZE, 4)] for k, d in enumerate(descs)}
    step = read_switch(img, STEP_HOOK)
    arrive = read_switch(img, ARRIVE_HOOK)
    cell = {}
    for i in range(CELL_HOOK_COUNT):
        at = CELL_HOOKS + 8 * i
        cell.setdefault(img.u8(at), []).append(img.u32(at + 4))
    tail = [img.u32(TAIL_KINDS + 4 * i) for i in range(TAIL_KIND_COUNT)]
    wm_area, wm_code = [], []
    for r in range(WM_RECORD_COUNT):
        at = WM_RECORDS + WM_RECORD_SIZE * r
        wm_area.append(img.u8(at + WM_AREA))
        wm_code.append([img.u32(at + 4 * f) for f in range(WM_CODE_FIELDS)])
    wm_hooks = [img.u32(WM_HOOKS + 4 * i) for i in range(WM_HOOK_COUNT)]
    triggers = []
    while OBJ_TRIGGERS + 4 * (len(triggers) + 1) < CELL_HOOKS and img.u32(OBJ_TRIGGERS + 4 * (len(triggers) + 1)):
        triggers.append(img.u32(OBJ_TRIGGERS + 4 * (len(triggers) + 1)))

    # the descriptor region: area 0's lowest table to area 199's descriptor end
    region_lo = min(v for v in fields[0] if v and descs[0] - 0x40000 < v < descs[0])
    region_hi = descs[-1] + DESC_SIZE

    # ---- the roots, before the walk: {start: [(area or None, how)]} -------
    def desc_roots(table_starts):
        roots = collections.defaultdict(list)
        stats = {}
        for k, d in enumerate(descs):
            f = fields[k]
            # an area's handler array and choice table overlap more often than
            # not (area 3's choice table is its handlers from [2] on): neither
            # stops the other's run
            own = table_starts - {f[CHOICE // 4], f[HANDLERS // 4]}
            ch = code_run(img, f[CHOICE // 4], own) if f[CHOICE // 4] else []
            hd = code_run(img, f[HANDLERS // 4], own) if f[HANDLERS // 4] else []
            for i, w in enumerate(ch):
                roots[w].append((k, '+0x34[%d] choice' % i))
            for i, w in enumerate(hd):
                roots[w].append((k, '+0x3C[%d] handler' % i))
            if img.in_text(f[INIT // 4]):
                roots[f[INIT // 4]].append((k, '+0x40 init'))
            stats[k] = (len(ch), len(hd), 1 if img.in_text(f[INIT // 4]) else 0)
        return roots, stats

    fixed = collections.defaultdict(list)
    for k, h in step.items():
        fixed[h].append((k, 'step hook'))
    for k, h in arrive.items():
        fixed[h].append((k, 'arrive hook'))
    for k, fs in cell.items():
        for h in fs:
            fixed[h].append((k, 'cell hook'))
    for i, h in enumerate(tail):
        if i and h != BARE_RET:
            fixed[h].append((None, 'tail kind %d' % i))
    for r in range(WM_RECORD_COUNT):
        for f, h in enumerate(wm_code[r]):
            if h:
                fixed[h].append((wm_area[r], 'world-map record %d +0x%X' % (r, 4 * f)))
        fixed[wm_hooks[r]].append((wm_area[r], 'world-map field hook %d' % r))
    fixed[wm_hooks[WM_RECORD_COUNT]].append((None, 'world-map field hook %d (none)' % WM_RECORD_COUNT))
    for i, h in enumerate(triggers):
        fixed[h].append((None, 'object trigger %d' % (i + 1)))

    # rel32 calls into the band from outside it (scan 7); those from the hook
    # switches are roots already, the rest are area-less roots of their own
    t = next(sec for sec in img.secs if sec[0] == '.text')
    seg = img.data[t[3]:t[3] + t[4]]
    outside_calls = collections.defaultdict(list)
    for i in range(len(seg) - 5):
        if seg[i] in (0xE8, 0xE9):
            src = img.text_lo + i
            if BAND_LO <= src < BAND_HI:
                continue
            tgt = (src + 5 + struct.unpack_from('<i', seg, i + 1)[0]) & 0xFFFFFFFF
            if BAND_LO <= tgt < BAND_HI:
                ins = mr.decode(img, src)
                if ins is not None and ins.mnemonic in ('call', 'jmp') and ins.operands[0].type == x86.X86_OP_IMM:
                    outside_calls[tgt].append(src)
    switches = [(STEP_HOOK, 0x56E2F8), (ARRIVE_HOOK, 0x56E5B8)]
    for tgt, srcs in outside_calls.items():
        for src in srcs:
            if not any(lo <= src < hi for lo, hi in switches):
                fixed[tgt].append((None, 'called from %#x' % src))

    # .data outside the descriptor region and the tables above naming a band
    # start (scan 7): area-less roots too
    fixed_words = set(range(TAIL_KINDS, TAIL_KINDS + 4 * TAIL_KIND_COUNT, 4))         | set(range(CELL_HOOKS, CELL_HOOKS + 8 * CELL_HOOK_COUNT, 4))         | set(range(WM_RECORDS, WM_RECORDS + WM_RECORD_SIZE * WM_RECORD_COUNT, 4))         | set(range(WM_HOOKS, WM_HOOKS + 4 * WM_HOOK_COUNT, 4))         | set(range(OBJ_TRIGGERS + 4, OBJ_TRIGGERS + 4 + 4 * len(triggers), 4))
    band_set = set(band)
    outside_data = collections.defaultdict(list)
    for nm, v, _, raw, rsz in img.secs:
        if nm == '.text':
            continue
        for off in range(0, rsz - 3, 4):
            w = struct.unpack_from('<I', img.data, raw + off)[0]
            if BAND_LO <= w < BAND_HI and (w in band_set or _looks_like_start(img, w)):
                va = v + off
                if region_lo <= va < region_hi + 0x1000 or va in fixed_words:
                    continue
                outside_data[w].append(va)
                fixed[w].append((None, 'named by .data %#x' % va))

    # ---- discovery, the tables, the data blocks, to a fixpoint -----------
    extra = {h for h in fixed if BAND_LO <= h < BAND_HI}
    refs = None             # {.data table: the areas that read it}, from the previous round
    for rnd in range(6):
        funcs, dropped, found = discover(img, band, extra)
        starts = sorted(funcs)
        sset = set(starts)
        table_starts = set(data_named) | set(descs) | {TAIL_KINDS, CELL_HOOKS, WM_RECORDS, WM_HOOKS}
        for k in range(AREAS):
            table_starts |= {v for v in fields[k] if img.off(v) is not None and not img.in_text(v)}
        for d in funcs.values():
            table_starts |= d.tables
        # the .data tables each function reads: runs of code pointers
        runs = {}
        for d in funcs.values():
            for t in d.tables | d.drefs:
                if t not in runs:
                    runs[t] = code_run(img, t, table_starts)
        roots, dstats = desc_roots(table_starts)
        for h, v in fixed.items():
            roots[h].extend(v)

        def func_of(x):
            i = bisect.bisect_right(starts, x) - 1
            return starts[i] if i >= 0 else None

        def resolve(t):
            """A target's function: itself if a start, else the start whose body holds it."""
            if t in sset:
                return t
            f = func_of(t)
            return f if f is not None and f <= t < funcs[f].end else None

        edges = {}
        for s, d in funcs.items():
            e = set()
            for t in d.calls | d.tails | d.shared:
                e.add(t if not (BAND_LO <= t < BAND_HI) else resolve(t))
            e |= {t for t in d.imms if t in sset}
            for t in d.tables | d.drefs:
                e |= {w if not (BAND_LO <= w < BAND_HI) else resolve(w) for w in runs[t]}
            for n in d.tail_kinds:
                if 0 < n < TAIL_KIND_COUNT and tail[n] != BARE_RET:
                    e.add(resolve(tail[n]) if BAND_LO <= tail[n] < BAND_HI else tail[n])
            e.discard(None)
            e.discard(s)
            edges[s] = e

        def closure(seeds):
            seen, front, st = set(), set(), [x for x in seeds]
            while st:
                x = st.pop()
                if x in seen or x in front:
                    continue
                if BAND_LO <= x < BAND_HI and x in sset:
                    seen.add(x)
                    st.extend(edges[x])
                else:
                    front.add(x)
            return seen, front

        # the data blocks: each area's span by descriptor order, corrected by the walk
        desc_block = []
        for k, d in enumerate(descs):
            lo = descs[k - 1] + DESC_SIZE if k else region_lo
            desc_block.append((lo, d + DESC_SIZE))
        block_los = [b[0] for b in desc_block]

        def block_owner(x):
            i = bisect.bisect_right(block_los, x) - 1
            return i if 0 <= i < AREAS and x < desc_block[i][1] else (AREAS - 1 if x >= region_hi else None)

        desc_table_words = set()
        for k in range(AREAS):
            for fo in (CHOICE, HANDLERS):
                t = fields[k][fo // 4]
                if t:
                    own = table_starts - {fields[k][CHOICE // 4], fields[k][HANDLERS // 4]}
                    n = len(code_run(img, t, own))
                    desc_table_words.update(range(t, t + 4 * n, 4))
        data_ptrs = []          # (address, value, desc-order owner)
        # to the last descriptor's end; past it only the tables the last
        # area's own code alone reads (the BATE / BATTLE tables follow it)
        ref_starts = sorted(refs) if refs else []
        ranges = [(region_lo & ~3, region_hi)]
        for t in ref_starts:
            if t >= region_hi and refs[t] == {AREAS - 1}:
                ranges.append((t, t + 4 * max(1, len(code_run(img, t, table_starts)))))
        for lo_, hi_ in ranges:
            for x in range(lo_, hi_, 4):
                v = img.u32(x)
                if v is None or not (BAND_LO <= v < BAND_HI):
                    continue
                if v not in sset and not _looks_like_start(img, v):
                    continue
                k = block_owner(x)
                if k is not None and descs[k] <= x < descs[k] + DESC_SIZE:
                    continue        # the descriptor's own +0x40
                data_ptrs.append((x, v, k))

        def data_owner(x, k):
            """The walk's correction: the table holding x (the nearest start
            at or below it that code or a descriptor names, within 0x400) is
            read by one area only - that area's."""
            if not refs or k is None:
                return k
            i = bisect.bisect_right(ref_starts, x) - 1
            if i < 0 or x - ref_starts[i] >= 0x400:
                return k
            owners = refs[ref_starts[i]]
            return next(iter(owners)) if len(owners) == 1 else k

        data_roots = collections.defaultdict(list)
        moved = []
        for x, v, k in data_ptrs:
            if x in desc_table_words:
                continue
            j = data_owner(x, k)
            if j != k:
                moved.append((x, v, k, j))
            data_roots[v].append((j, 'data %#x' % x))
        all_roots = collections.defaultdict(list)
        for h, v in roots.items():
            all_roots[h].extend(v)
        for h, v in data_roots.items():
            all_roots[h].extend(v)

        new = {v for v in all_roots if BAND_LO <= v < BAND_HI and v not in sset and _looks_like_start(img, v)}
        for d in funcs.values():
            for t in d.tables | d.drefs:
                new |= {w for w in runs[t] if BAND_LO <= w < BAND_HI and w not in sset and _looks_like_start(img, w)}
        # per-area closures
        seeds = collections.defaultdict(set)
        for h, v in all_roots.items():
            for k, _ in v:
                seeds[k].add(h if not (BAND_LO <= h < BAND_HI) else (resolve(h) or h))
        reach, front = {}, {}
        for k, s in seeds.items():
            reach[k], front[k] = closure(s)
        reached_by = collections.defaultdict(set)
        for k, s in reach.items():
            if k is not None:
                for x in s:
                    reached_by[x].add(k)
        # for the next round: which areas read each table in the descriptor
        # region - a descriptor's own fields, and its area's exclusive code
        nrefs = collections.defaultdict(set)
        for k in range(AREAS):
            for v in fields[k]:
                if region_lo <= v < region_hi:
                    nrefs[v].add(k)
            for x in reach.get(k, ()):
                if reached_by[x] == {k}:
                    for t in funcs[x].drefs | funcs[x].tables:
                        if region_lo <= t < region_hi + 0x1000:
                            nrefs[t].add(k)
        nrefs = dict(nrefs)
        if not (new - set(extra)) and nrefs == refs:
            break
        extra |= new
        refs = nrefs
    rounds = rnd + 1

    # ---- units: blocks of exclusive functions -----------------------------
    exclusive = collections.defaultdict(list)
    for x in starts:
        rb = reached_by.get(x, set())
        if len(rb) == 1:
            exclusive[next(iter(rb))].append(x)
    last_reached = max(x for x in starts if reached_by.get(x))
    tail_lo = next((x for x in starts if x > last_reached), BAND_HI)
    firsts = sorted((min(xs), k) for k, xs in exclusive.items())
    units = [dict(lo=lo, areas=[k]) for lo, k in firsts]
    # interleaved: an area's exclusive function inside another area's block -> one unit
    merged_pairs = []
    while True:
        los = [u['lo'] for u in units]
        idx = {k: i for i, u in enumerate(units) for k in u['areas']}
        hit = None
        for k, xs in exclusive.items():
            for x in xs:
                i = bisect.bisect_right(los, x) - 1
                if i != idx[k]:
                    hit = (min(i, idx[k]), max(i, idx[k]))
                    break
            if hit:
                break
        if not hit:
            break
        i, j = hit
        merged_pairs.append(tuple(k for u in units[i:j + 1] for k in u['areas']))
        units[i:j + 1] = [dict(lo=units[i]['lo'], areas=sorted(k for u in units[i:j + 1] for k in u['areas']))]
    for i, u in enumerate(units):
        u['hi'] = units[i + 1]['lo'] if i + 1 < len(units) else tail_lo
        u['name'] = '/'.join('AREA%03d' % k for k in u['areas'])
        u['funcs'] = [x for x in starts if u['lo'] <= x < u['hi']]
        u['bytes'] = sum(funcs[x].end - x for x in u['funcs'])
        u['ours'] = [x for x in u['funcs'] if x in ours]
        u['world'] = world_of.get(u['areas'][0])
    # the starts before the first block are the first block's gaps
    u = units[0]
    u['lo'] = BAND_LO
    u['funcs'] = [x for x in starts if u['lo'] <= x < u['hi']]
    u['bytes'] = sum(funcs[x].end - x for x in u['funcs'])
    u['ours'] = [x for x in u['funcs'] if x in ours]
    tail_funcs = [x for x in starts if x >= tail_lo]
    ulos = [u['lo'] for u in units]

    def unit_of(x):
        if x >= tail_lo:
            return None
        return units[bisect.bisect_right(ulos, x) - 1]

    # ---- groups: whole units, one world at a time -------------------------
    groups = []
    by_world = collections.defaultdict(list)
    for u in units:
        if u['areas']:
            by_world[u['world']].append(u)
    for w in sorted(by_world):
        us = by_world[w]
        todo = [len(u['funcs']) - len(u['ours']) for u in us]
        total = sum(todo)
        g = max(1, round(total / a.group_size))
        cuts, acc, want = [], 0, 1
        for i, n in enumerate(todo):
            acc += n
            if want < g and acc >= want * total / g:
                # cut after this unit, or before it if that is closer to the mark
                mark = want * total / g
                if i > 0 and (not cuts or cuts[-1] < i) and abs(acc - n - mark) < abs(acc - mark):
                    cuts.append(i)
                else:
                    cuts.append(i + 1)
                want += 1
        bounds = [0] + sorted(set(c for c in cuts if 0 < c < len(us))) + [len(us)]
        for gi in range(len(bounds) - 1):
            part = us[bounds[gi]:bounds[gi + 1]]
            if not part:
                continue
            groups.append(dict(name='AR%d%s' % (w, chr(ord('A') + gi) if len(bounds) > 2 else ''), world=w,
                               units=part))
    for n, gr in enumerate(groups):
        us = gr['units']
        gr['lo'], gr['hi'] = us[0]['lo'], us[-1]['hi']
        gr['funcs'] = [x for u in us for x in u['funcs']]
        gr['take'] = [x for x in gr['funcs'] if x not in ours]
        gr['bytes'] = sum(funcs[x].end - x for x in gr['take'])
        gr['areas'] = [k for u in us for k in u['areas']]
        for u in us:
            u['group'] = gr['name']

    # ---- where each start came from ---------------------------------------
    how = collections.defaultdict(list)
    for h, v in all_roots.items():
        r = resolve(h) if BAND_LO <= h < BAND_HI else None
        for k, what in v:
            if r is not None:
                how[r].append(('area %d ' % k if k is not None else '') + what)
    tail_armed = collections.defaultdict(set)
    for x in starts:
        for n in funcs[x].tail_kinds:
            tail_armed[n].add(x)

    # ---- live reach: the route reaches of hidden functions -----------------
    live = collections.defaultdict(set)
    for f in glob.glob(os.path.join(a.analysis, 'hidden_reached_*.json')):
        route = os.path.basename(f)[len('hidden_reached_'):-len('.json')]
        for x in json.load(open(f)):
            live[int(x['entry'], 16) if isinstance(x['entry'], str) else x['entry']].add(route)
    attract = os.path.join(a.analysis, 'pc_hidden_reached.json')
    if os.path.exists(attract):
        for x in json.load(open(attract)):
            live[int(x['entry'], 16) if isinstance(x['entry'], str) else x['entry']].add('attract')
    for f in glob.glob(os.path.join(a.analysis, 'calltrace', '*', 'bof3x.callcounts.tsv')):
        d = os.path.basename(os.path.dirname(f))
        route = d[len('recipe_'):] if d.startswith('recipe_') else ('attract' if d in ('all_a', 'all_b', 'hidden_b') else None)
        if route is None:
            continue
        for line in open(f):
            p = line.split('\t')
            if len(p) >= 3 and p[1] == '0x00000000':
                x = int(p[0], 16)
                if BAND_LO <= x < BAND_HI:
                    live[x].add(route)

    # PSX twins of the roots (analysis/area_pairs*.json, attract-remaining.md section 5)
    psx = {}
    for fn in ('area_pairs.json', 'area_pairs_filled.json'):
        p = os.path.join(a.analysis, fn)
        if os.path.exists(p):
            d = json.load(open(p))
            for x in (d['pairs'] if isinstance(d, dict) else d):
                psx.setdefault(x['pc'], x['psx'])

    # ---- the tables -------------------------------------------------------
    def name_of(x):
        return ours.get(x) or named.get(x, '')

    def reached_cell(x):
        rb = reached_by.get(x, set())
        if not rb:
            return 'none'
        return ','.join(str(k) for k in sorted(rb)) if len(rb) <= 6 else '%d areas' % len(rb)

    def source(x):
        return ('R' if x in recorded else '') + ('H' if x in hidden else '') + ('S' if x in found else '')

    os.makedirs(a.analysis, exist_ok=True)
    rows_tsv = os.path.join(a.analysis, 'area_rows.tsv')
    unit_by_area = {k: u for u in units for k in u['areas']}
    with open(rows_tsv, 'w', encoding='utf-8') as f:
        f.write('area\tworld\tdescriptor\tchoice\thandlers\tinit\tstep_hook\tarrive_hook\tcell_hook\ttail_kinds\t'
                'world_map_record\tdata_roots\tclosure\texclusive\tunit\tgroup\tfirst\tlast\tfunctions\tbytes\tours\t'
                'live\tfunction_starts (addr:size, * ours)\n')
        for k in range(AREAS):
            ch, hd, ini = dstats[k]
            u = unit_by_area.get(k)
            kinds = sorted({n for x in reach.get(k, ()) for n in funcs[x].tail_kinds})
            dr = sum(1 for v in data_roots.values() for j, _ in v if j == k)
            lv = sorted({r for x in (u['funcs'] if u else ()) for r in live.get(x, ())})
            f.write('%d\t%s\t%#x\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' % (
                k, world_of.get(k, ''), descs[k], ch, hd, '%#x' % fields[k][INIT // 4] if ini else '',
                '%#x' % step[k] if k in step else '', '%#x' % arrive[k] if k in arrive else '',
                ','.join('%#x' % h for h in cell.get(k, [])), ','.join(map(str, kinds)),
                ','.join(str(r) for r in range(WM_RECORD_COUNT) if wm_area[r] == k),
                dr, len(reach.get(k, ())), len(exclusive.get(k, ())),
                u['name'] if u else '-', u.get('group', '') if u else '',
                '%#x' % u['funcs'][0] if u else '', '%#x' % u['funcs'][-1] if u else '',
                len(u['funcs']) if u else '', u['bytes'] if u else '', len(u['ours']) if u else '', ','.join(lv),
                ','.join('%X:%X%s' % (x, funcs[x].end - x, '*' if x in ours else '') for x in u['funcs']) if u else ''))
    funcs_tsv = os.path.join(a.analysis, 'area_funcs.tsv')
    with open(funcs_tsv, 'w', encoding='utf-8') as f:
        f.write('start\tsize\tunit\tblock\tgroup\tsource\tours\treached_by\troots\tlive\tcatalogue\n')
        for x in starts:
            u = unit_of(x)
            rb = reached_by.get(x, set())
            kind = 'shared' if len(rb) > 1 else ('exclusive' if rb else 'gap')
            f.write('%#x\t%#x\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' % (
                x, funcs[x].end - x, u['name'] if u else 'TAIL', kind, u.get('group', '') if u else '',
                source(x), ours.get(x, named.get(x, '') and '(%s)' % named[x]), reached_cell(x),
                '; '.join(how.get(x, [])), ','.join(sorted(live.get(x, ()))), label.get(x, '')))

    # ---- one unit ---------------------------------------------------------
    if a.unit:
        want = a.unit.upper()
        if want.isdigit():
            want = 'AREA%03d' % int(want)
        u = next((w for w in units if want in w['name'].split('/') or want == w['name']), None)
        if u is None:
            sys.exit('no unit %s (an area with no exclusive function has no block of its own)' % a.unit)
        if a.clones:
            print_clones(img, [x for x in u['funcs'] if x not in ours], funcs, named, how, reached_by)
            return
        print('%s  %#x..%#x  %d functions, %#x bytes, %d ours; world %s; group %s' % (
            u['name'], u['lo'], u['hi'], len(u['funcs']), u['bytes'], len(u['ours']), u['world'], u.get('group', '')))
        for k in u['areas']:
            ch, hd, ini = dstats[k]
            print('  area %d: descriptor %#x, %d choice, %d handlers%s%s%s%s' % (
                k, descs[k], ch, hd, ', init %#x' % fields[k][INIT // 4] if ini else '',
                ', step hook %#x' % step[k] if k in step else '', ', arrive hook %#x' % arrive[k] if k in arrive else '',
                ', cell hook %s' % ','.join('%#x' % h for h in cell[k]) if k in cell else ''))
        for x in u['funcs']:
            rb = reached_by.get(x, set())
            bits = []
            if how.get(x):
                bits.append('; '.join(how[x]))
            if x in psx:
                bits.append('psx %#x' % psx[x])
            bits.append('reached by ' + reached_cell(x) if rb else 'reached by none (a gap)')
            if live.get(x):
                bits.append('live: ' + ','.join(sorted(live[x])))
            if funcs[x].tail_kinds:
                bits.append('arms tail kind %s' % ','.join(map(str, sorted(funcs[x].tail_kinds))))
            print('  %#x %5x %-26s %s' % (x, funcs[x].end - x, name_of(x) + ('*' if x in ours else ''), '; '.join(bits)))
        return

    # ---- the report -------------------------------------------------------
    total_ours = sum(1 for x in starts if x in ours)
    print('band %#x..%#x: %d starts recorded or hidden, %d ours; %d dropped (inside another: a case, fallen into, '
          'before its jump table): %s; %d found (a direct call, a tail jmp, a table entry, after a jump table): %s; '
          'discovery and data blocks to a fixpoint in %d rounds'
          % (BAND_LO, BAND_HI, len(band), sum(1 for x in band if x in ours), len(dropped),
             ' '.join('%#x' % x for x in dropped), len(found), ' '.join('%#x' % x for x in found), rounds))
    print('functions: %d, %d ours; area code ends at %#x: past it %d starts no area reaches (%s)' % (
        len(starts), total_ours, tail_lo, len(tail_funcs),
        ', '.join('%s %d' % kv for kv in collections.Counter(label.get(x, 'found') for x in tail_funcs).most_common())))
    lab = collections.Counter(label.get(x, 'found by the walk') for x in starts if x < tail_lo)
    print('catalogue labels in the area band: %s' % ', '.join('%s %d' % kv for kv in lab.most_common()))
    print()
    nch = sum(s[0] for s in dstats.values())
    nhd = sum(s[1] for s in dstats.values())
    nin = sum(s[2] for s in dstats.values())
    in_band = lambda hs: {h for h in hs if BAND_LO <= h < BAND_HI}
    print('roots:')
    print('  descriptors: +0x34 choice %d entries over %d areas, +0x3C handlers %d over %d areas, +0x40 init %d; '
          '%d distinct' % (nch, sum(1 for s in dstats.values() if s[0]), nhd, sum(1 for s in dstats.values() if s[1]),
                           nin, len({h for h, v in roots.items() if any(w.startswith('+0x') for _, w in v)})))
    print('  +0x38 set in areas %s (a colour matrix in .data, not code)' % [k for k in range(AREAS) if fields[k][0x38 // 4]])
    print('  step hook %d areas -> %d handlers (%d outside the band: %s); arrive hook %d areas -> %d' % (
        len(step), len(set(step.values())), len(set(step.values()) - in_band(step.values())),
        ' '.join('%#x' % h for h in sorted(set(step.values()) - in_band(step.values()))),
        len(arrive), len(set(arrive.values()))))
    cv = [h for fs in cell.values() for h in fs]
    print('  cell hooks: %d records, %d areas, %d distinct functions, %d in the band (outside: %s)' % (
        len(cv), len(cell), len(set(cv)), len(in_band(cv)), ' '.join('%#x' % h for h in sorted(set(cv) - in_band(cv)))))
    tv = [h for i, h in enumerate(tail) if i and h != BARE_RET]
    unarmed = [i for i in range(1, TAIL_KIND_COUNT) if BAND_LO <= tail[i] < BAND_HI and not tail_armed.get(i)]
    print('  tail kinds: %d slots, %d set, %d distinct, %d in the band; armed by band code (mov byte [0x9039F3], n): '
          '%d kinds; band slots no band code arms: %s' % (
              TAIL_KIND_COUNT, len(tv), len(set(tv)), len(in_band(tv)),
              len([n for n in tail_armed if 0 < n < TAIL_KIND_COUNT]), ' '.join('%d:%#x' % (i, tail[i]) for i in unarmed)))
    print('  object triggers: %d ids, %d distinct, %d in the band' % (
        len(triggers), len(set(triggers)), len(in_band(triggers))))
    wv = [h for r in wm_code for h in r if h] + wm_hooks
    print('  world map: %d records (areas %s), %d code fields + %d field hooks, %d distinct, %d in the band' % (
        WM_RECORD_COUNT, ','.join(map(str, wm_area)), sum(1 for r in wm_code for h in r if h), WM_HOOK_COUNT,
        len(set(wv)), len(in_band(wv))))
    dv = [(x, v) for x, v, _ in data_ptrs]
    print('  data blocks %#x..%#x: %d code pointers, %d in a descriptor\'s +0x34 / +0x3C table, %d beyond (%d distinct '
          'targets); %d moved off the descriptor order by the walk (area from area: pointers): %s' % (
              region_lo, region_hi, len(dv), sum(1 for x, _ in dv if x in desc_table_words),
              sum(1 for x, _ in dv if x not in desc_table_words), len({v for x, v in dv if x not in desc_table_words}),
              len(moved), ', '.join('%d from %d: %d' % (j, k, n) for (k, j), n in sorted(
                  collections.Counter((k, j) for _, _, k, j in moved).items(), key=lambda kv: (-kv[1], kv[0])))))
    oc = {x: [c for c in v if not any(lo <= c < hi for lo, hi in switches)] for x, v in outside_calls.items()}
    oc = {x: v for x, v in oc.items() if v}
    print('  outside the seven: %d band starts called from outside the band, not by the hook switches (%s); %d named '
          'by .data outside the descriptor region and the tables above (%s)' % (
              len(oc), ' '.join('%#x<-%s' % (x, ','.join('%#x' % s for s in v[:2])) for x, v in sorted(oc.items())),
              len([x for x in outside_data if x < tail_lo]), ' '.join('%#x@%s' % (x, ','.join('%#x' % s for s in v[:2]))
                                                                  for x, v in sorted(outside_data.items()) if x < tail_lo)))
    print()
    reached = {x for x in starts if reached_by.get(x)}
    area_less = {x for x in reach.get(None, set()) if x < tail_lo}
    print('reach: %d of %d functions before %#x reached by some area; %d only by area-less roots (object triggers, '
          'callers and .data outside the band, tail kinds no code arms, the no-world-map hook); %d reached by none' % (
              len(reached), len([x for x in starts if x < tail_lo]), tail_lo, len(area_less - reached),
              len([x for x in starts if x < tail_lo and x not in reached and x not in area_less])))
    ex = {k: len(v) for k, v in exclusive.items()}
    shared = [x for x in starts if len(reached_by.get(x, ())) > 1]
    shc = collections.Counter(len(reached_by[x]) for x in shared)
    print('areas with band code: %d (with an exclusive function: %d); exclusive per area median %s, mean %.1f, max %d' % (
        sum(1 for k in range(AREAS) if reach.get(k)), len(ex), median(list(ex.values())),
        sum(ex.values()) / max(1, len(ex)), max(ex.values())))
    print('largest: %s' % ', '.join('%d (%d)' % (k, n) for k, n in sorted(ex.items(), key=lambda kv: -kv[1])[:8]))
    print('shared bodies (reached by two or more areas): %d; by count of areas: %s' % (
        len(shared), ', '.join('%d: %d' % kv for kv in sorted(shc.items()))))
    order = [k for _, k in firsts]
    inv = [(order[i], order[i + 1]) for i in range(len(order) - 1) if order[i + 1] < order[i]]
    lasts = {k: max(xs) for k, xs in exclusive.items()}
    ovl = [(firsts[i][1], firsts[i + 1][1]) for i in range(len(firsts) - 1) if lasts[firsts[i][1]] > firsts[i + 1][0]]
    print('blocks out of area order: %d (%s); overlapping the next: %d (%s); merged into one unit: %s' % (
        len(inv), ', '.join('%d before %d' % p for p in inv), len(ovl), ', '.join('%d/%d' % p for p in ovl),
        ', '.join('/'.join(map(str, m)) for m in merged_pairs) or 'none'))
    gaps = [x for x in starts if x < tail_lo and not reached_by.get(x)]
    print('gaps (reached by no area, assigned to the block they lie in): %d, %d ours' % (
        len(gaps), sum(1 for x in gaps if x in ours)))
    fr = set().union(*[front[k] for k in front if k is not None])
    fr_ours = {x for x in fr if x in ours}
    frl = collections.Counter(('ours' if x in ours else (label.get(x) or ('named' if x in named else 'unlabelled')))
                              for x in fr)
    print('frontier (outside the band, called or named by area code): %d functions, %d ours by name; %s' % (
        len(fr), len(fr_ours), ', '.join('%s %d' % kv for kv in frl.most_common(10))))
    outside_area = [x for x, lb in label.items() if lb.startswith('Area overlays') and not BAND_LO <= x < BAND_HI]
    print('catalogue "Area overlays" outside the band: %d; reached by an area: %d' % (
        len(outside_area), sum(1 for x in outside_area if x in fr)))
    lv = collections.defaultdict(set)
    for u in units:
        for x in u['funcs']:
            for r in live.get(x, ()):
                lv[r].add(u['name'])
    print('units a recorded route reaches (hidden_reached_*.json, calltrace): %s' % (
        '; '.join('%s: %s' % (r, ', '.join(sorted(v))) for r, v in sorted(lv.items())) or 'none'))
    print()
    worlds = collections.defaultdict(list)
    for u in units:
        if u['areas']:
            worlds[u['world']].append(u)
    for w in sorted(worlds):
        us = worlds[w]
        n = sum(len(u['funcs']) for u in us)
        o = sum(len(u['ours']) for u in us)
        lbl = collections.Counter(label.get(x, '') for u in us for x in u['funcs'])
        print('world %d: %#x..%#x, %d units, %d functions, %d ours, %d to take; catalogue: %s' % (
            w, us[0]['lo'], us[-1]['hi'], len(us), n, o, n - o, ', '.join('%s %d' % kv for kv in lbl.most_common(4))))
    if a.groups:
        print()
        print_groups(groups, funcs, live)
    elif not a.quiet:
        print()
        print('%-24s %-9s %-9s %4s %4s %7s %4s  %s' % ('unit', 'first', 'hi', 'fns', 'excl', 'bytes', 'ours', 'group'))
        for u in units:
            print('%-24s %#-9x %#-9x %4d %4d %7d %4d  %s' % (
                u['name'][:24], u['lo'], u['hi'], len(u['funcs']), sum(ex.get(k, 0) for k in u['areas']),
                u['bytes'], len(u['ours']), u.get('group', '')))
    print('\nwrote %s, %s' % (rows_tsv, funcs_tsv))


def print_groups(groups, funcs, live):
    print('| Group | World | Areas | Band | Fns | Ours | To take | Bytes to take | Live |')
    print('|---|--:|---|---|--:|--:|--:|--:|---|')
    for g in groups:
        ar = g['areas']
        lv = sorted({r for x in g['funcs'] for r in live.get(x, ())})
        print('| %s | %d | %s | `%#x..%#x` | %d | %d | %d | %d | %s |' % (
            g['name'], g['world'], _ranges(ar), g['lo'], g['hi'], len(g['funcs']), len(g['funcs']) - len(g['take']),
            len(g['take']), g['bytes'], ','.join(lv) or '-'))


def _ranges(xs):
    xs = sorted(xs)
    out, i = [], 0
    while i < len(xs):
        j = i
        while j + 1 < len(xs) and xs[j + 1] == xs[j] + 1:
            j += 1
        out.append(str(xs[i]) if i == j else '%d..%d' % (xs[i], xs[j]))
        i = j + 1
    return ', '.join(out)


def print_clones(img, addrs, funcs, named, how, reached_by):
    """C++ for a group's clone table (area_harness.h, magic_harness's API one
    for one), by capstone: magic_rows.clone_sites, the spell harness's own.
    The comment line says which root table each root came from - the call
    shape the harness must use - and what a clone cannot move."""
    lines = []
    for x in addrs:
        end = funcs[x].end
        calls, imms, tables, refused = mr.clone_sites(img, x, end)
        tag = '%X' % x
        name = named.get(x, 'Fn_' + tag)
        shape = '; '.join(how.get(x, [])) or ('reached by area %s' % ','.join(map(str, sorted(reached_by.get(x, ()))))
                                             if reached_by.get(x) else 'a gap: reached by no area')
        codes = sorted(v for v in funcs[x].imms if v not in {c[1] for c in calls})
        print('// 0x%X: 0x%X bytes; %s%s%s' % (x, end - x, shape, ''.join(
            ('; +0x%X %s' if r[1].startswith('note') else '; REFUSED +0x%X %s') % r for r in refused),
            ('; note: code immediates %s (stored or pushed, not re-aimed)' % ' '.join('%#x' % v for v in codes)) if codes else ''))
        if calls:
            print('constexpr area_harness::CallSite kCalls%s[] = {%s};' % (
                tag, ', '.join('{0x%X, 0x%X}' % c for c in calls)))
        if imms:
            print('constexpr area_harness::Imm kImms%s[] = {%s};' % (tag, ', '.join('{0x%X, 0x%X}' % i for i in imms)))
        if tables:
            print('constexpr area_harness::JumpTable kTables%s[] = {%s};' % (
                tag, ', '.join('{0x%X, 0x%X, %d}' % t for t in tables)))
        cell = lambda v, k: ('k%s%s, AH_N(k%s%s)' % (k, tag, k, tag)) if v else 'nullptr, 0'
        lines.append('    {"%s", 0x%X, 0x%X, %s, %s, %s, reinterpret_cast<const void*>(&::%s)},' % (
            name, x, end - x, cell(calls, 'Calls'), cell(imms, 'Imms'), cell(tables, 'Tables'), name))
    print('#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])')
    print('const area_harness::Clone kClones[] = {')
    for line in lines:
        print(line)
    print('};')


if __name__ == '__main__':
    main()
