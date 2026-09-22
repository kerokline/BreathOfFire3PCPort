#!/usr/bin/env python
"""Pair PSX functions with PC ones through the tables both builds kept.

    python tools/psx_pair.py areas     -> analysis/area_pairs.json
    python tools/psx_pair.py tables    -> analysis/table_matches.json
    python tools/psx_pair.py fill      -> analysis/area_pairs_filled.json
    python tools/psx_pair.py propagate -> analysis/pairs_propagated.json

`areas`: the boot EXE's 200-entry area descriptor table (0x801802EC) has a PC
twin at 0x667590 (docs/attract-remaining.md section 5). Each descriptor's +0x3C
is a handler array and +0x40 an init function on both sides; the PSX side is the
sibling's names/area_records.toml. Pairs them slot for slot, then measures
whether each overlay's order survives on the PC and whether the gaps between
paired anchors hold as many function starts on both sides.

`tables`: every run of code pointers in the PC data and in the PSX images (boot
EXE plus the sibling's 406 overlay captures), matched by shape - each distinct
target numbered by first appearance, nulls and bare returns marked - as whole
tables or as a window of a longer run.

`fill`: pairs every start inside an anchor gap of equal counts by position,
and checks the result against call edges with a shifted baseline.

`propagate`: grows the pairs to a fixed point with four methods, each run only
when the ones before it add nothing - call lists of equal length aligned
position by position (refused if any position contradicts a pair); gap fill in
every address space, the boot EXE included (refused if any pair in the gap is
more than 2x off the typical size ratio); pointer tables that already agree on
two slots; and callers - an unpaired PSX function whose paired callees are all
called by exactly one PC function. Pairs that a usable alignment contradicts
at the end are kept but tagged call-disputed. docs/attract-remaining.md 5.1
has the measured precision of each.

Reads the sibling checkout (--sibling). Output is derived from copyrighted game
code: it lives under analysis/ and is never committed (CLAUDE.md rule 1).
"""
import argparse, base64, bisect, collections, functools, json, math, statistics, struct, tomllib
import pe_hidden

PC_AREA_TABLE = 0x667590   # 200 pointers to 0x44-byte area descriptors
PSX_BOOT_LO = 0x80093800   # SLPS_009.90 code, after its 0x800-byte header
PSX_BOOT_TEXT = 0x163800
AREAS = 200


def psx_area_records(sib):
    recs = tomllib.load(open(sib + 'names/area_records.toml', 'rb'))['record']
    section, slots = {}, collections.defaultdict(dict)
    for r in recs:
        section[r['area']] = r['section']
        if r['kind'].startswith('handler['):
            slots[r['area']][int(r['kind'][8:-1])] = int(r['entry'], 16)
        elif r['kind'] == 'init':
            slots[r['area']]['init'] = int(r['entry'], 16)
    return section, slots


def cmd_areas(a):
    img = pe_hidden.Image(a.exe)
    rd = img.dword
    section, psx = psx_area_records(a.sibling)
    pairs, disagree = [], []
    for k in range(AREAS):
        p = rd(PC_AREA_TABLE + 4 * k)
        h, init = (rd(p + 0x3C), rd(p + 0x40)) if p else (0, 0)
        n = 0
        while h and img.in_text(rd(h + 4 * n)):
            n += 1
        handlers = sorted(j for j in psx.get(k, {}) if j != 'init')
        if n != len(handlers) or bool(init) != ('init' in psx.get(k, {})):
            disagree.append(dict(area=k, pc_handlers=n, psx_handlers=len(handlers),
                                 pc_init=init, psx_init=psx.get(k, {}).get('init')))
            continue
        for j in handlers:
            pairs.append(dict(area=k, section=section[k], slot=j, psx=psx[k][j], pc=rd(h + 4 * j)))
        if init:
            pairs.append(dict(area=k, section=section[k], slot='init', psx=psx[k]['init'], pc=init))

    back = collections.defaultdict(set)
    keys = collections.defaultdict(set)
    for r in pairs:
        back[r['pc']].add((r['section'], r['psx']))
        keys[(r['section'], r['psx'])].add(r['pc'])
    print(f'{AREAS - len(disagree)} of {AREAS} areas agree on handler count and init; '
          f'{len(pairs)} pairs, {len(back)} distinct PC functions; '
          f'{sum(1 for v in keys.values() if len(v) > 1)} PSX entries with more than one PC twin; '
          f'{sum(1 for v in back.values() if len(v) > 1)} PC functions shared by several PSX entries')
    for d in disagree:
        print('  disagrees:', d)

    # Order and gap counts, over anchors no other PSX entry shares.
    caps = {c['source_md5']: c for c in json.load(open(a.sibling + 'analysis/overlay_captures_all.json'))}
    funcs = {f['entry'] for f in json.load(open(a.funcs))['functions']}
    hidden = {h['entry'] for h in json.load(open(a.hidden))}
    pc_starts = sorted(funcs | hidden | set(back))
    by = collections.defaultdict(set)
    for r in pairs:
        if len(back[r['pc']]) == 1:
            by[r['section']].add((r['psx'], r['pc']))
    ordered = total = 0
    gaps = collections.Counter()
    for s, v in by.items():
        if len(v) < 2:
            continue
        v = sorted(v)
        total += 1
        ordered += [b for _, b in v] == sorted(b for _, b in v)
        if s not in caps:
            continue
        c = caps[s]
        ps = sorted({int(x, 16) for key in ('static_discovery_entry_pcs', 'header_entry_pcs',
                                            'engine_entry_pcs') for x in c.get(key, [])}
                    | {p for p, _ in v})
        for (a1, b1), (a2, b2) in zip(v, v[1:]):
            if b2 > b1:
                n_psx = bisect.bisect_left(ps, a2) - bisect.bisect_left(ps, a1)
                n_pc = bisect.bisect_left(pc_starts, b2) - bisect.bisect_left(pc_starts, b1)
                gaps[n_pc - n_psx] += 1
    print(f'{ordered} of {total} overlays keep their PSX order on the PC; '
          f'{gaps[0]} of {sum(gaps.values())} anchor gaps hold equal start counts; '
          f'PC minus PSX elsewhere: {dict(sorted((k, n) for k, n in gaps.items() if k))}')
    json.dump(dict(pairs=pairs, disagree=disagree), open(a.out or 'analysis/area_pairs.json', 'w'), indent=0)


def psx_starts(cap, extra=()):
    """The sibling's known starts in one overlay capture."""
    return sorted({int(x, 16) for key in ('static_discovery_entry_pcs', 'header_entry_pcs',
                                          'engine_entry_pcs') for x in cap.get(key, [])} | set(extra))


def psx_callees(cap, starts):
    """jal targets of each PSX start, up to the next start."""
    lo = int(cap['load_addr'], 16) if isinstance(cap['load_addr'], str) else cap['load_addr']
    d = base64.b64decode(cap['bytes_b64'])
    out = {}
    for s, e in zip(starts, starts[1:] + [lo + len(d)]):
        t = set()
        for x in range(max(s, lo), min(e, lo + len(d)) - 3, 4):
            w, = struct.unpack_from('<I', d, x - lo)
            if w >> 26 == 3:  # jal
                t.add(((x + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2))
        out[s] = t
    return out


def pc_callees(img, md, start, end):
    """Direct call targets of the PC code in [start, end)."""
    o = img.off(start)
    t = set()
    for ins in md.disasm(img.data[o:o + (end - start)], start):
        if ins.mnemonic == 'call' and ins.op_str.startswith('0x'):
            t.add(int(ins.op_str, 16))
    return t


def cmd_fill(a):
    """Pair every start inside an anchor gap whose two sides hold equal start
    counts, by position; then check the pairs against call edges."""
    import capstone
    img = pe_hidden.Image(a.exe)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    anchors = json.load(open(a.pairs))['pairs']
    caps = {c['source_md5']: c for c in json.load(open(a.sibling + 'analysis/overlay_captures_all.json'))}
    funcs = {f['entry'] for f in json.load(open(a.funcs))['functions']}
    hidden = {h['entry'] for h in json.load(open(a.hidden))}
    back = collections.defaultdict(set)
    for r in anchors:
        back[r['pc']].add((r['section'], r['psx']))
    pc_starts = sorted(funcs | hidden | set(back))

    pairs = {}      # (section, psx) -> (pc, how)
    shifted = {}    # the same gaps paired one position off: the baseline
    for r in anchors:
        pairs[(r['section'], r['psx'])] = (r['pc'], 'table')
    by = collections.defaultdict(set)
    for r in anchors:
        if len(back[r['pc']]) == 1:
            by[r['section']].add((r['psx'], r['pc']))
    filled_gaps = skipped = 0
    for s, v in by.items():
        if s not in caps or len(v) < 2:
            continue
        v = sorted(v)
        ps = psx_starts(caps[s], [p for p, _ in v])
        for (a1, b1), (a2, b2) in zip(v, v[1:]):
            if b2 <= b1:
                skipped += 1
                continue
            inner_psx = ps[bisect.bisect_right(ps, a1):bisect.bisect_left(ps, a2)]
            inner_pc = pc_starts[bisect.bisect_right(pc_starts, b1):bisect.bisect_left(pc_starts, b2)]
            if len(inner_psx) != len(inner_pc):
                skipped += 1
                continue
            filled_gaps += 1
            for i, (x, y) in enumerate(zip(inner_psx, inner_pc)):
                pairs.setdefault((s, x), (y, 'gap'))
                if len(inner_pc) > 1:
                    shifted[(s, x)] = inner_pc[(i + 1) % len(inner_pc)]

    # Call-edge check. A PSX edge f -> t, both paired in the same overlay, is
    # confirmed when PC(f) calls PC(t) directly. Boot EXE targets count through
    # symbols.toml's psx field.
    boot = {}
    for f in tomllib.load(open(a.symbols, 'rb'))['func']:
        if 'psx' in f:
            boot[f['psx']] = f['pc']
    nxt = {}
    for i, e in enumerate(pc_starts[:-1]):
        nxt[e] = pc_starts[i + 1]
    pc_calls = {}

    def calls(pc):
        if pc not in pc_calls:
            pc_calls[pc] = pc_callees(img, md, pc, nxt.get(pc, pc + 16))
        return pc_calls[pc]

    stats = collections.Counter()
    by_sec = collections.defaultdict(dict)
    for (s, x), (y, how) in pairs.items():
        by_sec[s][x] = (y, how)
    for s, m in by_sec.items():
        if s not in caps:
            continue
        ps = psx_starts(caps[s], m)
        cal = psx_callees(caps[s], ps)
        for x, (y, how) in m.items():
            for t in cal.get(x, ()):
                if t in m:
                    want, kind = m[t][0], 'overlay'
                elif t in boot:
                    want, kind = boot[t], 'boot'
                else:
                    continue
                stats[(how, kind, 'edges')] += 1
                stats[(how, kind, 'found')] += want in calls(y)
                if (s, x) in shifted:
                    stats[('shifted', kind, 'edges')] += 1
                    stats[('shifted', kind, 'found')] += want in calls(shifted[(s, x)])

    n_gap = sum(1 for _, how in pairs.values() if how == 'gap')
    print(f'{filled_gaps} gaps filled, {skipped} skipped (unequal counts or out of order); '
          f'{len(pairs)} pairs: {len(pairs) - n_gap} from the table, {n_gap} by position')
    for how in ('table', 'gap', 'shifted'):
        for kind in ('overlay', 'boot'):
            e, f = stats[(how, kind, 'edges')], stats[(how, kind, 'found')]
            if e:
                print(f'  {how:7s} pairs, {kind:7s} callee: {f:5d} of {e:5d} PSX calls found on the PC '
                      f'({100 * f / e:.0f}%)')
    json.dump([dict(section=s, psx=x, pc=y, how=how) for (s, x), (y, how) in sorted(pairs.items())],
              open(a.out or 'analysis/area_pairs_filled.json', 'w'), indent=0)


class PsxCode:
    """PSX code by address space: 'boot' or an overlay's section md5. A jal from
    an overlay lands in the overlay when the target is inside its image, else in
    the boot EXE (overlay bands can overlap the boot image's range)."""

    def __init__(self, sib, anchors_by_sec):
        self.boot = open(sib + 'disc/SLPS_009.90', 'rb').read()[0x800:]
        self.img = {'boot': (PSX_BOOT_LO, self.boot)}
        self.starts = {}
        for c in json.load(open(sib + 'analysis/overlay_captures_all.json')):
            lo = int(c['load_addr'], 16) if isinstance(c['load_addr'], str) else c['load_addr']
            self.img[c['source_md5']] = (lo, base64.b64decode(c['bytes_b64']))
            self.starts[c['source_md5']] = psx_starts(c, anchors_by_sec.get(c['source_md5'], ()))
        boot = set()
        for line in open(sib + 'analysis/functions.tsv'):
            if line.startswith('0x'):
                boot.add(int(line.split()[0], 16))
        for f in tomllib.load(open(sib + 'symbols.toml', 'rb'))['func']:
            boot.add(f['pc'])
        self.starts['boot'] = sorted(x for x in boot if PSX_BOOT_LO <= x < PSX_BOOT_LO + PSX_BOOT_TEXT)

    def space_of(self, space, target):
        lo, d = self.img[space]
        if space != 'boot' and lo <= target < lo + len(d):
            return space
        if PSX_BOOT_LO <= target < PSX_BOOT_LO + PSX_BOOT_TEXT:
            return 'boot'
        return None

    def calls(self, space, f):
        """Ordered jal targets of f, as (space, target), to the next known start."""
        lo, d = self.img[space]
        st = self.starts[space]
        i = bisect.bisect_right(st, f)
        end = min(st[i] if i < len(st) else lo + len(d), lo + len(d))
        out = []
        for x in range(f, end - 3, 4):
            if not lo <= x < lo + len(d) - 3:
                break
            w, = struct.unpack_from('<I', d, x - lo)
            if w >> 26 == 3:
                t = ((x + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
                sp = self.space_of(space, t)
                if sp:
                    out.append((sp, t))
            elif w == 0x03E00008 and x + 8 >= end:
                break
        return out


SIZE_TOLERANCE = math.log(2)  # a gap is refused if any pair in it is off the typical size ratio by more than 2x


def psx_size(code, sp, x):
    st = code.starts[sp]
    lo, d = code.img[sp]
    i = bisect.bisect_right(st, x)
    return (st[i] if i < len(st) else lo + len(d)) - x


def pc_size(pc_starts, y):
    i = bisect.bisect_right(pc_starts, y)
    return (pc_starts[i] if i < len(pc_starts) else y) - y


def size_ratio(pairs, code, pc_starts):
    """Median log(PC size / PSX size) over the area-table pairs - independent evidence."""
    r = [math.log(pc_size(pc_starts, y) / psx_size(code, sp, x))
         for (sp, x), (y, h) in pairs.items()
         if h == 'table' and sp in code.img and pc_size(pc_starts, y) > 0 and psx_size(code, sp, x) > 0]
    return statistics.median(r)


def gap_fill(pairs, code, pc_starts, tag, ratio=None):
    """Positional pairs inside every gap between neighbouring anchors (sorted by
    PSX address, one space at a time) whose PC order holds and whose start
    counts are equal on both sides. Anchors: undisputed pairs whose PC function
    has no other PSX twin. Returns how many pairs it added."""
    back = collections.defaultdict(set)
    for k, (y, _) in pairs.items():
        back[y].add(k)
    by = collections.defaultdict(list)
    for (sp, x), (y, h) in pairs.items():
        if h != 'call-disputed' and len(back[y]) == 1:
            by[sp].append((x, y))
    added = 0
    for sp, v in by.items():
        if sp not in code.starts:
            continue
        v.sort()
        ps = sorted(set(code.starts[sp]) | {x for x, _ in v})
        for (a1, b1), (a2, b2) in zip(v, v[1:]):
            if b2 <= b1:
                continue
            inner_psx = ps[bisect.bisect_right(ps, a1):bisect.bisect_left(ps, a2)]
            inner_pc = pc_starts[bisect.bisect_right(pc_starts, b1):bisect.bisect_left(pc_starts, b2)]
            if not inner_psx or len(inner_psx) != len(inner_pc):
                continue
            if ratio is not None and any(
                    psx_size(code, sp, x) <= 0 or pc_size(pc_starts, y) <= 0 or
                    abs(math.log(pc_size(pc_starts, y) / psx_size(code, sp, x)) - ratio) > SIZE_TOLERANCE
                    for x, y in zip(inner_psx, inner_pc)):
                continue
            for x, y in zip(inner_psx, inner_pc):
                if (sp, x) not in pairs:
                    pairs[(sp, x)] = (y, tag)
                    added += 1
    return added


def table_fill(pairs, code, pc_tabs, psx_tabs, tag):
    """Pair every slot of a PSX table (or window of one) with the PC table it
    agrees with: at least two slots already paired the same way, none
    contradicting, the window unique for that PC table, and no PSX pointer
    asked to take two PC values."""
    added = 0
    for t in pc_tabs:
        n = len(t['slots'])
        best = []
        for u in psx_tabs:
            m = len(u['slots'])
            for o in range(m - n + 1):
                agree = bad = 0
                want = {}
                for (k1, x), (k2, y) in zip(u['slots'][o:o + n], t['slots']):
                    if (k1 < 0) != (k2 < 0) and not (k1 == -2 or k2 == -2):
                        bad += 1
                        break
                    if k1 < 0 or k2 < 0:
                        continue
                    key = (code.space_of(u['space'], x), x)
                    if key in pairs:
                        if pairs[key][0] == y:
                            agree += 1
                        else:
                            bad += 1
                            break
                    elif want.setdefault(key, y) != y:
                        bad += 1
                        break
                if agree >= 2 and not bad:
                    best.append(want)
        if len(best) == 1:
            for key, y in best[0].items():
                if key[0] and key not in pairs:
                    pairs[key] = (y, tag)
                    added += 1
    return added


def caller_fill(pairs, code, pc_calls_of, pc_starts, tag):
    """An unpaired PSX function whose paired callees (two or more distinct PC
    functions) are all called by exactly one PC function pairs with it."""
    callers = collections.defaultdict(set)
    for y in pc_starts:
        for t in pc_calls_of(y):
            callers[t].add(y)
    added = 0
    for sp, st in code.starts.items():
        for f in st:
            if (sp, f) in pairs:
                continue
            want = {pairs[t][0] for t in code.calls(sp, f) if t in pairs and pairs[t][1] != 'call-disputed'}
            if len(want) < 2:
                continue
            cand = set.intersection(*(callers[y] for y in want))
            if len(cand) == 1:
                pairs[(sp, f)] = (cand.pop(), tag)
                added += 1
    return added


def cmd_propagate(a):
    """Grow the pairs along calls: when a paired PSX function's jal list and its
    PC twin's call list have equal length, pair them position by position. A
    target voted two different PC twins is a conflict and is not paired."""
    import capstone
    img = pe_hidden.Image(a.exe)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    seed = json.load(open(a.pairs))
    pairs = {(r['section'], r['psx']): (r['pc'], r['how']) for r in seed}
    for f in tomllib.load(open(a.symbols, 'rb'))['func']:
        if 'psx' in f and PSX_BOOT_LO <= f['psx'] < PSX_BOOT_LO + PSX_BOOT_TEXT:
            pairs.setdefault(('boot', f['psx']), (f['pc'], 'symbols'))
    by_sec = collections.defaultdict(set)
    for (s, x) in pairs:
        by_sec[s].add(x)
    code = PsxCode(a.sibling, by_sec)
    funcs = {f['entry'] for f in json.load(open(a.funcs))['functions']}
    hidden = {h['entry'] for h in json.load(open(a.hidden))}
    pc_starts = sorted(funcs | hidden | {y for y, _ in pairs.values()})

    @functools.lru_cache(maxsize=None)
    def pc_calls(y):
        i = bisect.bisect_right(pc_starts, y)
        end = pc_starts[i] if i < len(pc_starts) else y + 16
        o = img.off(y)
        out = []
        for ins in md.disasm(img.data[o:o + (end - y)], y):
            if ins.mnemonic == 'call' and ins.op_str.startswith('0x'):
                out.append(int(ins.op_str, 16))
        return tuple(out)

    ratio = size_ratio(pairs, code, pc_starts)
    print(f'typical size ratio PC/PSX, from the area-table pairs: x{math.exp(ratio):.2f}')
    lists = {}
    pc_tabs, psx_tabs = pc_tables(img), psx_tables(a.sibling)

    def aligned(key, y):
        """(psx targets, pc targets) of a pair when the lists have equal length."""
        if key not in lists:
            ps = code.calls(*key) if key[0] in code.img else []
            pcs = pc_calls(y)
            lists[key] = (ps, pcs) if ps and len(ps) == len(pcs) else None
        return lists[key]

    INDEPENDENT = ('table', 'gap', 'symbols')  # evidence that is not a call alignment
    conflicts = set()
    rnd = 0
    while True:
        rnd += 1
        votes = collections.defaultdict(lambda: collections.defaultdict(int))  # t -> u -> best support
        for key, (y, _) in list(pairs.items()):
            al = aligned(key, y)
            if not al:
                continue
            # Used only if no position contradicts a pair already made; support
            # is how many positions agree with one.
            known = [(t, u) for t, u in zip(*al) if t in pairs]
            if any(pairs[t][0] != u for t, u in known):
                continue
            support = len(known)
            for t, u in zip(*al):
                if t not in pairs:
                    votes[t][u] = max(votes[t][u], support)
        new = 0
        for t, v in votes.items():
            if len(v) > 1:
                conflicts.add(t)
            if t in conflicts:
                continue
            (u, support), = v.items()
            pairs[t] = (u, 'call-anchored' if support else 'call')
            new += 1
        filled = tabled = called = 0
        if not new:
            filled = gap_fill(pairs, code, pc_starts, f'gap{rnd}', ratio)
        if not new and not filled:
            tabled = table_fill(pairs, code, pc_tabs, psx_tabs, 'table-anchored')
        if not new and not filled and not tabled:
            called = caller_fill(pairs, code, pc_calls, pc_starts, 'callers')
        lists.clear()
        print(f'round {rnd}: {new} by calls, {filled} by position, {tabled} by tables, {called} by callers')
        if not (new or filled or tabled or called):
            break

    # Precision against pairs from independent evidence: in every usable
    # alignment, each position whose PSX target has such a pair, judged with
    # that one position left out of the contradiction test.
    prec = collections.Counter()
    for key, (y, _) in pairs.items():
        al = aligned(key, y)
        if not al:
            continue
        for i, (t, u) in enumerate(zip(*al)):
            if t not in pairs or pairs[t][1] not in INDEPENDENT:
                continue
            others = [(t2, u2) for j, (t2, u2) in enumerate(zip(*al)) if j != i and t2 in pairs]
            if any(pairs[t2][0] != u2 for t2, u2 in others):
                continue
            tier = 'anchored' if others else 'unanchored'
            prec[(tier, pairs[t][0] == u)] += 1
    # Internal consistency: alignments that contradict the final pairs, and the
    # propagated pairs they touch - those are demoted to 'call-disputed'.
    disputed, bad = set(), 0
    for key, (y, _) in pairs.items():
        al = aligned(key, y)
        if not al:
            continue
        wrong = [t for t, u in zip(*al) if t in pairs and pairs[t][0] != u]
        if wrong:
            bad += 1
            disputed.update(t for t in wrong + [key] if pairs[t][1].startswith('call'))
    for t in disputed:
        pairs[t] = (pairs[t][0], 'call-disputed')
    print(f'{bad} usable alignments contradict the final pairs; {len(disputed)} propagated pairs disputed')
    how = collections.Counter(h for _, h in pairs.values())
    boot_pairs = sum(1 for (s, _) in pairs if s == 'boot')
    back = collections.defaultdict(set)
    for k, (y, _) in pairs.items():
        back[y].add(k)
    print(f'{len(pairs)} pairs {dict(how)}; {boot_pairs} of them boot EXE functions; '
          f'{len(conflicts)} targets with conflicting votes left unpaired')
    for tier in ('anchored', 'unanchored'):
        print(f'precision, {tier} alignments, against independent pairs: '
              f'{prec[(tier, True)]} right, {prec[(tier, False)]} wrong')
    print(f'PC functions with more than one PSX twin: {sum(1 for v in back.values() if len(v) > 1)}')
    json.dump([dict(section=s, psx=x, pc=y, how=h) for (s, x), (y, h) in sorted(pairs.items())],
              open(a.out or 'analysis/pairs_propagated.json', 'w'), indent=0)


def shape(slots):
    m, out = {}, []
    for kind, v in slots:
        out.append(kind if kind < 0 else m.setdefault(v, len(m)))
    return tuple(out)


def pc_tables(img):
    out = []
    for s in img.secs:
        if s['name'] == '.text':
            continue
        blob = img.data[s['raw']:s['raw'] + s['rsz']]
        i = 0
        while i + 4 <= len(blob):
            run, j = [], i
            while j + 4 <= len(blob):
                x, = struct.unpack_from('<I', blob, j)
                if not (img.in_text(x) or (x == 0 and run)):
                    break
                run.append(x)
                j += 4
            while run and run[-1] == 0:
                run.pop()
            if sum(1 for x in run if x) >= 3:
                slots = [(-1, 0) if x == 0 else (-2, 0) if img.data[img.off(x)] == 0xC3 else (0, x)
                         for x in run]
                out.append(dict(va=s['va'] + i, slots=slots))
                i = j
            else:
                i += 4
    return out


def psx_tables(sib):
    boot = open(sib + 'disc/SLPS_009.90', 'rb').read()[0x800:]
    images = [('BOOT', PSX_BOOT_LO, boot, 'boot')]
    for c in json.load(open(sib + 'analysis/overlay_captures_all.json')):
        images.append((f"{c['source_file']}#{c['source_index']}", int(c['load_addr'], 16)
                       if isinstance(c['load_addr'], str) else c['load_addr'],
                       base64.b64decode(c['bytes_b64']), c['source_md5']))
    out = []
    for name, lo, d, space in images:
        def word(x):
            for base, blob in ((lo, d), (PSX_BOOT_LO, boot)):
                if base <= x < base + len(blob) - 7:
                    return struct.unpack_from('<II', blob, x - base)
            return None

        def code(x):
            return not x & 3 and (lo <= x < lo + len(d) or PSX_BOOT_LO <= x < PSX_BOOT_LO + PSX_BOOT_TEXT)

        i = 0
        while i + 4 <= len(d):
            run, j = [], i
            while j + 4 <= len(d):
                x, = struct.unpack_from('<I', d, j)
                if not (code(x) or (x == 0 and run)):
                    break
                run.append(x)
                j += 4
            while run and run[-1] == 0:
                run.pop()
            if sum(1 for x in run if x) >= 3:
                slots = [(-1, 0) if x == 0 else (-2, 0) if word(x) == (0x03E00008, 0) else (0, x) for x in run]
                out.append(dict(img=name, space=space, va=lo + i, slots=slots))
                i = j
            else:
                i += 4
    return out


def cmd_tables(a):
    img = pe_hidden.Image(a.exe)
    pcs, pss = pc_tables(img), psx_tables(a.sibling)
    rows = []
    for t in pcs:
        sig, n = shape(t['slots']), len(t['slots'])
        hits = [(u['img'], u['va'] + 4 * o, len(u['slots']))
                for u in pss for o in range(len(u['slots']) - n + 1)
                if shape(u['slots'][o:o + n]) == sig]
        info = sum(1 for k in range(n) if sig[k] < 0 or sig[k] in sig[:k])  # null, ret or repeat
        rows.append(dict(pc=t['va'], n=n, info=info, hits=hits))
    one = [r for r in rows if len(r['hits']) == 1 and r['info'] >= 2]
    print(f'{len(pcs)} PC tables, {len(pss)} PSX tables; {len(one)} PC tables with exactly one '
          f'PSX twin and at least two null / return / repeated slots:')
    for r in sorted(one, key=lambda r: -r['info']):
        img_, va, m = r['hits'][0]
        print(f"  0x{r['pc']:06X} ({r['n']}, {r['info']}) <-> {img_} 0x{va:08X} (run of {m})")
    json.dump(rows, open(a.out or 'analysis/table_matches.json', 'w'), indent=0)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--exe', default='bof3/BOF3.exe')
    ap.add_argument('--funcs', default='analysis/pc_funcs.json')
    ap.add_argument('--hidden', default='analysis/pc_hidden.json')
    ap.add_argument('--sibling', default='../BreathOfFire3Recomp/')
    ap.add_argument('--out')
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('areas').set_defaults(fn=cmd_areas)
    p = sub.add_parser('fill')
    p.add_argument('--pairs', default='analysis/area_pairs.json')
    p.add_argument('--symbols', default='symbols.toml')
    p.set_defaults(fn=cmd_fill)
    p = sub.add_parser('propagate')
    p.add_argument('--pairs', default='analysis/area_pairs_filled.json')
    p.add_argument('--symbols', default='symbols.toml')
    p.set_defaults(fn=cmd_propagate)
    sub.add_parser('tables').set_defaults(fn=cmd_tables)
    a = ap.parse_args()
    a.fn(a)


if __name__ == '__main__':
    main()
