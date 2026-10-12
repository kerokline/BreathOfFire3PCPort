"""Known differences for the A/B/C harness (recipes/abc-known.toml,
docs/abc-harness.md 5): which differing frame regions and state-hash pages
an install pair is expected to show, each tied to a ledger entry.

A rule matches a region of a frame pair when every condition it states
holds:

    pair_trait = "psp"        exactly one side of the pair has the trait
    both_traits = ["en-150"]  both sides have each
    routes = ["attract"]      the route's name
    ticks = [LO, HI]          the frame's tick (recipe frame), inclusive
    modes = [2]               Game_Mode, the same on both sides
    areas = [4, 31]           Game_AreaNumber, the same on both sides
    area_data = "base"        the area's AREAnnn.DAT has a `base` chunk whose
                              hash differs between the two sides' caches
                              (their manifest.toml); "loc" the same for the
                              side's language layer, loc/<lang>
    box = [X0, Y0, X1, Y1]    the region lies inside, in game pixels (320 x 240)

A rule with `frames = false` never matches a frame; it names state-hash
pages (`pages`, symbol prefixes) only. Frames whose two sides are in
different scenes match nothing: they are different moments, unexplained.
"""
import os, tomllib


class Known:
    def __init__(self, path, sides=()):
        with open(path, 'rb') as fh:
            self.doc = tomllib.load(fh)
        self.rules = [dict(r, _caches=_Caches()) for r in self.doc.get('known', [])]
        for r in self.rules:
            for k in ('id', 'ledger', 'what'):
                if k not in r:
                    raise SystemExit(f'{path}: a [[known]] entry without `{k}`')


class _Caches:
    """manifest.toml chunk hashes per cache, read once."""
    memo = {}

    def chunks(self, cache):
        if not cache:
            return None
        if cache in self.memo:
            return self.memo[cache]
        p = os.path.join(cache, 'manifest.toml')
        rows = None
        if os.path.exists(p):
            with open(p, 'rb') as fh:
                doc = tomllib.load(fh)
            rows = {}
            for c, slot, layer, _src, h in doc.get('cache', {}).get('assets', []):
                rows.setdefault(c.upper(), {})[(slot, layer)] = h
        self.memo[cache] = rows
        return rows


def _area_differs(rule, area, p, q, layer_kind):
    ca, cb = p.meta.get('cache'), q.meta.get('cache')
    ma, mb = rule['_caches'].chunks(ca), rule['_caches'].chunks(cb)
    if ma is None or mb is None:
        return False          # no manifest to read: not shown, so not explained
    name = f'AREA{area:03d}.DAT'
    ra, rb = ma.get(name, {}), mb.get(name, {})
    if layer_kind == 'base':
        la = lb = 'base'
    else:
        la, lb = 'loc/' + p.meta.get('lang', ''), 'loc/' + q.meta.get('lang', '')
    a = {s: h for (s, l), h in ra.items() if l == la}
    b = {s: h for (s, l), h in rb.items() if l == lb}
    return bool(a or b) and a != b


def match_region(rules, ctx, region):
    """The id of the first rule that explains `region` in this frame pair,
    else None."""
    p, q = ctx['sides']
    sa, sb = ctx['scene']
    for r in rules:
        if r.get('frames', True) is False:
            continue
        t = r.get('pair_trait')
        if t and ((t in p.traits) == (t in q.traits)):
            continue
        if any(x not in p.traits or x not in q.traits for x in r.get('both_traits', [])):
            continue
        if 'routes' in r and ctx['route'] not in r['routes']:
            continue
        if 'ticks' in r:
            lo, hi = r['ticks']
            if ctx['tick'] is None or not lo <= ctx['tick'] <= hi:
                continue
        needs_scene = any(k in r for k in ('modes', 'areas', 'area_data'))
        if needs_scene:
            if not sa or sa != sb:
                continue
            mode, area = sa
            if 'modes' in r and mode not in r['modes']:
                continue
            if 'areas' in r and area not in r['areas']:
                continue
            if 'area_data' in r and not _area_differs(r, area, p, q, r['area_data']):
                continue
        if 'box' in r:
            x0, y0, x1, y1 = r['box']
            b = region['box']
            if not (b[0] >= x0 and b[1] >= y0 and b[2] <= x1 and b[3] <= y1):
                continue
        return r['id']
    return None


def match_page(rules, p, q, symbols):
    """The id of the first state-hash rule whose `pages` prefixes name one of
    `symbols` (the page's, as statehash.py diff prints them), for this pair."""
    for r in rules:
        pages = r.get('pages')
        if not pages:
            continue
        t = r.get('pair_trait')
        if t and ((t in p.traits) == (t in q.traits)):
            continue
        if any(x not in p.traits or x not in q.traits for x in r.get('both_traits', [])):
            continue
        if any(s.startswith(pre) for s in symbols for pre in pages):
            return r['id']
    return None
