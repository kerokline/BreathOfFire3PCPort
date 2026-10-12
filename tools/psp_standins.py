#!/usr/bin/env python
"""A PSP-only install's base: the PSP's own sections standing in.

docs/psp-stand-ins.md; docs/psp-only-build.md 2.2 and 7 (the owner's call 1,
2026-10-10: a PSP-only install uses the PSP's art and data as its base).

`recipes/pc-zh.toml` names, for every chunk of the PC's 742 containers, each
held build that carries it byte for byte. About 270 `base/` chunks have no PSP
source: the PSP's own content changes (`disc`, `type1`: P8, `pc-byte`: RYUD),
the port's art (`art`: the dial page, `pc-icons`: FIRST's icons respaced) and
the port-edited arenas (`pc-edit`, `pc-hold`). With **only PSP discs** among
the sources (the base policy `psp`), each of those is filled from the PSP's
own section at the same place:

    why          what                                                   how
    psp-base     the PSP's own section where the PSX's differs          copy (or widen / ryud, as the
                 (its art, palettes, map bands, UI page, P8)            PC's own rule on the PSX's)
    language     PSP-EU's own language where the PC kept Japan's        copy
    port-art     the PSP's dial page, as a PSX disc's stands in         copy
    arena        the PSP's arena as it ships (decompressed), where      copy
                 the port edited its animation scripts
    start-arena  START as the PSP's PL012 arena (the port's choice)     copy
    hold         the 27A rule (arena-edits.md 4) on the PSP's arena     hold
    icons        the respace-icons rule on the PSP's FIRST page         icons

With the PC's DAT/ or any PSX disc among the sources (the policy `psx`), none
of this applies and the build is the one it was.

    python tools/psp_standins.py recipes --disc JP --disc PSPJP --disc PSPEU [--out recipes/psp-standins.toml]
    python tools/psp_standins.py check

`recipes` aligns each PSP EMI with the JP PSX disc's (region_diff.align) to
find the PSP's section at the place the recipe names on JP, applies the how,
and records the output's sha256 - names, indices and hashes only, never bytes
(CLAUDE.md rule 1). The build hashes every stand-in against it before writing.
`check` (CI, via importer.py check) needs no game data.
"""
import argparse
import collections
import datetime
import hashlib
import json
import os
import sys
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)

RECIPE = os.path.join(ROOT, "recipes", "psp-standins.toml")
PSP = ("psp-jp", "psp-eu")
WHY = {
    "psp-base": "the PSP's own section where the PlayStation's differs: its content change is a PSP-only install's base",
    "language": "PSP-EU's own language in a section the PC kept as Japan's",
    "port-art": "the PSP's own dial page, its buttons where the port drew its keyboard legend (as a PSX disc's stands in)",
    "arena": "the PSP's own arena as it ships it, decompressed: the port's script edits absent, the PSP's own change in",
    "start-arena": "the PSP's PL012 arena, as the PC gives START (the port's choice)",
    "hold": "the PSP's own arena with the port's 27A hold applied (arena-edits.md 4)",
    "icons": "the PSP's own FIRST page with the port's respace-icons rule applied",
}
# The classes of pc-zh.toml a PSP-only base fills; `bank` is not among them
# (the 16 containers' dropped tone: MB's, psp-only-build.md 2.1).
CLASSES = ("disc", "type1", "pc-byte", "pc-edit", "pc-hold", "pc-icons", "art")
HOWS = ("copy", "widen", "ryud", "hold", "icons")
POLICY_KEY = "base_policy"


def sha(b):
    return hashlib.sha256(b).hexdigest()


def _importer():
    import importer
    return importer


def transform(how, b):
    imp = _importer()
    return {"copy": lambda s: s, "widen": imp.widen_enemies, "ryud": imp.ryud_byte,
            "hold": imp.hold_arena, "icons": imp.respace_icons}[how](b)


# ---------------------------------------------------------------- policy

def policy(sources):
    """`psp` when every source that gives base/ chunks is a PSP disc (the PC's
    BOF3.exe gives none), else `psx`: the PlayStation's base, as before."""
    data = [s for s in sources if type(s).__name__ != "ExeSource"]
    return "psp" if data and all(s.id in PSP for s in data) else "psx"


# ---------------------------------------------------------------- the generator

def jp_place(ch, sources_of, owns_of):
    """(EMI key, section, how) of the chunk on the JP PSX disc, from the
    recipe: its exact source there, else JP's own stand-in."""
    for b, how, key, sec in sources_of(ch):
        if b == "psx-jp":
            return key, sec, how
    for b, key, sec, h in owns_of(ch):
        if b == "psx-jp":
            return key, sec, "own"
    return None


def psp_how(ch, jp_how):
    """The transform the PSP's section takes for this chunk: the PC's own rule
    where the PSX's section takes one, else the section as it is."""
    k = ch["class"]
    if k == "pc-icons":
        return "icons"
    if k == "pc-hold":
        return "hold"
    if k == "pc-byte":
        return "ryud"
    if jp_how == "widen":
        return "widen"
    return "copy"


def cmd_recipes(a):
    imp = _importer()
    import region_diff
    rec = imp.load_recipe(a.recipe)
    discs = {s.id: s for s in (imp.open_source(p) for p in a.disc)}
    if "psx-jp" not in discs or not set(PSP) & set(discs):
        raise SystemExit("recipes wants the JP PSX disc (to place each section) and a PSP disc")
    jp = discs["psx-jp"]
    psps = [discs[b] for b in PSP if b in discs]
    aligned = {}

    def psp_section(s, key, sec):
        if (s.id, key) not in aligned:
            if key not in s.build.emis:
                aligned[(s.id, key)] = None
            else:
                ja, pa = jp.build.sections(key), s.build.sections(key)
                aligned[(s.id, key)] = ({i: j for i, j in region_diff.align(ja, pa) if i is not None and j is not None},
                                        pa)
        got = aligned[(s.id, key)]
        if got is None or sec not in got[0]:
            return None
        return key, got[1][got[0][sec]]

    jp_by_hash = None

    def twin(s, key, sec, tag):
        """A JP section the alignment leaves unpaired (FIRST's 64 KB page: the
        PSP's FIRST has another in its place): the same page wherever else it
        sits on the JP disc, paired there; taken only if every such place
        gives the PSP one and the same section, one at the chunk's own
        rectangle first."""
        nonlocal jp_by_hash
        if jp_by_hash is None:
            jp_by_hash = collections.defaultdict(list)
            for k in sorted(jp.build.emis):
                for i, t, dest, b in jp.build.sections(k):
                    jp_by_hash[sha(b)].append((k, i))
        want = sha(jp.build.sections(key)[sec][3])
        got = [x for x in (psp_section(s, k, i) for k, i in jp_by_hash[want] if (k, i) != (key, sec)) if x]
        if not got or len({sha(x[1][3]) for x in got}) != 1:
            return None
        return min(got, key=lambda x: x[1][2] >> 8 != tag >> 8)

    rows, stats, notes, info = [], collections.Counter(), [], []
    for f in rec["file"]:
        stem = f["name"][:-4]
        for slot, ch in enumerate(f["chunks"]):
            if ch["layer"] != "base" or ch["class"] not in CLASSES:
                continue
            carried = {b for b, *_ in imp.sources_of(ch)}
            lack = [s for s in psps if s.id not in carried]
            if not lack:
                continue
            place = jp_place(ch, imp.sources_of, imp.owns_of)
            if not place:
                notes.append("%s slot %d (%s): no place on the JP disc" % (f["name"], slot, ch["class"]))
                continue
            key, sec, jp_how = place
            on = []
            for s in lack:
                got = psp_section(s, key, sec) or twin(s, key, sec, ch["tag"])
                if got is None:
                    notes.append("%s slot %d (%s): %s has no section paired with %s#%d"
                                 % (f["name"], slot, ch["class"], s.id, key, sec))
                    continue
                pkey, (j, t, dest, b) = got
                if pkey != key:
                    info.append("%s slot %d (%s): %s's page from %s#%d, where the JP disc's same page is paired"
                                 % (f["name"], slot, ch["class"], s.id, pkey, j))
                # a page's tag is its tile rectangle; the loader leaves the low
                # byte unread, and the PSP's pages set it (asset-loading-path.md 2)
                if ch["kind"] == 1 and (t != 3 or dest >> 8 != ch["tag"] >> 8):
                    notes.append("%s slot %d: %s's %s#%d is type %d at %08X, not the page" % (f["name"], slot, s.id, pkey, j, t, dest))
                    continue
                if ch["class"] in ("type1", "pc-edit", "pc-hold") and t != 1:
                    notes.append("%s slot %d: %s's %s#%d is type %d, not an arena" % (f["name"], slot, s.id, pkey, j, t))
                    continue
                how = psp_how(ch, jp_how)
                out = transform(how, b)
                if out is None and how == "ryud":
                    how, out = "copy", b       # the PSP's byte is not the PSX's: its own section as it is
                if out is None:
                    notes.append("%s slot %d: %s's section is not the shape `%s` takes" % (f["name"], slot, s.id, how))
                    continue
                k = ch["class"]
                if k == "art":
                    why = "port-art"
                elif k == "pc-edit":
                    why = "start-arena" if stem == "START" else "arena"
                elif k in ("pc-hold", "pc-icons"):
                    why = "hold" if k == "pc-hold" else "icons"
                elif k == "disc" and any(o.id in carried for o in psps if o is not s):
                    why = "language"
                else:
                    why = "psp-base"
                h = sha(out)
                exact = h == ch["sha256"]
                stats[(s.id, k, why, how, "exact" if exact else "own")] += 1
                on.append(("%s:%s:%s#%d:%s" % (s.id, how, pkey, j, h), s.id, why))
            if on:
                rows.append((f["name"], slot, ch["class"], on))
    head = [
        "# recipes/psp-standins.toml - GENERATED by `tools/psp_standins.py recipes`; do not edit by hand.",
        "# A PSP-only install's base (docs/psp-stand-ins.md): for each base/ chunk of",
        "# recipes/pc-zh.toml that a PSP disc does not carry byte for byte, that disc's own",
        "# section at the JP disc's place, as `build:how:EMI#section:sha256` of the output.",
        "# Used only when every source is a PSP disc (base_policy = \"psp\").",
        "# Hashes and indices only, no game data (CLAUDE.md rule 1).",
        "",
        "[meta]",
        "generated = %s" % datetime.date.today().isoformat(),
        "builds = [%s]" % ", ".join(json.dumps(s.id) for s in psps),
        "",
    ]
    lines = []
    for name, slot, k, on in rows:
        lines.append("[[chunk]]\nfile = %s\nslot = %d\nclass = %s\non = [%s]\nwhy = { %s }\n"
                     % (json.dumps(name), slot, json.dumps(k), ", ".join(json.dumps(x[0]) for x in on),
                        ", ".join("%s = %s" % (b, json.dumps(w)) for _, b, w in on)))
    with open(a.out or RECIPE, "w", newline="\n", encoding="utf-8") as fh:
        fh.write("\n".join(head) + "\n" + "\n".join(lines))
    print("%s: %d chunks" % (a.out or RECIPE, len(rows)))
    for key, n in sorted(stats.items()):
        print("  %-7s %-9s %-12s %-6s %-5s %4d" % (key + (n,)))
    for n in info:
        print("  INFO", n)
    for n in notes:
        print("  NOTE", n)
    return 1 if notes and a.strict else 0


# ---------------------------------------------------------------- build

def load(path=None):
    p = path or RECIPE
    if not os.path.exists(p):
        return {}
    with open(p, "rb") as fh:
        r = tomllib.load(fh)
    return {(c["file"], c["slot"]): c for c in r.get("chunk", [])}


def parse_on(x):
    b, how, rest = x.split(":", 2)
    at, h = rest.rsplit(":", 1)
    key, sec = at.rsplit("#", 1)
    return b, how, key, int(sec), h


_table = None


def stand_in(f, slot, ch, chunked):
    """For importer.get_chunk, when no source carries the chunk: under the psp
    policy, the PSP's own section from the first PSP source in the player's
    order that the recipe lists, hashed; else None. -> (build, (how, EMI,
    section), payload, None), how "own" unless the output is the PC's chunk."""
    global _table
    if policy(chunked) != "psp":
        return None
    if _table is None:
        _table = load()
    row = _table.get((f["name"], slot))
    if not row or row["class"] != ch["class"]:
        return None
    for s in chunked:
        for b, how, key, sec, h in map(parse_on, row["on"]):
            if b != s.id:
                continue
            body = transform(how, s.sections(key)[sec][3])
            if body is None or sha(body) != h:
                raise SystemExit("%s chunk %d from %s's own section: hash differs from recipes/psp-standins.toml"
                                 % (f["name"], slot, b))
            return s.id, (how if h == ch["sha256"] else "own", key, sec), body, None
    return None


def manifest_lines(sources):
    """The manifest's [build] table (the contract with the launcher, LT):
    `base_policy`, and under `psp` the stand-in recipe's hash."""
    p = policy(sources)
    out = ["[build]", "%s = %s" % (POLICY_KEY, json.dumps(p))]
    if p == "psp":
        with open(RECIPE, "rb") as fh:
            out.append("standins_sha256 = %s" % json.dumps(sha(fh.read())))
    return out + [""]


# ---------------------------------------------------------------- check (CI)

def check(path=None, recipe=None):
    """The stand-in recipe against pc-zh.toml, no game data: every row names a
    base chunk of a class the policy fills, which the row's build does not
    carry; every how and build known; every hash a sha256; one row per chunk."""
    imp = _importer()
    errs = []
    p = path or RECIPE
    if not os.path.exists(p):
        return ["%s missing" % os.path.relpath(p, ROOT)]
    with open(p, "rb") as fh:
        rows = tomllib.load(fh).get("chunk", [])
    rec = {f["name"]: f["chunks"] for f in imp.load_recipe(recipe)["file"]}
    seen = set()
    for r in rows:
        at = (r.get("file"), r.get("slot"))
        if at in seen:
            errs.append("%s slot %s: two rows" % at)
        seen.add(at)
        chunks = rec.get(r.get("file"))
        if not chunks or not isinstance(r.get("slot"), int) or not 0 <= r["slot"] < len(chunks):
            errs.append("%s slot %s: not a chunk of pc-zh.toml" % at)
            continue
        ch = chunks[r["slot"]]
        if ch["layer"] != "base" or ch["class"] not in CLASSES or ch["class"] != r.get("class"):
            errs.append("%s slot %d: class %s, the recipe's %s/%s" % (at + (r.get("class"), ch["layer"], ch["class"])))
        carried = {b for b, *_ in imp.sources_of(ch)}
        builds = []
        for x in r.get("on", []):
            try:
                b, how, key, sec, h = parse_on(x)
            except ValueError:
                errs.append("%s slot %d: %r" % (at + (x,)))
                continue
            builds.append(b)
            if b not in PSP or how not in HOWS or len(h) != 64 or b in carried:
                errs.append("%s slot %d: stand-in %s" % (at + (x,)))
        if not builds or len(set(builds)) != len(builds):
            errs.append("%s slot %d: builds %s" % (at + (builds,)))
        why = r.get("why", {})
        if sorted(why) != sorted(builds) or any(w not in WHY for w in why.values()):
            errs.append("%s slot %d: why %s" % (at + (why,)))
    return errs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("recipes")
    r.add_argument("--disc", action="append", required=True)
    r.add_argument("--recipe")
    r.add_argument("--out")
    r.add_argument("--strict", action="store_true")
    sub.add_parser("check")
    a = ap.parse_args()
    if a.cmd == "recipes":
        return cmd_recipes(a)
    errs = check()
    for e in errs[:30]:
        print("ERROR", e)
    print("psp_standins check: %d error(s)" % len(errs))
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
