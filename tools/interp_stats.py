#!/usr/bin/env python
"""Read the in-between's statistics lines out of a game log.

    python tools/interp_stats.py LOG [LOG ...] [--top 15] [--csv FILE]

LOG is a bof3x.log of a run with BOF3X_INTERP=1 and BOF3X_INTERP_STATS=1
(docs/frame-interpolation.md section 2): one `interp-stat` line for each
in-between, with what the matcher did with the frame's draws, rule by rule.
It needs no picture dump, so it covers every in-between of a route.

For each log: a summary line, then the in-betweens to read by eye, worst
first. An in-between is listed for:

  loose    draws shown where frame N+1 has them beside draws that moved:
           no pair and not following a neighbour, or in a broken run
           (not counted when the whole frame is a cut, or nothing moved)
  changed  cels found by shape or by place (rules 3), which carry a motion
  slow     the matcher took more than 8 ms, or dropped its order table

A listed in-between is not a fault; it is where the matcher was least sure.
`cut` frames are shown unmoved whole, by design, and only counted - but a
run of cuts in a scene that is not changing is worth a look, so runs of
three or more are listed.

--csv appends one row a log (the route's name is the log's file name), for a
chart over commits.
"""
import argparse, csv, os, re, subprocess

FIELDS = ('draws order exact loose carried gap lone far unmatched followed moved snapped runs cut dropped').split()


def read(path):
    rows, late = [], 0
    for line in open(path, encoding='utf-8', errors='replace'):
        if not line.startswith('interp-stat'):
            continue
        if ' none ' in line:
            late += 1
            continue
        m = re.search(r'fc (\d+) (.*) ms ([\d.]+) after ([\d.]+)', line)
        if not m:
            continue
        words = m[2].split()
        row = dict(zip(words[0::2], (int(w) for w in words[1::2])))
        row.update(fc=int(m[1]), ms=float(m[3]), after=float(m[4]))
        rows.append(row)
    return rows, late


def unpaired(r):
    """Draws shown unmoved in an in-between where other draws moved."""
    if r['cut'] or not r['moved'] or r['draws'] < 16:
        return 0
    return max(0, r['far'] + r['unmatched'] - r['followed']) + r['snapped']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('logs', nargs='+')
    ap.add_argument('--top', type=int, default=15, help='in-betweens listed for each log')
    ap.add_argument('--csv', help='append a row for each log')
    a = ap.parse_args()
    out = None
    if a.csv:
        new = not os.path.exists(a.csv)
        out = csv.writer(open(a.csv, 'a', newline=''))
        if new:
            out.writerow(['commit', 'route', 'in_betweens', 'late', 'cuts', 'cut_runs', 'loose_frames', 'loose_draws',
                          'changed_cels', 'slow', 'ms_mean', 'ms_max'])
    try:
        commit = subprocess.run(['git', 'rev-parse', '--short', 'HEAD'], capture_output=True, text=True,
                                cwd=os.path.dirname(os.path.abspath(__file__))).stdout.strip()
    except OSError:
        commit = ''
    for path in a.logs:
        rows, late = read(path)
        name = os.path.splitext(os.path.basename(path))[0]
        if not rows:
            print(f'{name}: no interp-stat lines')
            continue
        cuts = [r['fc'] for r in rows if r['cut']]
        # Runs of cuts: consecutive in-betweens (a late pass between them breaks one).
        runs, start, prev = [], None, None
        for fc in cuts + [None]:
            if prev is not None and fc == prev + 1:
                prev = fc
                continue
            if start is not None and prev - start >= 2:
                runs.append((start, prev))
            start = prev = fc
        loose = [(unpaired(r), r) for r in rows if unpaired(r) * 20 > r['draws'] or unpaired(r) >= 24]
        changed = sum(r['carried'] + r['gap'] for r in rows if not r['cut'])
        slow = [r for r in rows if r['ms'] > 8 or r['dropped']]
        ms = [r['ms'] for r in rows]
        print(f'{name}: {len(rows)} in-betweens, {late} not shown after a late frame; {len(cuts)} cuts '
              f'({len(runs)} runs of 3 or more); {len(loose)} with loose draws ({sum(n for n, _ in loose)} draws); '
              f'{changed} changed cels carried; {len(slow)} slow; match {sum(ms) / len(ms):.2f} ms each, {max(ms):.1f} at most')
        if out:
            out.writerow([commit, name, len(rows), late, len(cuts), len(runs), len(loose), sum(n for n, _ in loose), changed,
                          len(slow), f'{sum(ms) / len(ms):.2f}', f'{max(ms):.1f}'])
        for first, last in runs[:a.top]:
            print(f'  cuts {first}..{last}')
        for n, r in sorted(loose, key=lambda x: -x[0] / max(1, x[1]['draws']))[:a.top]:
            print(f"  loose {r['fc']}: {n} of {r['draws']} draws (too far {r['far']}, no candidate {r['unmatched']}, "
                  f"followed {r['followed']}, broken runs {r['runs']} with {r['snapped']} draws)")
        for r in sorted(slow, key=lambda r: -r['ms'])[:a.top]:
            print(f"  slow {r['fc']}: {r['ms']:.1f} ms, {r['draws']} draws" + (', order table dropped' if r['dropped'] else ''))


if __name__ == '__main__':
    main()
