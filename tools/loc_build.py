#!/usr/bin/env python
"""Build per-language overlay DATs from the player's own disc.

docs/dialogue-localisation.md. An overlay `<lang>.<NAME>.DAT` is an ordinary
DAT container (tools/dat.py) holding only the chunks that differ; the engine
walks it after the original file, so its chunks land on top.

    python tools/loc_build.py all   --disc DISC --game bof3 [--lang en-US] [--upscaler CMD | --glyphs PNG] [--only AREA000]
    python tools/loc_build.py all   --discs CDImage --game bof3 [--dry-run]     # every held disc in the directory
    python tools/loc_build.py all   --disc DISC --cache CACHE [--lang en-US]    # disc-only: no PC install
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

## Every valid disc at once: `--discs DIR`

`all --discs DIR` is the "all valid game discs" mode. Every disc image in
DIR (`.cue`, `.iso`, and a `.bin` no cue there names) is identified the way
the importer identifies a source: every file of its ISO9660 tree hashed
against fixtures.toml's per-file manifests (`importer.identify`). A disc
that matches a held PlayStation release gets one overlay set under that
release's own tag (`en-US.*`, `en-150.*`, `fr-FR.*`, `de-DE.*`, `ja-JP.*`);
anything else is reported and skipped - an image fixtures.toml does not hold,
a second copy of a build already built, the PC port's own files, and the
PSP discs, which `all` has not been validated against. `--lang` is refused
here: a tag only ever comes from identity. The game directory is checked
first: `BOF3.exe` must hash as the catalogued port, and its `DAT/` is named
if it is the shipped tree. `--dry-run` identifies and writes nothing (with one
`--disc` too). English discs are built first: the French and German title
menus borrow their CONFIG row from an English overlay already built.

## Without the PC install: `--cache CACHE`

`--cache` replaces `--game` with an importer cache built from a PlayStation
disc alone (`importer.py build --source DISC`, which writes base/dat/ and
base/exe/; `importer.py build --lang` runs this itself when no PC source is
given). The text blocks are converted on the donor's own slot table, the font
is appended to a blank table where the PC build keeps the port's Chinese one,
the exe anchors come from base/exe/data.bin or, where the disc's image does
not carry them, from PC_SHA. Overlays go to CACHE/loc/<tag>/dat/<NAME>.DAT.
What this gives up against a build over the PC install, measured container by
container, is docs/loc-build-disc-only.md.

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
import re
import shlex
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_reuse  # noqa: E402,F401  (first: run by importer.py's setup, records this run into its step)
import dat        # noqa: E402
import font_pc    # noqa: E402
import language_tags  # noqa: E402  (the retired bare codes, DIV-0005)
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
# A disc-only build (DiscCache) has no shipped table to keep the second
# suffix's two glyphs from, so it takes the cells the disc's own mapper gives
# its codes: on the 21-wide 12 px grid a 0x15 nn code is cell nn + 0x5B (the
# sibling's docs/TEXT_ENGINE.md, 0x80151F4C) - which puts EX's 0x151B / 0x151C
# at EX_CELLS above, and 0x151F / 0x1520 at these two. Their shapes are not
# identified (docs/loc-build-disc-only.md section 4).
SECOND_CELLS = ((204, 60), (216, 60))
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


# ---------------------------------------------------------------- a PSP disc (docs/loc-build-psp.md)

# A PSP disc carries the PlayStation's EMIs under PSP_GAME/USRDIR/<region>/
# where a PSX disc has BIN/, in the same container form but for two header
# changes (region-diff.md 5.1, region_diff.nd): every RAM destination of a
# type-0 or type-1 section has bit 31 cleared, and eight type-3 image words
# have bit 0 set (measured 2026-10-12 over every EMI of PSP-JP against PSX-JP:
# 2,683 bit-31 words, 8 image words, nothing else). `PspDisc` presents such a
# disc as a PSX disc: paths under BIN/, and each EMI's section table with both
# undone, so every reader below sees the PlayStation's destinations. The
# section bodies are the PSP's own. What it cannot present is the boot EXE:
# the PSP's names, verbs and tables are in BOOT.BIN, a native ELF (`boot`).

PSP_ROOT = "PSP_GAME/USRDIR/"
PSP_BOOT = "PSP_GAME/SYSDIR/BOOT.BIN"
PSP_EMI_TYPE = 0x1C        # a section entry's type, u16 (region_diff.emi_sections_blob)


def psp_emi(blob):
    """An EMI from a PSP disc with its section table as the PlayStation's."""
    if blob[8:16] != b"MATH_TBL":
        return blob
    out = bytearray(blob)
    count, = struct.unpack_from("<I", blob, 0)
    for i in range(count):
        dest, = struct.unpack_from("<I", blob, 0x14 + 16 * i)
        kind, = struct.unpack_from("<H", blob, PSP_EMI_TYPE + 16 * i)
        if kind in (0, 1) and 0 < dest < 0x80000000:
            dest |= 0x80000000
        elif kind == 3:
            dest &= ~1
        struct.pack_into("<I", out, 0x14 + 16 * i, dest)
    return bytes(out)


class PspDisc:
    """A PSP disc read as a PlayStation one (above). `build` is the fixtures.toml
    build its UMD_DATA.BIN serial names (psp-jp, psp-eu), `japanese` whether
    its text is (that build's tag)."""
    psp = True

    def __init__(self, disc):
        import importer
        self.disc = disc
        roots = sorted({p[len(PSP_ROOT):].split("/", 1)[0] for p in disc.files
                        if p.startswith(PSP_ROOT) and p.endswith(".EMI")})
        if len(roots) != 1:
            raise SystemExit("PSP disc: %d data roots under %s (%s), expected one" % (len(roots), PSP_ROOT, roots))
        self.root = PSP_ROOT + roots[0] + "/"
        self.path_of = {"BIN/" + p[len(self.root):]: p for p in disc.files if p.startswith(self.root)}
        self.files = dict((k, disc.files[v]) for k, v in self.path_of.items())
        serial = disc.read("UMD_DATA.BIN").split(b"|")[0].decode("ascii", "replace").upper().replace("_", "-")
        found = [b for b in importer._fixtures()["build"] if b.get("serial", "").upper() == serial
                 and b.get("role") == "psp-disc"]
        if len(found) != 1:
            raise SystemExit("PSP disc %s: fixtures.toml holds no PSP build of that serial" % serial)
        self.build, self.tag = found[0]["id"], found[0]["tag"]
        self.japanese = primary(self.tag) == "ja"
        self._boot = None

    def read(self, name):
        name = name.upper().replace("\\", "/")
        blob = self.disc.read(self.path_of.get(name, name))
        return psp_emi(blob) if name.endswith(".EMI") else blob

    def find(self, basename):
        b = basename.upper()
        return sorted(p for p in self.files if p.rsplit("/", 1)[-1] == b)

    def boot(self):
        """BOOT.BIN's first program header (.text, .rodata, .data at 0): the
        image the PSP's names and the exe's strings are read from."""
        if self._boot is None:
            elf = self.disc.read(PSP_BOOT)
            if elf[:4] != b"\x7fELF":
                raise SystemExit("%s is not a plain ELF" % PSP_BOOT)
            phoff, = struct.unpack_from("<I", elf, 28)
            _, off, va, _, fs, _, _, _ = struct.unpack_from("<8I", elf, phoff)
            if va != 0:
                raise SystemExit("%s: the first program header is at 0x%X, not 0" % (PSP_BOOT, va))
            self._boot = elf[off:off + fs]
            shoff, = struct.unpack_from("<I", elf, 32)
            size, count, names = struct.unpack_from("<HHH", elf, 46)
            heads = [struct.unpack_from("<10I", elf, shoff + i * size) for i in range(count)]
            strs = heads[names][4]
            text = [h for h in heads if elf[strs + h[0]:elf.index(b"\0", strs + h[0])] == b".text"]
            if len(text) != 1:
                raise SystemExit("%s: %d .text sections" % (PSP_BOOT, len(text)))
            self.text_end = text[0][3] + text[0][5]
        return self._boot

    def pointers(self):
        """What a pointer to data in BOOT.BIN is: an offset past .text, inside the image."""
        return (self.text_end, len(self.boot()))


def open_disc(path):
    """A disc image for the converters: a PlayStation disc as it is, a PSP disc
    through PspDisc."""
    disc = psx_disc.Disc(path)
    if "SYSTEM.CNF" not in disc.files and "UMD_DATA.BIN" in disc.files:
        return PspDisc(disc)
    return disc


def is_psp(disc):
    return getattr(disc, "psp", False)


# What a pointer word in a donor looks like: a PSX overlay's are KSEG0
# addresses below 2 MB; BOOT.BIN's are link-time offsets into its own image
# (relocations are applied at load, tools/psp_elf.py), so below its size.
PSX_POINTERS = (0x80000000, 0x80200000)
donor_pointers = PSX_POINTERS


def is_pointer(v):
    return donor_pointers[0] <= v < donor_pointers[1]


def world_blobs(disc):
    """(name, bytes) of every module a fishing area's text could be in: each
    WORLDnn/AREAnnn.EMI of a PlayStation disc; BOOT.BIN alone on a PSP disc,
    whose EMIs keep the Japanese build's overlays (region-diff.md 5) and whose
    own code and strings are the ELF's (its pointers then offsets from 0)."""
    if is_psp(disc):
        yield PSP_BOOT, disc.boot()
        return
    for name in sorted(disc.files):
        if "/WORLD" in name and name.endswith(".EMI"):
            yield name, disc.read(name)


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

    A cell counts when it has ink in the dialogue set, and the run stops at
    the first empty one. (Until 2026-10-12 it had to have ink in the 8 x 8 set
    too: the same run on all five PlayStation sheets, which end in a cell
    empty in both; PSP-EU's adds three dialogue-only cells, 0x94..0x96, which
    its ending text uses - their UI cells are blank, docs/loc-build-psp.md 3.4.)"""
    last = LAST_CODE
    for code in range(EXT_FIRST, EXT_MAX + 1):
        if not any(v for r in donor_cell(rows, code, CELLS_Y0, CELL_H) for v in r):
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


def blank_table():
    """The disc-only build's stand-in for the port's Chinese table (DiscCache):
    as many glyphs, every one blank. The Latin repaint and everything appended
    are the same as over the shipped table; what differs is only what the PC
    build keeps of the port's glyphs (docs/loc-build-disc-only.md section 4)."""
    return bytes(font_pc.GLYPH_BYTES * APPEND_AT)


def build_table(base_table, rows, redrawn=None, mono=False, disc_only=False):
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
        if i < len(EX_CELLS) or disc_only:
            x0, y0 = (EX_CELLS + SECOND_CELLS)[i]
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
    """The disc's boot EXE is SLPS_ / SCPS_ - a Japanese build; a PSP disc, its build's tag."""
    if is_psp(disc):
        return disc.japanese
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
            if b == 0x05 and i + 1 < len(buf) and is_button_arg(buf[i + 1]):
                raise SystemExit("the donor's text already holds 05 %02X, a colour in the button code's range "
                                 "(docs/button-dialogue.md 2): refusing to read it as a colour" % buf[i + 1])
            if b == 0x05 and button_map is not None:
                out += donor_button_code(buf, i + 2)      # before the colour it marks
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


def table_after_bad_head(donor):
    """A slot count for a block whose first slots point outside it: the
    smallest offset into the block of the slots before it, / 2, or 0.

    The French disc's AREA004 text section (0x80010000, 1,008 bytes) is the
    one such block on the five discs (docs/loc-build-disc-only.md 4.5): its
    first seven words are 0x312A and six 0x3F3D, past its end, and from slot 7
    on the table is an ascending one whose first offset, 0x200, puts the end
    of the table at 256 slots - the US and German counts. convert_block points
    the slots before at that first message: the German table's slots 0..7
    all point at its first message, the US's at it and at offsets into it."""
    low, i = len(donor) + 1, 0
    while 2 * (i + 1) <= min(low, len(donor)):
        w = struct.unpack_from("<H", donor, 2 * i)[0]
        if 2 * (i + 1) <= w <= len(donor) and not w & 1:
            low = min(low, w)
        i += 1
    return low // 2 if low <= len(donor) else 0


def convert_block(donor, base, room):
    """A donor text block -> a PC one. Slots whose donor message is not
    English keep the PC file's own message.

    `base` None is the disc-only build (DiscCache): no PC block to keep a
    message from. The slot count is then the donor's own - the same as the
    PC's in every block of the US disc (288 of 288, 2026-10-10,
    docs/loc-build-disc-only.md) - and a slot whose donor message does not
    convert is an empty message, where the PC build keeps the port's Chinese
    one (on the US disc: the sixteen Japanese template messages of 31 areas,
    496 slots)."""
    if base is None:
        n = struct.unpack_from("<H", donor, 0)[0] // 2 if len(donor) >= 2 else 0
        bad_head = not n or 2 * n > len(donor)
        if bad_head:
            n = table_after_bad_head(donor)
        if not n or 2 * n > len(donor):
            raise ValueError("disc-only: the donor block's slot table (%d slots) does not fit its %d bytes, "
                             "and there is no PC block to take the count from" % (n, len(donor)))
        d_offs = list(struct.unpack_from("<%dH" % n, donor, 0))
        if bad_head:
            # The head slots past the block's end point at its first message,
            # as the German table's first eight do (and the US's, by offsets
            # into that message): table_after_bad_head.
            for slot in range(n):
                if 2 * n <= d_offs[slot] <= len(donor):
                    break
                d_offs[slot] = 2 * n
        body, placed, table, kept, empty = bytearray(), {}, [], 0, set()
        for slot in range(n):
            off = d_offs[slot]
            if off not in placed:
                # A slot pointing outside its block (15 French and 11 German
                # areas, where the PC's slot is empty or unused) is empty too.
                msg = convert_run(donor, off)[0] if 2 * n <= off <= len(donor) else None
                if msg is None:
                    msg = b"\0"
                    empty.add(off)
                placed[off] = 2 * n + len(body)
                body += msg
            if off in empty:     # once per slot, as before; convert_run once per message (it counts button codes)
                kept += 1
            table.append(placed[off])
        block = struct.pack("<%dH" % n, *table) + bytes(body)
        if len(block) > room:
            raise ValueError("block is 0x%X bytes, room is 0x%X" % (len(block), room))
        return block, kept
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
                msg = pc_button_codes(base[b_offs[slot]:message_end(base, b_offs[slot])])
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


# ---------------------------------------------------------------- the dialogue's button codes

# The button unification round, DT-1 (docs/button-dialogue.md; the census is
# docs/key-icons-by-mapping.md 2.6 and docs/button-roles-by-release.md 5). A
# button named in an area's dialogue is a colour control and an icon glyph,
# `05 cc` then the icon, in every release. In the layer's output each such
# mention is preceded by a BUTTON CODE that names Japan's bit - the bit the
# PC's code tests - so the draw can show whatever button the Config has put
# that role on:
#
#     05 (0xB0 + n) 06   05 cc  <the release's own icon glyph>
#
# n is the bit's index: triangle 0x10 is 0xB4, circle 0x20 0xB5, cross 0x40
# 0xB6, square 0x80 0xB7. The code is a colour control with a colour no text
# uses (0x05's arguments in every area text block of the PC and of all seven
# held discs are 0x01..0x07; measured 2026-10-17, and convert_run refuses a
# donor holding one in the range), closed at once by 0x06. Both of today's
# readers of the area text draw exactly what they drew before:
#   - MsgBox_Step / MsgBox_StatePrint: 0x05 takes the colour, 0x06 takes
#     colour 0, neither is a character nor costs a frame, and the `05 cc`
#     right after sets the colour the icon is drawn in, as before;
#   - Text_DrawString (the fishing's guide list draws pool messages - the
#     fishing help lines among them - through Text_DrawAt): 0x05 remembers
#     the colour and takes the new one, 0x06 takes the remembered one back,
#     so the colour and the remembered colour after the code are what they
#     were before it; it costs two of the draw's `count` passes.
# A box-selector code (0x0C nn) was the first design and was dropped:
# Text_DrawString draws a 0x0C as the glyph 0x0C - 0x26, past the table, and
# traps. The draw (DT-2: src/game/button_text.h, both readers) shows the
# glyph of sigma(bit) in the icon's place when that is not the button the icon
# shows, and the icon itself when it is. The icon stays in the output: it is
# what the draw resolves the glyph family from (a Latin layer's, ja-JP's or the
# port's), what it draws byte for byte when the Config leaves the button where
# the text has it, and what BOF3X_ORIGINAL=ButtonText draws
# (docs/button-dialogue.md 4.4).
BUTTON_CTRL, BUTTON_ARG0, BUTTON_CLOSE = 0x05, 0xB0, 0x06
TRIANGLE, CIRCLE, CROSS, SQUARE = 0x10, 0x20, 0x40, 0x80
BUTTON_NAME = {TRIANGLE: "triangle", CIRCLE: "circle", CROSS: "cross", SQUARE: "square"}
# The areas whose dialogue names a button (key-icons 2.6): the wood chop 8,
# the fishing help 30 / 89 / 129, the well 68, the rope pull 75, the antenna
# 111, the ram 121, the crane 135. Not buttons: area 78's crosses, the faerie
# village's times sign, and the PC's area 1 - its circled 3 in colour 2 is the
# first character of a place name, where the JP disc's same slot has two kanji
# in that colour and the PC the same second character (DT-2, 2026-10-17;
# docs/button-dialogue.md 4.3). The draw's list is the same (text_draw.cpp
# kButtonAreas).
BUTTON_AREAS = (8, 30, 68, 75, 89, 111, 121, 129, 135)
# The release's icon -> the button it shows, in that release's terms.
LATIN_ICON = {0x80: CIRCLE, 0x81: CROSS, 0x82: TRIANGLE, 0x83: SQUARE}   # the PlayStation West's and PSP-EU's font
JA_ICON_LEAD = 0x15
JA_ICON = {0x08: CIRCLE, 0x09: CROSS, 0x0A: TRIANGLE, 0x0B: SQUARE}       # 0x15 nn on JP; 0x0A by order, no text uses it
# The PC's own text: one of the port's circled numerals (b .. g) or its X,
# whose shape is NOT the button (circled 3 is circle in area 1 and square in
# area 121); the colour before it is. The colours, measured off the PC's and
# the US disc's text, are per drawn shape on both: circle 2, cross 1,
# triangle 6, square 5.
PC_ICON_GLYPHS = frozenset(b"bcdefgX")
PC_COLOUR = {2: CIRCLE, 1: CROSS, 6: TRIANGLE, 5: SQUARE}
# The maps from Japan's button to a release's (button-roles-by-release.md 2).
MAP_P = {CIRCLE: CROSS, CROSS: TRIANGLE, TRIANGLE: SQUARE, SQUARE: CIRCLE}   # US, EU, FR, DE
MAP_S = {CIRCLE: CROSS, CROSS: CIRCLE, TRIANGLE: SQUARE, SQUARE: TRIANGLE}   # PSP-EU
MAP_ID = {b: b for b in BUTTON_NAME}                                           # JP, PSP-JP, the PC
# PSP-EU's two honest texts (the owner's answer 3, 2026-10-17): the well and
# the ram keep the PlayStation West's icons, so they translate by P.
PSP_EU_BY_P = (68, 121)
button_map = None        # set per area by build_all: the release's button -> Japan's, or None
button_area = None
button_counts = {}       # (area, "donor" | "pc", Japan's bit) -> count, for the build's report


def is_button_arg(a):
    return BUTTON_ARG0 <= a <= BUTTON_ARG0 + 7


def button_code(bit):
    return bytes([BUTTON_CTRL, BUTTON_ARG0 + bit.bit_length() - 1, BUTTON_CLOSE])


def code_bit(arg):
    """The bit a button code's argument names, or None."""
    return 1 << (arg - BUTTON_ARG0) if is_button_arg(arg) else None


def release_map(disc):
    """The release's map from Japan's buttons, by what the disc is: JP's and
    PSP-JP's are the identity, PSP-EU's is S, every other (Latin) PlayStation
    disc's is P. Not by tag: PSX-EU and PSP-EU are both en-150."""
    if donor_ja:
        return MAP_ID, "identity (JP)"
    if is_psp(disc):
        return MAP_S, "S (PSP-EU; areas %s by P)" % ", ".join(map(str, PSP_EU_BY_P))
    return MAP_P, "P (the PlayStation West)"


def area_number(name):
    m = re.match(r"AREA(\d{3})\.DAT$", name.upper())
    return int(m.group(1)) if m else None


def set_button_area(name, rmap, psp_eu):
    """The module state convert_run reads for one area's text block; None outside the button areas."""
    global button_map, button_area
    n = area_number(name)
    button_area = n
    if n not in BUTTON_AREAS or rmap is None:
        button_map = None
        return
    m = MAP_P if psp_eu and n in PSP_EU_BY_P else rmap
    button_map = {v: k for k, v in m.items()}        # the release's button -> Japan's


def count_button(source, bit):
    key = (button_area, source, bit)
    button_counts[key] = button_counts.get(key, 0) + 1


def donor_button_code(buf, i):
    """For a donor's `05 cc` whose icon is at i: the button code to write
    before the `05 cc`, or b"" when no icon follows. The colour and the icon
    are left for the caller to encode, as before - the icon is the fallback
    the draw shows until DT-2."""
    if donor_ja:
        if i + 1 < len(buf) and buf[i] == JA_ICON_LEAD and buf[i + 1] in JA_ICON:
            bit = button_map[JA_ICON[buf[i + 1]]]
        else:
            return b""
    elif i < len(buf) and buf[i] in LATIN_ICON:
        bit = button_map[LATIN_ICON[buf[i]]]
    else:
        return b""
    count_button("donor", bit)
    return button_code(bit)


def pc_button_codes(msg):
    """A PC message a layer keeps: the button code inserted before each
    `05 cc` that colours one of the port's icon glyphs, by the colour (the
    PC is Japan's buttons: no map). Outside the button areas, unchanged."""
    if button_area not in BUTTON_AREAS or button_map is None:
        return msg
    out, i = bytearray(), 0
    while i < len(msg):
        b = msg[i]
        if b == CHOICE:
            out += msg[i:i + 4]
            i += 4
            continue
        if b in ARG1:
            if b == BUTTON_CTRL and i + 1 < len(msg) and is_button_arg(msg[i + 1]):
                raise SystemExit("the PC's message already holds 05 %02X, a colour in the button code's range" % msg[i + 1])
            if b == 0x05 and i + 2 < len(msg) and msg[i + 2] in PC_ICON_GLYPHS and msg[i + 1] in PC_COLOUR:
                bit = PC_COLOUR[msg[i + 1]]
                count_button("pc", bit)
                out += button_code(bit)
            out += msg[i:i + 2]
            i += 2
            continue
        n = 2 if b & 0x80 else 1
        out += msg[i:i + n]
        i += n
    return bytes(out)


def pc_text_tokens(block, off):
    """[(offset, bytes)] of one PC-encoded message from `off` to its NUL, by
    the PC stepper's rules (a first byte with bit 7 set takes a second; the
    two-byte controls; a choice's four bytes and its options' NULs)."""
    out, end = [], min(message_end(block, off), len(block))   # a block may end without its NUL
    i = off
    while i < end:
        b = block[i]
        n = 4 if b == CHOICE else 2 if (b & 0x80 or b in ARG1) else 1
        out.append((i, block[i:i + n]))
        i += n
    return out


def text_buttons(block, pc_text=False):
    """A PC-encoded area text block's button mentions: [(slot offset, Japan's
    bit or None, fallback glyph bytes, colour)] - a code `05 Bn 06` with the
    `05 cc` and the glyph after it, or None for an icon-shaped `05 cc` + glyph
    with no code before it: one of a layer's Latin icon glyphs 0x9E3..0x9E6,
    or with `pc_text` (the PC's own Chinese, where a single-byte letter is
    not English) the PC's colour cue. A code not followed by `05 cc` and a
    glyph is reported with the glyph b"" (DT-2 would have nothing to replace)."""
    n = struct.unpack_from("<H", block, 0)[0] // 2
    offs = sorted(set(struct.unpack_from("<%dH" % n, block, 0)))
    latin = {bytes([0x80 | (g >> 8), g & 0xFF]) for g in range(APPEND_AT + 0x80 - FIRST_CODE, APPEND_AT + 0x84 - FIRST_CODE)}
    found = []
    for off in offs:
        if not 2 * n <= off < len(block):
            continue
        toks = pc_text_tokens(block, off)
        tok = lambda k: toks[k][1] if k < len(toks) else b"\0"
        k = 0
        while k < len(toks):
            t = tok(k)
            if t[0] == BUTTON_CTRL and len(t) == 2 and is_button_arg(t[1]):
                ok = (tok(k + 1) == bytes([BUTTON_CLOSE]) and tok(k + 2)[0] == 0x05 and len(tok(k + 2)) == 2
                      and tok(k + 3)[0] > 0x16)
                found.append((off, code_bit(t[1]), tok(k + 3) if ok else b"", tok(k + 2)[1] if ok else 0))
                k += 4 if ok else 1
                continue
            if t[0] == 0x05 and len(t) == 2 and k + 1 < len(toks):
                nxt = tok(k + 1)
                if (pc_text and len(nxt) == 1 and nxt[0] in PC_ICON_GLYPHS and t[1] in PC_COLOUR) or nxt in latin:
                    found.append((off, None, nxt, t[1]))
            k += 1
    return found


def cmd_buttons(args):
    """The census of a layer's (or the PC's own) area text: every button code
    and every icon left without one, per area, counted - no text printed."""
    d = args.dat
    if not os.path.isdir(d):
        raise SystemExit("%s: not a directory" % d)
    prefix = (args.lang + ".") if args.lang else ""
    total, bare, rows = {}, 0, []
    for f in sorted(os.listdir(d)):
        if not f.upper().startswith(prefix.upper()):
            continue
        rest = f[len(prefix):]
        if not prefix and "." in os.path.splitext(rest)[0]:
            continue                                  # an overlay beside the originals
        n = area_number(rest)
        if n is None:
            continue
        blob, chunks = dat.load(os.path.join(d, f))
        for c in chunks:
            if c.kind != 0 or c.tag != 0:
                continue
            got = text_buttons(blob[c.offset:c.offset + c.size], args.pc_text)
            if not got:
                continue
            per = {}
            for _, bit, glyph, colour in got:
                key = ("code " + BUTTON_NAME[bit]) if bit else "NO CODE"
                key += " (glyph %s, colour %d)" % (glyph.hex() or "none", colour)
                per[key] = per.get(key, 0) + 1
                if bit:
                    total[bit] = total.get(bit, 0) + 1
                else:
                    bare += 1
            outside = "" if n in BUTTON_AREAS else "   <- not a button area"
            rows.append("area %3d: %s%s" % (n, "; ".join("%s x%d" % kv for kv in sorted(per.items())), outside))
    print("\n".join(rows))
    print("%s: %d button codes (%s), %d icons without a code, in %d areas"
          % (d if not prefix else d + " " + args.lang, sum(total.values()),
             ", ".join("%s %d" % (BUTTON_NAME[b], total[b]) for b in sorted(total)), bare, len(rows)))
    return 0


def button_report(counts):
    """One line per area: the codes written, by Japan's button, and where from."""
    rows = {}
    for (area, source, bit), n in sorted(counts.items(), key=lambda kv: (kv[0][0], kv[0][1], kv[0][2])):
        rows.setdefault(area, []).append("%s %s x%d" % (source, BUTTON_NAME[bit], n))
    return ["  area %d: %s" % (a, ", ".join(r)) for a, r in sorted(rows.items())]


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


# ---------------------------------------------------------------- what the overlays are built against

# Two answers to "the PC side" (docs/loc-build-disc-only.md). `--game` is the
# PC install: its DAT/ containers and BOF3.exe, as every build before
# 2026-10-10. `--cache` is an importer cache built from a PlayStation disc
# alone: its base/dat/ (the language-neutral containers) and base/exe/data.bin
# (BOF3.exe's .data in the PC's layout, from the disc, tools/exe_tables.py),
# with recipes/pc-zh.toml naming the containers and the chunks the PC's
# loc/zh-CN layer holds - which a disc-only cache has no bytes of.

class PcInstall:
    """The PC install: DAT/ and BOF3.exe. Overlays go beside the originals as <tag>.<NAME>.DAT."""
    disc_only = False

    def __init__(self, game):
        self.game, self.dat = game, os.path.join(game, "DAT")   # dat_dir's check is the build's (build_all)
        self._exe = None

    def __str__(self):
        return self.dat

    def exe(self, va, size):
        if self._exe is None:
            with open(os.path.join(self.game, "BOF3.exe"), "rb") as f:
                self._exe = f.read()
        exe = self._exe
        pe = struct.unpack_from("<I", exe, 0x3C)[0]
        nsec, optsz = struct.unpack_from("<H", exe, pe + 6)[0], struct.unpack_from("<H", exe, pe + 20)[0]
        image_base = struct.unpack_from("<I", exe, pe + 24 + 28)[0]
        for i in range(nsec):
            vsz, rva, rsz, raw = struct.unpack_from("<IIII", exe, pe + 24 + optsz + i * 40 + 8)
            if image_base + rva <= va < image_base + rva + rsz:
                return exe[raw + va - image_base - rva:][:size]
        raise SystemExit("0x%X is not in BOF3.exe" % va)

    def carried(self, va, size):
        return True

    def names(self):
        """The shipped containers: every NAME.DAT, no overlay (a dot in the stem)."""
        return [n for n in sorted(os.listdir(self.dat))
                if os.path.splitext(n)[1].upper() == ".DAT" and "." not in os.path.splitext(n)[0]]

    def chunks(self, name):
        """[(kind, tag, bytes or None, size)] of a container in order, or None if there is none."""
        path = os.path.join(self.dat, name)
        if not os.path.exists(path):
            return None
        blob, chunks = dat.load(path)
        return [(c.kind, c.tag, blob[c.offset:c.offset + c.size], c.size) for c in chunks]

    def out_path(self, tag, name):
        return os.path.join(self.dat, "%s.%s" % (tag, name))

    def english_title_pages(self):
        """[(label, path)] of the English START.DAT overlays already built beside the originals."""
        language_tags.note_retired_overlays(self.dat)
        return [(f, os.path.join(self.dat, f)) for f in sorted(os.listdir(self.dat)) if f.endswith(".START.DAT")
                and f[:-len(".START.DAT")] in language_tags.TAGS and primary(f[:-len(".START.DAT")]) == "en"]


class DiscCache:
    """An importer cache from a PlayStation disc: base/dat/, base/exe/, and the recipe.

    A chunk of the PC's loc/zh-CN layer (the area text, the pools, the font,
    the text CLUT, the title page, ...) has no bytes here: `chunks` gives it as
    (kind, tag, None, size), the size the recipe records. A read of the exe
    image is the disc's: `carried` says whether every byte of a range was
    filled from the disc (`map`, `table`, `widen` - whose 16-byte name fields
    are blank -, or `exe`); a `pointer` or `none` byte is zero. Overlays go to
    <cache>/loc/<tag>/dat/<NAME>.DAT, the layout importer.py writes."""
    disc_only = True
    CARRIED = ("exe", "map", "table", "widen")

    def __init__(self, cache):
        import tomllib
        self.cache = cache
        self.dat = os.path.join(cache, "base", "dat")
        meta_path = os.path.join(cache, "base", "exe", "data.toml")
        if not os.path.isdir(self.dat) or not os.path.exists(meta_path):
            raise SystemExit("%s: not an importer cache with base/dat/ and base/exe/ (importer.py build writes both)" % cache)
        with open(meta_path, "rb") as f:
            meta = tomllib.load(f)
        with open(os.path.join(cache, "base", "exe", meta["image"]["file"]), "rb") as f:
            self.image = f.read()
        if hashlib.sha256(self.image).hexdigest() != meta["image"]["sha256"]:
            raise SystemExit("%s: base/exe/data.bin does not hash as data.toml says" % cache)
        self.va, self.build = meta["image"]["va"], meta["image"]["build"]
        self.ranges = sorted((lo, hi, how) for lo, hi, how, _ in meta["image"]["ranges"])
        with open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "recipes", "pc-zh.toml"), "rb") as f:
            self.recipe = {r["name"]: r["chunks"] for r in tomllib.load(f)["file"]}

    def __str__(self):
        return self.cache

    def exe(self, va, size):
        if not (self.va <= va and va + size <= self.va + len(self.image)):
            raise SystemExit("0x%X: not in the cache's .data image (base/exe/data.bin)" % va)
        return self.image[va - self.va:va - self.va + size]

    def carried(self, va, size):
        at = va
        for lo, hi, how in self.ranges:
            if hi <= at or lo > at:
                continue
            if how not in self.CARRIED:
                return False
            at = hi
            if at >= va + size:
                return True
        return at >= va + size

    def names(self):
        return sorted(self.recipe)

    def chunks(self, name):
        if name not in self.recipe:
            return None
        path = os.path.join(self.dat, name)
        have = {}
        if os.path.exists(path):
            blob, chunks = dat.load(path)
            have = {(c.kind, c.tag): blob[c.offset:c.offset + c.size] for c in chunks}
        return [(c["kind"], c["tag"], have.get((c["kind"], c["tag"])) if c["layer"] == "base" else None, c["size"])
                for c in self.recipe[name]]

    def out_path(self, tag, name):
        d = os.path.join(self.cache, "loc", tag, "dat")
        os.makedirs(d, exist_ok=True)
        return os.path.join(d, name)

    def english_title_pages(self):
        d = os.path.join(self.cache, "loc")
        tags = sorted(t for t in os.listdir(d) if t in language_tags.TAGS and primary(t) == "en") if os.path.isdir(d) else []
        return [("loc/%s/dat/START.DAT" % t, os.path.join(d, t, "dat", "START.DAT")) for t in tags
                if os.path.exists(os.path.join(d, t, "dat", "START.DAT"))]


def chunk_of(game, name, kind, tag):
    """(bytes or None, size) of one chunk of a container, or None if it has no such chunk."""
    for k, t, b, size in game.chunks(name) or ():
        if (k, t) == (kind, tag):
            return b, size
    return None


# The anchors (docs/loc-build-disc-only.md section 3). Every converter of the
# exe's tables finds its donor's copy on the disc by bytes the PC's .data and
# the disc share - a table's numbers, the bytes around a run of strings. Over
# the PC install they are read out of BOF3.exe. A disc-only cache has them in
# base/exe/data.bin only where the disc's image carries them (DiscCache.carried):
# the US image lacks five of them (measured 2026-10-10), each other build its
# own few. So each is also held as the SHA-256 of the PC's bytes - never the
# bytes (CLAUDE.md rule 1; the owner's stance on Capcom's tables in code,
# 2026-10-10) - and where the image lacks one the disc is searched for the
# window that hashes to it. A build over the PC install checks every hash
# (check_anchors); `loc_build.py anchors --game DIR` prints them anew.

def anchor_pieces(name):
    """The PC ranges [(va, size)] whose bytes, end to end, are the anchor `name`."""
    pieces = {
        "verb sets": [(VERB_SETS, VERB_SHARED_SETS * 5)],
        "char tail 0": [(CHAR_RECORDS + CHAR_NAME_PC, CHAR_STRIDE - CHAR_NAME_PC)],
        "char tails": [(CHAR_RECORDS + k * CHAR_STRIDE + CHAR_NAME_PC, CHAR_STRIDE - CHAR_NAME_PC)
                       for k in range(CHAR_COUNT)],
        "merchant": [(MERCHANT_ANCHOR, 12)],
        "battle boxes": [(BATTLE_BOXES, BATTLE_COUNT * 8)],
        "label head": [(LABEL_HEAD, LABEL_HEAD_LEN)],
        "label tail": [(LABEL_TAIL, LABEL_TAIL_LEN)],
        "item types head": [(LABEL_TYPES_HEAD, 16)],
        "battle tail": [(LABEL_BATTLE_TAIL, 16)],
        "master head": [(LABEL_MASTER_HEAD, LABEL_MASTER_HEAD_LEN)],
        "master lists": [(LABEL_MASTER_LISTS, LABEL_MASTER_LISTS_LEN)],
        "sort head": [(LABEL_SORT_HEAD, LABEL_SORT_HEAD_LEN)],
        "formation pair 0": [(LABEL_FORMATIONS + 16, 12)],
        "formation pairs": [(LABEL_FORMATIONS + 28 * i + 16, 12) for i in range(LABEL_FORMATION_COUNT)],
        "wheel triangle": [(LABEL_WHEEL_TRIANGLE, LABEL_WHEEL_TRIANGLE_LEN)],
        "gene head": [(LABEL_GENE_HEAD, LABEL_GENE_HEAD_LEN)],
        "gene tail": [(LABEL_GENE_TAIL, LABEL_GENE_TAIL_LEN)],
        "gene list head": [(LABEL_GENE_LIST_HEAD, 16)],
        "village rows": [(LABEL_VILLAGE_ROWS, LABEL_VILLAGE_ROWS_LEN)],
        "village rects": [(LABEL_VILLAGE_RECTS, LABEL_VILLAGE_RECTS_LEN)],
        "village kinds": [(LABEL_VILLAGE_KINDS, LABEL_VILLAGE_KINDS_LEN)],
        "village draws": [(LABEL_VILLAGE_DRAWS, LABEL_VILLAGE_DRAWS_LEN)],
        "faerie stat 0": [(FAERIE_TRAITS, 4)],
        "faerie stats": [(FAERIE_TRAITS + FAERIE_STRIDE * r, 4) for r in range(FAERIE_COUNT)],
        "fish rows": [(FISH_ROWS, FISH_ROWS_LEN)],
        "fish quads": [(FISH_QUADS, FISH_QUADS_LEN)],
        "fish label/pause": [(FISH_LINES + 8 * i + 4, 2) for i in range(FISH_LINE_COUNT)],
    }
    for what, va, stride, count, name_at in NAME_TABLES:
        # A record's numbers: the bytes before its name and after it.
        nums = lambda i: [(va + i * stride + o, n) for o, n in
                          ((0, name_at), (name_at + NAME_LEN, stride - name_at - NAME_LEN)) if n]
        pieces["names %s 1" % what] = nums(1)
        pieces["names %s" % what] = [r for i in range(count) for r in nums(i)]
    return pieces if name is None else pieces[name]


PC_SHA = {   # measured 2026-10-10 from the catalogued BOF3.exe (pc-zh); `loc_build.py anchors`
    "verb sets": "72d024369e7e143add3077c29177169b4749452a4d424f857155ec4534e5b3dd",
    "char tail 0": "3d87375533e043f9434d744b922e9a57b4f96c6732df8d2ead55a574a76dde31",
    "char tails": "fbd633004285ecca106244557620f4d706104d34c80cb7d0109bf5658befaac8",
    "merchant": "26d1f260ec3e0fa54fbc894d1fccf7111db0129cfa169165fef87e6597572f60",
    "battle boxes": "52fc175de9440d1ea1a495528710f2869d23154267a87e89ccd8e0b8754ab104",
    "label head": "cfff8c35c1cb007ec4105f58fbc99e146cccf729715050b25f8c64ecff6590ed",
    "label tail": "1c26f21c8646343815ec3b3541c445a31220b10ee68d2b4f6b8245fa53a5d1da",
    "item types head": "72ac7c1f9bba1118d9b55836b835730a5af01dee452b4b4e3bc3b28375692e39",
    "battle tail": "8787ddb8581a74257063551bf9817d376dd90d9473ed54ed76f87d8093031139",
    "master head": "e402f65d5ec079e5f6243d5ece75757f76dd438ee28353683a64e147c371ac71",
    "master lists": "54beb205640729fdfc3ced948578d31a73fa12a56c252c8a3ee91afe32c89b3b",
    "sort head": "40f0388a0e776618e2ab16570edaa576ca06d8d57e2d4af0e5ea6c3eea4fddc9",
    "formation pair 0": "15ec7bf0b50732b49f8228e07d24365338f9e3ab994b00af08e5a3bffe55fd8b",
    "formation pairs": "f8314c458ab4cfd90fcf9a9caaf3f12e7838bc487d8e0a2b5eb3e1d8dead7289",
    "wheel triangle": "9edcafb2b4be187893d796687492a3c6b2e2e9f24313e88282bbab2b034ea462",
    "gene head": "d88f9d085672f99d2e4a3f0c857923b955f12558e54082090d0310f31eabbccb",
    "gene tail": "f7e59947f1ac782643c0f49b41afdc2bf5507f670c31959b0df5f63a22cd0902",
    "gene list head": "2304b97170680e3e156bad1c5ce3952eeea46704fd3c824fac99fb710ccc3a22",   # 2026-10-18
    "village rows": "2f7dbbfd3c47aca0262b45b79b984c1f4c09e8b8a08d3bb30b72b07d9d2001ea",
    "village rects": "2a876d9960a822d13d0ff20bc03f51ab82a5c9e99dafdfa27176be74c8cba4f5",
    "village kinds": "86edc365dbd7e4f155187a6a953e3d4a3c53209e5caf65dd9699b5cf2ae14bd6",
    "village draws": "dacebb658cae366ff12389c5e5ef4a01e54d13371f98436e4bc902eb269ee28a",   # 2026-10-13
    "faerie stat 0": "f4a1f368908311763fa2bb8141c0615019783aa727e077441117c83d0c3c6816",
    "faerie stats": "253445f72876405c1d232e8634ead180729f7c18ab4cdeacdde0b75532f0b3f6",
    "fish rows": "da3afd1fdbe1a1867fa5a0f146518d5975ac37d3bee2ccff3ae10f32d5b54a9b",
    "fish quads": "f3a51dbade9224428cd300dba79198a6ccfa34071a87a32f9a1e49e1f5f243e7",
    "fish label/pause": "1f30c4f31acbf1c081ae6abf092a99b230fec618b1977c86cd6dfd495fe0a631",
    "names consumables 1": "d840ea7a397ea0f4467d0dee3fe3a27515542be311f33897d047545fe76fd935",
    "names consumables": "f11bfab8b494d5f44eb63d3b8da267eacb01a09b943b6031de9b444d9d768d12",
    "names key items 1": "29eeb6ae650e1e06294bc5e31567365f8cbe5b25740d3cb741087ef06bb26466",
    "names key items": "a6c1d879dc56fdfa619b9aa1500d97c4824288211cdc0f8eec8237e89480e103",
    "names weapons 1": "95bb725c3dbcc5932084822d92e5a3a4c788f5c6aaab25234d5aed1f55aeb099",
    "names weapons": "f99aeb401bf431705f0efa686510d04075d3a12a2761d9c2431545b7a08c60aa",
    "names armour 1": "9bbd8e2da3903e90a46ee981c38a6a92308e3b1595663f1d85aab44d29dbd49b",
    "names armour": "cf9f143239a5fc88734371491d3a7db3cde424d013980a4d4c9682cd37893d36",
    "names accessories 1": "0c7fe77cef07ccb76b9685cd689b0fcdfede919af3428b2f4e0a6d2efe0406cc",
    "names accessories": "13b23beb564c835e69b6553f562d26d26e1effdcde3927c8a081587929d5d370",
    "names abilities 1": "6b1fdb7eb1c2b10b2782cd384058fe52407161babd406bc0afec9573c2924791",
    "names abilities": "ef65c5f3e9060baa836fa9d69a58831e588e479e00efba2daf796d72e80aaf7e",
}
# The trait records whose PC name field (FAERIE_NAMES + 20 r) holds no glyph
# string, kept empty (convert_labels group 16): none - every one of the sixty
# is named (D289: the name field is the record's first 16 bytes, the stats
# after it; the sixtieth "unnamed" record of before was record 59's stats
# read as a name). Checked like PC_SHA.
FAERIE_UNNAMED = ()


def sha_hex(b):
    return hashlib.sha256(b).hexdigest()


def anchor(game, name):
    """The anchor's bytes, or None where a disc-only image does not carry every piece of them."""
    pieces = anchor_pieces(name)
    if not all(game.carried(va, n) for va, n in pieces):
        return None
    return b"".join(game.exe(va, n) for va, n in pieces)


def find_anchor(game, blob, name, start=0, end=None):
    """blob.find(the anchor, start, end): by its bytes, or else by PC_SHA."""
    a = anchor(game, name)
    if a is not None:
        return blob.find(a, start) if end is None else blob.find(a, start, end)
    size = sum(n for _, n in anchor_pieces(name))
    return find_hashed(blob, size, PC_SHA[name], start, None if end is None else end - size)


def same_anchor(game, name, got):
    """Whether `got` is the anchor: its bytes, or else its hash."""
    a = anchor(game, name)
    return got == a if a is not None else sha_hex(got) == PC_SHA[name]


def anchor_table(game):
    """{name: sha256} of every anchor, from the PC install."""
    return {name: sha_hex(b"".join(game.exe(va, n) for va, n in pieces))
            for name, pieces in anchor_pieces(None).items()}


def check_anchors(game):
    """Against the PC install: PC_SHA and FAERIE_UNNAMED are what its BOF3.exe holds."""
    bad = sorted(n for n, h in anchor_table(game).items() if PC_SHA.get(n) != h)
    names = game.exe(FAERIE_NAMES, FAERIE_STRIDE * FAERIE_COUNT)
    if tuple(r for r in range(FAERIE_COUNT) if names[FAERIE_STRIDE * r] < 0x80) != FAERIE_UNNAMED:
        bad.append("FAERIE_UNNAMED")
    if bad:
        raise SystemExit("loc_build: the anchors %s are not this BOF3.exe's (PC_SHA; `loc_build.py anchors`)"
                         % ", ".join(bad))


def find_hashed(blob, size, digest, start=0, end=None):
    """The first offset >= start (and <= end) whose `size` bytes hash to `digest`, or -1."""
    end = len(blob) - size if end is None else min(end, len(blob) - size)
    sha = hashlib.sha256
    for at in range(start, end + 1):
        if sha(blob[at:at + size]).hexdigest() == digest:
            return at
    return -1


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
# The records whose numbers BOOT.BIN changed (DIV-0104: consumable 87's one
# byte). Over an image not built from a PSP disc - a PSX disc or the PC's exe
# first - the table is confirmed with these records' numbers taken as the
# image's, every other record's still the donor's.
PSP_NUMBERS_DIFFER = {"consumables": (87,)}


def convert_pool(donor, base):
    d0, d1 = struct.unpack_from("<II", donor, 0)
    b0, b1 = struct.unpack_from("<II", base, 0) if base is not None else (8, None)   # None: disc-only
    if d0 != 8 or b0 != 8:
        raise ValueError("pool header is not (8, n)")
    first, kept0 = convert_block(donor[d0:d1], base[b0:b1] if base is not None else None, POOL_ROOM)
    second, kept1 = convert_block(donor[d1:], base[b1:] if base is not None else None, POOL_ROOM)
    pool = struct.pack("<II", 8, 8 + len(first)) + first + second
    if len(pool) > POOL_ROOM:
        raise ValueError("pool is 0x%X bytes, room is 0x%X" % (len(pool), POOL_ROOM))
    return pool, kept0 + kept1


def exe_bytes(game, va, size):
    """The PC's .data bytes at va: BOF3.exe's (PcInstall) or the cache's image of it (DiscCache)."""
    if isinstance(game, str):
        game = PcInstall(game)
    return game.exe(va, size)


def convert_names(game, donor, psp=False):
    """[(tag, payload)] for the six tables, names from `donor` (GAME.EMI section 0,
    or a PSP disc's BOOT.BIN with `psp`)."""
    chunks, report = [], []
    for what, va, stride, count, name_at in NAME_TABLES:
        pc = exe_bytes(game, va, stride * count)
        donor_len = JA_NAME_LEN if donor_ja else DONOR_NAME_LEN
        d_stride = stride - NAME_LEN + donor_len
        # The numeric bytes of a record, name cut out - the same on both sides.
        def numbers(buf, i, n_len, st, origin=0):
            rec = buf[origin + i * st:origin + (i + 1) * st]
            return rec[:name_at] + rec[name_at + n_len:]
        # Record 1's numbers locate the table, every record's confirm it (the anchors).
        differ = [i for i in PSP_NUMBERS_DIFFER.get(what, ()) if psp and game.carried(va + i * stride, stride)]
        at, found = find_anchor(game, donor, "names %s 1" % what), None
        while at >= 0 and found is None:
            start = at - d_stride - (0 if name_at else donor_len)
            got = [numbers(donor, i, donor_len, d_stride, start) for i in range(count)]
            for i in differ:
                got[i] = numbers(pc, i, NAME_LEN, stride)
            if start >= 0 and same_anchor(game, "names %s" % what, b"".join(got)):
                found = start
            at = find_anchor(game, donor, "names %s 1" % what, at + 1)
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


def config_payload(labels, records, ctrls):
    """The kind-7 payload from six labels, seventeen (count, x, string) records
    and six controller names, each string in donor script codes."""
    payload = bytearray([CONFIG_LABELS])
    for i, raw in enumerate(labels):
        # Repointed, not written in place, so the only limit is ours.
        payload += config_encode(raw, "label %d" % i, 32) + b"\0"
    payload.append(CONFIG_RECORDS)
    for i, (count, x, raw) in enumerate(records):
        # count and x are the donor's own, verbatim: they are what puts the
        # options where the disc puts them. The PC record holds 14 bytes of
        # string after them.
        payload += bytes([count, x]) + config_encode(raw, "option %d" % i, 14) + b"\0"
    payload.append(CONFIG_CTRL)
    for i, raw in enumerate(ctrls):
        payload += config_encode(raw, "controller %d" % i, 64) + b"\0"
    return bytes(payload)


# The PSP's own Config screen, in BOOT.BIN (docs/loc-build-psp.md 3.3, read
# 2026-10-17 off PSP-EU): six rows, Msg Speed / Window Clr / Background /
# Autorun / Button Config. / Delete Data - the PlayStation's Sound row gone,
# two rows added that have no options - over fifteen 8-byte option records
# (the PlayStation's seventeen without Stereo and Mono) and the same six
# controller records. Its row tables are the PlayStation's with the Sound
# row taken out, and they locate the block as CONFIG_TABLES does there: the
# records end at them, the controller records start after them.
PSP_CONFIG_TABLES = bytes([0, 3, 7, 11, 13, 15, 3, 4, 4, 2, 0, 0])
PSP_CONFIG_RECORDS = 15
# The PC's record -> the PSP's (None: the Sound row's two, which the PSP has
# not). 15 and 16 are unused on both screens (docs/config-screen.md 1).
PSP_CONFIG_RECORD_OF = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, None, None, 11, 12, 13, 14)
# The PC's label -> the PSP's row: Msg Speed, Window Clr, Background and
# Autorun are the same rows; Sound has none, and Controller none by its word
# (the PSP's row for the same panel says "Button Config.", fourteen letters
# where the PC's column was set for ten, DIV-0015).
PSP_CONFIG_LABEL_OF = (0, 1, 2, None, 3, None)
# Ours (the owner's decision, 2026-10-17, D246): the words of the slots the
# PSP's screen has no word for, in the disc's script codes (ASCII letters).
# One-word names of the screen's functions, nothing else of the game.
CONFIG_BUILTIN_LABELS = {3: b"Sound", 5: b"Controller"}
CONFIG_BUILTIN_OPTIONS = {11: b"Stereo", 12: b"Mono"}


def psp_config_code_labels(boot, text_end, first_va, last_va):
    """The two row labels the PSP's label routine builds on its stack - one
    `ori` / `sb` pair a byte - rather than copying from .data, in stack order.

    The routine is the one whose calls pass the first and the last .data label
    as their source ($a1); its stack bytes are read from its prologue (`addiu
    $sp, $sp, -N`) to the last of those calls. [] if it is not found as that."""
    calls = mips_calls([(0, boot[:text_end])])
    first = [a for a, _, regs in calls if regs.get(5) == first_va]
    last = [a for a, _, regs in calls if regs.get(5) == last_va]
    if len(first) != 1 or len(last) != 1 or not first[0] < last[0] < first[0] + 0x200:
        return []
    start = first[0]
    while start > first[0] - 0x400:
        w, = struct.unpack_from("<I", boot, start)
        if w >> 16 == 0x27BD and w & 0x8000:            # addiu $sp, $sp, -N
            break
        start -= 4
    else:
        return []
    regs, stack = {0: 0}, {}
    for at in range(start, last[0] + 8, 4):
        w, = struct.unpack_from("<I", boot, at)
        op, rs, rt, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
        if op == 0x0D and rs == 0:                      # ori rt, $zero, imm
            regs[rt] = imm
        elif op == 0x28 and rs == 29:                   # sb rt, off($sp)
            if rt not in regs or regs[rt] > 0xFF:
                return []
            stack[imm] = regs[rt]
        elif op in (0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0E, 0x0F) or 0x20 <= op <= 0x26:
            regs.pop(rt, None)
        elif op == 0:
            regs.pop((w >> 11) & 31, None)
        regs[0] = 0
    runs, cur = [], bytearray()
    for off in sorted(stack):
        if cur and off != prev + 1:
            runs.append(bytes(cur))
            cur = bytearray()
        cur.append(stack[off])
        prev = off
    if cur:
        runs.append(bytes(cur))
    if any(not r.endswith(b"\0") or b"\0" in r[:-1] or len(r) < 2 for r in runs):
        return []
    return [r[:-1] for r in runs]


def convert_config_psp(boot, text_end):
    """([(kind, tag, payload)], {slot: source}) for the PC's Config screen from
    a PSP BOOT.BIN, or ([], {}) if its block is not found.

    The PSP's own word in every slot its screen has one for; the rest from
    CONFIG_BUILTIN_*, the Sound row's two records at the count and x of the
    Autorun row's, which are the same pair on every disc held (06 26, 04 66)."""
    at = boot.find(PSP_CONFIG_TABLES)
    if at < 0 or boot.find(PSP_CONFIG_TABLES, at + 1) >= 0:
        return [], {}
    records = at - PSP_CONFIG_RECORDS * 8
    recs = []
    for i in range(PSP_CONFIG_RECORDS):
        rec = boot[records + i * 8:records + i * 8 + 8]
        recs.append((rec[0], rec[1], rec[2:].split(b"\0")[0]))
    # The four .data labels end at the records, NUL-terminated.
    head = boot[records - 64:records]
    if not head.endswith(b"\0"):
        return [], {}
    tokens, pos = [], records - 64
    for t in head.split(b"\0"):
        trimmed = config_trim(t)
        if trimmed:
            tokens.append((pos + len(t) - len(trimmed), trimmed))
        pos += len(t) + 1
    if len(tokens) < 4:
        return [], {}
    data_labels = tokens[-4:]
    code_labels = psp_config_code_labels(boot, text_end, data_labels[0][0], data_labels[3][0])
    if len(code_labels) != 2:
        raise SystemExit("config: BOOT.BIN's label routine builds %d labels on its stack, wanted 2" % len(code_labels))
    # The PSP's rows: a stack label, two copied, a stack label, two copied.
    psp_labels = [code_labels[0], data_labels[0][1], data_labels[1][1], code_labels[1],
                  data_labels[2][1], data_labels[3][1]]
    ctrl = at + len(PSP_CONFIG_TABLES)
    ctrls = []
    for i in range(CONFIG_CTRL):
        rec = boot[ctrl + i * CTRL_STRIDE:ctrl + (i + 1) * CTRL_STRIDE]
        raw = rec[1:].split(b"\0")[0]
        if rec[0] != len(raw):
            return [], {}
        ctrls.append(raw)

    source = {}
    labels = []
    for i, p in enumerate(PSP_CONFIG_LABEL_OF):
        if p is None:
            labels.append(CONFIG_BUILTIN_LABELS[i])
            source["label %d" % i] = "ours"
        else:
            labels.append(psp_labels[p])
            source["label %d" % i] = "BOOT.BIN"
    options = []
    for i, p in enumerate(PSP_CONFIG_RECORD_OF):
        if p is None:
            # The Sound row's count and x: the Autorun row's (PC 13, 14).
            count, x, _ = recs[PSP_CONFIG_RECORD_OF[i + 2]]
            options.append((count, x, CONFIG_BUILTIN_OPTIONS[i]))
            source["option %d" % i] = "ours"
        else:
            options.append(recs[p])
            source["option %d" % i] = "BOOT.BIN"
    for i in range(CONFIG_CTRL):
        source["controller %d" % i] = "BOOT.BIN"
    return [(KIND_CONFIG, 0, config_payload(labels, options, ctrls))], source


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

    records = [(r[0], r[1], r[2:].split(b"\0")[0])
               for r in (donor[records + i * 8:records + i * 8 + 8] for i in range(CONFIG_RECORDS))]
    ctrl = gap + len(CONFIG_GAP)
    ctrls = [donor[ctrl + i * CTRL_STRIDE + 1:ctrl + (i + 1) * CTRL_STRIDE].split(b"\0")[0]
             for i in range(CONFIG_CTRL)]
    return [(KIND_CONFIG, 0, config_payload(labels, records, ctrls))]


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
    starts = verb_starts(game, donor)
    if starts is None:
        return []
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


def verb_starts(game, donor):
    """Where each verb string starts in `donor`, by its pointer table (convert_verbs), or None."""
    table_end = find_anchor(game, donor, "verb sets")
    if table_end < 0:
        return None
    ptrs = []
    while table_end - 4 * (len(ptrs) + 1) >= 0:
        v = struct.unpack_from("<I", donor, table_end - 4 * (len(ptrs) + 1))[0]
        if not is_pointer(v):
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
    return starts


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
    tail_len = CHAR_STRIDE - CHAR_NAME_PC
    at = find_anchor(game, donor, "char tail 0")
    if at < CHAR_NAME_US:
        return []
    base = at - CHAR_NAME_US
    payload = bytearray([CHAR_COUNT])
    records = [donor[base + k * CHAR_STRIDE:base + k * CHAR_STRIDE + CHAR_NAME_US + tail_len] for k in range(CHAR_COUNT)]
    if not same_anchor(game, "char tails", b"".join(us[CHAR_NAME_US:] for us in records)):
        raise SystemExit("names: the character records differ from the PC's past their names")
    for k in range(CHAR_COUNT):
        us = records[k]
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
    for name, blob in world_blobs(disc):
        at = find_anchor(game, blob, "merchant")
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
    at = find_anchor(game, donor, "battle boxes")
    if at < BATTLE_COUNT * BATTLE_ROOM:
        return []
    # The PSX's slots end at the box table; BOOT.BIN's are byte-aligned and
    # end one NUL before it, the table being s16 (PSP-EU, 2026-10-12). So the
    # slots end at the first of up to three NULs before the table where
    # every slot is then a word padded with NULs.
    def slots_fit(p):
        slots = [donor[at - p - (BATTLE_COUNT - i) * BATTLE_ROOM:at - p - (BATTLE_COUNT - i - 1) * BATTLE_ROOM]
                 for i in range(BATTLE_COUNT)]
        return not any(donor[at - p:at]) and all(s[0] and not s.split(b"\0", 1)[1].strip(b"\0")
                                                 for s in slots if b"\0" in s)
    pad = next((p for p in range(4) if slots_fit(p)), 0)
    at -= pad
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


# The short labels (DIV-0064, src/game/labels.cpp): five groups of slots in
# BOF3.exe's .data, one chunk each, the tag the group's number.
#
#   1  the status words, 2 x 8 at 0x66A0E8, and
#   2  the menu's stats, 4 x 8 at 0x66A0F8. START.EMI has both as two 8-byte
#      and four 4-byte slots between bytes the PC still has: LABEL_HEAD before
#      them and LABEL_TAIL after.
#   3  the item types, 0x66A120. START.EMI has them right after the sixteen
#      bytes the PC has before its pointer table LABEL_TYPES_HEAD, packed and
#      4-byte aligned, then their five pointers.
#   4  the skill types, 0x66A200, and
#   5  the battle's stats, 4 x 8 at 0x669CF0. BATTLE.EMI has five skill types
#      (the fifth the PC's 0x66A220), their five pointers, four 6-byte stat
#      slots and then the sixteen bytes the PC has at LABEL_BATTLE_TAIL.
#
#   6  the camp's master list (2026-10-07): its title, 8 bytes at 0x66A1F0
#      (the US and German discs MSTR, the French ME), and the mark beside a
#      completed master, 4 bytes at 0x66A2D8 - the one byte `t`, whose
#      single-byte slot of the shipped font is a star, which the overlay's
#      repaint of that slot turns into a lowercase t (the owner's cross,
#      2026-10-06). SHOP.EMI has both as NUL-terminated strings padded to 4
#      between the 24 bytes the PC has at LABEL_MASTER_HEAD and the masters'
#      requirement lists it has at LABEL_MASTER_LISTS; the disc's mark is
#      its code 0x84, the filled star of the dialogue set.
#   7  the sort menus (the owner's sortScreens route, 2026-10-07): eleven
#      slots from 0x66A170 of 8 or 12 bytes - the item sort's title and
#      three choices, the equipment sort's three, the ability sort's two and
#      the two more - behind the pointer table 0x66B12C (and 0x66B374 for
#      the AP pair), so repointed into the DLL's buffers like groups 3 and
#      4: the French PC elevé and Défense and the German AP niedr are over
#      8. START.EMI has the eleven right after the 28 bytes the PC has at
#      LABEL_SORT_HEAD, then their eleven pointers.
#   8  the camp's Skill Notes sort: its title 0x66A1DC (8) and first choice
#      0x66A1E4 (12), behind 0x66B36C. SHOP.EMI has them after the same 28
#      bytes, with the ability sort's two again and four pointers.
#   9  the Skill Ink count's label, 8 bytes at 0x66A118 (a push in the
#      code). SHOP.EMI has it after the SKILL slot the PC has at 0x664290
#      (every disc's; the NOTE before it is LISTE on the French and German)
#      and its three pointers.
#  10  the formation names: ten records of 28 at 0x6636B0 (a name of 16,
#      then three s16 pairs), drawn by the 8 px draw. START.EMI has the ten
#      as records of 20 (US: a name of 7, its length, the same three pairs)
#      or 22 (German: a name of 8, the length and a pad), so the run is
#      found by the pairs at either stride.
#  12  the gene splicing window's tabs (2026-10-10): 3 x 8 at 0x66A14C behind
#      the pointer table 0x66A164. BATTLE.EMI has them as 6-byte slots
#      (Data, Pick, Best) between the AP / 1 / 0 bytes the PC has at
#      LABEL_GENE_HEAD and the pieces and DATA / BEST it has at
#      LABEL_GENE_TAIL.
#  13  the faerie village's board lists (2026-10-10): the ten facility
#      names 0x669E18.. behind 0x669E68 and the ten choices 0x669E90..
#      behind 0x669EE0, read through the tables only, so repointed into the
#      DLL's 16-byte buffers like group 7. A COMMU*.EMI has the twenty as
#      8-byte slots (Merchant, Explorer, Antiques and Handyman fill theirs
#      with no NUL) right before the list rows and record offsets the PC has
#      at LABEL_VILLAGE_ROWS.
#  14  the village's words (2026-10-10), five slots the DLL's own draws read
#      from its buffers: the board panel's culture label 0x669E10 (the
#      disc's word is group 13's seventh choice); the word the ranked lists'
#      headings draw after a count, 名 at 0x669F08 on the PC, the pair
#      `faeries` / `faery` on the disc (two copies of the pair of 8-byte
#      slots end right at the kind table the PC has at LABEL_VILLAGE_KINDS);
#      and the hi-lo game's money and stake titles 0x669F60 / 0x669F68 - a
#      COMMU*.EMI's Cash and Pot, 8-byte slots each ending in the disc's
#      zenny code, right after the 72 bytes of sprite rectangles the PC has
#      at LABEL_VILLAGE_RECTS.
#  15  the Identify panel's two headings 弱点 / 持有物 (0x66A3E0 / 0x66A3E8),
#      which no disc carries a word for: a space each, which the DLL draws
#      through buffers of its own - nothing. Amended 2026-10-13 (DIV-0117):
#      then the four words every disc's MAGIC060.EMI draws on the panel -
#      WEAK and POINT (the small font, the weakness row), the item word above
#      the items (the French OBJET, the German OBJEKT) and the EXP label (the
#      German EP) - found in the routine by the places it draws them, the
#      panel's own coordinates (identify_words). From every PlayStation disc,
#      the Japanese one too (its words are the PC's own English); one byte a
#      letter, raw: the 8 px draw maps a letter's byte itself.
#  16  the faerie village's sixty faeries' names (2026-10-10): the name field
#      of each 20-byte trait record at FAERIE_NAMES (a name of 16, then the
#      four stat bytes at FAERIE_TRAITS + 20 r), copied five bytes at a time
#      into the save at a birth. COMMU00.EMI has the sixty as 9-byte records
#      - a 5-byte name padded with the space code, the same four stat bytes -
#      found by the stats (every one of the sixty equal to the PC's), so the
#      disc's record r is the PC's record r. Amended 2026-10-18 (D289): the
#      name went into record r + 1's field (the record read as stats first),
#      faerie 0 nameless and the sixtieth name dropped; a record whose PC name
#      field holds no glyph string would be sent empty and kept (none does).
#  17  the name the battle extra copies over character 7's as its equipment
#      screen opens (2026-10-13, DIV-0118), 5 bytes at 0x669CE8: BATE.EMI's
#      Beyd, the string its memcpy of 5 reads two calls after the sound 0x102
#      (guest_name), as the PC's BattleExtra_EquipOpen does.
#  18  fight 26's turn counter (2026-10-13, DIV-0119): BOSS025.EMI's words
#      after (or around) the count - `turns left`, the European singular
#      `turn  left`, the French `Rest   tours` / `Rest   tour` - and the
#      routine's layout, read off its code (turn_counter): the count's x, the
#      words' x, the singular's test; the layout a third string of raw bytes.
#  19  the hi-lo game's three choices (2026-10-13, DIV-0120): HIGH / LOW / SET,
#      three 4-byte slots (a four-letter word fills its slot, no NUL) right
#      before the 72 bytes of sprite rectangles the PC has at
#      LABEL_VILLAGE_RECTS, in whichever COMMU*.EMI holds them.
#  20  kind 0x5E's menu title (2026-10-18, the install-gap round, group LA;
#      docs/install-gap-labels.md): the 20 bytes at 0x66A098, which
#      EffectKind5E_DrawMenu prints through "%s" - SCENA10.EMI's twin's title
#      (parts_title), cut at the count its draw is handed (US 12, French 17);
#      on PSP-EU, BOOT.BIN's own twin. Also 17 on PSP-EU from BOOT.BIN.
#  21  the field panel's labels (the same): EQUIP 0x663668 (8), EXP 0x663670
#      (4), NEXT 0x663674 (8) - START.EMI's three slots right after the label
#      tail (the French SUIV., the German AUSR and WEIT).
#  22  the shared list's title, 8 bytes at 0x664288 (NOTE; the French and
#      German LISTE): SHOP.EMI's slot right before group 9's SKILL slot.
#  23  the gene list's title, 8 bytes at 0x66B1F0 (GENE): STATUS.EMI's word
#      after the 16 bytes the PC has at LABEL_GENE_LIST_HEAD.
#  11  the zenny unit, 4 bytes at 0x66A31C: the one byte `s`, whose
#      single-byte slot of the shipped font is the port's coin, repainted
#      by the overlay into a letter. START.EMI has the US code 0x60 (the
#      dialogue set's Z) in the slot right after the icon wheel's triangle
#      (the 20 bytes the PC has at LABEL_WHEEL_TRIANGLE), before the full
#      stop and the verbs' pointer table.
#
# Groups 3 and 4 are repointed by the DLL into 16-byte buffers of its own
# (their readers all go through pointer tables), so their room is 16; the
# others are written into their slots. A string that does not fit goes out
# empty, and the slot stays as shipped.
KIND_LABELS = 15
LABEL_HEAD, LABEL_HEAD_LEN, LABEL_TAIL, LABEL_TAIL_LEN = 0x663648, 24, 0x663660, 8
LABEL_TYPES_HEAD, LABEL_BATTLE_TAIL = 0x663960, 0x66B5B4
LABEL_MASTER_HEAD, LABEL_MASTER_HEAD_LEN, LABEL_MASTER_LISTS, LABEL_MASTER_LISTS_LEN = 0x66B3B8, 24, 0x66B3D0, 16
LABEL_SORT_HEAD, LABEL_SORT_HEAD_LEN = 0x66B110, 28
LABEL_NOTE_HEAD = b"SKILL\0\0\0"
LABEL_FORMATIONS, LABEL_FORMATION_COUNT = 0x6636B0, 10
LABEL_WHEEL_TRIANGLE, LABEL_WHEEL_TRIANGLE_LEN = 0x6637C8, 24
LABEL_GENE_HEAD, LABEL_GENE_HEAD_LEN, LABEL_GENE_TAIL, LABEL_GENE_TAIL_LEN = 0x66AF38, 20, 0x66AF4E, 32
LABEL_VILLAGE_ROWS, LABEL_VILLAGE_ROWS_LEN, LABEL_VILLAGE_LISTS = 0x652C6C, 32, 20
LABEL_VILLAGE_RECTS, LABEL_VILLAGE_RECTS_LEN = 0x652D04, 72
LABEL_VILLAGE_KINDS, LABEL_VILLAGE_KINDS_LEN = 0x653180, 12
# CommuDraw_ShowSteps' categories (8 x 9 bytes, rest_4d.cpp): BOOT.BIN's stake title ends before them
LABEL_VILLAGE_DRAWS, LABEL_VILLAGE_DRAWS_LEN = 0x652DEC, 32
IDENTIFY_HEADINGS = (b" ", b" ")    # a space each, raw: no disc has a word for the port's headings
# The places MAGIC060's member panel draws its words, (x, y) -> slot: every
# disc's routine (US 0x801EF1E0..0x801EF2A0, JP 0x801EF1E4..; the PC's
# Identify_DrawMember draws at the same rows), the first call at each.
IDENTIFY_PLACES = {(0x5A, 0x44): 0, (0x80, 0x44): 1, (0x5A, 0x8A): 2, (0xB0, 0x64): 3}
IDENTIFY_WORD_NAMES = ("WEAK", "POINT", "the item word", "the EXP label")
GUEST_COPY, GUEST_SOUND = 5, 0x102      # BATE's memcpy of the guest's name, and the sound two calls before it
TURN_BOX, TURN_ROW = (0x6B, 0x10), 0x12  # BOSS025's box and the counter's row (the PC's Boss26Fx_DrawCount's)
FAERIE_TRAITS, FAERIE_COUNT, FAERIE_STRIDE, FAERIE_DISC_STRIDE = 0x653210, 60, 20, 9
FAERIE_NAMES = 0x653200     # record r's name field, 16 bytes at + 20 r; its stats follow (FAERIE_TRAITS)
# Group 20: kind 0x5E's menu title, its PlayStation twin in SCENA10.EMI.
PARTS_TITLE_ROOM = 20       # the PC's field 0x66A098..0x66A0AC (its NUL at 0x66A0AC)
# Group 23: the gene list's title follows the 16 bytes the PC has at LABEL_GENE_LIST_HEAD.
LABEL_GENE_LIST_HEAD = 0x66B1E0
LABEL_ROOMS = {1: (8, 8), 2: (8, 8, 8, 8), 3: (16,) * 5, 4: (16,) * 5, 5: (8, 8, 8, 8), 6: (8, 4),
               7: (16,) * 11, 8: (8, 12), 9: (8,), 10: (16,) * 10, 11: (4,), 12: (8, 8, 8), 13: (16,) * 20,
               14: (16,) * 5, 15: (16,) * 6, 16: (16,) * 60, 17: (5,), 18: (16,) * 3, 19: (16,) * 3,
               20: (PARTS_TITLE_ROOM,), 21: (8, 4, 8), 22: (8,), 23: (8,)}
LABEL_NAMES = {1: "status words", 2: "menu stats", 3: "item types", 4: "skill types", 5: "battle stats",
               6: "master list", 7: "sort menus", 8: "note sort", 9: "ink label", 10: "formations",
               11: "zenny unit", 12: "gene tabs", 13: "village lists", 14: "village words",
               15: "identify words", 16: "faerie names", 17: "guest name", 18: "turn counter",
               19: "hi-lo choices", 20: "parts title", 21: "panel labels", 22: "list title", 23: "gene title"}


def label_pointers(donor, at, count):
    """`count` ascending PSX pointers at `at`, or None."""
    if at < 0 or at + 4 * count > len(donor):
        return None
    ptrs = struct.unpack_from("<%dI" % count, donor, at)
    if any(not is_pointer(p) for p in ptrs) or any(b <= a for a, b in zip(ptrs, ptrs[1:])):
        return None
    return ptrs


def label_chunk(tag, raws, report, raw=()):
    """The kind-15 chunk of group `tag`: its strings encoded one by one, but
    the slots in `raw` as they are (bytes the DLL reads itself: group 15's
    words, which the 8 px draw maps by their ASCII; group 18's layout)."""
    payload, kept = bytearray([len(raws)]), 0
    for i, (word, room) in enumerate(zip(raws, LABEL_ROOMS[tag])):
        if i in raw:
            if not word or 0 in word or len(word) + 1 > room:
                raise SystemExit("labels: %s %d is not a string of 1..%d bytes: %s"
                                 % (LABEL_NAMES[tag], i, room - 1, word.hex(" ")))
            payload += word + b"\0"
            continue
        raw_ = word
        enc = [encode_char(c) for c in raw_]
        if not raw_ and tag in (15, 16):  # a faerie record, or an identify word not found: as shipped
            payload += b"\0"
            continue
        if not raw_ or any(e is None for e in enc):
            raise SystemExit("labels: %s %d holds a code this language does not have: %s"
                             % (LABEL_NAMES[tag], i, raw_.hex(" ")))
        out = b"".join(enc)
        if len(out) + 1 > room:
            out, kept = b"", kept + 1
        payload += out + b"\0"
    report.append("%s %d%s" % (LABEL_NAMES[tag], len(raws), " (%d kept)" % kept if kept else ""))
    return (KIND_LABELS, tag, bytes(payload))


def label_run(blob, at, count):
    """`count` NUL-ended strings from `at`, named by the table of as many
    ascending PSX pointers that follows them (the first pointing at `at`), or
    None. The US disc pads each to its PC room, the French and German discs
    to four bytes, so the pointers place them."""
    for table in range(at + 4 * count, at + 0x100, 4):
        ptrs = label_pointers(blob, table, count)
        if not ptrs:
            continue
        starts = [at + p - ptrs[0] for p in ptrs]
        if starts[-1] >= table or any(blob[s] == 0 for s in starts):
            return None
        return [blob[s:table].split(b"\0")[0] for s in starts]
    return None


# ---- the groups the overlays' code names (15's words, 17, 18): read off the
# disc's MIPS, by what the routine does - the PC's twin draws or copies the
# same things at the same places - not by addresses, which differ per disc.

def kseg0(sections):
    """[(dest, bytes)] of an EMI's sections loaded into KSEG0 (code and its data)."""
    return [(d, b) for d, b in sections if 0x80000000 <= d < 0x80800000]


def mips_cstr(sections, va, limit=32):
    """The NUL-ended string at `va` in `sections`, or None (not in them, or no NUL within `limit`)."""
    for d, b in sections:
        if d <= va < d + len(b):
            end = b.find(b"\0", va - d, va - d + limit)
            return b[va - d:end] if end >= 0 else None
    return None


def mips_insns(sections):
    """(address, op, rs, rt, rd, unsigned imm, signed imm, funct, word) of every
    aligned word of `sections` (data decodes too; the readers check what they find)."""
    for d, b in sections:
        for o in range(0, len(b) - 3, 4):
            w, = struct.unpack_from("<I", b, o)
            imm = w & 0xFFFF
            yield (d + o, w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, imm,
                   imm - 0x10000 if imm & 0x8000 else imm, w & 0x3F, w)


MIPS_CALL_CLOBBERS = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31)


def mips_calls(sections):
    """[(address, target, {register: value})] for every `jal` in `sections`, the
    registers the straight-line code before it (and its delay slot) set from
    constants: lui; addiu / ori from $zero or a known register; addu of two
    known (`move`). Anything else that writes a register forgets it; a call
    forgets the registers a callee may change. Branches are not followed, so a
    value can come from the other side of one: every reader checks what it
    takes (a string where it wants a string, the coordinates it keys on)."""
    out, known = [], {0: 0}
    insns = list(mips_insns(sections))

    def put(r, v):
        if v is None:
            known.pop(r, None)
        else:
            known[r] = v & 0xFFFFFFFF

    def step(at, op, rs, rt, rd, imm, simm, funct, w):
        if op == 0x0F:                                  # lui
            put(rt, imm << 16)
        elif op == 0x09:                                # addiu
            put(rt, known[rs] + simm if rs in known else None)
        elif op == 0x0D:                                # ori
            put(rt, known[rs] | imm if rs in known else None)
        elif op == 0 and funct == 0x21:                 # addu (move)
            put(rd, known[rs] + known[rt] if rs in known and rt in known else None)
        elif op == 0 and funct not in (0x08, 0x09, 0x0C, 0x0D, 0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B):
            put(rd, None)                               # every other SPECIAL writing rd (not jr / syscall / mthi / mult..)
        elif op in (0x08, 0x0A, 0x0B, 0x0C, 0x0E) or 0x20 <= op <= 0x26:
            put(rt, None)                               # addi, slti(u), andi, xori, loads
        known[0] = 0

    for k, ins in enumerate(insns):
        at, op = ins[0], ins[1]
        if op == 0x03:                                  # jal: its delay slot first
            if k + 1 < len(insns):
                step(*insns[k + 1])
            target = (at & 0xF0000000) | ((ins[8] & 0x3FFFFFF) << 2)
            out.append((at, target, {r: v for r, v in known.items() if 4 <= r <= 7}))
            for r in MIPS_CALL_CLOBBERS:
                known.pop(r, None)
            continue
        step(*ins)
    return out


def is_label_word(word, room):
    """A word one byte a letter (A..Z, a..z) that fits a slot of `room` with its NUL."""
    return bool(word) and len(word) + 1 <= room and all(0x41 <= c <= 0x5A or 0x61 <= c <= 0x7A for c in word)


def identify_words(sections):
    """[WEAK, POINT, the item word, the EXP label] as MAGIC060's member panel
    draws them - the first call at each of IDENTIFY_PLACES whose $a0 is a
    string - or None if any is missing or is not a word of capital letters."""
    words = [None] * len(IDENTIFY_PLACES)
    for at, target, regs in mips_calls(sections):
        slot = IDENTIFY_PLACES.get((regs.get(5), regs.get(6)))
        if slot is None or words[slot] is not None or 4 not in regs:
            continue
        s = mips_cstr(sections, regs[4])
        if s and all(0x41 <= c <= 0x5A for c in s) and len(s) < 16:
            words[slot] = s
    return words if all(words) else None


def guest_name(sections, data=None):
    """BATE's guest name: the string its memcpy of GUEST_COPY bytes reads two
    calls after the sound GUEST_SOUND (US 0x801D0F18 / 0x801D0F54), or None.
    `data`, if given, is where the string is read (PSP-EU's BOOT.BIN whole,
    `sections` then its code alone: the same copy at 0x200DA8, the string
    right after the whelp's name, 2026-10-18)."""
    calls = mips_calls(sections)
    for k in range(2, len(calls)):
        regs = calls[k][2]
        if regs.get(6) == GUEST_COPY and 5 in regs and calls[k - 2][2].get(4) == GUEST_SOUND:
            s = mips_cstr(sections if data is None else data, regs[5])
            if s and is_label_word(s, GUEST_COPY):
                return s
    return None


def turn_counter(sections):
    """BOSS025's turn counter: (plural, singular, layout) or None. The routine
    begins at the call drawing TURN_BOX; to its `jr $ra`, the two calls with
    $a1 = TURN_ROW are the count's draw and the words' (their $a0 the x), the
    strings loaded between them the words - the first the plural, a second
    the singular (the US has one) - and the singular's test the routine's
    `sltiu` on the turn ('>', at least the value) or `beq` against a
    constant ('=', equal to it); 'n' and the plural twice where it has none.
    The layout is the raw bytes count x, words x, test, value."""
    insns = list(mips_insns(sections))
    calls = mips_calls(sections)
    for k, (at, target, regs) in enumerate(calls):
        if (regs.get(4), regs.get(5)) != TURN_BOX:
            continue
        end = next((i[0] for i in insns if i[0] > at and i[1] == 0 and i[7] == 0x08 and i[2] == 31), None)
        if end is None:
            continue
        row = [c for c in calls[k + 1:] if c[0] < end and c[2].get(5) == TURN_ROW and 4 in c[2]]
        if len(row) != 2:
            continue
        hi, words, test, last_const = {}, [], None, {}
        for a, op, rs, rt, rd, imm, simm, funct, w in insns:
            if not row[0][0] < a < row[1][0]:
                continue
            if op == 0x0F:
                hi[rt] = imm << 16
            elif op == 0x09 and rs in hi:
                s = mips_cstr(sections, (hi[rs] + simm) & 0xFFFFFFFF)
                if s and b"%" not in s:
                    words.append(s)
            elif op == 0x09 and rs == 0:
                last_const[rt] = imm
            elif op == 0x0B and test is None:                     # sltiu
                test = (b">", imm)
            elif op in (0x04, 0x05) and test is None and (rs in last_const or rt in last_const):
                test = (b"=", last_const.get(rt, last_const.get(rs)))
        if not words or len(words) > 2:
            continue
        if len(words) == 1:
            plural = singular = words[0]
            test = (b"n", 1)
        else:
            plural, singular = words
            if test is None:
                continue
        x_count, x_words = row[0][2][4], row[1][2][4]
        if not (0 < x_count < 0x100 and 0 < x_words < 0x100 and 0 < test[1] < 0x100):
            continue
        return plural, singular, bytes([x_count, x_words]) + test[0] + bytes([test[1]])
    return None


def parts_title(code, data):
    """Kind 0x5E's menu title (group 20), or None: the PC's EffectKind5E_DrawMenu
    0x47F9E0 prints the title through "%s" into the text scratch and draws it
    with Text_DrawAt's count 0xF; its twin (PSX SCENA10.EMI, US 0x801F7E40;
    PSP-EU BOOT.BIN 0x41938) does the same with its own count - the call
    whose second argument is a "%s" and third a string, the next call's
    fourth argument the characters drawn (US, German 12, French 17, Japanese
    15). The US and German titles fill their 12 bytes with no NUL (a pointer
    table follows), so the string is cut at the count. Exactly one such call,
    else None."""
    calls = mips_calls(code)
    found = []
    for k in range(len(calls) - 1):
        regs, count = calls[k][2], calls[k + 1][2].get(7)
        if 5 not in regs or 6 not in regs or mips_cstr(data, regs[5], 4) != b"%s":
            continue
        if count is None or not 0 < count < PARTS_TITLE_ROOM:
            continue
        for d, b in data:
            if d <= regs[6] < d + len(b):
                found.append(b[regs[6] - d:regs[6] - d + count].split(b"\0")[0])
    return found[0] if len(found) == 1 and found[0] else None


def module_sections(disc, name):
    """[(dest, bytes)] of the disc's first `name` (an EMI), or None if it has none."""
    found = disc.find(name)
    return emi_sections(disc.read(found[0])) if found else None


def code_label_chunks(magic, bate, boss, report):
    """Groups 15, 17 and 18 from the sections of MAGIC060.EMI, BATE.EMI and
    BOSS025.EMI (each may be None): {tag: chunk}, each group found. Group 15
    always - the two headings' spaces, and the four words where MAGIC060 has
    them, else empty, which the DLL reads as the PC's own (the Japanese
    disc's English)."""
    out = {}
    words = identify_words(kseg0(magic)) if magic else None
    if words is None:
        report.append("identify words: MAGIC060's four words not found, the PC's own drawn")
        words = [b""] * len(IDENTIFY_WORD_NAMES)
    out[15] = label_chunk(15, list(IDENTIFY_HEADINGS) + words, report, raw={0, 1} | {i + 2 for i, w in enumerate(words) if w})
    name = guest_name(kseg0(bate)) if bate else None
    if name:
        out[17] = label_chunk(17, [name], report)
    elif bate:
        report.append("guest name: not found in BATE.EMI")
    turns = turn_counter(kseg0(boss)) if boss else None
    if turns:
        out[18] = label_chunk(18, list(turns), report, raw={2})
    elif boss:
        report.append("turn counter: not found in BOSS025.EMI")
    return out


def convert_labels(game, start, battle, shop=None, commu=(), magic=None, bate=None, boss=None, scena10=None,
                   status=None):
    """[(kind, tag, payload)] and a report, from the whole START.EMI, BATTLE.EMI and SHOP.EMI (each may
    be None), every COMMU*.EMI (the faerie village's overlays), the sections of MAGIC060.EMI,
    BATE.EMI and BOSS025.EMI (code_label_chunks) and SCENA10.EMI (group 20), and the whole
    STATUS.EMI (group 23)."""
    chunks, report = [], []
    panel_labels = list_title = None
    cut = lambda blob, at, size: blob[at:at + size].split(b"\0")[0]
    if start:
        at = find_anchor(game, start, "label head")
        while at >= 0 and not same_anchor(game, "label tail",
                                          start[at + LABEL_HEAD_LEN + 32:at + LABEL_HEAD_LEN + 32 + LABEL_TAIL_LEN]):
            at = find_anchor(game, start, "label head", at + 1)
        if at >= 0:
            first = at + LABEL_HEAD_LEN
            chunks.append(label_chunk(1, [cut(start, first + 8 * i, 8) for i in range(2)], report))
            chunks.append(label_chunk(2, [cut(start, first + 16 + 4 * i, 4) for i in range(4)], report))
            # 21: the three panel labels the PC has right after the tail
            # (EQUIP 0x663668, EXP 0x663670, NEXT 0x663674): the disc's own
            # words in the same three slots (the French SUIV., the German AUSR).
            tail = first + 32 + LABEL_TAIL_LEN
            panel = [cut(start, tail, 8), cut(start, tail + 8, 4), cut(start, tail + 12, 8)]
            if all(w and all(is_word_byte(c) for c in w) for w in panel):
                panel_labels = label_chunk(21, panel, report)
            else:
                report.append("panel labels: not three words after the label tail")
        at = find_anchor(game, start, "item types head")
        if at >= 0:
            first = at + 16
            table = next((t for t in range(first + 4, first + 0x80, 4) if label_pointers(start, t, 5)), None)
            if table is None:
                raise SystemExit("labels: no pointer table after the item types")
            ptrs = label_pointers(start, table, 5)
            starts = [first + p - ptrs[0] for p in ptrs]
            if starts[-1] >= table or 0 in [start[s] for s in starts]:
                raise SystemExit("labels: the item types' pointers do not fit their strings")
            chunks.append(label_chunk(3, [cut(start, s, 16) for s in starts], report))
    if battle:
        at = find_anchor(game, battle, "battle tail")
        table = at - 24 - 20
        ptrs = label_pointers(battle, table, 5) if at >= 0 else None
        if ptrs:
            for pad in range(1, 17):         # the last string's NUL and any alignment
                starts = [table - pad - (ptrs[-1] - p) for p in ptrs]
                end = battle.find(b"\0", starts[-1])
                if starts[0] > 0 and all(battle[s] != 0 for s in starts) \
                        and all(battle[s - 1] == 0 for s in starts[1:]) \
                        and end < table and not any(battle[end:table]):
                    break
            else:
                raise SystemExit("labels: no placement of the skill types fits their pointer table")
            chunks.append(label_chunk(4, [cut(battle, s, 16) for s in starts], report))
            chunks.append(label_chunk(5, [cut(battle, at - 24 + 6 * i, 6) for i in range(4)], report))
    if shop:
        at = find_anchor(game, shop, "master head")
        while at >= 0:
            # Two strings between the head and the lists, each NUL-ended and
            # padded to four bytes: the title, then the mark.
            p, raws = at + LABEL_MASTER_HEAD_LEN, []
            while len(raws) < 2 and p < len(shop) and shop[p] != 0:
                end = shop.find(b"\0", p)
                raws.append(shop[p:end])
                p = (end + 1 + 3) & ~3
            if len(raws) == 2 and same_anchor(game, "master lists", shop[p:p + LABEL_MASTER_LISTS_LEN]):
                chunks.append(label_chunk(6, raws, report))
                break
            at = find_anchor(game, shop, "master head", at + 1)
        else:
            raise SystemExit("labels: SHOP.EMI has the PC's bytes around the master list's strings nowhere")
    if start:
        at, found = find_anchor(game, start, "sort head"), None
        while at >= 0 and not found:
            found = label_run(start, at + LABEL_SORT_HEAD_LEN, 11)
            at = find_anchor(game, start, "sort head", at + 1)
        if not found:
            raise SystemExit("labels: START.EMI has no eleven sort strings after the PC's bytes at 0x%X" % LABEL_SORT_HEAD)
        chunks.append(label_chunk(7, found, report))
        # The formations: the PC's ten records of 28 (name 16, pairs 12), the
        # disc's of 20 (name 7, its length, the same pairs) or 22 (German: name
        # 8, its length, a pad, the pairs), found by the pairs. The name is cut
        # at its own width: at 22, stride - 13 would take the length byte too,
        # and an eight-letter name has no NUL to stop at.
        name_len = {20: 7, 22: 8}
        disc_pairs = lambda base, stride: [start[base + stride * i + stride - 12:base + stride * (i + 1)]
                                           for i in range(LABEL_FORMATION_COUNT)]
        first = lambda at: find_anchor(game, start, "formation pair 0", at)
        same = lambda got: same_anchor(game, "formation pairs", b"".join(got))
        at, found = first(0), None
        while at >= 0 and not found:
            for stride in name_len:
                base = at - (stride - 12)
                if base >= 0 and same(disc_pairs(base, stride)):
                    found = (base, stride)
                    break
            at = first(at + 1)
        if not found:
            raise SystemExit("labels: START.EMI has no run of the PC's ten formation records")
        base, stride = found
        names = [cut(start, base + stride * i, name_len[stride]) for i in range(LABEL_FORMATION_COUNT)]
        chunks.append(label_chunk(10, names, report))
        at = find_anchor(game, start, "wheel triangle")
        unit = at + LABEL_WHEEL_TRIANGLE_LEN
        if at < 0 or start[unit + 4:unit + 8] != b"\x3e\0\0\0":
            raise SystemExit("labels: START.EMI's icon wheel triangle is not followed by the unit and the full stop")
        chunks.append(label_chunk(11, [cut(start, unit, 4)], report))
    if battle:
        # The gene window's tabs: three strings between the two anchors.
        at = find_anchor(game, battle, "gene head")
        while at >= 0:
            end = find_anchor(game, battle, "gene tail", at + LABEL_GENE_HEAD_LEN, at + LABEL_GENE_HEAD_LEN + 64)
            words = [w for w in battle[at + LABEL_GENE_HEAD_LEN:end].split(b"\0") if w] if end >= 0 else []
            if len(words) == 3:
                chunks.append(label_chunk(12, words, report))
                break
            at = find_anchor(game, battle, "gene head", at + 1)
        else:
            report.append("gene tabs: not found")
    # The village: the twenty list slots before the rows, the money and stake
    # titles after the rectangles, in whichever community overlay holds them.
    lists = titles = pair = hilo = None
    def slot_run(blob, end, stride, count):
        """`count` slots of `stride` ending at `end`, each a string from its first
        byte, NUL-padded to the stride (or filling it), or None."""
        start = end - stride * count
        if start < 0:
            return None
        out = []
        for i in range(count):
            slot = blob[start + stride * i:start + stride * (i + 1)]
            word = slot.split(b"\0")[0]
            if not word or any(c < 0x30 for c in word) or slot[len(word):].strip(b"\0"):
                return None
            out.append(word)
        return out

    for blob in commu:
        at = find_anchor(game, blob, "village kinds")
        if pair is None and at >= 32:
            # Two (plural, singular) pairs of 8-byte slots end right at the
            # kind table; the first pair is the headings' word.
            found = [cut(blob, at - 32 + 8 * i, 8) for i in range(2)]
            if all(found) and found == [cut(blob, at - 16 + 8 * i, 8) for i in range(2)]:
                pair = found
        at = find_anchor(game, blob, "village rows")
        if lists is None and at > 0:
            # Two lists of ten slots end at the rows (a pad of up to three
            # NULs between): each list's stride is its longest word, plus a
            # NUL unless the word fills it - 8 and 8 on the US and German
            # discs, 10 and 9 on the French.
            for pad in range(4):
                for sb in range(8, 13):
                    second = slot_run(blob, at - pad, sb, LABEL_VILLAGE_LISTS // 2)
                    if not second or max(len(w) for w in second) < sb - 1:
                        continue
                    for sa in range(8, 13):
                        first = slot_run(blob, at - pad - sb * (LABEL_VILLAGE_LISTS // 2), sa, LABEL_VILLAGE_LISTS // 2)
                        if first and max(len(w) for w in first) >= sa - 1:
                            lists = first + second
                            break
                    if lists:
                        break
                if lists:
                    break
        at = find_anchor(game, blob, "village rects")
        if titles is None and at >= 0:
            found = [cut(blob, at + LABEL_VILLAGE_RECTS_LEN + 8 * i, 8) for i in range(2)]
            if all(found):
                titles = found
        if hilo is None and at >= 12:
            # The hi-lo choices: three 4-byte slots right before the rectangles.
            found = [cut(blob, at - 12 + 4 * i, 4) for i in range(3)]
            if all(is_label_word(w, 5) for w in found):
                hilo = found
    if lists:
        chunks.append(label_chunk(13, lists, report))
    else:
        report.append("village lists: not found")
    if lists and titles and pair:
        chunks.append(label_chunk(14, [lists[16]] + pair + titles, report))
    else:
        report.append("village words: not found")
    coded = code_label_chunks(magic, bate, boss, report)
    chunks.append(coded[15])
    # The faeries: sixty 9-byte records whose stat bytes are the PC's, in
    # whichever community overlay holds them.
    disc_stats = lambda blob, base: [blob[base + FAERIE_DISC_STRIDE * r + 5:base + FAERIE_DISC_STRIDE * (r + 1)]
                                     for r in range(FAERIE_COUNT)]
    if game.carried(FAERIE_NAMES, FAERIE_STRIDE * FAERIE_COUNT):
        names_pc = exe_bytes(game, FAERIE_NAMES, FAERIE_STRIDE * FAERIE_COUNT)
        unnamed = {r for r in range(FAERIE_COUNT) if names_pc[FAERIE_STRIDE * r] < 0x80}
    else:           # disc-only: the name fields are not in the image; the PC's nameless records as measured
        unnamed = set(FAERIE_UNNAMED)
    first = lambda blob, at: find_anchor(game, blob, "faerie stat 0", at)
    same = lambda got: same_anchor(game, "faerie stats", b"".join(got))
    faeries = None
    for blob in commu:
        at = first(blob, 0)
        while at >= 5 and faeries is None:
            base = at - 5
            if same(disc_stats(blob, base)):
                faeries = []
                for r in range(FAERIE_COUNT):
                    name = blob[base + FAERIE_DISC_STRIDE * r:base + FAERIE_DISC_STRIDE * r + 5].rstrip(b"\xff\0")
                    faeries.append(b"" if r in unnamed else name)
            at = first(blob, at + 1)
        if faeries:
            break
    if faeries:
        chunks.append(label_chunk(16, faeries, report))
    else:
        report.append("faerie names: not found")
    if shop:
        at, found = find_anchor(game, shop, "sort head"), None
        while at >= 0 and not found:
            found = label_run(shop, at + LABEL_SORT_HEAD_LEN, 4)
            at = find_anchor(game, shop, "sort head", at + 1)
        if not found:
            raise SystemExit("labels: SHOP.EMI has no four note-sort strings after the PC's bytes at 0x%X" % LABEL_SORT_HEAD)
        chunks.append(label_chunk(8, found[:2], report))
        # The SKILL slot, three pointers, then a short word (the skill types'
        # own SKILL is followed by pointers only).
        at = shop.find(LABEL_NOTE_HEAD)
        while at >= 0:
            ink = at + len(LABEL_NOTE_HEAD) + 12
            word = cut(shop, ink, 8)
            if label_pointers(shop, at + len(LABEL_NOTE_HEAD), 3) and 0 < len(word) < 8 and max(word) < 0x80:
                break
            at = shop.find(LABEL_NOTE_HEAD, at + 1)
        if at < 0:
            raise SystemExit("labels: SHOP.EMI has no SKILL slot with three pointers and a word after it")
        chunks.append(label_chunk(9, [word], report))
        # 22: the shared list's title, the 8-byte slot right before that SKILL
        # (0x664288 on the PC, NOTE; the French and German LISTE).
        title = cut(shop, at - 8, 8)
        if title and all(is_word_byte(c) for c in title):
            list_title = label_chunk(22, [title], report)
        else:
            report.append("list title: no word before the SKILL slot")
    chunks += [coded[t] for t in (17, 18) if t in coded]
    if hilo:
        chunks.append(label_chunk(19, hilo, report))
    else:
        report.append("hi-lo choices: not found")
    # 20: kind 0x5E's menu title, read off SCENA10's twin of EffectKind5E_DrawMenu.
    title = parts_title(kseg0(scena10), kseg0(scena10)) if scena10 else None
    if title:
        chunks.append(label_chunk(20, [title], report))
    elif scena10:
        report.append("parts title: not found in SCENA10.EMI")
    chunks += [c for c in (panel_labels, list_title) if c]
    # 23: the gene list's title: the word after the 16 bytes the PC has at
    # LABEL_GENE_LIST_HEAD, in STATUS.EMI (whose twin of MenuList_GeneWinDraw
    # reads it, US 0x801EB250); the other copy of those bytes has no word after it.
    if status:
        at, found = find_anchor(game, status, "gene list head"), []
        while at >= 0:
            word = cut(status, at + 16, 8)
            if word and all(is_word_byte(c) for c in word):
                found.append(word)
            at = find_anchor(game, status, "gene list head", at + 1)
        if len(found) == 1:
            chunks.append(label_chunk(23, found, report))
        else:
            report.append("gene title: %d words after the PC's bytes in STATUS.EMI, wanted one" % len(found))
    return chunks, report


# The labels from a PSP disc (docs/loc-build-psp.md section 3). The PSP's own
# code reads them out of BOOT.BIN, which holds the START, STATUS, BATE, SHOP,
# BATTLE and COMMU modules' data as a native build: the same records and
# strings, but packed at byte alignment where the PlayStation's overlays pad
# to four, some fields resized to their English words, and a few blocks
# moved apart. So an anchor that ends in alignment NULs on the PC matches
# only without them (`anchor_core`), and the hashed anchors whose PC bytes
# include such padding (the battle tail, the gene head, the village rows)
# match nowhere. Each group below is found by the rule measured on PSP-EU's
# BOOT.BIN (2026-10-12), the only PSP build with Latin text; a group whose
# rule finds nothing is reported and left out, never guessed.

PSP_NOTE = b"NOTE\0"         # SHOP's Skill Notes word, which the ink label precedes in BOOT.BIN


def anchor_core(game, name):
    """A carried anchor without its leading and trailing NULs, and how many
    NULs led: (bytes, lead), or (None, 0) where the image does not carry it."""
    a = anchor(game, name)
    if a is None:
        return None, 0
    return a.strip(b"\0"), len(a) - len(a.lstrip(b"\0"))


def find_all(blob, needle):
    out, i = [], blob.find(needle)
    while i >= 0:
        out.append(i)
        i = blob.find(needle, i + 1)
    return out


def is_word_byte(c):
    """A text code a label is made of: a letter, digit or sign of the script
    (0x30 and up; LAST_CODE and the accented codes past it), or the space."""
    return c >= 0x30


def words_after(blob, at, count, reach=16):
    """`count` NUL-ended words from `at`: before each, up to `reach` bytes that
    are not text (NULs, a table's numbers) are skipped. None if any is missing."""
    out, p = [], at
    for _ in range(count):
        q = p
        while q < len(blob) and q - p < reach and not (is_word_byte(blob[q]) and blob[q] != SPACE_IN):
            q += 1
        end = blob.find(b"\0", q)
        word = blob[q:end] if end > q else b""
        if not word or not all(is_word_byte(c) for c in word):
            return None
        out.append(word)
        p = end + 1
    return out


def slots_before(blob, end, stride, count):
    """`count` slots of `stride` ending at `end`, each a word from its first
    byte padded with NULs (or filling it), or None (convert_labels' slot_run)."""
    start = end - stride * count
    if start < 0:
        return None
    out = []
    for i in range(count):
        slot = blob[start + stride * i:start + stride * (i + 1)]
        word = slot.split(b"\0")[0]
        if not word or not all(is_word_byte(c) for c in word) or slot[len(word):].strip(b"\0"):
            return None
        out.append(word)
    return out


def table_run(blob, at, count, reach=0x100):
    """`count` strings from `at` named by the first table of `count` ascending
    pointers after them, the first pointing at `at`: (words, table) or None.
    label_run with the table 4-aligned in the image rather than from `at`."""
    t = (at + count + 3) & ~3
    while t < at + reach:
        ptrs = label_pointers(blob, t, count)
        if ptrs and ptrs[0] == at:
            starts = [at + p - ptrs[0] for p in ptrs]
            if starts[-1] < t and all(blob[s] for s in starts):
                return [blob[s:t].split(b"\0")[0] for s in starts], t
        t += 4
    return None


def village_run(blob):
    """The village's twenty list words: the one run in `blob` of exactly
    twenty 8-byte slots, each a word padded with NULs or filling it."""
    slot = b"(?:" + b"|".join(b"[\\x30-\\xff]{%d}\\x00{%d}" % (n, 8 - n) for n in range(8, 0, -1)) + b")"
    found = [m.start() for m in re.finditer(b"(?=(" + slot + b"{20}))", blob, re.S)]
    runs = [a for a in found if a - 8 not in found and a + 8 not in found]   # maximal, not inside a longer run
    return runs


def convert_labels_psp(game, boot, verbs, magic=None, text_end=None):
    """[(kind, tag, payload)] and a report from BOOT.BIN, the PSP's labels;
    `verbs` is where convert_verbs found the menu verbs (verb_starts);
    `magic` MAGIC060.EMI's sections, for group 15's words (the PSP's EMIs are
    the Japanese build's: its words are the PC's own English); `text_end`
    BOOT.BIN's code size, for the groups read off its code (17, 20). Group
    18 comes from BOSS025, whose PSP copy is the Japanese build's too, and is
    not built here. Groups 21..23 are not built either: PSP-EU's words there
    are the PC's own English (its image holds them)."""
    chunks, report = [], []
    missing = lambda what, why: report.append("%s: not found in BOOT.BIN (%s)" % (what, why))
    # 1: the status words, the first two words after the verb sets.
    vs = find_anchor(game, boot, "verb sets")
    status = words_after(boot, vs + len(anchor_core(game, "verb sets")[0]), 2) if vs >= 0 else None
    tail, _ = anchor_core(game, "label tail")
    tails = find_all(boot, tail) if tail else []
    # 2: the menu's stats, four 4-byte slots ending at a label tail (the PSX's form).
    stats = next((s for s in (slots_before(boot, h, 4, 4) for h in tails) if s), None)
    if status and stats:
        chunks.append(label_chunk(1, status, report))
        chunks.append(label_chunk(2, stats, report))
    else:
        missing("status words and menu stats", "no two words after the verb sets, or no four 4-byte slots before a label tail")
    # 4 and 5: BATTLE's skill types, their five pointers, then four 6-byte stat
    # slots ending at a label tail (the PSX's form, the tail's core for its 16 bytes).
    skills = None
    for h in tails:
        battle = slots_before(boot, h, 6, 4)
        ptrs = label_pointers(boot, h - 24 - 20, 5) if battle else None
        found = table_run(boot, ptrs[0], 5) if ptrs else None
        if found and found[1] == h - 24 - 20:
            skills = (ptrs[0], found[0], battle)
            break
    # 3: the item types, the five strings whose pointer table ends where the skill types begin.
    types = None
    if skills:
        ptrs = label_pointers(boot, skills[0] - 20, 5)
        found = table_run(boot, ptrs[0], 5) if ptrs else None
        types = found[0] if found and found[1] == skills[0] - 20 else None
    if types:
        chunks.append(label_chunk(3, [w[:16] for w in types], report))
    else:
        missing("item types", "no five-pointer table ending at the skill types")
    if skills:
        chunks.append(label_chunk(4, [w[:16] for w in skills[1]], report))
        chunks.append(label_chunk(5, skills[2], report))
    else:
        missing("skill types and battle stats", "no five strings, five pointers and four 6-byte slots before a label tail")
    # 7 and 8: the sort menus (eleven strings) and the Skill Notes sort (four),
    # each with its pointer table, right after the sort head's core.
    head, lead = anchor_core(game, "sort head")
    # A hit with an eleven-string run is the sort menus'; one with four and
    # no eleven the Skill Notes' (the eleven's table also starts with four).
    sort = note = None
    for h in find_all(boot, head) if head else []:
        eleven = table_run(boot, h + len(head), 11)
        sort = sort or eleven
        note = note or (None if eleven else table_run(boot, h + len(head), 4))
    if sort:
        chunks.append(label_chunk(7, sort[0], report))
    else:
        missing("sort menus", "no eleven strings and their pointers after the sort head")
    # 6: the master list's title and mark, the first two words after the note sort's table.
    master = words_after(boot, note[1] + 16, 2) if note else None
    if master:
        chunks.append(label_chunk(6, master, report))
    else:
        missing("master list", "no two words after the note sort's pointers")
    if note:
        chunks.append(label_chunk(8, note[0][:2], report))
    else:
        missing("note sort", "no four strings and their pointers after the sort head")
    # 9: the ink label, the word that ends right before SHOP's NOTE.
    n = boot.find(b"\0" + PSP_NOTE)
    ink = boot[boot.rfind(b"\0", 0, n) + 1:n] if n > 0 else b""
    if ink and all(is_word_byte(c) for c in ink):
        chunks.append(label_chunk(9, [ink], report))
    else:
        missing("ink label", "no word before NOTE")
    # 10: the formations, as convert_labels finds them: ten records of 20 or
    # 22 whose pairs are the PC's (pair 0 by its hash, every pair checked).
    first = lambda at: find_anchor(game, boot, "formation pair 0", at)
    at, found = first(0), None
    while at >= 0 and not found:
        for stride, n in ((20, 7), (22, 8)):
            base = at - (stride - 12)
            if base >= 0 and same_anchor(game, "formation pairs", b"".join(
                    boot[base + stride * i + stride - 12:base + stride * (i + 1)] for i in range(LABEL_FORMATION_COUNT))):
                found = [boot[base + stride * i:base + stride * i + n].split(b"\0")[0]
                         for i in range(LABEL_FORMATION_COUNT)]
                break
        at = first(at + 1)
    if found:
        chunks.append(label_chunk(10, found, report))
    else:
        missing("formations", "no run of the PC's ten formation records")
    # 11: the zenny unit. BOOT.BIN keeps the unit and the full stop right
    # before the verb strings (the PSX: after the wheel triangle): the unit
    # is the word two before the first verb, the full stop the one between.
    unit = None
    if verbs:
        stop_at = boot.rfind(b"\0", 0, verbs[0] - 1) + 1
        unit_at = stop_at - 1
        while is_word_byte(boot[unit_at - 1]):
            unit_at -= 1
        if boot[stop_at:verbs[0] - 1] == b"\x3e" and boot[stop_at - 1] == 0:
            unit = boot[unit_at:stop_at - 1]
    if unit and len(unit) < 4:
        chunks.append(label_chunk(11, [unit], report))
    else:
        missing("zenny unit", "no unit and full stop before the verbs")
    # 12: the gene window's tabs, three slots ending (up to three NULs) before the gene tail.
    gene = find_anchor(game, boot, "gene tail")
    tabs = next((t for pad in range(4) for stride in range(4, 9)
                 for t in [slots_before(boot, gene - pad, stride, 3) if gene >= 0 else None] if t), None)
    if tabs:
        chunks.append(label_chunk(12, tabs, report))
    else:
        missing("gene tabs", "no three slots before the gene tail")
    # 13: the village's lists, the one run of twenty 8-byte slots.
    runs = village_run(boot)
    lists = slots_before(boot, runs[0] + 160, 8, LABEL_VILLAGE_LISTS) if len(runs) == 1 else None
    if lists:
        chunks.append(label_chunk(13, lists, report))
    else:
        missing("village lists", "%d runs of twenty 8-byte slots, wanted one" % len(runs))
    # 14: the village's words (D247 / D265, docs/abc-sweep-1013-village.md 3).
    # The culture label is list 13's seventh choice of the second list, as on
    # the PSX; the plural and singular pairs end at the kind table as on the
    # PSX, two copies, in slots of 7 here; the money title follows the
    # rectangles. The stake title is not beside it in BOOT.BIN: it is the first
    # word after the pointer table that ends before CommuDraw's categories
    # (LABEL_VILLAGE_DRAWS) - the name entry's words' table, then `Pot`, NUL,
    # the zenny code, as the PSX's 8-byte slot has it.
    kinds = find_anchor(game, boot, "village kinds")
    pair = next((s[:2] for stride in range(5, 9)
                 for s in [slots_before(boot, kinds, stride, 4) if kinds >= 0 else None]
                 if s and s[:2] == s[2:]), None)
    rects = find_anchor(game, boot, "village rects")
    money = boot[rects + LABEL_VILLAGE_RECTS_LEN:rects + LABEL_VILLAGE_RECTS_LEN + 8].split(b"\0")[0] \
        if rects >= 0 else b""
    draws = find_anchor(game, boot, "village draws")
    stake = b""
    if draws >= 0:
        p = draws & ~3
        while p > draws - 64 and not label_pointers(boot, p - 4, 1):
            p -= 4
        if label_pointers(boot, p - 4, 1):
            stake = boot[p:boot.find(b"\0", p)]
    words = [lists[16]] + pair + [money, stake] if lists and pair else None
    if words and all(w and len(w) < 8 and all(is_word_byte(c) for c in w) for w in words):
        chunks.append(label_chunk(14, words, report))
    else:
        missing("village words", "no culture choice, plural pair, money title or stake title (%s)" % words)
    if not donor_ja:
        chunks.append(code_label_chunks(magic, None, None, report)[15])
    # 16: the faeries, as convert_labels finds them (records of 9, the stats the PC's).
    first = lambda at: find_anchor(game, boot, "faerie stat 0", at)
    at, faeries = first(0), None
    while at >= 5 and faeries is None:
        base = at - 5
        if same_anchor(game, "faerie stats", b"".join(
                boot[base + FAERIE_DISC_STRIDE * r + 5:base + FAERIE_DISC_STRIDE * (r + 1)] for r in range(FAERIE_COUNT))):
            faeries = [b"" if r in FAERIE_UNNAMED else
                       boot[base + FAERIE_DISC_STRIDE * r:base + FAERIE_DISC_STRIDE * r + 5].rstrip(b"\xff\0")
                       for r in range(FAERIE_COUNT)]
        at = first(at + 1)
    if faeries:
        chunks.append(label_chunk(16, faeries, report))
    else:
        missing("faerie names", "no sixty 9-byte records with the PC's stats")
    # 19: the hi-lo choices, the three 4-byte slots right before the
    # rectangles, as on the PSX (PSP-EU's BOOT.BIN: HIGH, LOW, SET).
    hilo = [boot[rects - 12 + 4 * i:rects - 8 + 4 * i].split(b"\0")[0] for i in range(3)] if rects >= 12 else None
    if hilo and all(is_label_word(w, 5) for w in hilo):
        chunks.append(label_chunk(19, hilo, report))
    else:
        missing("hi-lo choices", "no three 4-byte words before the rectangles")
    # 17 and 20, read off the PSP's own code as convert_labels reads the PSX
    # overlays' (2026-10-18): BOOT.BIN's copy of 5 bytes two calls after the
    # sound 0x102 (PSP-EU 0x200DA8, the name right after Whelp), and its twin
    # of EffectKind5E_DrawMenu (0x41938, its count 12).
    if text_end:
        code, data = [(0, boot[:text_end])], [(0, boot)]
        name = guest_name(code, data)
        if name:
            chunks.append(label_chunk(17, [name], report))
        else:
            missing("guest name", "no copy of 5 bytes two calls after the sound 0x102")
        title = parts_title(code, data)
        if title:
            chunks.append(label_chunk(20, [title], report))
        else:
            missing("parts title", "no one \"%s\" print of a title before a counted draw")
    # In convert_labels' order, so that FIRST.DAT is byte for byte the PSX's where the words are.
    order = (1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15, 16, 8, 9, 17, 18, 19, 20, 21, 22, 23)
    return sorted(chunks, key=lambda c: order.index(c[1])), report


# The fishing minigame's text (DIV-0069, src/game/fishing_text.cpp,
# docs/fishing-text.md). Two groups in BOF3.exe's .data that every overlay left
# Chinese, one chunk each, the tag the group's number:
#
#   1  the thirteen lines effect kind 0xF types across the top window: 8-byte
#      records at FISH_LINES (a pointer, a label byte, a pause byte), read
#      through the pointers only, so repointed into the DLL's buffers;
#   2  the three tab labels above the equip menu, behind the pointer table
#      FISH_TABS, repointed likewise;
#   3  the four panel headings ROD, LURE, EQUIP, GUIDE (0x653EA0, D286,
#      2026-10-18): the disc's own words, which the French and German modules
#      lay out at their own widths after the tabs (fishing_headings, from the
#      code's draws), kept in the DLL's buffers and drawn by our draws centred
#      where the PC's are.
#
# On a disc the fishing module every fishing area carries (WORLDnn/AREAnnn.EMI)
# has both: the lines as 12-byte records (a count, a pointer, the same label
# and pause bytes) ending twelve bytes before the row table the PC still has
# byte for byte at FISH_ROWS; the tabs right after the edge-quad records the
# PC has at FISH_QUADS, fixed-width, the width the code's `addiu $a3, $zero, n`
# before the `lui` / `addiu` of their address hands the draw (US and German 4,
# French 7). Strings are the disc's own, one byte a letter as the verbs are.
KIND_FISHING = 16
FISH_LINES, FISH_LINE_COUNT, FISH_ROWS, FISH_ROWS_LEN = 0x653B98, 13, 0x653C04, 0x24
FISH_QUADS, FISH_QUADS_LEN, FISH_TABS = 0x653E6C, 50, 0x66A088
FISH_LINE_ROOM, FISH_TAB_ROOM = 64, 16      # src/game/fishing_text.cpp's buffers


def fishing_tab_width(sec, dest, base):
    """The fixed width the module's code draws its tab labels at, or None:
    `addiu $a3, $zero, n` just before `lui r, hi; addiu r, r, lo` of `base`,
    and the second label's `addiu rd, r, n` after it."""
    hi, lo = ((base >> 16) + (1 if base & 0x8000 else 0)) & 0xFFFF, base & 0xFFFF
    words = struct.unpack_from("<%dI" % (len(sec) // 4), sec)
    for k in range(1, len(words) - 1):
        w, nxt = words[k], words[k + 1]
        if w >> 26 != 0x0F or w & 0xFFFF != hi:
            continue
        r = (w >> 16) & 31
        if nxt >> 26 != 0x09 or (nxt >> 21) & 31 != r or (nxt >> 16) & 31 != r or nxt & 0xFFFF != lo:
            continue
        for j in range(k - 1, max(0, k - 4), -1):
            a3 = words[j]
            if a3 >> 26 == 0x09 and (a3 >> 21) & 31 == 0 and (a3 >> 16) & 31 == 7:
                n = a3 & 0xFFFF
                second = any(v >> 26 == 0x09 and (v >> 21) & 31 == r and v & 0xFFFF == n
                             for v in words[k + 2:k + 32])
                return n if second and 0 < n < FISH_TAB_ROOM else None
    return None


FISH_HEAD_COUNT, FISH_HEAD_ROOM = 4, 16     # src/game/fishing_text.cpp's heading buffers
FISH_HEAD_ROWS = (7, 7, 7, 0x47)            # the y each heading is drawn at: the PC's ROD, LURE, EQUIP, GUIDE


def fishing_headings(sec, dest, after):
    """The four panel headings (tag 3; D286), or None: the module's code draws
    each from an address past its tabs (`after`, up to 0x40 on) built by `lui`
    / `addiu`, with `addiu $a3, $zero, n` (or `ori`) the characters drawn and
    `addiu $a1, r, y` the row nearest it - y 7 for the first three and 0x47 for
    the fourth, as the PC's EffectKind0F_DrawItemsB / _DrawItemsA /
    _DrawEquipped draw ROD, LURE, EQUIP and GUIDE (US 0x801D81E4, 0x801D8878,
    0x801D8948, 0x801D8CEC; the French counts 5, 5, 7, 7; the German 4, 5, 7,
    6). In address order, which is the PC's. Each heading is the disc's bytes
    to its count or its NUL."""
    words = struct.unpack_from("<%dI" % (len(sec) // 4), sec)
    luis, found = {}, {}
    nearest = lambda k, test: min(((abs(j - k), words[j] & 0xFFFF) for j in range(max(0, k - 10), min(len(words), k + 11))
                                   if test(words[j])), default=(None, None))[1]
    is_a3 = lambda w: w >> 26 in (0x09, 0x0D) and (w >> 21) & 31 == 0 and (w >> 16) & 31 == 7
    is_a1 = lambda w: w >> 26 == 0x09 and (w >> 21) & 31 != 0 and (w >> 16) & 31 == 5
    for k, w in enumerate(words):
        op = w >> 26
        if op == 0x0F:
            luis[(w >> 16) & 31] = (k, (w & 0xFFFF) << 16)
            continue
        rs = (w >> 21) & 31
        if op != 0x09 or rs not in luis or k - luis[rs][0] > 4:
            continue
        imm = w & 0xFFFF
        v = (luis[rs][1] + (imm - 0x10000 if imm & 0x8000 else imm)) & 0xFFFFFFFF
        if not after <= v < after + 0x40:
            continue
        n, y = nearest(k, is_a3), nearest(k, is_a1)
        if n is None or y is None or not 0 < n < FISH_HEAD_ROOM:
            continue
        if found.setdefault(v, (n, y)) != (n, y):
            return None
    if len(found) != FISH_HEAD_COUNT or tuple(found[v][1] for v in sorted(found)) != FISH_HEAD_ROWS:
        return None
    return [sec[v - dest:v - dest + found[v][0]].split(b"\0")[0] for v in sorted(found)]


def convert_fishing(game, disc):
    """[(kind, tag, payload)] and a report, from the first fishing module on `disc`."""
    for name, blob in world_blobs(disc):
        for dest, sec in ([(0, blob)] if name == PSP_BOOT else emi_sections(blob)):
            r = find_anchor(game, sec, "fish rows")
            if r < 0 or not (dest & 0x80000000 or name == PSP_BOOT):
                continue
            chunks, report = [], []
            # The records end twelve bytes before the rows on a PSX disc; in
            # BOOT.BIN the rows are byte-aligned and the twelve bytes between
            # are nine (PSP-EU, 2026-10-12), so the records end 12..9 bytes
            # before them: the first gap whose label and pause bytes are the PC's.
            marks_at = lambda first: b"".join(sec[first + 12 * i + 8:first + 12 * i + 10]
                                              for i in range(FISH_LINE_COUNT))
            gaps = [12] + ([11, 10, 9] if name == PSP_BOOT else [])
            first = next((r - g - 12 * FISH_LINE_COUNT for g in gaps
                          if same_anchor(game, "fish label/pause", marks_at(r - g - 12 * FISH_LINE_COUNT))),
                         r - 12 - 12 * FISH_LINE_COUNT)
            if first < 0:
                raise SystemExit("fishing: %s's row table has no line records before it" % name)
            payload = bytearray([FISH_LINE_COUNT])
            marks = marks_at(first)
            if not same_anchor(game, "fish label/pause", marks):
                raise SystemExit("fishing: %s's lines' label and pause bytes are not the PC's" % name)
            for i in range(FISH_LINE_COUNT):
                count, ptr = struct.unpack_from("<II", sec, first + 12 * i)
                at = ptr - dest
                if not 0 <= at < len(sec) or not 0 < count < FISH_LINE_ROOM:
                    raise SystemExit("fishing: %s line %d points at 0x%08X, %d bytes" % (name, i, ptr, count))
                raw = sec[at:at + count].split(b"\0")[0]   # a count may include the NUL, or end without one
                out = encode_text(raw) if raw else None
                if out is None or len(out) + 1 > FISH_LINE_ROOM:
                    raise SystemExit("fishing: %s line %d holds a code this language does not have, or is long: %s"
                                     % (name, i, sec[at:at + count].hex(" ")))
                payload += out + b"\0"
            chunks.append((KIND_FISHING, 1, bytes(payload)))
            report.append("%d lines" % FISH_LINE_COUNT)
            q = find_anchor(game, sec, "fish quads")
            base = None if q < 0 else (q + FISH_QUADS_LEN + 3) & ~3
            width = None if base is None else fishing_tab_width(sec, dest, dest + base)
            if name == PSP_BOOT and q >= 0 and not width:
                # BOOT.BIN: the labels follow the quads unaligned, fixed-width
                # and filling it (PSP-EU: three of 4, no NUL), and its code is
                # the PSP's own, so the width is the run of text over three.
                base = q + FISH_QUADS_LEN
                run = base
                while run < len(sec) and is_word_byte(sec[run]):
                    run += 1
                width = (run - base) // 3 if (run - base) % 3 == 0 and 0 < run - base < 3 * FISH_TAB_ROOM else None
            if width:
                payload = bytearray([3])
                for i in range(3):
                    raw = sec[base + width * i:base + width * (i + 1)].split(b"\0")[0]
                    out = encode_text(raw) if raw else None
                    if out is None or len(out) + 1 > FISH_TAB_ROOM:
                        raise SystemExit("fishing: %s tab %d does not convert: %s" % (name, i, raw.hex(" ")))
                    payload += out + b"\0"
                chunks.append((KIND_FISHING, 2, bytes(payload)))
                report.append("3 tabs")
            else:
                report.append("no tabs found")
            heads = fishing_headings(sec, dest, dest + base + 3 * width) if width and name != PSP_BOOT else None
            if heads:
                payload = bytearray([FISH_HEAD_COUNT])
                for i, raw in enumerate(heads):
                    out = encode_text(raw) if raw else None
                    if out is None or len(out) + 1 > FISH_HEAD_ROOM:
                        raise SystemExit("fishing: %s heading %d does not convert: %s" % (name, i, raw.hex(" ")))
                    payload += out + b"\0"
                chunks.append((KIND_FISHING, 3, bytes(payload)))
                report.append("4 headings")
            else:
                report.append("no headings found")
            return chunks, report
    return [], []


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
    # The table's twelve words are pointers, which no disc carries (a disc-only
    # image leaves them zero): there the check is BOF3.exe's to make, and the
    # DLL's patch (src/game/battle_text.cpp) is what reads MESSAGE_SHIPPED.
    table = struct.unpack("<%dI" % MESSAGE_COUNT, exe_bytes(game, MESSAGE_TABLE, 4 * MESSAGE_COUNT))
    if not game.disc_only and table != MESSAGE_SHIPPED:
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


# The single-byte slots a disc-only Japanese table paints from the disc's own
# sheet (build_table_ja): the codes the JP script and ASCII share - the
# brackets, comma, stop, slash, equals, digits and capitals (the sibling's
# docs/TEXT_ENGINE.md code table; 0x2D is the long-vowel bar there, not a
# hyphen). Over the PC install those slots keep the port's own half-width
# glyphs, which the exe's own strings (its numbers among them) draw with; a
# blank table would leave those strings blank.
JA_SHARED = (0x28, 0x29, 0x2C, 0x2E, 0x2F) + tuple(range(0x30, 0x3A)) + (0x3D,) + tuple(range(0x41, 0x5B))


def build_table_ja(base_table, disc, disc_only=False):
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
    if disc_only:
        for code in JA_SHARED:
            slot = code - 0x26
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
        base = blank_table() if args.src.disc_only else font_pc.font_chunk(os.path.join(args.src.dat, "FIRST.DAT"))
        table, advances = build_table_ja(base, disc, args.src.disc_only)
        print("font: %d glyphs (Japanese: %d single-byte and symbol cells at 0x%X, %d kanji at 0x%X), sha256 %s"
              % (len(table) // font_pc.GLYPH_BYTES, JA_CELLS, JA_AT, JA_CELLS, JA_KANJI_AT,
                 hashlib.sha256(table).hexdigest()))
        return [(3, 0, table), (4, PC_ADVANCE, advances)]
    rows = donor_sheet(disc)
    ext_last = latin_extension(rows)
    base = blank_table() if args.src.disc_only else font_pc.font_chunk(os.path.join(args.src.dat, "FIRST.DAT"))
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
    table, advances = build_table(base, rows, redrawn, args.mono, args.src.disc_only)
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


def primary(tag):
    """A language tag's primary subtag: `en` of `en-US`, `ja` of `ja-JP`."""
    return tag.split("-", 1)[0].lower()


def language_tag(tag):
    """--lang: a BCP 47 language tag (`en-US`, `en-150`, `fr-FR`). It names
    the overlays, `<tag>.<NAME>.DAT`; what the code does by language goes by
    its primary subtag. fixtures.toml's `tag` per build is the one each disc's
    text carries (docs/importer.md section 5). The bare en, fr, de, ja of
    before 2026-10-08 are retired and refused (DIV-0005)."""
    msg = language_tags.retired(tag, "--lang")
    if msg:
        raise argparse.ArgumentTypeError(msg)
    if not re.fullmatch(r"[a-z]{2,3}(-[A-Za-z]{4})?(-(?:[A-Z]{2}|[0-9]{3}))?", tag):
        raise argparse.ArgumentTypeError("%r is not a language tag of the form ll[-Ssss][-RR|-999]" % tag)
    return tag


def disc_tag(path):
    """The tag the disc's text carries, fixtures.toml's `tag` for the build the
    disc identifies as (the importer's identity check: every file's hash). The
    engine and launcher read the tag since 2026-10-08 (DAT/<tag>.*,
    kLanguages); an overlay under any other name is not offered. An unheld
    disc has no tag: give --lang."""
    import importer
    found = importer.identify(path)
    if not found:
        raise SystemExit("%s is not a build fixtures.toml holds; give --lang <tag>" % path)
    return importer.tag_of(found[0])


def build_pause(args, font_chunks):
    lines = PAUSE_LINES.get(primary(args.lang))
    if lines is None:
        print("pause lines: none written for '%s'; the exe's own stay" % args.lang)
        return []
    if (primary(args.lang) == "ja") != donor_ja:
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
    base = chunk_of(args.src, "FIRST.DAT", 0, CLUT_TAG)
    donor = [s for dest, s in emi_sections(disc.read(disc.find("FIRST.EMI")[0])) if dest & 0x7FFFFFFF in CLUT_DESTS]
    if base is None or len(donor) != 1 or len(donor[0]) < CLUT_ROW:
        raise SystemExit("FIRST: no CLUT strip to take the white row from")
    if base[0] is None:
        # Disc-only: the strip is the PC's loc/zh-CN chunk. The disc's whole
        # strip is the PC's with the donor's row 0 - the strips differ in that
        # row alone (US: rows 1..15 equal, 2026-10-10) - so it is the same bytes.
        if len(donor[0]) != base[1]:
            raise SystemExit("FIRST: the disc's CLUT strip is %d bytes, the PC's %d" % (len(donor[0]), base[1]))
        return [(0, CLUT_TAG, bytes(donor[0]))]
    strip = bytearray(base[0])
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
# The two 16-row bands those boxes are in (rows 0..31 of the US sheet, as
# texels): the French and German START.EMI sheets carry them too, whole,
# 128 rows down (measured 2026-10-12: rows 128..159 of both equal the US
# sheet's rows 0..31 texel for texel), so a French or German disc has the
# letters CONFIG is cut from without an English disc. First 16 hex digits.
TITLE_LETTERS_SHA256 = "6ef65d0ead08aed0"


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


def title_letters(sheet):
    """The sheet's rows from the measured letters' bands (TITLE_LETTERS_SHA256)
    on, so that TITLE_LETTER's boxes find them; None if it has no such band."""
    for y0 in range(0, len(sheet) - TITLE_BAND + 1, 8):
        band = bytes(v for y in range(y0, y0 + TITLE_BAND) for v in sheet[y])
        if hashlib.sha256(band).hexdigest().startswith(TITLE_LETTERS_SHA256):
            return sheet[y0:]
    return None


def build_title(args, disc):
    """NEW GAME and LOAD GAME as the disc has them, and CONFIG cut from their letters."""
    found = disc.find("START.EMI")
    base = chunk_of(args.src, "START.DAT", 1, TITLE_TAG)     # only its size is used
    donor = [s for dest, s in emi_sections(disc.read(found[0])) if dest == TITLE_TAG] if found else []
    if base is None or len(donor) != 1 or len(donor[0]) != base[1]:
        raise SystemExit("START: no title menu sheet to rebuild")
    sheet = tiles_to_rows(donor[0], 2)
    page = [[0] * 256 for _ in range(256)]
    top = (TITLE_BAND - TITLE_CAP) // 2     # the port's rows are 32 tall about the same centre
    widths = []
    measured = hashlib.sha256(donor[0]).hexdigest().startswith(TITLE_DONOR_SHA256)
    for i, (x0, y0, w) in enumerate((TITLE_NEW, TITLE_LOAD)):
        if not measured:
            # Another disc's lettering in the same two bands (the French and
            # German discs, 2026-09-29: NOUVEAU JEU / CHARGER JEU, NEUES SPIEL /
            # SPIEL LADEN): its width is the ink's right edge plus 2, which is
            # what the measured widths above are for the US sheet (128 -> 130,
            # 138 -> 140).
            w = max(x for y in range(y0, y0 + TITLE_CAP) for x, v in enumerate(sheet[y]) if v) + 2
            w += w & 1                  # even, as the draw centres it at 160 - w / 2
        for y in range(TITLE_CAP):
            page[TITLE_BAND * i + top + y][:w] = sheet[y0 + y][x0:x0 + w]
        widths.append(w)
    letters = sheet if measured else title_letters(sheet)
    if letters is None:
        # The third row is the port's, and this sheet does not carry the
        # bands the letters were measured on (the French and German sheets
        # do, 128 rows down: title_letters): take it from the English page already built
        # from the US disc, an English overlay's START.DAT beside the shipped
        # file (`en-US.`, `en-150.`: the US and EU-English pages are
        # byte-identical, 2026-10-08) - the owner's choice, 2026-09-29: each
        # disc's own two rows, our CONFIG. Only the engine's tags: an `en.`
        # page of before 2026-10-08 is a retired code's (DIV-0005), noted.
        en_pages = args.src.english_title_pages()
        en_names = [label for label, _ in en_pages]
        if not en_names:
            print("title menu: this disc's sheet is not the one the letters were measured on, and no "
                  "English START.DAT holds a CONFIG row to borrow (build the English overlay first); left as shipped")
            return []
        pages = {open(path, "rb").read() for _, path in en_pages}
        if len(pages) != 1:
            raise SystemExit("title menu: the English overlays %s differ; which CONFIG row to borrow is not settled"
                             % ", ".join(en_names))
        en_path = en_pages[0][1]
        en_blob, en_chunks = dat.load(en_path)
        en_sheet = [c for c in en_chunks if c.kind == 1 and c.tag == TITLE_TAG]
        en_widths = [c for c in en_chunks if c.kind == TITLE_KIND]
        if len(en_sheet) != 1 or len(en_widths) != 1 or en_widths[0].size != 3:
            raise SystemExit("title menu: %s has no title page and widths to borrow from" % en_names[0])
        en_rows = tiles_to_rows(en_blob[en_sheet[0].offset:en_sheet[0].offset + en_sheet[0].size], 2)
        for y in range(TITLE_BAND):
            page[TITLE_BAND * 2 + y] = list(en_rows[TITLE_BAND * 2 + y])
        widths.append(en_blob[en_widths[0].offset + 2])
        print("title menu: this disc's two rows (%d and %d wide), CONFIG from %s (%d)" % (widths[0], widths[1], en_names[0], widths[2]))
        return [(1, TITLE_TAG, rows_to_tiles(page, 2)), (TITLE_KIND, 0, bytes(widths))]
    # CONFIG. Gaps in columns, by eye against NEW GAME's own spacing.
    if not measured:
        print("title menu: this disc's two rows (%d and %d wide), CONFIG from its own English letters "
              "(%d rows down)" % (widths[0], widths[1], len(sheet) - len(letters)))
    word = ((title_c(letters), 1), (title_letter(letters, "O"), 1), (title_letter(letters, "N"), 2),
            (title_f(letters), 0), (title_stem(letters), 2), (title_letter(letters, "G"), 0))
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


# ---------------------------------------------------------------- the faerie village's board sheet

# DIV-0088 (docs/village-text-scan.md section 4). The board's three side
# buttons - hunt, clear, build - are paint on the community's sprite sheet,
# the kind-1 chunk tagged VILLAGE_SHEET_TAG (VRAM 896, 256; 256 x 256 at 4
# bits) of COMMU01.DAT and COMMU05.DAT, and every disc repainted the three
# labels in its own language: against the US, French and German COMMU01.EMI
# sections of the same dest the PC's sheet differs in rows 224..255 only
# where the three labels are (and in rows 96..175, the board's own frame
# pieces, which the PC's draws place and the disc's art must not replace).
# The overlay is the PC's sheet with the label rows' differing bytes taken
# from the disc.
VILLAGE_SHEET_TAG, VILLAGE_SHEET_SIZE, VILLAGE_SHEET_ROW = 0x1C080200, 0x8000, 128
VILLAGE_LABEL_ROWS = range(224, 256)


def build_village_sheet(game, disc):
    """{name: [(kind, tag, payload)]} for the community files whose sheet the disc repaints,
    and the names a disc-only build (DiscCache) leaves out."""
    out, skipped = {}, []
    for name in ("COMMU01", "COMMU05"):
        found = disc.find(name + ".EMI")
        base = chunk_of(game, name + ".DAT", 1, VILLAGE_SHEET_TAG)
        if not found or base is None:
            continue
        if base[0] is None:
            # Disc-only: the sheet is the PC's loc/zh-CN chunk, which differs
            # from the disc's outside the label rows too - in pixel rows
            # 96..191, where the port cleared the PlayStation's empty cells
            # and laid a strip of the sheet's own icons for its draws. That
            # re-layout is a rule (RELAID, relay_sheet), so the disc's sheet
            # re-laid is the PC's with the disc's labels - and, on a French or
            # German disc, the disc's other repaints too
            # (docs/loc-build-disc-only.md 4.3).
            donor = [sec for dest, sec in emi_sections(disc.read(found[0])) if dest == VILLAGE_SHEET_TAG]
            if not donor or len(donor[0]) != VILLAGE_SHEET_SIZE:
                skipped.append(name)
                continue
            out[name + ".DAT"] = [(1, VILLAGE_SHEET_TAG, relay_sheet(VILLAGE_SHEET_TAG, donor[0]))]
            continue
        pc = [base[0]]
        donor = [sec for dest, sec in emi_sections(disc.read(found[0])) if dest == VILLAGE_SHEET_TAG]
        if not pc or not donor or len(pc[0]) != VILLAGE_SHEET_SIZE or len(donor[0]) != VILLAGE_SHEET_SIZE:
            continue
        sheet, changed = bytearray(pc[0]), 0
        for y in VILLAGE_LABEL_ROWS:
            for i in range(y * VILLAGE_SHEET_ROW, (y + 1) * VILLAGE_SHEET_ROW):
                if sheet[i] != donor[0][i]:
                    sheet[i] = donor[0][i]
                    changed += 1
        if changed:
            out[name + ".DAT"] = [(1, VILLAGE_SHEET_TAG, bytes(sheet))]
    return out, skipped


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
        # The PC's chunks are only checked against (sizes, the frame header).
        # A disc-only build has the sizes from the recipe, and its base/ holds
        # the disc's own frame section (a stand-in, the donor itself), so there
        # the header check is the shape's alone.
        pc = {(k, t): (b, size) for k, t, b, size in game.chunks(area + ".DAT") or ()}
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
                head = struct.unpack_from("<I", pc[key][0] if pc[key][0] is not None else s)[0]
                if struct.unpack_from("<I", s)[0] != head or head % 4 or head > len(s):
                    raise SystemExit("plates: %s's frame section header is not the PC's" % area)
                if any(not head <= o < len(s) for o in struct.unpack_from("<%dI" % (head // 4), s)[1:]):
                    raise SystemExit("plates: %s's frame section points outside itself" % area)
            elif len(s) != pc[key][1]:
                raise SystemExit("plates: %s chunk 0x%X is %d bytes, the PC's %d"
                                 % (area, key[1], len(s), pc[key][1]))
        out[area + ".DAT"] = [(k, t, s) for (k, t), s in sorted(donor.items())]
    return out


# ---------------------------------------------------------------- the port's sheets, re-laid from a disc's

# Two of the port's loc/zh-CN images are a PlayStation sheet re-laid, not new
# art (docs/loc-build-disc-only.md 4.3 and 4.6, measured 2026-10-12 against
# the JP, US, EU, French and German discs): the sheet at VRAM (896, 256) -
# the glyph atlas of thirteen modules, the faerie village's board sheet among
# them - and the one at (960, 0), the ending / kanji sheet of ENDKANJI and
# FIRST. Each is 64 x 256 halfwords at 4 bits, stored as 32 x 32-halfword
# tiles (0x800 bytes, 128 pixels a row) left to right, then down. Against the
# JP disc's section of the same dest, the port's sheet differs only where
# it cleared the PlayStation's pixels (rectangles, `clear`) and laid a strip
# of the sheet's own 16 x 16 icons, which sit at a pitch of 16 on the
# PlayStation's, at a pitch of 20 for its draws (`strip`: count, from, to).
# Applied to the JP disc's section the rule gives the port's chunk byte for
# byte; applied to a Western disc's, the port's except where that disc
# repainted outside the rule's rectangles (its words). Pixel coordinates
# only - the port's layout, no pixel of either sheet (CLAUDE.md rule 1).
SHEET_TILE, SHEET_TILE_W = 0x800, 128          # bytes, pixels a tile row (32 halfwords at 4 bits)
RELAID = {
    0x1C080200: {"clear": ((0, 96, 240, 48), (0, 96, 120, 96)),
                 "strip": (8, (120, 208), (0, 100))},
    0x1E000200: {"clear": ((0, 72, 256, 84), (0, 72, 204, 136), (0, 72, 105, 144), (227, 72, 29, 136),
                           (0, 168, 256, 40), (239, 219, 17, 30), (160, 224, 96, 24), (0, 232, 52, 8),
                           (56, 232, 200, 16)),
                 "strip": (12, (56, 232), (0, 80))},
}
ICON, ICON_PITCH, STRIP_PITCH = 16, 16, 20


def sheet_pixels(tag, body):
    """A kind-1 4-bit sheet as rows of pixel values, from its tiles."""
    tiles_w = (tag >> 8) & 0xFF
    w = tiles_w * SHEET_TILE_W
    rows = [bytearray(w) for _ in range(len(body) // SHEET_TILE // tiles_w * 32)]
    for t in range(len(body) // SHEET_TILE):
        x0, y0 = (t % tiles_w) * SHEET_TILE_W, (t // tiles_w) * 32
        for y in range(32):
            line = body[t * SHEET_TILE + y * 64:t * SHEET_TILE + y * 64 + 64]
            row = rows[y0 + y]
            for i, b in enumerate(line):
                row[x0 + 2 * i], row[x0 + 2 * i + 1] = b & 15, b >> 4
    return rows


def sheet_body(tag, rows):
    tiles_w = (tag >> 8) & 0xFF
    out = bytearray()
    for t in range(len(rows) // 32 * tiles_w):
        x0, y0 = (t % tiles_w) * SHEET_TILE_W, (t // tiles_w) * 32
        for y in range(32):
            row = rows[y0 + y]
            out += bytes(row[x0 + 2 * i] | row[x0 + 2 * i + 1] << 4 for i in range(64))
    return bytes(out)


def relay_sheet(tag, body):
    """A disc's section of dest `tag` re-laid as the port laid its own (RELAID)."""
    rule = RELAID[tag]
    src = sheet_pixels(tag, body)
    rows = [bytearray(r) for r in src]
    for x, y, w, h in rule["clear"]:
        for yy in range(y, y + h):
            rows[yy][x:x + w] = bytes(w)
    count, (sx, sy), (dx, dy) = rule["strip"]
    for i in range(count):
        a, b = sx + ICON_PITCH * i, dx + STRIP_PITCH * i
        for yy in range(ICON):
            rows[dy + yy][b:b + ICON] = src[sy + yy][a:a + ICON]
    return sheet_body(tag, rows)


def relaid_region(tag):
    """The pixels relay_sheet writes, as a set of (x, y)."""
    rule, out = RELAID[tag], set()
    for x, y, w, h in rule["clear"]:
        out.update((xx, yy) for yy in range(y, y + h) for xx in range(x, x + w))
    count, _, (dx, dy) = rule["strip"]
    for i in range(count):
        out.update((dx + STRIP_PITCH * i + xx, dy + yy) for yy in range(ICON) for xx in range(ICON))
    return out


def check_relaid(game, disc):
    """Against the PC install: the rule gives the port's chunk on every pixel it
    writes, from this disc's section - so a wrong rectangle cannot survive a PC
    build. [(tag, container)] checked; a SystemExit on a difference."""
    done = []
    for tag, name in ((0x1C080200, "STATUS"), (0x1E000200, "ENDKANJI")):
        found = disc.find(name + ".EMI")
        pc = chunk_of(game, name + ".DAT", 1, tag)
        if not found or pc is None or pc[0] is None:
            continue
        donor = [s for dest, s in emi_sections(disc.read(found[0])) if dest == tag]
        if not donor or len(donor[0]) != len(pc[0]):
            raise SystemExit("re-laid sheets: %s has no section of dest 0x%X the PC's size" % (name, tag))
        got, want = sheet_pixels(tag, relay_sheet(tag, donor[0])), sheet_pixels(tag, pc[0])
        bad = sum(1 for x, y in relaid_region(tag) if got[y][x] != want[y][x])
        if bad:
            raise SystemExit("re-laid sheets: %s 0x%X: the rule differs from the port's sheet in %d pixels"
                             % (name, tag, bad))
        done.append((tag, name))
    return done


def vram_rect(tag, size):
    """(x, y, w, h) in halfwords of a kind-1 chunk, as LoadDatFile places it."""
    w = ((tag >> 8) & 0xFF) * 32
    return (tag >> 24) * 32, ((tag >> 16) & 0xFF) * 32, w, size // 2 // w


def covers(a, b):
    """Whether rectangle a covers rectangle b wholly, and whether they meet at all."""
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    meet = ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah
    return ax <= bx and ay <= by and ax + aw >= bx + bw and ay + ah >= by + bh, meet


def build_standins(game, disc, overlays):
    """Disc-only: {name: [(kind, tag, payload)]} for each loc/zh-CN image no
    builder above replaced - the disc's own section of the same dest, re-laid
    where the port re-laid it (RELAID) - and [(name, tag, why)] left out.

    The overlay walks after the container's base chunks, so a stand-in must be
    what the PC's slot order leaves: one that a later base chunk covers wholly
    (FIRST.DAT's atlas, under 0x1A080400) is left out, as the PC's end state
    has none of it; one a later chunk only partly covers would need cutting,
    and stops the build (none does)."""
    out, left = {}, []
    for name in game.names():
        rows = game.chunks(name) or ()
        for slot, (kind, tag, body, size) in enumerate(rows):
            if kind != 1 or game.recipe[name][slot]["layer"] != "loc/zh-CN":
                continue
            if any(k == kind and t == tag for k, t, _ in overlays.get(name, ())):
                continue
            mine = vram_rect(tag, size)
            later = [covers(vram_rect(t, n), mine) for k, t, b, n in rows[slot + 1:] if k == 1]
            if any(whole for whole, _ in later):
                left.append((name, tag, "covered by a later slot"))
                continue
            if any(meet for _, meet in later):
                raise SystemExit("stand-ins: %s 0x%X is partly covered by a later slot" % (name, tag))
            found = disc.find(os.path.splitext(name)[0] + ".EMI")
            donor = [s for dest, s in emi_sections(disc.read(found[0])) if dest == tag] if found else []
            if len(donor) != 1 or len(donor[0]) != size:
                left.append((name, tag, "no section of that dest and size on the disc"))
                continue
            out.setdefault(name, []).append((1, tag, relay_sheet(tag, donor[0]) if tag in RELAID else donor[0]))
    return out, left


def reset_donor():
    """The module state one donor leaves for the next (`--discs` builds several
    in one process): the Latin atlas's extension, the Japanese flag, the pair
    table, the Japanese sheets and the button codes' area and counts."""
    global ext_last, donor_ja, ja_sheets
    ext_last, donor_ja, ja_sheets = EXT_FIRST - 1, False, None
    ja_pairs.clear()
    button_counts.clear()
    set_button_area("", None, False)


DISC_EXT = (".cue", ".iso", ".bin")


def scan_discs(directory):
    """The disc images in a directory, sorted: every .cue and .iso, and a .bin
    that no .cue there names (a cue's tracks are read through the cue)."""
    names = sorted(n for n in os.listdir(directory) if n.lower().endswith(DISC_EXT))
    named = set()
    for n in names:
        if n.lower().endswith(".cue"):
            with open(os.path.join(directory, n), "r", errors="replace") as f:
                named.update(m.lower() for m in re.findall(r'FILE\s+"([^"]+)"', f.read(), re.I))
    return [os.path.join(directory, n) for n in names if not (n.lower().endswith(".bin") and n.lower() in named)]


def identify_discs(paths):
    """[(path, build id or None, tag or None, why)] for each image: the build
    fixtures.toml holds it as (every file hashed, `importer.identify`), and
    whether `all` builds from it. Only a verified PlayStation release is built
    from, once per build: the PSP discs have not been run through `all`
    (docs/dialogue-localisation.md section 1), and a second image of a build
    already listed would overwrite the first's overlays with the same bytes."""
    import importer
    builds = {b["id"]: b for b in importer._fixtures()["build"]}
    out, seen = [], {}
    for p in paths:
        print("identifying %s (every file hashed against fixtures.toml)..." % p, flush=True)
        found = importer.identify(p)
        if not found:
            out.append((p, None, None, "not a build fixtures.toml holds"))
            continue
        bid, b = found[0], builds[found[0]]
        if b["role"] != "psx-disc":
            out.append((p, bid, b["tag"], "%s is %s, not a PlayStation disc" % (bid, b["role"])))
        elif b.get("status") != "verified":
            out.append((p, bid, b["tag"], "%s is catalogued but not verified" % bid))
        elif bid in seen:
            out.append((p, bid, b["tag"], "a second image of %s; %s is built" % (bid, seen[bid])))
        else:
            seen[bid] = p
            out.append((p, bid, b["tag"], ""))
    return out


def source_of(args):
    """args.src: the PC install (`--game`) or a disc-only importer cache (`--cache`)."""
    if (args.game is None) == (args.cache is None):
        raise SystemExit("all: give --game DIR (the PC install) or --cache CACHE (a disc-only importer cache), not both")
    return PcInstall(args.game) if args.game else DiscCache(args.cache)


def check_source(src):
    if src.disc_only:
        print("cache: base/exe/data.bin from %s, %d containers in base/dat/; the PC's loc/zh-CN chunks not there"
              % (src.build, len(os.listdir(src.dat))))
    else:
        check_game(src.game)


def check_game(game):
    """The install against fixtures.toml: BOF3.exe must be the catalogued
    port (every exe address below is only meaningful in that image, CLAUDE.md
    rule 3); DAT/ is named when it is the shipped tree and reported otherwise -
    overlays beside the originals are excluded from the match, an edited
    original is not, and the overlays are still built over whatever is there."""
    import importer
    exe = os.path.join(game, "BOF3.exe")
    if not os.path.isfile(exe):
        raise SystemExit("%s: no BOF3.exe" % game)
    found = importer.identify(exe)
    if found != ("pc-zh", "exe"):
        raise SystemExit("%s does not hash as the catalogued BOF3.exe (fixtures.toml pc-zh); refusing to build over it" % exe)
    print("game: BOF3.exe is pc-zh's (fixtures.toml); hashing DAT/...", flush=True)
    tree = importer.identify(dat_dir(game))
    print("game: DAT/ is %s" % ("the shipped tree (%s, %s)" % tree if tree else
                                "NOT the shipped tree - an original differs from fixtures/pc-zh.DAT.files.tsv; building over it anyway"))


def cmd_all_discs(args):
    """`--discs DIR` (and a repeated `--disc`): identify every image, build one
    overlay set per held PlayStation release under its own tag, report the rest."""
    paths = list(args.disc or [])
    if args.discs:
        if not os.path.isdir(args.discs):
            raise SystemExit("%s: not a directory" % args.discs)
        paths += scan_discs(args.discs)
    if not paths:
        raise SystemExit("no disc images: --discs %s holds none of %s" % (args.discs, ", ".join(DISC_EXT)))
    args.src = source_of(args)
    check_source(args.src)
    # English first, then the rest in filename order: the French and German
    # title menus borrow their CONFIG row from an English START.DAT already
    # built (build_title), as importer.build_languages orders them.
    found = sorted(identify_discs(paths), key=lambda r: not (r[2] and not r[3] and primary(r[2]) == "en"))
    rows = []
    for p, bid, tag, why in found:
        if why:
            rows.append((p, bid or "-", tag or "-", "skipped: " + why))
            continue
        if args.dry_run:
            rows.append((p, bid, tag, "would build %s.*.DAT" % tag))
            continue
        print("\n== %s: %s, overlays %s.*.DAT ==" % (p, bid, tag))
        args.lang = tag
        n = build_all(args, p)
        rows.append((p, bid, tag, "built %d overlay files" % n))
    w = max(len(r[0]) for r in rows)
    print("\n" + "\n".join("%-*s  %-10s %-7s %s" % (w, *r) for r in rows))
    if not any(r[3].startswith(("built", "would build")) for r in rows):
        raise SystemExit("no held PlayStation disc among them: nothing built")
    return 0


def cmd_all(args):
    """One disc: every overlay in one pass, one file written per shipped DAT that needs one."""
    if args.discs or len(args.disc) != 1:
        if args.lang_given:
            raise SystemExit("--lang names one disc's overlays; with --discs or several --disc the tag is each disc's own")
        return cmd_all_discs(args)
    args.src = source_of(args)
    if args.dry_run:
        check_source(args.src)
    if args.lang is None:
        args.lang = disc_tag(args.disc[0])
        print("--lang not given: the disc's own tag, %s" % args.lang)
    if args.dry_run:
        print("--dry-run: would build %s overlays from %s into %s; nothing written"
              % (args.lang, args.disc[0], os.path.dirname(args.src.out_path(args.lang, "FIRST.DAT"))))
        return 0
    build_all(args, args.disc[0])
    return 0


def build_all(args, disc_path):
    """Every overlay from one disc under args.lang; the count of files written."""
    reset_donor()
    src = args.src
    if src.disc_only:
        if args.pc_white:
            raise SystemExit("--pc-white keeps the port's text CLUT row, which a disc-only cache does not have")
    else:
        dat_dir(src.game)
        check_anchors(src)      # the disc-only build's hashes are this exe's (PC_SHA)
    disc = open_disc(disc_path)
    overlays = {"FIRST.DAT": build_font(args, disc)}
    overlays["FIRST.DAT"] += build_pause(args, overlays["FIRST.DAT"])   # after the glyphs it names
    if not args.pc_white:
        overlays["FIRST.DAT"] += build_white_clut(args, disc)
    title = build_title(args, disc)
    if title:
        overlays["START.DAT"] = title
    texts = pools = kept_text = kept_pool = enemies = kept_enemy = 0
    rmap, how = release_map(disc)
    psp_eu = is_psp(disc) and not donor_ja
    for name in src.names():
        stem = os.path.splitext(name)[0]
        if args.only and stem.upper() not in (args.only.upper(), "FIRST"):
            continue
        found = disc.find(stem + ".EMI")
        if not found:
            continue
        sections = None
        for kind, tag, base, size in src.chunks(name):
            if kind != 0 or tag not in (0, POOL_TAG, ENEMY_TAG):
                continue
            if sections is None:
                sections = emi_sections(disc.read(found[0]))
            if base is None and tag == ENEMY_TAG:
                # Disc-only, the container not in base/ (the EU, French and
                # German discs alone leave eight areas out for their banks):
                # the stats are the donor's own, widened as the importer
                # widens them into base/ (importer.widen_enemies), so the
                # names are built all the same.
                import importer
                donor = [s for dest, s in sections if dest & 0x7FFFFFFF == ENEMY_DEST & 0x7FFFFFFF]
                base = importer.widen_enemies(donor[0]) if donor else None
                if base is None:
                    print("  SKIP %s tag %X: no enemy table in the cache's base/ or on the disc" % (name, tag))
                    continue
            try:
                if tag == ENEMY_TAG:
                    donor = [s for dest, s in sections if dest & 0x7FFFFFFF == ENEMY_DEST & 0x7FFFFFFF]
                    if not donor:
                        continue
                    names, kept = convert_enemy_names(base, donor[0])
                    enemies, kept_enemy = enemies + len(names), kept_enemy + kept
                    overlays.setdefault(name, []).extend(names)
                    continue
                if tag == 0:
                    donor = [s for dest, s in sections if dest & 0x7FFFFFFF == 0x10000]
                    if not donor:
                        continue
                    set_button_area(name, rmap, psp_eu)
                    try:
                        block, kept = convert_block(donor[0], base, TEXT_ROOM)
                    finally:
                        set_button_area("", None, False)
                    texts, kept_text = texts + 1, kept_text + kept
                else:
                    donor = [s for want in POOL_DESTS for dest, s in sections if dest & 0x7FFFFFFF == want]
                    if not donor:
                        continue
                    block, kept = convert_pool(donor[0], base)
                    pools, kept_pool = pools + 1, kept_pool + kept
            except ValueError as e:
                print("  SKIP %s tag %X: %s" % (name, tag, e))
                continue
            overlays.setdefault(name, []).append((0, tag, block))
    start_emi = disc.find("START.EMI")
    global donor_pointers
    donor_pointers = PSX_POINTERS
    read_module = lambda found: disc.read(found[0])
    if is_psp(disc):
        # The exe's strings: BOOT.BIN, where the PSP's own code reads them.
        # The EMIs' overlays are the Japanese build's on both PSP discs.
        boot = disc.boot()
        donor_pointers = disc.pointers()
        read_module = lambda found: boot
    if donor_ja:
        # The exe's own strings (kinds 7..12) are found with the US layouts and
        # drawn through the Latin layout patches; a Japanese overlay leaves
        # them as shipped for now.
        print("config, verbs, character names, merchant, battle labels and messages: "
              "not built for a Japanese disc")
        start_emi = None
    if start_emi and not args.only:
        if is_psp(disc):
            # BOOT.BIN's Config screen is the PSP's own, not the PlayStation's
            # the PC draws: no Sound row, Button Config. and Delete Data added
            # (docs/loc-build-psp.md 3.3). Its words where it has the slot, ours
            # where it has not (D246, the owner's decision 2026-10-17).
            cfg, source = convert_config_psp(boot, disc.text_end)
            if cfg:
                ours = [k for k, v in source.items() if v == "ours"]
                print("config screen: 6 labels, 17 options, 6 controller names; from BOOT.BIN %d, ours %d (%s)"
                      % (len(source) - len(ours), len(ours), ", ".join(ours)))
            else:
                print("config screen: not found in BOOT.BIN")
        else:
            cfg = convert_config(read_module(start_emi))
            print("config screen: " + ("6 labels, 17 options, 6 controller names" if cfg else "not found on this disc"))
        overlays["FIRST.DAT"] += cfg
        verbs = convert_verbs(src, read_module(start_emi))
        overlays["FIRST.DAT"] += verbs
        print("menu verbs: " + ("%d" % VERB_COUNT if verbs else "not found on this disc"))
        chars = convert_char_names(src, read_module(start_emi))
        overlays["FIRST.DAT"] += chars
        print("character names: " + ("%d" % CHAR_COUNT if chars else "not found on this disc"))
        merchant = convert_merchant(src, disc)
        overlays["FIRST.DAT"] += merchant
        print("merchant name: " + ("found" if merchant else "not found on this disc"))
        fishing, report = convert_fishing(src, disc)
        overlays["FIRST.DAT"] += fishing
        print("fishing: " + (", ".join(report) if report else "not found on this disc"))
    battle_emi = None if donor_ja else disc.find("BATTLE.EMI")
    if battle_emi and not args.only:
        cmds = convert_battle_commands(src, read_module(battle_emi))
        overlays["FIRST.DAT"] += cmds
        print("battle commands: " + ("%d" % BATTLE_COUNT if cmds else "not found on this disc"))
        msgs = convert_battle_messages(src, read_module(battle_emi))
        overlays["FIRST.DAT"] += msgs
        print("battle messages: " + ("%d" % MESSAGE_COUNT if msgs else "not found on this disc"))
    shop_emi = None if donor_ja else disc.find("SHOP.EMI")
    if (start_emi or battle_emi or shop_emi) and not args.only:
        commu = [] if is_psp(disc) else \
            [disc.read(p) for p in sorted(disc.files) if re.match(r"(.*/)?COMMU[0-9A-Z]*\.EMI$", p)]
        if is_psp(disc):
            labels, report = convert_labels_psp(src, boot, verb_starts(src, boot), module_sections(disc, "MAGIC060.EMI"),
                                                disc.text_end)
        else:
            status_emi = disc.find("STATUS.EMI")
            labels, report = convert_labels(src, read_module(start_emi) if start_emi else None,
                                            read_module(battle_emi) if battle_emi else None,
                                            read_module(shop_emi) if shop_emi else None, commu,
                                            module_sections(disc, "MAGIC060.EMI"), module_sections(disc, "BATE.EMI"),
                                            module_sections(disc, "BOSS025.EMI"), module_sections(disc, "SCENA10.EMI"),
                                            read_module(status_emi) if status_emi else None)
        overlays["FIRST.DAT"] += labels
        print("labels: " + (", ".join(report) if report else "not found on this disc"))
    elif donor_ja and not is_psp(disc) and not args.only:
        # A Japanese disc builds no labels but group 15: its panel's words are
        # the PC's own English, and its headings are the small words too.
        report = []
        overlays["FIRST.DAT"] += [code_label_chunks(module_sections(disc, "MAGIC060.EMI"), None, None, report)[15]]
        print("labels: " + ", ".join(report))

    game_emi = disc.find("GAME.EMI")
    if is_psp(disc) and not args.only:
        # The PSP reads its names out of BOOT.BIN; its GAME.EMI is the PSX's,
        # kept beside it and the same on both PSP discs (region-diff.md 5.4 P7).
        names, report = convert_names(src, disc.boot(), psp=True)
        overlays["FIRST.DAT"] += names
        print("names (BOOT.BIN): " + ", ".join(report))
    elif game_emi and not args.only:
        names, report = convert_names(src, emi_sections(disc.read(game_emi[0]))[0][1])
        overlays["FIRST.DAT"] += names
        print("names: " + ", ".join(report))
    print("enemy names: %d (%d kept)" % (enemies, kept_enemy))
    print("button codes: %d in the dialogue, by %s (docs/button-dialogue.md)" % (sum(button_counts.values()), how))
    for line in button_report(button_counts):
        print(line)
    if not args.only:
        sheets, left = build_village_sheet(src, disc)
        for name, chunks in sheets.items():
            overlays.setdefault(name, []).extend(chunks)
        print("village board sheet: " + (", ".join(sorted(sheets)) if sheets else "not repainted on this disc")
              + ("; not built disc-only (the PC's sheet is not in the cache): %s" % ", ".join(left) if left else ""))
        plates = build_plates(src, disc)
        for name, chunks in plates.items():
            overlays.setdefault(name, []).extend(chunks)
        print("place plates: %d world maps" % len(plates))
        if src.disc_only:
            standins, left = build_standins(src, disc, overlays)
            for name, chunks in standins.items():
                overlays.setdefault(name, []).extend(chunks)
            print("stand-ins: %d (%s)%s" % (sum(len(c) for c in standins.values()), ", ".join(
                "%s 0x%X%s" % (n, t, " re-laid" if t in RELAID else "")
                for n, cs in sorted(standins.items()) for _, t, _ in cs),
                "".join("; left out: %s 0x%X, %s" % e for e in left)))
        else:
            checked = check_relaid(src, disc)
            print("re-laid sheets: the rule is the port's on its pixels (%s)"
                  % ", ".join("%s 0x%X" % (n, t) for t, n in checked))
    if donor_ja and ja_pairs:
        overlays["FIRST.DAT"] = ja_pair_chunks(overlays["FIRST.DAT"])
        print("pair codes: %d from glyph 0x%X (DIV-0057)" % (len(ja_pairs), PAIR_AT))
    for name, chunks in overlays.items():
        write_overlay(src.out_path(args.lang, name), chunks)
    how = "left empty, disc-only" if src.disc_only else "kept as shipped"
    print("%d overlay files in %s: %d area texts (%d slots %s), %d system pools (%d %s)"
          % (len(overlays), os.path.dirname(src.out_path(args.lang, "FIRST.DAT")), texts, kept_text, how,
             pools, kept_pool, how if src.disc_only else "kept"))
    return len(overlays)


def cmd_anchors(args):
    """PC_SHA as the PC install's BOF3.exe has it, to paste; and whether the table in this file agrees."""
    check_game(args.game)
    game = PcInstall(args.game)
    for name, digest in anchor_table(game).items():
        print('    "%s": "%s",%s' % (name, digest, "" if PC_SHA.get(name) == digest else "   # differs from PC_SHA"))
    check_anchors(game)
    print("PC_SHA and FAERIE_UNNAMED: this BOF3.exe's")
    return 0


def cmd_export(args):
    img = export_image(donor_sheet(open_disc(args.disc)))
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    img.save(args.out)
    print("wrote %s: %d x %d, cells %d x %d; bring it back at up to 24 x 24 a cell (16 x 24 for 2x)"
          % (args.out, img.width, img.height, CELL_W, CELL_H))


def cmd_sheet(args):
    from PIL import Image
    rows = donor_sheet(open_disc(args.disc))
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


def selftest_empty_records():
    """Each of the four record converters, on a record holding an empty
    string, stops with its own message (a SystemExit naming the converter;
    the verbs' placement refuses an empty string before encoding it),
    not a NameError: the faerie branch of `label_chunk` was copied into them
    once with no `tag` bound (docs/loc-build-disc-only.md 7.6). The anchors
    are pinned to a synthetic blob of zeros; no game data."""
    global find_anchor, same_anchor
    saved = find_anchor, same_anchor
    reset_donor()

    class Disc:
        files = {"BIN/WORLD/WORLD00.EMI": None}

        def read(self, name):
            return bytes(64)

    verbs = bytes(64) + b"".join(struct.pack("<I", 0x80010000 + 4 * i) for i in range(VERB_COUNT))
    cases = (("verbs", lambda: convert_verbs(None, verbs)),     # an empty verb has no placement
             ("names", lambda: convert_char_names(None, bytes(CHAR_STRIDE * CHAR_COUNT + 16))),
             ("merchant", lambda: convert_merchant(None, Disc())),
             ("battle", lambda: convert_battle_commands(None, bytes(BATTLE_COUNT * BATTLE_ROOM + 16))))
    anchors = {"verb sets": len(verbs), "char tail 0": CHAR_NAME_US, "merchant": 0, "battle boxes": BATTLE_COUNT * BATTLE_ROOM}
    try:
        find_anchor = lambda game, blob, name, start=0, end=None: anchors[name]   # noqa: E731
        same_anchor = lambda game, name, got: True                                 # noqa: E731
        for what, run in cases:
            try:
                got = run()
            except SystemExit as e:
                if not str(e).startswith(what + ":"):
                    raise SystemExit("selftest: %s stopped with another converter's message: %s" % (what, e))
                print("  %-8s empty string: %s" % (what, e))
                continue
            raise SystemExit("selftest: %s converted an empty string: %r" % (what, got))
    finally:
        find_anchor, same_anchor = saved


def cmd_cover(args):
    """Which of the recipe's loc/zh-CN chunks a language layer replaces
    (docs/loc-build-disc-only.md 7.1): a chunk of the same kind and tag in the
    layer's container, the enemy names by any record of their table; a kind-1
    image that a later base slot of its container covers wholly needs none
    (build_standins). Counts by what, and every chunk left."""
    import collections
    import tomllib
    with open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "recipes", "pc-zh.toml"), "rb") as f:
        recipe = {r["name"]: r["chunks"] for r in tomllib.load(f)["file"]}
    layer = args.layer or os.path.join(args.cache, "loc", args.lang, "dat")
    have, left = collections.Counter(), []
    total = collections.Counter()
    for name, rows in sorted(recipe.items()):
        path = os.path.join(layer, name)
        got = set()
        if os.path.exists(path):
            blob, chunks = dat.load(path)
            got = {(c.kind, c.tag) for c in chunks}
        for slot, c in enumerate(rows):
            if c["layer"] == "loc/zh-CN":
                what = c.get("what", c["class"])
                hit = (c["kind"], c["tag"]) in got
                if not hit and c["kind"] == 1:
                    mine = vram_rect(c["tag"], c["size"])
                    hit = any(covers(vram_rect(d["tag"], d["size"]), mine)[0]
                              for d in rows[slot + 1:] if d["kind"] == 1)
                    what += (" (covered by a later slot)" if hit else "")
            elif c.get("names") == "loc/zh-CN":
                what = "enemy names"
                hit = any(k == 0 and c["tag"] < t < c["tag"] + c["size"] for k, t in got)
            else:
                continue
            total[what] += 1
            if hit:
                have[what] += 1
            else:
                left.append((name, c["kind"], c["tag"], what))
    print("%s: %d of %d loc/zh-CN chunks replaced" % (layer, sum(have.values()), sum(total.values())))
    for what in sorted(total):
        print("  %4d of %4d  %s" % (have[what], total[what], what))
    for name, kind, tag, what in left:
        print("  left: %-12s kind %d tag 0x%08X  %s" % (name, kind, tag, what))
    return 0


def selftest_rules():
    """The disc-only rules on synthetic bytes (no game data): a sheet's tiles
    and pixels round-trip; relay_sheet clears its rectangles and lays the
    strip from the sheet as it was; a block whose head slots point past its
    end gets its count from the first in-block offset, and those slots its
    first message."""
    import random
    rnd = random.Random(16)
    for tag in RELAID:
        body = bytes(rnd.randrange(256) for _ in range(0x8000))
        if sheet_body(tag, sheet_pixels(tag, body)) != body:
            raise SystemExit("selftest: sheet 0x%X does not round-trip" % tag)
        src, got = sheet_pixels(tag, body), sheet_pixels(tag, relay_sheet(tag, body))
        rule = RELAID[tag]
        count, (sx, sy), (dx, dy) = rule["strip"]
        strip = {(dx + STRIP_PITCH * i + x, dy + y): src[sy + y][sx + ICON_PITCH * i + x]
                 for i in range(count) for y in range(ICON) for x in range(ICON)}
        region = relaid_region(tag)
        for y, row in enumerate(got):
            for x, v in enumerate(row):
                want = strip.get((x, y), 0 if (x, y) in region else src[y][x])
                if v != want:
                    raise SystemExit("selftest: relay_sheet 0x%X pixel (%d, %d)" % (tag, x, y))
    print("  re-laid sheets: %s round-trip and re-lay" % ", ".join("0x%X" % t for t in RELAID))
    msgs = [b"A\0", b"BC\0", b"\0"]
    n = 6
    offs, body = [], bytearray()
    for m in msgs:
        offs.append(2 * n + len(body))
        body += m
    head = [0x3F3D, 0x3F3D]                      # two slots past the end
    table = head + offs + [offs[-1]]
    block = struct.pack("<%dH" % n, *table) + bytes(body)
    if table_after_bad_head(block) != n:
        raise SystemExit("selftest: table_after_bad_head gave %d, wanted %d" % (table_after_bad_head(block), n))
    reset_donor()
    out, _ = convert_block(block, None, TEXT_ROOM)
    o = struct.unpack_from("<%dH" % n, out)
    if o[0] != o[1] or o[1] != o[2] or out[o[0]:out.index(b"\0", o[0])] != b"A":
        raise SystemExit("selftest: the bad head's slots do not point at the first message")
    print("  a slot table with a bad head: %d slots, the head at the first message" % n)


def selftest_psp():
    """The PSP rules on synthetic bytes (no game data): psp_emi undoes the
    header changes; words_after, slots_before and table_run read packed
    strings; village_run finds the one run of twenty 8-byte slots."""
    global donor_pointers
    head = bytearray(0x800)
    struct.pack_into("<I", head, 0, 3)
    head[8:16] = b"MATH_TBL"
    for i, (dest, kind) in enumerate(((0x10000, 0), (0x1C080201, 3), (1, 6))):
        struct.pack_into("<IIIH", head, 0x10 + 16 * i, 0, dest, 0, kind)
    got = [d for d, _ in emi_sections(psp_emi(bytes(head)))]
    if got != [0x80010000, 0x1C080200, 1]:
        raise SystemExit("selftest: psp_emi gave %s" % [hex(d) for d in got])
    blob = bytearray(b"\x02\x14\x16\0\0" + b"ABCD\0EF\0" + b"GH\0\0IJ\0\0" + b"\0" * 3)
    if words_after(bytes(blob), 0, 2) != [b"ABCD", b"EF"] or slots_before(bytes(blob), 21, 4, 2) != [b"GH", b"IJ"]:
        raise SystemExit("selftest: words_after / slots_before")
    base = 0x1000
    strings = b"ONE\0TWO\0THREE\0"
    starts = [0, 4, 8]
    pad = (-(base + len(strings))) % 4
    image = bytearray(base) + strings + b"\0" * pad
    image += struct.pack("<3I", *(base + s for s in starts))
    saved, donor_pointers = donor_pointers, (base, len(image) + 0x100)
    try:
        found = table_run(bytes(image), base, 3)
    finally:
        donor_pointers = saved
    if not found or found[0] != [b"ONE", b"TWO", b"THREE"]:
        raise SystemExit("selftest: table_run gave %s" % (found,))
    words = b"".join((b"W%d" % i).ljust(8, b"\0") for i in range(20))
    village = b"\x01\x02" + words + b"\x05\x02"
    if village_run(village) != [2]:
        raise SystemExit("selftest: village_run gave %s" % village_run(village))
    print("  PSP: section table, packed words and slots, a pointer-named run, the village run")


def selftest_config_psp():
    """The PSP Config screen (D246) on synthetic bytes (no game data): two
    labels built on the routine's stack and four in .data, fifteen records
    before the PSP's row tables and six controller records after them; the
    PC's six labels and seventeen records take the PSP's where it has them and
    ours (CONFIG_BUILTIN_*) where it has not, the Sound row at the count and x
    of the Autorun row."""
    a0, a1, zero, sp, ra = 4, 5, 0, 29, 31
    ori = lambda rt, imm: (0x0D << 26) | (rt << 16) | imm
    sb = lambda rt, off: (0x28 << 26) | (sp << 21) | (rt << 16) | off
    lui = lambda rt, imm: (0x0F << 26) | (rt << 16) | imm
    addiu = lambda rt, rs, imm: (0x09 << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)
    jal = lambda target: (0x03 << 26) | ((target >> 2) & 0x3FFFFFF)
    text_end = 0x1000
    data = bytearray(b"\0\0\x01\0\x02\0\x03\0\x04\0\x05\0")
    starts = []
    for word in (b"Ww", b"Bb", b"Kk\xffk", b"Dd"):
        starts.append(text_end + len(data))
        data += word + b"\0"
    recs = [(i + 1, 0x10 + i, b"O%d" % i) for i in range(PSP_CONFIG_RECORDS)]
    for count, x, word in recs:
        data += bytes([count, x]) + word.ljust(6, b"\0")
    data += PSP_CONFIG_TABLES
    ctrls = [b"C" * (i + 2) for i in range(CONFIG_CTRL)]
    for word in ctrls:
        data += bytes([len(word)]) + word.ljust(CTRL_STRIDE - 1, b"\0")

    def load(va):
        return [lui(a1, (va + 0x8000) >> 16), jal(0x800), addiu(a1, a1, va & 0xFFFF)]

    code = [0x27BDFF80]                                 # addiu $sp, $sp, -0x80
    code += [ori(a0, 0x41), sb(a0, 0x20), ori(a0, 0x61), sb(a0, 0x21), sb(zero, 0x22)]
    code += load(starts[0]) + load(starts[1])
    code += [ori(a1, 0x45), sb(a1, 0x30), ori(a1, 0x65), sb(a1, 0x31), sb(zero, 0x32)]
    code += load(starts[2]) + load(starts[3]) + [(ra << 21) | 0x08, 0]
    boot = bytearray(0x100) + struct.pack("<%dI" % len(code), *code)
    boot += bytearray(text_end - len(boot)) + data
    got, source = convert_config_psp(bytes(boot), text_end)
    options = [recs[i] for i in range(11)] + [(recs[11][0], recs[11][1], CONFIG_BUILTIN_OPTIONS[11]),
                                               (recs[12][0], recs[12][1], CONFIG_BUILTIN_OPTIONS[12])] + recs[11:]
    want = config_payload([b"Aa", b"Ww", b"Bb", CONFIG_BUILTIN_LABELS[3], b"Ee", CONFIG_BUILTIN_LABELS[5]],
                          options, ctrls)
    ours = sorted(k for k, v in source.items() if v == "ours")
    if got != [(KIND_CONFIG, 0, want)] or ours != ["label 3", "label 5", "option 11", "option 12"]:
        raise SystemExit("selftest: convert_config_psp gave %s, ours %s" % (got, ours))
    print("  PSP Config: the stack labels, the .data labels, records and controller names; ours in the four slots it lacks")


def selftest_code_labels():
    """The MIPS readers of groups 15, 17 and 18 on synthetic code (no game
    data): each finds its words by the calls' constants, and the guest
    reader refuses a copy with no sound two calls before it."""
    base, a0, a1, a2, v0, zero, ra = 0x80010000, 4, 5, 6, 2, 0, 31
    lui = lambda rt, imm: (0x0F << 26) | (rt << 16) | imm
    addiu = lambda rt, rs, imm: (0x09 << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)
    sltiu = lambda rt, rs, imm: (0x0B << 26) | (rs << 21) | (rt << 16) | imm
    beq = lambda rs, rt, off: (0x04 << 26) | (rs << 21) | (rt << 16) | off
    jal = lambda target: (0x03 << 26) | ((target >> 2) & 0x3FFFFFF)
    jr_ra = (ra << 21) | 0x08
    strings = {}
    pool = bytearray()

    def string(s):
        if s not in strings:
            strings[s] = base + 0x1000 + len(pool)
            pool.extend(s + b"\0" * (4 - len(s) % 4))
        return strings[s]

    def load(rt, s):
        a = string(s)
        return [lui(rt, (a + 0x8000) >> 16), addiu(rt, rt, a & 0xFFFF)]

    def section(code):
        blob = bytearray(struct.pack("<%dI" % len(code), *code))
        blob += b"\0" * (0x1000 - len(blob)) + pool
        return [(base, bytes(blob))]

    code = []
    for word, (x, y) in zip((b"AAAA", b"BBBBB", b"CCCC", b"DDD"), IDENTIFY_PLACES):
        code += load(a0, word) + [addiu(a1, zero, x), jal(base + 0x800), addiu(a2, zero, y)]
    if identify_words(section(code)) != [b"AAAA", b"BBBBB", b"CCCC", b"DDD"]:
        raise SystemExit("selftest: identify_words gave %s" % identify_words(section(code)))
    copy = load(a1, b"Ee") + [jal(base + 0x900), addiu(a2, zero, GUEST_COPY)]
    code = [jal(base + 0x800), addiu(a0, zero, GUEST_SOUND), jal(base + 0x880), 0] + copy
    if guest_name(section(code)) != b"Ee" or guest_name(section([jal(base + 0x880), 0] * 2 + copy)) is not None:
        raise SystemExit("selftest: guest_name")
    code = ([addiu(a0, zero, TURN_BOX[0]), jal(base + 0x800), addiu(a1, zero, TURN_BOX[1])]
            + load(a1, b"%2d") + [jal(base + 0x880), 0, addiu(a0, zero, 0x94), jal(base + 0x900), addiu(a1, zero, TURN_ROW),
                                  sltiu(v0, v0, 0x14), beq(v0, zero, 3), 0]
            + load(v0, b"Ff\xffgg") + load(v0, b"Ff\xffg")
            + [addiu(a0, zero, 0x70), jal(base + 0x980), addiu(a1, zero, TURN_ROW), jr_ra, 0])
    want = (b"Ff\xffgg", b"Ff\xffg", bytes([0x94, 0x70]) + b">" + bytes([0x14]))
    if turn_counter(section(code)) != want:
        raise SystemExit("selftest: turn_counter gave %s" % (turn_counter(section(code)),))
    # Group 20: a "%s" print of a 12-byte title with no NUL (a pointer after
    # it), then a draw of 12; a second print, so two, is refused.
    a3 = 7
    fmt = string(b"%s")
    title_at = base + 0x1000 + len(pool)
    pool.extend(b"Hhhhhh\xffiiiii" + struct.pack("<I", base))
    printed = [lui(a1, (fmt + 0x8000) >> 16), addiu(a1, a1, fmt & 0xFFFF),
               lui(a2, (title_at + 0x8000) >> 16), jal(base + 0x800), addiu(a2, a2, title_at & 0xFFFF),
               addiu(a3, zero, 12), jal(base + 0x880), 0]
    if parts_title(section(printed), section(printed)) != b"Hhhhhh\xffiiiii" \
            or parts_title(section(printed * 2), section(printed * 2)) is not None:
        raise SystemExit("selftest: parts_title")
    # D286's headings: four lui / addiu of an address past the tabs, each
    # beside a count in $a3 and its row in $a1 (7, 7, 7, 0x47); a fourth row
    # of 7 is refused.
    t0, s6 = 8, 22
    after = base + 0x1000 + len(pool)
    pool.extend(b"Jjjjj\0\0\0Kkkk\0\0\0\0Lllllll" + b"Mmmmmm\0\0")
    def draw(at, n, y):
        return [lui(v0, (at + 0x8000) >> 16), addiu(v0, v0, at & 0xFFFF), addiu(a1, s6, y), 0,
                addiu(a3, zero, n), addiu(a0, t0, 0x30), jal(base + 0x800), 0] + [0] * 12
    rows = [(0, 5, 7), (8, 4, 7), (16, 7, 7), (23, 6, 0x47)]
    blob = section(sum((draw(after + o, n, y) for o, n, y in rows), []))[0][1]
    if fishing_headings(blob, base, after) != [b"Jjjjj", b"Kkkk", b"Lllllll", b"Mmmmmm"]:
        raise SystemExit("selftest: fishing_headings gave %s" % (fishing_headings(blob, base, after),))
    blob = section(sum((draw(after + o, n, 7) for o, n, y in rows), []))[0][1]
    if fishing_headings(blob, base, after) is not None:
        raise SystemExit("selftest: fishing_headings took four rows of 7")
    print("  labels from code: the identify words, the guest name (and a copy with no sound refused), the turn counter,"
          " the parts title (two prints refused), the fishing headings (the wrong rows refused)")


def selftest_buttons():
    """The dialogue's button codes on synthetic bytes (no game data): each
    release's icon becomes the code of Japan's bit by its map, the glyph kept
    after it; PSP-EU's areas 68 and 121 go by P; a kept PC message by its
    colour; nothing outside the button areas or without the icon shape; a
    donor that already uses the codes' argument range is refused."""
    def one(donor_msg, base_msg, area, rmap, psp_eu=False, ja=False):
        global donor_ja
        reset_donor()
        donor_ja = ja
        donor = struct.pack("<H", 2) + donor_msg
        base = struct.pack("<H", 2) + base_msg
        set_button_area("AREA%03d.DAT" % area, rmap, psp_eu)
        try:
            out, _ = convert_block(donor, base, TEXT_ROOM)
        finally:
            set_button_area("", None, False)
            donor_ja = False
        return out[2:]
    icon = lambda c: encode_char(c)
    for code, want in ((0x81, CIRCLE), (0x82, CROSS), (0x83, TRIANGLE), (0x80, SQUARE)):   # P^-1
        got = one(b"\x05\x01" + bytes([code]) + b"\x06\0", b"\0", 30, MAP_P)
        if got != button_code(want) + b"\x05\x01" + icon(code) + b"\x06\0":
            raise SystemExit("selftest: P icon 0x%X gave %s" % (code, got.hex()))
    for code, want in ((0x81, CIRCLE), (0x80, CROSS), (0x83, TRIANGLE), (0x82, SQUARE)):   # S^-1
        got = one(b"\x05\x01" + bytes([code]) + b"\0", b"\0", 8, MAP_S, psp_eu=True)
        if got != button_code(want) + b"\x05\x01" + icon(code) + b"\0":
            raise SystemExit("selftest: S icon 0x%X gave %s" % (code, got.hex()))
    for area in PSP_EU_BY_P:
        got = one(b"\x05\x02\x80\0", b"\0", area, MAP_S, psp_eu=True)
        if got != button_code(SQUARE) + b"\x05\x02" + icon(0x80) + b"\0":
            raise SystemExit("selftest: PSP-EU area %d is not by P" % area)
    got = one(b"\x05\x02\x15\x08\0", b"\0", 75, MAP_ID, ja=True)
    if got[:5] != button_code(CIRCLE) + b"\x05\x02":
        raise SystemExit("selftest: a Japanese icon gave %s" % got.hex())
    for area in (78, 175, 1):
        if one(b"\x05\x01\x81\0", b"\0", area, MAP_P) != b"\x05\x01" + icon(0x81) + b"\0":
            raise SystemExit("selftest: area %d's icon-shaped text got a code" % area)
    if one(b"\x13\x01\0", b"\x05\x02d\x86\x19\x06\0", 1, MAP_P) != b"\x05\x02d\x86\x19\x06\0":
        raise SystemExit("selftest: area 1's place name got a code")
    if one(b"\x05\x01A\x81\0", b"\0", 30, MAP_P) != b"\x05\x01" + icon(0x41) + icon(0x81) + b"\0":
        raise SystemExit("selftest: an icon not right after the colour got a code")
    # A donor message that does not convert keeps the PC's, coded by its colour.
    for colour, glyph, want in ((2, b"b", CIRCLE), (1, b"X", CROSS), (2, b"d", CIRCLE), (5, b"d", SQUARE)):
        pc = b"\x05" + bytes([colour]) + glyph + b"\x06\x81@\0"
        got = one(b"\x05\x01\x13\x01\0", pc, 121, MAP_P)
        if got != button_code(want) + b"\x05" + bytes([colour]) + glyph + b"\x06\x81@\0":
            raise SystemExit("selftest: the PC's colour %d gave %s" % (colour, got.hex()))
    if one(b"\x13\x01\0", b"\x05\x03b\0", 30, MAP_P) != b"\x05\x03b\0":
        raise SystemExit("selftest: a PC glyph in a colour that is no button's got a code")
    try:
        one(b"\x05\xB5\x06\0", b"\0", 30, MAP_P)
    except SystemExit as e:
        if "05 B5" not in str(e):
            raise
    else:
        raise SystemExit("selftest: a donor holding the colour 0xB5 was not refused")
    if code_bit(button_code(SQUARE)[1]) != SQUARE or code_bit(0x07) is not None:
        raise SystemExit("selftest: code_bit")
    # The census reader finds what the converter wrote, and nothing in a plain block.
    block = struct.pack("<H", 2) + button_code(CIRCLE) + b"\x05\x01" + icon(0x81) + b"\x05\x02bA" + \
        button_code(CROSS) + b"A\0"
    got = [(bit, colour, glyph) for _, bit, glyph, colour in text_buttons(block, pc_text=True)]
    if got != [(CIRCLE, 1, icon(0x81)), (None, 2, b"b"), (CROSS, 0, b"")]:
        raise SystemExit("selftest: text_buttons gave %s" % got)
    if [bit for _, bit, _, _ in text_buttons(block)] != [CIRCLE, CROSS]:
        raise SystemExit("selftest: text_buttons read a layer's letter as an icon")
    reset_donor()
    print("  button codes: P, S (and PSP-EU's 68 / 121 by P), JP, the PC's colours; areas 78, 175 and 1 left; "
          "the argument range refused")


def cmd_selftest(args):
    selftest_empty_records()
    selftest_rules()
    selftest_psp()
    selftest_config_psp()
    selftest_code_labels()
    selftest_buttons()
    print("loc_build selftest: ok")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name, fn in (("all", cmd_all), ("sheet", cmd_sheet), ("export", cmd_export)):
        s = sub.add_parser(name)
        if name == "all":
            s.add_argument("--disc", action="append", default=[], help="a disc image (.cue, .bin, .iso); repeat for several")
            s.add_argument("--discs", help="a directory: every held PlayStation disc in it is built, each under its own tag")
            s.add_argument("--dry-run", action="store_true", help="identify each image (and check the game) and write nothing")
        else:
            s.add_argument("--disc", required=True)
        s.add_argument("--lang", type=language_tag,
                       help="the overlays' language tag, BCP 47 (en-US, en-150, fr-FR, de-DE, ja-JP); "
                            "default: the disc's own tag from fixtures.toml, which needs the disc to be a held build")
        if name == "all":
            s.add_argument("--glyphs", help="an upscaled sheet to use instead of doubling the donor's cells")
            s.add_argument("--upscaler", help="a command that upscales {in} by {scale} (to {out}, or to one new PNG)")
            s.add_argument("--mono", action="store_true", help="every glyph advances 8, as the US release; default tightens ' and ,")
            s.add_argument("--pc-white", action="store_true", help="keep the port's brightened white text palette; default restores the disc's (DIV-0013)")
        if name in ("sheet", "export"):
            s.add_argument("--out", required=True)
        else:
            s.add_argument("--game", help="the PC install (BOF3.exe and DAT/): overlays go into its DAT/ as <tag>.<NAME>.DAT")
            s.add_argument("--cache", help="instead of --game: an importer cache built from a disc alone (base/dat/, "
                                           "base/exe/); overlays go to its loc/<tag>/dat/ (docs/loc-build-disc-only.md)")
        if name == "all":
            s.add_argument("--only")
        s.set_defaults(fn=fn)
    s = sub.add_parser("cover", help="which loc/zh-CN chunks a cache's language layer replaces")
    s.add_argument("--cache", required=True)
    s.add_argument("--lang", required=True)
    s.add_argument("--layer", help="the layer's dat/ directory, if not the cache's loc/<lang>/dat")
    s.set_defaults(fn=cmd_cover)
    s = sub.add_parser("buttons", help="count the button codes (and icons left without one) in a DAT directory's area text")
    s.add_argument("--dat", required=True, help="a DAT directory: an install's DAT/ or a cache's loc/<tag>/dat/")
    s.add_argument("--lang", help="read the <tag>.AREAnnn.DAT overlays beside the originals; without it the "
                                  "directory's own AREAnnn.DAT (the PC's text, or a cache's layer)")
    s.add_argument("--pc-text", action="store_true", help="the text is the PC's own Chinese: also count the "
                                                          "port's colour-cued icons that carry no code")
    s.set_defaults(fn=cmd_buttons)
    s = sub.add_parser("selftest", help="the converters' synthetic checks (no game data)")
    s.set_defaults(fn=cmd_selftest)
    s = sub.add_parser("anchors", help="print PC_SHA's entries anew from a PC install's BOF3.exe")
    s.add_argument("--game", required=True)
    s.set_defaults(fn=cmd_anchors)
    args = ap.parse_args()
    if args.cmd in ("anchors", "selftest", "cover", "buttons"):
        return args.fn(args)
    args.lang_given = args.lang is not None
    if args.cmd == "all" and not args.disc and not args.discs:
        ap.error("all needs --disc DISC or --discs DIR")
    if args.cmd != "all" and args.lang is None:
        args.lang = disc_tag(args.disc)
        print("--lang not given: the disc's own tag, %s" % args.lang)
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
