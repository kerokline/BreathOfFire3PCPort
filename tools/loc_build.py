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
GLYPH_LIMIT = 0xA00              # 0x516C94: cmp cx, 0xA00 / jbe
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


def donor_cell(rows, code):
    i = code - FIRST_CODE
    x0, y0 = CELL_W * (i % CELLS_PER_ROW), CELLS_Y0 + CELL_H * (i // CELLS_PER_ROW)
    return [rows[y0 + y][x0:x0 + CELL_W] for y in range(CELL_H)]


def pc_glyph(cell):
    """8 x 12 nibbles -> one 288-byte PC glyph, doubled, left-aligned."""
    out = bytearray()
    for y in range(font_pc.GLYPH):
        src = cell[y // 2]
        row = [src[x // 2] if x < 2 * CELL_W else 0 for x in range(font_pc.GLYPH)]
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
    assert len(table) // font_pc.GLYPH_BYTES - 1 <= GLYPH_LIMIT
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
    print("font: %d glyphs (%d appended at 0x%X, %d single-byte slots repainted), sha256 %s"
          % (len(table) // font_pc.GLYPH_BYTES, LAST_CODE - FIRST_CODE + 1, APPEND_AT, len(ASCII_OF),
             hashlib.sha256(table).hexdigest()))
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


def cmd_all(args):
    """Every overlay, in one pass, one file written per shipped DAT that needs one."""
    disc, d = psx_disc.Disc(args.disc), dat_dir(args.game)
    overlays = {"FIRST.DAT": build_font(args, disc)}
    if not args.pc_white:
        overlays["FIRST.DAT"] += build_white_clut(args, disc)
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
