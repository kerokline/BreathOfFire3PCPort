#!/usr/bin/env python
"""The static differential check of a disc-built .data image against the PC's, per data family.

docs/data-diff.md. A disc-built install's base/exe/ image (exe_tables.py
`disc_built`, the image the importer writes) zeroes what no rule places; the
gap list (recipes/exe-gaps.tsv) classes those runs, but its `unread` class only
says no watched route read them - issues 130, 134 and 139 were such runs inside
the area descriptors and their link lists. This tool decodes the tables the
game reads and a player meets early, from the PC's .data and from each disc's
image in memory, and compares them value by value:

    area_descriptor   the 200 descriptors' header words (Area_Descriptors)
    area_link         each area's links (+0x20, +0x30) and the alternatives of
                      an indirect link (+0x24, +0x28): Area_LinkAt's reads
    area_zone         each area's zone records (Area_StateRecords): box, zone id,
                      track or music set - Area_ZoneAtIn's and Area_PickMusic's
    music_set         the twelve music sets at 0x669A48 (Area_ZoneTrack's)
    <table key>       every tables.toml table in .data, field by field
                      (tables.py `records`): the item tables, abilities, shops,
                      the EXP rows, the event battles and the encounter tables

A value the PC has that the disc-built image cannot reach (a link past a
zeroed count), or that is non-zero on the PC and zero in the image where no
disc placed the bytes, is a **hole**; any other difference - the disc's own
placed value, zero or not - is a **release difference**. Each must be listed in
recipes/data-known.toml (`kind = "difference"` with its reason, `kind =
"hole"` with where it is tracked); anything unlisted fails. Fields whose bytes
the engine fills itself at run time (exe_tables' `own` and `keys` classes) and
the 16-byte name fields (a language layer's, tables.toml [meta]) are not
compared; the first are counted as `runtime`.

    python tools/data_diff.py run --exe BOF3.exe --disc DISC [--disc DISC ...]
                                  [--report recipes/data-diff.tsv] [--values FILE] [--tree DIR]
    python tools/data_diff.py check        # CI: no game data - the committed report
    python tools/data_diff.py selftest     # the decoders and the verdicts on synthetic images

`run` writes the report (keys, verdicts and provenance - never a value: CLAUDE.md
rule 1) and checks it; `--values` writes the differing values too, for reading
by hand, and only outside the repository. `--tree` imports exe_tables, tables
and importer from another checkout (an older recipe, as the issue-130
regression check in docs/data-diff.md 4 does). `check` reads the committed
report with no game data: every row listed in data-known.toml, every entry
still matching a row, and the report made from the images recipes/exe.toml
now describes and the views this tool now decodes - a recipe change without a
re-run of `run` fails here.
"""
import argparse
import collections
import fnmatch
import hashlib
import json
import os
import struct
import sys
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
REPORT = os.path.join(ROOT, "recipes", "data-diff.tsv")
KNOWN = os.path.join(ROOT, "recipes", "data-known.toml")
EXE_TOML = os.path.join(ROOT, "recipes", "exe.toml")
TABLES = os.path.join(ROOT, "tables.toml")
SYMBOLS = os.path.join(ROOT, "symbols.toml")

DATA_LO, DATA_HI = 0x5DA000, 0x676000     # .data's initialised bytes (exe_tables.DATA_LO / DATA_HI)
PC = "pc-zh"
TYPES = {"u8": ("<B", 1), "s8": ("<b", 1), "u16": ("<H", 2), "s16": ("<h", 2), "u32": ("<I", 4), "s32": ("<i", 4)}
REPORT_COLS = ["build", "family", "key", "verdict", "how"]
VERDICTS = ("hole", "differ")
KINDS = {"hole": ("hole",), "difference": ("differ",)}
RUNTIME = ("own", "keys")                 # bytes the engine fills itself at run time: not compared
# `language` is not among them: its class comes partly from a symbol-name rule
# (exe_own.py NAME_RULES), which on the PSP builds reached into area 6's
# descriptor (docs/data-diff.md 3) - no decoded field here is text.
PLACED = ("map", "place", "table", "widen", "rebuilt", "layout")   # the image's bytes a disc or the layout gave

# ------------------------------------------------------------------ the views
#
# The descriptor, link, zone and music-set layouts are the readers' own, as
# the reimplementations read them (no record type in src/game/records.h holds
# them: the readers take raw offsets). Each line cites its reader.

AREAS = 200                               # Area_Descriptors' count (symbols.toml, tables.toml area_descriptors)
DESC_SIZE = 0x44                          # +0x40 the last word read (init); a dispatcher's table may follow (area_w1d.md)
DESC_FIELDS = (                           # (offset, type, name)
    (0x00, "u32", "word_00"),
    (0x04, "u32", "word_04"),
    (0x08, "u32", "model"),               # area_w2f.md (Area108_Model), area_w1c.md
    (0x0C, "u32", "word_0c"),
    (0x10, "u32", "move_scripts"),        # MoveScript_Flow, Field_ObjectStep (symbols.toml)
    (0x14, "u32", "entries"),             # Party_DropIn (area_entry.cpp)
    (0x18, "u32", "scripts"),             # Party_DropIn's script pointer (area_entry.cpp)
    (0x1C, "u32", "entry_lists"),         # tables.toml area_descriptors' `what`
    (0x20, "u32", "links"),               # Area_LinkAt (area_entry.cpp)
    (0x24, "u32", "alt_lists"),           # Area_LinkAt, an indirect link's lists
    (0x28, "u32", "alt_counts"),          # Area_LinkAt, their counts
    (0x2C, "u32", "word_2c"),
    (0x30, "u8", "link_last"),            # Area_LinkAt: the index of the last link
    (0x31, "u8", "byte_31"),
    (0x32, "u8", "input_flags"),          # GameMode_Enter's Field_InputFlags byte (exe-import-engine.md 7.8)
    (0x33, "u8", "byte_33"),
    (0x34, "u32", "choices"),             # MsgBox_ChoiceCommit (area-rows.md)
    (0x38, "u32", "colour_matrix"),       # area_w2a.md
    (0x3C, "u32", "handlers"),            # MoveScript_Flow op 03 (area-rows.md)
    (0x40, "u32", "init"),                # area-rows.md
)
LINK_SIZE = 12
LINK_FIELDS = (                           # Area_LinkAt (area_entry.cpp, area-entry.md)
    (0x0, "u8", "x"), (0x1, "u8", "z"), (0x2, "u16", "dest_area"), (0x4, "u16", "dest_x"),
    (0x6, "u16", "dest_z"), (0x8, "u8", "along_z"), (0x9, "u8", "run"), (0xA, "u8", "flags"),
)
INDIRECT = 0xFFFF                         # +2: word +4 indexes the alternatives
ALT_SIZE = 10
ALT_FIELDS = (                            # Area_LinkAt's alternative: area, x, z, Flags_Test(row, bit), flags
    (0x0, "u16", "area"), (0x2, "u16", "x"), (0x4, "u16", "z"), (0x6, "u8", "flag_row"),
    (0x7, "u8", "flag_bit"), (0x8, "u8", "flags"),
)
ZONE_TABLE = 0x668D80                     # Area_StateRecords: a zone list per area (Area_ZoneAtIn, window_task.cpp)
ZONE_SIZE = 8
ZONE_MAX = 64                             # a list runs to the next list (no end test in the reader)
ZONE_FIELDS = (                           # Area_ZoneAtIn's box, Area_ZoneIdAt's +5, Area_ZoneTrack's +6 / +7
    (0x0, "u8", "x0"), (0x1, "u8", "z0"), (0x2, "u8", "x1"), (0x3, "u8", "z1"), (0x4, "u8", "byte_4"),
    (0x5, "u8", "zone_id"), (0x6, "u8", "track_or_set"), (0x7, "u8", "is_set"),
)
MUSIC_SETS, MUSIC_SETS_N = 0x669A48, 12   # window_task_callees.h kMusicSets; window_task_fuzz.cpp kSets
MUSIC_ENTRY = (                           # Area_ZoneTrack: (track, flag row, flag bit), the fallback after
    (0x0, "u8", "track"), (0x1, "u8", "flag_row"), (0x2, "u8", "flag_bit"),
)


def views_digest(cat):
    """A hash of what this tool decodes: its own layouts and every compared
    table's shape in tables.toml (not its prose). The report carries it, so
    a changed view without a re-run fails `check`."""
    shape = [DESC_SIZE, DESC_FIELDS, LINK_SIZE, LINK_FIELDS, ALT_SIZE, ALT_FIELDS, ZONE_TABLE, ZONE_SIZE, ZONE_MAX,
             ZONE_FIELDS, MUSIC_SETS, MUSIC_SETS_N, MUSIC_ENTRY, RUNTIME]
    for t in table_list(cat):
        shape.append([t["key"], t["symbol"], t["count"], t["stride"], t.get("name"),
                      [[f["at"], f["type"], f["name"], f.get("bits"), f.get("repeat")] for f in t.get("field", [])]])
    return hashlib.sha256(json.dumps(shape, sort_keys=True, default=list).encode()).hexdigest()


def table_list(cat):
    return [t for t in cat.get("table", []) if "symbol" in t and t.get("field")]


class Image:
    """A .data image at DATA_LO, read by PC address; None outside it."""

    def __init__(self, data):
        self.data = data

    def inside(self, a, n=1):
        return DATA_LO <= a and a + n <= DATA_LO + len(self.data)

    def read(self, a, typ):
        code, w = TYPES[typ]
        if not self.inside(a, w):
            return None
        return struct.unpack_from(code, self.data, a - DATA_LO)[0]


def put_fields(out, prefix, img, base, fields):
    for off, typ, name in fields:
        v = img.read(base + off, typ)
        if v is not None:
            out[prefix + name] = (v, base + off, TYPES[typ][1])


def decode_areas(img):
    """{key: (value, PC address, width)} for area_descriptor, area_link,
    area_zone and music_set, each image through its own pointers."""
    out = {}
    descs = [img.read(0x667590 + 4 * a, "u32") for a in range(AREAS)]     # Area_Descriptors
    for a, d in enumerate(descs):
        out["area_descriptor/a%03d/pointer" % a] = (d, 0x667590 + 4 * a, 4)
        if d is None or not img.inside(d, DESC_SIZE):
            continue
        put_fields(out, "area_descriptor/a%03d/" % a, img, d, DESC_FIELDS)
        links, last = img.read(d + 0x20, "u32"), img.read(d + 0x30, "u8")
        if not links or not img.inside(links, LINK_SIZE * (last + 1)):
            continue
        for n in range(last + 1):
            at = links + n * LINK_SIZE
            pre = "area_link/a%03d/l%02d/" % (a, n)
            put_fields(out, pre, img, at, LINK_FIELDS)
            if img.read(at + 2, "u16") != INDIRECT:
                continue
            idx = img.read(at + 4, "u16")
            lists, counts = img.read(d + 0x24, "u32"), img.read(d + 0x28, "u32")
            lst = img.read(lists + 4 * idx, "u32") if lists else None
            cnt = img.read(counts + idx, "u8") if counts else None
            if lst is None or cnt is None:
                continue
            out[pre + "alt_list"] = (lst, lists + 4 * idx, 4)
            out[pre + "alt_count"] = (cnt, counts + idx, 1)
            for k in range(cnt + 1):           # the first that holds, or the one after the last
                put_fields(out, pre + "alt%d/" % k, img, lst + k * ALT_SIZE, ALT_FIELDS)
    zones = [img.read(ZONE_TABLE + 4 * a, "u32") for a in range(AREAS)]
    starts = sorted({z for z in zones if z and img.inside(z)} | {ZONE_TABLE})
    for a, z in enumerate(zones):
        out["area_zone/a%03d/pointer" % a] = (z, ZONE_TABLE + 4 * a, 4)
        if not z or not img.inside(z):
            continue
        nxt = min([s for s in starts if s > z] or [z + ZONE_SIZE])
        for k in range(min(ZONE_MAX, (nxt - z) // ZONE_SIZE)):
            put_fields(out, "area_zone/a%03d/z%02d/" % (a, k), img, z + k * ZONE_SIZE, ZONE_FIELDS)
    for s in range(MUSIC_SETS_N):
        at = MUSIC_SETS + 8 * s
        lst, cnt = img.read(at, "u32"), img.read(at + 4, "u8")
        out["music_set/s%02d/list" % s] = (lst, at, 4)
        out["music_set/s%02d/count" % s] = (cnt, at + 4, 1)
        if not lst or not img.inside(lst):
            continue
        for k in range(cnt):
            put_fields(out, "music_set/s%02d/e%d/" % (s, k), img, lst + 3 * k, MUSIC_ENTRY)
        put_fields(out, "music_set/s%02d/fallback_" % s, img, lst + 3 * cnt, MUSIC_ENTRY[:1])
    return out


def decode_tables(img, cat, syms, tables_mod):
    """{key: (value, PC address, width)} for every tables.toml table in .data,
    field by field through tables.py's `records` (the PC layout, the name
    field skipped: a language layer's)."""
    out = {}
    for t in table_list(cat):
        lo = syms[t["symbol"]]
        size = t["stride"] * t["count"]
        if not img.inside(lo, size):
            continue
        raw = img.data[lo - DATA_LO:lo - DATA_LO + size]
        rows = tables_mod.records(t, raw, tables_mod.PC_NAME_LEN)
        widths = {}
        for f in t["field"]:
            n_rep, step = f.get("repeat", [1, TYPES[f["type"]][1]])
            for r in range(n_rep):
                widths[f["name"] if n_rep == 1 else "%s_%d" % (f["name"], r)] = (f["at"] + r * step, TYPES[f["type"]][1])
        for row in rows:
            i = row["id"]
            for name, v in row.items():
                if name in ("id", "name"):
                    continue
                off, w = widths[name]
                out["%s/%04d/%s" % (t["key"], i, name)] = (v, lo + i * t["stride"] + off, w)
    return out


def family(key):
    return key.split("/", 1)[0]


# ------------------------------------------------------------------ the diff

def diff(pc, disc, how_of):
    """[(key, verdict, how)] for one build, and the per-family counts
    {family: Counter(compared=, runtime=, hole=, differ=)}. `how_of(addr, width)`
    names where the disc image's bytes came from, or one of RUNTIME."""
    rows, counts = [], collections.defaultdict(collections.Counter)
    for key in sorted(set(pc) | set(disc)):
        fam = family(key)
        p, d = pc.get(key), disc.get(key)
        at, w = (d or p)[1], (d or p)[2]
        how = how_of(at, w)
        if how in RUNTIME:
            counts[fam]["runtime"] += 1
            continue
        counts[fam]["compared"] += 1
        pv, dv = (p[0] if p else None), (d[0] if d else None)
        if pv == dv:
            continue
        # a hole: a value the PC has and the image's decode does not reach (a link
        # past a zeroed count), or the PC's non-zero where the image's bytes are
        # zero and no disc placed them; a zero the disc placed is its own value
        unfilled = any(h not in PLACED for h in how.split("+"))
        verdict = "hole" if p and (d is None or (pv and not dv and unfilled)) else "differ"
        counts[fam][verdict] += 1
        rows.append((key, verdict, how))
    return rows, counts


def provenance(owner, ranges, gaps, own, pointer_words):
    """how_of(addr, width): the disc image's source of those bytes - a range's
    `how` (map, place, table, widen, rebuilt, layout), or for bytes nothing
    filled their class: own, pointer, a gap row's how (gap, language, unread,
    keys), else none. Mixed bytes join with '+'."""
    def one(x):
        k = owner[x - DATA_LO]
        if k is not None:
            return ranges[k][2]
        if x in own:
            return "own"
        if x in gaps:
            return gaps[x]
        if x in pointer_words:
            return "pointer"
        return "none"

    def how_of(at, w):
        hs = []
        for x in range(at, at + w):
            if DATA_LO <= x < DATA_HI:
                h = one(x)
                if h not in hs:
                    hs.append(h)
        return "+".join(hs) if hs else "outside"
    return how_of


# ------------------------------------------------------------------ the known list

def load_known(path=KNOWN):
    with open(path, "rb") as f:
        doc = tomllib.load(f)
    errs, ids = [], set()
    rules = doc.get("known", [])
    for i, r in enumerate(rules):
        where = "data-known.toml entry %d (%s)" % (i + 1, r.get("id", "?"))
        for k in ("id", "kind", "keys", "reason", "cite"):
            if k not in r:
                errs.append("%s: no `%s`" % (where, k))
        if r.get("kind") not in KINDS:
            errs.append("%s: kind %r is not one of %s" % (where, r.get("kind"), ", ".join(KINDS)))
        if r.get("kind") == "hole" and "tracked" not in r:
            errs.append("%s: a listed hole names where it is tracked (`tracked`)" % where)
        if r.get("id") in ids:
            errs.append("%s: id repeated" % where)
        ids.add(r.get("id"))
        if not isinstance(r.get("keys", []), list) or not r.get("keys"):
            errs.append("%s: `keys` is a non-empty list of patterns" % where)
    return rules, errs


def matches(rule, bid, key, verdict):
    if verdict not in KINDS.get(rule.get("kind"), ()):
        return False
    if "builds" in rule and bid not in rule["builds"]:
        return False
    return any(fnmatch.fnmatchcase(key, pat) for pat in rule.get("keys", []))


def judge(rows, rules):
    """(unexplained rows, {rule id: rows matched})."""
    used, bad = collections.Counter(), []
    for bid, fam, key, verdict, how in rows:
        hit = [r["id"] for r in rules if matches(r, bid, key, verdict)]
        if not hit:
            bad.append((bid, fam, key, verdict, how))
        for h in hit:
            used[h] += 1
    return bad, used


# ------------------------------------------------------------------ the report

def write_report(path, images, digest, counts, rows):
    lines = ["# recipes/data-diff.tsv - GENERATED by `tools/data_diff.py run`; do not edit (docs/data-diff.md).",
             "# Per disc build, every decoded value of the disc-built .data image that differs from the",
             "# PC's: its key, `hole` (the PC's non-zero, the image's zero or missing) or `differ`, and where",
             "# the image's bytes came from. Keys, verdicts and provenance only - no value (CLAUDE.md rule 1).",
             "# Every row is listed in recipes/data-known.toml; `data_diff.py check` holds that in CI.",
             "# views: " + digest]
    for bid in sorted(images):
        lines.append("# image: %s %s" % (bid, images[bid]))
    for bid in sorted(counts):
        for fam in sorted(counts[bid]):
            c = counts[bid][fam]
            lines.append("# compared: %s %s %d runtime %d hole %d differ %d" % (
                bid, fam, c["compared"], c["runtime"], c["hole"], c["differ"]))
    lines.append("\t".join(REPORT_COLS))
    for r in rows:
        lines.append("\t".join(r))
    with open(path, "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def read_report(path=REPORT):
    meta = {"views": None, "images": {}, "compared": []}
    rows = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("# views: "):
                meta["views"] = line.split(": ", 1)[1]
            elif line.startswith("# image: "):
                bid, h = line.split(": ", 1)[1].split()
                meta["images"][bid] = h
            elif line.startswith("# compared: "):
                meta["compared"].append(line.split(": ", 1)[1].split())
            elif line.startswith("#") or line.split("\t") == REPORT_COLS:
                continue
            elif line:
                r = line.split("\t")
                if len(r) != len(REPORT_COLS):
                    raise SystemExit("data_diff: %s: a row of %d columns: %r" % (path, len(r), line))
                rows.append(tuple(r))
    return meta, rows


def explain(rows, rules, used_out=None):
    bad, used = judge(rows, rules)
    errs = ["unlisted %s: %s %s (bytes from %s) - fix the importer or list it in recipes/data-known.toml"
            % (v, b, k, h) for b, _, k, v, h in bad]
    for r in rules:
        if not used[r["id"]]:
            errs.append("data-known.toml %s matches no row of the report: stale, remove it" % r["id"])
    if used_out is not None:
        used_out.update(used)
    return errs


# ------------------------------------------------------------------ commands

def cmd_check(a):
    """The committed report against the repository alone (no game data)."""
    import tables as tables_mod
    errs = []
    rules, kerrs = load_known(a.known)
    errs += kerrs
    meta, rows = read_report(a.report)
    with open(EXE_TOML, "rb") as f:
        exe = tomllib.load(f)
    want = {b: v["sha256"] for b, v in exe.get("build", {}).items() if b != PC}
    if meta["images"] != want:
        for b in sorted(set(want) | set(meta["images"])):
            if meta["images"].get(b) != want.get(b):
                errs.append("%s: the report was made from image %s, recipes/exe.toml now says %s - re-run "
                            "`tools/data_diff.py run` with the discs" % (b, meta["images"].get(b), want.get(b)))
    cat, _, _ = tables_mod.load()
    if meta["views"] != views_digest(cat):
        errs.append("the decoded views changed since the report (tables.toml's shapes or this tool's layouts): "
                    "re-run `tools/data_diff.py run` with the discs")
    seen = {(b, f) for b, f, *_ in meta["compared"]}
    for b in want:
        if not any(x[0] == b for x in seen):
            errs.append("%s: the report compared nothing for this build" % b)
    used = collections.Counter()
    errs += explain(rows, rules, used)
    errs += selftest_errors()
    for e in errs:
        print("error: " + e)
    holes = sum(1 for r in rows if r[3] == "hole")
    print("data_diff: %d builds, %d rows (%d holes, %d differences), %d known entries, %d error(s)"
          % (len(meta["images"]), len(rows), holes, len(rows) - holes, len(rules), len(errs)))
    return 1 if errs else 0


def out_outside_repo(p):
    o = os.path.abspath(p)
    if o.startswith(ROOT + os.sep):
        raise SystemExit("data_diff: --values holds game values: write it outside the repository (CLAUDE.md rule 1)")
    os.makedirs(os.path.dirname(o) or ".", exist_ok=True)
    return o


def cmd_run(a):
    if a.tree:
        sys.path.insert(0, os.path.join(os.path.abspath(a.tree), "tools"))
    import exe_tables
    import importer
    import psx_disc
    import tables as tables_mod
    print("modules from %s" % os.path.dirname(os.path.abspath(exe_tables.__file__)))
    cat, syms, _ = tables_mod.load()
    pc_img, _ = exe_tables.pc_data(a.exe)
    pc_vals = decode_areas(Image(pc_img))
    pc_vals.update(decode_tables(Image(pc_img), cat, syms, tables_mod))
    pointers = exe_tables.load_pointers()
    refused = exe_tables.load_rebuild_refused()
    gaps_all, _ = exe_tables.load_gaps()
    own = exe_tables.own_bytes()
    pointer_words = set()
    for p, words, _ in pointers:
        pointer_words.update(range(p, p + 4 * words))
    recipe = exe_tables.load_recipe()
    images, counts, rows, values = {}, {}, [], []
    for path in a.disc:
        ident = importer.identify(path)
        if not ident:
            raise SystemExit("%s: not a catalogued build" % path)
        bid = ident[0]
        img, owner, ranges, _, _ = exe_tables.disc_built(psx_disc.Disc(path), bid, pointers, refused[bid])
        h = hashlib.sha256(bytes(img)).hexdigest()
        if h != recipe["build"][bid]["sha256"]:
            raise SystemExit("%s: the image hashes %s, recipes/exe.toml says %s" % (bid, h, recipe["build"][bid]["sha256"]))
        images[bid] = h
        gaps = {}
        for lo, hi, how, _, _ in gaps_all.get(bid, ()):
            for x in range(lo, hi):
                gaps[x] = how
        how_of = provenance(owner, ranges, gaps, own, pointer_words)
        dv = decode_areas(Image(img))
        dv.update(decode_tables(Image(img), cat, syms, tables_mod))
        r, c = diff(pc_vals, dv, how_of)
        counts[bid] = c
        for key, verdict, how in r:
            rows.append((bid, family(key), key, verdict, how))
            values.append((bid, key, verdict, how, pc_vals.get(key, (None,))[0], dv.get(key, (None,))[0]))
        print("%-9s %s" % (bid, ", ".join("%s %d/%d" % (f, c[f]["hole"], c[f]["differ"]) for f in sorted(c)
                                          if c[f]["hole"] or c[f]["differ"]) or "no difference"))
    if not a.tree:
        digest = views_digest(cat)
    else:
        digest = views_digest(cat) + " (tree %s)" % a.tree
    write_report(a.report, images, digest, counts, rows)
    print("%d rows -> %s" % (len(rows), a.report))
    if a.values:
        with open(out_outside_repo(a.values), "w", newline="\n", encoding="utf-8") as f:
            f.write("build\tkey\tverdict\thow\tpc\tdisc\n")
            for bid, key, verdict, how, pv, dv_ in values:
                fmt = lambda v: "-" if v is None else "0x%X" % v if v > 0xFFFF else str(v)
                f.write("%s\t%s\t%s\t%s\t%s\t%s\n" % (bid, key, verdict, how, fmt(pv), fmt(dv_)))
        print("values -> %s" % a.values)
    rules, errs = load_known(a.known)
    errs += explain(rows, rules)
    for e in errs:
        print("error: " + e)
    print("data_diff: %d rows, %d error(s)" % (len(rows), len(errs)))
    return 1 if errs else 0


# ------------------------------------------------------------------ self-test

def synthetic():
    """A PC image and a disc image of one area (area 0): a descriptor, two
    links (one indirect, two alternatives), a zone list and a music set -
    made-up numbers, no game data."""
    img = bytearray(DATA_HI - DATA_LO)

    def w(a, typ, v):
        struct.pack_into(TYPES[typ][0], img, a - DATA_LO, v)
    d, links, lists, counts, alts = 0x600000, 0x600100, 0x600200, 0x600210, 0x600300
    w(0x667590, "u32", d)
    w(d + 0x20, "u32", links)
    w(d + 0x24, "u32", lists)
    w(d + 0x28, "u32", counts)
    w(d + 0x30, "u8", 1)
    w(links + 0, "u8", 5); w(links + 2, "u16", 7); w(links + 4, "u16", 9); w(links + 9, "u8", 2)
    w(links + 12, "u8", 6); w(links + 14, "u16", INDIRECT); w(links + 16, "u16", 0)
    w(lists, "u32", alts)
    w(counts, "u8", 1)
    w(alts, "u16", 3); w(alts + 10, "u16", 4)
    zones = 0x667800
    w(ZONE_TABLE, "u32", zones)
    w(zones + 2, "u8", 0xFF); w(zones + 3, "u8", 0xFF); w(zones + 6, "u8", 12)
    tracks = 0x669B00
    w(MUSIC_SETS, "u32", tracks); w(MUSIC_SETS + 4, "u8", 1)
    w(tracks, "u8", 30); w(tracks + 3, "u8", 31)
    return img, w


def selftest_errors():
    errs = []
    pc, _ = synthetic()
    same = decode_areas(Image(pc))
    want = {"area_descriptor/a000/link_last": 1, "area_link/a000/l00/dest_area": 7, "area_link/a000/l01/dest_area": INDIRECT,
            "area_link/a000/l01/alt0/area": 3, "area_link/a000/l01/alt1/area": 4, "area_zone/a000/z00/track_or_set": 12,
            "music_set/s00/e0/track": 30, "music_set/s00/fallback_track": 31}
    for k, v in want.items():
        if same.get(k, (None,))[0] != v:
            errs.append("selftest: %s decodes %r, not %r" % (k, same.get(k, (None,))[0], v))
    how_of = lambda at, w: "map"
    rows, _ = diff(same, same, how_of)
    if rows:
        errs.append("selftest: an image against itself differs: %r" % rows[:3])
    # issue 130's shape: the link count left unfilled - the second link vanishes
    disc, w = synthetic()
    w(0x600000 + 0x30, "u8", 0)
    rows, _ = diff(same, decode_areas(Image(disc)), lambda at, w: "unread")
    got = {(k, v) for k, v, _ in rows}
    for k in ("area_descriptor/a000/link_last", "area_link/a000/l01/dest_area", "area_link/a000/l01/alt0/area"):
        if (k, "hole") not in got:
            errs.append("selftest: a zeroed link count does not report %s as a hole" % k)
    # issue 139's shape: an alternative left unfilled (a gap)
    disc, w = synthetic()
    w(0x600300, "u16", 0)
    rows, _ = diff(same, decode_areas(Image(disc)), lambda at, w: "gap")
    if [(k, v) for k, v, _ in rows] != [("area_link/a000/l01/alt0/area", "hole")]:
        errs.append("selftest: a zeroed alternative reports %r" % rows)
    # the same zero placed from the disc is the disc's own value
    rows, _ = diff(same, decode_areas(Image(disc)), how_of)
    if [(k, v) for k, v, _ in rows] != [("area_link/a000/l01/alt0/area", "differ")]:
        errs.append("selftest: a placed zero reports %r" % rows)
    # a release difference: a non-zero value changed
    disc, w = synthetic()
    w(0x669B00, "u8", 40)
    rows, _ = diff(same, decode_areas(Image(disc)), how_of)
    if [(k, v) for k, v, _ in rows] != [("music_set/s00/e0/track", "differ")]:
        errs.append("selftest: a changed track reports %r" % rows)
    # runtime-filled bytes are not compared
    rows, c = diff(same, decode_areas(Image(disc)), lambda at, w: "own")
    if rows or c["music_set"]["compared"]:
        errs.append("selftest: own-class bytes were compared")
    # the known list
    rules = [{"id": "t", "kind": "difference", "keys": ["music_set/s00/*"], "reason": "r", "cite": "c"}]
    bad, used = judge([("psx-us", "music_set", "music_set/s00/e0/track", "differ", "map")], rules)
    if bad or used["t"] != 1:
        errs.append("selftest: a listed difference is not explained")
    bad, _ = judge([("psx-us", "music_set", "music_set/s00/e0/track", "hole", "map")], rules)
    if not bad:
        errs.append("selftest: a hole passed under a `difference` entry")
    return errs


def cmd_selftest(a):
    errs = selftest_errors()
    for e in errs:
        print("error: " + e)
    print("data_diff selftest: %d error(s)" % len(errs))
    return 1 if errs else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    p = s.add_parser("run")
    p.add_argument("--exe", required=True, help="the PC's BOF3.exe")
    p.add_argument("--disc", action="append", required=True, help="a disc (.cue / .iso; repeat)")
    p.add_argument("--report", default=REPORT)
    p.add_argument("--known", default=KNOWN)
    p.add_argument("--values", help="also write the differing values here (outside the repository)")
    p.add_argument("--tree", help="import exe_tables, tables and importer from this checkout")
    p = s.add_parser("check")
    p.add_argument("--report", default=REPORT)
    p.add_argument("--known", default=KNOWN)
    s.add_parser("selftest")
    a = ap.parse_args()
    sys.path.insert(0, TOOLS)
    sys.exit({"run": cmd_run, "check": cmd_check, "selftest": cmd_selftest}[a.cmd](a) or 0)


if __name__ == "__main__":
    main()
