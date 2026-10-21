"""Compare the machine code of two builds, object file by object file.

The typed-data round (docs/typed-data-round.md) changes how our sources
spell a memory access, never what the access is. The strongest check of
that needs no game run: the two builds' objects disassemble the same.

    python tools/codegen_same.py BASE_BUILD NEW_BUILD [--only GLOB ...]

BASE_BUILD and NEW_BUILD are cmake build directories (each `build/` of a
worktree). For every `CMakeFiles/**/*.obj` present in both, the code
sections are disassembled with `llvm-objdump -d -r --no-show-raw-insn`
(relocations shown by symbol, so a call or a data reference that moved is
a difference), the per-file header line dropped, and the text compared.
Then every initialised data section (`.data`, `.rdata`, `.rdata$...`; not
`.bss`, not debug) is dumped byte for byte with its relocations
(`llvm-objdump -s -r`), so a constant table that names another address - a
`{Sprite_Objects_addr, ...}` region, say - is a difference too (the review of
PR #94, 2026-10-21: until then only code was compared). `--code-only` skips
the data. Debug sections are not read: a line number that moved is not a
change.

Prints one line per object that differs, with the first differing function
(DATA and the section for a data difference), and exits 1 if any differs. An
object present on one side only is listed and counts as a difference unless
--allow-new / --allow-gone names it. Exits 2 when nothing was compared (a
build directory without objects, or an --only glob that matched none): a
gate that compared nothing has proved nothing.
"""

from __future__ import annotations

import argparse
import fnmatch
import os
import shutil
import subprocess
import sys
from pathlib import Path


def find_objdump() -> str:
    for name in ("llvm-objdump", "llvm-objdump.exe"):
        p = shutil.which(name)
        if p:
            return p
    cxx = shutil.which("i686-w64-mingw32-clang++")
    if cxx:
        cand = Path(cxx).with_name("llvm-objdump.exe")
        if cand.exists():
            return str(cand)
    sys.exit("codegen_same: llvm-objdump not found (it ships with the llvm-mingw toolchain)")


def objects(build: Path) -> dict[str, Path]:
    root = build / "CMakeFiles"
    out = {}
    for p in root.rglob("*.obj"):
        out[p.relative_to(root).as_posix()] = p
    return out


def disasm(objdump: str, path: Path) -> list[str]:
    r = subprocess.run([objdump, "-d", "-r", "--no-show-raw-insn", str(path)],
                       capture_output=True, text=True, check=True)
    lines = r.stdout.splitlines()
    # The first non-empty line names the file ("<path>:\tfile format ..."); drop it.
    return [l for l in lines if l.strip() and "file format" not in l]


def data_sections(objdump: str, path: Path) -> list[str]:
    """The object's initialised, non-debug data sections, by name."""
    r = subprocess.run([objdump, "-h", str(path)], capture_output=True, text=True, check=True)
    names = []
    for l in r.stdout.splitlines():
        f = l.split()
        # "Idx Name Size VMA Type": Type DATA, not "DATA, DEBUG", not BSS
        if len(f) >= 5 and f[0].isdigit() and f[4].startswith("DATA") and "DEBUG" not in l:
            names.append(f[1])
    return names


def data_dump(objdump: str, path: Path) -> list[str]:
    names = data_sections(objdump, path)
    if not names:
        return []
    r = subprocess.run([objdump, "-s", "-r"] + ["--section=" + n for n in names] + [str(path)],
                       capture_output=True, text=True, check=True)
    return [l for l in r.stdout.splitlines() if l.strip() and "file format" not in l]


def first_section(lines: list[str], index: int) -> str:
    for l in reversed(lines[: index + 1]):
        if l.startswith("Contents of section") or l.startswith("RELOCATION RECORDS"):
            return l
    return "?"


def first_function(lines: list[str], index: int) -> str:
    for l in reversed(lines[: index + 1]):
        if l.endswith(">:"):
            return l
    return "?"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("base", type=Path)
    ap.add_argument("new", type=Path)
    ap.add_argument("--only", action="append", default=[], help="compare only objects matching this glob (repeatable)")
    ap.add_argument("--allow-new", action="append", default=[], help="glob of objects allowed only in NEW")
    ap.add_argument("--allow-gone", action="append", default=[], help="glob of objects allowed only in BASE")
    ap.add_argument("--code-only", action="store_true", help="compare the code sections only, not the data")
    a = ap.parse_args()

    objdump = find_objdump()
    for d in (a.base, a.new):
        if not (d / "CMakeFiles").is_dir():
            print(f"codegen_same: {d} has no CMakeFiles: not a build directory")
            return 2
    base, new = objects(a.base), objects(a.new)

    def wanted(k: str) -> bool:
        return not a.only or any(fnmatch.fnmatch(k, g) for g in a.only)

    bad = 0
    for k in sorted(set(base) | set(new)):
        if not wanted(k):
            continue
        if k not in new:
            if not any(fnmatch.fnmatch(k, g) for g in a.allow_gone):
                print(f"GONE   {k}")
                bad += 1
            continue
        if k not in base:
            if not any(fnmatch.fnmatch(k, g) for g in a.allow_new):
                print(f"NEW    {k}")
                bad += 1
            continue
        x, y = disasm(objdump, base[k]), disasm(objdump, new[k])
        if x != y:
            i = next((j for j, (p, q) in enumerate(zip(x, y)) if p != q), min(len(x), len(y)))
            print(f"DIFFER {k}  first in {first_function(y if i < len(y) else x, i)}")
            bad += 1
            continue
        if a.code_only:
            continue
        x, y = data_dump(objdump, base[k]), data_dump(objdump, new[k])
        if x != y:
            i = next((j for j, (p, q) in enumerate(zip(x, y)) if p != q), min(len(x), len(y)))
            print(f"DATA   {k}  first in {first_section(y if i < len(y) else x, i)}")
            bad += 1
    compared = sum(1 for k in set(base) & set(new) if wanted(k))
    print(f"codegen_same: {compared} objects compared ({'code' if a.code_only else 'code and data'}), {bad} differ")
    for g in a.only:
        if not any(fnmatch.fnmatch(k, g) for k in set(base) | set(new)):
            print(f"codegen_same: --only {g} matched no object")
            return 2
    if compared == 0:
        print("codegen_same: nothing compared (no objects under CMakeFiles?)")
        return 2
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
