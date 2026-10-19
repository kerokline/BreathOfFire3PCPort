#!/usr/bin/env python
"""Measure the width of every module-constant data address (typed-data round,
group SG; docs/typed-data-round.md 2).

    python tools/data_widths.py --exe <path>/BOF3.exe --funcs analysis/pc_funcs.json \
        --jumptables analysis/pc_jumptables.json --toml symbols.toml \
        --settled <out>/settled.tsv --unsettled analysis/typed_globals_unsettled.tsv

What it does. The targets are the distinct addresses that a `constexpr kName =
0x...` in a src/**/*_callees.h names, inside .rdata/.data/.bss, that have no
[[data]] entry with a `ctype` other than one this group wrote (an entry whose
evidence says "(typed-data round group SG)" is re-measured, not trusted), less
the [[block]] base addresses (a block is a record family, stage C's; scalars
inside a block's records are not skipped). The code read is every host range
of pc_funcs.json plus 0x401000..0x4020B0 (the area handlers no host covers),
each split at the [[func]] starts inside it and disassembled with capstone
from its start. The inline tables of pc_jumptables.json ([start, bytes,
largest byte]) and every switch's dword table (`jmp [reg*4 + table]` into
.text, found by a first pass, its cells while they point into .text) are
stepped over; a byte capstone still cannot decode is skipped and decoding
restarts one byte later.

The rule, in order, for each target address A:

0. inside the records of a [[block]] with `record_stride` / `record_count`
   (pc .. pc + stride * count, the first `record_size` bytes of each record
   when the block gives one): unsettled, a field of that record family
   (stage C1's struct in src/game/records.h, not a global of its own).
1. inside the extent (pc .. pc + size * count) of an earlier typed [[data]]
   entry, unless A has its own entry: unsettled, an element of that entry.
2. a hand-read exclusion in HAND below: unsettled with the reason given there.
3. indexed: any operand [reg + A] / [reg*k + A] (lea included), or
   `add reg, A` (a record base after a shl): unsettled, a table whose count is
   unread.
4. no operand at A at all: unsettled, reached only by base + offset or by
   pointer (or only its address taken by a plain lea).
5. inside the DAT arena (ARENA below): unsettled, overlay-loaded.
6. the effective width of each operand at A: its own size, except that a
   dword `mov r32, [A]` whose value is masked by `and r, imm` (imm at most
   0xFF / 0xFFFF) or narrowed by movzx / movsx of its low byte / word before
   any use of the whole register (copies followed, reads of al / ax alone
   counted at their width; see effective()) is read as 8 or 16 bits. More
   than one effective width: unsettled, mixed widths.
7. a store at a neighbouring address K that writes a byte of [A, A + width)
   (`mov dword ptr [A - 1]`, `and word ptr [A - 1], imm`, ...): unsettled,
   mixed widths (also written as part of the wider value at K). Reads that
   cover A from a neighbour are not counted against it.
8. otherwise settled: float / double if every FPU access is a float one and
   the operands have one size (integer moves then copy the bit pattern);
   else signed (`signed char` / `short`) when every widening read is movsx,
   else unsigned; 4-byte integers `unsigned long`; import address table
   slots `void *`.

Last, the settled extents are checked against each other and the earlier
typed entries, and any overlap is printed.

With `--census analysis/data_census.tsv` (stage C3, docs/typed-data-stage-c.md
5) the targets are instead the module-only addresses of the census: every
`const` row of scope `module` whose value no `at` constant names and no
[[data]] entry has (the module constants of .cpp files and of non-`at`
namespaces that the scan above never read), the constants column taken from
the census (repeat the option to measure the union of several censuses, an
older tree's included: a constant B0 or B1 removed still names an address no
entry has). Every typed entry, SG's included, is then a prior entry for rule
1, since none of them is re-measured.

Output: two TSVs of addresses, instruction text and names - our measurement,
nothing copied out of the game. The exe is only read, from the path given.
"""
import argparse, bisect, json, os, re, struct, sys, tomllib
from collections import Counter, defaultdict

from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG, X86_REG_INVALID

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_symbols import module_constants  # noqa: E402

GAP = (0x401000, 0x4020B0)       # area handlers outside every pc_funcs host
IAT = (0x5C4000, 0x5C41B4)       # PE data directory 12
SG_TAG = '(typed-data round group SG)'

# Hand-read exclusions: settled by rule 7, but a read of the code says the
# address is not one scalar. Each cites the instructions.
HAND = {
    0x904D00: 'text buffer: the sprintf destination (push 0x904d00 at 0x402f1f; '
              'mov edi, 0x904d00 at 0x42fec9, 0x431c87), stored 16 bytes at a time '
              'at +0/+4/+8/+0xC (0x42ff07..0x42ff1f, 0x45be57..0x45be72); a char '
              'array, its count unread',
    0x904D0F: 'inside the 0x904D00 text buffer (its last byte, written by mov dword ptr '
              '[0x904d0c] at 0x42ff1f and 0x45be72)',
    0x7CADF0: 'push 0x7cadf0 at 0x59f5f6 passes it as D3d_FitTextureSize\'s out '
              'width, which stores a dword through it (mov dword ptr [eax], ecx at '
              '0x59fa2b; [ecx], eax at 0x59fa38); a u16 by its direct operands, a '
              'dword by that store',
    0x7CADF2: 'push 0x7cadf2 at 0x59f5f1 passes it as D3d_FitTextureSize\'s out '
              'height, which stores a dword through it (mov dword ptr [edx], ecx at '
              '0x59fa2d; [edx], eax at 0x59fa3a); a u16 by its direct '
              'operands, a dword by that store',
}
# The DAT arena: LoadDatFile copies every kind-0 payload to MessagePools
# (0x803580) + tag, so what an address here holds changes with the DAT
# loaded; one global type would be wrong. The extent is MessagePools' note in
# symbols.toml: the highest tag + size over the shipped DAT files, 0xE0C4C.
# Rule 1 does not catch these (a [[block]] has no extent).
ARENA = (0x803580, 0x803580 + 0xE0C4C)
ARENA_WHY = 'overlay-loaded arena (MessagePools 0x803580 + tag, filled from the DAT loaded)'

SIZES = {'char': 1, 'signed char': 1, 'unsigned char': 1, 'short': 2,
         'unsigned short': 2, 'const unsigned short': 2, 'float': 4, 'double': 8}
FINT = {'fild', 'fist', 'fistp', 'fiadd', 'fisub', 'fisubr', 'fimul', 'fidiv',
        'fidivr', 'ficom', 'ficomp', 'fisttp'}
FCTL = {'fnstcw', 'fldcw', 'fstcw', 'fnstsw', 'fstsw'}
NOWRITE = {'cmp', 'test', 'push', 'bt'}
WNAME = {1: 'byte', 2: 'word', 4: 'dword', 8: 'qword'}


def csize(ct):
    return 4 if ct.endswith('*') or ct not in SIZES else SIZES[ct]


def sections(b):
    pe = struct.unpack_from('<I', b, 0x3C)[0]
    n = struct.unpack_from('<H', b, pe + 6)[0]
    opt = struct.unpack_from('<H', b, pe + 20)[0]
    out = {}
    for i in range(n):
        name, vs, va, rs, ra = struct.unpack_from('<8sIIII', b, pe + 24 + opt + 40 * i)
        out[name.rstrip(b'\0').decode()] = (0x400000 + va, vs, ra)
    return out


class Ins:
    __slots__ = ('ea', 'size', 'mn', 'ops', 'text', 'mem', 'imms', 'regs_r', 'regs_w', 'first_mem')

    def __init__(self, i, dlo, dhi):
        self.ea, self.size, self.mn = i.address, i.size, i.mnemonic
        self.text = f'{i.mnemonic} {i.op_str}'.strip()
        self.mem, self.imms = [], []
        ops = i.operands
        self.first_mem = bool(ops) and ops[0].type == X86_OP_MEM
        self.ops = [(o.type, i.reg_name(o.reg) if o.type == X86_OP_REG else
                     (o.imm & 0xFFFFFFFF) if o.type == X86_OP_IMM else None) for o in ops]
        for o in ops:
            if o.type == X86_OP_MEM:
                if o.mem.segment not in (0, X86_REG_INVALID) and i.reg_name(o.mem.segment) in ('fs', 'gs'):
                    continue
                d = o.mem.disp & 0xFFFFFFFF
                if dlo <= d < dhi:
                    self.mem.append((d, o.size, o.mem.base != 0 or o.mem.index != 0))
            elif o.type == X86_OP_IMM:
                v = o.imm & 0xFFFFFFFF
                if dlo <= v < dhi and not i.mnemonic.startswith(('j', 'call')):
                    self.imms.append(v)
        try:
            r, w = i.regs_access()
            self.regs_r = {i.reg_name(x) for x in r}
            self.regs_w = {i.reg_name(x) for x in w}
        except Exception:  # capstone cannot say: treat as touching everything
            self.regs_r = self.regs_w = None


FAMILY = {}
for _r in 'abcd':
    FAMILY.update({f'e{_r}x': _r, f'{_r}x': _r, f'{_r}l': _r, f'{_r}h': _r})
for _r in ('si', 'di', 'bp', 'sp'):
    FAMILY.update({f'e{_r}': _r, _r: _r})
LOW = {f'{r}l': r for r in 'abcd'}
LOW.update({f'{r}x': r for r in 'abcd'})
LOW.update({r: FAMILY[r] for r in ('si', 'di', 'bp')})


def effective(stream, k, load_size):
    """The width a dword register load's value is used at: (width, note).

    Followed through register copies for up to seven instructions: an `and`
    with an immediate of at most 0xFF / 0xFFFF, or a movzx / movsx of the low
    byte / word, narrows it to 8 / 16 bits; reads of only al / ax (stores,
    test, cmp) are uses at that width and the walk goes on. Any read of the
    full register, a branch, a call, or the end of the window before a mask
    leaves it at its own width, as does a load that is not a dword.
    note is 'and 0x7f' / 'movzx' / 'low byte use' when narrowed, else ''."""
    ins = stream[k]
    if not ins.ops or ins.ops[0][0] != X86_OP_REG or ins.mn != 'mov' or load_size != 4:
        return load_size, ''
    live = {FAMILY.get(ins.ops[0][1])}
    used = 0
    for j in stream[k + 1:k + 8]:
        if j.mn.startswith(('j', 'call', 'ret', 'loop')) or j.regs_r is None:
            break
        o = j.ops
        if j.mn == 'mov' and len(o) == 2 and o[0][0] == o[1][0] == X86_OP_REG and FAMILY.get(o[1][1]) in live:
            if len(o[1][1]) == 3 and o[1][1].startswith('e'):
                live.add(FAMILY.get(o[0][1]))     # a full copy
                continue
        if j.mn == 'and' and len(o) == 2 and o[0][0] == X86_OP_REG and o[1][0] == X86_OP_IMM                 and FAMILY.get(o[0][1]) in live and o[1][1] <= 0xFFFF:
            w = max(used, 1 if o[1][1] <= 0xFF else 2)
            return w, f'and 0x{o[1][1]:x}'
        if j.mn in ('movzx', 'movsx') and len(o) == 2 and o[1][0] == X86_OP_REG                 and FAMILY.get(o[1][1]) in live and o[1][1] in LOW:
            return max(used, 1 if o[1][1].endswith('l') else 2), j.mn
        part = [x for x in j.regs_r if FAMILY.get(x) in live]
        if any(len(x) == 3 for x in part):        # eax..: the whole value
            break
        for x in part:
            used = max(used, 1 if x.endswith('l') else 2)
        live -= {FAMILY.get(x) for x in j.regs_w}
        if not live:
            return (used, 'low byte use' if used == 1 else 'low word use') if used else (load_size, '')
    return load_size, ''


def scan(exe, funcs, jumptables, func_starts):
    b = open(exe, 'rb').read()
    secs = sections(b)
    tva, tvs, tra = secs['.text']
    dlo = secs['.rdata'][0]
    dhi = secs['.data'][0] + secs['.data'][1]
    code = b[tra:tra + tvs]
    hosts = sorted((int(f['entry']), int(f['entry']) + int(f['size'])) for f in json.load(open(funcs))['functions'])
    hosts.append(GAP)
    # inline_tables rows are [start, bytes, largest byte]
    inline = [(int(t, 16), int(t, 16) + n) for t, n, _ in json.load(open(jumptables))['inline_tables']]
    starts = sorted(func_starts)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    segs = []
    for lo, hi in sorted(set(hosts)):
        edges = [lo] + starts[bisect.bisect_right(starts, lo):bisect.bisect_left(starts, hi)] + [hi]
        segs += list(zip(edges, edges[1:]))

    def decode(skips):
        skips = sorted(skips)
        sa = [x[0] for x in skips]
        streams, tables, restarts = [], set(), 0
        for lo, hi in segs:
            stream, p = [], lo
            while p < hi:
                j = bisect.bisect_right(sa, p) - 1
                if j >= 0 and p < skips[j][1]:
                    p = min(skips[j][1], hi)
                    continue
                stop = min(sa[j + 1] if j + 1 < len(sa) else hi, hi)
                for i in md.disasm(code[p - tva:stop - tva], p):
                    stream.append(Ins(i, dlo, dhi))
                    for o in i.operands:   # a switch's dword table in .text
                        if o.type == X86_OP_MEM and o.mem.scale == 4 and o.mem.index != 0                                 and tva <= (o.mem.disp & 0xFFFFFFFF) < tva + tvs:
                            tables.add(o.mem.disp & 0xFFFFFFFF)
                    p = i.address + i.size
                if p < stop:           # capstone stopped at a byte it cannot decode
                    restarts += 1
                    p += 1
            streams.append(stream)
        return streams, tables, restarts

    # pass 1 finds the switch tables, pass 2 steps over them
    _, tables, _ = decode(inline)
    cells = []
    for t in sorted(tables):
        e = t
        while e + 4 <= tva + tvs and tva <= struct.unpack_from('<I', code, e - tva)[0] < tva + tvs:
            e += 4
        if e > t:
            cells.append((t, e))
    streams, _, restarts = decode(inline + cells)
    acc, imm = [], []          # (addr, size, indexed, stream, k) ; (addr, Ins)
    for stream in streams:
        for k, ins in enumerate(stream):
            for d, sz, ix in ins.mem:
                acc.append((d, sz, ix, stream, k))
            for v in ins.imms:
                imm.append((v, ins))
    return acc, imm, (dlo, dhi), restarts, len(segs), len(cells)


def block_field(A, records):
    """(name, record, offset) when A lies in a [[block]]'s records, else None.
    records: [(pc, stride, count, size, name)] of the blocks with
    record_stride; size (record_size, else the stride) bounds each record."""
    for pc, stride, count, size, name in records:
        if pc <= A < pc + stride * count and (A - pc) % stride < size:
            return name, (A - pc) // stride, (A - pc) % stride
    return None


def classify(acc, imm, targets, prior, blocks, own, records=()):
    by = defaultdict(list)
    for a in acc:
        by[a[0]].append(a)
    immby = defaultdict(list)
    for v, ins in imm:
        immby[v].append(ins)
    keys = sorted(by)
    pstarts = sorted(prior)

    def eff(a):
        d, sz, ix, st, k = a
        ins = st[k]
        if ins.first_mem and ins.mn not in NOWRITE and ins.mn != 'lea':
            return sz, 'w', ''
        w, note = effective(st, k, sz)
        return w, 'r', note

    out = {}
    for A in sorted(targets):
        if A in blocks:
            continue
        bf = block_field(A, records)
        if bf is not None:
            out[A] = ('u', f'a field of {bf[0]}: record {bf[1]} +0x{bf[2]:X} (a record family, '
                           f'stage C1\'s struct, not a global of its own)')
            continue
        # the earliest typed entry whose extent holds A; an address that has
        # its own [[data]] entry is that entry, not an element of another
        box = None if A in own else next(
            (P for P in pstarts[:bisect.bisect_right(pstarts, A)] if P < A < P + prior[P][1]), None)
        if box is not None:
            out[A] = ('u', f'inside {prior[box][0]} (0x{box:X} + 0x{A - box:X}); an element of a typed entry, not its own')
            continue
        if A in HAND:
            out[A] = ('u', HAND[A])
            continue
        al = by[A]
        ixs = [a for a in al if a[2]]
        leas = [a for a in al if not a[2] and a[3][a[4]].mn == 'lea']
        adds = [x for x in immby[A] if x.mn == 'add']
        if ixs or adds:
            ex = [f'{a[3][a[4]].text} at 0x{a[3][a[4]].ea:x}' for a in ixs[:3]]
            ex += [f'{x.text} at 0x{x.ea:x}' for x in adds[:3]]
            sizes = sorted({a[1] for a in ixs if a[3][a[4]].mn != 'lea'})
            kinds = []
            if any(a[3][a[4]].mn == 'lea' for a in ixs):
                kinds.append('lea [reg + addr]')
            if any(a[3][a[4]].mn != 'lea' for a in ixs):
                kinds.append('/'.join(f'{s}-byte' for s in sizes) + ' by [reg + addr]')
            if adds:
                kinds.append('add reg, addr')
            out[A] = ('u', f'indexed: {"; ".join(kinds)} (e.g. {"; ".join(ex)}); a table, its count unread')
            continue
        direct = [a for a in al if not a[2] and a[3][a[4]].mn != 'lea']
        if not direct:
            why = 'no instruction in the 3,004 functions of pc_funcs.json or 0x401000..0x4020B0 addresses it; reached by base + offset or by pointer, width unread'
            if leas:
                why = f'no access but lea of its address ({leas[0][3][leas[0][4]].text} at 0x{leas[0][3][leas[0][4]].ea:x}); reached by pointer, width unread'
            out[A] = ('u', why)
            continue
        if ARENA[0] <= A < ARENA[1]:
            ws = Counter(eff(a)[0] for a in direct)
            masked = sum(1 for a in direct if eff(a)[2].startswith('and'))
            note = ', '.join(f'{n} {w}-byte' for w, n in sorted(ws.items())) + ' operand(s)'
            if masked:
                note += f', {masked} of them dword loads masked to {8 * max(ws)} bits'
            out[A] = ('u', f'{ARENA_WHY}; {note}')
            continue
        e = [(eff(a), a) for a in direct]
        widths = Counter(x[0][0] for x in e)
        fpu = [a for a in direct if a[3][a[4]].mn.startswith('f') and a[3][a[4]].mn not in FCTL]
        fint = [a for a in fpu if a[3][a[4]].mn in FINT]
        raw = Counter(a[1] for a in direct)
        if fpu and not fint and len(raw) == 1:
            W = next(iter(raw))
            ctype = {4: 'float', 8: 'double'}.get(W)
        elif len(widths) == 1:
            W = next(iter(widths))
            ctype = None
        else:
            parts = []
            for w in sorted(widths):
                ex = [a for (ew, _, _), a in e if ew == w][:2]
                parts.append(f'{w}-byte x{widths[w]} (e.g. ' + '; '.join(f'{a[3][a[4]].text} at 0x{a[3][a[4]].ea:x}' for a in ex) + ')')
            out[A] = ('u', 'mixed widths: ' + ', '.join(parts))
            continue
        # rule 6: a store at a neighbour that writes a byte of [A, A + W)
        cover = []
        lo = bisect.bisect_left(keys, A - 7)
        for K in keys[lo:]:
            if K >= A + W:
                break
            if K == A:
                continue
            for a in by[K]:
                if a[2] or a[3][a[4]].mn == 'lea':
                    continue
                w, kind, _ = eff(a)
                if kind == 'w' and K < A + W and K + w > A:
                    cover.append((K, w, a))
        if cover:
            K0 = Counter((K, w) for K, w, _ in cover).most_common(1)[0][0]
            ex = [a for K, w, a in cover if (K, w) == K0][:2]
            out[A] = ('u', f'mixed widths: its {W}-byte operands, but also written as part of the '
                           f'{WNAME.get(K0[1], K0[1])} at 0x{K0[0]:x} ({len(cover)} store(s), e.g. '
                      + '; '.join(f'{a[3][a[4]].text} at 0x{a[3][a[4]].ea:x}' for a in ex) + ')')
            continue
        mnem = [a[3][a[4]].mn for a in direct] + [x[0][2] for x in e if x[0][2] in ('movzx', 'movsx')]
        if ctype is None:
            if IAT[0] <= A < IAT[1] and W == 4:
                ctype = 'void *'
            elif W == 4:
                ctype = 'unsigned long'
            elif W in (1, 2):
                signed = 'movsx' in mnem and 'movzx' not in mnem
                ctype = ('signed char' if W == 1 else 'short') if signed else \
                        ('unsigned char' if W == 1 else 'unsigned short')
            else:
                out[A] = ('u', f'mixed widths: {W}-byte operands only, not an integer width')
                continue
        masked = [(x, a) for x, a in e if x[2]]
        shown = [f'{a[3][a[4]].text} at 0x{a[3][a[4]].ea:x}' for a in direct[:3]]
        more = f' and {len(direct) - 3} more' if len(direct) > 3 else ''
        notes = []
        if masked:
            how = Counter(x[2] for x, a in masked)
            notes.append(f'{len(masked)} of them {WNAME[max(raw)]} loads narrowed before use ({", ".join(f"{k} x{n}" for k, n in sorted(how.items()))}, '
                         f'e.g. at 0x{masked[0][1][3][masked[0][1][4]].ea:x})')
        if ctype in ('signed char', 'short'):
            notes.append('Every widening read is movsx')
        elif 'movzx' in mnem:
            notes.append('Widened with movzx')
        if IAT[0] <= A < IAT[1]:
            notes.append('A slot of the import address table (PE data directory 12, 0x5C4000..0x5C41B4): a pointer the loader fills')
        if ctype in ('float', 'double'):
            notes.append(f'FPU: {", ".join(f"{k} x{n}" for k, n in sorted(Counter(a[3][a[4]].mn for a in fpu).items()))}; '
                         'the integer moves copy the bit pattern')
        if leas:
            notes.append(f'Its address is also taken by lea at 0x{leas[0][3][leas[0][4]].ea:x}')
        im = [x for x in immby[A] if x.mn != 'add']
        if im:
            notes.append(f'Its address is also an immediate {len(im)} time(s) ({im[0].text} at 0x{im[0].ea:x})')
        out[A] = ('s', ctype, len(direct), W, '; '.join(shown) + more, notes)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--exe', required=True, help='BOF3.exe (read only)')
    ap.add_argument('--funcs', required=True, help='analysis/pc_funcs.json')
    ap.add_argument('--jumptables', required=True, help='analysis/pc_jumptables.json')
    ap.add_argument('--toml', required=True, help='symbols.toml')
    ap.add_argument('--settled', required=True, help='out TSV: address, ctype, operands, width, examples, notes')
    ap.add_argument('--unsettled', required=True, help='out TSV: address, constants, reason')
    ap.add_argument('--census', action='append',
                    help='data_census.tsv: measure its module-only addresses instead (stage C3); '
                         'repeatable, the union of the files given')
    a = ap.parse_args()
    root = os.path.dirname(os.path.abspath(a.toml))
    t = tomllib.load(open(a.toml, 'rb'))
    blocks = {x['pc'] for x in t.get('block', [])}
    records = [(x['pc'], x['record_stride'], x['record_count'],
                x.get('record_size', x['record_stride']), x['name'])
               for x in t.get('block', []) if 'record_stride' in x]
    prior = {}
    typed = set()
    for e in t['data']:
        if e.get('ctype') and (a.census or SG_TAG not in e.get('evidence', '')):
            prior[e['pc']] = (e['name'], csize(e['ctype']) * e.get('count', 1))
            typed.add(e['pc'])
    consts = defaultdict(list)
    acc, imm, (dlo, dhi), restarts, nseg, ncells = scan(a.exe, a.funcs, a.jumptables, [f['pc'] for f in t['func']])
    for h, line, k, v in module_constants(root):
        if dlo <= v < dhi:
            consts[v].append(f'{h}:{line} {k}')
    targets = {v for v in consts if v not in typed}
    if a.census:
        consts = defaultdict(list)
        entries = {e['pc'] for e in t['data']}
        for path in a.census:
            rows = []
            with open(path, encoding='utf-8') as f:
                for line in f:
                    r = line.rstrip('\n').split('\t')
                    if r[0] == 'const' and r[3] == 'data':
                        rows.append(r)
            at_named = {int(r[2], 16) for r in rows if r[10] == 'at'}
            for r in rows:
                v = int(r[2], 16)
                if r[10] == 'module' and v not in at_named and v not in entries and dlo <= v < dhi:
                    for m in r[5].split(';'):
                        if f'{m} {r[1]}' not in consts[v]:
                            consts[v].append(f'{m} {r[1]}')
        targets = set(consts)
    # an entry SG itself created is re-measured; any other entry is its own
    own = {e['pc'] for e in t['data']
           if a.census or not (e.get('evidence', '').startswith('measured') and SG_TAG in e.get('evidence', ''))}
    res = classify(acc, imm, targets, prior, blocks, own, records)
    with open(a.settled, 'w', encoding='utf-8', newline='\n') as f:
        f.write('address\tctype\toperands\twidth\texamples\tnotes\tconstants\n')
        for A, r in sorted(res.items()):
            if r[0] == 's':
                f.write(f'0x{A:X}\t{r[1]}\t{r[2]}\t{r[3]}\t{r[4]}\t{". ".join(r[5])}\t{"; ".join(consts[A])}\n')
    with open(a.unsettled, 'w', encoding='utf-8', newline='\n') as f:
        f.write('address\tconstants\treason\n')
        for A, r in sorted(res.items()):
            if r[0] == 'u':
                f.write(f'0x{A:X}\t{"; ".join(consts[A])}\t{r[1]}\n')
    def kind(A, r):
        if r[0] == 's':
            return 'settled'
        if A in HAND:
            return 'hand-read'
        return 'inside' if r[1].startswith('inside') else r[1].split(':')[0].split(' (')[0].split(';')[0]
    kinds = Counter(kind(A, r) for A, r in res.items())
    # no two typed extents may overlap: the settled ones against each other and
    # against the earlier entries (a warning; Party_Zenny inside Cond_Flags'
    # declared rows is the one known, the rows being a hypothesis)
    ext = sorted([(P, P + n, nm) for P, (nm, n) in prior.items()] +
                 [(A, A + csize(r[1]), f'0x{A:X}') for A, r in res.items() if r[0] == 's'])
    for i, (p0, p1, nm) in enumerate(ext):
        for q0, q1, nm2 in ext[i + 1:]:
            if q0 >= p1:
                break
            if nm.startswith('0x') or nm2.startswith('0x'):
                print(f'data_widths: overlap: {nm} 0x{p0:X}..0x{p1:X} and {nm2} 0x{q0:X}..0x{q1:X}')
    print(f'data_widths: {len(consts)} module-constant data addresses, {len(targets)} targets '
          f'({len(blocks & targets)} block bases skipped), {nseg} segments, {ncells} switch tables stepped over, '
          f'{restarts} decode restarts, '
          f'{len(acc)} data operands; ' + ', '.join(f'{k} {n}' for k, n in sorted(kinds.items())))


if __name__ == '__main__':
    main()
