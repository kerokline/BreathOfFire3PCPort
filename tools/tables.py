#!/usr/bin/env python
"""The table catalog: check it, list it, and read tables out of the player's own files.

tables.toml says where each game table lives in each build and what its fields
mean. It holds no values (CLAUDE.md rule 1): the values come out of the
player's BOF3.exe or disc when this tool reads them, and go to the terminal or
to analysis/, which is gitignored.

    python tools/tables.py check                       # the catalog alone - no game files; CI runs this
    python tools/tables.py list [--group items]        # tables and their fields
    python tools/tables.py dump weapons --game bof3    # from the PC exe
    python tools/tables.py dump weapons --disc DISC    # from a PSX disc, at the build's recorded address
    python tools/tables.py dump --group items --disc DISC --game bof3 --csv analysis/tables
                                                       # a build with no recorded address: found by the
                                                       # PC table's numeric bytes, as loc_build.py does

A field's `at` is its offset in the PC record. On a disc the name field is
narrower (8 bytes on the Japanese builds, 12 on the Western ones), so a field
past the name sits `16 - len` bytes earlier there; the numeric bytes are
otherwise the same (534 of 534 records, measured 2026-09-20, loc_build.py).
"""
import argparse
import csv
import os
import struct
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

TYPES = {"u8": ("<B", 1), "s8": ("<b", 1), "u16": ("<H", 2), "s16": ("<h", 2), "u32": ("<I", 4), "s32": ("<i", 4)}
STATUSES = {"evidence", "hypothesis", "unknown"}
PC_NAME_LEN = 16


class Missing(Exception):
    pass


def fail(msg):
    sys.exit("tables: " + msg)


def load():
    with open(os.path.join(ROOT, "tables.toml"), "rb") as f:
        cat = tomllib.load(f)
    with open(os.path.join(ROOT, "symbols.toml"), "rb") as f:
        syms = {e["name"]: e["pc"] for e in tomllib.load(f).get("data", [])}
    with open(os.path.join(ROOT, "fixtures.toml"), "rb") as f:
        builds = {b["id"]: b for b in tomllib.load(f)["build"]}
    return cat, syms, builds


def check(cat, syms, builds):
    """Every error in the catalog, as strings. Reads the repository only."""
    errs, seen = [], set()
    name_lens = cat["meta"]["name_len"]
    for t in cat.get("table", []):
        key = t.get("key", "?")
        where = "table %s" % key
        if key in seen:
            errs.append("%s: key used twice" % where)
        seen.add(key)
        for f in ("key", "symbol", "group", "what", "index", "count", "stride"):
            if f not in t:
                errs.append("%s: no %s" % (where, f))
        if t.get("symbol") not in syms:
            errs.append("%s: symbol %r is not a symbols.toml [[data]] name" % (where, t.get("symbol")))
        stride = t.get("stride", 0)
        name = t.get("name")
        if name is not None and not (0 <= name["at"] and name["at"] + PC_NAME_LEN <= stride):
            errs.append("%s: the name field does not fit the stride" % where)
        taken, bits_taken, bit_bytes = {}, {}, set()
        if name is not None:
            for b in range(name["at"], name["at"] + PC_NAME_LEN):
                taken[b] = "name"
        for fld in t.get("field", []):
            fw = "%s field %s" % (where, fld.get("name", "?"))
            if fld.get("type") not in TYPES:
                errs.append("%s: type %r" % (fw, fld.get("type")))
                continue
            if fld.get("status") not in STATUSES:
                errs.append("%s: status %r" % (fw, fld.get("status")))
            if fld.get("status") != "unknown" and not fld.get("cite"):
                errs.append("%s: a %s field cites nothing" % (fw, fld.get("status")))
            if not fld.get("meaning"):
                errs.append("%s: no meaning" % fw)
            width = TYPES[fld["type"]][1]
            if not 0 <= fld["at"] or fld["at"] + width > stride:
                errs.append("%s: +0x%X..+0x%X is outside the 0x%X-byte record" % (fw, fld["at"], fld["at"] + width, stride))
            if "bits" in fld:
                lo, hi = fld["bits"]
                if not 0 <= lo <= hi < 8 * width:
                    errs.append("%s: bits %d..%d do not fit a %s" % (fw, lo, hi, fld["type"]))
                for bit in range(lo, hi + 1):
                    k = (fld["at"], fld["type"], bit)
                    if k in bits_taken:
                        errs.append("%s: bit %d is also %s" % (fw, bit, bits_taken[k]))
                    bits_taken[k] = fld["name"]
                bit_bytes.update(range(fld["at"], fld["at"] + width))
                continue
            for b in range(fld["at"], fld["at"] + width):
                if (b in taken or b in bit_bytes) and not fld.get("overlaps"):
                    errs.append("%s: byte +0x%X is also %s (mark `overlaps` if intended)" % (fw, b, taken.get(b, "a bit field")))
                taken.setdefault(b, fld["name"])
        for p in t.get("psx", []):
            if p.get("build") not in builds:
                errs.append("%s: psx build %r is not in fixtures.toml" % (where, p.get("build")))
            elif name_len_for(builds[p["build"]], name_lens) is None:
                errs.append("%s: no name length for build %s ([meta] name_len)" % (where, p["build"]))
            for f in ("file", "addr", "status", "cite"):
                if f not in p:
                    errs.append("%s psx %s: no %s" % (where, p.get("build"), f))
    return errs


def name_len_for(build, name_lens):
    if build["id"] in name_lens:
        return name_lens[build["id"]]
    return name_lens.get("japanese" if build.get("language") == "Japanese" else "western")


def narrowed(t, at, name_len):
    """A PC offset moved to a record whose name field is `name_len` bytes."""
    name = t.get("name")
    if name is None or at < name["at"] + PC_NAME_LEN:
        return at
    return at - PC_NAME_LEN + name_len


# --------------------------------------------------------------- the sources

def exe_bytes(game, va, size):
    import loc_build
    return loc_build.exe_bytes(game, va, size)


def disc_build(disc, builds):
    cnf = disc.read("SYSTEM.CNF").split(b"\n")[0].upper().replace(b".", b"")   # SLUS_004.22 -> SLUS_00422
    for b in builds.values():
        serial = b.get("serial", "").upper().replace("-", "_").encode()
        if serial and serial in cnf:
            return b
    fail("the disc's boot line %r names no build in fixtures.toml" % cnf)


def emi_at(disc, file, addr, size):
    """`size` bytes at PSX address `addr` in `file`: an EMI's sections by
    their load addresses, or with file = "BOOT" the boot EXE SYSTEM.CNF names."""
    import loc_build
    if file == "BOOT":
        boot = disc.read("SYSTEM.CNF").split(b"\n")[0].split(b":")[-1].strip().lstrip(b"\\").split(b";")[0]
        exe = disc.read(boot.decode().replace("\\", "/"))
        if exe[:8] != b"PS-X EXE":
            raise Missing("the boot file %s is not a PS-X EXE" % boot.decode())
        t_addr, t_size = struct.unpack_from("<II", exe, 0x18)
        if t_addr <= addr and addr + size <= t_addr + t_size:
            return exe[0x800 + addr - t_addr:0x800 + addr - t_addr + size]
        raise Missing("0x%08X..+0x%X is not in the boot EXE (0x%08X..0x%08X)" % (addr, size, t_addr, t_addr + t_size))
    found = disc.find(file)
    if not found:
        raise Missing("no %s on this disc" % file)
    for dest, blob in loc_build.emi_sections(disc.read(found[0])):
        if dest <= addr and addr + size <= dest + len(blob):
            return blob[addr - dest:addr - dest + size]
    raise Missing("0x%08X..+0x%X is in no section of %s" % (addr, size, file))


def psx_bytes(disc, where, size):
    """The recorded address read from the first of its files that holds it.
    `file` may list several when which one holds the table is not settled."""
    files = where["file"] if isinstance(where["file"], list) else [where["file"]]
    why = []
    for f in files:
        try:
            raw = emi_at(disc, f, where["addr"], size)
            if len(files) > 1:
                print("(%s: found in %s)" % (where["build"], f), file=sys.stderr)
            return raw
        except Missing as e:
            why.append(str(e))
    fail("; ".join(why))


def found_by_numbers(t, donor_sections, pc, name_len):
    """The table in `donor_sections` whose numeric bytes equal the PC table's,
    as loc_build.convert_names finds it. Returns the donor bytes."""
    stride, count = t["stride"], t["count"]
    d_stride = stride - PC_NAME_LEN + name_len
    name_at = t["name"]["at"]

    def numbers(buf, i, n_len, st, origin=0):
        rec = buf[origin + i * st:origin + (i + 1) * st]
        return rec[:name_at] + rec[name_at + n_len:]
    want = [numbers(pc, i, PC_NAME_LEN, stride) for i in range(count)]
    for _, blob in donor_sections:
        at = blob.find(want[1])
        while at >= 0:
            start = at - d_stride - (0 if name_at else name_len)
            if start >= 0 and all(numbers(blob, i, name_len, d_stride, start) == want[i] for i in range(count)):
                return blob[start:start + d_stride * count]
            at = blob.find(want[1], at + 1)
    return None


def records(t, raw, name_len):
    stride = t["stride"] - PC_NAME_LEN + name_len if t.get("name") else t["stride"]
    rows = []
    for i in range(t["count"]):
        rec = raw[i * stride:(i + 1) * stride]
        row = {"id": i}
        if t.get("name"):
            nm = rec[t["name"]["at"]:t["name"]["at"] + name_len].split(b"\0")[0]
            row["name"] = "".join(chr(c) if 0x20 <= c < 0x7F else "\\x%02X" % c for c in nm)
        for fld in t.get("field", []):
            code, _ = TYPES[fld["type"]]
            v = struct.unpack_from(code, rec, narrowed(t, fld["at"], name_len))[0]
            if "bits" in fld:
                lo, hi = fld["bits"]
                v = (v >> lo) & ((1 << (hi - lo + 1)) - 1)
            row[fld["name"]] = v
        rows.append(row)
    return rows


# ------------------------------------------------------------------ commands

def cmd_check(a, cat, syms, builds):
    errs = check(cat, syms, builds)
    for e in errs:
        print("error: " + e)
    n = len(cat.get("table", []))
    nf = sum(len(t.get("field", [])) for t in cat.get("table", []))
    print("tables: %d tables, %d fields, %d error(s)" % (n, nf, len(errs)))
    return 1 if errs else 0


def selected(a, cat):
    ts = cat.get("table", [])
    if a.group:
        ts = [t for t in ts if t["group"] == a.group]
    if getattr(a, "tables", None):
        want = set(a.tables)
        ts = [t for t in ts if t["key"] in want]
        missing = want - {t["key"] for t in ts}
        if missing:
            fail("no table %s in tables.toml" % ", ".join(sorted(missing)))
    if not ts:
        fail("nothing selected")
    return ts


def cmd_list(a, cat, syms, builds):
    for t in selected(a, cat):
        print("%-12s %s  0x%X  %d x 0x%X  (%s; by %s)" % (t["key"], t["symbol"], syms[t["symbol"]], t["count"],
                                                      t["stride"], t["what"], t["index"]))
        for p in t.get("psx", []):
            print("    %-8s %s at 0x%08X  [%s]" % (p["build"], p["file"], p["addr"], p["status"]))
        if t.get("name"):
            print("    +0x%02X  name[16]" % t["name"]["at"])
        for fld in sorted(t.get("field", []), key=lambda f: (f["at"], f.get("bits", [0])[0])):
            bits = fld.get("bits")
            span = "" if bits is None else ("b%d" % bits[0] if bits[0] == bits[1] else "b%d-%d" % tuple(bits))
            print("    +0x%02X %-6s %-4s %-16s %-10s %s" % (fld["at"], span, fld["type"], fld["name"], fld["status"], fld["meaning"]))
    return 0


def cmd_dump(a, cat, syms, builds):
    if not a.game and not a.disc:
        fail("dump reads the player's files: give --game DIR (BOF3.exe) and/or --disc DISC")
    ts = selected(a, cat)
    errs = check(cat, syms, builds)
    if errs:
        fail("the catalog has %d error(s); run `tables.py check`" % len(errs))
    disc = build = None
    if a.disc:
        import psx_disc
        disc = psx_disc.Disc(a.disc)
        build = disc_build(disc, builds)
    for t in ts:
        pc_size = t["stride"] * t["count"]
        if disc is None:
            label, name_len = "pc-zh", PC_NAME_LEN
            raw = exe_bytes(a.game, syms[t["symbol"]], pc_size)
        else:
            label = build["id"]
            name_len = name_len_for(build, cat["meta"]["name_len"]) if t.get("name") else PC_NAME_LEN
            size = pc_size - (PC_NAME_LEN - name_len) * t["count"]
            where = next((p for p in t.get("psx", []) if p["build"] == build["id"]), None)
            if where is not None:
                raw = psx_bytes(disc, where, size)
            elif a.game and t.get("name") and t.get("find_in"):
                import loc_build
                found = disc.find(t["find_in"])
                if not found:
                    fail("%s: no %s on this disc" % (t["key"], t["find_in"]))
                raw = found_by_numbers(t, loc_build.emi_sections(disc.read(found[0])),
                                       exe_bytes(a.game, syms[t["symbol"]], pc_size), name_len)
                if raw is None:
                    fail("%s: no table in %s with the PC table's numbers" % (t["key"], t["find_in"]))
            else:
                fail("%s: no address recorded for %s; pass --game to find it by the PC table's numbers"
                     % (t["key"], build["id"]))
        rows = records(t, raw, name_len)
        cols = list(rows[0].keys())
        if a.csv:
            out_dir = os.path.join(a.csv, label)
            os.makedirs(out_dir, exist_ok=True)
            path = os.path.join(out_dir, t["key"] + ".csv")
            with open(path, "w", newline="", encoding="utf-8") as f:
                w = csv.DictWriter(f, fieldnames=cols)
                w.writeheader()
                w.writerows(rows)
            print("%s: %d records -> %s" % (t["key"], len(rows), path))
        else:
            print("== %s (%s)" % (t["key"], label))
            print("\t".join(cols))
            for r in rows:
                print("\t".join(str(r[c]) for c in cols))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("check").set_defaults(fn=cmd_check)
    p = sub.add_parser("list")
    p.add_argument("tables", nargs="*")
    p.add_argument("--group")
    p.set_defaults(fn=cmd_list)
    p = sub.add_parser("dump")
    p.add_argument("tables", nargs="*")
    p.add_argument("--group")
    p.add_argument("--game", help="the directory holding BOF3.exe")
    p.add_argument("--disc", help="a PSX disc image (.cue, .bin or .iso)")
    p.add_argument("--csv", help="write <dir>/<build>/<table>.csv (use analysis/: it is gitignored)")
    p.set_defaults(fn=cmd_dump)
    a = ap.parse_args()
    if a.cmd == "dump" and a.csv and not os.path.abspath(a.csv).startswith(os.path.join(ROOT, "analysis")) \
            and os.path.abspath(a.csv).startswith(ROOT):
        fail("--csv inside the repository must be under analysis/ (gitignored): the output is game data")
    cat, syms, builds = load()
    return a.fn(a, cat, syms, builds)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:          # `| head`
        os._exit(0)
