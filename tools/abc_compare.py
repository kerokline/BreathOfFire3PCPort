#!/usr/bin/env python
"""The picture half of the A/B/C install harness (tools/abc_run.py,
docs/abc-harness.md): frames matched by tick across two or more runs, a
difference measure and a heat map per frame and pair, the regions that
differ, the known differences they match, a contact sheet and an HTML index.

    python tools/abc_compare.py --side A=DIR --side B=DIR [--side C=DIR] --out DIR
        [--known recipes/abc-known.toml] [--route NAME] [--every-sheet N]

A side is a directory of frames (`tNNNNNN.png`, the game's own timed frames,
BOF3X_SHOT_EVERY; or a recipe's `NAME.png` shots) with, optionally, its
`bof3x.log` beside them (the scene per frame: mode and area) and the
`meta.json` abc_run.py writes (the install's traits and cache, which the
known-difference rules read). abc_run.py calls compare() directly; this
command line exists for frames from earlier runs.

Nothing here reads game data except the frames and, for the known rules,
the caches' manifest hashes; nothing is written outside --out.
"""
import argparse, html, json, os, re, sys, time

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import abc_known  # noqa: E402

# Defaults, chosen on the frames of analysis/shots (docs/abc-harness.md 4):
TOL = 4          # a pixel differs when a channel moves by more than this (0..255)
MIN_PIXELS = 16  # a frame differs when more pixels than this differ
CELL = 4         # region grid cell, in game pixels (320 x 240 frame)
GAME_W = 320     # the PlayStation's frame width: regions are given in its pixels

FRAME_LOG = re.compile(r'input\s+frame (t\d+) (?:saved|NOT saved) mode (\d+) area (\d+)')
SHOT_LOG = re.compile(r'input\s+shot (\S+) recipe frame (\d+)')
TNAME = re.compile(r'^t(\d+)$')
DIGITS = re.compile(r'(\d+)')


class Side:
    def __init__(self, key, path):
        self.key, self.path = key, path
        self.frames = {}          # name -> png path
        for f in sorted(os.listdir(path)):
            stem, ext = os.path.splitext(f)
            if ext.lower() == '.png' and not stem.startswith('_'):
                self.frames[stem] = os.path.join(path, f)
        self.scene = {}           # name -> (mode, area)
        self.shot_tick = {}       # shot name -> recipe frame
        log = os.path.join(path, 'bof3x.log')
        if os.path.exists(log):
            with open(log, encoding='utf-8', errors='replace') as fh:
                for line in fh:
                    if m := FRAME_LOG.search(line):
                        self.scene[m[1]] = (int(m[2]), int(m[3]))
                    elif m := SHOT_LOG.search(line):
                        self.shot_tick.setdefault(m[1], int(m[2]))
        self.meta = {}
        mp = os.path.join(path, 'meta.json')
        if os.path.exists(mp):
            with open(mp, encoding='utf-8') as fh:
                self.meta = json.load(fh)
        self.name = self.meta.get('install', key)
        self.traits = set(self.meta.get('traits', []))

    def tick(self, name):
        if m := TNAME.match(name):
            return int(m[1])
        if name in self.shot_tick:
            return self.shot_tick[name]
        m = DIGITS.findall(name)
        return int(m[-1]) if m else None


def load(path):
    with Image.open(path) as im:
        return np.asarray(im.convert('RGB'))


def fit(a, b):
    """b at a's size (nearest), for a pair whose sizes differ: they are
    compared anyway, and the size difference is a finding of its own."""
    if a.shape == b.shape:
        return b
    return np.asarray(Image.fromarray(b).resize((a.shape[1], a.shape[0]), Image.NEAREST))


def regions(mask, scale, cell=CELL):
    """Connected areas of differing pixels, on a grid of `cell` game pixels,
    8-connected after a one-cell dilation (so a line of text is one region).
    Each: (x0, y0, x1, y1) in game pixels, and the differing pixel count."""
    h, w = mask.shape
    cp = max(1, int(round(cell * scale)))
    gh, gw = (h + cp - 1) // cp, (w + cp - 1) // cp
    pad = np.zeros((gh * cp, gw * cp), dtype=np.int32)
    pad[:h, :w] = mask
    counts = pad.reshape(gh, cp, gw, cp).sum(axis=(1, 3))
    hot = counts > 0
    grown = hot.copy()
    grown[1:, :] |= hot[:-1, :]; grown[:-1, :] |= hot[1:, :]
    grown[:, 1:] |= hot[:, :-1]; grown[:, :-1] |= hot[:, 1:]
    seen = np.zeros_like(grown)
    out = []
    for y in range(gh):
        for x in range(gw):
            if not grown[y, x] or seen[y, x]:
                continue
            stack, cells = [(y, x)], []
            seen[y, x] = True
            while stack:
                cy, cx = stack.pop()
                cells.append((cy, cx))
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        ny, nx = cy + dy, cx + dx
                        if 0 <= ny < gh and 0 <= nx < gw and grown[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True
                            stack.append((ny, nx))
            real = [(cy, cx) for cy, cx in cells if hot[cy, cx]]
            if not real:
                continue
            ys = [c[0] for c in real]; xs = [c[1] for c in real]
            n = int(sum(counts[cy, cx] for cy, cx in real))
            out.append({'box': [min(xs) * cell, min(ys) * cell, (max(xs) + 1) * cell, (max(ys) + 1) * cell],
                        'pixels': n})
    out.sort(key=lambda r: -r['pixels'])
    return out


def moved(a, b, box, scale, tol=TOL, reach=(16, 8)):
    """Whether a region's difference is the same picture moved: the offset
    (dx, dy), in game pixels within `reach`, at which the second side's
    pixels around the box best match the first's, when that leaves fewer
    than half the differing pixels of no offset; else None. Measured at game
    resolution (every `scale`-th pixel). This is what tells a shifted
    combatant or text line from a changed texture, and a region it finds
    moved is never explained by a data rule."""
    st = max(1, int(round(scale)))
    ga, gb = a[::st, ::st].astype(np.int16), b[::st, ::st].astype(np.int16)
    h, w = ga.shape[:2]
    x0, y0, x1, y1 = box
    x0, y0, x1, y1 = max(0, x0), max(0, y0), min(w, x1), min(h, y1)
    if (x1 - x0) * (y1 - y0) < 16:
        return None
    area = (x1 - x0) * (y1 - y0)

    def miss(dx, dy):
        # the part of the box whose moved copy is still on the frame
        ax0, ay0 = max(x0, -dx), max(y0, -dy)
        ax1, ay1 = min(x1, w - dx), min(y1, h - dy)
        if (ax1 - ax0) * (ay1 - ay0) * 2 < area:
            return None
        pa = ga[ay0:ay1:2, ax0:ax1]       # every second row: half the cost, the same answer for a shift
        pb = gb[ay0 + dy:ay1 + dy:2, ax0 + dx:ax1 + dx]
        return float((np.abs(pa - pb).max(axis=2) > tol).mean())

    base = miss(0, 0)
    if not base:
        return None
    best, at = base, None

    def look(cands):
        nonlocal best, at
        for dx, dy in cands:
            if dx == 0 and dy == 0:
                continue
            m = miss(dx, dy)
            if m is not None and m < best:
                best, at = m, (dx, dy)

    # every offset: a coarse grid misses an odd shift (12 is no better than 0
    # for a picture moved 13), and this is the costly part of a comparison
    look((dx, dy) for dy in range(-reach[1], reach[1] + 1) for dx in range(-reach[0], reach[0] + 1))
    return list(at) if at and best * 2 < base else None


def measure(a, b, tol=TOL):
    """Per-pixel difference (the largest channel's), the differing mask, and
    the numbers: the fraction of pixels differing, the mean absolute
    difference over every channel, the largest."""
    d = np.abs(a.astype(np.int16) - b.astype(np.int16))
    dmax = d.max(axis=2)
    mask = dmax > tol
    return dmax, mask, {'changed': float(mask.mean()), 'pixels': int(mask.sum()),
                        'mad': float(d.mean()), 'max': int(dmax.max())}


def heat(a, dmax, regs, path, scale):
    """A dimmed grey copy of the first side, the difference over it from red
    (small) to yellow (large), each region boxed: magenta unexplained, cyan
    matched to a known difference."""
    grey = a.astype(np.float32).mean(axis=2) * 0.35
    rgb = np.stack([grey, grey, grey], axis=2)
    t = np.clip(dmax.astype(np.float32) / 96.0, 0, 1)
    on = dmax > 0
    rgb[..., 0] = np.where(on, 120 + 135 * np.maximum(t, 0.4), rgb[..., 0])
    rgb[..., 1] = np.where(on, 255 * t * t, rgb[..., 1])
    rgb[..., 2] = np.where(on, 0, rgb[..., 2])
    im = Image.fromarray(rgb.astype(np.uint8))
    dr = ImageDraw.Draw(im)
    for r in regs:
        x0, y0, x1, y1 = (int(v * scale) for v in r['box'])
        dr.rectangle([x0, y0, x1 - 1, y1 - 1], outline=(0, 230, 230) if r.get('known') else (255, 0, 255),
                     width=max(1, int(scale)))
    im.save(path)


def pair_name(p, q):
    return f'{p.key}-{q.key}'


def compare(sides, out, known=None, route='', tol=TOL, min_pixels=MIN_PIXELS, sheet_rows=16, thumb=0.5,
            log=print):
    """Compare the sides frame by frame. Writes OUT/heat/<pair>/<frame>.png,
    OUT/sheet_NN.png, OUT/index.html and OUT/result.json, and returns the
    result dictionary (report.py-style data for abc_run's Markdown)."""
    t0 = time.time()
    os.makedirs(out, exist_ok=True)
    pairs = [(sides[i], sides[j]) for i in range(len(sides)) for j in range(i + 1, len(sides))]
    names = set()
    for s in sides:
        names |= set(s.frames)
    ticks = {}
    for n in names:
        t = next((s.tick(n) for s in sides if s.tick(n) is not None), None)
        ticks[n] = t if t is not None else 10 ** 9
    order = sorted(names, key=lambda n: (ticks[n], n))
    rules = known.rules if known else []
    rows = []
    for n in order:
        row = {'frame': n, 'tick': ticks[n] if ticks[n] < 10 ** 9 else None, 'scene': {}, 'pairs': {}}
        imgs = {}
        for s in sides:
            row['scene'][s.key] = s.scene.get(n)
            if n in s.frames:
                imgs[s.key] = load(s.frames[n])
        for p, q in pairs:
            pn = pair_name(p, q)
            if p.key not in imgs or q.key not in imgs:
                miss = [s.key for s in (p, q) if s.key not in imgs]
                row['pairs'][pn] = {'missing': miss}
                continue
            a, b = imgs[p.key], imgs[q.key]
            res = {}
            if a.shape != b.shape:
                res['size'] = [list(a.shape[1::-1]), list(b.shape[1::-1])]
            b2 = fit(a, b)
            dmax, mask, num = measure(a, b2, tol)
            res.update(num)
            scale = a.shape[1] / GAME_W
            res['differs'] = num['pixels'] > min_pixels or 'size' in res
            if res['differs']:
                regs = regions(mask, scale)
                ctx = {'route': route, 'tick': row['tick'], 'frame': n,
                       'scene': (p.scene.get(n), q.scene.get(n)), 'sides': (p, q)}
                for k, r in enumerate(regs):
                    if k < 3 and r['pixels'] >= 32:
                        r['moved'] = moved(a, b2, r['box'], scale, tol)
                    r['known'] = None if r.get('moved') else abc_known.match_region(rules, ctx, r)
                res['regions'] = regs
                res['unexplained'] = [r for r in regs if not r['known']]
                hd = os.path.join(out, 'heat', pn)
                os.makedirs(hd, exist_ok=True)
                res['heat'] = os.path.relpath(os.path.join(hd, n + '.png'), out)
                heat(a, dmax, regs, os.path.join(out, res['heat']), scale)
            row['pairs'][pn] = res
        rows.append(row)
    sheets = contact_sheets(sides, pairs, rows, out, sheet_rows, thumb)
    result = {'route': route, 'sides': [{'key': s.key, 'install': s.name, 'path': s.path,
                                          'frames': len(s.frames), 'traits': sorted(s.traits)} for s in sides],
              'pairs': [pair_name(p, q) for p, q in pairs], 'tol': tol, 'min_pixels': min_pixels,
              'rows': rows, 'sheets': sheets, 'seconds': round(time.time() - t0, 1)}
    result['summary'] = summarise(result, pairs)
    with open(os.path.join(out, 'result.json'), 'w', encoding='utf-8') as fh:
        json.dump(result, fh, indent=1, default=str)
    index_html(result, out)
    log(f'compare: {len(rows)} frames, {len(pairs)} pairs, {len(sheets)} sheets, {result["seconds"]} s -> {out}')
    return result


def summarise(result, pairs):
    """Per pair: frames compared, frames differing, the first differing tick,
    the first with an unexplained region, the first where the scene (mode,
    area) parts - after it the frames are of different moments."""
    out = {}
    for p, q in pairs:
        pn = pair_name(p, q)
        s = {'compared': 0, 'differ': 0, 'unexplained': 0, 'missing': 0, 'first_diff': None,
             'first_unexplained': None, 'first_scene_split': None, 'sizes': 0}
        for row in result['rows']:
            r = row['pairs'].get(pn, {})
            if 'missing' in r:
                s['missing'] += 1
                continue
            s['compared'] += 1
            sa, sb = row['scene'].get(p.key), row['scene'].get(q.key)
            if sa and sb and sa != sb and s['first_scene_split'] is None:
                s['first_scene_split'] = row['tick']
            if 'size' in r:
                s['sizes'] += 1
            if r.get('differs'):
                s['differ'] += 1
                if s['first_diff'] is None:
                    s['first_diff'] = row['tick'] if row['tick'] is not None else row['frame']
                if r.get('unexplained'):
                    s['unexplained'] += 1
                    if s['first_unexplained'] is None:
                        s['first_unexplained'] = row['tick'] if row['tick'] is not None else row['frame']
        out[pn] = s
    return out


def font(size):
    for f in ('consola.ttf', 'arial.ttf', 'DejaVuSans.ttf'):
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            pass
    return ImageFont.load_default()


def contact_sheets(sides, pairs, rows, out, per, thumb):
    """One row per frame: each side, then each pair's heat map (blank when the
    pair is equal), a label column with the tick, scene and percentages.
    Paged `per` rows to a sheet."""
    if not rows:
        return []
    w0 = h0 = None
    for s in sides:
        for p in s.frames.values():
            with Image.open(p) as im:
                w0, h0 = im.size
            break
        if w0:
            break
    tw, th = int(w0 * thumb), int(h0 * thumb)
    label_w, head_h = 190, 26
    cols = [s.key for s in sides] + ['d' + pair_name(p, q) for p, q in pairs]
    f, fs = font(14), font(12)
    sheets = []
    for page in range(0, len(rows), per):
        chunk = rows[page:page + per]
        sheet = Image.new('RGB', (label_w + len(cols) * (tw + 4), head_h + len(chunk) * (th + 4)), (24, 24, 28))
        dr = ImageDraw.Draw(sheet)
        for c, name in enumerate(cols):
            side = next((s for s in sides if s.key == name), None)
            text = f'{name} {side.name}' if side else name
            dr.text((label_w + c * (tw + 4) + 4, 5), text[:40], fill=(230, 230, 230), font=f)
        for r, row in enumerate(chunk):
            y = head_h + r * (th + 4)
            lines = [row['frame'] + (f'  tick {row["tick"]}' if row['tick'] is not None and
                                     not row['frame'].startswith('t') else '')]
            sc = {k: v for k, v in row['scene'].items() if v}
            if sc:
                vals = set(sc.values())
                if len(vals) == 1:
                    m, a = vals.pop()
                    lines.append(f'mode {m} area {a}')
                else:
                    lines += [f'{k}: mode {v[0]} area {v[1]}' for k, v in sc.items()]
            for p, q in pairs:
                pr = row['pairs'].get(pair_name(p, q), {})
                if 'missing' in pr:
                    lines.append(f'{pair_name(p, q)}: missing {",".join(pr["missing"])}')
                else:
                    tag = '' if not pr.get('differs') else (' UNEXPL' if pr.get('unexplained') else ' known')
                    lines.append(f'{pair_name(p, q)}: {100 * pr.get("changed", 0):.2f}%{tag}')
            for i, t in enumerate(lines):
                col = (255, 120, 255) if 'UNEXPL' in t else (220, 220, 220)
                dr.text((6, y + 4 + 16 * i), t, fill=col, font=fs)
            for c, name in enumerate(cols):
                x = label_w + c * (tw + 4)
                src = None
                side = next((s for s in sides if s.key == name), None)
                if side:
                    src = side.frames.get(row['frame'])
                else:
                    pr = row['pairs'].get(name[1:], {})
                    if pr.get('heat'):
                        src = os.path.join(out, pr['heat'])
                    elif 'changed' in pr:
                        dr.text((x + 8, y + th // 2 - 8), 'equal' if not pr.get('differs') else '', fill=(120, 200, 120), font=f)
                if src:
                    with Image.open(src) as im:
                        sheet.paste(im.convert('RGB').resize((tw, th), Image.BILINEAR), (x, y))
        name = f'sheet_{page // per + 1:02d}.png'
        sheet.save(os.path.join(out, name))
        sheets.append(name)
    return sheets


def index_html(result, out):
    """A small page: the summary per pair, the sheets, and a table of every
    frame that differs with links to its heat maps."""
    e = html.escape
    rows = []
    for row in result['rows']:
        cells = []
        any_diff = False
        for pn in result['pairs']:
            pr = row['pairs'].get(pn, {})
            if 'missing' in pr:
                cells.append(f'<td class=miss>missing {e(",".join(pr["missing"]))}</td>')
            elif pr.get('differs'):
                any_diff = True
                cls = 'un' if pr.get('unexplained') else 'kn'
                cells.append(f'<td class={cls}><a href="{e(pr["heat"])}">{100 * pr["changed"]:.2f}%</a>'
                             f' {len(pr.get("unexplained", []))}/{len(pr.get("regions", []))}</td>')
            else:
                cells.append('<td class=eq>=</td>')
        if not any_diff and not any('missing' in row['pairs'].get(pn, {}) for pn in result['pairs']):
            continue
        sc = '; '.join(f'{k} {v[0]}/{v[1]}' for k, v in row['scene'].items() if v)
        rows.append(f'<tr><td>{e(row["frame"])}</td><td>{e(sc)}</td>{"".join(cells)}</tr>')
    summ = []
    for pn, s in result['summary'].items():
        summ.append(f'<tr><td>{e(pn)}</td>' + ''.join(f'<td>{e(str(s[k]))}</td>' for k in
                    ('compared', 'differ', 'unexplained', 'missing', 'first_diff', 'first_unexplained',
                     'first_scene_split')) + '</tr>')
    sides = ''.join(f'<li>{e(s["key"])}: {e(s["install"])} ({s["frames"]} frames, {e(s["path"])})</li>'
                    for s in result['sides'])
    sheets = ''.join(f'<p><a href="{e(s)}">{e(s)}</a></p>' for s in result['sheets'])
    page = f'''<!doctype html><meta charset=utf-8><title>A/B/C {e(result["route"])}</title>
<style>body{{font:14px system-ui,sans-serif;background:#16161a;color:#ddd;margin:16px}}a{{color:#8cf}}
table{{border-collapse:collapse}}td,th{{border:1px solid #444;padding:2px 6px}}.un{{background:#502050}}
.kn{{background:#204848}}.eq{{color:#6a6}}.miss{{background:#553}}</style>
<h1>A/B/C: {e(result["route"])}</h1><ul>{sides}</ul>
<p>A pixel differs when a channel moves by more than {result["tol"]}; a frame differs past {result["min_pixels"]} pixels.
Cells: the % of pixels differing, then unexplained / all regions. Magenta: an unexplained region; teal: every region
matched a known difference (recipes/abc-known.toml).</p>
<table><tr><th>pair</th><th>compared</th><th>differ</th><th>unexplained</th><th>missing</th><th>first diff</th>
<th>first unexplained</th><th>scene parts</th></tr>{"".join(summ)}</table>
<h2>Contact sheets</h2>{sheets}
<h2>Frames that differ</h2><table><tr><th>frame</th><th>scene (mode/area)</th>{"".join(f"<th>{e(p)}</th>" for p in result["pairs"])}</tr>
{"".join(rows)}</table>'''
    with open(os.path.join(out, 'index.html'), 'w', encoding='utf-8') as fh:
        fh.write(page)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--side', action='append', required=True, metavar='KEY=DIR')
    ap.add_argument('--out', required=True)
    ap.add_argument('--known', default=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                                    'recipes', 'abc-known.toml'))
    ap.add_argument('--route', default='')
    ap.add_argument('--tol', type=int, default=TOL)
    ap.add_argument('--min-pixels', type=int, default=MIN_PIXELS)
    ap.add_argument('--sheet-rows', type=int, default=16)
    a = ap.parse_args()
    sides = []
    for kv in a.side:
        k, _, d = kv.partition('=')
        if not os.path.isdir(d):
            sys.exit(f'no directory {d}')
        sides.append(Side(k, d))
    known = abc_known.Known(a.known, sides) if a.known and os.path.exists(a.known) else None
    compare(sides, a.out, known, a.route, a.tol, a.min_pixels, a.sheet_rows)


if __name__ == '__main__':
    main()
