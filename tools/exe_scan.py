"""The exe image a one-source install runs on, against the PC's .data, byte by
byte (docs/install-gap-census.md section 8, 2026-10-17).

For each disc: base/exe/ built exactly as a disc-only setup builds it
(exe_tables.build_from), then laid out as the engine lays it at start
(exe_image.cpp ExeImage_Source): the image's bytes; `own` ranges filled by the
engine (ExeOwn_Fill, which the pc-zh image proves equal to the PC's); `keys`
by the engine's rule (equal); `language`, `unread`, `gap` and `none` left zero.
Then every byte of the PC's initialised .data is one of:

  equal          the same byte
  differ/<how>   a placed byte with the disc's own value (map, table, widen, ...)
  missing/<how>  zero where the PC has a value, by data.toml's class: `gap` (no
                 disc carries it), `language` (a language layer may write it),
                 `unread`, and `none` - which must never happen (the recipe
                 says the PC's are zero there)
  extra          non-zero where the PC has zero (a disc's own value)

    python tools/exe_scan.py --game bof3 --disc ZIP_OR_CUE ... [--out analysis/exe_scan]

Writes <out>/<build>.tsv (per symbol: the counts) and <out>/summary.tsv, and
prints the summary. Addresses, classes, symbols and counts only (CLAUDE.md
rule 1); analysis/ is gitignored."""
import argparse
import collections
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exe_tables      # noqa: E402
import player_sources  # noqa: E402
import importer        # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILLED_EQUAL = ("own", "keys")           # the engine writes the PC's bytes there at start


def read_ranges(toml_path):
    import tomllib
    with open(toml_path, "rb") as f:
        t = tomllib.load(f)
    return t["image"]["build"], t["image"]["ranges"]


def scan_one(pc, img, ranges, syms):
    cls = [None] * len(pc)
    frm = [""] * len(pc)
    for lo, hi, how, what in ranges:
        for x in range(lo, hi):
            cls[x - exe_tables.DATA_LO] = how
            frm[x - exe_tables.DATA_LO] = what
    eff = bytearray(img)
    for i, how in enumerate(cls):
        if how in FILLED_EQUAL:
            eff[i] = pc[i]
    total = collections.Counter()
    by_sym = collections.defaultdict(collections.Counter)
    for i in range(len(pc)):
        how = cls[i]
        if eff[i] == pc[i]:
            k = "equal"
        elif eff[i] == 0:
            k = "missing/" + how
        elif pc[i] == 0:
            k = "extra/" + how
        else:
            k = "differ/" + how
        total[k] += 1
        if k != "equal":
            name, _ = exe_tables.symbol_at(syms, exe_tables.DATA_LO + i)
            by_sym[(name, frm[i] if how in ("gap", "language", "unread", "none") else "")][k] += 1
    return total, by_sym


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", required=True, help="the PC game folder (BOF3.exe)")
    ap.add_argument("--disc", action="append", required=True, help="a disc image, .cue / .iso or a .zip of one")
    ap.add_argument("--out", default=os.path.join(ROOT, "analysis", "exe_scan"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    pc, _ = exe_tables.pc_data(os.path.join(a.game, "BOF3.exe"))
    syms = exe_tables.data_symbols()
    summary = {}
    for path in a.disc:
        tmp = tempfile.mkdtemp(prefix="bof3x-scan-")
        try:
            src = path
            if path.lower().endswith(".zip"):
                src, _ = player_sources.unpack_zip(path, tmp)
            bid = importer.open_source(src).id
            cache = os.path.join(tmp, "cache")
            exe_tables.build_from(src, bid, cache)
            build, ranges = read_ranges(os.path.join(cache, "base", "exe", "data.toml"))
            with open(os.path.join(cache, "base", "exe", "data.bin"), "rb") as f:
                img = f.read()
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
        total, by_sym = scan_one(pc, img, ranges, syms)
        summary[bid] = total
        with open(os.path.join(a.out, bid + ".tsv"), "w", newline="\n") as f:
            keys = sorted({k for c in by_sym.values() for k in c})
            f.write("symbol\tfrom\t" + "\t".join(keys) + "\n")
            pcs, names = syms
            order = lambda s: pcs[names.index(s[0])] if s[0] in names else 0
            for s in sorted(by_sym, key=order):
                f.write("%s\t%s\t%s\n" % (s[0], s[1], "\t".join(str(by_sym[s][k]) for k in keys)))
        print("%-10s %s" % (bid, "  ".join("%s %d" % kv for kv in sorted(total.items()))), flush=True)
    keys = sorted({k for t in summary.values() for k in t})
    with open(os.path.join(a.out, "summary.tsv"), "w", newline="\n") as f:
        f.write("build\t" + "\t".join(keys) + "\n")
        for bid, t in summary.items():
            f.write("%s\t%s\n" % (bid, "\t".join(str(t[k]) for k in keys)))
    print("wrote %s" % a.out)


if __name__ == "__main__":
    main()
