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

    --exceptions FILE   (with --by-function; the round doc's 7a, the owner's call 8)

FILE is analysis/tu_gate_exceptions.tsv: a header line `symbol class reason
commit` (tab-separated), `#` lines comments, one row per symbol the x86_64
gate lets differ. A DIFFER, GONE or NEW line of a code symbol listed there
whose class is a known one - `inline` (the compiler inlined, or stopped
inlining, a moved or copied helper because its callers in the file changed)
or `low32-site` (Low32()'s failure message names the new file and line) -
prints as `EXCEPT <class> <symbol>` and is not counted. The symbol is the
name the line prints (for a `local:<hash>` key, any of the names it lists).
A data, bss or inject line is never excepted. A row whose class is not a
known one, a row without a symbol and a class, or a file without the
header, is an error: the line is printed and the exit is 2, before anything
is compared. A listed symbol that did not differ is reported on an `unused
exception` line (not an error: a stale row, to prune by hand).
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

    def __init__(self, obj: Coff, anonymous_locals: bool = False):
        self.o = obj
        self.anonymous_locals = anonymous_locals
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
        sec = self.o.sections[secn - 1]
        data = bytearray(sec["data"][start:end])
        notes = []
        for off, symi, typ in self.relocs_in(secn, start, end):
            d, layout = self.reloc_desc(sec["data"], off, symi, typ, depth)
            if layout:
                w = self.width(typ)
                data[off - start:off - start + w] = bytes(w)
            notes.append(f"{off - start:X}={d}")
        return data.hex() + (" " + ",".join(notes) if notes else "")

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


def object_symbols(path: Path, anonymous_locals: bool = False) -> list[tuple[str, str, list[str], bool]]:
    """(name, kind, normalised body, file-local) for every defined symbol of one object."""
    obj = Coff(path)
    n = Normaliser(obj, anonymous_locals)
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


def symbol_table(objs: dict[str, Path], wanted, anonymous_locals: bool = False):
    """name -> {"kind": code | data | bss, "bodies": {hash: [objects]}}; texts by hash; functions read."""
    from concurrent.futures import ProcessPoolExecutor
    from functools import partial
    keys = [k for k in sorted(objs) if wanted(k)]
    table: dict[str, dict] = {}
    texts: dict[str, list[str]] = {}
    nfunc = 0
    with ProcessPoolExecutor() as pool:
        reader = partial(object_symbols, anonymous_locals=anonymous_locals)
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
                texts[h] = lines
    return table, texts, nfunc


EXCEPTION_CLASSES = ("inline", "low32-site")


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


def excepted(exceptions: dict, key: str, e: dict) -> str | None:
    """The class that excepts this code symbol's line, or None."""
    if not exceptions or e["kind"] != "code":
        return None
    for n in [key] + sorted(e["names"]):
        if n in exceptions:
            exceptions[n]["used"] = True
            return exceptions[n]["class"]
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
    base, btext, bn = symbol_table(objects(a.base), wanted, a.anonymous_locals)
    new, ntext, nn = symbol_table(objects(a.new), wanted, a.anonymous_locals)
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
            cls = excepted(exceptions, name, x)
            if cls:
                print(f"EXCEPT {cls} {name}  (GONE; {x['kind']}, in {objects_of(x)}{named(name, x)}; not counted)")
                excepts += 1
                continue
            print(f"GONE   {name}  ({x['kind']}, in {objects_of(x)}{named(name, x)})")
            bad += 1
            continue
        if x is None:
            cls = excepted(exceptions, name, y)
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
            cls = excepted(exceptions, name, y)
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
    print(f"codegen_same --by-function: {counted['code']} code, {counted['data']} data, {counted['bss']} bss symbols "
          f"compared ({bn} / {nn} function definitions read), {bad} differ"
          + (f"; {copies} with another number of identical copies" if copies else "")
          + (f"; {injects} inject registrations changed (--inject-ok)" if injects else "")
          + (f"; {excepts} excepted (--exceptions)" if excepts else ""))
    for sym, x in sorted(exceptions.items()):
        if not x["used"]:
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
                    help="with --by-function: analysis/tu_gate_exceptions.tsv; a listed code symbol of a known "
                         "class prints as EXCEPT and is not counted")
    a = ap.parse_args()
    if a.exceptions and not a.by_function:
        ap.error("--exceptions belongs to --by-function")
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
