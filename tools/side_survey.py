#!/usr/bin/env python
"""Survey the area blocks' side-face texture words (docs/known-defects.md D239).

    python tools/side_survey.py --dat bof3/DAT [--only AREA041] [--cells AREA041:X0,Z0,X1,Z1]
    python tools/side_survey.py --disc "CDImage/Breath of Fire III (USA).cue" [...]

The question D239 leaves: `MapView_CellTextures` `0x56F9B0` reads a cell's
texture words as the cell's own, then the south (`+0x8E`) side's if that
side item exists, then the east (`+0x7E`) side's. The sides exist by a
corner-height test at the moment the cell's draw item is created
(`MapView_Build`). Is the map authored own, south, east (the code's walk is
right; a deck cell that gets both sides at run time is simply unforeseen
data), or own, east, south (the walk is wrong whenever both exist)?

For every area block (PC: each DAT's kind-0 chunk of tag 0xC8000, the
block at `AreaMap_Header` `0x8CB580`; PSX: each EMI section bound for
0x80104000) and every cell with a tile, this computes from the block alone:

  - the sides the code would allocate from the corner heights as loaded
    (src/game/map_layers.cpp BuildCell's two tests, read at the same
    addresses, the row's last cell and the last row included);
  - the tile's run of texture dwords: from its own index to the next index
    any cell of the block uses (or the textures' end, the header's u16 +4);
  - each word decoded as Prim_SetTexture decodes it (src/game/prim.cpp): its
    source (grid / rect / quad / zero), and the texel lengths of the face's
    vertical edge (corner 0 to corner 2) and horizontal edge (0 to 1);
  - each side's drop: the most its two near corners stand above the
    neighbour's, in corner units (a unit is 16 world units, a cell 128; the
    grid's 16-texel tile spans a cell, so one unit is two texels).

Then three tables: side sets against run lengths (does a run hold exactly
1 + the sides the code allocates?); single-sided cells' second word,
texels against drop (the baseline of how a side's texture fits its drop);
and the two-sided cells with three words under both orders - which order
makes each word fit the drop it would be drawn over, as the single-sided
cells' words fit theirs.

What the block alone cannot say: heights moved at run time (area 41's sky
effect rewrites the deck's corners every frame, D239), so a cell's static
side set is the authored one, not necessarily the one a frame allocates.
`--cells` prints a rectangle cell by cell for that reading.

Output is counts, coordinates, word kinds and texel spans; `--cells` adds
the words in hex. Derived from game data: keep it under analysis/ (CLAUDE.md
rule 1).
"""
import argparse
import collections
import glob
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dat  # noqa: E402

PC_TAG = 0xC8000          # LoadDatFile's arena offset of AreaMap_Header
PSX_DEST = 0x80104000     # the same block's EMI destination (region_read.py)
CORNERS = 0x30            # AreaMap_Corners 0x8CB5B0 - AreaMap_Header


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0] if 0 <= o and o + 2 <= len(b) else None


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0] if 0 <= o and o + 4 <= len(b) else None


def s8(v):
    return v - 256 if v >= 128 else v


# ---------------------------------------------------------------- one word

def corners_uv(block, word):
    """The four (u, v) Prim_SetTexture writes for `word`, or None when its
    table read leaves the block."""
    page = (word >> 28) & 3
    if word & 0xF00 == 0:
        u0, v0 = (word & 0xF) << 4, word & 0xF0
        u = [u0, u0 + 15, u0, u0 + 15]
        v = [v0, v0, v0 + 15, v0 + 15]
    elif word & 0x800:
        base = u16(block, 6 + 4 * page)
        if base is None:
            return None
        at = (base + (word & 0x7FF) * 2) * 4
        a, b = u32(block, at), u32(block, at + 4)
        if a is None or b is None:
            return None
        u = [s8(a >> 24), (a >> 8) & 0xFF, s8(b >> 24), (b >> 8) & 0xFF]
        v = [(a >> 16) & 0xFF, a & 0xFF, (b >> 16) & 0xFF, b & 0xFF]
    else:
        base = u16(block, 4 + 4 * page)
        if base is None:
            return None
        c = u32(block, (base + (word & 0xFF)) * 4)
        if c is None:
            return None
        u0, v0 = s8(c >> 24), (c >> 16) & 0xFF
        u = [u0, u0 + ((c >> 8) & 0xFF), u0, u0 + ((c >> 8) & 0xFF)]
        v = [v0, v0, v0 + (c & 0xFF), v0 + (c & 0xFF)]
    if word & 0x40000:                      # the quarter turn: 0..3 take 2, 0, 3, 1
        u = [u[2], u[0], u[3], u[1]]
        v = [v[2], v[0], v[3], v[1]]
    if word & 0x10000:
        u = [u[1], u[0], u[3], u[2]]
        v = [v[1], v[0], v[3], v[2]]
    if word & 0x20000:
        u = [u[2], u[3], u[0], u[1]]
        v = [v[2], v[3], v[0], v[1]]
    return u, v


Word = collections.namedtuple("Word", "raw kind tall wide")


def signature(wd):
    """What a face's word says beside its shape: source, shade (bits 19..23)
    and the turn / mirrors (16..18) - a side lit or turned by its facing
    would show here."""
    if wd is None:
        return "past end"
    if wd.kind == "zero":
        return "zero"
    return "%s shade %02X turn %d" % (wd.kind, (wd.raw >> 16) & 0xF8, (wd.raw >> 16) & 7)


def decode(block, word):
    if word == 0:
        return Word(0, "zero", 0, 0)
    kind = "grid" if word & 0xF00 == 0 else ("quad" if word & 0x800 else "rect")
    uv = corners_uv(block, word)
    if uv is None:
        return Word(word, kind + "?", None, None)
    u, v = uv
    # A side face's vertices 0 and 1 are its top edge, 2 and 3 its foot
    # (map_layers.cpp: both sides copy the quad's top corners to +8 / +0x18
    # and put the neighbour's lower corners at +0x28 / +0x38).
    tall = max(abs(u[2] - u[0]), abs(v[2] - v[0])) + 1
    wide = max(abs(u[1] - u[0]), abs(v[1] - v[0])) + 1
    return Word(word, kind, tall, wide)


# ---------------------------------------------------------------- one block

Cell = collections.namedtuple("Cell", "x z tile south east drop_s drop_e run words edge")


def survey_block(block):
    w, d = block[0], block[1]
    base = u16(block, 2)
    if not w or not d or base is None:
        return None, "empty header"
    n = w * d
    tiles_at = 4 * base
    tex_at = 4 * (base + (n + 1) // 2)
    tex_end = 4 * (u16(block, 4) or 0)
    if not tex_at <= tex_end <= len(block):
        tex_end = len(block)
    tile = [u16(block, tiles_at + 2 * i) or 0 for i in range(n)]
    used = sorted({t for t in tile if t})
    nxt = {t: (used[k + 1] if k + 1 < len(used) else (tex_end - tex_at) // 4) for k, t in enumerate(used)}

    def corner(cx, cz, k):
        # As BuildCell reads it: byte k of the dword at cell (cz * w + cx),
        # a row's last cell's "next" being the next row's first.
        o = CORNERS + 4 * (cz * w + cx) + k
        return s8(block[o]) if o < len(block) else None

    cells = []
    for i, t in enumerate(tile):
        if not t:
            continue
        cx, cz = i % w, i // w
        own = [corner(cx, cz, k) for k in range(4)]
        nx = [corner(cx, cz, 4 + k) for k in range(4)]                  # corner + 4: the next cell
        bl = [corner(cx, cz + 1, k) for k in range(4)]                  # corner + w * 4: the row below
        edge = cx == w - 1 or cz == d - 1 or None in nx or None in bl
        east = None not in nx and (nx[2] < own[3] or nx[0] < own[1])
        south = None not in bl and (bl[1] < own[3] or bl[0] < own[2])
        drop_e = max(own[3] - nx[2], own[1] - nx[0]) if east else 0
        drop_s = max(own[3] - bl[1], own[2] - bl[0]) if south else 0
        run = nxt[t] - t
        words = []                                       # the three the code can read
        for k in range(3):
            raw = u32(block, tex_at + 4 * (t + k))
            words.append(decode(block, raw) if raw is not None else None)
        cells.append(Cell(cx, cz, t, south, east, drop_s, drop_e, run, words, edge))
    return dict(w=w, d=d, cells=cells, tiles=len(used)), None


# ---------------------------------------------------------------- the sources

def blocks_from_dat(path, only):
    for p in sorted(glob.glob(os.path.join(path, "*.DAT"))):
        name = os.path.basename(p)
        if name.lower().startswith(("en.", "fr.", "de.", "ja.")):
            continue                                   # language overlays (loc_build.py)
        if only and not any(name.upper().startswith(o.upper()) for o in only):
            continue
        blob, chunks = dat.load(p)
        for c in chunks:
            if c.kind == 0 and c.tag == PC_TAG:
                yield name.rsplit(".", 1)[0], blob[c.offset:c.offset + c.size]


def blocks_from_disc(path, only):
    import region_diff
    build = region_diff.Build("disc", path)
    for key in sorted(build.emis):
        stem = key.rsplit("/", 1)[-1].rsplit(".", 1)[0]
        if only and not any(stem.upper().startswith(o.upper()) for o in only):
            continue
        try:
            secs = build.sections(key)
        except ValueError:
            continue
        for _, _, dest, data in secs:
            if dest == PSX_DEST:
                yield stem, data


# ---------------------------------------------------------------- the report

def sides_name(c):
    return {(False, False): "none", (True, False): "S", (False, True): "E", (True, True): "SE"}[(c.south, c.east)]


def fit(word, drop):
    """log2 of the word's vertical texels over two a drop unit. Only its
    spread matters: the single-sided cells' median calibrates it."""
    if word is None or word.tall in (None, 0) or drop <= 0:
        return None
    return math.log2(word.tall / (2.0 * drop))


def median(vals):
    v = sorted(x for x in vals if x is not None)
    return v[len(v) // 2] if v else None


def summary(vals, centre):
    v = [x for x in vals if x is not None]
    if not v:
        return "n 0"
    good = sum(1 for x in v if abs(x - centre) <= 0.5)
    return "n %d, median %+.2f, within x1.4 of the baseline %d (%d%%)" % (len(v), median(v), good, 100 * good // len(v))


def report(areas, show_edges):
    total = collections.Counter()
    runs = collections.defaultdict(collections.Counter)
    sigs = collections.defaultdict(collections.Counter)
    single = collections.defaultdict(list)
    pairs = []                                   # SE cells with three words in their run
    per_area = []
    for name, info in areas:
        cs = [c for c in info["cells"] if show_edges or not c.edge]
        a_sides = collections.Counter(sides_name(c) for c in cs)
        a_short = sum(1 for c in cs if c.run < 1 + c.south + c.east)
        a_long = sum(1 for c in cs if c.run > 1 + c.south + c.east)
        per_area.append((name, info["w"], info["d"], len(cs), info["tiles"], a_sides, a_short, a_long))
        for c in cs:
            s = sides_name(c)
            total[s] += 1
            runs[s][min(c.run, 5)] += 1
            for k in (2, 3):
                if c.run >= k:
                    sigs[(s, k)][signature(c.words[k - 1])] += 1
            if s == "S" and c.run >= 2:
                single["S"].append(fit(c.words[1], c.drop_s))
            if s == "E" and c.run >= 2:
                single["E"].append(fit(c.words[1], c.drop_e))
            if s == "SE" and c.run >= 3:
                pairs.append((name, c))

    print("== per area: map w x d, cells with a tile, distinct tiles, side sets; runs short of / longer than 1 + sides")
    for name, w, d, n, t, sd, short, long_ in per_area:
        print("  %-10s %3d x %-3d cells %5d tiles %4d  none %5d S %4d E %4d SE %4d  short %4d long %4d"
              % (name, w, d, n, t, sd["none"], sd["S"], sd["E"], sd["SE"], short, long_))
    print()
    print("== side sets against the tile's run length (dwords; 5 = five or more)%s"
          % ("" if show_edges else ", the map's last row and column left out (--edges keeps them)"))
    for s in ("none", "S", "E", "SE"):
        print("  %-4s %6d  %s" % (s, total[s], "  ".join("run %d: %d" % (k, runs[s][k]) for k in sorted(runs[s]))))
    print()
    print("== words 2 and 3 inside the run, by signature (the eight most common), per side set")
    for s in ("none", "S", "E", "SE"):
        for k in (2, 3):
            if sigs[(s, k)]:
                print("  %-4s word %d (%d): %s" % (s, k, sum(sigs[(s, k)].values()),
                                                  "; ".join("%s x%d" % kv for kv in sigs[(s, k)].most_common(8))))
    print()

    # Two independent votes on the two-sided cells, each scored against what
    # the single-sided cells (whose one side word is unambiguous) look like.
    centre = median(single["S"] + single["E"])
    print("== single-sided cells, word 2's vertical texels against its drop (log2 of texels / (2 x drop))")
    for s in ("S", "E"):
        print("  %-2s %s" % (s, summary(single[s], centre if centre is not None else 0.0)))
    print()
    print("== the %d two-sided cells with three words in their run, under each order" % len(pairs))
    if not pairs:
        return
    if centre is not None:
        fa, fb, votes = [], [], collections.Counter()
        for _, c in pairs:
            a = (fit(c.words[1], c.drop_s), fit(c.words[2], c.drop_e))
            b = (fit(c.words[1], c.drop_e), fit(c.words[2], c.drop_s))
            fa.extend(a)
            fb.extend(b)
            if c.drop_s == c.drop_e:
                votes["equal drops, no vote"] += 1
            elif None in a or None in b:
                votes["no shape, no vote"] += 1
            else:
                ea = abs(a[0] - centre) + abs(a[1] - centre)
                eb = abs(b[0] - centre) + abs(b[1] - centre)
                votes["own,S,E" if ea < eb else "own,E,S" if eb < ea else "tie"] += 1
        print("  by shape against drop (baseline %+.2f from the single-sided cells):" % centre)
        print("    own,S,E  %s" % summary(fa, centre))
        print("    own,E,S  %s" % summary(fb, centre))
        print("    per cell, the order with the smaller misfit: %s" % dict(votes))
    ps, pe = sigs[("S", 2)], sigs[("E", 2)]
    if ps and pe:
        keys = set(ps) | set(pe)
        def p(table, key):
            return (table[key] + 1.0) / (sum(table.values()) + len(keys) + 1.0)
        votes = collections.Counter()
        for _, c in pairs:
            g2, g3 = signature(c.words[1]), signature(c.words[2])
            la = math.log(p(ps, g2)) + math.log(p(pe, g3))
            lb = math.log(p(pe, g2)) + math.log(p(ps, g3))
            votes["own,S,E" if la > lb + 1e-9 else "own,E,S" if lb > la + 1e-9 else "tie"] += 1
        print("  by signature, each word scored by how often single-sided S and E words carry it: %s" % dict(votes))
    print("  the cells, by area: %s" % dict(collections.Counter(n for n, _ in pairs)))


def detail(name, info, rect):
    x0, z0, x1, z1 = rect
    print("== %s cells x %d..%d, z %d..%d: sides, drops (units), run, words (kind tall x wide)" % (name, x0, x1, z0, z1))
    for c in info["cells"]:
        if x0 <= c.x <= x1 and z0 <= c.z <= z1:
            ws = "  ".join("%08X %s %sx%s" % (wd.raw, wd.kind, wd.tall, wd.wide) if wd else "-" for wd in c.words)
            print("  (%2d, %2d) tile %4d %-4s S %d E %d run %d%s  %s"
                  % (c.x, c.z, c.tile, sides_name(c), c.drop_s, c.drop_e, c.run, " edge" if c.edge else "", ws))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--dat", help="the PC install's DAT directory")
    src.add_argument("--disc", help="a PSX disc (.cue, .bin or .iso)")
    ap.add_argument("--only", nargs="*", help="file name prefixes (AREA041 ...)")
    ap.add_argument("--edges", action="store_true", help="keep the map's last row and column")
    ap.add_argument("--cells", action="append", default=[], metavar="AREA:X0,Z0,X1,Z1",
                    help="print a rectangle of one area cell by cell (repeatable)")
    a = ap.parse_args()
    gen = blocks_from_dat(a.dat, a.only) if a.dat else blocks_from_disc(a.disc, a.only)
    areas, seen = [], collections.Counter()
    for name, block in gen:
        seen[name] += 1
        if seen[name] > 1:
            name = "%s#%d" % (name, seen[name])
        info, why = survey_block(block)
        if info is None:
            print("  %s: skipped, %s" % (name, why))
            continue
        areas.append((name, info))
    if not areas:
        raise SystemExit("no area blocks found")
    report(areas, a.edges)
    for spec in a.cells:
        nm, r = spec.split(":")
        rect = tuple(int(v) for v in r.split(","))
        for name, info in areas:
            if name.upper() == nm.upper():
                print()
                detail(name, info, rect)


if __name__ == "__main__":
    sys.exit(main())
