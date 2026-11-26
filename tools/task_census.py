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

With --callers, the nested rows (a task that is neither fresh nor, by its chain,
at a site) are tallied over all runs by their whole chain, named from the
yielding function up to the task body, innermost first, one return address a
name: where a nested yield's place is, caller by caller (B2,
docs/state-by-name-sites.md 8). Words above the body are stale and are cut at
the first task body (BODIES below).

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
# Field_Task's pass end is its own function since B2; the nested sites' resume entries make it too, and a
# compiler may inline it into any of them, so each can call Task_YieldAt for site 1.
NESTED_RESUME = {9: "ResumeWaitEnter", 10: "ResumeWaitToMenu", 11: "ResumeWaitArea", 12: "ResumeWaitMenuBack",
                 13: "ResumeMenuWaitOpen", 14: "ResumeMenuWaitClose", 15: "ResumeMenuLeaveSleep"}
SITE_FUNCTIONS[1] = SITE_FUNCTIONS[1] | {"FieldTaskPassEnd"} | set(NESTED_RESUME.values())

# Group B2's nested sites (task_yield.h 9..15; docs/state-by-name-sites.md 8). A wait's own yield is
# Task_YieldAt called by WaitSleep or by the wait's pass, loop or body (any may be inlined into the next); its
# kind is the first Field_/Menu_ wait function in the chain, and its caller the first caller function above
# that (the step, a function of the step's tail, or the resume entry). A resume entry names its site alone.
# GameMode_Field holds two sites (10, 11): the chain names both, and the return address tells them apart
# (--callers prints it; the notes pin it against the disassembly).
FIELD_WAIT = {"FieldWaitPass", "FieldWaitLoop", "Field_WaitTransition"}
MENU_WAIT = {"MenuWaitPass", "MenuWaitLoop", "Menu_WaitTransition"}
WAIT_GLUE = {"WaitSleep", "WaitResume"}
NESTED_WAITS = {   # site -> (the wait's functions, its callers)
    9: (FIELD_WAIT, {"GameMode_Enter"}),
    10: (FIELD_WAIT, {"GameMode_Field"}),
    11: (FIELD_WAIT, {"GameMode_Field"}),
    12: (FIELD_WAIT, {"GameMode3_Leave", "MenuCloseAfterSleep", "MenuCloseAfterWait", "ResumeMenuWaitClose",
                      "ResumeMenuLeaveSleep"}),
    13: (MENU_WAIT, {"GameMode3_Enter"}),
    14: (MENU_WAIT, {"GameMode3_Leave"}),
}
# Site 15, GameMode3_Leave's own sleep: Task_YieldAt called by MenuCloseAfterWait, or by what it is inlined into.
SLEEP_15 = {"MenuCloseAfterWait", "GameMode3_Leave", "ResumeMenuWaitClose"}
NESTED_CALLERS = set().union(*(c for _, c in NESTED_WAITS.values())) | set(NESTED_RESUME.values())
YIELD_AT = "Task_YieldAt"
# The task bodies and their loops and resume entries: a chain's words past the first of these are above the task's
# first frame, so stale (--callers cuts there).
BODIES = (set().union(*SITE_FUNCTIONS.values()) - {"FieldTaskPassEnd"}) | set(NESTED_RESUME.values())


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
    """The sites a chain's own yield can be at, by its names (empty: none). `fns` is the chain innermost first, cut
    at the task body. Task_Sleep called by Task_YieldAt, called by a function listed for a flat site; or, for a
    nested site (B2), the wait's functions and its caller as NESTED_WAITS has them. A function listed for two
    sites (SwingIn's two loops when inlined; GameMode_Field's two waits) gives a set."""
    if len(fns) < 2 or base_name(fns[0]) != YIELD_AT:
        return set()
    names = [base_name(f) for f in fns]
    out = {sid for sid, fset in SITE_FUNCTIONS.items() if names[1] in fset}
    if names[1] in SLEEP_15:
        out.add(15)
    # a nested wait: Task_YieldAt called by the wait's glue or its own functions. WaitSleep is often inlined into
    # the loop, and a stale word (a return address left in the loop's frame by a call it made, Menu_Frame's) can
    # then lie between Task_YieldAt and the loop: the wait's functions are looked for in the three words above.
    near = names[1:4]
    if any(n in WAIT_GLUE | FIELD_WAIT | MENU_WAIT for n in near):
        kind = next((n for n in names[1:] if n in FIELD_WAIT | MENU_WAIT), None)
        at = names.index(kind) + 1 if kind else 1
        caller = next((n for n in names[at:] if n in NESTED_CALLERS), None)
        for sid, (wait, callers) in NESTED_WAITS.items():
            if caller == NESTED_RESUME[sid] or (caller in callers and kind in wait):
                out.add(sid)
    return out


def cut_at_body(fns):
    """A named chain up to and including its first task body (BODIES): the words above are stale."""
    out = []
    for f in fns:
        out.append(f)
        if base_name(f) in BODIES:
            break
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", required=True, help="the ELF the runs were made with")
    ap.add_argument("--flat", default="", help="functions whose own yield a resume entry can stand for, by name")
    ap.add_argument("--tsv", help="also write every site of every run as one table")
    ap.add_argument("--sites", action="store_true", help="check the yield word against the chain (the A2 census)")
    ap.add_argument("--callers", action="store_true", help="tally the nested rows by their whole named chain")
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
    callers = {}   # (task, named chain) -> {"frames", "routes": Counter, "min", "max", "modes"}
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
                raw = [name_of(addrs, names, x).rsplit("+", 1)[0] if lo <= x < hi else f"{x:#x}" for x in s["chain"]]
                is_flat = bool(chain_site(cut_at_body(raw)))
            else:
                is_flat = fn in flat_names
            k = "fresh" if fresh else ("flat" if is_flat else "nested")
            kind[(task, sid)] = k
            if a.callers and k == "nested":
                named = []
                for x in s["chain"]:
                    nm = base_name(name_of(addrs, names, x).rsplit("+", 1)[0]) if lo <= x < hi else f"{x:#x}"
                    off = f"+{x - addrs[bisect.bisect_right(addrs, x) - 1]:#x}" if lo <= x < hi else ""
                    named.append(nm + off)
                    if nm in BODIES:
                        break
                c = callers.setdefault((task, tuple(named)), {"frames": 0, "routes": collections.Counter(),
                                                               "min": s["min"], "max": s["max"], "modes": set()})
                c["frames"] += s["frames"]
                c["routes"][route] += s["frames"]
                c["min"], c["max"] = min(c["min"], s["min"]), max(c["max"], s["max"])
                c["modes"].update(x for x in s["modes"].split(",") if x)
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
                    fns = [name_of(addrs, names, x).rsplit("+", 1)[0] if lo <= x < hi else f"{x:#x}" for x in s["chain"]]
                    full[(task, sid)] = (chain_site(cut_at_body(fns)), s["fresh"] == s["frames"], s["fresh"])
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
    if a.callers:
        print("  nested rows by their whole chain (innermost first, cut at the task body):")
        for (task, named), c in sorted(callers.items(), key=lambda kv: (kv[0][0], -kv[1]["frames"])):
            routes = ", ".join(f"{r} {n}" for r, n in c["routes"].most_common())
            print(f"    task {task} {c['frames']:>6} frames  depth {c['min']:#x}..{c['max']:#x}  modes "
                  f"{','.join(sorted(c['modes'], key=int))}\n      {' < '.join(named)}\n      routes: {routes}")
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
