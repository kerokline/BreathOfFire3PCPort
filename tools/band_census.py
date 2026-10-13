"""Sum a BOF3X_BANDLOG file into a per-scene table (docs/band-census.md).

    python tools/band_census.py LOG [--shots DIR] [--rows] [--min-frames N]
    python tools/band_census.py --self-test

The log (src/game/band_census.cpp) has one row per object per stretch of
frames in which the object's sprite lay inside the wide picture but wholly
outside the original's 320 columns, with nothing about it changing: area,
chapter, object, side, band, parked, scene. The game cuts a row every
BOF3X_BANDLOG_SPAN frames (60 by default, so that a run ended by taskkill
loses little); this tool first joins the pieces again, then groups the rows
of one area whose frames overlap or nearly touch into a "scene" - a stretch
of play with something in the bands - and prints, for each, the frames, the
objects, the class, and with --shots the shot nearest its middle.

Classes, worst first:
  parked-scene   an object parked (still, its movement script at an end or a
                 wait) while the leader's control is taken - the owner's case
                 of 2026-10-13, actors left standing just off the 320 view
  still-scene    still in a scene, no movement script read (a party member,
                 the kind-2 object, an extra object)
  parked         parked with the player in control
  still          still, no script read, the player in control
  passing        moving: an entrance or an exit seen early
The band column says whether the 16:9 mode shows it (16:9) or only 21:9.

Shots: files in DIR named t<frame>.bmp (BOF3X_SHOT_EVERY) or f<frame>.png
(tools/recipe_shots.py), numbered by the recipe frame - the log's rfirst /
rlast columns, which are 0 when no recipe played.
"""
import argparse
import os
import re
import sys

COLUMNS = ['area', 'chapter', 'mode', 'who', 'index', 'type', 'sprite', 'first', 'last', 'seen', 'rfirst', 'rlast',
           'xlo', 'xhi', 'ylo', 'yhi', 'side', 'band', 'parked', 'scene', 'mask', 'script']
INTS = {'area', 'chapter', 'mode', 'index', 'type', 'first', 'last', 'seen', 'rfirst', 'rlast', 'scene'}
FLOATS = {'xlo', 'xhi', 'ylo', 'yhi'}
KEY = ('area', 'chapter', 'mode', 'who', 'index', 'type', 'sprite', 'side', 'band', 'parked', 'scene')
JOIN_GAP = 3        # a row continues the one before when it starts within this many frames of its end
SCENE_GAP = 60      # rows of one area this close in frames belong to one scene
ORDER = ['parked-scene', 'still-scene', 'parked', 'still', 'passing']


def parse(lines):
    head, rows = {}, []
    cols = COLUMNS
    for line in lines:
        line = line.rstrip('\n')
        if not line:
            continue
        if line.startswith('#'):
            t = line[1:].strip().split('\t')
            if t and t[0].startswith('bandlog'):
                for item in t[1:]:
                    k, _, v = item.partition(' ')
                    head[k] = v
            elif t and t[0] == 'area':
                cols = t
            elif t and t[0].startswith('end:'):
                head['end'] = t[0][4:].strip()
            continue
        f = line.split('\t')
        if len(f) != len(cols):
            raise SystemExit(f'band_census: a row of {len(f)} fields, the header has {len(cols)}: {line!r}')
        r = {}
        for k, v in zip(cols, f):
            r[k] = int(v, 16) if k == 'mask' else int(v) if k in INTS else float(v) if k in FLOATS else v
        rows.append(r)
    return head, rows


def join(rows):
    """The game's span cuts joined again: same key, frames continuing."""
    out, open_ = [], {}
    for r in sorted(rows, key=lambda r: (r['first'], r['last'])):
        k = tuple(r[c] for c in KEY)
        p = open_.get(k)
        if p is not None and r['first'] <= p['last'] + JOIN_GAP:
            p['last'] = max(p['last'], r['last'])
            p['seen'] += r['seen']
            p['rlast'] = max(p['rlast'], r['rlast'])
            p['xlo'], p['xhi'] = min(p['xlo'], r['xlo']), max(p['xhi'], r['xhi'])
            p['ylo'], p['yhi'] = min(p['ylo'], r['ylo']), max(p['yhi'], r['yhi'])
            p['mask'] |= r['mask']
            p['script'] = r['script']
            continue
        p = dict(r)
        open_[k] = p
        out.append(p)
    return out


def cls(r):
    scene = '-scene' if r['scene'] else ''
    if r['parked'] == 'yes':
        return 'parked' + scene
    if r['parked'] == 'still':
        return 'still' + scene
    return 'passing'


def scenes(rows):
    """Rows of one area (and chapter) whose frame ranges overlap or come
    within SCENE_GAP of each other, as one scene each."""
    out = []
    by_area = {}
    for r in rows:
        by_area.setdefault((r['area'], r['chapter']), []).append(r)
    for (area, chapter), rs in by_area.items():
        rs.sort(key=lambda r: r['first'])
        cur = None
        for r in rs:
            if cur is None or r['first'] > cur['last'] + SCENE_GAP:
                cur = {'area': area, 'chapter': chapter, 'first': r['first'], 'last': r['last'],
                       'rfirst': r['rfirst'], 'rlast': r['rlast'], 'rows': []}
                out.append(cur)
            cur['last'] = max(cur['last'], r['last'])
            cur['rfirst'] = min(cur['rfirst'], r['rfirst'])
            cur['rlast'] = max(cur['rlast'], r['rlast'])
            cur['rows'].append(r)
    out.sort(key=lambda s: s['first'])
    return out


def shots(directory):
    found = []
    for name in os.listdir(directory):
        m = re.fullmatch(r'[ft](\d+)\.(png|bmp)', name, re.IGNORECASE)
        if m:
            found.append((int(m.group(1)), name))
    found.sort()
    return found


def nearest(found, frame):
    if not found:
        return '-'
    return min(found, key=lambda s: (abs(s[0] - frame), s[0]))[1]


def describe(r):
    who = f"{r['who']}{r['index']}" if r['index'] >= 0 else r['who']
    return f"{who} {r['side']} {r['band']} {cls(r)} x {r['xlo']:.0f}..{r['xhi']:.0f} y {r['ylo']:.0f}..{r['yhi']:.0f}"


def table(rows, found, min_frames, show_rows):
    lines = []
    rows = [r for r in rows if r['last'] - r['first'] + 1 >= min_frames]
    for s in scenes(rows):
        worst = min((ORDER.index(cls(r)) for r in s['rows']), default=len(ORDER) - 1)
        band = '21:9 only' if all(r['band'] == '21:9' for r in s['rows']) else '16:9'
        objs = sorted({(r['who'], r['index']) for r in s['rows']})
        mid = (s['rfirst'] + s['rlast']) // 2
        shot = nearest(found, mid) if found is not None and s['rlast'] else '-'
        lines.append(f"area {s['area']:3d}  chapter {s['chapter']:2d}  frames {s['first']}..{s['last']}  "
                     f"recipe {s['rfirst']}..{s['rlast']}  {len(objs)} object(s)  {ORDER[worst]}  {band}  shot {shot}")
        if show_rows:
            for r in s['rows']:
                mid_r = (r['rfirst'] + r['rlast']) // 2
                shot_r = nearest(found, mid_r) if found is not None and r['rlast'] else '-'
                lines.append(f"    {r['first']}..{r['last']} ({r['seen']} seen)  {describe(r)}  "
                             f"script {r['script']}  mask {r['mask']:02X}  shot {shot_r}")
    return lines


def self_test():
    head = '# bandlog 1\tcolumns 120\tpark 30\tspan 60\n# ' + '\t'.join(COLUMNS) + '\n'

    def row(first, last, x=-80.0, parked='yes', scene=1, index=4, area=12, rf=None):
        rf = first + 10 if rf is None else rf
        side, band = ('L', '21:9') if x < -53 else ('L', '16:9') if x < 0 else ('R', '16:9') if x < 373 else ('R', '21:9')
        return '\t'.join(str(v) for v in [area, 2, 3, 'obj', index, 1, '00612340', first, last, last - first + 1, rf,
                                          rf + last - first, x, x + 16, 50.0, 80.0, side, band, parked, scene, '02',
                                          'end']) + '\n'
    text = (head + row(31, 90) + row(91, 150) + row(151, 170) + row(1, 30, parked='no') +
            row(400, 420, x=330.0, parked='no', scene=0, index=6) + row(600, 640, x=400.0, parked='still', index=7, area=13))
    h, rows = parse(text.splitlines())
    bad = []
    if h.get('columns') != '120':
        bad.append(f"header {h}")
    j = join(rows)
    if len(j) != 4:
        bad.append(f'joined {len(j)} rows, want 4')
    long_ = [r for r in j if r['first'] == 31]
    if not long_ or long_[0]['last'] != 170 or long_[0]['seen'] != 140:
        bad.append(f'the span pieces not joined: {long_}')
    sc = scenes(j)
    if [(s['area'], s['first'], s['last'], len(s['rows'])) for s in sc] != [(12, 1, 170, 2), (12, 400, 420, 1),
                                                                            (13, 600, 640, 1)]:
        bad.append(f"scenes {[(s['area'], s['first'], s['last'], len(s['rows'])) for s in sc]}")
    found = [(0, 'f00000.png'), (90, 'f00090.png'), (180, 'f00180.png')]
    if nearest(found, 100) != 'f00090.png' or nearest(found, 135) != 'f00090.png' or nearest(found, 136) != 'f00180.png':
        bad.append('nearest shot')
    lines = table(j, found, 1, False)
    if 'parked-scene' not in lines[0] or 'passing' not in lines[1] or 'still-scene' not in lines[2]:
        bad.append(f'classes {lines}')
    for b in bad:
        print('band_census self-test MISMATCH:', b)
    print(f'band_census self-test: {len(bad)} mismatches')
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('log', nargs='?', help='the BOF3X_BANDLOG file')
    ap.add_argument('--shots', metavar='DIR', help='a run\'s shot directory: name the shot nearest each scene')
    ap.add_argument('--rows', action='store_true', help='list each scene\'s rows under it')
    ap.add_argument('--min-frames', type=int, default=1, metavar='N', help='leave out rows shorter than N frames')
    ap.add_argument('--self-test', action='store_true', help='check the joins, scenes and shot pick on made-up rows')
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    if not a.log:
        ap.error('a log file, or --self-test')
    with open(a.log, encoding='utf-8') as f:
        head, rows = parse(f)
    found = shots(a.shots) if a.shots else None
    rows = join(rows)
    print(f"# {a.log}: columns {head.get('columns', '?')}, parked after {head.get('park', '?')} frames; "
          f"{len(rows)} rows joined" + ('' if 'end' in head else ' (no end line: the run was killed, '
                                        'rows open then are missing)'))
    for line in table(rows, found, a.min_frames, a.rows):
        print(line)
    return 0


if __name__ == '__main__':
    sys.exit(main())
