#!/usr/bin/env python
"""The unified-data importer: recipes, identity, plan, cache, verify.

docs/importer.md; docs/unified-data-plan.md section 3 (step 2). One recipe
file per target build says, for every chunk of every container, which
held builds carry it byte for byte and how to get it out of each. The
importer resolves every chunk against the player's ordered sources and writes
one cache: `base/` (language-neutral) and `loc/<lang>/`, each a tree of
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
(region_diff.py's, PC against JP) decides its layer. The recipe holds names,
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
import type1        # noqa: E402

RECIPE = os.path.join(ROOT, "recipes", "pc-zh.toml")
TARGET = "pc-zh"
# The order a recipe lists a chunk's disc sources in; the player's source
# order, not this, decides which one is used.
DISC_ORDER = ("psx-jp", "psx-us", "psx-eu-en", "psx-fr", "psx-de", "psp-jp", "psp-eu")
LANG_OF = {"pc-zh": "zh"}
TEXT_CLASSES = ("text", "layout+text")


def sha(b):
    return hashlib.sha256(b).hexdigest()


# ---------------------------------------------------------------- identity

def _fixtures():
    with open(os.path.join(ROOT, "fixtures.toml"), "rb") as f:
        return tomllib.load(f)


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
        return type1.decode(b) if how[0] == "type1" else b


class ExeSource:
    def __init__(self, bid, path):
        self.id, self.path = bid, path


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
        for i, t, dest, b in src.build.sections(key):
            idx[sha(b)].append(("copy", key, i))
            if t == 1 and src.id.startswith("psx-"):
                idx[sha(type1.decode(b))].append(("type1", key, i))
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


def why_pc_only(stem, c, cls):
    """The reason a chunk has no disc source, in a few words."""
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


def cmd_recipes(a):
    pc = open_source(a.dat)
    if pc.id != TARGET:
        raise SystemExit("%s is %s, not the %s DAT tree" % (a.dat, pc.id, TARGET))
    discs = [open_source(p) for p in a.disc]
    discs.sort(key=lambda s: DISC_ORDER.index(s.id) if s.id in DISC_ORDER else 99)
    print("indexing %s" % ", ".join(s.id for s in discs))
    idx = {s.id: disc_index(s) for s in discs}
    cls = classify_rows(a.region)
    names = sorted(_manifest_rows("fixtures/pc-zh.DAT.files.tsv"))
    lines, stats = [], collections.Counter()
    for name in names:
        stem = name[:-4]
        blob, chunks = pc.chunks(name)
        lines.append("\n[[file]]\nname = %s\nchunks = [" % toml_str(name))
        for c in chunks:
            body = blob[c.offset:c.offset + c.size]
            h = sha(body)
            row = cls.get((stem, c.index))
            ck, cs = (row[2], row[3]) if row else (None, None)
            src, primary = [], None
            for s in discs:
                cands = idx[s.id].get(h)
                if cands:
                    loc = best(cands, stem, ck if s.id == "psx-jp" else None, cs)
                    if primary and any(c[1:] == primary for c in cands):
                        loc = next(c for c in cands if c[1:] == primary)  # one place on every disc, where it can be
                    primary = primary or loc[1:]
                    src.append([s.id] + list(loc))
            if src:
                layer = "base"
                k, what = (row[0], row[1]) if row else ("identical", "")
                k = "disc" if k == "identical" else k
                if any(x[1] == "type1" for x in src):
                    k, what = "type1", ""
            else:
                k, what = why_pc_only(stem, c, row)
                layer = "loc/zh" if (k in TEXT_CLASSES or c.kind == 3) else "base"
            stats[(layer, k, "disc" if src else "pc only")] += 1
            fields = "kind = %d, tag = 0x%08X, size = %d, sha256 = %s, layer = %s, class = %s" % (
                c.kind, c.tag, c.size, toml_str(h), toml_str(layer), toml_str(k))
            if what:
                fields += ", what = %s" % toml_str(what)
            if src:
                at = [x for x in src if tuple(x[2:]) == tuple(primary)]
                alt = [x for x in src if tuple(x[2:]) != tuple(primary)]
                fields += ", emi = %s, on = [%s]" % (toml_str("%s#%d" % tuple(primary)),
                                                      ", ".join(toml_str("%s:%s" % (x[0], x[1])) for x in at))
                if alt:
                    fields += ", alt = [%s]" % ", ".join(toml_str("%s:%s:%s#%d" % tuple(x)) for x in alt)
            lines.append("  { %s }," % fields)
        lines.append("]")
    head = [
        "# recipes/pc-zh.toml - GENERATED by `tools/importer.py recipes`; do not edit by hand.",
        "# Every chunk of the PC port's 742 DAT/ containers, in file order: kind, tag, size,",
        "# the sha256 of its payload, the cache layer it belongs to, why, and every held build",
        "# that carries it byte for byte - [build, how, EMI, section] with how `copy` or",
        "# `type1` (tools/type1.py) - and last the PC install itself, [pc-zh, chunk, index].",
        "# Hashes and indices only, no game data (CLAUDE.md rule 1). docs/importer.md.",
        "",
        "[meta]",
        "target = %s" % toml_str(TARGET),
        "generated = %s" % datetime.date.today().isoformat(),
        "builds = [%s]" % ", ".join(toml_str(s.id) for s in [pc] + discs),
        "classes = %s" % toml_str(os.path.basename(a.region)),
    ]
    out = a.out or RECIPE
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(head + lines) + "\n")
    tot = sum(stats.values())
    print("%s: %d files, %d chunks" % (out, len(names), tot))
    for (layer, k, s), n in sorted(stats.items()):
        print("  %-7s %-12s %-8s %5d" % (layer, k, s, n))


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


def chunk_bytes(kind, tag, body):
    return struct.pack("<4I", kind, tag, len(body), 0) + body


def cmd_build(a):
    rec = load_recipe(a.recipe)
    sources = [open_source(p) for p in a.source]
    if len({(s.id, type(s)) for s in sources}) != len(sources):
        raise SystemExit("two sources are the same build")
    print("sources, in order: %s" % ", ".join("%s (%s)" % (s.id, s.path) for s in sources))
    chunked = [s for s in sources if not isinstance(s, ExeSource)]
    assets, missing, used = [], collections.Counter(), collections.Counter()
    written = collections.Counter()
    for f in rec["file"]:
        layers = collections.OrderedDict()
        for slot, ch in enumerate(f["chunks"]):
            got = None
            for s in chunked:          # the player's order decides
                for src in sources_of(ch):
                    if src[0] != s.id:
                        continue
                    body = s.get(f["name"], slot, src[1:])
                    if sha(body) != ch["sha256"]:
                        raise SystemExit("%s chunk %d from %s: hash differs from the recipe" % (f["name"], slot, s.id))
                    got = (s.id, src, body)
                    break
                if got:
                    break
            layers.setdefault(ch["layer"], [])
            if not got:
                missing[(ch["layer"], ch["class"])] += 1
                layers[ch["layer"]].append(None)
                assets.append((f["name"], slot, ch, None))
                continue
            used[(got[0], ch["layer"])] += 1
            layers[ch["layer"]].append(chunk_bytes(ch["kind"], ch["tag"], got[2]))
            assets.append((f["name"], slot, ch, got[1]))
        for layer, parts in layers.items():
            if any(p is None for p in parts):
                continue          # a container is written whole or not at all
            d = os.path.join(a.out, layer, "dat")
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, f["name"]), "wb") as fh:
                fh.write(b"".join(parts))
            written[layer] += 1
    loc_assets = build_languages(a.lang, sources, a.out)
    write_manifest(a.out, a.recipe or RECIPE, rec, sources, assets, loc_assets)
    for (bid, layer), n in sorted(used.items()):
        print("  %-7s from %-7s %5d chunks" % (layer, bid, n))
    for layer, n in sorted(written.items()):
        print("  %-7s %d containers written" % (layer, n))
    if missing:
        print("  missing (no source given carries them):")
        for (layer, k), n in sorted(missing.items()):
            print("    %-7s %-12s %5d" % (layer, k, n))
    print("manifest: %s" % os.path.join(a.out, "manifest.toml"))


# The language a disc can give loc/<lang> (tools/loc_build.py's donors).
LANG_DONORS = {"en": ("psx-us", "psx-eu-en"), "fr": ("psx-fr",), "de": ("psx-de",), "ja": ("psx-jp",)}


def build_languages(langs, sources, out):
    """loc/<lang>/ for each language asked for, built by tools/loc_build.py
    from the first donor disc in the player's order, against the PC's own
    containers and BOF3.exe - exactly the overlays it writes into a game's
    DAT/ today, so the engine reads them unchanged. English goes first: the
    French and German title menus borrow its CONFIG row (loc_build.build_title)."""
    import shutil
    import subprocess
    import tempfile
    pc = next((s for s in sources if isinstance(s, PcSource)), None)
    exe = next((s for s in sources if isinstance(s, ExeSource)), None)
    if langs and not (pc and exe):
        raise SystemExit("a language layer is built against the PC's DAT/ and BOF3.exe: give both as sources")
    assets = []
    langs = sorted(langs, key=lambda l: (l != "en", l))
    with tempfile.TemporaryDirectory(prefix="bof3_loc_") as game:
        os.makedirs(os.path.join(game, "DAT"))
        os.symlink(os.path.abspath(exe.path), os.path.join(game, "BOF3.exe"))
        for name in _manifest_rows("fixtures/pc-zh.DAT.files.tsv"):
            os.symlink(os.path.abspath(os.path.join(pc.path, name)), os.path.join(game, "DAT", name))
        for lang in langs:
            donor = next((s for s in sources if isinstance(s, DiscSource) and s.id in LANG_DONORS.get(lang, ())), None)
            if not donor:
                raise SystemExit("loc/%s: no source carries it (a %s disc)" % (lang, " or ".join(LANG_DONORS.get(lang, ("?",)))))
            r = subprocess.run([sys.executable, os.path.join(TOOLS, "loc_build.py"), "all", "--disc", donor.path,
                                "--game", game, "--lang", lang], capture_output=True, text=True)
            if r.returncode:
                raise SystemExit("loc_build.py --lang %s failed:\n%s%s" % (lang, r.stdout, r.stderr))
            d = os.path.join(out, "loc", lang, "dat")
            os.makedirs(d, exist_ok=True)
            n = 0
            for f in sorted(os.listdir(os.path.join(game, "DAT"))):
                if f.startswith(lang + "."):
                    shutil.copyfile(os.path.join(game, "DAT", f), os.path.join(d, f[len(lang) + 1:]))
                    with open(os.path.join(d, f[len(lang) + 1:]), "rb") as fh:
                        assets.append((f[len(lang) + 1:], "loc/" + lang, "%s:loc_build" % donor.id, sha(fh.read())))
                    n += 1
            print("  loc/%-4s from %-9s %d containers (tools/loc_build.py)" % (lang, donor.id, n))
    return assets


def write_manifest(out, recipe_path, rec, sources, assets, loc_assets=()):
    with open(recipe_path, "rb") as f:
        rsha = sha(f.read())
    lines = ["# The cache's provenance, written by tools/importer.py build. Every chunk: its",
             "# container, slot, layer, the build and recipe step it came from, its hash.",
             "", "[meta]",
             "target = %s" % toml_str(rec["meta"]["target"]),
             "recipe_sha256 = %s" % toml_str(rsha),
             "built = %s" % toml_str(datetime.datetime.now().isoformat(timespec="seconds")),
             ""]
    for s in sources:
        lines += ["[[source]]", "build = %s" % toml_str(s.id), "path = %s" % toml_str(os.path.basename(os.path.normpath(s.path))), ""]
    lines.append("assets = [")
    for name, slot, ch, src in assets:
        where = "" if not src else src[0] + ":" + src[1] + ("" if src[2] is None else ":%s#%d" % (src[2], src[3]))
        lines.append("  [%s, %d, %s, %s, %s]," % (toml_str(name), slot, toml_str(ch["layer"]),
                     toml_str(where), toml_str(ch["sha256"])))
    lines.append("]")
    if loc_assets:
        lines += ["", "# Language layers: whole overlay containers, as tools/loc_build.py writes them.", "overlays = ["]
        for name, layer, where, h in loc_assets:
            lines.append("  [%s, %s, %s, %s]," % (toml_str(name), toml_str(layer), toml_str(where), toml_str(h)))
        lines.append("]")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "manifest.toml"), "w", newline="\n", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


# ---------------------------------------------------------------- verify

def cmd_verify(a):
    """Compose base/ + loc/zh/ back into the PC's containers, in the recipe's
    slot order, and hash each against fixtures/pc-zh.DAT.files.tsv."""
    rec = load_recipe(a.recipe)
    want = _manifest_rows("fixtures/pc-zh.DAT.files.tsv")
    ok, bad, absent = 0, [], []
    for f in rec["file"]:
        parts, readers = [], {}
        for ch in f["chunks"]:
            layer = ch["layer"]
            if layer not in readers:
                p = os.path.join(a.cache, layer, "dat", f["name"])
                readers[layer] = iter(dat.load(p)[1]) if os.path.exists(p) else None
                readers[layer + "#blob"] = open(p, "rb").read() if os.path.exists(p) else None
            it = readers[layer]
            if it is None:
                parts = None
                break
            c = next(it)
            blob = readers[layer + "#blob"]
            parts.append(chunk_bytes(c.kind, c.tag, blob[c.offset:c.offset + c.size]))
        if parts is None:
            absent.append(f["name"])
            continue
        body = b"".join(parts)
        (ok := ok + 1) if (len(body), sha(body)) == want[f["name"]] else bad.append(f["name"])
    print("verify %s against fixtures/pc-zh.DAT.files.tsv: %d of %d containers byte-identical, %d differ, %d incomplete"
          % (a.cache, ok, len(want), len(bad), len(absent)))
    for n in bad[:20]:
        print("   differs:", n)
    rc = 1 if bad else 0
    if a.overlays:
        rc |= verify_overlays(a.cache, a.overlays)
    return rc


def verify_overlays(cache, theirs):
    """Each loc/<lang>/ layer (but zh, which the recipe checks) against the
    <lang>.<NAME>.DAT overlays of an install, byte for byte, both ways."""
    rc = 0
    loc = os.path.join(cache, "loc")
    for lang in sorted(os.listdir(loc)) if os.path.isdir(loc) else []:
        if lang == LANG_OF[TARGET]:
            continue
        d = os.path.join(loc, lang, "dat")
        ours = set(os.listdir(d))
        inst = {f[len(lang) + 1:] for f in os.listdir(theirs) if f.startswith(lang + ".")}
        same = [n for n in sorted(ours & inst)
                if open(os.path.join(d, n), "rb").read() == open(os.path.join(theirs, lang + "." + n), "rb").read()]
        differ = sorted((ours & inst) - set(same))
        print("verify loc/%s against %s/%s.*.DAT: %d identical, %d differ, %d only in the cache, %d only in the install"
              % (lang, theirs, lang, len(same), len(differ), len(ours - inst), len(inst - ours)))
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
    the manifest's size, every source names a build fixtures.toml holds, and
    every layer is base or a language. Needs no game data (CI)."""
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
            if c["layer"] != "base" and not re.fullmatch(r"loc/[a-z]{2}", c["layer"]):
                errs.append("%s: layer %s" % (f["name"], c["layer"]))
            for s in sources_of(c):
                if s[0] not in builds:
                    errs.append("%s: source build %s not in fixtures.toml" % (f["name"], s[0]))
                if s[1] not in ("copy", "type1", "chunk"):
                    errs.append("%s: how %s" % (f["name"], s[1]))
    for e in errs[:30]:
        print("ERROR", e)
    print("importer check: %d files, %d chunks, %d error(s)" % (len(names), n, len(errs)))
    return 1 if errs else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    p = s.add_parser("recipes")
    p.add_argument("--dat", required=True, help="the PC port's DAT/ directory")
    p.add_argument("--disc", action="append", default=[], help="a held disc, any order; repeat")
    p.add_argument("--region", default=os.path.join(ROOT, "analysis", "region", "psx-jp_vs_pc-zh.json"),
                   help="region_diff.py pc's output, for the class of chunks no disc carries")
    p.add_argument("--out")
    p = s.add_parser("identify")
    p.add_argument("path", nargs="+")
    p = s.add_parser("build")
    p.add_argument("--source", action="append", required=True, help="a PC DAT/, BOF3.exe or a disc; order is preference")
    p.add_argument("--out", required=True)
    p.add_argument("--lang", action="append", default=[], choices=sorted(LANG_DONORS),
                   help="a language layer to build (repeat); needs the PC's DAT/, BOF3.exe and that language's disc")
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
