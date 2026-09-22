#!/usr/bin/env python
"""Scan BOF3.exe for the movement-script command F8 07 (attach) - known-defects D6.

    python tools/movement_scan.py            # summary, and every bit-7 candidate
    python tools/movement_scan.py --all      # every F8 07 xx triple
    python tools/movement_scan.py --decode   # decode every script from its start

The movement scripts are compiled into the exe with the rest of each area
section and reached through the area descriptor table at 0x667590 (200
pointers; arrays of script pointers at descriptor +0x10, +0x18 and +0x1C).
The default scan is of raw bytes - a superset: it does not decode scripts, so
a hit is a candidate until its context says otherwise, and no hit means no
script has one. See docs/movement-script.md.

--decode follows each script's control flow from its start (decode(), which
says what ends a path). Op lengths are the game's own: MoveScript_FindLabel
0x579450 steps F8 07 by 5, other F8 by 2, 0E / 0F by 4 or 5 by bit 2 of their
fourth byte, every other op by the table at 0x6639FC - read from the exe here,
not copied - and labels are resolved by that same walk. It reports every
attachment reached, labels the game's walk would never find, and paths that
run on past the next script's start.
"""
import argparse
import collections
import struct

EXE = 'bof3/BOF3.exe'
AREA_TABLE, AREAS = 0x667590, 200
OP_LENGTHS = 0x6639FC   # 256 bytes, identical to PSX GAME.EMI 0x801C97F4 (2026-09-22)
# The descriptor's script arrays: +0x10 and +0x1C read by the interpreter's
# callers 0x517BF0 and 0x573090; +0x18 by op 86, which stores one to the
# struct +0x130 that the third caller 0x5253E0 runs (PSX +0x124).
FIELDS = (0x10, 0x18, 0x1C)
# Ops after which a script's path does not go on (PSX GAME.EMI handlers, read
# 2026-09-22): FF stays on itself; EF and D8 set bit 3 of the script
# context's flag byte (FUN_801aa944, FUN_801ab470), which ends the script;
# E6 clears the sprite's flag byte; 86 hands the sprite a +0x18 script and
# stays on itself (FUN_801ac458). What follows them is padding thousands of times.
PATH_ENDS = (0xFF, 0xEF, 0xD8, 0xE6, 0x86)
# Every array entry that points past its own array points into this block,
# which is zero in the file: scripts built at run time. Its writer is unfound
# (no code immediate points into it, 2026-09-22). The bounds are the entries'
# own range, measured, rounded out to 0x10.
RUNTIME = (0x6758E0, 0x675960)


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--all', action='store_true')
    ap.add_argument('--decode', action='store_true')
    args = ap.parse_args()
    data = open(EXE, 'rb').read()
    secs = sections(data)
    dsec = next(s for s in secs if s[0] == '.data')

    def off(a):
        for _, va, rsz, raw in secs:
            if va <= a < va + rsz:
                return raw + a - va
        return None

    def u32(a):
        o = off(a)
        return None if o is None else struct.unpack_from('<I', data, o)[0]

    def in_data(p):
        return p is not None and dsec[1] <= p < dsec[1] + dsec[2]

    # Script starts: each descriptor's arrays (FIELDS), walked while the words
    # point into .data. The arrays carry no length; the walk's stop
    # is this tool's bound, not the game's.
    starts = collections.defaultdict(list)   # pointer -> [(area, field, index)]
    runtime = []
    for area in range(AREAS):
        d = u32(AREA_TABLE + 4 * area)
        if not in_data(d):
            continue
        for field in FIELDS:
            arr = u32(d + field)
            if not in_data(arr):
                continue
            for k in range(256):
                p = u32(arr + 4 * k)
                if in_data(p) and RUNTIME[0] <= p < RUNTIME[1]:
                    runtime.append((area, field, k, p))   # written at run time: nothing to decode
                    continue
                if not in_data(p) or p >= arr:   # scripts precede their array
                    break
                starts[p].append((area, field, k))
    ordered = sorted(starts)
    if runtime:
        print(f'{len(runtime)} array entries in {len({a for a, _, _, _ in runtime})} areas point into '
              f'{RUNTIME[0]:#x}..{RUNTIME[1]:#x}, zero in the file - scripts written at run time, not decoded')
    print(f'{len(ordered)} distinct script starts in '
          f'{len({a for v in starts.values() for a, _, _ in v})} areas')
    if args.decode:
        decode(data, off, ordered, starts)
        return

    _, va, rsz, raw = dsec
    blob = data[raw:raw + rsz]
    triples = []
    i = blob.find(b'\xF8\x07')
    while i >= 0:
        if i + 2 < len(blob):
            triples.append((va + i, blob[i + 2]))
        i = blob.find(b'\xF8\x07', i + 1)
    bit7 = [(a, h) for a, h in triples if h & 0x80]
    print(f'{len(triples)} F8 07 xx triples in .data, {len(bit7)} with bit 7')

    import bisect
    for a, h in (triples if args.all else bit7):
        j = bisect.bisect_right(ordered, a) - 1
        where = 'before any script start'
        if j >= 0:
            s = ordered[j]
            area, field, k = starts[s][0]
            where = f'{a - s:#x} past script start {s:#x} (area {area}, +{field:#x}[{k}])'
        o = raw + a - va
        print(f'  {a:#08x} handle {h:02X}  {where}')
        print(f'      {data[o - 16:o + 16].hex(" ")}')


def op_length(script, at, lengths):
    """Length of the op at `at`, as MoveScript_FindLabel steps over it."""
    op = script[at]
    if op == 0xF8:
        return 5 if script[at + 1] == 7 else 2
    if op in (0x0E, 0x0F):
        return 5 if script[at + 3] & 2 else 4
    return lengths[op]


def exec_length(script, at, lengths):
    """Length of the op as the interpreter executes it. The same as the label
    search's but for C1, which its handler advances by 4 (MoveScript_GroupC,
    and PSX FUN_801abb00 alike) where the table says 5 - a latent defect of
    the table on both platforms (docs/movement-script.md section 4)."""
    if script[at] == 0xC1:
        return 4
    return op_length(script, at, lengths)


def find_label(script, label, lengths, exec_lengths=False):
    """MoveScript_FindLabel's walk: from 0, op by op, to `0A label`. None when
    the walk meets an op of length 0 first - the game's walk would never end."""
    at = 0
    while at < 0x10000:
        if script[at] == 0x0A and script[at + 1] == label:
            return at
        length = (exec_length if exec_lengths else op_length)(script, at, lengths)
        if length == 0:
            return None
        at += length
    return None


def decode(data, off, ordered, starts):
    """Every op a script can reach from its start, following its control flow.

    A path ends at PATH_ENDS, 0B (to 0), an 01 (to its target or label),
    04..09 (to one of two targets: a position, or a label when bit 0x4000 is
    set) or an op of length 0; an 02 loop both goes back and falls through.
    The rest of the groups not yet read (60, 80, 90, C0, D0, E0) are taken to
    fall through - so what is reached is a superset wherever one of them ends
    a path instead."""
    lengths = data[off(OP_LENGTHS):off(OP_LENGTHS) + 256]
    ops = collections.Counter()
    handles = collections.Counter()
    ends = collections.Counter()      # what a path ended on
    before_zero = collections.Counter()   # the op a path fell through from into a length-0 byte
    attaches, missing, outside, skewed = {}, [], [], []

    def label(script, s, at, lo):
        """The game's label search - and, where a C1 lies on its walk, the
        position the walk would reach if the table had C1's executed length."""
        t = find_label(script, lo, lengths)
        if t != find_label(script, lo, lengths, exec_lengths=True):
            skewed.append((s, at, lo, t, find_label(script, lo, lengths, exec_lengths=True)))
        if t is None:
            missing.append((s, at, lo))
        return t
    decoded = set()                   # absolute addresses of every op reached
    for n, s in enumerate(ordered):
        limit = ordered[n + 1] if n + 1 < len(ordered) else s + 0x10000
        o = off(s)
        script = data[o:o + 0x10010]
        seen, todo = set(), [(0, None)]
        while todo:
            at, prev = todo.pop()
            while True:
                at &= 0xFFFF
                if at in seen:
                    break
                op = script[at]
                length = exec_length(script, at, lengths)
                if length == 0:
                    ends['an op of length 0'] += 1
                    before_zero[prev] += 1
                    break
                seen.add(at)
                if s + at not in decoded:
                    decoded.add(s + at)
                    ops[op] += 1
                    if op == 0xF8 and script[at + 1] == 7:
                        handles[script[at + 2]] += 1
                        attaches[s + at] = (s, at, script[at + 2])
                if s + at >= limit:
                    outside.append((s, at))
                if op == 0x01:
                    hi, lo = script[at + 1], script[at + 2]
                    if (hi & 0xC0) == 0x40:
                        t = label(script, s, at, lo)
                        if t is not None:
                            todo.append((t, op))
                    else:
                        todo.append((at + (hi << 8 | lo), op))
                    ends['01'] += 1
                    break
                if 0x04 <= op <= 0x09:
                    # hi:lo is a position, and a label only with bit 0x4000 set -
                    # MoveScript_FindLabel tests that bit before it searches.
                    for hi, lo in ((script[at + 3], script[at + 4]), (script[at + 5], script[at + 6])):
                        if not hi & 0x40:
                            todo.append((hi << 8 | lo, op))
                            continue
                        t = label(script, s, at, lo)
                        if t is not None:
                            todo.append((t, op))
                    ends['04..09'] += 1
                    break
                if op == 0x02:
                    d = script[at + 1] - 256 if script[at + 1] & 0x80 else script[at + 1]
                    todo.append((at + d, op))
                if op == 0x0B:
                    todo.append((0, op))
                    ends['0B'] += 1
                    break
                if op in PATH_ENDS:
                    ends[f'{op:02X}'] += 1
                    break
                prev = op
                at += length
    print(f'reached {len(decoded)} ops; paths ended at: '
          + ', '.join(f'{k} {v}' for k, v in ends.most_common()))
    print('  falling into a length-0 byte, after: '
          + ', '.join(f'{k:02X} x{v}' if k is not None else f'(the start) x{v}' for k, v in before_zero.most_common(10)))
    print(f'F8 07 attachments reached: {len(attaches)}; handles: '
          + ', '.join(f'{h:02X} x{c}' for h, c in sorted(handles.items())))
    bit7 = [a for a in attaches.values() if a[2] & 0x80]
    print(f'  with bit 7 (known-defects D6): {len(bit7)}')
    for s, at, h in bit7:
        area, field, k = starts[s][0]
        print(f'    script {s:#x} (area {area}, +{field:#x}[{k}]) +{at:#x}: handle {h:02X}')
    print(f'labels the game would never find: {len(missing)}')
    for s, at, lo in missing[:10]:
        area, field, k = starts[s][0]
        print(f'    script {s:#x} (area {area}, +{field:#x}[{k}]) +{at:#x}: label {lo:02X}')
    print(f'label searches that cross a C1 and end elsewhere than they would at its executed length: {len(skewed)}')
    for s, at, lo, t, e in skewed[:10]:
        area, field, k = starts[s][0]
        fmt = lambda v: 'never' if v is None else f'+{v:#x}'
        print(f'    script {s:#x} (area {area}, +{field:#x}[{k}]) +{at:#x}: label {lo:02X} -> {fmt(t)} (at 4: {fmt(e)})')
    print(f'ops reached past the next script start: {len(outside)} in '
          f'{len({s for s, _ in outside})} scripts')
    print(f'{len(ops)} distinct ops reached; most common: '
          + ', '.join(f'{k:02X} x{v}' for k, v in ops.most_common(12)))
    return decoded, attaches


if __name__ == '__main__':
    main()
