"""Summarise BOF3X_STPCENSUS files (src/hook/stp_census.h, docs/stp-census.md).

    python tools/stp_census.py census_a.tsv [census_b.tsv ...] [--top N]

Each file: one row per (area, code, tpage, CLUT, texture window) of the
semi-transparent textured primitives a run drew, with how many of the texels
they sample are transparent (0), bit 15 set (blended on the PlayStation) or
bit 15 clear and not 0 (opaque on the PlayStation, blended by the port). The
summary says, per file and in all, how many draws would change if the port
blended only bit-15 texels, by blend mode and by kind, and lists the rows
with the most changed draws.
"""
import argparse
import collections
import csv
import os

BLEND = {0: "0 half", 1: "1 add", 2: "2 sub", 3: "3 quarter-add"}
KIND = {0x24: "FT3", 0x2C: "FT4", 0x34: "GT3", 0x3C: "GT4", 0x64: "SPRT", 0x74: "SPRT8", 0x7C: "SPRT16",
        0x84: "cell"}


def read(path):
    with open(path, encoding="utf-8") as f:
        head = f.readline().strip()
        rows = list(csv.DictReader(f, delimiter="\t"))
    for r in rows:
        for k in ("area", "draws", "frames", "first", "last", "tex_zero", "tex_stp", "tex_opaque", "draws_stp_only",
                  "draws_opaque_only", "draws_mixed", "draws_clear", "blend", "mode"):
            r[k] = int(r[k])
        r["code_n"] = int(r["code"], 16)
        r["changed"] = r["draws_opaque_only"] + r["draws_mixed"]
    return head, rows


def table(title, groups):
    print(f"\n  {title}")
    print(f"    {'':16} {'draws':>9} {'stp only':>9} {'opq only':>9} {'mixed':>9} {'clear':>7}  changed")
    for name, g in sorted(groups.items()):
        d = sum(r["draws"] for r in g)
        ch = sum(r["changed"] for r in g)
        print(f"    {name:16} {d:9} {sum(r['draws_stp_only'] for r in g):9} "
              f"{sum(r['draws_opaque_only'] for r in g):9} {sum(r['draws_mixed'] for r in g):9} "
              f"{sum(r['draws_clear'] for r in g):7}  {100 * ch / d if d else 0:5.1f}%")


def summarise(name, head, rows, top):
    print(f"== {name}: {head.lstrip('# ')}")
    table("by blend mode", collections.defaultdict(list, {
        b: [r for r in rows if r["blend"] == k] for k, b in BLEND.items() if any(r["blend"] == k for r in rows)}))
    kinds = collections.defaultdict(list)
    for r in rows:
        kinds[KIND.get(r["code_n"] & 0xFC, hex(r["code_n"]))].append(r)
    table("by kind", kinds)
    areas = collections.defaultdict(list)
    for r in rows:
        areas[f"area {r['area']}"].append(r)
    table("by area", areas)
    changed = sorted((r for r in rows if r["changed"]), key=lambda r: -r["changed"])
    print(f"\n  rows with changed draws: {len(changed)} of {len(rows)}; the top {top}:")
    print("    area code tpage blend clut  window          draws changed  texels stp/opaque/zero   frames first..last uv")
    for r in changed[:top]:
        print(f"    {r['area']:4} {r['code']:>4} {r['tpage']:>5} {r['blend']:5} {r['clut']:>5} {r['window']:15} "
              f"{r['draws']:6} {r['changed']:7}  {r['tex_stp']:>8}/{r['tex_opaque']:>8}/{r['tex_zero']:>8} "
              f"{r['frames']:6} {r['first']}..{r['last']} {r['uv_box']}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+")
    ap.add_argument("--top", type=int, default=15)
    a = ap.parse_args()
    every = []
    for path in a.files:
        head, rows = read(path)
        summarise(os.path.basename(path), head, rows, a.top)
        every += rows
        print()
    if len(a.files) > 1:
        table("all files, by blend mode", collections.defaultdict(list, {
            b: [r for r in every if r["blend"] == k] for k, b in BLEND.items() if any(r["blend"] == k for r in every)}))


if __name__ == "__main__":
    main()
