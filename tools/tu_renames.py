#!/usr/bin/env python
"""The source-layout round's rename record: analysis/tu_renames.tsv, checked and filled.

    python tools/tu_renames.py --check           # every row well formed, and each rename done in its file
    python tools/tu_renames.py --fill            # write each empty `commit` from git; idempotent
    python tools/tu_renames.py --check --table FILE

Step B0 of the round (docs/source-layout-round.md 7a, the owner's call 7):
every rename of a file-local helper made so that two sources' helpers of one
name can share a target file is a row, with its reason, and so is each hand
decision on shared state, an undeclared global and a macro-made function.
The table is never pruned: it is the record to revisit a rename from. It
holds our names and file paths only; no byte of the game.

The table: `#` lines are comments; the first other line is the header,
exactly these tab-separated columns:

    source_file   the file the item is defined in (src/...), as at the rename
    old_name      the item's name before
    new_name      its name after (`=` for a decision that keeps the name)
    kind          helper | constant | variable | state | macro | global | qualify
    reason        why: the clash it removes (the two sources, the target), or the decision
    target_file   the file the two sources' items meet in (the move's target)
    other_source  the other source whose item of that name differs
    commit        the commit that made the rename; written by --fill

--check, a row at a time, every failure listed with its line, exit 1 if any:

- the columns are there and `kind` is one of the six; source_file, old_name,
  new_name and reason are not empty; a commit, when given, is a commit of
  this repository (in a shallow clone, as CI's, a hash by its shape only);
  no row twice; an item clashing in several targets has a row each, all
  with one new_name;
- source_file exists in the tree, or analysis/tu_moves.tsv lists it as an
  `old_path` (a file the moves have since emptied);
- for kind helper, constant or variable - a rename done in the source -
  old_name is no longer a file-local of source_file (an anonymous-namespace
  or `static` item, a using-declaration, an alias, a #define; a namespace's
  global of that name may stay) and new_name is defined there, both by
  tools/cpp_items.py's names of the file's items (a prototype is not a
  definition; a struct a B0 seam promotion moved to the source's own
  <stem>_callees.h is looked for there too). A new_name spelled `ns::name` is a using-declaration dropped
  and its uses named in full: only the old name is looked for. Where
  source_file has been moved away, new_name must be defined in one of the
  files the map moved its items into (the old name is not looked for: a
  target may hold another source's item of that name). Where only some of its items moved (the
  source stays, for its _Inject()), new_name may be defined in one of those files instead;
- kind state, global and macro are decisions: shape only;
- for kind qualify - a name two sources' `using namespace` lines would make
  ambiguous in a target, written qualified in the source's bodies (the
  qualification pass, the round's 7b item 11) - new_name is `ns::old_name`,
  target_file and other_source (the other namespaces, comma-separated) are
  given, and new_name is named in source_file's code (or, the file moved
  away, in one of the files it moved into).

--fill: each row with an empty commit gets the first commit on the current
branch that changed the number of `new_name`s in source_file
(`git log --reverse -S<new_name> -- <source_file>`), short to 8 digits; a
row whose new_name is `=` gets the commit that added the row to the table;
a qualify row gets the newest such commit (an older one may have named it
in full already).
A row git finds nothing for is listed and left empty (commit the rename
first). Only the commit column of such rows changes; a second run changes
nothing.
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cpp_items as ci  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
TABLE = ROOT / "analysis" / "tu_renames.tsv"
MOVES = ROOT / "analysis" / "tu_moves.tsv"
COLUMNS = ["source_file", "old_name", "new_name", "kind", "reason", "target_file", "other_source", "commit"]
KINDS = ("helper", "constant", "variable", "state", "macro", "global", "qualify")
RENAMES = ("helper", "constant", "variable")
DEFINING = ("func", "type", "alias", "typedef", "using", "pp")


def git(*args: str) -> str:
    r = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else ""


def read_table(path: Path) -> tuple[list[str], int | None, list[tuple[int, list[str]]], list[str]]:
    """(the file's lines without their ends, the header's index, [(line index, fields)], problems)."""
    with open(path, encoding="utf-8", newline="") as f:
        raw = f.read()
    lines = raw.splitlines()
    rows: list[tuple[int, list[str]]] = []
    problems: list[str] = []
    header = None
    for i, line in enumerate(lines):
        if not line.strip() or line.startswith("#"):
            continue
        fields = line.split("\t")
        if header is None:
            if fields != COLUMNS:
                problems.append(f"{rel(path)}:{i + 1}: the header must be {' '.join(COLUMNS)} (tab-separated): {line}")
            header = i
            continue
        rows.append((i, fields))
    if header is None:
        problems.append(f"{rel(path)}: no header line")
    return lines, header, rows, problems


def rel(p: Path) -> str:
    p = p.resolve()
    return p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else str(p)


def moved_paths() -> dict[str, set[str]]:
    """old_path -> the new paths analysis/tu_moves.tsv sends its items to (empty when no map)."""
    out: dict[str, set[str]] = {}
    if not MOVES.exists():
        return out
    with open(MOVES, encoding="utf-8", newline="") as f:
        body = [l for l in f if not l.startswith("#")]
    for r in csv.DictReader(body, delimiter="\t"):
        if r.get("old_path"):
            out.setdefault(r["old_path"], set()).add(r.get("new_path") or "")
    return out


_defined: dict[str, tuple[set[str], set[str]]] = {}


def defined_names(path: str, local_only: bool = False) -> set[str]:
    """The names the items of one file define (cpp_items), prototypes left out; with local_only,
    the file-local ones alone (an anonymous-namespace or `static` item, a using-declaration, an
    alias, a #define) - what a rename renames, not a namespace's global of the same name."""
    if path not in _defined:
        text = (ROOT / path).read_text(encoding="utf-8").replace("\r\n", "\n")
        names: set[str] = set()
        local: set[str] = set()
        for it in ci.FileItems(text).leaves():
            if it.kind in DEFINING or (it.kind == "decl" and it.is_var) or it.kind == "macro":
                names.update(it.names)
                if it.local or it.kind == "pp":
                    local.update(it.names)
        _defined[path] = (names, local)
    return _defined[path][1 if local_only else 0]


def named_in_full(path: str, qualified: str) -> bool:
    """`ns::name` written in the file's code (comments and strings left out; spacing free)."""
    want = [t for t in re.split(r"(::)", qualified) if t]
    toks = [t.text for t in ci.lex((ROOT / path).read_text(encoding="utf-8").replace("\r\n", "\n"))
            if t.kind in ("id", "punct")]
    n = len(want)
    return any(toks[i:i + n] == want for i in range(len(toks) - n + 1))


def check(path: Path) -> int:
    lines, header, rows, problems = read_table(path)
    moves = moved_paths()
    # CI's checkout is one commit deep: a row's commit is then checked for its shape only
    shallow = git("rev-parse", "--is-shallow-repository") == "true"
    seen: dict[tuple[str, ...], int] = {}
    renamed: dict[tuple[str, str, str], tuple[str, int]] = {}
    for i, f in rows:
        where = f"{rel(path)}:{i + 1}"
        if len(f) != len(COLUMNS):
            problems.append(f"{where}: {len(f)} fields, not {len(COLUMNS)}: {lines[i]}")
            continue
        r = dict(zip(COLUMNS, (x.strip() for x in f)))
        bad = []
        if r["kind"] not in KINDS:
            bad.append(f"kind {r['kind']!r} is not one of {' | '.join(KINDS)}")
        for c in ("source_file", "old_name", "new_name", "reason"):
            if not r[c]:
                bad.append(f"{c} is empty")
        if r["kind"] in RENAMES:
            if r["new_name"] == "=" or r["new_name"] == r["old_name"]:
                bad.append("a rename needs a new name")
            for c in ("target_file", "other_source"):
                if not r[c]:
                    bad.append(f"{c} is empty (a rename names the clash it removes)")
        if r["kind"] == "qualify":
            if not r["new_name"].endswith("::" + r["old_name"]) or r["new_name"] == "::" + r["old_name"]:
                bad.append(f"a qualification's new name is ns::{r['old_name']}")
            for c in ("target_file", "other_source"):
                if not r[c]:
                    bad.append(f"{c} is empty (a qualification names the ambiguity it removes)")
        if r["commit"]:
            if not re.fullmatch(r"[0-9a-f]{7,40}", r["commit"]):
                bad.append(f"commit {r['commit']!r} is not a commit hash")
            elif not shallow and not git("rev-parse", "--verify", "--quiet", r["commit"] + "^{commit}"):
                bad.append(f"commit {r['commit']} is not a commit of this repository")
        # one item may clash in several targets (a row each); it is renamed one way only
        key = (r["source_file"], r["old_name"], r["kind"], r["target_file"], r["other_source"])
        if key in seen:
            bad.append(f"the same row as line {seen[key] + 1}")
        seen.setdefault(key, i)
        first = renamed.setdefault((r["source_file"], r["old_name"], r["kind"]), (r["new_name"], i))
        if first[0] != r["new_name"]:
            bad.append(f"{r['old_name']} is renamed {first[0]} on line {first[1] + 1}")
        src = r["source_file"]
        exists = bool(src) and (ROOT / src).is_file()
        if src and not exists and src not in moves:
            bad.append(f"{src} is not in the tree, nor an old_path of {rel(MOVES)}")
        if not bad and r["kind"] in RENAMES:
            if exists:
                if r["old_name"] in defined_names(src, local_only=True):
                    bad.append(f"{r['old_name']} is still defined in {src}")
                # `ns::name`: a using-declaration dropped and its uses named in full; defined elsewhere
                # a seam's struct promoted in B0 lives in the source's own <stem>_callees.h
                seam = src[:-len(".cpp")] + "_callees.h" if src.endswith(".cpp") else ""
                # a helper the moves took out of a source that stays (its _Inject() keeps it) is
                # looked for in the files the map moved the source's items into
                homes = sorted(p for p in moves.get(src, ()) if p and (ROOT / p).is_file())
                if "::" not in r["new_name"] and r["new_name"] not in defined_names(src) and not (
                        seam and (ROOT / seam).is_file() and r["new_name"] in defined_names(seam)) and not any(
                        r["new_name"] in defined_names(p) for p in homes):
                    bad.append(f"{r['new_name']} is not defined in {src}")
            else:
                homes = sorted(p for p in moves.get(src, ()) if p and (ROOT / p).is_file())
                if "::" not in r["new_name"] and not any(r["new_name"] in defined_names(p) for p in homes):
                    bad.append(f"{r['new_name']} is defined in none of the files {src} moved into "
                               f"({', '.join(homes) or 'none in the tree'})")
        if not bad and r["kind"] == "qualify":
            homes = ([src] if exists else []) + sorted(p for p in moves.get(src, ()) if p and (ROOT / p).is_file())
            if not any(named_in_full(p, r["new_name"]) for p in homes):
                bad.append(f"{r['new_name']} is not named in {src if exists else ', '.join(homes) or 'the tree'}")
        for b in bad:
            problems.append(f"{where}: {b}: {lines[i]}")
    for p in problems:
        print(p)
    print(f"tu_renames --check: {len(rows)} rows, {len(problems)} problems")
    return 1 if problems else 0


def first_commit(r: dict, table: Path) -> str:
    if r["new_name"] == "=":
        needle = "\t".join([r["source_file"], r["old_name"], r["new_name"], r["kind"]])
        out = git("log", "--reverse", "--format=%H", "-S" + needle, "--", rel(table))
    elif r["kind"] == "qualify":
        out = git("log", "--format=%H", "-S" + r["new_name"], "--", r["source_file"])
    else:
        out = git("log", "--reverse", "--format=%H", "-S" + r["new_name"], "--", r["source_file"])
    first = out.splitlines()[0] if out else ""
    return first[:8]


def fill(path: Path) -> int:
    lines, header, rows, problems = read_table(path)
    if problems:
        for p in problems:
            print(p)
        return 1
    with open(path, encoding="utf-8", newline="") as f:
        raw = f.read()
    eol = "\r\n" if "\r\n" in raw else "\n"
    filled = missing = 0
    for i, f in rows:
        if len(f) != len(COLUMNS):
            print(f"{rel(path)}:{i + 1}: {len(f)} fields, not {len(COLUMNS)}; run --check")
            return 1
        r = dict(zip(COLUMNS, (x.strip() for x in f)))
        if r["commit"]:
            continue
        c = first_commit(r, path)
        if not c:
            what = (f"the row in {rel(path)}" if r["new_name"] == "=" else f"{r['new_name']} in {r['source_file']}")
            print(f"{rel(path)}:{i + 1}: no commit found for {what}; commit it first")
            missing += 1
            continue
        f[COLUMNS.index("commit")] = c
        lines[i] = "\t".join(f)
        filled += 1
    if filled:
        with open(path, "w", encoding="utf-8", newline="") as out:
            out.write(eol.join(lines) + (eol if raw.endswith(("\n", "\r")) else ""))
    print(f"tu_renames --fill: {filled} commits written, {missing} not found, "
          f"{sum(1 for _, f in rows if len(f) == len(COLUMNS) and f[-1].strip()) - filled} already there")
    return 1 if missing else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="check every row; exit 1 with each failing one")
    ap.add_argument("--fill", action="store_true", help="write each empty commit from git")
    ap.add_argument("--table", type=Path, default=TABLE)
    a = ap.parse_args()
    if a.check == a.fill:
        ap.error("give one of --check or --fill")
    return check(a.table) if a.check else fill(a.table)


if __name__ == "__main__":
    sys.exit(main())
