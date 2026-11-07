"""The DLL's src/game source list, generated at configure time (the source-layout round).

docs/source-layout-round.md 2.3 asks that two groups' moves merge without a
hand-resolved conflict. A committed list of files cannot promise that: two
groups that each add a file next to the same old one edit neighbouring lines.
So no list of game files is committed. CMake globs src/game/**/*.cpp
(CONFIGURE_DEPENDS, so a file added or deleted reconfigures), and this tool
orders what it found:

1. cmake/game_sources_order.txt - the order the sources had when the list was
   adopted (`adopt`, below), one path per line, kept as it is through the
   round. The link order decides the order of the objects' static
   initialisers, which the relocatable-image round found matters (HANDOFF,
   the `draw_pool` trap); keeping it means a move changes no file's place
   but its own. A path no longer on disk is skipped; the non-game paths
   among them (src/input, src/audio, src/render, which sat inside the run)
   are kept where they are.
2. A file on disk but not in the order file goes right after the file its
   lowest-`pc` function came from, read from analysis/tu_moves.tsv's
   old_path (the map tools/move_funcs.py writes); several after one file go
   in pc order; `x_fuzz.cpp` goes right after `x.cpp`; a file with neither
   goes after the last game file, by path. A file's position depends only
   on the tree, so the result is the same whichever group's merge came
   first.

    python tools/game_sources.py adopt              # once: CMakeLists.txt's list -> the order file
    python tools/game_sources.py emit --found F --out O   # cmake/game_sources.cmake runs this
    python tools/game_sources.py list               # print the ordered list for this tree

`adopt` takes the run of `_dll_sources` from its first src/game entry to its
last, writes it to cmake/game_sources_order.txt and puts
`${BOF3X_GAME_SOURCES}` in its place, with `include(cmake/game_sources.cmake)`
before the list. The adopted build links the same objects in the same order
as before (`codegen_same.py` object mode: 0 differ; the DLL's bytes compared
in the dry run of docs/source-layout-round.md 3 "SL3 as run").
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LF = "\n"
CRLF = "\r\n"
ORDER = ROOT / "cmake" / "game_sources_order.txt"
MAP = ROOT / "analysis" / "tu_moves.tsv"
HEADER = ("# The order of the DLL's src/game sources (and the few others inside their run), adopted from\n"
          "# CMakeLists.txt by tools/game_sources.py. Kept as it is through the source-layout round: a\n"
          "# file missing from disk is skipped, a new file is placed by the rule in tools/game_sources.py.\n")


def read_order(path: Path) -> list[str]:
    if not path.exists():
        sys.exit(f"game_sources: no {path.name}: the list is not adopted yet (python tools/game_sources.py adopt)")
    return [l.strip() for l in path.read_text(encoding="utf-8").splitlines() if l.strip() and not l.startswith("#")]


def provenance(map_path: Path) -> dict[str, tuple[int, str]]:
    """new_path -> (lowest pc, its old_path), from the move map."""
    out: dict[str, tuple[int, str]] = {}
    if not map_path.exists():
        return out
    with open(map_path, encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"))
    for r in rows:
        if r.get("kind") != "func" or not r.get("pc"):
            continue
        pc = int(r["pc"], 16)
        cur = out.get(r["new_path"])
        if cur is None or pc < cur[0]:
            out[r["new_path"]] = (pc, r["old_path"])
    return out


def ordered(found: list[str], order: list[str], prov: dict[str, tuple[int, str]]) -> list[str]:
    on_disk = set(found)
    known = set(order)
    new = sorted(p for p in on_disk if p not in known)
    after: dict[str, list[tuple[int, str]]] = {}
    tail: list[str] = []
    placed = set()
    for p in new:
        if p in prov:
            after.setdefault(prov[p][1], []).append((prov[p][0], p))
            placed.add(p)
    for p in new:
        if p in placed:
            continue
        if p.endswith("_fuzz.cpp") and (p[:-9] + ".cpp") in on_disk:
            after.setdefault(p[:-9] + ".cpp", []).append((-1, p))
            continue
        tail.append(p)
    out: list[str] = []

    def put(p: str) -> None:
        out.append(p)
        for _, q in sorted(after.get(p, []), key=lambda x: (x[0] < 0, x[0], x[1])):
            if q not in out:
                put(q)

    last_game = max((i for i, p in enumerate(order) if p.startswith("src/game/")), default=-1)
    for i, p in enumerate(order):
        if p.startswith("src/game/"):
            if p in on_disk:
                put(p)
            else:                                  # gone: what was placed after it still has a place
                for _, q in sorted(after.get(p, []), key=lambda x: (x[0] < 0, x[0], x[1])):
                    if q not in out:
                        put(q)
        else:
            out.append(p)
        if i == last_game:
            for q in sorted(tail):
                if q not in out:
                    put(q)
    # anything placed after a file that is neither in the order nor on disk
    for p in sorted(on_disk):
        if p not in out:
            put(p)
    return out


def found_now() -> list[str]:
    return sorted(p.relative_to(ROOT).as_posix() for p in (ROOT / "src" / "game").rglob("*.cpp"))


def adopt() -> int:
    cm = ROOT / "CMakeLists.txt"
    eol = CRLF if CRLF.encode() in cm.read_bytes() else LF
    text = cm.read_text(encoding="utf-8")
    m = re.search(r"set\(_dll_sources\n(.*?)\)\n", text, re.S)
    if not m:
        sys.exit("game_sources: no set(_dll_sources ...) in CMakeLists.txt")
    lines = m.group(1).split("\n")
    idx = [i for i, l in enumerate(lines) if "src/game/" in l]
    if not idx:
        sys.exit("game_sources: _dll_sources has no src/game entry (adopted already?)")
    first, last = idx[0], idx[-1]
    run = []
    for l in lines[first:last + 1]:
        run += l.split()
    if any(not re.fullmatch(r"src/[\w/]+\.(cpp|rc)", p) for p in run):
        sys.exit("game_sources: an entry in the run is not a plain path: " + ", ".join(p for p in run if not re.fullmatch(r"src/[\w/]+\.(cpp|rc)", p)))
    ORDER.write_text(HEADER + "\n".join(run) + "\n", encoding="utf-8", newline=eol)
    new_lines = lines[:first] + ["  ${BOF3X_GAME_SOURCES}"] + lines[last + 1:]
    body = "set(_dll_sources\n" + "\n".join(new_lines) + ")\n"
    text = text[:m.start()] + ("# src/game's part of the list, generated from the tree (tools/game_sources.py):\n"
                               "include(cmake/game_sources.cmake)\n") + body + text[m.end():]
    cm.write_text(text, encoding="utf-8", newline=eol)
    print(f"game_sources: {len(run)} entries adopted into {ORDER.relative_to(ROOT).as_posix()}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=("adopt", "emit", "list"))
    ap.add_argument("--found", type=Path, help="emit: the globbed paths, one per line")
    ap.add_argument("--out", type=Path, help="emit: the cmake file to write")
    ap.add_argument("--order", type=Path, default=ORDER)
    ap.add_argument("--map", type=Path, default=MAP)
    a = ap.parse_args()
    if a.mode == "adopt":
        return adopt()
    found = ([l.strip() for l in a.found.read_text(encoding="utf-8").splitlines() if l.strip()]
             if a.found else found_now())
    result = ordered(found, read_order(a.order), provenance(a.map))
    if a.mode == "list":
        print("\n".join(result))
        return 0
    body = "# Generated by tools/game_sources.py; do not edit.\nset(BOF3X_GAME_SOURCES\n" + \
        "".join(f"  {p}\n" for p in result) + ")\n"
    if not a.out.exists() or a.out.read_text(encoding="utf-8") != body:
        a.out.parent.mkdir(parents=True, exist_ok=True)
        a.out.write_text(body, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
