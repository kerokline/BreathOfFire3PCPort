#!/usr/bin/env python
"""Catalogue every function of BOF3.exe that is not ours, grouped by what it is
part of, so the next takeover waves have a shape.

    python tools/remaining_catalog.py            -> analysis/remaining_catalog.md
                                                    analysis/remaining_catalog.tsv

The universe is every recorded function start (analysis/pc_funcs.json) plus
every pointer-reached start pe_hidden.py found (analysis/pc_hidden.json), less
the functions symbols.toml gives an `impl`. Each one gets a label from the
first of these that speaks, and the label's source is kept beside it:

  range      an address range attract_catalog.py already names: the CRT, the
             MP3 decoder, the Windows shell, the task system, the platform
             set-up, sound, the renderer, the PSX library layer.
  name       symbols.toml names it (not ours yet): the name's prefix.
  pair       a PSX pair (analysis/pairs_propagated.json, tools/psx_pair.py)
             puts it in a PSX overlay - the sibling's names/overlays.toml gives
             the overlay's family and role - or in the boot EXE, where the
             sibling's symbols.toml may name it.
  table      a pointer-reached function whose pointer sits in a table
             symbols.toml names (a handler table is a subsystem).
  host       a pointer-reached function inherits the label of the recorded
             function it was folded into, when that one is labelled by any of
             the above.
  neighbour  the nearest labelled function below and above agree and are
             within NEIGHBOUR_GAP bytes: the linker kept the source's file
             order (docs/SHARED_SOURCE.md section 2), so a run of starts
             between two functions of one file is usually that file.

A run of starts nothing above labels also gets a HINT, not a label: the label
whose functions touch the same named data and call the same named functions
(cosine similarity of IDF-weighted reference counts). Measured leave-one-run-out
on the labelled runs the method gets about a third of them back, so it is
written beside the run for a reader to weigh, never into the label column.

Reach is measured only where a run armed the entry: the recipe_* runs armed
entries_plus_hidden.txt (2,936 recorded starts plus the 89 hidden ones the
attract run found); the hidden_* first-call runs (analysis/trace_hidden_recipes.sh)
arm the 7,294 pointer-reached starts. Where neither armed a hidden start, reach
means "the host that swallowed it was reached", which is an upper bound.

Reads the sibling checkout for the overlay and name corpora (--sibling).
The per-function output is derived from copyrighted game code: it lives under
analysis/ and is never committed (CLAUDE.md rule 1); the group counts go in
docs/remaining-catalog.md by hand.
"""
import argparse, bisect, collections, json, math, os, sys, tomllib

sys.path.insert(0, os.path.dirname(__file__))
import attract_catalog as ac

NEIGHBOUR_GAP = 0x3000
PROFILE_MIN = 0.5
PROFILE_RUN_MIN = 3
OWNED_CALLER = 0xFFFFFFFF

# The range labels attract_catalog.py knows, as (group, subsystem). The last,
# catch-all group there ('Miscellaneous game code') is deliberately not used:
# a function nothing else labels stays unlabelled here.
RANGE_GROUPS = [g for g in ac.GROUPS if g[1] != 'Miscellaneous game code']
PLATFORM = {'Not functions', 'Windows shell', 'Platform set-up', 'Task system',
            'MSVC CRT', 'MP3 decoder'}
LIBRARY = {'Sound', 'Renderer', 'PSX library layer'}

# Overlay families -> the part of the game. GAME / START / SHOP / BATTLE /
# BATL_END sit in the sibling's ETC and BATTLE families and are the game's
# modes; they are labelled by overlay name, the rest by family.
FAMILY_LABEL = {
    'BMAGIC': 'Battle magic effects (BMAGIC overlays)',
    'BOSS': 'Boss battle scripts (BOSS overlays)',
    'SCENARIO': 'Scenario event banks (SCENA overlays)',
    'PLCHAR': 'Party character sets (PLP overlays)',
    'WORLD00': 'Area overlays, world 0', 'WORLD01': 'Area overlays, world 1',
    'WORLD02': 'Area overlays, world 2', 'WORLD03': 'Area overlays, world 3',
    'WORLD04': 'Area overlays, world 4', 'WORLD05': 'Area overlays, world 5',
    'WORLD06': 'Area overlays, world 6', 'WORLD07': 'Area overlays, world 7',
    'LOGO.EXE': 'Logo',
}
NAME_LABEL = {
    'GAME': 'Field core (GAME.EMI)',
    'START': 'Field menu (START.EMI)',
    'SHOP': 'Shop / inn / save point (SHOP.EMI)',
    'BATTLE': 'Battle engine (BATTLE.EMI)',
    'BATL_END': 'Battle result (BATL_END.EMI)',
    'BATE': 'Battle extra (BATE.EMI)',
    'BATL_OVR': 'Battle overlay BATL_OVR.EMI',
    'COMMU00': 'Communication minigames (COMMU overlays)',
    'COMMU01': 'Communication minigames (COMMU overlays)', 'COMMU02': 'Communication minigames (COMMU overlays)',
    'COMMU03': 'Communication minigames (COMMU overlays)', 'COMMU04': 'Communication minigames (COMMU overlays)',
    'COMMU05': 'Communication minigames (COMMU overlays)',
    'SCE10EFF': 'Scenario effects (SCE1xEF overlays)', 'SCE15EF0': 'Scenario effects (SCE1xEF overlays)',
    'SCE15EF1': 'Scenario effects (SCE1xEF overlays)', 'SCE15EF2': 'Scenario effects (SCE1xEF overlays)',
    'SCE15EF3': 'Scenario effects (SCE1xEF overlays)',
    'SHISU': 'Master / apprentice (SHISU, SISYOU)', 'SISYOU': 'Master / apprentice (SHISU, SISYOU)',
}

# Name prefixes -> subsystem, for symbols.toml names on both sides. Order
# matters only where a prefix is a prefix of another.
PREFIX_LABEL = [
    (('Battle', 'Effect_Apply', 'Enemy', 'Boss', 'Formation', 'Encounter', 'Turn', 'Actor', 'Stat_', 'Damage', 'BattleWin', 'MagicFx', 'Magic', 'Sparkle'), 'Boot: battle'),
    (('Msg', 'Text', 'Font', 'Glyph', 'Kanji', 'Window', 'Menu', 'Choice'), 'Boot: text, windows and menus'),
    (('Field', 'Party', 'Sprite', 'Move', 'Event', 'Area', 'Map', 'Kind2', 'Object', 'Camera', 'Examine', 'Draw', 'Prim', 'Effect', 'Anim', 'Cell', 'Leader', 'Member'), 'Boot: field, map and sprites'),
    (('Inventory', 'Item', 'Equip', 'Char', 'Ability', 'Skill', 'Zenny', 'Flag', 'Save', 'Load', 'Shop', 'Inn'), 'Boot: party state, items and saves'),
    (('Gfx', 'D3d', 'Gte', 'Gpu', 'Ot', 'Tex', 'Clut', 'Display', 'Present', 'Vram', 'Screen', 'Fmv'), 'Renderer'),
    (('Sound', 'Snd', 'Music', 'Bgm', 'Se_', 'Spu', 'Stream'), 'Sound'),
    (('Task', 'Boot', 'Game', 'Title', 'NewGame', 'Mode', 'Scenario', 'Chapter', 'WorldMap', 'Demo', 'Logo'), 'Boot: top-level modes and tasks'),
    (('Pad', 'Input', 'Key', 'DInput', 'Cfg', 'Config', 'Disc', 'File', 'Dat', 'Load', 'Rand', 'Math', 'Mem', 'Str'), 'Boot: platform and utilities'),
]


def prefix_label(name):
    for prefixes, label in PREFIX_LABEL:
        if any(name.startswith(p) for p in prefixes):
            return label
    return 'Boot: ' + name.split('_')[0]


def read_counts(path):
    reach = {}
    if not os.path.exists(path):
        return reach
    for line in open(path):
        if line[0] == '#' or not line.strip():
            continue
        e, c, n = line.split()
        if int(c, 16) == 0:
            reach[int(e, 16)] = int(n)
    return reach


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--funcs', default='analysis/pc_funcs.json')
    ap.add_argument('--hidden', default='analysis/pc_hidden.json')
    ap.add_argument('--pairs', default='analysis/pairs_propagated.json')
    ap.add_argument('--area-pairs', default='analysis/area_pairs.json')
    ap.add_argument('--symbols', default='symbols.toml')
    ap.add_argument('--xref', default='analysis/pc_xref.json')
    ap.add_argument('--sibling', default='../BreathOfFire3Recomp/')
    ap.add_argument('--runs', default='analysis/calltrace',
                    help='directory of traced runs; attract = hidden_b+all_a+all_b, '
                         'shop / worldmap / combat = recipe_*')
    ap.add_argument('--out', default='analysis/remaining_catalog.md')
    ap.add_argument('--tsv', default='analysis/remaining_catalog.tsv')
    a = ap.parse_args()

    funcs = {f['entry']: f for f in json.load(open(a.funcs))['functions']}
    hidden = {h['entry']: h for h in json.load(open(a.hidden))}
    starts = sorted(set(funcs) | set(hidden))
    text_end = 0x401000 + json.load(open(a.funcs))['text']['size']

    def extent(e):
        i = bisect.bisect_right(starts, e)
        nxt = starts[i] if i < len(starts) else text_end
        rec = (funcs.get(e) or hidden.get(e)).get('size') or nxt - e
        return min(rec, nxt - e)

    sym = tomllib.load(open(a.symbols, 'rb'))
    named = {f['pc']: f for f in sym['func']}
    ours = {pc for pc, f in named.items() if 'impl' in f}
    tables = []
    for d in sym['data']:
        if d.get('ctype') in ('unsigned long', 'void *', 'unsigned int') and d.get('count'):
            tables.append((d['pc'], d['pc'] + 4 * d['count'], d['name']))

    # Hints for what an unlabelled run touches: named data and blocks, by the
    # global cross-reference (tools/pe_xref.py), and named callees.
    touch = collections.defaultdict(list)          # insn address -> data names
    if os.path.exists(a.xref):
        xref = json.load(open(a.xref))
        dname = {d['pc']: d['name'] for d in sym['data']}
        blocks = sorted((b['pc'], b['name']) for b in sym['block'])
        for k, uses in xref.items():
            addr = int(k, 16)
            name = dname.get(addr)
            if not name:
                i = bisect.bisect_right(blocks, (addr, 'ÿ')) - 1
                if i >= 0 and addr - blocks[i][0] < 0x400:
                    name = blocks[i][1] + '+'
            if name:
                for insn, _, _ in uses:
                    touch[insn].append(name)
    touch_insns = sorted(touch)

    def touched(lo, hi):
        c = collections.Counter()
        for i in range(bisect.bisect_left(touch_insns, lo), bisect.bisect_left(touch_insns, hi)):
            c.update(touch[touch_insns[i]])
        return c

    def table_of(addr):
        for lo, hi, name in tables:
            if lo <= addr < hi:
                return name
        return None

    # PSX side: overlays and names.
    overlays = {o['md5']: o for o in tomllib.load(open(a.sibling + 'names/overlays.toml', 'rb'))['overlay']}
    psx_boot = {f['pc']: f['name'] for f in tomllib.load(open(a.sibling + 'symbols.toml', 'rb'))['func']}
    psx_ovl = {f['pc']: f['name'] for f in tomllib.load(open(a.sibling + 'names/functions.toml', 'rb'))['function']}
    pairs = {}
    for p in json.load(open(a.pairs)):
        pairs.setdefault(p['pc'], p)          # first pair wins; they are ordered by method
    area_of = {}
    if os.path.exists(a.area_pairs):
        for p in json.load(open(a.area_pairs)):
            if 'area' in p:
                area_of.setdefault(p['pc'], p['area'])

    def overlay_label(sec):
        if sec == 'boot':
            return None
        o = overlays.get(sec)
        if not o:
            return 'PSX overlay ' + sec[:8]
        if o['name'] in NAME_LABEL:
            return NAME_LABEL[o['name']]
        if o['family'] in FAMILY_LABEL:
            return FAMILY_LABEL[o['family']]
        return o['family']

    # Reach.
    R = os.path.join
    reach = {
        'attract': {},
        'shop': read_counts(R(a.runs, 'recipe_shop', 'bof3x.callcounts.tsv')),
        'worldmap': read_counts(R(a.runs, 'recipe_worldmap', 'bof3x.callcounts.tsv')),
        'combat': read_counts(R(a.runs, 'recipe_combat', 'bof3x.callcounts.tsv')),
    }
    for run in ('hidden_b', 'all_a', 'all_b'):
        for e, n in read_counts(R(a.runs, run, 'bof3x.callcounts.tsv')).items():
            reach['attract'][e] = reach['attract'].get(e, 0) + n
    # First-call traces over the hidden entry list (analysis/trace_hidden_recipes.sh,
    # and hidden_a for the attract cycle): an entry in one was entered, once at least.
    for route, run in (('attract', 'hidden_a'), ('shop', 'hidden_shop'),
                       ('worldmap', 'hidden_worldmap'), ('combat', 'hidden_combat')):
        path = R(a.runs, run, 'bof3x.calltrace.tsv')
        if os.path.exists(path):
            for line in open(path):
                if line[0] != '#' and line.strip():
                    e = int(line.split()[1], 16)
                    reach[route][e] = max(reach[route].get(e, 0), 1)
    armed = set()
    for r in reach.values():
        armed |= set(r)

    # --- labelling ---------------------------------------------------------
    label, how, note = {}, {}, {}

    def put(e, lab, h, n=''):
        if e not in label and lab:
            label[e], how[e], note[e] = lab, h, n

    for e in starts:
        if e in ours:
            continue
        for _, title, pred in RANGE_GROUPS:
            if pred(e):
                put(e, title, 'range')
                break
    for e in starts:
        if e in ours or e in label:
            continue
        if e in named:
            put(e, prefix_label(named[e]['name']), 'name', named[e]['name'])
    for e in starts:
        if e in ours or e in label:
            continue
        p = pairs.get(e)
        if not p:
            continue
        lab = overlay_label(p['section'])
        pname = psx_boot.get(p['psx']) or psx_ovl.get(p['psx']) or ''
        if lab is None:                      # boot EXE
            lab = prefix_label(pname) if pname else 'Boot: unnamed'
        n = pname or f"psx {p['psx']:08X}"
        if e in area_of:
            n += f' area {area_of[e]}'
        put(e, lab, 'pair:' + p['how'], n)
    for e in starts:
        if e in ours or e in label or e not in hidden:
            continue
        for sec, addr in hidden[e]['refs']:
            if sec != '.text':
                t = table_of(addr)
                if t:
                    put(e, 'Table ' + t, 'table', t)
                    break
    for e in starts:
        if e in ours or e in label or e not in hidden:
            continue
        h = hidden[e]['host']
        if h in label and not label[h].startswith('Table '):
            put(e, label[h], 'host', f'host 0x{h:06X}')
        elif h in ours and h in named:
            put(e, prefix_label(named[h]['name']), 'host', f'host {named[h]["name"]} (ours)')
    # Neighbour fill, from the anchors so far only.
    anchors = [e for e in starts if e in label and how[e] != 'host' and not label[e].startswith('Table ')]
    for e in starts:
        if e in ours or e in label:
            continue
        i = bisect.bisect_left(anchors, e)
        if 0 < i < len(anchors):
            lo, hi = anchors[i - 1], anchors[i]
            if label[lo] == label[hi] and hi - lo <= NEIGHBOUR_GAP:
                put(e, label[lo], 'neighbour', f'between 0x{lo:06X} and 0x{hi:06X}')
    # Profile: what a function touches (named data, by the cross-reference) and
    # calls (named functions). A run of unlabelled starts is given the label
    # whose labelled functions look most like it, by cosine similarity, when
    # the similarity clears PROFILE_MIN. The method is measured first on the
    # labelled runs, leave-one-run-out, and the accuracy is printed.
    def feature(e):
        c = touched(e, e + extent(e))
        for callee in funcs.get(e, {}).get('callees', []):
            if callee in named:
                c['call:' + named[callee]['name']] += 1
        return c

    def split_runs(es):
        runs, cur = [], []
        for e in es:
            if cur and e - (cur[-1] + extent(cur[-1])) > 0x400:
                runs.append(cur); cur = []
            cur.append(e)
        if cur:
            runs.append(cur)
        return runs

    def cosine(u, v, vn=None):
        dot = sum(u[k] * v.get(k, 0) for k in u)
        un = sum(x * x for x in u.values()) ** 0.5
        vn = vn or sum(x * x for x in v.values()) ** 0.5
        return dot / (un * vn) if un and vn else 0.0

    # Coarse classes: a profile separates kinds of code, not one world's areas
    # from another's, nor one boot file from the next.
    def coarse(lab):
        if lab.startswith('Area overlays'):
            return 'Area overlays'
        if lab.startswith('Boot:') or lab in ('Top-level modes', 'Text and windows'):
            return 'Boot-resident game code'
        return lab

    profilable = lambda lab: lab not in PLATFORM and lab not in LIBRARY and not lab.startswith('Table ')
    feats = {}
    for e in starts:
        if e in ours or e not in label or not profilable(label[e]):
            continue
        feats[e] = feature(e)
    # Inverse document frequency over labelled functions: a name every
    # subsystem touches says nothing about which one this is.
    df = collections.Counter()
    for f in feats.values():
        df.update(set(f))
    n_docs = len(feats)
    idf = {k: math.log(n_docs / n) for k, n in df.items()}

    def weigh(vec):
        return collections.Counter({k: (1 + math.log(v)) * idf.get(k, math.log(n_docs)) for k, v in vec.items()})

    profile = collections.defaultdict(collections.Counter)
    for e, f in feats.items():
        profile[coarse(label[e])].update(weigh(f))
    norms = {lab: sum(x * x for x in v.values()) ** 0.5 for lab, v in profile.items()}

    def classify(vec, exclude=None):
        w = weigh(vec)
        scores = []
        for lab, prof in profile.items():
            if exclude and lab == exclude[0]:
                prof = collections.Counter({k: v - exclude[1].get(k, 0) for k, v in prof.items()})
                scores.append((cosine(w, prof), lab))
            else:
                scores.append((cosine(w, prof, norms[lab]), lab))
        scores.sort(reverse=True)
        return scores[:2]

    # Leave-one-run-out check on the labelled runs of PROFILE_RUN_MIN starts or more.
    loo = collections.defaultdict(lambda: [0, 0])
    confusion = collections.Counter()
    for lab in list(profile):
        for run in split_runs([e for e in starts if e in feats and coarse(label[e]) == lab]):
            if len(run) < PROFILE_RUN_MIN:
                continue
            vec = collections.Counter()
            for e in run:
                vec.update(feats[e])
            best = classify(vec, exclude=(lab, weigh(vec)))
            loo[lab][1] += 1
            if best and best[0][1] == lab and best[0][0] >= PROFILE_MIN:
                loo[lab][0] += 1
            elif best:
                confusion[(lab, best[0][1] if best[0][0] >= PROFILE_MIN else '(under min)')] += 1
    loo_total = [sum(v[0] for v in loo.values()), sum(v[1] for v in loo.values())]

    hint = {}                                    # run start -> profile hint
    unl = [e for e in starts if e not in ours and e not in label]
    for run in split_runs(unl):
        if len(run) < PROFILE_RUN_MIN:
            continue
        vec = collections.Counter()
        for e in run:
            vec.update(feature(e))
        best = classify(vec)
        if best:
            (s1, l1), (s2, l2) = best[0], (best[1] if len(best) > 1 else (0, ''))
            hint[run[0]] = f'{l1} {s1:.2f}, next {l2} {s2:.2f}'
    for e in starts:
        if e not in ours and e not in label:
            put(e, 'Unlabelled', 'none')

    # --- rows --------------------------------------------------------------
    rows = []
    for e in starts:
        if e in ours:
            continue
        is_hidden = e in hidden
        host = hidden[e]['host'] if is_hidden else None
        rr = {}
        for k, r in reach.items():
            if e in r:
                rr[k] = r[e]
            elif is_hidden and host in r:
                rr[k] = -r[host]                 # negative: host reached, entry not armed
        rows.append(dict(entry=e, size=extent(e), hidden=is_hidden, host=host,
                         label=label[e], how=how[e], note=note[e], reach=rr,
                         name=named[e]['name'] if e in named else ''))

    # --- output ------------------------------------------------------------
    def part_of(lab):
        if lab in PLATFORM:
            return '0 Platform (I8 / I12, not per-function)'
        if lab in LIBRARY:
            return '1 Library layer'
        if lab.startswith('Boot:') or lab.startswith('Table ') or lab in ('Top-level modes', 'Text and windows'):
            return '2 Boot-resident game code'
        if lab.startswith('Field') or lab.startswith('Shop') or lab in ('Event script', 'Map and draw layers', 'Sprite draw'):
            return '3 Field modes'
        if lab.startswith('Battle') or lab.startswith('Boss'):
            return '4 Battle'
        if lab.startswith('Area overlays'):
            return '5 Area overlays'
        if lab.startswith('Scenario') or lab.startswith('Party') or lab.startswith('Communication') or lab.startswith('Master'):
            return '6 Scenario and character sets'
        return '7 Unlabelled'

    groups = collections.defaultdict(list)
    for r in rows:
        groups[(part_of(r['label']), r['label'])].append(r)

    def reached_by(r, k):
        return k in r['reach'] and r['reach'][k] > 0

    def host_only(r, k):
        return k in r['reach'] and r['reach'][k] < 0

    out = ['# What is not ours, by what it is part of', '',
           f'{len(rows)} functions of {len(starts)} starts are not ours ({len(ours)} are). '
           'Generated by `tools/remaining_catalog.py`; see its docstring for how each label '
           'was chosen. **Reach** counts entries a traced run entered (`+`) and, for a '
           'pointer-reached function no run armed, entries whose host was entered (`~`, an '
           'upper bound). "Any" is the union of the attract cycle and the three recorded '
           'routes. Bytes are the extent to the next start.', '',
           '| Part | Group | Functions | Hidden | KiB | Attract | Shop | World map | Combat | Any + | Any ~ | Named | Paired |',
           '|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|']
    order = sorted(groups, key=lambda k: (k[0], -len(groups[k])))
    for k in order:
        rs = groups[k]
        anyp = sum(1 for r in rs if any(reached_by(r, x) for x in reach))
        anyh = sum(1 for r in rs if not any(reached_by(r, x) for x in reach) and any(host_only(r, x) for x in reach))
        out.append(f'| {k[0]} | {k[1]} | {len(rs)} | {sum(r["hidden"] for r in rs)} | '
                   f'{sum(r["size"] for r in rs) / 1024:.0f} | '
                   + ' | '.join(str(sum(1 for r in rs if reached_by(r, x))) for x in ('attract', 'shop', 'worldmap', 'combat'))
                   + f' | {anyp} | {anyh} | {sum(1 for r in rs if r["name"])} | {sum(1 for r in rs if r["how"].startswith("pair"))} |')
    out.append(f'| | **Total** | **{len(rows)}** | **{sum(r["hidden"] for r in rows)}** | '
               f'**{sum(r["size"] for r in rows) / 1024:.0f}** | | | | | | | | |')
    out.append('')
    out.append('How the labels were reached:')
    out.append('')
    hc = collections.Counter(r['how'].split(':')[0] for r in rows)
    out.append('| Source | Functions |'); out.append('|---|--:|')
    for h, n in hc.most_common():
        out.append(f'| {h} | {n} |')
    out.append('')
    out.append(f'The profile hint, leave-one-run-out on the labelled runs of {PROFILE_RUN_MIN} or more starts: '
               f'{loo_total[0]} of {loo_total[1]} runs get their own label back at or over {PROFILE_MIN}. '
               'That is why it is a hint beside the unlabelled runs and not a label.')
    out.append('')
    out.append('| Label | Runs right | Runs |'); out.append('|---|--:|--:|')
    for lab, (ok, n) in sorted(loo.items(), key=lambda kv: -kv[1][1]):
        out.append(f'| {lab} | {ok} | {n} |')
    out.append('')
    out.append('Where it went wrong (true label, label given):'); out.append('')
    for (t, g), n in confusion.most_common():
        out.append(f'- {t} -> {g}: {n}')
    out.append('')

    for k in order:
        rs = groups[k]
        out.append(f'## {k[1]}'); out.append('')
        out.append(f'{k[0]}. {len(rs)} functions, {sum(r["size"] for r in rs):,} bytes.'); out.append('')
        # Address runs: consecutive starts with the same label, to show the shape.
        runs, cur = [], []
        for r in rs:
            if cur and r['entry'] - (cur[-1]['entry'] + cur[-1]['size']) > 0x400:
                runs.append(cur); cur = []
            cur.append(r)
        if cur:
            runs.append(cur)
        out.append('Address runs (a gap over 0x400 bytes starts a new one):'); out.append('')
        out.append('| From | To | Functions | Hidden | Reached (any) | Touches (named data, by reference count) | Calls (named functions) | Profile hint |')
        out.append('|---|---|--:|--:|--:|---|---|---|')
        for run in runs:
            lo, hi = run[0]['entry'], run[-1]['entry'] + run[-1]['size']
            t = ', '.join(f'{n} x{c}' for n, c in touched(lo, hi).most_common(4))
            cc = collections.Counter()
            for r in run:
                for callee in funcs.get(r['entry'], {}).get('callees', []):
                    if callee in named:
                        cc[named[callee]['name']] += 1
            calls = ', '.join(f'{n} x{c}' for n, c in cc.most_common(4))
            out.append(f'| `0x{lo:06X}` | `0x{hi:06X}` | {len(run)} | '
                       f'{sum(r["hidden"] for r in run)} | {sum(1 for r in run if any(reached_by(r, x) for x in reach))} | {t} | {calls} | {hint.get(lo, "")} |')
        out.append('')
        out.append('| Entry | Bytes | Hidden in | Label from | Name / PSX | Attract | Shop | WM | Combat |')
        out.append('|---|--:|---|---|---|--:|--:|--:|--:|')
        for r in rs:
            def cell(x):
                v = r['reach'].get(x)
                return '' if v is None else (f'+{v}' if v > 0 else f'~{-v}')
            out.append(f'| `0x{r["entry"]:06X}` | {r["size"]} | '
                       f'{"`0x%06X`" % r["host"] if r["hidden"] else ""} | {r["how"]} | '
                       f'{r["name"] or r["note"]} | {cell("attract")} | {cell("shop")} | {cell("worldmap")} | {cell("combat")} |')
        out.append('')
    open(a.out, 'w', encoding='utf-8').write('\n'.join(out))

    with open(a.tsv, 'w', encoding='utf-8') as f:
        f.write('entry\tsize\thidden\thost\tpart\tlabel\thow\tname\tnote\tattract\tshop\tworldmap\tcombat\n')
        for r in rows:
            f.write('\t'.join([f'0x{r["entry"]:06X}', str(r['size']), str(int(r['hidden'])),
                               f'0x{r["host"]:06X}' if r['hidden'] else '', part_of(r['label']), r['label'],
                               r['how'], r['name'], r['note']]
                              + [str(r['reach'].get(x, '')) for x in ('attract', 'shop', 'worldmap', 'combat')]) + '\n')
    print(f'{len(rows)} not ours of {len(starts)}; {len(groups)} groups -> {a.out}, {a.tsv}')
    for k in order:
        print(f'{len(groups[k]):5d}  {k[0]:40s} {k[1]}')


if __name__ == '__main__':
    main()
