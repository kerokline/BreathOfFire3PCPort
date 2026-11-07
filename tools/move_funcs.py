"""Move functions between our source files without touching a body.

The source-layout round (docs/source-layout-round.md 2.3, group SL3) moves
every function of ours into the file of its recovered translation unit. This
tool does one such move set mechanically:

    python tools/move_funcs.py TABLE [--file-column COL] [--names A,B | --range LO..HI | --unit U]
                               [--check] [--map-only] [--map analysis/tu_moves.tsv] [--shared copy|header]

TABLE is a TSV with a header line (lines starting with # are skipped). The
columns read:

    name            the function (a [[func]] name in symbols.toml)              required
    file            where it goes: src/game/<subsystem>/<stem>.cpp              required
                    (--file-column names another column; `proposed_file`,
                    SL1's name in analysis/tu_layout.tsv, is tried when
                    `file` is absent)
    pc              its address, hex (else symbols.toml's)                      optional
    unit            the unit id, copied into the map                            optional

Any other column is ignored, so analysis/tu_layout.tsv feeds it as it is. A
row whose file is the function's present `impl`, or whose file column is
empty, is no move. Every row must name a [[func]] with an `impl`.

What one move carries, for each function F leaving source S for target T:

- F's definition with the comment block that leads it (no blank line
  between), byte for byte, into T in `pc` order, under the same
  `#pragma clang attribute push(...)` it had in S;
- S's file-local items F reaches (anonymous-namespace or `static` functions,
  constants and variables, local types, `using` aliases, `#define`s),
  followed transitively, each in the namespaces it sat in. An item only
  moved functions reach goes with them. An item also reached from code that
  stays in S, or from two targets, is **copied** (the default, `--shared
  copy`): S keeps it and each target gets the same text. With `--shared
  header` it goes instead, verbatim, to `<S dir>/<S stem>_local.h`, which S
  and each target include; that mode is refused when one target would get
  one name through two such headers, which the dry run met at once (every
  wave file has its own `U`, `Long`, `S`). Either way the item keeps its own
  spelling - inside `namespace {` or `static` - not `inline`: internal
  linkage per file is what it had, and the `inline` keyword adds clang's
  inline hint, which can change a caller's code. A shared item that is state
  (a non-const variable, or a function with a `static` local) is refused:
  two files would hold two copies of it;
- `using namespace X;` lines of S: every target gets them;
- the `namespace at = <group>::at;` alias of each source (the callees
  constants): T gets its own. Where T's functions come from groups with
  different aliases, T's callees header `<T stem>_callees.h` declares
  `namespace <T stem>::at` holding a `using namespace` of each group's, so
  every `at::kName` in a moved body names the constant it named before. The
  constants stay in their groups' headers (stage E retires the remnants,
  docs/source-layout-round.md 5a); a name two groups both define is an
  ambiguity the build reports;
- F's `BOF3_INJECT(F);` line, out of S's `_Inject()` into T's
  `<TStem>_Inject()`, which src/hook/inject_all.cpp calls right after the
  last of its sources' `_Inject()` calls - after each source's self-test,
  which clones the originals before they are patched; T's header is included
  after the last source header's line there;
- `impl = "..."` in symbols.toml.

What it does not move, and says so: the fuzz rows (a row is an entry of its
group's own table type, counted by `static_assert`s and driven by the group's
harness code - one fuzz file per new file is stage C's work,
docs/source-layout-round.md 5; the rows keep working, since they name the
function by its declaration in symbols.gen.h; the lines are listed); the
declarations in a group's own header (symbols.gen.h declares every function
with an `impl`; the group header's copies stay valid).

Refused, before anything is written, every reason listed: a function
tools/cpp_items.py does not find, one inside a named namespace, an `#if` or
an `extern "C" { }` block; two different items of one name meeting in T
(the same code under other comments is one item); shared state as above; a
moved function that names a global S defines and no included header
declares; a target file that already exists (adding to one is not built:
a group moves a unit's functions into its new file in one run); a source
with uncommitted changes (the map's old lines must be at a commit).

Idempotent: the rows already done (impl already T) are skipped, so a second
run changes nothing.

The map, analysis/tu_moves.tsv (call 4: the docs keep their old paths):
one row per moved function or file-local item, sorted by old path and line.
Columns:

    pc          the function's address (hex), empty for a file-local item
    name        our name
    kind        func; local (moved with its functions); copied (stayed in
                old_path, a copy in new_path; one row per copy); shared (to
                the _local.h header, --shared header)
    unit        from TABLE, else empty
    old_path    where it was at `base`
    old_first   its first line there, the leading comment included
    old_def     the line its code starts on (the signature)
    old_last    its last line there
    new_path    where it is now
    base        the commit whose tree old_* describe (HEAD when the rows were written)

A function's row already in the map keeps its old_* and base and gets the
new path, so a function moved twice still points back at the tree before
the round. `--map-only` writes the rows for every move of TABLE and moves
nothing: the coordinator's step on `main` before stage B, so that every
row describes the tree before the moves and the groups' moves only find
their rows there - no group edits a line of the map, so none conflicts on
it. tools/cite_lookup.py reads it.

The build's src/game source list: tools/game_sources.py - generated at
configure time from the files on disk, in the order of
cmake/game_sources_order.txt with a new file placed after the file its
lowest-pc function came from (this map's old_path), so two groups' moves
never edit one line of a list.

--check plans every move and prints what it would do, writing nothing. The
gate after a move is `tools/codegen_same.py BASE NEW --by-function
--inject-ok`.
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import subprocess
import sys
import tomllib
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cpp_items as ci  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
MAP_COLUMNS = ["pc", "name", "kind", "unit", "old_path", "old_first", "old_def", "old_last", "new_path", "base"]
ALIAS_AT = re.compile(r"^\s*namespace\s+at\s*=\s*(::)?([\w:]+)\s*;", re.M)
INJECT_LINE = re.compile(r"^[ \t]*BOF3_INJECT\((\w+)\);[ \t]*\r?\n", re.M)
STATIC_LOCAL = re.compile(r"\bstatic\b(?!_)")


class Refused(Exception):
    pass


def rel(p: Path) -> str:
    p = p.resolve()
    return p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else str(p)


CR, LF = chr(13), chr(10)
CRLF = CR + LF
_eol: str | None = None


def checkout_eol() -> str:
    """The checkout's line end (core.autocrlf gives CRLF on Windows): every file written uses it."""
    global _eol
    if _eol is None:
        with open(ROOT / "symbols.toml", "rb") as f:
            _eol = CRLF if CRLF.encode() in f.read(65536) else LF
    return _eol


def read(path: str) -> str:
    """A file's text with LF line ends (all offsets and line numbers are over this text)."""
    with open(ROOT / path, encoding="utf-8", newline="") as f:
        return f.read().replace(CRLF, LF)


def write(path: str, text: str) -> None:
    (ROOT / path).parent.mkdir(parents=True, exist_ok=True)
    with open(ROOT / path, "w", encoding="utf-8", newline="") as f:
        f.write(text.replace(CRLF, LF).replace(LF, checkout_eol()))


def camel(stem: str) -> str:
    return "".join(p[:1].upper() + p[1:] for p in stem.split("_") if p)


def header_of(cpp: str) -> str:
    return cpp[:-4] + ".h"


def include_name(path: str) -> str:
    """The include spelling of a src/ path: src/game/x.h -> game/x.h."""
    assert path.startswith("src/"), path
    return path[4:]


def local_header(source: str) -> str:
    return source[:-4] + "_local.h"


def git_head(paths: list[str]) -> str:
    """HEAD, the tree the map's old lines describe - refused when a source differs from it."""
    r = subprocess.run(["git", "rev-parse", "--short=8", "HEAD"], cwd=ROOT, capture_output=True, text=True)
    head = r.stdout.strip()
    if not head:
        raise Refused("not in a git checkout: the map needs the commit its old lines are at")
    d = subprocess.run(["git", "diff", "--name-only", "HEAD", "--"] + paths, cwd=ROOT, capture_output=True, text=True)
    dirty = [l for l in d.stdout.splitlines() if l.strip()]
    if dirty:
        raise Refused("commit these first, so that the map's old lines are at a commit: " + ", ".join(dirty))
    return head


# --- input -------------------------------------------------------------------

@dataclass
class Move:
    name: str
    pc: int
    source: str
    target: str
    unit: str = ""


def load_symbols() -> dict[str, dict]:
    with open(ROOT / "symbols.toml", "rb") as f:
        d = tomllib.load(f)
    return {f["name"]: f for f in d["func"]}


def load_table(path: Path, column: str | None, syms: dict[str, dict], pick) -> list[Move]:
    with open(path, encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"))
    if not rows:
        sys.exit(f"move_funcs: {path} has no rows")
    cols = rows[0].keys()
    col = column or ("file" if "file" in cols else "proposed_file" if "proposed_file" in cols else None)
    if col is None or col not in cols or "name" not in cols:
        sys.exit(f"move_funcs: {path} needs the columns `name` and `file` (or --file-column); it has {', '.join(cols)}")
    moves = []
    bad = []
    for r in rows:
        name, target = (r.get("name") or "").strip(), (r.get(col) or "").strip()
        if not name or not target:
            continue
        f = syms.get(name)
        if f is None or not f.get("impl"):
            bad.append(f"{name}: no [[func]] with an impl in symbols.toml")
            continue
        pc = int(r["pc"], 16) if r.get("pc") else f["pc"]
        m = Move(name, pc, f["impl"], target.replace("\\", "/"), (r.get("unit") or "").strip())
        if not pick(m):
            continue
        if not re.fullmatch(r"src/game/(\w+/)*\w+\.cpp", m.target):
            bad.append(f"{name}: target {m.target} is not src/game/<...>/<stem>.cpp")
            continue
        moves.append(m)
    if bad:
        sys.exit("move_funcs: the table is refused:\n  " + "\n  ".join(bad))
    return moves


# --- the plan for one source file ---------------------------------------------

@dataclass
class SourcePlan:
    source: str
    text: str
    items: ci.FileItems
    funcs: dict[str, ci.Item]                     # moved function name -> its item
    target_of: dict[str, str]                     # moved function name -> target
    moved_locals: dict[str, list[ci.Item]] = field(default_factory=dict)   # target -> local items
    shared: list[ci.Item] = field(default_factory=list)                    # to <S>_local.h
    shared_users: dict[int, set[str]] = field(default_factory=dict)        # id(item) -> owners
    alias_at: str | None = None                   # the target namespace of S's `namespace at = X;`
    alias_item: ci.Item | None = None
    using_dirs: list[ci.Item] = field(default_factory=list)   # `using namespace X;`: every target gets them
    inject_lines: list[str] = field(default_factory=list)


def owners_of(fi: ci.FileItems, roots: list[tuple[ci.Item, str]], local_by_name: dict[str, list[ci.Item]]):
    owners: dict[int, set[str]] = defaultdict(set)
    for item, owner in roots:
        stack = [item]
        seen = {id(item)}
        while stack:
            it = stack.pop()
            for u in it.uses:
                for dep in local_by_name.get(u, ()):
                    if id(dep) in seen or dep is item:
                        continue
                    seen.add(id(dep))
                    owners[id(dep)].add(owner)
                    stack.append(dep)
    return owners


def plan_source(source: str, moves: list[Move], problems: list[str]) -> SourcePlan | None:
    text = read(source)
    fi = ci.FileItems(text)
    leaves = fi.leaves()
    by_name: dict[str, list[ci.Item]] = defaultdict(list)
    for it in leaves:
        if it.kind == "func":
            by_name[it.names[0]].append(it)
    plan = SourcePlan(source, text, fi, {}, {})
    for m in moves:
        found = by_name.get(m.name, [])
        if len(found) != 1:
            problems.append(f"{source}: {m.name}: {'not found' if not found else 'defined twice'} by cpp_items")
            continue
        it = found[0]
        if any(n for n in it.ns):
            problems.append(f"{source}: {m.name} sits in namespace {'::'.join(it.ns)}; only file-scope functions move")
            continue
        if "" in it.ns or it.is_static:
            problems.append(f"{source}: {m.name} is file-local; it moves with its callers, not by the table")
            continue
        if it.conds:
            problems.append(f"{source}: {m.name} sits inside {' / '.join(it.conds)}")
            continue
        if it.linkage:
            problems.append(f"{source}: {m.name} sits inside an extern \"C\" {{ }} block")
            continue
        plan.funcs[m.name] = it
        plan.target_of[m.name] = m.target
    if not plan.funcs:
        return None

    m_alias = None
    local_by_name: dict[str, list[ci.Item]] = defaultdict(list)
    for it in leaves:
        if it.kind == "pp" and it.names:                       # a #define in the .cpp is file-local
            local_by_name[it.names[0]].append(it)
        elif it.kind != "pp" and it.local and it not in plan.funcs.values():
            if it.kind == "using" and not it.names:
                plan.using_dirs.append(it)
                continue
            if it.kind == "alias" and it.names == ["at"]:
                m_alias = ALIAS_AT.search(it.text)
                if m_alias:
                    plan.alias_at, plan.alias_item = m_alias.group(2), it
                    continue                                   # each file has its own `at`
            for n in it.names:
                local_by_name[n].append(it)
    # who reaches what: every moved function for its target, everything else for S
    roots = []
    moved_ids = {id(it) for it in plan.funcs.values()}
    local_ids = {id(i) for lst in local_by_name.values() for i in lst}
    for it in leaves:
        if id(it) in moved_ids:
            roots.append((it, plan.target_of[it.names[0]]))
        elif it.kind != "pp" and id(it) not in local_ids and it is not plan.alias_item:
            roots.append((it, source))
    owners = owners_of(fi, roots, local_by_name)
    seen = set()
    for lst in local_by_name.values():
        for it in lst:
            if id(it) in seen:
                continue
            seen.add(id(it))
            own = owners.get(id(it), set())
            if not own or own == {source}:
                continue
            if len(own) == 1:
                plan.moved_locals.setdefault(next(iter(own)), []).append(it)
                continue
            # reached from two places: a header both include, if it is a value and not state
            if it.is_var and not it.is_const:
                problems.append(f"{source}: {'/'.join(it.names)} (line {it.code_line}) is file-static state reached by "
                                f"{', '.join(sorted(own))}: two copies would split it; decide by hand")
                continue
            if it.kind == "func" and has_static_local(it):
                problems.append(f"{source}: {'/'.join(it.names)} (line {it.code_line}) holds a static local and is "
                                f"reached by {', '.join(sorted(own))}: two copies would split it")
                continue
            if it.conds:
                problems.append(f"{source}: shared {'/'.join(it.names)} sits inside {' / '.join(it.conds)}")
                continue
            plan.shared.append(it)
            plan.shared_users[id(it)] = own
    for t in plan.moved_locals:
        plan.moved_locals[t].sort(key=lambda i: i.start)
    plan.shared.sort(key=lambda i: i.start)
    # a moved function that names a global variable S defines (extern linkage) needs its declaration
    globals_here = {n: it for it in leaves if it.kind == "decl" and it.is_var and not it.local for n in it.names}
    needed = {u for it in plan.funcs.values() for u in it.uses & set(globals_here)}
    declared = declared_in_headers(text, needed)
    for name, it in plan.funcs.items():
        for u in sorted(it.uses & set(globals_here) - declared):
            problems.append(f"{source}: {name} names {u}, a global {source} defines and no header it includes "
                            f"declares; give it a declaration first")
    # the inject lines
    for m in INJECT_LINE.finditer(text):
        if m.group(1) in plan.funcs:
            plan.inject_lines.append(m.group(1))
    missing = set(plan.funcs) - set(plan.inject_lines)
    if missing:
        problems.append(f"{source}: no BOF3_INJECT line for {', '.join(sorted(missing))}")
    return plan


def has_static_local(it: ci.Item) -> bool:
    """A function whose body declares a `static` (one object per copy of the function)."""
    code = it.text[it.code_start - it.start:]
    return bool(STATIC_LOCAL.search(code[code.find("{"):]))


def code_tokens(it: ci.Item) -> str:
    """An item's code without its comments and spacing."""
    return " ".join(t.text for t in ci.lex(it.text) if t.kind != "comment")


def declared_in_headers(text: str, names: set[str]) -> set[str]:
    """Which of `names` an `extern` declaration in a project header `text` includes declares."""
    if not names:
        return set()
    found = set()
    for inc in re.findall(r'^\s*#\s*include\s+"([^"]+)"', text, re.M):
        p = ROOT / "src" / inc
        if not p.exists():
            continue
        h = p.read_text(encoding="utf-8")
        for n in names - found:
            if re.search(rf"\bextern\b[^;]*\b{re.escape(n)}\b", h):
                found.add(n)
    return found


# --- writing ---------------------------------------------------------------------

def strip_spans(text: str, spans: list[tuple[int, int]]) -> str:
    out, pos = [], 0
    for a, b in sorted(spans):
        out.append(text[pos:a])
        pos = b
    out.append(text[pos:])
    s = "".join(out)
    return re.sub(r"\n{3,}", "\n\n", s)


def block(items: list[ci.Item], anon_wrap: bool = True) -> str:
    """Items in order, each in the namespaces it sat in (named and anonymous, reopened as written)
    and under the `#pragma clang attribute push` stack it had."""
    out: list[str] = []
    cur_ns: tuple[str, ...] = ()
    cur_pr: tuple[str, ...] = ()

    def close_to(depth: int) -> None:
        nonlocal cur_ns
        while len(cur_ns) > depth:
            n = cur_ns[-1]
            out.append("}  // namespace" + (f" {n}" if n else "") + "\n\n")
            cur_ns = cur_ns[:-1]

    for it in items:
        ns = it.ns if anon_wrap else tuple(n for n in it.ns if n)
        if it.pragmas != cur_pr:
            close_to(0)
            out.extend("#pragma clang attribute pop\n\n" for _ in cur_pr)
            out.extend(p + "\n\n" for p in it.pragmas)
            cur_pr = it.pragmas
        common = 0
        while common < min(len(ns), len(cur_ns)) and ns[common] == cur_ns[common]:
            common += 1
        close_to(common)
        for n in ns[common:]:
            out.append(f"namespace {n} {{\n\n" if n else "namespace {\n\n")
            cur_ns = cur_ns + (n,)
        out.append(it.text.rstrip("\n") + "\n\n")
    close_to(0)
    out.extend("#pragma clang attribute pop\n\n" for _ in cur_pr)
    return "".join(out)


def includes_of(text: str) -> list[str]:
    return [l.strip() for l in text.splitlines() if re.match(r"\s*#\s*include\b", l)]


def sort_includes(lines: list[str], first: str | None) -> list[str]:
    std = sorted({l for l in lines if "<" in l})
    ours = sorted({l for l in lines if "<" not in l} - ({first} if first else set()))
    out = ([first, ""] if first else []) + std + ([""] if std else []) + ours
    return out


def insert_inject_all(text: str, sources: list[str], target_h: str, target_inject: str) -> str:
    """The include and the call in src/hook/inject_all.cpp, each after the last source's."""
    lines = text.splitlines(keepends=True)
    inc = f'#include "{include_name(target_h)}"\n'
    if inc not in lines:
        idx = [i for i, l in enumerate(lines) for s in sources if l.strip() == f'#include "{include_name(header_of(s))}"']
        if not idx:
            raise Refused(f"inject_all.cpp includes none of {', '.join(header_of(s) for s in sources)}")
        lines.insert(max(idx) + 1, inc)
    call = f"    {target_inject}();\n"
    if call in lines:
        return "".join(lines)
    calls = []
    for s in sources:
        name = source_inject_name(s)
        calls += [i for i, l in enumerate(lines) if re.match(rf"\s*{name}\(\);", l)]
    if not calls:
        raise Refused("inject_all.cpp calls none of the sources' _Inject()")
    i = max(calls) + 1
    while i < len(lines) and re.match(r"\s{8,}//", lines[i]):     # the call's comment continues
        i += 1
    lines.insert(i, call)
    return "".join(lines)


_inject_names: dict[str, str] = {}


def source_inject_name(source: str) -> str:
    if source not in _inject_names:
        fi = ci.FileItems(read(source))
        names = [it.names[0] for it in fi.functions() if it.names[0].endswith("_Inject") and not it.ns]
        if len(names) != 1:
            raise Refused(f"{source}: {len(names)} _Inject() functions")
        _inject_names[source] = names[0]
    return _inject_names[source]


def remove_inject_lines(text: str, names: set[str]) -> str:
    return INJECT_LINE.sub(lambda m: "" if m.group(1) in names else m.group(0), text)


def rewrite_impl(toml: str, new_impl: dict[str, str]) -> str:
    out = []
    block_name = None
    pending = None        # index in out of an impl line seen before the name
    for line in toml.splitlines(keepends=True):
        s = line.strip()
        if s.startswith("[["):
            block_name, pending = None, None
        elif s.startswith("name = ") and block_name is None:
            block_name = s[7:].strip().strip('"')
            if pending is not None and block_name in new_impl:
                out[pending] = f'impl = "{new_impl[block_name]}"\n'
        elif s.startswith("impl = "):
            if block_name is None:
                pending = len(out)
            elif block_name in new_impl:
                line = f'impl = "{new_impl[block_name]}"\n'
        out.append(line)
    return "".join(out)


# --- the map -----------------------------------------------------------------------

def load_map(path: Path) -> list[dict]:
    if not path.exists():
        return []
    with open(path, encoding="utf-8", newline="") as f:
        lines = [l for l in f if not l.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t"))


MAP_HEADER = ("# analysis/tu_moves.tsv: where each function and file-local item of src/game moved in the\n"
              "# source-layout round (docs/source-layout-round.md; tools/move_funcs.py writes it,\n"
              "# tools/cite_lookup.py reads it). old_* are lines of old_path in the tree of `base`.\n")


def save_map(path: Path, rows: list[dict]) -> None:
    rows.sort(key=lambda r: (r["old_path"], int(r["old_first"] or 0), r["name"]))
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(MAP_HEADER)
        f.write("\t".join(MAP_COLUMNS) + "\n")
        for r in rows:
            f.write("\t".join(str(r.get(c, "")) for c in MAP_COLUMNS) + "\n")


def map_row(pc, name, kind, unit, old_path, it: ci.Item, new_path, base) -> dict:
    return {"pc": f"0x{pc:X}" if pc is not None else "", "name": name, "kind": kind, "unit": unit,
            "old_path": old_path, "old_first": it.first_line, "old_def": it.code_line, "old_last": it.last_line,
            "new_path": new_path, "base": base}


def merge_rows(rows: list[dict], new: list[dict]) -> list[dict]:
    index = {(r["name"], r["kind"]): r for r in rows}
    for n in new:
        # a row already there keeps the tree before the round; only its new path follows
        old = index.get((n["name"], n["kind"]))
        if old and old["kind"] == "func":
            old["new_path"] = n["new_path"]
            continue
        key = [r for r in rows if r["name"] == n["name"] and r["old_path"] == n["old_path"] and r["kind"] == n["kind"]
               and (n["kind"] != "copied" or r["new_path"] == n["new_path"])]
        if key:
            key[0]["new_path"] = n["new_path"]
            continue
        rows.append(n)
        index[(n["name"], n["kind"])] = n
    return rows


# --- the run ---------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("table", type=Path)
    ap.add_argument("--file-column")
    ap.add_argument("--names", help="comma-separated: move only these functions")
    ap.add_argument("--range", help="LO..HI (hex pcs, HI exclusive): move only these")
    ap.add_argument("--unit", help="move only this unit's rows")
    ap.add_argument("--check", action="store_true", help="plan and print; write nothing")
    ap.add_argument("--map-only", action="store_true", help="write the map's rows for the moves; move nothing")
    ap.add_argument("--map", type=Path, default=ROOT / "analysis" / "tu_moves.tsv")
    ap.add_argument("--shared", choices=("copy", "header"), default="copy",
                    help="a file-local item reached from two files: copied into each (default), or to <S>_local.h")
    a = ap.parse_args()

    names = set(a.names.split(",")) if a.names else None
    lo = hi = None
    if a.range:
        x, y = a.range.split("..")
        lo, hi = int(x, 16), int(y, 16)

    def pick(m: Move) -> bool:
        return ((names is None or m.name in names) and (lo is None or lo <= m.pc < hi)
                and (a.unit is None or m.unit == a.unit))

    syms = load_symbols()
    moves = load_table(a.table, a.file_column, syms, pick)
    todo = [m for m in moves if m.source != m.target]
    done = len(moves) - len(todo)
    if not todo:
        print(f"move_funcs: nothing to move ({done} rows already in their file)")
        return 0

    by_source: dict[str, list[Move]] = defaultdict(list)
    for m in todo:
        by_source[m.source].append(m)
    problems: list[str] = []
    plans = {s: plan_source(s, ms, problems) for s, ms in sorted(by_source.items())}
    plans = {s: p for s, p in plans.items() if p}
    move_of = {m.name: m for m in todo}
    base = git_head(sorted(plans)) if not a.check else "check"

    # --- assemble each target --------------------------------------------------
    targets: dict[str, dict] = {}
    for s, p in plans.items():
        for name, t in p.target_of.items():
            e = targets.setdefault(t, {"funcs": [], "locals": [], "sources": [], "aliases": {}, "shared_from": set()})
            e["funcs"].append((move_of[name].pc, name, p.funcs[name], s))
            if s not in e["sources"]:
                e["sources"].append(s)
            if p.alias_at:
                e["aliases"][s] = p.alias_at
        for t, its in p.moved_locals.items():
            targets[t]["locals"] += [(s, it, "local") for it in its]
        for it in p.shared:
            for t in sorted(p.shared_users[id(it)] - {s}):
                if a.shared == "copy":
                    targets[t]["locals"].append((s, it, "copied"))
                else:
                    targets[t]["shared_from"].add(s)
    for e in targets.values():
        e["locals"].sort(key=lambda x: (e["sources"].index(x[0]), x[1].start))

    # name clashes inside a target: two different items of one name
    for t, e in sorted(targets.items()):
        if (ROOT / t).exists():
            problems.append(f"{t} exists: adding to an existing file is not built yet; move into a new file")
            continue
        seen: dict[str, tuple[str, str]] = {}
        provided = list(e["locals"])
        if a.shared == "header":       # what the _local.h headers bring is in the file too
            provided = [(s, it, "header") for s in sorted(e["shared_from"]) for it in plans[s].shared] + provided
        kept = []
        for s, it, how in provided:
            body = code_tokens(it)       # the same code with other comments or spacing is the same item
            scope = "::".join(n for n in it.ns if n)
            keys = [(scope, n) for n in it.names]
            clash = [k for k in keys if k in seen and seen[k][1] != body]
            if clash:
                problems.append(f"{t}: {', '.join('::'.join(filter(None, k)) for k in clash)} from {s} (line "
                                f"{it.code_line}) differs from the one from {seen[clash[0]][0]}; rename by hand or "
                                f"keep the two apart")
                continue
            if keys and all(k in seen for k in keys):
                if how == "header" and any(seen[k][2] == "header" for k in keys):
                    problems.append(f"{t}: {'/'.join(it.names)} would come twice through the _local.h headers; "
                                    f"use --shared copy")
                continue            # the same item from another source: once
            for k in keys:
                seen[k] = (s, body, how)
            if how != "header":
                kept.append((s, it, how))
        e["locals"] = kept
        for pc, name, it, s in e["funcs"]:
            if ("", name) in seen:
                problems.append(f"{t}: {name} is already defined there")

    if problems:
        print("move_funcs: refused, nothing written:")
        for p in problems:
            print("  " + p)
        return 1

    # --- the map rows --------------------------------------------------------------
    rows = []
    for s, p in plans.items():
        for name, it in p.funcs.items():
            m = move_of[name]
            rows.append(map_row(m.pc, name, "func", m.unit, s, it, m.target, base))
        for t, its in p.moved_locals.items():
            for it in its:
                rows.append(map_row(None, "/".join(it.names), "local", "", s, it, t, base))
        for it in p.shared:
            if a.shared == "header":
                rows.append(map_row(None, "/".join(it.names), "shared", "", s, it, local_header(s), base))
            else:
                for t in sorted(p.shared_users[id(it)] - {s}):
                    rows.append(map_row(None, "/".join(it.names), "copied", "", s, it, t, base))

    if a.check or a.map_only:
        for t, e in sorted(targets.items()):
            print(f"{t}: {len(e['funcs'])} functions from {', '.join(e['sources'])}; "
                  f"{sum(1 for x in e['locals'] if x[2] == 'local')} file-local items moved, "
                  f"{sum(1 for x in e['locals'] if x[2] == 'copied')} copied; "
                  f"`at` {' + '.join(sorted(set(e['aliases'].values()))) or '-'}")
        for s, p in plans.items():
            if p.shared:
                how = f"to {local_header(s)}" if a.shared == "header" else "copied, the original kept"
                print(f"{s}: {len(p.shared)} shared items ({how}): " + ", ".join("/".join(i.names) for i in p.shared))
        if a.map_only:
            save_map(a.map, merge_rows(load_map(a.map), rows))
            print(f"move_funcs: {len(rows)} map rows written to {rel(a.map)}")
        return 0

    # --- write the sources ------------------------------------------------------------
    for s, p in plans.items():
        spans = [(it.start, it.end) for it in p.funcs.values()]
        spans += [(it.start, it.end) for its in p.moved_locals.values() for it in its]
        shared = p.shared if a.shared == "header" else []
        text = p.text
        if shared:
            lh = local_header(s)
            write_local_header(lh, s, p, shared)
            first = shared[0]
            # the include goes where the first shared item was, at file scope
            inc = f'#include "{include_name(lh)}"\n'
            if "" in first.ns:
                inc = "}  // namespace\n\n" + inc + "\nnamespace {\n"
            spans += [(it.start, it.end) for it in shared[1:]]
            marker = "\x00INC\x00"
            text = text[:first.start] + marker + text[first.end:]
            delta = len(marker) - (first.end - first.start)
            spans = [(x + delta, y + delta) if x >= first.end else (x, y) for x, y in spans]
            text = strip_spans(text, spans).replace(marker, inc)
        else:
            text = strip_spans(text, spans)
        text = remove_inject_lines(text, set(p.inject_lines))
        text = re.sub(r"namespace \{\n\s*\}  // namespace\n\n?", "", text)
        write(s, text)

    # --- write the targets -------------------------------------------------------------
    inject_all = read("src/hook/inject_all.cpp")
    for t, e in sorted(targets.items()):
        e["funcs"].sort()
        write_target(t, e, plans)
        inject_all = insert_inject_all(inject_all, e["sources"], header_of(t), target_inject_name(t))
    write("src/hook/inject_all.cpp", inject_all)

    write("symbols.toml", rewrite_impl(read("symbols.toml"), {m.name: m.target for m in todo}))
    save_map(a.map, merge_rows(load_map(a.map), rows))
    print(f"move_funcs: moved {len(todo)} functions into {len(targets)} files ({done} already there); "
          f"map {rel(a.map)}")
    for s, p in plans.items():
        if p.shared:
            how = f"through {local_header(s)}" if a.shared == "header" else "copied, the original kept"
            print(f"  shared from {s} ({how}): " + ", ".join("/".join(i.names) for i in p.shared))
    fuzz = fuzz_rows(set(move_of))
    if fuzz:
        print(f"  fuzz rows that name a moved function stay in their group's file (stage C): "
              f"{len(fuzz)} lines in {len({f for f, _ in fuzz})} files")
        for f, n in fuzz:
            print(f"    {f}:{n}")
    return 0


def target_inject_name(t: str) -> str:
    return camel(Path(t).stem) + "_Inject"


def write_local_header(lh: str, s: str, p: SourcePlan, shared: list[ci.Item]) -> None:
    incs = [l for l in includes_of(p.text) if include_name(lh) not in l]
    if (ROOT / lh).exists():
        old = read(lh)
        have = {it.text for it in ci.FileItems(old).leaves()}
        add = [it for it in shared if it.text not in have]
        if not add:
            return
        write(lh, old.rstrip("\n") + "\n\n" + block(add, True))
        return
    head = (f"// File-local items of {Path(s).name} that a moved function shares with code that stays there or\n"
            f"// moved elsewhere (tools/move_funcs.py, docs/source-layout-round.md 2): each keeps its own spelling\n"
            f"// and internal linkage, one copy per including file. A file including this one defines its\n"
            f"// `namespace at` first.\n#pragma once\n\n")
    write(lh, head + "\n".join(sort_includes(incs, None)) + "\n\n" + block(shared, True))


def write_target(t: str, e: dict, plans: dict[str, SourcePlan]) -> None:
    th = header_of(t)
    stem = Path(t).stem
    inject = target_inject_name(t)
    aliases = sorted(set(e["aliases"].values()))
    exists = (ROOT / t).exists()
    if exists:
        raise Refused(f"{t} exists: adding to an existing file is not built yet (move into a new file, "
                      f"or extend write_target)")
    incs: list[str] = []
    for s in e["sources"]:
        incs += includes_of(plans[s].text)
    shared_hdrs = [f'#include "{include_name(local_header(s))}"' for s in sorted(e["shared_from"])]
    callees = None
    if len(aliases) > 1:
        callees = t[:-4] + "_callees.h"
        write(callees, "// The callees constants this file's functions name as at::k...: the union of their\n"
                       "// groups' namespaces (tools/move_funcs.py, docs/source-layout-round.md 2). The constants\n"
                       "// stay in the groups' headers; a name two of them define differently is refused.\n"
                       "#pragma once\n\n"
                       + "".join(f'#include "{include_name(header_of(s)[:-2] + "_callees.h")}"\n'
                                 for s in e["sources"] if s in e["aliases"])
                       + f"\nnamespace {stem}::at {{\n"
                       + "".join(f"using namespace ::{x};\n" for x in aliases)
                       + f"}}  // namespace {stem}::at\n")
        incs.append(f'#include "{include_name(callees)}"')
    src_names = ", ".join(Path(s).name for s in e["sources"])
    out = [f"// Moved here by tools/move_funcs.py from {src_names} (docs/source-layout-round.md).\n"]
    out += [l + "\n" for l in sort_includes(incs, f'#include "{include_name(th)}"')]
    out.append("\n")
    if aliases:
        target_ns = f"{stem}::at" if len(aliases) > 1 else aliases[0]
        out.append(f"namespace {{\n\nnamespace at = {target_ns};\n\n}}  // namespace\n\n")
    for h in shared_hdrs:
        out.append(h + "\n")
    if shared_hdrs:
        out.append("\n")
    dirs = []
    for s in e["sources"]:
        for it in plans[s].using_dirs:
            if it.text.strip() not in {d.text.strip() for d in dirs}:
                dirs.append(it)
    out.append(block(dirs, True))
    out.append(block([it for _, it, _ in e["locals"]], True))
    out.append(block([it for _, _, it, _ in e["funcs"]], True))
    out.append(f"void {inject}() {{\n")
    for _, name, _, _ in e["funcs"]:
        out.append(f"    BOF3_INJECT({name});\n")
    out.append("}\n")
    write(t, re.sub(r"\n{3,}", "\n\n", "".join(out)))
    write(th, f"// tools/move_funcs.py (docs/source-layout-round.md). The functions of {Path(t).name} are declared\n"
              f"// by symbols.gen.h (each has an `impl` in symbols.toml).\n#pragma once\n\nvoid {inject}();\n")


def fuzz_rows(names: set[str]) -> list[tuple[str, int]]:
    out = []
    rx = re.compile(r"\b(" + "|".join(sorted(map(re.escape, names))) + r")\b")
    for f in sorted((ROOT / "src" / "game").rglob("*_fuzz.cpp")):
        for n, line in enumerate(f.read_text(encoding="utf-8").splitlines(), 1):
            if rx.search(line):
                out.append((rel(f), n))
    return out


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Refused as e:
        print(f"move_funcs: refused: {e}")
        sys.exit(1)
