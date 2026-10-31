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
rule 1); analysis/ is gitignored.

    python tools/exe_scan.py --accepted [GAPS ACCEPTED]

The accounting (docs/exe-gap-round.md 4, group AC; no game data, a CI check):
every `how = gap` row of recipes/exe-gaps.tsv has exactly one row in
recipes/exe-gaps-accepted.tsv with the same build, lo, hi and symbol, every
accepted row has its gap, each answer is one of layer / port-own / unread /
pending and each evidence is non-empty. Prints the counts per answer per
build; exits 1 on any failure, naming the rows. A placement that removes or
narrows a gap fails here until its accepted row is dropped or redrawn, and a
regeneration that adds one fails until it is accounted."""
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
GAPS = os.path.join(ROOT, "recipes", "exe-gaps.tsv")
ACCEPTED = os.path.join(ROOT, "recipes", "exe-gaps-accepted.tsv")
ANSWERS = ("layer", "port-own", "unread", "pending")
BUILDS = ("psx-jp", "psx-us", "psx-eu-en", "psx-fr", "psx-de", "psp-jp", "psp-eu")
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


def _tsv_rows(path, columns):
    """(line number, fields) of a tab-separated recipe, past its comments and header."""
    rows = []
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            if fields[0] == "build":
                if fields != list(columns):
                    raise SystemExit("%s:%d: the header is %s, expected %s" % (path, n, fields, list(columns)))
                continue
            rows.append((n, fields))
    return rows


def check_accepted(gaps_path=GAPS, accepted_path=ACCEPTED):
    """The accounting check (module doc): a list of failures, and the counts
    {build: Counter(answer)} of the accepted rows that matched a gap."""
    errors = []
    gaps = collections.OrderedDict()
    for n, f in _tsv_rows(gaps_path, ("build", "lo", "hi", "how", "kind", "symbol")):
        if len(f) != 6:
            errors.append("%s:%d: %d columns, expected 6" % (os.path.basename(gaps_path), n, len(f)))
            continue
        if f[3] == "gap":
            gaps[(f[0], f[1], f[2], f[5])] = n
    accepted = collections.OrderedDict()
    for n, f in _tsv_rows(accepted_path, ("build", "lo", "hi", "symbol", "answer", "evidence")):
        where = "%s:%d" % (os.path.basename(accepted_path), n)
        if len(f) != 6:
            errors.append("%s: %d columns, expected 6" % (where, len(f)))
            continue
        key = tuple(f[:4])
        if key in accepted:
            errors.append("%s: %s %s..%s %s is accounted twice (first at line %d)" % ((where,) + key + (accepted[key][0],)))
            continue
        accepted[key] = (n, f[4], f[5])
        if f[4] not in ANSWERS:
            errors.append("%s: answer %r is not one of %s" % (where, f[4], " / ".join(ANSWERS)))
        if not f[5].strip():
            errors.append("%s: %s %s..%s %s has no evidence" % ((where,) + key))
    by_range = {(k[0], k[1], k[2]): k[3] for k in accepted}
    for key, n in gaps.items():
        if key not in accepted:
            hint = ""
            other = by_range.get(key[:3])
            if other is not None:
                hint = " (accepted under the symbol %s: renamed?)" % other
            errors.append("%s:%d: gap %s %s..%s %s has no accepted row%s" % ((os.path.basename(gaps_path), n) + key + (hint,)))
    for key, (n, _, _) in accepted.items():
        if key not in gaps:
            errors.append("%s:%d: %s %s..%s %s is no longer a gap (placed or redrawn): "
                          "drop or redraw the row" % ((os.path.basename(accepted_path), n) + key))
    counts = collections.OrderedDict((b, collections.Counter()) for b in BUILDS)
    for key, (_, answer, _) in accepted.items():
        if key in gaps:
            counts.setdefault(key[0], collections.Counter())[answer] += 1
    return errors, counts


def main_accepted(paths):
    gaps_path, accepted_path = paths if paths else (GAPS, ACCEPTED)
    errors, counts = check_accepted(gaps_path, accepted_path)
    print("%-10s %s  %s" % ("build", "  ".join("%8s" % a for a in ANSWERS), "   total"))
    total = collections.Counter()
    for build, c in counts.items():
        total.update(c)
        print("%-10s %s  %8d" % (build, "  ".join("%8d" % c[a] for a in ANSWERS), sum(c.values())))
    print("%-10s %s  %8d" % ("all", "  ".join("%8d" % total[a] for a in ANSWERS), sum(total.values())))
    for e in errors:
        print("NEEDS FIX: " + e)
    print("exe-gaps accepted: %d rows accounted, %d failure(s)" % (sum(total.values()), len(errors)))
    return 1 if errors else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--accepted", nargs="*", metavar="TSV",
                    help="check recipes/exe-gaps-accepted.tsv against recipes/exe-gaps.tsv (or GAPS ACCEPTED) and exit")
    ap.add_argument("--game", help="the PC game folder (BOF3.exe)")
    ap.add_argument("--disc", action="append", help="a disc image, .cue / .iso or a .zip of one")
    ap.add_argument("--out", default=os.path.join(ROOT, "analysis", "exe_scan"))
    a = ap.parse_args()
    if a.accepted is not None:
        if len(a.accepted) not in (0, 2):
            ap.error("--accepted takes no path or two (GAPS ACCEPTED)")
        sys.exit(main_accepted(a.accepted))
    if not a.game or not a.disc:
        ap.error("--game and --disc are required for a scan")
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
