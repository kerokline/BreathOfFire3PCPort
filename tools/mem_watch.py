#!/usr/bin/env python
"""Log every change of a few byte ranges in a running BOF3.exe.

    python tools/mem_watch.py --seconds 300 8034E0:1 9039F0:10
    python tools/mem_watch.py --seconds 300 --label menu 8034E0:1

Each argument is HEXADDR:LENGTH. One line per change: seconds since start,
the range, and its bytes. Read-only, as tools/task_stacks.py (whose process
access this reuses): nothing is written to the game and no input is sent.

Sampling is every ~5 ms against a 33 ms logic frame, so a value that lives for
one frame can still be missed if this script stalls.
"""
import argparse, os, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import task_stacks as ts


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('ranges', nargs='+', help='HEXADDR:LENGTH')
    ap.add_argument('--seconds', type=float, default=60)
    ap.add_argument('--label', help='also save under analysis/memwatch/<label>.tsv')
    a = ap.parse_args()
    ranges = []
    for r in a.ranges:
        addr, _, n = r.partition(':')
        ranges.append((int(addr, 16), int(n or '1', 0)))
    game = ts.Game(ts.find_game())
    out = None
    if a.label:
        d = os.path.join(ts.ROOT, 'analysis', 'memwatch')
        os.makedirs(d, exist_ok=True)
        out = open(os.path.join(d, a.label + '.tsv'), 'w')
    last, t0 = {}, time.time()
    while time.time() - t0 < a.seconds:
        for addr, n in ranges:
            b = game.read(addr, n)
            if b is None:
                sys.exit('read failed at 0x%X - has the game closed?' % addr)
            if last.get(addr) != b:
                last[addr] = b
                line = '%8.2f\t0x%06X\t%s' % (time.time() - t0, addr, b.hex(' '))
                print(line, flush=True)
                if out:
                    out.write(line + '\n'); out.flush()
        time.sleep(0.005)


if __name__ == '__main__':
    main()
