#!/usr/bin/env python
"""Validate Ghidra BSim as the PLAN section 3 matcher, against our own pairs.

BSim fingerprints functions from Ghidra's architecture-independent P-code, so
in principle it does cross-architecture matching by construction. The question
is whether it works on THIS pair of binaries. We have a free answer key: the
hand-verified PSX<->PC pairs in symbols.toml.

    python tools/bsim_probe.py build     # H2 db + PSX signatures
    python tools/bsim_probe.py query     # query our PC functions, write json
    python tools/bsim_probe.py score     # rank of the true counterpart, per pair

Only pairs whose PSX side is boot-EXE resident are scored: the PSX overlays are
not imported, and the battle overlays load *over* the boot EXE's address range
(BATTLE.EMI#15 at 0x80093800), so their addresses would resolve to unrelated
boot code. symbols.toml marks those with psx_overlay.
"""
import argparse, json, os, subprocess, sys, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GHIDRA = os.environ.get("PC_GHIDRA_DIR", r"D:\Utilities\ghidra_12.1.3_PUBLIC")
PROJECT_DIR = os.environ.get("PC_GHIDRA_PROJECT_DIR", r"D:\Utilities\GhidraProjects")
PROJECT = os.environ.get("PC_GHIDRA_PROJECT", "BoF3PC")
BSIM = os.path.join(GHIDRA, "support", "bsim.bat")
DB = os.environ.get("PC_BSIM_DB", "file:/" + PROJECT_DIR.replace("\\", "/") + "/bsim/bof3psx")
SIGDIR = os.path.join(PROJECT_DIR, "bsim", "sigs")
PSX_PROG = "SLPS_009.90.text"
PC_PROG = "BOF3.exe"
OUT = os.path.join(ROOT, "analysis", "bsim_query.json")
CONFIG = "medium_nosize"      # size-insensitive: the port's functions are not the same length


def run(cmd, **kw):
    print("+ " + " ".join(cmd), flush=True)
    p = subprocess.run(cmd, text=True, encoding="utf-8", errors="replace", **kw)
    return p.returncode


def ghidra_url(prog):
    return "ghidra:/%s/%s?/%s" % (PROJECT_DIR.replace("\\", "/"), PROJECT, prog)


def pairs():
    d = tomllib.load(open(os.path.join(ROOT, "symbols.toml"), "rb"))
    out = []
    for f in d["func"]:
        if "psx" not in f:
            continue
        out.append(dict(pc=f["pc"], psx=f["psx"], name=f["name"],
                        status=f["status"], overlay=f.get("psx_overlay")))
    return out


def cmd_build():
    os.makedirs(os.path.dirname(SIGDIR), exist_ok=True)
    run([BSIM, "createdatabase", DB, CONFIG, "-n", "BoF3 PSX boot EXE",
         "-d", "SLPS-00990 boot EXE signatures for PSX<->PC matching"])
    return run([BSIM, "generatesigs", ghidra_url(PSX_PROG), SIGDIR,
                "--bsim", DB, "--commit", "--overwrite"])


def cmd_query():
    tgt = [hex(p["pc"]) for p in pairs() if not p["overlay"]]
    script = os.path.join(ROOT, "tools", "ghidra")
    return run([sys.executable, "-m", "pyghidra.ghidra_launch", "--install-dir", GHIDRA,
                "ghidra.app.util.headless.AnalyzeHeadless", PROJECT_DIR, PROJECT,
                "-process", PC_PROG, "-noanalysis", "-scriptPath", script,
                "-postScript", "bsim_query.py", DB, OUT, ",".join(tgt)])


def cmd_score():
    res = {r["addr"].lower(): r for r in json.load(open(OUT))["results"]}
    hit = 0
    scored = [p for p in pairs() if not p["overlay"]]
    print("%-28s %-10s %-10s %-6s %-9s %s" %
          ("name", "pc", "psx", "rank", "similar", "top match"))
    for p in scored:
        r = res.get("0x%08x" % p["pc"]) or res.get(hex(p["pc"]))
        if r is None:
            print("%-28s %-10s %-10s  -- no BSim result --" % (p["name"], hex(p["pc"]), hex(p["psx"])))
            continue
        rank, sim = None, None
        for i, m in enumerate(r["matches"], 1):
            if int(m["addr"], 16) == p["psx"]:
                rank, sim = i, m["similarity"]
                break
        top = r["matches"][0] if r["matches"] else None
        topstr = "%s sim %.3f" % (top["addr"], top["similarity"]) if top else "(none)"
        if rank == 1:
            hit += 1
        print("%-28s %-10s %-10s %-6s %-9s %s" %
              (p["name"], hex(p["pc"]), hex(p["psx"]),
               rank if rank else ">%d" % len(r["matches"]),
               "%.3f" % sim if sim else "-", topstr))
    print("\ntrue counterpart ranked #1 for %d of %d pairs" % (hit, len(scored)))


ap = argparse.ArgumentParser()
ap.add_argument("cmd", choices=["build", "query", "score"])
a = ap.parse_args()
{"build": cmd_build, "query": cmd_query, "score": cmd_score}[a.cmd]()
