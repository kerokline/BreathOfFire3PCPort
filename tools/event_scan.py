#!/usr/bin/env python
"""Decode every area's placement script - the event script at area descriptor +0.

    python tools/event_scan.py              # summary
    python tools/event_scan.py --area 4     # one area's script, op by op
    python tools/event_scan.py --exe PATH   # another copy of BOF3.exe

The placement script is what Area_RunPlacement 0x579740 runs through
EventScript_Run 0x5797C0 when an area is entered (0x594E60) or reloaded
(0x580535). Its grammar is the interpreter's, read 2026-09-22
(docs/event-script.md):

    op  < F0          one of 15 handlers by the high nibble; length from
                      EventScript_OpLengths 0x663B0C by the high nibble
    F0 c x            if condition c (x its operand) ... [FD ...] FE
    F1 c x            the same, negated
    F4 c              switch on condition c: { F6 x ... F8 | F7 ... F8 } F5
    F9 b / FA         the flag bank: b, or back to Cond_ByteFA
    FF                the end

The table is read from the exe here, not copied. The walk is structural - it
visits every byte both arms of every branch would, and does not evaluate
conditions - so a script that decodes here decodes on every path the game
could take. What the game does with a byte this grammar does not expect is
recorded as an anomaly: the interpreter hangs on most of them (the doc lists
which).
"""
import argparse
import collections
import struct

EXE = 'bof3/BOF3.exe'
AREA_TABLE, AREAS = 0x667590, 200
OP_LENGTHS = 0x663B0C       # 16 bytes by high nibble; the PSX GAME.EMI 0x801C943C for 0..E
CONDITIONS = 17             # EventScript_Conditions 0x663B30: entries 0..16 take a script position
FIELD_MOVE_SPEEDS = 0x6697F0  # EventObj_SetFlags divides 16 by the byte at [speed index]
DIRECTION_ANGLES = 8        # Sprite_DirectionAngles 0x65F644: 8 words, then other data


def sections(data):
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    n, = struct.unpack_from('<H', data, pe + 6)
    osz, = struct.unpack_from('<H', data, pe + 20)
    opt = pe + 24
    base, = struct.unpack_from('<I', data, opt + 28)
    out = []
    for i in range(n):
        s = opt + osz + i * 40
        name = data[s:s + 8].rstrip(b'\0').decode('latin1')
        vsz, va, rsz, raw = struct.unpack_from('<IIII', data, s + 8)
        out.append((name, base + va, rsz, raw))
    return out


class Image:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        self.secs = sections(self.data)

    def off(self, a):
        for _, va, rsz, raw in self.secs:
            if va <= a < va + rsz:
                return raw + a - va
        return None

    def section(self, a):
        for name, va, rsz, _ in self.secs:
            if va <= a < va + rsz:
                return name
        return None

    def u32(self, a):
        return struct.unpack_from('<I', self.data, self.off(a))[0]

    def byte(self, a):
        return self.data[self.off(a)]

    def bytes(self, a, n):
        o = self.off(a)
        return self.data[o:o + n]


class Walk:
    """One script, structurally: every op with its nesting, and what is odd."""

    def __init__(self, img, start, lengths, speeds):
        self.img, self.start, self.L, self.speeds = img, start, lengths, speeds
        self.ops = []           # (address, depth, text)
        self.anomalies = []
        self.stats = collections.Counter()
        self.conds = collections.Counter()
        self.speed_idx = collections.Counter()
        self.places = 0         # Sprite_Objects placements (0x, 1x, 2x) on the longest path, bounded above
        self.extra_idx = collections.Counter()
        self.bx_dir = collections.Counter()
        self.end = None

    def odd(self, a, what):
        self.anomalies.append((a, what))

    def op(self, a, depth):
        b = self.img.byte(a)
        n = self.L[b >> 4]
        raw = self.img.bytes(a, n)
        nib = b >> 4
        self.stats['%Xx' % nib] += 1
        text = '%Xx %s' % (nib, raw.hex())
        if nib in (0, 1, 2):
            self.places += 1
            s = raw[0xB]
            self.speed_idx[s] += 1
            if s and self.speeds[s] == 0:
                self.odd(a, 'speed index %d divides by zero' % s)
        elif nib == 0xB:
            self.extra_idx[raw[0xB]] += 1
            self.bx_dir[raw[0] & 0xF] += 1
            s = raw[6]
            self.speed_idx[s] += 1
            if s and self.speeds[s] == 0:
                self.odd(a, 'speed index %d divides by zero' % s)
            if raw[0xB] > 3:
                self.odd(a, 'Bx extra object %d, past the four' % raw[0xB])
            if (raw[0] & 0xF) >= DIRECTION_ANGLES:
                self.odd(a, 'Bx direction %d reads past Sprite_DirectionAngles' % (raw[0] & 0xF))
        if n == 0:
            self.odd(a, 'op of length 0')
            n = 1
        self.ops.append((a, depth, text))
        return a + n

    def cond(self, a, c, signed_ok):
        self.conds[c] += 1
        idx = c if not signed_ok else (c - 256 if c >= 0x80 else c)
        if signed_ok and not 0 <= idx < CONDITIONS:
            self.odd(a, 'switch condition %d outside the table' % idx)
        if not signed_ok and (c & 0x1F) >= CONDITIONS:
            self.odd(a, 'if condition %d (as %d) outside the conditions' % (c, c & 0x1F))

    def block(self, a, depth, ends, case=False):
        """Ops until a byte in `ends`; returns (address of that byte, byte).

        In a case body (`case`) F6 x and F7 are further labels: EventScript_
        CaseRun steps over them (2 and 1 bytes), so consecutive labels share
        one body, as C's do; EventScript_CaseSkip stops at an F6, where the
        switch tests it."""
        guard = 0
        while True:
            guard += 1
            if guard > 100000:
                self.odd(a, 'runaway')
                return a, None
            b = self.img.byte(a)
            if b in ends:
                return a, b
            if b < 0xF0:
                a = self.op(a, depth)
            elif case and b == 0xF6:
                self.ops.append((a, depth - 1, 'CASE x %02x' % self.img.byte(a + 1)))
                self.stats['case'] += 1
                a += 2
            elif case and b == 0xF7:
                self.ops.append((a, depth - 1, 'DEFAULT'))
                self.stats['default'] += 1
                a += 1
            elif b in (0xF0, 0xF1):
                c, x = self.img.byte(a + 1), self.img.byte(a + 2)
                self.stats['if' if b == 0xF0 else 'ifnot'] += 1
                self.cond(a, c, False)
                self.ops.append((a, depth, '%s cond %d x %02x' % ('IF' if b == 0xF0 else 'IFNOT', c, x)))
                a, e = self.block(a + 3, depth + 1, (0xFD, 0xFE))
                if e == 0xFD:
                    self.ops.append((a, depth, 'ELSE'))
                    self.stats['else'] += 1
                    a, e = self.block(a + 1, depth + 1, (0xFE, 0xFD))
                    if e == 0xFD:
                        self.odd(a, 'a second FD in one if (the interpreter only sets the flag again)')
                        a, e = self.block(a + 1, depth + 1, (0xFE,))
                if e != 0xFE:
                    return a, None
                self.ops.append((a, depth, 'ENDIF'))
                a += 1
            elif b == 0xF4:
                c = self.img.byte(a + 1)
                self.stats['switch'] += 1
                self.cond(a, c, True)
                self.ops.append((a, depth, 'SWITCH cond %d' % c))
                a += 2
                while True:
                    k = self.img.byte(a)
                    if k == 0xF5:
                        self.ops.append((a, depth, 'ENDSWITCH'))
                        a += 1
                        break
                    if k == 0xF6:
                        self.ops.append((a, depth, 'CASE x %02x' % self.img.byte(a + 1)))
                        self.stats['case'] += 1
                        a += 2
                    elif k == 0xF7:
                        self.ops.append((a, depth, 'DEFAULT'))
                        self.stats['default'] += 1
                        a += 1
                    else:
                        self.odd(a, 'byte %02x between cases (the interpreter hangs)' % k)
                        return a, None
                    a, e = self.block(a, depth + 1, (0xF8,), case=True)
                    if e != 0xF8:
                        return a, None
                    a += 1
            elif b == 0xF9:
                self.stats['bank'] += 1
                self.ops.append((a, depth, 'BANK %d' % self.img.byte(a + 1)))
                a += 2
            elif b == 0xFA:
                self.stats['bank_reset'] += 1
                self.ops.append((a, depth, 'BANK default'))
                a += 1
            else:
                self.odd(a, 'byte %02x where an op should be' % b)
                return a, None

    def run(self):
        a, e = self.block(self.start, 0, (0xFF,))
        if e == 0xFF:
            self.end = a + 1
        return self


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=EXE)
    ap.add_argument('--area', type=int)
    args = ap.parse_args()
    img = Image(args.exe)
    lengths = img.bytes(OP_LENGTHS, 16)
    speeds = img.bytes(FIELD_MOVE_SPEEDS, 256)

    areas = [args.area] if args.area is not None else range(AREAS)
    total = collections.Counter()
    conds = collections.Counter()
    speed = collections.Counter()
    extra = collections.Counter()
    bxdir = collections.Counter()
    where = collections.Counter()
    anomalies = []
    most_places = (0, None)
    n_bytes = 0
    walked = 0
    for area in areas:
        desc = img.u32(AREA_TABLE + 4 * area)
        if not desc:
            continue
        start = img.u32(desc)
        where[img.section(start)] += 1
        w = Walk(img, start, lengths, speeds).run()
        walked += 1
        if args.area is not None:
            for a, depth, text in w.ops:
                print('%08X  %s%s' % (a, '  ' * depth, text))
        total.update(w.stats)
        conds.update(w.conds)
        speed.update(w.speed_idx)
        extra.update(w.extra_idx)
        bxdir.update(w.bx_dir)
        anomalies += [(area, a, t) for a, t in w.anomalies]
        if w.places > most_places[0]:
            most_places = (w.places, area)
        if w.end:
            n_bytes += w.end - start

    print('areas walked: %d; scripts in %s; %d bytes to their FF' % (
        walked, ', '.join('%s x%d' % kv for kv in sorted(where.items())), n_bytes))
    print('ops: ' + ', '.join('%s %d' % kv for kv in sorted(total.items())))
    print('conditions (if: index & 0x1F, switch: signed): ' + ', '.join('%d x%d' % kv for kv in sorted(conds.items())))
    print('speed indices placed (EventObj_SetFlags divides by Field_MoveSpeeds[i]): ' +
          ', '.join('%d x%d' % kv for kv in sorted(speed.items())))
    print('Bx extra objects: ' + ', '.join('%d x%d' % kv for kv in sorted(extra.items())) +
          '; Bx directions: ' + ', '.join('%d x%d' % kv for kv in sorted(bxdir.items())))
    print('most placements in one script (every branch counted): %d, area %s - the interpreter stops placing at 30' %
          most_places)
    print('anomalies: %d' % len(anomalies))
    for area, a, t in anomalies[:40]:
        print('  area %d at %08X: %s' % (area, a, t))


if __name__ == '__main__':
    main()
