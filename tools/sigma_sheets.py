#!/usr/bin/env python
"""Contact sheets for the button routes (docs/button-routes.md): one sheet per
site, a row per install kind, the identity run's frame beside the swapped
run's for each of the site's frames.

    python tools/sigma_sheets.py --root C:/Users/kerok/b_rc                     # every site
    python tools/sigma_sheets.py --root C:/Users/kerok/b_rc --site worldmap --out DIR
    python tools/sigma_sheets.py --list
    python tools/sigma_sheets.py --root R --id-tag id --sw-tag id2 --sw-suffix ''   # two identity runs side by side

The runs are tools/abc_run.py's, made with the button installs file
(tools/recipes/sigma-installs.toml) and two tags: the identity route
`sigma_<site>.txt` under `--id-tag` (default `id`), its swapped twin
`sigma_<site>_sw.txt` under `--sw-tag` (default `sw`), so a frame is
`<root>/runs/<tag>_<route>/<install>/<frame>.png`. A frame that is missing (a
run not made yet, a route that ended early or crashed) is drawn as a grey
cell saying so, never left out, so a sheet always has the same shape.

The site table is docs/button-routes.md's `<!-- sites -->` block: one line
per frame, `site route frame crop label`, the crop a box in 320 x 240 game
pixels (`-` the whole frame). A crop narrower than 160 is drawn at 2x, a
whole frame at 0.75x (--scale overrides both).

The sheets are pictures of the game: they go under the root (default
`<root>/sheets`), outside the tree (rule 1).
"""
import argparse, os, sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INSTALLS = (('pc', 'PC (bof3)'), ('psx-us', 'PSX US disc only'), ('psp-eu', 'PSP-EU only'),
            ('pc-enus', 'PC + en-US layer'))


def load_sites(doc=os.path.join(HERE, 'docs', 'button-routes.md')):
    sites = {}
    on = False
    for line in open(doc, encoding='utf-8'):
        s = line.strip()
        if s == '<!-- sites -->':
            on = not on
            continue
        if not on or not s or s.startswith('#') or s.startswith('```'):
            continue
        parts = s.split(None, 4)
        if len(parts) < 4:
            continue
        site, route, frame, crop = parts[:4]
        label = parts[4] if len(parts) > 4 else frame
        box = None if crop == '-' else tuple(int(v) for v in crop.split(','))
        sites.setdefault(site, {'route': route, 'frames': []})['frames'].append((frame, label, box))
    return sites


def size_of(box, scale):
    w, h = (box[2] - box[0], box[3] - box[1]) if box else (320, 240)
    if scale is None:
        scale = 2.0 if box and w < 160 else 0.75 if not box else 1.0
    return int(w * scale), int(h * scale)


def cell(path, box, size):
    if path and os.path.exists(path):
        im = Image.open(path).convert('RGB')
        if box:
            sx, sy = im.width / 320, im.height / 240
            im = im.crop((int(box[0] * sx), int(box[1] * sy), int(box[2] * sx), int(box[3] * sy)))
        return im.resize(size, Image.NEAREST)
    im = Image.new('RGB', size, (96, 96, 96))
    ImageDraw.Draw(im).text((6, size[1] // 2 - 6), 'no frame', fill=(230, 230, 230))
    return im


def sheet(root, site, e, a):
    route = e['route']
    sizes = [size_of(box, a.scale) for _, _, box in e['frames']]
    label_w, head_h, gap = 130, 34, 6
    row_h = max(h for _, h in sizes)
    W = label_w + sum(2 * (w + gap) for w, _ in sizes) + gap
    H = head_h + len(INSTALLS) * (row_h + gap) + gap
    out = Image.new('RGB', (W, H), (24, 24, 28))
    d = ImageDraw.Draw(out)
    xs, x = [], label_w + gap
    for (frame, label, _), (w, _) in zip(e['frames'], sizes):
        xs.append(x)
        d.text((x, 4), f'{label} ({frame})', fill=(255, 255, 255))
        d.text((x, 18), 'identity', fill=(170, 220, 170))
        d.text((x + w + gap, 18), 'swapped', fill=(240, 190, 120))
        x += 2 * (w + gap)
    found = 0
    sides = ((a.id_tag, route), (a.sw_tag, route + a.sw_suffix))
    for r, (inst, name) in enumerate(INSTALLS):
        y = head_h + r * (row_h + gap) + gap
        d.text((6, y + row_h // 2 - 6), name, fill=(255, 255, 255))
        for (frame, _, box), (w, h), x in zip(e['frames'], sizes, xs):
            for j, (tag, rt) in enumerate(sides):
                p = os.path.join(root, 'runs', f'{tag}_{rt}', inst, frame + '.png')
                found += os.path.exists(p)
                out.paste(cell(p, box, (w, h)), (x + j * (w + gap), y))
    os.makedirs(a.out, exist_ok=True)
    dst = os.path.join(a.out, f'{site}.png')
    out.save(dst)
    n = 2 * len(e['frames']) * len(INSTALLS)
    print(f'{dst}: {len(e["frames"])} frame(s) x {len(INSTALLS)} installs, {found} of {n} cells found')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--root', default=os.environ.get('BOF3X_ABC_ROOT', os.path.join(os.path.expanduser('~'), 'abc')))
    ap.add_argument('--site', action='append', help='a site (repeat); default every site in the table')
    ap.add_argument('--out', help='default <root>/sheets')
    ap.add_argument('--id-tag', default='id')
    ap.add_argument('--sw-tag', default='sw')
    ap.add_argument('--sw-suffix', default='_sw', help="the right column's route suffix ('' for a second identity run)")
    ap.add_argument('--scale', type=float, default=None)
    ap.add_argument('--list', action='store_true')
    a = ap.parse_args()
    sites = load_sites()
    if not sites:
        sys.exit('no sites: docs/button-routes.md has no <!-- sites --> block')
    if a.list:
        for s, e in sites.items():
            print(f'{s}: {e["route"]}: ' + ', '.join(f'{f} {b or "whole"}' for f, _, b in e['frames']))
        return
    a.out = a.out or os.path.join(a.root, 'sheets')
    for s in a.site or list(sites):
        if s not in sites:
            sys.exit(f'no site {s!r} (--list)')
        sheet(a.root, s, sites[s], a)


if __name__ == '__main__':
    main()
