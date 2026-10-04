#!/usr/bin/env python
"""Compare runs by the state hash (BOF3X_STATEHASH, src/hook/statehash.cpp):
one hash per 4 KiB page of BOF3.exe's .data, once a logic frame.

    python tools/statehash.py info RUN.sh
    python tools/statehash.py diff A.sh B.sh [--from T] [--to T] [--align recipe]
    python tools/statehash.py check REF.sh REFB.sh NEW.sh [--from T] [--to T]
    python tools/statehash.py bytes A.sh.T.bin B.sh.T.bin [--skip-out FILE]

diff   the first tick at which two runs differ, and per page the ticks it
       differed on (first, last, count), with the symbols the page holds.
check  the regression question: REF and REFB are two runs of the reference
       side, NEW the side under test. A page is noise at a tick when the two
       references disagree there; what is reported is NEW against REF on every
       page and tick where the references agree. Exit 1 when anything is.
bytes  two raw dumps (BOF3X_STATEHASH_DUMP) compared byte for byte: the
       differing ranges with their symbols, and with --skip-out a skip list
       (BOF3X_STATEHASH_SKIP) of them to review - never feed it back unread.

Runs are aligned by tick (the logic frames the latch saw, from the first) or,
with --align recipe, by the recipe frame of a scripted run. docs/state-hash.md
has the method and what the hash does not see.
"""
import argparse, bisect, os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE, END = 0x5DA000, 0x93E000


class Run:
    def __init__(self, path, align='tick'):
        d = open(path, 'rb').read()
        if d[:8] != b'BOF3SH1\0':
            sys.exit(f'{path}: not a state hash file')
        self.base, self.pages, self.page, self.skips = struct.unpack_from('<4I', d, 8)
        self.path = path
        self.keys = []      # per record: the alignment key
        self.fc = []        # per record: Frame_Counter
        self.recs = []      # per record: [(page, hash)...] changed
        o = 24
        while o + 16 <= len(d):
            tick, fc, rf, n = struct.unpack_from('<4I', d, o)
            if o + 16 + 8 * n > len(d):
                break       # the game was killed inside a record
            ch = struct.unpack_from(f'<{2 * n}I', d, o + 16)
            self.keys.append(rf if align == 'recipe' else tick)
            self.fc.append(fc)
            self.recs.append(ch)
            o += 16 + 8 * n

    def frames(self):
        """Yield (key, Frame_Counter, state list) per record; the list is reused."""
        state = [0] * self.pages
        for key, fc, ch in zip(self.keys, self.fc, self.recs):
            for i in range(0, len(ch), 2):
                state[ch[i]] = ch[i + 1]
            yield key, fc, state


_syms = None


def symbols():
    """(sorted addresses, names) of every symbols.toml entry inside .data."""
    global _syms
    if _syms is None:
        import tomllib
        with open(os.path.join(ROOT, 'symbols.toml'), 'rb') as f:
            t = tomllib.load(f)
        rows = []
        for kind in t.values():
            if not isinstance(kind, list):
                continue
            for e in kind:
                pc = e.get('pc') if isinstance(e, dict) else None
                if isinstance(pc, str):
                    try:
                        pc = int(pc, 0)
                    except ValueError:
                        continue
                if isinstance(pc, int) and BASE <= pc < END and e.get('name'):
                    rows.append((pc, e['name']))
        rows.sort()
        _syms = ([a for a, _ in rows], [n for _, n in rows])
    return _syms


def names_in(lo, hi, limit=4):
    addrs, names = symbols()
    i, j = bisect.bisect_left(addrs, lo), bisect.bisect_left(addrs, hi)
    out = [f'{names[k]}' for k in range(i, min(j, i + limit))]
    if j - i > limit:
        out.append(f'+{j - i - limit}')
    if not out and i > 0:
        out = [f'({names[i - 1]}+0x{lo - addrs[i - 1]:X})']
    return ' '.join(out)


def at(lo):
    addrs, names = symbols()
    i = bisect.bisect_right(addrs, lo) - 1
    return f'{names[i]}+0x{lo - addrs[i]:X}' if i >= 0 else '?'


def walk(runs, lo, hi):
    """Step the runs together by key; yield (key, [state...]) where all have it."""
    its = [r.frames() for r in runs]
    cur = [next(it, None) for it in its]
    while all(c is not None for c in cur):
        k = max(c[0] for c in cur)
        for n, it in enumerate(its):
            while cur[n] is not None and cur[n][0] < k:
                cur[n] = next(it, None)
        if any(c is None for c in cur):
            return
        if any(c[0] != k for c in cur):
            continue
        if hi is not None and k > hi:
            return
        if k >= lo:
            yield k, [c[2] for c in cur]
        cur = [next(it, None) for it in its]


def report(per_page, run, total, what):
    if not per_page:
        print(f'{what}: identical on all {total} compared ticks')
        return
    first = min(v[0] for v in per_page.values())
    print(f'{what}: {len(per_page)} pages differ over {total} compared ticks; the first at tick {first}')
    print('page       first    last   ticks  symbols')
    for p, (f, l, c) in sorted(per_page.items(), key=lambda kv: (kv[1][0], kv[0])):
        a = run.base + p * run.page
        print(f'0x{a:06X} {f:7d} {l:7d} {c:7d}  {names_in(a, a + run.page)}')


def cmd_info(a):
    r = Run(a.run)
    print(f'{a.run}: {len(r.recs)} records, ticks {r.keys[0]}..{r.keys[-1]}, Frame_Counter {r.fc[0]}..{r.fc[-1]}, '
          f'{r.pages} pages of 0x{r.page:X} from 0x{r.base:X}, {r.skips} skip ranges')
    busy = {}
    for ch in r.recs[1:]:
        for i in range(0, len(ch), 2):
            busy[ch[i]] = busy.get(ch[i], 0) + 1
    print(f'{len(busy)} pages changed at least once after the first record')


def cmd_diff(a):
    ra, rb = Run(a.a, a.align), Run(a.b, a.align)
    per, total = {}, 0
    for k, (sa, sb) in walk([ra, rb], a.lo, a.hi):
        total += 1
        if sa != sb:
            for p in range(ra.pages):
                if sa[p] != sb[p]:
                    v = per.get(p)
                    per[p] = (k, k, 1) if v is None else (v[0], k, v[2] + 1)
    report(per, ra, total, 'diff')
    return 1 if per else 0


def cmd_check(a):
    ref, refb, new = Run(a.ref, a.align), Run(a.refb, a.align), Run(a.new, a.align)
    noise, per, total = {}, {}, 0
    for k, (sr, sb, sn) in walk([ref, refb, new], a.lo, a.hi):
        total += 1
        if sr == sb == sn:
            continue
        for p in range(ref.pages):
            if sr[p] != sb[p]:
                v = noise.get(p)
                noise[p] = (k, k, 1) if v is None else (v[0], k, v[2] + 1)
            elif sr[p] != sn[p]:
                v = per.get(p)
                per[p] = (k, k, 1) if v is None else (v[0], k, v[2] + 1)
    print(f'{total} ticks compared; the references disagree on {len(noise)} pages '
          f'({sum(1 for v in noise.values() if v[2] == total)} of them at every tick)')
    if a.noise:
        report(noise, ref, total, 'noise (reference against reference)')
    report(per, ref, total, 'check (new against the reference, where the references agree)')
    return 1 if per else 0


def cmd_bytes(a):
    da, db = open(a.a, 'rb').read(), open(a.b, 'rb').read()
    if len(da) != END - BASE or len(db) != END - BASE:
        sys.exit('a dump is not the size of .data')
    ranges, i, n = [], 0, len(da)
    while i < n:
        if da[i] != db[i]:
            j = i + 1
            # one range across gaps of equal bytes shorter than --gap
            last = i
            while j < n and j - last <= a.gap:
                if da[j] != db[j]:
                    last = j
                j += 1
            ranges.append((BASE + i, last - i + 1))
            i = last + 1
        else:
            i += 1
    print(f'{len(ranges)} differing ranges, {sum(l for _, l in ranges)} bytes')
    out = open(a.skip_out, 'w') if a.skip_out else None
    for addr, ln in ranges:
        o = addr - BASE
        sa, sb = da[o:o + min(ln, 8)].hex(), db[o:o + min(ln, 8)].hex()
        print(f'0x{addr:06X} {ln:6d}  {sa:16s} {sb:16s}  {at(addr)}')
        if out:
            out.write(f'0x{addr:06X} {ln}  # {at(addr)}\n')
    return 1 if ranges else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)

    def window(p):
        p.add_argument('--from', dest='lo', type=int, default=0)
        p.add_argument('--to', dest='hi', type=int, default=None)
        p.add_argument('--align', choices=['tick', 'recipe'], default='tick')

    p = sub.add_parser('info'); p.add_argument('run'); p.set_defaults(fn=cmd_info)
    p = sub.add_parser('diff'); p.add_argument('a'); p.add_argument('b'); window(p); p.set_defaults(fn=cmd_diff)
    p = sub.add_parser('check'); p.add_argument('ref'); p.add_argument('refb'); p.add_argument('new'); window(p)
    p.add_argument('--noise', action='store_true', help='list the noise pages too'); p.set_defaults(fn=cmd_check)
    p = sub.add_parser('bytes'); p.add_argument('a'); p.add_argument('b'); p.add_argument('--skip-out')
    p.add_argument('--gap', type=int, default=3, help='equal bytes a range may span (default 3)')
    p.set_defaults(fn=cmd_bytes)
    a = ap.parse_args()
    sys.exit(a.fn(a) or 0)


if __name__ == '__main__':
    main()
