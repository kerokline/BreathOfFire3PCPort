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
Debug sections are not read: a line number that moved is not a change.

Prints one line per object that differs, with the first differing function,
and exits 1 if any differs. An object present on one side only is listed and
counts as a difference unless --allow-new / --allow-gone names it.
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
    a = ap.parse_args()

    objdump = find_objdump()
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
        if x == y:
            continue
        i = next((j for j, (p, q) in enumerate(zip(x, y)) if p != q), min(len(x), len(y)))
        print(f"DIFFER {k}  first in {first_function(y if i < len(y) else x, i)}")
        bad += 1
    compared = sum(1 for k in set(base) & set(new) if wanted(k))
    print(f"codegen_same: {compared} objects compared, {bad} differ")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
