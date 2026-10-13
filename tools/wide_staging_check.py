#!/usr/bin/env python
"""The wide staging's site table, measured and checked against the executable
(DIV D, docs/wide-staging.md).

    python tools/wide_staging_check.py              # every site: the checks, then the outcome at 53 and 120 columns
    python tools/wide_staging_check.py --measure    # the measured fields of each row (hash, beyond, pps10, top),
                                                    # in the table's own order, for src/game/wide_staging.cpp
    python tools/wide_staging_check.py --self-test  # the rule and the parser on made-up rows

The site table lives in src/game/wide_staging.cpp (kSites, kCaseZero); this
tool parses it, so the code and the check read one table. Per site it:

1. finds the script through the area descriptor (Area_Descriptors 0x667590,
   +0x10 / +0x18, the index) and checks the address the row names, the
   FNV-1a hash of the script's first `span` bytes (and of the placement op's
   bytes, for a start set by a placement op), and that the op at `move` is a
   move (10..4F with steps, or F2), the one at `at0d` an 0D, the one at
   `before` an EB or E6;
2. replays the actor as tools/staging_scan.py does (its Actor, its camera:
   SS's bank focus for the row, the field projection) to the site's point -
   the start before the walk (kIn) or the point the walk ends at (kOut,
   kOutBefore) - and steps outward from it, one step of the walk's direction
   at a time, to the first point wholly outside the wide picture (the anchor
   past -columns - 12 or 320 + columns + 12, SS's half sprite) or off its top
   or bottom (the feet outside rows -8..272, SS's VIS_Y);
3. --measure: prints the row's fields from that - `beyond` (px the point
   already lies past the 320 view's edge), `pps10` (screen px per step
   outward, x10, the largest value for which the rule's step count reaches
   the exact one at 53 and at 120) and `top` (steps to the top or bottom, 0
   for none);
4. the check: the rule's steps (wide_staging.cpp Steps(): ceil((columns + 12
   - beyond) * 10 / pps10), capped by top) at 53 and 120, the point it puts
   the actor at and its band, the new start's map cells, the end point
   unchanged, the extra frames at the actor's speed, and what follows the walk
   in the actor's own script: the clock stepped or set by the actor itself
   (gated: nothing can pass it), the script's end (nothing waits on it), or a
   wait for the clock another script steps (at risk: the row must be `fast`).

Output is derived from the executable (coordinates, addresses, hashes); no
script bytes are printed or written (CLAUDE.md rule 1).
"""
import argparse
import math
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import event_scan as es        # noqa: E402
import movement_scan as ms     # noqa: E402
import staging_scan as ss      # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src', 'game', 'wide_staging.cpp')
HALF_W = 12                   # SS's half sprite (staging_scan.HALF_W); wide_staging.cpp kHalfWidth
WIDTHS = (53, 120)
MAX_STEPS = 80

ROW = re.compile(r'^\s*\{\s*(\d+),\s*(0x[0-9A-Fa-f]+),\s*(\d+),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),'
                 r'\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+|kNone),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+|kNone),'
                 r'\s*(kIn|kOut|kOutBefore),\s*(-?\d+),\s*(\d+),\s*(\d+),\s*(true|false)\s*\},?\s*//\s*(.*)$')


def parse(path=SRC):
    """The rows of kSites, in order."""
    rows, inside = [], False
    for line in open(path, encoding='utf-8'):
        if 'kSites[] = {' in line:
            inside = True
            continue
        if inside and line.strip().startswith('};'):
            break
        if not inside:
            continue
        m = ROW.match(line)
        if not m:
            continue
        g = m.groups()
        num = lambda s: None if s == 'kNone' else int(s, 0)   # noqa: E731
        rows.append(dict(area=int(g[0]), field=int(g[1], 16), index=int(g[2]), script=int(g[3], 16), hash=int(g[4], 16),
                         span=int(g[5], 16), place=int(g[6], 16), at0d=num(g[7]), move=int(g[8], 16), before=num(g[9]),
                         shape=g[10], beyond=int(g[11]), pps10=int(g[12]), top=int(g[13]), fast=g[14] == 'true',
                         note=g[15].strip(), line=line))
    return rows


CZ_ROW = re.compile(r'^\s*\{\s*(\d+),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+)\s*\},')
CZ_OP = re.compile(r'^\s*\{\s*(\d+),\s*(0x[0-9A-Fa-f]+),\s*(-?\d+),\s*(\d+),\s*(\d+),\s*(true|false)\s*\},')


def parse_case_zero(path=SRC):
    """kCaseZeroScripts and kCaseZero."""
    scripts, ops, where = [], [], None
    for line in open(path, encoding='utf-8'):
        if 'kCaseZeroScripts[] = {' in line:
            where = 's'
            continue
        if 'kCaseZero[] = {' in line:
            where = 'o'
            continue
        if where and line.strip().startswith('};'):
            where = None
            continue
        if where == 's' and CZ_ROW.match(line):
            g = CZ_ROW.match(line).groups()
            scripts.append(dict(index=int(g[0]), script=int(g[1], 16), hash=int(g[2], 16), span=int(g[3], 16)))
        elif where == 'o' and CZ_OP.match(line):
            g = CZ_OP.match(line).groups()
            ops.append(dict(index=int(g[0]), at=int(g[1], 16), delta=int(g[2]), dir=int(g[3]), steps=int(g[4]),
                            after=g[5] == 'true'))
    return scripts, ops


def case_zero(cx, measure):
    """E: the scripts' address and hash, each op an F2; the route's points with the rewrite, against SS's camera."""
    bad = 0
    scripts, ops = parse_case_zero()
    print('case zero: %d scripts, %d ops' % (len(scripts), len(ops)))
    for s in scripts:
        r = dict(area=175, field=0x18, index=s['index'])
        p = script_of(cx, r)
        o = cx.img.off(p)
        raw = cx.img.data[o:o + 0x10010]
        h = fnv(raw[:s['span']])
        ok = p == s['script'] and (measure or h == s['hash'])
        print('   +0x18[%d] %#x hash %#010x %s' % (s['index'], p, h, 'ok' if ok else 'PROBLEM (row %#x %#010x)' % (s['script'], s['hash'])))
        bad += 0 if ok else 1
        rows, actors, cams = cx.area(175)
        label = 'party +0x18[%d]@%#x' % (s['index'], p)
        focus, ssrow = focus_for(cx, r, label, None)
        ops_s = ss.reached_ops(cx.img, cx.lmv, p)
        pos = [None, None]
        for at, b in ops_s:
            op = b[0]
            mine = [c for c in ops if c['index'] == s['index'] and c['at'] == at]
            if op == 0x0D and len(b) >= 5:
                nx, nz = b[1] + (0.5 if b[2] else 0), b[3] + (0.5 if b[4] else 0)
                if not (nx == 0 and nz == 0):
                    pos = [nx, nz]
                continue
            mv = move_of(b) if (0x10 <= op <= 0x4F or op == 0xF2) else None
            if not mv or pos[0] is None:
                continue
            if mine:
                c = mine[0]
                if op != 0xF2 or mv[1] + c['delta'] < 1:
                    print('   PROBLEM: +%#x is not an F2 of 4 steps or more' % at)
                    bad += 1
                walk = (mv[0], mv[1] + c['delta'])
                seq = [walk, (c['dir'], c['steps'])] if c['after'] else [(c['dir'], c['steps']), walk]
            else:
                seq = [mv]
            for d, n in seq:
                dx, dz = cx.step_cells(d)
                pos = [pos[0] + dx * n, pos[1] + dz * n]
            if mine and focus:
                q = ss.project(pos[0] - focus[0], pos[1] - focus[1])
                print('   +%#05x rewritten: then at (%.1f, %.1f), sx %.0f sy %.0f%s' % (
                    at, pos[0], pos[1], q[0], q[1], ' (inside the 320 view)' if 12 <= q[0] <= 308 else ''))
    return bad


def steps_rule(beyond, pps10, top, c):
    """wide_staging.cpp Steps(), the same arithmetic."""
    if c == 0:
        return 0
    need = c + HALF_W - beyond
    h = 0 if need <= 0 else (need * 10 + pps10 - 1) // pps10
    if top and top < h:
        h = top
    return h


def fnv(data, h=0x811C9DC5):
    for b in data:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


class Ctx:
    def __init__(self, exe):
        self.img = es.Image(exe)
        self.lev = self.img.bytes(es.OP_LENGTHS, 16)
        self.speeds = self.img.bytes(es.FIELD_MOVE_SPEEDS, 256)
        self.lmv = self.img.bytes(ms.OP_LENGTHS, 256)
        self.steps = [struct.unpack_from('<ii', self.img.bytes(ss.F9_STEPS + 8 * d, 8)) for d in range(8)]
        self.sites = ss.bank_sites(ROOT)
        self.cache = {}

    def area(self, a):
        if a not in self.cache:
            self.cache[a] = ss.scan_area(self.img, self.lev, self.lmv, self.speeds, self.steps, a, self.sites)
        return self.cache[a]

    def step_cells(self, d):
        sx, sz = self.steps[d & 7]
        f = 2 if (d & 7) in (2, 6) else 1
        return sx * 16 * f / 65536, sz * 16 * f / 65536


def script_of(cx, r):
    d = cx.img.u32(es.AREA_TABLE + 4 * r['area'])
    arr = cx.img.u32(d + r['field'])
    return cx.img.u32(arr + 4 * r['index'])


def move_of(op_bytes):
    """(direction, steps) of a move op, or None."""
    op = op_bytes[0]
    if 0x10 <= op <= 0x4F:
        return (op - 0x10) >> 3, op & 7
    if op == 0xF2:
        return op_bytes[1] & 7, op_bytes[2]
    return None


def replay(cx, ops, start, upto):
    """Positions before and after the op at `upto`, replaying moves and places in address order."""
    pos = list(start) if start else [None, None]
    for at, b in ops:
        before = tuple(pos)
        op = b[0]
        mv = move_of(b) if (0x10 <= op <= 0x4F or op == 0xF2) else None
        if mv is None and op in (0xF3, 0xFA) and len(b) >= 3:
            mv = (b[1] & 7, b[2])
        if mv and pos[0] is not None:
            dx, dz = cx.step_cells(mv[0])
            pos = [pos[0] + dx * mv[1], pos[1] + dz * mv[1]]
        elif op == 0x0D and len(b) >= 5:
            nx, nz = b[1] + (0.5 if b[2] else 0), b[3] + (0.5 if b[4] else 0)
            if not (nx == 0 and nz == 0):
                pos = [nx, nz]
        if at == upto:
            return before, tuple(pos)
    return None, None


RISK_GAP = 3   # a wait this few clock values past the walk's is one the clock can pass while the actor walks on


def after_walk(ops, at):
    """What follows the op at `at` in the actor's own script, in address order: 'gated' (it steps or sets
    the clock itself first), 'end' (its script ends or it is removed first), 'waits' (for a clock value
    another script steps; 'at risk' when that value is RISK_GAP or fewer past the clock at the walk)."""
    k = -1
    for a, b in ops:
        op = b[0]
        clock_op = 0xA0 <= op <= 0xAF and ((op >> 2) & 3) == 0
        if a <= at:
            if clock_op:
                t = op & 3
                k = b[1] if t in (0, 1) else (k + 1 if k >= 0 else k)
            continue
        if clock_op:
            t = op & 3
            if t in (0, 2):
                return 'gated (the actor %s the clock next, +%#x)' % ('sets' if t == 0 else 'steps', a)
            if t == 1:
                gap = b[1] - k if k >= 0 else 0
                kind = 'at risk' if gap <= RISK_GAP else 'waits'
                return '%s (it waits for clock %#x next, +%#x; the walk is at clock %s, %d values before)' % (
                    kind, b[1], a, '%#x' % k if k >= 0 else 'start', gap)
        if op in (0xE6, 0xFF, 0xEF, 0xD8, 0x86, 0x0B):
            return 'end (op %02X at +%#x: nothing in its script waits after the walk)' % (op, a)
        if op == 0x01:
            return 'branch (+%#x): read by hand' % a
    return 'end of the reachable ops'


def focus_for(cx, r, label, point):
    """SS's camera for the row: rebuild the focus from the scan's own row for this actor."""
    rows, actors, cams = cx.area(r['area'])
    cand = [x for x in rows if x['site'] == label]
    if not cand:
        return None, None
    best = cand[0]
    if point is not None and point[0] is not None:
        # the row whose rest is the site's point, when the actor has several
        def dist(x):
            rx, rz = map(float, x['rest'].strip('()').split(','))
            return math.hypot(rx - point[0], rz - point[1])
        best = min(cand, key=dist)
    rx, rz = map(float, best['rest'].strip('()').split(','))
    sx, sy = float(best['sx']), float(best['sy'])
    dx, dz = 0.0, 0.0
    for _ in range(60):
        p = ss.project(dx, dz)
        e = (p[0] - sx, p[1] - sy)
        h = 1e-3
        px, pz = ss.project(dx + h, dz), ss.project(dx, dz + h)
        a, b2, c, d = (px[0] - p[0]) / h, (pz[0] - p[0]) / h, (px[1] - p[1]) / h, (pz[1] - p[1]) / h
        det = a * d - b2 * c
        dx -= (d * e[0] - b2 * e[1]) / det
        dz -= (-c * e[0] + a * e[1]) / det
    return (rx - dx, rz - dz), best


def outside(sx, sy, c):
    """Wholly outside the wide picture: past its side by the half sprite, or the feet above row -8 (SS's
    VIS_Y: the sprite is drawn up from its feet). Not the bottom: a sprite's head shows above feet some
    way below row 240, by a height not read here."""
    return sx <= -c - HALF_W or sx >= 320 + c + HALF_W or sy < ss.VIS_Y[0]


def label_of(r, actor_place):
    if r['field'] == 0x10:
        return actor_place
    return 'party +0x18[%d]@%#x' % (r['index'], r['script'])


def site(cx, r, measure):
    out = []
    p = script_of(cx, r)
    o = cx.img.off(p)
    raw = cx.img.data[o:o + 0x10010]
    problems = []
    if p != r['script']:
        problems.append('the descriptor names %#x, the row %#x' % (p, r['script']))
    h = fnv(raw[:r['span']])
    place_label = None
    if r['place']:
        nib = cx.img.data[cx.img.off(r['place'])] >> 4
        n = cx.lev[nib]
        op = cx.img.bytes(r['place'], n)
        h = fnv(op, h)
        sc = ss.LAYOUT[nib][4] if nib in ss.LAYOUT else None
        if sc is None or op[sc] != r['index']:
            problems.append('the placement op at %#x does not name script %d' % (r['place'], r['index']))
        place_label = 'obj %Xx@%#x +0x10[%d]' % (nib, r['place'], r['index'])
    mv = move_of(raw[r['move']:r['move'] + 3])
    if mv is None or mv[1] == 0:
        problems.append('no move with steps at +%#x' % r['move'])
    if r['at0d'] is not None and raw[r['at0d']] != 0x0D:
        problems.append('no 0D at +%#x' % r['at0d'])
    if r['before'] is not None and raw[r['before']] not in (0xEB, 0xE6):
        problems.append('no EB / E6 at +%#x' % r['before'])
    if r['shape'] == 'kIn' and not (r['place'] or r['at0d'] is not None):
        problems.append('kIn without a start (place or at0d)')
    if r['fast'] and r['shape'] != 'kIn':
        problems.append('fast on a kOut row (wide_staging.cpp refuses it)')
    if not measure and h != r['hash']:
        problems.append('hash %#010x, the row %#010x' % (h, r['hash']))
    if problems or mv is None:
        return {'hash': h, 'problems': problems}

    rows, actors, cams = cx.area(r['area'])
    ops = ss.reached_ops(cx.img, cx.lmv, p)
    label = label_of(r, place_label)
    acs = [a for a in actors if a.label == label] if label else []
    if r['field'] == 0x10 and not r['place']:
        # every object the area places with this script: the walk changes for each of them
        acs = [a for a in actors if a.label.endswith('+0x10[%d]' % r['index'])]
    if not acs:
        problems.append('no actor %s in the scan' % label)
        return {'hash': h, 'problems': problems}
    d, n = mv
    dx, dz = cx.step_cells(d)
    res = {'hash': h, 'problems': problems, 'instances': []}
    for ac in acs:
        before, after = replay(cx, ops, ac.start if r['field'] == 0x10 else None, r['move'])
        if before is None or before[0] is None:
            problems.append('%s: the walk at +%#x is not reached from a known point' % (ac.label, r['move']))
            continue
        if r['shape'] == 'kIn':
            pt, sign = before, -1
        else:
            pt, sign = after, 1
        focus, ssrow = focus_for(cx, r, ac.label, pt)
        if focus is None:
            problems.append('%s: no SS row for the camera' % ac.label)
            continue
        track = []
        for k in range(MAX_STEPS + 1):
            q = ss.project(pt[0] + sign * k * dx - focus[0], pt[1] + sign * k * dz - focus[1])
            track.append(q)
        sx0, sy0 = track[0]
        right = sx0 >= 160
        beyond = round(sx0 - 320) if right else round(-sx0)
        exact_h, exact_any = {}, {}
        for c in WIDTHS:
            exact_h[c] = next((k for k, q in enumerate(track) if q and ((q[0] >= 320 + c + HALF_W) if right else (q[0] <= -c - HALF_W))), None)
            exact_any[c] = next((k for k, q in enumerate(track) if q and outside(q[0], q[1], c)), None)
        top = next((k for k, q in enumerate(track) if q and q[1] < ss.VIS_Y[0]), 0) or 0
        # The largest pps10 whose rule still reaches the count needed at each width - the side's exact
        # count, or `top` when that comes sooner (the rule takes the smaller of the two):
        # ceil(X * 10 / p) >= e holds for every p < X * 10 / (e - 1).
        pps10 = None
        for c in WIDTHS:
            e, x = exact_h[c], c + HALF_W - beyond
            if top and (not e or top < e):
                e = top
            if not e or x <= 0:
                continue
            v = x * 10 if e == 1 else -(-x * 10 // (e - 1)) - 1
            pps10 = v if pps10 is None else min(pps10, v)
        if pps10 is None:
            pps10 = max(1, min((c + HALF_W - beyond) * 10 for c in WIDTHS))
        pps10 = max(1, min(pps10, 999))
        inst = {'label': ac.label, 'point': pt, 'sx': sx0, 'sy': sy0, 'beyond': beyond, 'pps10': pps10, 'top': top,
                'exact': exact_any, 'side': 'R' if right else 'L', 'ssrow': ssrow}
        use = (beyond, pps10, top) if measure else (r['beyond'], r['pps10'], r['top'])
        for c in WIDTHS:
            k = steps_rule(use[0], use[1], use[2], c)
            q = track[k] if k <= MAX_STEPS else None
            new = (pt[0] + sign * k * dx, pt[1] + sign * k * dz)
            inst[c] = {'steps': k, 'sx': q[0] if q else None, 'sy': q[1] if q else None,
                       'out': bool(q) and outside(q[0], q[1], c), 'cells': new}
        # end point: the rewritten walk ends where the original's did (kIn: start - k + n + k)
        inst['speed'] = None
        if r['field'] == 0x10:
            for (a, nib, x, z, scr, speed, hidden, path, chap) in ss.placements(cx.img, cx.lev, cx.speeds, r['area']):
                if ac.label.startswith('obj %Xx@%#x ' % (nib, a)):
                    inst['speed'] = cx.speeds[speed] if speed < 256 else None
        inst['after'] = after_walk(ops, r['before'] if r['before'] is not None else r['move'])
        res['instances'].append(inst)
    return res


def frames(k, d, speed):
    if not speed:
        return None
    return k * ((0x20 if (d & 7) in (2, 6) else 0x10) // speed)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=es.EXE)
    ap.add_argument('--measure', action='store_true')
    ap.add_argument('--self-test', action='store_true')
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    rows = parse()
    cx = Ctx(args.exe)
    bad = 0
    print('%d rows in %s' % (len(rows), os.path.relpath(SRC, ROOT)))
    for i, r in enumerate(rows):
        res = site(cx, r, args.measure)
        head = 'row %d  area %d +%#x[%d] %s move +%#x  %s' % (i, r['area'], r['field'], r['index'], r['shape'], r['move'], r['note'])
        print(head)
        for p in res['problems']:
            print('   PROBLEM: %s' % p)
            bad += 1
        for inst in res.get('instances', []):
            print('   %s  point (%.1f, %.1f) sx %.0f sy %.0f, side %s; SS row: %s %s' % (
                inst['label'], inst['point'][0], inst['point'][1], inst['sx'], inst['sy'], inst['side'],
                inst['ssrow']['class'], inst['ssrow']['band']))
            if args.measure:
                print('   measured: hash %#010x beyond %d pps10 %d top %d; exact steps %s' % (
                    res['hash'], inst['beyond'], inst['pps10'], inst['top'], inst['exact']))
            raw = cx.img.data[cx.img.off(r['script']) + r['move']:cx.img.off(r['script']) + r['move'] + 3]
            d = move_of(raw)[0]
            for c in WIDTHS:
                v = inst[c]
                fr = frames(v['steps'], d, inst['speed'])
                ok = v['out']
                cells = v['cells']
                cells_ok = 1 <= cells[0] < 255 and 1 <= cells[1] < 255
                print('   %3d columns: %2d steps -> sx %s sy %s  %s; cells (%.1f, %.1f)%s; +%s frames%s' % (
                    c, v['steps'], '%.0f' % v['sx'] if v['sx'] is not None else '?', '%.0f' % v['sy'] if v['sy'] is not None else '?',
                    'outside' if ok else 'NOT OUTSIDE', cells[0], cells[1], '' if cells_ok else ' OFF THE MAP RANGE',
                    fr if fr is not None else '?', '' if inst['speed'] else ' (speed: the party member\'s, not read)'))
                if not args.measure and (not ok or not cells_ok):
                    bad += 1
            print('   after the walk: %s%s' % (inst['after'], '' if r['fast'] or not inst['after'].startswith('at risk')
                                             else '  -- AT RISK BUT NOT FAST'))
            if not args.measure and inst['after'].startswith('at risk') and not r['fast']:
                bad += 1
    bad += case_zero(cx, args.measure)
    print('%d problems' % bad)
    return 1 if bad and not args.measure else 0


def self_test():
    bad = 0
    # the rule: 0 columns 0 steps; steps never below what reaching c + 12 needs; capped by top
    for beyond in range(-40, 60, 3):
        for pps10 in (5, 37, 120, 250):
            for c in (0, 53, 120):
                k = steps_rule(beyond, pps10, 0, c)
                if c == 0 and k:
                    bad += 1
                if c and beyond + k * pps10 / 10 < c + HALF_W - 1e-9:
                    bad += 1
                if c and k and beyond + (k - 1) * pps10 / 10 >= c + HALF_W:
                    bad += 1
                if steps_rule(beyond, pps10, 3, c) > 3:
                    bad += 1
    line = '    {175, 0x10, 7, 0x641CBC, 0x12345678, 0x0004, 0x006418BD, kNone, 0x003, kNone, kIn, -17, 72, 4, true},  // a note\n'
    r = ROW.match(line.rstrip('\n'))
    if not r or r.group(11) != 'kIn' or r.group(15) != 'true':
        bad += 1
    print('wide_staging_check self-test: %d mismatches' % bad)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
