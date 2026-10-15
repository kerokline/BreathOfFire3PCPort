#!/usr/bin/env python
"""Where a player's install lives: one path resolver for the Python tools.

docs/install-sources.md section 2. A release (tools/make_release.py) is one
folder, the *home*, holding the launcher and everything the importer writes:

    <home>/bof3x-launcher.exe, bof3x.dll, bof3x.exe, music-names.tsv
    <home>/runtime/python/      the bundled interpreter
    <home>/runtime/tools/       these scripts; runtime/ also holds fixtures/,
                                recipes/ and the rest the importer reads
    <home>/sources.toml         the player's sources, in the build's order
    <home>/cache/               the data cache (importer.py build)
    <home>/game/                the game directory the launcher starts

home() is BOF3X_HOME when set, else the release layout (this file in
<home>/runtime/tools/), else None: a repository checkout, where every tool
keeps the default it had before this module (the repo's build/, bof3/,
CDImage/, ~/abc). A flag or an older environment variable always wins over
anything here; these are defaults only.
"""
import os

TOOLS = os.path.dirname(os.path.abspath(__file__))
# The folder holding fixtures.toml, recipes/ and tools/: the repository in a
# checkout, <home>/runtime in a release. importer.py's ROOT is the same path.
ROOT = os.path.dirname(TOOLS)


def release_home():
    """<home> when this file sits in <home>/runtime/tools/, else None."""
    if os.path.basename(TOOLS).lower() == "tools" and os.path.basename(ROOT).lower() == "runtime":
        return os.path.dirname(ROOT)
    return None


def home():
    """BOF3X_HOME, else the release layout's home, else None (a checkout)."""
    env = os.environ.get("BOF3X_HOME")
    if env:
        return os.path.abspath(env)
    return release_home()


def _under(name, h=None):
    h = h or home()
    return os.path.join(h, name) if h else None


def game_dir(h=None):
    """<home>/game, or None in a checkout."""
    return _under("game", h)


def cache_dir(h=None):
    """<home>/cache, or None in a checkout."""
    return _under("cache", h)


def sources_file(h=None):
    """<home>/sources.toml, or None in a checkout."""
    return _under("sources.toml", h)


def runtime_dir(h=None):
    """<home>/runtime, or None in a checkout."""
    return _under("runtime", h)


def tmp_dir(h=None):
    """Scratch for unpacking a .zip source: <home>/cache/tmp, else the
    system's temporary folder."""
    t = _under(os.path.join("cache", "tmp"), h)
    if t is None:
        import tempfile
        t = tempfile.gettempdir()
    return t


def launcher_dir():
    """Where the launcher, bof3x.dll and bof3x.log are: <home> in a release
    (or under BOF3X_HOME), the repo's build/ in a checkout."""
    return home() or os.path.join(ROOT, "build")


def default_game(repo_default):
    """A tool's --game default: <home>/game, else its own repo default."""
    return game_dir() or repo_default
