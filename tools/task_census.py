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

The yield word (src/hook/task_yield.h; docs/state-by-name-sites.md). A file
written by a build with the yield word has, per row, the site the task carried
at those frames (0: none) and the build's site list. Then, with --sites, the
chain and the word are checked against each other, both ways:

  - a row is "flat" by its chain when Task_Sleep was called by Task_YieldAt
    and Task_YieldAt by a function listed for a site (SITE_FUNCTIONS below:
    the body, its loop and its resume entry; the names are the source's);
  - every frame on which each live task is flat by its chain or fresh must
    carry, for each such task, the site its chain names ("a flat frame with
    no site" otherwise), and every row that carries a site must be flat by
    its chain at that very site ("a site the chain does not agree with"); a
    fresh row (created, not yet run) must carry none;
  - it prints the frames per site and task, and every disagreement.

With --list-runs SITE[,SITE...], it also lists the runs of frames (by
Frame_Counter) on which some task carried one of those sites: where to take a
quicksave that stands there.

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


# The functions whose own yield each site stands for (task_yield.h kSites),
# by the name nm -C gives them with the namespace and the parameters taken
# off: the body, the loop the body and the resume entry share, and the resume
# entry (the compiler may inline a loop into either).
SITE_FUNCTIONS = {
    1: {"Field_Task", "FieldTaskLoop", "ResumeFieldTask"},
    2: {"TitleTask_Run", "TitleTaskRunLoop", "ResumeTitleTaskRun"},
    3: {"Title_Task", "TitleTaskLoop", "ResumeTitleTask"},
    4: {"Transition_FadeSub", "FadeSubLoop", "ResumeFadeSub"},
    5: {"Transition_FadeAdd", "FadeAddLoop", "ResumeFadeAdd"},
    6: {"SwingOut", "SwingOutLoop", "ResumeSwingOut"},
    7: {"SwingIn", "SwingInOpenLoop", "ResumeSwingInOpen"},
    8: {"SwingIn", "SwingInOpenLoop", "SwingInTurnLoop", "ResumeSwingInTurn"},
}
YIELD_AT = "Task_YieldAt"


def base_name(fn):
    """A function's name without its namespaces and parameters: what SITE_FUNCTIONS holds."""
    if "(" in fn and fn.endswith(")"):
        depth = 0
        for i in range(len(fn) - 1, -1, -1):
            depth += {")": 1, "(": -1}.get(fn[i], 0)
            if depth == 0:
                fn = fn[:i]
                break
    fn = fn.replace("(anonymous namespace)::", "")
    return fn.rsplit("::", 1)[-1]


def name_of(addrs, names, a):
    i = bisect.bisect_right(addrs, a) - 1
    if i < 0:
        return f"{a:#x}"
    return f"{names[i]}+{a - addrs[i]:#x}"


def read(path):
    sites, frames, meta, yields, runs = {}, [], {}, {}, []
    for line in open(path, encoding="utf-8"):
        f = line.rstrip("\n").split("\t")
        if f[0] == "#module":
            meta = {"base": int(f[1], 16), "extent": int(f[2], 16), "frames": int(f[4]), "slice": int(f[6], 16)}
        elif f[0] == "#yieldsite":
            yields[int(f[1])] = {"name": f[2], "bytes": int(f[3])}
        elif f[0] == "site":
            sites[(int(f[1]), int(f[2]))] = {
                "frames": int(f[3]), "first": int(f[4]), "min": int(f[5], 16), "max": int(f[6], 16),
                "fresh": int(f[7]), "modes": f[8], "chain": [int(x, 16) for x in f[9].split()],
                "yield": int(f[10]) if len(f) > 10 else None}
        elif f[0] == "frame":
            frames.append(([int(x) for x in f[1:5]], int(f[5])))
        elif f[0] == "run":
            runs.append({"first": int(f[1]), "frames": int(f[2]), "rows": [int(x) for x in f[3:7]],
                         "yields": [int(x) for x in f[7:11]]})
    return meta, sites, frames, yields, runs


def chain_site(fns):
    """The site a chain's own yield is at, by its names (0: none): Task_Sleep called by Task_YieldAt, called by a
    function listed for one site. A function listed for two (SwingIn's two loops when inlined) gives a set."""
    if len(fns) < 2 or base_name(fns[0]) != YIELD_AT:
        return set()
    return {sid for sid, names in SITE_FUNCTIONS.items() if base_name(fns[1]) in names}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", required=True, help="the ELF the runs were made with")
    ap.add_argument("--flat", default="", help="functions whose own yield a resume entry can stand for, by name")
    ap.add_argument("--tsv", help="also write every site of every run as one table")
    ap.add_argument("--sites", action="store_true", help="check the yield word against the chain (the A2 census)")
    ap.add_argument("--list-runs", default="", help="list the runs of frames that carry these site ids")
    ap.add_argument("runs", nargs="+")
    a = ap.parse_args()
    addrs, names = symbols(a.elf)
    flat_names = {x for x in a.flat.split(",") if x}
    want_runs = {int(x) for x in a.list_runs.split(",") if x}
    rows = []
    total = collections.Counter()
    by_fn = {}
    per_site = collections.Counter()      # (task, site) -> frames, over all runs
    disagree_total = collections.Counter()
    for path in a.runs:
        meta, sites, frames, ysites, runs = read(path)
        route = os.path.splitext(os.path.basename(path))[0]
        lo, hi = meta["base"], meta["base"] + meta["extent"]
        print(f"== {route}: {meta['frames']} frames, slice {meta['slice']:#x}")
        kind = {}
        for (task, sid), s in sorted(sites.items(), key=lambda kv: (kv[0][0], -kv[1]["frames"])):
            chain = [name_of(addrs, names, x) if lo <= x < hi else f"{x:#x}" for x in s["chain"][:3]]
            fn = chain[0].rsplit("+", 1)[0]
            if base_name(fn) == YIELD_AT and len(chain) > 1:   # the body that called Task_YieldAt is the place
                fn = chain[1].rsplit("+", 1)[0]
            fresh = s["fresh"] == s["frames"]
            if a.sites and not flat_names:   # flat by the chain: Task_YieldAt called by a site's function
                raw = [name_of(addrs, names, x).rsplit("+", 1)[0] if lo <= x < hi else f"{x:#x}" for x in s["chain"][:2]]
                is_flat = bool(chain_site(raw))
            else:
                is_flat = fn in flat_names
            k = "fresh" if fresh else ("flat" if is_flat else "nested")
            kind[(task, sid)] = k
            y = s["yield"]
            ytext = "" if y is None else f"yield {y}:{ysites.get(y, {}).get('name', '?') if y else '-'}  "
            print(f"  task {task} site {sid:>3} {k:<6} {s['frames']:>7} frames  depth {s['min']:#x}..{s['max']:#x}  "
                  f"modes {s['modes']:<10} {ytext}{' < '.join(chain)}")
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
        if a.sites:
            if any(s["yield"] is None for s in sites.values()):
                print("  no yield column: a file from a build without the yield word")
            else:
                full = {}
                for (task, sid), s in sites.items():
                    fns = [name_of(addrs, names, x).rsplit("+", 1)[0] if lo <= x < hi else f"{x:#x}" for x in s["chain"][:2]]
                    full[(task, sid)] = (chain_site(fns), s["fresh"] == s["frames"], s["fresh"])
                    if s["fresh"] != s["frames"] or s["yield"]:
                        per_site[(task, s["yield"])] += s["frames"] - (s["fresh"] if not s["yield"] else 0)
                bad = collections.Counter()
                for (task, sid), (cs, fresh, nfresh) in full.items():
                    y, n = sites[(task, sid)]["yield"], sites[(task, sid)]["frames"]
                    if fresh and y:
                        bad[f"task {task} row {sid}: created and not yet run, carrying site {y}"] += n
                    elif nfresh and y:
                        bad[f"task {task} row {sid}: carrying site {y} on {nfresh} frames it stood created"] += nfresh
                    elif y and y not in cs:
                        bad[f"task {task} row {sid}: site {y}, the chain names {sorted(cs) or 'no site'}"] += n
                flat_frames = carried = 0
                for ids, n in frames:
                    live = [(t, i) for t, i in enumerate(ids) if i >= 0]
                    if all(full[(t, i)][1] or full[(t, i)][0] for t, i in live):
                        flat_frames += n
                        miss = [(t, i) for t, i in live if not full[(t, i)][1] and sites[(t, i)]["yield"] not in full[(t, i)][0]]
                        if miss:
                            for t, i in miss:
                                bad[f"task {t} row {i}: flat by its chain at {sorted(full[(t, i)][0])}, carrying "
                                    f"{sites[(t, i)]['yield']}"] += n
                        else:
                            carried += n
                print(f"  the yield word: {flat_frames} frames flat by the chain, {carried} of them with every live task "
                      f"carrying its site; {sum(bad.values())} disagreeing (task, frame) counts")
                for text, n in sorted(bad.items()):
                    print(f"    DISAGREE {n:>6} frames  {text}")
                disagree_total.update(bad)
        if want_runs:
            for r in runs:
                hit = [(t, y) for t, y in enumerate(r["yields"]) if y in want_runs]
                if hit:
                    print(f"  run Frame_Counter {r['first']}..{r['first'] + r['frames'] - 1} ({r['frames']} frames): "
                          f"yields {r['yields']}")
        if flat_names or a.sites:
            print(f"  frames with every live task flat: {tally['flat']} of {seen} ({100.0 * tally['flat'] / seen:.1f} %); "
                  f"with a task nested: {tally['nested']} ({100.0 * tally['nested'] / seen:.1f} %)")
    seen = sum(total.values())
    print(f"== all runs: {seen} frames, {len(a.runs)} runs")
    for (task, fn), e in sorted(by_fn.items(), key=lambda kv: (kv[0][0], -kv[1]["frames"])):
        print(f"  task {task} {'flat  ' if fn in flat_names or base_name(fn) in set().union(*SITE_FUNCTIONS.values()) and a.sites else 'nested'} {e['frames']:>7} frames  {len(e['routes']):>2} runs  "
              f"depth {e['min']:#x}..{e['max']:#x}  {fn}")
    if flat_names or a.sites:
        print(f"  frames with every live task flat: {total['flat']} ({100.0 * total['flat'] / seen:.1f} %); "
              f"with a task nested: {total['nested']} ({100.0 * total['nested'] / seen:.1f} %)")
    if a.sites:
        print("  frames per task and yield word (0: none; created-and-not-yet-run frames left out):")
        for (task, y), n in sorted(per_site.items()):
            print(f"    task {task} site {y}: {n} frames")
        print(f"  disagreements over all runs: {sum(disagree_total.values())}")
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
