#!/usr/bin/env python
"""Function starts hidden from every catalogue: the complete scan.

docs/mode-rest.md section 0 tells how thirteen functions (game modes 3..6 and
their steps) stayed outside every start list for fourteen rounds: reached only
through .data tables, they lay inside GameMode_Field's catalogue extent, and
tools/pe_hidden.py's rule (a 16-aligned start after ret / jmp and padding) did
not see them because a jump table's data, not a ret, came before the first.
This is the scan that report proposed, in two halves:

1. Every dispatch through a table of 4-byte cells - `jmp` / `call [reg*4 +
   imm32]`, and `mov reg, [reg*4 + imm32]` followed by `jmp reg` / `call reg` -
   found by a byte scan of .text and kept when the instruction decodes there;
   its table walked while the cells point into .text, bounded by the next named
   [[data]] or [[func]] in symbols.toml. Then every named [[data]] table whose
   first cell points into .text, walked the same way. A target that is neither
   a [[func]] start nor inside the dispatching function's own reachable code is
   a candidate.
2. Every [[func]]'s bytes, to the next [[func]] start, against its reachable
   code: the flow from the start (branches inside, the cases of its own switch
   tables - a table in .text; a table in .data is a pointer table whose cells
   are functions, not cases; ret, tail jumps to a start or out of the range,
   and other indirect jumps end a path). Bytes the flow never reaches that are
   not padding, a switch table, or an inline table the code reads as data hold
   a start no flow explains: a candidate, host named. The candidate's own flow
   is walked in turn, so one gap may give several. The catalogue's extent
   (pc_hidden.json's size, else pc_funcs.json's) is compared with where the
   code ends, and the sizes the groups measured by hand (the first `0x..
   bytes` in a [[func]]'s evidence) with the catalogue's.

Each candidate is classed by what reaches it: `function` (called directly,
through a table in .data or a call table, by a tail jump from another
function, or a pointer to it in data), `chunk` (only a nearby tail jump of one
known function reaches it: that function's second piece), `case` (a .text
switch table's target, or a NOTFN row of the round-fourteen cut),
`catalogued` (a pc_funcs.json / pc_hidden.json start with no [[func]]: known,
not hidden), `function?` (only an immediate or a .text table names it), `none`
(nothing reaches it, or not code).

    python tools/pe_jumptables.py [--symbols symbols.toml] [--out analysis/pc_jumptables.json]

`--drop A,B,...` leaves those names out of symbols.toml first, to show the
scan finds them (the control, docs/hidden-start-scan.md section 3).

Output is derived from copyrighted game code: it lives under analysis/ and is
never committed (CLAUDE.md rule 1).
"""
import argparse, bisect, collections, json, os, re, struct, sys, tomllib

import capstone
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe_hidden import Image  # noqa: E402

REGS = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
# The padding MSVC puts between functions: nop, int3, and its multi-byte no-ops.
PAD_SEQS = [bytes.fromhex(h) for h in (
    '8da42400000000', '8dbc2700000000', '8db600000000', '8dbf00000000',
    '8d9b00000000', '8d742600', '8d642400', '8d4900', '8d7600', '8d3f',
    '8bff', '8bc0', '8bc9', '8bd2', '8bdb', '8bf6', '8bed', '89f6', '87db',
    '90', 'cc')]
TABLE_MEM = re.compile(r'^dword ptr \[(\w+)\*4 \+ (0x[0-9a-f]+)\]$')
DISP_RE = re.compile(r'\[[^\]]*?(0x[0-9a-f]{6,})\]')
BYTES_RE = re.compile(r'\b(0x[0-9A-Fa-f]+) bytes\b')


def pad_len(data, off, limit):
    """Bytes of padding starting at file offset off, at most limit."""
    n = 0
    while n < limit:
        for s in PAD_SEQS:
            if data[off + n:off + n + len(s)] == s and n + len(s) <= limit:
                n += len(s)
                break
        else:
            break
    return n


class Scan:
    def __init__(self, a):
        self.img = img = Image(a.exe)
        self.lo = img.text['va']
        self.hi = img.text['va'] + img.text['vsz']
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.cache = {}
        self.mid = set()  # addresses strictly inside an instruction some flow decoded
        self.calls = collections.defaultdict(set)  # direct call target -> sites
        self.tails = collections.defaultdict(set)  # tail-jump target -> sites
        self.dataref = set()  # .text addresses some flow reads as data (inline tables)
        sym = tomllib.load(open(a.symbols, 'rb'))
        drop = set(a.drop.split(',')) if a.drop else set()
        self.funcs = {f['pc']: f for f in sym.get('func', []) if f['name'] not in drop}
        self.data = {d['pc']: d for d in sym.get('data', []) if d['name'] not in drop}
        self.dropped = sorted(drop & {e['name'] for e in sym.get('func', []) + sym.get('data', [])})
        self.starts = sorted(self.funcs)
        self.named = sorted(set(self.funcs) | set(self.data))
        pf = json.load(open(a.funcs))['functions']
        self.pc_funcs = {f['entry']: f['size'] for f in pf}
        self.pc_hidden = {h['entry']: h['size'] for h in json.load(open(a.hidden))}
        self.notfn = set()
        if a.cut and os.path.exists(a.cut):
            for line in open(a.cut, encoding='utf-8'):
                c = line.rstrip('\n').split('\t')
                if len(c) > 1 and c[0] == 'NOTFN':
                    try:
                        self.notfn.add(int(c[1], 16))
                    except ValueError:
                        pass
        self.index_pointers()

    # -- the image ------------------------------------------------------------
    def in_text(self, va):
        return self.lo <= va < self.hi

    def insn(self, va):
        i = self.cache.get(va)
        if i is None:
            o = self.img.off(va)
            i = next(self.md.disasm(self.img.data[o:o + 16], va, 1), None)
            i = (i.size, i.mnemonic, i.op_str) if i else (0, '', '')
            self.cache[va] = i
        return i

    def index_pointers(self):
        """Every dword in the image whose value is a .text address, by value."""
        self.ptrs = collections.defaultdict(list)
        for s in self.img.secs:
            blob = np.frombuffer(self.img.data, dtype=np.uint8,
                                 count=s['rsz'], offset=s['raw'])
            for k in range(4):
                n = (len(blob) - k) // 4
                w = np.frombuffer(blob[k:k + 4 * n].tobytes(), dtype='<u4')
                hit = np.nonzero((w >= self.lo) & (w < self.hi))[0]
                for j in hit:
                    self.ptrs[int(w[j])].append((s['name'], s['va'] + k + 4 * int(j)))

    def next_named(self, va):
        i = bisect.bisect_right(self.named, va)
        return self.named[i] if i < len(self.named) else 1 << 32

    def table(self, t, limit=None):
        """Cells of the table at t while they point into .text, bounded by the
        next named symbol after t (and by limit)."""
        if self.img.off(t) is None:
            return []
        stop = self.next_named(t)
        if limit is not None:
            stop = min(stop, limit)
        d = self.data.get(t)
        if d is not None and d.get('count'):
            stop = min(stop, t + 4 * d['count'])
        out = []
        while t + 4 * len(out) + 4 <= stop:
            o = self.img.off(t + 4 * len(out))
            if o is None:
                break
            c, = struct.unpack_from('<I', self.img.data, o)
            if not self.in_text(c) or c in self.mid:
                break
            out.append(c)
        return out

    # -- the catalogue --------------------------------------------------------
    def next_start(self, va, starts=None):
        starts = starts or self.starts
        i = bisect.bisect_right(starts, va)
        return starts[i] if i < len(starts) else self.hi

    def extent(self, s):
        """The region a start owns - to the next [[func]] start - and the
        catalogue's size for it (pc_hidden.json's, else pc_funcs.json's)."""
        return self.next_start(s), self.pc_hidden.get(s, self.pc_funcs.get(s))

    # -- the flow -------------------------------------------------------------
    def flow(self, s, end, starts):
        """Instructions reachable from s inside [s, end); the bytes of in-extent
        tables the flow read; dispatches seen. A jump to a known start is a
        tail call."""
        seen, tables, dispatch = {}, [], []
        work = [s]
        while work:
            p = work.pop()
            last_load = {}
            while s <= p < end and p not in seen:
                size, m, ops = self.insn(p)
                if not size:
                    break
                seen[p] = size
                nxt = p + size
                if m in ('ret', 'retf', 'int3', 'hlt', 'ud2'):
                    break
                if m.startswith('j') or m == 'call':
                    tm = TABLE_MEM.match(ops)
                    reg_t = last_load.get(ops) if ops in REGS else None
                    t = int(tm.group(2), 16) if tm else reg_t
                    if t is not None:
                        cells = self.table(t)
                        dispatch.append((p, m, t, cells))
                        if m == 'jmp':
                            # A switch's table sits in .text beside its code
                            # (MSVC); a table in .data is a pointer table whose
                            # cells are functions of their own, not cases.
                            if self.in_text(t):
                                if s <= t < end:
                                    tables.append((t, 4 * len(cells)))
                                for c in cells:
                                    if s <= c < end and (c == s or c not in starts):
                                        work.append(c)
                            break
                        p = nxt
                        continue
                    try:
                        tgt = int(ops, 16)
                    except ValueError:
                        tgt = None
                    if m == 'call':
                        if tgt is not None:
                            self.calls[tgt].add(p)
                        p = nxt
                        continue
                    if tgt is not None and m == 'jmp' and not (s <= tgt < end):
                        self.tails[tgt].add(p)
                    if tgt is not None and s <= tgt < end and (tgt == s or tgt not in starts):
                        work.append(tgt)
                    if m == 'jmp':
                        break
                    p = nxt
                    continue
                for v in DISP_RE.findall(ops):
                    v = int(v, 16)
                    if self.in_text(v):
                        self.dataref.add(v)
                if m == 'mov':
                    d, _, src = ops.partition(', ')
                    tm = TABLE_MEM.match(src)
                    if d in REGS:
                        if tm:
                            last_load[d] = int(tm.group(2), 16)
                        else:
                            last_load.pop(d, None)
                p = nxt
        return seen, tables, dispatch

    def shape(self, va, n=4):
        out, p = [], va
        for _ in range(n):
            size, m, ops = self.insn(p)
            if not size:
                out.append('(undecodable)')
                break
            out.append(f'{m} {ops}'.strip())
            if m in ('ret', 'jmp'):
                break
            p += size
        return '; '.join(out)

    def refs(self, va):
        r = self.ptrs.get(va, [])
        return dict(data=[f'{v:#x}' for n, v in r if n != '.text'],
                    text=[f'{v:#x}' for n, v in r if n == '.text'],
                    call=[f'{v:#x}' for v in sorted(self.calls.get(va, ()))],
                    tail=[f'{v:#x}' for v in sorted(self.tails.get(va, ()))])


def run(a):
    sc = Scan(a)
    starts = set(sc.starts)
    flows = {}
    disagree = []
    padding_only = 0
    short, long_ = [], []  # catalogue extents ending before / after the code

    # Half 2 first: every [[func]]'s reachable code, and what its extent
    # holds beyond it.
    gap_cands = {}
    inline = {}  # inline data tables met in gaps: start -> bytes
    for s in sc.starts:
        if not sc.in_text(s):
            continue
        end, size = sc.extent(s)
        seen, tables, dispatch = sc.flow(s, end, starts)
        flows[s] = (seen, dispatch)
        # Where the reachable code (and the tables it read) really ends,
        # against the catalogue's extent.
        code_end = max([p + n for p, n in seen.items()] +
                       [t + n for t, n in tables if t >= s] + [s])
        if size is not None:
            cat_end = s + size
            if cat_end < code_end:
                short.append((s, cat_end, code_end))
            elif cat_end > code_end:
                o = sc.img.off(code_end)
                if pad_len(sc.img.data, o, cat_end - code_end) < cat_end - code_end:
                    long_.append((s, cat_end, code_end))
        # The groups' hand-measured size against the catalogue's.
        ev = sc.funcs[s].get('evidence', '')
        mm = BYTES_RE.search(ev)
        if mm and size is not None:
            hand = int(mm.group(1), 16)
            if hand != size:
                lo_, hi_ = sorted((s + hand, s + size))
                o = sc.img.off(lo_)
                if pad_len(sc.img.data, o, hi_ - lo_) == hi_ - lo_:
                    padding_only += 1
                else:
                    i0 = bisect.bisect_right(sc.starts, s)
                    i1 = bisect.bisect_left(sc.starts, s + size)
                    # Which is right: the starts the catalogue ran over, or
                    # where the flow says the code ends.
                    if size > hand and i1 > i0:
                        kind = 'catalogue runs over %d later starts' % (i1 - i0)
                    elif abs((code_end - s) - hand) <= 3 and abs((code_end - s) - size) > 3:
                        kind = 'the flow agrees with the hand size'
                    elif abs((code_end - s) - size) <= 3:
                        kind = 'the flow agrees with the catalogue'
                    else:
                        kind = 'neither: the flow ends at %#x' % (code_end - s)
                    disagree.append(dict(pc=f'{s:#x}', name=sc.funcs[s]['name'],
                                         hand=f'{hand:#x}', catalogue=f'{size:#x}',
                                         flow=f'{code_end - s:#x}', by=size - hand,
                                         settles=kind,
                                         source='pc_hidden.json' if s in sc.pc_hidden
                                         else 'pc_funcs.json'))
        # Gaps: walk them, each new start's flow covering more.
        covered = bytearray(end - s)
        for p, n in seen.items():
            covered[p - s:p - s + n] = b'\1' * min(n, end - p)
        for t, n in tables:
            covered[t - s:t - s + n] = b'\1' * min(n, end - t)
        i = 0
        while i < len(covered):
            if covered[i]:
                i += 1
                continue
            j = i
            while j < len(covered) and not covered[j]:
                j += 1
            o = sc.img.off(s + i)
            k = pad_len(sc.img.data, o, j - i)
            if i + k < j and s + i + k in sc.dataref:
                # An inline table the code reads as data (a switch's byte
                # index, a dword table read without a jump): its cells, then
                # padding.
                q = s + i + k
                if sc.in_text(sc.img.dword(q)):
                    while q + 4 <= s + j and sc.in_text(sc.img.dword(q)):
                        q += 4
                else:
                    while q < s + j and not pad_len(sc.img.data, sc.img.off(q), s + j - q):
                        q += 1
                inline[s + i + k] = q - (s + i + k)
                covered[i + k:q - s] = b'' * (q - s - i - k)
                continue
            if i + k < j:
                c = s + i + k
                gap_cands[c] = dict(host=s, host_end=end, gap=[s + i, s + j])
                cseen, ctables, cdisp = sc.flow(c, s + j, starts)
                flows[c] = (cseen, cdisp)
                for p, n in cseen.items():
                    covered[p - s:p - s + n] = b'\1' * min(n, end - p)
                for t, n in ctables:
                    covered[t - s:t - s + n] = b'\1' * min(n, end - t)
                if not cseen:  # undecodable: data; skip the gap
                    covered[c - s:j] = b'\1' * (j - (c - s))
                i = c - s
                continue
            i = j

    # Half 1: every table dispatch in .text, by bytes, kept when it decodes
    # and does not sit inside an instruction some flow decoded.
    owner_of = {}
    for f, (seen, _) in flows.items():
        for p, n in seen.items():
            owner_of.setdefault(p, f)
            sc.mid.update(range(p + 1, p + n))
    sc.mid.difference_update(owner_of)
    text = sc.img.data[sc.img.off(sc.lo):sc.img.off(sc.lo) + sc.hi - sc.lo]
    sites = []
    for m in re.finditer(rb'\xff[\x24\x14][\x85\x8d\x95\x9d\xad\xb5\xbd]', text):
        va = sc.lo + m.start()
        size, mn, ops = sc.insn(va)
        tm = TABLE_MEM.match(ops)
        if size == 7 and mn in ('jmp', 'call') and tm and va not in sc.mid:
            sites.append((va, mn, int(tm.group(2), 16), f'{mn} {ops}'))
    for m in re.finditer(rb'\x8b[\x04\x0c\x14\x1c\x2c\x34\x3c][\x85\x8d\x95\x9d\xad\xb5\xbd]', text):
        va = sc.lo + m.start()
        size, mn, ops = sc.insn(va)
        d, _, src = ops.partition(', ')
        tm = TABLE_MEM.match(src)
        if size != 7 or mn != 'mov' or not tm or va in sc.mid:
            continue
        # The register used as a jump or call target within the next few.
        p = va + size
        for _ in range(4):
            sz, m2, o2 = sc.insn(p)
            if not sz:
                break
            if m2 in ('jmp', 'call') and o2 == d:
                sites.append((p, m2, int(tm.group(2), 16), f'mov {ops}; {m2} {o2}'))
                break
            if o2.startswith(d + ',') or m2 in ('jmp', 'ret', 'call'):
                break
            p += sz

    found = collections.defaultdict(lambda: dict(via=[]))

    def host_of(va):
        f = owner_of.get(va)
        if f is not None:
            return f, True
        i = bisect.bisect_right(sc.starts, va) - 1
        return (sc.starts[i] if i >= 0 else None), False

    n_targets = 0
    off_flow = []  # dispatch sites no start's flow reaches
    for va, mn, t, how in sites:
        cells = sc.table(t)
        f, on_flow = host_of(va)
        if not on_flow:
            off_flow.append(f'{va:#x}')
        own = flows.get(f, ({}, []))[0] if on_flow else {}
        for c in cells:
            n_targets += 1
            if c in starts or c in own:
                continue
            found[c]['via'].append(dict(kind='dispatch', site=f'{va:#x}', insn=how,
                                        table=f'{t:#x}', cells=len(cells),
                                        host=f'{f:#x}' if f else None,
                                        site_on_flow=on_flow,
                                        site_offset=va - f if f else None))
    # The named tables.
    named_tables = 0
    inside = []  # cells of a named .data table that land inside a known function's code
    for d in sorted(sc.data.values(), key=lambda d: d['pc']):
        cells = sc.table(d['pc'])
        if not cells:
            continue
        named_tables += 1
        for c in cells:
            if c in starts:
                continue
            if owner_of.get(c) in starts:  # inside a known start's own code
                if not sc.in_text(d['pc']):
                    inside.append(dict(pc=f'{c:#x}', table=d['name'],
                                       inside=sc.funcs.get(owner_of[c], {}).get('name', hex(owner_of[c]))))
                continue
            found[c]['via'].append(dict(kind='named table', table=f"{d['pc']:#x}",
                                        name=d['name'], ctype=d.get('ctype'),
                                        cells=len(cells)))
    for c, g in gap_cands.items():
        found[c]['via'].append(dict(kind='extent', host=f"{g['host']:#x}",
                                    host_name=sc.funcs[g['host']]['name'],
                                    host_end=f"{g['host_end']:#x}",
                                    gap=[f'{x:#x}' for x in g['gap']]))

    # Classify.
    out = []
    for c in sorted(found):
        r = found[c]
        kinds = {v['kind'] for v in r['via']}
        refs = sc.refs(c)
        shp = sc.shape(c)
        host = None
        for v in r['via']:
            if v.get('host'):
                host = int(v['host'], 16)
                break
        if host is None:
            i = bisect.bisect_right(sc.starts, c) - 1
            host = sc.starts[i] if i >= 0 else None
        # How the code before it ends.
        cls, why = None, ''
        data_tables = [v for v in r['via'] if v['kind'] == 'named table' or
                       (v['kind'] == 'dispatch' and not sc.in_text(int(v['table'], 16)))]
        call_disp = [v for v in r['via'] if v['kind'] == 'dispatch' and v['insn'].startswith('call')]
        tail_disp = [v for v in r['via'] if v['kind'] == 'dispatch' and v['insn'].startswith('jmp')
                     and not sc.in_text(int(v['table'], 16))
                     and v['site_on_flow'] and v['site_offset'] <= 0x10]
        tail_hosts = {owner_of.get(int(t, 16)) for t in refs['tail']
                      if abs(int(t, 16) - c) < 0x200}
        if len(tail_hosts) != len(refs['tail']):
            tail_hosts.add(None)
        if c in sc.notfn:
            cls, why = 'case', 'a NOTFN row of the round-fourteen cut'
        elif c in sc.pc_funcs or c in sc.pc_hidden:
            cls = 'catalogued'
            why = ('a direct call target (pc_funcs.json)' if c in sc.pc_funcs
                   else 'a padding-rule start (pc_hidden.json)')
        elif shp.startswith('(undecodable)'):
            cls, why = 'none', 'undecodable: data'
        elif (refs['tail'] and not refs['call'] and not refs['data'] and not data_tables
              and not call_disp and len(tail_hosts) == 1 and None not in tail_hosts):
            h = tail_hosts.pop()
            cls = 'chunk'
            why = 'a chunk of %s: only its tail jump reaches it' % (
                sc.funcs[h]['name'] if h in sc.funcs else hex(h))
        elif data_tables or call_disp or tail_disp or refs['data'] or refs['call'] or refs['tail']:
            cls = 'function'
            why = ('called directly' if refs['call'] else
                   'a tail jump from another function' if refs['tail'] else
                   'called through a table' if call_disp else
                   'a cell of a .data table' if data_tables else
                   'a tail dispatch at a start' if tail_disp else
                   'a pointer to it in data')
        elif kinds == {'dispatch'} or (kinds <= {'dispatch', 'extent'} and 'dispatch' in kinds):
            cls, why = 'case', 'a jmp-table target in .text, mid-function'
        elif refs['text']:
            cls, why = 'function?', 'an immediate or a .text table names it'
        else:
            cls, why = 'none', 'no reference: unreached code after its host'
        out.append(dict(pc=f'{c:#x}', cls=cls, why=why, shape=shp,
                        host=f'{host:#x}' if host is not None else None,
                        host_name=sc.funcs[host]['name'] if host in sc.funcs else None,
                        refs=refs, via=r['via']))

    res = dict(symbols=a.symbols, dropped=sc.dropped, starts=len(sc.starts), dispatch_sites=len(sites),
               dispatch_targets=n_targets, named_tables=named_tables,
               evidence_size_padding_only=padding_only,
               dispatch_sites_off_flow=off_flow,
               # The round-fourteen cut's NOTFN rows: how many a known start's
               # flow reaches as its own code (a case), and which it does not.
               notfn_in_flow=sum(1 for c in sc.notfn if owner_of.get(c) in starts),
               notfn_not_in_flow=[f'{c:#x}' for c in sorted(sc.notfn) if owner_of.get(c) not in starts],
               # start, bytes, the largest byte (a switch's byte index stays small)
               inline_tables=[[f'{t:#x}', n, max(sc.img.data[sc.img.off(t):sc.img.off(t) + n] or b'\0')]
                              for t, n in sorted(inline.items())],
               data_cells_inside_code=inside,
               catalogue_short=[[f'{x:#x}' for x in r] for r in short],
               catalogue_long=[[f'{x:#x}' for x in r] for r in long_],
               evidence_size_disagree=disagree, candidates=out)
    json.dump(res, open(a.out, 'w'), indent=1)
    cc = collections.Counter(r['cls'] for r in out)
    print(f'{len(sc.starts)} starts; {len(sites)} table dispatches in .text '
          f'({n_targets} cells), {named_tables} named tables; '
          f'{len(out)} candidates {dict(cc)}; hand sizes: {len(disagree)} disagree, '
          f'{padding_only} by padding only; {len(inline)} inline tables; catalogue extents {len(short)} short of the code, '
          f'{len(long_)} past it -> {a.out}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--exe', default='bof3/BOF3.exe')
    ap.add_argument('--symbols', default='symbols.toml')
    ap.add_argument('--funcs', default='analysis/pc_funcs.json')
    ap.add_argument('--hidden', default='analysis/pc_hidden.json')
    ap.add_argument('--cut', default='analysis/round14_cut.tsv')
    ap.add_argument('--out', default='analysis/pc_jumptables.json')
    ap.add_argument('--drop', default='',
                    help='comma-separated [[func]] / [[data]] names to leave out (the control)')
    run(ap.parse_args())


if __name__ == '__main__':
    main()
