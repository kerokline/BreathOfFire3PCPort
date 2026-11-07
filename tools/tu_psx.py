#!/usr/bin/env python
"""Recover the PlayStation build's translation units, and place each PC twin in one.

    python tools/tu_psx.py [--sibling ../BreathOfFire3Recomp]
        -> analysis/tu_psx.tsv         one row per PC function with a PSX twin
        -> analysis/tu_psx_ranges.tsv  PC address ranges whose twins are one PSX image
    python tools/tu_psx.py --units <scratch>/units.tsv   (also every PSX unit, both levels)
    python tools/tu_psx.py --strings --exe <BOF3.exe>    (source-file names in both binaries)

docs/source-layout-round.md section 3 (SL2) is the brief and has the run's
numbers. The PSX build is the second observation of the source tree the PC
port was compiled from (docs/SHARED_SOURCE.md). Psy-Q's linker lays every
object's contribution to each section out in the same object order, so a
file's statics sit in one block of each section, in the order of the file's
place in `.text`. Joins, strongest first:

  - **window**: a data item of the image's own (boot: its data and the zero
    runs its code reads; overlay: its section and the room up to the next
    band, its .bss) read only by functions within `WINDOW` functions and
    `MAX_SPAN` bytes: a file's static. Its first to last reader are joined.
  - **inversion**: two such items lie in one ordering domain in the opposite
    order to their readers. Object order is the same in every section, so two
    units never invert: the readers of both, and all between, are one unit.
    A drop of more than `MAXJOIN` functions is read as a section restart (or
    an extern) and joins nothing.
  - **shared** (overlays only): a boot-image data item read by a window of at
    most `SHARED_WINDOW` functions of the overlay. Capcom's overlays keep most
    of their variables in the boot image's data, so this is the only data an
    overlay function often reads; weaker, since it may be an extern.
  - **call** (a hint column only, never a join): a helper whose every caller is
    a near neighbour. On the boot image's Sony libraries it put two files'
    copies of one static (memclr, csqrt, InitCARD...) in one unit five times in
    eleven, so it does not join.

Two levels are written: the unit (`psx_unit`: window + inversion + shared)
and the data unit (`psx_unit_data`: window + inversion, the image's own data
only). On the Sony libraries neither level puts two copies of one static in
one unit (0 of 11). A boundary between two units is open: no join crosses it,
which a unit edge needs but does not prove, so a unit is a lower bound - a
real file may hold several neighbouring units. A function with no join is a
unit of one. An overlay (`GAME.EMI#0`, `BATTLE.EMI#15`) is a separately linked
image, so its units are independent of the boot executable's.

The agreement columns compare each unit with the PC: `same` (its twins are
contiguous on the PC with nothing between), `superset` (contiguous, with PC
functions that have no twin between them - port code or unpaired),
`subset` (some of the unit's PSX functions have no PC twin), `interleaved`
(a PC function twinned into another unit lies between), `single`.

Inputs, all read from the sibling (read only): `disc/SLPS_009.90` (the boot
executable), `analysis/overlay_captures_all.json` (406 overlay images and
their known starts), `analysis/emi_sections.json` with the disc (sections the
captures hold under another name, or skipped), `analysis/functions.tsv` and
`symbols.toml` (the boot executable's starts), `names/functions.toml` (named
overlay starts). From this repo: `symbols.toml`'s `psx` / `psx_overlay`.

The TSVs hold addresses, our names and counts only: no byte of either game.
"""
import argparse
import base64
import bisect
import collections
import json
import os
import re
import struct
import sys
import tomllib

BOOT = 'SLPS_009.90'
BOOT_LO = 0x80093800             # t_addr of the PS-EXE header
WINDOW = 12                      # a static's readers lie within this many functions
MAX_SPAN = 0x3000                # ... and within this many bytes of .text

# MIPS registers written by a jal (caller-saved), so lui values do not survive one
CLOBBER_CALL = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31}
LOADS = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26}
STORES = {0x28, 0x29, 0x2A, 0x2B, 0x2E}
COP_MEM = {0x31, 0x32, 0x39, 0x3A}  # lwc1/lwc2/swc1/swc2 (GTE loads and stores)


def s16(x):
    return x - 0x10000 if x & 0x8000 else x


class Image:
    """One separately linked PSX image: the boot executable or one overlay section."""

    def __init__(self, name, lo, data, starts, gp=None):
        self.name, self.lo, self.data, self.gp = name, lo, data, gp
        self.hi = lo + len(data)
        self.starts = sorted(s for s in set(starts) if lo <= s < self.hi and s % 4 == 0)
        self.text_lo = self.text_hi = None

    def word(self, a):
        return struct.unpack_from('<I', self.data, a - self.lo)[0]

    def func_end(self, i):
        """End of function i: the next start, cut at the end of the image's text."""
        nxt = self.starts[i + 1] if i + 1 < len(self.starts) else self.text_hi
        return min(nxt, self.text_hi)


def jal_targets(img, lo, hi):
    out = set()
    for a in range(lo, hi - 3, 4):
        w = img.word(a)
        if w >> 26 == 3:
            out.add(((a + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2))
    return out


def find_text(img):
    """The image's code range: from the first start to the last function's jr ra + delay slot.

    Psy-Q lays an image out as header/.rdata?, .text, .data...; past the last
    start the next `jr ra` (0x03E00008) ends the text."""
    if not img.starts:
        img.text_lo = img.text_hi = img.lo
        return
    img.text_lo = img.starts[0]
    a = img.starts[-1]
    while a + 8 <= img.hi:
        if img.word(a) == 0x03E00008:
            img.text_hi = a + 8
            break
        a += 4
    else:
        img.text_hi = img.hi


def refs_of(img, lo, hi):
    """Absolute data addresses one function forms: lui+addiu/ori/load/store pairs and
    gp-relative accesses. Returns {address: kind} with kind 'abs' or 'gp'."""
    reg = {}
    out = {}
    for a in range(lo, hi - 3, 4):
        w = img.word(a)
        op, rs, rt, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
        if op == 0x0F:                                   # lui
            reg[rt] = imm << 16
            continue
        if op in (0x08, 0x09) or op in LOADS or op in STORES or op in COP_MEM or op == 0x0D:
            base = None
            if rs == 28 and img.gp is not None:
                base, kind = img.gp, 'gp'
            elif rs in reg:
                base, kind = reg[rs], 'abs'
            if base is not None:
                v = (base | imm) if op == 0x0D else (base + s16(imm)) & 0xFFFFFFFF
                out.setdefault(v, kind)
            if op in (0x08, 0x09, 0x0D) or op in LOADS:
                reg.pop(rt, None)
            continue
        if op == 0:                                      # SPECIAL: writes rd
            rd = (w >> 11) & 31
            if w & 0x3F == 0x21 and (rs in reg) != (rt in reg) and rd:
                # addu rd, lui-reg, index: a table indexed off a %hi (switch tables, arrays)
                reg[rd] = reg[rs] if rs in reg else reg[rt]
                continue
            reg.pop(rd, None)
            if w & 0x3F == 0x09:                         # jalr
                for r in CLOBBER_CALL:
                    reg.pop(r, None)
        elif op == 3:                                    # jal
            for r in CLOBBER_CALL:
                reg.pop(r, None)
        elif op in (0x0A, 0x0B, 0x0C, 0x0E):             # slti sltiu andi xori
            reg.pop(rt, None)
        elif op == 0x12:                                 # COP2 mfc2/cfc2 write rt
            if rs in (0, 2):
                reg.pop(rt, None)
        elif op == 0x10 and rs in (0, 2):
            reg.pop(rt, None)
    return out


def load_sibling(sib):
    """Every PSX image and its function starts."""
    named = collections.defaultdict(set)
    nf = os.path.join(sib, 'names', 'functions.toml')
    if os.path.exists(nf):
        for f in tomllib.load(open(nf, 'rb')).get('function', []):
            named[f['overlay']].add(f['pc'])
    images = {}
    boot = open(os.path.join(sib, 'disc', BOOT), 'rb').read()
    hdr = struct.unpack_from('<10I', boot, 0x10)
    assert hdr[2] == BOOT_LO, hex(hdr[2])
    bdata = boot[0x800:0x800 + hdr[3]]
    starts = set()
    for line in open(os.path.join(sib, 'analysis', 'functions.tsv')):
        p = line.rstrip('\n').split('\t')
        if line.startswith('0x') and p[3] != 'data':
            starts.add(int(p[0], 16))
    for f in tomllib.load(open(os.path.join(sib, 'symbols.toml'), 'rb')).get('func', []):
        starts.add(f['pc'])
    gp = boot_gp(bdata)
    img = Image('main', BOOT_LO, bdata, starts, gp)
    # the boot executable's code is one non-zero run; overlay bands inside it are zero
    img.starts = [s for s in img.starts if img.word(s) != 0 or img.word(s + 4) != 0]
    images['main'] = img
    by_md5 = {}
    for c in json.load(open(os.path.join(sib, 'analysis', 'overlay_captures_all.json'))):
        lo = int(c['load_addr'], 16) if isinstance(c['load_addr'], str) else c['load_addr']
        name = '%s#%d' % (c['source_file'].rsplit('/', 1)[-1], c['source_index'])
        st = {int(x, 16) for k in ('static_discovery_entry_pcs', 'header_entry_pcs', 'engine_entry_pcs')
              for x in c.get(k, [])} | named.get(c['source_md5'], set())
        images[name] = Image(name, lo, base64.b64decode(c['bytes_b64']), st)
        images[name].md5 = c['source_md5']
        by_md5[c['source_md5']] = name
    images['_alias'] = {}
    secs = os.path.join(sib, 'analysis', 'emi_sections.json')
    if os.path.exists(secs):
        # sections the captures hold under another name (same bytes), and code sections
        # the captures skipped, read from the disc the sibling surveyed
        disc = None
        for r in json.load(open(secs))['sections']:
            name = '%s#%d' % (r['file'].rsplit('/', 1)[-1], r['index'])
            if name in images or 'md5' not in r:
                continue
            if r['md5'] in by_md5:
                images['_alias'][name] = by_md5[r['md5']]
            elif r.get('class') in ('code', 'mixed'):
                if disc is None:
                    disc = open_disc(sib)
                    if disc is False:
                        continue
                if not disc:
                    continue
                blob = disc.read(r['file'])[r['offset']:r['offset'] + r['size']]
                images[name] = Image(name, r['dest'], blob, named.get(r['md5'], set()))
                images[name].md5 = r['md5']
                images[name].from_disc = True
                by_md5[r['md5']] = name
    return images


def open_disc(sib):
    """The sibling's disc image through tools/psx_disc.py, or False when absent."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import psx_disc
    d = os.path.join(sib, 'disc')
    for f in sorted(os.listdir(d)) if os.path.isdir(d) else []:
        if f.lower().endswith('.cue'):
            return psx_disc.Disc(os.path.join(d, f))
    return False


def boot_gp(data):
    """gp as the boot entry sets it: the first lui gp / addiu gp,gp pair."""
    for o in range(0, len(data) - 4, 4):
        w, = struct.unpack_from('<I', data, o)
        if w >> 26 == 0x0F and (w >> 16) & 31 == 28:
            w2, = struct.unpack_from('<I', data, o + 4)
            if w2 >> 26 == 0x09 and (w2 >> 16) & 31 == 28 and (w2 >> 21) & 31 == 28:
                return ((w & 0xFFFF) << 16) + s16(w2 & 0xFFFF)
    return None


def finish_starts(img, extra=()):
    """Add jal targets inside the image and the twins' addresses, then fix the text range."""
    img.starts = sorted(set(img.starts) | {x for x in extra if img.lo <= x < img.hi})
    find_text(img)
    for _ in range(3):
        add = set()
        for i, s in enumerate(img.starts):
            add |= {t for t in jal_targets(img, s, img.func_end(i)) if img.text_lo <= t < img.text_hi}
        new = sorted(set(img.starts) | add)
        if new == img.starts:
            break
        img.starts = new
        find_text(img)
    img.starts = [s for s in img.starts if s < img.text_hi]


MAIN_DATA = (0x80141000, 0x80196800)   # the boot image's data and the zero runs its code reads,
                                       # below GAME.EMI's band (measured, docs/source-layout-round.md SL2)
DOMAIN_GAP = 0x1000                    # a larger hole between two read items starts a new ordering domain
DROP = 64                              # an owner drop of more functions than this is a section restart
MAXJOIN = 12                           # an inversion joins at most this many functions; a larger drop restarts
SHARED_WINDOW = 6                      # the weaker joins (boot-data globals, calls) span at most this many


def data_range(img):
    """Where the image's own statics can lie: the boot image's data, or an overlay's
    section and the room after it up to the next band's load address (its .bss)."""
    if img.name == 'main':
        return MAIN_DATA
    return (img.lo, getattr(img, 'band_hi', img.hi))


def collect(img, rng):
    """{item address: set of reader indices} for the data in rng the image's functions read."""
    lo, hi = rng
    rd = collections.defaultdict(set)
    for i, s in enumerate(img.starts):
        for a in refs_of(img, s, img.func_end(i)):
            if lo <= a < hi and not (img.text_lo <= a < img.text_hi):
                rd[a].add(i)
    return rd


def callers(img):
    """{callee index: set of caller indices} over the image's own jal edges."""
    idx = {s: i for i, s in enumerate(img.starts)}
    out = collections.defaultdict(set)
    for i, s in enumerate(img.starts):
        for t in jal_targets(img, s, img.func_end(i)):
            if t in idx and idx[t] != i:
                out[idx[t]].add(i)
    return out


# evidence kinds, strongest first: the unit's confidence is its weakest link
KINDS = ('window', 'inversion', 'shared', 'call')


def recover_units(img):
    """Join functions into units (see the module doc).

    Returns (found, stats): found = {kind: [(first index, last index, address)]},
    each a join of the functions first..last with the item (or callee) address
    that gives it; assemble() turns a choice of kinds into units."""
    pos = img.starts
    rd = collect(img, data_range(img))

    def local(r, w=WINDOW):
        return max(r) - min(r) <= w and pos[max(r)] - pos[min(r)] <= MAX_SPAN

    found = {k: [] for k in KINDS}

    items = sorted(a for a, r in rd.items() if local(r))
    for a in items:
        r = rd[a]
        if len(r) > 1:
            found['window'].append((min(r), max(r), a))
    # ordering domains, then the isolation filter, then inversions
    doms, cur = [], []
    for a in items:
        if cur and (a - cur[-1] > DOMAIN_GAP or max(rd[cur[-1]]) - min(rd[a]) > DROP):
            doms.append(cur)
            cur = []
        cur.append(a)
    if cur:
        doms.append(cur)
    for d in doms:
        keep = []
        for k, a in enumerate(d):
            near = [d[x] for x in (k - 1, k + 1) if 0 <= x < len(d)]
            if any(abs(min(rd[b]) - min(rd[a])) <= WINDOW for b in near):
                keep.append(a)
        run_hi = None
        for a in keep:
            lo_a, hi_a = min(rd[a]), max(rd[a])
            if run_hi is not None and run_hi - lo_a > MAXJOIN:
                run_hi = None                    # too far back: a section restart, or an extern
            if run_hi is not None and lo_a < run_hi:
                found['inversion'].append((lo_a, run_hi, a))
            if run_hi is None or hi_a > run_hi:
                run_hi = hi_a
    # an overlay's statics mostly live in the boot image's data (Capcom's globals):
    # a boot-data item read by one short window of the overlay only
    if img.name != 'main':
        sh = collect(img, MAIN_DATA)
        for a, r in sh.items():
            if len(r) > 1 and local(r, SHARED_WINDOW):
                found['shared'].append((min(r), max(r), a))
    # a helper whose every caller in the image is a near neighbour
    for j, c in callers(img).items():
        r = c | {j}
        if local(r, SHARED_WINDOW):
            found['call'].append((min(r), max(r), pos[j]))
    stats = dict(items=len(items), domains=len(doms), **{k: len(v) for k, v in found.items()})
    return found, stats


def assemble(n, found, kinds):
    """Units from the joins of the given kinds: (units, links)."""
    links = {}
    for kind in kinds:
        for i, j, a in found[kind]:
            for k in range(i, j):
                links.setdefault(k, (kind, a))
    units, start = [], 0
    for k in range(n):
        if k == n - 1 or k not in links:
            units.append((start, k))
            start = k + 1
    return units, links




def set_bands(images):
    """band_hi for each overlay: the next load address above its own (or its own end)."""
    los = sorted({i.lo for i in images.values()} | {MAIN_DATA[0]})
    for i in images.values():
        if i.name == 'main':
            continue
        nxt = [x for x in los if x > i.lo]
        i.band_hi = max(i.hi, min(nxt[0] if nxt else i.hi, i.hi + 0x8000))


def load_pc(path):
    """This repo's [[func]] rows in pc order, with each one's end (the next start)."""
    fs = sorted(tomllib.load(open(path, 'rb'))['func'], key=lambda f: f['pc'])
    for f, g in zip(fs, fs[1:] + [None]):
        f['_end'] = g['pc'] if g else f['pc'] + 1
    return fs


def resolve_images(images, fs):
    """The PSX image of every PC twin: `psx_overlay` when set; else the boot executable
    when its code holds the address; else the one overlay whose known starts hold it;
    else the overlay its PC neighbours' twins are in, when that overlay's code holds it."""
    known = {n: set(i.starts) for n, i in images.items()}
    main = images['main']
    out = {}
    for k, f in enumerate(fs):
        if 'psx' not in f:
            continue
        x = f['psx']
        if 'psx_overlay' in f:
            how = 'symbols'
            if '_psx_overlay_named' in f:
                how = 'symbols:%s-same-bytes' % f['_psx_overlay_named']
            out[f['pc']] = (f['psx_overlay'], how)
            continue
        if main.text_lo <= x < main.text_hi:
            out[f['pc']] = ('main', 'boot-code')
            continue
        cands = [n for n, i in images.items() if n != 'main' and x in known[n]]
        if len(cands) == 1:
            out[f['pc']] = (cands[0], 'one-start')
            continue
        near = collections.Counter()
        for g in fs[max(0, k - 8):k + 9]:
            if g.get('psx_overlay'):
                near[g['psx_overlay']] += 1
        pick = [n for n, _ in near.most_common() if n in images and
                images[n].text_lo <= x < images[n].text_hi]
        if pick:
            out[f['pc']] = (pick[0], 'neighbours')
        elif cands:
            out[f['pc']] = ('?' + ','.join(sorted(cands)[:3]) + (',...' if len(cands) > 3 else ''),
                            'ambiguous-%d' % len(cands))
        else:
            out[f['pc']] = ('?', 'in-no-image')
    return out


def build(sib, symbols):
    images = load_sibling(sib)
    alias = images.pop('_alias')
    set_bands(images)
    for img in images.values():
        finish_starts(img)
    fs = load_pc(symbols)
    for f in fs:
        if f.get('psx_overlay') in alias:
            f['_psx_overlay_named'] = f['psx_overlay']
            f['psx_overlay'] = alias[f['psx_overlay']]
    where = resolve_images(images, fs)
    want = collections.defaultdict(set)
    for f in fs:
        if f['pc'] in where and where[f['pc']][0] in images:
            want[where[f['pc']][0]].add(f['psx'])
    result = {}
    for n in sorted(want):
        img = images[n]
        finish_starts(img, want[n])
        found, stats = recover_units(img)
        result[n] = (img, found, stats)
    return images, fs, where, result


# 'u': the unit; 'd': joined by the image's own data only. Call joins are not in either:
# measured on the boot executable's Sony libraries they put two files' copies of one
# static (memclr, csqrt) in one unit, so they are a hint column only (see the module doc).
LEVELS = {'u': ('window', 'inversion', 'shared'), 'd': ('window', 'inversion')}


def unit_rows(img, units, links, tag='u'):
    """Per unit: id, range, size, the confidence (its weakest link) and the evidence."""
    rows = []
    for u, (i, j) in enumerate(units):
        kinds = collections.Counter(links[k][0] for k in range(i, j))
        conf = 'single' if i == j else max(kinds, key=KINDS.index)
        ev = ','.join('%s:%d' % (k, kinds[k]) for k in KINDS if kinds[k])
        ex = ','.join(sorted({'%s@%08X' % (links[k][0], links[k][1]) for k in range(i, j)})[:3])
        rows.append(dict(uid='%s:%s%03d' % (img.name, tag, u), lo=img.starts[i], hi=img.func_end(j),
                         first=i, last=j, n=j - i + 1, conf=conf, ev=ev, ex=ex))
    return rows


def agreement(fs, where, uof, urows):
    """Each unit against the PC: where its twins sit in PC order and what lies between."""
    pcidx = {f['pc']: k for k, f in enumerate(fs)}
    tw_unit = {pc: uof.get((n, fs[pcidx[pc]]['psx'])) for pc, (n, _) in where.items()}
    members = collections.defaultdict(list)
    for pc, u in tw_unit.items():
        if u:
            members[u].append(pcidx[pc])
    agree = {}
    for u, ks in members.items():
        lo, hi = min(ks), max(ks)
        other = sum(1 for k in range(lo, hi + 1)
                    if fs[k]['pc'] in tw_unit and tw_unit[fs[k]['pc']] != u)
        bare = sum(1 for k in range(lo, hi + 1) if fs[k]['pc'] not in tw_unit)
        r = urows[u]
        twinned = len({fs[k]['psx'] for k in ks})
        if r['n'] == 1:
            word = 'single'
        elif other:
            word = 'interleaved'
        elif twinned < r['n']:
            word = 'subset+superset' if bare else 'subset'
        elif bare:
            word = 'superset'
        else:
            word = 'same'
        agree[u] = dict(word=word, pc_lo=fs[lo]['pc'], pc_hi=fs[hi]['_end'], other=other,
                        bare=bare, twinned=twinned)
    return tw_unit, agree


COLS = ['pc', 'name', 'psx', 'psx_image', 'image_from',
        'psx_unit', 'psx_unit_range', 'unit_funcs', 'unit_twinned', 'confidence', 'evidence',
        'evidence_items', 'agreement', 'pc_span_of_unit', 'pc_inside_other_unit', 'pc_inside_no_twin',
        'psx_unit_data', 'psx_unit_data_range', 'unit_data_funcs', 'unit_data_twinned', 'agreement_data',
        'call_joined', 'file_name']


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--sibling', default='../BreathOfFire3Recomp')
    ap.add_argument('--symbols', default='symbols.toml')
    ap.add_argument('--out', default='analysis/tu_psx.tsv')
    ap.add_argument('--ranges', default='analysis/tu_psx_ranges.tsv',
                    help='PC address ranges whose twins are one PSX image')
    ap.add_argument('--units', help='also write every PSX unit of the paired images here')
    ap.add_argument('--strings', action='store_true',
                    help='search both binaries for source-file names, then stop')
    ap.add_argument('--exe', default='bof3/BOF3.exe')
    a = ap.parse_args()
    if a.strings:
        return find_strings(a)
    images, fs, where, result = build(a.sibling, a.symbols)
    lv = {}
    for tag, kinds in LEVELS.items():
        uof, urows = {}, {}
        for n, (img, found, stats) in result.items():
            units, links = assemble(len(img.starts), found, kinds)
            for r in unit_rows(img, units, links, tag):
                urows[r['uid']] = r
                for i in range(r['first'], r['last'] + 1):
                    uof[(n, img.starts[i])] = r['uid']
        tw_unit, agree = agreement(fs, where, uof, urows)
        lv[tag] = (urows, tw_unit, agree)
    names = file_names(result)
    call_hint = {}
    for n, (img, found, stats) in result.items():
        _, links = assemble(len(img.starts), found, ('call',))
        for i, s_ in enumerate(img.starts):
            side = [w for w, k in (('prev', i - 1), ('next', i)) if k in links]
            if side:
                call_hint[(n, s_)] = '+'.join(side)
    n_rows = 0
    with open(a.out, 'w', newline='\n') as o:
        o.write('# tools/tu_psx.py: one row per PC function with a PSX twin, and the PSX unit that holds\n'
                '# the twin. Addresses, our names and counts only. docs/source-layout-round.md 3 (SL2).\n')
        o.write('\t'.join(COLS) + '\n')
        for f in fs:
            if f['pc'] not in where:
                continue
            img_name, how = where[f['pc']]
            row = ['0x%X' % f['pc'], f['name'], '0x%08X' % f['psx'], img_name, how]
            for tag in ('u', 'd'):
                urows, tw_unit, agree = lv[tag]
                u = tw_unit.get(f['pc'])
                r = urows.get(u) if u else None
                g = agree.get(u) if u else None
                row += [u or '', ('0x%08X..0x%08X' % (r['lo'], r['hi'])) if r else '',
                        str(r['n']) if r else '', str(g['twinned']) if g else '']
                if tag == 'u':
                    row += [r['conf'] if r else '', r['ev'] if r else '', r['ex'] if r else '']
                row += [g['word'] if g else '']
                if tag == 'u':
                    row += [('0x%X..0x%X' % (g['pc_lo'], g['pc_hi'])) if g else '',
                            str(g['other']) if g else '', str(g['bare']) if g else '']
            row.append(call_hint.get((img_name, f['psx']), ''))
            row.append(names.get((img_name, f['psx']), ''))
            o.write('\t'.join(row) + '\n')
            n_rows += 1
    write_ranges(a.ranges, fs, where)
    if a.units:
        with open(a.units, 'w', newline='\n') as o:
            o.write('unit\trange\tfuncs\tconfidence\tevidence\tevidence_items\tfile_names\n')
            for tag in ('u', 'd'):
                for u, r in lv[tag][0].items():
                    img = result[u.rsplit(':', 1)[0]][0]
                    fn = sorted({names[(img.name, img.starts[i])] for i in range(r['first'], r['last'] + 1)
                                 if (img.name, img.starts[i]) in names})
                    o.write('%s\t0x%08X..0x%08X\t%d\t%s\t%s\t%s\t%s\n' % (
                        u, r['lo'], r['hi'], r['n'], r['conf'], r['ev'], r['ex'], ','.join(fn)))
    summary(a.out, n_rows, where, result, lv)


def write_ranges(path, fs, where):
    """PC ranges whose twinned functions are all in one PSX image (PC functions with no
    twin between two twins of the same image stay inside the range)."""
    runs = []
    for f in fs:
        if f['pc'] not in where:
            continue
        n = where[f['pc']][0]
        if runs and runs[-1][0] == n:
            runs[-1][2] = f['_end']
            runs[-1][3] += 1
        else:
            runs.append([n, f['pc'], f['_end'], 1])
    pcs = [f['pc'] for f in fs]
    with open(path, 'w', newline='\n') as o:
        o.write('# tools/tu_psx.py: PC address ranges whose PSX twins are all in one image, in pc order.\n')
        o.write('psx_image\tpc_lo\tpc_hi\ttwins\tpc_funcs\n')
        for n, lo, hi, k in runs:
            o.write('%s\t0x%X\t0x%X\t%d\t%d\n' % (
                n, lo, hi, k, bisect.bisect_left(pcs, hi) - bisect.bisect_left(pcs, lo)))


def summary(out, n_rows, where, result, lv):
    print('%s: %d rows' % (out, n_rows))
    print('image found by:', dict(collections.Counter(h for _, h in where.values())))
    tot = collections.Counter()
    for n, (img, found, stats) in result.items():
        tot['images'] += 1
        tot['functions'] += len(img.starts)
        for k, v in stats.items():
            tot[k] += v
    print('PSX images with twins: %d; their functions %d' % (tot['images'], tot['functions']))
    print('data items read %(items)d in %(domains)d ordering domains; joins: window %(window)d, '
          'inversion %(inversion)d, shared %(shared)d, call %(call)d' % tot)
    for tag, (urows, tw_unit, agree) in lv.items():
        multi = [r for r in urows.values() if r['n'] > 1]
        print('[%s] units %d; of two or more %d holding %d functions; largest %d' % (
            tag, len(urows), len(multi), sum(r['n'] for r in multi), max(r['n'] for r in urows.values())))
        print('[%s] unit confidence:' % tag, dict(collections.Counter(r['conf'] for r in urows.values())))
        print('[%s] agreement (units with twins):' % tag,
              dict(collections.Counter(g['word'] for g in agree.values())))


# ---------------------------------------------------------------------------
# Source-file names in the binaries

NAME_RE = re.compile(rb'[A-Za-z0-9_\-/\\:.]{1,60}\.(?:c|C|h|H|cpp|CPP|cxx|obj|OBJ|lib|LIB|asm|ASM|s|S)(?![A-Za-z0-9])')
PLAUSIBLE = re.compile(rb'[A-Za-z_][A-Za-z0-9_/\\\-]{2,}\.(?:c|C|h|H|cpp|CPP|cxx|obj|OBJ|lib|LIB|asm|ASM)')
RCS_RE = re.compile(rb'\$Id: ([A-Za-z0-9_]+\.[A-Za-z]+),v ')
MARKERS = re.compile(rb'(?i)(__FILE__|assert|\.pdb|RSDS|NB10|line %d)')


def scan(blob):
    """(offset, name, kind) for every source-file-shaped string: kind 'rcs' for an RCS $Id
    line, 'name' for a plausible bare file name, 'marker' for assert / pdb text."""
    hits = []
    for m in RCS_RE.finditer(blob):
        hits.append((m.start(), m.group(1).decode(), 'rcs'))
    for m in NAME_RE.finditer(blob):
        s = m.group(0)
        if PLAUSIBLE.fullmatch(s) and not any(h[0] <= m.start() < h[0] + 80 for h in hits if h[2] == 'rcs'):
            hits.append((m.start(), s.decode(), 'name'))
    for m in MARKERS.finditer(blob):
        hits.append((m.start(), m.group(0).decode('latin1'), 'marker'))
    return sorted(hits)


def file_names(result):
    """Real file names that the boot executable's RCS $Id strings give. Sony's library
    sources put the string first in a file's .rdata, so the first item read after it
    belongs to that file, and so do the functions that read it.
    Returns {(image, function start): file name}."""
    names = {}
    if 'main' not in result:
        return names
    img = result['main'][0]
    rd = collect(img, data_range(img))
    items = sorted(rd)
    for off, name, kind in scan(img.data):
        if kind != 'rcs':
            continue
        k = bisect.bisect_right(items, img.lo + off)
        if k < len(items) and items[k] - (img.lo + off) < 0x100:
            for i in rd[items[k]]:
                names[('main', img.starts[i])] = name
    return names


def find_strings(a):
    """Every source-file-shaped string in the PC exe, the PSX images, and (when the
    sibling has its disc) every file on the PSX disc. Bare names are mostly chance
    byte runs in data; an RCS line or a marker is the signal."""
    blobs = [('PC ' + a.exe, open(a.exe, 'rb').read())]
    images = load_sibling(a.sibling)
    images.pop('_alias')
    for n, img in sorted(images.items()):
        blobs.append(('PSX ' + n, img.data))
    disc = open_disc(a.sibling)
    if disc:
        for f, (lba, size) in sorted(disc.files.items()):
            if size < 40_000_000:
                blobs.append(('disc ' + f, disc.read(f)))
    total = collections.Counter()
    for label, blob in blobs:
        for off, name, kind in scan(blob):
            print('%s\t+0x%X\t%s\t%s' % (label, off, kind, name))
            total[kind] += 1
    print('# %d blobs searched (PC exe, %d PSX images, %d disc files); hits by kind: %s' % (
        len(blobs), len(images), len(blobs) - 1 - len(images), dict(total)))


if __name__ == '__main__':
    sys.exit(main())
