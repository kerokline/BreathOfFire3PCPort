#!/usr/bin/env python
"""The boss tool: every function of the boss band, found from the engine's
three root sets and cut into units for the boss round (docs/takeover-queue-bosses.md,
docs/boss-rows.md).

On the PlayStation each BOSS overlay was a program of its own, entered once
by the battle engine through a 56-entry table indexed by the boss id (the
sibling's `docs/loader_records/BOSS.md`). The PC linked the 35 distinct
images into one band of `.text`, `0x437A00..0x441000`, and compiled the
enemies' scripts once each behind a second table. The engine reaches the
band through:

  1. `Boss_SetupTable` 0x656954, 56 entries by the event-battle byte
     0x904AAA: `Battle_InitBossEncounter` 0x4942A0 tail-jumps through it
     after `0x494500`. Each entry installs the fight's three hooks at
     0x904B64 / 0x904B68 / 0x904B6C (the per-frame script, the transition,
     the event hook the phases call with 0..6) and returns.
  2. `BossKind_Table` 0x64B088, 63 entries by an enemy's kind byte:
     `EnemyRunAll`'s event-battle path (battle_flow.md) runs the kind's
     per-frame dispatcher, which switches on the enemy's state bytes
     through the kind's own tables in `.data` (0x64C7B0..0x64DDEC; each
     enemy's +0xF4 hook and +0xF8 / +0xFC tables are stored by that code).
  3. The effect dispatchers' stack tables: `BattleFx_Dispatch` 0x4352A0
     slot 8, `BattleMagicFx_Dispatch` 0x435350 slot 93 (Head Cracker's
     rock, ours), and the eight-slot dispatcher at 0x4357D0 (slots 2..7):
     boss-specific effect tasks.

A unit is one root's closure (B<id>: a boss set-up; K<kind>: an enemy
kind's script; F<n>: an effect task), stopping at the helpers three or more
units share (the H group, taken first as the spell round's L was). Data
tables are read from the address the code names to the next address any
code or `symbols.toml` names, skipping a flag header (the sibling's
`Boss021_HandlerTable` shape: bytes, FF padding, then pointers). Groups
are whole units in address order, about --group-size functions not yet
ours each; a function no root reaches is a gap and goes with the group
whose span holds it (as `magic_rows.py`'s unreached went with their
overlays).

Writes analysis/boss_rows.tsv (a unit a line) and analysis/boss_funcs.tsv
(a function a line); both game-derived and gitignored (CLAUDE.md rule 1).
Addresses are load-bearing constants (rule 3).
"""

import argparse
import bisect
import collections
import csv
import json
import os
import sys
import tomllib

from capstone import x86

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import magic_rows as mr  # noqa: E402
import area_rows as ar   # noqa: E402  Descent / discover, re-banded below

BAND_LO = 0x437A00              # kind 6's dispatcher, the band's first function
BAND_HI = 0x441000              # 0x440EF0 (330 bytes) is the last; Field_StartEventBattle 0x4410B0 follows
SETUP_TABLE, SETUP_COUNT = 0x656954, 56       # jmp [eax*4 + 0x656954] at 0x4942AC, eax = byte 0x904AAA
KIND_TABLE = 0x64B088                          # EnemyRunAll's event-battle table (battle_flow.md); [0] is null
KIND_MAX = 64
HOOKS = (0x904B64, 0x904B68, 0x904B6C)
FX_DISPATCHERS = ((0x4352A0, 0x435350, 'BattleFx_Dispatch'),
                  (0x435350, 0x4357D0, 'BattleMagicFx_Dispatch'),
                  (0x4357D0, 0x435830, 'Fx8_Dispatch'))
START_EVENT_BATTLE = 0x4410B0   # Field_StartEventBattle(id): the chapters' call sites give id -> chapter
DATA_LO = 0x5B0000
SHARED_MIN = 3                  # units a helper must be reached by to count as shared


def load_toml(path):
    with open(path, 'rb') as fh:
        return tomllib.load(fh)


def stack_slots(img, lo, hi):
    """(slot, value) for every `mov dword [esp + k], imm32` with a .text value in [lo, hi)."""
    out, pc = [], lo
    while pc < hi:
        ins = mr.decode(img, pc)
        if ins is None:
            pc += 1
            continue
        ops = ins.operands
        if ins.mnemonic == 'mov' and len(ops) == 2 and ops[0].type == x86.X86_OP_MEM \
                and ops[0].mem.base == x86.X86_REG_ESP and ops[1].type == x86.X86_OP_IMM:
            v = ops[1].imm & 0xFFFFFFFF
            if img.in_text(v):
                out.append((ops[0].mem.disp // 4, v))
        pc += ins.size
    return out


def event_battle_sites(img):
    """Every `push imm8/imm32; call Field_StartEventBattle` in .text: (site, id or None)."""
    text = next(s for s in img.secs if s[0] == '.text')
    pc, prev, out = text[1], None, []
    while pc < text[1] + text[2]:
        ins = mr.decode(img, pc)
        if ins is None:
            pc += 1
            continue
        if ins.mnemonic == 'call' and ins.operands[0].type == x86.X86_OP_IMM \
                and (ins.operands[0].imm & 0xFFFFFFFF) == START_EVENT_BATTLE:
            v = None
            if prev is not None and prev.mnemonic == 'push' and prev.operands[0].type == x86.X86_OP_IMM:
                v = prev.operands[0].imm & 0xFF
            out.append((pc, v))
        prev = ins
        pc += ins.size
    return out


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--exe', default=os.path.join(repo, 'bof3', 'BOF3.exe'))
    ap.add_argument('--analysis', default=os.path.join(repo, 'analysis'))
    ap.add_argument('--sibling', default=os.path.join(os.path.dirname(repo), 'BreathOfFire3Recomp'))
    ap.add_argument('--symbols', default=os.path.join(repo, 'symbols.toml'))
    ap.add_argument('--unit', help='print one unit (B12, K07, F2, H) function by function')
    ap.add_argument('--clones', action='store_true',
                    help="with --unit: print the unit's functions not yet ours as clone tables (C++)")
    ap.add_argument('--groups', action='store_true', help='print the proposed groups')
    ap.add_argument('--group-size', type=int, default=50, help='functions not yet ours a group aims at (default 50)')
    ap.add_argument('--quiet', action='store_true', help='write the TSVs, print only the totals')
    ap.add_argument('--no-write', action='store_true',
                    help='do not write boss_rows.tsv / boss_funcs.tsv into --analysis (a read-only run: the canonical '
                         'cut is boss_funcs_0928_4554.tsv, and a run at a later symbols.toml recuts the group column)')
    ap.add_argument('--disc', help="the US disc's .cue: with it the report names every kind from the areas' enemy records")
    a = ap.parse_args()

    ar.BAND_LO, ar.BAND_HI = BAND_LO, BAND_HI
    img = mr.Image(a.exe)
    sym = load_toml(a.symbols)
    ours = {f['pc']: f['name'] for f in sym.get('func', []) if 'impl' in f}
    named = {f['pc']: f['name'] for f in sym.get('func', [])}
    data_named = {d['pc']: d['name'] for d in sym.get('data', [])}

    pf = json.load(open(os.path.join(a.analysis, 'pc_funcs.json')))
    recorded = {f['entry'] for f in pf['functions']}
    hidden = {h['entry'] for h in json.load(open(os.path.join(a.analysis, 'pc_hidden.json')))}
    band = sorted(s for s in recorded | hidden if BAND_LO <= s < BAND_HI)
    label, combat = {}, {}
    cat = os.path.join(a.analysis, 'remaining_catalog.tsv')
    if os.path.exists(cat):
        with open(cat, encoding='utf-8') as fh:
            for row in csv.DictReader(fh, delimiter='\t'):
                label[int(row['entry'], 16)] = row['label']
                combat[int(row['entry'], 16)] = row.get('combat', '')
    id_file, id_entry_psx = {}, {}
    sib = os.path.join(a.sibling, 'names', 'boss_records.toml')
    if os.path.exists(sib):
        recs = load_toml(sib)
        recs = recs.get('boss') or recs.get('record') or next(iter(recs.values()))
        for r in recs:
            id_file[r['boss']] = os.path.basename(r['file']).replace('.EMI', '')
            id_entry_psx[r['boss']] = r.get('entry')

    # ---- the roots ---------------------------------------------------------
    setup = [img.u32(SETUP_TABLE + 4 * i) for i in range(SETUP_COUNT)]
    kinds = []
    for k in range(KIND_MAX):
        v = img.u32(KIND_TABLE + 4 * k)
        if k and not (v and img.in_text(v)):
            break
        kinds.append(v)
    fx_roots = []       # (name, slot, target)
    for lo, hi, name in FX_DISPATCHERS:
        for slot, v in stack_slots(img, lo, hi):
            if BAND_LO <= v < BAND_HI and v != setup[0]:
                fx_roots.append((name, slot, v))
    sites = event_battle_sites(img)

    # ---- discovery ----------------------------------------------------------
    extra = set(setup) | {k for k in kinds if BAND_LO <= k < BAND_HI} | {v for _, _, v in fx_roots}
    funcs, dropped, found = ar.discover(img, band, extra=extra)
    starts = sorted(funcs)

    # data tables: bounded by the next address any code names or symbols.toml names
    tstarts = set(data_named)
    for d in funcs.values():
        for t in d.tables | d.drefs:
            if t >= DATA_LO:
                tstarts.add(t)
    tsorted = sorted(tstarts)

    def run_from(t, limit=256):
        i = bisect.bisect_right(tsorted, t)
        nxt = tsorted[i] if i < len(tsorted) else t + 0x1000
        p = t
        while p < min(t + 0x40, nxt) and not (img.u32(p) and img.in_text(img.u32(p))):
            p += 1                                   # the flag header
        out = []
        while p + 4 <= nxt and len(out) < limit:
            w = img.u32(p)
            if w is None or not img.in_text(w):
                break
            out.append(w)
            p += 4
        return out

    refs, dtabs = {}, {}
    for f, d in funcs.items():
        out, dt = set(d.calls | d.tails | d.shared | d.imms), {}
        for t in d.tables | d.drefs:
            if t >= DATA_LO:
                r = run_from(t)
                if r:
                    dt[t] = r
                    out.update(r)
        refs[f], dtabs[f] = out, dt

    def closure(root, stop):
        seen, frontier, work = set(), set(), [root]
        while work:
            f = work.pop()
            if f in seen:
                continue
            if f not in funcs or (f in stop and f != root):
                if img.in_text(f):
                    frontier.add(f)
                continue
            seen.add(f)
            for t in refs[f]:
                if BAND_LO <= t < BAND_HI:
                    work.append(t)
                elif img.in_text(t):
                    frontier.add(t)
        return seen, frontier

    units = collections.OrderedDict()      # name -> (root, how)
    for i in range(1, SETUP_COUNT):
        units['B%02d' % i] = (setup[i], 'Boss_SetupTable[%d]' % i)
    for k, v in enumerate(kinds):
        if BAND_LO <= v < BAND_HI:
            units['K%02d' % k] = (v, 'BossKind_Table[%d]' % k)
    for name, slot, v in fx_roots:
        units['F%d' % slot if name == 'Fx8_Dispatch' else 'F%s%d' % (name[0], slot)] = (v, '%s slot %d' % (name, slot))

    def walk(stop):
        cl, fr, own = {}, {}, collections.defaultdict(set)
        for u, (root, _) in units.items():
            cl[u], fr[u] = closure(root, stop)
            for f in cl[u]:
                own[f].add(u)
        return cl, fr, own

    shared = set()
    while True:
        cl, fr, own = walk(shared)
        more = {f for f, o in own.items() if len(o) >= SHARED_MIN} - shared
        if not more:
            break
        shared |= more
    gaps = sorted(f for f in funcs if f not in own and f not in shared)

    # ---- units in address order, then groups ------------------------------
    rows = []
    for u, (root, how) in units.items():
        c = cl[u]
        if not c:
            continue
        lo, hi = min(c), max(funcs[f].end for f in c)
        rows.append(dict(unit=u, root=root, how=how, lo=lo, hi=hi, funcs=sorted(c),
                         excl=sorted(f for f in c if own[f] == {u}),
                         take=sorted(f for f in c if f not in ours),
                         bytes=sum(funcs[f].end - f for f in c),
                         tables=sorted({t for f in c for t in dtabs[f]}),
                         frontier=sorted(fr[u])))
    rows.sort(key=lambda r: r['lo'])
    groups = []
    if shared:
        groups.append(dict(name='BH', units=['H'], funcs=sorted(shared), lo=min(shared),
                           hi=max(funcs[f].end for f in shared)))
    cur, n = [], 0
    for r in rows:
        t = len(r['take'])
        if cur and n + t > a.group_size * 1.1:
            groups.append(cur)
            cur, n = [], 0
        cur.append(r)
        n += t
    if cur:
        groups.append(cur)
    cut, placed = [], set()
    for gi, g in enumerate(groups):
        if isinstance(g, dict):
            cut.append(g)
            placed |= set(g['funcs'])
            continue
        fs = sorted(set().union(*(r['funcs'] for r in g)) - placed)     # a two-unit body goes with its first group
        placed |= set(fs)
        cut.append(dict(name='BS%d' % gi if gi else 'BS1', units=[r['unit'] for r in g], funcs=fs,
                        lo=min(r['lo'] for r in g), hi=max(r['hi'] for r in g)))
    # letters instead of numbers, BH first
    k = 0
    for g in cut:
        if g['name'] != 'BH':
            g['name'] = 'BS%s' % chr(ord('A') + k)
            k += 1
    # gaps go with the group whose span holds them, else the nearest before
    spans = [(g['lo'], g['hi'], g) for g in cut if g['name'] != 'BH']
    gap_of = {}
    for f in gaps:
        hit = [g for lo, hi, g in spans if lo <= f < hi]
        if not hit:
            before = [g for lo, hi, g in spans if hi <= f]
            hit = [before[-1]] if before else [spans[0][2]]
        g = hit[0]
        g['funcs'].append(f)
        gap_of[f] = g['name']
    for g in cut:
        g['funcs'].sort()
        g['take'] = [f for f in g['funcs'] if f not in ours]
        g['bytes'] = sum(funcs[f].end - f for f in g['take'])
        g['lo'], g['hi'] = min(g['funcs']), max(funcs[f].end for f in g['funcs'])
    group_of = {}
    for g in cut:
        for f in g['funcs']:
            group_of.setdefault(f, g['name'])

    # ---- the TSVs -----------------------------------------------------------
    # --no-write (round eleven's cleanup, takeover-queue-round11.md section 4):
    # the TSVs are recut from the current symbols.toml, so a run once the
    # functions are ours overwrites the canonical cut; the read-only run
    # sends them to os.devnull, as area_rows.py's does.
    rows_tsv = os.path.join(a.analysis, 'boss_rows.tsv')
    funcs_tsv = os.path.join(a.analysis, 'boss_funcs.tsv')
    if not a.no_write:
        os.makedirs(a.analysis, exist_ok=True)
    with open(rows_tsv if not a.no_write else os.devnull, 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh, delimiter='\t')
        w.writerow(['unit', 'root', 'how', 'lo', 'hi', 'fns', 'excl', 'take', 'bytes', 'tables', 'frontier', 'file', 'group', 'funcs'])
        for r in rows:
            i = int(r['unit'][1:]) if r['unit'][0] == 'B' else None
            w.writerow([r['unit'], '%#x' % r['root'], r['how'], '%#x' % r['lo'], '%#x' % r['hi'], len(r['funcs']),
                        len(r['excl']), len(r['take']), r['bytes'], ' '.join('%#x' % t for t in r['tables']),
                        ' '.join('%#x' % t for t in r['frontier']), id_file.get(i, ''),
                        group_of.get(r['root'], ''), ' '.join('%#x' % f for f in r['funcs'])])
    with open(funcs_tsv if not a.no_write else os.devnull, 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh, delimiter='\t')
        w.writerow(['start', 'size', 'units', 'kind', 'group', 'ours', 'name', 'label', 'combat', 'tables'])
        for f in starts:
            kind = 'shared' if f in shared else ('gap' if f in gap_of else ('exclusive' if len(own[f]) == 1 else 'two-units'))
            w.writerow(['%#x' % f, funcs[f].end - f, ' '.join(sorted(own[f])), kind, group_of.get(f, ''),
                        'ours' if f in ours else '', named.get(f, ''), label.get(f, ''), combat.get(f, ''),
                        ' '.join('%#x' % t for t in sorted(dtabs[f]))])

    # ---- one unit ---------------------------------------------------------------
    if a.unit:
        u = a.unit.upper()
        if u == 'H':
            fs, title = sorted(shared), 'H: the helpers %d or more units share' % SHARED_MIN
        else:
            r = next((r for r in rows if r['unit'] == u), None)
            if r is None:
                sys.exit('no unit %s' % u)
            fs, title = r['funcs'], '%s: root %#x, %s, %#x..%#x' % (u, r['root'], r['how'], r['lo'], r['hi'])
        if not a.clones:
            print(title)
            for f in fs:
                print('  %#x %5d %-32s %s %s %s' % (f, funcs[f].end - f, named.get(f, ''), 'OURS' if f in ours else '',
                                                    ' '.join(sorted(own[f])), ' '.join('%#x' % t for t in sorted(dtabs[f]))))
            if u != 'H':
                print('  tables:', ' '.join('%#x%s' % (t, ' ' + data_named[t] if t in data_named else '') for t in r['tables']))
                print('  frontier:', ' '.join('%#x%s' % (t, ':' + named[t] if t in named else '') for t in r['frontier']))
            return
        print_clones(img, [f for f in fs if f not in ours], funcs, named, own, units, dtabs)
        return

    # ---- the report -----------------------------------------------------------
    n_ours = sum(1 for f in funcs if f in ours)
    print('band %#x..%#x: %d functions (%d listed, %d dropped, %d found), %d ours, %d to take (%d bytes)' % (
        BAND_LO, BAND_HI, len(funcs), len(band), len(dropped), len(found), n_ours,
        len(funcs) - n_ours, sum(funcs[f].end - f for f in funcs if f not in ours)))
    print('roots: Boss_SetupTable %d entries (%d distinct, [0] the bare ret %#x); BossKind_Table %d entries (%d in the band); effect slots %d' % (
        SETUP_COUNT, len(set(setup)), setup[0], len(kinds), sum(1 for k in kinds if BAND_LO <= k < BAND_HI), len(fx_roots)))
    for name, slot, v in fx_roots:
        print('  %s slot %d -> %#x %s' % (name, slot, v, named.get(v, '')))
    reached = set(own) | shared
    print('reached: %d (%d by one unit, %d by two, %d shared by %d+); gaps %d (%d ours): %s' % (
        len(reached), sum(1 for f in own if len(own[f]) == 1), sum(1 for f in own if len(own[f]) == 2),
        len(shared), SHARED_MIN, len(gaps), sum(1 for f in gaps if f in ours),
        ' '.join('%#x' % f for f in gaps if f not in ours)))
    print('shared helpers:', ' '.join('%#x(%d)' % (f, funcs[f].end - f) for f in sorted(shared)))
    twos = [(f, sorted(own[f])) for f in sorted(own) if len(own[f]) == 2]
    print('in exactly two units:', ' '.join('%#x[%s]' % (f, ','.join(o)) for f, o in twos))
    ov = [(x['unit'], y['unit']) for x, y in zip(rows, rows[1:]) if y['lo'] < x['hi']]
    print('units %d; consecutive spans overlapping: %d %s' % (len(rows), len(ov), ov))
    print('ids in address order:', ' '.join(r['unit'][1:] for r in rows if r['unit'][0] == 'B'))
    fr_all = set().union(*(r['frontier'] for r in rows))
    print('frontier %d: %d ours; named not ours %s; unnamed %s' % (
        len(fr_all), sum(1 for t in fr_all if t in ours),
        ' '.join('%#x:%s' % (t, named[t]) for t in sorted(fr_all) if t in named and t not in ours),
        ' '.join('%#x' % t for t in sorted(fr_all) if t not in named)))
    if a.no_write:
        print('\n(--no-write: %s, %s not written)' % (rows_tsv, funcs_tsv))
    if not a.quiet:
        print('\nunit   root      how                      span               fns excl take bytes tables file')
        for r in rows:
            i = int(r['unit'][1:]) if r['unit'][0] == 'B' else None
            print('  %-5s %#x %-24s %#x..%#x %3d %3d %3d %5d %2d %s' % (
                r['unit'], r['root'], r['how'], r['lo'], r['hi'], len(r['funcs']), len(r['excl']), len(r['take']),
                r['bytes'], len(r['tables']), id_file.get(i, '')))
        by_chapter = collections.defaultdict(list)
        for pc, v in sites:
            by_chapter['%#x' % (pc & ~0xFFF)].append(v)
        print('\nField_StartEventBattle sites: %d; ids by 4 KiB page of the caller: %s' % (
            len(sites), '; '.join('%s: %s' % (p, ','.join('?' if v is None else str(v) for v in vs)) for p, vs in sorted(by_chapter.items()))))
    if a.disc:
        print_names(a.disc, img, rows, cut, id_file)
    if a.groups:
        print('\n| Group | Units | Band | Fns | Ours | To take | Bytes to take |')
        print('|---|---|---|--:|--:|--:|--:|')
        for g in cut:
            print('| %s | %s | `%#x..%#x` | %d | %d | %d | %d |' % (
                g['name'], _unit_ranges(g['units']), g['lo'], g['hi'], len(g['funcs']),
                len(g['funcs']) - len(g['take']), len(g['take']), g['bytes']))
        print('\n%d groups, %d functions to take' % (len(cut), sum(len(g['take']) for g in cut)))


ENEMY_DEST, ENEMY_HEAD, ENEMY_COUNT = 0x800E4000, 0x48, 8     # the area's enemy records (DIV-0053)
DONOR_STRIDE, DONOR_NAME, DONOR_KIND = 0x88, 8, 0x84           # US EMI: 8-byte name; the PC's stride is 0x8C, name 12, kind +0x88
RECORDS = 0x64DDEC                                             # EventBattle_Records: +2 the formation row


def print_names(cue, img, rows, cut, id_file):
    """Which enemy each kind is. The working record's state byte +0x100, the
    index EnemyRunAll dispatches BossKind_Table by, is copied by
    Battle_CopyEnemyData 0x4946C0 from the enemy data record's +0x88
    (`mov cl, [edx + 0x8C5650]; mov [eax + 0x93BA60], cl`, 0x494883), and
    the enemy data is the area's (0x800E4000 in every AREAnnn.EMI). So the
    US disc's area files name every kind in English; a set-up's fight is the
    area whose formation row (EventBattle_Records[id] +2) carries the kinds
    that follow the set-up in address order."""
    import psx_disc     # noqa  the sibling-shaped disc reader in tools/
    import loc_build    # noqa  emi_sections
    disc = psx_disc.Disc(cue)
    kind_names, kind_areas, row_kinds = collections.defaultdict(set), collections.defaultdict(set), {}
    for area in range(200):
        world = min(area // 38, 4)
        try:
            blob = disc.read('BIN/WORLD%02d/AREA%03d.EMI' % (world, area))
        except Exception:
            continue
        sec = [s for d, s in loc_build.emi_sections(blob) if d == ENEMY_DEST]
        if not sec:
            continue
        p = sec[0]
        recs = []
        for k in range(ENEMY_COUNT):
            r = p[ENEMY_HEAD + DONOR_STRIDE * k:ENEMY_HEAD + DONOR_STRIDE * (k + 1)]
            recs.append((r[:DONOR_NAME].split(bytes([0]))[0].decode('latin1').replace(chr(255), ' '), r[DONOR_KIND]))
        for nm, k in recs:
            if k:
                kind_names[k].add(nm)
                kind_areas[k].add(area)
        for rw in range(4, 8):
            ks = {recs[sl][1] for sl in p[9 * rw:9 * rw + 8] if sl != 0xFF and recs[sl][1]}
            if ks:
                row_kinds[(area, rw)] = ks
    print('')
    print("kinds by the areas' enemy records (US disc):")
    for k in sorted(kind_names):
        print('  K%02d %-28s areas %s' % (k, ', '.join(sorted(kind_names[k])), ' '.join(map(str, sorted(kind_areas[k])))))
    print('')
    print("set-ups: the record's row, the kinds before it in address order (since the previous set-up), and the areas whose row carries them:")
    order = sorted((r for r in rows if r['unit'][0] in 'BK'), key=lambda r: r['root'])   # by root: a shared body can lie far below
    for i, r in enumerate(order):
        if r['unit'][0] != 'B':
            continue
        bid = int(r['unit'][1:])
        rw = img.u8(RECORDS + 4 * bid + 2)
        ks = []      # a file's kinds precede its set-ups: the kinds since the last set-up of another file
        for prv in reversed(order[:i]):
            if prv['unit'][0] == 'B':
                if id_file.get(int(prv['unit'][1:])) != id_file.get(bid):
                    break
                continue
            ks.append(int(prv['unit'][1:]))
        ks.reverse()
        areas = sorted(a for (a, w), kk in row_kinds.items() if w == rw and kk & set(ks))
        names = sorted({n for k in ks for n in kind_names.get(k, ())})
        print('  B%02d %-8s row %d kinds %-18s %-34s areas %s' % (bid, id_file.get(bid, ''), rw, ' '.join('K%02d' % k for k in ks) or '-', ', '.join(names) or '-', ' '.join(map(str, areas)) or '-'))


def _unit_ranges(us):
    return ', '.join(us) if len(us) <= 8 else '%s .. %s (%d units)' % (us[0], us[-1], len(us))


def print_clones(img, addrs, funcs, named, own, units, dtabs):
    """C++ for a unit's clone table on magic_harness's API (the boss harness is
    to be written from it; the namespace is a placeholder until it exists).
    The comment line says which root reaches each function and which .data
    tables its code names - the harness must swap those for recorders."""
    lines = []
    for x in addrs:
        end = funcs[x].end
        calls, imms, tables, refused = mr.clone_sites(img, x, end)
        tag = '%X' % x
        name = named.get(x, 'Fn_' + tag)
        roots = '; '.join('%s (%s)' % (u, units[u][1]) for u in sorted(own.get(x, ()))) or 'a gap: reached by no root'
        dt = ' '.join('%#x' % t for t in sorted(dtabs[x]))
        codes = sorted(v for v in funcs[x].imms if v not in {c[1] for c in calls})
        print('// 0x%X: 0x%X bytes; %s%s%s%s' % (x, end - x, roots, ('; data tables ' + dt) if dt else '', ''.join(
            ('; +0x%X %s' if r[1].startswith('note') else '; REFUSED +0x%X %s') % r for r in refused),
            ('; note: code immediates %s (stored or pushed, not re-aimed)' % ' '.join('%#x' % v for v in codes)) if codes else ''))
        if calls:
            print('constexpr boss_harness::CallSite kCalls%s[] = {%s};' % (tag, ', '.join('{0x%X, 0x%X}' % c for c in calls)))
        if imms:
            print('constexpr boss_harness::Imm kImms%s[] = {%s};' % (tag, ', '.join('{0x%X, 0x%X}' % i for i in imms)))
        if tables:
            print('constexpr boss_harness::JumpTable kTables%s[] = {%s};' % (tag, ', '.join('{0x%X, 0x%X, %d}' % t for t in tables)))
        cell = lambda v, k: ('k%s%s, BH_N(k%s%s)' % (k, tag, k, tag)) if v else 'nullptr, 0'
        lines.append('    {"%s", 0x%X, 0x%X, %s, %s, %s, reinterpret_cast<const void*>(&::%s)},' % (
            name, x, end - x, cell(calls, 'Calls'), cell(imms, 'Imms'), cell(tables, 'Tables'), name))
    print('#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])')
    print('const boss_harness::Clone kClones[] = {')
    for line in lines:
        print(line)
    print('};')


if __name__ == '__main__':
    main()
