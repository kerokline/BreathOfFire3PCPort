#!/usr/bin/env python
"""Build the player's release zip: the launcher, the engine and a bundled importer.

    python tools/make_release.py --version v0.1.0-alpha.1 [--build build] [--out ZIP]
                                 [--downloads DIR] [--stage DIR]

docs/releases/README.md says how a release is cut; docs/install-sources.md
section 2 is the layout this writes, which the launcher relies on:

    bof3x-launcher.exe  bof3x.dll  bof3x.exe  music-names.tsv
    README.txt  LICENSE  THIRD_PARTY.md  THIRD_PARTY_RUNTIME.txt
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
"""
import argparse
import ast
import hashlib
import os
import re
import shutil
import subprocess
import sys
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
BUILD_FILES = ("bof3x-launcher.exe", "bof3x.dll", "bof3x.exe", "music-names.tsv")

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


README = """Breath of Fire III - bof3x {version} (alpha)
=================================================

A renovation of the 2001 PC port of Breath of Fire III. This download holds
no game data: you supply your own copy of the game.

1. Unzip this folder somewhere with a short path, e.g. C:\\Games\\bof3x
   (Windows cannot play the opening movie from a very long folder path).
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

ffmpeg (optional, recommended): the opening logo movie from a disc, the PSP's
sound effects and the PC logo movie's repair need ffmpeg. Get a Windows build
from https://ffmpeg.org/download.html#build-windows and either put it on your
PATH or locate ffmpeg.exe in the launcher. Then run setup again.

Not supported yet: .chd and .cso images (convert them back to .cue/.bin or
.iso), 2048-byte .iso images of a PlayStation disc (they lose the disc's
audio and movie sectors).

This is an alpha: much has been watched, not all of it played through. See
the release notes for what is known. Licences: LICENSE (this project),
THIRD_PARTY.md and THIRD_PARTY_RUNTIME.txt (what it bundles).
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


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--version", required=True, help="the release's tag, e.g. v0.1.0-alpha.1")
    ap.add_argument("--build", default=os.path.join(ROOT, "build"), help="where cmake built the launcher (default build/)")
    ap.add_argument("--out", help="the zip (default build/release/bof3x-VERSION-win.zip)")
    ap.add_argument("--downloads", default=os.path.join(ROOT, "build", "release-downloads"),
                    help="where the pinned downloads are kept between runs")
    ap.add_argument("--stage", help="lay the release out here and keep it (default a temporary folder)")
    a = ap.parse_args()
    out = os.path.abspath(a.out or os.path.join(ROOT, "build", "release", "bof3x-%s-win.zip" % a.version))
    missing = [f for f in BUILD_FILES if not os.path.isfile(os.path.join(a.build, f))]
    if missing:
        raise SystemExit("%s lacks %s: build first (cmake --preset i686 && cmake --build build)"
                         % (a.build, ", ".join(missing)))
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
            f.write(README.format(version=a.version))
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
    print("%s: %d files, %.1f MB" % (out, len(files), os.path.getsize(out) / 1e6))
    print("  python %s (%s: %s), %s" % (PYTHON_VERSION, PTH, ", ".join(l for l in pth if l.strip() and not l.startswith("#")),
                                         ", ".join(w[0] for w in WHEELS)))
    print("  runtime/tools: %d files (%s)" % (len(tools), " ".join(tools)))
    print("  runtime data: %s" % ", ".join(DATA))
    print("  not bundled (never reached from a player's import): %s" % ", ".join(sorted(k for k in third if k in NOT_BUNDLED)))
    print("  game-data scan: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
