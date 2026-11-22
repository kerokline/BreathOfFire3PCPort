#!/usr/bin/env python
"""Build the player's release zip: the launcher, the engine and a bundled importer.

    python tools/make_release.py --version v0.1.0-alpha.1 [--build build] [--out ZIP]
                                 [--downloads DIR] [--stage DIR]

docs/releases/README.md says how a release is cut; docs/install-sources.md
section 2 is the layout this writes, which the launcher relies on. Two
widths make two zips from the same layout: --build build (the i686 preset,
the player's default, -win.zip) and --build build-x86_64 (the x86_64 preset,
the 64-bit developer build, -win-x64.zip; docs/x64-build-round.md 6). The
width is read from the build's bof3x.exe; only the README's last paragraph
differs, and the bundled importer is the same amd64 Python either way:

    bof3x-launcher.exe  bof3x.dll  bof3x.exe  music-names.tsv  save-names.tsv
    README.txt  LICENSE  THIRD_PARTY.md  THIRD_PARTY_RUNTIME.txt
    licenses/             the notices of the work inside the launcher (the
                          drawn one since docs/launcher-portable.md stage P6):
                          dear-imgui/LICENSE.txt from the build's fetched tree,
                          droid-sans/ and tex-gyre-pagella/ from third_party/
    runtime/python/       python.org's embeddable CPython (amd64), its ._pth
                          naming ../tools and Lib/site-packages; numpy, Pillow
                          and capstone installed there from pinned wheels
    runtime/tools/        the .py files tools/importer.py reaches, measured by
                          walking its imports (and the scripts it runs by path)
    runtime/fixtures.toml, fixtures/, recipes/, ...   what those files read
                          relative to their ROOT (the folder above tools/), so
                          ROOT resolves to runtime/ with no code change

Every download is pinned by version and sha256 and kept in --downloads, so a
second run is offline. The zip is checked for game data before it is written
(CLAUDE.md rule 1): no file of a game-data kind, and no file whose sha256 is
one fixtures.toml catalogues.

The Linux tarball (docs/linux-round.md, group D2; section 6, calls 3 and 5):

    python tools/make_release.py --linux --version V [--build build-linux-x86_64] [--out TAR.GZ]

writes bof3x-V-linux-x86_64.tar.gz from the linux-x86_64 preset's build, one
folder of the same name inside it, portable as the zip is (the home is the
executable's folder):

    bof3x  bof3x-launcher  libSDL3.so.0  libdxvk_d3d11.so.0  libdxvk_dxgi.so.0
    music-names.tsv  save-names.tsv  README.txt  LICENSE  THIRD_PARTY.md
    licenses/             the zip's three, and dxvk/ and sdl3/ from the
                          build's fetched trees (the two libraries it ships)
    runtime/tools/        the zip's reached tools, and requirements.txt
    runtime/fixtures.toml, fixtures/, recipes/, ...   as the zip

No Python is in it: the importer runs on the system's (call 3). The launcher
takes runtime/python/bin/python3 when the player made that venv, else
python3 on PATH (src/launcher/ui/tool_run.cpp FindRuntime). Nothing is
downloaded. The same game-data scan runs on its stage; the modes are set in
the archive (0755 for the programs and libraries, 0644 for the rest), and the
ELF's RUNPATH and NEEDED names are read from the file and checked against
what the folder holds. The tarball is not code-signed.
"""
import argparse
import ast
import hashlib
import os
import re
import shutil
import subprocess
import struct
import sys
import tarfile
import tempfile
import tomllib
import urllib.request
import zipfile

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)

# python.org's embeddable package. sha256 measured 2026-10-15 and equal to the
# checksum in python.org's own python-3.13.16-embed-amd64.zip.spdx.json.
PYTHON_VERSION = "3.13.16"
PYTHON_ZIP = "python-%s-embed-amd64.zip" % PYTHON_VERSION
PYTHON_URL = "https://www.python.org/ftp/python/%s/%s" % (PYTHON_VERSION, PYTHON_ZIP)
PYTHON_SHA256 = "97dae5274cc54867065e8d5a3226e48c35017ed332a0fdb0e27d5b5821961297"
PTH = "python313._pth"

# The importer's third-party packages: (requirement, wheel, sha256). The
# hashes measured 2026-10-15, each equal to PyPI's published digest. The
# versions are those the owner's tools run with (numpy 2.2.6, Pillow 12.0.0,
# capstone 5.0.9), so the bundle runs what was tested.
WHEELS = [
    ("numpy==2.2.6", "numpy-2.2.6-cp313-cp313-win_amd64.whl",
     "b0544343a702fa80c95ad5d3d608ea3599dd54d4632df855e4c8d24eb6ecfa1c"),
    ("pillow==12.0.0", "pillow-12.0.0-cp313-cp313-win_amd64.whl",
     "4cf7fed4b4580601c4345ceb5d4cbf5a980d030fd5ad07c4d2ec589f95f09905"),
    ("capstone==5.0.9", "capstone-5.0.9-py3-none-win_amd64.whl",
     "732cedbbb56d42e723f14d7af6387f1454194a820b4b96b56d1e53f865ef85d0"),
]
# Import name -> the wheel that provides it.
PROVIDES = {"numpy": "numpy", "PIL": "pillow", "capstone": "capstone"}
# Third-party imports the reached tools make only on paths a player's import
# never takes; not bundled. Each with where it is and why it is safe to omit.
NOT_BUNDLED = {
    "ahocorasick": "exe_twins.py runs()/places(): the recipe generators (exe_tables.py map/places), the owner's",
    "inventory": "seq.py cmd_verify: tools/bgm/inventory.py, which needs the sibling checkout",
    "sudachipy": "ruby_build.py Reader: the reading-row builder's tokeniser; mods.py's setup step refuses loudly "
                 "without it (reading-row-round.md 6, the owner's call on bundling SudachiPy and its dictionary)",
}
ENTRY = ("importer.py", "player_sources.py")

# What the reached tools read relative to ROOT during identify / sources /
# setup (build + install), measured 2026-10-15 by an audit hook on every
# open() under ROOT during setups from a US disc, the PC folder + US disc and
# a PSP-JP disc (docs/install-sources.md section 6): fixtures.toml, seven
# fixtures/*.tsv, sixteen recipes/ files, symbols.toml, tables.toml,
# exe_maps/<build>.tsv and src/hook/exe_own.cpp (psp_exe.py reads its
# Dat_FileNames pool; the first bundled setup without it stopped there).
# Directories are copied whole; src/ only that one file, our own code.
# One list with what the measured setup steps record as data (build_reuse.SHIPPED_DATA, HANDOFF 11).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_reuse import SHIPPED_DATA as DATA   # noqa: E402
# Copied beside the launcher.
TOP = [("LICENSE", "LICENSE"), ("docs/THIRD_PARTY.md", "THIRD_PARTY.md")]
BUILD_FILES = ("bof3x-launcher.exe", "bof3x.dll", "bof3x.exe", "music-names.tsv", "save-names.tsv")
# The notices of the third-party work inside the launcher (the drawn one since
# stage P6 of docs/launcher-portable.md; docs/THIRD_PARTY.md sections 6 and 7),
# into licenses\ in the zip: (folder in licenses\, the file's place in the
# repository). Dear ImGui's MIT notice is its fetched tree's own LICENSE.txt
# (imgui_notice below); Droid Sans's Apache License 2.0 and TeX Gyre
# Pagella's four text files are in third_party/.
LICENSES = [("droid-sans", "third_party/droid-sans/LICENSE-Apache-2.0.txt"),
            ("droid-sans", "third_party/droid-sans/README-bof3x.txt"),
            ("tex-gyre-pagella", "third_party/tex-gyre-pagella/GUST-FONT-LICENSE.txt"),
            ("tex-gyre-pagella", "third_party/tex-gyre-pagella/LPPL-1.3c.txt"),
            ("tex-gyre-pagella", "third_party/tex-gyre-pagella/MANIFEST-TeX-Gyre-Pagella.txt"),
            ("tex-gyre-pagella", "third_party/tex-gyre-pagella/README-bof3x.txt")]


def imgui_notice(build):
    """Dear ImGui's LICENSE.txt in the tree the build compiled: <build>/_deps/imgui-src, or the folder a
    -DFETCHCONTENT_SOURCE_DIR_IMGUI named (CMakeCache.txt)."""
    tree = os.path.join(build, "_deps", "imgui-src")
    cache = os.path.join(build, "CMakeCache.txt")
    if os.path.isfile(cache):
        with open(cache, encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith("FETCHCONTENT_SOURCE_DIR_IMGUI:") and line.split("=", 1)[1].strip():
                    tree = line.split("=", 1)[1].strip()
    p = os.path.join(tree, "LICENSE.txt")
    if not os.path.isfile(p):
        raise SystemExit("%s: Dear ImGui's LICENSE.txt is not there; the launcher carries Dear ImGui and the release "
                         "owes its notice (docs/THIRD_PARTY.md section 6). Build with cmake first." % p)
    with open(p, encoding="utf-8", errors="replace") as f:
        if "Omar Cornut" not in f.read():
            raise SystemExit("%s is not Dear ImGui's MIT notice" % p)
    return p

# Kinds of file that are game data, or disc images of it: never in a release.
GAME_DATA_EXT = (".dat", ".bin", ".iso", ".cue", ".chd", ".cso", ".avi", ".mp3", ".str", ".xa", ".emi", ".pmf",
                 ".at3", ".vag", ".vab", ".sep", ".seq", ".cab", ".mdf", ".mds", ".img")
GAME_DATA_NAMES = ("bof3.exe", "start.exe", "setup.exe", "data.tag", "layout.bin", "bof3.cfg")
# A few files of the embeddable Python and the wheels end in one of those
# extensions legitimately; none does today (checked by the scan).


def sha_file(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def fetch(url, name, sha, downloads):
    """downloads/name, fetched once and checked against its pinned sha256."""
    os.makedirs(downloads, exist_ok=True)
    p = os.path.join(downloads, name)
    if not os.path.isfile(p) or sha_file(p) != sha:
        print("download %s" % url)
        with urllib.request.urlopen(url) as r, open(p + ".part", "wb") as f:
            shutil.copyfileobj(r, f)
        os.replace(p + ".part", p)
    got = sha_file(p)
    if got != sha:
        raise SystemExit("%s: sha256 %s, pinned %s - refusing it" % (name, got, sha))
    return p


def fetch_wheels(downloads):
    d = os.path.join(downloads, "wheels")
    os.makedirs(d, exist_ok=True)
    missing = [w for w in WHEELS if not os.path.isfile(os.path.join(d, w[1])) or sha_file(os.path.join(d, w[1])) != w[2]]
    if missing:
        subprocess.run([sys.executable, "-m", "pip", "download", "--quiet", "--no-deps", "--only-binary=:all:",
                        "--platform", "win_amd64", "--python-version", "3.13", "--implementation", "cp",
                        "-d", d] + [w[0] for w in missing], check=True)
    for req, name, sha in WHEELS:
        p = os.path.join(d, name)
        if not os.path.isfile(p):
            raise SystemExit("%s: pip did not write %s" % (req, name))
        if sha_file(p) != sha:
            raise SystemExit("%s: sha256 %s, pinned %s - refusing it" % (name, sha_file(p), sha))
    return d


def reached_tools():
    """(the tools/*.py the entry points reach, {third-party import: [module]}).
    Every import statement counts, at any depth (importer.py imports many
    lazily), and so does a string literal naming a tools/*.py, which is how
    importer.py runs loc_build.py."""
    local = {f[:-3] for f in os.listdir(TOOLS) if f.endswith(".py")}
    seen, third, todo = set(), {}, [e[:-3] for e in ENTRY]
    while todo:
        m = todo.pop()
        if m in seen:
            continue
        seen.add(m)
        with open(os.path.join(TOOLS, m + ".py"), encoding="utf-8") as f:
            tree = ast.parse(f.read())
        for n in ast.walk(tree):
            names = []
            if isinstance(n, ast.Import):
                names = [a.name.split(".")[0] for a in n.names]
            elif isinstance(n, ast.ImportFrom) and n.module and n.level == 0:
                names = [n.module.split(".")[0]]
            elif isinstance(n, ast.Constant) and isinstance(n.value, str) and re.fullmatch(r"\w+\.py", n.value) \
                    and n.value[:-3] in local:
                names = [n.value[:-3]]
            for x in names:
                if x in local:
                    todo.append(x)
                elif x not in sys.stdlib_module_names:
                    third.setdefault(x, []).append(m)
    return sorted(seen), third


def edit_pth(pydir):
    p = os.path.join(pydir, PTH)
    with open(p, encoding="utf-8") as f:
        lines = f.read().splitlines()
    for extra in ("../tools", "Lib/site-packages"):
        if extra not in lines:
            at = max((i for i, l in enumerate(lines) if l.strip() and not l.startswith("#")), default=-1) + 1
            lines.insert(at, extra)
    with open(p, "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")
    return lines


README = """Breath of Fire III - bof3x {version} ({stage})
=================================================

A renovation of the 2001 PC port of Breath of Fire III. This download holds
no game data: you supply your own copy of the game.

1. Unzip this folder somewhere with a short path, e.g. C:\\Games\\bof3x.
2. Run bof3x-launcher.exe.
3. On the Sources page, add what you own - any of:
     - the PC game's folder (the one holding BOF3.exe and DAT)
     - a PlayStation disc image as .cue/.bin (or a .zip of them)
     - a PSP disc image as .iso (or a .zip of it)
   Each is checked against the known releases; the first one makes the
   install, later ones add their languages and extras.
4. Play.

Everything the setup writes stays in this folder: sources.toml, cache\\ and
game\\. Your own files are only read, never changed.

ffmpeg (optional): a PlayStation disc needs none. Only a PC install's logo
movie repair and the PSP's sound effects and logo movie need ffmpeg. Get a Windows build
from https://ffmpeg.org/download.html#build-windows and either put it on your
PATH or locate ffmpeg.exe in the launcher. Then run setup again.

Not supported yet: .chd and .cso images (convert them back to .cue/.bin or
.iso), 2048-byte .iso images of a PlayStation disc (they lose the disc's
audio and movie sectors).

{stage_note} See
the release notes for what is known. Licences: LICENSE (this project),
THIRD_PARTY.md and THIRD_PARTY_RUNTIME.txt (what it bundles). The licenses\\
folder holds the licence texts of the work inside bof3x-launcher.exe: Dear
ImGui, which draws the window, and the two fonts of its text, Droid Sans and
TeX Gyre Pagella.
{width_note}"""

# The README's first line and its last paragraph follow the tag: an alpha was
# watched more than played; a beta has been played through by hand
# (docs/beta-readiness.md 5). Anything else (a plain v0.1.0, a latest-<sha>
# build) is named a build and told nothing it cannot promise.
def stage_of(version: str) -> tuple[str, str]:
    if "-alpha" in version:
        return "alpha", "This is an alpha: much has been watched, not all of it played through."
    if "-beta" in version:
        return "beta", "This is a beta: the game has been played to its end by hand."
    return "build", "This is a build from the project's main branch, not a release."


# Appended to the README of the -win-x64 zip alone; the -win zip's README is
# unchanged. The player is told which zip is the default in one sentence.
WIDTH_NOTE_64 = """
This zip is the 64-bit build (bof3x-*-win-x64.zip), made for developers. The
32-bit zip (bof3x-*-win.zip) is the default. Both need the same copy of the
game.
"""

RUNTIME_NOTES = """Third-party software in runtime\\ (the bundled importer)
======================================================

runtime\\python\\
  CPython {py} embeddable package for Windows (amd64), from python.org,
  unmodified but for its {pth} (two search paths added).
  Licence: the PSF License Agreement - runtime\\python\\LICENSE.txt, which also
  carries the notices of the libraries CPython bundles.

runtime\\python\\Lib\\site-packages\\ (installed from PyPI wheels, unmodified)
{wheels}
  Each package's own licence text is in its *.dist-info folder there. numpy's
  LICENSE.txt lists what its wheel bundles: OpenBLAS and LAPACK (BSD), and the
  GCC runtime library (GPL-3.0 with the GCC Runtime Library Exception, which
  permits redistributing it with other software under any terms), and
  numpy.libs\\ carries Microsoft's msvcp140 redistributable.
    numpy    - BSD-3-Clause
    Pillow   - MIT-CMU (HPND)
    capstone - BSD-3-Clause

runtime\\tools\\, runtime\\fixtures*, runtime\\recipes\\ ... are this project's own
files (LICENSE). They hold names, sizes and hashes, never game data.
"""


def exe_width(p):
    """32 or 64 from bof3x.exe's optional-header magic (0x10B PE32, 0x20B
    PE32+; tools/make_exe.py writes the one its --width names)."""
    with open(p, "rb") as f:
        head = f.read(0x40)
        if head[:2] != b"MZ" or len(head) < 0x40:
            raise SystemExit("%s: not a PE file" % p)
        f.seek(int.from_bytes(head[0x3C:0x40], "little"))
        pe = f.read(0x1A)
    if pe[:4] != b"PE\0\0":
        raise SystemExit("%s: not a PE file" % p)
    magic = int.from_bytes(pe[0x18:0x1A], "little")
    if magic == 0x10B:
        return 32
    if magic == 0x20B:
        return 64
    raise SystemExit("%s: optional-header magic %#x is neither PE32 nor PE32+" % (p, magic))


def scan(stage):
    """Problems: game-data kinds by name, and any file whose sha256 is a
    catalogued artifact's (fixtures.toml)."""
    with open(os.path.join(ROOT, "fixtures.toml"), "rb") as f:
        fx = tomllib.load(f)
    known = {a["sha256"] for b in fx["build"] for a in b.get("artifacts", {}).values() if a.get("sha256")}
    bad = []
    for d, _, fs in os.walk(stage):
        for n in fs:
            p = os.path.join(d, n)
            rel = os.path.relpath(p, stage)
            if n.lower().endswith(GAME_DATA_EXT) or n.lower() in GAME_DATA_NAMES:
                bad.append("%s: a game-data kind of file" % rel)
            elif os.path.getsize(p) > 256 and sha_file(p) in known:
                bad.append("%s: a file fixtures.toml catalogues" % rel)
    return bad


# ---- The Linux tarball (--linux) ---------------------------------------------------
# docs/linux-round.md, group D2. Its own code path: nothing above changes for
# the Windows zips.

# What the linux-x86_64 preset's build folder holds beside the ELF
# (cmake/linux_target.cmake): the ELF, the launcher, the three libraries
# under their sonames (RUNPATH $ORIGIN finds them there) and the names files.
LINUX_BUILD_FILES = ("bof3x", "bof3x-launcher", "libSDL3.so.0", "libdxvk_d3d11.so.0", "libdxvk_dxgi.so.0",
                     "music-names.tsv", "save-names.tsv")
LINUX_PROGRAMS = ("bof3x", "bof3x-launcher")
# Libraries a player's system holds (glibc, the C++ runtime): the one kind
# of NEEDED name allowed that the folder does not hold. Anything else is a
# library the tarball would need and does not carry, and the script stops.
LINUX_SYSTEM_LIBS = ("libc.so.6", "libm.so.6", "libstdc++.so.6", "libgcc_s.so.1", "ld-linux-x86-64.so.2",
                     "libdl.so.2", "libpthread.so.0", "librt.so.1")
# The notices of the third-party work in the two shipped libraries the zip
# does not have, from the trees the build fetched (docs/THIRD_PARTY.md
# sections 2 and 8): (folder in licenses/, tree, the file in it, a text the
# file must hold). "dxvk" is the tree cmake/dxvk_native.cmake built the
# libraries from (DXVK v3.1.1, b1a1c99a, with its submodules); libdisplay-info
# and dxbc-spirv are compiled into them; Vulkan-Headers, SPIRV-Headers and the
# MinGW DirectX headers are headers they compile against.
LINUX_LICENSES = [("dxvk", "dxvk", "LICENSE", "Philip Rebohle"),
                  ("dxbc-spirv", "dxvk", "subprojects/dxbc-spirv/LICENSE", "Philip Rebohle"),
                  ("libdisplay-info", "dxvk", "subprojects/libdisplay-info/LICENSE", "libdisplay-info Contributors"),
                  ("vulkan-headers", "dxvk", "include/vulkan/LICENSE.md", "Khronos"),
                  ("vulkan-headers", "dxvk", "include/vulkan/LICENSES/Apache-2.0.txt", "Apache License"),
                  ("vulkan-headers", "dxvk", "include/vulkan/LICENSES/MIT.txt", "MIT"),
                  ("spirv-headers", "dxvk", "include/spirv/LICENSE", "Khronos"),
                  ("mingw-directx-headers", "dxvk", "include/native/directx/README.md", "LGPL"),
                  ("mingw-directx-headers", "dxvk", "include/native/directx/COPYING.MinGW-w64.txt", "MinGW-w64"),
                  ("sdl3", "sdl3", "LICENSE.txt", "Sam Lantinga")]


def cache_value(build, key):
    """A CMakeCache.txt entry's value, or ''."""
    p = os.path.join(build, "CMakeCache.txt")
    if os.path.isfile(p):
        with open(p, encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith(key + ":"):
                    return line.split("=", 1)[1].strip()
    return ""


def linux_trees(build):
    """{tree name: folder} for LINUX_LICENSES: DXVK as the build's external project fetched it, SDL3 as
    FetchContent did (or the folder -DFETCHCONTENT_SOURCE_DIR_SDL3 named)."""
    if cache_value(build, "BOF3X_DXVK_LIBDIR"):
        raise SystemExit("%s was configured with BOF3X_DXVK_LIBDIR=%s: DXVK's libraries were built elsewhere, so the "
                         "release cannot vouch for their commit or their notices. Configure without it."
                         % (build, cache_value(build, "BOF3X_DXVK_LIBDIR")))
    return {"dxvk": os.path.join(build, "_deps", "dxvk-native", "src", "dxvk_native"),
            "sdl3": cache_value(build, "FETCHCONTENT_SOURCE_DIR_SDL3") or os.path.join(build, "_deps", "sdl3-src")}


def elf_dynamic(p):
    """(NEEDED names, RUNPATH, RPATH, the strings of .dynstr) of a 64-bit little-endian ELF, read from its
    PT_DYNAMIC segment."""
    with open(p, "rb") as f:
        data = f.read()
    if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        raise SystemExit("%s: not a 64-bit little-endian ELF" % p)
    phoff, = struct.unpack_from("<Q", data, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", data, 0x36)
    loads, dyn = [], None
    for i in range(phnum):
        p_type, _, p_offset, p_vaddr, _, p_filesz, _, _ = struct.unpack_from("<IIQQQQQQ", data, phoff + i * phentsize)
        if p_type == 1:
            loads.append((p_vaddr, p_offset, p_filesz))
        elif p_type == 2:
            dyn = (p_offset, p_filesz)
    if dyn is None:
        raise SystemExit("%s: no dynamic segment" % p)
    tags = []
    for at in range(dyn[0], dyn[0] + dyn[1], 16):
        tag, val = struct.unpack_from("<qQ", data, at)
        if tag == 0:
            break
        tags.append((tag, val))
    strtab = next(v for t, v in tags if t == 5)
    strsz = next(v for t, v in tags if t == 10)
    off = next(o + strtab - va for va, o, sz in loads if va <= strtab < va + sz)
    table = data[off:off + strsz]
    def s(i):
        return table[i:table.index(b"\0", i)].decode("utf-8", "replace")
    needed = [s(v) for t, v in tags if t == 1]
    runpath = [s(v) for t, v in tags if t == 29]
    rpath = [s(v) for t, v in tags if t == 15]
    return needed, runpath, rpath, [x.decode("utf-8", "replace") for x in table.split(b"\0") if x]


def newest(strings, prefix):
    """The highest version named prefix+N.N[.N] among strings, as text, or ''."""
    best = None
    for x in strings:
        m = re.fullmatch(re.escape(prefix) + r"(\d+(?:\.\d+)+)", x)
        if m:
            v = tuple(int(n) for n in m.group(1).split("."))
            best = v if best is None or v > best else best
    return prefix + ".".join(map(str, best)) if best else ""


def requirement_names(path):
    """The package names of a pip requirements file, lower case."""
    names = set()
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line:
                names.add(re.split(r"[=<>!~\[; ]", line, maxsplit=1)[0].lower())
    return names


README_LINUX = """Breath of Fire III - bof3x {version} ({stage}), Linux x86-64
=================================================================

A renovation of the 2001 PC port of Breath of Fire III. This download holds
no game data. You supply your own copy of the game.

This is the first Linux build. It ran only on a computer with no screen and
with a software graphics driver. Nobody has played it on a graphics card or
heard its sound yet. Please tell us what you see.

What you need
-------------

- A 64-bit Linux system (x86-64) with {glibc} or newer and the GNU C++
  library with {glibcxx} or newer.
- A Vulkan 1.3 graphics driver (Mesa or your graphics card's driver).
- X11 or Wayland.
- Python 3.11 or newer, with the packages in runtime/tools/requirements.txt
  (Pillow and capstone). The setup of the game uses them. Steps 2 and 3
  below install them.
- ffmpeg (optional): a PlayStation disc needs none. Only a PC install's logo
  movie repair and the PSP's sound effects and logo movie need ffmpeg.
  Install it from your system's packages, then run the setup again.

How to start
------------

1. Extract this folder to a place you can write to, e.g. ~/Games:
     tar -xzf bof3x-{version}-linux-x86_64.tar.gz -C ~/Games
2. Open a terminal in the folder bof3x-{version}-linux-x86_64.
3. Make a Python environment for the setup in runtime/python:
     python3 -m venv runtime/python
     runtime/python/bin/pip install -r runtime/tools/requirements.txt
   On Debian and Ubuntu, "python3 -m venv" needs the package python3-venv.
   With uv, do this instead:
     uv venv runtime/python
     uv pip install --python runtime/python -r runtime/tools/requirements.txt
   If you do not make runtime/python, the launcher uses python3 from your
   PATH. Then install Pillow and capstone for that Python yourself.
4. Start the launcher:
     ./bof3x-launcher
5. On the Sources page, add what you own - any of:
     - the PC game's folder (the one holding BOF3.exe and DAT)
     - a PlayStation disc image as .cue/.bin (or a .zip of them)
     - a PSP disc image as .iso (or a .zip of it)
   Each is checked against the known releases. The first one makes the
   install. Later ones add their languages and extras.
6. Play.

Everything the setup writes stays in this folder: sources.toml, cache/ and
game/. The settings (bof3x.ini), the log (bof3x.log) and the graphics logs
also stay here. Your own files are only read, never changed. To use another
folder for these files, set BOF3X_HOME.

Not supported yet: .chd and .cso images (convert them back to .cue/.bin or
.iso), 2048-byte .iso images of a PlayStation disc (they lose the disc's
audio and movie sectors). The Linux build has no developer launcher: the
hosted build and a moved image are on Windows only.

{stage_note} See
the release notes for what is known. Licences: LICENSE (this project) and
THIRD_PARTY.md (what it uses). The licenses/ folder holds the licence texts
of the work inside the programs and libraries of this folder: Dear ImGui and
the fonts Droid Sans and TeX Gyre Pagella (in bof3x-launcher), SDL3
(libSDL3.so.0), and DXVK with the work it uses (libdxvk_d3d11.so.0 and
libdxvk_dxgi.so.0).
"""


def main_linux(a):
    """The Linux tarball: docs/linux-round.md group D2. See the module's docstring."""
    stage_word, stage_note = stage_of(a.version)
    build = os.path.abspath(a.build or os.path.join(ROOT, "build-linux-x86_64"))
    missing = [f for f in LINUX_BUILD_FILES if not os.path.isfile(os.path.join(build, f))]
    if missing:
        raise SystemExit("%s lacks %s: build first, on Linux (cmake --preset linux-x86_64 && cmake --build %s)"
                         % (build, ", ".join(missing), os.path.basename(os.path.normpath(build))))
    name = "bof3x-%s-linux-x86_64" % a.version
    out = os.path.abspath(a.out or os.path.join(ROOT, "build", "release", name + ".tar.gz"))

    # The importer's third-party imports against the requirements files: a
    # Linux player installs them (call 3), so each import a player's setup
    # can reach is in requirements.txt or, named there, in requirements-extra.txt.
    tools, third = reached_tools()
    req = requirement_names(os.path.join(TOOLS, "requirements.txt"))
    extra = requirement_names(os.path.join(TOOLS, "requirements-extra.txt"))
    uncovered = sorted(k for k in third if k not in NOT_BUNDLED and PROVIDES.get(k, k).lower() not in req | extra)
    if uncovered:
        raise SystemExit("the importer imports %s, which neither tools/requirements.txt nor requirements-extra.txt "
                         "names, nor NOT_BUNDLED lists" % ", ".join(uncovered))
    in_req = sorted(k for k in third if PROVIDES.get(k, k).lower() in req)
    in_extra = sorted(k for k in third if k not in in_req and PROVIDES.get(k, k).lower() in extra)

    # Each binary: what it needs, and where the loader finds it.
    elf = {}
    for f in LINUX_BUILD_FILES:
        if f in LINUX_PROGRAMS or ".so" in f:
            elf[f] = elf_dynamic(os.path.join(build, f))
    for f in LINUX_PROGRAMS:
        needed, runpath, rpath, _ = elf[f]
        if runpath != ["$ORIGIN"] or rpath:
            raise SystemExit("%s: RUNPATH %s, RPATH %s - the release needs RUNPATH $ORIGIN alone "
                             "(cmake/linux_target.cmake)" % (f, runpath or "none", rpath or "none"))
    for f, (needed, _, _, _) in elf.items():
        foreign = [n for n in needed if n not in LINUX_BUILD_FILES and n not in LINUX_SYSTEM_LIBS]
        if foreign:
            raise SystemExit("%s needs %s, which the tarball does not hold and LINUX_SYSTEM_LIBS does not name"
                             % (f, ", ".join(foreign)))
        # A bundled library a library needs must be loaded already by the
        # programs (their RUNPATH does not reach a library's own NEEDED).
        if f not in LINUX_PROGRAMS:
            for n in needed:
                if n in LINUX_BUILD_FILES and n not in elf["bof3x"][0]:
                    raise SystemExit("%s needs %s, which bof3x does not load first" % (f, n))
    dynstr = [x for v in elf.values() for x in v[3]]
    glibc, glibcxx = newest(dynstr, "GLIBC_"), newest(dynstr, "GLIBCXX_")

    trees = linux_trees(build)
    notices = []
    for folder, tree, rel, must in LINUX_LICENSES:
        p = os.path.join(trees[tree], *rel.split("/"))
        if not os.path.isfile(p):
            raise SystemExit("%s is not there: the release owes this notice (docs/THIRD_PARTY.md). Build with cmake "
                             "first, from a configure that fetched DXVK and SDL3." % p)
        with open(p, encoding="utf-8", errors="replace") as f:
            if must not in f.read():
                raise SystemExit("%s does not hold %r: not the notice the release expects" % (p, must))
        notices.append((folder, p))

    tmp = None
    if a.stage:
        stage = os.path.abspath(a.stage)
        if os.path.exists(stage):
            shutil.rmtree(stage)
    else:
        tmp = tempfile.mkdtemp(prefix="bof3x_release_")
        stage = os.path.join(tmp, "stage")
    top = os.path.join(stage, name)
    try:
        os.makedirs(top)
        for f in LINUX_BUILD_FILES:
            shutil.copyfile(os.path.join(build, f), os.path.join(top, f))
        for src, dst in TOP:
            shutil.copyfile(os.path.join(ROOT, src), os.path.join(top, dst))
        lic = os.path.join(top, "licenses")
        os.makedirs(os.path.join(lic, "dear-imgui"))
        shutil.copyfile(imgui_notice(build), os.path.join(lic, "dear-imgui", "LICENSE.txt"))
        for folder, src in LICENSES:
            os.makedirs(os.path.join(lic, folder), exist_ok=True)
            shutil.copyfile(os.path.join(ROOT, *src.split("/")), os.path.join(lic, folder, os.path.basename(src)))
        for folder, p in notices:
            os.makedirs(os.path.join(lic, folder), exist_ok=True)
            shutil.copyfile(p, os.path.join(lic, folder, os.path.basename(p)))
        rt = os.path.join(top, "runtime")
        os.makedirs(os.path.join(rt, "tools"))
        for m in tools:
            shutil.copyfile(os.path.join(TOOLS, m + ".py"), os.path.join(rt, "tools", m + ".py"))
        shutil.copyfile(os.path.join(TOOLS, "requirements.txt"), os.path.join(rt, "tools", "requirements.txt"))
        for d in DATA:
            s, t = os.path.join(ROOT, *d.split("/")), os.path.join(rt, *d.split("/"))
            os.makedirs(os.path.dirname(t), exist_ok=True)
            (shutil.copytree if os.path.isdir(s) else shutil.copyfile)(s, t)
        with open(os.path.join(top, "README.txt"), "w", encoding="utf-8", newline="\n") as f:
            f.write(README_LINUX.format(version=a.version, stage=stage_word, stage_note=stage_note,
                                        glibc=glibc.replace("GLIBC_", "glibc "), glibcxx=glibcxx))
        bad = scan(stage)
        if bad:
            raise SystemExit("game data in the release - refusing to write it:\n  " + "\n  ".join(bad[:20]))
        files = sorted(os.path.relpath(os.path.join(d, n), stage).replace(os.sep, "/")
                       for d, _, fs in os.walk(stage) for n in fs)
        dirs = sorted(os.path.relpath(d, stage).replace(os.sep, "/") for d, _, _ in os.walk(stage) if d != stage)

        def entry(rel, is_dir):
            # The modes are the archive's, whatever the host's filesystem
            # keeps: a tarball made on Windows has the same ones.
            ti = tarfile.TarInfo(rel)
            st = os.stat(os.path.join(stage, *rel.split("/")))
            ti.mtime = int(st.st_mtime)
            ti.uid = ti.gid = 0
            ti.uname = ti.gname = ""
            if is_dir:
                ti.type, ti.mode = tarfile.DIRTYPE, 0o755
            else:
                base = rel.rsplit("/", 1)[-1]
                ti.size = st.st_size
                ti.mode = 0o755 if rel.count("/") == 1 and (base in LINUX_PROGRAMS or ".so" in base) else 0o644
            return ti

        os.makedirs(os.path.dirname(out), exist_ok=True)
        if os.path.exists(out):
            os.remove(out)
        with tarfile.open(out + ".part", "w:gz", compresslevel=9, format=tarfile.PAX_FORMAT) as t:
            for rel in sorted(dirs + files, key=lambda r: (r.split("/"))):
                if rel in dirs:
                    t.addfile(entry(rel, True))
                else:
                    with open(os.path.join(stage, *rel.split("/")), "rb") as f:
                        t.addfile(entry(rel, False), f)
        os.replace(out + ".part", out)
    finally:
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)
    print("%s: %d files, %.1f MB, from %s" % (out, len(files), os.path.getsize(out) / 1e6, build))
    for f in sorted(elf):
        needed, runpath, _, _ = elf[f]
        print("  %s: NEEDED %s%s" % (f, " ".join(needed), "; RUNPATH " + ":".join(runpath) if runpath else ""))
    print("  needs at least %s, %s (the binaries' version needs)" % (glibc, glibcxx))
    print("  licenses: dear-imgui droid-sans tex-gyre-pagella %s" % " ".join(dict.fromkeys(n[0] for n in notices)))
    print("  runtime/tools: %d files and requirements.txt (%s)" % (len(tools), " ".join(tools)))
    print("  the player installs: %s (requirements.txt); for paths a setup does not take by default: %s "
          "(requirements-extra.txt); NOT_BUNDLED: %s"
          % (", ".join(in_req), ", ".join(in_extra) or "none", ", ".join(sorted(k for k in third if k in NOT_BUNDLED))))
    print("  runtime data: %s" % ", ".join(DATA))
    print("  game-data scan: clean")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--version", required=True, help="the release's tag, e.g. v0.1.0-alpha.1")
    ap.add_argument("--build", help="where cmake built the launcher (default build/; with --linux build-linux-x86_64/)")
    ap.add_argument("--out", help="the zip (default build/release/bof3x-VERSION-win.zip; with --linux "
                                  "build/release/bof3x-VERSION-linux-x86_64.tar.gz)")
    ap.add_argument("--downloads", default=os.path.join(ROOT, "build", "release-downloads"),
                    help="where the pinned downloads are kept between runs")
    ap.add_argument("--stage", help="lay the release out here and keep it (default a temporary folder)")
    ap.add_argument("--linux", action="store_true",
                    help="the Linux tarball from the linux-x86_64 preset's build, in place of the Windows zip")
    a = ap.parse_args()
    if a.linux:
        return main_linux(a)
    a.build = a.build or os.path.join(ROOT, "build")
    stage_word, stage_note = stage_of(a.version)
    missing = [f for f in BUILD_FILES if not os.path.isfile(os.path.join(a.build, f))]
    if missing:
        # The advice names the preset whose binaryDir this is (CMakePresets.json).
        folder = os.path.basename(os.path.normpath(a.build))
        preset = "x86_64" if folder == "build-x86_64" else "i686"
        raise SystemExit("%s lacks %s: build first (cmake --preset %s && cmake --build %s)"
                         % (a.build, ", ".join(missing), preset, folder if preset == "x86_64" else "build"))
    width = exe_width(os.path.join(a.build, "bof3x.exe"))
    out = os.path.abspath(a.out or os.path.join(ROOT, "build", "release",
                                                "bof3x-%s-win%s.zip" % (a.version, "-x64" if width == 64 else "")))
    tools, third = reached_tools()
    bundled = {PROVIDES[k] for k in third if k in PROVIDES}
    unknown = sorted(k for k in third if k not in PROVIDES and k not in NOT_BUNDLED)
    if unknown:
        raise SystemExit("the importer imports %s, which the release neither bundles nor lists in NOT_BUNDLED: %s"
                         % (", ".join(unknown), "; ".join("%s from %s" % (k, ", ".join(third[k])) for k in unknown)))
    if bundled != {w[0].split("==")[0] for w in WHEELS}:
        raise SystemExit("WHEELS (%s) is not what the importer imports (%s)"
                         % (", ".join(w[0] for w in WHEELS), ", ".join(sorted(bundled))))
    pyzip = fetch(PYTHON_URL, PYTHON_ZIP, PYTHON_SHA256, a.downloads)
    wheels = fetch_wheels(a.downloads)

    tmp = None
    if a.stage:
        stage = os.path.abspath(a.stage)
        if os.path.exists(stage):
            shutil.rmtree(stage)
    else:
        tmp = tempfile.mkdtemp(prefix="bof3x_release_")
        stage = os.path.join(tmp, "stage")
    try:
        os.makedirs(stage)
        for f in BUILD_FILES:
            shutil.copy2(os.path.join(a.build, f), os.path.join(stage, f))
        for src, dst in TOP:
            shutil.copy2(os.path.join(ROOT, src), os.path.join(stage, dst))
        lic = os.path.join(stage, "licenses")
        os.makedirs(os.path.join(lic, "dear-imgui"))
        shutil.copy2(imgui_notice(a.build), os.path.join(lic, "dear-imgui", "LICENSE.txt"))
        for folder, src in LICENSES:
            os.makedirs(os.path.join(lic, folder), exist_ok=True)
            shutil.copy2(os.path.join(ROOT, *src.split("/")), os.path.join(lic, folder, os.path.basename(src)))
        rt = os.path.join(stage, "runtime")
        pydir = os.path.join(rt, "python")
        with zipfile.ZipFile(pyzip) as z:
            z.extractall(pydir)
        pth = edit_pth(pydir)
        site = os.path.join(pydir, "Lib", "site-packages")
        subprocess.run([sys.executable, "-m", "pip", "install", "--quiet", "--no-index", "--find-links", wheels,
                        "--target", site, "--platform", "win_amd64", "--python-version", "3.13",
                        "--implementation", "cp", "--only-binary=:all:", "--no-deps", "--no-compile"]
                       + [w[0] for w in WHEELS], check=True)
        for d in ("bin",):        # console scripts pip writes for the host; nothing calls them
            shutil.rmtree(os.path.join(site, d), ignore_errors=True)
        os.makedirs(os.path.join(rt, "tools"))
        for m in tools:
            shutil.copy2(os.path.join(TOOLS, m + ".py"), os.path.join(rt, "tools", m + ".py"))
        for d in DATA:
            s, t = os.path.join(ROOT, *d.split("/")), os.path.join(rt, *d.split("/"))
            os.makedirs(os.path.dirname(t), exist_ok=True)
            (shutil.copytree if os.path.isdir(s) else shutil.copy2)(s, t)
        with open(os.path.join(stage, "README.txt"), "w", encoding="utf-8", newline="\r\n") as f:
            f.write(README.format(version=a.version, stage=stage_word, stage_note=stage_note,
                                  width_note=WIDTH_NOTE_64 if width == 64 else ""))
        with open(os.path.join(stage, "THIRD_PARTY_RUNTIME.txt"), "w", encoding="utf-8", newline="\r\n") as f:
            f.write(RUNTIME_NOTES.format(py=PYTHON_VERSION, pth=PTH,
                                         wheels="\n".join("  %s (%s)" % (w[0], w[1]) for w in WHEELS)))
        bad = scan(stage)
        if bad:
            raise SystemExit("game data in the release - refusing to write it:\n  " + "\n  ".join(bad[:20]))
        os.makedirs(os.path.dirname(out), exist_ok=True)
        files = sorted(os.path.relpath(os.path.join(d, n), stage).replace(os.sep, "/")
                       for d, _, fs in os.walk(stage) for n in fs)
        if os.path.exists(out):
            os.remove(out)
        with zipfile.ZipFile(out + ".part", "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for rel in files:
                z.write(os.path.join(stage, *rel.split("/")), rel)
        os.replace(out + ".part", out)
    finally:
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)
    print("%s: %d files, %.1f MB, the %d-bit build from %s" % (out, len(files), os.path.getsize(out) / 1e6, width, a.build))
    print("  python %s (%s: %s), %s" % (PYTHON_VERSION, PTH, ", ".join(l for l in pth if l.strip() and not l.startswith("#")),
                                         ", ".join(w[0] for w in WHEELS)))
    print("  runtime/tools: %d files (%s)" % (len(tools), " ".join(tools)))
    print("  runtime data: %s" % ", ".join(DATA))
    print("  not bundled (never reached from a player's import): %s" % ", ".join(sorted(k for k in third if k in NOT_BUNDLED)))
    print("  game-data scan: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
