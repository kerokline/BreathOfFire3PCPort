#!/usr/bin/env python
"""A `table-edit` mod's step: text cell edits built into a layer of table files.

docs/image-layer-round.md 4, docs/mods.md. A table-edit mod holds one data
file, `edits.toml`: `[[set]]` rows, each one number of one record of one
tables.toml table.

    [[set]]
    table = "weapons"      # a tables.toml key
    record = 12            # the record's index, from 0 (the CSV's `id` column)
    field = "attack"       # a column of `tables.py export`'s CSV for that table
    value = 40

    [[set]]                # a byte no field names yet: its offset in the record and its type
    table = "weapons"
    record = 12
    at = 0x11
    type = "u8"
    value = 3

The file holds only the mod author's own numbers: no table, no record of the
game's (CLAUDE.md rule 1). The step reads the install's own table file
(<cache>/base/exe/tables/<key>.bin, tools/exe_tables.py), writes the values
into a copy of it, and emits the layer the exe host lays
(src/hook/exe_image.cpp LayLayerTables):

    <cache>/opt/<layer>/exe/tables.toml    the tables, and `cells`: exactly the edited bytes
    <cache>/opt/<layer>/exe/<key>.bin      the whole table, the edits in it

Only the cells are laid, so two mods that edit different cells of one table do
not disturb each other; where both write one byte the later mod wins.

    python tools/table_edit_build.py check EDITS.toml          the rows alone, against the catalogue
    python tools/table_edit_build.py build --cache C --layer mod-x --edits EDITS.toml --where mod:x@1.0
    python tools/table_edit_build.py selftest                  refusals and a build on a synthetic table

mods.py runs parse() when a zip is added and build() in the build step.
"""
import argparse
import hashlib
import json
import os
import shutil
import struct
import sys
import tempfile
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import tables   # noqa: E402

ROWS_MAX = 4096
ROW_KEYS = {"table", "record", "field", "at", "type", "value"}


class Bad(Exception):
    """An edits file the step does not take; one sentence for the mod's author or the player."""


def sha(b):
    return hashlib.sha256(b).hexdigest()


def columns(t):
    """{column: (offset in the PC record, type, bits or None)} - the columns
    `tables.py export` writes for table `t`, by the rule tables.records has:
    a field's name, or name_<n> for the n-th copy of a repeated field."""
    out = {}
    for fld in t.get("field", []):
        _, width = tables.TYPES[fld["type"]]
        n_rep, step = fld.get("repeat", [1, width])
        for r in range(n_rep):
            out[fld["name"] if n_rep == 1 else "%s_%d" % (fld["name"], r)] = (
                fld["at"] + r * step, fld["type"], tuple(fld["bits"]) if "bits" in fld else None)
    return out


def _int(v):
    return isinstance(v, int) and not isinstance(v, bool)


def parse(doc, cat=None):
    """The edits of an edits.toml document, checked against the catalogue:
    [{table, record, at, type, bits, value, what}] in the file's order, or Bad."""
    if cat is None:
        cat, _, _ = tables.load()
    by_key = {t["key"]: t for t in cat["table"]}
    extra = sorted(set(doc) - {"set"})
    if extra:
        raise Bad("%s is not something an edits file has; it holds [[set]] rows only" % extra[0])
    rows = doc.get("set")
    if not isinstance(rows, list) or not rows:
        raise Bad("no [[set]] row: an edits file names at least one number to change")
    if len(rows) > ROWS_MAX:
        raise Bad("%d [[set]] rows, over the %d an edits file may hold" % (len(rows), ROWS_MAX))
    out, taken = [], {}
    for n, row in enumerate(rows, 1):
        where = "[[set]] row %d" % n
        if not isinstance(row, dict):
            raise Bad("%s is not a table of keys" % where)
        extra = sorted(set(row) - ROW_KEYS)
        if extra:
            raise Bad("%s has %s, which a row does not have (table, record, field or at and type, value)"
                      % (where, extra[0]))
        key = row.get("table")
        if not isinstance(key, str) or key not in by_key:
            raise Bad("%s: table %s is not a table of the catalogue (tables.py list names them)"
                      % (where, json.dumps(key)))
        t = by_key[key]
        if "symbol" not in t:
            raise Bad("%s: table %s is not in the PC's data (%s)" % (where, key, t.get("pc_twin", "no PC table")))
        if "in_image" in t:
            raise Bad("%s: table %s is not a table a mod can change: %s" % (where, key, t["in_image"]))
        rec = row.get("record")
        if not _int(rec) or not (0 <= rec < t["count"]):
            raise Bad("%s: record %s is not a record of %s (0 to %d)" % (where, json.dumps(rec), key, t["count"] - 1))
        name = t.get("name")
        if "field" in row:
            if "at" in row or "type" in row:
                raise Bad("%s names a field and an offset; give `field`, or `at` and `type`" % where)
            f = row["field"]
            if f == "name":
                raise Bad("%s: a name is not a table's number - it is the language's text, which a text mod "
                          "changes; the name column of the CSV is a title for the row only" % where)
            if f == "id":
                raise Bad("%s: `id` is the record's index, not a field" % where)
            cols = columns(t)
            if not isinstance(f, str) or f not in cols:
                raise Bad("%s: %s has no field %s (its fields: %s; a byte with no name is `at` and `type`)"
                          % (where, key, json.dumps(f), ", ".join(cols) or "none named yet"))
            at, typ, bits = cols[f]
            what = "%s[%d].%s" % (key, rec, f)
        else:
            at, typ, bits = row.get("at"), row.get("type"), None
            if not _int(at) or typ not in tables.TYPES:
                raise Bad("%s: give `field`, or `at` (an offset in the record) and `type` (%s)"
                          % (where, ", ".join(tables.TYPES)))
            what = "%s[%d]+0x%X" % (key, rec, at)
        code, width = tables.TYPES[typ]
        if not (0 <= at and at + width <= t["stride"]):
            raise Bad("%s: offset 0x%X with type %s runs past the record of %s (%d bytes)"
                      % (where, at, typ, key, t["stride"]))
        if name is not None and at < name["at"] + tables.PC_NAME_LEN and name["at"] < at + width:
            raise Bad("%s: offset 0x%X is in the name field of %s; a name is the language's text, which a "
                      "text mod changes" % (where, at, key))
        v = row.get("value")
        if not _int(v):
            raise Bad("%s: value %s is not a whole number" % (where, json.dumps(v)))
        if bits is not None:
            lo, hi = 0, (1 << (bits[1] - bits[0] + 1)) - 1
        elif typ[0] == "s":
            lo, hi = -(1 << (8 * width - 1)), (1 << (8 * width - 1)) - 1
        else:
            lo, hi = 0, (1 << (8 * width)) - 1
        if not (lo <= v <= hi):
            raise Bad("%s: value %d does not fit %s (%d to %d)" % (where, v, what, lo, hi))
        mask = ((1 << (bits[1] - bits[0] + 1)) - 1) << bits[0] if bits is not None else (1 << (8 * width)) - 1
        for b in range(width):
            m = (mask >> (8 * b)) & 0xFF
            if not m:
                continue
            cell = (key, rec, at + b)
            if taken.get(cell, 0) & m:
                raise Bad("%s: %s is set twice (byte +0x%X of record %d of %s)" % (where, what, at + b, rec, key))
            taken[cell] = taken.get(cell, 0) | m
        out.append({"table": key, "record": rec, "at": at, "type": typ, "bits": bits, "value": v, "what": what})
    return out


def read_edits(path):
    with open(path, "rb") as f:
        try:
            doc = tomllib.load(f)
        except tomllib.TOMLDecodeError as e:
            raise Bad("not valid TOML (%s)" % e)
    return parse(doc)


def base_rows(cache):
    """{key: [key, pc, end, count, stride, sha256, was]} of the install's table files."""
    p = os.path.join(cache, "base", "exe", "tables", "tables.toml")
    if not os.path.isfile(p):
        raise Bad("this install's game data has no table files (base\\exe\\tables): it was built by an older "
                  "release. Build the install again on the Sources page")
    with open(p, "rb") as f:
        return {r[0]: r for r in tomllib.load(f)["tables"]}


def build(cache, layer, edits_path, where):
    """opt/<layer>/exe/ of `cache` from the edits: the files written into a new
    folder and swapped in. Returns [(file name, sha256)] of what it wrote, the
    toml last. Bad when the edits or the install's tables are not there or do
    not agree with the catalogue."""
    edits = read_edits(edits_path)
    cat, _, _ = tables.load()
    by_key = {t["key"]: t for t in cat["table"]}
    rows = base_rows(cache)
    bodies, cells, order = {}, {}, []
    for e in edits:
        key = e["table"]
        t, r = by_key[key], rows.get(key)
        if r is None:
            raise Bad("this install has no table file for %s; build the install again on the Sources page" % key)
        if (r[3], r[4]) != (t["count"], t["stride"]):
            raise Bad("the install's %s is %d records of %d bytes, this release's catalogue %d of %d: build the "
                      "install again on the Sources page" % (key, r[3], r[4], t["count"], t["stride"]))
        if key not in bodies:
            with open(os.path.join(cache, "base", "exe", "tables", key + ".bin"), "rb") as f:
                body = f.read()
            if len(body) != r[2] - r[1] or sha(body) != r[5]:
                raise Bad("the install's table file %s.bin is not the one its tables.toml records; build the "
                          "install again on the Sources page" % key)
            bodies[key] = bytearray(body)
            cells[key] = set()
            order.append(key)
        code, width = tables.TYPES[e["type"]]
        off = e["record"] * t["stride"] + e["at"]
        if e["bits"] is None:
            struct.pack_into(code, bodies[key], off, e["value"])
        else:
            lo, hi = e["bits"]
            ucode = code.upper()
            word = struct.unpack_from(ucode, bodies[key], off)[0]
            mask = ((1 << (hi - lo + 1)) - 1) << lo
            struct.pack_into(ucode, bodies[key], off, (word & ~mask) | (e["value"] << lo))
        cells[key].add((e["record"], e["at"], width))
    final = os.path.join(cache, "opt", layer)
    work = os.path.join(cache, "opt", "." + layer + ".new")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(os.path.join(work, "exe"))
    lines = ["# opt/%s/exe/tables.toml - written by tools/table_edit_build.py (docs/image-layer-round.md 4)." % layer,
             "# One file a table the mod changes: <key>.bin is the install's whole table with the mod's",
             "# numbers in it. `cells` are the bytes that are the mod's - [key, record, at, length] -",
             "# and only those are laid over .data (src/hook/exe_image.cpp).",
             "", "build = %s" % json.dumps(where), "", "tables = ["]
    wrote, cell_lines = [], []
    for key in order:
        body = bytes(bodies[key])
        with open(os.path.join(work, "exe", key + ".bin"), "wb") as f:
            f.write(body)
        r = rows[key]
        lines.append('  ["%s", 0x%06X, %d, %d, "%s"],' % (key, r[1], r[3], r[4], sha(body)))
        wrote.append((key + ".bin", sha(body)))
        cell_lines += ['  ["%s", %d, %d, %d],' % ((key,) + c) for c in sorted(cells[key])]
    lines += ["]", "", "cells = ["] + cell_lines + ["]"]
    text = ("\n".join(lines) + "\n").encode("utf-8")
    with open(os.path.join(work, "exe", "tables.toml"), "wb") as f:
        f.write(text)
    wrote.append(("tables.toml", sha(text)))
    shutil.rmtree(final, ignore_errors=True)
    os.rename(work, final)
    return wrote


# ---------------------------------------------------------------- the self-test

def selftest():
    """Every refusal on a document made for it, then a build over a table file
    of zeros in a temporary cache. No game data: the catalogue gives only the
    table's key, count, stride and field names."""
    fails = []

    def expect(cond, what):
        print("%-86s %s" % (what, "ok" if cond else "FAIL"))
        if not cond:
            fails.append(what)

    cat, _, _ = tables.load()
    t = next(x for x in cat["table"] if x["key"] == "weapons")
    cols = columns(t)
    plain = next(c for c, (at, typ, bits) in cols.items() if bits is None)
    bit_cols = [c for c, (at, typ, bits) in cols.items() if bits is not None]
    good = {"set": [{"table": "weapons", "record": 3, "field": plain, "value": 1}]}
    cases = [
        ("good", good, None),
        ("raw", {"set": [{"table": "weapons", "record": 0, "at": t["stride"] - 1, "type": "u8", "value": 255}]}, None),
        ("no_rows", {}, "no [[set]] row"),
        ("extra_table", {"set": good["set"], "run": {"x": 1}}, "is not something an edits file has"),
        ("extra_key", {"set": [dict(good["set"][0], script="x")]}, "which a row does not have"),
        ("unknown_table", {"set": [dict(good["set"][0], table="hats")]}, "is not a table of the catalogue"),
        ("in_image", {"set": [{"table": "key_defaults", "record": 0, "at": 0, "type": "u8", "value": 1}]},
         "is not a table a mod can change"),
        ("record_past", {"set": [dict(good["set"][0], record=t["count"])]}, "is not a record of weapons"),
        ("record_bool", {"set": [dict(good["set"][0], record=True)]}, "is not a record of weapons"),
        ("unknown_field", {"set": [dict(good["set"][0], field="sharpness")]}, "has no field"),
        ("name_field", {"set": [dict(good["set"][0], field="name")]}, "a name is not a table's number"),
        ("name_bytes", {"set": [{"table": "weapons", "record": 0, "at": t["name"]["at"] + 2, "type": "u8", "value": 1}]},
         "is in the name field"),
        ("field_and_at", {"set": [dict(good["set"][0], at=0, type="u8")]}, "names a field and an offset"),
        ("at_past", {"set": [{"table": "weapons", "record": 0, "at": t["stride"] - 1, "type": "u16", "value": 1}]},
         "runs past the record"),
        ("bad_type", {"set": [{"table": "weapons", "record": 0, "at": 17, "type": "float", "value": 1}]},
         "give `field`, or `at`"),
        ("too_big", {"set": [dict(good["set"][0], value=1 << 40)]}, "does not fit"),
        ("not_a_number", {"set": [dict(good["set"][0], value="40")]}, "is not a whole number"),
        ("twice", {"set": [good["set"][0], dict(good["set"][0], value=2)]}, "is set twice"),
        ("too_many", {"set": [good["set"][0]] * (ROWS_MAX + 1)}, "over the %d" % ROWS_MAX),
    ]
    if len(bit_cols) >= 2:
        a, b = bit_cols[0], bit_cols[1]
        if cols[a][0] == cols[b][0]:      # two bit fields of one word: both may be set
            cases.append(("two_bits_one_word", {"set": [{"table": "weapons", "record": 1, "field": a, "value": 1},
                                                        {"table": "weapons", "record": 1, "field": b, "value": 1}]}, None))
    for name, doc, want in cases:
        try:
            parse(doc, cat)
            got = None
        except Bad as e:
            got = str(e)
        if want is None:
            expect(got is None, "%s: accepted%s" % (name, "" if got is None else " - NOT: " + got))
        else:
            expect(got is not None and want in got, "%s: refused (%s)" % (name, (got or "")[:60]))

    root = tempfile.mkdtemp(prefix="bof3x_table_edit_")
    try:
        cache = os.path.join(root, "cache")
        td = os.path.join(cache, "base", "exe", "tables")
        os.makedirs(td)
        size = t["count"] * t["stride"]
        zeros = bytes(size)
        with open(os.path.join(td, "weapons.bin"), "wb") as f:
            f.write(zeros)
        with open(os.path.join(td, "tables.toml"), "w", newline="\n") as f:
            f.write('build = "test"\n\ntables = [\n  ["weapons", 0x%06X, 0x%06X, %d, %d, "%s", "test"],\n]\n'
                    % (0x600000, 0x600000 + size, t["count"], t["stride"], sha(zeros)))
        at, typ, _ = cols[plain]
        code, width = tables.TYPES[typ]
        edits = os.path.join(root, "edits.toml")
        with open(edits, "w", newline="\n") as f:
            f.write('[[set]]\ntable = "weapons"\nrecord = 3\nfield = "%s"\nvalue = 1\n\n'
                    '[[set]]\ntable = "weapons"\nrecord = 0\nat = %d\ntype = "u8"\nvalue = 255\n' % (plain, t["stride"] - 1))
        wrote = build(cache, "mod-test", edits, "mod:test@1.0")
        d = os.path.join(cache, "opt", "mod-test", "exe")
        with open(os.path.join(d, "weapons.bin"), "rb") as f:
            body = f.read()
        with open(os.path.join(d, "tables.toml"), "rb") as f:
            meta = tomllib.load(f)
        want = bytearray(zeros)
        struct.pack_into(code, want, 3 * t["stride"] + at, 1)
        want[t["stride"] - 1] = 255
        expect(body == bytes(want), "build: the install's table with the two numbers in it, nothing else changed")
        expect(meta["tables"] == [["weapons", 0x600000, t["count"], t["stride"], sha(body)]] and
               sorted(meta["cells"]) == sorted([["weapons", 3, at, width], ["weapons", 0, t["stride"] - 1, 1]]) and
               meta["build"] == "mod:test@1.0", "build: tables.toml names the table, its hash and exactly the two cells")
        expect([w[0] for w in wrote] == ["weapons.bin", "tables.toml"], "build: reports the two files it wrote")
        again = build(cache, "mod-test", edits, "mod:test@1.0")
        expect(again == wrote, "build again: the same bytes")
        with open(os.path.join(td, "weapons.bin"), "wb") as f:
            f.write(b"\x01" + zeros[1:])
        try:
            build(cache, "mod-test", edits, "mod:test@1.0")
            expect(False, "a base table file that is not its row's: refused")
        except Bad as e:
            expect("is not the one its tables.toml records" in str(e), "a base table file that is not its row's: refused")
        shutil.rmtree(os.path.join(cache, "base"))
        try:
            build(cache, "mod-test", edits, "mod:test@1.0")
            expect(False, "an install with no table files: refused")
        except Bad as e:
            expect("no table files" in str(e), "an install with no table files: refused, with the rebuild sentence")
    finally:
        shutil.rmtree(root, ignore_errors=True)
    print("table_edit_build selftest: %d failure(s)" % len(fails))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("check")
    p.add_argument("edits")
    p = sub.add_parser("build")
    p.add_argument("--cache", required=True)
    p.add_argument("--layer", required=True)
    p.add_argument("--edits", required=True)
    p.add_argument("--where", required=True)
    sub.add_parser("selftest")
    a = ap.parse_args()
    if a.cmd == "selftest":
        return selftest()
    try:
        if a.cmd == "check":
            edits = read_edits(a.edits)
            print("%s: %d edits in %d table(s)" % (a.edits, len(edits), len({e["table"] for e in edits})))
            return 0
        wrote = build(a.cache, a.layer, a.edits, a.where)
        print("opt/%s/exe: %s" % (a.layer, ", ".join(w[0] for w in wrote)))
        return 0
    except Bad as e:
        print("table_edit_build: %s" % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
