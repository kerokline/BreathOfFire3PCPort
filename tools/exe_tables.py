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
build's address, never the PC's. What no disc carries stays zero and is
listed.

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


def disc_image(disc, bid):
    """(image, owner, ranges) from a disc alone, before the pointer mask: the
    0x9C000-byte image, a per-byte list of which range wrote it (None:
    nothing), and the ranges [pc, end, how, file, address of pc]."""
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
             "# word of the PC's: unfilled, never copied), `none` (no disc carries it: unfilled).",
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
    img, owner, ranges = disc_image(psx_disc.Disc(path), bid)
    mask(img, owner, pointers)
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
            c["placed", "equal" if img[i] == pc[i] else "differ"] += 1
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
             "pointer_words = %d" % sum(n for _, n, _ in pointers), "",
             "[build.%s]" % PC, 'sha256 = "%s"' % sha(pc), "bss_end = 0x%06X" % bss_end, ""]
    for (bid, _), (img, owner, ranges) in zip(discs(a.disc), images):
        mask(img, owner, pointers)
        c = counts(pc, img, owner, ranges, pointers)
        lines += ["[build.%s]" % bid, 'sha256 = "%s"' % sha(img),
                  "ranges = %d" % len(ranges),
                  "placed_equal = %d" % c["placed", "equal"], "placed_differ = %d" % c["placed", "differ"],
                  "pointer_pc_zero = %d" % c["pointer", "pc-zero"], "pointer_pc_nonzero = %d" % c["pointer", "pc-nonzero"],
                  "none_pc_zero = %d" % c["none", "pc-zero"], "none_pc_nonzero = %d" % c["none", "pc-nonzero"],
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
    for bid, disc in discs(a.disc):
        img, owner, ranges = disc_image(disc, bid)
        xl = translatable(bid, pc, list(owner), bytes(img), pointers)
        mask(img, owner, pointers)
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
        summary[bid] = {"counts": {"%s/%s" % k: v for k, v in c.items()}, "pointers": xl,
                        "identical": sum(x == y for x, y in zip(img, pc)), "tables": idt,
                        "differ_by_how": dict(collections.Counter(ranges[owner[i]][2] for i in range(len(pc))
                                                                  if owner[i] is not None and img[i] != pc[i]))}
        print("== %s: %d of %d bytes identical; %s" % (bid, summary[bid]["identical"], len(pc), dict(c)))
        print("   differing placed bytes by how: %s" % summary[bid]["differ_by_how"])
        print("   pointer words: %s" % xl)
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
    for bid, disc in discs(a.disc):
        img, owner, ranges = disc_image(disc, bid)
        mask(img, owner, pointers)
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
    _, _, builds = tables.load()
    keys = ("placed_equal", "placed_differ", "pointer_pc_zero", "pointer_pc_nonzero", "none_pc_zero", "none_pc_nonzero")
    for bid, b in rec["build"].items():
        if bid not in builds:
            errs.append("build %s: not in fixtures.toml" % bid)
        if not re.fullmatch(r"[0-9a-f]{64}", b.get("sha256", "")):
            errs.append("build %s: sha256" % bid)
        if bid != PC and sum(b.get(k, 0) for k in keys) != rec["meta"]["size"]:
            errs.append("build %s: the counts add to %d, not %d" % (bid, sum(b.get(k, 0) for k in keys), rec["meta"]["size"]))
    for e in errs:
        print("error: " + e)
    print("exe_tables check: %d builds, %d pointer words in %d runs, %d error(s)"
          % (len(rec["build"]), n, len(load_pointers()), len(errs)))
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
