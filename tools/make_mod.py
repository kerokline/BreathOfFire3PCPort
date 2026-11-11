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
        "mod": {"id": "furigana", "name": "Japanese (Furigana)", "version": "0.1.1", "author": "Kevin Kline",
                "kind": "reading-row",
                "requires": ["ja-JP"]},
        "table": {"base": "ja-JP", "readings": "readings.toml"},
    },
}


def mod_toml(spec):
    m, t = spec["mod"], spec["table"]
    kind = mods.KINDS[m["kind"]]
    q = lambda v: json.dumps(v, ensure_ascii=False)
    lines = ["# Written by tools/make_mod.py (docs/mods.md).", "[mod]"]
    lines += ["%s = %s" % (k, q(m[k])) for k in ("id", "name", "version", "author", "url", "credit", "kind", "requires")
              if k in m]
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


def table_edit(src, dst, out, mod_id, name, version, author=None, url=None, credit=None):
    """A table-edit mod (docs/image-layer-round.md 4) from two folders of
    `tables.py export` CSVs: `src` as exported, `dst` the same files edited.
    Every number that differs becomes one [[set]] row; the zip holds mod.toml
    and edits.toml - the author's numbers only, no table. A changed `name`
    column is reported and left out (a name is the language's text). Returns
    (the zip, rows written, names ignored)."""
    import csv
    import table_edit_build
    import tables
    cat, _, _ = tables.load()
    by_key = {t["key"]: t for t in cat["table"]}
    rows, ignored = [], 0
    for fname in sorted(os.listdir(dst)):
        key = fname[:-4]
        if not fname.endswith(".csv") or key not in by_key:
            continue
        a_path = os.path.join(src, fname)
        if not os.path.isfile(a_path):
            raise SystemExit("make_mod table-edit: %s is in %s and not in %s" % (fname, dst, src))
        with open(a_path, newline="", encoding="utf-8") as f:
            a = list(csv.DictReader(f))
        with open(os.path.join(dst, fname), newline="", encoding="utf-8") as f:
            b = list(csv.DictReader(f))
        if len(a) != len(b) or [r["id"] for r in a] != [r["id"] for r in b]:
            raise SystemExit("make_mod table-edit: %s has other records than the export (a mod changes numbers; "
                             "it does not add, remove or move a record)" % fname)
        cols = table_edit_build.columns(by_key[key])
        for ra, rb in zip(a, b):
            for col in rb:
                if ra.get(col) == rb[col] or col == "id":
                    continue
                if col == "name":
                    ignored += 1
                    continue
                if col not in cols:
                    raise SystemExit("make_mod table-edit: %s has a column %s the catalogue does not" % (fname, col))
                try:
                    value = int(rb[col], 0)
                except ValueError:
                    raise SystemExit("make_mod table-edit: %s record %s, %s: %r is not a whole number"
                                     % (fname, rb["id"], col, rb[col]))
                rows.append((key, int(rb["id"]), col, value))
    if not rows:
        raise SystemExit("make_mod table-edit: no number differs between %s and %s%s"
                         % (src, dst, " (%d changed names were left out: a rename is a text mod)" % ignored if ignored else ""))
    q = lambda v: json.dumps(v, ensure_ascii=False)
    edits = ["# Written by tools/make_mod.py table-edit (docs/image-layer-round.md 4): the numbers this mod",
             "# sets, by table, record and field. It holds no table of the game's.", ""]
    for key, rec, col, value in rows:
        edits += ["[[set]]", "table = %s" % q(key), "record = %d" % rec, "field = %s" % q(col), "value = %d" % value, ""]
    meta = {"id": mod_id, "name": name, "version": version, "kind": "table-edit", "requires": []}
    for k, v in (("author", author), ("url", url), ("credit", credit)):
        if v:
            meta[k] = v
    toml = mod_toml({"mod": meta, "table": {"edits": "edits.toml"}})
    out = os.path.abspath(out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    part = out + ".part.zip"
    mods.make_zip(part, [("mod.toml", toml), ("edits.toml", "\n".join(edits).encode("utf-8"))])
    try:
        got, _ = mods.read_zip(part)
    except mods.Refused as e:
        os.remove(part)
        raise SystemExit("make_mod table-edit: the zip made is not one mods.py takes: %s" % e)
    os.replace(part, out)
    print("%s: %s %s (table-edit), %d number(s) in %d table(s)%s" % (
        out, got["id"], got["version"], len(rows), len({r[0] for r in rows}),
        "; %d changed name(s) left out - a rename is a text mod" % ignored if ignored else ""))
    return out, len(rows), ignored


def selftest_table_edit(expect, d):
    """Two CSV folders made from the catalogue's columns alone (zeros: no game
    data), one number and one name changed."""
    import csv
    import table_edit_build
    import tables
    cat, _, _ = tables.load()
    t = next(x for x in cat["table"] if x["key"] == "weapons")
    cols = table_edit_build.columns(t)
    plain = next(c for c, (at, typ, bits) in cols.items() if bits is None)
    a, b = os.path.join(d, "csv_a"), os.path.join(d, "csv_b")
    for folder, edit in ((a, False), (b, True)):
        os.makedirs(folder)
        with open(os.path.join(folder, "weapons.csv"), "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=["id", "name"] + list(cols))
            w.writeheader()
            for i in range(t["count"]):
                row = dict({"id": i, "name": "row %d" % i}, **{c: 0 for c in cols})
                if edit and i == 5:
                    row[plain], row["name"] = 7, "renamed"
                w.writerow(row)
    out, n, ignored = table_edit(a, b, os.path.join(d, "te", "sharper.zip"), "sharper", "Sharper", "1.0", "A. Tester")
    meta, data = mods.read_zip(out)
    expect(n == 1 and ignored == 1 and meta["layer"] == "mod-sharper" and sorted(data) == ["edits.toml", "mod.toml"],
           "table-edit: one number in the zip, the changed name left out, mods.py reads it")
    import tomllib
    row = tomllib.loads(data["edits.toml"].decode())["set"]
    expect(row == [{"table": "weapons", "record": 5, "field": plain, "value": 7}], "  edits.toml: the one [[set]] row")
    try:
        table_edit(a, a, os.path.join(d, "te", "none.zip"), "none", "None", "1.0")
        expect(False, "table-edit: two equal folders refused")
    except SystemExit as e:
        expect("no number differs" in str(e), "table-edit: two equal folders refused")


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
        selftest_table_edit(expect, d)
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
    te = ap.add_argument_group("table-edit", "make_mod.py table-edit --from DIR --to DIR --id ID --title NAME --out ZIP: "
                               "a mod of the numbers that differ between two `tables.py export` folders")
    te.add_argument("--from", dest="src_csv", help="the CSVs as exported")
    te.add_argument("--to", dest="dst_csv", help="the same CSVs, edited")
    te.add_argument("--id", help="the mod's id (lowercase letters, digits, hyphens; 19 characters at most)")
    te.add_argument("--title", help="the mod's name, as the Mods tab shows it")
    te.add_argument("--mod-version", default="1.0")
    te.add_argument("--author")
    te.add_argument("--url")
    te.add_argument("--credit")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if a.name == "table-edit":
        need = [k for k, v in (("--from", a.src_csv), ("--to", a.dst_csv), ("--id", a.id), ("--title", a.title),
                               ("--out", a.out)) if not v]
        if need:
            ap.error("table-edit needs %s" % ", ".join(need))
        table_edit(a.src_csv, a.dst_csv, a.out, a.id, a.title, a.mod_version, a.author, a.url, a.credit)
        return 0
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
