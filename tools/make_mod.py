#!/usr/bin/env python
"""Build a mod's zip from the repository's data: the furigana readings as the project's first mod.

    python tools/make_mod.py furigana [--src data/furigana] [--out build/release/furigana-VERSION.zip]
    python tools/make_mod.py furigana --version-of        the version, for release.yml's file name
    python tools/make_mod.py --selftest

docs/mods.md. The zip holds `mod.toml`, written here from MODS, and the data
files it names, taken from the source folder - **nothing else**: never a text
layer (derived game data, CLAUDE.md rule 1), never code. The result is checked
by tools/mods.py's own reader before it is kept, so a zip this script makes is
one `mods.py add` takes. The zip is deterministic (fixed member times and
order): the same data makes the same bytes.

The furigana mod's data, `data/furigana/readings.toml`, is the reading-row
round's group FB's (docs/reading-row-round.md section 2 FB): the readings
chosen by hand for words the dictionary reads wrong, ours, no Capcom bytes.
Until it exists this script refuses with the folder's name; release.yml builds
the zip only once the folder is there.
"""
import argparse
import json
import os
import shutil
import sys
import tempfile

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import mods   # noqa: E402

# The mods this repository makes. A version moves when the data does: a
# player's Mods tab replaces an installed mod of the same id with a newer zip.
MODS = {
    "furigana": {
        "src": os.path.join(ROOT, "data", "furigana"),
        "mod": {"id": "furigana", "name": "Japanese (Furigana)", "version": "0.1.0", "kind": "reading-row",
                "requires": ["ja-JP"]},
        "table": {"base": "ja-JP", "readings": "readings.toml"},
    },
}


def mod_toml(spec):
    m, t = spec["mod"], spec["table"]
    kind = mods.KINDS[m["kind"]]
    q = lambda v: json.dumps(v, ensure_ascii=False)
    lines = ["# Written by tools/make_mod.py (docs/mods.md).", "[mod]"]
    lines += ["%s = %s" % (k, q(m[k])) for k in ("id", "name", "version", "kind", "requires")]
    lines += ["", "[%s]" % kind["table"]] + ["%s = %s" % (k, q(t[k])) for k in kind["keys"]]
    return ("\n".join(lines) + "\n").encode("utf-8")


def build(name, src=None, out=None):
    """The zip for MODS[name]; its path. SystemExit when the data is missing or
    the result is not a mod mods.py takes."""
    if name not in MODS:
        raise SystemExit("make_mod: no mod %r (there are: %s)" % (name, ", ".join(MODS)))
    spec = MODS[name]
    src = os.path.abspath(src or spec["src"])
    kind = mods.KINDS[spec["mod"]["kind"]]
    files = [spec["table"][k] for k, how in kind["keys"].items() if how == "file"]
    missing = [f for f in files if not os.path.isfile(os.path.join(src, f))]
    if missing:
        raise SystemExit("make_mod %s: %s is not in %s - the mod's data is not there yet (for furigana, the "
                         "reading-row round's group FB writes data/furigana/)" % (name, ", ".join(missing), src))
    out = os.path.abspath(out or os.path.join(ROOT, "build", "release", "%s-%s.zip" % (name, spec["mod"]["version"])))
    members = [("mod.toml", mod_toml(spec))]
    for f in files:
        with open(os.path.join(src, f), "rb") as fh:
            members.append((f, fh.read()))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    part = out + ".part.zip"
    mods.make_zip(part, members)
    try:
        meta, _ = mods.read_zip(part)
    except mods.Refused as e:
        os.remove(part)
        raise SystemExit("make_mod %s: the zip made is not one mods.py takes: %s" % (name, e))
    os.replace(part, out)
    print("%s: %s %s (%s), %d bytes: %s" % (out, meta["id"], meta["version"], meta["kind"], os.path.getsize(out),
                                           ", ".join(m[0] for m in members)))
    return out


def selftest():
    """Against a temporary data folder (never data/furigana/): the zip made,
    taken by mods.py, deterministic; a folder without the data refused."""
    fails = []

    def expect(cond, what):
        print("%-70s %s" % (what, "ok" if cond else "FAIL"))
        if not cond:
            fails.append(what)

    d = tempfile.mkdtemp(prefix="bof3x_make_mod_")
    try:
        src = os.path.join(d, "furigana")
        os.makedirs(src)
        with open(os.path.join(src, "readings.toml"), "wb") as f:
            f.write(mods.GOOD_READINGS)
        with open(os.path.join(src, "NOTES.md"), "w") as f:      # not named: left out of the zip
            f.write("notes\n")
        a = build("furigana", src, os.path.join(d, "a", "furigana.zip"))
        b = build("furigana", src, os.path.join(d, "b", "furigana.zip"))
        with open(a, "rb") as fa, open(b, "rb") as fb:
            expect(fa.read() == fb.read(), "the same data: the same zip bytes")
        meta, data = mods.read_zip(a)
        expect(sorted(data) == ["mod.toml", "readings.toml"], "the zip holds mod.toml and readings.toml only")
        expect(meta["layer"] == "ruby-furigana-ja-JP" and meta["requires"] == ["ja-JP"],
               "mods.py reads it: layer ruby-furigana-ja-JP, requires ja-JP")
        try:
            build("furigana", os.path.join(d, "empty"), os.path.join(d, "c.zip"))
            expect(False, "a folder without readings.toml: refused")
        except SystemExit as e:
            expect("readings.toml is not in" in str(e), "a folder without readings.toml: refused (%s...)" % str(e)[:30])
    finally:
        shutil.rmtree(d, ignore_errors=True)
    print("make_mod selftest: %d failure(s)" % len(fails))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("name", nargs="?", help="the mod: %s" % ", ".join(MODS))
    ap.add_argument("--src", help="the data folder (default: the mod's own, e.g. data/furigana)")
    ap.add_argument("--out", help="the zip (default build/release/<name>-<version>.zip)")
    ap.add_argument("--version-of", action="store_true", help="print the mod's version and stop (release.yml)")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if not a.name:
        ap.error("name a mod, or --selftest")
    if a.version_of:
        if a.name not in MODS:
            raise SystemExit("make_mod: no mod %r" % a.name)
        print(MODS[a.name]["mod"]["version"])
        return 0
    build(a.name, a.src, a.out)
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
