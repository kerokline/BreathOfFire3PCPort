#!/usr/bin/env python3
"""Read BOF3X_TASKCENSUS files (src/hook/task_census.cpp; docs/state-by-name-round.md 2).

    python3 tools/task_census.py --elf build-linux-x86_64/bof3x RUN.tsv [RUN.tsv ...]
    python3 tools/task_census.py --elf ... --tsv OUT.tsv RUN.tsv ...

A census file has one row per (task, chain of return addresses) with the frames
the task stood there at the frame boundary, and one row per combination of the
four tasks' sites. This names each address by the function of ours that holds
it (`nm` on the ELF: the Linux build is linked at a fixed address, so a run's
addresses are the file's), and prints, per run:

  - each site: frames, stack depth, the function that called Task_Sleep and the
    two candidates above it (a candidate above a task's entry frame is stale:
    the depth is the measure, the names are a reading aid);
  - with --flat NAME[,NAME...], the frames by how every live task stood: all in
    one of the named functions ("flat": a place a resume entry can stand for),
    or at least one elsewhere ("nested"). A task created and not yet run
    ("fresh") counts as flat: its place is its entry.

Over all runs it prints each yielding function once, with its frames, the runs
that reach it and its depths. Which functions are flat is a reading of the
source, not of the depth: the list is the caller's.

Addresses of our code, names, counts and depths only: no game data.
"""
import argparse
import bisect
import collections
import os
import subprocess
import sys


def symbols(elf):
    out = subprocess.run(["nm", "-C", "--defined-only", elf], check=True, capture_output=True, text=True).stdout
    table = []
    for line in out.splitlines():
        parts = line.split(" ", 2)
        if len(parts) == 3 and parts[1] in "tTwW":
            table.append((int(parts[0], 16), parts[2]))
    table.sort()
    return [a for a, _ in table], [n for _, n in table]


def name_of(addrs, names, a):
    i = bisect.bisect_right(addrs, a) - 1
    if i < 0:
        return f"{a:#x}"
    return f"{names[i]}+{a - addrs[i]:#x}"


def read(path):
    sites, frames, meta = {}, [], {}
    for line in open(path, encoding="utf-8"):
        f = line.rstrip("\n").split("\t")
        if f[0] == "#module":
            meta = {"base": int(f[1], 16), "extent": int(f[2], 16), "frames": int(f[4]), "slice": int(f[6], 16)}
        elif f[0] == "site":
            sites[(int(f[1]), int(f[2]))] = {
                "frames": int(f[3]), "first": int(f[4]), "min": int(f[5], 16), "max": int(f[6], 16),
                "fresh": int(f[7]), "modes": f[8], "chain": [int(x, 16) for x in f[9].split()]}
        elif f[0] == "frame":
            frames.append(([int(x) for x in f[1:5]], int(f[5])))
    return meta, sites, frames


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", required=True, help="the ELF the runs were made with")
    ap.add_argument("--flat", default="", help="functions whose own yield a resume entry can stand for, by name")
    ap.add_argument("--tsv", help="also write every site of every run as one table")
    ap.add_argument("runs", nargs="+")
    a = ap.parse_args()
    addrs, names = symbols(a.elf)
    flat_names = {x for x in a.flat.split(",") if x}
    rows = []
    total = collections.Counter()
    by_fn = {}
    for path in a.runs:
        meta, sites, frames = read(path)
        route = os.path.splitext(os.path.basename(path))[0]
        lo, hi = meta["base"], meta["base"] + meta["extent"]
        print(f"== {route}: {meta['frames']} frames, slice {meta['slice']:#x}")
        kind = {}
        for (task, sid), s in sorted(sites.items(), key=lambda kv: (kv[0][0], -kv[1]["frames"])):
            chain = [name_of(addrs, names, x) if lo <= x < hi else f"{x:#x}" for x in s["chain"][:3]]
            fn = chain[0].rsplit("+", 1)[0]
            fresh = s["fresh"] == s["frames"]
            k = "fresh" if fresh else ("flat" if fn in flat_names else "nested")
            kind[(task, sid)] = k
            print(f"  task {task} site {sid:>3} {k:<6} {s['frames']:>7} frames  depth {s['min']:#x}..{s['max']:#x}  "
                  f"modes {s['modes']:<10} {' < '.join(chain)}")
            rows.append((route, task, sid, k, s["frames"], s["first"], s["min"], s["max"], s["modes"], " < ".join(chain)))
            if not fresh:
                e = by_fn.setdefault((task, fn), {"frames": 0, "routes": set(), "min": s["min"], "max": s["max"]})
                e["frames"] += s["frames"]
                e["routes"].add(route)
                e["min"], e["max"] = min(e["min"], s["min"]), max(e["max"], s["max"])
        tally = collections.Counter()
        for ids, n in frames:
            ks = [kind[(t, i)] for t, i in enumerate(ids) if i >= 0]
            tally["nested" if "nested" in ks else "flat"] += n
        seen = sum(tally.values())
        total.update(tally)
        if flat_names:
            print(f"  frames with every live task flat: {tally['flat']} of {seen} ({100.0 * tally['flat'] / seen:.1f} %); "
                  f"with a task nested: {tally['nested']} ({100.0 * tally['nested'] / seen:.1f} %)")
    seen = sum(total.values())
    print(f"== all runs: {seen} frames, {len(a.runs)} runs")
    for (task, fn), e in sorted(by_fn.items(), key=lambda kv: (kv[0][0], -kv[1]["frames"])):
        print(f"  task {task} {'flat  ' if fn in flat_names else 'nested'} {e['frames']:>7} frames  {len(e['routes']):>2} runs  "
              f"depth {e['min']:#x}..{e['max']:#x}  {fn}")
    if flat_names:
        print(f"  frames with every live task flat: {total['flat']} ({100.0 * total['flat'] / seen:.1f} %); "
              f"with a task nested: {total['nested']} ({100.0 * total['nested'] / seen:.1f} %)")
    if a.tsv:
        with open(a.tsv, "w", encoding="utf-8", newline="\n") as f:
            f.write("# tools/task_census.py: where each task stood at the frame boundary, per run. Names of our "
                    "functions, counts and depths only (docs/state-by-name-round.md 2).\n")
            f.write("#route\ttask\tsite\tkind\tframes\tfirst_frame\tmin_depth\tmax_depth\tmodes\tchain\n")
            for r in rows:
                f.write("\t".join(str(x) if not isinstance(x, int) or i not in (6, 7) else f"{x:#x}"
                                  for i, x in enumerate(r)) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
