#!/usr/bin/env python
"""The scenario round's bands, function by function, and a group's clone table
for the scenario harness.

    python tools/scenario_rows.py                   -> analysis/scenario_funcs.tsv
                                                       and a summary per band
    python tools/scenario_rows.py --unit SC0        -> one band's functions
    python tools/scenario_rows.py --unit SC0 --clones
                                                    -> its clone table for the
                                                       scenario harness
                                                       (docs/scenario_harness.md)

The bands are the scenario round's groups (docs/takeover-queue-scenario.md
section 3, with docs/takeover-queue-round10.md section 1's boundaries for the
first wave): whole chapters in address order, 0x537F20..0x56D5E0, plus the
shared engine-side helpers (SE, nine addresses outside the band) and chapter
15's one function before it (0x537580). A group owns the functions whose
address lies in its band (the walk's closure crosses bands; the band does not).

**The starts** in a band are pe_funcs.py's (analysis/pc_funcs.json),
pe_hidden.py's (analysis/pc_hidden.json), the scenario walk's added starts
(analysis/scenario_roots.json `added_starts`, tools/scenario_roots.py) and the
chapter tables' own roots (the 20 vtables' slots and the call tables'
entries, read off the exe: two roots of chapter 0, 0x539A10 and 0x539A30, are
in no list). They are corrected by tools/magic_rows.py's recursive descent,
run over the band: a start that is a case of another function's jump table or
that a function falls into is dropped, and code no start covers (after a jump
table, or reached only by a tail jmp) becomes a start. Jump tables are
bounded at their cmp / ja (magic_rows.py, S26); a switch's table lives in
.text right after its function and its cases may lie past a neighbouring
start (docs/scenario-roots.md section 3).

Sizes are a function's own bytes - its recursive descent, jump tables
included, to its last instruction - not the padding after it.

**The clone table** (--clones) is magic_rows.py's for the scenario harness:
per function not yet ours, its extent, every E8 / E9 that leaves it, every
stack-table immediate, every jump table, a note for each call / jmp through
.data (list that table as a DataTable) and a REFUSED line for what a byte copy
cannot carry (a conditional jump out of the extent, a short jmp out, an
indirect jmp through .text). A root's comment says its call shape (a vtable
slot, a hook (x, z), a call-table entry), which becomes the Clone's `shape`.

Reads bof3/BOF3.exe (--exe), analysis/ (--analysis, the main checkout's when
run from a worktree), symbols.toml and the sibling checkout (--sibling, for
the PSX twin hypotheses of analysis/pairs_propagated.json, printed as a
column). The output lists addresses and sizes only, but it is derived from
copyrighted game code: it lives under analysis/ and is never committed
(CLAUDE.md rule 1).
"""
import argparse, bisect, collections, json, os, struct, sys, tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import magic_rows  # noqa: E402  (the descent, the clone sites)
from magic_rows import Image, clone_sites, _code_run  # noqa: E402

BANK_LO = 0x537F20          # chapter 0's slot 0
BANK_HI = 0x56D5E0          # Scenario_Start, the engine again
VTABLES, CALL_A, CALL_B = 0x662C80, 0x660B84, 0x660BD4
CHAPTERS = 20
SLOTS = 5
SLOT_SHAPE = ['kSlot', 'kObject', 'kHook', 'kHook', 'kHook']
SLOT_USE = ['slot 0, the frame (Field_ModeDispatch)', 'slot 1, the object trigger (0x56D6D0, the object)',
            'slot 2, the step hook (x, z), al', 'slot 3, the arrive hook (x, z), al', 'slot 4, the cell hook (x, z), al']

# The groups (docs/takeover-queue-scenario.md section 3; wave one's
# boundaries from docs/takeover-queue-round10.md section 1). Each band runs to
# the next one's start; SC16 (done in title-states but slot 1) and SC17
# (chapters 17..19, stubs) are listed so every start of the bank has a band.
BANDS = [
    ('SC0', [(0x537F20, 0x539AD0)], '0'),
    ('SC1', [(0x539AD0, 0x53DDA0)], '1'),
    ('SC2a', [(0x53DDA0, 0x540000)], '2 (first half)'),
    ('SC2b', [(0x540000, 0x5428C0)], '2 (second half)'),
    ('SC3', [(0x5428C0, 0x546390)], '3, 4'),
    ('SC5', [(0x546390, 0x54A910)], '5'),
    ('SC6', [(0x54A910, 0x54F080)], '6'),
    ('SC7', [(0x54F080, 0x553B30)], '7, 8'),
    ('SC9a', [(0x553B30, 0x557170)], '9'),
    ('SC9b', [(0x557170, 0x55C040)], '9 (tail), 10'),
    ('SC11', [(0x55C040, 0x55E4E0)], '11'),
    ('SC12', [(0x55E4E0, 0x561DB0)], '12 (first block)'),
    ('SC13', [(0x561DB0, 0x567DC0)], '12 (tail), 13, 14'),
    ('SC15', [(0x567DC0, 0x56B2A0), (0x537580, 0x537581)], '15'),
    ('SC16', [(0x56B2A0, 0x56C130)], '16 (title-states; slot 1 left)'),
    ('SC17', [(0x56C130, BANK_HI)], '17, 18, 19'),
]
# SE: the engine-side helpers the chapters share (round ten section 1: nine).
CH15_OUTSIDE = 0x537580     # chapter 15's one function before the bank (SC15's)
SE_ADDRS = [0x4410B0, 0x508000, 0x5080A0, 0x519F70, 0x520000, 0x524870, 0x579D70, 0x57A010, 0x591CC0]


def load_toml(path):
    with open(path, 'rb') as f:
        return tomllib.load(f)


def read_roots(img):
    """Per chapter: the vtable, its five slots, call tables A and B with their
    entries. A call table runs to the next table pointer (A and B are
    interleaved back to back, 0x65F664..)."""
    a_ptrs = [img.u32(CALL_A + 4 * c) for c in range(CHAPTERS)]
    b_ptrs = [img.u32(CALL_B + 4 * c) for c in range(CHAPTERS)]
    bounds = sorted({p for p in a_ptrs + b_ptrs if p})
    roots = {}
    for c in range(CHAPTERS):
        vt = img.u32(VTABLES + 4 * c)
        slots = [img.u32(vt + 4 * k) for k in range(SLOTS)]
        tabs = {}
        for kind, p in (('A', a_ptrs[c]), ('B', b_ptrs[c])):
            ents = []
            if p:
                i = bisect.bisect_right(bounds, p)
                stop = bounds[i] if i < len(bounds) else p + 4 * 64
                x = p
                while x < stop:
                    w = img.u32(x)
                    if w is None or not img.in_text(w):
                        break
                    ents.append(w)
                    x += 4
            tabs[kind] = (p, ents)
        roots[c] = dict(vtable=vt, slots=[s if s and img.in_text(s) else None for s in slots], A=tabs['A'], B=tabs['B'])
    return roots


CALLS_RANGE = [0, 0]        # set by main: the call tables' engine-side block


def band_of(x):
    for name, ranges, _ in BANDS:
        for lo, hi in ranges:
            if lo <= x < hi:
                return name
    if x in SE_ADDRS:
        return 'SE'
    if CALLS_RANGE[0] <= x < CALLS_RANGE[1]:
        return 'CALLS'
    return None


def drop_byte_tables(img, funcs):
    """MSVC's two-level switch keeps its byte table in .text after the jump
    table: movzx r, byte [r + T2] / jmp [r*4 + T]. magic_rows.py's descent
    counts it only when a cmp bounds it, so the code-after-a-table rule can
    take T2 for a function (SC1: 0x53B120, 0x53D390). Every start that lies
    in a byte table a function loads from is dropped, and that function's
    extent runs over the table (to its cmp's bound, else to the next start).
    Returns the dropped starts."""
    tables = []     # (T2, owner, bound or None)
    for s in sorted(funcs):
        end = funcs[s]['end']
        o = img.off(s)
        if o is None:
            continue
        prev = []
        for ins in magic_rows.MD.disasm(img.data[o:o + (end - s)], s):
            ops = ins.operands
            if ins.mnemonic in ('mov', 'movzx') and len(ops) == 2 and ops[1].type == magic_rows.x86.X86_OP_MEM \
                    and ops[1].size == 1 and (ops[1].mem.index != 0 or ops[1].mem.base != 0):
                t2 = ops[1].mem.disp & 0xFFFFFFFF
                if img.in_text(t2):
                    b = magic_rows._cmp_bound(prev)
                    tables.append((t2, s, b + 1 if b is not None and b < 0x400 else None))
            prev = (prev + [ins])[-6:]
    dropped = []
    starts = sorted(funcs)
    for t2, owner, n in tables:
        nxt = [x for x in starts if x > owner and x not in dropped]
        for f in nxt:
            if t2 <= f < (t2 + n if n else t2 + 0x100) and funcs[f].get('found'):
                dropped.append(f)
        bound = t2 + n if n else next((x for x in nxt if x >= t2 and x not in dropped), t2)
        if owner in funcs:
            funcs[owner]['end'] = max(funcs[owner]['end'], bound)
    for f in dropped:
        funcs.pop(f, None)
    return sorted(set(dropped))


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--exe', default=os.path.join(repo, 'bof3', 'BOF3.exe'))
    ap.add_argument('--analysis', default=os.path.join(repo, 'analysis'))
    ap.add_argument('--sibling', default=os.path.join(os.path.dirname(repo), 'BreathOfFire3Recomp'))
    ap.add_argument('--symbols', default=os.path.join(repo, 'symbols.toml'))
    ap.add_argument('--unit', help='print one band (SE, SC0, SC1, SC2a, ... SC17) function by function')
    ap.add_argument('--clones', action='store_true',
                    help="with --unit: print the band's functions not yet ours as scenario_harness clone tables (C++)")
    ap.add_argument('--with-ours', action='store_true',
                    help='with --clones: print the functions already ours too (a taken group re-checking its table)')
    ap.add_argument('--quiet', action='store_true', help='write the TSV, print only the totals')
    a = ap.parse_args()

    img = Image(a.exe)
    sym = load_toml(a.symbols)
    ours = {f['pc']: f['name'] for f in sym.get('func', []) if 'impl' in f}
    named = {f['pc']: f['name'] for f in sym.get('func', [])}

    pf = json.load(open(os.path.join(a.analysis, 'pc_funcs.json')))
    recorded = {f['entry'] for f in pf['functions']}
    hidden = {h['entry'] for h in json.load(open(os.path.join(a.analysis, 'pc_hidden.json')))}
    sr = json.load(open(os.path.join(a.analysis, 'scenario_roots.json')))
    added = {int(x, 16) for x in sr['added_starts']}
    walked = {int(x, 16) for x in sr['walked']}
    roots = read_roots(img)
    root_of = collections.defaultdict(list)   # address -> [(chapter, 'slot k' / 'A[i]' / 'B[i]', shape)]
    for c, r in roots.items():
        for k, s in enumerate(r['slots']):
            if s:
                root_of[s].append((c, SLOT_USE[k], SLOT_SHAPE[k]))
        for kind in ('A', 'B'):
            for i, s in enumerate(r[kind][1]):
                root_of[s].append((c, 'call table %s entry %d' % (kind, i), 'kEntry'))
    root_starts = set(root_of)

    twins = {}
    pp = os.path.join(a.analysis, 'pairs_propagated.json')
    if os.path.exists(pp):
        # a hypothesis only: the file has known errors (HANDOFF item 9)
        for p in json.load(open(pp)):
            twins.setdefault(p['pc'], '%#x (%s)' % (p['psx'], p.get('how', '?')))

    # the descent over the whole bank, with magic_rows.py's rules
    magic_rows.BAND_LO, magic_rows.BAND_HI = BANK_LO, BANK_HI
    listed = recorded | hidden | added | root_starts
    bank = sorted(s for s in listed if BANK_LO <= s < BANK_HI)
    funcs = magic_rows.discover(img, bank)
    # the call tables' block: the engine-side code call tables A and B point
    # at (0x519890..), outside every chapter's band - a unit of its own, CALLS
    entries = sorted({e for r in roots.values() for kind in ('A', 'B') for e in r[kind][1]})
    calls_lo = min(e for e in entries if e >= 0x519000)
    later = sorted(s for s in listed if s > max(entries))
    calls_hi = later[0] if later else max(entries) + 0x100
    CALLS_RANGE[:] = [calls_lo, calls_hi]
    magic_rows.BAND_LO, magic_rows.BAND_HI = calls_lo, calls_hi
    funcs.update(magic_rows.discover(img, sorted(s for s in listed if calls_lo <= s < calls_hi)))
    # the SE helpers and chapter 15's 0x537580, each alone
    all_known = set(listed)
    for x in SE_ADDRS + [CH15_OUTSIDE]:
        funcs[x] = magic_rows.descend(img, x, all_known)
    dropped_bytes = drop_byte_tables(img, funcs)
    magic_rows.read_tables(img, funcs, sym)
    starts = sorted(funcs)
    dropped = sorted(set(bank) - set(funcs))
    found = sorted(s for s in starts if funcs[s].get('found'))

    def size(x):
        return funcs[x]['end'] - x

    def src(x):
        return (('R' if x in recorded else '') + ('H' if x in hidden else '') + ('A' if x in added else '')
                + ('T' if x in root_starts else '') + ('S' if funcs[x].get('found') else ''))

    bands = collections.OrderedDict()
    for name, ranges, chapters in BANDS[:1] + [('SE', [], 'shared helpers')] + BANDS[1:] + [
            ('CALLS', [tuple(CALLS_RANGE)], 'the call tables engine-side block')]:
        fs = [s for s in starts if band_of(s) == name]
        bands[name] = dict(ranges=ranges, chapters=chapters, funcs=fs)

    os.makedirs(a.analysis, exist_ok=True)
    tsv = os.path.join(a.analysis, 'scenario_funcs.tsv')
    with open(tsv, 'w', encoding='utf-8') as f:
        f.write('start\tsize\tgroup\tours\twalked\tsource\tname\troots\tpsx_twin_hypothesis\n')
        for x in starts:
            f.write('%#x\t%#x\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' % (
                x, size(x), band_of(x) or '-', 'ours' if x in ours else '', 'walked' if x in walked else '',
                src(x), named.get(x, ''), '; '.join('ch%d %s' % (c, u) for c, u, _ in root_of.get(x, [])),
                twins.get(x, '')))

    if a.unit:
        u = bands.get(a.unit)
        if u is None:
            sys.exit('no band %s (the bands: %s)' % (a.unit, ', '.join(bands)))
        if a.clones:
            print_clones(img, [x for x in u['funcs'] if a.with_ours or x not in ours], funcs, named, root_of)
            return
        rng = ', '.join('%#x..%#x' % r for r in u['ranges']) or ' '.join('%#x' % x for x in SE_ADDRS)
        print('%s  chapters %s  %s  %d functions, %#x bytes, %d ours, %d not walked' % (
            a.unit, u['chapters'], rng, len(u['funcs']), sum(size(x) for x in u['funcs']),
            sum(1 for x in u['funcs'] if x in ours), sum(1 for x in u['funcs'] if x not in walked)))
        for x in u['funcs']:
            print('  %#x %5x %-5s %-4s %-7s %-28s %s' % (
                x, size(x), src(x), 'ours' if x in ours else '', 'walked' if x in walked else '-',
                ours.get(x, named.get(x, '')), '; '.join('ch%d %s' % (c, us) for c, us, _ in root_of.get(x, []))))
        return

    overlaps = [(x, y) for x, y in zip(starts, starts[1:]) if funcs[x]['end'] > y and band_of(x) and band_of(y)]
    print('bank %#x..%#x (and 0x537580, and SE): %d starts listed (pc_funcs, pc_hidden, the walk\'s added, the tables\' roots); '
          '%d dropped (a jump-table case or fallen into): %s; %d found by the descent: %s'
          % (BANK_LO, BANK_HI, len(bank), len(dropped), ' '.join('%#x' % x for x in dropped), len(found),
             ' '.join('%#x' % x for x in found)))
    print('extents overlapping the next start: %d %s' % (len(overlaps), ' '.join('%#x>%#x' % o for o in overlaps)))
    print('byte tables of two-level switches taken for code and dropped: %s' % (
        ' '.join('%#x' % x for x in dropped_bytes) or 'none'))
    print('the call tables block CALLS %#x..%#x (engine-side, outside every chapter band)' % tuple(CALLS_RANGE))
    print('roots in no start list: %s' % (' '.join('%#x' % x for x in sorted(root_starts - recorded - hidden - added)
                                                   if BANK_LO <= x < BANK_HI) or 'none'))
    if not a.quiet:
        print()
        print('%-5s %-18s %-24s %4s %7s %4s %5s %7s' % ('group', 'chapters', 'band', 'fns', 'bytes', 'ours', 'take', 'unwalkd'))
        for name, u in bands.items():
            fs = u['funcs']
            rng = ', '.join('%#x..%#x' % r for r in u['ranges']) or 'nine addresses'
            print('%-5s %-18s %-24s %4d %7d %4d %5d %7d' % (
                name, u['chapters'][:18], rng[:24], len(fs), sum(size(x) for x in fs), sum(1 for x in fs if x in ours),
                sum(1 for x in fs if x not in ours), sum(1 for x in fs if x not in walked)))
    print('\nwrote %s' % tsv)


def print_clones(img, addrs, funcs, named, root_of):
    """C++ for a group's clone table (scenario_harness.h), by capstone."""
    lines = []
    for x in addrs:
        end = funcs[x]['end']
        calls, imms, tables, refused = clone_sites(img, x, end)
        tag = '%X' % x
        name = named.get(x, 'Fn_' + tag)
        rs = root_of.get(x, [])
        print('// 0x%X: 0x%X bytes%s%s' % (x, end - x, ''.join(
            ('; +0x%X %s' if r[1].startswith('note') else '; REFUSED +0x%X %s') % r for r in refused),
            ''.join('; root: chapter %d %s' % (c, u) for c, u, _ in rs)))
        if calls:
            print('constexpr scenario_harness::CallSite kCalls%s[] = {%s};' % (
                tag, ', '.join('{0x%X, 0x%X}' % c for c in calls)))
        if imms:
            print('constexpr scenario_harness::Imm kImms%s[] = {%s};' % (tag, ', '.join('{0x%X, 0x%X}' % i for i in imms)))
        if tables:
            print('constexpr scenario_harness::JumpTable kTables%s[] = {%s};' % (
                tag, ', '.join('{0x%X, 0x%X, %d}' % t for t in tables)))
        cell = lambda v, k: ('k%s%s, SH_N(k%s%s)' % (k, tag, k, tag)) if v else 'nullptr, 0'
        shapes = sorted({s for _, _, s in rs})
        shape = ''
        if len(shapes) > 1:
            # Roots of different shapes (an object slot that is also a call
            # table's entry, say): no one shape is right for all of them, and
            # picking one fuzzes the function with the wrong arguments or
            # compares an al a void function never set. Left for the reader,
            # loudly: the token does not compile until a shape is chosen.
            shape = ', SHAPE_UNDECIDED_%s' % '_'.join(shapes)
            print('scenario_rows: 0x%X has roots of shapes %s; its row needs one chosen by hand'
                  % (x, ', '.join(shapes)), file=sys.stderr)
        elif shapes and shapes != ['kSlot']:
            # the harness's default is kSlot (void, no arguments); a hook
            # answers in al (ret_mask 0xFF), an object slot takes the object
            s = shapes[0]
            shape = ', %s, false, scenario_harness::Shape::%s' % ('0xFF' if s == 'kHook' else '0', s)
        lines.append('    {"%s", 0x%X, 0x%X, %s, %s, %s, reinterpret_cast<const void*>(&::%s)%s},' % (
            name, x, end - x, cell(calls, 'Calls'), cell(imms, 'Imms'), cell(tables, 'Tables'), name, shape))
    print('#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])')
    print('const scenario_harness::Clone kClones[] = {')
    for line in lines:
        print(line)
    print('};')


if __name__ == '__main__':
    main()
