#!/usr/bin/env python
"""Static scan of BOF3.exe for the exe's own text: every NUL-ended string of
glyph codes in .data (pair codes 0x80.. two bytes a glyph, 0x21..0x7F one, the
controls 0x01/0x05/0x06/0x07 and the space 0x20) that a .data pointer word or
a .text immediate reaches, rendered with the port's font (FIRST.DAT's kind-3
table) into sheets so the Chinese can be read, with a TSV of the addresses
and their referrers. The way to find the strings no route has drawn yet
(docs/village-text-scan.md, 2026-10-10): the runtime text log
(BOF3X_TEXTLOG) sees only what a route draws.

    python tools/text_scan.py analysis/textscan            # the table region 0x640000..0x670000
    python tools/text_scan.py analysis/textscan --all      # every .data address

Output is pictures of Capcom's font and bytes of the executable: analysis/
only (gitignored), never committed (CLAUDE.md rule 1). Strings of one glyph
are left out (a count's suffix, a unit) - BOF3X_TEXTLOG finds those.
"""
import os
import struct
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import font_pc
from PIL import Image, ImageDraw

exe = open("bof3/BOF3.exe", "rb").read()
pe = struct.unpack_from("<I", exe, 0x3C)[0]
nsec = struct.unpack_from("<H", exe, pe + 6)[0]; opt = struct.unpack_from("<H", exe, pe + 20)[0]
base = struct.unpack_from("<I", exe, pe + 24 + 28)[0]
secs = {}
for i in range(nsec):
    o = pe + 24 + opt + 40 * i
    name = exe[o:o + 8].rstrip(b"\0").decode()
    vs, va, rs, ro = struct.unpack_from("<IIII", exe, o + 8)
    secs[name] = (base + va, vs, ro, rs)
text_va, text_vs, text_ro, text_rs = secs[".text"]
data_va, data_vs, data_ro, data_rs = secs[".data"]
data = exe[data_ro:data_ro + data_rs]
text = exe[text_ro:text_ro + text_rs]
data_end = data_va + data_rs

def is_pair(b): return 0x80 <= b <= 0x8A
def parse(off):
    """units from data[off]: list of glyph codes (int) or None if not a string; returns (units, length_bytes)."""
    units = []; i = off; pairs = 0
    while i < len(data) and len(units) < 40:
        b = data[i]
        if b == 0: break
        if is_pair(b):
            if i + 1 >= len(data): return None
            units.append(((b & 0x7F) << 8) | data[i + 1]); i += 2; pairs += 1
        elif b == 0x20: units.append(-1); i += 1
        elif 0x21 <= b <= 0x7F: units.append(b - 0x26); i += 1
        elif b in (0x01, 0x06): units.append(-2); i += 1
        elif b in (0x05, 0x07): i += 2; units.append(-2)
        else: return None
    if i >= len(data) or data[i] != 0: return None
    if pairs == 0 or len(units) < 2: return None
    if len(units) > 24: return None
    return units, i - off

# references: .data pointer words and .text imm32
refs = {}
for i in range(0, len(data) - 3, 4):
    v = struct.unpack_from("<I", data, i)[0]
    if data_va <= v < data_end: refs.setdefault(v, []).append(("data", data_va + i))
for i in range(len(text) - 4):
    v = struct.unpack_from("<I", text, i)[0]
    if data_va <= v < data_end: refs.setdefault(v, []).append(("text", text_va + i))

found = []
for va in sorted(refs):
    if "--all" not in sys.argv and not (0x640000 <= va < 0x670000): continue
    off = va - data_va
    r = parse(off)
    if r: found.append((va, r[0], r[1], refs[va]))
print("strings", len(found))

table = font_pc.font_chunk("bof3/DAT/FIRST.DAT")
nglyph = len(table) // font_pc.GLYPH_BYTES
def draw_units(units, img, x, y):
    for u in units:
        if u < 0: x += 12; continue
        if 0 <= u < nglyph:
            rows = font_pc.glyph_pixels(table, u)
            for yy in range(24):
                for xx in range(24):
                    n = rows[yy][xx]
                    if n: 
                        c = 255 - n * 16 if n < 8 else 0
                        img.putpixel((x + xx, y + yy), (c, c, c))
        x += 24
    return x

out = [a for a in sys.argv[1:] if not a.startswith("--")][0]
os.makedirs(out, exist_ok=True)
rows_per = 40
for s in range(0, len(found), rows_per):
    chunk = found[s:s + rows_per]
    img = Image.new("RGB", (1100, 26 * len(chunk) + 4), (40, 40, 40))
    dr = ImageDraw.Draw(img)
    for j, (va, units, n, rf) in enumerate(chunk):
        y = 2 + 26 * j
        kinds = ",".join(sorted(set(k for k, _ in rf)))
        dr.text((2, y + 6), f"{va:06X} {kinds:9} n{len(rf)}", fill=(255, 255, 0))
        draw_units(units, img, 230, y)
    img.save(f"{out}/scan{s // rows_per:02d}.png")
with open(f"{out}/scan.tsv", "w") as f:
    for va, units, n, rf in found:
        f.write(f"{va:06X}\t{n}\t{len(units)}\t{' '.join(f'{k}:{a:06X}' for k, a in rf[:6])}\t{data[va-data_va:va-data_va+n].hex(' ')}\n")
