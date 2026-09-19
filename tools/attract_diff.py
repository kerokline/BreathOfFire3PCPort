#!/usr/bin/env python
"""Compare two attract_watch recordings made from launch.

    python tools/attract_diff.py analysis/attract/a.tsv analysis/attract/b.tsv

Both runs are aligned on the first frame whose area word is 0xFFFF — the load
that begins the first attract scene — and compared as step functions of the
logic frame: at every frame either run logged, the Rand call count, message
index and area word must agree. Exit status 0 if they do, 1 if not.

The recordings count frames from outside the process, so a sampler stall can
cost a frame; --slack N (default 0) tolerates a constant or drifting offset of
up to N frames by comparing the *sequence* of states rather than their frame
numbers, and reports the largest frame disagreement it saw.
"""
import argparse, bisect, sys


def load(path):
    rows = []
    for line in open(path, encoding='utf-8'):
        if line[0] in '#t':
            continue
        t, frames, active, focused, rand_n, msg, area, s0, s1 = line.rstrip('\n').split('\t')
        rows.append(dict(frame=int(frames), active=int(active),
                         rand=None if rand_n == 'None' else int(rand_n),
                         msg=msg, area=area))
    start = next((r['frame'] for r in rows if r['area'] == '0xffff'), None)
    if start is None:
        sys.exit(f'{path}: never reached the attract sequence (no area 0xffff)')
    if any(r['active'] == 0 for r in rows):
        print(f'warning: {path} has inactive rows - the game lost focus during the run')
    pre = [r for r in rows if r['frame'] < start]
    rows = [dict(r, frame=r['frame'] - start) for r in rows if r['frame'] >= start]
    return rows, start, (pre[-1]['rand'] if pre else 0)


def at(rows, frames, f):
    """State of a run at frame f: the last row logged at or before it."""
    i = bisect.bisect_right(frames, f) - 1
    return rows[i] if i >= 0 else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('a')
    ap.add_argument('b')
    ap.add_argument('--slack', type=int, default=0)
    args = ap.parse_args()

    A, startA, preA = load(args.a)
    B, startB, preB = load(args.b)
    print(f'first attract frame: {startA} vs {startB} logic frames after launch')
    print(f'Rand calls before it: {preA} vs {preB}')
    last = min(A[-1]['frame'], B[-1]['frame'])
    print(f'comparing {last + 1} frames from the alignment point')

    ok = True
    if args.slack == 0:
        fa, fb = [r['frame'] for r in A], [r['frame'] for r in B]
        bad = []
        for f in sorted(set(fa + fb)):
            if f > last:
                break
            x, y = at(A, fa, f), at(B, fb, f)
            if (x['rand'], x['msg'], x['area']) != (y['rand'], y['msg'], y['area']):
                bad.append((f, x, y))
        if bad:
            ok = False
            print(f'{len(bad)} frames disagree; first five:')
            for f, x, y in bad[:5]:
                print(f'  +{f}: rand {x["rand"]} msg {x["msg"]} area {x["area"]}'
                      f'  vs  rand {y["rand"]} msg {y["msg"]} area {y["area"]}')
        else:
            print('IDENTICAL at every logged frame: Rand count, message index, area word')
    else:
        def states(rows):
            out = []
            for r in rows:
                if r['frame'] > last:
                    break
                k = (r['rand'], r['msg'], r['area'])
                if not out or out[-1][0] != k:
                    out.append((k, r['frame']))
            return out
        sa, sb = states(A), states(B)
        n = min(len(sa), len(sb))
        mism = next((i for i in range(n) if sa[i][0] != sb[i][0]), None)
        worst = max((abs(sa[i][1] - sb[i][1]) for i in range(mism if mism is not None else n)),
                    default=0)
        if mism is not None:
            ok = False
            print(f'state sequences diverge at state {mism}: '
                  f'{sa[mism]} vs {sb[mism]}')
        else:
            print(f'state SEQUENCES identical over {n} states '
                  f'({len(sa)} vs {len(sb)} logged)')
        print(f'largest frame disagreement before that: {worst} (slack {args.slack})')
        if worst > args.slack:
            ok = False
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
