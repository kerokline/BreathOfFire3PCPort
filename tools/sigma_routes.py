#!/usr/bin/env python
"""The swapped twin of a button route (docs/button-routes.md).

    python tools/sigma_routes.py variant tools/recipes/sigma_worldmap.txt tools/recipes/sigma_worldmap_sw.txt
    python tools/sigma_routes.py check                      # every sigma_*.txt against its _sw twin

A route is a recorded-form recipe (wait / hold / one-frame shot lines only,
as tools/recipe_shots.py writes them) with two header lines:

    # save NAME          the identity save (tools/recipe_saves.py's header)
    # sigma-from FRAME   the first frame after the save's load is applied

The twin plays the same frames from the save NAME_sw (tools/sigma_saves.py
swap: Speak <-> Action, View <-> Change) and, from FRAME on, presses the
swapped button wherever the route pressed the original: circle <-> cross,
L1 <-> R1. Before FRAME the title and the load menu run on no save's Config,
so their presses stay. R2 and L2 stay everywhere: the panel ORs them into
Speak and the confirm word whatever the swap (ConfigScreen_Controller), so a
route that confirms with R2 confirms on both sides. Square (Move) and
triangle (Menu) are not swapped. No frame moves: a press is renamed, never
added or removed, and the shot lines keep their places.
"""
import argparse, glob, os, re, sys

SWAP = {'circle': 'cross', 'cross': 'circle', 'l1': 'r1', 'r1': 'l1'}
HERE = os.path.dirname(os.path.abspath(__file__))


def swap_buttons(b):
    return '+'.join(SWAP.get(x, x) for x in b.split('+'))


def parse(path):
    header, steps = [], []
    for line in open(path, encoding='utf-8'):
        s = line.rstrip('\n')
        t = s.split('#')[0].split()
        if not t:
            (header if not steps else steps).append(('c', s))
            continue
        steps.append(('s', t))
    return header, steps


def head_value(header, key):
    for _, s in header:
        m = re.match(rf'#\s*{key}\s+(\S+)', s)
        if m:
            return m[1]
    return None


def variant(src, dst):
    header, steps = parse(src)
    save, start = head_value(header, 'save'), head_value(header, 'sigma-from')
    if not save or start is None:
        sys.exit(f'{src}: wants `# save NAME` and `# sigma-from FRAME` header lines')
    start = int(start)
    out = []
    for _, s in header:
        if re.match(r'#\s*save\s', s):
            s = f'# save {save}_sw'
        out.append(s)
    out.insert(len(out), f'# The swapped twin of {os.path.basename(src)}, written by tools/sigma_routes.py: '
                         f'from frame {start} circle <-> cross, L1 <-> R1.')
    f = 0
    for kind, t in steps:
        if kind == 'c':
            out.append(t)
            continue
        op = t[0]
        if op == 'wait':
            n = int(t[1])
            out.append(f'wait {n}')
        elif op == 'hold':
            n = int(t[2])
            if f >= start:
                out.append(f'hold {swap_buttons(t[1])} {n}')
            elif f + n <= start:
                out.append(f'hold {t[1]} {n}')
            else:                                   # a press across the load: split it at FRAME
                out.append(f'hold {t[1]} {start - f}')
                out.append(f'hold {swap_buttons(t[1])} {f + n - start}')
        elif op == 'shot' and len(t) >= 3 and t[2] == '1':
            n = 1
            b = t[3] if len(t) == 4 else ''
            if b and f >= start:
                b = swap_buttons(b)
            out.append(f'shot {t[1]} 1' + (f' {b}' if b else ''))
        else:
            sys.exit(f'{src}: not a recorded-form line: {" ".join(t)}')
        f += n
    with open(dst, 'w', newline='\n', encoding='utf-8') as fh:
        fh.write('\n'.join(out) + '\n')
    print(f'{dst}: {f} frames, the swap from frame {start}, save {save}_sw')
    return f


def frames(path):
    _, steps = parse(path)
    n = 0
    for kind, t in steps:
        if kind != 's':
            continue
        n += int(t[1]) if t[0] == 'wait' else int(t[2]) if t[0] == 'hold' else 1
    return n


def check():
    bad = 0
    for p in sorted(glob.glob(os.path.join(HERE, 'recipes', 'sigma_*.txt'))):
        if p.endswith('_sw.txt') or os.path.basename(p) == 'sigma_swap.txt':
            continue
        tw = p[:-4] + '_sw.txt'
        if not os.path.exists(tw):
            print(f'{os.path.basename(p)}: no twin')
            bad += 1
            continue
        tmp = tw + '.check'
        variant(p, tmp)
        same = open(tmp, encoding='utf-8').read() == open(tw, encoding='utf-8').read()
        os.remove(tmp)
        print(f'{os.path.basename(tw)}: {"current" if same else "STALE: regenerate"}')
        bad += not same
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('variant'); p.add_argument('src'); p.add_argument('dst')
    sub.add_parser('check')
    a = ap.parse_args()
    if a.cmd == 'variant':
        variant(a.src, a.dst)
    else:
        sys.exit(check())


if __name__ == '__main__':
    main()
