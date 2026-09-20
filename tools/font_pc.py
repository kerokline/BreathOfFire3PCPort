#!/usr/bin/env python
"""The PC port's global glyph table: look at it, and build one.

Format (docs/asset-loading-path.md §2, docs/dialogue-localisation.md): the one
kind-3 chunk of FIRST.DAT is N glyphs of 24 x 24 at 4 bits a pixel, low nibble
first, 12 bytes a row, 288 (0x120) bytes a glyph, row-major. N = 2,451 in the
shipped file. The character draw 0x516B70 turns a script byte b (0x21..0x7F)
into glyph b - 0x26 and a two-byte code into ((b0 & 0x7F) << 8) | b1.

    python tools/font_pc.py sheet  bof3/DAT/FIRST.DAT --out analysis/font/pc_sheet.png
    python tools/font_pc.py nibbles bof3/DAT/FIRST.DAT

Output is game data: analysis/ only (gitignored), never committed (CLAUDE.md
rule 1).
"""
import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dat  # noqa: E402

GLYPH = 24
ROW_BYTES = 12
GLYPH_BYTES = GLYPH * ROW_BYTES


def font_chunk(path):
    blob, chunks = dat.load(path)
    k3 = [c for c in chunks if c.kind == 3]
    if len(k3) != 1:
        raise SystemExit("%s: %d kind-3 chunks, expected 1" % (path, len(k3)))
    c = k3[0]
    if c.size % GLYPH_BYTES:
        raise SystemExit("kind-3 size 0x%X is not a multiple of 0x120" % c.size)
    return blob[c.offset:c.offset + c.size]


def glyph_pixels(table, n):
    """24 rows of 24 nibbles."""
    g = table[n * GLYPH_BYTES:(n + 1) * GLYPH_BYTES]
    rows = []
    for y in range(GLYPH):
        r = g[y * ROW_BYTES:(y + 1) * ROW_BYTES]
        row = []
        for b in r:
            row += [b & 0xF, b >> 4]
        rows.append(row)
    return rows


def cmd_sheet(args):
    from PIL import Image, ImageDraw
    table = font_chunk(args.dat)
    count = len(table) // GLYPH_BYTES
    first, last = args.first, min(args.last if args.last else count, count)
    cols = args.cols
    n = last - first
    rows = (n + cols - 1) // cols
    pitch, margin = GLYPH + 2, 40
    img = Image.new("RGB", (margin + cols * pitch, rows * pitch), (32, 32, 64))
    d = ImageDraw.Draw(img)
    for i in range(n):
        gx, gy = margin + (i % cols) * pitch, (i // cols) * pitch
        if i % cols == 0:
            d.text((2, gy + 6), "%04X" % (first + i), fill=(255, 255, 0))
        px = glyph_pixels(table, first + i)
        for y in range(GLYPH):
            for x in range(GLYPH):
                v = px[y][x] * 17
                img.putpixel((gx + x, gy + y), (v, v, v))
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    img.save(args.out)
    print("%d glyphs in table; wrote %d..%d to %s" % (count, first, last - 1, args.out))


def cmd_nibbles(args):
    table = font_chunk(args.dat)
    hist = collections.Counter()
    for b in table:
        hist[b & 0xF] += 1
        hist[b >> 4] += 1
    total = sum(hist.values())
    print("%d glyphs" % (len(table) // GLYPH_BYTES))
    for v in range(16):
        print("  nibble %X: %9d  %5.1f%%" % (v, hist[v], 100.0 * hist[v] / total))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("sheet")
    s.add_argument("dat")
    s.add_argument("--out", required=True)
    s.add_argument("--first", type=lambda v: int(v, 0), default=0)
    s.add_argument("--last", type=lambda v: int(v, 0), default=0)
    s.add_argument("--cols", type=int, default=32)
    s.set_defaults(fn=cmd_sheet)
    s = sub.add_parser("nibbles")
    s.add_argument("dat")
    s.set_defaults(fn=cmd_nibbles)
    args = ap.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
