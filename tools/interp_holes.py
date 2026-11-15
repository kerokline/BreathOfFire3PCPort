#!/usr/bin/env python
"""Find in-between frames that show what neither neighbouring frame shows.

    python tools/interp_holes.py DUMP_DIR [--every 6] [--min 30]

DUMP_DIR is what BOF3X_INTERP_DUMP wrote (docs/frame-interpolation.md
section 2): <Frame_Counter>_real.bmp and _mid.bmp for two passes in every
BOF3X_INTERP_DUMP_EVERY. In time order the pictures are N_real, N_mid,
(N+1)_real: N_mid is the in-between shown after frame N.

For each such triple, a pixel of the in-between counts when one of its
channels is more than 40 outside the range both neighbours have in the
25 x 25 pixels around it: a hole, a stray, a piece paired with the wrong
piece. Triples with more than --min such pixels are listed with the box
they fall in; the last line is the total. A flag is not a fault - a fast
window slide is flagged and correct - so read the listed frames by eye.
What it cannot see: a sprite a step ahead of the ground under it (both
neighbours have its pixels near by).

Needs numpy, scipy and Pillow.
"""
import argparse, os

import numpy as np
from PIL import Image
from scipy.ndimage import maximum_filter, minimum_filter


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dump')
    ap.add_argument('--every', type=int, default=6, help='BOF3X_INTERP_DUMP_EVERY of the run')
    ap.add_argument('--min', type=int, default=30, help='pixels before a triple is listed')
    a = ap.parse_args()

    def load(n, kind):
        return np.asarray(Image.open(os.path.join(a.dump, f'{n:06d}_{kind}.bmp')).convert('RGB')).astype(np.int16)

    def have(n, kind):
        return os.path.exists(os.path.join(a.dump, f'{n:06d}_{kind}.bmp'))

    frames = sorted({int(f[:6]) for f in os.listdir(a.dump) if f.endswith('_real.bmp')})
    triples = flagged = pixels = 0
    for n in frames:
        if n % a.every or not (have(n, 'mid') and have(n + 1, 'real')):
            continue
        before, mid, after = load(n, 'real'), load(n, 'mid'), load(n + 1, 'real')
        triples += 1
        size = (25, 25, 1)
        hi = np.maximum(maximum_filter(before, size=size), maximum_filter(after, size=size))
        lo = np.minimum(minimum_filter(before, size=size), minimum_filter(after, size=size))
        out = ((mid > hi + 40) | (mid < lo - 40)).any(axis=2)
        count = int(out.sum())
        if count > a.min:
            ys, xs = np.nonzero(out)
            flagged += 1
            pixels += count
            print(f'  {n}: {count} pixels in x {xs.min()}..{xs.max()}, y {ys.min()}..{ys.max()}')
    print(f'{a.dump}: {triples} triples, {flagged} flagged, {pixels} pixels')


if __name__ == '__main__':
    main()
