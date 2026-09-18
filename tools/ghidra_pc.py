#!/usr/bin/env python
"""Headless Ghidra driver for this repo's own project.

Deliberately a SEPARATE Ghidra project from the archival sibling's `BoF3`.
That project is theirs: its `ghidra_run.py merge` reads every program in it and
writes names into their `symbols.toml`, so dropping a PC-port binary in there
would quietly feed PC addresses into the archival name corpus. The two repos
have opposite invariants (CLAUDE.md) and the boundary is cheaper to keep than
to repair. Cost is one re-analysis of the PSX boot EXE.

    python tools/ghidra_pc.py import        # BOF3.exe + SLPS_009.90 -> project BoF3PC
    python tools/ghidra_pc.py import --only pc
    python tools/ghidra_pc.py list

Ghidra project lives outside the repo (it embeds a copy of the analysed
binary — CLAUDE.md rule 1, and .gitignore's /ghidra/ rule).
"""
import argparse, os, subprocess, sys, time, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GHIDRA_DIR = os.environ.get("PC_GHIDRA_DIR", r"D:\Utilities\ghidra_12.1.3_PUBLIC")
PROJECT_DIR = os.environ.get("PC_GHIDRA_PROJECT_DIR", r"D:\Utilities\GhidraProjects")
PROJECT = os.environ.get("PC_GHIDRA_PROJECT", "BoF3PC")
SCRATCH = os.environ.get("PC_SCRATCH", os.path.join(PROJECT_DIR, "_scratch"))

PC_EXE = os.path.join(ROOT, "bof3", "BOF3.exe")
PSX_EXE = os.path.join(ROOT, "..", "BreathOfFire3Recomp", "disc", "SLPS_009.90")
PSX_LANG = "MIPS:LE:32:default"      # same as the sibling's ghidra_run.py
PSX_TEXT = 0x80093800
PSX_HEADER = 0x800                   # PS-X EXE header, stripped before import


def headless(args, timeout=14400, verbose=True):
    cmd = [sys.executable, "-m", "pyghidra.ghidra_launch", "--install-dir", GHIDRA_DIR,
           "ghidra.app.util.headless.AnalyzeHeadless", PROJECT_DIR, PROJECT] + list(args)
    print("+ " + " ".join(cmd), flush=True)
    t0 = time.time()
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       text=True, encoding="utf-8", errors="replace", timeout=timeout)
    for ln in p.stdout.splitlines():
        if verbose or any(k in ln for k in ("ERROR", "Exception", "ANALYZING", "IMPORT")):
            print(ln, flush=True)
    print("headless exit %d (%.0fs)" % (p.returncode, time.time() - t0), flush=True)
    return p.returncode


def psx_text_image():
    """PS-X EXE minus its 0x800 header, so a raw import lands text at 0x80093800."""
    os.makedirs(SCRATCH, exist_ok=True)
    out = os.path.join(SCRATCH, "SLPS_009.90.text")
    data = open(PSX_EXE, "rb").read()[PSX_HEADER:]
    open(out, "wb").write(data)
    print("psx text image: %s (%d bytes)" % (out, len(data)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["import", "list"])
    ap.add_argument("--only", choices=["pc", "psx"])
    a = ap.parse_args()
    if a.cmd == "list":
        return headless(["-noanalysis", "-process", "-recursive"], timeout=600)
    if a.only != "psx":
        headless(["-import", PC_EXE, "-overwrite"])
    if a.only != "pc":
        headless(["-import", psx_text_image(), "-overwrite",
                  "-loader", "BinaryLoader",
                  "-loader-baseAddr", hex(PSX_TEXT),
                  "-processor", PSX_LANG])

main()
