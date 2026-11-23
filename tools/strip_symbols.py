#!/usr/bin/env python
"""Take the debug information out of a build's binaries, and keep it.

    python tools/strip_symbols.py --build build --out build/release/bof3x-<tag>-win-symbols.zip
    python tools/strip_symbols.py --build build-linux-x86_64 --linux --out ...-linux-x86_64-symbols.zip
    python tools/strip_symbols.py --check FILE...      # 0 when no file holds debug sections

The presets build RelWithDebInfo, so every binary holds its DWARF: 71 of
bof3x.dll's 93 MB and 31 of the launcher's 38 MB (measured 2026-11-23, the
32-bit build). A player's copy needs none of it. What a player hands back is
made without it: the log's CRASH lines name an address as module+0xOFFSET
(src/hook/crash.cpp), the Windows minidump is MiniDumpWriteDump's, and
addresses of the game's image are named from symbols.toml. The debug
information is read on the developer's side only (tools/crash_report.py --elf,
a debugger), from a file of the same build.

So the release keeps both: this tool copies each binary as built into a zip
(the symbols), then strips the build's own file in place of its .debug_*
sections (strip --strip-debug: the symbol table stays, so a debugger and
addr2line still name functions from the player's file). The log's "build:"
line (src/hook/dllmain.cpp, exe_host_posix.cpp) names the tag and the build
id - the GNU build id on Linux, bof3x.dll's CodeView GUID and age on Windows
- which the strip keeps: that is how a log finds its symbols.

Run it after the build and BEFORE the binaries are signed (a strip after
Authenticode breaks the signature) and before tools/make_release.py, which
refuses a binary that still holds debug sections.

It refuses a file that holds no debug sections: that file was stripped
already, and keeping it as "the symbols" would keep nothing. Build again.

bof3x.exe is not here: tools/make_exe.py writes it, with no debug sections.
"""
import argparse, os, shutil, struct, subprocess, sys, zipfile

WINDOWS_FILES = ["bof3x.dll", "bof3x-launcher.exe"]
LINUX_FILES = ["bof3x", "bof3x-launcher", "libSDL3.so.0", "libdxvk_d3d11.so.0", "libdxvk_dxgi.so.0"]


def section_names(path):
    """The section names of a PE or an ELF64 file, from its own headers."""
    with open(path, "rb") as f:
        d = f.read()
    if d[:4] == b"\x7fELF":
        if d[4] != 2 or d[5] != 1:
            raise SystemExit("%s: not a little-endian ELF64" % path)
        shoff, = struct.unpack_from("<Q", d, 0x28)
        entsize, num, strndx = struct.unpack_from("<HHH", d, 0x3A)
        if not shoff or not num:
            return []

        def header(i):
            name, = struct.unpack_from("<I", d, shoff + i * entsize)
            off, size = struct.unpack_from("<QQ", d, shoff + i * entsize + 0x18)
            return name, off, size
        _, str_off, str_size = header(strndx)
        table = d[str_off:str_off + str_size]
        return [table[n:table.index(b"\0", n)].decode("ascii", "replace") for n, _, _ in map(header, range(num))]
    if d[:2] == b"MZ":
        pe, = struct.unpack_from("<I", d, 0x3C)
        if d[pe:pe + 4] != b"PE\0\0":
            raise SystemExit("%s: no PE header" % path)
        coff = pe + 4
        num, = struct.unpack_from("<H", d, coff + 2)
        symtab, nsyms = struct.unpack_from("<II", d, coff + 8)
        optsize, = struct.unpack_from("<H", d, coff + 16)
        strings = symtab + nsyms * 18          # the COFF string table: long section names
        names = []
        for i in range(num):
            raw = d[coff + 20 + optsize + i * 40:][:8].rstrip(b"\0")
            if raw.startswith(b"/") and raw[1:].isdigit() and symtab:
                at = strings + int(raw[1:])
                raw = d[at:d.index(b"\0", at)]
            names.append(raw.decode("ascii", "replace"))
        return names
    raise SystemExit("%s: neither a PE nor an ELF file" % path)


def debug_sections(path):
    return [n for n in section_names(path) if n.startswith(".debug_") or n.startswith(".zdebug_")]


def strip_tool(linux):
    """Windows files: llvm-strip (llvm-mingw's, $LLVM_MINGW_ROOT/bin first). ELF files: strip or llvm-strip."""
    names = ["strip", "llvm-strip"] if linux else ["llvm-strip"]
    root = os.environ.get("LLVM_MINGW_ROOT")
    for n in names:
        if root and not linux:
            for cand in (os.path.join(root, "bin", n), os.path.join(root, "bin", n + ".exe")):
                if os.path.isfile(cand):
                    return cand
        found = shutil.which(n)
        if found:
            return found
    raise SystemExit("no %s on PATH%s" % (" or ".join(names), "" if linux else " (or under $LLVM_MINGW_ROOT/bin)"))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", help="the build folder whose binaries are stripped in place")
    ap.add_argument("--linux", action="store_true", help="the Linux build's five files, not the Windows build's two")
    ap.add_argument("--out", help="the zip that keeps the binaries as built (the symbols)")
    ap.add_argument("--check", nargs="+", metavar="FILE", help="only say which files hold debug sections; exit 1 if any")
    a = ap.parse_args()

    if a.check:
        bad = 0
        for p in a.check:
            found = debug_sections(p)
            print("%s: %s" % (p, ", ".join(found) if found else "no debug sections"))
            bad += bool(found)
        return 1 if bad else 0
    if not a.build or not a.out:
        ap.error("--build and --out are needed (or --check FILE...)")

    files = LINUX_FILES if a.linux else WINDOWS_FILES
    paths = [os.path.join(a.build, f) for f in files]
    missing = [p for p in paths if not os.path.isfile(p)]
    if missing:
        raise SystemExit("%s: not there. Build first." % ", ".join(missing))
    before = {p: debug_sections(p) for p in paths}
    # DXVK-native's two libraries are built without debug information (meson's
    # release build); every file of ours has it. A build with none in any file
    # was stripped already.
    if not any(before.values()):
        raise SystemExit("no file of %s holds debug sections: it was stripped already, and keeping these files "
                         "would keep no symbols. Build again, then run this once." % a.build)
    ours = [p for p in paths if os.path.basename(p).startswith("bof3x")]
    gone = [p for p in ours if not before[p]]
    if gone:
        raise SystemExit("%s: no debug sections, while another file has them. Build again." % ", ".join(gone))

    tool = strip_tool(a.linux)
    out = os.path.abspath(a.out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    folder = os.path.splitext(os.path.basename(out))[0]
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for p in paths:
            z.write(p, "%s/%s" % (folder, os.path.basename(p)))
    with zipfile.ZipFile(out) as z:
        if z.testzip() is not None:
            raise SystemExit("%s: the zip does not read back; nothing was stripped" % out)
    print("%s: %d files as built, %.1f MB" % (out, len(paths), os.path.getsize(out) / 1e6))

    for p in paths:
        size = os.path.getsize(p)
        tmp = p + ".stripped"
        subprocess.run([tool, "--strip-debug", "-o", tmp, p], check=True)
        left = debug_sections(tmp)
        if left:
            os.remove(tmp)
            raise SystemExit("%s: %s left %s" % (p, tool, ", ".join(left)))
        shutil.copymode(p, tmp)
        os.replace(tmp, p)
        print("  %s: %.1f MB -> %.1f MB (%s)" % (os.path.basename(p), size / 1e6, os.path.getsize(p) / 1e6,
                                               "%d debug sections out" % len(before[p]) if before[p] else "had none"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
