#!/usr/bin/env python
"""Contact sheets for the button routes (docs/button-routes.md): one sheet per
site, a row per install kind, the identity run's frame beside the swapped
run's for each of the site's frames.

    python tools/sigma_sheets.py --root C:/Users/kerok/b_rc                     # every site with runs
    python tools/sigma_sheets.py --root C:/Users/kerok/b_rc --site worldmap --out DIR
    python tools/sigma_sheets.py --list

The runs are tools/abc_run.py's, made with the button installs file
(tools/recipes/sigma-installs.toml) and two tags: the identity route
`sigma_<site>.txt` under `--id-tag` (default `id`), its swapped twin
`sigma_<site>_sw.txt` under `--sw-tag` (default `sw`), so the frames are
`<root>/runs/<tag>_<route>/<install>/<frame>.png`. A frame that is missing (a
run not made yet, a route that ended early) is drawn as a grey cell saying
so, never left out, so a sheet always has the same shape.

The sheets are pictures of the game: they go under the root (default
`<root>/sheets`), outside the tree (rule 1).
"""
import argparse, os, sys

from PIL import Image, ImageDraw

INSTALLS = (('pc', 'PC (bof3)'), ('psx-us', 'PSX US disc only'), ('psp-eu', 'PSP-EU only'),
            ('pc-enus', 'PC + en-US layer'))

# site: (route, [(frame, label)], crop box in 320 x 240 game pixels or None)
# The frames are the routes' own shot names (tools/recipe_shots.py's f<NNNNN>)
# or timed frames (t<NNNNNN>); docs/button-routes.md 3 says what each shows.
SITES = {}


def load_sites():
    """The site table, from docs/button-routes.md's machine block (between
    `<!-- sites -->` markers): one line per frame, `site route frame crop label`."""
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    doc = os.path.join(here, 'docs', 'button-routes.md')
    if not os.path.exists(doc):
        return
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
        e = SITES.setdefault(site, {'route': route, 'frames': [], 'crop': box})
        e['frames'].append((frame, label))


def cell(path, box, scale, size):
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
    box = e['crop']
    w, h = (box[2] - box[0], box[3] - box[1]) if box else (320, 240)
    scale = a.scale
    cw, ch = int(w * scale), int(h * scale)
    label_w, head_h, gap = 150, 34, 6
    frames = e['frames']
    cols = len(frames) * 2
    W = label_w + cols * (cw + gap) + gap
    H = head_h + len(INSTALLS) * (ch + gap) + gap
    out = Image.new('RGB', (W, H), (24, 24, 28))
    d = ImageDraw.Draw(out)
    route = e['route']
    for k, (frame, label) in enumerate(frames):
        x = label_w + k * 2 * (cw + gap) + gap
        d.text((x, 4), f'{label} ({frame})', fill=(255, 255, 255))
        d.text((x, 18), 'identity', fill=(170, 220, 170))
        d.text((x + cw + gap, 18), 'swapped', fill=(240, 190, 120))
    found = 0
    for r, (inst, name) in enumerate(INSTALLS):
        y = head_h + r * (ch + gap) + gap
        d.text((6, y + ch // 2 - 6), name, fill=(255, 255, 255))
        for k, (frame, _) in enumerate(frames):
            x = label_w + k * 2 * (cw + gap) + gap
            for j, (tag, rt) in enumerate(((a.id_tag, route), (a.sw_tag, route + '_sw'))):
                p = os.path.join(root, 'runs', f'{tag}_{rt}', inst, frame + '.png')
                found += os.path.exists(p)
                out.paste(cell(p, box, scale, (cw, ch)), (x + j * (cw + gap), y))
    os.makedirs(a.out, exist_ok=True)
    dst = os.path.join(a.out, f'{site}.png')
    out.save(dst)
    print(f'{dst}: {len(frames)} frame(s) x {len(INSTALLS)} installs, {found} of {2 * len(frames) * len(INSTALLS)} cells found')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--root', default=os.environ.get('BOF3X_ABC_ROOT', os.path.join(os.path.expanduser('~'), 'abc')))
    ap.add_argument('--site', action='append', help='a site (repeat); default every site in the table')
    ap.add_argument('--out', help='default <root>/sheets')
    ap.add_argument('--id-tag', default='id')
    ap.add_argument('--sw-tag', default='sw')
    ap.add_argument('--scale', type=float, default=1.0, help='game pixels to sheet pixels (crops are drawn at 2x when '
                                                                  'narrower than 160)')
    ap.add_argument('--list', action='store_true')
    a = ap.parse_args()
    load_sites()
    if not SITES:
        sys.exit('no sites: docs/button-routes.md has no <!-- sites --> block')
    if a.list:
        for s, e in SITES.items():
            print(f'{s}: {e["route"]}, crop {e["crop"]}, frames ' + ', '.join(f for f, _ in e['frames']))
        return
    a.out = a.out or os.path.join(a.root, 'sheets')
    for s in a.site or list(SITES):
        if s not in SITES:
            sys.exit(f'no site {s!r} (--list)')
        e = SITES[s]
        if a.scale == 1.0 and e['crop'] and e['crop'][2] - e['crop'][0] < 160:
            a2 = argparse.Namespace(**vars(a)); a2.scale = 2.0
            sheet(a.root, s, e, a2)
        else:
            sheet(a.root, s, e, a)


if __name__ == '__main__':
    main()
