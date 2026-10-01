#!/usr/bin/env python
"""The band tool: a group of round twelve's cut, function by function, with
its clone tables, its reach, its raw references in our source and the calls
between groups (docs/band-rows.md, docs/takeover-queue-field-battle.md).

    python tools/band_rows.py --exe .../BOF3.exe --analysis .../analysis --groups
    python tools/band_rows.py ... --group BE4              -> a function a line
    python tools/band_rows.py ... --group BE4 --clones     -> the clone tables (C++)
    python tools/band_rows.py ... --function 0x446DE0      -> one row and its clone
    python tools/band_rows.py ... --group BE4 --refs       -> raw 0x... in src/game
    python tools/band_rows.py ... --edges [--group BE4]    -> calls between groups
    python tools/band_rows.py ... --cut <tsv> --byte-tables --group EK1 --clones --harness area
                                  (round thirteen: any cut table, area_harness's form,
                                   two-level switches bounded; docs/band-rows.md section 6)
    python tools/band_rows.py ... --pointer-scan --groups
                                  (round twelve's debt 5: the starts in a band that only a
                                   .data pointer reaches, and the cut starts that are a case of
                                   a jump table read short; docs/band-rows.md section 7)

Rounds nine to eleven each had a table that enumerated their code (Magic_Rows,
the chapter vtables, the area descriptors, the boss set-ups). Round twelve's
groups are address bands, and the authority is the cut table
analysis/round12_cut.tsv (group, entry, size, label, hidden, host, name;
made by the plan's section 8). This tool reads that table and, for each
function, the exe itself:

  extent     a recursive descent of the start (capstone) inside its span -
             the start to the next start any list knows (pc_funcs.json,
             pc_hidden.json, symbols.toml, the cut) - following in-span
             branches and jump tables (bounded at their cmp / ja, with
             magic_rows.py's rules). One past the last byte reached is the
             extent; the cut's size (the catalogue's) is a hypothesis.
  reach      every direct call / jmp / jcc to the start (a sweep of .text,
             restarted at every known start; pc_xref.json indexes no rel32
             transfer, HANDOFF's trap), every immediate that names it
             (a stack table's mov [esp + k], a push, a store, a register
             load; pc_xref.json's are merged in), and every aligned dword of
             .data / .rdata / .text that holds it, with the run of code
             pointers it sits in and the symbols.toml name of that run.
  flags      inside-host: the start's first instruction is reached by its
             host's code (fall-through or an in-function branch or case) and
             no reference from anywhere else names it - round eleven's
             0x44103A shape; data: the start lies in a jump or byte table
             another descent read, or does not decode; falls-into: its own
             descent runs into the next start; uncovered: bytes between its
             extent and the next start that are not padding.
  clones     magic_rows.clone_sites over [start, extent): the E8 / E9 rel32
             that leave it, the stack-table immediates, the jump tables, and
             what a byte copy cannot carry, in boss_harness's (BH_) or
             scenario_harness's (SH_) form, each with a comment naming what
             reaches it and each callee as ours, this round's or Capcom's.

It writes nothing unless --tsv names a path, and never the cut table. The
output lists addresses, sizes and our own source lines; it is derived from
copyrighted game code, so keep it under analysis/ or a scratch directory,
never in a commit (CLAUDE.md rule 1). Addresses are load-bearing (rule 3).
"""
import argparse, bisect, collections, csv, json, os, re, struct, sys, tomllib

from capstone import x86

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import magic_rows as mr  # noqa: E402  Image, decode, clone_sites, the jump-table bounds

PADDING = (0x90, 0xCC)
BATTLE_PREFIX = 'BE'       # the battle groups default to the boss harness, the rest to the scenario harness
HARNESS = {'boss': ('boss_harness', 'BH_N'), 'scenario': ('scenario_harness', 'SH_N'),
           'area': ('area_harness', 'AH_N')}     # round thirteen's cut (docs/band-rows.md section 6)
POINTER_CTYPES = ('unsigned long', 'void *', 'const void *', 'unsigned int')
BYTE_TABLES = False        # --byte-tables (off: the output round twelve's groups were given)
POINTER_SCAN = False       # --pointer-scan (off: the same; docs/band-rows.md section 7)
TABLE_RUN_MAX = 1024       # the most cells a .text jump table is followed for, past its read


def load_toml(path):
    with open(path, 'rb') as fh:
        return tomllib.load(fh)


# --------------------------------------------------------------------------
# The descent: one start, inside its span

def read_extent(img, s, limit):
    """Recursive descent of s inside [s, limit). Returns a dict: end (one
    past the last byte reached, jump tables included), seen (instruction
    addresses reached), tables ([(lo, hi)] of jump and byte tables read),
    outs ([(site, kind, target)] - call / jmp / jcc leaving the span),
    imms ([(site, kind, value)] - .text values named by an immediate),
    drefs (absolute .data addresses named), falls (True when the code runs
    into limit), bad (an address that did not decode), notes."""
    seen, tables, outs, imms, drefs, notes, far = set(), [], [], [], set(), [], set()
    end, falls, bad = s, False, None
    work = [s]
    while work:
        pc = work.pop()
        prev = []
        while pc not in seen:
            if pc >= limit:
                falls = True
                break
            ins = mr.decode(img, pc)
            if ins is None:
                bad = pc
                break
            m, ops = ins.mnemonic, ins.operands
            if m in ('nop', 'int3') and pc != s:
                break           # padding is not the function's
            seen.add(pc)
            end = max(end, pc + ins.size)
            for op in ops:
                if op.type == x86.X86_OP_MEM:
                    d = op.mem.disp & 0xFFFFFFFF
                    if op.mem.base == 0 and img.off(d) is not None and not img.in_text(d):
                        drefs.add(d)
                elif op.type == x86.X86_OP_IMM and m != 'call' and not m.startswith('j') and not m.startswith('loop'):
                    v = op.imm & 0xFFFFFFFF
                    if img.in_text(v):
                        imms.append((pc, _imm_kind(ins), v))
                    elif img.off(v) is not None and m not in ('cmp', 'test') and v >= 0x5B0000:
                        drefs.add(v)
            if m == 'call':
                if ops[0].type == x86.X86_OP_IMM:
                    outs.append((pc, 'call', ops[0].imm & 0xFFFFFFFF))
                else:
                    notes.append((pc, 'indirect call ' + ins.op_str))
            elif m in ('ret', 'retf', 'hlt'):
                break
            elif m == 'jmp':
                op = ops[0]
                if op.type == x86.X86_OP_IMM:
                    t = op.imm & 0xFFFFFFFF
                    if s <= t < limit:
                        work.append(t)
                    else:
                        outs.append((pc, 'jmp', t))
                elif op.type == x86.X86_OP_MEM and op.mem.index != 0 and img.in_text(op.mem.disp & 0xFFFFFFFF):
                    t = op.mem.disp & 0xFFFFFFFF
                    cap = mr._jump_cap(img, prev)
                    if cap >= 1 << 30 and BYTE_TABLES:
                        # --byte-tables: behind MSVC's two-level switch the
                        # dword table has as many entries as the largest byte
                        # of its byte table, plus one. Without it the read
                        # stops at the first case past the span, so such a
                        # case is not seen as one (round thirteen's 0x4201F0,
                        # 0x422530; round twelve's 0x578A40, docs/band-rows.md 6)
                        cap = _byte_table_entries(img, prev) or cap
                    n = 0
                    while n < cap:
                        w = img.u32(t + 4 * n)
                        if w is None or not (s <= w < max(t, s + 1)) and not (s <= w < limit):
                            break
                        if cap >= 1 << 30 and not (s <= w < limit):
                            break
                        if s <= w < limit:
                            work.append(w)
                        else:
                            far.add(w)
                            notes.append((pc, 'case %#x past the span' % w))
                        n += 1
                    tables.append((t, t + 4 * n))
                    if s <= t < limit:
                        end = max(end, t + 4 * n)
                    else:
                        notes.append((pc, 'jump table %#x past the span (%d entries)' % (t, n)))
                    if mr._byte_table(img, prev):
                        for p in prev[-3:]:
                            if p.mnemonic in ('mov', 'movzx') and len(p.operands) == 2 \
                                    and p.operands[1].type == x86.X86_OP_MEM and p.operands[1].size == 1:
                                t2 = p.operands[1].mem.disp & 0xFFFFFFFF
                                bound = mr._cmp_bound(prev)
                                if img.in_text(t2) and bound is not None and bound < 0x400:
                                    tables.append((t2, t2 + bound + 1))
                                    if s <= t2 < limit:
                                        end = max(end, t2 + bound + 1)
                else:
                    notes.append((pc, 'indirect jmp ' + ins.op_str))
                break
            elif m.startswith('j') or m.startswith('loop'):
                if ops[0].type == x86.X86_OP_IMM:
                    t = ops[0].imm & 0xFFFFFFFF
                    if s <= t < limit:
                        work.append(t)
                    else:
                        outs.append((pc, 'jcc', t))
            prev.append(ins)
            prev = prev[-6:]
            pc += ins.size
    return dict(end=end, seen=seen, tables=tables, outs=outs, imms=imms, drefs=drefs,
                falls=falls, bad=bad, notes=notes, far=far)


def _byte_table_entries(img, prev):
    """The dword table's entry count behind MSVC's two-level switch
    (cmp r, bound; ja; mov / movzx r2, byte [r + T2]; jmp [r2*4 + T]): the
    largest byte of T2[0..bound], plus one. None when the byte table or its
    bound cannot be read (the caller then keeps the unbounded read)."""
    bound = mr._cmp_bound(prev)
    if bound is None or not 0 <= bound < 0x400:
        return None
    for p in reversed(prev[-3:]):
        if p.mnemonic in ('mov', 'movzx') and len(p.operands) == 2 \
                and p.operands[1].type == x86.X86_OP_MEM and p.operands[1].size == 1:
            t2 = p.operands[1].mem.disp & 0xFFFFFFFF
            if not img.in_text(t2):
                return None
            bs = [img.u8(t2 + i) for i in range(bound + 1)]
            if any(x is None for x in bs):
                return None
            return max(bs) + 1
    return None


def _imm_kind(ins):
    ops = ins.operands
    if ins.mnemonic == 'push':
        return 'push'
    if ins.mnemonic == 'mov' and len(ops) == 2 and ops[0].type == x86.X86_OP_MEM:
        return 'stack' if ops[0].mem.base == x86.X86_REG_ESP else 'store'
    if ins.mnemonic == 'mov' and len(ops) == 2 and ops[0].type == x86.X86_OP_REG:
        return 'reg'
    return ins.mnemonic


# --------------------------------------------------------------------------
# The sweep: every reference to a set of addresses

HEX = re.compile(r'0x[0-9a-f]+')


def sweep_refs(img, starts, want):
    """{target: [(site, kind)]} for every instruction of .text that names a
    target of `want`: call / jmp / jcc rel, or an immediate or displacement
    (kind 'stack', 'push', 'store', 'reg', 'mem' or the mnemonic). Linear,
    restarted at every known start and one byte past an undecodable one.
    Also returns the instruction spans that named something (site, size)."""
    refs = collections.defaultdict(list)
    spans = {}
    md = mr.MD
    text_lo, text_hi = img.text_lo, img.text_hi
    cuts = sorted({x for x in starts if text_lo <= x < text_hi} | {text_lo, text_hi})
    tsec = next(sc for sc in img.secs if sc[0] == '.text')
    base_off = tsec[3] - tsec[1]
    data = img.data
    for lo, hi in zip(cuts, cuts[1:]):
        pos = lo
        while pos < hi:
            last = pos
            for a, size, m, o in md.disasm_lite(data[pos + base_off:hi + base_off], pos):
                last = a + size
                if '0x' not in o:
                    continue
                for tok in HEX.findall(o):
                    v = int(tok, 16)
                    if v not in want:
                        continue
                    if m == 'call' or m == 'jmp' or m.startswith('j') or m.startswith('loop'):
                        kind = m if m in ('call', 'jmp') else ('jcc' if o.startswith('0x') else 'mem')
                        if not o.startswith('0x'):
                            kind = 'mem'
                    elif '[' in o and tok in o[o.index('['):o.index(']') + 1] and o.index('[') < o.index(tok):
                        kind = 'mem' if not (o.rfind(tok) > o.index(']')) else _lite_kind(m, o)
                    else:
                        kind = _lite_kind(m, o)
                    refs[v].append((a, kind))
                    spans[a] = size
            if last <= pos:
                pos += 1
            else:
                pos = last
                if pos < hi:
                    pos += 1      # the byte that stopped capstone
    return refs, spans


def _lite_kind(m, o):
    if m == 'push':
        return 'push'
    if m == 'mov' and o.startswith('dword ptr [esp'):
        return 'stack'
    if m == 'mov' and o.startswith('dword ptr ['):
        return 'store'
    if m == 'mov':
        return 'reg'
    return m


def cell_refs(img, want):
    """{target: [(cell, section)]}: every 4-aligned dword of every section that
    holds a target of `want`."""
    out = collections.defaultdict(list)
    for name, va, _, raw, rsz in img.secs:
        lo = (va + 3) & ~3
        o = raw + (lo - va)
        n = (rsz - (lo - va)) // 4
        if n <= 0:
            continue
        for i, (w,) in enumerate(struct.iter_unpack('<I', img.data[o:o + 4 * n])):
            if w in want:
                out[w].append((lo + 4 * i, name))
    return out


def run_start(img, cell, limit=256):
    """The first dword of the run of .text pointers that holds cell."""
    p = cell
    for _ in range(limit):
        w = img.u32(p - 4)
        if w is None or not img.in_text(w):
            break
        p -= 4
    return p


# --------------------------------------------------------------------------

class Band:
    def __init__(self, a):
        self.a = a
        self.img = mr.Image(a.exe)
        sym = load_toml(a.symbols)
        self.ours = {f['pc']: f['name'] for f in sym.get('func', []) if 'impl' in f}
        self.named = {f['pc']: f['name'] for f in sym.get('func', [])}
        self.data_named = {d['pc']: d['name'] for d in sym.get('data', [])}
        self.data_sized = []
        for d in sym.get('data', []):
            if d.get('count') and d.get('ctype') in POINTER_CTYPES:
                self.data_sized.append((d['pc'], d['pc'] + 4 * d['count'], d['name']))
        self.blocks = sorted((b['pc'], b['name']) for b in sym.get('block', []))

        pf = json.load(open(os.path.join(a.analysis, 'pc_funcs.json')))
        self.recorded = {f['entry']: f for f in pf['functions']}
        self.hidden = {h['entry']: h for h in json.load(open(os.path.join(a.analysis, 'pc_hidden.json')))}
        with open(a.cut, encoding='utf-8') as fh:
            self.rows = list(csv.DictReader(fh, delimiter='\t'))
        need = {'group', 'entry', 'size', 'label', 'hidden', 'host', 'name'}
        if not self.rows or not need <= set(self.rows[0]):
            sys.exit('%s: not a cut table (want the columns %s)' % (a.cut, ', '.join(sorted(need))))
        self.cut = collections.OrderedDict()
        self.group_of = {}
        for r in self.rows:
            e = int(r['entry'], 16)
            r['e'] = e
            r['host_e'] = int(r['host'], 16) if r['host'] else None
            self.cut[e] = r
            if e in self.group_of:
                sys.exit('%s: %#x listed twice' % (a.cut, e))
            self.group_of[e] = r['group']
        self.groups = list(collections.OrderedDict((r['group'], 1) for r in self.rows))
        self.starts = sorted(set(self.recorded) | set(self.hidden) | set(self.named) | set(self.cut))
        self.strong = sorted(set(self.recorded))

        self.twins = {}
        pp = os.path.join(a.analysis, 'pairs_propagated.json')
        if os.path.exists(pp):
            for p in json.load(open(pp)):        # hypotheses (HANDOFF item 9): printed, not trusted
                self.twins.setdefault(p['pc'], (p['psx'], p.get('how', '?')))
        self.psx_names = {}
        sib = os.path.join(a.sibling, 'symbols.toml') if a.sibling else None
        if sib and os.path.exists(sib):
            for f in load_toml(sib).get('func', []):
                self.psx_names[f['pc']] = f['name']
        self._ext, self._host, self._reach, self._extra = {}, {}, None, None
        self._pscan, self._tables, self._rows = None, None, {}
        self.absorbed = set()

    def extra_of(self):
        """{start: (group, the cut start before it)} for the code no list has
        inside a cut function's span (Band.unlisted): functions the cut does
        not list, printed with the group whose span holds them, flagged.
        With --pointer-scan, also the starts in a group's band that a .data
        pointer names and no descent reaches (Band.pointer_scan)."""
        if self._extra is None:
            extra = {}
            for s0, r in self.cut.items():
                for q, _ in self.unlisted(s0, self.limit(s0))[0]:
                    extra.setdefault(q, (r['group'], s0))
            if POINTER_SCAN:
                for w, info in self.pointer_scan(extra).items():
                    if info['row'] and w not in extra:
                        extra[w] = (info['group'], info['before'])
            self._extra = extra
        return self._extra

    # ---- the pointer scan (--pointer-scan; docs/band-rows.md section 7) ----
    def band_of(self, g):
        """A group's band: its first cut entry to the span end of its last."""
        es = [e for e, r in self.cut.items() if r['group'] == g]
        return min(es), self.limit(max(es))

    def pointer_scan(self, extra):
        """{start: info} for every 4-aligned dword outside .text that holds a
        .text address inside a group's band which no list knows (pc_funcs,
        pc_hidden, symbols.toml, the cut, the code Band.unlisted found - the
        `extra` passed in). FC2's four dispatchers and FC3's five were such
        starts: an 18-byte dispatcher in the padding after a function, named
        by Effect_KindHandlers' or FieldCore_State2Steps' cell and by nothing
        in .text, so no descent and no 16-byte rule found them (round twelve
        section 8). info: group, before (the cut start before it), cells
        ([(cell, section)]), inside (the member whose descent or table holds
        the address, else None), decodes, row (True when it becomes a member:
        decodes and inside no member's code)."""
        if self._pscan is not None:
            return self._pscan
        bands = sorted((lo, hi, g) for g in self.groups for lo, hi in [self.band_of(g)])
        known = set(self.starts) | self.absorbed | set(extra)
        found = collections.defaultdict(list)
        lows = [b[0] for b in bands]
        img = self.img
        for name, va, _, raw, rsz in img.secs:
            if name == '.text':
                continue
            lo = (va + 3) & ~3
            o = raw + (lo - va)
            n = (rsz - (lo - va)) // 4
            if n <= 0:
                continue
            for i, (w,) in enumerate(struct.iter_unpack('<I', img.data[o:o + 4 * n])):
                if not img.in_text(w) or w in known:
                    continue
                k = bisect.bisect_right(lows, w) - 1
                if k < 0 or not bands[k][0] <= w < bands[k][1]:
                    continue
                found[w].append((lo + 4 * i, name, bands[k][2]))
        out = {}
        members = sorted(set(self.cut) | set(extra))
        for w, cells in sorted(found.items()):
            g = cells[0][2]
            before = max((e for e, r in self.cut.items() if r['group'] == g and e <= w), default=None)
            # the members before w whose span still runs over it, any group's:
            # the last of one band can run into the next
            inside = None
            i = bisect.bisect_left(members, w) - 1
            while i >= 0 and self.limit(members[i]) > w:
                d = self.extent(members[i])
                if w in d['seen'] or any(lo <= w < hi for lo, hi in d['tables']):
                    inside = members[i]
                    break
                i -= 1
            # ... and the listed start before it, any list's (pc_funcs, the
            # hidden, the named): a dword that lands inside a Capcom function's
            # body is a constant that reads as an address, not an entry. The
            # first run (2026-10-01) printed 305 hits, most of them this.
            if inside is None:
                j = bisect.bisect_right(self.starts, w) - 1
                while j >= 0 and self.starts[j] in self.absorbed:
                    j -= 1
                if j >= 0 and self.starts[j] not in members and self.limit(self.starts[j]) > w:
                    d = self.extent(self.starts[j])
                    if w in d['seen'] or any(lo <= w < hi for lo, hi in d['tables']):
                        inside = self.starts[j]
            # a function of its own starts where MSVC puts one - on a 16-byte
            # boundary or right after padding - and decodes to its returns
            # without running into the next start (the 2026-10-01 run's rows of
            # 1..9 bytes at 0x430001, 0x440001, 0x580000.. were short pairs
            # and small constants, not code)
            boundary = w % 16 == 0 or img.u8(w - 1) in PADDING
            decodes = mr.decode(img, w) is not None and img.u8(w) not in PADDING + (0,)
            if decodes and inside is None:
                d = read_extent(img, w, self.limit(w))
                decodes = d['bad'] is None and not d['falls'] and d['end'] > w
            # the second run (2026-10-01): a value named only from .rsrc is a
            # resource's (0x53E0F0); one named from many cells, none in a run
            # of code pointers or a sized table, is a constant that recurs in
            # records (0x580000 from 43, 0x540200 from 22), not a pointer a
            # function would have in a table
            # (the third run: five of 0x540200's 22 cells had a dword of the
            # same shape before them and counted as a run, so the test is
            # the sized tables alone - a function pointer in no known table
            # is named from one cell, not from five)
            constant = all(sec == '.rsrc' for _, sec, _ in cells) or (
                len(cells) > 4 and not any(self._in_sized(c) for c, _, _ in cells))
            out[w] = dict(group=g, before=before, cells=[(c, sec) for c, sec, _ in cells], inside=inside,
                          decodes=decodes, boundary=boundary, constant=constant,
                          row=decodes and boundary and not constant and inside is None and before is not None)
        # a row inside another row's span is an entry into it - its span,
        # not its instruction starts: 0x570002 is the third byte of
        # 0x570000's first instruction (the third run)
        rows = sorted(w for w, i in out.items() if i['row'])
        for w in rows:
            i = bisect.bisect_left(rows, w) - 1
            while i >= 0 and self.limit(rows[i]) > w:
                d = read_extent(img, rows[i], self.limit(rows[i]))
                if rows[i] <= w < d['end']:
                    out[w]['inside'], out[w]['row'] = rows[i], False
                    break
                i -= 1
        self._pscan = out
        return out

    def _in_sized(self, cell):
        """True when the cell is in a sized pointer table of symbols.toml."""
        return any(lo <= cell < hi for lo, hi, _ in self.data_sized)

    def table_owner(self, cell):
        """(owner start, table base) when `cell` lies in the run of .text code
        pointers that continues a jump table some descent read - a case the
        read stopped short of (its cmp bound, or a two-level switch without
        --byte-tables), as 0x578A40 was of MoveScript_Group9's table at
        0x578AD8 (round twelve section 8). None when no table runs to it."""
        if self._tables is None:
            tabs = []
            for m in sorted(self.members_all() | self.bounds()):
                for lo, hi in self.extent(m)['tables']:
                    tabs.append((lo, hi, m))
            self._tables = sorted(tabs)
        img = self.img
        best = None
        for lo, hi, m in self._tables:
            if lo > cell:
                break
            if lo <= cell < hi:
                return m, lo
            if (cell - lo) // 4 > TABLE_RUN_MAX or (cell - lo) % 4:
                continue
            p = hi
            while p <= cell and (w := img.u32(p)) is not None and img.in_text(w):
                p += 4
            if p > cell:
                best = (m, lo)
        return best

    def members_all(self):
        return set(self.cut) | set(self.extra_of())

    def bounds(self):
        """The starts that end a member's span, outside the cut (a listed start
        that may be a case of the member before it)."""
        return {self.limit(x) for x in self.members_all()} - self.members_all()

    def group_any(self, x):
        if x in self.group_of:
            return self.group_of[x]
        e = self.extra_of().get(x)
        return e[0] if e else None

    def members(self, g):
        """A group's cut functions, then the code no list has in their spans."""
        return sorted([s for s, r in self.cut.items() if r['group'] == g]
                      + [q for q, (gg, _) in self.extra_of().items() if gg == g])

    # ---- extents ----------------------------------------------------------
    def limit(self, s):
        i = bisect.bisect_right(self.starts, s)
        while i < len(self.starts) and self.starts[i] in self.absorbed:
            i += 1
        return self.starts[i] if i < len(self.starts) else self.img.text_hi

    def settle(self):
        """A start flagged 'inside host' is not a bound: the function before it
        runs on over it (its jump table's cases, its fall-through). Absorb
        those and read again, to a fixpoint (two rounds today)."""
        for _ in range(8):
            new = {s for s in sorted(self.members_all() | self.bounds())
                   if any(f.startswith('inside host') for f in self.row(s)['flags'])}
            if new <= self.absorbed:
                return
            self.absorbed |= new
            self._ext, self._host, self._reach, self._extra = {}, {}, None, None
            self._pscan, self._tables, self._rows = None, None, {}
        sys.exit('settle: no fixpoint in 8 rounds')

    def prev_start(self, s):
        i = bisect.bisect_left(self.starts, s)
        while i > 0 and self.starts[i - 1] in self.absorbed:
            i -= 1
        return self.starts[i - 1] if i > 0 else None

    def extent(self, s):
        if s not in self._ext:
            self._ext[s] = read_extent(self.img, s, self.limit(s))
        return self._ext[s]

    def host_read(self, h):
        """A host's descent over its whole recorded body: to the next
        pc_funcs start, the hidden starts inside it not counted as bounds."""
        if h not in self._host:
            i = bisect.bisect_right(self.strong, h)
            lim = self.strong[i] if i < len(self.strong) else self.img.text_hi
            self._host[h] = read_extent(self.img, h, lim)
        return self._host[h]

    def unlisted(self, x, lim):
        """Code no list has, after x's extent and before lim: the next 16-byte
        boundary past the padding, when it decodes, descended in turn
        (magic_rows.py's rule: MSVC aligns functions to 16). Returns
        [(start, descent)] and the first byte left over that is neither
        padding nor such code (lim when none)."""
        out = []
        d = self.extent(x) if lim == self.limit(x) else read_extent(self.img, x, lim)
        while True:
            if d['falls'] or d['bad'] is not None:
                return out, lim
            tail = d['end']
            while tail < lim and self.img.u8(tail) in PADDING:
                tail += 1
            if tail >= lim:
                return out, lim
            q = (tail + 15) & ~15
            if q != tail or q >= lim or self.img.u8(q) in PADDING + (0,) or mr.decode(self.img, q) is None:
                return out, tail
            d = read_extent(self.img, q, lim)
            if d['bad'] is not None:
                return out, tail
            out.append((q, d))

    # ---- reach ------------------------------------------------------------
    def reach(self):
        if self._reach is None:
            want = self.members_all() | set(self.a_extra) | self.bounds()
            refs, spans = sweep_refs(self.img, self.starts, want)
            # pc_xref.json's immediates (mov / push of the address), merged by site
            xp = os.path.join(self.a.analysis, 'pc_xref.json')
            self.xref_extra = 0
            if os.path.exists(xp):
                xr = json.load(open(xp))
                for t in want:
                    for site, _, text in xr.get(hex(t), []):
                        if not any(site == x for x, _ in refs[t]):
                            refs[t].append((site, _lite_kind(text.split()[0], text.split(' ', 1)[1] if ' ' in text else '')))
                            self.xref_extra += 1
            cells = cell_refs(self.img, want)
            # a .text cell inside an instruction that names the value is that instruction's operand
            for t in list(cells):
                keep = []
                for c, sec in cells[t]:
                    if sec == '.text' and any(site <= c < site + spans.get(site, 16) for site, _ in refs[t]):
                        continue
                    keep.append((c, sec))
                cells[t] = keep
            self._reach = (refs, cells)
        return self._reach

    def func_of(self, site):
        i = bisect.bisect_right(self.starts, site) - 1
        while i > 0 and self.starts[i] in self.absorbed:
            i -= 1
        return self.starts[i] if i >= 0 else None

    def who(self, x):
        """ours / this round's / Capcom's, with a name."""
        if x in self.ours:
            return 'ours %s' % self.ours[x]
        if x in self.group_of:
            return '%s %s' % (self.group_of[x], self.named.get(x, 'Fn_%X' % x))
        if x in self.extra_of():
            return '%s Fn_%X (not in the cut)' % (self.extra_of()[x][0], x)
        if x in self.named:
            return "Capcom's %s" % self.named[x]
        return "Capcom's raw"

    def table_name(self, cell):
        for lo, hi, name in self.data_sized:
            if lo <= cell < hi:
                return '%s[%d]' % (name, (cell - lo) // 4)
        # the run of code pointers holding the cell, cut at the last name
        # symbols.toml gives inside it (runs sit back to back in .data)
        rs = run_start(self.img, cell)
        inside = [(pc, nm) for pc, nm in self.data_named.items() if rs <= pc <= cell]
        if inside:
            pc, nm = max(inside)
            sized = [hi for lo, hi, n in self.data_sized if lo == pc]
            if not sized or cell < sized[0]:
                return '%s[%d]' % (nm, (cell - pc) // 4)
            return 'run %#x[%d] (after %s)' % (sized[0], (cell - sized[0]) // 4, nm)
        return 'run %#x[%d]' % (rs, (cell - rs) // 4)

    def data_name(self, d):
        if d in self.data_named:
            return self.data_named[d]
        i = bisect.bisect_right(self.blocks, (d, '￿')) - 1
        if i >= 0 and d - self.blocks[i][0] < 0x400:
            return '%s+%#x' % (self.blocks[i][1], d - self.blocks[i][0])
        for lo, hi, name in self.data_sized:
            if lo <= d < hi:
                return '%s+%#x' % (name, d - lo)
        return None

    # ---- one row ----------------------------------------------------------
    def row(self, s):
        if s not in self._rows:
            self._rows[s] = self._row(s)
        return self._rows[s]

    def _row(self, s):
        r = self.cut.get(s)
        ex = self.extent(s)
        lim = self.limit(s)
        refs, cells = self.reach()
        host = r['host_e'] if r else None
        hr = self.host_read(host) if host is not None else None
        prv = self.prev_start(s)
        pr = read_extent(self.img, prv, s) if prv is not None else None
        # the code no list has between the previous start and this one: its
        # last piece is this start's real predecessor
        pre_unl, _ = self.unlisted(prv, s) if prv is not None else ([], s)
        pred, pd = (pre_unl[-1] if pre_unl else (prv, pr))
        readers = [d for d in (hr, pr, pd) if d]
        # references, the enclosing code's own branches apart
        inside = set().union(*(d['seen'] for d in readers)) if readers else set()
        body_refs, own_refs = [], []
        for site, kind in sorted(refs.get(s, [])):
            (own_refs if site in inside and kind in ('jmp', 'jcc') else body_refs).append((site, kind))
        tabs = [(lo, hi) for d in readers for lo, hi in d['tables']]
        own_cells, ext_cells = [], []
        for c, sec in sorted(cells.get(s, [])):
            (own_cells if sec == '.text' and any(lo <= c < hi for lo, hi in tabs) else ext_cells).append((c, sec))
        # --pointer-scan: a .text cell no reader placed in a table may continue a
        # jump table some other descent read short; then it is that table's case
        far_cells = []
        if POINTER_SCAN and not body_refs and ext_cells and all(sec == '.text' for _, sec in ext_cells):
            for c, sec in list(ext_cells):
                to_ = self.table_owner(c)
                if to_ is not None and to_[0] != s:
                    far_cells.append((c, to_[0], to_[1]))
                    ext_cells.remove((c, sec))
        flags = []
        if r is None and s in self.extra_of():
            ps = self._pscan.get(s) if POINTER_SCAN and self._pscan else None
            if ps and ps['row']:
                flags.append('not in the cut: code no list has, found by the pointer scan (%s), in the span of %#x' % (
                    ', '.join('%s cell %#x %s' % (sec, c, self.table_name(c)) for c, sec in ps['cells']), ps['before']))
            else:
                flags.append('not in the cut: code no list has, in the span of %#x' % self.extra_of()[s][1])
        by_host = hr is not None and (s in hr['seen'] or s in hr['far'])
        by_prev = pd is not None and ((pd['falls'] and pd['bad'] is None) or s in pd['far'])
        if (by_host or by_prev or own_refs or own_cells or far_cells) and not body_refs and not ext_cells:
            how = []
            if by_host:
                how.append('reached by host %#x' % host)
            if by_prev:
                how.append('%s %#x%s' % ('a case of' if s in pd['far'] else 'falls in from', pred,
                                         ', code no list has' if pre_unl else ''))
            if own_cells:
                how.append("a case in its host's jump table")
            for c, m, lo in far_cells:
                how.append("a case of %#x's table %#x (cell %#x, past the read; %s)" % (m, lo, c, self.who(m)))
            flags.append('inside host, no address reference (%s)' % ', '.join(how))
        if any(lo <= s < hi for lo, hi in tabs) or ex['bad'] == s:
            flags.append('data: %s' % ('in a table of %#x' % (host if hr and any(lo <= s < hi for lo, hi in hr['tables']) else prv)
                                         if ex['bad'] != s else 'does not decode'))
        elif ex['bad'] is not None:
            flags.append('does not decode at %#x' % ex['bad'])
        if ex['falls']:
            flags.append('falls into %#x' % lim)
        unl, left = self.unlisted(s, lim)
        if unl:
            flags.append('code no list has after it: %s' % ', '.join('%#x (%d bytes)' % (q, d['end'] - q) for q, d in unl))
        if left < lim:
            flags.append('uncovered %#x..%#x (not padding, not code on a boundary: data or unreached)' % (left, lim))
        if not body_refs and not ext_cells and not own_refs and not own_cells and not by_host and not by_prev:
            flags.append('no reference found')
        cat = int(r['size']) if r else None
        # a difference that is only padding (the catalogue's size ran to the
        # next start) is not a difference in the code
        pad_only = cat is not None and all(self.img.u8(x) in PADDING for x in range(min(s + cat, ex['end']), max(s + cat, ex['end'])))
        return dict(s=s, r=r, ex=ex, size=ex['end'] - s, cat=cat, lim=lim, differs=cat is not None and not pad_only,
                    host=host, unlisted=unl, refs=body_refs, own_refs=own_refs, cells=ext_cells, own_cells=own_cells,
                    far_cells=far_cells, flags=flags, group=r['group'] if r else (self.group_any(s) or '-'))

    def reach_text(self, row, full=True):
        parts = []
        for site, kind in row['refs']:
            f = self.func_of(site)
            parts.append('%s %#x in %#x (%s)' % (kind, site, f, self.who(f)) if full else '%s %#x' % (kind, site))
        for site, kind in row['own_refs']:
            parts.append('%s %#x in its host' % (kind, site))
        for c, sec in row['cells']:
            parts.append('%s cell %#x %s' % (sec, c, self.table_name(c)) if sec != '.text' else '.text cell %#x' % c)
        for c, sec in row['own_cells']:
            parts.append("case %#x of its host's table" % c)
        for c, m, lo in row.get('far_cells', []):
            parts.append("case %#x of %#x's table %#x" % (c, m, lo))
        return parts


# --------------------------------------------------------------------------
# Printing

def print_row(b, row):
    s, r = row['s'], row['r']
    hid = ''
    if r and r['hidden'] == '1':
        h = row['host']
        hid = 'hidden in %#x%s' % (h, (' (ours %s)' % b.ours[h]) if h in b.ours else (' (%s)' % b.who(h)))
    tw = b.twins.get(s)
    twin = ('twin %#x %s%s' % (tw[0], tw[1], (' ' + b.psx_names[tw[0]]) if tw[0] in b.psx_names else '')) if tw else ''
    diff = '' if row['cat'] is None or row['cat'] == row['size'] else ' (cut %d%s)' % (row['cat'], '' if row['differs'] else ', padding')
    name = b.ours.get(s) and ('OURS ' + b.ours[s]) or b.named.get(s, '')
    print('%#x %-4s %5d%s %s' % (s, row['group'], row['size'], diff, ' | '.join(x for x in (name, hid, twin) if x)))
    rt = b.reach_text(row)
    print('    reached: %s' % (summarise(b, row, rt) if rt else 'nothing names it'))
    if row['flags']:
        print('    FLAG: %s' % '; '.join(row['flags']))


SHOW = 12


def summarise(b, row, rt):
    """The first SHOW references, then the rest counted by the caller's side."""
    if len(rt) <= SHOW:
        return '; '.join(rt)
    rest = collections.Counter()
    for site, _ in row['refs'][SHOW:]:
        f = b.func_of(site)
        rest['ours' if f in b.ours else ('this round' if f in b.group_of else "Capcom's")] += 1
    more = len(rt) - SHOW - sum(rest.values())
    if more > 0:
        rest['cells and host'] += more
    return '%s; ... %d more (%s)' % ('; '.join(rt[:SHOW]), len(rt) - SHOW,
                                    ', '.join('%d %s' % (n, k) for k, n in sorted(rest.items())))


def print_clone(b, row, harness):
    ns, macro = HARNESS[harness]
    s = row['s']
    img = b.img
    end = row['ex']['end']
    calls, imms, tables, refused = mr.clone_sites(img, s, end)
    tag = '%X' % s
    name = b.named.get(s, 'Fn_' + tag)
    diff = '' if row['cat'] is None or row['cat'] == row['size'] else ' (the cut says %d%s)' % (
        row['cat'], '' if row['differs'] else ', the rest padding')
    print('// 0x%X %s: 0x%X bytes%s%s' % (s, row['group'], end - s, diff,
                                         ('; hidden in 0x%X, %s' % (row['host'], b.who(row['host']))) if row['r'] and row['r']['hidden'] == '1' else ''))
    rt = b.reach_text(row)
    print('//   reached by: %s' % (summarise(b, row, rt) if rt else 'nothing names it'))
    for f in row['flags']:
        print('//   FLAG: %s' % f)
    for off, t in calls:
        print('//   +0x%X -> 0x%X %s' % (off, t, b.who(t)))
    for off, v in imms:
        print('//   imm +0x%X = 0x%X %s' % (off, v, b.who(v)))
    ds = sorted(row['ex']['drefs'])
    if ds:
        print('//   data: %s' % ' '.join('%#x%s' % (d, (' ' + b.data_name(d)) if b.data_name(d) else '') for d in ds))
    for r in refused:
        print(('//   +0x%X %s' if r[1].startswith('note') else '//   REFUSED +0x%X %s') % r)
    if calls:
        print('constexpr %s::CallSite kCalls%s[] = {%s};' % (ns, tag, ', '.join('{0x%X, 0x%X}' % c for c in calls)))
    if imms:
        print('constexpr %s::Imm kImms%s[] = {%s};' % (ns, tag, ', '.join('{0x%X, 0x%X}' % i for i in imms)))
    if tables:
        print('constexpr %s::JumpTable kTables%s[] = {%s};' % (ns, tag, ', '.join('{0x%X, 0x%X, %d}' % t for t in tables)))
    cell = lambda v, k: ('k%s%s, %s(k%s%s)' % (k, tag, macro, k, tag)) if v else 'nullptr, 0'
    return '    {"%s", 0x%X, 0x%X, %s, %s, %s, reinterpret_cast<const void*>(&::%s)},  // ret_mask: yours' % (
        name, s, end - s, cell(calls, 'Calls'), cell(imms, 'Imms'), cell(tables, 'Tables'), name)


def print_clones(b, rows, harness):
    ns, macro = HARNESS[harness]
    lines = []
    for row in rows:
        if any(f.startswith('data') or f.startswith('does not decode') for f in row['flags']):
            print('// 0x%X %s: no clone - %s' % (row['s'], row['group'], '; '.join(row['flags'])))
            continue
        lines.append(print_clone(b, row, harness))
    print('#define %s(a) static_cast<int>(sizeof a / sizeof a[0])' % macro)
    print('const %s::Clone kClones[] = {' % ns)
    for line in lines:
        print(line)
    print('};')
    return len(lines)


def edges(b):
    """(caller group, caller, kind, site, callee, callee group) for every
    call / jmp / jcc / immediate from a cut function to a cut function of
    another group."""
    out = []
    for s in sorted(set(b.cut) | set(b.extra_of())):
        g = b.group_any(s)
        ex = b.extent(s)
        for site, kind, t in ex['outs']:
            tg = b.group_any(t)
            if tg and tg != g:
                out.append((g, s, kind, site, t, tg))
        for site, kind, v in ex['imms']:
            tg = b.group_any(v)
            if tg and tg != g:
                out.append((g, s, 'imm ' + kind, site, v, tg))
    return out


def print_pointer_scan(b, group=None):
    """The --pointer-scan report: per group, every start in the band that
    only a .data pointer names, with its cells and what became of it; then
    the cut starts the scan found to be a case of a table read short."""
    ps = b.pointer_scan(dict(b.extra_of()))     # settled already: the cache answers
    groups = [group] if group else b.groups
    hits = {w: i for w, i in ps.items() if i['group'] in groups}
    rows = sum(1 for i in hits.values() if i['row'])
    entries = sum(1 for i in hits.values() if not i['row'] and i['inside'] is not None)
    print('\npointer scan: %d dwords outside .text hold a band address no list knows: %d rows, %d entries into '
          'read code, %d data (a value that reads as an address: inside no read, not a function start, '
          'not decoding to a return, or a constant recurring in records)' % (len(hits), rows, entries, len(hits) - rows - entries))
    for g in groups:
        ws = sorted(w for w, i in hits.items() if i['group'] == g)
        if not ws:
            continue
        print('  %s (band %#x..%#x):' % ((g,) + b.band_of(g)))
        data = []
        for w in ws:
            i = ps[w]
            cells = ', '.join('%s cell %#x %s' % (sec, c, b.table_name(c)) for c, sec in i['cells'])
            if i['row']:
                what = 'a row now: %d bytes (code no list has, after %#x)' % (b.row(w)['size'], i['before'])
            elif i['inside'] is not None:
                what = "inside %#x's code or table (%s): an entry into it, not a function of its own" % (
                    i['inside'], b.who(i['inside']))
            elif i['decodes'] and i['boundary'] and i['before'] is None:
                what = 'before the first cut row of the band'
            else:
                data.append(w)
                continue
            print('    %#x  %s  <- %s' % (w, what, cells))
        if data:
            print('    data (%d): %s' % (len(data), ' '.join('%#x' % w for w in data)))
    cases = []
    for s in sorted(b.cut):
        if group and b.cut[s]['group'] != group:
            continue
        row = b.row(s)
        for c, m, lo in row['far_cells']:
            cases.append((s, c, m, lo))
    print('  %d cut starts are a case of a jump table read short:' % len(cases))
    for s, c, m, lo in cases:
        print('    %#x (%s) is cell %#x of %#x\'s table %#x (%s)' % (s, b.cut[s]['group'], c, m, lo, b.who(m)))


def grep_refs(repo, addrs):
    """{addr: [(file, line, text)]} for every raw 0x... of addrs in src/game."""
    pats = {x: re.compile(r'(?<![0-9A-Za-z_])0[xX]0*%X(?![0-9A-Fa-f])' % x, re.I) for x in addrs}
    big = re.compile(r'(?<![0-9A-Za-z_])0[xX]0*(%s)(?![0-9A-Fa-f])' % '|'.join('%X' % x for x in addrs), re.I)
    out = collections.defaultdict(list)
    root = os.path.join(repo, 'src', 'game')
    for d, _, fs in os.walk(root):
        for f in sorted(fs):
            p = os.path.join(d, f)
            try:
                lines = open(p, encoding='utf-8', errors='replace').read().splitlines()
            except OSError:
                continue
            for i, line in enumerate(lines, 1):
                for m in big.finditer(line):
                    v = int(m.group(1), 16)
                    if v in pats:
                        out[v].append((os.path.relpath(p, repo).replace('\\', '/'), i, line.strip()))
    return out


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--exe', default=os.path.join(repo, 'bof3', 'BOF3.exe'))
    ap.add_argument('--analysis', default=os.path.join(repo, 'analysis'))
    ap.add_argument('--symbols', default=os.path.join(repo, 'symbols.toml'))
    ap.add_argument('--sibling', default=os.path.join(os.path.dirname(repo), 'BreathOfFire3Recomp'),
                    help="the sibling checkout, for the PSX twin's name (its symbols.toml); optional")
    ap.add_argument('--cut', help='the cut table (default <analysis>/round12_cut.tsv); read, never written')
    ap.add_argument('--groups', action='store_true', help="the cut's groups with counts, and the tool's reading of each")
    ap.add_argument('--group', help='a group of the cut (BE1..BE7, FC1..FS): a function a line')
    ap.add_argument('--function', help='an address (or a comma-separated list): its row and its clone, in the cut or not')
    ap.add_argument('--clones', action='store_true', help='with --group: the clone tables (C++)')
    ap.add_argument('--with-ours', action='store_true', help='with --clones: the functions already ours too')
    ap.add_argument('--harness', choices=sorted(HARNESS), help='the clone namespace (default: boss for BE*, scenario for the rest)')
    ap.add_argument('--refs', action='store_true', help="with --group: every raw 0x... of its functions in src/game")
    ap.add_argument('--edges', action='store_true', help='the calls between groups of the cut (with --group: that group\'s)')
    ap.add_argument('--tsv', help='write the rows printed (or all, with --groups) to this path; never the cut table')
    ap.add_argument('--byte-tables', action='store_true',
                    help="bound a two-level switch's dword table by its byte table's largest entry, so a case "
                         'past the span is read as a case (off by default: the output round twelve used)')
    ap.add_argument('--pointer-scan', action='store_true',
                    help='find the starts in a band that only a .data pointer names (rows, flagged) and the cut '
                         'starts that are a case of a jump table read short (flagged inside host, absorbed); '
                         'docs/band-rows.md section 7 (off by default: the output round twelve used)')
    a = ap.parse_args()
    global BYTE_TABLES, POINTER_SCAN
    BYTE_TABLES = a.byte_tables
    POINTER_SCAN = a.pointer_scan
    a.cut = a.cut or os.path.join(a.analysis, 'round12_cut.tsv')
    if a.tsv and os.path.exists(a.tsv) and os.path.samefile(a.tsv, a.cut):
        sys.exit('--tsv %s is the cut table: refusing to write it' % a.tsv)
    if a.tsv and os.path.normcase(os.path.abspath(a.tsv)) == os.path.normcase(os.path.abspath(a.cut)):
        sys.exit('--tsv %s is the cut table: refusing to write it' % a.tsv)

    extra = [int(x, 16) for x in a.function.split(',')] if a.function else []
    b = Band(a)
    b.a_extra = extra
    b.settle()
    if a.group and a.group not in b.groups:
        sys.exit('no group %s (the cut has %s)' % (a.group, ' '.join(b.groups)))
    if not (a.groups or a.group or a.function or a.edges):
        a.groups = True

    tsv_rows = []

    if a.function:
        rows = [b.row(s) for s in extra]
        for row in rows:
            print_row(b, row)
        tsv_rows += rows
        g = rows[0]['group']
        if g == '-' and not a.harness:
            sys.exit('%#x is in no group of the cut: say --harness boss or scenario' % extra[0])
        h = a.harness or ('boss' if g.startswith(BATTLE_PREFIX) else 'scenario')
        print()
        print_clones(b, rows, h)

    if a.group and not a.edges:
        fs = b.members(a.group)
        rows = [b.row(s) for s in fs]
        tsv_rows += rows
        if a.refs:
            got = grep_refs(repo, fs)
            n = 0
            for s in fs:
                for f, i, text in got.get(s, []):
                    print('%#x %s:%d: %s' % (s, f, i, text[:160]))
                    n += 1
            print('\n%s: %d raw references to %d of its %d functions in src/game' % (
                a.group, n, sum(1 for s in fs if got.get(s)), len(fs)))
        elif a.clones:
            h = a.harness or ('boss' if a.group.startswith(BATTLE_PREFIX) else 'scenario')
            sel = [r for r in rows if a.with_ours or r['s'] not in b.ours]
            n = print_clones(b, sel, h)
            print('// %s: %d clones of %d functions (%d ours left out)' % (a.group, n, len(rows), len(rows) - len(sel)))
        else:
            for row in rows:
                print_row(b, row)
            cr = [r for r in rows if r['r']]
            print("\n%s: %d functions of the cut and %d not listed; %d bytes read of the cut's (it says %d), "
                  '%d extents differ from the cut by code (%d more by padding only), %d flagged' % (
                      a.group, len(cr), len(rows) - len(cr), sum(r['size'] for r in cr), sum(r['cat'] for r in cr),
                      sum(1 for r in cr if r['differs']),
                      sum(1 for r in cr if r['size'] != r['cat'] and not r['differs']),
                      sum(1 for r in cr if r['flags'])))

    if a.edges:
        es = edges(b)
        if a.group:
            es = [e for e in es if e[0] == a.group or e[5] == a.group]
        print('| Caller group | Caller | Kind | Site | Callee | Callee group |')
        print('|---|---|---|---|---|---|')
        for g, s, kind, site, t, tg in sorted(es, key=lambda e: (b.groups.index(e[0]), e[1], e[3])):
            print('| %s | `%#x` | %s | `%#x` | `%#x` %s | %s |' % (g, s, kind, site, t, b.named.get(t, ''), tg))
        pairs = collections.Counter((e[0], e[5]) for e in es)
        print('\n%d edges; by pair: %s' % (len(es), ', '.join('%s->%s %d' % (x, y, n) for (x, y), n in sorted(pairs.items()))))

    if a.groups:
        print('| Group | First..last entry | Fns | Hidden | Bytes | Ours |')
        print('|---|---|--:|--:|--:|--:|')
        tot = [0, 0, 0, 0]
        for g in b.groups:
            rs = [r for r in b.rows if r['group'] == g]
            n, hd, by, ou = len(rs), sum(1 for r in rs if r['hidden'] == '1'), sum(int(r['size']) for r in rs), \
                sum(1 for r in rs if r['e'] in b.ours)
            tot = [tot[0] + n, tot[1] + hd, tot[2] + by, tot[3] + ou]
            print('| %s | `%#x..%#x` | %d | %d | %s | %d |' % (g, rs[0]['e'], rs[-1]['e'], n, hd, '{:,}'.format(by), ou))
        print('| all | | %d | %d | %s | %d |' % (tot[0], tot[1], '{:,}'.format(tot[2]), tot[3]))
        print()
        es = edges(b)
        print('| Group | Fns (cut + not listed) | Bytes read (cut) | Extents differ (code / padding only) '
              '| Flagged: inside host / data / falls into / code no list has / uncovered / none | Clones | Edges out / in |')
        print('|---|--:|--:|--:|---|--:|---|')
        for g in b.groups:
            rows = [b.row(s) for s in b.members(g)]
            tsv_rows += rows
            cr = [r for r in rows if r['r']]
            fl = lambda k: sum(1 for r in cr if any(f.startswith(k) for f in r['flags']))
            clones = sum(1 for r in rows if r['s'] not in b.ours
                         and not any(f.startswith('data') or f.startswith('does not decode') for f in r['flags']))
            print('| %s | %d + %d | %s | %d / %d | %d / %d / %d / %d / %d / %d | %d | %d / %d |' % (
                g, len(cr), len(rows) - len(cr), '{:,}'.format(sum(r['size'] for r in cr)),
                sum(1 for r in cr if r['differs']), sum(1 for r in cr if r['size'] != r['cat'] and not r['differs']),
                fl('inside host'), fl('data') + fl('does not decode'), fl('falls into'), fl('code no list'), fl('uncovered'),
                fl('no reference'),
                clones, sum(1 for e in es if e[0] == g), sum(1 for e in es if e[5] == g)))
        print('\n(pc_xref.json added %d immediates the sweep had not seen)' % b.xref_extra)

    if a.pointer_scan and (a.groups or a.group):
        print_pointer_scan(b, a.group)

    if a.tsv:
        with open(a.tsv, 'w', newline='', encoding='utf-8') as fh:
            w = csv.writer(fh, delimiter='\t')
            w.writerow(['group', 'entry', 'size_read', 'size_cut', 'hidden', 'host', 'host_ours', 'name', 'twin', 'flags', 'reached'])
            for row in tsv_rows:
                r = row['r'] or {}
                tw = b.twins.get(row['s'])
                w.writerow([row['group'], '%#x' % row['s'], row['size'], row['cat'] if row['cat'] is not None else '',
                            r.get('hidden', ''), r.get('host', ''), 'ours' if row['host'] in b.ours else '',
                            b.named.get(row['s'], ''), '%#x %s' % tw if tw else '', '; '.join(row['flags']),
                            '; '.join(b.reach_text(row, full=False))])
        print('wrote %s (%d rows)' % (a.tsv, len(tsv_rows)), file=sys.stderr)


if __name__ == '__main__':
    main()
