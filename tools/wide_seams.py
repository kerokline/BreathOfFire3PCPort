#!/usr/bin/env python
"""Find the wide picture's seams: frames where something stops at the old 4:3 edge.

    python tools/wide_seams.py DIR [DIR ...] [--min 0.5] [--jump 40]

For every BOF3X_WIDE=1 capture (426 x 240 game pixels; DIV-0041) in the folders, the
share of rows on which the picture jumps across the narrow frame's edge - between
game columns 52 and 53 (left) and 372 and 373 (right): any channel moving by more than
--jump (default 40 of 255) between the last band column and the first middle column,
and the middle column not black (a dark band beside a dark scene is no seam). A
full-frame fill, fade or flash left at 320 columns shows as a seam on nearly every
row; scenery is continuous and scores low. Frames at or above --min (default 0.5) on
either side are listed with the band and middle colours of the frame's middle row, so
a fill left narrow can be found without looking at every frame
(psp-widescreen-a.md 6 items 1..3, 9, 10). Reads only the frames.
"""
import argparse, os, re, sys

import numpy as np
from PIL import Image

GAME_H, WIDE_W, NARROW_W = 240, 426, 320


def native(path):
    a = np.asarray(Image.open(path).convert('RGB'))
    k = max(1, round(a.shape[0] / GAME_H))
    return a[k // 2::k, k // 2::k][:GAME_H]


def seams(a, jump):
    off = (a.shape[1] - NARROW_W) // 2
    out = []
    for band, mid in ((off - 1, off), (off + NARROW_W, off + NARROW_W - 1)):
        b = a[:, band].astype(int)
        m = a[:, mid].astype(int)
        step = np.abs(b - m).max(axis=1) > jump
        lit = m.max(axis=1) > 24
        out.append(float((step & lit).mean()))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('dirs', nargs='+')
    ap.add_argument('--min', type=float, default=0.5)
    ap.add_argument('--jump', type=int, default=40)
    a = ap.parse_args()
    for d in a.dirs:
        names = sorted(f for f in os.listdir(d) if f.lower().endswith('.png') and not f.startswith('_'))
        hits = 0
        for f in names:
            im = native(os.path.join(d, f))
            if im.shape[1] != WIDE_W:
                continue
            left, right = seams(im, a.jump)
            if max(left, right) >= a.min:
                hits += 1
                y = GAME_H // 2
                off = (im.shape[1] - NARROW_W) // 2
                print(f'{d}\t{os.path.splitext(f)[0]}\tleft {left:.2f}\tright {right:.2f}\t'
                      f'row {y}: band {tuple(int(v) for v in im[y, off - 1])} middle {tuple(int(v) for v in im[y, off])}')
        print(f'# {d}: {len(names)} frames, {hits} at or above {a.min}', file=sys.stderr)


if __name__ == '__main__':
    main()
