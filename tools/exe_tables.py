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
and stay unfilled (docs/exe-import-engine.md section 1). Every pointer word
still unfilled then gets its target from recipes/exe-layout.tsv, the PC's
pointer layout held as addresses (section 7). Before that, what nothing else
filled can come from the code of a reader's PSX twin: recipes/exe-imm.tsv's
owning-function rows (tools/exe_imm.py, docs/exe-immediate-places.md). What
no disc carries stays zero and is listed.

    python tools/exe_tables.py build   --source PATH --out CACHE        # BOF3.exe or a disc
    python tools/exe_tables.py recipe  --game DIR --disc DISC [...]     # recipes/exe.toml + exe-*.tsv
    python tools/exe_tables.py measure --game DIR --disc DISC [...] --out analysis/exe_import
    python tools/exe_tables.py xref    --game DIR --disc DISC [...] --out analysis/exe_import
    python tools/exe_tables.py verify  --cache CACHE
    python tools/exe_tables.py gaps    --game DIR --measure DIR         # recipes/exe-gaps.tsv
    python tools/exe_tables.py gate-skip --cache CACHE --game DIR [--out FILE]   # the gate's BOF3X_STATEHASH_SKIP
    python tools/exe_tables.py check                                    # the recipe alone (CI)
    python tools/exe_tables.py check --game DIR --disc DISC [...]       # + exe-imm.tsv's rows decoded on the discs
    python tools/exe_tables.py feed  --game DIR --disc PSX-JP [--out TSV] [--write]   # code-pointer pairs -> symbols.toml psx

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
LAYOUT = os.path.join(ROOT, "recipes", "exe-layout.tsv")
PSP_FILLS = os.path.join(ROOT, "recipes", "exe-psp.tsv")    # tools/psp_exe.py's (docs/psp-image.md)
IMM = os.path.join(ROOT, "recipes", "exe-imm.tsv")           # the owning-function rows (docs/exe-immediate-places.md)
IMM_COLS = ["build", "kind", "pc", "pc_end", "file", "section", "func", "at", "insns"]
IMM_KINDS = ("imm", "immref")
LAYOUT_COLS = ["pc", "kind", "target"]
KIND = {"text": "code", "data": "data", "bss": "bss", "rdata": "rdata"}    # exe-pointers.tsv's `to` -> the layout's kind
TEXT_LO = 0x401000
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


def func_starts():
    """{pc: name} of symbols.toml's [[func]] starts."""
    with open(os.path.join(ROOT, "symbols.toml"), "rb") as f:
        return {e["pc"]: e["name"] for e in tomllib.load(f).get("func", [])}


def own_bytes():
    """The PC addresses the engine fills itself (the NB group's src/hook/exe_own.cpp
    regions, read through tools/exe_own.py's own_regions; docs/exe-engine-own.md):
    a set; a tree without that tool stops here (the own class cannot be guessed)."""
    try:
        import exe_own
    except ImportError:
        raise SystemExit("tools/exe_own.py is needed for the own class: " + str(sys.exc_info()[1]))
    return {x for lo, hi, _ in exe_own.own_regions() for x in range(lo, hi)}


def own_numbers(pc):
    """The PC's pointer-shaped words inside the engine's own regions: text the
    engine fill writes (docs/exe-engine-own.md), never pointers. {pc address: why}."""
    own = own_bytes()
    return {a: "text in an engine-own region" for a in pointer_words(pc) if any(a + k in own for k in range(4))}


def code_numbers(exe, pc):
    """The PC's pointer-shaped words into .text that are numbers, not pointers
    (docs/exe-import-engine.md section 7.2): {pc address: why}. A code pointer
    points at a function start - a symbols.toml [[func]] start, or an
    instruction boundary right after a `ret` or `int3` (a start the catalogue
    lacks: the C runtime's and the blitters' after `Gfx_PackRgb`). Any other
    value into .text is a number that looks like an address (text such as
    `DAT\\0`, two small halfwords such as a box's x and y): `not an instruction
    boundary` by a linear capstone sweep from the [[func]] start below it, or
    `inside a function` (a boundary, not after a `ret` / `int3`). Needs
    BOF3.exe's .text; decided whether or not a disc places the word."""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    tva, _, traw, trsz = pe_sections(exe)[".text"]
    starts = func_starts()
    order = sorted(starts)
    words = collections.defaultdict(list)          # the [[func]] start below -> [(word, target)]
    for a in pointer_words(pc):
        v = struct.unpack_from("<I", pc, a - DATA_LO)[0]
        if TEXT_LO <= v < RDATA_LO and v not in starts:
            words[order[bisect.bisect_right(order, v) - 1]].append((a, v))
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    out = {}
    for st, ws in words.items():
        want = {v for _, v in ws}
        prev, seen, at, end = {}, set(), st, max(want)
        last = None
        while at <= end:                               # the linear sweep: a byte capstone refuses is skipped
            ins = next(md.disasm(exe[traw + at - tva:traw + at - tva + 16], at), None)
            if at in want:
                seen.add(at)
                prev[at] = last
            if ins is None:
                last, at = None, at + 1
                continue
            last, at = ins.mnemonic, at + ins.size
        for a, v in ws:
            if v not in seen:
                out[a] = "not an instruction boundary"
            elif prev[v] not in ("ret", "int3"):
                out[a] = "inside a function"
    return out


def bss_numbers(pc):
    """The PC's pointer-shaped words into .bss that are numbers, not pointers
    (docs/exe-import-engine.md section 7.2): {pc address: why}. No disc maps
    .bss, so no disc tests these words; a pointer to a .bss variable is
    4-aligned (every one the C runtime's data and the inventory lists hold is),
    so a word into .bss is a number when it is `not 4-aligned` (packed bytes of
    a record, such as the area tables' 12-byte rows), `two small halfwords` (each
    below 0x100 or from 0xFF00: an x and a y, a width and a height - the
    `MsgBox_PlacementTable` and `Title_Sprites` entries that
    docs/exe-code-pointers-midfunc.md read), `text` (printable characters to a
    NUL: the C runtime's `pen`, `%lx`), or `a column of numbers` (aligned by
    chance in a record table whose column holds such numbers either side)."""
    out = {}
    for a in pointer_words(pc):
        v = struct.unpack_from("<I", pc, a - DATA_LO)[0]
        if target_class(v) != "bss":
            continue
        b = pc[a - DATA_LO:a - DATA_LO + 4]
        n = len(b.rstrip(b"\0"))
        if v % 4:
            out[a] = "not 4-aligned"
        elif all(h < 0x100 or h >= 0xFF00 for h in (v & 0xFFFF, v >> 16)):
            # Every .bss address is 0x676000..0x93D6EC, so the high halfword always
            # passes: what this tests is the low one - a pointer within 0x100 bytes
            # of a 64 KiB boundary would be classed a number (none of today's hits
            # is one: IN's "every class that must equal, equal" on the real install).
            out[a] = "two small halfwords"
        elif n >= 2 and all(0x20 <= c < 0x7F for c in b[:n]):
            out[a] = "text"
    # A word aligned by chance in a record table's column of such numbers: two
    # words of the same column (a stride of 8..32 bytes, on one side or both)
    # are numbers by the rules above.
    shape = set(out)
    for a in pointer_words(pc):
        if a in out or target_class(struct.unpack_from("<I", pc, a - DATA_LO)[0]) != "bss":
            continue
        if any((a - s in shape and a + s in shape) or (a + s in shape and a + 2 * s in shape)
               or (a - s in shape and a - 2 * s in shape) for s in range(8, 36, 4)):
            out[a] = "a column of numbers"
    return out


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


def layout_rows(pc, pointers, numbers):
    """recipes/exe-layout.tsv's rows (docs/exe-import-engine.md section 7): every
    pointer word of `pointers` with its kind and its target as an address of the
    PC's layout - a [[func]] start by its symbols.toml name, any other target as
    the address - and every word `numbers` classes a number (kind `number`,
    no target: its bytes are not the layout's to hold)."""
    starts = func_starts()
    rows = []
    for p, n, cls in pointers:
        for a in range(p, p + 4 * n, 4):
            v = struct.unpack_from("<I", pc, a - DATA_LO)[0]
            rows.append((a, KIND[cls], starts[v] if cls == "text" and v in starts else "0x%06X" % v))
    rows += [(a, "number", "-") for a in numbers]
    return sorted(rows)


def write_layout(rows):
    with open(LAYOUT, "w", newline="\n") as f:
        f.write("# recipes/exe-layout.tsv - GENERATED by `tools/exe_tables.py recipe`; do not edit.\n"
                "# The pointer words of BOF3.exe's initialised .data held as layout (docs/exe-import-engine.md\n"
                "# section 7): the word at `pc` holds `target`, an address of the PC's layout - a symbols.toml\n"
                "# [[func]] start by name, any other target as the address. `kind`: code / data / bss / rdata, the\n"
                "# section the target is in; `number`, a word exe_tables.code_numbers / bss_numbers class a number\n"
                "# that looks like an address into .text or .bss (no target: not the layout's to hold). A disc\n"
                "# build fills every pointer word no disc places or rebuilds from here. Addresses only\n"
                "# (CLAUDE.md rule 1).\n")
        f.write("\t".join(LAYOUT_COLS) + "\n")
        for a, kind, t in rows:
            f.write("0x%06X\t%s\t%s\n" % (a, kind, t))


def load_layout(path=LAYOUT):
    """[(pc, kind, target)] from recipes/exe-layout.tsv, target as written."""
    with open(path, encoding="utf-8") as f:
        lines = [l.rstrip("\n") for l in f if not l.startswith("#")]
    if not lines or lines[0].split("\t") != LAYOUT_COLS:
        fail("exe-layout.tsv: the column line is not %s" % "\t".join(LAYOUT_COLS))
    out = []
    for l in lines[1:]:
        a, kind, t = l.split("\t")
        out.append((int(a, 16), kind, t))
    return out


def layout_values(rows=None):
    """{pc address: the word's value} for the layout's pointer words, a [[func]]
    name resolved through symbols.toml."""
    names = {n: a for a, n in func_starts().items()}
    out = {}
    for a, kind, t in (load_layout() if rows is None else rows):
        if kind == "number":
            continue
        if t.startswith("0x"):
            out[a] = int(t, 16)
        elif t in names:
            out[a] = names[t]
        else:
            fail("exe-layout.tsv: 0x%06X's target %s is no symbols.toml [[func]]; rerun `recipe`" % (a, t))
    return out


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


def disc_image(disc, bid, places=(), imm=None):
    """(image, owner, ranges) from a disc alone, before the pointer mask: the
    0x9C000-byte image, a per-byte list of which range wrote it (None:
    nothing), and the ranges [pc, end, how, file, address of pc]. `places`:
    recipes/exe-places.tsv's rows for the build (`place` ranges, after the
    map's segments and before the catalogued tables). `imm`: the owning-function
    rows (recipes/exe-imm.tsv's for the build when None; none while psp_exe's
    recipe builds its base image), applied last to the bytes nothing else
    filled, as `place` ranges whose `from` ends `:imm` / `:immref`."""
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
    import psp_exe   # a PSP build: the game values its disc carries in another shape (docs/psp-image.md)
    for lo, raw, where, addr in psp_exe.fill(disc, bid, owner, srcs=src, ranges=ranges, img=img):
        put(lo, raw, "place", where, addr)
    for lo, raw, where, addr in psp_exe.unwide(bid, img, owner):   # the PSP's 16:9 side placements undone
        put(lo, raw, "place", where, addr)
    if imm is None:
        imm = () if psp_exe._RECIPE_BASE else load_imm().get(bid, ())
    for row in imm:
        for lo, raw, addr in imm_pieces(row, src):
            i = 0
            while i < len(raw):                      # only what nothing else filled
                if owner[lo + i - DATA_LO] is not None:
                    i += 1
                    continue
                j = i
                while j < len(raw) and owner[lo + j - DATA_LO] is None:
                    j += 1
                put(lo + i, raw[i:j], "place", "%s#%d:%s" % (row["file"], row["section"], row["kind"]),
                    addr + i if row["kind"] == "immref" else addr)
                i = j
    return img, owner, ranges


# ------------------------------------------------------------------ the owning-function rows (docs/exe-immediate-places.md)

def load_imm(path=IMM):
    """{build: [row]} from recipes/exe-imm.tsv; none when the file is absent."""
    out = collections.defaultdict(list)
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8") as f:
        lines = [l.rstrip("\n") for l in f if not l.startswith("#")]
    if not lines or lines[0].split("\t") != IMM_COLS:
        fail("exe-imm.tsv: the column line is not %s" % "\t".join(IMM_COLS))
    for l in lines[1:]:
        c = l.split("\t")
        if len(c) != len(IMM_COLS):
            fail("exe-imm.tsv: a row of %d columns" % len(c))
        out[c[0]].append({"build": c[0], "kind": c[1], "pc": int(c[2], 16), "pc_end": int(c[3], 16), "file": c[4],
                          "section": int(c[5]), "func": int(c[6], 16), "at": int(c[7]), "insns": c[8]})
    return out


def write_imm(rows):
    with open(IMM, "w", newline="\n") as f:
        f.write("# recipes/exe-imm.tsv - GENERATED by `tools/exe_tables.py recipe`; do not edit.\n"
                "# The owning-function places (docs/exe-immediate-places.md): [pc, pc_end) of .data no other\n"
                "# range fills, rebuilt from the code of the PSX twin of a PC function that reads it - the\n"
                "# function at `func` in `file` / `section`. immref: `insns` H+L are a lui and the instruction\n"
                "# that adds its low half; the address they form plus `at` bytes is where the row's bytes are in\n"
                "# the file. imm: `insns` are the stores S of the values instructions V make (or a pair H+L, or\n"
                "# z for $zero), `at` the stores' displacement of pc (tools/exe_imm.py). Offsets are hex from\n"
                "# `func`. A row is written only where it rebuilds BOF3.exe's bytes exactly.\n"
                "# Addresses and counts only (CLAUDE.md rule 1).\n")
        f.write("\t".join(IMM_COLS) + "\n")
        for bid in ORDER:
            for r in rows.get(bid, ()):
                f.write("%s\t%s\t0x%06X\t0x%06X\t%s\t%d\t0x%08X\t%d\t%s\n" % (
                    bid, r["kind"], r["pc"], r["pc_end"], r["file"], r["section"], r["func"], r["at"], r["insns"]))


def imm_pieces(row, srcs):
    """[(pc, bytes, disc address)] an owning-function row rebuilds from the disc
    (`srcs`: {(file, section): (base, bytes)}). Stops (rule 4) on a row its disc
    does not decode: the file or section absent, an instruction not of the
    row's kind, the data off the file, or a piece outside [pc, pc_end)."""
    import exe_imm
    key = (row["file"], row["section"])
    if key not in srcs:
        fail("exe-imm.tsv: %s 0x%06X: %s#%d is not on this disc" % (row["build"], row["pc"], key[0], key[1]))
    read = exe_imm.blob_reader(srcs[key][1], srcs[key][0])
    n = row["pc_end"] - row["pc"]
    try:
        if row["kind"] == "immref":
            addr = exe_imm.decode_ref(read, row["func"], *exe_imm.parse_ref(row["insns"])) + row["at"]
            hold = [k for k in srcs if k[0] == row["file"] and srcs[k][0] <= addr
                    and addr + n <= srcs[k][0] + len(srcs[k][1])]
            if not hold:
                raise ValueError("0x%08X + %d is in no section of %s" % (addr, n, row["file"]))
            base, blob = srcs[hold[0]]
            return [(row["pc"], blob[addr - base:addr - base + n], addr)]
        if row["kind"] == "imm":
            out = []
            for off, raw in exe_imm.decode_row(read, row["func"], row["at"], exe_imm.parse_elements(row["insns"])):
                if off + len(raw) > n:
                    raise ValueError("an element past pc_end")
                out.append((row["pc"] + off, raw, row["func"]))
            return out
        raise ValueError("kind %s" % row["kind"])
    except ValueError as e:
        fail("exe-imm.tsv: %s 0x%06X: %s" % (row["build"], row["pc"], e))


def recipe_imm(pc, exe, held, places, pointers, report=print):
    """{build: [row]} - find_imm over every held disc (`held`: discs()), the
    image as the maps, the places and the fills leave it. psx-jp's symbols.toml
    twins reach the other builds through build_twins when psx-jp is held."""
    readers = text_readers(exe)
    cls = {}
    ptr = set()
    for p, n, c in pointers:
        ptr.update(range(p, p + 4 * n))
        for x in range(p, p + 4 * n, 4):
            cls[x] = c
    own = own_bytes()
    starts = data_symbols()[0]
    jp, out = None, {}
    for bid, disc in held:
        srcs = {(f, s): (base, blob) for f, s, base, blob in exe_twins.sources(disc)}
        img, owner, ranges = disc_image(disc, bid, places.get(bid, ()), imm=())
        pairs = code_pairs(pc, img, owner, ranges, cls)
        if bid == "psx-jp":
            jt = sym_twins(srcs, pairs)
            twins = build_twins(srcs, pairs)
            for t, v in jt.items():
                twins[t] |= v
            jp = (srcs, jt)
        else:
            twins = build_twins(srcs, pairs, jp, need=set(jp[1]) if jp else ())
        stats = collections.Counter()
        out[bid] = find_imm(pc, bid, srcs, owner, ptr, own, readers, twins, starts, stats)
        report("%s: %d owning-function rows (%s)" % (bid, len(out[bid]), ", ".join(
            "%s %d" % kv for kv in sorted(stats.items()))))
    return out


IMM_REACH = 0x400      # a reader's operand counts for a region from its data symbol's start, this far below


def text_readers(exe):
    """{a .data address: {PC function}} from BOF3.exe's .text: every 4-byte value at
    any byte offset that is a .data address, credited to the symbols.toml [[func]]
    holding it. The instructions' absolute operands are among them; a value that is
    no operand only adds a candidate, which a row's exact rebuild then refuses."""
    va, _, raw, rsz = pe_sections(exe)[".text"]
    text = exe[raw:raw + rsz]
    starts = sorted(func_starts())
    out = collections.defaultdict(set)
    for k in range(4):
        n = (len(text) - k) // 4
        for i, (v,) in enumerate(struct.iter_unpack("<I", text[k:k + 4 * n])):
            if DATA_LO <= v < DATA_HI:
                j = bisect.bisect_right(starts, va + k + 4 * i) - 1
                if j >= 0:
                    out[v].add(starts[j])
    return out


def imm_regions(pc, owner, ptr, own):
    """The runs of the image no range fills, outside the pointer words and the
    engine's own bytes, that hold a byte the PC has non-zero: [(lo, hi)]."""
    out, i, n = [], 0, len(owner)
    free = lambda i: owner[i] is None and DATA_LO + i not in ptr and DATA_LO + i not in own
    while i < n:
        if not free(i):
            i += 1
            continue
        j = i
        while j < n and free(j):
            j += 1
        if any(pc[i:j]):
            out.append((DATA_LO + i, DATA_LO + j))
        i = j
    return out


def sym_twins(srcs, pairs):
    """{PC function: {((file, section), PSX address)}} from symbols.toml's `psx`
    fields, which are psx-jp's addresses: the section `psx_overlay` names, else
    the boot executable's and each section of a file the build's own placed
    code-pointer words pair the function from, where one holds the address."""
    with open(os.path.join(ROOT, "symbols.toml"), "rb") as f:
        funcs = tomllib.load(f).get("func", [])
    by_name = {os.path.basename(f): f for f, _ in srcs}
    files = collections.defaultdict(set)
    for f, t in pairs:
        files[t].add(f)
    inside = lambda k, a: srcs[k][0] <= a < srcs[k][0] + len(srcs[k][1])
    out = collections.defaultdict(set)
    for e in funcs:
        if "psx" not in e:
            continue
        a = e["psx"]
        if "psx_overlay" in e:
            name, _, sec = e["psx_overlay"].partition("#")
            ks = [(by_name.get(name), int(sec))] if sec else []
        else:
            ks = [k for k in srcs if (k[0] in ("BOOT", "BOOT.BIN") or k[0] in files[e["pc"]])]
        for k in ks:
            if k in srcs and inside(k, a):
                out[e["pc"]].add((k, a))
    return out


def build_twins(srcs, pairs, jp=None, need=()):
    """{PC function: {((file, section), the build's PSX address)}}: every placed
    code-pointer word's pair (code_pairs) whose address a section of its own file
    holds; and, from `jp` - (psx-jp's sources, its sym_twins) for another build -
    each psx-jp twin of a function in `need` found again, by its body
    (exe_imm.body_signature), exactly once in this build's same file section."""
    import exe_imm
    byfile = collections.defaultdict(list)
    for k in srcs:
        byfile[k[0]].append(k)
    out = collections.defaultdict(set)
    for (f, t), w in pairs.items():
        for k in byfile[f]:
            if srcs[k][0] <= w < srcs[k][0] + len(srcs[k][1]):
                out[t].add((k, w))
    if jp is not None:
        jp_srcs, jp_twins = jp
        for t in need:
            for k, g in jp_twins.get(t, ()):
                if k not in srcs:
                    continue
                read = exe_imm.blob_reader(jp_srcs[k][1], jp_srcs[k][0])
                end = exe_imm.extent(read, g)
                sig = exe_imm.body_signature(read, g, end) if end else None
                if not sig or len(sig) < 4:
                    continue
                hits = exe_imm.find_body(sig, srcs[k][1], srcs[k][0])
                if len(hits) == 1:
                    out[t].add((k, hits[0]))
    return out


def find_imm(pc, bid, srcs, owner, ptr, own, readers, twins, data_starts, stats=None):
    """The owning-function rows for one build (docs/exe-immediate-places.md): for
    each region imm_regions gives, each PC function whose operands name it (from
    its data symbol's start, `readers` from text_readers), each of that
    function's twins (build_twins, sym_twins): an `immref` row where an address
    the twin's lui pair forms, displaced as the PC's operand is from the region,
    holds the region's bytes in a section of the twin's file - every number byte
    from the operand to the region's end (or the section's end) equal to the
    PC's, four bytes or more of which two non-zero; or an `imm` row where the
    twin's stores of constants rebuild every non-zero byte of the region, two
    elements or more. A region two different places fit is left (ambiguous),
    as is every region nothing fits. `stats`, a Counter, counts why."""
    import exe_imm
    stats = collections.Counter() if stats is None else stats
    byfile = collections.defaultdict(list)
    for k in srcs:
        byfile[k[0]].append(k)
    rkeys = sorted(readers)
    scans = {}
    rows = []
    for lo, hi in imm_regions(pc, owner, ptr, own):
        stats["regions"] += 1
        j = bisect.bisect_right(data_starts, lo) - 1
        base = data_starts[j] if j >= 0 and data_starts[j] > lo - IMM_REACH else lo
        ops = collections.defaultdict(set)
        for v in rkeys[bisect.bisect_left(rkeys, base):bisect.bisect_left(rkeys, hi)]:
            for f in readers[v]:
                ops[f].add(v)
        if not ops:
            stats["no reader"] += 1
            continue
        if not any(f in twins for f in ops):
            stats["no twin"] += 1
            continue
        stats["a twinned reader"] += 1
        want = pc[lo - DATA_LO:hi - DATA_LO]
        nz = [i for i, b in enumerate(want) if b]
        found = {}
        for f in sorted(ops):
            for k, g in sorted(twins.get(f, ())):
                if (k, g) not in scans:
                    read = exe_imm.blob_reader(srcs[k][1], srcs[k][0])
                    end = exe_imm.extent(read, g)
                    scans[k, g] = (read, exe_imm.scan(read, g, end) if end else ([], []))
                read, (stores, refs) = scans[k, g]
                for _, at, elems in exe_imm.match_run(want, nz, exe_imm.store_bytes(stores)):
                    pieces = exe_imm.decode_row(read, g, at, elems)
                    if len(elems) < 2 or sum(1 for b in b"".join(r for _, r in pieces) if b) < 2:
                        continue
                    r0 = pieces[0][0]
                    rhi = max(o + len(r) for o, r in pieces)
                    row = {"build": bid, "kind": "imm", "pc": lo + r0, "pc_end": lo + rhi, "file": k[0],
                           "section": k[1], "func": g, "at": at + r0, "insns": exe_imm.format_elements(elems)}
                    found.setdefault(("imm", k, g, at), row)
                for addr, offs in refs:
                    if len(offs) != 2:
                        continue
                    try:
                        if exe_imm.decode_ref(read, g, offs[0], offs[1]) != addr:
                            continue
                    except ValueError:
                        continue
                    for a_op in sorted(ops[f]):
                        hold = [kk for kk in byfile[k[0]] if srcs[kk][0] <= addr + (lo - a_op) < srcs[kk][0] + len(srcs[kk][1])]
                        if len(hold) != 1:
                            continue
                        b2, blob2 = srcs[hold[0]]
                        end, ok, agree, agree_nz = hi, True, 0, 0
                        for x in range(min(a_op, lo), hi):
                            o = addr + (x - a_op) - b2
                            if not 0 <= o < len(blob2):
                                if x < lo:
                                    ok = False
                                else:
                                    end = x
                                break
                            if x in ptr:
                                continue
                            if blob2[o] != pc[x - DATA_LO]:
                                ok = False
                                break
                            agree += 1
                            agree_nz += pc[x - DATA_LO] != 0
                        if not ok or end <= lo or not any(want[:end - lo]) or agree < 4 or agree_nz < 2:
                            continue
                        place = (hold[0][0], addr + (lo - a_op))
                        row = {"build": bid, "kind": "immref", "pc": lo, "pc_end": end, "file": k[0], "section": k[1],
                               "func": g, "at": lo - a_op, "insns": "%X+%X" % (offs[0], offs[1])}
                        found.setdefault(("immref",) + place, row)
        if not found:
            stats["nothing fits"] += 1
            continue
        places = {key for key in found if key[0] == "immref"}
        if len(places) > 1 or (not places and len(found) > 1):
            stats["ambiguous"] += 1
            continue
        key = min(places) if places else min(found)
        rows.append(found[key])
        stats["placed " + found[key]["kind"]] += 1
        stats["bytes " + found[key]["kind"]] += sum(1 for x in range(found[key]["pc"], found[key]["pc_end"]) if pc[x - DATA_LO])
    return rows


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


def code_pairs(pc, img, owner, ranges, cls):
    """{(file, PC function): the build's function} from the image's placed
    code-pointer words: every word of exe-pointers.tsv's `text` class that one
    range placed whole pairs the PC's function (the PC's word) with the build's
    (the disc's word), keyed by the file that range came from; a key whose
    words disagree is dropped (docs/exe-import-engine.md 3.1). `cls`: {word
    address: exe-pointers.tsv's class}."""
    pairs, bad = {}, set()
    for a, c in cls.items():
        if c != "text":
            continue
        ks = {owner[a - DATA_LO + j] for j in range(4)}
        if None in ks or len(ks) != 1:
            continue
        key = (ranges[ks.pop()][3].split("#")[0], struct.unpack_from("<I", pc, a - DATA_LO)[0])
        w = struct.unpack_from("<I", img, a - DATA_LO)[0]
        if pairs.setdefault(key, w) != w:
            bad.add(key)
    for key in bad:
        del pairs[key]
    return pairs


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
    A code pointer is a key too once the image has paired it: every placed
    code-pointer word gives (its file, the PC's function) -> the build's
    function, kept where all such words of that file agree; a paired code
    pointer in an unplaced region is searched for in its file like a data
    pointer's target, and checked like one. New places give new targets and
    new pairs, so it runs again until nothing moves. Returns rows for
    recipes/exe-places.tsv."""
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
        img, owner, ranges = disc_image(disc, bid, rows, imm=())
        pieces = sorted((q[2], q[2] + q[1] - q[0], q[3], q[0]) for q in linear_pieces(owner, ranges))
        plo = [q[0] for q in pieces]

        def fwd(t):
            i = bisect.bisect_right(plo, t) - 1
            if i >= 0 and pieces[i][0] <= t < pieces[i][1]:
                return pieces[i][2].split("#")[0], pieces[i][3] + t - pieces[i][0]
            return None
        pairs = code_pairs(pc, img, owner, ranges, cls)
        twins = collections.defaultdict(list)
        for (f, t), w in pairs.items():
            twins[t].append((f, w))
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
        keys, need = {}, collections.defaultdict(set)    # word -> [(file, the build's word)]
        for lo, hi in regions:
            for a in range(lo + (-lo % 4), hi - 3, 4):
                t = struct.unpack_from("<I", pc, a - DATA_LO)[0]
                if cls.get(a) == "data":
                    f = fwd(t)
                    if f:
                        keys[a] = [f]
                elif cls.get(a) == "text" and t in twins:
                    keys[a] = twins[t]
                for f in keys.get(a, ()):
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
        def stretches(lo, hi, place, win, target=None):
            """The stretches of [lo, hi) at `place` (section, displacement) that
            disagree nowhere and are keyed enough to place (step 3). `target`: a
            PC address a pointer from outside the region names the place of
            (section 7.7); a stretch holding its word and 8 equal bytes places."""
            (k, delta), out = place, []
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
                        tw = pairs.get((k[0], struct.unpack_from("<I", pc, w0 - DATA_LO)[0]))
                        res.append((w == tw) if tw is not None else None if code_word(w, psp) else False)
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
                    if nk >= 2 or (nk >= 1 and eq >= 16) or (win is not None and a0 <= win and win + 16 <= a1 and eq >= 24) \
                            or (target is not None and a0 <= target and target + 4 <= a1 and eq >= 8):
                        out.append({"pc": a0, "pc_end": a1, "file": k[0], "section": k[1],
                                    "addr": a0 + delta, "agree": eq, "keys": nk})
                s0 = i2 + 1
            return out
        # the pointers into a region (section 7.7): a placed data-pointer word whose
        # PC target lies in an unplaced region holds, in the disc's own word, the
        # target's address in the build - its place, when one section of the
        # pointer's file (or else of the whole disc) holds that address
        rlos = [r[0] for r in regions]
        incoming = collections.defaultdict(lambda: collections.defaultdict(set))
        for a, c in cls.items():
            if c != "data":
                continue
            ks = {owner[a - DATA_LO + j] for j in range(4)}
            if None in ks or len(ks) != 1:
                continue
            k = ks.pop()
            if ranges[k][2] not in ("map", "table", "place"):
                continue
            t = struct.unpack_from("<I", pc, a - DATA_LO)[0]
            i = bisect.bisect_right(rlos, t) - 1
            if i < 0 or not regions[i][0] <= t < regions[i][1]:
                continue
            w = struct.unpack_from("<I", img, a - DATA_LO)[0]
            hold = [s for s in byfile[ranges[k][3].split("#")[0]] if srcs[s][0] <= w < srcs[s][0] + len(srcs[s][1])]
            if not hold:
                hold = [s for s in srcs if srcs[s][0] <= w < srcs[s][0] + len(srcs[s][1])]
            if len(hold) == 1:
                incoming[regions[i]][t].add((hold[0], w - t))
        new = []
        for lo, hi in regions:
            votes = {"data": collections.Counter(), "text": collections.Counter()}
            for a in range(lo + (-lo % 4), hi - 3, 4):
                for f in keys.get(a, ()):
                    for k in byfile[f[0]]:
                        for h in hits.get((k, f[1]), ()):
                            votes[cls[a]][k, h - a] += 1
            # the data pointers' place when they have one: a code pointer of a
            # resident or much-shared function votes for many places
            top = (votes["data"] or votes["text"] or collections.Counter()).most_common(2)
            got = []
            if top and not (len(top) > 1 and top[1][1] == top[0][1]):
                got = stretches(lo, hi, top[0][0], None)
            if not got:
                # no key, or none that places: a 16-byte window of numbers found once in the
                # files the neighbours come from - any window when no pointer keys the region
                # (section 7.4), a window between two code pointers either way
                near = {ranges[owner[x - DATA_LO]][3].split("#")[0]
                        for x in (lo - 1, hi) if DATA_LO <= x < DATA_HI and owner[x - DATA_LO] is not None}
                ws = [] if top else [(w, False) for w in byte_windows(pc, cls, lo, hi)[:8]]
                for w, anchored in ws + [(w, True) for w in code_windows(pc, cls, lo, hi)[:8]]:
                    pat = pc[w - DATA_LO:w - DATA_LO + 16]
                    found = [(k, base + o - w) for f in sorted(near) for k in byfile[f]
                             for base, blob in [srcs[k]] for o in finds(blob, pat)
                             if not anchored or (o >= 4 and o + 20 <= len(blob)
                                                 and code_word(struct.unpack_from("<I", blob, o - 4)[0], psp)
                                                 and code_word(struct.unpack_from("<I", blob, o + 16)[0], psp))]
                    if len(found) == 1:
                        got = stretches(lo, hi, found[0], w)
                        break
            into = incoming.get((lo, hi), {})
            for t, at in sorted(into.items()):
                if len(at) != 1 or any(r["pc"] <= t < r["pc_end"] for r in got):
                    continue
                for r in stretches(lo, hi, next(iter(at)), None, target=t):
                    if not any(r["pc"] < g["pc_end"] and g["pc"] < r["pc_end"] for g in got):
                        # the pointers into the stretch that name this place are its keys too
                        r["keys"] += sum(1 for t2, at2 in into.items() if r["pc"] <= t2 < r["pc_end"] and at2 == at)
                        got.append(r)
            new += got
        if not new:
            break
        rows = sorted(rows + new, key=lambda r: r["pc"])
    return rows


def byte_windows(pc, cls, lo, hi):
    """The 4-aligned 16-byte windows of [lo, hi) with no pointer word in them and
    at least six different byte values, not overlapping - keys of numbers for a
    region no pointer keys (docs/exe-import-engine.md section 7.4)."""
    out, a = [], lo + (-lo % 4)
    while a <= hi - 16:
        if not any(a + j in cls for j in range(0, 16, 4)) and len(set(pc[a - DATA_LO:a - DATA_LO + 16])) >= 6:
            out.append(a)
            a += 16
        else:
            a += 4
    return out


def code_windows(pc, cls, lo, hi):
    """The 4-aligned 16-byte windows of [lo, hi) with no pointer word in them, a
    code pointer word on either side and not one byte value throughout - keys
    of numbers too uniform for byte_windows, matched only where the build's
    words either side are code addresses too (docs/exe-import-engine.md
    section 7.4)."""
    return [a for a in range(lo + (-lo % 4), hi - 15, 4)
            if cls.get(a - 4) == "text" and cls.get(a + 16) == "text"
            and not any(a + j in cls for j in range(0, 16, 4))
            and len(set(pc[a - DATA_LO:a - DATA_LO + 16])) >= 2]


def finds(blob, pat):
    out, o = [], blob.find(pat)
    while o >= 0:
        out.append(o)
        o = blob.find(pat, o + 1)
    return out


def write_places(places):
    with open(PLACES, "w", newline="\n") as f:
        f.write("# recipes/exe-places.tsv - GENERATED by `tools/exe_tables.py recipe`; do not edit.\n"
                "# What exe_maps/ left unplaced, placed by the PC's own data pointers (docs/exe-import-engine.md\n"
                "# section 3): [pc, pc_end) is the build's `file` / `section` from `addr` on, as in\n"
                "# exe_maps/; `agree` bytes compared equal (numbers equal, data pointers the target's\n"
                "# address in the build), `keys` data pointers voted for the place (in it, or placed\n"
                "# pointers into it, section 7.7); `keys` 0 is a place a 16-byte window of numbers found\n"
                "# once in a neighbour's file (section 7.4), at least 24 bytes agreeing. Addresses and\n"
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


def apply_layout(img, owner, ranges, values):
    """Fill every pointer word still unfilled after the rebuild from the layout
    (recipes/exe-layout.tsv, docs/exe-import-engine.md section 7): one `layout`
    range. A rebuilt word must already hold the layout's value (the disc's own
    word, run backwards, agrees with the PC's layout) - a difference stops the
    build. Returns the words written."""
    k, n = len(ranges), 0
    ranges.append([DATA_LO, DATA_HI, "layout", "exe-layout.tsv", 0])
    for a in sorted(values):
        o = a - DATA_LO
        ks = {owner[o + j] for j in range(4)}
        if ks == {None}:
            struct.pack_into("<I", img, o, values[a])
            owner[o:o + 4] = [k] * 4
            n += 1
        elif struct.unpack_from("<I", img, o)[0] != values[a] or len(ks) != 1 or ranges[ks.pop()][2] != "rebuilt":
            fail("0x%06X: a pointer word of exe-layout.tsv is placed or rebuilt to another value; rerun `recipe`" % a)
    return n


def disc_built(disc, bid, pointers, refused):
    """The disc's image as base/exe/ holds it: placed, the pointer words
    masked, the data pointers the rule rebuilds written back but those the
    recipe refuses for this build, then every pointer word still unfilled from
    the layout table. (image, owner, ranges, rebuilt, outcomes)."""
    img, owner, ranges = disc_image(disc, bid, load_places().get(bid, ()))
    rebuilt, c, _ = rebuild_candidates(img, owner, ranges, pointers)
    mask(img, owner, pointers)
    c["refused"] = sum(1 for a in rebuilt if a in refused)
    c["written"] = apply_rebuild(img, owner, ranges, rebuilt, refused)
    c["layout"] = apply_layout(img, owner, ranges, layout_values())
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


# ------------------------------------------------------------------ the bytes no disc gives: classed by address
#
# recipes/exe-gaps.tsv (the disc-only round, IN; docs/exe-import-engine.md
# section 4.4): per build, every run of a disc-built image's unfilled bytes
# that is not the engine's own (`own`) and holds a byte the PC's .data has
# non-zero, with the class tools/exe_own.py's measurement gives it
# (docs/exe-engine-own.md section 3). A `none` range of data.toml is then a
# range where the PC's bytes are zero too - the loader's zero is exact - and
# every other unfilled byte is named: `language` (a language layer's handlers
# write it or our code reads around it, docs/loc-build-disc-only.md 5.4),
# `unread` (nothing the exe host runs reads it), `keys` (Key_TableDefault, the
# engine's rule), or `gap` (zero where the PC has a value: Capcom's game
# values no disc carries, bytes on the disc the places miss, DIV-0055's
# re-laid plates, and the eight language addresses no layer reaches). The
# table holds addresses, classes and symbol names only (CLAUDE.md rule 1).
GAPS = os.path.join(ROOT, "recipes", "exe-gaps.tsv")
GAP_COLS = ["build", "lo", "hi", "how", "kind", "symbol"]
GAP_HOWS = ("gap", "language", "unread", "keys")
GAP_KINDS = {"gap": ("game-value", "disc-unplaced", "port-relaid", "language-zero", "port-platform", "rule",
                     "game-format"),
             "language": ("language",), "unread": ("unread",), "keys": ("rule",)}
# The eight language addresses that stay zero on a disc-only image (LG,
# docs/loc-build-disc-only.md 5.4): (address, bytes or None for the PC's
# string to its NUL, what). The faerie records' stats are 60 records of 20.
LANGUAGE_ZERO = ([(0x6531F4, None, "the ranked lists' page counter format"),
                  (0x653200, 16, "faerie record 0's name field")]
                 + [(0x653210 + 20 * r, 4, "a faerie record's four stats") for r in range(60)]
                 + [(0x6536B0, 11, "the faerie village areas' limits (areas 175..185)"),
                    (0x669CCC, None, "the empty save slot's text"),
                    (0x669CE8, 5, "the name EquipOpen copies over character record 7's"),
                    (0x66A094, None, "a one-glyph word after a count"),
                    (0x66A098, None, "a menu's title")])


def language_zero(pc):
    """The bytes of LANGUAGE_ZERO, from the PC's image for the string lengths."""
    out = set()
    for a, n, _ in LANGUAGE_ZERO:
        if n is None:
            n = pc.index(b"\0", a - DATA_LO) - (a - DATA_LO)
        out.update(range(a, a + max(n, 1)))
    return out


def load_gaps(path=GAPS):
    """({build: [(lo, hi, how, kind, symbol)]}, [the builds its `# measured:` line
    names]) from recipes/exe-gaps.tsv."""
    rows, measured = collections.defaultdict(list), []
    if not os.path.exists(path):
        return rows, measured
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("# measured:"):
                measured = line.split(":", 1)[1].split()
            if line.startswith("#") or line.split("\t") == GAP_COLS:
                continue
            bid, lo, hi, how, kind, sym = line.split("\t")
            rows[bid].append((int(lo, 16), int(hi, 16), how, kind, sym))
    return rows, measured


def gap_rows(pc, bid, segs):
    """One build's rows: `segs` is exe_own.py measure's <bid>.own.tsv as [(lo, hi,
    cls)] (the segment's class from its readers and the disc search); each byte
    classed as that tool classes it (held, then its BYTES row, then its REGIONS
    row, then the segment's), the language bytes of LANGUAGE_ZERO a gap, and
    the runs of one class holding a non-zero PC byte kept."""
    import exe_own
    held = own_bytes()
    lz = language_zero(pc)
    data = sorted((e["pc"], e["name"]) for e in exe_own.data_symbols()[0])
    dpcs = [p for p, _ in data]

    def what(x, seg_cls):
        if x in held:
            return None
        cls = exe_own.byte_class(x) or exe_own.region_class(x)[0] or seg_cls
        if cls == "language":
            return ("gap", "language-zero") if x in lz else ("language", "language")
        if cls == "rule (RD)":
            return ("keys", "rule")
        if cls == "unread":
            return ("unread", "unread")
        if cls in ("game-value", "disc-unplaced", "port-relaid", "port-platform", "rule", "game-format"):
            return ("gap", cls)     # "rule", "port-platform": in such a region of exe_own.py's, but not held
        fail("%s 0x%06X: class %s has no data.toml class" % (bid, x, cls))

    out = []
    for lo, hi, seg_cls in segs:
        x = lo
        while x < hi:
            c = what(x, seg_cls)
            y = x
            while y < hi and what(y, seg_cls) == c:
                y += 1
            if c is not None and any(pc[i - DATA_LO] for i in range(x, y)):
                k = bisect.bisect_right(dpcs, x) - 1
                if k < 0:
                    sym = "0x%06X" % x
                elif data[k][0] == x:
                    sym = data[k][1]
                else:
                    sym = "%s+0x%X" % (data[k][1], x - data[k][0])
                out.append((x, y, c[0], c[1], sym))
            x = y
    return out


def cmd_gaps(a):
    """recipes/exe-gaps.tsv from exe_own.py measure's per-build tables (its --out)
    and the PC's .data (for which bytes are non-zero, and the string lengths)."""
    pc, _ = pc_data(os.path.join(a.game, "BOF3.exe"))
    builds = [b for b in ORDER if os.path.exists(os.path.join(a.measure, "%s.own.tsv" % b))]
    if not builds:
        fail("%s holds no <build>.own.tsv (tools/exe_own.py measure --out)" % a.measure)
    lines = ["# recipes/exe-gaps.tsv - GENERATED by `tools/exe_tables.py gaps`; do not edit.",
             "# Per build, the runs of a disc-built image of .data that no disc fills and the engine",
             "# does not fill (docs/exe-import-engine.md 4.4) holding a byte the PC's has non-zero, classed",
             "# by tools/exe_own.py's measurement (docs/exe-engine-own.md 3). data.toml names them with `how`;",
             "# an unfilled range outside every row is `none`: the PC's bytes there are zero as well.",
             "# Addresses, classes and symbol names only (CLAUDE.md rule 1).",
             "# measured: " + " ".join(builds),
             "\t".join(GAP_COLS)]
    for bid in builds:
        segs = []
        with open(os.path.join(a.measure, "%s.own.tsv" % bid), encoding="utf-8") as f:
            cols = f.readline().rstrip("\n").split("\t")
            for line in f:
                r = dict(zip(cols, line.rstrip("\n").split("\t")))
                segs.append((int(r["lo"], 16), int(r["hi"], 16), r["cls"]))
        rows = gap_rows(pc, bid, segs)
        by = collections.Counter()
        for lo, hi, how, kind, sym in rows:
            by[how, kind] += sum(1 for i in range(lo, hi) if pc[i - DATA_LO])
            lines.append("%s\t0x%06X\t0x%06X\t%s\t%s\t%s" % (bid, lo, hi, how, kind, sym))
        print("%s: %d rows; PC non-zero bytes by class: %s" % (bid, len(rows), ", ".join(
            "%s/%s %d" % (h, k, n) for (h, k), n in sorted(by.items()))))
    with open(GAPS, "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("-> %s" % os.path.relpath(GAPS, ROOT))


def check_gaps(rec):
    """recipes/exe-gaps.tsv alone: the builds it measured are recipes/exe.toml's
    disc builds, its rows in order, inside .data, of a known class, clear of the
    engine's own regions and of every pointer word."""
    if not os.path.exists(GAPS):
        return ["exe-gaps.tsv: absent (tools/exe_tables.py gaps)"]
    errs = []
    rows, measured = load_gaps()
    discs_ = sorted(b for b in rec["build"] if b != PC)
    if sorted(measured) != discs_:
        errs.append("exe-gaps.tsv: measured %s, recipes/exe.toml's disc builds are %s" % (measured, discs_))
    own = own_bytes()
    ptr = set()
    for p, w, _ in load_pointers():
        ptr.update(range(p, p + 4 * w))
    for bid, rs in rows.items():
        if bid not in measured:
            errs.append("exe-gaps.tsv: rows for %s, which the measured line does not name" % bid)
        last = DATA_LO
        for lo, hi, how, kind, sym in rs:
            if lo < last or hi <= lo or hi > DATA_HI or how not in GAP_HOWS or kind not in GAP_KINDS[how]:
                errs.append("exe-gaps.tsv: %s 0x%06X: out of order, empty, outside .data or of no known class" % (bid, lo))
            if any(x in own or x in ptr for x in range(lo, hi)):
                errs.append("exe-gaps.tsv: %s 0x%06X: over an engine-own region or a pointer word" % (bid, lo))
            last = hi
    return errs

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
             "# disc's own word gives by the map run backwards; `from` is that word), `layout` (a",
             "# pointer word no disc places or rebuilds, filled from recipes/exe-layout.tsv: the PC's",
             "# layout, held as addresses), `own` (no disc carries it; the engine fills it itself,",
             "# src/hook/exe_own.cpp's regions: unfilled here), and for what no disc carries, unfilled,",
             "# by recipes/exe-gaps.tsv: `language` (a language layer's handlers write it or our code reads",
             "# around it), `unread` (nothing the exe host runs reads it), `keys` (Key_TableDefault, the",
             "# engine's rule), `gap` (zero where the PC has a value; `from` is its kind and symbol), and",
             "# `none` (the PC's bytes are zero there too).",
             "", "[image]", 'file = "data.bin"', "va = 0x%06X" % DATA_LO, "size = %d" % len(img),
             "bss_end = 0x%06X" % bss_end, 'sha256 = "%s"' % sha(img), 'build = "%s"' % bid,
             'source = %s' % json.dumps(os.path.basename(os.path.normpath(source))),
             'generated = "%s"' % datetime.date.today().isoformat(), "", "ranges = ["]
    if owner is None:
        lines.append('  [0x%06X, 0x%06X, "exe", ".data"],' % (DATA_LO, DATA_HI))
    else:
        ptr, own = set(), own_bytes()
        for pc, words, _ in pointers:
            ptr.update(range(pc, pc + 4 * words))
        gaps, measured = load_gaps()
        if bid not in measured:
            fail("recipes/exe-gaps.tsv has not measured %s: run exe_own.py measure and exe_tables.py gaps" % bid)
        lines.insert(lines.index("ranges = ["), 'gaps = "recipes/exe-gaps.tsv"')
        lines.insert(lines.index("ranges = ["), "")
        gap = {}
        for lo, hi, how, gkind, sym in gaps[bid]:
            for x in range(lo, hi):
                gap[x] = (how, "%s %s" % (gkind, sym) if how == "gap" else sym)
        kind = lambda x: "own" if x in own else "pointer" if x in ptr else gap.get(x, ("none", ""))[0]
        gap_from = lambda x: gap.get(x, ("none", ""))[1] if kind(x) in GAP_HOWS else ""
        for lo, hi, k in spans(owner, ranges):
            if k is not None and ranges[k][2] == "layout":
                lines.append('  [0x%06X, 0x%06X, "layout", "%s"],' % (lo, hi, ranges[k][3]))
                continue
            if k is not None:
                rpc, _, how, where, addr = ranges[k]
                at = addr if how == "widen" else addr + lo - rpc
                lines.append('  [0x%06X, 0x%06X, "%s", "%s@0x%08X"],' % (lo, hi, how, where, at))
                continue
            p = lo                                      # an unfilled run: pointer words and the rest
            while p < hi:
                kp, wp = kind(p), gap_from(p)
                q = p
                while q < hi and kind(q) == kp and gap_from(q) == wp:
                    q += 1
                lines.append('  [0x%06X, 0x%06X, "%s", "%s"],' % (p, q, kp, wp))
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


def verify_cache(cache, info=None):
    """base/exe/data.bin against data.toml's hash and the recipe's for its build:
    BOF3.exe's .data when the PC was the source, the recorded disc-built image
    otherwise, and on a disc build every `layout` word against
    recipes/exe-layout.tsv's target. Returns a list of problems; `info`, a list,
    gets the layout's count."""
    d = os.path.join(cache, "base", "exe")
    if not os.path.exists(os.path.join(d, "data.toml")):
        return ["base/exe/: absent"]
    with open(os.path.join(d, "data.toml"), "rb") as f:
        meta = tomllib.load(f)
    img = meta["image"]
    with open(os.path.join(d, img["file"]), "rb") as f:
        raw = f.read()
    h = sha(raw)
    rec = load_recipe()["build"].get(img["build"])
    errs = []
    if h != img["sha256"]:
        errs.append("base/exe/%s: hashes %s, data.toml says %s" % (img["file"], h, img["sha256"]))
    if rec is None:
        errs.append("base/exe/: build %s has no image in recipes/exe.toml" % img["build"])
    elif h != rec["sha256"]:
        errs.append("base/exe/%s: not the %s image recipes/exe.toml records" % (img["file"], img["build"]))
    if img["build"] != PC:                       # the layout's words: each the table's target
        values = layout_values()
        words = [a for lo, hi, how, _ in meta["image"]["ranges"] if how == "layout" for a in range(lo, hi, 4)]
        bad = [a for a in words if a not in values or struct.unpack_from("<I", raw, a - DATA_LO)[0] != values[a]]
        left = sum(hi - lo for lo, hi, how, _ in meta["image"]["ranges"] if how == "pointer") // 4
        if bad:
            errs.append("base/exe/: %d layout words are not recipes/exe-layout.tsv's target, the first 0x%06X" % (len(bad), bad[0]))
        if rec is not None and len(words) != rec.get("layout_words"):
            errs.append("base/exe/: %d layout words, recipes/exe.toml says %s" % (len(words), rec.get("layout_words")))
        if left:
            errs.append("base/exe/: %d pointer words left unfilled" % left)
        if info is not None:
            info.append("layout %d of %d words recipes/exe-layout.tsv's targets; pointer words unfilled: %d"
                        % (len(words) - len(bad), len(words), left))
    return errs


# ------------------------------------------------------------------ the gate's skip list

SKIP_FILE = os.path.join(ROOT, "tools", "statehash_skip.txt")
GATE_NOT_PC = ("language", "unread", "gap")           # data.toml classes whose bytes are not the PC's at the map
GATE_FILLED = ("layout", "own", "keys")                # the PC's bytes at the map (ExeImage_CompareWithFile's Fatal)
GATE_PLACED = ("map", "place", "table", "widen")       # the disc's bytes: skipped where they differ from the PC's


def gate_skip(cache, exe, with_filled=False):
    """The state hash's skip list for the gate (docs/state-hash.md 4; the
    disc-only round): tools/statehash_skip.txt as committed, then every range of
    the cache's disc-built image whose bytes the PC-built image does not share
    at the map - the classes that are not the PC's bytes (language, unread,
    gap) whole, and the runs of a disc-placed range whose bytes differ from the
    PC's .data (`exe`, BOF3.exe). With `with_filled` also the ranges the engine
    fills with the PC's bytes (layout, own, keys), which the brief's list names
    and the map proves equal. (text, ranges added, {class: bytes})."""
    d = os.path.join(cache, "base", "exe")
    with open(os.path.join(d, "data.toml"), "rb") as f:
        meta = tomllib.load(f)["image"]
    if meta["build"] == PC:
        fail("%s is built from BOF3.exe: its image is the PC's; the gate wants a disc-built cache" % cache)
    if "gaps" not in meta:
        fail("%s was built before recipes/exe-gaps.tsv: rebuild it" % cache)
    with open(os.path.join(d, meta["file"]), "rb") as f:
        img = f.read()
    pc, _ = pc_data(exe)
    with open(SKIP_FILE, encoding="utf-8") as f:
        text = f.read()
    added, by = [], collections.Counter()
    whole = GATE_NOT_PC + (GATE_FILLED if with_filled else ())
    for lo, hi, how, src in meta["ranges"]:
        if how in whole:
            added.append((lo, hi, "%s %s" % (how, src) if src else how))
            by[how] += hi - lo
        elif how in GATE_PLACED:
            x = lo
            while x < hi:
                if img[x - DATA_LO] == pc[x - DATA_LO]:
                    x += 1
                    continue
                y = x
                while y < hi and img[y - DATA_LO] != pc[y - DATA_LO]:
                    y += 1
                added.append((x, y, "%s, the disc's bytes differ from the PC's" % how))
                by[how] += y - x
                x = y
    lines = [text.rstrip("\n"), "",
             "# --- the gate (tools/exe_tables.py gate-skip, the disc-only round): %s's image (%s)"
             % (os.path.basename(os.path.normpath(cache)), meta["build"]),
             "# against the PC's, every range whose bytes differ at the map; the ranges above are the committed list."]
    lines += ["0x%06X %-7d # gate  %s" % (lo, hi - lo, why) for lo, hi, why in added]
    return "\n".join(lines) + "\n", added, by


def cmd_gate_skip(a):
    text, added, by = gate_skip(a.cache, os.path.join(a.game, "BOF3.exe"), a.with_filled)
    out = a.out or os.path.join(a.cache, "base", "exe", "gate-skip.txt")
    with open(out, "w", newline="\n", encoding="utf-8") as f:
        f.write(text)
    print("gate-skip: the committed list plus %d ranges, %d bytes (%s) -> %s"
          % (len(added), sum(by.values()), ", ".join("%s %d" % kv for kv in sorted(by.items())), out))

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


def decide_pointers(pc, images, numbers=()):
    """The PC's pointer-shaped words that are pointers: all but those every
    build that places them holds equal to the PC's (a number that only looks
    like an address), and those `numbers` (code_numbers, bss_numbers) classes
    numbers by reading .text or by a .bss address's shape - which settles the
    words no disc tests instead of calling them pointers by default. [(pc, words, class)] runs, split by the
    class of what they point at."""
    words = []
    for a in pointer_words(pc):
        if a in numbers:
            continue
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
    own = {x - DATA_LO for x in own_bytes()}
    c = collections.Counter()
    for i in range(len(pc)):
        if owner[i] is None:
            c["own" if i in own else "pointer" if i in ptr else "none", "pc-zero" if pc[i] == 0 else "pc-nonzero"] += 1
        else:
            how = ranges[owner[i]][2]
            c["rebuilt" if how == "rebuilt" else "layout" if how == "layout" else "placed",
              "equal" if img[i] == pc[i] else "differ"] += 1
    return c


def cmd_recipe(a):
    pc, bss_end = pc_data(os.path.join(a.game, "BOF3.exe"))
    with open(os.path.join(a.game, "BOF3.exe"), "rb") as f:
        numbers = code_numbers(f.read(), pc)
    numbers.update(bss_numbers(pc))
    numbers.update(own_numbers(pc))
    print("%d pointer-shaped words into .text or .bss are numbers: %s" % (len(numbers),
                                                                         dict(collections.Counter(numbers.values()))))
    images = []
    for bid, disc in discs(a.disc):
        img, owner, ranges = disc_image(disc, bid, imm=())
        images.append((img, owner, ranges))
        print("%s: %d ranges, %d bytes placed before the pointer mask" % (bid, len(ranges), sum(o is not None for o in owner)))
    pointers = decide_pointers(pc, images, numbers)
    cand = len(pointer_words(pc))
    # The layout's `number` rows: the words code_numbers / bss_numbers settle that the discs
    # alone would have called pointers (the rest every disc already holds equal).
    words = lambda runs: {p + 4 * k for p, n, _ in runs for k in range(n)}
    numbers = sorted(words(decide_pointers(pc, images)) - words(pointers))
    write_layout(layout_rows(pc, pointers, numbers))
    values = layout_values()
    # The places by data pointer come after the pointer decision, which stays the
    # maps' alone, so exe-pointers.tsv does not move with them.
    places = {}
    for bid, disc in discs(a.disc):
        places[bid] = find_places(pc, disc, bid, pointers)
        print("%s: %d places by data pointer, %d bytes" % (bid, len(places[bid]),
                                                            sum(r["pc_end"] - r["pc"] for r in places[bid])))
    write_places(places)
    with open(os.path.join(a.game, "BOF3.exe"), "rb") as f:
        imm = recipe_imm(pc, f.read(), discs(a.disc), places, pointers)
    write_imm(imm)
    images = [disc_image(disc, bid, places[bid], imm.get(bid, [])) for bid, disc in discs(a.disc)]
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
                "# pointers, and are not listed, nor are the words into .text that are no function start\n"
                "# and the words into .bss that cannot be a variable's address (exe_tables.code_numbers,\n"
                "# bss_numbers; exe-layout.tsv's `number` rows). Addresses and counts only (CLAUDE.md rule 1).\n"
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
             'places_sha256 = "%s"' % sha(open(PLACES, "rb").read()),
             'psp = "exe-psp.tsv"',
             'psp_sha256 = "%s"' % sha(open(PSP_FILLS, "rb").read()),
             'imm = "exe-imm.tsv"',
             'imm_sha256 = "%s"' % sha(open(IMM, "rb").read()),
             'layout = "exe-layout.tsv"',
             'layout_sha256 = "%s"' % sha(open(LAYOUT, "rb").read()),
             "layout_numbers = %d" % len(numbers),
             "layout_code_unnamed = %d" % sum(1 for _, k, t in load_layout() if k == "code" and t.startswith("0x")), "",
             "[build.%s]" % PC, 'sha256 = "%s"' % sha(pc), "bss_end = 0x%06X" % bss_end, ""]
    for (bid, _), (img, owner, ranges) in zip(discs(a.disc), images):
        rebuilt, _, _ = rebuild_candidates(img, owner, ranges, pointers)
        mask(img, owner, pointers)
        apply_rebuild(img, owner, ranges, rebuilt, set(refused[bid]))
        nl = apply_layout(img, owner, ranges, values)
        c = counts(pc, img, owner, ranges, pointers)
        if c["layout", "differ"]:
            fail("%s: %d layout bytes differ from the PC's" % (bid, c["layout", "differ"]))
        o = outcomes[bid]
        lines += ["[build.%s]" % bid, 'sha256 = "%s"' % sha(img),
                  "ranges = %d" % len(ranges),
                  "places = %d" % len(places[bid]), "imm_rows = %d" % len(imm.get(bid, ())), "placed_by_pointer = %d" % sum(r["pc_end"] - r["pc"] for r in places[bid]),
                  "placed_equal = %d" % c["placed", "equal"], "placed_differ = %d" % c["placed", "differ"],
                  "pointer_pc_zero = %d" % c["pointer", "pc-zero"], "pointer_pc_nonzero = %d" % c["pointer", "pc-nonzero"],
                  "none_pc_zero = %d" % c["none", "pc-zero"], "none_pc_nonzero = %d" % c["none", "pc-nonzero"],
                  "own_pc_zero = %d" % c["own", "pc-zero"], "own_pc_nonzero = %d" % c["own", "pc-nonzero"],
                  "rebuilt_equal = %d" % c["rebuilt", "equal"],
                  "layout_equal = %d" % c["layout", "equal"], "layout_words = %d" % nl,
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
        apply_layout(img, owner, ranges, layout_values())
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


# What an address is by where it lies, before its symbol's name is asked
# (docs/exe-import-engine.md 5 item 6; docs/exe-chinese-remaining.md 4): the
# exe's string pool and the community block hold the text and names a
# language layer carries, whatever the symbol a raw address is named after
# says; past a state, step or kind table (a raw address named after one) are
# the numbers a region of code pointers keeps unplaced until those pointers
# pair - function twins.
REGIONS = (
    (0x669CCC, 0x66A450, "language: the exe's string pool"),
    (0x6531F4, 0x6536E0, "language: the community block (format, faerie records, area limits)"),
)
CODE_TABLES = ("beside a code-pointer table: numbers its unpaired code pointers keep unplaced (want function twins)",
               r"(States|StateTable|Steps|Kinds|Handlers)$")


def group_of(name, ext, near_ptr, at=None):
    for lo, hi, g in REGIONS:
        if at is not None and lo <= at < hi:
            return g
    if re.search(CODE_TABLES[1], name.split("+")[0]):
        return CODE_TABLES[0]
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
            f.write("0x%06X\t%s\t%s\t%d\t%s\t%s\n" % (v, k, n, ext, group_of(n, ext, near[v], v), "\t".join(per[b][v] for b in bids)))
    for kind in ("symbol", "raw"):
        print("== %s addresses (%d)" % (kind, sum(1 for x in addrs.values() if x[0] == kind)))
        cls = sorted({per[b][v] for b in bids for v in addrs if addrs[v][0] == kind})
        print("   %-18s %s" % ("class", " ".join("%9s" % b for b in bids)))
        for c in cls:
            print("   %-18s %s" % (c, " ".join("%9d" % sum(1 for v in addrs if addrs[v][0] == kind and per[b][v] == c)
                                                for b in bids)))
    first = bids[0]
    for want in ("none", "none+pointer", "partly", "differs", "differs+pointer"):
        g = collections.Counter(group_of(addrs[v][1], addrs[v][2], near[v], v) for v in addrs if per[first][v] == want)
        print("== %s on %s, by group: %s" % (want, first, dict(g.most_common())))
    print("-> %s" % os.path.join(out, "src_addresses.tsv"))


def check_layout(rec):
    """recipes/exe-layout.tsv alone: it hashes as recipes/exe.toml records, its
    rows are in order, 4-aligned and inside .data; its pointer rows are exactly
    exe-pointers.tsv's words with the kind of their `to`, and its `number` rows
    none of them; every code target a [[func]] start by name, or (the starts the
    catalogue lacks, docs/exe-code-pointers-midfunc.md section 6) an address in
    .text that is no [[func]] start, as many as the recipe counts; every other
    target inside its kind's section of the image (.rdata, .data, .bss)."""
    errs = []
    with open(LAYOUT, "rb") as f:
        if sha(f.read()) != rec["meta"].get("layout_sha256"):
            errs.append("exe-layout.tsv: not the file recipes/exe.toml was generated with")
    rows = load_layout()
    want = {}
    for p, w, cls in load_pointers():
        for a in range(p, p + 4 * w, 4):
            want[a] = KIND[cls]
    names = {n: a for a, n in func_starts().items()}
    bss_end = rec["build"][PC]["bss_end"]
    span = {"rdata": (RDATA_LO, DATA_LO), "data": (DATA_LO, DATA_HI), "bss": (DATA_HI, bss_end)}
    last, unnamed, numbers, seen = DATA_LO - 4, 0, 0, set()
    for a, kind, t in rows:
        if a <= last or a % 4 or a + 4 > DATA_HI:
            errs.append("exe-layout.tsv: 0x%06X out of order, unaligned or outside .data" % a)
        last = a
        if kind == "number":
            numbers += 1
            if a in want or t != "-":
                errs.append("exe-layout.tsv: 0x%06X: a number that exe-pointers.tsv lists, or with a target" % a)
            continue
        seen.add(a)
        if want.get(a) != kind:
            errs.append("exe-layout.tsv: 0x%06X is %s, exe-pointers.tsv says %s" % (a, kind, want.get(a)))
        if kind == "code":
            if t.startswith("0x"):
                unnamed += 1
                v = int(t, 16)
                if not TEXT_LO <= v < RDATA_LO or v in names.values():
                    errs.append("exe-layout.tsv: 0x%06X: code target %s outside .text or a [[func]] start by address" % (a, t))
            elif t not in names:
                errs.append("exe-layout.tsv: 0x%06X: code target %s is no symbols.toml [[func]]" % (a, t))
        elif kind in span:
            v = int(t, 16)
            if not span[kind][0] <= v < span[kind][1]:
                errs.append("exe-layout.tsv: 0x%06X: %s target %s outside its section" % (a, kind, t))
        else:
            errs.append("exe-layout.tsv: 0x%06X: kind %s" % (a, kind))
    if set(want) - seen:
        errs.append("exe-layout.tsv: %d pointer words of exe-pointers.tsv not held" % len(set(want) - seen))
    if numbers != rec["meta"].get("layout_numbers") or unnamed != rec["meta"].get("layout_code_unnamed"):
        errs.append("exe-layout.tsv: %d numbers and %d unnamed code targets, the recipe says %s and %s"
                    % (numbers, unnamed, rec["meta"].get("layout_numbers"), rec["meta"].get("layout_code_unnamed")))
    return errs


FEED_BUILD = "psx-jp"        # symbols.toml's `psx` addresses are this build's (SLPS-00990)
FEED_RESIDENT = (0x80010000, 0x80196800)   # a `psx` with no psx_overlay below the first ETC overlay: resident


def feed_pairs(pc, exe, disc):
    """The placed code-pointer words' (PC function, PSX function) pairs of the
    psx-jp image, checked (docs/exe-immediate-places.md 3): [(PC function, file,
    section, PSX address, verdict, why)], verdict one of `new` (no `psx` yet; the
    pair agrees), `overlay` (the `psx` it already has, now with its file),
    `same` (nothing to add), `differs` (another `psx` than the entry's; left),
    `copies` (the PC function pairs with several PSX copies, one per file; left),
    `refused` (the fingerprints disagree), `no func` (no [[func]] starts there)."""
    import exe_imm
    srcs = {(f, s): (base, blob) for f, s, base, blob in exe_twins.sources(disc)}
    cls = {}
    for p, n, c in load_pointers():
        for x in range(p, p + 4 * n, 4):
            cls[x] = c
    img, owner, ranges = disc_image(disc, FEED_BUILD, load_places().get(FEED_BUILD, ()), imm=())
    pairs = code_pairs(pc, img, owner, ranges, cls)
    with open(os.path.join(ROOT, "symbols.toml"), "rb") as f:
        funcs = {e["pc"]: e for e in tomllib.load(f)["func"]}
    starts = sorted(funcs)
    psx_to_pc = {e["psx"]: p for p, e in funcs.items() if "psx" in e and "psx_overlay" not in e
                 and FEED_RESIDENT[0] <= e["psx"] < FEED_RESIDENT[1] and e.get("status") in ("evidence", "verified")}
    va, _, raw, rsz = pe_sections(exe)[".text"]
    text = exe[raw:raw + rsz]
    byfile = collections.defaultdict(list)
    for k in srcs:
        byfile[k[0]].append(k)
    where = collections.defaultdict(set)
    for (f, t), w in pairs.items():
        for k in byfile[f]:
            if srcs[k][0] <= w < srcs[k][0] + len(srcs[k][1]):
                where[t].add((k, w))
    out = []
    for t in sorted(where):
        if t not in funcs:
            out.append((t, None, None, None, "no func", ""))
            continue
        e = funcs[t]
        if len({w for _, w in where[t]}) > 1:
            out.append((t, None, None, None, "copies", "%d PSX addresses" % len({w for _, w in where[t]})))
            continue
        k, w = sorted(where[t])[0]
        fp = exe_imm.psx_fingerprint(exe_imm.blob_reader(srcs[k][1], srcs[k][0]), w)
        i = bisect.bisect_right(starts, t)
        end = min(starts[i] if i < len(starts) else t + 0x400, t + 0x2000)
        ok, why = (False, "no extent") if fp is None else exe_imm.pair_agrees(exe_imm.pc_fingerprint(text, va, t, end), fp, psx_to_pc)
        if not ok:
            verdict = "refused"
        elif "psx" not in e:
            verdict = "new"
        elif e["psx"] != w:
            verdict = "differs"
        elif "psx_overlay" not in e and k[0] not in ("BOOT", "BOOT.BIN"):
            verdict = "overlay"
        else:
            verdict = "same"
        out.append((t, k[0], k[1], w, verdict, why))
    return out


def write_feed(rows, today):
    """symbols.toml: the `new` pairs' psx / psx_overlay / psx_status = "hypothesis"
    / psx_evidence lines, and the `overlay` pairs' psx_overlay, each after its
    entry's `name` (or `psx`) line, keyed by the entry's `pc`. Returns the count."""
    path = os.path.join(ROOT, "symbols.toml")
    with open(path, "rb") as f:
        raw = f.read()
    nl = b"\r\n" if b"\r\n" in raw else b"\n"
    lines = raw.split(nl)
    todo = {t: (f, s, w, v, why) for t, f, s, w, v, why in rows if v in ("new", "overlay")}
    out, cur, n, i = [], None, 0, 0
    block_start = None
    idx = {}
    for j, l in enumerate(lines):          # [[func]] block -> its pc, and the line to insert after
        if l.startswith(b"[["):
            cur = l.strip() == b"[[func]]"
            block_start = j
            continue
        if cur and l.startswith(b"pc = 0x"):
            idx[int(l[5:].strip(), 16)] = block_start
    ins = {}
    for t, (f, s, w, v, why) in todo.items():
        if t not in idx:
            fail("symbols.toml: no [[func]] at 0x%06X" % t)
        j = idx[t] + 1
        after = None
        while j < len(lines) and not lines[j].startswith(b"[[") and lines[j].strip():
            if lines[j].startswith(b"name = ") or lines[j].startswith(b"psx = "):
                after = j
            j += 1
        if after is None:
            fail("symbols.toml: the [[func]] at 0x%06X has no name line" % t)
        ov = "" if f in ("BOOT", "BOOT.BIN") else "%s#%d" % (os.path.basename(f), s)
        new = []
        if v == "new":
            new.append("psx = 0x%08X" % w)
            if ov:
                new.append('psx_overlay = "%s"' % ov)
            new.append('psx_status = "hypothesis"')
            new.append('psx_evidence = "exe_tables.py feed %s: a placed code-pointer word of %s pairs it (psx-jp '
                       'image); the fingerprints agree: %s (docs/exe-immediate-places.md 3)"' % (today, f, why))
        else:
            new.append('psx_overlay = "%s"' % ov)
        ins[after] = [x.encode("utf-8") for x in new]
        n += 1
    for j, l in enumerate(lines):
        out.append(l)
        out += ins.get(j, [])
    with open(path, "wb") as f:
        f.write(nl.join(out))
    return n


def cmd_feed(a):
    pc, _ = pc_data(os.path.join(a.game, "BOF3.exe"))
    held = [(b, d) for b, d in discs(a.disc) if b == FEED_BUILD]
    if not held:
        fail("feed needs the %s disc (symbols.toml's psx addresses are its)" % FEED_BUILD)
    with open(os.path.join(a.game, "BOF3.exe"), "rb") as f:
        rows = feed_pairs(pc, f.read(), held[0][1])
    c = collections.Counter(r[4] for r in rows)
    print("%d PC functions a placed code-pointer word pairs: %s" % (len(rows), dict(sorted(c.items()))))
    for t, f, s, w, v, why in rows:
        if v == "differs":
            print("  differs: 0x%06X pairs with 0x%08X in %s#%s (%s)" % (t, w, f, s, why))
    if a.out:
        with open(a.out, "w", newline="\n") as f:
            f.write("pc\tfile\tsection\tpsx\tverdict\twhy\n")
            for t, fl, s, w, v, why in rows:
                f.write("0x%06X\t%s\t%s\t%s\t%s\t%s\n" % (t, fl or "-", "-" if s is None else s,
                                                         "-" if w is None else "0x%08X" % w, v, why))
    if a.write:
        print("symbols.toml: %d entries written" % write_feed(rows, datetime.date.today().isoformat()))


def check_imm(rec, game=None, held=()):
    """recipes/exe-imm.tsv: it hashes as recipes/exe.toml records (or is absent
    where the recipe names none), its rows per build in order, inside .data, of a
    known kind, over no pointer word, no engine-own byte, no map segment and no
    place of the build, their instructions well formed, as many as the recipe
    counts; and tools/exe_imm.py's decoder on its synthetic code. With the PC
    (`game`) and discs (`held`, discs()): each held build's rows decoded on its
    disc, every byte equal to BOF3.exe's - a row that does not rebuild the PC's
    bytes on the build it names is an error."""
    import exe_imm
    errs = ["exe_imm's synthetic decoder test: " + e for e in exe_imm.selftest()]
    named = rec["meta"].get("imm_sha256")
    if not os.path.exists(IMM):
        if named:
            errs.append("exe-imm.tsv: absent, recipes/exe.toml names one")
        return errs
    with open(IMM, "rb") as f:
        if sha(f.read()) != named:
            errs.append("exe-imm.tsv: not the file recipes/exe.toml was generated with")
    ptr = set()
    for p, w, _ in load_pointers():
        ptr.update(range(p, p + 4 * w))
    own = own_bytes()
    places = load_places()
    rows = load_imm()
    for bid, rs in rows.items():
        if bid not in ORDER:
            errs.append("exe-imm.tsv: rows for %s, not a disc build" % bid)
            continue
        if len(rs) != rec["build"].get(bid, {}).get("imm_rows"):
            errs.append("exe-imm.tsv: %s has %d rows, the recipe says %s" % (bid, len(rs), rec["build"].get(bid, {}).get("imm_rows")))
        taken = [(r["pc"], r["pc_end"]) for r in tables.read_map(os.path.join(tables.MAPS, bid + ".tsv"))]
        taken += [(r["pc"], r["pc_end"]) for r in places.get(bid, ())]
        last = DATA_LO
        for r in rs:
            where = "exe-imm.tsv: %s 0x%06X" % (bid, r["pc"])
            if r["kind"] not in IMM_KINDS or not DATA_LO <= r["pc"] < r["pc_end"] <= DATA_HI or r["pc"] < last:
                errs.append(where + ": unknown kind, empty, outside .data or out of order")
            last = r["pc_end"]
            if any(x in ptr or x in own for x in range(r["pc"], r["pc_end"])):
                errs.append(where + ": over a pointer word or an engine-own byte")
            if any(lo < r["pc_end"] and r["pc"] < hi for lo, hi in taken):
                errs.append(where + ": over a map segment or a place")
            try:
                if r["kind"] == "immref":
                    exe_imm.parse_ref(r["insns"])
                elif r["kind"] == "imm":
                    exe_imm.parse_elements(r["insns"])
            except ValueError as e:
                errs.append("%s: %s" % (where, e))
    if game and held:
        pc, _ = pc_data(os.path.join(game, "BOF3.exe"))
        for bid, disc in held:
            srcs = {(f, s): (base, blob) for f, s, base, blob in exe_twins.sources(disc)}
            for r in rows.get(bid, ()):
                try:
                    pieces = imm_pieces(r, srcs)
                except SystemExit as e:
                    errs.append(str(e))
                    continue
                for lo, raw, _ in pieces:
                    if not r["pc"] <= lo <= lo + len(raw) <= r["pc_end"] or raw != pc[lo - DATA_LO:lo - DATA_LO + len(raw)]:
                        errs.append("exe-imm.tsv: %s 0x%06X: does not rebuild BOF3.exe's bytes on its disc" % (bid, r["pc"]))
                        break
    return errs


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
            if (r["pc"] < last or r["pc_end"] <= r["pc"] or r["pc_end"] > DATA_HI or (r["keys"] < 1 and r["agree"] < 24)
                    or (i >= 0 and segs[i]["pc_end"] > r["pc"])):
                errs.append("exe-places.tsv: %s 0x%06X: out of order, empty, outside .data, keyless or over a map segment"
                            % (bid, r["pc"]))
            last = r["pc_end"]
    errs += check_layout(rec)
    errs += check_gaps(rec)
    import psp_exe
    with open(PSP_FILLS, "rb") as f:
        if sha(f.read()) != rec["meta"].get("psp_sha256"):
            errs.append("exe-psp.tsv: not the file recipes/exe.toml was generated with")
    errs += psp_exe.check()
    game, held = getattr(a, "game", None), getattr(a, "disc", None)
    errs += check_imm(rec, game, discs(held) if game and held else ())
    _, _, builds = tables.load()
    keys = ("placed_equal", "placed_differ", "pointer_pc_zero", "pointer_pc_nonzero", "none_pc_zero", "none_pc_nonzero",
            "rebuilt_equal", "layout_equal", "own_pc_zero", "own_pc_nonzero")
    for bid, b in rec["build"].items():
        if bid not in builds:
            errs.append("build %s: not in fixtures.toml" % bid)
        if not re.fullmatch(r"[0-9a-f]{64}", b.get("sha256", "")):
            errs.append("build %s: sha256" % bid)
        if bid != PC and sum(b.get(k, 0) for k in keys) != rec["meta"]["size"]:
            errs.append("build %s: the counts add to %d, not %d" % (bid, sum(b.get(k, 0) for k in keys), rec["meta"]["size"]))
        if bid != PC and b.get("rebuilt_equal", 0) != 4 * (b.get("rebuild_words", 0) - b.get("rebuild_refused", 0)):
            errs.append("build %s: %d rebuilt bytes, not 4 x (rebuilt - refused) words" % (bid, b.get("rebuilt_equal", 0)))
        if bid != PC and (b.get("layout_equal") != 4 * b.get("layout_words", -1)
                          or b.get("rebuilt_equal", 0) // 4 + b.get("layout_words", 0) != rec["meta"]["pointer_words"]
                          or b.get("pointer_pc_zero", 0) + b.get("pointer_pc_nonzero", 0)):
            errs.append("build %s: the rebuilt and layout words are not every pointer word, or a pointer byte is "
                        "left unfilled" % bid)
    for e in errs:
        print("error: " + e)
    print("exe_tables check: %d builds, %d pointer words in %d runs, %d refused rebuilds, the rule's round trip, "
          "the layout (%d words, %d numbers), %d error(s)" % (len(rec["build"]), n, len(load_pointers()),
                                                            sum(len(x) for x in refused.values()), len(load_layout()),
                                                            rec["meta"].get("layout_numbers", 0), len(errs)))
    return 1 if errs else 0


def cmd_build(a):
    import importer
    ident = importer.identify(a.source)
    if not ident:
        fail("%s: not a build fixtures.toml catalogues" % a.source)
    bid, h = build_from(a.source, ident[0], a.out)
    print("base/exe/ from %s: %s" % (bid, h))


def cmd_verify(a):
    info = []
    errs = verify_cache(a.cache, info)
    for e in errs:
        print("error: " + e)
    for i in info:
        print("verify base/exe/: " + i)
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
    p = sub.add_parser("gaps")
    p.add_argument("--game", required=True, help="the directory holding BOF3.exe")
    p.add_argument("--measure", required=True, help="tools/exe_own.py measure's --out (its <build>.own.tsv)")
    p.set_defaults(fn=cmd_gaps)
    p = sub.add_parser("gate-skip")
    p.add_argument("--cache", required=True, help="a disc-built cache")
    p.add_argument("--game", required=True, help="the directory holding BOF3.exe (the PC's .data to compare with)")
    p.add_argument("--out", help="default <cache>/base/exe/gate-skip.txt")
    p.add_argument("--with-filled", action="store_true",
                   help="also skip the ranges the engine fills with the PC's bytes (layout, own, keys)")
    p.set_defaults(fn=cmd_gate_skip)
    p = sub.add_parser("feed")
    p.add_argument("--game", required=True, help="the directory holding BOF3.exe")
    p.add_argument("--disc", action="append", required=True, help="the psx-jp disc (others are ignored)")
    p.add_argument("--out", help="a TSV of every pair and its verdict (analysis/, not the tree)")
    p.add_argument("--write", action="store_true", help="write the checked pairs into symbols.toml")
    p.set_defaults(fn=cmd_feed)
    p = sub.add_parser("check")
    p.add_argument("--game", help="with --disc: decode each held build's owning-function rows against BOF3.exe here")
    p.add_argument("--disc", action="append", help="a disc whose build's exe-imm.tsv rows to decode (with --game)")
    p.set_defaults(fn=cmd_check)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
