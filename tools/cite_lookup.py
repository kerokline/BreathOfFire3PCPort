"""Where does a `src/game/...` citation point now? (the source-layout round, the owner's call 4)

The docs keep the paths they were written with; the source-layout round moves
functions between files (docs/source-layout-round.md 2, 7). This answers a
citation through analysis/tu_moves.tsv (tools/move_funcs.py writes it):

    python tools/cite_lookup.py src/game/rest_3f.cpp:1109 [src/game/x.cpp ...]
    python tools/cite_lookup.py --scan docs/ [--scan CLAUDE.md ...] [--strict]

A citation `path:line` (or `path:first-last`) is answered with the function or
file-local item that held that line, its pc, and where it lives now, path and
line:

1. a map row whose old_path is the path and whose old_first..old_last holds
   the line (the narrowest): the item moved; its new place is read from the
   tree now;
2. else, when the path was a move's source, the file as it was at the map's
   `base` commit (git show) is cut into items (tools/cpp_items.py) and the
   item holding the line is looked up by name - in the same file if it
   stayed, through the map if it moved;
3. else, when no move touched the path and it still exists, the citation
   stands as written.

A line outside every item of a file that moves touched (an `#include`, the
file's head comment, a blank line), a path that no longer exists and is in
no map row, or a name no longer found anywhere is **unresolved**. A path
without a line resolves when the file exists, or - when it was a move's
source and no longer exists - to the files its items went to.

--scan reads every text file under each path given (.md, .txt, .cpp, .h,
.py, .toml, .yml, .tsv) for `src/game/...cpp|h` citations and lists the
unresolved ones as `file:line: citation: reason`, then counts. Exit 0, or 1
with --strict when any is unresolved. The docs are never rewritten (call 4).
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import subprocess
import sys
from functools import lru_cache
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cpp_items as ci  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
CITE = re.compile(r"(?<![\w/.])(src/game/[\w/]+\.(?:cpp|h))(?::(\d+)(?:\s*(?:-|\.\.|–)\s*(\d+))?)?")
SCAN_SUFFIXES = {".md", ".txt", ".cpp", ".h", ".py", ".toml", ".yml", ".yaml", ".tsv"}


def load_map(path: Path) -> list[dict]:
    if not path.exists():
        return []
    with open(path, encoding="utf-8", newline="") as f:
        return list(csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"))


@lru_cache(maxsize=None)
def items_now(path: str):
    p = ROOT / path
    if not p.exists():
        return None
    return ci.FileItems(p.read_text(encoding="utf-8").replace("\r\n", "\n"))


@lru_cache(maxsize=None)
def items_at(commit: str, path: str):
    r = subprocess.run(["git", "show", f"{commit}:{path}"], cwd=ROOT, capture_output=True, text=True, encoding="utf-8")
    if r.returncode != 0:
        return None
    return ci.FileItems(r.stdout.replace("\r\n", "\n"))


def find_now(path: str, name: str):
    """(line, item) of the item called `name` in `path` today, or None."""
    fi = items_now(path)
    if fi is None:
        return None
    for it in fi.leaves():
        if name in it.names or "/".join(it.names) == name:
            return it.code_line, it
    return None


class Lookup:
    def __init__(self, map_path: Path):
        self.rows = load_map(map_path)
        self.by_old: dict[str, list[dict]] = {}
        self.by_name: dict[str, list[dict]] = {}
        for r in self.rows:
            self.by_old.setdefault(r["old_path"], []).append(r)
            self.by_name.setdefault(r["name"], []).append(r)

    def where(self, name: str, hint: str | None) -> str | None:
        """`path:line` of `name` today: in `hint` if it is there, else where the map sent it."""
        if hint:
            f = find_now(hint, name)
            if f:
                return f"{hint}:{f[0]}"
        for r in self.by_name.get(name, []):
            f = find_now(r["new_path"], name)
            if f:
                return f"{r['new_path']}:{f[0]}"
        return None

    def resolve(self, path: str, line: int | None) -> tuple[bool, str]:
        rows = self.by_old.get(path, [])
        exists = (ROOT / path).exists()
        if line is None:
            if exists:
                return True, path + (" (some of its items moved: " + ", ".join(sorted({r["new_path"] for r in rows})) + ")"
                                     if rows else "")
            if rows:
                return True, f"{path} is gone; its items went to " + ", ".join(sorted({r["new_path"] for r in rows}))
            return False, "no such file, and no move came from it"
        if not rows:
            if exists:
                return True, f"{path}:{line} (no move touched the file)"
            return False, "no such file, and no move came from it"
        hits = [r for r in rows if int(r["old_first"]) <= line <= int(r["old_last"])]
        if hits:
            r = min(hits, key=lambda r: int(r["old_last"]) - int(r["old_first"]))
            now = self.where(r["name"], r["new_path"])
            pc = f" ({r['pc']})" if r["pc"] else ""
            if now is None:
                return False, f"{r['kind']} {r['name']}{pc} moved to {r['new_path']} but is not found there now"
            if r["kind"] == "copied":        # the original stayed; the target holds a copy
                stay = self.where(r["name"], path) if exists else None
                copies = sorted({x["new_path"] for x in hits if x["name"] == r["name"]})
                return True, (f"local {r['name']}, " + (f"still {stay}" if stay else "gone from it")
                              + f"; copied to {', '.join(copies)}")
            return True, f"{r['kind']} {r['name']}{pc}, now {now}"
        base = rows[0]["base"]
        fi = items_at(base, path)
        if fi is None:
            return False, f"git show {base}:{path} failed"
        it = fi.at_line(line)
        while it is not None and it.kind in ("namespace", "linkage") and it.children:
            inner = [c for c in it.children if c.first_line <= line <= c.last_line]
            if not inner:
                break
            it = inner[0]
        if it is None or it.kind in ("pp", "namespace", "linkage") or not it.names:
            what = ("file-level (an #include or other directive, or the comment above one)"
                    if it is not None and it.kind == "pp" else "in no item (a file comment or a blank line)")
            return False, f"line {line} of {path} at {base} is {what}, in a file a move touched"
        name = it.names[0]
        now = self.where(name, path if exists else None)
        if now is None:
            return False, f"{name} (line {line} at {base}) is not found in the tree now"
        return True, f"{it.kind} {name} (line {line} at {base}), now {now}"


def scan(paths: list[Path]):
    for p in paths:
        files = [p] if p.is_file() else sorted(q for q in p.rglob("*") if q.is_file() and q.suffix in SCAN_SUFFIXES)
        for f in files:
            try:
                text = f.read_text(encoding="utf-8")
            except (UnicodeDecodeError, OSError):
                continue
            for n, line in enumerate(text.splitlines(), 1):
                for m in CITE.finditer(line):
                    yield f, n, m


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cite", nargs="*", help="src/game/<file>[:line[-line]]")
    ap.add_argument("--scan", action="append", type=Path, default=[], help="a file or directory to read citations from")
    ap.add_argument("--map", type=Path, default=ROOT / "analysis" / "tu_moves.tsv")
    ap.add_argument("--strict", action="store_true", help="--scan: exit 1 when any citation is unresolved")
    a = ap.parse_args()
    look = Lookup(a.map)
    if not a.cite and not a.scan:
        ap.error("give a citation or --scan")
    bad = 0
    for c in a.cite:
        m = CITE.fullmatch(c.replace("\\", "/"))
        if not m:
            print(f"{c}: not a src/game/... citation")
            bad += 1
            continue
        ok, text = look.resolve(m.group(1), int(m.group(2)) if m.group(2) else None)
        print(f"{c}: {text}" if ok else f"{c}: UNRESOLVED: {text}")
        bad += not ok
    if a.scan:
        total = 0
        for f, n, m in scan(a.scan):
            total += 1
            ok, text = look.resolve(m.group(1), int(m.group(2)) if m.group(2) else None)
            if not ok:
                bad += 1
                where = f.resolve().relative_to(ROOT).as_posix() if f.resolve().is_relative_to(ROOT) else str(f)
                print(f"{where}:{n}: {m.group(0)}: {text}")
        print(f"cite_lookup: {total} citations read, {total - bad} resolved, {bad} unresolved")
        return 1 if (bad and a.strict) else 0
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
