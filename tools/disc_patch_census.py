#!/usr/bin/env python
"""What a patched PlayStation disc changes, edit by edit: the census of a romhack.

    python tools/disc_patch_census.py ORIG PATCHED --out analysis/hack/census.tsv [--twins DIR]

docs/romhack-mods.md section 2 (issue 216). ORIG is a held disc (a .cue or the
data track), PATCHED the same disc after a hack's patch (tools/vcdiff.py
apply). Every file is compared; an EMI section by section, each distinct
(old, new) pair once however many EMIs carry it, the two aligned so an
insertion is one edit and not a change to everything after it. Each edit run
is one row:

    file, section, copies   the first EMI that holds the section, and how many do
    dest, kind              the section's load address and type
    where                   kept (the PC has it as a DAT chunk: dat index, kind) |
                            dropped (an overlay the port compiled into BOF3.exe) |
                            none (the EMI has no PC container) | boot | file
    addr, old, new          the run's address in ORIG and its length before and after
    is                      code | data | code? | data? | moved | - (kept sections: not asked)
                            moved: an address that changed only because the bytes it
                            points at moved (the hack's tool relinked it), not an edit
    name                    for code, the function it is in and its PC twin; for
                            data, the PC address exe_maps places it at and the
                            symbol there

`is` comes from, in order: the function table of a decompilation of this disc
(--twins DIR holding region_twin_map.py's us_jp.tsv and pc_map.tsv: inside a
function there is code, with its name; elsewhere in a covered section, data);
exe_maps/<build>.tsv (a byte the PC holds in .data is data); else a guess, marked
`?`, from whether the words around the run decode as MIPS instructions.

The TSV holds addresses, lengths and names only - no byte of the game (CLAUDE.md
rule 1). It goes to analysis/ or a scratch folder all the same.
"""
import argparse
import collections
import hashlib
import json
import os
import struct
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import psx_disc   # noqa: E402

ANCHOR = 12          # bytes that must agree for two streams to be back in step


def emi_sections(blob):
    count, = struct.unpack_from("<I", blob, 0)
    out, pos = [], 0x800
    for i in range(count):
        size, dest = struct.unpack_from("<II", blob, 0x10 + 16 * i)
        kind, = struct.unpack_from("<H", blob, 0x1C + 16 * i)
        out.append((i, kind, dest, blob[pos:pos + size]))
        pos += ((size + 0x7FF) >> 11) * 0x800
    return out


def edits(a, b):
    """[(a0, a1, b0, b1)]: the runs where a and b differ, found by walking both
    and, at a difference, taking the nearest place the next ANCHOR bytes agree
    again. Greedy, so not minimal, but an insertion or a deletion stays one run."""
    out = []
    i = j = 0
    na, nb = len(a), len(b)
    while i < na and j < nb:
        if a[i] == b[j]:
            while i + 4096 <= na and j + 4096 <= nb and a[i:i + 4096] == b[j:j + 4096]:
                i += 4096
                j += 4096
            while i < na and j < nb and a[i] == b[j]:
                i += 1
                j += 1
            continue
        best = None
        for da in range(0, 8192):
            if i + da + ANCHOR > na or (best is not None and da >= best[0] + best[1]):
                break
            k = b.find(a[i + da:i + da + ANCHOR], j, j + 16384)
            if k >= 0 and (best is None or da + (k - j) < best[0] + best[1]):
                best = (da, k - j)
        if best is None:
            out.append((i, na, j, nb))
            return out
        out.append((i, i + best[0], j, j + best[1]))
        i += best[0]
        j += best[1]
    if i < na or j < nb:
        out.append((i, na, j, nb))
    return out


def load_maps(build):
    """{(FILE, section or None): [(addr, addr_end, pc)]} from exe_maps/<build>.tsv."""
    maps = collections.defaultdict(list)
    path = os.path.join(ROOT, "exe_maps", build + ".tsv")
    if not os.path.isfile(path):
        return maps
    with open(path) as f:
        for line in f:
            if line.startswith(("#", "pc\t")):
                continue
            pc, pc_end, name, sec, addr = line.rstrip("\n").split("\t")[:5]
            n = int(pc_end, 16) - int(pc, 16)
            maps[(name.upper(), int(sec) if sec.isdigit() else None)].append((int(addr, 16), int(addr, 16) + n, int(pc, 16)))
    return maps


def load_symbols():
    """(sorted [(pc, name)] of [[data]], {pc: name} of [[func]]) from symbols.toml."""
    import tomllib
    with open(os.path.join(ROOT, "symbols.toml"), "rb") as f:
        doc = tomllib.load(f)
    data = sorted((d["pc"], d["name"]) for d in doc.get("data", []) if "pc" in d and "name" in d)
    return data, {d["pc"]: d["name"] for d in doc.get("func", []) if "pc" in d and "name" in d}


def data_name(data, pc):
    import bisect
    i = bisect.bisect_right(data, (pc, "￿")) - 1
    if i < 0:
        return ""
    at, name = data[i]
    return name if at == pc else "%s+0x%X" % (name, pc - at) if pc - at < 0x4000 else ""


def load_twins(d):
    """{target: sorted [(us, size, name)]}, {(target, us): pc} from a twins folder."""
    funcs, pcs = collections.defaultdict(list), {}
    if not d:
        return funcs, pcs
    with open(os.path.join(d, "us_jp.tsv")) as f:
        next(f)
        for line in f:
            target, us, size, kind, name = line.rstrip("\n").split("\t")[:5]
            funcs[target].append((int(us, 16), int(size), os.path.basename(name).replace(".c", "")))
    for v in funcs.values():
        v.sort()
    p = os.path.join(d, "pc_map.tsv")
    if os.path.isfile(p):
        with open(p) as f:
            head = next(f).rstrip("\n").split("\t")
            for line in f:
                r = dict(zip(head, line.rstrip("\n").split("\t")))
                if r.get("us") and r.get("target"):
                    pcs.setdefault((r["target"], int(r["us"], 16)), int(r["pc"], 16))
    return funcs, pcs


def target_sections(disc, funcs):
    """{sha of a section of ORIG: target} for the decompilation's targets, so a
    copy of the same overlay in another EMI is known by its bytes."""
    out = {}
    stems = {p.rsplit("/", 1)[-1][:-4].lower(): p for p in disc.files if p.endswith(".EMI")}
    for target in funcs:
        parts = target.split("/")
        if parts[0] != "emi" or parts[-2] not in stems:
            continue
        secs = emi_sections(disc.read(stems[parts[-2]]))
        idx = int(parts[-1])
        if idx < len(secs):
            out[hashlib.sha1(secs[idx][3]).digest()] = target
    return out


def moved_by(runs):
    """The distances bytes of a section moved by: each running total of what
    the edit runs inserted and deleted, 0 left out."""
    out, d = set(), 0
    for a0, a1, b0, b1 in runs:
        d += (b1 - b0) - (a1 - a0)
        if d:
            out.add(d)
        if (b1 - b0) != (a1 - a0):
            out.add((b1 - b0) - (a1 - a0))
    return out


def relocation(da, db, a0, a1, b0, b1, shifts):
    """When a same-length run inside one word is only an address that follows
    moved bytes - a jump target, a 16-bit offset or a pointer word that differs
    by one of `shifts` - that distance; else None. The edit is the patch tool's
    relinking, not a change the author made by hand."""
    if not shifts or a1 - a0 != b1 - b0 or a1 - a0 > 4 or (a0 & ~3) != ((a1 - 1) & ~3) or (a0 & 3) != (b0 & 3):
        return None
    w0, = struct.unpack_from("<I", da, a0 & ~3)
    w1, = struct.unpack_from("<I", db, b0 & ~3)
    d32 = (w1 - w0) & 0xFFFFFFFF
    d32 -= (d32 & 0x80000000) << 1
    if d32 in shifts:                                   # a pointer word, or an immediate with no carry
        return d32
    if (w0 >> 26) == (w1 >> 26) and (w0 >> 26) in (2, 3):   # j, jal: a word index
        d = ((w1 - w0) & 0x3FFFFFF)
        d -= (d & 0x2000000) << 1
        if d * 4 in shifts:
            return d * 4
    if (w0 >> 16) == (w1 >> 16):                        # the low half of an address
        d = (w1 - w0) & 0xFFFF
        d -= (d & 0x8000) << 1
        if d in shifts:
            return d
    return None


def looks_like_code(md, sec, off):
    """A guess: do the 32 words around `off` decode as MIPS instructions?"""
    lo = max(0, (off & ~3) - 64)
    words = sec[lo:lo + 128]
    if len(words) < 64 or not any(words):
        return False
    good = total = 0
    for k in range(0, len(words) - 3, 4):
        w = words[k:k + 4]
        total += 1
        if w == b"\0\0\0\0" or next(md.disasm(w, 0), None) is not None:
            good += 1
    return good * 10 >= total * 9 and words.count(b"\x00") * 3 < len(words) * 2


def func_start(sec, off, dest):
    """The nearest `addiu sp, sp, -N` at or before `off`, within 8 KiB: a function's first word, as a guess."""
    for k in range(off & ~3, max(-1, (off & ~3) - 0x2000), -4):
        w, = struct.unpack_from("<I", sec, k)
        if w & 0xFFFF8000 == 0x27BD8000:
            return dest + k
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("orig")
    ap.add_argument("patched")
    ap.add_argument("--out", required=True)
    ap.add_argument("--build", default="psx-us", help="ORIG's fixtures.toml id, for exe_maps/ (default psx-us)")
    ap.add_argument("--twins", help="a folder with region_twin_map.py's us_jp.tsv and pc_map.tsv")
    ap.add_argument("--census", default=os.path.join(ROOT, "analysis", "dat_census.json"),
                    help="tools/dat_census.py's JSON: which EMI sections the PC kept")
    a = ap.parse_args()
    try:
        import capstone
        md = capstone.Cs(capstone.CS_ARCH_MIPS, capstone.CS_MODE_MIPS32 | capstone.CS_MODE_LITTLE_ENDIAN)
    except ImportError:
        raise SystemExit("disc_patch_census needs capstone (pip install capstone)")
    o, n = psx_disc.Disc(a.orig), psx_disc.Disc(a.patched)
    maps = load_maps(a.build)
    data_syms, func_syms = load_symbols()
    funcs, pcs = load_twins(a.twins)
    by_sha = target_sections(o, funcs)
    census = {}
    if os.path.isfile(a.census):
        with open(a.census) as f:
            census = json.load(f)
    else:
        print("no %s: kept and dropped sections are not told apart (tools/dat_census.py writes it)" % a.census)

    moved = sum(1 for p in o.files if p in n.files and o.files[p][0] != n.files[p][0])
    print("files: %d in ORIG, %d in PATCHED; %d only in ORIG, %d only in PATCHED; %d at another LBA" % (
        len(o.files), len(n.files), len(set(o.files) - set(n.files)), len(set(n.files) - set(o.files)), moved))

    rows = []
    seen = {}
    changed_files = 0

    def classify(target, mkey, sec, dest, a0, a1):
        """(is, name) for the run [a0, a1) of a section that is not a DAT chunk."""
        addr = dest + a0
        if target:
            for us, size, name in funcs[target]:
                if us <= addr < us + size or (a0 == a1 and us < addr <= us + size):
                    pc = pcs.get((target, us))
                    twin = " = PC 0x%X %s" % (pc, func_syms.get(pc, "")) if pc else " (no PC twin recorded)"
                    return "code", "%s+0x%X%s" % (name, addr - us, twin.rstrip())
        for ms, me, pc in maps.get(mkey, ()):
            if ms <= addr < me:
                at = pc + addr - ms
                return "data", ("PC 0x%X %s" % (at, data_name(data_syms, at))).rstrip()
        if target:
            return "data", "outside every function of the section; exe_maps does not place it"
        if looks_like_code(md, sec, a0):
            fs = func_start(sec, a0, dest)
            return "code?", "function at 0x%08X?" % fs if fs else ""
        return "data?", ""

    for p in sorted(o.files):
        if p not in n.files:
            continue
        ob, nb = o.read(p), n.read(p)
        if ob == nb:
            continue
        changed_files += 1
        rel = p.split("/", 1)[1] if p.startswith("BIN/") else p
        stem = p.rsplit("/", 1)[-1].rsplit(".", 1)[0]
        if p.endswith(".EMI") and ob[8:16] == b"MATH_TBL" and nb[8:16] == b"MATH_TBL":
            so, sn = emi_sections(ob), emi_sections(nb)
            if len(so) != len(sn):
                rows.append([rel, "-", 1, "", "", "file", "", len(ob), len(nb), "-", "section count %d -> %d" % (len(so), len(sn))])
                continue
            c = census.get(stem)
            kept = {pr["emi"]: pr for pr in c["pairs"]} if c else {}
            for (i, kind, dest, da), (_, kind2, dest2, db) in zip(so, sn):
                if da == db and kind == kind2 and dest == dest2:
                    continue
                key = (dest, hashlib.sha1(da).digest(), hashlib.sha1(db).digest())
                if key in seen:
                    for r in seen[key]:
                        r[2] += 1
                    continue
                if i in kept:
                    where = "kept (dat %d kind %d)" % (kept[i]["dat"], kept[i]["kind"])
                elif not census:
                    where = "?"
                elif c is None or not c["dat_chunks"]:
                    where = "none"
                else:
                    where = "dropped"
                target = by_sha.get(key[1])
                mine = []
                if kind in (3, 7) or dest < 0x80000000:      # an image or sound: not aligned, one row
                    ch = sum(1 for x, y in zip(da, db) if x != y)
                    mine.append([rel, i, 1, "%08X" % dest, kind, where, "", len(da), len(db), "-", "%d bytes differ" % ch])
                else:
                    runs = edits(da, db)
                    shifts = moved_by(runs)
                    for a0, a1, b0, b1 in runs:
                        what, name = ("-", "") if where.startswith("kept") else classify(target, (rel.upper(), i), da, dest, a0, a1)
                        by = None if where.startswith("kept") else relocation(da, db, a0, a1, b0, b1, shifts)
                        if by is not None:
                            what, name = "moved", ("an address that follows the section's bytes moving by %+d; in %s: %s"
                                                   % (by, what, name)).rstrip(": ")
                        mine.append([rel, i, 1, "%08X" % dest, kind, where, "%08X" % (dest + a0), a1 - a0, b1 - b0, what, name])
                    if dest != dest2 or kind != kind2:
                        mine.append([rel, i, 1, "%08X" % dest, kind, where, "", 0, 0, "-", "header: dest %08X kind %d" % (dest2, kind2)])
                seen[key] = mine
                rows += mine
        else:
            boot = p.startswith(("SLUS", "SLPS", "SLES", "SCUS", "SCES", "SCPS")) and ob[:8] == b"PS-X EXE"
            if boot:
                t_addr, = struct.unpack_from("<I", ob, 0x18)
                head = edits(ob[:0x800], nb[:0x800])
                for a0, a1, b0, b1 in head:
                    rows.append([rel, "-", 1, "", "", "boot", "header+0x%X" % a0, a1 - a0, b1 - b0, "data", "the EXE header"])
                body_o, body_n = ob[0x800:], nb[0x800:]
                for a0, a1, b0, b1 in edits(body_o, body_n):
                    what, name = classify("exe/slus_004_22" if "exe/slus_004_22" in funcs and a.build == "psx-us" else None,
                                          ("BOOT", None), body_o, t_addr, a0, a1)
                    rows.append([rel, "-", 1, "%08X" % t_addr, "", "boot", "%08X" % (t_addr + a0), a1 - a0, b1 - b0, what, name])
            else:
                for a0, a1, b0, b1 in edits(ob, nb):
                    rows.append([rel, "-", 1, "", "", "file", "+0x%X" % a0, a1 - a0, b1 - b0, "-", ""])

    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w", newline="\n") as f:
        f.write("file\tsection\tcopies\tdest\tkind\twhere\taddr\told\tnew\tis\tname\n")
        for r in rows:
            f.write("\t".join(str(x) for x in r) + "\n")

    print("%d files differ; %d rows (edit runs of distinct sections) -> %s" % (changed_files, len(rows), a.out))
    tot = collections.OrderedDict()
    for r in rows:
        k = (r[5].split(" (")[0], r[9])
        e = tot.setdefault(k, [0, 0, 0])
        e[0] += 1
        e[1] += r[7]
        e[2] += r[8]
    print("%-10s %-6s %6s %9s %9s" % ("where", "is", "runs", "old bytes", "new bytes"))
    for (where, what), (runs, old, new) in sorted(tot.items()):
        print("%-10s %-6s %6d %9d %9d" % (where, what, runs, old, new))
    names = collections.Counter()
    for r in rows:
        if r[9].startswith("code"):
            names[(r[0], r[10].split("+0x")[0] if r[9] == "code" else r[10])] += 1
    print("%d functions hold code edits (%d of them named by --twins)" % (
        len(names), sum(1 for (f, nm) in names if not nm.endswith("?") and nm)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
