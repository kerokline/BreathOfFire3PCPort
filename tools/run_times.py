#!/usr/bin/env python
"""Sum analysis/run_times.tsv, the wall-clock log attract_run.py and
input_run.py append to: what the live runs (attract, A/B sides, recipes) cost.

    python tools/run_times.py [--since "2026-10-03"] [--by tool|what|side]

`side` splits each tool's runs into all-original, mixed and ours by
BOF3X_ORIGINAL. For rows with a frame count, `pace_s` is frames / 60: the part
of the wall time the game's own pace accounts for, and so the most a
fast-forward could remove.
"""
import argparse, collections, csv, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def side(r):
    o = r['original']
    return 'ours' if not o else 'original' if o.split(',')[0] == '*' else 'mixed'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--since', default='', help='a start prefix to compare against, e.g. "2026-10-03 08"')
    ap.add_argument('--by', default='tool', choices=['tool', 'what', 'side'])
    ap.add_argument('--file', default=os.path.join(ROOT, 'analysis', 'run_times.tsv'))
    a = ap.parse_args()
    rows = [r for r in csv.DictReader(open(a.file, encoding='utf-8'), delimiter='\t') if r['start'] >= a.since]
    key = {'tool': lambda r: r['tool'], 'what': lambda r: r['tool'] + ' ' + r['what'],
           'side': lambda r: r['tool'] + ' ' + side(r)}[a.by]
    t = collections.defaultdict(lambda: [0, 0.0, 0.0, 0])
    for r in rows:
        v = t[key(r)]
        v[0] += 1
        v[1] += float(r['wall_s'])
        if r['frames'].isdigit():
            v[2] += int(r['frames']) / 60
            v[3] += 1
    print(f'{"":32} runs   wall_min  pace_min (runs with frames)')
    for k in sorted(t, key=lambda k: -t[k][1]):
        n, wall, pace, nf = t[k]
        print(f'{k:32} {n:4} {wall / 60:10.1f} {pace / 60:9.1f} ({nf})')
    print(f'{"total":32} {len(rows):4} {sum(float(r["wall_s"]) for r in rows) / 60:10.1f}')


if __name__ == '__main__':
    main()
