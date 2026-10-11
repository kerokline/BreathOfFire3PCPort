#!/usr/bin/env python
"""Render the save slot's party icons from the player's own files, and make
the executable's icon from one of them.

The save slot draws each party member with Menu_DrawBigIcon 0x573E50
(src/game/menu_windows.cpp): a 0x28 x 0x30 sprite from texture page 0x1E
(4 bits a pixel, VRAM x 896, y 256) whose texture position and CLUT come from
the 4-byte record at 0x66367C + icon * 4 in BOF3.exe's .data: (u, v, clut-x
byte, clut-y byte), the CLUT at VRAM (clut-x & 0xF0, 0x1E0 + clut-y), sixteen
halfwords. The sheet and the CLUTs are kind-1 image chunks of DAT/FIRST.DAT
(chunk 2 lands at (896, 256); docs/asset-loading-path.md's kind-1 row says how
a chunk tiles into VRAM: 0x800-byte tiles of 32 x 32 halfwords from
x = (tag >> 24) * 32, y = ((tag >> 16) & 0xFF) * 32, (tag >> 8) & 0xFF tiles
wide, then the next row).

    python tools/make_icon.py sheet --game bof3 --out analysis/icon/icons.png
    python tools/make_icon.py ico   --game bof3 --icon N [--crop top|centre|bottom]
                                    --out analysis/icon/bof3x.ico

`sheet` renders every record that names a texture inside the page (the table
has no count of its own; a record whose CLUT row leaves VRAM ends the walk),
each at 4x with its index, so the owner can point at one. `ico` renders one,
crops it square (48 rows to 40: the owner's call which rows go), and writes a
Windows .ico with 16, 24, 32, 48 and 256 px images - nearest-neighbour for the
whole multiples, so the pixel art stays sharp.

Everything this writes is game data: analysis/ only (gitignored), never
committed (CLAUDE.md rule 1). The .ico goes into bof3x.exe's resources on the
player's machine, never into the repo.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dat  # noqa: E402

try:
    from PIL import Image, ImageDraw
except ImportError:
    sys.exit("make_icon: needs Pillow (python -m pip install pillow)")

VRAM_W, VRAM_H = 1024, 512          # halfwords
TILE = 0x800                        # bytes: 32 x 32 halfwords
PAGE_X, PAGE_Y = 896, 256           # texture page 0x1E: (0x1E & 0xF) * 64, (0x1E >> 4) * 256
ICON_W, ICON_H = 0x28, 0x30         # Menu_DrawBigIcon's SPRT
RECORDS = 0x66367C                  # the icon table in .data (menu_windows_callees.h kIconRects)
CLUT_BASE_Y = 0x1E0
DATA_VA, DATA_RAW = 0x5DA000, 0x1DA000   # BOF3.exe's .data (docs/exe-resources.md section 2)
DATA_SIZE = 0x9C000


def vram_from_dat(path, vram):
    """Upload every kind-1 chunk of a .DAT into `vram` (a bytearray of 1024 x 512 halfwords)."""
    blob, chunks = dat.load(path)
    for c in chunks:
        if c.kind != 1:
            continue
        x0 = (c.tag >> 24) * 32
        y0 = ((c.tag >> 16) & 0xFF) * 32
        wide = ((c.tag >> 8) & 0xFF) * 32
        x, y = x0, y0
        for t in range(c.size // TILE):
            tile = blob[c.offset + t * TILE:c.offset + (t + 1) * TILE]
            for row in range(32):
                if y + row >= VRAM_H or x + 32 > VRAM_W:
                    continue
                dst = ((y + row) * VRAM_W + x) * 2
                vram[dst:dst + 64] = tile[row * 64:(row + 1) * 64]
            x += 32
            if x >= x0 + wide:
                x = x0
                y += 32


def halfword(vram, x, y):
    i = (y * VRAM_W + x) * 2
    return vram[i] | (vram[i + 1] << 8)


def rgba(hw):
    """PSX 15-bit ABGR: 0x0000 is transparent; bit 15 is the semi-transparency flag, ignored here."""
    if hw == 0:
        return (0, 0, 0, 0)
    r, g, b = hw & 31, (hw >> 5) & 31, (hw >> 10) & 31
    return ((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2), 255)


def records(exe_path):
    with open(exe_path, "rb") as f:
        f.seek(DATA_RAW + (RECORDS - DATA_VA))
        raw = f.read(4 * 64)
    out = []
    for i in range(64):
        u, v, cx, cy = raw[i * 4:i * 4 + 4]
        # The table has no count; a record whose icon or CLUT would leave the
        # page or VRAM is past its end.
        if u + ICON_W > 256 or v + ICON_H > 256 or CLUT_BASE_Y + cy >= VRAM_H:
            break
        out.append((u, v, (cx & 0xF0), CLUT_BASE_Y + cy))
    return out


def render(vram, rec):
    u, v, clut_x, clut_y = rec
    clut = [rgba(halfword(vram, clut_x + i, clut_y)) for i in range(16)]
    im = Image.new("RGBA", (ICON_W, ICON_H))
    px = im.load()
    for y in range(ICON_H):
        for x in range(ICON_W):
            tx = u + x
            hw = halfword(vram, PAGE_X + tx // 4, PAGE_Y + v + y)
            idx = (hw >> ((tx & 3) * 4)) & 0xF
            px[x, y] = clut[idx]
    return im


def load_vram(game, vram_file=None):
    """The VRAM the icons are cut from: a dump of the running game's shadow
    (0x6C9F44, 1 MB - what the load screen shows) when one is given, else
    FIRST.DAT's image chunks alone (the sheet without the menu's CLUTs, which
    arrive by the compressed upload path, not as image chunks - section 2 of
    the doc)."""
    vram = bytearray(VRAM_W * VRAM_H * 2)
    if vram_file:
        with open(vram_file, "rb") as f:
            data = f.read(VRAM_W * VRAM_H * 2)
        vram[:len(data)] = data
        return vram
    vram_from_dat(os.path.join(game, "DAT", "FIRST.DAT"), vram)
    return vram


def cmd_sheet(a):
    vram = load_vram(a.game, a.vram)
    recs = records(os.path.join(a.game, "BOF3.exe"))
    if not recs:
        sys.exit("make_icon: no icon records read")
    scale, per_row, label = 4, 8, 14
    cw, ch = ICON_W * scale + 8, ICON_H * scale + label + 8
    rows = (len(recs) + per_row - 1) // per_row
    sheet = Image.new("RGBA", (cw * per_row, ch * rows), (40, 40, 40, 255))
    draw = ImageDraw.Draw(sheet)
    for i, rec in enumerate(recs):
        im = render(vram, rec).resize((ICON_W * scale, ICON_H * scale), Image.NEAREST)
        x, y = (i % per_row) * cw + 4, (i // per_row) * ch + label + 4
        # A checker under the icon shows where it is transparent.
        for yy in range(0, ICON_H * scale, 8):
            for xx in range(0, ICON_W * scale, 8):
                c = (90, 90, 90, 255) if ((xx + yy) // 8) % 2 else (70, 70, 70, 255)
                draw.rectangle([x + xx, y + yy, x + xx + 7, y + yy + 7], fill=c)
        sheet.alpha_composite(im, (x, y))
        draw.text((x, y - label), "%d  u%d v%d" % (i, rec[0], rec[1]), fill=(255, 255, 255, 255))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    sheet.save(a.out)
    print("%d icons -> %s" % (len(recs), a.out))


def crop_square(im, how):
    drop = ICON_H - ICON_W
    top = {"top": drop, "centre": drop // 2, "bottom": 0}[how]
    return im.crop((0, top, ICON_W, top + ICON_W))


def cmd_ico(a):
    vram = load_vram(a.game, a.vram)
    recs = records(os.path.join(a.game, "BOF3.exe"))
    if not 0 <= a.icon < len(recs):
        sys.exit("make_icon: icon %d is not in the table (%d read)" % (a.icon, len(recs)))
    base = crop_square(render(vram, recs[a.icon]), a.crop)   # 40 x 40
    sizes = [16, 24, 32, 48, 256]
    images = []
    for s in sizes:
        if s % ICON_W == 0:
            im = base.resize((s, s), Image.NEAREST)
        elif s > ICON_W:
            # A whole multiple, then letterboxed into the square: sharp and centred.
            k = s // ICON_W
            im = Image.new("RGBA", (s, s))
            im.alpha_composite(base.resize((ICON_W * k, ICON_W * k), Image.NEAREST),
                               ((s - ICON_W * k) // 2, (s - ICON_W * k) // 2))
        else:
            im = base.resize((s, s), Image.LANCZOS)
        images.append(im)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    # PNG-compressed entries, as Windows accepts since Vista; Pillow writes the directory.
    images[-1].save(a.out, format="ICO", sizes=[(s, s) for s in sizes], append_images=images[:-1])
    print("icon %d (%s crop) -> %s (%s)" % (a.icon, a.crop, a.out, ", ".join("%dpx" % s for s in sizes)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("sheet", help="every icon of the table at 4x, indexed")
    s.add_argument("--game", default="bof3")
    s.add_argument("--vram", default=None, help="a dump of the VRAM shadow (0x6C9F44, 1 MB) instead of FIRST.DAT")
    s.add_argument("--out", required=True)
    s.set_defaults(fn=cmd_sheet)
    i = sub.add_parser("ico", help="one icon, cropped square, as a Windows .ico")
    i.add_argument("--game", default="bof3")
    i.add_argument("--icon", type=int, required=True)
    i.add_argument("--crop", choices=["top", "centre", "bottom"], default="centre",
                   help="which 8 rows of the 48 go: top drops the top 8, bottom the bottom 8")
    i.add_argument("--vram", default=None, help="as for sheet")
    i.add_argument("--out", required=True)
    i.set_defaults(fn=cmd_ico)
    a = ap.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
