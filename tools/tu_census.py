#!/usr/bin/env python
"""Recover Capcom's translation units in BOF3.exe's game code (the source-layout round, SL1).

A unit is one object file of the 2001 MSVC6 link: a contiguous `.text` range
plus the `.data` / `.rdata` / `.bss` ranges its statics came from
(docs/source-layout-round.md 1). This tool reads `bof3/BOF3.exe` and
`symbols.toml` and writes `analysis/tu_layout.tsv`: one row per `[[func]]`
with an `impl` in the game's range `0x401000..0x5AB7E0`, its unit, the unit's
`.text` range, a confidence and the evidence. It writes addresses, our names
and counts only - no byte of the game (CLAUDE.md rule 1).

The signals, each measured by this tool (`--report` prints the numbers, and
calibrates each placing signal on PSX-joined runs, one unit, against PSX
image cuts, two units):

1. **PSX order** (`symbols.toml` `psx` / `psx_overlay`). Two PC functions in
   a row whose PSX twins are in the same image and in the same order, the
   second within `PSX_STEP` bytes of the first, are one file on both sides.
   PC functions with no twin between two such neighbours are inside that file
   too. A twin is (overlay, address): overlays load at shared addresses. A
   twin two PC functions claim (a helper copied per overlay) is left out.
   **Joined.** Neighbouring twins in different overlays, or a PSX step back
   by more than `SWAP_TOL`, are a **hard cut**: the two ends never share a
   unit (with twinless functions between, the cut is somewhere among them).
2. **Unit-local statics.** An initialised `.data` or `.rdata` variable (a
   string, a table, a static) read only by functions at most `LOCAL_SPAN`
   apart, or a `.bss` file static (below `BSS_STATIC_END`) read only by
   functions at most 40 apart, is one unit's: its readers are one unit. x87
   operands in `.rdata` are left out (`__real@` constants are COMDATs the
   linker folds across units), and so is `.bss` from `BSS_STATIC_END` on: it
   is not in object order (COMMON symbols and the port's large blocks).
   **Joined.** Where statics join and a hard cut cuts, a `.bss` static or two
   statics win, one `.data` static loses; both are listed (`--uncertain`).
3. **Data order.** The linker lays `.data` and `.bss` statics in the same
   object order as `.text`: a later piece whose static sits below the
   earlier piece's is the same unit. **Placed.**
4. **Local code-pointer tables**: a run of cells in `.data` naming functions
   at most `TABLE_SPAN` apart. **Placed.** (Cells by themselves are not used:
   global dispatch tables list many units.)
5. **The call graph.** A piece of at most three functions called only from
   its neighbour is that neighbour's `static` helper. **Placed.** Each row's
   evidence ends with `link static?` (called only inside its unit), `extern`
   or `uncalled`, for stage B.
6. **Switch tables** in `.text` are found per function and their bytes are
   not decoded as code; one outside its function's extent would be reported.
7. **Our names and one call** for the rest: neighbours with the same name
   prefix or stem, or a call between them, and one- or two-function
   stragglers that share their neighbour's first name word, unless a hard
   cut or two known PSX overlays forbid it. **Guessed.**

The assumption under all of these - a unit's functions are contiguous in
`.text` and in source order - is tested first against the PSX twins
(`--report`, "contiguity test").

    python tools/tu_census.py --exe bof3/BOF3.exe            # write the TSV and the sheet
    python tools/tu_census.py --exe bof3/BOF3.exe --report   # the measurements
    python tools/tu_census.py --check                        # CI: the TSV covers every impl function

`--check` reads only `symbols.toml` and the TSV (no game file), so CI can run
it. The facts read out of the binary are cached in
`analysis/tu_census_cache.json` (gitignored, derived from game code).
"""
import argparse
import bisect
import collections
import json
import os
import re
import struct
import sys
import tomllib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

GAME_LO, GAME_HI = 0x401000, 0x5AB7E0      # the game's code (round doc 0)
PSX_STEP = 0x2000                           # same file: next PSX twin within this
LOCAL_SPAN = 12                             # a static read inside this many functions
TSV = os.path.join(ROOT, 'analysis', 'tu_layout.tsv')
SHEET = os.path.join(ROOT, 'analysis', 'tu_layout_sheet.md')
PSX_TSV = os.path.join(ROOT, 'analysis', 'tu_psx.tsv')   # SL2's table (tools/tu_psx.py)


def load_psx_units(path):
    """SL2's PSX unit of each PC function with a twin: {pc: (psx_unit, agreement)}.
    Empty when the table is absent (the column then stays empty)."""
    if not path or not os.path.exists(path):
        return {}
    out = {}
    with open(path, encoding='utf-8') as fh:
        head = None
        for line in fh:
            if line.startswith('#') or not line.strip():
                continue
            cols = line.rstrip('\n').split('\t')
            if head is None:
                head = cols
                continue
            r = dict(zip(head, cols))
            out[int(r['pc'], 16)] = (r['psx_unit'], r['agreement'], r.get('psx', ''))
    return out


def psx_checks(M, units, psx):
    """For each of our units: the PSX units (of two or more functions) whose
    functions this unit shares with another of our units - a cut SL2 does
    not support. {unit index: [(psx_unit, [other unit ids])]}."""
    where = collections.defaultdict(set)      # psx unit -> our unit indices
    twins = collections.defaultdict(set)      # psx unit -> distinct PSX functions twinned
    for k, u in enumerate(units):
        for i in range(u['lo'], u['hi'] + 1):
            p = psx.get(M.g[i]['pc'])
            if p and p[0]:                    # an empty unit key is "no PSX unit", not one unit
                where[p[0]].add(k)
                twins[p[0]].add(p[2] if len(p) > 2 and p[2] else M.g[i]['pc'])
    out = collections.defaultdict(list)
    for pu, ks in where.items():
        # two or more *PlayStation* functions: two PC copies twinned to one PSX function are
        # one function (2026-11-08: 14 one-function flags read that way before)
        if len(twins[pu]) >= 2 and len(ks) > 1:
            for k in ks:
                out[k].append((pu, sorted(unit_id(M, units[x]) for x in ks if x != k)))
    return out
CACHE = os.path.join(ROOT, 'analysis', 'tu_census_cache.json')
COLUMNS = ['pc', 'name', 'unit', 'unit_lo', 'unit_hi', 'confidence', 'evidence',
           'impl', 'proposed_file', 'psx_unit_agreement']


# ---------------------------------------------------------------------------
# symbols.toml
# ---------------------------------------------------------------------------

def load_symbols(path):
    with open(path, 'rb') as fh:
        d = tomllib.load(fh)
    funcs = sorted(d['func'], key=lambda f: f['pc'])
    return funcs, d.get('data', []), d.get('block', [])


def game_funcs(funcs):
    return [f for f in funcs if GAME_LO <= f['pc'] < GAME_HI]


# ---------------------------------------------------------------------------
# The binary: one pass of capstone over every [[func]] extent
# ---------------------------------------------------------------------------

def extract(exe, funcs):
    """Per function (keyed by pc): data reads, direct calls and tail jumps,
    code pointers, switch tables. Only addresses are kept."""
    import capstone
    from capstone import x86
    from pe_hidden import Image, sweep

    img = Image(exe)
    secs = {s['name']: s for s in img.secs}
    rd, da = secs['.rdata'], secs['.data']
    RD = (rd['va'], rd['va'] + rd['vsz'])
    DA = (da['va'], da['va'] + da['vsz'])
    TX = (img.text['va'], img.text['va'] + img.text['vsz'])
    starts = [f['pc'] for f in funcs if TX[0] <= f['pc'] < TX[1]]
    startset = set(starts)
    ends = starts[1:] + [TX[1]]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    out = {}
    for a, b in zip(starts, ends):
        code = img.data[img.off(a):img.off(a) + (b - a)]
        ins_all, _ = sweep(md, code, a)
        # switch tables: jmp [reg*4 + disp] / mov reg,[reg*4 + disp] with disp in .text
        tables = []
        for ins in ins_all:
            if ins.mnemonic not in ('jmp', 'mov', 'call'):
                continue
            for op in ins.operands:
                if op.type == x86.X86_OP_MEM and op.mem.base == 0 and op.mem.scale == 4 \
                        and op.mem.index != 0:
                    t = op.mem.disp & 0xFFFFFFFF
                    if TX[0] <= t < TX[1]:
                        n = 0
                        while t + 4 * n < TX[1]:
                            o = img.off(t + 4 * n)
                            c = struct.unpack_from('<I', img.data, o)[0]
                            if not (a <= c < b) or (n and t + 4 * n in startset):
                                break
                            n += 1
                        tables.append((ins.address, t, n))
        inside = [t for _, t, n in tables if a <= t < b and n]
        mask = min(inside) if inside else b
        reads, calls, tails, codeptrs = [], [], [], []
        for ins in ins_all:
            if ins.address >= mask:
                break
            fpu = ins.mnemonic.startswith('f')
            for op in ins.operands:
                v = None
                if op.type == x86.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                    v = op.mem.disp & 0xFFFFFFFF
                elif op.type == x86.X86_OP_MEM and op.mem.base == 0:
                    v = op.mem.disp & 0xFFFFFFFF      # indexed: the table's base
                elif op.type == x86.X86_OP_MEM and op.mem.disp:
                    v = op.mem.disp & 0xFFFFFFFF      # [reg + disp]: a static array
                elif op.type == x86.X86_OP_IMM:
                    v = op.imm & 0xFFFFFFFF
                if v is None:
                    continue
                if RD[0] <= v < RD[1] or DA[0] <= v < DA[1]:
                    reads.append([v, 1 if fpu else 0])
                elif TX[0] <= v < TX[1]:
                    if ins.mnemonic == 'call' and op.type == x86.X86_OP_IMM:
                        calls.append(v)
                    elif ins.mnemonic == 'jmp' and op.type == x86.X86_OP_IMM:
                        if not (a <= v < b):
                            tails.append(v)
                    elif ins.mnemonic.startswith('j'):
                        pass
                    elif v in startset:
                        codeptrs.append(v)
        out[a] = dict(end=b, reads=reads, calls=sorted(set(calls)), tails=sorted(set(tails)),
                      codeptrs=sorted(set(codeptrs)),
                      tables=[[s, t, n] for s, t, n in tables])
    # code pointers held in initialised .data / .rdata: cell address -> function
    dptr = []
    for s in (rd, da):
        raw = img.data[s['raw']:s['raw'] + min(s['rsz'], s['vsz'])]
        for o in range(0, len(raw) - 3, 4):
            c = struct.unpack_from('<I', raw, o)[0]
            if c in startset:
                dptr.append([s['va'] + o, c])
    meta = dict(rdata=RD, data=DA, data_init_end=da['va'] + da['rsz'], text=TX)
    return dict(meta=meta, funcs={str(k): v for k, v in out.items()}, dptr=dptr)


def load_facts(exe, funcs, refresh=False):
    if not refresh and os.path.exists(CACHE):
        with open(CACHE) as fh:
            facts = json.load(fh)
        if len(facts['funcs']) == sum(1 for f in funcs if facts['meta']['text'][0] <= f['pc'] < facts['meta']['text'][1]):
            return facts
    if not exe or not os.path.exists(exe):
        sys.exit(f'tu_census: no cache and no game file at {exe!r} (pass --exe bof3/BOF3.exe)')
    facts = extract(exe, funcs)
    os.makedirs(os.path.dirname(CACHE), exist_ok=True)
    with open(CACHE, 'w') as fh:
        json.dump(facts, fh)
    return facts


# ---------------------------------------------------------------------------
# The model: functions, images, readers
# ---------------------------------------------------------------------------

BSS_STATIC_END = 0x6CA000    # measured: .bss below this is in object order (--report)
SWAP_TOL = 0x800             # a PSX step back by less than this is a local swap, not a cut


class Model:
    def __init__(self, funcs, data, facts, span=LOCAL_SPAN):
        self.all = funcs
        self.g = game_funcs(funcs)
        self.n = len(self.g)
        self.idx = {f['pc']: i for i, f in enumerate(self.g)}
        self.facts = facts
        m = facts['meta']
        self.RD, self.DA = m['rdata'], m['data']
        self.data_init_end = m['data_init_end']
        self.span = span
        self.named = sorted(d['pc'] for d in data)
        self.dname = {d['pc']: d['name'] for d in data}
        self._images()
        self._readers()
        self._calls()

    # PSX images: the overlay when symbols.toml has it, else inherited along a
    # PC-order chain of close twins, else 'main?'
    def _images(self):
        g = self.g
        # PSX overlays load at shared addresses, so a twin is (overlay, address);
        # one twin claimed by two PC functions (a helper copied per overlay and
        # paired to one copy) says nothing about either's place
        key = lambda f: (f.get('psx_overlay'), f['psx'])
        dup = collections.Counter(key(f) for f in g if isinstance(f.get('psx'), int))
        self.twin = [f['psx'] if isinstance(f.get('psx'), int) and dup[key(f)] == 1 else None
                     for f in g]
        self.twin_dup = sum(1 for f in g if isinstance(f.get('psx'), int) and dup[key(f)] > 1)
        img = [f.get('psx_overlay') if self.twin[i] is not None else None for i, f in enumerate(g)]
        known = [x is not None for x in img]
        paired = [i for i in range(self.n) if self.twin[i] is not None]
        for order in (paired, paired[::-1]):
            prev = None
            for i in order:
                if prev is not None and img[i] is None and img[prev] is not None:
                    if abs(self.twin[i] - self.twin[prev]) < PSX_STEP:
                        img[i] = img[prev]
                prev = i
        self.img = [x if x is not None else ('main?' if self.twin[i] is not None else None)
                    for i, x in enumerate(img)]
        self.img_known = known

    def region(self, a):
        if self.RD[0] <= a < self.RD[1]:
            return 'rdata'
        if self.DA[0] <= a < self.data_init_end:
            return 'data'
        if self.data_init_end <= a < BSS_STATIC_END:
            return 'bss'
        if BSS_STATIC_END <= a < self.DA[1]:
            return 'common'
        return None

    def varkey(self, a):
        """A read address -> the variable it belongs to: the named [[data]]
        holding it (within 0x40 of its start: a record's fields), else the
        address itself. Wider grouping merged the tables of neighbouring units,
        which sit back to back in .data."""
        i = bisect.bisect_right(self.named, a) - 1
        if i >= 0:
            d = self.named[i]
            nxt = self.named[i + 1] if i + 1 < len(self.named) else 1 << 32
            if a < min(nxt, d + 0x40):
                return d
        return a

    def _readers(self):
        self.readers = collections.defaultdict(set)      # var -> function indices
        self.fpu_only = collections.defaultdict(lambda: True)
        for k, v in self.facts['funcs'].items():
            i = self.idx.get(int(k))
            if i is None:
                continue
            for a, fpu in v['reads']:
                key = self.varkey(a)
                self.readers[key].add(i)
                if not fpu:
                    self.fpu_only[key] = False
        # code pointers in initialised data: function -> the cells naming it
        # (a data position, not a join: tables of neighbouring units abut)
        self.cells = collections.defaultdict(list)
        for cell, fn in self.facts['dptr']:
            if fn in self.idx:
                self.cells[self.idx[fn]].append(cell)

    def _calls(self):
        self.callers = collections.defaultdict(set)
        self.callees = collections.defaultdict(set)
        for k, v in self.facts['funcs'].items():
            i = self.idx.get(int(k))
            if i is None:
                continue
            for t in v['calls'] + v['tails'] + v['codeptrs']:
                j = self.idx.get(t)
                if j is not None and j != i:
                    self.callers[j].add(i)
                    self.callees[i].add(j)

    def close(self, i, j):
        """i before j in PC order: same PSX image, j's twin after i's within PSX_STEP."""
        a, b = self.img[i], self.img[j]
        if a != b and not (a == 'main?' or b == 'main?'):
            return False
        return 0 < self.twin[j] - self.twin[i] < PSX_STEP


# ---------------------------------------------------------------------------
# Edges: between function i and i+1, what joins them and what cuts them
# ---------------------------------------------------------------------------

def psx_edges(M):
    """PSX joins (edge ranges [i, j) covered by a close twin pair with only
    twinless functions between) and cuts (neighbouring twins not close)."""
    joins, cuts = [], {}
    paired = [i for i in range(M.n) if M.twin[i] is not None]
    for i, j in zip(paired, paired[1:]):
        if M.close(i, j):
            joins.append((i, j, f'psx {M.img[i]} {M.twin[i]:#x}>{M.twin[j]:#x}'))
            continue
        a, b = M.img[i], M.img[j]
        if a != b and M.img_known[i] and M.img_known[j]:
            kind, why = 'strong', f'psx image {a} | {b}'
        elif a == b or 'main?' in (a, b):
            d = M.twin[j] - M.twin[i]
            if -SWAP_TOL < d <= 0:
                continue                     # a local swap (contiguity test)
            kind = 'medium' if d < 0 else 'weak'
            why = f'psx {"back" if d < 0 else "far"} {M.twin[i]:#x}>{M.twin[j]:#x}'
        else:
            kind, why = 'medium', f'psx image {a} | {b}'
        # the cut lies somewhere in i..j; with nothing between it is edge i
        cuts[(i, j)] = (kind, why)
    return joins, cuts


def static_joins(M, span=None, bss_span=40):
    """Unit-local statics: (lo, hi, evidence) per variable whose readers span
    at most `span` functions (`bss_span` for .bss file statics)."""
    span = span or M.span
    out = []
    for key, rs in M.readers.items():
        reg = M.region(key)
        if reg in (None, 'common') or len(rs) < 2:
            continue
        if reg == 'rdata' and M.fpu_only[key]:
            continue
        lim = bss_span if reg == 'bss' else span
        lo, hi = min(rs), max(rs)
        if hi - lo <= lim:
            out.append((lo, hi, f'{reg} {key:#x}'))
    return out


# ---------------------------------------------------------------------------
# Pieces: joined segments, then placement, then the guessed remainder
# ---------------------------------------------------------------------------

RANK = {'joined': 0, 'placed': 1, 'guessed': 2, None: 3}


def weaker(a, b):
    return a if RANK[a] >= RANK[b] else b


def prefix(name):
    return name.split('_', 1)[0]


def coarse(name):
    """The prefix without its trailing number and letter: PartyAction8 ->
    PartyAction, EffectKind73B -> EffectKind, Area27 -> Area."""
    return re.sub(r'([0-9][0-9A-F]*[A-Z]?|[0-9]+)$', '', prefix(name)) or prefix(name)


def head(name):
    """The first word of the prefix: PartyFormAction8 -> Party."""
    m = re.match(r'[A-Z]?[a-z]+|[A-Z]+(?![a-z])', prefix(name))
    return m.group(0) if m else prefix(name)


TABLE_SPAN = 8   # a code-pointer table naming functions at most this far apart


def table_edges(M):
    """Edges covered by a local code-pointer table in .data / .rdata: a run
    of cells (each within 8 bytes of the last) whose functions lie at most
    TABLE_SPAN apart. Calibrated in --report."""
    cells = sorted((c, M.idx[f]) for c, f in M.facts['dptr'] if f in M.idx)
    runs, cur = [], cells[:1]
    for x, y in zip(cells, cells[1:]):
        if y[0] - x[0] <= 8:
            cur.append(y)
        else:
            runs.append(cur)
            cur = [y]
    if cur:
        runs.append(cur)
    cov = collections.defaultdict(list)
    for r in runs:
        ix = [i for _, i in r]
        if len(set(ix)) >= 2 and max(ix) - min(ix) <= TABLE_SPAN:
            for e in range(min(ix), max(ix)):
                cov[e].append(r[0][0])
    return cov, len(runs)


class Piece:
    __slots__ = ('lo', 'hi', 'how', 'why')

    def __init__(self, lo, hi, how, why):
        self.lo, self.hi, self.how, self.why = lo, hi, how, why


def build(M):
    n = M.n
    pj, pcuts = psx_edges(M)
    sj = static_joins(M)
    jp = [[] for _ in range(n)]
    js = [[] for _ in range(n)]
    for i, j, why in pj:
        for e in range(i, j):
            jp[e].append(why)
    for lo, hi, why in sj:
        for e in range(lo, hi):
            js[e].append(why)
    disagree = []          # (edge or range, text)
    inside = [False] * n   # edge e joins function e and e+1 (e < n-1)
    for e in range(n - 1):
        inside[e] = bool(jp[e] or js[e])
    hard = []              # cut ranges (i, j): i and j may never share a unit
    for (i, j), (kind, why) in sorted(pcuts.items()):
        covered = all(js[e] for e in range(i, j))
        if not covered:
            if kind in ('strong', 'medium'):
                hard.append((i, j, kind, why))
            continue
        # every edge of the range is joined by statics: the signals disagree
        ev = sorted({w for e in range(i, j) for w in js[e]})
        strong_static = any(w.startswith('bss') for w in ev) or len(ev) >= 2
        if kind == 'weak' or strong_static:
            verdict = 'joined (statics)'
        else:
            verdict = 'cut (psx)'
            # cut at the edge with the fewest static joins
            e = min(range(i, j), key=lambda e: (len(js[e]), e))
            inside[e] = False
            hard.append((i, j, kind, why))
        disagree.append(dict(i=i, j=j, kind=kind, psx=why, statics=ev, verdict=verdict))
    # joined segments
    pieces, start = [], 0
    for e in range(n - 1):
        if not inside[e]:
            pieces.append([start, e])
            start = e + 1
    pieces.append([start, n - 1])
    seg_of = [0] * n
    for k, (lo, hi) in enumerate(pieces):
        for i in range(lo, hi + 1):
            seg_of[i] = k
    return dict(pj=pj, pcuts=pcuts, sj=sj, jp=jp, js=js, inside=inside, hard=hard,
                disagree=disagree, segments=pieces, seg_of=seg_of)


def positions(M):
    """function -> {region: [addresses]}: the unit-local statics it reads
    (readers span at most M.span functions, one reader included) and the
    data cells that name it."""
    pos = [collections.defaultdict(list) for _ in range(M.n)]
    for key, rs in M.readers.items():
        reg = M.region(key)
        if reg in (None, 'common') or (reg == 'rdata' and M.fpu_only[key]):
            continue
        if max(rs) - min(rs) > M.span:
            continue
        for i in rs:
            pos[i][reg].append(key)
    for i, cells in M.cells.items():
        for c in cells:
            reg = M.region(c)
            if reg in ('data', 'rdata'):
                pos[i][reg + '-cell'].append(c)
    return pos


def place(M, B):
    """Merge neighbouring pieces on data order and the call graph (placed),
    then on our names and the PSX image (guessed). Returns units as lists of
    (lo, hi, how, why) pieces in text order, and the signal tallies."""
    pos = positions(M)
    tcov, _ = table_edges(M)
    hard = B['hard']
    # a unit may not hold both ends of a hard cut: for edge e, the hard cuts
    # with i <= e < j
    hard_at = collections.defaultdict(list)
    for h in hard:
        for e in range(h[0], h[1]):
            hard_at[e].append(h)
    units = [dict(lo=a, hi=b, pieces=[(a, b, 'joined' if b > a else None, 'segment')])
             for a, b in B['segments']]
    for u in units:
        if u['pieces'][0][2] is None:
            u['pieces'] = [(u['lo'], u['hi'], None, '')]

    def blocked(u, v):
        # would merging u and v (adjacent) put both ends of a hard cut in one unit?
        for h in hard_at[u['hi']]:
            if u['lo'] <= h[0] and h[1] <= v['hi']:
                return h
        return None

    def upos(u):
        out = collections.defaultdict(list)
        for i in range(u['lo'], u['hi'] + 1):
            for r, xs in pos[i].items():
                out[r].extend(xs)
        return out

    def callers_in(u, v):
        """every caller of u's functions (from outside u) lies in v, and there is one"""
        cs = set()
        for i in range(u['lo'], u['hi'] + 1):
            cs |= {c for c in M.callers[i] if not (u['lo'] <= c <= u['hi'])}
        return bool(cs) and all(v['lo'] <= c <= v['hi'] for c in cs)

    tally = collections.Counter()
    blocked_by = collections.Counter()
    seen_block = set()
    refused = []

    def evidence(u, v):
        # data order: a static of the later piece below one of the earlier
        # piece's in the same section. Calibrated (--report): fires inside
        # PSX-joined runs, almost never across PSX image cuts. Code-pointer
        # cells are not used: they fire more often across cuts than inside.
        pu, pv = upos(u), upos(v)
        for r in ('data', 'bss', 'rdata'):
            a, b = pu.get(r, []), pv.get(r, [])
            if a and b and min(b) < max(a):
                return 'placed', f'{r} order: {min(b):#x} < {max(a):#x}'
        if tcov.get(u['hi']):
            return 'placed', f'table {tcov[u["hi"]][0]:#x} names both'
        small = lambda w: w['hi'] - w['lo'] < 3
        if small(v) and callers_in(v, u):
            return 'placed', f'called only from {M.g[u["lo"]]["name"]}..'
        if small(u) and callers_in(u, v):
            return 'placed', f'called only from {M.g[v["lo"]]["name"]}..'
        return None

    def merge_pass(test, label):
        changed = True
        while changed:
            changed = False
            out = [units[0]]
            for v in units[1:]:
                u = out[-1]
                ev = test(u, v)
                if ev:
                    h = blocked(u, v)
                    if h:
                        key = (u['lo'], v['lo'], label)
                        if key not in seen_block:
                            seen_block.add(key)
                            blocked_by[(label, ev[1].split(':')[0].split(' ')[0])] += 1
                            refused.append((u['hi'], label, ev[1], h))
                        out.append(v)
                        continue
                    tally[(label, ev[1].split(':')[0].split(' ')[0])] += 1
                    # the smaller side's rows are in this unit only as surely
                    # as the attachment: they take the weaker of the two
                    side = v if (v['hi'] - v['lo']) <= (u['hi'] - u['lo']) else u
                    side['pieces'] = [(a, b, weaker(h2, ev[0]),
                                       (w + '; ' if w and h2 else '') + ev[1])
                                      for a, b, h2, w in side['pieces']]
                    out[-1] = dict(lo=u['lo'], hi=v['hi'], pieces=u['pieces'] + v['pieces'])
                    changed = True
                else:
                    out.append(v)
            units[:] = out

    merge_pass(evidence, 'placed')
    counts = [len(units)]

    # guessed: our names' prefix and the PSX image
    def images(u):
        return {M.img[i] for i in range(u['lo'], u['hi'] + 1) if M.img[i] not in (None, 'main?')}

    def prefixes(u):
        c = collections.Counter(prefix(M.g[i]['name']) for i in range(u['lo'], u['hi'] + 1))
        return c

    def guess(u, v):
        nonlocal stage
        iu, iv = images(u), images(v)
        if iu and iv and not (iu & iv):
            return None
        cu, cv = prefixes(u), prefixes(v)
        top_u, top_v = cu.most_common(1)[0][0], cv.most_common(1)[0][0]
        if top_u == top_v or top_v in cu or top_u in cv:
            return 'guessed', f'name prefix {top_v}'
        ku = {coarse(M.g[i]['name']) for i in range(u['lo'], u['hi'] + 1)}
        kv = collections.Counter(coarse(M.g[i]['name']) for i in range(v['lo'], v['hi'] + 1))
        if kv.most_common(1)[0][0] in ku:
            return 'guessed', f'name stem {kv.most_common(1)[0][0]}'
        if stage == 2 and min(u['hi'] - u['lo'], v['hi'] - v['lo']) < 2:
            # a straggler of one or two functions joins the neighbour whose
            # first name word it shares; two substantive pieces never join on it
            hu = collections.Counter(head(M.g[i]['name']) for i in range(u['lo'], u['hi'] + 1))
            hv = collections.Counter(head(M.g[i]['name']) for i in range(v['lo'], v['hi'] + 1))
            if hu.most_common(1)[0][0] == hv.most_common(1)[0][0]:
                return 'guessed', f'name head {hv.most_common(1)[0][0]}'
        # a call between the two (calibrated: inside runs 28%, across cuts 5%)
        for i in range(v['lo'], v['hi'] + 1):
            for c in M.callers[i]:
                if u['lo'] <= c <= u['hi']:
                    return 'guessed', f'calls {M.g[c]["name"]} > {M.g[i]["name"]}'
        for i in range(u['lo'], u['hi'] + 1):
            for c in M.callers[i]:
                if v['lo'] <= c <= v['hi']:
                    return 'guessed', f'calls {M.g[c]["name"]} > {M.g[i]["name"]}'
        return None

    stage = 1
    merge_pass(guess, 'guessed')
    counts.append(len(units))
    stage = 2
    merge_pass(guess, 'guessed')
    counts.append(len(units))
    B['unit_counts'] = counts
    for u in units:
        u['pieces'] = [(a, b, h or 'guessed', w or 'alone') for a, b, h, w in u['pieces']]
    B['refused'] = refused
    return units, tally, blocked_by


# ---------------------------------------------------------------------------
# Naming: subsystem folders, file names, one-line descriptions
# ---------------------------------------------------------------------------

IMAGE_SUB = {'AREA': 'area', 'MAGIC': 'magic', 'SCENA': 'scenario', 'SCE15EF': 'scenario',
             'SCE10EFF': 'scenario', 'BATTLE': 'battle', 'BATE': 'battle', 'BOSS': 'boss',
             'GAME': 'field', 'COMMU': 'community', 'START': 'menu', 'SHOP': 'shop',
             'SISYOU': 'menu', 'SHISU': 'minigame'}
HEAD_SUB = {
    'Area': 'area', 'World': 'area', 'Effect': 'effect', 'Scena': 'scenario', 'Scenario': 'scenario',
    'Battle': 'battle', 'Cmd': 'battle', 'Escape': 'battle', 'Enemy': 'battle', 'Dragon': 'battle',
    'Stat': 'battle', 'Counter': 'battle', 'Win': 'battle', 'Boss': 'boss',
    'Field': 'field', 'Party': 'field', 'Leader': 'field', 'Member': 'field', 'Encounter': 'field',
    'Move': 'field', 'Event': 'field', 'Map': 'field', 'Camera': 'field', 'Jump': 'field',
    'Sprite': 'field', 'Char': 'field',
    'Magic': 'magic', 'Fx': 'magic', 'Spell': 'magic',
    'Commu': 'community', 'Shop': 'shop', 'Shared': 'shop', 'Inn': 'shop',
    'Menu': 'menu', 'Window': 'menu', 'Item': 'menu', 'Config': 'menu', 'Tactics': 'menu',
    'Ability': 'menu', 'Master': 'menu', 'Gene': 'menu',
    'Msg': 'text', 'Text': 'text', 'Font': 'text', 'Glyph': 'text',
    'D': 'platform', 'Gfx': 'platform', 'Gpu': 'platform', 'Gte': 'platform', 'Display': 'platform',
    'Snd': 'platform', 'Sound': 'platform', 'Music': 'platform', 'File': 'platform', 'Pad': 'platform',
    'Cfg': 'platform', 'Fmv': 'platform', 'Prim': 'platform', 'Dd': 'platform', 'Math': 'platform',
    'Game': 'core', 'Task': 'core', 'Main': 'core', 'Mode': 'core', 'Load': 'core', 'Title': 'core',
    'Transition': 'core',
    'Fish': 'minigame', 'Shisu': 'minigame'}
SUB_TEXT = {
    'area': 'field scripts and hooks of the areas', 'effect': 'field effect kinds',
    'scenario': 'scenario scenes', 'battle': 'the battle engine', 'boss': 'boss scripts',
    'field': 'the field: party, leader, events and movement', 'magic': 'spell and ability effects',
    'community': 'the faerie village', 'shop': 'shops and the inn', 'menu': 'menus and windows',
    'text': 'message boxes and text', 'platform': "the porting house's platform layer",
    'core': 'game modes, title and transitions', 'minigame': 'minigames', 'misc': 'unsorted'}


def snake(s):
    s = re.sub(r'(?<=[a-z])(?=[A-Z0-9])|(?<=[0-9])(?=[A-Z][a-z])|(?<=[A-Z])(?=[A-Z][a-z])', '_', s)
    return s.lower()


def unit_names(M, units):
    """subsystem, stem, description per unit."""
    out = []
    for u in units:
        rng = range(u['lo'], u['hi'] + 1)
        votes = collections.Counter()
        for i in rng:
            im = M.img[i]
            # our names say what the code does; the PSX overlay says where it
            # was loaded (field effects ran from area overlays), so it only
            # decides names the head table does not know (the spells)
            hs = HEAD_SUB.get(head(M.g[i]['name']))
            if hs:
                votes[hs] += 2
            elif M.twin[i] is not None and im not in (None, 'main?'):
                votes[IMAGE_SUB.get(re.sub(r'[0-9]*\.EMI.*$', '', im), 'misc')] += 1
            else:
                votes['misc'] += 1
        sub = votes.most_common(1)[0][0]
        pre = collections.Counter(prefix(M.g[i]['name']) for i in rng)
        top = pre.most_common(3)
        stem = snake(top[0][0])
        imgs = collections.Counter(M.img[i] for i in rng if M.img[i] not in (None, 'main?'))
        out.append(dict(sub=sub, stem=stem, prefixes=top,
                        image=', '.join(k for k, _ in imgs.most_common(2)) or '-'))
    # a family the votes left as misc takes its neighbours' folder
    for k, o in enumerate(out):
        if o['sub'] == 'misc':
            for d in (1, -1, 2, -2):
                if 0 <= k + d < len(out) and out[k + d]['sub'] != 'misc':
                    o['sub'] = out[k + d]['sub']
                    break
    for o in out:
        pp = ', '.join(f'{p}_* {c}' for p, c in o['prefixes'])
        o['desc'] = f"{SUB_TEXT.get(o['sub'], o['sub'])}: {pp}"
    return out


def unit_id(M, u):
    return f"tu_{M.g[u['lo']]['pc']:06x}"


def unit_range(M, u):
    lo = M.g[u['lo']]['pc']
    hi = M.g[u['hi'] + 1]['pc'] if u['hi'] + 1 < M.n else GAME_HI
    return lo, hi


def proposals(M, units, names, hard):
    """Stand alone, or join neighbours in one file, with the reason. A unit is
    never split. Returns per unit (file, proposal, reason)."""
    hard_edges = {i for i, j, *_ in hard if j == i + 1}
    groups = [[0]]
    reasons = {}
    for k in range(1, len(units)):
        a, b = names[k - 1], names[k]
        ha = head(M.g[units[k - 1]['lo']]['name'])
        hb = head(M.g[units[k]['lo']]['name'])
        why = None
        # boss units stand alone (the owner, 2026-11-08): the units are cut one
        # or two bosses each and their PSX twins sit one overlay per boss
        if a['sub'] == b['sub'] == 'boss':
            why = None
        elif {ha, hb} <= {'Font', 'Glyph'}:
            why = "fonts together (the owner's example)"
        elif a['sub'] == b['sub'] and a['stem'] == b['stem']:
            why = f'the same names ({a["stem"]})' if a['sub'] != 'area' else f'one area ({a["stem"]})'
        elif (a['sub'] == b['sub'] and b['sub'] != 'area' and units[k - 1]['hi'] not in hard_edges
              and (units[k]['hi'] == units[k]['lo']
                   or (units[k - 1]['hi'] == units[k - 1]['lo'] and len(groups[-1]) == 1))):
            # a one-function unit joins one neighbour; it never bridges two
            why = 'a one-function unit, nothing separates it from its neighbour'
        if why:
            groups[-1].append(k)
            reasons[k] = why
        else:
            groups.append([k])
    out = [None] * len(units)
    used = collections.Counter()
    for gr in groups:
        n0 = names[gr[0]]
        stem = n0['stem']
        key = (n0['sub'], stem)
        used[key] += 1
        if used[key] > 1:
            stem = f'{stem}_{used[key]}'
        path = f'src/game/{n0["sub"]}/{stem}.cpp'
        ids = [unit_id(M, units[k]) for k in gr]
        why = '; '.join(dict.fromkeys(reasons[k] for k in gr[1:]))
        for k in gr:
            if len(gr) == 1:
                out[k] = (path, 'stand alone', 'its own file: no neighbour shares its names or folder rule')
            else:
                me = unit_id(M, units[k])
                others = [x for x in ids if x != me]
                txt = ('join with ' + ', '.join(others) if len(others) <= 4 else
                       f'join with {len(others)} units ({ids[0]} .. {ids[-1]})')
                out[k] = (path, txt, why)
    return out, groups


# ---------------------------------------------------------------------------
# Rows, uncertain boundaries
# ---------------------------------------------------------------------------

def linkage(M, u, i):
    if any(not (u['lo'] <= c <= u['hi']) for c in M.callers[i]):
        return 'extern'
    if M.callers[i] or i in M.cells:
        return 'static?'
    return 'uncalled'


def rows(M, B, units, files, psx=None):
    psx = psx or {}
    res = {}
    for k, u in enumerate(units):
        uid = unit_id(M, u)
        lo, hi = unit_range(M, u)
        for a, b, how, why in u['pieces']:
            for i in range(a, b + 1):
                ev = []
                if i < M.n - 1 and (B['jp'][i] or B['js'][i]) and i < u['hi']:
                    ev += (B['jp'][i] + B['js'][i])[:2]
                if i > u['lo'] and (B['jp'][i - 1] or B['js'][i - 1]):
                    ev += (B['jp'][i - 1] + B['js'][i - 1])[:1]
                if how != 'joined' or not ev:
                    ev.append(why)
                ev = '; '.join(dict.fromkeys(ev))
                if len(ev) > 300:
                    ev = ev[:297] + '...'
                f = M.g[i]
                res[f['pc']] = dict(pc=f'{f["pc"]:#x}', name=f['name'], unit=uid,
                                    unit_lo=f'{lo:#x}', unit_hi=f'{hi:#x}', confidence=how,
                                    evidence=f'{ev}; link {linkage(M, u, i)}',
                                    impl=f.get('impl', ''), proposed_file=files[k][0],
                                    psx_unit_agreement=(':'.join(psx[f['pc']][:2]) if f['pc'] in psx else ''))
    return res


def uncertain(M, B, units):
    """Every boundary where signals disagree or none decides, scored: the
    higher, the less certain. (score, i, j, text)."""
    edge_unit = {}
    for k, u in enumerate(units):
        for i in range(u['lo'], u['hi'] + 1):
            edge_unit[i] = k
    seen = []
    for d in B['disagree']:
        seen.append((100 + len(d['statics']), d['i'], d['j'],
                     f"statics join, PSX cuts ({d['psx']}); statics {', '.join(d['statics'][:3])}; taken: {d['verdict']}"))
    for i, j, kind, why in B['hard']:
        if j - i > 1:
            cut_at = [e for e in range(i, j) if edge_unit[e] != edge_unit[e + 1]]
            where = ', '.join(M.g[e]['name'] + ' | ' + M.g[e + 1]['name'] for e in cut_at[:2]) or 'none'
            seen.append((60 + min(j - i, 34), i, j,
                         f'{kind} PSX cut ({why}) lies somewhere in {j - i} edges without a twin; '
                         f'the table cuts at {where}'))
    for e, label, why, h in B.get('refused', []):
        if label == 'placed':
            seen.append((95, e, e + 1, f'{why} says one unit; a {h[2]} PSX cut ({h[3]}) says two: kept apart'))
        else:
            seen.append((45, e, e + 1, f'guess ({why}) refused by a {h[2]} PSX cut ({h[3]})'))
    hard_any = set()
    for i, j, *_ in B['hard']:
        hard_any.update(range(i, j))
    for k in range(1, len(units)):
        e = units[k]['lo'] - 1
        if e in hard_any:
            continue
        seen.append((40, e, e + 1, 'unit boundary with no cut evidence: no signal joined the two, none cut them'))
    for u in units:
        for a, b, how, why in u['pieces']:
            if how == 'guessed' and a > u['lo']:
                sc = 35 if 'calls' in why else 30 if 'head' in why else 20
                seen.append((sc, a - 1, a, f'guessed join: {why}'))
    seen.sort(key=lambda x: (-x[0], x[1]))
    return seen


# ---------------------------------------------------------------------------
# Measurements (--report)
# ---------------------------------------------------------------------------

def rich_records(exe):
    """(prodid, build, count) from the Rich header; the layout pe_rich.py reads."""
    data = open(exe, 'rb').read()
    end = data.find(b'Rich', 0x80, 0x400)
    if end < 0:
        return []
    key = struct.unpack_from('<I', data, end + 4)[0]
    start = None
    for o in range(end - 4, 0x40, -4):
        if struct.unpack_from('<I', data, o)[0] ^ key == 0x536E6144:   # 'DanS'
            start = o
            break
    out = []
    for o in range(start + 16, end, 8):
        comp = struct.unpack_from('<I', data, o)[0] ^ key
        cnt = struct.unpack_from('<I', data, o + 4)[0] ^ key
        out.append((comp >> 16, comp & 0xFFFF, cnt))
    return out


def lis(xs):
    tails = []
    for x in xs:
        i = bisect.bisect_right(tails, x)
        if i == len(tails):
            tails.append(x)
        else:
            tails[i] = x
    return len(tails)


def contiguity(M):
    """PSX-adjacent twin pairs of one image (step under PSX_STEP): how far
    apart, and in which direction, are they on the PC?"""
    byimg = collections.defaultdict(list)
    for i in range(M.n):
        if M.twin[i] is not None:
            byimg[M.img[i]].append(i)
    hist = collections.Counter()
    back = []
    for im, L in byimg.items():
        L.sort(key=lambda i: M.twin[i])
        for i, j in zip(L, L[1:]):
            if M.twin[j] - M.twin[i] >= PSX_STEP:
                continue
            d = j - i
            k = ('+1' if d == 1 else '+2..8' if 2 <= d <= 8 else '+9..40' if 9 <= d <= 40 else
                 '>+40' if d > 40 else '-1' if d == -1 else '-2..-8' if -8 <= d <= -2 else
                 '-9..-40' if -40 <= d <= -9 else '<-40')
            hist[k] += 1
            if -40 <= d < 0:
                back.append((i, j, d))
    return hist, back


def calibrate(M, B):
    """Each placing signal on two known sets: the middle of PSX-joined runs of
    four or more (one unit), and strong PSX image cuts between neighbours (two
    units)."""
    pos = positions(M)
    tcov, _ = table_edges(M)
    same, diff = [], []
    for lo, hi in B['segments']:
        if hi - lo >= 3 and all(B['jp'][e] for e in range(lo, hi)):
            m = (lo + hi) // 2
            same.append(((max(lo, m - 2), m), (m + 1, min(hi, m + 3)), m))
    for i, j, kind, _ in B['hard']:
        if j == i + 1 and kind == 'strong':
            diff.append(((max(0, i - 2), i), (j, min(M.n - 1, j + 2)), i))

    def P(a):
        out = collections.defaultdict(list)
        for i in range(a[0], a[1] + 1):
            for r, xs in pos[i].items():
                out[r].extend(xs)
        return out

    def only_callers(a, b):
        cs = set()
        for i in range(a[0], a[1] + 1):
            cs |= {c for c in M.callers[i] if not a[0] <= c <= a[1]}
        return bool(cs) and all(b[0] <= c <= b[1] for c in cs)

    tests = {
        '.data order': lambda a, b, e: bool(P(a).get('data')) and bool(P(b).get('data')) and min(P(b)['data']) < max(P(a)['data']),
        '.bss order': lambda a, b, e: bool(P(a).get('bss')) and bool(P(b).get('bss')) and min(P(b)['bss']) < max(P(a)['bss']),
        'code-pointer cells order': lambda a, b, e: bool(P(a).get('data-cell')) and bool(P(b).get('data-cell')) and min(P(b)['data-cell']) < max(P(a)['data-cell']),
        'local table (8)': lambda a, b, e: bool(tcov.get(e)),
        'only callers across': lambda a, b, e: only_callers(b, a) or only_callers(a, b),
        'any call across': lambda a, b, e: any(j in M.callees[i] or i in M.callees[j]
                                              for i in range(a[0], a[1] + 1) for j in range(b[0], b[1] + 1)),
        'same name prefix': lambda a, b, e: collections.Counter(prefix(M.g[i]['name']) for i in range(a[0], a[1] + 1)).most_common(1)[0][0]
        == collections.Counter(prefix(M.g[i]['name']) for i in range(b[0], b[1] + 1)).most_common(1)[0][0],
    }
    out = []
    for name, f in tests.items():
        s = sum(1 for a, b, e in same if f(a, b, e))
        d = sum(1 for a, b, e in diff if f(a, b, e))
        out.append((name, s, len(same), d, len(diff)))
    return out


def order_measure(M):
    """Named-variable level: is a variable's first reader later in .text than
    the previous variable's? Per region, consecutive pairs and LIS."""
    out = []
    for lab, lo, hi in (('.rdata', M.RD[0], M.RD[1]), ('.data', M.DA[0], M.data_init_end),
                        ('.bss below 0x6CA000', M.data_init_end, BSS_STATIC_END),
                        ('.bss from 0x6CA000', BSS_STATIC_END, M.DA[1])):
        ks = sorted(k for k in M.readers if lo <= k < hi)
        xs = [min(M.readers[k]) for k in ks]
        up = sum(1 for x, y in zip(xs, xs[1:]) if y >= x)
        out.append((lab, len(xs), up, lis(xs)))
    return out


# ---------------------------------------------------------------------------
# Outputs
# ---------------------------------------------------------------------------

def write_tsv(path, rowmap, extra):
    with open(path, 'w', newline='\n', encoding='utf-8') as fh:
        fh.write('# tools/tu_census.py: Capcom\'s translation units recovered from BOF3.exe and symbols.toml '
                 '(docs/source-layout-round.md 3 SL1). Addresses and our names only.\n')
        fh.write('# confidence: joined (a PSX order run or a unit-local static), placed (data order, a local '
                 'code-pointer table, or the only callers), guessed (our names or one call), outside (not the game\'s range).\n')
        fh.write('#' + '\t'.join(COLUMNS) + '\n')
        for pc in sorted(rowmap):
            r = rowmap[pc]
            fh.write('\t'.join(str(r[c]).replace('\t', ' ') for c in COLUMNS) + '\n')
        for r in extra:
            fh.write('\t'.join(str(r[c]) for c in COLUMNS) + '\n')


def read_tsv(path):
    out = []
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            if line.startswith('#') or not line.strip():
                continue
            out.append(dict(zip(COLUMNS, line.rstrip('\n').split('\t'))))
    return out


def write_sheet(path, M, units, names, files, groups, pchk=None, have_psx=False):
    pchk = pchk or {}
    L = []
    L.append('# The source-layout round: the owner\'s layout sheet (SL1)\n')
    L.append('**Status:** PROPOSAL, written by `tools/tu_census.py` from `bof3/BOF3.exe` and '
             '`symbols.toml` (2026-11-07; split into two tables 2026-11-08). The owner decides each row before stage B '
             '([`docs/source-layout-round.md`](../docs/source-layout-round.md) 7, calls 1-3). '
             'Addresses, our names and counts only.\n')
    L.append('How to read a row: a **unit** is one object file of the 2001 link, as the table recovers it '
             '(`analysis/tu_layout.tsv` holds each function\'s row and evidence). **Does** is read from our '
             'function names: the three commonest name prefixes and their counts. **PSX** is the overlay of '
             'its PlayStation twins, where they have one. **Proposal** is to keep the unit in a file of its '
             'own (*stand alone*) or to put it in one file with the named neighbours (*join*); a unit is '
             'never split. **Sure** is how many of its functions are `joined` / `placed` / `guessed`.\n')
    subs = collections.Counter(n['sub'] for n in names)
    nfiles = len(groups)
    L.append(f'{len(units)} units in `0x{GAME_LO:X}..0x{GAME_HI:X}`, proposed as {nfiles} files in '
             f'{len(subs)} folders under `src/game/`: ' +
             ', '.join(f'`{s}/` {c} units' for s, c in sorted(subs.items(), key=lambda x: -x[1])) + '.\n')
    L.append('Folders: ' + '; '.join(f'`{s}/` {SUB_TEXT.get(s, s)}' for s in sorted(subs)) + '.\n')
    if have_psx:
        L.append(f'**PSX check** (SL2\'s `analysis/tu_psx.tsv`, joined by the coordinator): a PlayStation unit of '
                 f'two or more functions whose twins this table puts in more than one unit - a cut the '
                 f'PlayStation does not support, worth a look before the moves. {len(pchk)} of '
                 f'{len(units)} units carry one; empty means none.\n')
    rows = []
    for k, u in enumerate(units):
        lo, hi = unit_range(M, u)
        c = collections.Counter()
        for a, b, how, _ in u['pieces']:
            c[how] += b - a + 1
        n = names[k]
        f = files[k]
        call = f[1].startswith('join') or bool(pchk.get(k))
        rows.append((call, f"| {k + 1} | `{unit_id(M, u)}` | `0x{lo:X}..0x{hi:X}` | {u['hi'] - u['lo'] + 1} | "
                     f"{c['joined']}/{c['placed']}/{c['guessed']} | {n['desc']} | {n['image']} | "
                     f"`{f[0][len('src/game/'):]}` | {f[1]} | {f[2]} | "
                     + '; '.join(f"`{pu}` also in {', '.join(o)}" for pu, o in pchk.get(k, [])) + ' |'))
    # Two tables (the owner's ask of 2026-11-08): the rows that want a call first - a *join* (a choice
    # between one file and several) or a PSX check (a cut the PlayStation does not support) - then the
    # rows with one recommended layout, a *stand alone* that no neighbour and no PlayStation unit
    # contests. The row number (#) is the unit's index in both tables, so a reference to a row holds.
    header = ['| # | unit | .text range | funcs | sure j/p/g | does | PSX | file | proposal | reason | PSX check |',
              '|---|---|---|---|---|---|---|---|---|---|---|']
    want = [r for call, r in rows if call]
    settled = [r for call, r in rows if not call]
    joins = sum(1 for k in range(len(units)) if files[k][1].startswith('join'))
    L.append(f'## Rows that want a call ({len(want)})\n')
    L.append(f'A *join* proposal (a choice between one file and several), a PSX check, or both: '
             f'{joins} join rows and {len(pchk)} rows with a PSX check.\n')
    L.extend(header + want)
    L.append(f'\n## Rows with one recommended layout ({len(settled)})\n')
    L.append('A *stand alone* unit that no neighbour shares a name or folder rule with and that no '
             'PlayStation unit contests: the tool found no alternative, so these rows need no call '
             'unless the owner wants to make one.\n')
    L.extend(header + settled)
    with open(path, 'w', newline='\n', encoding='utf-8') as fh:
        fh.write('\n'.join(L) + '\n')


def check(tsv, symbols):
    funcs, _, _ = load_symbols(symbols)
    want = {f['pc']: f for f in funcs if f.get('impl')}
    if not os.path.exists(tsv):
        print(f'tu_census --check: {tsv} is missing')
        return 1
    have = {}
    bad = []
    for r in read_tsv(tsv):
        pc = int(r['pc'], 16)
        if pc in have:
            bad.append(f'{r["pc"]} twice')
        have[pc] = r
    for pc, f in want.items():
        r = have.get(pc)
        if r is None:
            bad.append(f'{pc:#x} {f["name"]} ({f["impl"]}) has no row')
        elif r['name'] != f['name']:
            bad.append(f'{pc:#x} row names {r["name"]}, symbols.toml {f["name"]}')
        elif r['impl'] != f['impl']:
            bad.append(f'{pc:#x} {f["name"]}: row impl {r["impl"]}, symbols.toml {f["impl"]} (rerun the tool)')
    for pc in have:
        if pc not in want:
            bad.append(f'{pc:#x} {have[pc]["name"]} has a row but no impl in symbols.toml')
    moved = sum(1 for pc, r in have.items() if pc in want and r['proposed_file'] != r['impl'])
    for b in bad[:40]:
        print('tu_census --check:', b)
    if bad:
        print(f'tu_census --check: {len(bad)} problems')
        return 1
    print(f'tu_census --check: {len(want)} impl functions, every one has its row '
          f'({moved} not yet in their proposed file)')
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--exe', default=os.path.join(ROOT, 'bof3', 'BOF3.exe'))
    ap.add_argument('--symbols', default=os.path.join(ROOT, 'symbols.toml'))
    ap.add_argument('--out', default=TSV)
    ap.add_argument('--sheet', default=SHEET)
    ap.add_argument('--report', action='store_true', help='print the measurements')
    ap.add_argument('--uncertain', type=int, default=0, help='print the N least certain boundaries')
    ap.add_argument('--refresh', action='store_true', help='read the binary again, not the cache')
    ap.add_argument('--check', action='store_true', help='CI: the TSV covers every impl function')
    ap.add_argument('--psx', default=PSX_TSV, help="SL2's tu_psx.tsv, joined into the table and the sheet ('' to skip)")
    a = ap.parse_args()
    if a.check:
        return check(a.out, a.symbols)
    funcs, data, _ = load_symbols(a.symbols)
    facts = load_facts(a.exe, funcs, a.refresh)
    M = Model(funcs, data, facts)
    B = build(M)
    units, tally, blocked = place(M, B)
    names = unit_names(M, units)
    files, groups = proposals(M, units, names, B['hard'])
    psx = load_psx_units(a.psx)
    rowmap = rows(M, B, units, files, psx)
    pchk = psx_checks(M, units, psx)
    extra = []
    for f in funcs:
        if f.get('impl') and not (GAME_LO <= f['pc'] < GAME_HI):
            extra.append(dict(pc=f'{f["pc"]:#x}', name=f['name'], unit='outside', unit_lo='', unit_hi='',
                              confidence='outside', evidence='the runtime or the decoder: out of scope (round doc 0)',
                              impl=f['impl'], proposed_file=f['impl'], psx_unit_agreement=''))
    for pc in [p for p, r in rowmap.items() if not r['impl']]:
        del rowmap[pc]          # a [[func]] without impl has no row (it is in its unit's range)
    write_tsv(a.out, rowmap, extra)
    write_sheet(a.sheet, M, units, names, files, groups, pchk, bool(psx))
    if psx:
        print(f'tu_census: PSX units joined for {sum(1 for r in rowmap.values() if r["psx_unit_agreement"])} rows; '
              f'{len(pchk)} units share a PSX unit with another unit')
    conf = collections.Counter(r['confidence'] for r in rowmap.values())
    print(f'tu_census: {len(units)} units, {len(groups)} proposed files; rows {len(rowmap)} '
          f'({", ".join(f"{k} {v}" for k, v in conf.most_common())}) + {len(extra)} outside; '
          f'wrote {os.path.relpath(a.out, ROOT)}, {os.path.relpath(a.sheet, ROOT)}')
    if a.report:
        report(a, M, B, units, tally, blocked, groups)
    if a.uncertain:
        for k, (sc, i, j, txt) in enumerate(uncertain(M, B, units)[:a.uncertain]):
            print(f'{k + 1}\t{sc}\t{M.g[i]["pc"]:#x} {M.g[i]["name"]}\t{M.g[j]["pc"]:#x} {M.g[j]["name"]}\t{txt}')
    return 0


def report(a, M, B, units, tally, blocked, groups):
    p = print
    p('\n== contiguity test (PSX-adjacent twins of one image, step < 0x%X: PC distance in functions)' % PSX_STEP)
    hist, back = contiguity(M)
    tot = sum(hist.values())
    for k in ('+1', '+2..8', '+9..40', '>+40', '-1', '-2..-8', '-9..-40', '<-40'):
        p(f'  {k:8s} {hist[k]:5d}')
    fwd = hist['+1'] + hist['+2..8'] + hist['+9..40']
    bk = hist['-1'] + hist['-2..-8'] + hist['-9..-40']
    p(f'  close forward {fwd}, close backward {bk} ({100 * bk / max(1, fwd + bk):.1f}%), far {hist[">+40"] + hist["<-40"]} of {tot}')
    for i, j, d in back:
        p(f'    back {d:4d}: {M.g[i]["name"]} {M.twin[i]:#x} / {M.g[j]["name"]} {M.twin[j]:#x} ({M.img[i]})')
    p(f'  twins left out (one twin claimed by two PC functions): {M.twin_dup}')
    p('\n== data order against .text order (named variables; first reader)')
    for lab, n, up, l in order_measure(M):
        p(f'  {lab:22s} {n:5d} variables, next reader later {up}, LIS {l}')
    p('\n== switch tables')
    F = M.facts['funcs']
    nt = sum(len(v['tables']) for v in F.values())
    outside = sum(1 for k, v in F.items() for x in v['tables'] if not (int(k) <= x[1] < v['end']))
    p(f'  {nt} dispatches through a .text table; {outside} tables outside their function\'s extent')
    p('\n== signal calibration (fires inside one known unit / across a known cut)')
    for name, s, ns, d, nd in calibrate(M, B):
        p(f'  {name:26s} same {s:4d}/{ns}  cut {d:4d}/{nd}')
    p('\n== joins')
    p(f'  PSX joins {len(B["pj"])}; static joins {collections.Counter(w.split()[0] for *_, w in B["sj"])}')
    p(f'  PSX cuts {collections.Counter(k for k, _ in B["pcuts"].values())}; hard {len(B["hard"])}')
    p(f'  statics against PSX: {collections.Counter(d["verdict"] for d in B["disagree"])}')
    segs = B['segments']
    multi = [s for s in segs if s[1] > s[0]]
    p(f'  segments {len(segs)}: {len(multi)} of two or more holding {sum(b - a + 1 for a, b in multi)} functions, '
      f'{len(segs) - len(multi)} single')
    p(f'  merges {dict(tally)}')
    p(f'  merges refused by a hard cut {dict(blocked)}')
    H = sorted(B['hard'], key=lambda h: h[1])
    last, k = -1, 0
    for i, j, *_ in H:
        if i >= last:
            k += 1
            last = j
    p(f'\n== units: {len(units)} (at least {k + 1}: {k} disjoint hard cuts); {len(groups)} proposed files')
    c = B['unit_counts']
    p(f'  after the joins {len(B["segments"])}, after placing {c[0]}, after name prefixes and calls {c[1]}, '
      f'after first-word stragglers {c[2]}')
    sz = collections.Counter()
    for u in units:
        n = u['hi'] - u['lo'] + 1
        sz['1' if n == 1 else '2-4' if n <= 4 else '5-14' if n <= 14 else '15-49' if n <= 49 else '50+'] += 1
    p(f'  sizes {dict(sz)}')
    if os.path.exists(a.exe):
        rec = rich_records(a.exe)
        p('  Rich records (prodid, build, count): ' + ', '.join(f'{x}/{y}/{z}' for x, y, z in rec)
          + f'; total {sum(z for *_, z in rec)}')


if __name__ == '__main__':
    sys.exit(main())
