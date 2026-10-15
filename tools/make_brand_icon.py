#!/usr/bin/env python
"""Write data/icon/bof3x.ico: bold white "III" in Palatino Linotype on a black
square, at 16, 24, 32, 48 and 256 px - the icon of bof3x-launcher.exe and
bof3x.exe (the owner's choice, 2026-10-15; docs/exe-icon.md section 6).

Our own lettering, after the launcher's banner (docs/launcher-settings.md
section 2a), not Capcom's art: the .ico is committed and compiled in, so the
executables carry it from the build and are never rewritten afterwards. The
build does not run this - the release builds on Linux, without the font -
the committed file is the source; run this again only to change the design.

    python tools/make_brand_icon.py [--font C:/Windows/Fonts/palab.ttf] [--out data/icon/bof3x.ico]
                                    [--preview analysis/icon/brand.png]

Each size is drawn on its own, not scaled from the largest, so the strokes
stay crisp: 256 px the glyph, its ink a fixed share of the square; 48 px
and below by hand on whole pixels (draw_small), where the glyph's
fractional stems would grey out.
"""
import argparse
import io
import os
import struct
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("make_brand_icon: needs Pillow (python -m pip install pillow)")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIZES = (16, 24, 32, 48, 256)
TEXT = "III"
FILL = 0.72          # the ink's larger side, as a share of the square


HAND_DRAWN = 48      # this size and below: pixel-aligned rectangles, not the glyph


def draw_small(size):
    """One small size by hand, as icon fonts are: each I a stem and two slab
    serifs on whole pixels, in Palatino's proportions, so no stroke falls
    between pixels and greys out."""
    im = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    d = ImageDraw.Draw(im)
    stem = max(2, round(size * 0.12))
    over = max(1, round(size * 0.05))        # the serif past the stem, each side
    slab = max(1, round(size * 0.06))        # the serif's thickness
    gap = max(1, round(size * 0.05))
    width = stem + 2 * over
    height = round(size * 0.62)
    x = (size - (3 * width + 2 * gap)) // 2
    y = (size - height) // 2
    for _ in range(3):
        d.rectangle((x + over, y, x + over + stem - 1, y + height - 1), fill=(255, 255, 255, 255))
        d.rectangle((x, y, x + width - 1, y + slab - 1), fill=(255, 255, 255, 255))
        d.rectangle((x, y + height - slab, x + width - 1, y + height - 1), fill=(255, 255, 255, 255))
        x += width + gap
    return im


def draw(size, font_path):
    """One size: the ink of TEXT scaled to FILL of the square, centred."""
    if size <= HAND_DRAWN:
        return draw_small(size)
    im = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    d = ImageDraw.Draw(im)
    em = size
    for _ in range(40):     # the em that brings the ink's larger side to FILL
        font = ImageFont.truetype(font_path, em)
        x0, y0, x1, y1 = d.textbbox((0, 0), TEXT, font=font)
        side = max(x1 - x0, y1 - y0)
        if abs(side - FILL * size) < 0.5:
            break
        em = max(1, round(em * FILL * size / side))
    x0, y0, x1, y1 = d.textbbox((0, 0), TEXT, font=font)
    # whole pixels: a stem placed at a fraction is drawn grey across two columns
    d.text(((size - (x1 - x0)) // 2 - x0, (size - (y1 - y0)) // 2 - y0), TEXT, font=font, fill=(255, 255, 255, 255))
    return im


def bmp_image(im):
    """An .ico image entry as Windows reads one below 256 px: a 32-bit
    BITMAPINFOHEADER DIB, bottom-up, height doubled, then the AND mask."""
    w, h = im.size
    rows = [im.crop((0, y, w, y + 1)).tobytes("raw", "BGRA") for y in range(h - 1, -1, -1)]
    mask_row = ((w + 31) // 32) * 4
    header = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    return header + b"".join(rows) + b"\0" * (mask_row * h)


def png_image(im):
    out = io.BytesIO()
    im.save(out, "PNG")
    return out.getvalue()


def write_ico(images, path):
    entries, blobs, at = [], [], 6 + 16 * len(images)
    for im in images:
        blob = png_image(im) if im.size[0] >= 256 else bmp_image(im)
        w = im.size[0] % 256
        entries.append(struct.pack("<BBBBHHII", w, w, 0, 0, 1, 32, len(blob), at))
        blobs.append(blob)
        at += len(blob)
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as f:
        f.write(struct.pack("<HHH", 0, 1, len(images)) + b"".join(entries) + b"".join(blobs))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--font", default=os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "Fonts", "palab.ttf"),
                    help="Palatino Linotype Bold (palab.ttf, every Windows install has it)")
    ap.add_argument("--out", default=os.path.join(ROOT, "data", "icon", "bof3x.ico"))
    ap.add_argument("--preview", help="also a PNG of every size side by side, at 4x")
    a = ap.parse_args()
    if not os.path.isfile(a.font):
        sys.exit("make_brand_icon: no font at %s" % a.font)
    images = [draw(s, a.font) for s in SIZES]
    write_ico(images, a.out)
    print("%s: %s px" % (a.out, ", ".join(str(s) for s in SIZES)))
    if a.preview:
        small = [im.resize((im.size[0] * 4,) * 2, Image.NEAREST) if im.size[0] < 256 else im for im in images]
        sheet = Image.new("RGBA", (sum(i.size[0] for i in small) + 16 * len(small), 256 + 16), (128, 128, 128, 255))
        x = 8
        for im in small:
            sheet.paste(im, (x, 8))
            x += im.size[0] + 16
        os.makedirs(os.path.dirname(os.path.abspath(a.preview)), exist_ok=True)
        sheet.save(a.preview)
        print("preview: %s" % a.preview)


if __name__ == "__main__":
    main()
