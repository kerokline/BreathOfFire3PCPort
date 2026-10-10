#!/usr/bin/env python
"""base/exe/: the PC's exe-resident tables in the PC's .data layout (unified-data step 8).

docs/exe-import.md; docs/unified-data-plan.md section 5 step 3. The engine
reads the game's tables from BOF3.exe's initialised .data at fixed addresses
(0x5DA000..0x676000); a cache built without the PC's executable needs that
image from a disc. This module writes it:

    base/exe/data.bin    the initialised .data, 0x9C000 bytes from 0x5DA000
    base/exe/data.toml   the image's address, size and sha256, the build it
                         came from, and how every range of it was produced

From BOF3.exe the image is a copy of the section (the oracle). From a disc
alone, exe_maps/<build>.tsv's segments are copied from the file and address
they name (the boot EXE, an EMI section, the PSP ELF); every catalogued table
of tables.toml with a recorded place in that build is read from that place
instead, a 16-byte name field widened from the disc's 8 or 12 bytes with the
name left blank (the names are a language layer's, as the enemy tables' are,
docs/importer-transforms.md 2); and every pointer word of the PC's
(recipes/exe-pointers.tsv) is left unfilled - a disc's pointer is its own
build's address, never the PC's - but the pointers into .data the map run
backwards gives exactly (the disc's word, an address in that build, through
the segment that places it to the PC address), which are written back; the
words the rule would get wrong on a build are listed in recipes/exe-rebuild.tsv
and stay unfilled (docs/exe-import-engine.md section 1). What no disc carries
stays zero and is listed.

    python tools/exe_tables.py build   --source PATH --out CACHE        # BOF3.exe or a disc
    python tools/exe_tables.py recipe  --game DIR --disc DISC [...]     # recipes/exe.toml + exe-pointers.tsv
    python tools/exe_tables.py measure --game DIR --disc DISC [...] --out analysis/exe_import
    python tools/exe_tables.py xref    --game DIR --disc DISC [...] --out analysis/exe_import
    python tools/exe_tables.py verify  --cache CACHE
    python tools/exe_tables.py check                                    # the recipe alone (CI)

`recipe` needs the PC's executable and the discs: it decides which of the
PC's pointer-shaped words are pointers (those some disc holds differently)
and records each build's image hash and counts. The recipe holds addresses,
sizes, hashes and counts only (CLAUDE.md rule 1); the images and the
measurements are game data and go to a cache or analysis/.
"""
import argparse
import bisect
import collections
import datetime
import hashlib
import json
import os
import re
import struct
import sys
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import exe_twins     # noqa: E402
import tables        # noqa: E402

DATA_LO, DATA_HI = 0x5DA000, 0x676000      # .data's initialised bytes (its raw size, 0x9C000)
RDATA_LO = 0x5C4000
IMAGE = exe_twins.IMAGE                    # a word in here is a pointer into the PC image
RECIPE = os.path.join(ROOT, "recipes", "exe.toml")
POINTERS = os.path.join(ROOT, "recipes", "exe-pointers.tsv")
REBUILD = os.path.join(ROOT, "recipes", "exe-rebuild.tsv")
PLACES = os.path.join(ROOT, "recipes", "exe-places.tsv")
PLACE_COLS = ["build", "pc", "pc_end", "file", "section", "addr", "agree", "keys"]
PC = "pc-zh"
ORDER = ("psx-jp", "psx-us", "psx-eu-en", "psx-fr", "psx-de", "psp-jp", "psp-eu")


def sha(b):
    return hashlib.sha256(b).hexdigest()


def fail(msg):
    raise SystemExit("exe_tables: " + msg)


# ------------------------------------------------------------------ the PC side

def pe_sections(exe):
    pe = struct.unpack_from("<I", exe, 0x3C)[0]
    nsec, optsz = struct.unpack_from("<H", exe, pe + 6)[0], struct.unpack_from("<H", exe, pe + 20)[0]
    base = struct.unpack_from("<I", exe, pe + 24 + 28)[0]
    out = {}
    for i in range(nsec):
        s = pe + 24 + optsz + i * 40
        vsz, rva, rsz, raw = struct.unpack_from("<IIII", exe, s + 8)
        out[exe[s:s + 8].rstrip(b"\0").decode("latin1")] = (base + rva, vsz, raw, rsz)
    return out


def pc_data(exe_path):
    """(BOF3.exe's initialised .data, the section's virtual end)."""
    with open(exe_path, "rb") as f:
        exe = f.read()
    va, vsz, raw, rsz = pe_sections(exe)[".data"]
    if (va, va + rsz) != (DATA_LO, DATA_HI):
        fail("%s: .data is 0x%X..0x%X, not the pc-zh layout" % (exe_path, va, va + rsz))
    return exe[raw:raw + rsz], va + vsz


def name_fields():
    """The PC addresses of every catalogued 16-byte name field: glyph codes,
    never pointers, though four of them can look like one."""
    cat, syms, _ = tables.load()
    out = set()
    for t in cat["table"]:
        if t.get("name") and "symbol" in t:
            for r in range(t["count"]):
                at = syms[t["symbol"]] + r * t["stride"] + t["name"]["at"]
                out.update(range(at, at + tables.PC_NAME_LEN))
    return out


def pointer_words(pc):
    """PC addresses of the 4-aligned words of .data whose value points into
    the image, but those inside a name field."""
    names = name_fields()
    return [DATA_LO + o for o in range(0, len(pc) - 3, 4) if IMAGE[0] <= struct.unpack_from("<I", pc, o)[0] < IMAGE[1]
            and not any(DATA_LO + o + k in names for k in range(4))]


def target_class(v):
    return "text" if v < RDATA_LO else "rdata" if v < 0x5DA000 else "data" if v < DATA_HI else "bss"


# ------------------------------------------------------------------ the recipe files

def load_pointers(path=POINTERS):
    """[(pc, words, class)] from recipes/exe-pointers.tsv."""
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or line.startswith("pc\t"):
                continue
            pc, n, cls = line.rstrip("\n").split("\t")
            out.append((int(pc, 16), int(n), cls))
    return out


def load_recipe(path=RECIPE):
    with open(path, "rb") as f:
        return tomllib.load(f)


# ------------------------------------------------------------------ the disc side

def table_rows(cat, syms, bid):
    """[(table, its [[table.psx]] row for this build)] for every catalogued
    table with a PC symbol and a recorded place in `bid`. On a PSP disc the
    ELF's row, the copy the PSP's own code reads (as the maps prefer)."""
    out = []
    for t in cat["table"]:
        if "symbol" not in t:
            continue
        rows = [p for p in t.get("psx", []) if p["build"] == bid]
        if not rows:
            continue
        rows.sort(key=lambda p: (p["file"] != "BOOT.BIN", p["status"] != "evidence"))
        out.append((t, rows[0]))
    return out


def table_span(t, syms):
    return syms[t["symbol"]], syms[t["symbol"]] + t["stride"] * t["count"]


def disc_image(disc, bid, places=()):
    """(image, owner, ranges) from a disc alone, before the pointer mask: the
    0x9C000-byte image, a per-byte list of which range wrote it (None:
    nothing), and the ranges [pc, end, how, file, address of pc]. `places`:
    recipes/exe-places.tsv's rows for the build (`place` ranges, after the
    map's segments and before the catalogued tables)."""
    cat, syms, builds = tables.load()
    src = {(f, s): (base, blob) for f, s, base, blob in exe_twins.sources(disc)}
    img, owner, ranges = bytearray(DATA_HI - DATA_LO), [None] * (DATA_HI - DATA_LO), []

    def put(lo, raw, how, where, addr):
        k = len(ranges)
        ranges.append([lo, lo + len(raw), how, where, addr])
        img[lo - DATA_LO:lo - DATA_LO + len(raw)] = raw
        owner[lo - DATA_LO:lo - DATA_LO + len(raw)] = [k] * len(raw)
    for r in tables.read_map(os.path.join(tables.MAPS, bid + ".tsv")):
        base, blob = src[(r["file"], r["section"])]
        at = r["addr"] - base
        put(r["pc"], blob[at:at + r["pc_end"] - r["pc"]], "map",
            r["file"] + ("" if r["section"] is None else "#%d" % r["section"]), r["addr"])
    for r in places:
        base, blob = src[(r["file"], r["section"])]
        at = r["addr"] - base
        put(r["pc"], blob[at:at + r["pc_end"] - r["pc"]], "place",
            r["file"] + ("" if r["section"] is None else "#%d" % r["section"]), r["addr"])
    name_len = tables.name_len_for(builds[bid], cat["meta"]["name_len"])
    for t, p in table_rows(cat, syms, bid):
        lo, hi = table_span(t, syms)
        where = p["file"] + ("" if "section" not in p else "#%d" % p["section"])
        if t.get("name"):
            narrow = t["stride"] - tables.PC_NAME_LEN + name_len
            raw = tables.psx_bytes(disc, p, narrow * t["count"])
            at = t["name"]["at"]
            out = b"".join(rec[:at] + bytes(tables.PC_NAME_LEN) + rec[at + name_len:]
                           for rec in (raw[i * narrow:(i + 1) * narrow] for i in range(t["count"])))
            put(lo, out, "widen", where, p["addr"])          # a widened range's `from` is its table's start
        else:
            put(lo, tables.psx_bytes(disc, p, hi - lo), "table", where, p["addr"])
    return img, owner, ranges


def mask(img, owner, pointers):
    """Leave every pointer word unfilled. Returns the bytes cleared."""
    n = 0
    for pc, words, _ in pointers:
        o = pc - DATA_LO
        for i in range(o, o + 4 * words):
            if owner[i] is not None:
                owner[i] = None
                img[i] = 0
                n += 1
    return n


# ------------------------------------------------------------------ placing by data pointers

def load_places(path=PLACES):
    """{build: [row]} from recipes/exe-places.tsv, each row as tables.read_map's."""
    out = collections.defaultdict(list)
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8") as f:
        lines = [l.rstrip("\n") for l in f if not l.startswith("#")]
    if lines and lines[0].split("\t") != PLACE_COLS:
        fail("exe-places.tsv: the column line is not %s" % "\t".join(PLACE_COLS))
    for l in lines[1:]:
        c = l.split("\t")
        out[c[0]].append({"pc": int(c[1], 16), "pc_end": int(c[2], 16), "file": c[3],
                          "section": None if c[4] == "-" else int(c[4]), "addr": int(c[5], 16),
                          "agree": int(c[6]), "keys": int(c[7])})
    return out


def code_word(w, psp):
    """A disc word that can be a code pointer of that build: PSX main RAM, or
    the PSP's user memory (bit 31 clear, as the PSP's EMIs have it)."""
    return (0x08800000 <= w < 0x0A000000) if psp else (0x80010000 <= w < 0x80200000)


def find_places(pc, disc, bid, pointers, rounds=8):
    """Place what the map left unplaced by the PC's own data pointers - the
    twin method keyed by pointers instead of bytes (docs/exe-import-engine.md
    section 3). Needs the PC's .data. Every unplaced region holding a data
    pointer word whose target the image places is searched for: the target's
    address in the build (the map run forwards) is a 4-aligned word in some
    section of the target's file, and each such hit votes for the region's
    place there. The region's best place, if no other place has as many
    votes, is checked byte by byte against the PC's - numbers equal, a data
    pointer the target's address in the build, a code pointer a code address
    of the build, other pointer words wild - and the longest stretch with no
    disagreement holding two keys (or one key and 16 equal bytes) is placed.
    New places give new targets, so it runs again until nothing moves.
    Returns rows for recipes/exe-places.tsv."""
    psp = "SYSTEM.CNF" not in disc.files
    srcs = {}
    for f, sec, base, blob in exe_twins.sources(disc):
        srcs[(f, sec)] = (base, blob)
    byfile = collections.defaultdict(list)
    for k in srcs:
        byfile[k[0]].append(k)
    cls = {}
    for p, n, c in pointers:
        for a in range(p, p + 4 * n, 4):
            cls[a] = c
    rows = []
    for _ in range(rounds):
        img, owner, ranges = disc_image(disc, bid, rows)
        pieces = sorted((q[2], q[2] + q[1] - q[0], q[3], q[0]) for q in linear_pieces(owner, ranges))
        plo = [q[0] for q in pieces]

        def fwd(t):
            i = bisect.bisect_right(plo, t) - 1
            if i >= 0 and pieces[i][0] <= t < pieces[i][1]:
                return pieces[i][2].split("#")[0], pieces[i][3] + t - pieces[i][0]
            return None
        regions, i, n = [], 0, len(owner)
        while i < n:
            if owner[i] is None:
                j = i
                while j < n and owner[j] is None:
                    j += 1
                regions.append((DATA_LO + i, DATA_LO + j))
                i = j
            else:
                i += 1
        keys, need = {}, collections.defaultdict(set)
        for lo, hi in regions:
            for a in range(lo + (-lo % 4), hi - 3, 4):
                if cls.get(a) == "data":
                    f = fwd(struct.unpack_from("<I", pc, a - DATA_LO)[0])
                    if f:
                        keys[a] = f
                        for k in byfile[f[0]]:
                            need[k].add(f[1])
        hits = collections.defaultdict(list)
        for k, ws in need.items():
            base, blob = srcs[k]
            for w in ws:
                pat = struct.pack("<I", w)
                o = blob.find(pat)
                while o >= 0:
                    if o % 4 == 0:
                        hits[k, w].append(base + o)
                    o = blob.find(pat, o + 1)
        new = []
        for lo, hi in regions:
            votes = collections.Counter()
            for a in range(lo + (-lo % 4), hi - 3, 4):
                if a in keys:
                    for k in byfile[keys[a][0]]:
                        for h in hits.get((k, keys[a][1]), ()):
                            votes[k, h - a] += 1
            top = votes.most_common(2)
            if not top or (len(top) > 1 and top[1][1] == top[0][1]):
                continue
            (k, delta), _ = top[0]
            base, blob = srcs[k]
            res = []
            for a in range(lo, hi):
                o = a + delta - base
                w0 = a - (a - DATA_LO) % 4
                c = cls.get(w0)
                if not 0 <= o < len(blob) or not 0 <= w0 + delta - base <= len(blob) - 4:
                    res.append(False)
                elif c is None:
                    res.append(blob[o] == pc[a - DATA_LO])
                else:
                    w = struct.unpack_from("<I", blob, w0 + delta - base)[0]
                    if c == "data":
                        f = fwd(struct.unpack_from("<I", pc, w0 - DATA_LO)[0])
                        res.append(None if f is None else w == f[1])
                    elif c == "text":
                        res.append(None if code_word(w, psp) else False)
                    else:
                        res.append(None)
            s0 = 0
            for i2, v in enumerate(res + [False]):
                if v is not False:
                    continue
                if i2 > s0:
                    a0, a1 = lo + s0, lo + i2
                    eq = sum(1 for v2 in res[s0:i2] if v2)
                    nk = sum(1 for a in range(a0 + (-a0 % 4), a1 - 3, 4) if a in keys)
                    if nk >= 2 or (nk >= 1 and eq >= 16):
                        new.append({"pc": a0, "pc_end": a1, "file": k[0], "section": k[1],
                                    "addr": a0 + delta, "agree": eq, "keys": nk})
                s0 = i2 + 1
        if not new:
            break
        rows = sorted(rows + new, key=lambda r: r["pc"])
    return rows


def write_places(places):
    with open(PLACES, "w", newline="\n") as f:
        f.write("# recipes/exe-places.tsv - GENERATED by `tools/exe_tables.py recipe`; do not edit.\n"
                "# What exe_maps/ left unplaced, placed by the PC's own data pointers (docs/exe-import-engine.md\n"
                "# section 3): [pc, pc_end) is the build's `file` / `section` from `addr` on, as in\n"
                "# exe_maps/; `agree` bytes compared equal (numbers equal, data pointers the target's\n"
                "# address in the build), `keys` data pointers voted for the place. Addresses and\n"
                "# counts only (CLAUDE.md rule 1).\n")
        f.write("\t".join(PLACE_COLS) + "\n")
        for bid in ORDER:
            for r in places.get(bid, ()):
                f.write("%s\t0x%06X\t0x%06X\t%s\t%s\t0x%08X\t%d\t%d\n" % (
                    bid, r["pc"], r["pc_end"], r["file"], "-" if r["section"] is None else r["section"],
                    r["addr"], r["agree"], r["keys"]))


# ------------------------------------------------------------------ the data-pointer rebuild

def linear_pieces(owner, ranges):
    """The image's placed bytes as linear pieces (disc address, disc end, PC
    address, where): every `map` or `table` span, as the bytes ended up after
    the tables overrode the maps. `widen` spans are not linear (the disc's
    stride is not the PC's) and are left out: no pointer into them is rebuilt."""
    out = []
    for lo, hi, k in spans(owner, ranges):
        if k is None:
            continue
        rlo, _, how, where, addr = ranges[k]
        if how in ("map", "table"):
            out.append((addr + lo - rlo, addr + hi - rlo, lo, where))
    return out


def rebuild_candidates(img, owner, ranges, pointers):
    """The data-pointer rebuild (docs/exe-import-engine.md section 1): the map
    run backwards. For every PC pointer word into .data (recipes/exe-pointers.tsv
    class `data`) whose four bytes one range placed, the disc's word is an
    address in that build; the pieces of the same file and section that hold
    that address - else of the same file, another section - give the PC
    address it means. Exactly one PC value or none: two pieces at the same disc
    address that mean different PC places (the PC duplicated data) are
    `ambiguous`. Three wider rules were measured and are not tried (section
    1.2): another file's pieces (an overlay slot several files load into: 0 of
    283 JP words the PC's value), the resident EXE's pieces from an overlay
    (no word reached them), a one-past-the-end pointer (0 of 1 JP, 32 of 301
    PSP-JP).

    Returns ({pc address: (value, where, disc word)}, Counter of outcomes,
    {pc address: outcome} for the words of class `data` not rebuilt).
    Takes the image before the pointer mask (it needs the disc's words)."""
    pieces = linear_pieces(owner, ranges)
    by_where = collections.defaultdict(list)
    for p in pieces:
        by_where[p[3]].append(p)
        by_where[p[3].split("#")[0] + "#*"].append(p)
    for v in by_where.values():
        v.sort()
    out, c = {}, collections.Counter()
    why = {}
    for pc, n, cls in pointers:
        if cls != "data":
            continue
        for a in range(pc, pc + 4 * n, 4):
            o = a - DATA_LO
            ks = {owner[o + j] for j in range(4)}
            if None in ks or len(ks) != 1:
                why[a] = "word unplaced" if None in ks else "word split"
                c[why[a]] += 1
                continue
            where = ranges[ks.pop()][3]
            w = struct.unpack_from("<I", img, o)[0]
            got = None
            for key in (where, where.split("#")[0] + "#*"):
                vals = {q[2] + w - q[0] for q in by_where.get(key, ()) if q[0] <= w < q[1]}
                if vals:
                    got = vals.pop() if len(vals) == 1 else "ambiguous"
                    break
            if got is None or got == "ambiguous":
                why[a] = "target unplaced" if got is None else "ambiguous"
                c[why[a]] += 1
            else:
                out[a] = (got, where, w)
                c["rebuilt"] += 1
    return out, c, why


def load_rebuild_refused(path=REBUILD):
    """{build: set of PC word addresses} the rule rebuilds to a value that is
    not the PC's (recipes/exe-rebuild.tsv): left unfilled."""
    out = collections.defaultdict(set)
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or line.startswith("build\t"):
                continue
            bid, pc, n, _ = line.rstrip("\n").split("\t")
            out[bid].update(range(int(pc, 16), int(pc, 16) + 4 * int(n), 4))
    return out


def apply_rebuild(img, owner, ranges, rebuilt, refused):
    """Write the rebuilt words into the masked image (each its own `rebuilt`
    range, `from` the disc word's file and the word). Returns the words written."""
    n = 0
    for a in sorted(rebuilt):
        if a in refused:
            continue
        v, where, w = rebuilt[a]
        o = a - DATA_LO
        k = len(ranges)
        ranges.append([a, a + 4, "rebuilt", where, w])
        struct.pack_into("<I", img, o, v)
        owner[o:o + 4] = [k] * 4
        n += 1
    return n


def disc_built(disc, bid, pointers, refused):
    """The disc's image as base/exe/ holds it: placed, the pointer words
    masked, the data pointers the rule rebuilds written back but those the
    recipe refuses for this build. (image, owner, ranges, rebuilt, outcomes)."""
    img, owner, ranges = disc_image(disc, bid, load_places().get(bid, ()))
    rebuilt, c, _ = rebuild_candidates(img, owner, ranges, pointers)
    mask(img, owner, pointers)
    c["refused"] = sum(1 for a in rebuilt if a in refused)
    c["written"] = apply_rebuild(img, owner, ranges, rebuilt, refused)
    return img, owner, ranges, rebuilt, c


def selftest_rebuild():
    """A synthetic round trip of the rule (no game data): two overlay sections
    and a boot EXE, a duplicated piece, an unplaced word, a target in another
    file. Returns a list of problems."""
    errs = []
    img = bytearray(DATA_HI - DATA_LO)
    owner = [None] * len(img)
    ranges = [[0x5DA000, 0x5DA100, "map", "A.EMI#1", 0x801F0000],    # words 0x5DA000.., data at 0x801F0000
              [0x5DA100, 0x5DA200, "map", "A.EMI#2", 0x80100000],    # A's other section
              [0x5DA200, 0x5DA240, "map", "B.EMI#1", 0x801F0000],    # another overlay at the same address
              [0x5DA300, 0x5DA340, "map", "A.EMI#1", 0x801F0040],    # A#1's bytes placed twice by the PC
              [0x5DA400, 0x5DA440, "map", "A.EMI#1", 0x801F0040]]
    for k, (lo, hi, _, _, _) in enumerate(ranges):
        owner[lo - DATA_LO:hi - DATA_LO] = [k] * (hi - lo)
    words = {0x5DA010: (0x801F0020, 0x5DA020),     # same section
             0x5DA014: (0x80100010, 0x5DA110),     # same file, other section
             0x5DA018: (0x801F0050, "ambiguous"),  # the duplicated piece
             0x5DA01C: (0x80300000, None),         # nowhere: target unplaced
             0x5DA210: (0x80100010, None)}         # B's word into A's section: another file, not tried
    for a, (w, _) in words.items():
        struct.pack_into("<I", img, a - DATA_LO, w)
    owner[0x5DA0F0 - DATA_LO] = None                # a word one byte of which is unplaced
    pointers = [(0x5DA010, 4, "data"), (0x5DA0F0, 1, "data"), (0x5DA210, 1, "data"), (0x5DA220, 1, "text")]
    got, c, _ = rebuild_candidates(img, owner, ranges, pointers)
    for a, (w, want) in words.items():
        have = got.get(a, (None,))[0]
        if want == "ambiguous":
            if a in got:
                errs.append("0x%X: a duplicated piece rebuilt" % a)
        elif have != want:
            errs.append("0x%X: rebuilt %s, want %s" % (a, have, want))
    if dict(c) != {"rebuilt": 2, "ambiguous": 1, "target unplaced": 2, "word unplaced": 1}:
        errs.append("outcomes %s" % dict(c))
    mask(img, owner, pointers)
    n = apply_rebuild(img, owner, ranges, got, {0x5DA014})
    if n != 1 or struct.unpack_from("<I", img, 0x10)[0] != 0x5DA020 or struct.unpack_from("<I", img, 0x14)[0]:
        errs.append("apply: %d written" % n)
    return errs


def spans(owner, ranges):
    """The image as [(lo, hi, range index or None)] runs."""
    out, start = [], 0
    for i in range(1, len(owner) + 1):
        if i == len(owner) or owner[i] != owner[start]:
            out.append((DATA_LO + start, DATA_LO + i, owner[start]))
            start = i
    return out


# ------------------------------------------------------------------ base/exe/

def write_exe(out, bid, source, img, bss_end, ranges=None, owner=None, pointers=None):
    """base/exe/data.bin and data.toml. From the PC, one range; from a disc,
    every range that was written and every one that was not, with why."""
    d = os.path.join(out, "base", "exe")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "data.bin"), "wb") as f:
        f.write(img)
    lines = ["# base/exe/data.toml - written by tools/exe_tables.py (docs/exe-import.md).",
             "# data.bin is BOF3.exe's initialised .data in the PC's layout, to be mapped at `va`;",
             "# [va + size, bss_end) is zero-initialised. Each range: [pc, end, how, from] -",
             "# how `exe` (BOF3.exe's own section), `map` (exe_maps/<build>.tsv's segment),",
             "# `table` (a tables.toml table at its recorded place), `widen` (a name table, the",
             "# disc's numbers at the PC's offsets, the 16-byte name blank), `pointer` (a pointer",
             "# word of the PC's: unfilled, never copied), `rebuilt` (a pointer word into .data the",
             "# disc's own word gives by the map run backwards; `from` is that word), `none` (no",
             "# disc carries it: unfilled).",
             "", "[image]", 'file = "data.bin"', "va = 0x%06X" % DATA_LO, "size = %d" % len(img),
             "bss_end = 0x%06X" % bss_end, 'sha256 = "%s"' % sha(img), 'build = "%s"' % bid,
             'source = %s' % json.dumps(os.path.basename(os.path.normpath(source))),
             'generated = "%s"' % datetime.date.today().isoformat(), "", "ranges = ["]
    if owner is None:
        lines.append('  [0x%06X, 0x%06X, "exe", ".data"],' % (DATA_LO, DATA_HI))
    else:
        ptr = set()
        for pc, words, _ in pointers:
            ptr.update(range(pc, pc + 4 * words))
        for lo, hi, k in spans(owner, ranges):
            if k is not None:
                rpc, _, how, where, addr = ranges[k]
                at = addr if how == "widen" else addr + lo - rpc
                lines.append('  [0x%06X, 0x%06X, "%s", "%s@0x%08X"],' % (lo, hi, how, where, at))
                continue
            p = lo                                      # an unfilled run: pointer words and the rest
            while p < hi:
                is_ptr = p in ptr
                q = p
                while q < hi and (q in ptr) == is_ptr:
                    q += 1
                lines.append('  [0x%06X, 0x%06X, "%s", ""],' % (p, q, "pointer" if is_ptr else "none"))
                p = q
    lines.append("]")
    with open(os.path.join(d, "data.toml"), "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return sha(img)


def build_from(path, bid, out):
    """base/exe/ from one source: BOF3.exe (a copy) or a disc (the recipe's
    pointer list applied). Returns (build, sha256 of the image)."""
    import psx_disc
    if bid == PC:
        img, bss_end = pc_data(path)
        return bid, write_exe(out, bid, path, img, bss_end)
    rec = load_recipe()
    if bid not in rec.get("build", {}):
        fail("recipes/exe.toml has no image for %s" % bid)
    pointers = load_pointers()
    img, owner, ranges, _, _ = disc_built(psx_disc.Disc(path), bid, pointers, load_rebuild_refused()[bid])
    h = write_exe(out, bid, path, img, rec["build"][PC]["bss_end"], ranges, owner, pointers)
    if h != rec["build"][bid]["sha256"]:
        fail("the %s image hashes %s, recipes/exe.toml says %s" % (bid, h, rec["build"][bid]["sha256"]))
    return bid, h


def verify_cache(cache):
    """base/exe/data.bin against data.toml's hash and the recipe's for its build:
    BOF3.exe's .data when the PC was the source, the recorded disc-built image
    otherwise. Returns a list of problems."""
    d = os.path.join(cache, "base", "exe")
    if not os.path.exists(os.path.join(d, "data.toml")):
        return ["base/exe/: absent"]
    with open(os.path.join(d, "data.toml"), "rb") as f:
        img = tomllib.load(f)["image"]
    with open(os.path.join(d, img["file"]), "rb") as f:
        h = sha(f.read())
    rec = load_recipe()["build"].get(img["build"])
    errs = []
    if h != img["sha256"]:
        errs.append("base/exe/%s: hashes %s, data.toml says %s" % (img["file"], h, img["sha256"]))
    if rec is None:
        errs.append("base/exe/: build %s has no image in recipes/exe.toml" % img["build"])
    elif h != rec["sha256"]:
        errs.append("base/exe/%s: not the %s image recipes/exe.toml records" % (img["file"], img["build"]))
    return errs


# ------------------------------------------------------------------ the generator

def discs(paths):
    import psx_disc
    _, _, builds = tables.load()
    out = []
    for p in paths:
        d = psx_disc.Disc(p)
        out.append((tables.disc_build(d, builds)["id"], d))
    out.sort(key=lambda x: ORDER.index(x[0]))
    return out


def decide_pointers(pc, images):
    """The PC's pointer-shaped words that are pointers: all but those every
    build that places them holds equal to the PC's (a number that only looks
    like an address). [(pc, words, class)] runs, split by the class of what
    they point at."""
    words = []
    for a in pointer_words(pc):
        o = a - DATA_LO
        placed = [img[o:o + 4] for img, owner, _ in images if all(owner[i] is not None for i in range(o, o + 4))]
        if placed and all(w == pc[o:o + 4] for w in placed):
            continue
        words.append((a, target_class(struct.unpack_from("<I", pc, o)[0])))
    runs = []
    for a, cls in words:
        if runs and runs[-1][0] + 4 * runs[-1][1] == a and runs[-1][2] == cls:
            runs[-1][1] += 1
        else:
            runs.append([a, 1, cls])
    return [tuple(r) for r in runs]


def counts(pc, img, owner, ranges, pointers):
    """The image against the PC's, byte by byte."""
    ptr = set()
    for a, n, _ in pointers:
        ptr.update(range(a - DATA_LO, a - DATA_LO + 4 * n))
    c = collections.Counter()
    for i in range(len(pc)):
        if owner[i] is None:
            c["pointer" if i in ptr else "none", "pc-zero" if pc[i] == 0 else "pc-nonzero"] += 1
        else:
            c["rebuilt" if ranges[owner[i]][2] == "rebuilt" else "placed",
              "equal" if img[i] == pc[i] else "differ"] += 1
    return c


def cmd_recipe(a):
    pc, bss_end = pc_data(os.path.join(a.game, "BOF3.exe"))
    images = []
    for bid, disc in discs(a.disc):
        img, owner, ranges = disc_image(disc, bid)
        images.append((img, owner, ranges))
        print("%s: %d ranges, %d bytes placed before the pointer mask" % (bid, len(ranges), sum(o is not None for o in owner)))
    pointers = decide_pointers(pc, images)
    cand = len(pointer_words(pc))
    # The places by data pointer come after the pointer decision, which stays the
    # maps' alone, so exe-pointers.tsv does not move with them.
    places = {}
    for bid, disc in discs(a.disc):
        places[bid] = find_places(pc, disc, bid, pointers)
        print("%s: %d places by data pointer, %d bytes" % (bid, len(places[bid]),
                                                            sum(r["pc_end"] - r["pc"] for r in places[bid])))
    write_places(places)
    images = [disc_image(disc, bid, places[bid]) for bid, disc in discs(a.disc)]
    refused, outcomes = {}, {}
    for (bid, _), (img, owner, ranges) in zip(discs(a.disc), images):
        rebuilt, outcomes[bid], _ = rebuild_candidates(img, owner, ranges, pointers)
        refused[bid] = sorted(x for x, (v, _, _) in rebuilt.items() if v != struct.unpack_from("<I", pc, x - DATA_LO)[0])
    with open(REBUILD, "w", newline="\n") as f:
        f.write("# recipes/exe-rebuild.tsv - GENERATED by `tools/exe_tables.py recipe`; do not edit.\n"
                "# The data pointers the rebuild rule (the map run backwards, docs/exe-import-engine.md\n"
                "# section 1) gives a value that is not the PC's, per build: runs of `words` pointer\n"
                "# words from `pc` that base/exe/ leaves unfilled from that build. `why` is the\n"
                "# section the PC's own word points at. Addresses and counts only (CLAUDE.md rule 1).\n"
                "build\tpc\twords\twhy\n")
        for bid in refused:
            runs = []
            for x in refused[bid]:
                if runs and runs[-1][0] + 4 * runs[-1][1] == x:
                    runs[-1][1] += 1
                else:
                    runs.append([x, 1])
            for x, n in runs:
                f.write("%s\t0x%06X\t%d\tdata\n" % (bid, x, n))
    os.makedirs(os.path.dirname(POINTERS), exist_ok=True)
    with open(POINTERS, "w", newline="\n") as f:
        f.write("# recipes/exe-pointers.tsv - GENERATED by `tools/exe_tables.py recipe`; do not edit.\n"
                "# The pointer words of BOF3.exe's initialised .data: runs of `words` 4-byte words from\n"
                "# `pc` whose value points into the PC image (`to`: the section it points at). A disc's\n"
                "# word there is its own build's address, so base/exe/ leaves them unfilled; the PC's\n"
                "# pointer-shaped words every disc that places them holds equal are numbers, not\n"
                "# pointers, and are not listed. Addresses and counts only (CLAUDE.md rule 1).\n"
                "pc\twords\tto\n")
        for p, n, cls in pointers:
            f.write("0x%06X\t%d\t%s\n" % (p, n, cls))
    lines = ["# recipes/exe.toml - GENERATED by `tools/exe_tables.py recipe`; do not edit by hand.",
             "# base/exe/data.bin as each held build produces it (docs/exe-import.md): its sha256,",
             "# and its bytes against BOF3.exe's .data - placed and equal, placed and different,",
             "# pointer words left unfilled, and what no disc carries (where the PC's bytes are",
             "# zero, and where not). Hashes and counts only (CLAUDE.md rule 1).",
             "", "[meta]", 'generated = "%s"' % datetime.date.today().isoformat(),
             "va = 0x%06X" % DATA_LO, "size = %d" % len(pc),
             'pointers = "exe-pointers.tsv"',
             'pointers_sha256 = "%s"' % sha(open(POINTERS, "rb").read()),
             "pointer_candidates = %d" % cand,
             "pointer_words = %d" % sum(n for _, n, _ in pointers),
             'rebuild = "exe-rebuild.tsv"',
             'rebuild_sha256 = "%s"' % sha(open(REBUILD, "rb").read()),
             'places = "exe-places.tsv"',
             'places_sha256 = "%s"' % sha(open(PLACES, "rb").read()), "",
             "[build.%s]" % PC, 'sha256 = "%s"' % sha(pc), "bss_end = 0x%06X" % bss_end, ""]
    for (bid, _), (img, owner, ranges) in zip(discs(a.disc), images):
        rebuilt, _, _ = rebuild_candidates(img, owner, ranges, pointers)
        mask(img, owner, pointers)
        apply_rebuild(img, owner, ranges, rebuilt, set(refused[bid]))
        c = counts(pc, img, owner, ranges, pointers)
        o = outcomes[bid]
        lines += ["[build.%s]" % bid, 'sha256 = "%s"' % sha(img),
                  "ranges = %d" % len(ranges),
                  "places = %d" % len(places[bid]), "placed_by_pointer = %d" % sum(r["pc_end"] - r["pc"] for r in places[bid]),
                  "placed_equal = %d" % c["placed", "equal"], "placed_differ = %d" % c["placed", "differ"],
                  "pointer_pc_zero = %d" % c["pointer", "pc-zero"], "pointer_pc_nonzero = %d" % c["pointer", "pc-nonzero"],
                  "none_pc_zero = %d" % c["none", "pc-zero"], "none_pc_nonzero = %d" % c["none", "pc-nonzero"],
                  "rebuilt_equal = %d" % c["rebuilt", "equal"],
                  "rebuild_words = %d" % o["rebuilt"], "rebuild_refused = %d" % len(refused[bid]),
                  "rebuild_ambiguous = %d" % o["ambiguous"], "rebuild_target_unplaced = %d" % o["target unplaced"],
                  "rebuild_word_unplaced = %d" % (o["word unplaced"] + o["word split"]),
                  "identical = %d" % sum(x == y for x, y in zip(img, pc)), ""]
        print("%s: %s" % (bid, dict(c)))
    with open(RECIPE, "w", newline="\n") as f:
        f.write("\n".join(lines))
    print("%s: %d of %d pointer-shaped words are pointers, %d runs; %s" % (POINTERS, sum(n for _, n, _ in pointers),
                                                                           cand, len(pointers), RECIPE))


def data_symbols():
    with open(os.path.join(ROOT, "symbols.toml"), "rb") as f:
        data = sorted(tomllib.load(f)["data"], key=lambda e: e["pc"])
    return [e["pc"] for e in data], [e["name"] for e in data]


def symbol_at(syms, pc):
    """The [[data]] symbol at or below pc, and the offset into it."""
    pcs, names = syms
    i = bisect.bisect_right(pcs, pc) - 1
    return (names[i], pc - pcs[i]) if i >= 0 else ("?", 0)


def table_identity(t, syms, pc, img, owner):
    """One catalogued table in the image against the PC's: (verdict, detail)."""
    lo, hi = table_span(t, syms)
    o0, o1 = lo - DATA_LO, hi - DATA_LO
    if all(owner[i] is None for i in range(o0, o1)):
        return "unfilled", ""
    want = bytearray(pc[o0:o1])
    name = t.get("name")
    if name:
        for r in range(t["count"]):
            at = r * t["stride"] + name["at"]
            want[at:at + tables.PC_NAME_LEN] = bytes(tables.PC_NAME_LEN)
    got = img[o0:o1]
    holes = [i for i in range(o0, o1) if owner[i] is None]
    if got == want:
        return ("identical, names blank" if name else "identical"), ("%d pointer bytes unfilled" % len(holes)
                                                                    if holes else "")
    recs = sorted({(i // t["stride"]) for i in range(len(want)) if got[i] != want[i] and owner[o0 + i] is not None})
    nb = sum(1 for i in range(len(want)) if got[i] != want[i] and owner[o0 + i] is not None)
    return "differs", "%d bytes in record%s %s%s" % (nb, "" if len(recs) == 1 else "s", ",".join(map(str, recs[:20])),
                                                    "" if not holes else "; %d bytes unfilled" % len(holes))


def translatable(bid, pc, owner_before, img_before, pointers):
    """How the PC's pointer words fare on a disc, before the mask: a word into
    .data whose target the build's map places, and whose disc word is the
    target's address in that build, could be rebuilt from the disc by the
    map run backwards (not done: the words stay unfilled)."""
    segs = tables.read_map(os.path.join(tables.MAPS, bid + ".tsv"))
    st = [r["pc"] for r in segs]
    c = collections.Counter()
    for p, n, cls in pointers:
        for a in range(p, p + 4 * n, 4):
            o = a - DATA_LO
            if cls not in ("text", "data"):
                continue
            if any(owner_before[o + j] is None for j in range(4)):
                c[cls + " word unplaced"] += 1
                continue
            if cls == "text":
                c["text placed"] += 1
                continue
            t = struct.unpack_from("<I", pc, o)[0]
            i = bisect.bisect_right(st, t) - 1
            if i < 0 or not segs[i]["pc"] <= t < segs[i]["pc_end"]:
                c["data target unplaced"] += 1
            elif segs[i]["addr"] + t - segs[i]["pc"] == struct.unpack_from("<I", img_before, o)[0]:
                c["data translates"] += 1
            else:
                c["data differs"] += 1
    return dict(c)


def family(name):
    """A symbol's family for the rebuild's by-group counts: its first word,
    digits dropped (`Area25_Handlers` -> `Area`, `Scena06_CallA` -> `Scena`)."""
    return re.sub(r"\d+$", "", name.split("+")[0].split("_")[0]) or "?"


def rebuild_report(bid, pc, img, owner, ranges, pointers, refused, syms, out):
    """The rebuild rule against the PC's words, before the mask: every .data /
    .bss / .rdata pointer word's outcome, counted, and by the word's symbol
    family for those not rebuilt; per word in analysis/exe_import/<build>.rebuild.tsv."""
    rebuilt, c, notrebuilt = rebuild_candidates(img, owner, ranges, pointers)
    res = collections.Counter()
    fam = collections.defaultdict(collections.Counter)
    tgt = collections.defaultdict(collections.Counter)
    rows = []
    for p, n, cls in pointers:
        if cls == "text":
            continue
        for x in range(p, p + 4 * n, 4):
            o = x - DATA_LO
            want = struct.unpack_from("<I", pc, o)[0]
            if cls != "data":
                why = cls + ": not tried"
            elif x in rebuilt:
                v = rebuilt[x][0]
                why = "exact" if v == want else "refused (not the PC's)"
                if (v != want) != (x in refused):
                    fail("%s: 0x%06X: exe-rebuild.tsv is stale; rerun `recipe`" % (bid, x))
            else:
                why = notrebuilt[x]
            res[why] += 1
            name = symbol_at(syms, x)[0]
            if why != "exact":
                fam[why][family(name)] += 1
                tgt[why][family(symbol_at(syms, want)[0])] += 1
            rows.append((x, cls, name, symbol_at(syms, want)[0], why))
    with open(os.path.join(out, bid + ".rebuild.tsv"), "w", newline="\n") as f:
        f.write("pc\tto\tword_symbol\ttarget_symbol\toutcome\n")
        for x, cls, name, tname, why in rows:
            f.write("0x%06X\t%s\t%s\t%s\t%s\n" % (x, cls, name, tname, why))
    d = dict(res)
    for why in fam:
        d["by word family, " + why] = dict(fam[why].most_common(8))
    for why in ("refused (not the PC's)",):
        if why in tgt:
            d["by target family, " + why] = dict(tgt[why].most_common(8))
    d["_rebuilt"] = rebuilt
    return d


def cmd_measure(a):
    """Per build: the image against BOF3.exe's .data, every byte classed, the
    differing and the unfilled bytes by symbol, and the catalogued tables'
    identity. Writes analysis/exe_import/<build>.tsv (game-derived: addresses
    and counts, but kept out of the tree with the rest of analysis/)."""
    out = os.path.abspath(a.out)
    if out.startswith(ROOT) and not out.startswith(os.path.join(ROOT, "analysis")):
        fail("--out inside the repository must be under analysis/ (gitignored)")
    os.makedirs(out, exist_ok=True)
    pc, _ = pc_data(os.path.join(a.game, "BOF3.exe"))
    pointers = load_pointers()
    cat, tsyms, _ = tables.load()
    syms = data_symbols()
    summary = {}
    refused, places = load_rebuild_refused(), load_places()
    for bid, disc in discs(a.disc):
        img, owner, ranges = disc_image(disc, bid, places.get(bid, ()))
        xl = translatable(bid, pc, list(owner), bytes(img), pointers)
        rb = rebuild_report(bid, pc, img, owner, ranges, pointers, refused[bid], syms, out)
        mask(img, owner, pointers)
        apply_rebuild(img, owner, ranges, rb.pop("_rebuilt"), refused[bid])
        if sha(img) != load_recipe()["build"][bid]["sha256"]:
            fail("%s: the image is not the one recipes/exe.toml records; rerun `recipe`" % bid)
        c = counts(pc, img, owner, ranges, pointers)
        by = collections.defaultdict(collections.Counter)
        for i in range(len(pc)):
            name, _ = symbol_at(syms, DATA_LO + i)
            if owner[i] is None:
                if pc[i]:
                    by[name]["unfilled"] += 1
            elif img[i] != pc[i]:
                by[name]["differ", ranges[owner[i]][2]] += 1
        with open(os.path.join(out, bid + ".tsv"), "w", newline="\n") as f:
            f.write("symbol\tpc\tdiffer_map\tdiffer_table\tdiffer_widen\tunfilled_nonzero\n")
            pcs, names = syms
            for name in sorted(by, key=lambda n: pcs[names.index(n)] if n in names else 0):
                b = by[name]
                f.write("%s\t0x%06X\t%d\t%d\t%d\t%d\n" % (name, pcs[names.index(name)] if name in names else 0,
                        b["differ", "map"], b["differ", "table"], b["differ", "widen"], b["unfilled"]))
        idt = {}
        for t in cat["table"]:
            if "symbol" in t:
                idt[t["key"]] = table_identity(t, tsyms, pc, img, owner)
        summary[bid] = {"counts": {"%s/%s" % k: v for k, v in c.items()}, "pointers": xl, "rebuild": rb,
                        "identical": sum(x == y for x, y in zip(img, pc)), "tables": idt,
                        "differ_by_how": dict(collections.Counter(ranges[owner[i]][2] for i in range(len(pc))
                                                                  if owner[i] is not None and img[i] != pc[i]))}
        print("== %s: %d of %d bytes identical; %s" % (bid, summary[bid]["identical"], len(pc), dict(c)))
        print("   differing placed bytes by how: %s" % summary[bid]["differ_by_how"])
        print("   pointer words: %s" % xl)
        print("   data-pointer rebuild: %s" % {k: v for k, v in rb.items() if not k.startswith("by ")})
        for k in sorted(x for x in rb if x.startswith("by ")):
            print("     %s: %s" % (k, rb[k]))
        for k, (v, d) in idt.items():
            print("   %-22s %-24s %s" % (k, v, d))
    with open(os.path.join(out, "summary.json"), "w") as f:
        json.dump(summary, f, indent=1)


def src_addresses():
    """Every .rdata / .data / .bss address src/ reads: the symbols.toml [[data]]
    names src/ uses (a word match over *.cpp / *.h, exe_twins' rule) and every
    six-digit hex literal in code (comments stripped) in that range.
    {address: (kind, name, extent)} - a symbol's extent its count x ctype, else
    to the next symbol (at most 64 KiB); a raw constant's, one word."""
    with open(os.path.join(ROOT, "symbols.toml"), "rb") as f:
        data = sorted(tomllib.load(f)["data"], key=lambda e: e["pc"])
    words, lits = collections.Counter(), set()
    for r, _, fs in os.walk(os.path.join(ROOT, "src")):
        for fn in fs:
            if fn.endswith((".cpp", ".h")):
                with open(os.path.join(r, fn), encoding="utf-8", errors="replace") as fh:
                    text = fh.read()
                words.update(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", text))
                code = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
                lits.update(int(m, 16) for m in re.findall(r"\b0x[0-9A-Fa-f]{6}\b", code))
    BSS_END = 0x93E000
    out, starts = {}, {e["pc"]: i for i, e in enumerate(data)}
    for i, e in enumerate(data):
        if not RDATA_LO <= e["pc"] < BSS_END or not (words[e["name"]] or e["pc"] in lits):
            continue
        end = data[i + 1]["pc"] if i + 1 < len(data) else BSS_END
        size = exe_twins.CSIZE.get(e.get("ctype"), 0) * e["count"] if "count" in e else 0
        ext = size or min(end - e["pc"], 0x10000)
        out[e["pc"]] = ("symbol", e["name"], max(1, ext))
    for v in lits:
        if RDATA_LO <= v < BSS_END and v not in starts:
            j = bisect.bisect_right([e["pc"] for e in data], v) - 1
            out[v] = ("raw", "%s+0x%X" % (data[j]["name"], v - data[j]["pc"]) if j >= 0 else "?", 4)
    return out


def classify(addr, ext, pc, owner, img, ptr):
    """One read address against a build's image: what the engine half must do.
    `ptr` maps a pointer byte's offset to the section its word points at."""
    if addr < DATA_LO:
        return "rdata"
    if addr >= DATA_HI:
        return "bss"
    o0, o1 = addr - DATA_LO, min(addr + ext, DATA_HI) - DATA_LO
    c = collections.Counter()
    for i in range(o0, o1):
        if owner[i] is not None:
            c["equal" if img[i] == pc[i] else "differ"] += 1
        elif i in ptr:
            c["pointer"] += 1
        else:
            c["none-zero" if pc[i] == 0 else "none"] += 1
    if c["pointer"] and not (c["equal"] + c["differ"] + c["none"]):
        to = {ptr[i] for i in range(o0, o1) if i in ptr}
        return "pointer:" + ("text" if to == {"text"} else "data" if to <= {"data", "bss", "rdata"} else "mixed")
    if c["none"] and not (c["equal"] + c["differ"]):
        return "none" if not c["pointer"] else "none+pointer"
    if c["none"]:
        return "partly"
    if not (c["equal"] + c["differ"]):
        return "zero"
    kind = "differs" if c["differ"] else "reproduced"
    return kind + ("+pointer" if c["pointer"] else "")


GROUPS = (       # what an address no disc carries is, by its symbol's name
    ("platform: keys and input", r"^(Key_|DInput|Input_|Pad_|Cfg_)"),
    ("platform: graphics", r"^(D3d|Gfx_|Gpu_|DDraw|Cursor_|Screen|Display|Window|Win_|Crt|Fmv|Video)"),
    ("platform: sound and music", r"^(Snd_|Sound|Music|Mp3|DSound|Bgm|Wave|Cd_)"),
    ("platform: C runtime and imports", r"^(Imp_|Crt_|_|Rt_|Heap|File_|Dat_|Task_Stack)"),
    ("language: text, names, formats", r"(Messages?|Text|Names?$|Name_|NameTable|Formats?$|Format_|Lines|Verbs|Choice|Roll|Glyph|Font)"),
    ("re-laid by the port: plate drift, UV", r"(DriftUV|PlateAnims|Plate|UV)"),
)


def group_of(name, ext, near_ptr):
    for g, rx in GROUPS:
        if re.search(rx, name.split("+")[0]):
            return g
    if name.startswith("Char_DefaultRecords"):
        return "widened: the new-game characters' 5-byte names, 9 on the PC"
    if "+" not in name and ext <= 16:
        return "small: 16 bytes or less, too common to place by bytes"
    if near_ptr:
        return "mixed: within 16 bytes of a pointer word (no 16-byte key the survey can seed)"
    return "other"


def cmd_xref(a):
    out = os.path.abspath(a.out)
    if out.startswith(ROOT) and not out.startswith(os.path.join(ROOT, "analysis")):
        fail("--out inside the repository must be under analysis/ (gitignored)")
    os.makedirs(out, exist_ok=True)
    pc, _ = pc_data(os.path.join(a.game, "BOF3.exe"))
    pointers = load_pointers()
    ptr = {}
    for p, n, cls in pointers:
        for i in range(p - DATA_LO, p - DATA_LO + 4 * n):
            ptr[i] = cls
    addrs = src_addresses()
    near = {v: any(i in ptr for i in range(v - DATA_LO - 16, v - DATA_LO + ext + 16)) for v, (_, _, ext) in addrs.items()}
    per = {}
    refused = load_rebuild_refused()
    for bid, disc in discs(a.disc):
        img, owner, ranges, _, _ = disc_built(disc, bid, pointers, refused[bid])
        per[bid] = {v: classify(v, ext, pc, owner, img, ptr) for v, (_, _, ext) in addrs.items()}
    bids = list(per)
    with open(os.path.join(out, "src_addresses.tsv"), "w", newline="\n") as f:
        f.write("addr\tkind\tname\textent\tgroup\t%s\n" % "\t".join(bids))
        for v in sorted(addrs):
            k, n, ext = addrs[v]
            f.write("0x%06X\t%s\t%s\t%d\t%s\t%s\n" % (v, k, n, ext, group_of(n, ext, near[v]), "\t".join(per[b][v] for b in bids)))
    for kind in ("symbol", "raw"):
        print("== %s addresses (%d)" % (kind, sum(1 for x in addrs.values() if x[0] == kind)))
        cls = sorted({per[b][v] for b in bids for v in addrs if addrs[v][0] == kind})
        print("   %-18s %s" % ("class", " ".join("%9s" % b for b in bids)))
        for c in cls:
            print("   %-18s %s" % (c, " ".join("%9d" % sum(1 for v in addrs if addrs[v][0] == kind and per[b][v] == c)
                                                for b in bids)))
    first = bids[0]
    for want in ("none", "none+pointer", "partly", "differs", "differs+pointer"):
        g = collections.Counter(group_of(addrs[v][1], addrs[v][2], near[v]) for v in addrs if per[first][v] == want)
        print("== %s on %s, by group: %s" % (want, first, dict(g.most_common())))
    print("-> %s" % os.path.join(out, "src_addresses.tsv"))


def cmd_check(a):
    """recipes/exe.toml and exe-pointers.tsv alone: the pointer list hashes as
    the recipe says, its runs are in order inside .data, every build is
    fixtures.toml's, and each build's counts add up to the image. No game data."""
    errs = []
    rec = load_recipe()
    with open(POINTERS, "rb") as f:
        if sha(f.read()) != rec["meta"]["pointers_sha256"]:
            errs.append("exe-pointers.tsv: not the file recipes/exe.toml was generated with")
    last, n = DATA_LO, 0
    for p, w, cls in load_pointers():
        if p < last or p % 4 or p + 4 * w > DATA_HI or cls not in ("text", "rdata", "data", "bss"):
            errs.append("exe-pointers.tsv: run 0x%X" % p)
        last, n = p + 4 * w, n + w
    if n != rec["meta"]["pointer_words"]:
        errs.append("exe-pointers.tsv: %d words, the recipe says %d" % (n, rec["meta"]["pointer_words"]))
    with open(REBUILD, "rb") as f:
        if sha(f.read()) != rec["meta"].get("rebuild_sha256"):
            errs.append("exe-rebuild.tsv: not the file recipes/exe.toml was generated with")
    data_words = set()
    for p, w, cls in load_pointers():
        if cls == "data":
            data_words.update(range(p, p + 4 * w, 4))
    refused = load_rebuild_refused()
    for bid, xs in refused.items():
        if bid not in rec["build"] or bid == PC:
            errs.append("exe-rebuild.tsv: build %s" % bid)
        elif len(xs) != rec["build"][bid].get("rebuild_refused"):
            errs.append("exe-rebuild.tsv: %s refuses %d words, the recipe says %d"
                        % (bid, len(xs), rec["build"][bid].get("rebuild_refused")))
        if xs - data_words:
            errs.append("exe-rebuild.tsv: %s refuses %d words that are no data pointer" % (bid, len(xs - data_words)))
    errs += ["the rebuild rule's synthetic round trip: " + e for e in selftest_rebuild()]
    with open(PLACES, "rb") as f:
        if sha(f.read()) != rec["meta"].get("places_sha256"):
            errs.append("exe-places.tsv: not the file recipes/exe.toml was generated with")
    for bid, rows in load_places().items():
        segs = tables.read_map(os.path.join(tables.MAPS, bid + ".tsv")) if bid in rec["build"] else []
        if not segs:
            errs.append("exe-places.tsv: build %s has no map" % bid)
        if len(rows) != rec["build"].get(bid, {}).get("places"):
            errs.append("exe-places.tsv: %s has %d places, the recipe says %s" % (bid, len(rows), rec["build"].get(bid, {}).get("places")))
        st = [r["pc"] for r in segs]
        last = DATA_LO
        for r in rows:
            i = bisect.bisect_right(st, r["pc_end"] - 1) - 1
            if (r["pc"] < last or r["pc_end"] <= r["pc"] or r["pc_end"] > DATA_HI or r["keys"] < 1
                    or (i >= 0 and segs[i]["pc_end"] > r["pc"])):
                errs.append("exe-places.tsv: %s 0x%06X: out of order, empty, outside .data, keyless or over a map segment"
                            % (bid, r["pc"]))
            last = r["pc_end"]
    _, _, builds = tables.load()
    keys = ("placed_equal", "placed_differ", "pointer_pc_zero", "pointer_pc_nonzero", "none_pc_zero", "none_pc_nonzero",
            "rebuilt_equal")
    for bid, b in rec["build"].items():
        if bid not in builds:
            errs.append("build %s: not in fixtures.toml" % bid)
        if not re.fullmatch(r"[0-9a-f]{64}", b.get("sha256", "")):
            errs.append("build %s: sha256" % bid)
        if bid != PC and sum(b.get(k, 0) for k in keys) != rec["meta"]["size"]:
            errs.append("build %s: the counts add to %d, not %d" % (bid, sum(b.get(k, 0) for k in keys), rec["meta"]["size"]))
        if bid != PC and b.get("rebuilt_equal", 0) != 4 * (b.get("rebuild_words", 0) - b.get("rebuild_refused", 0)):
            errs.append("build %s: %d rebuilt bytes, not 4 x (rebuilt - refused) words" % (bid, b.get("rebuilt_equal", 0)))
    for e in errs:
        print("error: " + e)
    print("exe_tables check: %d builds, %d pointer words in %d runs, %d refused rebuilds, the rule's round trip, "
          "%d error(s)" % (len(rec["build"]), n, len(load_pointers()), sum(len(x) for x in refused.values()), len(errs)))
    return 1 if errs else 0


def cmd_build(a):
    import importer
    ident = importer.identify(a.source)
    if not ident:
        fail("%s: not a build fixtures.toml catalogues" % a.source)
    bid, h = build_from(a.source, ident[0], a.out)
    print("base/exe/ from %s: %s" % (bid, h))


def cmd_verify(a):
    errs = verify_cache(a.cache)
    for e in errs:
        print("error: " + e)
    print("verify base/exe/: %d problem(s)" % len(errs))
    return 1 if errs else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("build")
    p.add_argument("--source", required=True, help="BOF3.exe or a disc image")
    p.add_argument("--out", required=True)
    p.set_defaults(fn=cmd_build)
    p = sub.add_parser("recipe")
    p.add_argument("--game", required=True, help="the directory holding BOF3.exe")
    p.add_argument("--disc", action="append", required=True)
    p.set_defaults(fn=cmd_recipe)
    p = sub.add_parser("measure")
    p.add_argument("--game", required=True, help="the directory holding BOF3.exe")
    p.add_argument("--disc", action="append", required=True)
    p.add_argument("--out", required=True)
    p.set_defaults(fn=cmd_measure)
    p = sub.add_parser("xref")
    p.add_argument("--game", required=True, help="the directory holding BOF3.exe")
    p.add_argument("--disc", action="append", required=True)
    p.add_argument("--out", required=True)
    p.set_defaults(fn=cmd_xref)
    p = sub.add_parser("verify")
    p.add_argument("--cache", required=True)
    p.set_defaults(fn=cmd_verify)
    sub.add_parser("check").set_defaults(fn=cmd_check)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
