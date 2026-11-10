"""Empty src/game of the old group files (the source-layout round, stage C).

Stage B moved every function of Capcom's out of the flat `src/game/<group>.cpp`
files into the files of its recovered translation units, under the fourteen
folders (docs/source-layout-round.md 4). What it left flat is of three kinds,
and this tool gives each its place (round doc 5, "Stage C as run"):

1. **A fuzz file** (`<group>_fuzz.cpp`, the group's start-up differential
   fuzz) moves whole, unchanged inside, to `src/game/<folder>/<group>_fuzz.cpp`.

2. **A remainder** - a group file that still holds code of ours: a divergence
   switch and its arming in `_Inject()`, a self-test written in the file, a
   module that never held a function of Capcom's - moves whole, unchanged
   inside, to `src/game/<folder>/<group>.cpp` (the owner's call of
   2026-11-09: "move whole, dissolve later"). Its header stays where it is:
   every `#include "game/<group>.h"` keeps working. Where the folder already
   holds a unit file of that name, or another folder does, the remainder is
   `<group>_ours.cpp`.

3. **A pure shell** - nothing left but an `_Inject()` that is empty or is
   `if (bof3::WantsShadow("g")) g::SelfTest();`, perhaps the group's callee
   seam, and file-local helpers nothing reaches - is retired:
   - its seam (`Callees g = kOriginals;`, the table and what the table's
     initialiser reaches in the shell) moves, verbatim, into the new file
     that holds the group's lowest-`pc` function (the owner's call C1),
     inside `namespace <group>`; a file-local the home already holds with
     the same text is not copied;
   - its self-test line replaces its `<Group>_Inject();` call in
     src/hook/inject_all.cpp, where it stood, so the order of the self-tests
     against the binds is kept; an empty `_Inject()`'s call goes;
   - its header is deleted when it declared nothing but the `_Inject()` and
     the `SelfTest()` (every `#include` of it removed, the self-test declared
     in inject_all.cpp's block); a header that declares more keeps the rest;
   - the shell is deleted.

The folder of a group is the folder most of its functions moved to
(analysis/tu_moves.tsv; the lowest-pc function's folder breaks a tie); the
modules that never held a function of Capcom's are in OURS below.

The map gains a row for each: kind `file` (a whole file moved: old_path,
new_path), `seam` (name `<group>::g`, to its home) and `shell` (deleted;
new_path empty). tools/game_sources.py gives a moved file its old place in
the link order through the `file` rows; tools/cite_lookup.py answers an old
path through them.

    python tools/retire_shells.py --check            # the plan, nothing written
    python tools/retire_shells.py --check --strict   # CI: no flat src/game/*.cpp but the harness files and the runtime's
    python tools/retire_shells.py --moves            # kinds 1 and 2
    python tools/retire_shells.py --shells [--only a,b]   # kind 3

Refused before anything is written: uncommitted changes under src/game or in
the files this edits; a flat file that defines a function with an `impl`; a
pure shell whose inject call is not found exactly once in inject_all.cpp.
Idempotent: what is done is no longer flat, so a second run finds nothing.
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import subprocess
import sys
import tomllib
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cpp_items as ci  # noqa: E402
import move_funcs as mf  # noqa: E402

ROOT = mf.ROOT
GAME = "src/game"
INJECT_ALL = "src/hook/inject_all.cpp"
MAP = ROOT / "analysis" / "tu_moves.tsv"

# The harness files: shared by many groups, not a group's own. They stay flat.
HARNESS = {"area_harness", "boss_harness", "boss_harness_eh", "magic_harness", "scenario_harness",
           "scenario_harness_ekh", "scenario_harness_fh"}
# The runtime's file: its functions' range is outside the round (round doc 9; analysis/tu_layout.tsv
# class `outside`), so it and its fuzz stay where they are.
OUTSIDE = {"crt_rest", "crt_rest_fuzz"}
STAY = HARNESS | OUTSIDE

# Modules of ours that never held a function of Capcom's: no move says where they belong.
OURS = {
    "area_data_reload": "core", "band_census": "field", "battle_text": "battle", "char_names": "text",
    "cheats": "core", "crt_rest": "platform", "d3d_lines": "platform", "dat_cache": "platform",
    "faerie_ring": "field", "fishing_text": "text", "game_clock": "platform", "gfx_filter": "platform",
    "labels": "text", "lang_layout": "text", "layering": "field", "map_rim": "field", "menu_frame": "menu",
    "menu_verbs": "menu", "music_loops": "platform", "music_seq": "platform", "name_tables": "text",
    "pause_text": "text", "pillarbox": "platform", "psp_code": "core", "quick_menu": "menu",
    "rdata_consts": "platform", "shoulder_glyphs": "text", "sound_in": "platform", "sound_sp": "platform",
    "sound_st": "platform", "text_advance": "text", "text_pairs": "text", "title_menu": "core",
    "wide_rand": "field", "wide_staging": "field", "widescreen": "platform", "yes_no_layout": "menu",
    # two fuzz files with no group file of their own (d3d_fuzz.cpp, ddraw_fuzz.cpp)
    "d3d": "platform", "ddraw": "platform",
}

BLOCK_BEGIN = "// retire_shells: self-tests begin (tools/retire_shells.py keeps these lines)"
BLOCK_END = "// retire_shells: self-tests end"
SELFTEST = re.compile(r'if\s*\(bof3::WantsShadow\("(\w+)"\)\)\s*\{?\s*([\w:]+)::SelfTest\(\);\s*\}?')


def git(*args: str) -> str:
    r = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, encoding="utf-8")
    if r.returncode != 0:
        raise mf.Refused(f"git {' '.join(args)}: {r.stderr.strip()}")
    return r.stdout


def strip_comments(text: str) -> str:
    return re.sub(r"/\*.*?\*/", "", re.sub(r"//[^\n]*", "", text), flags=re.S)


class Shell:
    """One flat group file as it is now."""

    def __init__(self, stem: str, path: str | None = None):
        self.stem = stem
        self.path = path or f"{GAME}/{stem}.cpp"
        self.src = mf.read(self.path)
        self.fi = ci.FileItems(self.src)
        self.leaves = list(self.fi.leaves())
        self.injects = []          # (name, kind, item): kind empty | selftest | other
        self.ext_funcs: list[str] = []
        self.ext_vars: list[str] = []
        self.g = None              # the seam's `g`
        for it in self.leaves:
            if it.kind == "func" and not it.local:
                name = it.names[0] if it.names else "?"
                if name.endswith("_Inject") and not it.ns:
                    body = self.src[it.code_start:it.end]
                    code = strip_comments(body[body.index("{") + 1:body.rindex("}")]).strip()
                    kind = "empty" if not code else "selftest" if SELFTEST.fullmatch(code) else "other"
                    self.injects.append((name, kind, it, code))
                else:
                    self.ext_funcs.append(name)
            elif it.kind == "decl" and it.is_var and not it.local and not it.is_extern:
                if it.names == ["g"] and it.ns == (stem,):
                    self.g = it
                else:
                    self.ext_vars.append(",".join(it.names))

    @property
    def pure(self) -> bool:
        return (not self.ext_funcs and not self.ext_vars and len(self.injects) <= 1
                and all(k in ("empty", "selftest") for _, k, _, _ in self.injects))

    def seam_items(self) -> tuple[list[ci.Item], list[ci.Item]]:
        """(the seam's own items in namespace <stem>, the file-locals they reach), each in file order."""
        defs: dict[str, ci.Item] = {}
        for it in self.leaves:
            if it.kind == "pp" and not it.names:
                continue
            for n in it.names:
                defs.setdefault(n, it)
        seen: list[ci.Item] = [self.g]
        todo = [self.g]
        while todo:
            it = todo.pop()
            for u in sorted(it.uses):
                d = defs.get(u)
                if d is not None and d not in seen and not (d.kind == "func" and d.names[0].endswith("_Inject")):
                    seen.append(d)
                    todo.append(d)
        seen.sort(key=lambda it: it.start)
        own = [it for it in seen if it.ns == (self.stem,)]
        return own, [it for it in seen if it not in own]


def flat_stems() -> list[str]:
    return sorted(p.stem for p in (ROOT / GAME).glob("*.cpp"))


def load_folders() -> tuple[dict[str, str], dict[str, tuple[int, str]]]:
    """group stem -> folder; group stem -> (lowest pc, the file that holds it now)."""
    votes: dict[str, Counter] = defaultdict(Counter)
    low: dict[str, tuple[int, str]] = {}
    for r in mf.load_map(MAP):
        if r["kind"] != "func" or not r["pc"]:
            continue
        stem = Path(r["old_path"]).stem
        if not r["new_path"].startswith(GAME + "/") or r["new_path"].count("/") != 3:
            continue
        votes[stem][r["new_path"].split("/")[2]] += 1
        pc = int(r["pc"], 16)
        if stem not in low or pc < low[stem][0]:
            low[stem] = (pc, r["new_path"])
    folders = {}
    for stem, c in votes.items():
        best = max(c.values())
        tied = sorted(f for f, n in c.items() if n == best)
        folders[stem] = low[stem][1].split("/")[2] if len(tied) > 1 and low[stem][1].split("/")[2] in tied else tied[0]
    for stem, f in OURS.items():
        folders.setdefault(stem, f)
    return folders, low


def impl_paths() -> set[str]:
    with open(ROOT / "symbols.toml", "rb") as f:
        return {fn["impl"] for fn in tomllib.load(f).get("func", []) if fn.get("impl")}


def unit_basenames() -> dict[str, list[str]]:
    out: dict[str, list[str]] = defaultdict(list)
    for p in (ROOT / GAME).glob("*/*.cpp"):
        out[p.stem].append(p.parent.name)
    return out


def file_row(old: str, new: str, kind: str, base: str, name: str = "", first: int = 1, last: int | None = None) -> dict:
    n = last if last is not None else mf.read(old).count("\n") + 1
    return {"pc": "", "name": name, "kind": kind, "unit": "", "old_path": old, "old_first": first, "old_def": first,
            "old_last": n, "new_path": new, "base": base}


def add_rows(new: list[dict]) -> None:
    rows = mf.load_map(MAP)
    have = {(r["kind"], r["name"], r["old_path"]) for r in rows}
    rows += [r for r in new if (r["kind"], r["name"], r["old_path"]) not in have]
    mf.save_map(MAP, rows)


# --- kinds 1 and 2: whole files ------------------------------------------------------

def plan_moves(folders: dict[str, str], problems: list[str]) -> list[tuple[str, str]]:
    units = unit_basenames()
    impls = impl_paths()
    moves = []
    for stem in flat_stems():
        if stem in STAY:
            continue
        path = f"{GAME}/{stem}.cpp"
        if path in impls:
            problems.append(f"{path}: still the impl of a function (stage B is not done for it)")
            continue
        group = stem[:-5] if stem.endswith("_fuzz") else stem
        if not stem.endswith("_fuzz") and Shell(stem).pure:
            continue
        folder = folders.get(group)
        if folder is None:
            problems.append(f"{path}: no folder (no function of {group} moved, and it is not in OURS)")
            continue
        name = stem + ("_ours" if stem in units else "")
        moves.append((path, f"{GAME}/{folder}/{name}.cpp"))
    return moves


def do_moves(moves: list[tuple[str, str]], base: str) -> None:
    rows = []
    for old, new in moves:
        rows.append(file_row(old, new, "file", base))
    for old, new in moves:
        git("mv", old, new)
    add_rows(rows)


# --- kind 3: pure shells ---------------------------------------------------------------

def header_rest(stem: str, inject: str | None) -> tuple[str, bool]:
    """(what the group header declares beyond its _Inject() and SelfTest(), whether it includes anything)."""
    t = strip_comments(mf.read(f"{GAME}/{stem}.h")).replace("#pragma once", "")
    has_include = "#include" in t
    if inject:
        t = re.sub(rf"void\s+{inject}\(\);", "", t)
    t = re.sub(r"namespace\s+\w+\s*\{\s*void SelfTest\(\);\s*\}", "", t)
    return t.strip(), has_include


def declares_seam(text: str, stem: str) -> bool:
    """The home sees the seam's declaration: it includes game/<stem>_callees.h, or game/<stem>.h when that
    header includes the callees header or declares the seam itself (`extern Callees g;`, sprite_pose's)."""
    if f'#include "game/{stem}_callees.h"' in text:
        return True
    own = ROOT / f"{GAME}/{stem}.h"
    if f'#include "game/{stem}.h"' not in text or not own.is_file():
        return False
    h = own.read_text(encoding="utf-8")
    return f'#include "game/{stem}_callees.h"' in h or re.search(r"\bextern\s+\w+\s+g\s*;", h) is not None


def insert_seam(shell: Shell, home: str) -> tuple[str, list[str], list[str]]:
    """The home's new text with the seam in; the names copied; the names the home already held."""
    own, deps = shell.seam_items()
    text = mf.read(home)
    hfi = ci.FileItems(text)
    hleaves = list(hfi.leaves())
    by_name: dict[str, list[ci.Item]] = {}
    for it in hleaves:
        for n in it.names:
            by_name.setdefault(n, []).append(it)

    def code(src: str, it: ci.Item) -> str:
        return re.sub(r"\s+", " ", strip_comments(src[it.code_start:it.end])).strip()

    copy, held, after = [], [], 0
    for d in deps:
        if d.kind == "alias" and re.fullmatch(rf"namespace\s+(\w+)\s*=\s*(::)?{shell.stem}::\1\s*;",
                                              strip_comments(shell.src[d.code_start:d.end]).strip()):
            continue                  # `namespace at = <group>::at;`: inside namespace <group> it is `at` already
        # a file-scope declaration (`extern "C" ... BOF3_ASM_NAME(...);`) the home holds with the same text is
        # held too: the seam needs only the name declared (the cleanup round, group K2's sound seam)
        same = [h for h in by_name.get(d.names[0], [])
                if (h.local or (d.kind == h.kind == "decl" and not d.ns and not h.ns)) and code(text, h) == code(shell.src, d)
                and (not h.ns or h.ns == ("",) or h.ns[0] == shell.stem)] if d.names else []
        if same:
            held.append(d.names[0])
            after = max(after, same[0].end)
        else:
            # the copy goes into namespace <group>'s anonymous namespace: another item of its name there clashes
            if d.names and any(h.ns and h.ns[0] == shell.stem for h in by_name.get(d.names[0], [])):
                raise mf.Refused(f"{home} holds another `{d.names[0]}` in namespace {shell.stem}: the seam's copy would clash")
            if not d.local and not d.ns:
                raise mf.Refused(f"the seam reaches `{d.names[0]}`, a file-scope declaration {home} does not hold as written")
            copy.append(d)
    if (not declares_seam(text, shell.stem)) and "Callees" in shell.src[own[0].start:own[0].end]:
        raise mf.Refused(f"{home} does not include game/{shell.stem}_callees.h, which declares the seam it would hold")
    first_fn = next((it.start for it in hleaves if it.kind == "func" and not it.local and not it.ns), len(text))
    # after a held helper that sits inside a namespace block: go to the block's end
    at = first_fn
    if after > first_fn:
        at = after
    for top in hfi.items:
        if top.start < at < top.end and top.kind in ("namespace", "linkage"):
            at = top.end
    block = [f"// {shell.stem}'s callee seam, moved here from {shell.stem}.cpp by tools/retire_shells.py\n"
             f"// (docs/source-layout-round.md 5): the group's lowest function lives in this file.\n",
             f"namespace {shell.stem} {{\n"]
    if copy:
        block.append("namespace {\n\n")
        block += [shell.src[d.start:d.end] for d in copy]
        block.append("\n}  // namespace\n")
    block.append("\n")
    block += [shell.src[it.start:it.end] for it in own]
    block.append(f"\n}}  // namespace {shell.stem}\n\n")
    return text[:at] + "".join(block) + text[at:], [d.names[0] for d in copy if d.names], held


def edit_inject_all(text: str, shell: Shell, drop_include: bool, declare: str | None) -> str:
    if shell.injects:
        name, kind, _, code = shell.injects[0]
        calls = re.findall(rf"\b{name}\(\);", text)
        if len(calls) != 1:
            raise mf.Refused(f"{INJECT_ALL}: {len(calls)} calls of {name}()")
        if kind == "selftest":
            m = SELFTEST.fullmatch(code)
            text = re.sub(rf"\b{name}\(\);", f'if (bof3::WantsShadow("{m.group(1)}")) {m.group(2)}::SelfTest();', text)
        else:
            # the call's line goes with the comment lines that continue it
            lines = text.splitlines(keepends=True)
            i = next(i for i, l in enumerate(lines) if re.search(rf"\b{name}\(\);", l))
            if not re.fullmatch(rf"\s*{name}\(\);\s*(//.*)?\n?", lines[i]):
                raise mf.Refused(f"{INJECT_ALL}:{i + 1}: {name}() shares its line with other code")
            j = i + 1
            while j < len(lines) and re.match(r"\s{8,}//", lines[j]):
                j += 1
            text = "".join(lines[:i] + lines[j:])
    if drop_include:
        text = text.replace(f'#include "game/{shell.stem}.h"\n', "")
    if declare:
        line = f"namespace {declare} {{ void SelfTest(); }}\n"
        if BLOCK_BEGIN not in text:
            lines = text.splitlines(keepends=True)
            last = max(i for i, l in enumerate(lines) if l.startswith("#include"))
            lines.insert(last + 1, "\n// The start-up self-tests of the groups whose own files the source-layout round retired:\n"
                                   "// each is defined in its group's <folder>/<group>_fuzz.cpp and called below where the\n"
                                   "// group's _Inject() called it.\n" + BLOCK_BEGIN + "\n" + BLOCK_END + "\n")
            text = "".join(lines)
        b, e = text.index(BLOCK_BEGIN) + len(BLOCK_BEGIN) + 1, text.index(BLOCK_END)
        decls = sorted(set(text[b:e].splitlines(keepends=True)) | {line})
        text = text[:b] + "".join(decls) + text[e:]
    return text


def includers(include: str) -> list[str]:
    out = []
    for p in list((ROOT / "src").rglob("*.cpp")) + list((ROOT / "src").rglob("*.h")):
        if include.encode() in p.read_bytes():
            out.append(p.relative_to(ROOT).as_posix())
    return sorted(out)


# --- the seams still in a file of ours (the cleanup round, IDEAS I60) ------------------------

def ours_with_seam() -> list[Shell]:
    """The files that moved whole (the map's file rows) and still define their group's `g`."""
    out = []
    for r in mf.load_map(MAP):
        if r["kind"] != "file" or r["new_path"].endswith("_fuzz.cpp") or not (ROOT / r["new_path"]).is_file():
            continue
        sh = Shell(Path(r["old_path"]).stem, r["new_path"])
        if sh.g is not None:
            out.append(sh)
    return out


def alone_on_its_lines(src: str, it: ci.Item) -> bool:
    """The item's lines hold nothing but the item (not `}  namespace x { Callees g = kOriginals; }`)."""
    lead = src[src.rfind("\n", 0, it.code_start) + 1:it.code_start]
    body = strip_comments(src[it.code_start:it.end]).rstrip()
    return not lead.strip() and body.endswith(";") and body.count(";") >= 1 and not re.search(r";\s*\S", body.split("\n")[-1])


def plan_seam(sh: Shell, low: dict[str, tuple[int, str]]) -> tuple[str, list[ci.Item], str | None]:
    """(home, the items that leave the file, why not)."""
    if sh.stem not in low:
        return "", [], "no function of the group moved: nothing says where the seam's home is"
    home = low[sh.stem][1]
    if home == sh.path:
        return home, [], "the group's lowest function is in this file"
    own, _ = sh.seam_items()
    for it in own:
        if not alone_on_its_lines(sh.src, it):
            return home, [], f"line {it.code_line}: the seam shares its line with other code"
    # an item of the seam that other code of the file reaches stays as a copy; it must be a constant
    others = [it for it in sh.leaves if it not in own]
    leave = []
    for it in own:
        used = any(set(it.names) & o.uses for o in others)
        # the group's callees header declares the table extern: one definition, so it moves
        hdr = ROOT / f"{GAME}/{sh.stem}_callees.h"
        declared = hdr.is_file() and re.search(rf"extern\s[^;]*{it.names[0]}\s*;", hdr.read_text(encoding="utf-8"))
        if it is sh.g or not used or declared:
            leave.append(it)
        elif not it.is_const:
            return home, [], f"{it.names[0]} is state the file's other code reaches"
    return home, leave, None


def do_seam(sh: Shell, home: str, leave: list[ci.Item], base: str) -> dict:
    text, copied, held = insert_seam(sh, home)
    mf.write(home, text)
    src = sh.src
    for it in sorted(leave, key=lambda it: -it.start):
        src = src[:it.start] + src[it.end:]
    # a namespace block the seam leaves empty goes with it
    src = re.sub(rf"namespace {sh.stem} \{{\n\s*\}}  // namespace {sh.stem}\n\n?", "", src)
    mf.write(sh.path, src)
    own, _ = sh.seam_items()
    print(f"  seam {sh.stem}::g: {sh.path} -> {home}" + (f"; copied {', '.join(copied)}" if copied else "")
          + (f"; the home held {', '.join(held)}" if held else ""))
    return file_row(sh.path, home, "seam", base, f"{sh.stem}::g", own[0].first_line, own[-1].last_line)


class ShellPlan:
    def __init__(self, shell: Shell, low: dict[str, tuple[int, str]]):
        self.shell = shell
        s = shell.stem
        self.inject = shell.injects[0][0] if shell.injects else None
        self.kind = shell.injects[0][1] if shell.injects else "none"
        self.home = low[s][1] if shell.g is not None and s in low else None
        if shell.g is not None and self.home is None:
            raise mf.Refused(f"{shell.path}: a seam and no moved function to say where its home is")
        self.has_header = (ROOT / f"{GAME}/{s}.h").exists()
        rest, has_include = header_rest(s, self.inject) if self.has_header else ("", False)
        self.delete_header = self.has_header and not rest and not has_include
        self.keep_header_rest = self.has_header and not self.delete_header
        m = SELFTEST.fullmatch(shell.injects[0][3]) if self.kind == "selftest" else None
        self.selftest_ns = m.group(2) if m else None

    def line(self) -> str:
        s = self.shell
        bits = [f"{s.path}: {self.kind}"]
        if self.home:
            own, deps = s.seam_items()
            bits.append(f"seam -> {self.home} (reaches {', '.join(d.names[0] for d in deps if d.names) or 'nothing local'})")
        bits.append("header deleted" if self.delete_header else "header kept (declares more)" if self.keep_header_rest else "no header")
        return "; ".join(bits)


def do_shell(p: ShellPlan, base: str) -> list[dict]:
    s = p.shell
    rows = []
    if p.home:
        text, copied, held = insert_seam(s, p.home)
        mf.write(p.home, text)
        own, _ = s.seam_items()
        rows.append(file_row(s.path, p.home, "seam", base, f"{s.stem}::g", own[0].first_line, own[-1].last_line))
        print(f"  seam {s.stem}::g -> {p.home}" + (f"; copied {', '.join(copied)}" if copied else "")
              + (f"; the home held {', '.join(held)}" if held else ""))
    ia = mf.read(INJECT_ALL)
    ia = edit_inject_all(ia, s, p.delete_header, p.selftest_ns)
    mf.write(INJECT_ALL, ia)
    rows.append(file_row(s.path, "", "shell", base))
    if p.delete_header:
        inc = f'#include "game/{s.stem}.h"'
        for f in includers(inc):
            if f in (s.path, f"{GAME}/{s.stem}.h"):
                continue
            t = mf.read(f)
            mf.write(f, t.replace(inc + "\n", ""))
        git("rm", "-q", f"{GAME}/{s.stem}.h")
    elif p.keep_header_rest and p.inject:
        h = f"{GAME}/{s.stem}.h"
        t = mf.read(h)
        mf.write(h, re.sub(rf"^void\s+{p.inject}\(\);\n", "", t, flags=re.M))
    git("rm", "-q", s.path)
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="print the plan, write nothing")
    ap.add_argument("--moves", action="store_true", help="the fuzz files and the remainders")
    ap.add_argument("--shells", action="store_true", help="the pure shells")
    ap.add_argument("--seams", action="store_true",
                    help="the cleanup round: the seams still in a file of ours, to their group's lowest function's file")
    ap.add_argument("--only", default="", help="--shells: these group stems only (comma-separated)")
    ap.add_argument("--strict", action="store_true",
                    help="--check: exit 1 when a flat src/game/*.cpp is left that is not a harness file or the runtime's")
    a = ap.parse_args()
    try:
        folders, low = load_folders()
        problems: list[str] = []
        moves = plan_moves(folders, problems)
        only = {x for x in a.only.split(",") if x}
        shells = []
        for stem in flat_stems():
            if stem in STAY or stem.endswith("_fuzz"):
                continue
            sh = Shell(stem)
            if sh.pure and (not only or stem in only):
                shells.append(ShellPlan(sh, low))
        if problems:
            for p in problems:
                print("REFUSED:", p)
            return 1
        if a.seams:
            only_s = {x for x in a.only.split(",") if x}
            todo, rows = [], []
            for sh in ours_with_seam():
                if only_s and sh.stem not in only_s:
                    continue
                home, leave, why = plan_seam(sh, low)
                if not why:
                    try:
                        insert_seam(sh, home)       # the dry pass: every refusal before anything is written
                    except mf.Refused as e:
                        why = str(e)
                if why:
                    print(f"left   {sh.path}: {why}")
                else:
                    todo.append((sh, home, leave))
                    if a.check:
                        print(f"seam   {sh.path} -> {home} ({', '.join(it.names[0] for it in leave)} leave)")
            print(f"retire_shells --seams: {len(todo)} seams to move")
            if a.check:
                return 0
            base = mf.git_head([GAME, "analysis/tu_moves.tsv"])
            for sh, home, leave in todo:
                rows.append(do_seam(Shell(sh.stem, sh.path), home, leave, base))
            add_rows(rows)
            return 0
        if a.check or not (a.moves or a.shells):
            for old, new in moves:
                print(f"move   {old} -> {new}")
            for p in shells:
                print("retire " + p.line())
            print(f"retire_shells: {len(moves)} whole files to move ({sum(1 for o, _ in moves if o.endswith('_fuzz.cpp'))} fuzz), "
                  f"{len(shells)} pure shells to retire ({sum(1 for p in shells if p.home)} with a seam, "
                  f"{sum(1 for p in shells if p.delete_header)} headers to delete)")
            if a.strict and (moves or shells):
                print("retire_shells --check --strict: src/game holds a flat .cpp that is neither a harness file nor "
                      "the runtime's: a new file goes into its folder (docs/source-layout.md)")
                return 1
            return 0
        base = mf.git_head([GAME, INJECT_ALL, "analysis/tu_moves.tsv"])
        if a.moves:
            do_moves(moves, base)
            print(f"retire_shells: {len(moves)} files moved")
        if a.shells:
            rows = []
            for p in shells:
                print(f"retire {p.shell.path}")
                rows += do_shell(p, base)
            add_rows(rows)
            print(f"retire_shells: {len(shells)} shells retired")
        return 0
    except mf.Refused as e:
        print("REFUSED:", e)
        return 1


if __name__ == "__main__":
    sys.exit(main())
