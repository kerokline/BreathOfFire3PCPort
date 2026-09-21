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

def encode_char(code):
    if code == SPACE_IN:
        return bytes([SPACE_OUT])
    if code in ASCII_OF:
        return bytes([ASCII_OF[code]])
    if FIRST_CODE <= code <= LAST_CODE:
        g = APPEND_AT + code - FIRST_CODE
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
            ok = False
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
    n = struct.unpack_from("<H", donor, 0)[0] // 2
    if struct.unpack_from("<H", base, 0)[0] // 2 != n:
        raise ValueError("slot counts differ")
    d_offs = struct.unpack_from("<%dH" % n, donor, 0)
    b_offs = struct.unpack_from("<%dH" % n, base, 0)
    body, placed, table, kept = bytearray(), {}, [], 0
    for slot in range(n):
        key = ("d", d_offs[slot])
        if key not in placed:
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
        d_stride = stride - NAME_LEN + DONOR_NAME_LEN
        # The numeric bytes of a record, name cut out - the same on both sides.
        def numbers(buf, i, n_len, st, origin=0):
            rec = buf[origin + i * st:origin + (i + 1) * st]
            return rec[:name_at] + rec[name_at + n_len:]
        want = [numbers(pc, i, NAME_LEN, stride) for i in range(count)]
        probe = want[1]
        at, found = donor.find(probe), None
        while at >= 0 and found is None:
            start = at - d_stride - (0 if name_at else DONOR_NAME_LEN)
            if start >= 0 and all(numbers(donor, i, DONOR_NAME_LEN, d_stride, start) == want[i] for i in range(count)):
                found = start
            at = donor.find(probe, at + 1)
        if found is None:
            raise SystemExit("%s: no table in the donor with the PC table's numbers at stride %d" % (what, d_stride))
        payload, kept = bytearray(), 0
        for i in range(count):
            d_name = donor[found + i * d_stride + name_at:][:DONOR_NAME_LEN].split(b"\0")[0]
            enc = [encode_char(c) for c in d_name]
            if d_name and all(e is not None for e in enc) and sum(map(len, enc)) < NAME_LEN:
                name = b"".join(enc)
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
    while i > 0 and (0x2A <= token[i - 1] <= 0x93 or token[i - 1] == SPACE_IN):
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


def build_font(args, disc):
    rows = donor_sheet(disc)
    base = font_pc.font_chunk(os.path.join(dat_dir(args.game), "FIRST.DAT"))
    if args.glyphs and args.upscaler:
        raise SystemExit("--glyphs and --upscaler are two answers to one question")
    redrawn = None
    if args.glyphs:
        redrawn = cells_from_png(args.glyphs)
    elif args.upscaler:
        redrawn = run_upscaler(args.upscaler, rows)
    table, advances = build_table(base, rows, redrawn, args.mono)
    n_cells = LAST_CODE - FIRST_CODE + 1
    print("font: %d glyphs (%d dialogue cells at 0x%X, %d UI cells at 0x%X, "
          "%d single-byte slots repainted), sha256 %s"
          % (len(table) // font_pc.GLYPH_BYTES, n_cells, APPEND_AT, n_cells, SMALL_APPEND_AT,
             len(ASCII_OF), hashlib.sha256(table).hexdigest()))
    # kind 4 is ours (DIV-0006): a pen advance a glyph; the tag is the space's.
    return [(3, 0, table), (4, CELL_W, advances)]


# The strip is kind-0 tag 0x8000 in FIRST.DAT and the 0x200-byte section for
# 0x80033800 in FIRST.EMI (measured 2026-09-20; 0x8200 / 0x8400 pair with
# 0x80033A00 / 0x80033C00 the same way).
CLUT_TAG, CLUT_DEST, CLUT_ROW = 0x8000, 0x33800, 32


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
    donor = [s for dest, s in emi_sections(disc.read(disc.find("FIRST.EMI")[0])) if dest & 0x7FFFFFFF == CLUT_DEST]
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


def cmd_all(args):
    """Every overlay, in one pass, one file written per shipped DAT that needs one."""
    disc, d = psx_disc.Disc(args.disc), dat_dir(args.game)
    overlays = {"FIRST.DAT": build_font(args, disc)}
    if not args.pc_white:
        overlays["FIRST.DAT"] += build_white_clut(args, disc)
    title = build_title(args, disc)
    if title:
        overlays["START.DAT"] = title
    texts = pools = kept_text = kept_pool = 0
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
            if c.kind != 0 or c.tag not in (0, POOL_TAG):
                continue
            if sections is None:
                sections = emi_sections(disc.read(found[0]))
            base = blob[c.offset:c.offset + c.size]
            try:
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
    battle_emi = disc.find("BATTLE.EMI")
    if battle_emi and not args.only:
        cmds = convert_battle_commands(args.game, disc.read(battle_emi[0]))
        overlays["FIRST.DAT"] += cmds
        print("battle commands: " + ("%d" % BATTLE_COUNT if cmds else "not found on this disc"))

    game_emi = disc.find("GAME.EMI")
    if game_emi and not args.only:
        names, report = convert_names(args.game, emi_sections(disc.read(game_emi[0]))[0][1])
        overlays["FIRST.DAT"] += names
        print("names: " + ", ".join(report))
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
