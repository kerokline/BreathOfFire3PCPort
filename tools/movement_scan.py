#!/usr/bin/env python
"""Scan BOF3.exe for the movement-script command F8 07 (attach) - known-defects D6.

    python tools/movement_scan.py            # summary, and every bit-7 candidate
    python tools/movement_scan.py --all      # every F8 07 xx triple

The movement scripts are compiled into the exe with the rest of each area
section and reached through the area descriptor table at 0x667590 (200
pointers; arrays of script pointers at descriptor +0x10 and +0x1C). The scan
is of raw bytes - a superset: it does not decode scripts, so a hit is a
candidate until its context says otherwise, and no hit means no script has
one. See docs/movement-script.md.
"""
import argparse
import collections
import struct

EXE = 'bof3/BOF3.exe'
AREA_TABLE, AREAS = 0x667590, 200


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

    # Script starts: each descriptor's +0x10 and +0x1C arrays, walked while
    # the words point into .data. The arrays carry no length; the walk's stop
    # is this tool's bound, not the game's.
    starts = collections.defaultdict(list)   # pointer -> [(area, field, index)]
    for area in range(AREAS):
        d = u32(AREA_TABLE + 4 * area)
        if not in_data(d):
            continue
        for field in (0x10, 0x1C):
            arr = u32(d + field)
            if not in_data(arr):
                continue
            for k in range(256):
                p = u32(arr + 4 * k)
                if not in_data(p) or p >= arr:   # scripts precede their array
                    break
                starts[p].append((area, field, k))
    ordered = sorted(starts)
    print(f'{len(ordered)} distinct script starts in '
          f'{len({a for v in starts.values() for a, _, _ in v})} areas')

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


if __name__ == '__main__':
    main()
