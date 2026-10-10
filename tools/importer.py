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
    python tools/importer.py build    --source PATH [--source PATH ...] [--preset NAME] [--lang TAG ...]
                                      [--opt LAYER ...] [--no-opt LAYER ...] --out CACHE
    python tools/importer.py verify   --cache CACHE
    python tools/importer.py check    (CI: the recipes against fixtures/, no game data)
    python tools/importer.py opt-recipes --dat DAT --disc JP --disc US --disc PSPJP --disc PSPEU
    python tools/importer.py install  --cache CACHE --game DIR [--lang TAG ...] [--opt LAYER ...] [--no-opt LAYER ...]

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
step 8; from a disc the data pointers rebuilt by the map run backwards,
docs/exe-import-engine.md). `--lang` builds loc/<tag>/ with tools/loc_build.py: against the PC's
DAT/ and BOF3.exe when both are sources, else disc-only against the cache's own
base/ (docs/loc-build-disc-only.md). `opt/<name>/` layers carry a PSP disc's content changes the player may
turn on, and `area4-walls` a Western PSX disc's walls in area 4 (DIV-0080),
built and installed by default when such a disc is given (`--no-opt` leaves it
out) (recipes/opt.toml, docs/opt-layers.md, step 4); `--preset` is a named
source order plus layers; `install` copies a cache's layers into a game's DAT/
under the overlay names the engine walks. The recipe holds names,
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
import language_tags  # noqa: E402  (the retired bare codes, DIV-0005)
import type1        # noqa: E402
import vag          # noqa: E402  (wave-from-vag, docs/sound-import.md)
import xa           # noqa: E402  (wave-from-xa)
import seq          # noqa: E402  (base/bgm/, docs/seq-import.md)

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
        bank = vag.importer_source(s, f, slot, ch)      # wave-from-vag (docs/sound-import.md), hashed inside
        if bank:
            return bank[0], bank[1][1:], bank[2], None
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
    if a.preset:
        sources, lang, opt = resolve_preset(a.preset, sources)
        a.lang, a.opt = a.lang + [x for x in lang if x not in a.lang], a.opt + [x for x in opt if x not in a.opt]
    discs = {s.id for s in sources if isinstance(s, DiscSource)}
    a.opt = default_opt(a.opt, a.no_opt, lambda name: bool(discs & set(DEFAULT_OPT[name])))
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
    snd = xa.importer_snd(sources, a.out)
    bgm = seq.importer_bgm(sources, a.out)
    # base/exe/ (docs/exe-import.md): from the PC's executable when given, else the first disc.
    # Before the languages: a disc-only language layer is built against it.
    exe_src = next((s for s in sources if isinstance(s, ExeSource)), None) or \
        next((s for s in sources if isinstance(s, DiscSource)), None)
    exe = exe_tables.build_from(exe_src.path, exe_src.id, a.out) if exe_src else None
    if exe:
        print("  base/exe  from %-9s data.bin %s" % exe)
    loc_assets = build_languages(a.lang, sources, a.out)
    opt_assets = build_opt(a.opt, sources, a.out, a.opt_recipe)
    write_manifest(a.out, a.recipe or RECIPE, rec, sources, assets, loc_assets, opt_assets, a.opt_recipe, exe, bgm)
    for (bid, layer, own), n in sorted(used.items()):
        print("  %-9s from %-9s %5d chunks%s" % (layer, bid, n, " (its own sections standing in)" if own else ""))
    for layer, n in sorted(written.items()):
        print("  %-9s %d containers written" % (layer, n))
    if snd:
        print("  base/snd  from %s: %d files (wave-from-xa)" % snd)
    if bgm:
        print("  base/bgm  from %s: %d files (seq.py)" % (bgm[0], len(bgm[1])))
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


def resolve_languages(asked, sources):
    """Each --lang to (tag, donor): a tag (`en-150`) is the PSX disc whose
    fixtures.toml tag it is, exactly (docs/importer.md section 5,
    docs/importer-transforms.md section 7). The bare codes of before
    2026-10-08 (`en`, fr, de, ja) are retired and refused (DIV-0005,
    tools/language_tags.py). tools/loc_build.py reads PSX discs only."""
    donors = [s for s in sources if isinstance(s, DiscSource) and s.id.startswith("psx-")]
    out = {}
    for want in asked:
        language_tags.refuse_retired(want, "--lang")
        d = next((s for s in donors if tag_of(s.id) == want), None)
        if not d:
            have = ", ".join("%s (%s)" % (s.id, tag_of(s.id)) for s in donors) or "none"
            raise SystemExit("--lang %s: no PSX disc given carries it; the discs given: %s" % (want, have))
        out[tag_of(d.id)] = d
    return sorted(out.items(), key=lambda kv: (primary(kv[0]) != "en", kv[0]))


def _place(src, dst):
    """The player's file at dst in build_languages' scratch game: a symlink,
    else a hard link, else a copy. Windows refuses a symlink without Developer
    Mode or the privilege (WinError 1314) and a hard link across volumes; a
    copy always works. tools/loc_build.py only writes new <tag>.* files beside
    these, never through them, so a link cannot change the player's tree."""
    import shutil
    src = os.path.abspath(src)
    try:
        os.symlink(src, dst)
        return
    except OSError:
        pass
    try:
        os.link(src, dst)
        return
    except OSError:
        pass
    shutil.copy2(src, dst)


def build_languages(asked, sources, out):
    """loc/<tag>/ for each language asked for, built by tools/loc_build.py
    from its donor disc (resolve_languages) against the PC's own containers
    and BOF3.exe - exactly the overlays it writes into a game's DAT/, so the
    engine reads them unchanged. English goes first: the French and German
    title menus borrow its CONFIG row (loc_build.build_title).

    With neither the PC's DAT/ nor its BOF3.exe among the sources, the layer is
    built disc-only: `loc_build.py all --cache` against this cache's base/dat/
    and base/exe/, which the build has already written
    (docs/loc-build-disc-only.md: what differs from the PC build, and why)."""
    import shutil
    import subprocess
    import tempfile
    pc = next((s for s in sources if isinstance(s, PcSource)), None)
    exe = next((s for s in sources if isinstance(s, ExeSource)), None)
    if not asked:
        return []
    if not pc and not exe:
        return build_languages_disc_only(asked, sources, out)
    if not (pc and exe):
        raise SystemExit("a language layer is built against the PC's DAT/ and BOF3.exe (give both as sources), "
                         "or against the disc alone (give neither)")
    assets = []
    with tempfile.TemporaryDirectory(prefix="bof3_loc_") as game:
        os.makedirs(os.path.join(game, "DAT"))
        _place(exe.path, os.path.join(game, "BOF3.exe"))
        for name in _manifest_rows("fixtures/pc-zh.DAT.files.tsv"):
            _place(os.path.join(pc.path, name), os.path.join(game, "DAT", name))
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


def build_languages_disc_only(asked, sources, out):
    """loc/<tag>/ from the donor disc against the cache's own base/ (DiscCache)."""
    import subprocess
    assets = []
    for tag, donor in resolve_languages(asked, sources):
        d = os.path.join(out, "loc", tag, "dat")
        if os.path.isdir(d):            # a rebuild: no container of an earlier layer left behind
            for f in os.listdir(d):
                os.remove(os.path.join(d, f))
        r = subprocess.run([sys.executable, os.path.join(TOOLS, "loc_build.py"), "all", "--disc", donor.path,
                            "--cache", out, "--lang", tag], capture_output=True, text=True)
        if r.returncode:
            raise SystemExit("loc_build.py --cache --lang %s failed:\n%s%s" % (tag, r.stdout, r.stderr))
        names = sorted(os.listdir(d))
        for name in names:
            with open(os.path.join(d, name), "rb") as fh:
                assets.append((name, "loc/" + tag, "%s:loc_build-disc-only" % donor.id, sha(fh.read())))
        print("  loc/%-7s from %-9s %d containers (tools/loc_build.py, disc-only)" % (tag, donor.id, len(names)))
    return assets


def write_manifest(out, recipe_path, rec, sources, assets, loc_assets=(), opt_assets=(), opt_recipe=None, exe=None,
                   bgm=None):
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
    if opt_assets:
        with open(opt_recipe or OPT_RECIPE, "rb") as f:
            osha = sha(f.read())
        lines += ["", "# Optional layers (opt/<name>/, docs/opt-layers.md): every chunk, its layer, its",
                  "# index in that layer's container, the build and what it was cut from, its hash.",
                  "opt_recipe_sha256 = %s" % toml_str(osha), "opt = ["]
        for name, layer, i, where, h in opt_assets:
            lines.append("  [%s, %s, %d, %s, %s]," % (toml_str(name), toml_str(layer), i, toml_str(where), toml_str(h)))
        lines.append("]")
    if bgm:
        lines += ["", "# base/bgm/: the disc's songs and banks (tools/seq.py, docs/seq-format.md): file, source, hash.",
                  "bgm = ["]
        for rel, where, h in bgm[1]:
            lines.append("  [%s, %s, %s]," % (toml_str(rel), toml_str(where), toml_str(h)))
        lines.append("]")
    if exe:
        lines += ["", "# base/exe/data.bin: BOF3.exe's .data in the PC's layout (tools/exe_tables.py).", "[exe]",
                  "build = %s" % toml_str(exe[0]), 'file = "base/exe/data.bin"', "sha256 = %s" % toml_str(exe[1])]
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "manifest.toml"), "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


# ---------------------------------------------------------------- opt/ layers
#
# docs/opt-layers.md (unified-data step 4). A later build's content change the
# player may turn on (docs/unified-data-plan.md section 7): opt/<name>/dat/
# NAME.DAT, ordinary containers the engine walks after the language overlay
# (DIV-0086, BOF3X_OPT). A layer carries exactly the measured change, as
# sub-range chunks landing inside the base chunk they change: a kind-0 chunk at
# the base chunk's tag + the offset (LoadDatFile copies to the arena + tag), a
# kind-1 chunk at the tile rectangle of the tiles it replaces (the tag is a
# rectangle, asset-loading-path.md section 2), a kind-5 chunk on one name
# record (DIV-0008, record-granular since DIV-0086). recipes/opt.toml is
# generated (`opt-recipes`) from the player's discs: every chunk's hash, what
# it is cut from, and the hash of each base chunk it lands on with the layer
# laid over it (`composed`), so `verify` proves the composition with no disc.

OPT_RECIPE = os.path.join(ROOT, "recipes", "opt.toml")
PSP = ("psp-jp", "psp-eu")
CLUT_ROW, PAGE_TILE, BAND_GAP = 32, 0x800, 16
P6_DEST, CLUT_DEST, PAGE_DEST, BAND_DEST = 0x8002D800, 0x8002BE00, 0x0A081000, 0x80104000
# Stallion's palette rows (psp-stallion.md section 2): area 67 rows 6-7, area 166
# rows 0-7 of the 0x8002D800 section (PC tag 0xA000). The generator asserts the
# counts.
P6_ROWS = {"AREA067": [6, 7], "AREA166": list(range(8))}
OPT_LAYERS = {
    "psp-art": "P6: Stallion's palettes, AREA067 rows 6-7 and AREA166 rows 0-7 of the 0x8002D800 "
               "section, as the PSP discs ship them (psp-stallion.md 2)",
    "psp-tiles": "the PSP's other art in the areas: the 0x0A081000 page tiles it redrew or blanked "
                 "(the port's own dial-page tiles kept) and the rows of the 0x8002BE00 palettes it changed",
    "psp-maps": "the PSP's edits to ten areas' map bands (0x80104000, PC tag 0xC8000): texture "
                "coordinates, tile words and cell-run records; no cell byte, no height (AREA004 is DIV-0080's)",
    "psp-names-en-150": "the PSP-EU's item and ability names where they differ from the US disc's "
                        "tables (P7: ability 116; three more abilities, four items)",
    "psp-names-ja-JP": "the PSP-JP's renamed ability 116 and key items 2, 5, 7, 9",
    "area4-walls": "DIV-0080: AREA004's collision as the Western PSX discs ship it - the area block's whole "
                   "cell-byte plane (in PC tag 0xC8000) and the whole battle placement map (PC tag 0xC0800); "
                   "not the tile words or texture records",
}
# area4-walls (DIV-0080, docs/opt-layers.md section 1): from a Western PSX disc,
# not a PSP one. Two chunks: the area block's whole cell-byte plane
# (AreaMap_Bytes: 4 x the header's dword +0x14, width x depth bytes), so the
# later discs' re-texture of 30 cells (tile words, texture records) is not
# carried, and the whole placement map. Whole planes, not the differing bytes,
# so that no cell's place or value is in the recipe. JP's section is found by its
# destination, the Western twin by the same index, type and size (its kind-0
# sections sit 0x8000 higher).
WALLS_LAYER, WALLS_EMI, WALLS_DESTS = "area4-walls", "WORLD00/AREA004.EMI", (0x80104000, 0x8002A000)
WESTERN = ("psx-us", "psx-eu-en", "psx-fr", "psx-de")
OPT_SOURCES = PSP + WESTERN
# The layers on by default, each with the builds that carry it: `build` builds
# one whenever such a disc is among its sources, `install` installs one
# whenever the cache holds it, and the launcher plays one whenever it is
# installed and the ini's opt= is empty (src/launcher/config.h kOptDefault).
# `--no-opt NAME` leaves it out of a build or an install. DIV-0080's walls,
# on by default since 2026-10-10 (the owner's word).
DEFAULT_OPT = {WALLS_LAYER: WESTERN}


def default_opt(asked, without, have):
    """`asked` (--opt) plus each DEFAULT_OPT layer `have(name)` says is
    available, less `without` (--no-opt). A name in both lists, or one no
    layer has, is refused."""
    for name in without:
        if name not in OPT_LAYERS:
            raise SystemExit("--no-opt %s: no such layer; there are %s" % (name, ", ".join(OPT_LAYERS)))
        if name in asked:
            raise SystemExit("--opt %s and --no-opt %s together" % (name, name))
    out = list(asked)
    for name in DEFAULT_OPT:
        if name not in out and name not in without and have(name):
            out.append(name)
            print("  %s: on by default (--no-opt %s leaves it out)" % (name, name))
    return out
# A names layer: the PSP build it reads, the PSX build its records differ from,
# and its text's encoding (Japanese or not, loc_build.py's).
NAME_LAYERS = {"psp-names-en-150": ("psp-eu", "psx-us", False), "psp-names-ja-JP": ("psp-jp", "psx-jp", True)}
NAME_FILE, NAME_FIELD, NAME_ROOM = "FIRST.DAT", 16, 15


def _runs(idx, gap=0):
    """Sorted ints -> [(first, last)] runs, merging gaps of at most `gap`."""
    out = []
    for i in idx:
        if out and i - out[-1][1] <= gap + 1:
            out[-1][1] = i
        else:
            out.append([i, i])
    return [tuple(r) for r in out]


def opt_units(layer, kind, tag, jp, psp, pc):
    """The chunks a layer carries for one section: [(kind, tag, at, size)], `at`
    a byte offset into the section. Where the PC's chunk is not the JP section
    (the port's dial page), a unit the port changed is not taken."""
    if kind == 1:
        cols, x0, y0 = (tag >> 8) & 0xFF, tag >> 24, (tag >> 16) & 0xFF
        diff = [i for i in range(len(jp) // PAGE_TILE)
                if jp[i * PAGE_TILE:(i + 1) * PAGE_TILE] != psp[i * PAGE_TILE:(i + 1) * PAGE_TILE]
                and pc[i * PAGE_TILE:(i + 1) * PAGE_TILE] == jp[i * PAGE_TILE:(i + 1) * PAGE_TILE]]
        out = []
        for a, b in _runs(diff):
            while a <= b:            # one chunk per run within a page row
                e = min(b, a - a % cols + cols - 1)
                x, y, n = x0 + a % cols, y0 + a // cols, e - a + 1
                out.append((1, (x << 24) | (y << 16) | (n << 8) | (tag & 0xFF), a * PAGE_TILE, n * PAGE_TILE))
                a = e + 1
        return out
    if layer == "psp-maps":
        diff = [i for i in range(len(jp)) if jp[i] != psp[i] and pc[i] == jp[i]]
        return [(0, tag + a, a, b - a + 1) for a, b in _runs(diff, BAND_GAP)]
    diff = [r for r in range(len(jp) // CLUT_ROW)
            if jp[r * CLUT_ROW:(r + 1) * CLUT_ROW] != psp[r * CLUT_ROW:(r + 1) * CLUT_ROW]]
    return [(0, tag + a * CLUT_ROW, a * CLUT_ROW, (b - a + 1) * CLUT_ROW) for a, b in _runs(diff)]


def opt_layer_of(stem, t, dest):
    if t == 0 and dest == P6_DEST and stem in P6_ROWS:
        return "psp-art"
    if (t == 0 and dest == CLUT_DEST and stem.startswith("AREA")) or (t == 3 and dest == PAGE_DEST):
        return "psp-tiles"
    if t == 0 and dest == BAND_DEST and stem != "AREA004":
        return "psp-maps"
    return None


# What the PSP changed that no layer carries, by (region_diff class, what):
# the reason, for the generator's report and docs/opt-layers.md section 3.
OPT_EXCLUDED = {
    ("art", "an image page"): "the PSP's shoulder-button labels on the battle UI page, DEMO's logo "
                              "pages, SCENA17's page: the PSP's buttons, its title, an unassigned scene",
    ("art", "the glyph atlas"): "the PSX font's tiles; the PC draws with its own font",
    ("art", "the ending / kanji font sheet"): "the PSX kanji sheet; the PC draws with its own font",
    ("art", "the title menu page"): "P14, the PSP's title menu (its fishing entry); the PC's own page",
    ("art", "DEMO's language page"): "language pages (they differ PSP-JP to PSP-EU)",
    ("art", "a CLUT section (palettes)"): "beside a page not taken (DEMO's logo, SCENA17) or START's",
    ("logic-data", "data"): "AREA004's band (DIV-0080's walls are area4-walls, from a Western PSX disc; the "
                            "PSP's form lacks the placement half) and Ryu's form data (P8, unread)",
    ("logic-data", "a sound bank's cue entries"): "the cue byte (region-diff.md 8.3): nothing on the PC",
    ("text", "the area message block (an edit within one language)"): "text, not names: a language layer's",
    ("text", "the system message pool (an edit within one language)"): "text, not names: a language layer's "
                                                                       "(the battle pool's filled slot among them)",
}
# The PSP's own formats (region-diff.md 5.1) are not content changes: what
# psp-unwrap undoes, the base's business, not a layer's.
OPT_CONVERTED = "the PSP's format, not a content change (psp-unwrap; the base's, not a layer's)"


def name_records(src, t, n_len):
    """A disc's (PSX GAME.EMI or PSP BOOT.BIN) records of one name table, by
    tables.toml's recorded address for that build."""
    import tables
    where = next((w for w in t.get("psx", []) if w["build"] == src.id), None)
    if where is None:
        raise SystemExit("tables.toml records no %s table for %s" % (t["key"], src.id))
    st = t["stride"] - NAME_FIELD + n_len
    raw = tables.psx_bytes(src.build.disc, where, st * t["count"])
    return [raw[i * st:(i + 1) * st] for i in range(t["count"])]


def encode_name(raw, ja):
    """A disc name -> the PC's 16-byte field in a language layer's encoding
    (loc_build.py's, as its convert_names writes them); None when it cannot be
    encoded, or would need a pair code (DIV-0057), which belongs to the ja-JP
    layer's own pair table."""
    import loc_build
    loc_build.donor_ja = ja
    loc_build.ja_pairs.clear()
    enc = loc_build.ja_name(raw, NAME_ROOM) if ja else loc_build.encode_text(raw)
    if enc is None or len(enc) > NAME_ROOM or loc_build.ja_pairs:
        return None
    return enc.ljust(NAME_FIELD, b"\0")


def name_tables():
    """[(table, PC address of its record 0)] of the item and ability tables
    (tables.toml, symbols.toml)."""
    import tables
    cat, syms, _ = tables.load()
    return [(t, syms[t["symbol"]]) for t in cat["table"] if t.get("group") == "items" and "name" in t]


def name_len(bid):
    return 8 if tag_of(bid).startswith("ja") else 12


def cmd_opt_recipes(a):
    discs = {s.id: s for s in (open_source(p) for p in a.disc)}
    need = {"psx-jp", "psx-us"} | set(PSP)
    if need - set(discs):
        raise SystemExit("opt-recipes wants the discs %s; missing %s" % (", ".join(sorted(need)),
                                                                        ", ".join(sorted(need - set(discs)))))
    pc = open_source(a.dat)
    if pc.id != TARGET or not isinstance(pc, PcSource):
        raise SystemExit("%s is not the %s DAT tree" % (a.dat, TARGET))
    rec = load_recipe(a.recipe)
    files = {f["name"]: f for f in rec["file"]}
    where = {}
    for f in rec["file"]:
        for slot, ch in enumerate(f["chunks"]):
            for b, how, key, sec in sources_of(ch):
                if b == "psx-jp" and how == "copy":
                    where[(key, sec)] = (f["name"], slot)
    p = os.path.join(a.pairs, "psx-jp_vs_psp-jp.json")
    if not os.path.exists(p):
        raise SystemExit("%s missing: python tools/region_diff.py pair psx-jp=JP psp-jp=PSPJP --out %s" % (p, p))
    jp, pj, pe = discs["psx-jp"], discs["psp-jp"], discs["psp-eu"]
    layers = collections.defaultdict(lambda: collections.OrderedDict())
    excluded, stats = collections.Counter(), collections.Counter()
    for key, rows in sorted(json.load(open(p))["files"].items()):
        stem = key.rsplit("/", 1)[-1][:-4]
        for r in rows:
            if r["verdict"] == "identical" or r.get("a") is None or r.get("b") is None:
                continue
            _, t, dest, jb = jp.sections(key)[r["a"]]
            layer = opt_layer_of(stem, t, dest)
            if layer is None:
                excluded[(r.get("class"), r.get("what"))] += 1
                continue
            _, tb, db, pb = pj.sections(key)[r["b"]]
            # PSP-EU: the same section index (its kind-0 bands sit 0x8000 higher, the
            # Western layout, region-diff.md 7 (3)), of the same type and size
            eu = [s for s in [pe.sections(key).get(r["b"])] if s and s[1] == tb and len(s[3]) == len(pb)]
            if tb != t or region_nd(tb, db) != dest or len(pb) != len(jb) or len(eu) != 1:
                raise SystemExit("%s section %d: the PSP's twin is not the same shape" % (key, r["a"]))
            name = stem + ".DAT"
            got = where.get((key, r["a"]))
            if got is None:          # the port's own art over the JP section: the PC chunk by its tag
                got = next(((name, i) for i, c in enumerate(files[name]["chunks"])
                            if t == 3 and c["kind"] == 1 and c["tag"] == dest and c["layer"] == "base"), None)
            if got is None:
                raise SystemExit("%s section %d: no PC base chunk it lands on" % (key, r["a"]))
            slot = got[1]
            ch = files[name]["chunks"][slot]
            blob, chunks = pc.chunks(name)
            c = chunks[slot]
            pcb = blob[c.offset:c.offset + c.size]
            units = opt_units(layer, ch["kind"], ch["tag"], jb, pb, pcb)
            if not units:            # every tile the PSP changed is one the port redrew too
                stats[(layer, "sections wholly under the port's dial page")] += 1
                continue
            if layer == "psp-art" and [u[2] // CLUT_ROW + k for u in units for k in range(u[3] // CLUT_ROW)] != P6_ROWS[stem]:
                raise SystemExit("%s: the PSP's palette rows are not psp-stallion.md's" % name)
            composed = bytearray(pcb)
            out = []
            for kind, tag, at, size in units:
                body = pb[at:at + size]
                on = ["psp-jp"] + (["psp-eu"] if eu[0][3][at:at + size] == body else [])
                composed[at:at + size] = body
                out.append((kind, tag, at, size, sha(body), on))
                stats[(layer, "chunks")] += 1
                stats[(layer, "tiles" if kind == 1 else "bytes")] += size // PAGE_TILE if kind == 1 else size
                stats[(layer, "on both PSP discs")] += len(on) == 2
            if pcb == jb and bytes(composed) != pb:
                raise SystemExit("%s slot %d: the layer over the PC's chunk is not the PSP's section" % (name, slot))
            if pcb != jb:
                stats[(layer, "port-art sections (the port's tiles kept)")] += 1
            stats[(layer, "sections")] += 1
            layers[layer].setdefault(name, []).append((slot, key, r["b"], sha(bytes(composed)), out))
    walls_recipe(discs, jp, where, files, pc, layers, stats)
    for lname, (psp_id, base_id, ja) in NAME_LAYERS.items():
        src, base = discs[psp_id], discs[base_id]
        for t, pc_at in name_tables():
            n = name_len(psp_id)
            a_ = name_records(base, t, name_len(base_id))
            b_ = name_records(src, t, n)
            for i in range(t["count"]):
                if a_[i][:n] == b_[i][:n]:
                    continue
                field = encode_name(b_[i][:n].split(b"\0")[0], ja)
                if field is None:
                    raise SystemExit("%s: %s record %d cannot be encoded for the layer" % (lname, t["key"], i))
                tag = pc_at + t["name"]["at"] + i * t["stride"]
                layers[lname].setdefault(NAME_FILE, []).append((None, t["key"], i, None, [(5, tag, 0, NAME_FIELD, sha(field), [psp_id])]))
                stats[(lname, "records")] += 1
    head = [
        "# recipes/opt.toml - GENERATED by `tools/importer.py opt-recipes`; do not edit by hand.",
        "# The optional layers (docs/opt-layers.md): for each, every container it overlays and",
        "# every chunk - kind, tag, size, the sha256 of its payload, what it is cut from (`emi`",
        "# the PSP EMI and section, `at` the byte offset in it; or `table` / `record` for a name",
        "# from the PSP's BOOT.BIN), the PSP builds that carry it byte for byte (`on`) - and,",
        "# for each PC base chunk it lands on (`over`, its slot in recipes/pc-zh.toml), the",
        "# sha256 of that chunk with the layer laid over it (`composed`). Hashes and offsets",
        "# only, no game data (CLAUDE.md rule 1).",
        "",
        "[meta]",
        "target = %s" % toml_str(TARGET),
        "generated = %s" % datetime.date.today().isoformat(),
        "recipe_sha256 = %s" % toml_str(sha(open(a.recipe or RECIPE, "rb").read())),
        "pairs = [\"psx-jp_vs_psp-jp.json\"]",
    ]
    lines = []
    for lname in OPT_LAYERS:
        lines += ["", "[[layer]]", "name = %s" % toml_str(lname), "what = %s" % toml_str(OPT_LAYERS[lname])]
        for name, parts in sorted(layers[lname].items()):
            lines += ["[[layer.file]]", "name = %s" % toml_str(name)]
            if parts[0][0] is not None:
                lines.append("composed = [%s]" % ", ".join("{ over = %d, sha256 = %s }" % (s, toml_str(h)) for s, _, _, h, _ in parts))
            lines.append("chunks = [")
            for slot, key, sec, _, out in parts:
                for kind, tag, at, size, h, on in out:
                    src = ("over = %d, emi = %s, at = %d" % (slot, toml_str("%s#%d" % (key, sec)), at)) if slot is not None \
                        else "table = %s, record = %d" % (toml_str(key), sec)
                    lines.append("  { kind = %d, tag = 0x%08X, size = %d, sha256 = %s, %s, on = [%s] },"
                                 % (kind, tag, size, toml_str(h), src, ", ".join(toml_str(x) for x in on)))
            lines.append("]")
    out = a.out or OPT_RECIPE
    with open(out, "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(head + lines) + "\n")
    print("%s:" % out)
    for (layer, k), n in sorted(stats.items()):
        print("  %-17s %-42s %6d" % (layer, k, n))
    print("  not carried (region_diff class, what: sections):")
    for (k, what), n in sorted(excluded.items(), key=str):
        print("    %-10s %-34s %4d  %s" % (k, what, n, OPT_CONVERTED if k == "converted" else OPT_EXCLUDED[(k, what)]))


def walls_recipe(discs, jp, where, files, pc, layers, stats):
    """The area4-walls layer's two chunks (DIV-0080): AREA004's area block cell
    plane and its placement map, each whole as the US disc has it - not cut to
    the bytes that differ, so the recipe records no cell's place or value, only
    the planes' tags, sizes and hashes - with `on` the Western discs given that
    carry it byte for byte. The PC's chunks must be JP's sections exactly, the
    US disc's must differ from them, and the composed chunks must be the US
    disc's (the cell plane; the placement map whole)."""
    us = discs["psx-us"]
    stem = WALLS_EMI.rsplit("/", 1)[-1][:-4]
    name = stem + ".DAT"
    js = jp.sections(WALLS_EMI)
    western = [discs[b] for b in WESTERN if b in discs]
    for dest in WALLS_DESTS:
        idx = [i for i, s in js.items() if s[1] == 0 and s[2] == dest]
        if len(idx) != 1:
            raise SystemExit("%s: %d JP sections at 0x%08X" % (WALLS_EMI, len(idx), dest))
        i = idx[0]
        jb = js[i][3]
        twins = {}
        for s in western:
            t = s.sections(WALLS_EMI).get(i)
            if not t or t[1] != 0 or len(t[3]) != len(jb):
                raise SystemExit("%s section %d: %s's twin is not the same shape" % (WALLS_EMI, i, s.id))
            twins[s.id] = t[3]
        ub = twins["psx-us"]
        if dest == 0x80104000:       # the cell-byte plane alone (AreaMap_Bytes)
            lo = struct.unpack_from("<I", jb, 0x14)[0] * 4
            hi = lo + jb[0] * jb[1]
        else:
            lo, hi = 0, len(jb)
        got = where.get((WALLS_EMI, i))
        if got is None:
            raise SystemExit("%s section %d: no PC base chunk it lands on" % (WALLS_EMI, i))
        slot = got[1]
        ch = files[name]["chunks"][slot]
        blob, chunks = pc.chunks(name)
        c = chunks[slot]
        pcb = blob[c.offset:c.offset + c.size]
        if pcb != jb:
            raise SystemExit("%s slot %d: the PC's chunk is not JP's section" % (name, slot))
        changed = sum(jb[k] != ub[k] for k in range(lo, hi))
        if not changed:
            raise SystemExit("%s section %d: the US disc's equals JP's" % (WALLS_EMI, i))
        composed = bytearray(pcb)
        body = ub[lo:hi]
        on = [s for s in WESTERN if s in twins and twins[s][lo:hi] == body]
        composed[lo:hi] = body
        out = [(0, ch["tag"] + lo, lo, len(body), sha(body), on)]
        stats[(WALLS_LAYER, "chunks")] += 1
        stats[(WALLS_LAYER, "bytes")] += len(body)
        stats[(WALLS_LAYER, "bytes that differ from JP's")] += changed
        stats[(WALLS_LAYER, "chunks on every Western disc given")] += len(on) == len(twins)
        if composed[lo:hi] != ub[lo:hi]:
            raise SystemExit("%s slot %d: the layer over the PC's chunk is not the US disc's" % (name, slot))
        stats[(WALLS_LAYER, "sections")] += 1
        stats[(WALLS_LAYER, "Western discs read")] = len(twins)
        layers[WALLS_LAYER].setdefault(name, []).append((slot, WALLS_EMI, i, sha(bytes(composed)), out))


def region_nd(t, d):
    import region_diff
    return region_diff.nd(t, d)


def load_opt_recipe(path=None):
    with open(path or OPT_RECIPE, "rb") as f:
        r = tomllib.load(f)
    return {l["name"]: l for l in r.get("layer", [])}


def opt_payload(s, ch, cache):
    """One layer chunk's payload from a PSP source, before its hash check."""
    if ch["kind"] == 5:
        t = next(t for t, _ in name_tables() if t["key"] == ch["table"])
        rec = cache.setdefault((s.id, t["key"]), name_records(s, t, name_len(s.id)))[ch["record"]]
        return encode_name(rec[:name_len(s.id)].split(b"\0")[0], tag_of(s.id).startswith("ja"))
    key, sec = ch["emi"].rsplit("#", 1)
    body = s.sections(key)[int(sec)][3]
    return body[ch["at"]:ch["at"] + ch["size"]]


def build_opt(asked, sources, out, path=None):
    """opt/<name>/dat/NAME.DAT for each layer asked for, from the first disc in
    the player's order that carries each chunk (a PSP disc; for area4-walls a
    Western PSX one); every payload hashed against recipes/opt.toml. A layer is
    written whole or not at all."""
    if not asked:
        return []
    layers = load_opt_recipe(path)
    assets = []
    for name in asked:
        if name not in layers:
            raise SystemExit("--opt %s: no such layer; recipes/opt.toml has %s" % (name, ", ".join(layers)))
        psp = [s for s in sources if s.id in OPT_SOURCES and isinstance(s, DiscSource)]
        files, cache, used = [], {}, collections.Counter()
        for f in layers[name].get("file", []):
            parts = []
            for i, ch in enumerate(f["chunks"]):
                s = next((s for s in psp if s.id in ch["on"]), None)
                if s is None:
                    raise SystemExit("--opt %s: %s chunk %d is on %s; the sources given carry none of them"
                                     % (name, f["name"], i, " / ".join(ch["on"])))
                body = opt_payload(s, ch, cache)
                if body is None or sha(body) != ch["sha256"] or len(body) != ch["size"]:
                    raise SystemExit("--opt %s: %s chunk %d from %s: hash differs from recipes/opt.toml"
                                     % (name, f["name"], i, s.id))
                parts.append(chunk_bytes(ch["kind"], ch["tag"], body))
                src = ("%s:name:%s#%d" % (s.id, ch["table"], ch["record"]) if ch["kind"] == 5
                       else "%s:copy:%s@%d" % (s.id, ch["emi"], ch["at"]))
                assets.append((f["name"], "opt/" + name, i, src, ch["sha256"]))
                used[s.id] += 1
            files.append((f["name"], b"".join(parts)))
        d = os.path.join(out, "opt", name, "dat")
        os.makedirs(d, exist_ok=True)
        for fname, body in files:
            with open(os.path.join(d, fname), "wb") as fh:
                fh.write(body)
        print("  opt/%-16s %d containers, %s" % (name, len(files), ", ".join("%d chunks from %s" % (n, b) for b, n in used.items())))
    return assets


def verify_opt(cache, path=None):
    """Each opt/<name>/ layer of the cache: every chunk against the manifest and
    the recipe (kind, tag, hash), and every base chunk it lands on composed with
    it - the cache's base/ chunk with the layer laid over it, as LoadDatFile
    lays it (arena + tag, VRAM by the tag's rectangle) - against `composed`."""
    d = os.path.join(cache, "opt")
    if not os.path.isdir(d):
        return 0
    layers = load_opt_recipe(path)
    rec = {f["name"]: f for f in load_recipe()["file"]}
    with open(os.path.join(cache, "manifest.toml"), "rb") as fh:
        man = {(n, l, i): h for n, l, i, w, h in tomllib.load(fh)["cache"].get("opt", [])}
    rc = 0
    for name in sorted(os.listdir(d)):
        bad, chunks_ok, comp_ok, comp_absent = [], 0, 0, 0
        if name not in layers:
            print("verify opt/%s: not a layer recipes/opt.toml knows" % name)
            rc = 1
            continue
        want = {f["name"]: f for f in layers[name].get("file", [])}
        have = set(os.listdir(os.path.join(d, name, "dat")))
        if have != set(want):
            bad.append("containers: %d in the cache, %d in the recipe" % (len(have), len(want)))
        for fname in sorted(have & set(want)):
            blob, chunks = dat.load(os.path.join(d, name, "dat", fname))
            f = want[fname]
            if len(chunks) != len(f["chunks"]):
                bad.append("%s: %d chunks, the recipe has %d" % (fname, len(chunks), len(f["chunks"])))
                continue
            bodies = []
            for i, (c, ch) in enumerate(zip(chunks, f["chunks"])):
                body = blob[c.offset:c.offset + c.size]
                if (c.kind, c.tag) != (ch["kind"], ch["tag"]) or sha(body) != ch["sha256"] \
                        or man.get((fname, "opt/" + name, i)) != ch["sha256"]:
                    bad.append("%s chunk %d: not the chunk the recipe and manifest record" % (fname, i))
                else:
                    chunks_ok += 1
                bodies.append(body)
            for comp in f.get("composed", []):
                base = os.path.join(cache, "base", "dat", fname)
                if not os.path.exists(base):
                    comp_absent += 1
                    continue
                bblob, bchunks = dat.load(base)
                slots = [i for i, ch in enumerate(rec[fname]["chunks"]) if ch["layer"] == "base"]
                bc = bchunks[slots.index(comp["over"])]
                img = bytearray(bblob[bc.offset:bc.offset + bc.size])
                for ch, body in zip(f["chunks"], bodies):
                    if ch.get("over") != comp["over"]:
                        continue
                    if ch["kind"] == 0:
                        at = ch["tag"] - bc.tag
                    else:                        # the tile rectangle inside the page's
                        cols = (bc.tag >> 8) & 0xFF
                        at = (((ch["tag"] >> 16) & 0xFF) - ((bc.tag >> 16) & 0xFF)) * cols + (ch["tag"] >> 24) - (bc.tag >> 24)
                        at *= PAGE_TILE
                    if at < 0 or at + len(body) > len(img):
                        bad.append("%s: a chunk lands outside the chunk it overlays" % fname)
                        continue
                    img[at:at + len(body)] = body
                if sha(bytes(img)) == comp["sha256"]:
                    comp_ok += 1
                else:
                    bad.append("%s slot %d: composed with the layer, not the recipe's" % (fname, comp["over"]))
        ncomp = sum(len(f.get("composed", [])) for f in want.values())
        print("verify opt/%s: %d containers, %d of %d chunks as recorded, %d of %d base chunks composed as recorded%s, "
              "%d problem(s)" % (name, len(have), chunks_ok, sum(len(f["chunks"]) for f in want.values()), comp_ok,
                                 ncomp, " (%d not in this cache's base/)" % comp_absent if comp_absent else "", len(bad)))
        for b in bad[:10]:
            print("   ", b)
        rc |= bool(bad)
    return rc


def check_opt(path=None):
    """recipes/opt.toml against recipes/pc-zh.toml and fixtures.toml alone (CI):
    every layer known, every chunk a known kind landing inside a base chunk of
    its own kind (kind 0 inside the bytes, kind 1 inside the page's rectangle),
    every name chunk on one record of a name table, every source a PSP build
    (area4-walls: a Western PSX build)."""
    if not os.path.exists(path or OPT_RECIPE):
        return ["recipes/opt.toml missing"]
    layers = load_opt_recipe(path)
    rec = {f["name"]: f for f in load_recipe()["file"]}
    import loc_build          # the tables NameTables_Apply accepts (src/game/name_tables.cpp's kTables)
    tabs = [(va + at, st, n) for _, va, st, n, at in loc_build.NAME_TABLES]
    errs = []
    if set(layers) != set(OPT_LAYERS):
        errs.append("layers: %s" % ", ".join(sorted(set(layers) ^ set(OPT_LAYERS))))
    for lname, l in layers.items():
        for f in l.get("file", []):
            if f["name"] not in rec:
                errs.append("%s: %s not a PC container" % (lname, f["name"]))
                continue
            pcs = rec[f["name"]]["chunks"]
            for ch in f["chunks"]:
                allowed = WESTERN if lname == WALLS_LAYER else PSP
                if not ch["on"] or any(b not in allowed for b in ch["on"]) or len(ch["sha256"]) != 64:
                    errs.append("%s %s: source or hash" % (lname, f["name"]))
                if ch["kind"] == 5:
                    if lname not in NAME_LAYERS or ch["size"] != NAME_FIELD or not any(
                            ch["tag"] >= b and (ch["tag"] - b) % st == 0 and (ch["tag"] - b) // st < n for b, st, n in tabs):
                        errs.append("%s %s: name chunk 0x%X" % (lname, f["name"], ch["tag"]))
                    continue
                o = pcs[ch["over"]] if 0 <= ch.get("over", -1) < len(pcs) else None
                if o is None or o["layer"] != "base" or o["kind"] != ch["kind"]:
                    errs.append("%s %s: lands on no base chunk of its kind" % (lname, f["name"]))
                elif ch["kind"] == 0 and not (o["tag"] <= ch["tag"] and ch["tag"] + ch["size"] <= o["tag"] + o["size"]):
                    errs.append("%s %s: kind-0 chunk 0x%X outside 0x%X" % (lname, f["name"], ch["tag"], o["tag"]))
                elif ch["kind"] == 1:
                    cols, n = (o["tag"] >> 8) & 0xFF, (ch["tag"] >> 8) & 0xFF
                    x, y = (ch["tag"] >> 24) - (o["tag"] >> 24), ((ch["tag"] >> 16) & 0xFF) - ((o["tag"] >> 16) & 0xFF)
                    if not (0 <= x and x + n <= cols and 0 <= y and (y * cols + x + n) * PAGE_TILE <= o["size"]
                            and ch["size"] == n * PAGE_TILE):
                        errs.append("%s %s: kind-1 chunk 0x%08X outside 0x%08X" % (lname, f["name"], ch["tag"], o["tag"]))
            for comp in f.get("composed", []):
                if not any(ch.get("over") == comp["over"] for ch in f["chunks"]):
                    errs.append("%s %s: composed slot %d has no chunk" % (lname, f["name"], comp["over"]))
    return errs


# ---------------------------------------------------------------- install

def cmd_install(a):
    """Copy a cache's layers into a game's DAT/ under the overlay names the
    engine walks: loc/<tag>/dat/NAME.DAT as DAT/<tag>.NAME.DAT (DIV-0005),
    opt/<name>/dat/NAME.DAT as DAT/<name>.NAME.DAT (DIV-0086). A layer's old
    files in DAT/ (<name>.*.DAT) are removed first, so none is left stale. The
    engine reads DAT/ in the game directory until it reads the cache.

    A DEFAULT_OPT layer (area4-walls) is installed whenever the cache holds it;
    `--no-opt NAME` leaves it out and removes its files from DAT/, so the
    launcher's default (it plays an installed default layer) does not
    keep an old copy on."""
    import shutil
    for tag in a.lang:
        language_tags.refuse_retired(tag, "--lang")
    dat_dir = os.path.join(a.game, "DAT")
    if not os.path.isfile(os.path.join(dat_dir, "FIRST.DAT")):
        raise SystemExit("%s: no DAT/FIRST.DAT - not a game directory" % a.game)
    opt = default_opt(a.opt, a.no_opt, lambda name: os.path.isdir(os.path.join(a.cache, "opt", name, "dat")))
    for name in a.no_opt:
        old = [f for f in os.listdir(dat_dir) if f.startswith(name + ".")]
        for f in old:
            os.remove(os.path.join(dat_dir, f))
        print("opt/%s: left out%s" % (name, ", %d installed files removed from DAT/" % len(old) if old else ""))
    for kind, names in (("loc", a.lang), ("opt", opt)):
        for name in names:
            src = os.path.join(a.cache, kind, name, "dat")
            if not os.path.isdir(src):
                raise SystemExit("%s/%s is not in %s: build it first (importer.py build --%s %s)"
                                 % (kind, name, a.cache, "lang" if kind == "loc" else "opt", name))
            files = sorted(os.listdir(src))
            missing = [f for f in files if not os.path.isfile(os.path.join(dat_dir, f))]
            if missing:
                raise SystemExit("%s/%s overlays %s, which %s lacks" % (kind, name, ", ".join(missing[:5]), dat_dir))
            stale = [f for f in os.listdir(dat_dir) if f.startswith(name + ".") and f[len(name) + 1:] not in files]
            for f in stale:
                os.remove(os.path.join(dat_dir, f))
            for f in files:
                shutil.copyfile(os.path.join(src, f), os.path.join(dat_dir, "%s.%s" % (name, f)))
            print("%s/%s: %d files as DAT/%s.*.DAT%s" % (kind, name, len(files), name,
                                                          ", %d stale removed" % len(stale) if stale else ""))
            if kind == "opt" and name in DEFAULT_OPT:
                print("  played by default (the launcher's ini opt= empty); opt=none turns it off")
            elif kind == "opt":
                print("  play with opt=%s in bof3x.ini or BOF3X_OPT (comma-separated, in order, for several)" % name)
    # Overlays under a retired bare code (DAT/en.*.DAT, DIV-0005) are not
    # this install's to remove; it says they are dead weight.
    language_tags.note_retired_overlays(dat_dir)


# ---------------------------------------------------------------- presets
#
# A preset is a named source order plus layers (docs/unified-data-plan.md
# section 3 item 2). The player gives the files in any order; the preset puts
# them in its own, and refuses when one it names is missing or one given is not
# its. (build, what): `dat` a PC DAT/ tree, `exe` BOF3.exe, `disc` a disc.
# A preset with a Western PSX disc builds area4-walls from it as any build
# with such a disc does (DEFAULT_OPT; DIV-0080, the walls every later build
# has), so no preset lists it; `--no-opt area4-walls` leaves it out.
PRESETS = {
    "pc-install": ([("pc-zh", "dat"), ("pc-zh", "exe")], [], []),
    "us-disc": ([("psx-us", "disc")], [], []),
    "jp-disc": ([("psx-jp", "disc")], [], []),
    "eu-en-disc": ([("psx-eu-en", "disc")], [], []),
    "fr-disc": ([("psx-fr", "disc")], [], []),
    "de-disc": ([("psx-de", "disc")], [], []),
    "pc-plus-us-text": ([("pc-zh", "dat"), ("pc-zh", "exe"), ("psx-us", "disc")], ["en-US"], []),
    "pc-plus-eu-en-text": ([("pc-zh", "dat"), ("pc-zh", "exe"), ("psx-eu-en", "disc")], ["en-150"], []),
    "pc-plus-fr-text": ([("pc-zh", "dat"), ("pc-zh", "exe"), ("psx-fr", "disc")], ["fr-FR"], []),
    "pc-plus-de-text": ([("pc-zh", "dat"), ("pc-zh", "exe"), ("psx-de", "disc")], ["de-DE"], []),
    "pc-plus-jp-text": ([("pc-zh", "dat"), ("pc-zh", "exe"), ("psx-jp", "disc")], ["ja-JP"], []),
}


def source_kind(s):
    return "exe" if isinstance(s, ExeSource) else "dat" if isinstance(s, PcSource) else "disc"


def resolve_preset(name, sources):
    """(sources in the preset's order, its --lang, its --opt)."""
    if name not in PRESETS:
        raise SystemExit("--preset %s: no such preset; there are %s" % (name, ", ".join(PRESETS)))
    order, lang, opt = PRESETS[name]
    have = {(s.id, source_kind(s)): s for s in sources}
    missing = [w for w in order if w not in have]
    if missing:
        raise SystemExit("--preset %s wants %s; not given: %s" % (
            name, ", ".join("%s %s" % w for w in order), ", ".join("%s %s" % w for w in missing)))
    # A PSP disc beyond the preset's own is the --opt layers' source: it goes
    # last, where it can only fill what the preset's sources leave. Any other
    # extra would change what the preset means, and is refused.
    extra = [w for w in have if w not in order and w[0] not in PSP]
    if extra:
        raise SystemExit("--preset %s takes %s (and a PSP disc for --opt); also given: %s (drop them, or build "
                         "without the preset)" % (name, ", ".join("%s %s" % w for w in order),
                                                  ", ".join("%s %s" % w for w in extra)))
    psp = [w for w in have if w not in order]
    print("preset %s: %s%s%s%s" % (name, " then ".join("%s %s" % w for w in order),
                                  ", then %s for the optional layers" % " and ".join(w[0] for w in psp) if psp else "",
                                  "; --lang " + " ".join(lang) if lang else "", "; --opt " + " ".join(opt) if opt else ""))
    return [have[w] for w in order + psp], lang, opt


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
    rc |= verify_bgm(a.cache)
    rc |= verify_opt(a.cache, a.opt_recipe)
    if a.overlays:
        rc |= verify_overlays(a.cache, a.overlays)
    return rc


def verify_bgm(cache):
    """base/bgm/ (docs/seq-import.md): every file the manifest's `bgm` rows name,
    hashed against its row; no file there that no row names; each song and bank
    read back by seq.py's reader, and each song's bank present; then the rows
    against fixtures/bgm.tsv's for the build they came from - what seq.py makes
    of that build's disc. A cache built with no PSX disc has neither rows nor
    files, and passes."""
    with open(os.path.join(cache, "manifest.toml"), "rb") as fh:
        rows = tomllib.load(fh)["cache"].get("bgm", [])
    root = os.path.join(cache, "base", "bgm")
    held = set()
    for d, _, fs in os.walk(root):
        held |= {os.path.relpath(os.path.join(d, f), cache).replace(os.sep, "/") for f in fs}
    if not rows and not held:
        return 0
    errs, songs, banks, builds = [], {}, set(), set()
    for rel, where, h in rows:
        builds.add(where.split(":", 1)[0])
        p = os.path.join(cache, rel)
        if not os.path.exists(p):
            errs.append("%s: missing" % rel)
            continue
        with open(p, "rb") as fh:
            body = fh.read()
        if sha(body) != h:
            errs.append("%s: not the file the manifest recorded" % rel)
            continue
        try:
            if rel.startswith("base/bgm/bank/"):
                banks.add(seq.read_bank(body)["name"])
            else:
                songs[rel] = seq.read_song(body)["bank"]
        except (ValueError, struct.error) as e:
            errs.append("%s: %s" % (rel, e))
    for rel in sorted(held - {r[0] for r in rows}):
        errs.append("%s: in base/bgm/ but not in the manifest" % rel)
    for rel, b in sorted(songs.items()):
        if b not in banks:
            errs.append("%s: its bank %s is not in the cache" % (rel, b))
    fix, _ = seq.load_fixture() if os.path.exists(seq.FIXTURE) else ({}, None)
    against = []
    for bid in sorted(builds):
        want = fix.get(bid)
        if want is None:
            against.append("%s: no fixture rows" % bid)
            continue
        mine = {r[0]: r[2] for r in rows if r[1].startswith(bid + ":")}
        differ = sorted(r for r in mine if r not in want or want[r][2] != mine[r])
        absent = sorted(set(want) - set(mine))
        for r in differ[:10]:
            errs.append("%s: not what seq.py makes of the %s disc (fixtures/bgm.tsv)" % (r, bid))
        if absent:
            errs.append("%d file(s) fixtures/bgm.tsv lists for %s are not in the manifest: %s"
                        % (len(absent), bid, ", ".join(absent[:3])))
        against.append("%s: %d of %d as fixtures/bgm.tsv" % (bid, len(mine) - len(differ), len(want)))
    print("  base/bgm/ (%s): %d files, %d songs, %d banks; %s; %d problem(s)"
          % (", ".join(sorted(builds)) or "no build", len(held), len(songs), len(banks), "; ".join(against), len(errs)))
    for e in errs[:20]:
        print("   ", e)
    return 1 if errs else 0


def verify_overlays(cache, theirs):
    """Each loc/<tag>/ layer (but the target's own, which the recipe checks)
    against an install's <tag>.<NAME>.DAT overlays, byte for byte, both ways.
    A tag the install has no overlays under is not compared. Overlays under a
    retired bare code (`en.`, DIV-0005) are never compared: the engine
    refuses the code, so they are only noted, to delete."""
    rc = 0
    language_tags.note_retired_overlays(theirs)
    loc = os.path.join(cache, "loc")
    for tag in sorted(os.listdir(loc)) if os.path.isdir(loc) else []:
        if tag == tag_of(TARGET):
            continue
        d = os.path.join(loc, tag, "dat")
        ours = set(os.listdir(d))
        lang = tag
        if not any(f.startswith(tag + ".") for f in os.listdir(theirs)):
            print("verify loc/%s: the install has no %s.*.DAT - not compared" % (tag, tag))
            continue
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
    oerrs = check_opt(a.opt_recipe)
    serrs = ["seq.py: " + e for e in seq.check()]     # base/bgm/'s writer and reader, a synthetic round trip
    serrs += ["seq.py: " + e for e in seq.check_fixture(builds)]   # fixtures/bgm.tsv's shape and format versions
    import cache_walk                                  # DIV-0089: the engine's cache walk against cmd_install
    werrs = cache_walk.check()
    for e in (errs + oerrs + serrs + werrs)[:30]:
        print("ERROR", e)
    print("importer check: %d files, %d chunks, %d error(s); recipes/opt.toml %d layers, %d error(s)"
          % (len(names), n, len(errs), len(load_opt_recipe(a.opt_recipe)) if os.path.exists(a.opt_recipe or OPT_RECIPE) else 0,
             len(oerrs)))
    print("seq check: synthetic SEP and VAB round trip, fixtures/bgm.tsv, %d error(s)" % len(serrs))
    print("cache_walk check: the engine's cache walk against install on a synthetic cache, %d error(s)" % len(werrs))
    errs += oerrs + serrs + werrs
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
                   help="a language layer to build (repeat): a tag (en-US, en-150, fr-FR, de-DE, ja-JP; the "
                        "bare en/fr/de/ja are retired, DIV-0005); needs the PC's DAT/, BOF3.exe and that disc")
    p.add_argument("--opt", action="append", default=[],
                   help="an optional layer to build (repeat): %s; needs a PSP disc, area4-walls a US, European, "
                        "French or German PSX disc, and is built by default when one is given (docs/opt-layers.md)"
                        % ", ".join(OPT_LAYERS))
    p.add_argument("--no-opt", action="append", default=[],
                   help="a default layer not to build (repeat): %s" % ", ".join(DEFAULT_OPT))
    p.add_argument("--preset", help="a named source order plus layers: %s; the sources are still given "
                                    "with --source, in any order" % ", ".join(PRESETS))
    p.add_argument("--recipe")
    p.add_argument("--opt-recipe")
    p = s.add_parser("opt-recipes")
    p.add_argument("--dat", required=True, help="the PC port's DAT/ directory")
    p.add_argument("--disc", action="append", default=[], help="the JP, US and both PSP discs, and any other "
                   "Western PSX disc for area4-walls' `on`; repeat")
    p.add_argument("--pairs", default=os.path.join(ROOT, "analysis", "region"),
                   help="where region_diff.py pair's psx-jp_vs_psp-jp.json is")
    p.add_argument("--recipe")
    p.add_argument("--out")
    p = s.add_parser("install")
    p.add_argument("--cache", required=True)
    p.add_argument("--game", required=True, help="a game directory: its DAT/ receives the overlays")
    p.add_argument("--lang", action="append", default=[], help="a loc/<tag>/ layer to install (repeat)")
    p.add_argument("--opt", action="append", default=[], help="an opt/<name>/ layer to install (repeat)")
    p.add_argument("--no-opt", action="append", default=[],
                   help="a default layer (%s, installed whenever the cache holds it) to leave out, removing "
                        "its files from DAT/ (repeat)" % ", ".join(DEFAULT_OPT))
    p = s.add_parser("verify")
    p.add_argument("--cache", required=True)
    p.add_argument("--overlays", help="an install's DAT/ whose <lang>.*.DAT overlays the cache's loc/ layers must equal")
    p.add_argument("--recipe")
    p.add_argument("--opt-recipe")
    p = s.add_parser("check")
    p.add_argument("--recipe")
    p.add_argument("--opt-recipe")
    a = ap.parse_args()
    fn = {"recipes": cmd_recipes, "identify": cmd_identify, "build": cmd_build, "verify": cmd_verify,
          "check": cmd_check, "opt-recipes": cmd_opt_recipes, "install": cmd_install}[a.cmd]
    sys.exit(fn(a) or 0)


if __name__ == "__main__":
    main()
