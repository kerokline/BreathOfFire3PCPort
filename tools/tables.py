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
    python tools/tables.py locate --game bof3 --disc DISC [--disc DISC ...]
                                                       # every table's place in each disc, as
                                                       # [[table.psx]] rows to review and paste

DISC is a PSX disc (.cue / .bin) or a PSP disc (.iso). On a PSP disc `file`
"BOOT.BIN" is PSP_GAME/SYSDIR/BOOT.BIN, a plain ELF, and `addr` its link-time
offset (tools/psp_elf.py); an EMI's addresses are its sections' destinations
with bit 31 restored. A table with no name field is located through
exe_maps/<build>.tsv (tools/exe_twins.py map), which `check` also validates.

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

MAPS = os.path.join(ROOT, "exe_maps")
MAP_COLS = ["pc", "pc_end", "file", "section", "addr", "agree", "copies", "how"]
MAP_HOW = {"run", "search", "neighbour", "search+neighbour", "neighbour-file", "name"}
PC_DATA = (0x5C4000, 0x676000)
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
        for f in ("key", "group", "what", "index", "count", "stride"):
            if f not in t:
                errs.append("%s: no %s" % (where, f))
        if ("symbol" in t) == ("pc_twin" in t):
            errs.append("%s: give a symbol, or pc_twin saying why the PC has no such table" % where)
        elif "symbol" in t and t["symbol"] not in syms:
            errs.append("%s: symbol %r is not a symbols.toml [[data]] name" % (where, t.get("symbol")))
        if "pc_twin" in t and not t.get("psx"):
            errs.append("%s: a table with no PC twin needs a [[table.psx]] row" % where)
        stride = t.get("stride", 0)
        if "count_status" in t and (t["count_status"] not in STATUSES or not t.get("count_cite")):
            errs.append("%s: count_status wants evidence / hypothesis / unknown and a count_cite" % where)
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
            n_rep, step = fld.get("repeat", [1, width])
            if n_rep < 1 or step < width:
                errs.append("%s: repeat %r (a count of at least 1, a step of at least the width)" % (fw, fld["repeat"]))
            last = fld["at"] + (n_rep - 1) * step
            if not 0 <= fld["at"] or last + width > stride:
                errs.append("%s: +0x%X..+0x%X is outside the 0x%X-byte record" % (fw, fld["at"], last + width, stride))
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
            for b in (fld["at"] + r * step + k for r in range(n_rep) for k in range(width)):
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
            if p.get("status") not in STATUSES - {"unknown"}:
                errs.append("%s psx %s: status %r (evidence or hypothesis)" % (where, p.get("build"), p.get("status")))
    errs += check_maps(builds)
    return errs


def read_map(path):
    rows = []
    with open(path, encoding="utf-8") as f:
        lines = [l.rstrip("\n") for l in f if not l.startswith("#")]
    if not lines or lines[0].split("\t") != MAP_COLS:
        raise ValueError("the column line is not %s" % "\t".join(MAP_COLS))
    for n, l in enumerate(lines[1:], 2):
        c = l.split("\t")
        if len(c) != len(MAP_COLS):
            raise ValueError("line %d: %d columns" % (n, len(c)))
        rows.append({"pc": int(c[0], 16), "pc_end": int(c[1], 16), "file": c[2],
                     "section": None if c[3] == "-" else int(c[3]), "addr": int(c[4], 16),
                     "agree": int(c[5]), "copies": int(c[6]), "how": c[7]})
    return rows


def check_maps(builds):
    """exe_maps/<build>.tsv: a known build, segments in order, inside the PC's
    initialised data, not overlapping, agreeing bytes no more than the span."""
    errs = []
    if not os.path.isdir(MAPS):
        return errs
    for name in sorted(os.listdir(MAPS)):
        where = "exe_maps/" + name
        if not name.endswith(".tsv") or name[:-4] not in builds:
            errs.append("%s: not <a fixtures.toml build id>.tsv" % where)
            continue
        try:
            rows = read_map(os.path.join(MAPS, name))
        except ValueError as e:
            errs.append("%s: %s" % (where, e))
            continue
        last = PC_DATA[0]
        for r in rows:
            if not (last <= r["pc"] < r["pc_end"] <= PC_DATA[1]):
                errs.append("%s: segment 0x%X..0x%X out of order or outside .rdata/.data" % (where, r["pc"], r["pc_end"]))
            if r["how"] not in MAP_HOW:
                errs.append("%s: segment 0x%X: how %r" % (where, r["pc"], r["how"]))
            if not 0 < r["agree"] <= r["pc_end"] - r["pc"]:
                errs.append("%s: segment 0x%X agrees on %d bytes of %d" % (where, r["pc"], r["agree"], r["pc_end"] - r["pc"]))
            last = r["pc_end"]
    return errs


def map_lookup(build_id, pc, size):
    """Where exe_maps puts [pc, pc + size): (file, section, addr, bytes of it
    the segment covers), from the segment holding pc, or None."""
    path = os.path.join(MAPS, build_id + ".tsv")
    if not os.path.exists(path):
        return None
    for r in read_map(path):
        if r["pc"] <= pc < r["pc_end"]:
            return r["file"], r["section"], r["addr"] + pc - r["pc"], min(size, r["pc_end"] - pc)
    return None


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
    if "SYSTEM.CNF" not in disc.files:                                         # a PSP disc
        umd = disc.read("UMD_DATA.BIN").split(b"|")[0].upper().replace(b"-", b"_")
        for b in builds.values():
            if b.get("serial") and b["serial"].upper().replace("-", "_").encode() == umd:
                return b
        fail("the PSP disc's id %r names no build in fixtures.toml" % umd)
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
    if file == "BOOT.BIN":
        elf = disc.read("PSP_GAME/SYSDIR/BOOT.BIN")
        if elf[:4] != b"\x7fELF":
            raise Missing("BOOT.BIN is not a plain ELF")
        phoff, = struct.unpack_from("<I", elf, 28)
        _, off, va, _, fs, _, _, _ = struct.unpack_from("<8I", elf, phoff)
        if va <= addr and addr + size <= va + fs:
            return elf[off + addr - va:off + addr - va + size]
        raise Missing("0x%X..+0x%X is not in BOOT.BIN's first program header" % (addr, size))
    if file == "BOOT":
        boot = disc.read("SYSTEM.CNF").split(b"\n")[0].split(b":")[-1].strip().lstrip(b"\\").split(b";")[0]
        exe = disc.read(boot.decode().replace("\\", "/"))
        if exe[:8] != b"PS-X EXE":
            raise Missing("the boot file %s is not a PS-X EXE" % boot.decode())
        t_addr, t_size = struct.unpack_from("<II", exe, 0x18)
        if t_addr <= addr and addr + size <= t_addr + t_size:
            return exe[0x800 + addr - t_addr:0x800 + addr - t_addr + size]
        raise Missing("0x%08X..+0x%X is not in the boot EXE (0x%08X..0x%08X)" % (addr, size, t_addr, t_addr + t_size))
    found = [p for p in disc.find(file.rsplit("/", 1)[-1]) if p.endswith(file)]
    if not found:
        raise Missing("no %s on this disc" % file)
    psp = "SYSTEM.CNF" not in disc.files
    for dest, blob in loc_build.emi_sections(disc.read(found[0])):
        if psp and 0x10000 <= dest < 0x800000:
            dest |= 0x80000000                       # the PSP clears bit 31 (region_diff.nd)
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


def locate_by_numbers(t, srcs, pc, name_len):
    """Every place in `srcs` ((file, section, address, bytes), tools/exe_twins.py
    sources) whose numeric bytes equal the PC table's, record for record: [(file,
    section, address)]."""
    out = []
    for file, sec, base, blob in srcs:
        for at, differ in _numbers_at(t, blob, pc, name_len):
            out.append((file, sec, base + at, differ))
    return out


NUMBERS_SLACK = 2    # records whose numbers may differ in a find (a later build's edit, psp-stallion.md 3.2)


def _numbers_at(t, blob, pc, name_len):
    stride, count = t["stride"], t["count"]
    d_stride = stride - PC_NAME_LEN + name_len
    name_at = t["name"]["at"]

    def numbers(buf, i, n_len, st, origin=0):
        rec = buf[origin + i * st:origin + (i + 1) * st]
        return rec[:name_at] + rec[name_at + n_len:]
    want = [numbers(pc, i, PC_NAME_LEN, stride) for i in range(count)]
    hits = []
    for anchor in (1, 2):
        at = blob.find(want[anchor])
        while at >= 0:
            start = at - anchor * d_stride - (0 if name_at else name_len)
            if start >= 0 and start + d_stride * count <= len(blob) and start not in [h[0] for h in hits]:
                differ = [i for i in range(count) if numbers(blob, i, name_len, d_stride, start) != want[i]]
                if len(differ) <= NUMBERS_SLACK:
                    hits.append((start, differ))
            at = blob.find(want[anchor], at + 1)
    return hits


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
            code, width = TYPES[fld["type"]]
            n_rep, step = fld.get("repeat", [1, width])
            for r in range(n_rep):
                v = struct.unpack_from(code, rec, narrowed(t, fld["at"] + r * step, name_len))[0]
                if "bits" in fld:
                    lo, hi = fld["bits"]
                    v = (v >> lo) & ((1 << (hi - lo + 1)) - 1)
                row[fld["name"] if n_rep == 1 else "%s_%d" % (fld["name"], r)] = v
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
        pc = "0x%X" % syms[t["symbol"]] if "symbol" in t else "-"
        print("%-12s %s  %s  %d x 0x%X  (%s; by %s)" % (t["key"], t.get("symbol", "(no PC twin)"), pc, t["count"],
                                                    t["stride"], t["what"], t["index"]))
        for p in t.get("psx", []):
            print("    %-9s %s%s at 0x%08X  [%s]" % (p["build"], p["file"], "" if "section" not in p else
                                                 "#%d" % p["section"], p["addr"], p["status"]))
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
        if disc is None and "symbol" not in t:
            print("%s: no PC twin (%s)" % (t["key"], t["pc_twin"]), file=sys.stderr)
            continue
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


def pc_addr(t, syms):
    return syms[t["symbol"]] if "symbol" in t else None


def masked_equal(pc, other):
    """Bytes of `other` equal to the PC's, a PC word pointing into the image
    counting as equal (the other build's pointer is its own address)."""
    eq, i = 0, 0
    while i < len(pc):
        if i % 4 == 0 and i + 4 <= len(pc) and 0x401000 <= struct.unpack_from("<I", pc, i)[0] < 0x940000:
            eq, i = eq + 4, i + 4
            continue
        eq += pc[i] == other[i] if i < len(other) else 0
        i += 1
    return eq


def cmd_locate(a, cat, syms, builds):
    import datetime
    import exe_twins
    import psx_disc
    ts = selected(a, cat)
    today = datetime.date.today().isoformat()
    for path in a.disc:
        disc = psx_disc.Disc(path)
        build = disc_build(disc, builds)
        srcs = exe_twins.sources(disc)
        print("# ---- %s (%s)" % (build["id"], os.path.basename(path)))
        for t in ts:
            if "symbol" not in t:
                continue
            pc_size = t["stride"] * t["count"]
            pc = exe_bytes(a.game, syms[t["symbol"]], pc_size)
            rows = []
            if t.get("name"):
                name_len = name_len_for(build, cat["meta"]["name_len"])
                for file, sec, addr, differ in locate_by_numbers(t, srcs, pc, name_len):
                    rows.append((file, sec, addr, "numbers equal in %s %d records%s, names narrowed to %d bytes"
                                 % ("all" if not differ else "%d of" % (t["count"] - len(differ)), t["count"],
                                    "" if not differ else " (id %s differs)" % ", ".join(map(str, differ)),
                                    name_len)))
            else:
                hit = map_lookup(build["id"], syms[t["symbol"]], pc_size)
                if hit:
                    file, sec, addr, _ = hit
                    try:
                        other = emi_at(disc, file, addr, pc_size)
                    except Missing:
                        other = b""
                    eq = sum(1 for x, y in zip(pc, other) if x == y)
                    ptr = masked_equal(pc, other) - eq
                    rows.append((file, sec, addr, "exe_maps/%s.tsv; %d of %d bytes equal%s"
                                 % (build["id"], eq, pc_size, "" if not ptr else
                                    ", %d more in words that point into the PC image" % ptr)))
            if not rows:
                print("# %s: not found in %s" % (t["key"], build["id"]))
                continue
            status = "evidence" if len(rows) == 1 else "hypothesis"
            for file, sec, addr, why in rows:
                print("[[table.psx]]   # %s" % t["key"])
                print('build = "%s"' % build["id"])
                print('file = "%s"' % file)
                if sec is not None:
                    print("section = %d" % sec)
                print("addr = 0x%08X" % addr)
                print('status = "%s"' % status)
                print('cite = "tables.py locate %s: %s%s"' % (today, why, "" if len(rows) == 1 else
                                                             "; one of %d places" % len(rows)))
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
    p = sub.add_parser("locate")
    p.add_argument("tables", nargs="*")
    p.add_argument("--group")
    p.add_argument("--game", required=True, help="the directory holding BOF3.exe")
    p.add_argument("--disc", action="append", required=True, help="a PSX or PSP disc image")
    p.set_defaults(fn=cmd_locate)
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
