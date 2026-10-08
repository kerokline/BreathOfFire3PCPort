#!/usr/bin/env python
"""The unified-data importer: recipes, identity, plan, cache, verify.

docs/importer.md; docs/unified-data-plan.md section 3 (step 2). One recipe
file per target build says, for every chunk of every container, which
held builds carry it byte for byte and how to get it out of each. The
importer resolves every chunk against the player's ordered sources and writes
one cache: `base/` (language-neutral) and `loc/<tag>/` (a BCP 47 tag from
fixtures.toml: zh-CN, en-US, en-150, ...), each a tree of
ordinary DAT containers, plus a manifest recording where every chunk came
from.

    python tools/importer.py recipes  --dat DAT --disc JP --disc US ... [--out recipes/pc-zh.toml]
    python tools/importer.py identify PATH ...
    python tools/importer.py build    --source PATH [--source PATH ...] --out CACHE
    python tools/importer.py verify   --cache CACHE
    python tools/importer.py check    (CI: the recipe against fixtures/, no game data)

`recipes` is the generator: it hashes every section of every disc given
(type-1 sections also decoded, tools/type1.py), and every PC chunk is looked up
by its hash, so **every source a recipe names is a proven byte-for-byte match**.
A chunk no disc carries keeps the PC install as its only source; its class
(region_diff.py's, PC against JP) decides its layer. Besides `copy` and
`type1`, three transforms make a disc a source of what the port changed
(`widen` the enemy tables, whose names go to the language layer; `icons`;
`ryud`), and a PSX disc's own section can stand in (`own`) where the PC kept
Japan's language or drew its own art - docs/importer-transforms.md (step 3).
`build` also writes base/exe/, BOF3.exe's .data in the PC's layout, from the
PC's executable or else the first disc (tools/exe_tables.py, docs/exe-import.md,
step 8). The recipe holds names,
indices, sizes and hashes - never bytes - so it is committed. The cache is game
data: it lives outside the repo or under analysis/ (CLAUDE.md rule 1).
"""
import argparse
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
import dat          # noqa: E402
import exe_tables   # noqa: E402
import type1        # noqa: E402

RECIPE = os.path.join(ROOT, "recipes", "pc-zh.toml")
TARGET = "pc-zh"
# The order a recipe lists a chunk's disc sources in; the player's source
# order, not this, decides which one is used.
DISC_ORDER = ("psx-jp", "psx-us", "psx-eu-en", "psx-fr", "psx-de", "psp-jp", "psp-eu")
TEXT_CLASSES = ("text", "layout+text")


def sha(b):
    return hashlib.sha256(b).hexdigest()


# ---------------------------------------------------------------- identity

def _fixtures():
    with open(os.path.join(ROOT, "fixtures.toml"), "rb") as f:
        return tomllib.load(f)


def tag_of(bid):
    """A build's language tag (fixtures.toml `tag`, BCP 47): the name of its
    language layer."""
    return next(b["tag"] for b in _fixtures()["build"] if b["id"] == bid)


def primary(tag):
    return tag.split("-", 1)[0].lower()


def _manifest_rows(rel):
    with open(os.path.join(ROOT, rel), newline="") as f:
        return {n: (int(s), h) for n, s, h in (l.split("\t") for l in f.read().replace("\r\n", "\n").splitlines())}


def identify(path):
    """(build id, artifact name) when a path matches a catalogued per-file
    manifest whole, or (a single file) a catalogued artifact's sha256; None
    otherwise. Unknown builds import nothing."""
    import region_diff
    if os.path.isfile(path) and not re.search(r"\.(cue|bin|iso)$", path, re.I):
        with open(path, "rb") as f:
            h = sha(f.read())
        for b in _fixtures()["build"]:
            for name, art in b.get("artifacts", {}).items():
                if art.get("sha256") == h:
                    return b["id"], name
        return None
    trees = {}
    for b in _fixtures()["build"]:
        for name, art in b.get("artifacts", {}).items():
            if "manifest" not in art:
                continue
            if os.path.isdir(path) != (art.get("match") is not None):
                continue
            m = art.get("match")
            if m not in trees:
                body = region_diff.manifest(path, m)[0]
                trees[m] = {n: (int(s), h) for n, s, h in (l.split("\t") for l in body.splitlines() if l)}
            if trees[m] == _manifest_rows(art["manifest"]):
                return b["id"], name
    return None


class PcSource:
    def __init__(self, bid, path):
        self.id, self.path, self._cache = bid, path, {}

    def chunks(self, name):
        if name not in self._cache:
            self._cache = {name: dat.load(os.path.join(self.path, name))}
        return self._cache[name]

    def get(self, name, slot, how):
        blob, chunks = self.chunks(name)
        c = chunks[slot]
        return blob[c.offset:c.offset + c.size]


class DiscSource:
    def __init__(self, bid, path):
        import region_diff
        self.id, self.path = bid, path
        self.build = region_diff.Build(bid, path)
        self._key, self._secs = None, None

    def sections(self, key):
        if key != self._key:
            self._key, self._secs = key, {s[0]: s for s in self.build.sections(key)}
        return self._secs

    def get(self, name, slot, how):
        _, key, idx = how
        b = self.sections(key)[idx][3]
        return TRANSFORMS[how[0]](b)


class ExeSource:
    def __init__(self, bid, path):
        self.id, self.path = bid, path


# ---------------------------------------------------------------- transforms
#
# Each takes one disc section and returns the PC's chunk, or None when the
# section is not that shape. The generator indexes every section through each
# (disc_index), so a recipe names a transform only where its output was hashed
# equal to the PC's chunk; the build hashes it again before writing.

# widen-enemy-names. The enemy table, AREAnnn's kind-0 chunk at 0xC2000 (the
# disc's 0x800E4000): a 0x48-byte header and eight records, on every disc of
# stride 0x88 with an 8-byte name first, on the PC of stride 0x8C with a
# 12-byte name (DAT_CONTAINER.md section 2). Past the names every byte is the
# same in all seven held builds, 200 of 200 tables (docs/importer-transforms.md
# section 2). The names are glyph codes of a language layer's own font
# (loc_build.encode_text, DIV-0053), so `base/` holds the table with every name
# blank and each language layer lays its names over it, one 12-byte kind-0
# chunk per record - the chunks loc_build.py already writes for en / fr / de / ja.
ENEMY_TAG, ENEMY_DEST, ENEMY_HEAD, ENEMY_COUNT = 0xC2000, 0x000E4000, 0x48, 8
DISC_STRIDE, DISC_NAME, PC_STRIDE, PC_NAME = 0x88, 8, 0x8C, 12


def widen_enemies(s):
    """A disc's enemy table in the PC's layout, every name blank."""
    if len(s) != ENEMY_HEAD + ENEMY_COUNT * DISC_STRIDE:
        return None
    return s[:ENEMY_HEAD] + b"".join(
        bytes(PC_NAME) + s[ENEMY_HEAD + k * DISC_STRIDE + DISC_NAME:ENEMY_HEAD + (k + 1) * DISC_STRIDE]
        for k in range(ENEMY_COUNT))


def split_enemies(c):
    """The PC's enemy table -> (it with every name blank, [(tag, 12-byte name)]
    for each record whose name field is not all zero)."""
    body, names = bytearray(c), []
    for k in range(ENEMY_COUNT):
        at = ENEMY_HEAD + k * PC_STRIDE
        if any(c[at:at + PC_NAME]):
            names.append((ENEMY_TAG + at, bytes(c[at:at + PC_NAME])))
            body[at:at + PC_NAME] = bytes(PC_NAME)
    return bytes(body), names


# respace-icons. FIRST's image page 0x080F0201: the port moved the seven
# item-type icons (24 x 24 px, 4 bpp, on a 24-px pitch along the page's top)
# to a 32-px pitch and changed no pixel (docs/importer-transforms.md section 4).
# In halfwords: 6 wide, from column 6i to 8i, rows 0..23; the page is two
# tiles of 32 x 32 halfwords side by side.
ICON_PAGE, ICONS, ICON_W, ICON_H, ICON_FROM, ICON_TO = 0x080F0201, 7, 6, 24, 6, 8
PAGE_COLS, TILE = 2, 0x800


def respace_icons(s):
    if len(s) % (PAGE_COLS * TILE):
        return None
    out = bytearray(s)
    for y in range(ICON_H):
        at = [(x // 32) * TILE + (y * 32 + x % 32) * 2 for x in range(PAGE_COLS * 32)]  # halfword x of row y
        row = [s[o:o + 2] for o in at]
        new = [b"\0\0"] * len(row)
        for i in range(ICONS):
            new[ICON_TO * i:ICON_TO * i + ICON_W] = row[ICON_FROM * i:ICON_FROM * i + ICON_W]
        for o, hw in zip(at, new):
            out[o:o + 2] = hw
    return bytes(out)


# The RYUD00..03 byte. Ryu's form data: the PC holds 0xFF where every disc
# holds 0xF7, at one offset, in all four files - unexplained (region-diff.md 9),
# but the PC's, so base/ carries it.
RYUD_BYTE, RYUD_WAS, RYUD_IS = 0x7ACE, 0xF7, 0xFF
RYUD_FILES = ("RYUD00", "RYUD01", "RYUD02", "RYUD03")


def ryud_byte(s):
    if len(s) <= RYUD_BYTE or s[RYUD_BYTE] != RYUD_WAS:
        return None
    return s[:RYUD_BYTE] + bytes([RYUD_IS]) + s[RYUD_BYTE + 1:]


TRANSFORMS = {"copy": lambda s: s, "type1": type1.decode, "widen": widen_enemies,
              "icons": respace_icons, "ryud": ryud_byte}


def open_source(path):
    ident = identify(path)
    if not ident:
        raise SystemExit("%s: not a build fixtures.toml catalogues whole - nothing is imported from it" % path)
    bid, art = ident
    if art == "exe":
        return ExeSource(bid, path)
    return (PcSource if os.path.isdir(path) else DiscSource)(bid, path)


# ---------------------------------------------------------------- the recipe generator

def disc_index(src):
    """sha256 -> [(how, emi key, section index)] for every section of a disc;
    a PSX type-1 section is indexed by its decode too (the PSP's ship
    decompressed under the same type number, region-diff.md 5.1)."""
    idx = collections.defaultdict(list)
    for key in sorted(src.build.emis):
        stem = key.rsplit("/", 1)[-1][:-4]
        for i, t, dest, b in src.build.sections(key):
            idx[sha(b)].append(("copy", key, i))
            if t == 1 and src.id.startswith("psx-"):
                idx[sha(type1.decode(b))].append(("type1", key, i))
            for how, wanted in (("widen", dest & 0x7FFFFFFF == ENEMY_DEST),
                                ("icons", t == 3 and dest == ICON_PAGE and stem == "FIRST"),
                                ("ryud", stem in RYUD_FILES)):
                out = TRANSFORMS[how](b) if wanted else None
                if out is not None:
                    idx[sha(out)].append((how, key, i))
    return idx


def best(cands, stem, census_key, census_sec):
    """One locator among byte-identical candidates: the census's own pairing,
    then the same-named EMI, then the first."""
    for c in cands:
        if c[1] == census_key and c[2] == census_sec:
            return c
    same = [c for c in cands if c[1].rsplit("/", 1)[-1][:-4] == stem]
    return (same or cands)[0]


def classify_rows(region_json):
    """(DAT stem, chunk index) -> (class, what) from region_diff.py pc."""
    out = {}
    r = json.load(open(region_json))
    for key, rows in r["files"].items():
        stem = key.rsplit("/", 1)[-1][:-4]
        for row in rows:
            if row.get("b") is None:
                continue
            cls = "identical" if row["verdict"] == "identical" else row.get("class", "differs")
            out[(stem, row["b"])] = (cls, row.get("what", ""), key, row["a"])
    return out


TYPE1_EDITS = ("BPLD012", "BPLD015", "BPLD016", "BPLD27A", "BPLU27A", "START",
               "PL012", "PL025", "PL026", "PL247", "PL257", "PL267", "PL278", "PL27A")


# The world map's dial page (kind 1, tag 0x0A081000) in the 16 world-map
# areas: region_diff.py calls it "words painted on" by its destination, but
# the port's change is one 14-tile block, byte-identical in all 16, and it holds
# no Chinese - the keyboard controls legend (SPACE / X / ENTER keys with
# pictograms, for the PlayStation's buttons and words), the button glyphs
# removed, the gauge bar and the ENGINE / OVER HEAT frames restyled, their
# words English as on the JP disc (rendered and read, 2026-10-08,
# docs/importer.md section 3). Port art every language on the PC wants: base.
DIAL_PAGE = 0x0A081000


def why_pc_only(stem, c, cls):
    """The reason a chunk has no disc source, in a few words."""
    if c.kind == 1 and c.tag == DIAL_PAGE and cls and cls[1] == "an area page (words painted on)":
        return "art", "the world map's dial page: the port's keyboard legend and gauge frames, no text"
    if c.kind == 2:
        return "bank", "audio bank: the disc's VAG samples as WAV (step 6, wave-from-vag)"
    if c.kind == 3:
        return "font", "the port's Chinese font (asset-loading-path.md section 2)"
    if cls is None:
        return "unpaired", "no census pair"
    k, what = cls[0], cls[1]
    if k == "layout" and stem in TYPE1_EDITS:
        return "pc-edit", "type 1 decoded, then edited by the port (type1-compression.md section 3)"
    return k, what


def toml_str(s):
    return json.dumps(s, ensure_ascii=False)


# The own-section stand-ins (docs/importer-transforms.md section 5). A chunk the
# PC carries as JP's bytes, or as the port's art, that a Western disc carries
# only in its own form: from that disc alone, the disc's own section at the
# same place stands in, at the PC's tag (the Western discs' kind-0 sections sit
# 0x8000 higher; the tag, not the destination, places a chunk). Allowed only
# for the kinds below, each hashed into the recipe per build.
OWN_WHY = {
    "language": "the disc's own language in a section the PC kept as Japan's: its plates, pages, glyph atlases",
    "page-clut": "the palette the disc's own repainted area page is drawn with",
    "div-0080": "AREA004's map band and placement map as every later disc has them: DIV-0080's walls already in",
    "port-art": "the disc's own dial page, the PlayStation's buttons where the port drew its keyboard legend",
}
OWN_FROM = ("psx-jp", "psx-us", "psx-eu-en", "psx-fr", "psx-de")


def load_pairs(region_dir, discs):
    """{build: {(JP EMI key, JP section): region_diff.py pair row}} for every
    PSX disc but JP's own."""
    out = {}
    for s in discs:
        if s.id == "psx-jp" or s.id not in OWN_FROM:
            continue
        p = os.path.join(region_dir, "psx-jp_vs_%s.json" % s.id)
        if not os.path.exists(p):
            raise SystemExit("%s missing: python tools/region_diff.py pair psx-jp=JP %s=DISC --out %s" % (p, s.id, p))
        rows = {}
        for key, rs in json.load(open(p))["files"].items():
            for row in rs:
                if row.get("a") is not None and row.get("b") is not None:
                    rows[(key, row["a"])] = row
        out[s.id] = rows
    return out


def own_sections(c, k, jp_at, carried, discs, pairs, stem):
    """[(build, EMI key, section, sha256, why)] for each PSX disc that lacks the
    chunk but carries its own form of it (OWN_WHY)."""
    out = []
    for s in discs:
        if s.id not in OWN_FROM or s.id in carried or not jp_at:
            continue
        if s.id == "psx-jp":
            if k != "art":
                continue
            key, i, why = jp_at[0], jp_at[1], "port-art"
        else:
            row = pairs[s.id].get(tuple(jp_at))
            if not row:
                continue
            pc_class = row.get("class") if row["verdict"] != "identical" else None
            if k == "art":
                why = "port-art"
            elif pc_class == "text":
                why = "language"
            elif pc_class == "logic-data" and stem == "AREA004":
                why = "div-0080"
            elif pc_class == "art" and row.get("what") == "a CLUT section (palettes)":
                why = "page-clut"
            else:
                continue
            key, i = jp_at[0], row["b"]
        _, t, dest, b = s.sections(key)[i]
        if c.kind == 1 and (t != 3 or dest != c.tag):
            continue
        out.append((s.id, key, i, sha(b), why))
    return out


def cmd_recipes(a):
    pc = open_source(a.dat)
    if pc.id != TARGET:
        raise SystemExit("%s is %s, not the %s DAT tree" % (a.dat, pc.id, TARGET))
    discs = [open_source(p) for p in a.disc]
    discs.sort(key=lambda s: DISC_ORDER.index(s.id) if s.id in DISC_ORDER else 99)
    print("indexing %s" % ", ".join(s.id for s in discs))
    idx = {s.id: disc_index(s) for s in discs}
    cls = classify_rows(a.region)
    pairs = load_pairs(a.pairs, discs)
    names = sorted(_manifest_rows("fixtures/pc-zh.DAT.files.tsv"))
    lines, stats = [], collections.Counter()
    for name in names:
        stem = name[:-4]
        blob, chunks = pc.chunks(name)
        lines.append("\n[[file]]\nname = %s\nchunks = [" % toml_str(name))
        rows_out = []
        for c in chunks:
            body = blob[c.offset:c.offset + c.size]
            h = sha(body)
            hb = None
            if c.kind == 0 and c.tag == ENEMY_TAG and c.size == ENEMY_HEAD + ENEMY_COUNT * PC_STRIDE:
                hb = sha(split_enemies(body)[0])
            row = cls.get((stem, c.index))
            ck, cs = (row[2], row[3]) if row else (None, None)
            src, primary = [], None
            for s in discs:
                cands = idx[s.id].get(hb or h)
                if cands:
                    loc = best(cands, stem, ck if s.id == "psx-jp" else None, cs)
                    if primary and any(c[1:] == primary for c in cands):
                        loc = next(c for c in cands if c[1:] == primary)  # one place on every disc, where it can be
                    primary = primary or loc[1:]
                    src.append([s.id] + list(loc))
            hows = {x[1] for x in src}
            if src:
                layer = "base"
                k, what = (row[0], row[1]) if row else ("identical", "")
                k = "disc" if k == "identical" else k
                if "type1" in hows:
                    k, what = "type1", ""
                elif "widen" in hows:
                    k, what = "enemy-table", "the stats in base/, the names in the language layer (widen-enemy-names)"
                elif "icons" in hows:
                    k, what = "pc-icons", "the item-type icons respaced 24 to 32 px by the port (respace-icons)"
                elif "ryud" in hows:
                    k, what = "pc-byte", "the disc's section with the PC's one byte at +0x7ACE (region-diff.md 9)"
            else:
                k, what = why_pc_only(stem, c, row)
                layer = "loc/" + tag_of(TARGET) if (k in TEXT_CLASSES or c.kind == 3) else "base"
            jp = next((x[2:] for x in src if x[0] == "psx-jp"), None) or ((ck, cs) if k == "art" and ck else None)
            own = own_sections(c, k, jp, {x[0] for x in src}, discs, pairs, stem) if layer == "base" else []
            rows_out.append([c, h, hb, layer, k, what, src, primary, own])
        # a palette stands in only beside the area page it colours
        page = {o[0] for r in rows_out if r[0].kind == 1 for o in r[8] if o[4] == "language"}
        for r in rows_out:
            r[8] = [o for o in r[8] if o[4] != "page-clut" or o[0] in page]
            if len({o[4] for o in r[8]}) > 1:
                raise SystemExit("%s: one chunk with stand-ins of two kinds" % name)
        for c, h, hb, layer, k, what, src, primary, own in rows_out:
            stats[(layer, k, "disc" if src else "pc only")] += 1
            for o in own:
                stats[("own", o[0], o[4])] += 1
            fields = "kind = %d, tag = 0x%08X, size = %d, sha256 = %s, layer = %s, class = %s" % (
                c.kind, c.tag, c.size, toml_str(h), toml_str(layer), toml_str(k))
            if what:
                fields += ", what = %s" % toml_str(what)
            if hb and src:
                fields += ", base = %s, names = %s" % (toml_str(hb), toml_str("loc/" + tag_of(TARGET)))
            if src:
                at = [x for x in src if tuple(x[2:]) == tuple(primary)]
                alt = [x for x in src if tuple(x[2:]) != tuple(primary)]
                fields += ", emi = %s, on = [%s]" % (toml_str("%s#%d" % tuple(primary)),
                                                      ", ".join(toml_str("%s:%s" % (x[0], x[1])) for x in at))
                if alt:
                    fields += ", alt = [%s]" % ", ".join(toml_str("%s:%s:%s#%d" % tuple(x)) for x in alt)
            if own:
                fields += ", own = [%s], own_why = %s" % (
                    ", ".join(toml_str("%s:%s#%d:%s" % o[:4]) for o in own), toml_str(own[0][4]))
            lines.append("  { %s }," % fields)
        lines.append("]")
    head = [
        "# recipes/pc-zh.toml - GENERATED by `tools/importer.py recipes`; do not edit by hand.",
        "# Every chunk of the PC port's 742 DAT/ containers, in file order: kind, tag, size,",
        "# the sha256 of its payload, the cache layer it belongs to, why, and every held build",
        "# that carries it byte for byte - [build, how, EMI, section] with how `copy`,",
        "# `type1` (tools/type1.py), `widen`, `icons` or `ryud` (importer.py's transforms) -",
        "# and last the PC install itself, [pc-zh, chunk, index]. `base` / `names`: the",
        "# enemy table's stats and where its names go; `own`: a disc's own section that",
        "# stands in when no source carries the chunk (docs/importer-transforms.md).",
        "# Hashes and indices only, no game data (CLAUDE.md rule 1). docs/importer.md.",
        "",
        "[meta]",
        "target = %s" % toml_str(TARGET),
        "generated = %s" % datetime.date.today().isoformat(),
        "builds = [%s]" % ", ".join(toml_str(s.id) for s in [pc] + discs),
        "classes = %s" % toml_str(os.path.basename(a.region)),
        "pairs = [%s]" % ", ".join(toml_str("psx-jp_vs_%s.json" % b) for b in sorted(pairs)),
    ]
    out = a.out or RECIPE
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(head + lines) + "\n")
    tot = sum(n for k, n in stats.items() if k[0] != "own")
    print("%s: %d files, %d chunks" % (out, len(names), tot))
    for (layer, k, s), n in sorted(stats.items()):
        print("  %-9s %-12s %-10s %5d" % (layer, k, s, n))


# ---------------------------------------------------------------- build

def load_recipe(path=None):
    with open(path or RECIPE, "rb") as f:
        return tomllib.load(f)


def sources_of(ch):
    """A recipe chunk's sources: (build, how, EMI, section) for each disc,
    then (pc-zh, "chunk", None, None)."""
    out = []
    if "emi" in ch:
        key, sec = ch["emi"].rsplit("#", 1)
        for x in ch["on"]:
            b, how = x.split(":")
            out.append((b, how, key, int(sec)))
    for x in ch.get("alt", []):
        b, how, rest = x.split(":", 2)
        key, sec = rest.rsplit("#", 1)
        out.append((b, how, key, int(sec)))
    return out + [(TARGET, "chunk", None, None)]


def owns_of(ch):
    """A recipe chunk's stand-ins: (build, EMI, section, sha256)."""
    out = []
    for x in ch.get("own", []):
        b, rest = x.split(":", 1)
        at, h = rest.rsplit(":", 1)
        key, sec = at.rsplit("#", 1)
        out.append((b, key, int(sec), h))
    return out


def chunk_bytes(kind, tag, body):
    return struct.pack("<4I", kind, tag, len(body), 0) + body


# Why a chunk is still missing, by (layer, class): the build's report.
MISSING_WHY = {
    "bank": "audio banks, the disc's VAG samples as WAV: step 6 (wave-from-vag)",
    "pc-edit": "the port's edits to type-1 arenas, unread (type1-compression.md 3): the PC install only",
    "art": "the port's dial page: the PC install, or a PSX disc whose own page stands in",
    "enemy-names": "the enemy tables' Chinese names",
}
FROM_OTHER_BUILDS = "on builds not given (the recipe's `on`); a PSP disc's differ: step 4"


def get_chunk(f, slot, ch, chunked):
    """(build, (how, EMI, section), payload, names) from the first source in the
    player's order that the recipe lists; else the first stand-in (own) in the
    player's order; else None. Every payload is hashed against the recipe.
    `names` is the enemy table's [(tag, name)] when the PC gave it, else None."""
    want = ch.get("base", ch["sha256"])
    for s in chunked:          # the player's order decides
        for src in sources_of(ch):
            if src[0] != s.id:
                continue
            body, names = s.get(f["name"], slot, src[1:]), None
            if "base" in ch and src[1] == "chunk":
                if sha(body) != ch["sha256"]:
                    raise SystemExit("%s chunk %d from %s: hash differs from the recipe" % (f["name"], slot, s.id))
                body, names = split_enemies(body)
            if sha(body) != want:
                raise SystemExit("%s chunk %d from %s (%s): hash differs from the recipe"
                                 % (f["name"], slot, s.id, src[1]))
            if "base" in ch and names is None:      # the names have one source: the PC's chunk
                pc = next((p for p in chunked if p.id == TARGET), None)
                if pc:
                    full = pc.get(f["name"], slot, ("chunk", None, None))
                    if sha(full) != ch["sha256"] or sha(split_enemies(full)[0]) != want:
                        raise SystemExit("%s chunk %d from %s: hash differs from the recipe" % (f["name"], slot, pc.id))
                    names = split_enemies(full)[1]
            return s.id, src[1:], body, names
    for s in chunked:
        for b, key, sec, h in owns_of(ch):
            if b != s.id:
                continue
            body = s.get(f["name"], slot, ("copy", key, sec))
            if sha(body) != h:
                raise SystemExit("%s chunk %d from %s's own section: hash differs from the recipe" % (f["name"], slot, b))
            return s.id, ("own", key, sec), body, None
    return None


def cmd_build(a):
    rec = load_recipe(a.recipe)
    sources = [open_source(p) for p in a.source]
    if len({(s.id, type(s)) for s in sources}) != len(sources):
        raise SystemExit("two sources are the same build")
    print("sources, in order: %s" % ", ".join("%s (%s)" % (s.id, s.path) for s in sources))
    chunked = [s for s in sources if not isinstance(s, ExeSource)]
    assets, missing, used = [], collections.Counter(), collections.Counter()
    written, blocked = collections.Counter(), collections.Counter()
    for f in rec["file"]:
        layers, tails = collections.OrderedDict(), collections.defaultdict(list)
        lacks = collections.defaultdict(set)
        for slot, ch in enumerate(f["chunks"]):
            got = get_chunk(f, slot, ch, chunked)
            layers.setdefault(ch["layer"], [])
            if "names" in ch:
                layers.setdefault(ch["names"], [])
            if not got:
                missing[(ch["layer"], ch["class"])] += 1
                lacks[ch["layer"]].add(ch["class"])
                layers[ch["layer"]].append(None)
                assets.append((f["name"], slot, ch["layer"], None, ch["sha256"]))
                continue
            bid, how, body, names = got
            used[(bid, ch["layer"], how[0] == "own")] += 1
            layers[ch["layer"]].append(chunk_bytes(ch["kind"], ch["tag"], body))
            h = ch.get("base", ch["sha256"]) if how[0] != "own" else sha(body)
            assets.append((f["name"], slot, ch["layer"], (bid,) + how, h))
            if "names" in ch:
                if names is None:          # from a disc: the stats only
                    missing[(ch["names"], "enemy-names")] += 1
                    lacks[ch["names"]].add("enemy-names")
                    tails[ch["names"]].append(None)
                else:
                    tails[ch["names"]] += [chunk_bytes(0, t, n) for t, n in names]
                    assets.append((f["name"], slot, ch["names"], (bid, "names", None, None),
                                   sha(b"".join(n for _, n in names))))
        for layer, parts in layers.items():
            parts = parts + tails[layer]
            if any(p is None for p in parts):
                blocked[(layer, " + ".join(sorted(lacks[layer])))] += 1
                continue          # a container is written whole or not at all
            d = os.path.join(a.out, layer, "dat")
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, f["name"]), "wb") as fh:
                fh.write(b"".join(parts))
            written[layer] += 1
    loc_assets = build_languages(a.lang, sources, a.out)
    # base/exe/ (docs/exe-import.md): from the PC's executable when given, else the first disc
    exe_src = next((s for s in sources if isinstance(s, ExeSource)), None) or \
        next((s for s in sources if isinstance(s, DiscSource)), None)
    exe = exe_tables.build_from(exe_src.path, exe_src.id, a.out) if exe_src else None
    if exe:
        print("  base/exe  from %-9s data.bin %s" % exe)
    write_manifest(a.out, a.recipe or RECIPE, rec, sources, assets, loc_assets, exe)
    for (bid, layer, own), n in sorted(used.items()):
        print("  %-9s from %-9s %5d chunks%s" % (layer, bid, n, " (its own sections standing in)" if own else ""))
    for layer, n in sorted(written.items()):
        print("  %-9s %d containers written" % (layer, n))
    if missing:
        print("  missing (no source given carries them):")
        for (layer, k), n in sorted(missing.items()):
            why = ("the Chinese layer: the PC install only; not needed unless %s is played" % layer[4:]
                   if layer == "loc/" + tag_of(TARGET) else MISSING_WHY.get(k, FROM_OTHER_BUILDS))
            print("    %-9s %-12s %5d  %s" % (layer, k, n, why))
        print("  containers not written, by what they lack:")
        for (layer, k), n in sorted(blocked.items()):
            print("    %-9s %5d  %s" % (layer, n, k))
    print("manifest: %s" % os.path.join(a.out, "manifest.toml"))


# The default tag of a bare language: the owner's decision of 2026-10-08, en-US
# is the default English. A language with one held tag needs no entry.
DEFAULT_TAG = {"en": "en-US"}


def resolve_languages(asked, sources):
    """Each --lang to (tag, donor): a full tag (`en-150`) is the PSX disc whose
    fixtures.toml tag it is, exactly; a bare language (`en`) is the disc of its
    default tag (DEFAULT_TAG) when one is given, else the first PSX disc in the
    player's order whose tag has it as the primary subtag (docs/importer.md
    section 5, docs/importer-transforms.md section 7). tools/loc_build.py
    reads PSX discs only."""
    donors = [s for s in sources if isinstance(s, DiscSource) and s.id.startswith("psx-")]
    out = {}
    for want in asked:
        if "-" in want:
            d = next((s for s in donors if tag_of(s.id) == want), None)
        else:
            d = next((s for s in donors if tag_of(s.id) == DEFAULT_TAG.get(want)), None) or \
                next((s for s in donors if primary(tag_of(s.id)) == want), None)
        if not d:
            have = ", ".join("%s (%s)" % (s.id, tag_of(s.id)) for s in donors) or "none"
            raise SystemExit("--lang %s: no PSX disc given carries it; the discs given: %s" % (want, have))
        out[tag_of(d.id)] = d
    return sorted(out.items(), key=lambda kv: (primary(kv[0]) != "en", kv[0]))


def build_languages(asked, sources, out):
    """loc/<tag>/ for each language asked for, built by tools/loc_build.py
    from its donor disc (resolve_languages) against the PC's own containers
    and BOF3.exe - exactly the overlays it writes into a game's DAT/, so the
    engine reads them unchanged. English goes first: the French and German
    title menus borrow its CONFIG row (loc_build.build_title)."""
    import shutil
    import subprocess
    import tempfile
    pc = next((s for s in sources if isinstance(s, PcSource)), None)
    exe = next((s for s in sources if isinstance(s, ExeSource)), None)
    if not asked:
        return []
    if not (pc and exe):
        raise SystemExit("a language layer is built against the PC's DAT/ and BOF3.exe: give both as sources")
    assets = []
    with tempfile.TemporaryDirectory(prefix="bof3_loc_") as game:
        os.makedirs(os.path.join(game, "DAT"))
        os.symlink(os.path.abspath(exe.path), os.path.join(game, "BOF3.exe"))
        for name in _manifest_rows("fixtures/pc-zh.DAT.files.tsv"):
            os.symlink(os.path.abspath(os.path.join(pc.path, name)), os.path.join(game, "DAT", name))
        for tag, donor in resolve_languages(asked, sources):
            r = subprocess.run([sys.executable, os.path.join(TOOLS, "loc_build.py"), "all", "--disc", donor.path,
                                "--game", game, "--lang", tag], capture_output=True, text=True)
            if r.returncode:
                raise SystemExit("loc_build.py --lang %s failed:\n%s%s" % (tag, r.stdout, r.stderr))
            d = os.path.join(out, "loc", tag, "dat")
            os.makedirs(d, exist_ok=True)
            n = 0
            for f in sorted(os.listdir(os.path.join(game, "DAT"))):
                if f.startswith(tag + "."):
                    name = f[len(tag) + 1:]
                    shutil.copyfile(os.path.join(game, "DAT", f), os.path.join(d, name))
                    with open(os.path.join(d, name), "rb") as fh:
                        assets.append((name, "loc/" + tag, "%s:loc_build" % donor.id, sha(fh.read())))
                    n += 1
            print("  loc/%-7s from %-9s %d containers (tools/loc_build.py)" % (tag, donor.id, n))
    return assets


def write_manifest(out, recipe_path, rec, sources, assets, loc_assets=(), exe=None):
    with open(recipe_path, "rb") as f:
        rsha = sha(f.read())
    lines = ["# The cache's provenance, written by tools/importer.py build. Every chunk: its",
             "# container, slot, layer, the build and recipe step it came from (`own`: that",
             "# disc's own section standing in; `names`: an enemy table's names), its hash.",
             "", "[meta]",
             "target = %s" % toml_str(rec["meta"]["target"]),
             "recipe_sha256 = %s" % toml_str(rsha),
             "built = %s" % toml_str(datetime.datetime.now().isoformat(timespec="seconds")),
             ""]
    for s in sources:
        lines += ["[[source]]", "build = %s" % toml_str(s.id), "path = %s" % toml_str(os.path.basename(os.path.normpath(s.path))), ""]
    lines += ["[cache]", "assets = ["]          # a table of its own, not the last [[source]]'s
    for name, slot, layer, src, h in assets:
        where = "" if not src else src[0] + ":" + src[1] + ("" if src[2] is None else ":%s#%d" % (src[2], src[3]))
        lines.append("  [%s, %d, %s, %s, %s]," % (toml_str(name), slot, toml_str(layer), toml_str(where), toml_str(h)))
    lines.append("]")
    if loc_assets:
        lines += ["", "# Language layers: whole overlay containers, as tools/loc_build.py writes them.", "overlays = ["]
        for name, layer, where, h in loc_assets:
            lines.append("  [%s, %s, %s, %s]," % (toml_str(name), toml_str(layer), toml_str(where), toml_str(h)))
        lines.append("]")
    if exe:
        lines += ["", "# base/exe/data.bin: BOF3.exe's .data in the PC's layout (tools/exe_tables.py).", "[exe]",
                  "build = %s" % toml_str(exe[0]), 'file = "base/exe/data.bin"', "sha256 = %s" % toml_str(exe[1])]
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "manifest.toml"), "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


# ---------------------------------------------------------------- verify

def cmd_verify(a):
    """Two checks. Every container the cache holds, chunk by chunk against the
    hashes the manifest recorded (which the build took from the recipe) - this
    covers a cache with a disc's own sections standing in, which cannot be the
    PC's. Then base/ + loc/zh-CN/ composed back into the PC's containers, in the
    recipe's slot order with the enemy tables' names laid over their stats, and
    each hashed against fixtures/pc-zh.DAT.files.tsv."""
    rec = load_recipe(a.recipe)
    want = _manifest_rows("fixtures/pc-zh.DAT.files.tsv")
    with open(os.path.join(a.cache, "manifest.toml"), "rb") as fh:
        assets = tomllib.load(fh)["cache"]["assets"]
    man = {(n, slot, layer): h for n, slot, layer, where, h in assets}
    own = {n for n, slot, layer, where, h in assets if where.split(":")[1:2] == ["own"]}
    held, held_ok, owned = 0, [], 0
    ok, bad, absent = 0, [], []
    for f in rec["file"]:
        files = {}
        for ch in f["chunks"]:
            for layer in (ch["layer"], ch.get("names")):
                if layer and layer not in files:
                    p = os.path.join(a.cache, layer, "dat", f["name"])
                    files[layer] = dat.load(p) if os.path.exists(p) else None
        at = collections.Counter()
        tables, parts = [], []
        for slot, ch in enumerate(f["chunks"]):
            got = files[ch["layer"]]
            if got is None:
                parts = None
                continue
            blob, chunks = got
            c = chunks[at[ch["layer"]]]
            at[ch["layer"]] += 1
            body = blob[c.offset:c.offset + c.size]
            if sha(body) != man.get((f["name"], slot, ch["layer"])) or (c.kind, c.tag) != (ch["kind"], ch["tag"]):
                bad.append("%s slot %d: not the chunk the manifest recorded" % (f["name"], slot))
            if "names" in ch:
                tables.append((len(parts), ch) if parts is not None else None)
            if parts is not None:
                parts.append([c.kind, c.tag, bytearray(body)])
        for layer, got in files.items():
            if got is None:
                continue
            held += 1
            blob, chunks = got
            names = chunks[at[layer]:]          # past the layer's slots: enemy names
            for c in names:
                j = next((j for j, ch in filter(None, tables)
                          if parts and parts[j][1] <= c.tag and c.tag + c.size <= parts[j][1] + ch["size"]), None)
                if c.kind != 0 or not any(t and t[1].get("names") == layer for t in tables):
                    bad.append("%s: %s holds a chunk past its slots that is no enemy name" % (f["name"], layer))
                elif j is not None:
                    o = c.tag - parts[j][1]
                    parts[j][2][o:o + c.size] = blob[c.offset:c.offset + c.size]
        if parts is None or any(files.get(ch.get("names")) is None for _, ch in filter(None, tables)):
            absent.append(f["name"])
            continue
        body = b"".join(chunk_bytes(k, t, bytes(p)) for k, t, p in parts)
        if (len(body), sha(body)) == want[f["name"]]:
            ok += 1
        elif f["name"] in own:
            owned += 1          # a stand-in: its chunks were checked above
        else:
            bad.append("%s: composed, it is not the PC's" % f["name"])
    print("verify %s: %d containers held, %d problem(s)"
          % (a.cache, held, len(bad)))
    print("  against fixtures/pc-zh.DAT.files.tsv: %d of %d containers byte-identical, %d hold a disc's own "
          "sections, %d incomplete" % (ok, len(want), owned, len(absent)))
    for n in bad[:20]:
        print("   ", n)
    with open(os.path.join(a.cache, "manifest.toml"), "rb") as fh:
        exe = tomllib.load(fh).get("exe")
    if exe:
        errs = exe_tables.verify_cache(a.cache)
        with open(os.path.join(a.cache, exe["file"]), "rb") as fh:
            if sha(fh.read()) != exe["sha256"]:
                errs.append("%s: not the image the manifest recorded" % exe["file"])
        print("  base/exe/ (%s): %s" % (exe["build"], "; ".join(errs) if errs else
                                         "the image recipes/exe.toml records for it"))
        bad += errs
    rc = 1 if bad else 0
    if a.overlays:
        rc |= verify_overlays(a.cache, a.overlays)
    return rc


def verify_overlays(cache, theirs):
    """Each loc/<tag>/ layer (but the target's own, which the recipe checks)
    against an install's <tag>.<NAME>.DAT overlays, byte for byte, both ways;
    where the install has none under the full tag, against its overlays under
    the bare language (`en.`, what the engine reads until it takes tags) - for
    the language's default tag only (DEFAULT_TAG: `en.` is en-US's)."""
    rc = 0
    loc = os.path.join(cache, "loc")
    for tag in sorted(os.listdir(loc)) if os.path.isdir(loc) else []:
        if tag == tag_of(TARGET):
            continue
        d = os.path.join(loc, tag, "dat")
        ours = set(os.listdir(d))
        lang = tag
        if not any(f.startswith(tag + ".") for f in os.listdir(theirs)):
            if DEFAULT_TAG.get(primary(tag), tag) != tag:
                print("verify loc/%s: the install has no %s.*.DAT, and its %s.* are %s's - not compared"
                      % (tag, tag, primary(tag), DEFAULT_TAG[primary(tag)]))
                continue
            lang = primary(tag)
        inst = {f[len(lang) + 1:] for f in os.listdir(theirs) if f.startswith(lang + ".")}
        same = [n for n in sorted(ours & inst)
                if open(os.path.join(d, n), "rb").read() == open(os.path.join(theirs, lang + "." + n), "rb").read()]
        differ = sorted((ours & inst) - set(same))
        print("verify loc/%s against %s/%s.*.DAT: %d identical, %d differ, %d only in the cache, %d only in the install"
              % (tag, theirs, lang, len(same), len(differ), len(ours - inst), len(inst - ours)))
        for n in (differ + sorted(ours ^ inst))[:10]:
            print("   ", n)
        rc |= bool(differ or ours ^ inst)
    return rc


# ---------------------------------------------------------------- identify, check

def cmd_identify(a):
    rc = 0
    for p in a.path:
        r = identify(p)
        print("%s: %s" % (p, "%s (%s)" % r if r else "unknown - nothing would be imported"))
        rc |= r is None
    return rc


def cmd_check(a):
    """The recipe against fixtures/ alone: every container's chunks add up to
    the manifest's size, every source names a build fixtures.toml holds and a
    transform importer.py has, every layer is base or the target's language,
    every enemy table splits into base and that language, and every stand-in
    is a PSX disc's with a known reason. Needs no game data (CI)."""
    rec = load_recipe(a.recipe)
    want = _manifest_rows("fixtures/pc-zh.DAT.files.tsv")
    builds = {b["id"] for b in _fixtures()["build"]}
    errs = []
    names = [f["name"] for f in rec["file"]]
    if sorted(names) != sorted(want):
        errs.append("files: %d in the recipe, %d in the manifest" % (len(names), len(want)))
    n = 0
    for f in rec["file"]:
        size = sum(16 + c["size"] for c in f["chunks"])
        if f["name"] in want and size != want[f["name"]][0]:
            errs.append("%s: chunks add to %d, the manifest says %d" % (f["name"], size, want[f["name"]][0]))
        for c in f["chunks"]:
            n += 1
            if c["layer"] != "base" and c["layer"] != "loc/" + tag_of(TARGET):
                errs.append("%s: layer %s" % (f["name"], c["layer"]))
            for s in sources_of(c):
                if s[0] not in builds:
                    errs.append("%s: source build %s not in fixtures.toml" % (f["name"], s[0]))
                if s[1] not in TRANSFORMS and s[1] != "chunk":
                    errs.append("%s: how %s" % (f["name"], s[1]))
            if ("base" in c) != ("names" in c) or ("names" in c and (c["names"] != "loc/" + tag_of(TARGET)
                                                                      or c["layer"] != "base" or c["kind"] != 0)):
                errs.append("%s: an enemy table's base / names" % f["name"])
            for b, key, sec, h in owns_of(c):
                if b not in OWN_FROM or c["layer"] != "base" or c.get("own_why") not in OWN_WHY or len(h) != 64:
                    errs.append("%s: stand-in %s" % (f["name"], b))
    for e in errs[:30]:
        print("ERROR", e)
    print("importer check: %d files, %d chunks, %d error(s)" % (len(names), n, len(errs)))
    return exe_tables.cmd_check(a) | (1 if errs else 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    p = s.add_parser("recipes")
    p.add_argument("--dat", required=True, help="the PC port's DAT/ directory")
    p.add_argument("--disc", action="append", default=[], help="a held disc, any order; repeat")
    p.add_argument("--region", default=os.path.join(ROOT, "analysis", "region", "psx-jp_vs_pc-zh.json"),
                   help="region_diff.py pc's output, for the class of chunks no disc carries")
    p.add_argument("--pairs", default=os.path.join(ROOT, "analysis", "region"),
                   help="where region_diff.py pair's psx-jp_vs_<build>.json are, for the own-section stand-ins")
    p.add_argument("--out")
    p = s.add_parser("identify")
    p.add_argument("path", nargs="+")
    p = s.add_parser("build")
    p.add_argument("--source", action="append", required=True, help="a PC DAT/, BOF3.exe or a disc; order is preference")
    p.add_argument("--out", required=True)
    p.add_argument("--lang", action="append", default=[],
                   help="a language layer to build (repeat): a tag (en-US, en-150, fr-FR, de-DE, ja-JP) or a bare "
                        "language (en: en-US when the US disc is given, else the first English disc in the "
                        "source order); needs the PC's DAT/, BOF3.exe and that disc")
    p.add_argument("--recipe")
    p = s.add_parser("verify")
    p.add_argument("--cache", required=True)
    p.add_argument("--overlays", help="an install's DAT/ whose <lang>.*.DAT overlays the cache's loc/ layers must equal")
    p.add_argument("--recipe")
    p = s.add_parser("check")
    p.add_argument("--recipe")
    a = ap.parse_args()
    fn = {"recipes": cmd_recipes, "identify": cmd_identify, "build": cmd_build, "verify": cmd_verify,
          "check": cmd_check}[a.cmd]
    sys.exit(fn(a) or 0)


if __name__ == "__main__":
    main()
