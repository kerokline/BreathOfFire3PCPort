#!/usr/bin/env python
"""Contact sheets for the owner's eye: frames side by side at the game's own
pixels, and the wide picture's middle set against the narrow one
(docs/eye-batch-1013.md).

    python tools/eye_sheet.py sheet --col PC=DIR --col PSX-EU=DIR [--col ...] --out sheet.png
        [--frames t000060,t000300,config_row1] [--title TEXT] [--diff 1,2] [--scale 2]
    python tools/eye_sheet.py wide --narrow DIR --wide DIR --out DIR [--frames ...] [--title TEXT]

A column is a folder of frames (`tNNNNNN.png` timed frames, BOF3X_SHOT_EVERY, or
a recipe's `NAME.png` shots), as tools/abc_run.py / input_run.py write them,
with the run's `bof3x.log` beside them when there is one (each timed frame's
mode and area go in the row label). Every frame is first brought back to the
game's own pixels - the capture is the render target, k screen pixels a game
pixel (DIV-0036), so it is sampled at the centre of each k x k block, k from
the height over 240 - then scaled --scale times (default 2) by nearest
neighbour, so a texel stays a sharp square.

`--diff I,J` adds a column of the pixels that differ between columns I and J
(1-based): the first dimmed to grey, a differing pixel (any channel by more
than 4 of 255, tools/abc_compare.py's measure) red.

`wide`: for every frame both folders hold, the wide capture's middle (the
narrow frame's width, centred: columns 53..372 of 426 at BOF3X_WIDE=1,
DIV-0041) against the narrow capture - the numbers in `wide.tsv` (differing
pixels of the middle, their box) and sheets of rows narrow | wide middle |
difference | the whole wide frame. The middle should equal the narrow frame
(psp-widescreen-a.md 6); what reaches the side bands is for the eye.

Reads only the frames and logs it is given; writes only --out.
"""
import argparse, os, re, sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

TOL = 4
GAME_H = 240
NARROW_W = 320
FRAME_LOG = re.compile(r'input\s+frame (t\d+) (?:saved|NOT saved) mode (\d+) area (\d+)')
SHOT_LOG = re.compile(r'input\s+shot (\S+) recipe frame (\d+)')


def font(size):
    for f in ('consola.ttf', 'arial.ttf', 'DejaVuSans.ttf'):
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            pass
    return ImageFont.load_default()


def native(path):
    """The capture at the game's own pixels: one sample from the centre of each k x k block."""
    im = np.asarray(Image.open(path).convert('RGB'))
    h = im.shape[0]
    k = max(1, round(h / GAME_H))
    if h != k * GAME_H:
        sys.exit(f'{path}: {im.shape[1]} x {h} is not a multiple of {GAME_H} rows')
    return im[k // 2::k, k // 2::k][:GAME_H], k


def scene_of(folder):
    out = {}
    log = os.path.join(folder, 'bof3x.log')
    if os.path.exists(log):
        with open(log, encoding='utf-8', errors='replace') as fh:
            for line in fh:
                if m := FRAME_LOG.search(line):
                    out[m[1]] = f'mode {m[2]} area {m[3]}'
                elif m := SHOT_LOG.search(line):
                    out.setdefault(m[1], f'recipe frame {m[2]}')
    return out


def frames_in(folder):
    return {os.path.splitext(f)[0] for f in os.listdir(folder)
            if f.lower().endswith('.png') and not f.startswith('_')}


def diff_mask(a, b, tol=TOL):
    w = min(a.shape[1], b.shape[1])
    d = np.abs(a[:, :w].astype(int) - b[:, :w].astype(int)).max(axis=2)
    return d > tol


def diff_picture(a, mask):
    g = a.astype(float).mean(axis=2, keepdims=True) * 0.35
    out = np.repeat(g, 3, axis=2).astype(np.uint8)
    out[:mask.shape[0], :mask.shape[1]][mask] = (255, 40, 40)
    return out


def box(mask):
    ys, xs = np.nonzero(mask)
    if not len(xs):
        return ''
    return f'{xs.min()},{ys.min()}-{xs.max() + 1},{ys.max() + 1}'


def draw_rows(rows, heads, out, title, scale, per=8):
    """rows: [(label, [array or None, ...])]; one PNG per `per` rows."""
    pad, lab_w, head_h = 6, 220, 26
    widths = []
    for c in range(len(heads)):
        widths.append(max((r[1][c].shape[1] for r in rows if r[1][c] is not None), default=NARROW_W) * scale)
    row_h = GAME_H * scale
    f, fs = font(16), font(13)
    paths = []
    pages = [rows[i:i + per] for i in range(0, len(rows), per)] or [[]]
    for n, page in enumerate(pages):
        W = lab_w + sum(widths) + pad * (len(widths) + 1)
        H = head_h * 2 + len(page) * (row_h + pad) + pad
        im = Image.new('RGB', (W, H), (24, 24, 24))
        d = ImageDraw.Draw(im)
        d.text((pad, 4), title + (f'  (page {n + 1}/{len(pages)})' if len(pages) > 1 else ''), fill=(255, 255, 255), font=f)
        x = lab_w + pad
        for c, hd in enumerate(heads):
            d.text((x, head_h + 4), hd, fill=(255, 220, 120), font=f)
            x += widths[c] + pad
        y = head_h * 2
        for label, cells in page:
            for i, line in enumerate(label.split('\n')):
                d.text((pad, y + 4 + 16 * i), line, fill=(220, 220, 220), font=fs)
            x = lab_w + pad
            for c, cell in enumerate(cells):
                if cell is not None:
                    big = Image.fromarray(cell).resize((cell.shape[1] * scale, cell.shape[0] * scale), Image.NEAREST)
                    im.paste(big, (x, y))
                else:
                    d.text((x + 8, y + 8), '(no frame)', fill=(255, 90, 90), font=fs)
                x += widths[c] + pad
            y += row_h + pad
        p = out if len(pages) == 1 else re.sub(r'(\.png)?$', f'_{n + 1:02d}.png', out, count=1)
        im.save(p)
        paths.append(p)
    return paths


def cmd_sheet(a):
    cols = [c.split('=', 1) for c in a.col]
    for _, d in cols:
        if not os.path.isdir(d):
            sys.exit(f'no folder {d}')
    names = a.frames.split(',') if a.frames else sorted(set.union(*(frames_in(d) for _, d in cols)))
    scenes = [scene_of(d) for _, d in cols]
    pair = [int(x) - 1 for x in a.diff.split(',')] if a.diff else None
    heads = [lab for lab, _ in cols] + ([f'diff {cols[pair[0]][0]} / {cols[pair[1]][0]}'] if pair else [])
    rows, lines = [], []
    for name in names:
        cells, ks = [], []
        for _, d in cols:
            p = os.path.join(d, name + '.png')
            if os.path.exists(p):
                im, k = native(p)
                cells.append(im)
                ks.append(k)
            else:
                cells.append(None)
        label = name
        sc = {s.get(name) for s in scenes if s.get(name)}
        if sc:
            label += '\n' + '\n'.join(sorted(sc))
        if pair:
            x, y = cells[pair[0]], cells[pair[1]]
            if x is not None and y is not None:
                m = diff_mask(x, y)
                cells.append(diff_picture(x, m))
                label += f'\n{int(m.sum())} px differ'
                lines.append(f'{name}\t{int(m.sum())}\t{box(m)}')
            else:
                cells.append(None)
        rows.append((label, cells))
    paths = draw_rows(rows, heads, a.out, a.title or os.path.basename(a.out), a.scale, a.per)
    if lines:
        with open(os.path.splitext(a.out)[0] + '_diff.tsv', 'w', encoding='utf-8') as fh:
            fh.write('frame\tdiffering_px\tbox\n' + '\n'.join(lines) + '\n')
    print(f'{len(rows)} rows -> ' + ', '.join(paths))


def cmd_wide(a):
    for d in (a.narrow, a.wide):
        if not os.path.isdir(d):
            sys.exit(f'no folder {d}')
    common = sorted(frames_in(a.narrow) & frames_in(a.wide))
    if a.frames:
        want = a.frames.split(',')
        common = [f for f in want if f in common]
    os.makedirs(a.out, exist_ok=True)
    sn, sw = scene_of(a.narrow), scene_of(a.wide)
    rows, lines = [], []
    for name in common:
        n, _ = native(os.path.join(a.narrow, name + '.png'))
        w, _ = native(os.path.join(a.wide, name + '.png'))
        off = (w.shape[1] - n.shape[1]) // 2
        mid = w[:, off:off + n.shape[1]]
        m = diff_mask(n, mid)
        part = '' if sn.get(name) == sw.get(name) else f' (scene parts: {sn.get(name)} / {sw.get(name)})'
        lines.append(f'{name}\t{sn.get(name, "")}\t{w.shape[1]}\t{off}\t{int(m.sum())}\t'
                     f'{100.0 * m.mean():.2f}\t{box(m)}{part}')
        label = f'{name}\n{sn.get(name, "")}\nmiddle: {int(m.sum())} px differ' + ('\nSCENE PARTS' if part else '')
        rows.append((label, [n, mid, diff_picture(n, m), w]))
    with open(os.path.join(a.out, 'wide.tsv'), 'w', encoding='utf-8') as fh:
        fh.write('frame\tscene\twide_w\toffset\tdiffering_px\tpercent\tbox (narrow pixels)\n' + '\n'.join(lines) + '\n')
    paths = draw_rows(rows, ['narrow', 'wide, middle columns', 'difference', 'wide, whole frame'],
                      os.path.join(a.out, 'wide_sheet.png'), a.title or 'wide against narrow', a.scale, a.per)
    print(f'{len(rows)} frames -> {os.path.join(a.out, "wide.tsv")}, ' + ', '.join(paths))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('sheet')
    s.add_argument('--col', action='append', required=True, help='LABEL=DIR, one per column, in order')
    s.add_argument('--frames', help='comma-separated frame names (default: every frame any column holds)')
    s.add_argument('--diff', help='I,J: a column of what differs between columns I and J (1-based)')
    s.add_argument('--out', required=True)
    w = sub.add_parser('wide')
    w.add_argument('--narrow', required=True)
    w.add_argument('--wide', required=True)
    w.add_argument('--frames')
    w.add_argument('--out', required=True, help='a folder: wide.tsv and wide_sheet*.png')
    for p in (s, w):
        p.add_argument('--title')
        p.add_argument('--scale', type=int, default=2)
        p.add_argument('--per', type=int, default=8, help='rows a page')
    a = ap.parse_args()
    {'sheet': cmd_sheet, 'wide': cmd_wide}[a.cmd](a)


if __name__ == '__main__':
    main()
