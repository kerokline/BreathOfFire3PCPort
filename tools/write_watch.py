#!/usr/bin/env python
"""Print a log's BOF3X_WRITEWATCH lines with the writers named.

    python tools/write_watch.py                      # game/bof3x.log, build/bof3x.dll
    python tools/write_watch.py LOG --dll DLL

The game logs a `writewatch` line for each new writer of a watched word (and
every 256th write of a known one; src/hook/write_watch.h, docs/write-watch.md).
This reads those lines and names each `bof3x.dll+0x...` with llvm-symbolizer
(tools/gapwatch.py's Symbolizer: the DLL must be the build that wrote the log)
and each `exe:XXXXXXXX` with the symbols.toml function holding it, then prints
one line a hit: the word, before -> after, the writer and its callers, the
frame, the recipe frame and the area. The callers are a scan of the stack for
return addresses into the DLL, not a walk: an old return address left on the
stack can appear among them.
"""
import argparse
import bisect
import os
import re
import sys
import tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gapwatch import Symbolizer  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HIT = re.compile(r'^writewatch  0x([0-9A-F]{8}): ([0-9A-F]{8}) -> ([0-9A-F]{8}) by (\S+) \(that writer\'s write (\d+), '
                 r'the word\'s (\d+)\); frame (\d+), recipe frame (\d+), area (\d+); callers:(.*)$')
REF = re.compile(r'bof3x\.dll\+0x[0-9A-Fa-f]+|exe:[0-9A-F]{8}')


def exe_functions():
    with open(os.path.join(ROOT, 'symbols.toml'), 'rb') as fh:
        doc = tomllib.load(fh)
    rows = sorted((f['pc'], f['name']) for f in doc.get('func', []) if 'pc' in f and 'name' in f)
    return [pc for pc, _ in rows], [name for _, name in rows]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('log', nargs='?', default=os.path.join(ROOT, 'game', 'bof3x.log'))
    ap.add_argument('--dll', default=os.path.join(ROOT, 'build', 'bof3x.dll'))
    a = ap.parse_args()
    with open(a.log, encoding='utf-8', errors='replace') as fh:
        lines = [l.rstrip('\n') for l in fh if l.startswith('writewatch')]
    if not lines:
        sys.exit(f'{a.log}: no writewatch lines (was BOF3X_WRITEWATCH set?)')
    hits = [m for m in (HIT.match(l) for l in lines) if m]
    for l in lines:
        if not HIT.match(l):
            print(l)
    refs = set()
    for m in hits:
        refs.update(REF.findall(m.group(4) + m.group(10)))
    dll = Symbolizer(a.dll).names(sorted(r for r in refs if r.startswith('bof3x.dll+')))
    pcs, names = exe_functions()

    def name(ref):
        if ref.startswith('exe:'):
            va = int(ref[4:], 16)
            i = bisect.bisect_right(pcs, va) - 1
            return f'{names[i]}+0x{va - pcs[i]:X} ({ref})' if i >= 0 else ref
        if ref in dll:
            fn, src = dll[ref]
            return f'{fn} [{src}]' if src else fn
        return ref

    for m in hits:
        word, was, now, by, nth, total, frame, rframe, area, callers = m.groups()
        print(f'0x{word} {was} -> {now}  area {area}, frame {frame}, recipe frame {rframe}, '
              f"write {total} of the word, {nth} of this writer's")
        print(f'    by      {name(by)}')
        for c in REF.findall(callers):
            print(f'    caller  {name(c)}')


if __name__ == '__main__':
    main()
