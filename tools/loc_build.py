#!/usr/bin/env python
"""Build per-language overlay DATs from the player's own disc.

docs/dialogue-localisation.md. An overlay `<lang>.<NAME>.DAT` is an ordinary
DAT container (tools/dat.py) holding only the chunks that differ; the engine
walks it after the original file, so its chunks land on top.

    python tools/loc_build.py all   --disc DISC --game bof3 [--lang en] [--upscaler CMD | --glyphs PNG] [--only AREA000]
    python tools/loc_build.py sheet --disc DISC --out analysis/font/en_cells.png
    python tools/loc_build.py export --disc DISC --out analysis/font/en_8x12.png

`export` writes the donor's 100 cells as they are - 8 x 12, 20 to a row, no
gaps, RGBA in the game's own font colours - for upscaling outside this tool.
`font --glyphs` takes such a sheet back at any cell size up to 24 x 24
(16 x 24 is the 2x size the 8 px advance assumes; the sheet's width / 20 and
height / 5 give the cell). Each pixel becomes the nearest nibble: alpha under
half is 0 (clear); otherwise the closest grey of the ramp 1..7; a pixel darker
than the ramp's darkest by more than half way to black is 8, the PC font's
outline colour, which the donor never uses.

Everything written is derived from game data: it goes into the player's game
directory or analysis/ (both gitignored) and is never committed (CLAUDE.md
rule 1).

## Replicating a build: `--upscaler`

No image is ever checked in. The pipeline is disc -> cells -> upscaler ->
glyph table, all of it on the player's machine, and `--upscaler` names the
middle step: a command line run in a fresh temporary directory, with `{in}`
replaced by the exported 160 x 60 sheet, `{scale}` by 2 and `{out}` by the
path we would like the result at. A tool that ignores `{out}` and writes a
PNG of its own naming into the working directory is fine: if `{out}` does not
appear, the one new PNG in the directory is taken. For
https://github.com/cole8888/Nearest-Neighbour-Upscale (MIT; `make`, with
CHANNELS_PER_PIXEL set to 4 to keep the alpha):

    python tools/loc_build.py all --disc DISC --game bof3
        --upscaler "/path/to/NearestNeighbourUpscale {in} {scale}"

A result without an alpha channel is accepted: exact black is then the clear
colour (the font's darkest real colour is (0, 0, 48)). `all` prints the
SHA-256 of the glyph table it built; two people with the same disc, the
same game files and the same upscaler should read out the same one. With no
`--upscaler` the cells are doubled here, nearest neighbour - which is what
that tool computes, so the two hashes should agree.

## The donor font (measured 2026-09-20 on the US disc, SLUS-00422)

`BIN/ETC/ENDKANJI.EMI` section 0 is the 256 x 512 4bpp atlas, stored as two
interleaved streams of 2048-byte chunks (sibling `tools/font_sheet.py`). From
y = 72 it carries the Western builds' dialogue font: **cells of 8 x 12, 31 to a
row** (the sheet is 252 px wide), in script-code order starting at `0x30`:
cell = code - 0x30. The US stepper advances a flat 8 px a character
(`addiu v0, v0, 8` at 0x80150770) - the font is monospaced, and upper and lower
case are simply different codes (`A` 0x41, `a` 0x61).

## The PC side

The character draw 0x516B70 maps a byte b in 0x21..0x7F to glyph b - 0x26 and
a two-byte code to ((b0 & 0x7F) << 8) | b1, refusing anything above 0xA00.
The shipped table has 2,451 glyphs, so 0x993..0xA00 is free. We:

  - keep the whole Chinese table, so untouched text still draws;
  - append the donor's 100 cells at glyph 0x993 + (code - 0x30);
  - and also paint the donor's glyph over the single-byte slot of every
    character that has an ASCII byte >= 0x26 the engine treats as plain
    (not 0x2A or 0x3C, which hang at line start), so common text stays one
    byte a character.

Each 8 x 12 cell is doubled to 16 x 24 and sits at the left of the 24 x 24
glyph. Nibbles are copied as they are: on both sides 0 is clear, 1 the body
and 2..7 the ramp away from it.
"""
import argparse
import hashlib
import os
import shlex
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dat        # noqa: E402
import font_pc    # noqa: E402
import psx_disc   # noqa: E402

FONT_EMI = "BIN/ETC/ENDKANJI.EMI"
CELL_W, CELL_H, CELLS_PER_ROW, CELLS_Y0 = 8, 12, 31, 72
FIRST_CODE, LAST_CODE = 0x30, 0x93
APPEND_AT = 0x993                # first glyph past the shipped table
# The donor carries the SAME 100 characters a second time at 8 x 8, rows
# 120..151, same 31 to a row and same code order (owner 2026-09-20, grid
# render). That is the set the 8 px UI draw 0x516E70 uses. Its quad is 8
# units where Text_DrawString's is 12, but it samples the whole 24 x 24 glyph
# into it, so these cells are stored tripled (pc_glyph).
SMALL_CELL_H, SMALL_CELLS_Y0 = 8, 120
SMALL_APPEND_AT = 0xA00          # the 8 x 8 set, on the round boundary past the 12 px one
SMALL_SPACE = SMALL_APPEND_AT + (0x93 - 0x30 + 1)   # a blank cell after it; see config_encode
# The Config screen's controller icons (DIV-0051, docs/controls.md section 6
# step 3), six glyphs after the blank: circle, cross, triangle, square, L1,
# R1. The shapes are the atlas's 12 x 12 set at y 48 (the sheet's third row,
# after the arrows and the heart; ink segments 181..190, 193..201, 205..214,
# 218..226, read 2026-09-24) - the owner's choice over the 8 x 8 and the
# 8 x 12 sets, both of which the atlas also holds. L1 and R1 are composed,
# as the PlayStation's panel reads: the dialogue set's capital (codes 0x4C,
# 0x52) with the 8 x 8 set's serifed "1" (code 0x31) low at its right, in a
# 12 x 12 cell. Each cell is doubled to fill the 24 x 24 glyph.
SHAPE_CELLS = ((180, 48), (192, 48), (204, 48), (216, 48))   # (x, y) of circle, cross, triangle, square
LABEL_CAPS = (0x4C, 0x52)                                     # L, R in the dialogue set
ICONS_AT = SMALL_SPACE + 1                                    # circle .. square, L1, R1
# The battle banner's two picture suffixes (DIV-0052, src/game/battle_text.cpp):
# " EX" on an extra turn, and a second of the same shape. The PC draws them as
# glyphs 0x50..0x53, which the single-byte slots of `v` .. `y` paint over, so
# the shipped four are kept again after the icons. The US disc writes them as
# the two-byte codes below (BATTLE.EMI's messages 1 and 3; the Japanese disc
# the same codes), in the PC's order.
SUFFIX_GLYPHS = (0x50, 0x51, 0x52, 0x53)
SUFFIX_AT = ICONS_AT + 6
SUFFIX_OF = {0x151B: 0x50, 0x151C: 0x51, 0x151F: 0x52, 0x1520: 0x53}
# EX itself is the disc's, not the port's redrawing (the owner, 2026-09-24:
# the port's does not match the PlayStation's): one picture across two cells of
# the atlas's 12 x 12 row at y 60, x 156 and 168 (ink x 159..176, y 63..71,
# the letters overlapping, nibbles 9..F banded top to bottom), each doubled to
# the 24 x 24 glyph with its nibbles as they are - the overlay's text palette
# is the disc's (DIV-0013). The second suffix's cells were not identified in
# the atlas, so it keeps the port's glyphs.
EX_CELLS = ((156, 60), (168, 60))
# The French and German discs carry more of the same grid: accented letters
# in the cells after 0x93, in both sets (FR 0x94..0xAA, DE 0x94..0xA7; the US
# disc's cells there are empty - measured 2026-09-24 off the three atlases,
# and the codes are what their scripts use, e with acute 0xA1 7,162 times in
# FR's area dialogue). They go in two blocks of their own past everything
# above, so an English build's table is byte for byte what it was. Filled in
# per disc by build_font (latin_extension); the Config screen's selected row
# swaps the UI block for the dialogue one by a constant offset, as it does
# for the first hundred (src/game/config_text.cpp).
EXT_FIRST, EXT_MAX = 0x94, 0xAB  # the grid holds 4 rows of 31: codes up to 0xAB
EXT_AT, EXT_SMALL_AT = 0xA80, 0xAA0
ext_last = EXT_FIRST - 1         # none until a donor's atlas says otherwise
# A Japanese disc (boots SLPS_, `is_japanese`) is a different font and a
# different encoding (the sibling's docs/TEXT_ENGINE.md, names/font.toml, read
# off the JP boot EXE's mapper 0x80151F4C; re-measured here 2026-09-24):
# ENDKANJI.EMI section 0 is 21 x 21 cells of 12 px - byte b < 0x5B is cell b,
# b >= 0x5B (the kana) is cell b + 0x23, and 0x15 nn is cell nn + 0x5B - and
# section 1 is the kanji sheet, 21 x 21, 0x12nn / 0x13nn being cell
# code - 0x1200. The cells use the Latin set's nibbles (1 body, 2..6 ramp,
# 7 the drop shadow). Both sheets go in whole, doubled, from JA_AT; 12 px on
# the PC's own 12-unit advance. The PC hangs its own brackets at 0x2A and
# 0x3C (MsgBox_Step 0x4979A0) - the same two JP hangs, the corner bracket and
# the white corner bracket - so those stay single bytes, with the disc's
# glyphs painted over the port's.
donor_ja = False
JA_AT, JA_CELLS, JA_COLS, JA_CELL = 0x993, 441, 21, 12
JA_KANJI_AT = JA_AT + JA_CELLS
JA_HANG = {0x2A: 0x2A, 0x3B: 0x3C}   # JP code -> the PC byte that hangs it
JA_SPACE = 0xFF                      # the JP word separator, 8,389 uses: a space
# DIV-0057, pair codes (docs/dialogue-localisation.md section 9): a JP name is
# at most 8 glyphs, two bytes each on the PC - 16, which leaves the items'
# 16-byte field no terminator and does not fit the battle banner's 8. A pair
# code is one two-byte code the DLL draws as two glyphs (Text_DrawString),
# advancing 24 (its kind-4 entry) and counting as two characters. Names are
# paired from their end until they fit; nothing else is. The table's own glyph
# at a pair code is a placeholder - both kana at half size - so a draw that
# does not expand pairs shows as small text rather than as a wrong word.
PAIR_AT, PAIR_KIND = 0xD10, 13
ja_pairs = {}                        # (glyph a, glyph b) -> pair glyph index
ja_sheets = None                     # (single, kanji) rows, kept for the placeholders
GLYPH_LIMIT = 0x1000             # ours, DIV-0016; the original 0x516C94 was cmp cx, 0xA00
PC_ADVANCE = 12                  # 0x497A44, 0x516CDE
TEXT_ROOM = 0x8000               # the CLUT strip as loaded starts here; the system pool at
                                 # 0x4000 is moved out of the way by the engine (DIV-0007)

# Donor code -> the ASCII byte whose single-byte slot it takes. Letters and
# digits are the same byte on both sides.
ASCII_OF = {c: c for c in list(range(0x30, 0x3A)) + list(range(0x41, 0x5B)) + list(range(0x61, 0x7B))}
ASCII_OF.update({0x3A: ord("("), 0x3B: ord(")"), 0x3C: ord(","), 0x3D: ord("-"), 0x3E: ord("."),
                 0x3F: ord("/"), 0x40: ord("="), 0x5C: ord("?"), 0x8D: ord("&"), 0x8E: ord("'"),
                 0x8F: ord(":"), 0x91: ord(";"),
                 # The double quote takes 0x2A, the byte MsgBox_Step hangs into the margin at
                 # the start of a line (0x4979A0) - as the US stepper hangs its 0x90
                 # (SLUS-00422 0x80150680). The engine makes the hang the glyph's advance.
                 0x90: 0x2A})
SPACE_IN, SPACE_OUT = 0xFF, 0x20

ARG1 = {0x04, 0x05, 0x07, 0x08, 0x0A, 0x0C, 0x0F, 0x16}   # sibling TEXT_ENGINE.md
LEAD = {0x12, 0x13, 0x15}
CHOICE = 0x14


# ---------------------------------------------------------------- the font

def emi_sections(blob):
    if blob[8:16] != b"MATH_TBL":
        raise ValueError("not an EMI")
    count, = struct.unpack_from("<I", blob, 0)
    out, pos = [], 0x800
    for i in range(count):
        size, dest = struct.unpack_from("<II", blob, 0x10 + 16 * i)
        out.append((dest, blob[pos:pos + size]))
        pos += ((size + 0x7FF) >> 11) * 0x800
    return out


def donor_sheet(disc):
    """The atlas as rows of nibbles, de-interleaved."""
    raw = emi_sections(disc.read(FONT_EMI))[0][1]
    rows = []
    for c in range(0, len(raw), 4096):
        left, right = raw[c:c + 2048], raw[c + 2048:c + 4096]
        for y in range(32):
            row = []
            for half in (left, right):
                for b in half[y * 64:(y + 1) * 64]:
                    row += [b & 0xF, b >> 4]
            rows.append(row)
    return rows


def latin_extension(rows):
    """The last code of the donor's accented cells past 0x93, or 0x93 if none.

    A cell counts when it has ink in both sets, and the run stops at the first
    empty one."""
    last = LAST_CODE
    for code in range(EXT_FIRST, EXT_MAX + 1):
        if not all(any(v for r in donor_cell(rows, code, y0, h) for v in r)
                   for y0, h in ((CELLS_Y0, CELL_H), (SMALL_CELLS_Y0, SMALL_CELL_H))):
            break
        last = code
    return last


def donor_cell(rows, code, y_first=CELLS_Y0, cell_h=CELL_H):
    i = code - FIRST_CODE
    x0, y0 = CELL_W * (i % CELLS_PER_ROW), y_first + cell_h * (i // CELLS_PER_ROW)
    return [rows[y0 + y][x0:x0 + CELL_W] for y in range(cell_h)]


def pc_glyph(cell, scale=2):
    """8 x H nibbles -> one 288-byte PC glyph, scaled, top-left-aligned.

    The 8 x 12 dialogue set is doubled: 16 x 24 of the 24 x 24 glyph, drawn
    1:1 by Text_DrawString's 12-unit quad. The 8 x 8 UI set is TRIPLED, which
    fills the glyph exactly: the 8-unit draw 0x516E70 shows the whole
    24 x 24 cell at 16 x 16 (its emitter 0x516D50 fixes the texture extent at
    0xC units whatever the quad), so a tripled cell lands at the
    PlayStation's doubled 8 x 8 - texel for texel under a point filter.
    """
    out = bytearray()
    scaled_h = scale * len(cell)
    for y in range(font_pc.GLYPH):
        src = cell[y // scale] if y < scaled_h else None
        row = [src[x // scale] if (src is not None and x < scale * CELL_W) else 0
               for x in range(font_pc.GLYPH)]
        for x in range(0, font_pc.GLYPH, 2):
            out.append(row[x] | (row[x + 1] << 4))
    return bytes(out)


def cell_advance(cell, mono):
    """The US build advances 8 for every cell. Two of its glyphs sit at the left
    of theirs with nothing after them - the apostrophe and the comma, ink in
    columns 1..3 - and read as a letter, a gap, a letter. Unless `mono`, a
    glyph whose ink ends by column 3 advances to two past it. Measured
    2026-09-20: that rule takes exactly those two, to 5; the stop, the colon
    and the exclamation mark are centred in their cells and stay at 8.
    docs/DIVERGENCE.md DIV-0009: this is a departure from the US release."""
    ink = [x for x in range(CELL_W) if any(row[x] for row in cell)]
    if mono or not ink or max(ink) > 3:
        return CELL_W
    return max(ink) + 2


def build_table(base_table, rows, redrawn=None, mono=False):
    count = len(base_table) // font_pc.GLYPH_BYTES
    if count != APPEND_AT:
        raise SystemExit("base table has %d glyphs, expected %d" % (count, APPEND_AT))
    table = bytearray(base_table)
    advances = bytearray([PC_ADVANCE]) * count
    for code in range(FIRST_CODE, LAST_CODE + 1):
        g = pc_glyph_from_rows(redrawn[code]) if redrawn else pc_glyph(donor_cell(rows, code))
        table += g
        advance = cell_advance(donor_cell(rows, code), mono)
        advances.append(advance)
        if code in ASCII_OF:
            slot = ASCII_OF[code] - 0x26
            table[slot * font_pc.GLYPH_BYTES:(slot + 1) * font_pc.GLYPH_BYTES] = g
            advances[slot] = advance
    # The 8 x 8 UI set, at its own base. The gap between the two blocks is
    # blank glyphs: SMALL_APPEND_AT is a round number, not a tight packing.
    blank = bytes(font_pc.GLYPH_BYTES)
    while len(table) // font_pc.GLYPH_BYTES < SMALL_APPEND_AT:
        table += blank
        advances.append(PC_ADVANCE)
    for code in range(FIRST_CODE, LAST_CODE + 1):
        cell = donor_cell(rows, code, SMALL_CELLS_Y0, SMALL_CELL_H)
        table += pc_glyph(cell, 3)
        # The UI draw advances a flat 8 of its own and never reads this table;
        # the value is here for the ordinary draw, should anything reach these
        # glyphs through it.
        advances.append(CELL_W)
    # One blank glyph for the space, so that every character of a UI string is
    # two bytes and `4 * len` is exactly its width.
    table += blank
    advances.append(CELL_W)
    # The six icons (ICONS_AT ..): 12 x 12 cells doubled. The disc's icons are
    # pre-coloured (the 8 x 8 circle's body is nibbles 9 and 10, the cross's
    # 14 and 15; counted 2026-09-24) where a letter's body is 1 with 7 for its
    # ramp, and the panel draws them through the button's colour index, which
    # maps 1 to that colour: every body nibble becomes 1 and 7 stays the ramp.
    cells = [[rows[y0 + y][x0:x0 + 12] for y in range(12)] for x0, y0 in SHAPE_CELLS]
    one = donor_cell(rows, 0x31, SMALL_CELLS_Y0, SMALL_CELL_H)
    for code in LABEL_CAPS:
        cap = donor_cell(rows, code)
        ink = [x for x in range(CELL_W) if any(r[x] for r in cap)]
        a0, a1 = min(ink), max(ink)
        cell = [[0] * 12 for _ in range(12)]
        for y in range(12):
            for x in range(a0, a1 + 1):
                cell[y][x - a0] = cap[y][x]
        oink = [x for x in range(CELL_W) if any(r[x] for r in one)]
        x0 = a1 - a0 + 1
        for y in range(SMALL_CELL_H):
            for x in range(min(oink), max(oink) + 1):
                if x0 + x - min(oink) < 12:
                    cell[4 + y][x0 + x - min(oink)] = one[y][x]
        cells.append(cell)
    for cell in cells:
        big = [[0] * font_pc.GLYPH for _ in range(font_pc.GLYPH)]
        for y in range(12):
            for x in range(12):
                v = cell[y][x]
                v = 7 if v == 7 else (1 if v else 0)
                for dy in range(2):
                    for dx in range(2):
                        big[2 * y + dy][2 * x + dx] = v
        table += pc_glyph_from_rows(big)
        advances.append(PC_ADVANCE)
    # The suffixes' glyphs (SUFFIX_AT ..): EX from the disc, the second
    # suffix as shipped, before any painting.
    for i, g in enumerate(SUFFIX_GLYPHS):
        if i < len(EX_CELLS):
            x0, y0 = EX_CELLS[i]
            big = [[rows[y0 + y // 2][x0 + x // 2] for x in range(font_pc.GLYPH)] for y in range(font_pc.GLYPH)]
            table += pc_glyph_from_rows(big)
        else:
            table += base_table[g * font_pc.GLYPH_BYTES:(g + 1) * font_pc.GLYPH_BYTES]
        advances.append(PC_ADVANCE)

    # The accented cells, if this donor has them: dialogue then UI, each at
    # its own round base, blanks in between.
    if ext_last >= EXT_FIRST:
        for at, y0, h, scale in ((EXT_AT, CELLS_Y0, CELL_H, 2), (EXT_SMALL_AT, SMALL_CELLS_Y0, SMALL_CELL_H, 3)):
            if len(table) // font_pc.GLYPH_BYTES > at:
                raise SystemExit("glyph block 0x%X overlaps the one before it" % at)
            while len(table) // font_pc.GLYPH_BYTES < at:
                table += blank
                advances.append(PC_ADVANCE)
            for code in range(EXT_FIRST, ext_last + 1):
                cell = donor_cell(rows, code, y0, h)
                table += pc_glyph(cell, scale)
                advances.append(cell_advance(cell, mono) if scale == 2 else CELL_W)

    glyphs = len(table) // font_pc.GLYPH_BYTES
    if glyphs - 1 > GLYPH_LIMIT:
        raise SystemExit("table would hold %d glyphs, the limit is 0x%X" % (glyphs, GLYPH_LIMIT))
    return bytes(table), bytes(advances)


# FIRST.DAT kind-0 tag 0x8000, CLUT 0 (read 2026-09-20): the grey ramp the
# nibbles 1..7 index, and the PC font's outline at 8.
RAMP = {1: 224, 2: 216, 3: 200, 4: 192, 5: 168, 6: 136, 7: 96}
OUTLINE, OUTLINE_RGB = 8, (0, 0, 48)
SHEET_COLS = 20


def nibble_of(r, g, b, a):
    if a < 128:
        return 0
    lum = (r * 299 + g * 587 + b * 114) // 1000
    if lum < RAMP[7] // 2:
        return OUTLINE
    return min(RAMP, key=lambda n: abs(RAMP[n] - lum))


def cells_from_png(path):
    """{code: rows of nibbles} from an upscaled sheet laid out as `export` does."""
    from PIL import Image
    img = Image.open(path)
    has_alpha = img.mode in ("RGBA", "LA", "PA") or "transparency" in img.info
    img = img.convert("RGBA")
    if not has_alpha:       # an upscaler that dropped the alpha: exact black is clear
        img.putdata([(r, g, b, 0) if (r, g, b) == (0, 0, 0) else (r, g, b, a) for r, g, b, a in img.getdata()])
    n = LAST_CODE - FIRST_CODE + 1
    sheet_rows = (n + SHEET_COLS - 1) // SHEET_COLS
    if img.width % SHEET_COLS or img.height % sheet_rows:
        raise SystemExit("%s: %d x %d is not %d columns by %d rows of cells"
                         % (path, img.width, img.height, SHEET_COLS, sheet_rows))
    cw, chh = img.width // SHEET_COLS, img.height // sheet_rows
    if cw > font_pc.GLYPH or chh > font_pc.GLYPH:
        raise SystemExit("%s: cells are %d x %d, the glyph is 24 x 24" % (path, cw, chh))
    px = img.load()
    out = {}
    for i in range(n):
        x0, y0 = cw * (i % SHEET_COLS), chh * (i // SHEET_COLS)
        out[FIRST_CODE + i] = [[nibble_of(*px[x0 + x, y0 + y]) for x in range(cw)] for y in range(chh)]
    return out


def pc_glyph_from_rows(cell):
    """Nibble rows of any size up to 24 x 24 -> one PC glyph, top-left aligned."""
    out = bytearray()
    for y in range(font_pc.GLYPH):
        src = cell[y] if y < len(cell) else []
        row = [src[x] if x < len(src) else 0 for x in range(font_pc.GLYPH)]
        for x in range(0, font_pc.GLYPH, 2):
            out.append(row[x] | (row[x + 1] << 4))
    return bytes(out)


# ---------------------------------------------------------------- the text

def is_japanese(disc):
    """The disc's boot EXE is SLPS_ / SCPS_ - a Japanese build."""
    try:
        cnf = disc.read("SYSTEM.CNF")
    except KeyError:
        return False
    boot = cnf.split(b"\n")[0].upper()
    return b"SLPS_" in boot or b"SCPS_" in boot


def ja_cell_of(code, lead=None):
    """A JP code -> its glyph in our table, or None."""
    if lead == 0x15:
        return JA_AT + code + 0x5B if code + 0x5B < JA_CELLS else None
    if lead in (0x12, 0x13):
        k = ((lead - 0x12) << 8) | code
        return JA_KANJI_AT + k if k < JA_CELLS else None
    if 0x17 <= code < 0x5B:
        return JA_AT + code
    if 0x5B <= code <= 0xFE:
        return JA_AT + code + 0x23
    return None


def ja_encode(code, lead=None):
    if lead is None and code == JA_SPACE:
        return bytes([SPACE_OUT])
    if lead is None and code in JA_HANG:
        return bytes([JA_HANG[code]])
    g = ja_cell_of(code, lead)
    return None if g is None else bytes([0x80 | (g >> 8), g & 0xFF])


def ja_name(raw, room):
    """A JP name -> PC bytes of at most `room`, pairing glyphs from its end
    until it fits; None if a code has no glyph or it cannot be made to fit."""
    units, i = [], 0
    while i < len(raw):
        if raw[i] in LEAD and i + 1 < len(raw):
            e, i = ja_encode(raw[i + 1], raw[i]), i + 2
        else:
            e, i = ja_encode(raw[i]), i + 1
        if e is None:
            return None
        units.append(e)
    k = len(units) - 1
    while sum(map(len, units)) > room and k > 0:
        a, b = units[k - 1], units[k]
        if len(a) == 2 and len(b) == 2 and a[0] & 0x80 and b[0] & 0x80 \
                and ((a[0] & 0x7F) << 8 | a[1]) < PAIR_AT and ((b[0] & 0x7F) << 8 | b[1]) < PAIR_AT:
            key = (((a[0] & 0x7F) << 8) | a[1], ((b[0] & 0x7F) << 8) | b[1])
            g = ja_pairs.setdefault(key, PAIR_AT + len(ja_pairs))
            units[k - 1:k + 1] = [bytes([0x80 | (g >> 8), g & 0xFF])]
            k -= 2
        else:
            k -= 1
    out = b"".join(units)
    return out if len(out) <= room else None


def ja_pair_chunks(font_chunks):
    """Extend the font's kind-3 table and kind-4 advances with the pair codes'
    placeholder glyphs, and the kind-13 pair table."""
    if not ja_pairs:
        return font_chunks
    out = []
    for kind, tag, payload in font_chunks:
        if kind == 3:
            table = bytearray(payload)
            while len(table) // font_pc.GLYPH_BYTES < PAIR_AT:
                table += bytes(font_pc.GLYPH_BYTES)
            for (a, b), g in sorted(ja_pairs.items(), key=lambda kv: kv[1]):
                table += ja_placeholder(a, b)
            if len(table) // font_pc.GLYPH_BYTES - 1 > GLYPH_LIMIT:
                raise SystemExit("pair codes run past glyph 0x%X" % GLYPH_LIMIT)
            payload = bytes(table)
        elif kind == 4:
            adv = bytearray(payload)
            while len(adv) < PAIR_AT:
                adv.append(PC_ADVANCE)
            for (a, b), g in sorted(ja_pairs.items(), key=lambda kv: kv[1]):
                adv.append(adv[a] + adv[b])
            payload = bytes(adv)
        out.append((kind, tag, payload))
    pairs = sorted(ja_pairs.items(), key=lambda kv: kv[1])
    body = struct.pack("<H", len(pairs)) + b"".join(struct.pack("<HH", a, b) for (a, b), g in pairs)
    return out + [(PAIR_KIND, PAIR_AT, body)]


def ja_cell_rows(glyph):
    """The 12 x 12 sheet cell behind one of our JP glyph indices."""
    single, kanji = ja_sheets
    rows, cell = (single, glyph - JA_AT) if glyph < JA_KANJI_AT else (kanji, glyph - JA_KANJI_AT)
    r, c = divmod(cell, JA_COLS)
    return [rows[JA_CELL * r + y][JA_CELL * c:JA_CELL * (c + 1)] for y in range(JA_CELL)]


def ja_placeholder(a, b):
    """Both cells at their native 12 px, side by side in the 24 x 24 glyph,
    centred vertically: drawn by an ordinary 12-unit quad they are half size."""
    big = [[0] * font_pc.GLYPH for _ in range(font_pc.GLYPH)]
    for k, g in enumerate((a, b)):
        cell = ja_cell_rows(g)
        for y in range(JA_CELL):
            for x in range(JA_CELL):
                big[6 + y][JA_CELL * k + x] = cell[y][x]
    return pc_glyph_from_rows(big)


def encode_text(raw):
    """A donor string with no controls -> PC bytes, or None if a code has no
    glyph. Lead bytes are the Japanese two-byte codes; a Latin donor has none."""
    out, i = bytearray(), 0
    while i < len(raw):
        if donor_ja and raw[i] in LEAD and i + 1 < len(raw):
            e = ja_encode(raw[i + 1], raw[i])
            i += 2
        else:
            e = encode_char(raw[i])
            i += 1
        if e is None:
            return None
        out += e
    return bytes(out)


def encode_char(code):
    if donor_ja:
        return ja_encode(code)
    if code == SPACE_IN:
        return bytes([SPACE_OUT])
    if code in ASCII_OF:
        return bytes([ASCII_OF[code]])
    if FIRST_CODE <= code <= LAST_CODE:
        g = APPEND_AT + code - FIRST_CODE
        return bytes([0x80 | (g >> 8), g & 0xFF])
    if EXT_FIRST <= code <= ext_last:
        g = EXT_AT + code - EXT_FIRST
        return bytes([0x80 | (g >> 8), g & 0xFF])
    return None


def convert_run(buf, i, stop_at_nul_only=False):
    """Convert from i to the message's terminating NUL. Returns (bytes, next i),
    or (None, next i) when the message holds a code English does not have - the
    donor's untranslated leftovers."""
    out, ok = bytearray(), True
    while True:
        b = buf[i] if i < len(buf) else 0     # a block may end without its NUL
        if b == 0:
            out.append(0)
            return (bytes(out) if ok else None), i + 1
        if b == CHOICE and not stop_at_nul_only:
            out += buf[i:i + 4]
            options = buf[i + 3] & 0xF
            i += 4
            for _ in range(options):
                sub, i = convert_run(buf, i, True)
                if sub is None:
                    ok = False
                else:
                    out += sub
            return (bytes(out) if ok else None), i
        if b in ARG1:
            out += buf[i:i + 2]
            i += 2
        elif b in LEAD:
            e = ja_encode(buf[i + 1], b) if donor_ja and i + 1 < len(buf) else None
            if e is None:
                ok = False
            else:
                out += e
            i += 2
        elif b <= 0x16:
            out.append(b)
            i += 1
        else:
            e = encode_char(b)
            if e is None:
                ok = False
            else:
                out += e
            i += 1


def message_end(buf, i):
    """Extent of a PC message, for carrying one over unchanged. The PC stepper
    skips a second byte after any first byte with the high bit set (0x497A2A)."""
    while True:
        b = buf[i] if i < len(buf) else 0
        if b == 0:
            return i + 1
        if b == CHOICE:
            options = buf[i + 3] & 0xF
            i += 4
            for _ in range(options):
                while buf[i]:
                    i += 2 if buf[i] & 0x80 or buf[i] in ARG1 else 1
                i += 1
            return i
        i += 2 if (b & 0x80 or b in ARG1) else 1


def convert_block(donor, base, room):
    """A donor text block -> a PC one. Slots whose donor message is not
    English keep the PC file's own message."""
    # The slot count is the PC file's: its event script is what names the
    # slots. The US tables agree with it everywhere; the French and German
    # ones do not always (2026-09-24: 15 FR and 11 DE areas) - some entries
    # point outside their block where the PC's slot is empty or unused, and
    # FR AREA009 has five slots appended. Those slots keep the PC's message.
    # Checked: of the slots both US and FR/DE point into their blocks, 96%
    # carry the same control codes, AREA009 included. An unused slot points
    # at the block's end, which is an empty message, not outside it.
    n = struct.unpack_from("<H", base, 0)[0] // 2
    d_offs = struct.unpack_from("<%dH" % n, donor.ljust(2 * n, b"\0"), 0)
    b_offs = struct.unpack_from("<%dH" % n, base, 0)
    body, placed, table, kept = bytearray(), {}, [], 0
    for slot in range(n):
        key = ("d", d_offs[slot])
        if key not in placed:
            msg = None
            if 2 * n <= d_offs[slot] <= len(donor):   # the end itself: an unused, empty slot
                msg, _ = convert_run(donor, d_offs[slot])
            if msg is None:
                key = ("b", b_offs[slot])
                msg = base[b_offs[slot]:message_end(base, b_offs[slot])]
            if key not in placed:
                placed[key] = 2 * n + len(body)
                body += msg
            placed[("d", d_offs[slot])] = placed[key]
        if key[0] == "b":
            kept += 1
        table.append(placed[key])
    block = struct.pack("<%dH" % n, *table) + bytes(body)
    if len(block) > room:
        raise ValueError("block is 0x%X bytes, room is 0x%X" % (len(block), room))
    return block, kept


def write_overlay(path, chunks):
    with open(path, "wb") as f:
        for kind, tag, payload in chunks:
            f.write(struct.pack("<4I", kind, tag, len(payload), 0))
            f.write(payload)
    dat.load(path)      # parses clean or raises


# ---------------------------------------------------------------- commands

def dat_dir(game):
    d = os.path.join(game, "DAT")
    if not os.path.isdir(d):
        raise SystemExit("%s: no DAT directory" % game)
    return d


def export_image(rows):
    from PIL import Image
    n = LAST_CODE - FIRST_CODE + 1
    img = Image.new("RGBA", (SHEET_COLS * CELL_W, ((n + SHEET_COLS - 1) // SHEET_COLS) * CELL_H), (0, 0, 0, 0))
    for i in range(n):
        for y, row in enumerate(donor_cell(rows, FIRST_CODE + i)):
            for x, v in enumerate(row):
                if v:
                    rgb = OUTLINE_RGB if v == OUTLINE else (RAMP.get(v, 96),) * 3
                    img.putpixel((CELL_W * (i % SHEET_COLS) + x, CELL_H * (i // SHEET_COLS) + y), rgb + (255,))
    return img


def run_upscaler(command, rows):
    """Export, run the player's upscaler in a scratch directory, read it back."""
    with tempfile.TemporaryDirectory(prefix="bof3x-font-") as tmp:
        src, want = os.path.join(tmp, "cells.png"), os.path.join(tmp, "upscaled.png")
        export_image(rows).save(src)
        argv = [a.replace("{in}", src).replace("{out}", want).replace("{scale}", "2")
                for a in shlex.split(command, posix=(os.name != "nt"))]
        print("upscaler:", " ".join(argv))
        subprocess.run(argv, cwd=tmp, check=True)
        if not os.path.exists(want):
            made = [f for f in os.listdir(tmp) if f.lower().endswith(".png") and f != "cells.png"]
            if len(made) != 1:
                raise SystemExit("upscaler left %d new PNG files in its directory, expected 1" % len(made))
            want = os.path.join(tmp, made[0])
        return cells_from_png(want)


# The system pool: arena tag 0x4000 on the PC, a section at 0x8001A000 on the
# Western discs and 0x80014000 on the Japanese-layout ones (sibling
# regional-builds.md). Two blocks of the area script's shape behind an 8-byte
# header of two u32 offsets; 309 and 455 slots in FIRST on both sides
# (measured 2026-09-20). The engine gives the relocated pool 0x8000 bytes.
POOL_TAG, POOL_ROOM = 0x4000, 0x8000
POOL_DESTS = (0x1A000, 0x14000)

# Item and ability names: fixed-stride records in BOF3.exe's .data, every name
# field 16 bytes where the JP disc has 8 and the US 12, every numeric field
# equal to the JP disc's (534 of 534 records, measured 2026-09-20 against the
# sibling's names/*.toml). (what, PC VA of record 0, PC stride, count, offset
# of the name in the record.) The donor's table is FOUND, not addressed: by
# the PC records' numeric bytes at the donor's stride, so any Western disc
# will do, and a disc whose numbers differ is refused.
NAME_LEN, DONOR_NAME_LEN = 16, 12
JA_NAME_LEN = 8     # the JP records' name field (sibling TEXT_TABLES.md); the PC's 16 is it widened
NAME_TABLES = [
    ("consumables", 0x656B28, 22, 92, 0),
    ("key items", 0x657310, 20, 16, 0),
    ("weapons", 0x657450, 28, 83, 0),
    ("armour", 0x657D68, 26, 68, 0),
    ("accessories", 0x658450, 24, 52, 0),
    ("abilities", 0x65C4D8, 24, 227, 8),
]
KIND_NAMES = 5      # ours (DIV-0008): tag = PC VA of record 0's name field, payload = count x 16 bytes


def convert_pool(donor, base):
    d0, d1 = struct.unpack_from("<II", donor, 0)
    b0, b1 = struct.unpack_from("<II", base, 0)
    if d0 != 8 or b0 != 8:
        raise ValueError("pool header is not (8, n)")
    first, kept0 = convert_block(donor[d0:d1], base[b0:b1], POOL_ROOM)
    second, kept1 = convert_block(donor[d1:], base[b1:], POOL_ROOM)
    pool = struct.pack("<II", 8, 8 + len(first)) + first + second
    if len(pool) > POOL_ROOM:
        raise ValueError("pool is 0x%X bytes, room is 0x%X" % (len(pool), POOL_ROOM))
    return pool, kept0 + kept1


def exe_bytes(game, va, size):
    with open(os.path.join(game, "BOF3.exe"), "rb") as f:
        exe = f.read()
    pe = struct.unpack_from("<I", exe, 0x3C)[0]
    nsec, optsz = struct.unpack_from("<H", exe, pe + 6)[0], struct.unpack_from("<H", exe, pe + 20)[0]
    image_base = struct.unpack_from("<I", exe, pe + 24 + 28)[0]
    for i in range(nsec):
        vsz, rva, rsz, raw = struct.unpack_from("<IIII", exe, pe + 24 + optsz + i * 40 + 8)
        if image_base + rva <= va < image_base + rva + rsz:
            return exe[raw + va - image_base - rva:][:size]
    raise SystemExit("0x%X is not in BOF3.exe" % va)


def convert_names(game, donor):
    """[(tag, payload)] for the six tables, names from `donor` (GAME.EMI section 0)."""
    chunks, report = [], []
    for what, va, stride, count, name_at in NAME_TABLES:
        pc = exe_bytes(game, va, stride * count)
        donor_len = JA_NAME_LEN if donor_ja else DONOR_NAME_LEN
        d_stride = stride - NAME_LEN + donor_len
        # The numeric bytes of a record, name cut out - the same on both sides.
        def numbers(buf, i, n_len, st, origin=0):
            rec = buf[origin + i * st:origin + (i + 1) * st]
            return rec[:name_at] + rec[name_at + n_len:]
        want = [numbers(pc, i, NAME_LEN, stride) for i in range(count)]
        probe = want[1]
        at, found = donor.find(probe), None
        while at >= 0 and found is None:
            start = at - d_stride - (0 if name_at else donor_len)
            if start >= 0 and all(numbers(donor, i, donor_len, d_stride, start) == want[i] for i in range(count)):
                found = start
            at = donor.find(probe, at + 1)
        if found is None:
            raise SystemExit("%s: no table in the donor with the PC table's numbers at stride %d" % (what, d_stride))
        payload, kept = bytearray(), 0
        for i in range(count):
            d_name = donor[found + i * d_stride + name_at:][:donor_len].split(b"\0")[0]
            enc = ja_name(d_name, NAME_LEN - 1) if donor_ja else encode_text(d_name)
            if d_name and enc is not None and len(enc) < NAME_LEN:
                name = enc
            else:
                name, kept = pc[i * stride + name_at:][:NAME_LEN], kept + 1
            payload += name.ljust(NAME_LEN, b"\0")[:NAME_LEN]
        chunks.append((KIND_NAMES, va + name_at, bytes(payload)))
        report.append("%s %d (%d kept)" % (what, count, kept))
    return chunks, report


# ---------------------------------------------------- the Config screen

# Ours (see docs/config-screen.md): the in-game Config screen's text, which is
# in BOF3.exe's .data and in no DAT, so it cannot ride the ordinary chunk
# paths. Tag 0, one chunk, carried by FIRST.DAT.
KIND_CONFIG = 7

CONFIG_LABELS = 6       # Msg Speed .. Controller, drawn by 0x461800
CONFIG_RECORDS = 17     # the option strings at 0x6536F8, 16 bytes each
CONFIG_CTRL = 6         # the controller panel's function names at 0x66A338
CTRL_STRIDE = 21        # on the donor: a count byte, then 20 for the string

# The row -> first record and row -> option count tables, which the PC build
# and every PSX build carry identically (read 2026-09-20 from BOF3.exe
# 0x653808 / 0x653810 and from the US disc's START.EMI). Finding them is what
# locates the records: the last 12 bytes of that pair are a unique anchor.
CONFIG_TABLES = bytes([0, 3, 7, 11, 13, 15, 0, 0, 3, 4, 4, 2, 2, 0])
# Between the labels and the controller names sits this seven-byte table; it
# also occurs in the records area, so the match is taken only where a plausible
# record follows (a small count, then a printable code).
CONFIG_GAP = bytes([1, 6, 2, 3, 4, 0, 0])


def config_trim(token):
    """The text at the end of a NUL-terminated token.

    The six labels sit immediately after a table of pointers, which carries no
    NUL of its own, so the first token comes back with that table glued to its
    front. Keep the longest run of script codes at the end that begins with a
    letter or a digit.
    """
    i = len(token)
    while i > 0 and (0x2A <= token[i - 1] <= max(LAST_CODE, ext_last) or token[i - 1] == SPACE_IN):
        i -= 1
    while i < len(token) and not (0x30 <= token[i] <= 0x5A or 0x61 <= token[i] <= 0x7A):
        i += 1
    return token[i:]


def small_char(code):
    """One donor code -> the two-byte PC code of the glyph a UI string wants.

    The 8 x 8 UI cells at SMALL_APPEND_AT, which is the set the PlayStation
    draws this screen with: measured off the owner's screenshot with the panel
    as the ruler, label ink is 8 rows on an 8 advance under a 12-row banner.

    Two bytes even where a single-byte slot exists, because the screen's width
    arithmetic wants the byte length to be exactly twice the character count.
    """
    if code == SPACE_IN:
        g = SMALL_SPACE          # the blank cell; a space has no glyph either way
    elif FIRST_CODE <= code <= LAST_CODE:
        g = SMALL_APPEND_AT + code - FIRST_CODE
    elif EXT_FIRST <= code <= ext_last:
        g = EXT_SMALL_AT + code - EXT_FIRST
    else:
        return None
    return bytes([0x80 | (g >> 8), g & 0xFF])


def config_encode(raw, what, room):
    """Donor script codes -> PC codes for the 8 x 8 UI set, with the room the
    engine's slot has.

    Not `encode_char`: the Config screen is drawn by 0x516E70, whose quad is 8
    units against the ordinary draw's 12 - 16 screen pixels a character against
    24 (measured off the owner's screenshot, docs/config-screen.md section 4).
    The single-byte slots hold the 12 px cells, so UI text must name the 8 x 8
    glyphs explicitly, two bytes each. That also keeps the byte length exactly
    twice the character count, which is what the screen's own centring and
    right-alignment arithmetic assumes.
    """
    enc = [small_char(c) for c in raw]
    if not raw or any(e is None for e in enc):
        raise SystemExit("config: %s holds a code English does not have: %s" % (what, raw.hex(" ")))
    out = b"".join(enc)
    if len(out) + 1 > room:
        raise SystemExit("config: %s encodes to %d bytes, the slot holds %d" % (what, len(out) + 1, room))
    return out


def convert_config(donor):
    """[(kind, tag, payload)] for the Config screen, or [] if the donor has none.

    `donor` is the whole START.EMI. The three blocks are found by structure,
    not by English words, so a German or French disc reaches the same code.
    """
    at = donor.find(CONFIG_TABLES[1:])
    if at < 1 or donor[at - 1] != 0:
        return []
    records = at - 1 - CONFIG_RECORDS * 8
    if records < 0:
        return []

    # The labels are the six NUL-terminated strings before the gap table, and
    # the controller names the six 16-byte records after it.
    gap, seen = -1, donor.find(CONFIG_GAP)
    while seen >= 0:
        nxt = donor[seen + len(CONFIG_GAP):seen + len(CONFIG_GAP) + 2]
        if len(nxt) == 2 and 1 <= nxt[0] <= 14 and 0x21 <= nxt[1] <= 0x7E:
            gap = seen
            break
        seen = donor.find(CONFIG_GAP, seen + 1)
    if gap < 0:
        return []

    labels = [config_trim(t) for t in donor[max(0, gap - 160):gap].split(b"\0")]
    labels = [t for t in labels if t]
    if len(labels) < CONFIG_LABELS:
        raise SystemExit("config: %d label strings before the gap table, wanted %d"
                         % (len(labels), CONFIG_LABELS))
    labels = labels[-CONFIG_LABELS:]

    payload = bytearray([CONFIG_LABELS])
    for i, raw in enumerate(labels):
        # Repointed, not written in place, so the only limit is ours.
        payload += config_encode(raw, "label %d" % i, 32) + b"\0"

    payload.append(CONFIG_RECORDS)
    for i in range(CONFIG_RECORDS):
        rec = donor[records + i * 8:records + i * 8 + 8]
        raw = rec[2:].split(b"\0")[0]
        # count and x are the donor's own, verbatim: they are what puts the
        # options where the disc puts them. The PC record holds 14 bytes of
        # string after them.
        payload += bytes([rec[0], rec[1]]) + config_encode(raw, "option %d" % i, 14) + b"\0"

    payload.append(CONFIG_CTRL)
    ctrl = gap + len(CONFIG_GAP)
    for i in range(CONFIG_CTRL):
        raw = donor[ctrl + i * CTRL_STRIDE + 1:ctrl + (i + 1) * CTRL_STRIDE].split(b"\0")[0]
        payload += config_encode(raw, "controller %d" % i, 64) + b"\0"

    return [(KIND_CONFIG, 0, bytes(payload))]


# The menu's short verbs - the buttons above a menu panel (DIV-0018,
# docs/config-screen.md section 8). On the PC: 22 NUL-padded 8-byte slots at
# VERB_SLOTS behind the pointer table VERB_POINTERS, and the button rows at
# VERB_SETS, 5-byte records of a count and up to four verb indices, read by
# the row draw 0x574890. The US disc has the same three things in START.EMI
# (and STATUS.EMI, BATE.EMI): its strings packed and 4-byte aligned, a table of
# 23 pointers, and set records byte-identical to the PC's for sets 0-7 - the
# ninth ends in a 23rd verb the PC does not have. So the sets are the anchor
# and the verbs pair by index.
KIND_VERBS = 8
VERB_POINTERS, VERB_SLOTS, VERB_SETS = 0x6637E4, 0x66A228, 0x66383C
VERB_COUNT, VERB_ROOM, VERB_SHARED_SETS = 22, 8, 8


def convert_verbs(game, donor):
    """[(kind, tag, payload)] for the menu verbs, or [] if the donor has none.

    `donor` is the whole START.EMI. Found by structure: the PC's own set
    records, read out of the player's BOF3.exe, locate the donor's, the
    pointer table ends where they begin, and the strings are placed by
    requiring every one of them to start just after a NUL.
    """
    anchor = exe_bytes(game, VERB_SETS, VERB_SHARED_SETS * 5)
    table_end = donor.find(anchor)
    if table_end < 0:
        return []
    ptrs = []
    while table_end - 4 * (len(ptrs) + 1) >= 0:
        v = struct.unpack_from("<I", donor, table_end - 4 * (len(ptrs) + 1))[0]
        if not 0x80000000 <= v < 0x80200000:
            break
        ptrs.insert(0, v)
    if len(ptrs) < VERB_COUNT or any(b <= a for a, b in zip(ptrs, ptrs[1:])):
        raise SystemExit("verbs: %d ascending pointers before the set records, wanted %d"
                         % (len(ptrs), VERB_COUNT))
    table = table_end - 4 * len(ptrs)
    span = ptrs[-1] - ptrs[0]
    for tail in range(2, 17):            # the last string, its NUL and any alignment
        first = table - tail - span
        starts = [first + p - ptrs[0] for p in ptrs]
        if first > 0 and all(donor[s - 1] == 0 and donor[s] != 0 for s in starts) \
                and donor.find(b"\0", starts[-1]) < table:
            break
    else:
        raise SystemExit("verbs: no placement of the strings fits the pointer table")

    payload = bytearray([VERB_COUNT])
    for i in range(VERB_COUNT):
        raw = donor[starts[i]:donor.index(b"\0", starts[i])]
        enc = [encode_char(c) for c in raw]
        if not raw or any(e is None for e in enc):
            raise SystemExit("verbs: verb %d holds a code English does not have: %s" % (i, raw.hex(" ")))
        out = b"".join(enc)
        if len(out) + 1 > VERB_ROOM:
            raise SystemExit("verbs: verb %d encodes to %d bytes, the slot holds %d" % (i, len(out) + 1, VERB_ROOM))
        payload += out + b"\0"
    return [(KIND_VERBS, 0, bytes(payload))]


# The characters' default names (DIV-0020): New Game copies eight 0xA4-byte
# records from CHAR_RECORDS (0x437820), each starting with a 9-byte name. The
# US START.EMI has the same eight records with a 5-byte name: every byte after
# the name equals the PC's, four places earlier (measured 2026-09-21, 155 of
# 155 in all eight - the widening docs/save-interchange.md describes). Record
# 0's tail is the anchor, and every record's tail is checked.
KIND_CHAR_NAMES = 10
CHAR_RECORDS, CHAR_STRIDE, CHAR_COUNT, CHAR_NAME_PC, CHAR_NAME_US = 0x64B390, 0xA4, 8, 9, 5


def convert_char_names(game, donor):
    """[(kind, tag, payload)] for the default names, or [] if `donor` (START.EMI) has none."""
    pc = exe_bytes(game, CHAR_RECORDS, CHAR_COUNT * CHAR_STRIDE)
    tail_len = CHAR_STRIDE - CHAR_NAME_PC
    at = donor.find(pc[CHAR_NAME_PC:CHAR_STRIDE])
    if at < CHAR_NAME_US:
        return []
    base = at - CHAR_NAME_US
    payload = bytearray([CHAR_COUNT])
    for k in range(CHAR_COUNT):
        us = donor[base + k * CHAR_STRIDE:base + k * CHAR_STRIDE + CHAR_NAME_US + tail_len]
        if us[CHAR_NAME_US:] != pc[k * CHAR_STRIDE + CHAR_NAME_PC:(k + 1) * CHAR_STRIDE]:
            raise SystemExit("names: character record %d differs from the PC's past its name" % k)
        raw = us[:CHAR_NAME_US].split(b"\x00")[0]
        enc = [encode_char(c) for c in raw]
        if not raw or any(e is None for e in enc):
            raise SystemExit("names: character %d holds a code English does not have: %s" % (k, raw.hex(" ")))
        out = b"".join(enc)
        if len(out) + 1 > CHAR_NAME_PC:
            raise SystemExit("names: character %d encodes to %d bytes, the field holds %d"
                             % (k, len(out) + 1, CHAR_NAME_PC))
        payload += out + b"\x00"
    return [(KIND_CHAR_NAMES, 0, bytes(payload))]


# Manillo, the fish merchant (DIV-0020 too; the owner named him, 2026-09-21):
# the PC keeps his name once, in the 8-byte slot MERCHANT_NAME, which the
# battle and seven field functions copy 16 bytes from. On the PSX it lives in
# the fishing module each fishing area carries, as a 12-byte slot right after
# twelve bytes the PC still has at MERCHANT_ANCHOR (the PC moved the name).
KIND_MERCHANT = 11
MERCHANT_NAME, MERCHANT_ROOM, MERCHANT_ANCHOR, MERCHANT_US_SLOT = 0x669CD8, 8, 0x6608CC, 12


def convert_merchant(game, disc):
    """[(kind, tag, payload)] for the merchant's name, or [] if no area on `disc` has it."""
    anchor = exe_bytes(game, MERCHANT_ANCHOR, 12)
    for name in sorted(disc.files):
        if "/WORLD" not in name or not name.endswith(".EMI"):
            continue
        blob = disc.read(name)
        at = blob.find(anchor)
        if at < 0:
            continue
        raw = blob[at + 12:at + 12 + MERCHANT_US_SLOT].split(b"\x00")[0]
        enc = [encode_char(c) for c in raw]
        if not raw or any(e is None for e in enc):
            raise SystemExit("merchant: %s holds a code English does not have: %s" % (name, raw.hex(" ")))
        out = b"".join(enc)
        if len(out) + 1 > MERCHANT_ROOM:
            raise SystemExit("merchant: the name encodes to %d bytes, the slot holds %d" % (len(out) + 1, MERCHANT_ROOM))
        return [(KIND_MERCHANT, 0, out + b"\x00")]
    return []


# The battle's command labels (DIV-0019): seven 8-byte slots at BATTLE_SLOTS,
# drawn left-aligned in a box beside the command cross by 0x4439A0, the box
# placed from BATTLE_BOXES, four s16 a command. The US BATTLE.EMI has the same
# seven slots - "Atk" "Abl" "Use" "Exa" "Def" "Chg" "Esc", the owner's
# screenshots of the PlayStation show the first and last - immediately
# followed by the same box table, byte for byte. The box table is the anchor.
KIND_BATTLE = 9
BATTLE_SLOTS, BATTLE_BOXES, BATTLE_COUNT, BATTLE_ROOM = 0x669D28, 0x64E2C8, 7, 8


def convert_battle_commands(game, donor):
    """[(kind, tag, payload)] for the command labels, or [] if `donor` (BATTLE.EMI) has none."""
    boxes = exe_bytes(game, BATTLE_BOXES, BATTLE_COUNT * 8)
    at = donor.find(boxes)
    if at < BATTLE_COUNT * BATTLE_ROOM:
        return []
    payload = bytearray([BATTLE_COUNT])
    for i in range(BATTLE_COUNT):
        slot = donor[at - (BATTLE_COUNT - i) * BATTLE_ROOM:at - (BATTLE_COUNT - i - 1) * BATTLE_ROOM]
        raw = slot.split(b"\0")[0]
        enc = [encode_char(c) for c in raw]
        if not raw or any(e is None for e in enc):
            raise SystemExit("battle: label %d holds a code English does not have: %s" % (i, slot.hex(" ")))
        out = b"".join(enc)
        if len(out) + 1 > BATTLE_ROOM:
            raise SystemExit("battle: label %d encodes to %d bytes, the slot holds %d" % (i, len(out) + 1, BATTLE_ROOM))
        payload += out + b"\0"
    return [(KIND_BATTLE, 0, bytes(payload))]


# The battle banner's twelve messages (DIV-0052, src/game/battle_text.cpp): the
# PC's pointer table at MESSAGE_TABLE, read only by 0x44A8E0 and, for message
# 1, 0x44A990. The US BATTLE.EMI has the same twelve as 13-byte slots, found
# by the two suffixes at slots 1 and 3; copied 12 bytes at most.
KIND_MESSAGES = 12
MESSAGE_TABLE, MESSAGE_COUNT, MESSAGE_SLOT, MESSAGE_ROOM = 0x669DE0, 12, 13, 12
MESSAGE_SHIPPED = (0x669D7C, 0x669D84, 0x669D8C, 0x669D94, 0x669D9C, 0x669DA4,
                   0x669DAC, 0x669DB4, 0x669DBC, 0x669DC4, 0x669DD0, 0x669DD8)


def encode_message(raw):
    """A donor battle message -> PC bytes, or None if it holds a code English does not have."""
    out, i = bytearray(), 0
    while i < len(raw):
        b = raw[i]
        if b in LEAD and i + 1 < len(raw):
            g = SUFFIX_OF.get((b << 8) | raw[i + 1])
            if g is None:
                return None
            g = SUFFIX_AT + SUFFIX_GLYPHS.index(g)
            out += bytes([0x80 | (g >> 8), g & 0xFF])
            i += 2
            continue
        e = encode_char(b)
        if e is None:
            return None
        out += e
        i += 1
    return bytes(out)


def convert_battle_messages(game, donor):
    """[(kind, tag, payload)] for the banner messages, or [] if `donor` (BATTLE.EMI) has none."""
    table = struct.unpack("<%dI" % MESSAGE_COUNT, exe_bytes(game, MESSAGE_TABLE, 4 * MESSAGE_COUNT))
    if table != MESSAGE_SHIPPED:
        raise SystemExit("battle messages: the table at 0x%X is not the one this build expects" % MESSAGE_TABLE)
    ex, second = b"\xff\x15\x1b\x15\x1c\x00", b"\xff\x15\x1f\x15\x20\x00"
    at = donor.find(ex)
    while at >= 0 and donor[at + 2 * MESSAGE_SLOT:at + 2 * MESSAGE_SLOT + len(second)] != second:
        at = donor.find(ex, at + 1)
    if at < MESSAGE_SLOT:
        return []
    start = at - MESSAGE_SLOT
    payload = bytearray([MESSAGE_COUNT])
    for i in range(MESSAGE_COUNT):
        slot = donor[start + i * MESSAGE_SLOT:start + (i + 1) * MESSAGE_SLOT]
        raw = slot.split(b"\0")[0]
        out = encode_message(raw) if raw else None
        if out is None:
            raise SystemExit("battle: message %d holds a code English does not have: %s" % (i, slot.hex(" ")))
        if len(out) > MESSAGE_ROOM:
            raise SystemExit("battle: message %d encodes to %d bytes, %d are copied" % (i, len(out), MESSAGE_ROOM))
        payload += out + b"\0"
    return [(KIND_MESSAGES, 0, bytes(payload))]


# The enemy records (DIV-0053): each AREAnnn.DAT's kind-0 chunk at arena
# ENEMY_TAG, a 0x48-byte header and eight records of 0x8C bytes whose first
# 12 are the name; 0x8C55C8 in memory, read by Battle_CopyEnemyData 0x4946C0.
# The US AREAnnn.EMI has the same header and records at stride 0x88 in the
# section for ENEMY_DEST, an 8-byte name and every later byte the same
# (all 448 live records of the 200 areas, 2026-09-24). Each name goes over
# its own 12 bytes as a kind-0 chunk; an area whose numbers disagree keeps
# its names. The banner and the name window draw 8 bytes at most.
ENEMY_TAG, ENEMY_DEST, ENEMY_HEAD, ENEMY_COUNT = 0xC2000, 0x800E4000, 0x48, 8
ENEMY_STRIDE, ENEMY_NAME, DONOR_ENEMY_STRIDE, DONOR_ENEMY_NAME, ENEMY_SHOWN = 0x8C, 12, 0x88, 8, 8


def convert_enemy_names(base, donor):
    """[(kind, tag, payload)] for one area's enemy names, and how many were kept."""
    if len(base) < ENEMY_HEAD + ENEMY_COUNT * ENEMY_STRIDE or len(donor) < ENEMY_HEAD + ENEMY_COUNT * DONOR_ENEMY_STRIDE:
        raise ValueError("enemy records: a chunk is short")
    if base[:ENEMY_HEAD] != donor[:ENEMY_HEAD]:
        raise ValueError("enemy records: the headers differ")
    out, kept = [], 0
    for k in range(ENEMY_COUNT):
        rec = base[ENEMY_HEAD + k * ENEMY_STRIDE:][:ENEMY_STRIDE]
        d_rec = donor[ENEMY_HEAD + k * DONOR_ENEMY_STRIDE:][:DONOR_ENEMY_STRIDE]
        if rec[ENEMY_NAME:] != d_rec[DONOR_ENEMY_NAME:]:
            raise ValueError("enemy records: record %d's numbers differ" % k)
        d_name = d_rec[:DONOR_ENEMY_NAME].split(b"\0")[0]
        if not any(rec[:ENEMY_NAME]) and not d_name:
            continue
        enc = (ja_name(d_name, ENEMY_SHOWN) if donor_ja else encode_text(d_name)) if d_name else None
        if enc is None or len(enc) > ENEMY_SHOWN:
            kept += 1
            continue
        out.append((0, ENEMY_TAG + ENEMY_HEAD + k * ENEMY_STRIDE, enc.ljust(ENEMY_NAME, b"\0")))
    return out, kept


def ja_rows(disc, block):
    """One of ENDKANJI.EMI's two sheets as rows of nibbles, de-interleaved."""
    raw = emi_sections(disc.read(FONT_EMI))[block][1]
    rows = []
    for c in range(0, len(raw), 4096):
        left, right = raw[c:c + 2048], raw[c + 2048:c + 4096]
        for y in range(32):
            row = []
            for half in (left, right):
                for b in half[y * 64:(y + 1) * 64]:
                    row += [b & 0xF, b >> 4]
            rows.append(row)
    return rows


def ja_glyph(rows, cell):
    r, c = divmod(cell, JA_COLS)
    big = [[rows[JA_CELL * r + y // 2][JA_CELL * c + x // 2] for x in range(font_pc.GLYPH)]
           for y in range(font_pc.GLYPH)]
    return pc_glyph_from_rows(big)


def build_table_ja(base_table, disc):
    count = len(base_table) // font_pc.GLYPH_BYTES
    if count != JA_AT:
        raise SystemExit("base table has %d glyphs, expected %d" % (count, JA_AT))
    global ja_sheets
    table = bytearray(base_table)
    single, kanji = ja_rows(disc, 0), ja_rows(disc, 1)
    ja_sheets = (single, kanji)
    for rows in (single, kanji):
        for cell in range(JA_CELLS):
            table += ja_glyph(rows, cell)
    for code, byte in JA_HANG.items():
        slot = byte - 0x26
        table[slot * font_pc.GLYPH_BYTES:(slot + 1) * font_pc.GLYPH_BYTES] = ja_glyph(single, code)
    glyphs = len(table) // font_pc.GLYPH_BYTES
    if glyphs - 1 > GLYPH_LIMIT:
        raise SystemExit("table would hold %d glyphs, the limit is 0x%X" % (glyphs, GLYPH_LIMIT))
    return bytes(table), bytes([PC_ADVANCE]) * glyphs


def build_font(args, disc):
    global ext_last, donor_ja
    donor_ja = is_japanese(disc)
    if donor_ja:
        if args.glyphs or args.upscaler:
            raise SystemExit("--glyphs / --upscaler are for the Latin set; this is a Japanese disc")
        base = font_pc.font_chunk(os.path.join(dat_dir(args.game), "FIRST.DAT"))
        table, advances = build_table_ja(base, disc)
        print("font: %d glyphs (Japanese: %d single-byte and symbol cells at 0x%X, %d kanji at 0x%X), sha256 %s"
              % (len(table) // font_pc.GLYPH_BYTES, JA_CELLS, JA_AT, JA_CELLS, JA_KANJI_AT,
                 hashlib.sha256(table).hexdigest()))
        return [(3, 0, table), (4, PC_ADVANCE, advances)]
    rows = donor_sheet(disc)
    ext_last = latin_extension(rows)
    base = font_pc.font_chunk(os.path.join(dat_dir(args.game), "FIRST.DAT"))
    if args.glyphs and args.upscaler:
        raise SystemExit("--glyphs and --upscaler are two answers to one question")
    redrawn = None
    if args.glyphs:
        redrawn = cells_from_png(args.glyphs)
    elif args.upscaler:
        redrawn = run_upscaler(args.upscaler, rows)
    if redrawn and ext_last >= EXT_FIRST:
        raise SystemExit("--glyphs / --upscaler cover codes 0x%X..0x%X; this disc also has 0x%X..0x%X, "
                         "which the sheet does not carry yet" % (FIRST_CODE, LAST_CODE, EXT_FIRST, ext_last))
    table, advances = build_table(base, rows, redrawn, args.mono)
    n_cells = LAST_CODE - FIRST_CODE + 1
    print("font: %d glyphs (%d dialogue cells at 0x%X, %d UI cells at 0x%X, "
          "%d single-byte slots repainted), sha256 %s"
          % (len(table) // font_pc.GLYPH_BYTES, n_cells, APPEND_AT, n_cells, SMALL_APPEND_AT,
             len(ASCII_OF), hashlib.sha256(table).hexdigest()))
    if ext_last >= EXT_FIRST:
        print("      accented cells 0x%X..0x%X: dialogue at 0x%X, UI at 0x%X"
              % (EXT_FIRST, ext_last, EXT_AT, EXT_SMALL_AT))
    # kind 4 is ours (DIV-0006): a pen advance a glyph; the tag is the space's.
    return [(3, 0, table), (4, CELL_W, advances)]


# DIV-0038: F9's pause lines, ours - the PlayStation has no such screen, and
# the PC port's own are Chinese in the exe (src/game/pause_text.cpp). Four
# lines: in game "F9 again" / "any other key", then on the title the same
# pair. The wording is ours, agreed with the owner 2026-09-25. A language
# with no entry gets no chunk, and Capcom's lines stay.
PAUSE_KIND = 14
PAUSE_LINES = {
    "en": ("Press F9 again for the title screen", "Press any other key to continue",
           "Press F9 again to quit the game", "Any other key returns to the title"),
    "fr": ("Appuyez sur F9 pour l'écran titre", "Une autre touche pour continuer",
           "Appuyez sur F9 pour quitter", "Une autre touche : écran titre"),
    "de": ("F9 erneut: zum Titelbildschirm", "Andere Taste: weiterspielen",
           "F9 erneut: Spiel beenden", "Andere Taste: zum Titelbild"),
    "ja": ("もういちど F9 で タイトルへ",
           "ほかの キーで つづける",
           "もういちど F9 で ゲームを おわる",
           "ほかの キーで タイトルへ"),
}
# Text -> donor codes, then encode_char as for the disc's own text. Latin:
# letters and digits are their own codes (ASCII_OF), and the punctuation and
# accent these lines use are the donor's (ASCII_OF; e acute 0xA1, see
# EXT_FIRST). Japanese: the kana are the JP script's gojuon run from 0x5B - 46
# hiragana, 9 small, 25 voiced, then the same in katakana from 0xAB (the
# sibling's tools/jptext.py, docs/TEXT_ENGINE.md) - and the long-vowel bar is
# single-byte 0x2D, as the name fields use it.
LATIN_CODE = {" ": SPACE_IN, "'": 0x8E, ":": 0x8F, "é": 0xA1}
JA_HIRA = ("あいうえおかきくけこさしすせそ"
           "たちつてとなにぬねのはひふへほ"
           "まみむめもやゆよらりるれろわをん"
           "ぁぃぅぇぉっゃゅょ"
           "がぎぐげござじずぜぞだぢづでど"
           "ばびぶべぼぱぴぷぺぽ")
assert len(JA_HIRA) == 80
JA_CODE = {c: 0x5B + i for i, c in enumerate(JA_HIRA)}
JA_CODE.update({chr(ord(c) + 0x60): 0xAB + i for i, c in enumerate(JA_HIRA)})
JA_CODE.update({" ": JA_SPACE, "ー": 0x2D})


def pause_code(ch):
    if donor_ja:
        if ch in JA_CODE:
            return JA_CODE[ch]
        return ord(ch) if "0" <= ch <= "9" or "A" <= ch <= "Z" else None
    if ch in LATIN_CODE:
        return LATIN_CODE[ch]
    return ord(ch) if ch.isascii() and ch.isalnum() else None


def pause_width(line, advances, space):
    """The line's width in units, as the DLL's TextAdvance_Of reads it."""
    w, i = 0, 0
    while i < len(line):
        b = line[i]
        if b == 0x20:
            w, i = w + space, i + 1
            continue
        g, i = (((b & 0x7F) << 8) | line[i + 1], i + 2) if b & 0x80 else (b - 0x26, i + 1)
        w += advances[g] if g < len(advances) else PC_ADVANCE
    return w


def build_pause(args, font_chunks):
    lines = PAUSE_LINES.get(args.lang)
    if lines is None:
        print("pause lines: none written for '%s'; the exe's own stay" % args.lang)
        return []
    if (args.lang == "ja") != donor_ja:
        raise SystemExit("pause lines: --lang %s with a %s disc" % (args.lang, "Japanese" if donor_ja else "Latin"))
    space, advances = [(tag, payload) for kind, tag, payload in font_chunks if kind == 4][0]
    body = bytearray()
    for text in lines:
        out = bytearray()
        for ch in text:
            code = pause_code(ch)
            e = None if code is None else encode_char(code)
            if e is None:
                raise SystemExit("pause lines: %r has no glyph in this overlay (in %r)" % (ch, text))
            out += e
        # WinMain centres each on 160 (PauseText_X), so it must fit 320 units.
        width = pause_width(out, advances, space)
        if width > 320:
            raise SystemExit("pause lines: %r is %d units wide; the screen is 320" % (text, width))
        body += out + b"\0"
    print("pause lines: 4 in '%s', widest %d units"
          % (args.lang, max(pause_width(bytes(l), advances, space) for l in body.split(b"\0")[:4])))
    return [(PAUSE_KIND, 0, bytes(body))]


# The strip is kind-0 tag 0x8000 in FIRST.DAT and the 0x200-byte section for
# 0x80033800 in FIRST.EMI (measured 2026-09-20; 0x8200 / 0x8400 pair with
# 0x80033A00 / 0x80033C00 the same way).
CLUT_TAG, CLUT_ROW = 0x8000, 32
CLUT_DESTS = (0x33800, 0x2B800)   # US and EU; JP, 0x8000 lower like the pool (DAT_CONTAINER.md section 2)


def build_white_clut(args, disc):
    """DIV-0013. FIRST's CLUT strip with row 0 - white text - as the donor has it.

    Measured 2026-09-20: the strips differ in that row alone. The discs (US and
    JP alike) have 25, 23, 20, 17, 13, 8, 4 for indices 1-7 and (0, 0, 1) at 8;
    the port has 28, 27, 25, 24, 21, 17, 12 and (0, 0, 6) - brightened for its
    anti-aliased Chinese glyphs. The donor's cells put their drop shadow at
    index 7, which the port's row draws mid-grey. Every other row is the
    port's own, untouched."""
    blob, chunks = dat.load(os.path.join(dat_dir(args.game), "FIRST.DAT"))
    base = [c for c in chunks if c.kind == 0 and c.tag == CLUT_TAG]
    donor = [s for dest, s in emi_sections(disc.read(disc.find("FIRST.EMI")[0])) if dest & 0x7FFFFFFF in CLUT_DESTS]
    if len(base) != 1 or len(donor) != 1 or len(donor[0]) < CLUT_ROW:
        raise SystemExit("FIRST: no CLUT strip to take the white row from")
    strip = bytearray(blob[base[0].offset:base[0].offset + base[0].size])
    strip[:CLUT_ROW] = donor[0][:CLUT_ROW]
    return [(0, CLUT_TAG, bytes(strip))]


# ---------------------------------------------------------------- the title menu

# DIV-0014. The title menu is artwork, not text (docs/title-menu.md): image
# chunk 0x1C000200 of START.DAT, a 256 x 256 4bpp page at VRAM (896, 0), and
# the section of the same tag in the discs' START.EMI. The port's draw
# 0x5888D0 takes row i from (0, 32 i), 32 tall, as wide as a table in its code
# says; the discs keep NEW GAME at (0, 0) and LOAD GAME at (0, 16), 16 tall.
# Both are drawn through the CLUTs of kind-0 tag 0x8600, identical on PC and
# disc, so the disc's nibbles can be used as they are.
TITLE_TAG, TITLE_KIND, TITLE_BAND, TITLE_CAP = 0x1C000200, 6, 32, 16
# The sheet the boxes below were measured on, 2026-09-20: US and JP alike.
TITLE_DONOR_SHA256 = "e06a47cfcb16a858"     # first 16 hex digits
TITLE_NEW, TITLE_LOAD = (0, 0, 130), (0, 16, 140)       # x, y, width of the two strings
# Letter boxes (x0, x1 inclusive, y of the 16-row band) the third row is cut from.
TITLE_LETTER = {"N": (1, 16, 0), "E": (19, 32, 0), "G": (64, 79, 0), "L": (1, 15, 16), "O": (16, 32, 16)}
TITLE_SHADOW = 15


def tiles_to_rows(data, tiles_w):
    """A kind-1 payload - 0x800-byte tiles of 32 x 32 words, row-major - as rows of 4bpp texels."""
    count = len(data) // 0x800
    rows = [[0] * (tiles_w * 128) for _ in range(count // tiles_w * 32)]
    for t in range(count):
        tx, ty = t % tiles_w, t // tiles_w
        for y in range(32):
            line = rows[ty * 32 + y]
            for i, b in enumerate(data[t * 0x800 + y * 64:t * 0x800 + (y + 1) * 64]):
                line[tx * 128 + 2 * i], line[tx * 128 + 2 * i + 1] = b & 0xF, b >> 4
    return rows


def rows_to_tiles(rows, tiles_w):
    out = bytearray()
    for ty in range(len(rows) // 32):
        for tx in range(tiles_w):
            for y in range(32):
                line = rows[ty * 32 + y][tx * 128:(tx + 1) * 128]
                out += bytes(line[i] | (line[i + 1] << 4) for i in range(0, 128, 2))
    return bytes(out)


def title_letter(sheet, name):
    x0, x1, y0 = TITLE_LETTER[name]
    cell = [sheet[y][x0:x1 + 1] for y in range(y0, y0 + TITLE_CAP)]
    if name == "O":     # its shadow column is also where the D's lower serif starts
        cell[14][-1] = cell[15][-1] = 0
    return cell


def title_shadow(cell, fresh):
    """The lettering's drop shadow is one down and one right (it predicts 83% of the
    donor's own shadow pixels; the rest is the artist's touching up)."""
    for x, y in fresh:
        if y + 1 < len(cell) and x + 1 < len(cell[0]) and cell[y + 1][x + 1] == 0:
            cell[y + 1][x + 1] = TITLE_SHADOW


def title_stem(sheet):
    """I: the L with its foot cut off after the stem's own serif, and the cut closed."""
    cell = [row[:8] for row in title_letter(sheet, "L")]
    cell[13][7] = 0                 # the foot's upturned tip does not reach here; be sure
    cell[14][7] = TITLE_SHADOW      # where the foot went on: the serif's shadow instead
    return cell


def title_f(sheet):
    """F: the E down to its middle arm, standing on the I's lower stem and serif."""
    e, stem = title_letter(sheet, "E"), title_stem(sheet)
    # E's stem is columns 1-4 of its box and L's 2-5 of its own, L's box having
    # a column for the tip of its lower serif: F's box gets that column too.
    cell = [[0] + row for row in e[:10]] + [[0] * (len(e[0]) + 1) for _ in range(6)]
    for y in range(10, TITLE_CAP):
        cell[y][:8] = stem[y]
    return cell


def title_c(sheet):
    """C: the G without its spur, its lower terminal the upper one turned over.

    The upper terminal hangs three rows below the top stroke, 3, 2 and 1 pixels
    wide (box columns 11-13, rows 2-4). Turned over it stands on the end of the
    bottom stroke in rows 12-10, same columns. The lettering is shaded by row - greys 1-5 at the
    top, blues 7-11 at the bottom, the same five steps - so a top pixel of
    step n becomes 6 + n."""
    cell = title_letter(sheet, "G")
    for y in range(9, 13):
        for x in range(8, len(cell[0])):
            cell[y][x] = 0
    fresh = []
    for src, dst in ((2, 12), (3, 11), (4, 10)):
        for x in range(11, 14):
            v = title_letter(sheet, "G")[src][x]
            if 1 <= v <= 5:
                cell[dst][x] = 6 + v
                fresh.append((x, dst))
    title_shadow(cell, fresh)
    return cell


def build_title(args, disc):
    """NEW GAME and LOAD GAME as the disc has them, and CONFIG cut from their letters."""
    found = disc.find("START.EMI")
    base_blob, base_chunks = dat.load(os.path.join(dat_dir(args.game), "START.DAT"))
    base = [c for c in base_chunks if c.kind == 1 and c.tag == TITLE_TAG]
    donor = [s for dest, s in emi_sections(disc.read(found[0])) if dest == TITLE_TAG] if found else []
    if len(base) != 1 or len(donor) != 1 or len(donor[0]) != base[0].size:
        raise SystemExit("START: no title menu sheet to rebuild")
    if not hashlib.sha256(donor[0]).hexdigest().startswith(TITLE_DONOR_SHA256):
        print("title menu: this disc's sheet is not the one the letters were measured on; left as shipped")
        return []
    sheet = tiles_to_rows(donor[0], 2)
    page = [[0] * 256 for _ in range(256)]
    top = (TITLE_BAND - TITLE_CAP) // 2     # the port's rows are 32 tall about the same centre
    widths = []
    for i, (x0, y0, w) in enumerate((TITLE_NEW, TITLE_LOAD)):
        for y in range(TITLE_CAP):
            page[TITLE_BAND * i + top + y][:w] = sheet[y0 + y][x0:x0 + w]
        widths.append(w)
    # CONFIG. Gaps in columns, by eye against NEW GAME's own spacing.
    word = ((title_c(sheet), 1), (title_letter(sheet, "O"), 1), (title_letter(sheet, "N"), 2),
            (title_f(sheet), 0), (title_stem(sheet), 2), (title_letter(sheet, "G"), 0))
    pen = 1
    for cell, gap in word:
        for y, row in enumerate(cell):
            line = page[TITLE_BAND * 2 + top + y]
            for x, v in enumerate(row):
                if v and (line[pen + x] in (0, TITLE_SHADOW)):
                    line[pen + x] = v
        pen += len(cell[0]) + gap
    widths.append(pen + (pen & 1))          # even, so that 160 - w / 2 centres it
    # kind 6 is ours (DIV-0014): the three row widths the draw's code holds.
    return [(1, TITLE_TAG, rows_to_tiles(page, 2)), (TITLE_KIND, 0, bytes(widths))]


# ---------------------------------------------------------------- the world-map place plates

# DIV-0055 (docs/world-map.md section 5, docs/world-map-hud.md section 5.2). The
# place names on the ten world maps are paint on each map's page, and the
# discs repainted them per language. On the PC, measured 2026-09-24 against
# the JP disc in all ten areas: the page (kind 1, tag = the dest) differs from
# JP's only in 4..7 tiles, every one of them inside the tiles each Western
# disc replaces; and the three data sections that go with it are JP's byte
# for byte:
#   - 0x800D3800 -> tag 0xB0000: the map's sprite frames, the plates' among
#     them, sized per plate (a Western disc's is 4..12 bytes longer);
#   - 0x800E3800 -> tag 0xC0000: 60..400 bytes that differ only where a disc
#     rearranged the page (AREA065 on every Western disc, AREA121 on the
#     German), so read as where the moved blocks now are;
#   - the palette section, 0x8002D800 on JP and 0x80035800 on the Western
#     discs -> tag 0xA000: its first CLUT is the plates' - each disc's plates
#     render right only under that disc's own (rendered 2026-09-24).
# So the donor's four go over the PC's whole. The world map's code, compiled
# into BOF3.exe from the JP overlay, is untouched; the Western overlays'
# code differs from JP's by four bytes in eight of the ten areas.
PLATE_AREAS = ("AREA016", "AREA033", "AREA045", "AREA065", "AREA087",
               "AREA088", "AREA115", "AREA121", "AREA151", "AREA152")
PLATE_PAGE = 0x0E001000
PLATE_DATA = {0x800D3800: 0xB0000, 0x800E3800: 0xC0000, 0x8002D800: 0xA000, 0x80035800: 0xA000}


def build_plates(game, disc):
    """{DAT name: [(kind, tag, payload)]} for the world maps' place plates."""
    out = {}
    for area in PLATE_AREAS:
        found = disc.find(area + ".EMI")
        if not found:
            raise SystemExit("plates: %s.EMI is not on this disc" % area)
        blob, chunks = dat.load(os.path.join(dat_dir(game), area + ".DAT"))
        pc = {(c.kind, c.tag): blob[c.offset:c.offset + c.size] for c in chunks}
        donor = {}
        for dest, s in emi_sections(disc.read(found[0])):
            if dest == PLATE_PAGE:
                donor[(1, PLATE_PAGE)] = s
            elif dest in PLATE_DATA:
                donor[(0, PLATE_DATA[dest])] = s
        if len(donor) != 1 + len(set(PLATE_DATA.values())):
            raise SystemExit("plates: %s has %d of the four sections" % (area, len(donor)))
        for key, s in donor.items():
            if key not in pc:
                raise SystemExit("plates: %s has no chunk kind %d tag 0x%X" % (area, key[0], key[1]))
            if key[1] == 0xB0000:
                # Sized per plate, so not the PC's size. A header of dword
                # offsets, section-relative, the first being the header's own
                # size (0x1C in AREA016, 0x30 in AREA065); where a disc
                # rearranged the page it reordered the blocks too (AREA065:
                # the third at 0x69C on the PC, 0x4B8 on the US disc). So the
                # check is the shape: the same header, every offset inside.
                head = struct.unpack_from("<I", pc[key])[0]
                if struct.unpack_from("<I", s)[0] != head or head % 4 or head > len(s):
                    raise SystemExit("plates: %s's frame section header is not the PC's" % area)
                if any(not head <= o < len(s) for o in struct.unpack_from("<%dI" % (head // 4), s)[1:]):
                    raise SystemExit("plates: %s's frame section points outside itself" % area)
            elif len(s) != len(pc[key]):
                raise SystemExit("plates: %s chunk 0x%X is %d bytes, the PC's %d"
                                 % (area, key[1], len(s), len(pc[key])))
        out[area + ".DAT"] = [(k, t, s) for (k, t), s in sorted(donor.items())]
    return out


def cmd_all(args):
    """Every overlay, in one pass, one file written per shipped DAT that needs one."""
    disc, d = psx_disc.Disc(args.disc), dat_dir(args.game)
    overlays = {"FIRST.DAT": build_font(args, disc)}
    overlays["FIRST.DAT"] += build_pause(args, overlays["FIRST.DAT"])   # after the glyphs it names
    if not args.pc_white:
        overlays["FIRST.DAT"] += build_white_clut(args, disc)
    title = build_title(args, disc)
    if title:
        overlays["START.DAT"] = title
    texts = pools = kept_text = kept_pool = enemies = kept_enemy = 0
    for name in sorted(os.listdir(d)):
        stem, ext = os.path.splitext(name)
        if ext.upper() != ".DAT" or "." in stem or (args.only and stem.upper() not in (args.only.upper(), "FIRST")):
            continue
        blob, chunks = dat.load(os.path.join(d, name))
        found = disc.find(stem + ".EMI")
        if not found:
            continue
        sections = None
        for c in chunks:
            if c.kind != 0 or c.tag not in (0, POOL_TAG, ENEMY_TAG):
                continue
            if sections is None:
                sections = emi_sections(disc.read(found[0]))
            base = blob[c.offset:c.offset + c.size]
            try:
                if c.tag == ENEMY_TAG:
                    donor = [s for dest, s in sections if dest & 0x7FFFFFFF == ENEMY_DEST & 0x7FFFFFFF]
                    if not donor:
                        continue
                    names, kept = convert_enemy_names(base, donor[0])
                    enemies, kept_enemy = enemies + len(names), kept_enemy + kept
                    overlays.setdefault(name, []).extend(names)
                    continue
                if c.tag == 0:
                    donor = [s for dest, s in sections if dest & 0x7FFFFFFF == 0x10000]
                    if not donor:
                        continue
                    block, kept = convert_block(donor[0], base, TEXT_ROOM)
                    texts, kept_text = texts + 1, kept_text + kept
                else:
                    donor = [s for want in POOL_DESTS for dest, s in sections if dest & 0x7FFFFFFF == want]
                    if not donor:
                        continue
                    block, kept = convert_pool(donor[0], base)
                    pools, kept_pool = pools + 1, kept_pool + kept
            except ValueError as e:
                print("  SKIP %s tag %X: %s" % (name, c.tag, e))
                continue
            overlays.setdefault(name, []).append((0, c.tag, block))
    start_emi = disc.find("START.EMI")
    if donor_ja:
        # The exe's own strings (kinds 7..12) are found with the US layouts and
        # drawn through the Latin layout patches; a Japanese overlay leaves
        # them as shipped for now.
        print("config, verbs, character names, merchant, battle labels and messages: "
              "not built for a Japanese disc")
        start_emi = None
    if start_emi and not args.only:
        cfg = convert_config(disc.read(start_emi[0]))
        overlays["FIRST.DAT"] += cfg
        print("config screen: " + ("6 labels, 17 options, 6 controller names" if cfg else "not found on this disc"))
        verbs = convert_verbs(args.game, disc.read(start_emi[0]))
        overlays["FIRST.DAT"] += verbs
        print("menu verbs: " + ("%d" % VERB_COUNT if verbs else "not found on this disc"))
        chars = convert_char_names(args.game, disc.read(start_emi[0]))
        overlays["FIRST.DAT"] += chars
        print("character names: " + ("%d" % CHAR_COUNT if chars else "not found on this disc"))
        merchant = convert_merchant(args.game, disc)
        overlays["FIRST.DAT"] += merchant
        print("merchant name: " + ("found" if merchant else "not found on this disc"))
    battle_emi = None if donor_ja else disc.find("BATTLE.EMI")
    if battle_emi and not args.only:
        cmds = convert_battle_commands(args.game, disc.read(battle_emi[0]))
        overlays["FIRST.DAT"] += cmds
        print("battle commands: " + ("%d" % BATTLE_COUNT if cmds else "not found on this disc"))
        msgs = convert_battle_messages(args.game, disc.read(battle_emi[0]))
        overlays["FIRST.DAT"] += msgs
        print("battle messages: " + ("%d" % MESSAGE_COUNT if msgs else "not found on this disc"))

    game_emi = disc.find("GAME.EMI")
    if game_emi and not args.only:
        names, report = convert_names(args.game, emi_sections(disc.read(game_emi[0]))[0][1])
        overlays["FIRST.DAT"] += names
        print("names: " + ", ".join(report))
    print("enemy names: %d (%d kept)" % (enemies, kept_enemy))
    if not args.only:
        plates = build_plates(args.game, disc)
        for name, chunks in plates.items():
            overlays.setdefault(name, []).extend(chunks)
        print("place plates: %d world maps" % len(plates))
    if donor_ja and ja_pairs:
        overlays["FIRST.DAT"] = ja_pair_chunks(overlays["FIRST.DAT"])
        print("pair codes: %d from glyph 0x%X (DIV-0057)" % (len(ja_pairs), PAIR_AT))
    for name, chunks in overlays.items():
        write_overlay(os.path.join(d, "%s.%s" % (args.lang, name)), chunks)
    print("%d overlay files in %s: %d area texts (%d slots kept as shipped), %d system pools (%d kept)"
          % (len(overlays), d, texts, kept_text, pools, kept_pool))


def cmd_export(args):
    img = export_image(donor_sheet(psx_disc.Disc(args.disc)))
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    img.save(args.out)
    print("wrote %s: %d x %d, cells %d x %d; bring it back at up to 24 x 24 a cell (16 x 24 for 2x)"
          % (args.out, img.width, img.height, CELL_W, CELL_H))


def cmd_sheet(args):
    from PIL import Image
    rows = donor_sheet(psx_disc.Disc(args.disc))
    codes = list(range(FIRST_CODE, LAST_CODE + 1))
    img = Image.new("L", (26 * 20, 26 * ((len(codes) + 19) // 20)), 40)
    for i, code in enumerate(codes):
        g = pc_glyph(donor_cell(rows, code))
        for y, row in enumerate(font_pc.glyph_pixels(g, 0)):
            for x, v in enumerate(row):
                img.putpixel((26 * (i % 20) + x, 26 * (i // 20) + y), 255 if v == 1 else (0 if v == 0 else 255 - v * 28))
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    img.save(args.out)
    print("wrote", args.out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name, fn in (("all", cmd_all), ("sheet", cmd_sheet), ("export", cmd_export)):
        s = sub.add_parser(name)
        s.add_argument("--disc", required=True)
        s.add_argument("--lang", default="en")
        if name == "all":
            s.add_argument("--glyphs", help="an upscaled sheet to use instead of doubling the donor's cells")
            s.add_argument("--upscaler", help="a command that upscales {in} by {scale} (to {out}, or to one new PNG)")
            s.add_argument("--mono", action="store_true", help="every glyph advances 8, as the US release; default tightens ' and ,")
            s.add_argument("--pc-white", action="store_true", help="keep the port's brightened white text palette; default restores the disc's (DIV-0013)")
        if name in ("sheet", "export"):
            s.add_argument("--out", required=True)
        else:
            s.add_argument("--game", required=True)
        if name == "all":
            s.add_argument("--only")
        s.set_defaults(fn=fn)
    args = ap.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
