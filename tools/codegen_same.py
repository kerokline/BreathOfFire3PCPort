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

    python tools/codegen_same.py BASE_BUILD NEW_BUILD --by-function [--only GLOB ...]

--by-function (the source-layout round's gate, docs/source-layout-round.md
2.4, group SL3): the two builds compared function by function, keyed by
symbol across objects, so a function that moved to another object is
compared with itself. Every object of each build is read as COFF (i386 and
AMD64, the bigobj header too); a symbol's bytes run to the next symbol of its
section, are disassembled with capstone and normalised so that what a move
legitimately changes does not count and nothing else is hidden:

- a relocation names its target: a named symbol by its name - a file-local
  one also by a hash of that local's own normalised body, so a moved caller
  that now reaches another file's helper of the same name differs; a
  section-relative one (a string literal, a jump table, a constant with no
  symbol) by the bytes of the item it points at, from its offset to the next
  anchor of that section (a symbol, another relocation's target, the end),
  with the item's own relocations named the same way; the field's
  layout-dependent offset is zeroed;
- a relative branch or call the assembler resolved inside one section (no
  relocation) names its target as `.+offset` inside the function, or by the
  symbol it lands in outside it;
- trailing int3 / nop padding is dropped (it follows from the length).

Initialised data (every initialised, non-debug data section; not .pdata,
.xdata, .drectve, .llvm_addrsig) is compared the same way by symbol, .bss
symbols by name and size. A name defined in several objects (an `inline` or
template COMDAT, a file-local helper such as `B` in many files, a local a
move copied into two files) is compared as the set of its distinct bodies: a
move that leaves one more copy of the same helper is no difference (counted
on the summary line; COPIES lines with --verbose). The per-file names the
compiler makes up (`__cxx_global_var_init.N`, `_GLOBAL__sub_I_<file>`) are
pooled under one key each, their bodies compared as a set the same way.

Prints DIFFER (code), DATA, BSS, GONE (in BASE only) or NEW (in NEW only)
per symbol with the objects that hold it, and exits 1 if any; 2 when either
side read no function. --only limits both builds' objects as in the object
mode (a function moved out of the glob then reads as GONE). --show NAME
prints both normalised bodies of one symbol. --anonymous-locals knows a
file-local symbol (an anonymous-namespace or `static` helper, constant or
variable) by its body alone, in its own row and wherever it is named: for
a commit that renames such a helper so that two files' helpers of one name
can share a target file. Every function still compares by its own name, so
a renamed helper whose code is the same passes, and one whose code changed
reads as a GONE / NEW pair `local:<hash>` with its names.

The source root (both modes). A `__FILE__` string holds the absolute path
of the tree that was built (x86_64's Low32() message, a static local's
path), so two worktrees' builds differ there although nothing changed. Each
build directory's root is read from its CMakeCache.txt
(CMAKE_HOME_DIRECTORY, else bof3x_SOURCE_DIR) and every spelling of it -
`/` or `\\`, the drive letter in either case - is replaced by one
placeholder in every initialised byte compared (the object mode's data
dumps; the per-function mode's data symbols and section-relative items).
When the two roots differ in length, what follows a path in its section
also moves and its alignment padding changes, so in the per-function mode
(said on a first line) the zeros that end each run between two data anchors
are not compared, and, after a section's first path only, neither is the
length of a zero run that follows a printable byte (a string's NUL and its
padding) nor the offset of a relocation there. The object mode compares
offsets as written, so a pair of trees with roots of different lengths can
still differ there on x86_64; its per-function mode is the gate. A build of
the same tree compares exactly as before.

--anonymous-locals also knows a promotion (stage B0's shared-state cases):
a symbol global in NEW, not a global of BASE, whose body is that of a BASE
file-local of the same name but for its linkage (demangled by llvm-cxxfilt,
with `static`'s `L` and `(anonymous namespace)::` dropped; or, when that
body has one file-local of the name, but for its named namespace too). NEW is then read
a second time with each such global known as the file-local it was, where it
is defined and wherever another object names it, so its callers compare as
before; it prints as `PROMOTED <name>` and is not a difference. A promoted
symbol whose body changed is not paired: it reads as GONE `local:<hash>` and
NEW `<name>`, and its callers as DIFFER.

    --exceptions FILE [--exception-classes CLASS[,CLASS]]
                        (with --by-function; the round doc's 7a, the owner's calls 8 and 10)

FILE is analysis/tu_gate_exceptions.tsv: a header line `symbol class reason
commit` (tab-separated), `#` lines comments, one row per symbol the gate
lets differ. A DIFFER, GONE or NEW line of a symbol listed there whose
class is a known one prints as `EXCEPT <class> <symbol>` and is not counted.
The classes:

- `inline` (x86_64): the compiler inlined, or stopped inlining, a moved or
  copied helper because its callers in the file changed;
- `low32-site` (x86_64): Low32()'s failure message names another file, line
  or function: __FILE__, __LINE__, __builtin_FUNCTION() of a moved or renamed
  caller;
- `data-layout` (both widths; the owner's call 10, 2026-11-08): a promoted
  variable is placed elsewhere in its .data / .bss, so a neighbouring
  file-local changes extent or hash, and its readers differ only in the
  reference to that local. A row names the data or bss symbol (mangled, or
  a name a `local:<hash>` key lists) or a reader function;
- `x64-indirect` (x86_64 only; call 10): an internal function pointer that
  x86_64 folded to a direct call goes through the pointer once the variable
  is external (`call @r4:Text_DrawAt` -> `call qword ptr [rip] @r4:..g_draw_at`,
  branch offsets a byte longer). A row names the caller, or the pointer
  itself: folded, it had no data symbol, so the promoted one reads NEW. Never
  used on an i386 build.

The symbol is the name the line prints (for a `local:<hash>` key, any of the
names it lists). A data or bss line is excepted by a `data-layout` or an
`x64-indirect` row only (DATA_CLASSES); an inject line never. --exception-classes limits which classes may except:
the rows of the other classes are read (and checked) but never used, and
never reported as unused. The i686 gate runs with `--exception-classes
data-layout`; the x86_64 gate with every class (no flag, the default). A row
whose class is not a known one, a name in --exception-classes that is not
one, a row without a symbol and a class, or a file without the header, is
an error: the line is printed and the exit is 2, before anything is
compared. A listed symbol of a class in use that did not differ is reported
on an `unused exception` line (not an error: a stale row, to prune by hand).
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import os
import re
import shutil
import struct
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


SOURCE_ROOT = b"<source-root>"


def source_roots(build: Path) -> list[bytes]:
    """The spellings of a build's source root that `__FILE__` strings carry (x86_64's Low32() message,
    a static local's path, ...): from CMakeCache.txt's CMAKE_HOME_DIRECTORY or bof3x_SOURCE_DIR, with
    `/` and `\\`, the drive letter in either case, longest first. Empty when the cache names none."""
    cache = build / "CMakeCache.txt"
    roots: set[str] = set()
    if cache.exists():
        for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            m = re.match(r"(CMAKE_HOME_DIRECTORY|bof3x_SOURCE_DIR):[A-Z]+=(.+)$", line.strip())
            if m and m.group(2).strip():
                roots.add(m.group(2).strip().rstrip("/\\"))
    out: set[bytes] = set()
    for r in roots:
        for s in (r.replace("\\", "/"), r.replace("/", "\\")):
            for v in (s, s[:1].lower() + s[1:], s[:1].upper() + s[1:]):
                out.add(v.encode("utf-8"))
    return sorted(out, key=len, reverse=True)


def unroot(data: bytes, roots: list[bytes]) -> bytes:
    """`data` with every spelling of the build's source root replaced by one placeholder."""
    for r in roots:
        if r in data:
            data = data.replace(r, SOURCE_ROOT)
    return data


def data_dump(objdump: str, path: Path, roots: list[bytes] = ()) -> list[str]:
    names = data_sections(objdump, path)
    if not names:
        return []
    r = subprocess.run([objdump, "-s", "-r"] + ["--section=" + n for n in names] + [str(path)],
                       capture_output=True, text=True, check=True)
    lines = [l for l in r.stdout.splitlines() if l.strip() and "file format" not in l]
    if not roots:
        return lines
    # each section's contents read back to bytes, the source root replaced, one hex line a section
    out: list[str] = []
    buf: bytearray | None = None
    for l in lines + ["<end>"]:
        hexrow = buf is not None and re.match(r" [0-9a-f]{4,} ", l)
        if hexrow:
            groups = l[1:].split(" ", 1)[1][:35].split()
            buf += bytes.fromhex("".join(groups))
            continue
        if buf is not None:
            out.append(" " + unroot(bytes(buf), roots).hex())
            buf = None
        if l.startswith("Contents of section"):
            buf = bytearray()
        if l != "<end>":
            out.append(l)
    return out


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


# --- the per-function mode (--by-function) ---------------------------------

IMAGE_FILE_MACHINE_I386 = 0x14C
IMAGE_FILE_MACHINE_AMD64 = 0x8664
SCN_CNT_CODE = 0x20
SCN_CNT_INIT = 0x40
SCN_CNT_UNINIT = 0x80
SCN_NRELOC_OVFL = 0x01000000
SKIP_SECTIONS = (".pdata", ".xdata", ".drectve", ".llvm_addrsig", ".debug", ".llvm.", ".rsrc")
# relocation type -> width of the field it patches, in bytes
RELOC_WIDTH = {
    IMAGE_FILE_MACHINE_I386: {0x06: 4, 0x07: 4, 0x0A: 2, 0x0B: 4, 0x14: 4},
    IMAGE_FILE_MACHINE_AMD64: {0x01: 8, 0x02: 4, 0x03: 4, 0x04: 4, 0x05: 4, 0x06: 4, 0x07: 4, 0x08: 4, 0x09: 4,
                               0x0A: 2, 0x0B: 4},
}
# The registration a move edits: a group's Name_Inject() and bof3::InjectAll (mangled, either width).
INJECT_REGISTRATION = re.compile(r"^_?_Z(\d+\w+_Injectv|N4bof39InjectAllEv)$")
# Names the compiler makes per translation unit: pooled under one key each.
POOLED = (re.compile(r"^(_?__cxx_global_var_init)(\.\d+)?$"), re.compile(r"^(_?_GLOBAL__sub_I_).*$"),
          re.compile(r"^(_?__cxx_global_array_dtor)(\.\d+)?$"), re.compile(r"^(_?__dtor_)(.*)$"))


class Coff:
    """The parts of a COFF object the per-function mode reads."""

    def __init__(self, path: Path):
        b = path.read_bytes()
        self.path = path
        if len(b) >= 56 and struct.unpack_from("<HH", b, 0) == (0, 0xFFFF):   # bigobj
            self.machine = struct.unpack_from("<H", b, 6)[0]
            nsec, symptr, nsym = struct.unpack_from("<III", b, 44)
            sec_off, symsize, secfmt = 56, 20, "<i"
        else:
            self.machine, nsec, _, symptr, nsym = struct.unpack_from("<HHIII", b, 0)
            opt = struct.unpack_from("<H", b, 16)[0]
            sec_off, symsize, secfmt = 20 + opt, 18, "<h"
        if self.machine not in RELOC_WIDTH:
            raise ValueError(f"{path}: machine 0x{self.machine:X} is not i386 or AMD64")
        strings = b[symptr + nsym * symsize:]

        def cstr(raw: bytes) -> str:
            return raw.split(b"\0", 1)[0].decode("latin-1")

        self.sections = []
        for i in range(nsec):
            h = sec_off + 40 * i
            name = cstr(b[h:h + 8])
            if name.startswith("/"):
                name = cstr(strings[int(name[1:]):int(name[1:]) + 4096])
            _, _, size, ptr, rptr, _, nrel, _, flags = struct.unpack_from("<IIIIIIHHI", b, h + 8)
            data = b[ptr:ptr + size] if ptr and not (flags & SCN_CNT_UNINIT) else b""
            first = 0
            if flags & SCN_NRELOC_OVFL:
                nrel = struct.unpack_from("<I", b, rptr)[0]
                first = 1
            rel = [struct.unpack_from("<IIH", b, rptr + 10 * k) for k in range(first, nrel)]
            self.sections.append({"name": name, "flags": flags, "data": data, "size": size, "relocs": rel})
        self.symbols = {}
        k = 0
        while k < nsym:
            o = symptr + k * symsize
            raw = b[o:o + 8]
            if raw[:4] == b"\0\0\0\0":
                off = struct.unpack_from("<I", raw, 4)[0]
                name = cstr(strings[off:off + 4096])
            else:
                name = cstr(raw)
            value = struct.unpack_from("<I", b, o + 8)[0]
            secn = struct.unpack_from(secfmt, b, o + 12)[0]
            typ, cls, naux = struct.unpack_from("<HBB", b, o + 12 + struct.calcsize(secfmt))
            is_section = (cls == 3 and value == 0 and naux > 0 and 0 < secn <= nsec
                          and name == self.sections[secn - 1]["name"])
            self.symbols[k] = {"name": name, "value": value, "section": secn, "cls": cls, "type": typ,
                               "is_section": is_section, "local": cls != 2}
            k += 1 + naux


def section_class(name: str) -> str:
    return name.split("$", 1)[0]


def skip_section(name: str) -> bool:
    return any(name.startswith(p) for p in SKIP_SECTIONS)


def pooled_key(name: str) -> str:
    for rx in POOLED:
        m = rx.match(name)
        if m:
            return m.group(1) + "*"
    return name


class Normaliser:
    """The normalised body of every symbol of one object."""

    def __init__(self, obj: Coff, anonymous_locals: bool = False, roots: list[bytes] = (), pad_free: bool = False,
                 promoted: dict[str, str] | None = None):
        self.o = obj
        self.anonymous_locals = anonymous_locals
        self.roots = list(roots)      # the build's source-root spellings (source_roots), read as one
        self.pad_free = pad_free      # the two roots differ in length: zero padding before an anchor not compared
        # --anonymous-locals' second pass: a global of NEW that was a file-local of BASE is read as
        # the file-local it was, where it is defined and (by its body's hash) where it is named
        self.promoted = promoted or {}
        for s in obj.symbols.values():
            if s["section"] > 0 and not s["is_section"] and s["cls"] == 2 and s["name"] in self.promoted:
                s["local"] = True
        self.cs = None
        self.by_section: dict[int, list[tuple[int, int]]] = {}
        for i, s in obj.symbols.items():
            if s["section"] > 0 and not s["is_section"] and s["cls"] in (2, 3, 6):
                self.by_section.setdefault(s["section"], []).append((s["value"], i))
        for v in self.by_section.values():
            v.sort()
        # An anchor ends an unnamed item: any symbol, any relocation's target.
        self.anchors: dict[int, set[int]] = {n: {v for v, _ in lst} for n, lst in self.by_section.items()}
        for sec in obj.sections:
            for off, symi, typ in sec["relocs"]:
                t = obj.symbols.get(symi)
                if t and t["is_section"]:
                    a = self.addend(sec["data"], off, typ)
                    if a is not None:
                        self.anchors.setdefault(t["section"], set()).add(a)
        self.sorted_anchors = {n: sorted(v) for n, v in self.anchors.items()}
        self.local_hash_cache: dict[int, str] = {}
        self.root_at: dict[int, int | None] = {}

    def width(self, typ: int) -> int:
        w = RELOC_WIDTH[self.o.machine].get(typ)
        if w is None:
            raise ValueError(f"{self.o.path}: relocation type 0x{typ:X} not known")
        return w

    def addend(self, data: bytes, off: int, typ: int):
        w = self.width(typ)
        if off + w > len(data):
            return None
        return int.from_bytes(data[off:off + w], "little", signed=True)

    def extent(self, secn: int, value: int) -> int:
        """Where the symbol at `value` of section `secn` ends: the next symbol, or the end."""
        for v, _ in self.by_section.get(secn, []):
            if v > value:
                return v
        return self.o.sections[secn - 1]["size"]

    def containing(self, secn: int, off: int):
        best = None
        for v, i in self.by_section.get(secn, []):
            if v <= off:
                best = (v, i)
            else:
                break
        return best

    def item(self, secn: int, off: int, depth: int) -> str:
        """A section-relative target, described by what it points at."""
        if secn <= 0 or secn > len(self.o.sections):
            return f"?sec{secn}+{off:X}"
        sec = self.o.sections[secn - 1]
        cls = section_class(sec["name"])
        if sec["flags"] & SCN_CNT_CODE:
            c = self.containing(secn, off)
            if c is None:
                return f"{cls}:?+{off:X}"
            return f"code:{self.symref(c[1], depth)}+{off - c[0]:X}"
        exact = [i for v, i in self.by_section.get(secn, []) if v == off]
        if exact:
            return self.symref(exact[0], depth)
        nxt = next((a for a in self.sorted_anchors.get(secn, []) if a > off), sec["size"])
        if sec["flags"] & SCN_CNT_UNINIT or not sec["data"]:
            return f"{cls}:zero[{nxt - off}]"
        if depth > 2:
            return f"{cls}:item"
        return f"{cls}:[{self.render_data(secn, off, nxt, depth)}]"

    def symref(self, symi: int, depth: int) -> str:
        s = self.o.symbols[symi]
        if s["is_section"]:
            return f"{section_class(s['name'])}+?"
        name = pooled_key(s["name"])
        if s["section"] <= 0 and s["name"] in self.promoted:     # a promoted symbol another object defines
            return "local" if depth >= 1 else f"local#{self.promoted[s['name']]}"
        if s["section"] > 0 and s["local"]:
            if self.anonymous_locals:
                name = "local"
            if depth < 1:
                return f"{name}#{self.local_hash(symi)}"
        return name

    def local_hash(self, symi: int) -> str:
        if symi not in self.local_hash_cache:
            self.local_hash_cache[symi] = "self"     # a local that reaches itself
            body = "\n".join(self.body(symi, depth=1))
            self.local_hash_cache[symi] = hashlib.sha1(body.encode()).hexdigest()[:12]
        return self.local_hash_cache[symi]

    def relocs_in(self, secn: int, start: int, end: int):
        return sorted(r for r in self.o.sections[secn - 1]["relocs"] if start <= r[0] < end)

    def reloc_desc(self, data: bytes, off: int, symi: int, typ: int, depth: int) -> tuple[str, bool]:
        """(the target's description, whether the field's bytes depend on the object's layout)."""
        t = self.o.symbols.get(symi)
        if t is None:
            return f"?sym{symi}", False
        if t["is_section"]:
            a = self.addend(data, off, typ) or 0
            return f"r{typ:X}:" + self.item(t["section"], a, depth + 1), True
        return f"r{typ:X}:" + self.symref(symi, depth), False

    def render_data(self, secn: int, start: int, end: int, depth: int) -> str:
        if self.pad_free:
            # a path of another length moves what follows it in its section, so the alignment padding of
            # the items after it changes: each run between two anchors read alone (render_run)
            cuts = [start] + [x for x in self.sorted_anchors.get(secn, []) if start < x < end] + [end]
            return "|".join(self.render_run(secn, a, b, depth, True) for a, b in zip(cuts, cuts[1:]))
        return self.render_run(secn, start, end, depth, False)

    def render_run(self, secn: int, start: int, end: int, depth: int, strip: bool) -> str:
        sec = self.o.sections[secn - 1]
        data = bytearray(sec["data"][start:end])
        notes = []
        for off, symi, typ in self.relocs_in(secn, start, end):
            d, layout = self.reloc_desc(sec["data"], off, symi, typ, depth)
            if layout:
                w = self.width(typ)
                data[off - start:off - start + w] = bytes(w)
            notes.append((off, d))
        if not strip:
            return (unroot(bytes(data), self.roots).hex()
                    + (" " + ",".join(f"{o - start:X}={d}" for o, d in notes) if notes else ""))
        # The roots differ in length, so what follows a path in its section sits elsewhere and its
        # alignment padding changes. Not compared: the zeros that end the run (the padding before the next
        # anchor) and, past the section's first path, the length of a zero run that follows a printable
        # byte (a string's NUL and the padding after it), shown as `~`; a relocation's offset there is
        # not compared either (its order and target are).
        fr = self.first_root(secn)
        parts: list[str] = []
        i, n = 0, len(data)
        chunk = bytearray()
        while i < n:
            if (data[i] == 0 and fr is not None and start + i >= fr and i > 0
                    and 0x20 <= data[i - 1] < 0x7F):
                j = i
                while j < n and data[j] == 0:
                    j += 1
                parts.append(unroot(bytes(chunk), self.roots).hex() + "~")
                chunk = bytearray()
                i = j
                continue
            chunk.append(data[i])
            i += 1
        parts.append(unroot(bytes(chunk).rstrip(b"\x00"), self.roots).hex())
        return "".join(parts) + (" " + ",".join(
            (f"{o - start:X}" if fr is None or o < fr else "*") + f"={d}" for o, d in notes) if notes else "")

    def first_root(self, secn: int):
        """The offset of the first source-root spelling in a section's bytes, or None."""
        if secn not in self.root_at:
            data = self.o.sections[secn - 1]["data"]
            hits = [k for k in (data.find(r) for r in self.roots) if k >= 0]
            self.root_at[secn] = min(hits) if hits else None
        return self.root_at[secn]

    def disassembler(self):
        if self.cs is None:
            import capstone
            mode = capstone.CS_MODE_32 if self.o.machine == IMAGE_FILE_MACHINE_I386 else capstone.CS_MODE_64
            self.cs = capstone.Cs(capstone.CS_ARCH_X86, mode)
            self.cs.detail = True
        return self.cs

    def body(self, symi: int, depth: int = 0) -> list[str]:
        s = self.o.symbols[symi]
        secn = s["section"]
        sec = self.o.sections[secn - 1]
        start, end = s["value"], self.extent(secn, s["value"])
        if not (sec["flags"] & SCN_CNT_CODE):
            if sec["flags"] & SCN_CNT_UNINIT or not sec["data"]:
                return [f"zero[{end - start}]"]
            # an initialised symbol ends at the next anchor too: what follows it unnamed but referenced
            # (a string literal, another constant) is compared where it is referenced, so a move that
            # takes those items to another object does not read as a change of this symbol
            end = next((x for x in self.sorted_anchors.get(secn, []) if start < x < end), end)
            return [self.render_data(secn, start, end, depth)]
        import capstone
        raw = bytearray(sec["data"][start:end])
        notes: dict[int, list[str]] = {}
        for off, symi2, typ in self.relocs_in(secn, start, end):
            d, layout = self.reloc_desc(sec["data"], off, symi2, typ, depth)
            if layout:
                w = self.width(typ)
                raw[off - start:off - start + w] = bytes(w)
            notes.setdefault(off - start, []).append(d)
        lines = []
        pos = 0
        for ins in self.disassembler().disasm(bytes(raw), start):
            rel_off = ins.address - start
            pos = rel_off + ins.size
            mine = [d for o, ds in notes.items() if rel_off <= o < pos for d in ds]
            text = f"{ins.mnemonic} {ins.op_str}".strip()
            relative = ins.group(capstone.CS_GRP_BRANCH_RELATIVE)
            if mine:
                # a relocated relative branch's operand is the next address: say only the target
                text = (ins.mnemonic if relative else text) + " @" + ",".join(mine)
            elif relative:
                ops = [op for op in ins.operands if op.type == capstone.x86.X86_OP_IMM]
                if ops:
                    tgt = ops[0].imm
                    if start <= tgt < end:
                        text = f"{ins.mnemonic} .+{tgt - start:X}"
                    else:
                        c = self.containing(secn, tgt)
                        where = f"{self.symref(c[1], depth)}+{tgt - c[0]:X}" if c else f"?+{tgt:X}"
                        text = f"{ins.mnemonic} {where}"
            lines.append(text)
        if pos < len(raw):
            lines.append("bytes " + bytes(raw[pos:]).hex())
        while lines and (lines[-1] == "int3" or lines[-1].startswith("nop") or lines[-1].startswith("data16")):
            lines.pop()
        return lines


def promoted_hashes(path: Path, names: set[str], roots: list[bytes] = (), pad_free: bool = False) -> dict[str, str]:
    """The hash a reference to each promoted symbol `path` defines reads as (Normaliser.local_hash)."""
    obj = Coff(path)
    n = Normaliser(obj, True, roots, pad_free, {x: "" for x in names})
    return {s["name"]: n.local_hash(i) for i, s in obj.symbols.items()
            if s["section"] > 0 and not s["is_section"] and s["cls"] == 2 and s["name"] in names}


def object_symbols(path: Path, anonymous_locals: bool = False, roots: list[bytes] = (), pad_free: bool = False,
                   promoted: dict[str, str] | None = None) -> list[tuple[str, str, list[str], bool]]:
    """(name, kind, normalised body, file-local) for every defined symbol of one object."""
    obj = Coff(path)
    n = Normaliser(obj, anonymous_locals, roots, pad_free, promoted)
    out = []
    for i, s in obj.symbols.items():
        if s["section"] <= 0 or s["is_section"] or s["cls"] not in (2, 3):
            continue
        sec = obj.sections[s["section"] - 1]
        if skip_section(sec["name"]):
            continue
        if sec["flags"] & SCN_CNT_CODE:
            kind = "code"
        elif sec["flags"] & SCN_CNT_UNINIT or not sec["data"]:
            kind = "bss"
        elif sec["flags"] & SCN_CNT_INIT:
            kind = "data"
        else:
            continue
        out.append((pooled_key(s["name"]), kind, n.body(i), s["local"]))
    return out


def symbol_table(objs: dict[str, Path], wanted, anonymous_locals: bool = False, roots: list[bytes] = (),
                 pad_free: bool = False, promoted: dict[str, str] | None = None):
    """name -> {"kind": code | data | bss, "bodies": {hash: [objects]}}; texts by hash; functions read."""
    from concurrent.futures import ProcessPoolExecutor
    from functools import partial
    keys = [k for k in sorted(objs) if wanted(k)]
    table: dict[str, dict] = {}
    texts: dict[str, list[str]] = {}
    nfunc = 0
    with ProcessPoolExecutor() as pool:
        reader = partial(object_symbols, anonymous_locals=anonymous_locals, roots=roots, pad_free=pad_free,
                         promoted=promoted)
        for key, syms in zip(keys, pool.map(reader, [objs[k] for k in keys], chunksize=8)):
            for name, kind, lines, local in syms:
                nfunc += kind == "code"
                h = hashlib.sha1("\n".join(lines).encode()).hexdigest()
                real = name
                if anonymous_locals and local:          # a file-local is its body, whatever its name
                    name = f"local:{h[:12]}"
                e = table.setdefault(name, {"kind": kind, "bodies": {}, "names": set()})
                e["names"].add(real)
                e["bodies"].setdefault(h, []).append(key)
                e.setdefault("where", {}).setdefault(real, set()).add(key)
                texts[h] = lines
    return table, texts, nfunc


def find_cxxfilt() -> str:
    for name in ("llvm-cxxfilt", "llvm-cxxfilt.exe"):
        p = shutil.which(name)
        if p:
            return p
    for cxx in ("i686-w64-mingw32-clang++", "x86_64-w64-mingw32-clang++", "clang++"):
        p = shutil.which(cxx)
        if p and Path(p).with_name("llvm-cxxfilt.exe").exists():
            return str(Path(p).with_name("llvm-cxxfilt.exe"))
    sys.exit("codegen_same: llvm-cxxfilt not found (it ships with the llvm-mingw toolchain)")


def linkage_free(names: list[str], i386: bool) -> dict[str, str]:
    """name -> its demangled spelling without what internal linkage adds: `static` (the L of
    _ZL...) and `(anonymous namespace)::` - so a promoted symbol and the file-local it was read alike."""
    if not names:
        return {}
    # i686 prefixes every C and C++ name with one underscore (llvm-cxxfilt's --strip-underscore drops
    # it only from a name it demangles, so a promoted C-spelled variable would keep it)
    plain = [n[1:] if i386 and n.startswith("_") else n for n in names]
    r = subprocess.run([find_cxxfilt()], input="\n".join(plain) + "\n", capture_output=True, text=True, check=True)
    out = r.stdout.splitlines()
    if len(out) != len(names):
        sys.exit("codegen_same: llvm-cxxfilt answered another number of lines than it was given")
    return {n: d.replace("(anonymous namespace)::", "") for n, d in zip(names, out)}


def find_promotions(base: dict, new: dict, i386: bool) -> dict[str, str]:
    """NEW's global name -> BASE's file-local name, for a global of NEW (not a global of BASE) whose
    body is a BASE file-local's of the same name but for its linkage (with --anonymous-locals)."""
    cand = sorted(k for k in new if not k.startswith("local:") and k not in base)
    if not cand:
        return {}
    local_names = sorted({n for k, e in base.items() if k.startswith("local:") for n in e["names"]})
    spell = linkage_free(cand + local_names, i386)
    by_spelling: dict[str, list[str]] = {}
    for k, e in base.items():
        if k.startswith("local:"):
            for n in e["names"]:
                by_spelling.setdefault(spell[n], []).append(k)
    def unqualified(d: str) -> str:
        """`ns::name(args)` -> `name(args)`: a promotion may also move the item into a named namespace."""
        cut = d.find("(")
        head, tail = (d, "") if cut < 0 else (d[:cut], d[cut:])
        return head.rsplit("::", 1)[-1] + tail

    by_tail: dict[str, list[str]] = {}
    for k, e in base.items():
        if k.startswith("local:"):
            for n in e["names"]:
                by_tail.setdefault(unqualified(spell[n]), []).append(k)
    out = {}
    for name in cand:
        for h in new[name]["bodies"]:
            k = f"local:{h[:12]}"
            if k in by_spelling.get(spell[name], ()):
                out[name] = next(n for n in sorted(base[k]["names"]) if spell[n] == spell[name])
            elif k in by_tail.get(unqualified(spell[name]), ()):
                # only the namespace changed: paired when that body has one file-local of that name
                same = [n for n in sorted(base[k]["names"]) if unqualified(spell[n]) == unqualified(spell[name])]
                if len(same) == 1 and by_tail[unqualified(spell[name])].count(k) == 1:
                    out[name] = same[0]
                else:
                    # two sources' file-locals of one name and one body (a zero byte each): the one
                    # defined in the object that now defines the global, when exactly one is
                    here = set(new[name]["bodies"][h])
                    mine = [n for n in same if base[k]["where"].get(n, set()) & here]
                    if len(mine) == 1:
                        out[name] = mine[0]
    return out


EXCEPTION_CLASSES = ("inline", "low32-site", "data-layout", "x64-indirect")
DATA_CLASSES = ("data-layout", "x64-indirect")  # the classes that may except a DATA, BSS or data NEW/GONE line
X64_ONLY_CLASSES = ("x64-indirect",)  # never used on an i386 build


def load_exceptions(path: Path) -> dict[str, dict]:
    """symbol -> {"class", "line", "used"}; an unknown class or a malformed line exits 2."""
    out: dict[str, dict] = {}
    header = False
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as e:
        print(f"codegen_same: --exceptions {path}: {e}")
        sys.exit(2)
    for n, line in enumerate(lines, 1):
        if not line.strip() or line.startswith("#"):
            continue
        f = [x.strip() for x in line.split("\t")]
        if not header:
            if f[:4] != ["symbol", "class", "reason", "commit"]:
                print(f"codegen_same: {path}:{n}: the header must be symbol, class, reason, commit: {line}")
                sys.exit(2)
            header = True
            continue
        if len(f) < 2 or not f[0] or not f[1]:
            print(f"codegen_same: {path}:{n}: a row needs a symbol and a class: {line}")
            sys.exit(2)
        if f[1] not in EXCEPTION_CLASSES:
            print(f"codegen_same: {path}:{n}: class {f[1]!r} is not one of {', '.join(EXCEPTION_CLASSES)} "
                  f"(a new class needs the coordinator's entry in docs/source-layout-round.md 7a): {line}")
            sys.exit(2)
        out[f[0]] = {"class": f[1], "line": line, "used": False}
    if not header:
        print(f"codegen_same: {path}: no header line (symbol, class, reason, commit)")
        sys.exit(2)
    return out


def excepted(exceptions: dict, key: str, e: dict, allowed: tuple[str, ...]) -> str | None:
    """The class that excepts this symbol's line, or None: a code symbol by a row of a class in
    `allowed`, a data or bss symbol only by a row of a DATA_CLASSES class in `allowed`."""
    if not exceptions:
        return None
    for n in [key] + sorted(e["names"]):
        x = exceptions.get(n)
        if not x or x["class"] not in allowed:
            continue
        if e["kind"] != "code" and x["class"] not in DATA_CLASSES:
            continue
        x["used"] = True
        return x["class"]
    return None


def named(key: str, e: dict) -> str:
    return f"; named {', '.join(sorted(e['names']))}" if key.startswith("local:") else ""


def objects_of(e: dict) -> str:
    return ", ".join(sorted({o for v in e["bodies"].values() for o in v}))


def by_function(a) -> int:
    for d in (a.base, a.new):
        if not (d / "CMakeFiles").is_dir():
            print(f"codegen_same: {d} has no CMakeFiles: not a build directory")
            return 2

    def wanted(k: str) -> bool:
        return not a.only or any(fnmatch.fnmatch(k, g) for g in a.only)

    exceptions = load_exceptions(a.exceptions) if a.exceptions else {}
    allowed = tuple(EXCEPTION_CLASSES)
    if a.exception_classes:
        allowed = tuple(c.strip() for c in a.exception_classes.split(",") if c.strip())
        unknown = [c for c in allowed if c not in EXCEPTION_CLASSES]
        if unknown or not allowed:
            print(f"codegen_same: --exception-classes {a.exception_classes}: {', '.join(unknown) or 'no class'} "
                  f"is not one of {', '.join(EXCEPTION_CLASSES)} (docs/source-layout-round.md 7a)")
            return 2
    broots, nroots = source_roots(a.base), source_roots(a.new)
    pad_free = {len(r) for r in broots} != {len(r) for r in nroots}
    if pad_free:
        print(f"codegen_same: the source roots differ in length ({a.base}, {a.new}): the alignment "
              f"padding a path moves is not compared (the docstring's 'source root')")
    base, btext, bn = symbol_table(objects(a.base), wanted, a.anonymous_locals, broots, pad_free)
    new_objs = objects(a.new)
    new, ntext, nn = symbol_table(new_objs, wanted, a.anonymous_locals, nroots, pad_free)
    promotions: dict[str, str] = {}
    some = next((p for k, p in sorted(new_objs.items()) if wanted(k)), None)
    i386 = some is not None and Coff(some).machine == IMAGE_FILE_MACHINE_I386
    if i386:
        allowed = tuple(c for c in allowed if c not in X64_ONLY_CLASSES)
    if a.anonymous_locals and some is not None:
        promotions = find_promotions(base, new, i386)
    if promotions:
        # the second pass: NEW read again with each promoted global known as the file-local it was
        defining = sorted({k for n in promotions for v in new[n]["bodies"].values() for k in v})
        lh: dict[str, str] = {}
        for k in defining:
            lh.update(promoted_hashes(new_objs[k], set(promotions), nroots, pad_free))
        new, ntext, nn = symbol_table(new_objs, wanted, True, nroots, pad_free, lh)
    if a.show:
        for side, t, tx in (("BASE", base, btext), ("NEW", new, ntext)):
            e = t.get(a.show)
            if not e:
                print(f"--- {side}: {a.show} not defined")
                continue
            for h, where in e["bodies"].items():
                print(f"--- {side}: {a.show} in {', '.join(where)}")
                print("\n".join(tx[h]))
        return 0
    bad = copies = injects = excepts = 0
    counted = {"code": 0, "data": 0, "bss": 0}
    for name in sorted(set(base) | set(new)):
        x, y = base.get(name), new.get(name)
        if a.inject_ok and INJECT_REGISTRATION.match(name) and (x is None or y is None or set(x["bodies"]) != set(y["bodies"])):
            print(f"INJECT {name}  ({'new' if x is None else 'gone' if y is None else 'changed'}; "
                  f"a moved BOF3_INJECT line, not counted)")
            injects += 1
            continue
        if y is None:
            cls = excepted(exceptions, name, x, allowed)
            if cls:
                print(f"EXCEPT {cls} {name}  (GONE; {x['kind']}, in {objects_of(x)}{named(name, x)}; not counted)")
                excepts += 1
                continue
            print(f"GONE   {name}  ({x['kind']}, in {objects_of(x)}{named(name, x)})")
            bad += 1
            continue
        if x is None:
            cls = excepted(exceptions, name, y, allowed)
            if cls:
                print(f"EXCEPT {cls} {name}  (NEW; {y['kind']}, in {objects_of(y)}{named(name, y)}; not counted)")
                excepts += 1
                continue
            print(f"NEW    {name}  ({y['kind']}, in {objects_of(y)}{named(name, y)})")
            bad += 1
            continue
        counted[y["kind"]] += 1
        if set(x["bodies"]) != set(y["bodies"]):
            tag = {"code": "DIFFER", "data": "DATA  ", "bss": "BSS   "}[y["kind"]]
            first = ""
            if len(x["bodies"]) == 1 and len(y["bodies"]) == 1:
                p, q = btext[next(iter(x["bodies"]))], ntext[next(iter(y["bodies"]))]
                i = next((j for j, (u, v) in enumerate(zip(p, q)) if u != v), min(len(p), len(q)))
                first = (f"  first at instruction {i}: {p[i] if i < len(p) else '<end>'!r}"
                         f" -> {q[i] if i < len(q) else '<end>'!r}")
            cls = excepted(exceptions, name, y, allowed)
            if cls:
                print(f"EXCEPT {cls} {name}  (DIFFER; base {objects_of(x)}; new {objects_of(y)}){first}; not counted")
                excepts += 1
                continue
            print(f"{tag} {name}  (base {objects_of(x)}; new {objects_of(y)}){first}")
            bad += 1
            continue
        nx = sum(len(v) for v in x["bodies"].values())
        ny = sum(len(v) for v in y["bodies"].values())
        if nx != ny:
            copies += 1
            if a.verbose:
                print(f"COPIES {name}  {nx} -> {ny} (the same bodies)")
    promoted = 0
    for n, was in sorted(promotions.items()):
        key = next((k for k, e in new.items() if k.startswith("local:") and n in e["names"]), None)
        if key and key in base and set(base[key]["bodies"]) == set(new[key]["bodies"]):
            print(f"PROMOTED {n}  (was the file-local {was}; the same body, in {objects_of(new[key])})")
            promoted += 1
    print(f"codegen_same --by-function: {counted['code']} code, {counted['data']} data, {counted['bss']} bss symbols "
          f"compared ({bn} / {nn} function definitions read), {bad} differ"
          + (f"; {copies} with another number of identical copies" if copies else "")
          + (f"; {injects} inject registrations changed (--inject-ok)" if injects else "")
          + (f"; {excepts} excepted (--exceptions)" if excepts else "")
          + (f"; {promoted} promoted to external linkage, the same body" if promoted else ""))
    for sym, x in sorted(exceptions.items()):
        if not x["used"] and x["class"] in allowed:
            print(f"codegen_same: unused exception {sym} ({x['class']}): it did not differ; prune the row by hand")
    if bn == 0 or nn == 0:
        print("codegen_same: nothing compared (no function in one of the builds)")
        return 2
    return 1 if bad else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("base", type=Path)
    ap.add_argument("new", type=Path)
    ap.add_argument("--only", action="append", default=[], help="compare only objects matching this glob (repeatable)")
    ap.add_argument("--allow-new", action="append", default=[], help="glob of objects allowed only in NEW")
    ap.add_argument("--allow-gone", action="append", default=[], help="glob of objects allowed only in BASE")
    ap.add_argument("--code-only", action="store_true", help="compare the code sections only, not the data")
    ap.add_argument("--by-function", action="store_true",
                    help="compare symbol by symbol across objects (the source-layout round's gate)")
    ap.add_argument("--show", metavar="NAME", help="with --by-function: print one symbol's normalised bodies")
    ap.add_argument("--verbose", action="store_true", help="with --by-function: list the COPIES lines too")
    ap.add_argument("--inject-ok", action="store_true",
                    help="with --by-function: list a changed *_Inject() / bof3::InjectAll as INJECT, not a difference")
    ap.add_argument("--anonymous-locals", action="store_true",
                    help="with --by-function: a file-local symbol is known by its body, not its name")
    ap.add_argument("--exceptions", type=Path, metavar="FILE",
                    help="with --by-function: analysis/tu_gate_exceptions.tsv; a listed symbol of a known "
                         "class prints as EXCEPT and is not counted")
    ap.add_argument("--exception-classes", metavar="CLASS[,CLASS]",
                    help="with --exceptions: only these classes may except (default every class); the i686 "
                         "gate passes data-layout")
    a = ap.parse_args()
    if a.exceptions and not a.by_function:
        ap.error("--exceptions belongs to --by-function")
    if a.exception_classes and not a.exceptions:
        ap.error("--exception-classes belongs to --exceptions")
    if a.by_function:
        if a.allow_new or a.allow_gone or a.code_only:
            ap.error("--allow-new, --allow-gone and --code-only belong to the object mode")
        return by_function(a)

    objdump = find_objdump()
    for d in (a.base, a.new):
        if not (d / "CMakeFiles").is_dir():
            print(f"codegen_same: {d} has no CMakeFiles: not a build directory")
            return 2
    base, new = objects(a.base), objects(a.new)
    broots, nroots = source_roots(a.base), source_roots(a.new)

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
        x, y = data_dump(objdump, base[k], broots), data_dump(objdump, new[k], nroots)
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
