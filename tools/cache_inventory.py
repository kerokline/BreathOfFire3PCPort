#!/usr/bin/env python
"""The disc-data inventory: every base/ chunk whose PC form differs from a disc's.

docs/disc-data-inventory.md. The owner prefers the PlayStation's data where
both the PC install and a disc carry a chunk (HANDOFF item 3, 2026-10-12);
today a PC + disc cache takes the PC's chunk wherever the PC has one
(importer.get_chunk: the player's source order decides). This tool writes
the raw difference table that pass classifies, so the pass is repeatable:

    python tools/cache_inventory.py dat --source DAT --source DISC [--source DISC ...]
                                        [--disc BUILD ...] [--cache CACHE] [--banks] [--all] --out TSV
    python tools/cache_inventory.py exe --exe BOF3.exe --source DISC [--source DISC ...] --out TSV

`dat`: for every chunk recipes/pc-zh.toml puts in `base/`, and every disc
build given (or each `--disc`), the disc's own form of that chunk as the
recipe and its stand-ins find it - the section byte for byte (`same`), the
type-1 section decoded (`same-type1`), the section through one of the port's
rules (`rule:icons`, `rule:ryud`, `rule:hold`: the raw section is compared,
before the rule), the enemy table at the disc's stride (`format-widen`), the
disc's own section standing in (`own:<why>`, importer.OWN_WHY), a PSP's own
section (`psp:<why>`, recipes/psp-standins.toml), a bank by wave-from-vag
(`derived-vag` with `--banks`; else `bank`), or `none`. One row per chunk and
disc where the form is not `same` / `same-type1` / `derived-vag` (every row
with `--all`): the PC's size and the disc form's, the bytes that differ and
their first and last offsets, for an image the 0x800-byte tiles that differ,
and with `--cache` what that cache took (its manifest's build:how).

`exe`: the base/exe/ image a disc alone gives (exe_tables.disc_built, the
image the importer writes) against BOF3.exe's .data, every byte the disc
places that differs from the PC's, in runs by symbol and by how it was placed.
The bytes a disc does not carry (gaps, pointer words not rebuilt) are not
differences: no disc form exists there, so a PC + disc cache can only take
the PC's; their count per build is in docs/exe-import.md section 3.

The table holds names, offsets, counts and hashes - no game bytes - but it is
derived from the player's files: write it under analysis/ (gitignored) or
scratch, never into the tree (CLAUDE.md rule 1). Sources are verified by
importer.identify as for a build.
"""
import argparse
import collections
import os
import sys
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import importer      # noqa: E402
import type1         # noqa: E402

TILE = 0x800
SAME = ("same", "same-type1", "derived-vag")
COLS = ["file", "slot", "kind", "tag", "class", "own_why", "disc", "form", "pc_size", "disc_size",
        "bytes_differ", "first", "last", "tiles_differ", "cache_took", "what"]


def out_path(p):
    o = os.path.abspath(p)
    if o.startswith(ROOT + os.sep) and not o.startswith(os.path.join(ROOT, "analysis") + os.sep):
        raise SystemExit("--out inside the repository must be under analysis/ (gitignored, CLAUDE.md rule 1)")
    d = os.path.dirname(o)
    if d:
        os.makedirs(d, exist_ok=True)
    return o


def diff(a, b, kind):
    """(bytes differing, first, last, tiles differing) of two payloads; the
    byte counts only when the sizes agree."""
    if a is None or b is None or len(a) != len(b):
        return "", "", "", ""
    at = [i for i in range(len(a)) if a[i] != b[i]]
    if not at:
        return 0, "", "", 0 if kind == 1 else ""
    tiles = len({i // TILE for i in at}) if kind == 1 else ""
    return len(at), "0x%X" % at[0], "0x%X" % at[-1], tiles


_psp_table = None


def psp_row(f, slot):
    global _psp_table
    if _psp_table is None:
        import psp_standins
        _psp_table = psp_standins.load()
    return _psp_table.get((f["name"], slot))


def disc_form(s, f, slot, ch, banks):
    """(form, the disc's raw payload or None) of one recipe chunk on one disc."""
    for b, how, key, sec in importer.sources_of(ch):
        if b != s.id:
            continue
        raw = s.sections(key)[sec][3]
        if how == "copy":
            return "same", raw
        if how == "type1":
            return "same-type1", type1.decode(raw)
        if how == "widen":
            return "format-widen", raw
        if how == "hold" and s.id.startswith("psx-"):
            return "rule:hold", type1.decode(raw)
        return "rule:" + how, raw
    for b, key, sec, h in importer.owns_of(ch):
        if b != s.id:
            continue
        raw = s.sections(key)[sec][3]
        if ch.get("own_why") in importer.OWN_TYPE1:
            raw = type1.decode(raw)
        return "own:" + ch["own_why"], raw
    if ch["class"] == "bank":
        if not banks:
            return "bank", None
        import vag
        got = vag.importer_source(s, f, slot, ch)
        return ("derived-vag", got[2]) if got else ("bank-differs", None)
    if s.id in importer.PSP:
        import psp_standins
        row = psp_row(f, slot)
        if row and row["class"] == ch["class"]:
            for b, how, key, sec, h in map(psp_standins.parse_on, row["on"]):
                if b == s.id:
                    return "psp:" + row.get("why", {}).get(b, how), s.sections(key)[sec][3]
    return "none", None


def cache_took(cache):
    """{(file, slot): "build:how"} for base/ chunks, from a cache's manifest."""
    if not cache:
        return {}
    with open(os.path.join(cache, "manifest.toml"), "rb") as fh:
        m = tomllib.load(fh)
    return {(a[0], a[1]): a[3] for a in m["cache"]["assets"] if a[2] == "base"}


def cmd_dat(a):
    out = out_path(a.out)
    rec = importer.load_recipe(a.recipe)
    srcs = [importer.open_source(p) for p in a.source]
    pc = next((s for s in srcs if isinstance(s, importer.PcSource)), None)
    if pc is None:
        raise SystemExit("give the PC's DAT/ as a --source: the inventory compares its chunks")
    discs = [s for s in srcs if isinstance(s, importer.DiscSource)]
    if a.disc:
        discs = [s for s in discs if s.id in a.disc]
        missing = set(a.disc) - {s.id for s in discs}
        if missing:
            raise SystemExit("--disc %s: not among the sources" % ", ".join(sorted(missing)))
    if not discs:
        raise SystemExit("no disc among the sources")
    took = cache_took(a.cache)
    rows, tally = [], collections.Counter()
    for f in rec["file"]:
        for slot, ch in enumerate(f["chunks"]):
            if ch["layer"] != "base":
                continue
            body = pc.get(f["name"], slot, ("chunk", None, None))
            if importer.sha(body) != ch["sha256"]:
                raise SystemExit("%s chunk %d: the PC's hash differs from the recipe" % (f["name"], slot))
            for s in discs:
                form, raw = disc_form(s, f, slot, ch, a.banks)
                tally[(s.id, ch["class"], form)] += 1
                if form in SAME and not a.all:
                    continue
                n, lo, hi, tiles = diff(body, raw, ch["kind"])
                rows.append([f["name"], slot, ch["kind"], "0x%08X" % ch["tag"], ch["class"], ch.get("own_why", ""),
                             s.id, form, len(body), "" if raw is None else len(raw), n, lo, hi, tiles,
                             took.get((f["name"], slot), ""), ch.get("what", "")])
    with open(out, "w", newline="\n", encoding="utf-8") as fh:
        fh.write("# tools/cache_inventory.py dat - every base/ chunk whose PC form differs from a disc's\n")
        fh.write("# sources: %s\n" % ", ".join(s.id for s in srcs))
        fh.write("\t".join(COLS) + "\n")
        for r in rows:
            fh.write("\t".join(str(x) for x in r) + "\n")
    print("%d rows -> %s" % (len(rows), a.out))
    for (bid, cls, form), n in sorted(tally.items()):
        print("  %-9s %-12s %-22s %5d%s" % (bid, cls, form, n, "" if form not in SAME else "  (not listed)"))


def cmd_exe(a):
    import exe_tables
    import psx_disc
    out = out_path(a.out)
    pc, _ = exe_tables.pc_data(a.exe)
    syms = exe_tables.data_symbols()
    pointers = exe_tables.load_pointers()
    refused = exe_tables.load_rebuild_refused()
    rows, summary = [], []
    for p in a.source:
        ident = importer.identify(p)
        if not ident:
            raise SystemExit("%s: not a catalogued build" % p)
        bid = ident[0]
        img, owner, ranges, _, _ = exe_tables.disc_built(psx_disc.Disc(p), bid, pointers, refused[bid])
        by = collections.Counter()
        run = None
        for i in range(len(pc)):
            k = owner[i]
            hit = k is not None and img[i] != pc[i]
            if hit:
                name, off = exe_tables.symbol_at(syms, exe_tables.DATA_LO + i)
                how, where = ranges[k][2], ranges[k][3]
                by[how] += 1
                key = (name, how, where)
                if run and run[0] == key and run[2] >= i - 8:
                    run[2], run[3] = i, run[3] + 1
                    continue
                if run:
                    rows.append((bid, run))
                run = [key, i, i, 1, off]
        if run:
            rows.append((bid, run))
        summary.append((bid, sum(1 for x in owner if x is not None), sum(by.values()), dict(by)))
    with open(out, "w", newline="\n", encoding="utf-8") as fh:
        fh.write("# tools/cache_inventory.py exe - placed base/exe/ bytes that differ from BOF3.exe's .data\n")
        fh.write("disc\tsymbol\tsym_off\tpc\tpc_end\tbytes_differ\thow\tfrom\n")
        for bid, ((name, how, where), lo, hi, n, off) in rows:
            fh.write("%s\t%s\t0x%X\t0x%06X\t0x%06X\t%d\t%s\t%s\n" % (
                bid, name, off, exe_tables.DATA_LO + lo, exe_tables.DATA_LO + hi + 1, n, how, where))
    print("%d runs -> %s" % (len(rows), a.out))
    for bid, placed, n, by in summary:
        print("  %-9s %7d bytes placed, %5d differ from the PC's: %s" % (bid, placed, n, by))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    p = s.add_parser("dat")
    p.add_argument("--source", action="append", required=True, help="the PC's DAT/ and the discs (repeat)")
    p.add_argument("--disc", action="append", default=[], help="compare against these builds only (repeat)")
    p.add_argument("--cache", help="a cache built from these sources: its manifest says what it took")
    p.add_argument("--banks", action="store_true", help="derive every bank by wave-from-vag (slow)")
    p.add_argument("--all", action="store_true", help="list the chunks whose disc form is the PC's too")
    p.add_argument("--recipe")
    p.add_argument("--out", required=True)
    p = s.add_parser("exe")
    p.add_argument("--exe", required=True, help="the PC's BOF3.exe")
    p.add_argument("--source", action="append", required=True, help="a disc (repeat)")
    p.add_argument("--out", required=True)
    a = ap.parse_args()
    sys.exit({"dat": cmd_dat, "exe": cmd_exe}[a.cmd](a) or 0)


if __name__ == "__main__":
    main()
