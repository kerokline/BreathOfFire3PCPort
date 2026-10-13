#!/usr/bin/env python
"""The ultrawide round's static staging scan: actors a scene parks, walks in,
spawns or ends just outside the original 320-column view.

    python tools/staging_scan.py                      # the table and the summary
    python tools/staging_scan.py --area 175           # one area, every event, with the reasons
    python tools/staging_scan.py --out FILE.tsv       # default analysis/ultrawide/staging_static.tsv

docs/ultrawide-staging.md is the method and the result. In short, four sources,
all read here, none copied:

1. The scenario banks (ours, src/game/scena_*.cpp, area_*.cpp, effect_18*.cpp):
   every literal leader placement (ChangeArea(area, x, z, ..), Field_ChangeArea),
   camera focus (Kind2At(x, z), the kind-2 words set from literals) and party
   placement (PlaceParty(x, z, ..)), with the area each one is for. These are
   where the bank sets the scene's camera: the field view follows the kind-2
   object (docs/kind2-object.md), which an area change or Field_ViewReset puts
   on the point given.
2. The placement scripts (event_scan.py's walk): every object placed with a
   movement script and a non-zero speed, its start point, its "hidden" bit
   (placement flags bit 0 -> sprite +0 bit 0x40, which Sprite_Draw skips) and
   the condition path it was placed under.
3. The movement scripts (movement_scan.py's lengths, labels and flow): each
   script run as a timeline on counter 0 (0x903848, ops A0 set / A1 wait-for /
   A2 step), the scene's shared clock. Moves (10..4F, F2, F3, FA: half a cell
   a step on each axis by MoveCmd_F9Steps, read from the exe), places (0D,
   F7), visibility (EB toggles bit 0x40, E6 ends the object), talk (E0 t, E3 t,
   E1 d t: message t) and ends (FF, EF, D8, 86) are followed; the descriptor
   +0x18 scripts (handed to the party members, each starting with an 0D) and
   +0x1C scripts (the kind-2 object, the camera) the same way.
4. The camera: the bank focus point for the area nearest the scene's speakers,
   moved by the kind-2 script's own steps on the same clock, projected with the
   field's default camera - Gte_SetGeomScreen 1000, offset (160, 120), the
   angles Field_ViewReset sets (0xFD56, 0, 0x200), depth Camera_Distance 0 +
   0x1194 (docs/map-scroll.md, docs/map-layers.md) - Gte_RotMatrix's Rx Ry Rz,
   elevation ignored. Without a bank focus the speakers' centroid stands in and
   the row says "needs census".

Output is derived from the executable (coordinates, script addresses): it goes
to analysis/ (gitignored), never into a committed file (CLAUDE.md rule 1).
"""
import argparse
import collections
import glob
import math
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import event_scan as es        # noqa: E402
import movement_scan as ms     # noqa: E402

F9_STEPS = 0x6696DC            # MoveCmd_F9Steps: (x, z) longs by direction, per unit of speed
VIEW_W, COLS_169, COLS_219 = 320, 53, 120
HALF_W = 12                    # half a field sprite's width, for "wholly outside"
VIS_Y = (-8, 272)              # an anchor (the feet) outside these rows shows nowhere

# Placement ops: x half/whole, z half/whole, script byte, speed byte, flags byte (EventObj_SetFlags).
# Read from the handlers 2026-10-13 (tools/pe_disasm.py on 0x57A010 .. 0x57B530; docs/ultrawide-staging.md 2).
LAYOUT = {
    0x0: (6, 7, 8, 9, 0x10, 0xB, 0xD), 0x1: (6, 7, 8, 9, None, 0xB, 0xD), 0x2: (6, 7, 8, 9, 0x10, 0xB, 0xD),
    0x3: (6, 7, 8, 9, 0x10, 0xB, 0xD), 0x4: (6, 7, 8, 9, None, 0xB, 0xD), 0x5: (5, 6, 7, 8, 0xF, 0xA, 0xC),
    0x6: (4, 5, 6, 7, 0xE, 0x9, 0xB), 0x7: (5, 6, 7, 8, 0xF, 0xA, 0xC), 0x8: (4, 5, 6, 7, None, None, None),
    0x9: (4, 5, 6, 7, None, None, 0xB), 0xA: (1, 2, 3, 4, 0xA, 0x6, 0x8), 0xB: (1, 2, 3, 4, 0xA, 0x6, 0x8),
}
UNMODELLED = {0xC3: 'C3', 0xC4: 'C4', 0xC7: 'C7', 0x84: '84', 0xD4: 'D4', 0xD5: 'D5', 0xD6: 'D6', 0x85: '85'}


# --------------------------------------------------------------------------- the projection
def _rot():
    a = (0xFD56 - 0x10000) * 2 * math.pi / 4096       # Camera_Angles[0], about x
    c = 0x200 * 2 * math.pi / 4096                    # Camera_Angles[2] (Cond_AngleFB), about z
    rz = ((math.cos(c), -math.sin(c), 0), (math.sin(c), math.cos(c), 0), (0, 0, 1))
    rx = ((1, 0, 0), (0, math.cos(a), -math.sin(a)), (0, math.sin(a), math.cos(a)))
    return [[sum(rx[i][k] * rz[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


ROT = _rot()


def project(dx, dz):
    """Screen point of a sprite dx, dz cells from the camera focus (Ry is 0)."""
    v = (dx * 128.0, dz * 128.0, 0.0)
    x, y, z = (sum(ROT[i][j] * v[j] for j in range(3)) for i in range(3))
    depth = z + 0x1194
    if depth <= 1:
        return None
    return 160 + 1000 * x / depth, 120 + 1000 * y / depth


def band(sx, sy):
    if sy < VIS_Y[0] or sy > VIS_Y[1]:
        return 'off vertically'
    if sx + HALF_W > 0 and sx - HALF_W < VIEW_W:
        return 'inside'
    if -COLS_169 <= sx < 0 or VIEW_W <= sx < VIEW_W + COLS_169:
        return '16:9 band'
    if -COLS_219 <= sx < 0 or VIEW_W <= sx < VIEW_W + COLS_219:
        return '21:9 band only'
    return 'beyond 560'


# --------------------------------------------------------------------------- 1. the banks
NUM = r'(-?(?:0x[0-9A-Fa-f]+|\d+))'
RE_CHANGE = re.compile(r'(?:Field_ChangeArea\)|\bTChangeArea|\bChangeArea)\(\s*' + NUM + r',\s*' + NUM + r',\s*' + NUM)
RE_KIND2 = re.compile(r'\bKind2At\(\s*' + NUM + r',\s*' + NUM + r'\)')
RE_KIND2X = re.compile(r'kKind2X\),\s*' + NUM + r'\)')
RE_KIND2Z = re.compile(r'kKind2Z\),\s*' + NUM + r'\)')
RE_PARTY = re.compile(r'\bPlaceParty\(\s*' + NUM + r',\s*' + NUM)
RE_AREA = re.compile(r'(?:Area\(\)|\barea)\s*==\s*(0x[0-9A-Fa-f]+|\d+)')
RE_FUNC = re.compile(r'^extern "C" .*?\b(\w+)\s*\(')


def bank_sites(root):
    """[(kind, area or None, x, z, file:line, function, chapter)] of every literal site."""
    out = []
    files = sorted(glob.glob(os.path.join(root, 'src', 'game', 'scena_*.cpp')) +
                   glob.glob(os.path.join(root, 'src', 'game', 'area_*.cpp')) +
                   glob.glob(os.path.join(root, 'src', 'game', 'effect_18*.cpp')))
    for path in files:
        if path.endswith('_fuzz.cpp'):
            continue
        lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
        func, chapter, area_ctx = None, None, None
        for i, line in enumerate(lines):
            m = RE_FUNC.match(line)
            if m:
                func, area_ctx = m.group(1), None
                c = re.match(r'Scena(\d\d)_', func)
                chapter = int(c.group(1)) if c else None
            a = RE_AREA.search(line)
            if a:
                area_ctx = int(a.group(1), 0)
            where = '%s:%d' % (os.path.relpath(path, root).replace('\\', '/'), i + 1)
            for m in RE_CHANGE.finditer(line):
                try:
                    ar, x, z = (int(g, 0) for g in m.groups())
                except ValueError:
                    continue
                out.append(('leader', ar, x / 65536, z / 65536, where, func, chapter))
            for m in RE_KIND2.finditer(line):
                x, z = (int(g, 0) for g in m.groups())
                out.append(('camera', area_ctx, x / 65536, z / 65536, where, func, chapter))
            mx = RE_KIND2X.search(line)
            if mx:
                for j in range(i, min(i + 4, len(lines))):
                    mz = RE_KIND2Z.search(lines[j])
                    if mz:
                        out.append(('camera', area_ctx, int(mx.group(1), 0) / 65536, int(mz.group(1), 0) / 65536,
                                    where, func, chapter))
                        break
            for m in RE_PARTY.finditer(line):
                try:
                    x, z = (int(g, 0) for g in m.groups())
                except ValueError:
                    continue
                out.append(('party', area_ctx, x / 65536, z / 65536, where, func, chapter))
    return out


# --------------------------------------------------------------------------- 2. the placements
def placements(img, lengths, speeds, area):
    """[(op address, nibble, x, z, script, speed, hidden, condition path, chapter)]"""
    d = img.u32(es.AREA_TABLE + 4 * area)
    if not d:
        return []
    w = es.Walk(img, img.u32(d), lengths, speeds).run()
    stack, out = [], []
    for a, depth, text in w.ops:
        del stack[depth:]
        head = text.split()[0]
        if head in ('IF', 'IFNOT', 'SWITCH', 'CASE', 'ELSE', 'DEFAULT'):
            if head == 'ELSE' and stack:
                stack[-1] = stack[-1] + ' else'
            elif head in ('CASE', 'DEFAULT'):
                stack.append(text)
            else:
                stack.append(text)
            continue
        if not (len(head) == 2 and head[1] == 'x'):
            continue
        nib = int(head[0], 16)
        if nib not in LAYOUT:
            continue
        raw = bytes.fromhex(text.split()[1])
        xh, xw, zh, zw, sc, sp, fl = LAYOUT[nib]
        x = raw[xw] + (0.5 if raw[xh] else 0)
        z = raw[zw] + (0.5 if raw[zh] else 0)
        speed = raw[sp] if sp is not None else 0
        script = raw[sc] if sc is not None else None
        hidden = bool(raw[fl] & 1) if fl is not None else False
        chap = None
        for s in stack:
            m = re.match(r'IF cond 0 x ([0-9a-f]{2})$', s)
            if m:
                chap = int(m.group(1), 16)
        out.append((a, nib, x, z, script, speed, hidden, ' / '.join(stack), chap))
    return out


# --------------------------------------------------------------------------- 3. the timelines
class Actor:
    """One script run from one start: its timeline on counter 0."""

    def __init__(self, area, label, start, x, z, hidden, chapter, steps, ops):
        self.area, self.label, self.start, self.chapter = area, label, start, chapter
        self.uses_clock = False
        self.talks = []         # (k, message, x, z)
        self.moves = []         # (k, x0, z0, x1, z1, visible): one per move, k the clock when it began
        self.events = []        # (k, kind, x, z, op offset, visible before): show, hide, end, place
        self.unmodelled = collections.Counter()
        self.final = None
        self.end_op = None
        self.run(x, z, hidden, steps, ops)

    def run(self, x, z, hidden, steps, ops):
        k = -1                  # the clock: -1 until the script first sets or waits for it
        vis = not hidden and x is not None   # a party script is nowhere until its first 0D
        pos = [x, z]
        moves_at = []           # (at, dx, dz) to repeat for 02 loops

        def move(direction, n, at):
            if pos[0] is None or n == 0:
                return
            sx, sz = steps[direction & 7]
            f = 2 if (direction & 7) in (2, 6) else 1
            dx, dz = sx * 16 * f * n / 65536, sz * 16 * f * n / 65536
            self.moves.append((k, pos[0], pos[1], pos[0] + dx, pos[1] + dz, vis))
            moves_at.append((at, dx, dz))
            pos[0] += dx
            pos[1] += dz

        for at, b in ops:
            op = b[0]
            if 0x10 <= op <= 0x4F:
                move((op - 0x10) >> 3, op & 7, at)
            elif op in (0xF2, 0xF3, 0xFA) and len(b) >= 3:
                move(b[1], b[2], at)
            elif op == 0x0D and len(b) >= 5:
                nx, nz = b[1] + (0.5 if b[2] else 0), b[3] + (0.5 if b[4] else 0)
                if nx == 0 and nz == 0:
                    # cell (0, 0) is off every map (MapView_Build releases a cell whose x or y is 0):
                    # the script's own "not here yet"
                    continue
                if pos[0] is None:
                    pos[0], pos[1] = nx, nz
                    if x is None:
                        vis = not hidden
                    self.start = (nx, nz)
                else:
                    self.events.append((k, 'place', nx, nz, at, vis))
                    self.moves.append((k, pos[0], pos[1], nx, nz, vis))
                    pos[0], pos[1] = nx, nz
            elif op == 0xF7 and len(b) >= 5 and pos[0] is not None:
                nx, nz = b[1] + b[2] / 256, b[3] + b[4] / 256
                self.moves.append((k, pos[0], pos[1], nx, nz, vis))
                pos[0], pos[1] = nx, nz
            elif op == 0x02 and len(b) >= 3:
                d = b[1] - 256 if b[1] & 0x80 else b[1]
                body = [(a2, dx, dz) for a2, dx, dz in moves_at if at + d <= a2 < at]
                for _ in range(b[2]):
                    for a2, dx, dz in body:
                        self.moves.append((k, pos[0], pos[1], pos[0] + dx, pos[1] + dz, vis))
                        pos[0] += dx
                        pos[1] += dz
            elif 0xA0 <= op <= 0xAF and ((op >> 2) & 3) == 0:
                self.uses_clock = True
                t = op & 3
                if t in (0, 1) and len(b) >= 2:
                    k = b[1]
                elif t == 2:
                    k = k + 1 if k >= 0 else k
            elif op == 0xEB:
                vis = not vis
                if pos[0] is not None:
                    self.events.append((k, 'show' if vis else 'hide', pos[0], pos[1], at, True))
            elif op == 0x0C:
                self.unmodelled['0C'] += 1
            elif op == 0xE6:
                if pos[0] is not None:
                    self.events.append((k, 'end', pos[0], pos[1], at, vis))
                vis = False
                self.end_op = 'E6'
                break
            elif op in (0xE0, 0xE3) and len(b) >= 2 and pos[0] is not None:
                self.talks.append((k, b[1], pos[0], pos[1]))
            elif op == 0xE1 and len(b) >= 3 and pos[0] is not None:
                self.talks.append((k, b[2], pos[0], pos[1]))
            elif op in UNMODELLED:
                self.unmodelled[UNMODELLED[op]] += 1
            elif op in (0xFF, 0xEF, 0xD8, 0x86):
                self.end_op = '%02X' % op
        self.final = (k, pos[0], pos[1], vis)

    def rests(self):
        """Stationary stretches: (k from, k to or None for the end, x, z, moved before)."""
        out = []
        moved = False
        cur_k = -1
        if self.start and self.start[0] is not None:
            px, pz = self.start
        else:
            return out
        for (k, x0, z0, x1, z1, vis) in self.moves:
            if k > cur_k:
                out.append((cur_k, k, px, pz, moved))
            moved = True
            px, pz = x1, z1
            cur_k = max(cur_k, k)
        out.append((cur_k, None, px, pz, moved))
        return out


def script_starts(img, area):
    d = img.u32(es.AREA_TABLE + 4 * area)
    res = {}
    if not d:
        return res
    for fld in ms.FIELDS:
        arr = img.u32(d + fld)
        if img.section(arr) != '.data':
            continue
        for k in range(256):
            p = img.u32(arr + 4 * k)
            if img.section(p) != '.data' or p >= arr:
                break
            if ms.RUNTIME[0] <= p < ms.RUNTIME[1]:
                continue
            res[(fld, k)] = p
    return res


def reached_ops(img, lengths, p):
    """Every op the script can reach, in address order (movement_scan's flow rules)."""
    o = img.off(p)
    script = img.data[o:o + 0x10010]
    seen, todo, ops = set(), [0], {}
    while todo:
        at = todo.pop()
        while True:
            at &= 0xFFFF
            if at in seen:
                break
            op = script[at]
            n = ms.exec_length(script, at, lengths)
            if n == 0:
                break
            seen.add(at)
            ops[at] = script[at:at + n]
            if op == 0x01:
                hi, lo = script[at + 1], script[at + 2]
                if (hi & 0xC0) == 0x40:
                    t = ms.find_label(script, lo, lengths)
                    if t is not None:
                        todo.append(t)
                else:
                    todo.append(at + (hi << 8 | lo))
                break
            if 0x04 <= op <= 0x09:
                for hi, lo in ((script[at + 3], script[at + 4]), (script[at + 5], script[at + 6])):
                    if not hi & 0x40:
                        todo.append(hi << 8 | lo)
                        continue
                    t = ms.find_label(script, lo, lengths)
                    if t is not None:
                        todo.append(t)
                break
            if op == 0x02:
                d = script[at + 1] - 256 if script[at + 1] & 0x80 else script[at + 1]
                todo.append(at + d)
            if op == 0x0B:
                break
            if op in ms.PATH_ENDS:
                break
            at += n
    return sorted(ops.items())


# --------------------------------------------------------------------------- 4. the scan
def scan_area(img, lengths_ev, lengths_mv, speeds, steps, area, focus_sites):
    starts = script_starts(img, area)
    ops_of = {key: reached_ops(img, lengths_mv, p) for key, p in starts.items()}
    actors = []
    for (a, nib, x, z, script, speed, hidden, path, chap) in placements(img, lengths_ev, speeds, area):
        if script is None or speed == 0 or (0x10, script) not in starts:
            continue
        label = 'obj %Xx@%#x +0x10[%d]' % (nib, a, script)
        actors.append(Actor(area, label, (x, z), x, z, hidden, chap, steps, ops_of[(0x10, script)]))
    for (fld, k), p in starts.items():
        if fld == 0x18:
            actors.append(Actor(area, 'party +0x18[%d]@%#x' % (k, p), None, None, None, False, None, steps, ops_of[(fld, k)]))
    cams = [Actor(area, 'camera +0x1C[%d]@%#x' % (k, p), (0.0, 0.0), 0.0, 0.0, False, None, steps, ops_of[(fld, k)])
            for (fld, k), p in starts.items() if fld == 0x1C]
    scene = [ac for ac in actors if ac.uses_clock]
    if not scene:
        return [], actors, cams
    talks = [(k, msg, x, z, ac) for ac in scene for (k, msg, x, z) in ac.talks]
    clock_cams = [c for c in cams if c.uses_clock and c.moves]
    rows = []

    def speakers(k0, k1, me):
        sel = [(x, z) for (k, msg, x, z, ac) in talks if ac is not me and k0 <= k <= (k1 if k1 is not None else 999)]
        return sel

    def nearest_speakers(k0, me):
        """The lines spoken nearest the clock value k0 (within 4 of the nearest), for a moment with none."""
        cand = [(abs(k - k0), x, z) for (k, msg, x, z, ac) in talks if ac is not me]
        if not cand:
            return []
        best = min(c[0] for c in cand)
        return [(x, z) for d, x, z in cand if d <= best + 4]

    def camera_at(k, ref):
        """The bank's focus point for this area nearest the speakers, and the kind-2 script's
        displacement at clock k: (fx, fz, (dx, dz) or None, confidence, note, chapter, function)."""
        best = None
        for (kind, ar, fx, fz, where, func, chapter) in focus_sites:
            if ar != area or kind == 'party':
                continue
            d = math.hypot(fx - ref[0], fz - ref[1])
            if best is None or d < best[0]:
                best = (d, fx, fz, kind, where, func, chapter)
        if best is None or best[0] > 24:
            return None
        d, fx, fz, kind, where, func, chapter = best
        note = '%s focus %s (%s), %.1f cells from the speakers' % (kind, where, func, d)
        conf = 'high'
        delta = None
        if len(clock_cams) == 1:
            dx = dz = 0.0
            for (kk, x0, z0, x1, z1, vis) in clock_cams[0].moves:
                if kk <= k:
                    dx += x1 - x0
                    dz += z1 - z0
            if dx or dz:
                delta = (dx, dz)
                note += '; kind-2 %s moved (%+.1f, %+.1f) by then' % (clock_cams[0].label, dx, dz)
        elif len(clock_cams) > 1:
            conf = 'medium'
            note += '; %d kind-2 scripts on the clock, none applied' % len(clock_cams)
        if any(not c.uses_clock and c.moves for c in cams):
            conf = 'medium'
            note += '; a kind-2 script moves off the clock'
        return fx, fz, delta, conf, note, chapter, func

    def estimate(x, z, k0, k1, me):
        spk = speakers(k0, k1, me) or nearest_speakers(k0, me)
        if not spk:
            return None
        ref = (sum(p[0] for p in spk) / len(spk), sum(p[1] for p in spk) / len(spk))
        cam = camera_at(k0, ref)
        if not cam:
            p = project(x - ref[0], z - ref[1])
            if p is None:
                return None
            return (p, 'needs census (%s if the speakers are centred)' % band(*p), 'low',
                    "no bank focus for the area: the speakers' centroid", None, None, ref)
        fx, fz, delta, conf, note, chapter, func = cam
        p = project(x - fx, z - fz)
        if p is None:
            return None
        b = band(*p)
        focus = (fx, fz)
        if delta:
            # The kind-2 script is the area's, but which scene starts it is not read: the band
            # stands only when it is the same with and without its steps.
            q = project(x - fx - delta[0], z - fz - delta[1])
            if q is None or band(*q) != b:
                qb = band(*q) if q else 'behind the camera'
                note += '; with those steps %s at %.0f' % (qb, q[0] if q else 0)
                b = 'needs census (%s without the camera script, %s with it)' % (b, qb)
                conf = 'low'
        if conf == 'high':
            lo, hi = band(p[0] - 30, p[1]), band(p[0] + 30, p[1])
            if lo != b or hi != b:
                conf = 'medium'
        return p, b, conf, note, chapter, func, focus

    for ac in scene:
        rests = ac.rests()
        # parked: a visible stretch at rest while someone else in the scene talks
        for (k0, k1, x, z, moved) in rests:
            visible = is_visible(ac, k0)
            if not visible:
                continue
            others = speakers(k0 if k0 >= 0 else -1, k1, ac)
            if not others:
                continue
            est = estimate(x, z, k0, k1, ac)
            if not est:
                continue
            p, b, conf, note, chapter, func, focus = est
            if quiet(b):
                continue
            nxt = [m for m in ac.moves if m[0] == k1] if k1 is not None else []
            klass = 'parked'
            if nxt:
                q = project(nxt[-1][3] - focus[0], nxt[-1][4] - focus[1])
                if q and band(*q) == 'inside':
                    klass = 'parked (waits, then enters)'
            rows.append(row(ac, klass, x, z, p, b, conf, k0, k1, len(others), note, chapter, func, moved))
        # entrance: the first move of a visible actor from a start outside the view, ending inside
        shown_at = {(k, x, z) for (k, kind, x, z, at, v) in ac.events if kind == 'show'}
        for (k, x0, z0, x1, z1, vis) in ac.moves[:1]:
            if not vis or (k, x0, z0) in shown_at:
                continue
            est = estimate(x0, z0, k, k, ac)
            if not est:
                continue
            p, b, conf, note, chapter, func, focus = est
            if quiet(b):
                continue
            q = project(x1 - focus[0], z1 - focus[1])
            if q and band(*q) == 'inside':
                rows.append(row(ac, 'entrance seen early', x0, z0, p, b, conf, k, k, 0, note, chapter, func))
        # spawn and despawn pops
        for (k, kind, x, z, at, was_vis) in ac.events:
            if kind == 'show' or (kind == 'place' and was_vis):
                klass = 'spawn pop'
            elif kind in ('hide', 'end') and was_vis:
                klass = 'despawn pop'
            else:
                continue
            est = estimate(x, z, k, k, ac)
            if not est:
                continue
            p, b, conf, note, chapter, func, focus = est
            if not ('16:9' in b or '21:9' in b):
                continue
            rows.append(row(ac, klass, x, z, p, b, conf, k, k, 0, note + '; op +%#x' % at, chapter, func))
    return rows, actors, cams


def quiet(b):
    """A band that is no staging finding: seen in the original, or never on screen."""
    if b.startswith('needs census'):
        inner = b[len('needs census ('):]
        return not ('16:9' in inner or '21:9' in inner or 'beyond' in inner)
    return b in ('inside', 'off vertically')


def is_visible(ac, k):
    vis = ac.start is not None
    hidden_events = [(kk, kind) for (kk, kind, x, z, at, v) in ac.events if kind in ('show', 'hide', 'end') and kk <= k]
    # the placement's own bit: an actor whose first visibility event is a show began hidden
    first = [kind for (kk, kind, x, z, at, v) in ac.events if kind in ('show', 'hide', 'end')]
    if first and first[0] == 'show':
        vis = False
    for kk, kind in hidden_events:
        vis = kind == 'show'
    return vis


def clock(k):
    return 'start' if k < 0 else '%#x' % k


def row(ac, klass, x, z, p, b, conf, k0, k1, n_spk, note, chapter, func, walked=True):
    fate = {'E6': 'removed (E6)', 'EF': 'stays (EF)', 'FF': 'stays (FF)', 'D8': 'stays (D8)', '86': 'handed on (86)'}
    return {
        'area': ac.area, 'chapter': ac.chapter if ac.chapter is not None else (chapter if chapter is not None else ''),
        'site': ac.label, 'bank': func or '', 'object': ac.label.split(' ')[0],
        'rest': '(%.1f, %.1f)' % (x, z), 'sx': '%.0f' % p[0], 'sy': '%.0f' % p[1], 'class': klass, 'band': b,
        'confidence': conf,
        'clock': clock(k0) + '..' + ('end' if k1 is None else clock(k1)),
        'fate': fate.get(ac.end_op, 'runs on'),
        'spoken': n_spk,
        'staged': 'walked there' if walked else 'placed there',
        'evidence': '%d lines spoken by others in the stretch; %s' % (n_spk, note)
                    + ('; unmodelled ops %s' % ','.join(sorted(ac.unmodelled)) if ac.unmodelled else ''),
    }


def merge(rows):
    """One row per (area, class, point, clock, band): the same rest reached by several
    script instances (one script placed by several branches, or copies of a party script)."""
    out, seen = [], {}
    for r in rows:
        key = (r['area'], r['class'], r['rest'], r['clock'], r['band'])
        if key in seen:
            seen[key]['instances'] += 1
            continue
        r = dict(r, instances=1)
        seen[key] = r
        out.append(r)
    return out


def score(r):
    """How likely a row is to be seen in play, for the summary's top ten: a band both modes show
    first, then the confidence, then parked over the momentary classes, then an actor the scene
    walked there over one placed there, then how many lines are spoken while it waits."""
    b = r['band']
    sb = 3 if b == '16:9 band' else 2 if b == '21:9 band only' else 1 if '16:9' in b or '21:9' in b else 0
    sc = {'high': 3, 'medium': 2, 'low': 1}[r['confidence']]
    sk = 2 if r['class'].startswith('parked') else 1
    sw = 1 if r['staged'] == 'walked there' else 0
    return (sb, sc, sk, sw, int(r['spoken']))


COLS = ['area', 'chapter', 'site', 'bank', 'object', 'rest', 'sx', 'sy', 'class', 'band', 'confidence', 'clock',
        'fate', 'staged', 'instances', 'spoken', 'evidence']


def bank_records(img):
    """Calls into the placement ops (EventOp_0x .. 9x) from code, with a .data record address
    pushed or added within the twelve instructions before: objects the banks place mid-scene."""
    import capstone
    handlers = {0x57A010: '0x', 0x57A1E0: '1x', 0x57A5E0: '2x', 0x57A7C0: '3x', 0x57A990: '4x', 0x57B310: '5x',
                0x57AD10: '6x', 0x57AB50: '7x', 0x57A3A0: '8x', 0x57B530: '9x', 0x57AEC0: 'Ax', 0x57B130: 'Bx'}
    text = next(sec for sec in es.sections(img.data) if sec[0] == '.text')
    _, tva, trs, traw = text
    t = img.data[traw:traw + trs]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    out = []
    for o in range(40, len(t) - 5):
        if t[o] != 0xE8:
            continue
        dest = (tva + o + 5 + struct.unpack_from('<i', t, o + 1)[0]) & 0xFFFFFFFF
        if dest not in handlers:
            continue
        a = tva + o
        recs = set()
        for back in range(16, 48):
            for ins in md.disasm(t[o - back:o], a - back):
                for m in re.finditer(r'0x([0-9a-f]{6})\b', ins.op_str):
                    v = int(m.group(1), 16)
                    if img.section(v) == '.data':
                        recs.add(v)
        out.append((a, handlers[dest], sorted(recs)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=es.EXE)
    ap.add_argument('--area', type=int)
    ap.add_argument('--out', default='analysis/ultrawide/staging_static.tsv')
    ap.add_argument('--top', type=int, default=10)
    args = ap.parse_args()
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    img = es.Image(args.exe)
    lengths_ev = img.bytes(es.OP_LENGTHS, 16)
    speeds = img.bytes(es.FIELD_MOVE_SPEEDS, 256)
    lengths_mv = img.bytes(ms.OP_LENGTHS, 256)
    steps = [struct.unpack_from('<ii', img.bytes(F9_STEPS + 8 * d, 8)) for d in range(8)]
    sites = bank_sites(root)

    areas = [args.area] if args.area is not None else range(es.AREAS)
    rows, n_actors, n_scene, n_cams, n_areas = [], 0, 0, 0, 0
    for area in areas:
        r, actors, cams = scan_area(img, lengths_ev, lengths_mv, speeds, steps, area, sites)
        rows += r
        n_actors += len(actors)
        n_scene += sum(1 for a in actors if a.uses_clock)
        n_areas += 1 if any(a.uses_clock for a in actors) else 0
        n_cams += len(cams)
        if args.area is not None:
            for a in actors:
                print(a.label, 'start', a.start, 'clock' if a.uses_clock else '', 'final', a.final, a.end_op,
                      'talks', [(hex(k), hex(m)) for k, m, _, _ in a.talks][:12])
    raw_rows = len(rows)
    rows = merge(rows)
    rows.sort(key=lambda r: (r['area'], r['class'], r['clock']))
    os.makedirs(os.path.dirname(args.out) or '.', exist_ok=True)
    with open(args.out, 'w', encoding='utf-8') as f:
        f.write('\t'.join(COLS) + '\n')
        for r in rows:
            f.write('\t'.join(str(r[c]) for c in COLS) + '\n')

    kinds = collections.Counter(s[0] for s in sites)
    print('bank sites with literal points: %d (%s); with an area: %d' % (
        len(sites), ', '.join('%s %d' % kv for kv in sorted(kinds.items())), sum(1 for s in sites if s[1] is not None)))
    if args.area is None:
        recs = bank_records(img)
        print('code calls into the placement ops: %d, %d with a .data record address before them' % (
            len(recs), sum(1 for _, _, r in recs if r)))
        # The bank's literal points and its placement calls, beside the table (the doc's section 3).
        side = os.path.splitext(args.out)[0] + '_bank.tsv'
        with open(side, 'w', encoding='utf-8') as f:
            f.write('kind\tarea\tchapter\tx\tz\tfunction\twhere\n')
            for (kind, ar, x, z, where, func, chapter) in sites:
                f.write('%s\t%s\t%s\t%.1f\t%.1f\t%s\t%s\n' % (kind, '' if ar is None else ar,
                                                             '' if chapter is None else chapter, x, z, func, where))
            for (a, op, r) in recs:
                f.write('place op %s\t\t\t\t\t%s\tcall %#x\n' % (op, ' '.join('%#x' % v for v in r), a))
        print('bank sites written to %s' % side)
    print('actors: %d scripted placements and party scripts, %d on a scene clock, in %d areas; %d camera scripts' % (
        n_actors, n_scene, n_areas, n_cams))
    print('rows: %d (%d before merging the same rest reached by several instances)' % (len(rows), raw_rows))
    by = collections.Counter()
    for r in rows:
        b = r['band'] if not r['band'].startswith('needs') else 'needs census'
        by[(r['class'].split(' (')[0], b)] += 1
    bands = ['16:9 band', '21:9 band only', 'beyond 560', 'needs census']
    classes = sorted({k for k, _ in by})
    print('  %-22s' % 'class' + ''.join('%16s' % b for b in bands) + '%8s' % 'all')
    for k in classes:
        print('  %-22s' % k + ''.join('%16d' % by[(k, b)] for b in bands) + '%8d' % sum(by[(k, b)] for b in bands))
    print('  %-22s' % 'all' + ''.join('%16d' % sum(by[(k, b)] for k in classes) for b in bands) + '%8d' % len(rows))
    conf = collections.Counter(r['confidence'] for r in rows)
    print('  confidence: ' + ', '.join('%s %d' % (c, conf[c]) for c in ('high', 'medium', 'low')))
    print('top %d by score (band, confidence, class, lines spoken):' % args.top)
    for r in sorted(rows, key=score, reverse=True)[:args.top]:
        print('  area %s ch %s  %-28s %-34s sx %5s sy %4s  %s  %s  clock %s  %s' % (
            r['area'], r['chapter'] or '-', r['class'], r['site'], r['sx'], r['sy'], r['band'], r['confidence'],
            r['clock'], r['staged']))
    if args.area is not None:
        for r in rows:
            print('\t'.join(str(r[c]) for c in COLS))


if __name__ == '__main__':
    main()
