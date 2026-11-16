#!/usr/bin/env python
"""Find in-between frames with a part that is not halfway between its frames.

    python tools/interp_motion.py DUMP_DIR [--every 6] [--min 3] [--frames 550-590]
                                  [--mark OUT_DIR] [--jobs 8]

A proof of concept (docs/frame-interpolation.md section 4). DUMP_DIR is what
BOF3X_INTERP_DUMP wrote; in time order the pictures are N_real, N_mid,
(N+1)_real.

tools/interp_holes.py finds pixels neither neighbour has. It cannot see a
thing drawn whole in the wrong place: a figure that stands where frame N+1
has it while the ground under it goes halfway, or a sprite a step ahead of
the ground. This looks for those. The in-between is cut into blocks of 16
pixels. For each block and each shift s within --reach, it measures how well
the block matches frame N at -s and frame N+1 at +s. A part that is halfway
has one s that is good for both: the same step back as forward. A block is
counted when each neighbour has a good match somewhere but no single s is
good for both - it matches the two frames at places that are not opposite.

A flag is not a fault. Flagged by design: a frame shown unmoved after a cut
or a broken run (it is frame N+1 again), a line of letters that changed, a
carried figure whose cel changed. Not seen: a part whose cel changed
outright (it matches neither frame), a flat part, a part that moved further
than the reach. Read flagged frames by eye; --mark writes the three pictures
one above the other with the counted blocks boxed.

Needs numpy and Pillow.
"""
import argparse, os
from multiprocessing import Pool

import numpy as np
from PIL import Image, ImageDraw

B = 16   # block, pixels


def gray(path):
    return np.asarray(Image.open(path).convert('L')).astype(np.int16)


def costs(mid, other, reach):
    """Mean absolute difference of each block of mid against other moved by
    each shift: [shift, block row, block column]; the shifts in order."""
    h, w = mid.shape
    hb, wb = (h - 2 * reach) // B, (w - 2 * reach) // B
    core = mid[reach:reach + hb * B, reach:reach + wb * B]
    shifts = [(dy, dx) for dy in range(-reach, reach + 1) for dx in range(-reach, reach + 1)]
    out = np.empty((len(shifts), hb, wb), np.float32)
    for k, (dy, dx) in enumerate(shifts):
        d = np.abs(core - other[reach + dy:reach + dy + hb * B, reach + dx:reach + dx + wb * B])
        out[k] = d.reshape(hb, B, wb, B).sum(axis=(1, 3), dtype=np.int32) / float(B * B)
    return out, shifts, core.reshape(hb, B, wb, B)


def measure(job):
    dump, n, a = job
    name = lambda m, kind: os.path.join(dump, f'{m:06d}_{kind}.bmp')
    before, mid, after = gray(name(n, 'real')), gray(name(n, 'mid')), gray(name(n + 1, 'real'))
    ca, shifts, blocks = costs(mid, before, a.reach)
    cb, _, _ = costs(mid, after, a.reach)
    # cb for shift s is frame N+1 at +s; the same step back in frame N is -s,
    # which is the shift list read backwards.
    both = (ca[::-1] + cb).min(axis=0)
    free_a, free_b = ca.min(axis=0), cb.min(axis=0)
    textured = blocks.transpose(0, 2, 1, 3).reshape(blocks.shape[0], blocks.shape[2], -1).std(axis=2) > a.texture
    flagged = textured & (free_a < a.good) & (free_b < a.good) & (both - free_a - free_b > a.apart)
    cells = np.argwhere(flagged)
    found = []
    for r, c in cells:
        sa = shifts[len(shifts) - 1 - int(ca[:, r, c].argmin())]   # as a step back: frame N at -sa
        sb = shifts[int(cb[:, r, c].argmin())]
        found.append((int(r), int(c), sa, sb))
    if a.mark and len(found) >= a.min:
        strip = Image.new('RGB', (mid.shape[1], mid.shape[0] * 3))
        for k, (m, kind) in enumerate(((n, 'real'), (n, 'mid'), (n + 1, 'real'))):
            strip.paste(Image.open(name(m, kind)).convert('RGB'), (0, k * mid.shape[0]))
        draw = ImageDraw.Draw(strip)
        for r, c, _, _ in found:
            x, y = a.reach + c * B, a.reach + r * B + mid.shape[0]
            draw.rectangle([x, y, x + B - 1, y + B - 1], outline=(255, 0, 255))
        strip.save(os.path.join(a.mark, f'{n:06d}.png'))
    return n, found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dump')
    ap.add_argument('--every', type=int, default=6, help='BOF3X_INTERP_DUMP_EVERY of the run; 1 for a dump of every frame')
    ap.add_argument('--min', type=int, default=3, help='blocks before a triple is listed')
    ap.add_argument('--frames', help='FIRST-LAST, the Frame_Counter range to read')
    ap.add_argument('--reach', type=int, default=20, help='the furthest shift tried, pixels')
    ap.add_argument('--good', type=float, default=5.0, help='a match: mean difference of a block, levels')
    ap.add_argument('--apart', type=float, default=10.0, help='how much worse the best opposite pair must be')
    ap.add_argument('--texture', type=float, default=14.0, help="a block's standard deviation before it is read")
    ap.add_argument('--mark', help='directory for the marked pictures of listed triples')
    ap.add_argument('--jobs', type=int, default=8)
    a = ap.parse_args()
    if a.mark:
        os.makedirs(a.mark, exist_ok=True)
    first, last = (int(x) for x in a.frames.split('-')) if a.frames else (0, 1 << 30)
    have = set(os.listdir(a.dump))
    jobs = []
    for n in sorted({int(f[:6]) for f in have if f.endswith('_mid.bmp')}):
        if n % a.every or not first <= n <= last:
            continue
        if f'{n:06d}_real.bmp' in have and f'{n + 1:06d}_real.bmp' in have:
            jobs.append((a.dump, n, a))
    flagged = blocks = 0
    with Pool(a.jobs) as pool:
        for n, found in pool.imap(measure, jobs, chunksize=4):
            if len(found) < a.min:
                continue
            flagged += 1
            blocks += len(found)
            rows, cols = [f[0] for f in found], [f[1] for f in found]
            # The commonest pair of steps among the blocks.
            steps = {}
            for _, _, sa, sb in found:
                steps[(sa, sb)] = steps.get((sa, sb), 0) + 1
            (sa, sb), count = max(steps.items(), key=lambda kv: kv[1])
            print(f'  {n}: {len(found)} blocks in x {a.reach + min(cols) * B}..{a.reach + max(cols) * B + B - 1}, '
                  f'y {a.reach + min(rows) * B}..{a.reach + max(rows) * B + B - 1}; '
                  f'{count} of them {sa[1]:+d},{sa[0]:+d} from frame N and {sb[1]:+d},{sb[0]:+d} to frame N+1')
    print(f'{a.dump}: {len(jobs)} triples, {flagged} flagged, {blocks} blocks')


if __name__ == '__main__':
    main()
