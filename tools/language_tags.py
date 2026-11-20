"""The language tags as the tools take them: the engine's own list and its
retired bare codes, mirrored from src/game/language_tags.h (kLanguageTags,
RetiredLanguageReplacement) - DIV-0005, docs/launcher-settings.md section 4.

The bare codes BOF3X_LANG, bof3x.ini's language= and the tools' --lang took
from 2026-09-27 to 2026-10-08 (en, fr, de, ja) are retired, not mapped (the
owner's word, 2026-10-10): every input that names a language refuses one, as
the launcher and the DLL do, naming the tag to write. A primary subtag compared
for logic (`primary(tag) == "en"`) is not an input and stays.
"""
import os
import re

# kLanguageTags, in its order.
TAGS = ("en-US", "en-150", "fr-FR", "de-DE", "ja-JP")

# RetiredLanguageReplacement's table: bare code -> what to write instead.
RETIRED = {
    "en": "en-US (en-150 for the European English)",
    "fr": "fr-FR",
    "de": "de-DE",
    "ja": "ja-JP",
}


def retired(code, where):
    """The refusal for `code` as `where` gave it (`--lang`, `BOF3X_LANG`), or
    None when it is not a retired bare code."""
    use = RETIRED.get(code)
    return None if use is None else "%s=%s is retired; use %s (DIV-0005)" % (where, code, use)


def refuse_retired(code, where):
    """SystemExit with retired()'s text when `code` is a retired bare code."""
    msg = retired(code, where)
    if msg:
        raise SystemExit(msg)
    return code


def refuse_retired_run(launcher, env):
    """Before a scripted launch: SystemExit when the environment the game gets
    or the bof3x.ini beside `launcher` names a retired code - the launcher
    would stop on it behind a message box, which a script would wait on."""
    refuse_retired(env.get("BOF3X_LANG", ""), "BOF3X_LANG")
    ini = os.path.join(os.path.dirname(os.path.abspath(launcher)), "bof3x.ini")
    if os.path.isfile(ini):
        with open(ini, encoding="utf-8", errors="replace") as f:
            for line in f:
                key, eq, value = line.strip().partition("=")
                if eq and key.strip() == "language":
                    msg = retired(value.strip(), "language")
                    if msg:
                        raise SystemExit("%s: %s" % (ini, msg))


def retired_overlays(dat_dir):
    """The overlay files under a retired bare code in a game's DAT/
    (`en.START.DAT`): no tool reads them any more, and the engine refuses the
    code that would."""
    if not os.path.isdir(dat_dir):
        return []
    pat = re.compile(r"(%s)\.[^.]+\.DAT" % "|".join(RETIRED), re.I)
    return sorted(f for f in os.listdir(dat_dir) if pat.fullmatch(f))


def note_retired_overlays(dat_dir):
    """One line on stdout when DAT/ still holds overlays under a retired code."""
    old = retired_overlays(dat_dir)
    if old:
        codes = sorted({f.split(".", 1)[0] for f in old})
        print("note: %s holds %d overlay files under the retired code%s %s (%s); unused - delete them"
              % (dat_dir, len(old), "s" if len(codes) > 1 else "", ", ".join(codes),
                 ", ".join("%s.*.DAT" % c for c in codes)))
    return old


def cache_kind(cache):
    """The install's kind as the cache records it (docs/exe-retire-round.md X1),
    common/cache_manifest.cpp ManifestInstallKind's answer: "pc" or "disc" from
    the manifest's [build] install_kind; for a manifest from before the key (or
    with a value the reader refuses, which the launcher then refuses to start
    on), "pc" when a [cache] assets row of the loc/zh-CN layer names a source
    (its chunk came from the PC's DAT/, the one source of that layer) - else
    "disc"; None with no manifest (no cache). A disc-built cache lists the
    loc/zh-CN slots too, with no source ("": the slot a language layer fills),
    so a row alone does not make the PC's (measured 2026-11-19: 275 such rows,
    none with a source, in each disc-built manifest read; 475, each with a
    source, in the PC installs'). common/cache_manifest.cpp's ManifestParse
    counts a row alone (zh_rows): it reads every older disc-built manifest as
    a PC install, a defect the exe-retire round's X3 hand-back names; this
    mirror answers as that reader should."""
    import tomllib
    man = os.path.join(cache, "manifest.toml") if cache else None
    if not man or not os.path.isfile(man):
        return None
    with open(man, "rb") as fh:
        m = tomllib.load(fh)
    kind = m.get("build", {}).get("install_kind")
    if kind in ("pc", "disc"):
        return kind
    rows = m.get("cache", {}).get("assets", [])
    return "pc" if any(isinstance(r, list) and len(r) > 3 and r[2] == "loc/zh-CN" and r[3] for r in rows) else "disc"


def original_available(game, cache=None):
    """DIV-0102 (docs/disc-only-install.md): whether `original`, the port's
    Chinese text, can be played with `cache` as the data cache: the cache's
    loc/zh-CN/dat/ holding a .DAT (built from the PC's DAT/), or the cache's
    kind the PC's (cache_kind: its DAT/ is the port's); with no manifest, True -
    with no cache only a PC folder starts. The launcher's rule at a player's
    start (config.cpp ConfigOriginalAvailable, cache_manifest.cpp
    InstallChineseText with the launcher's ask, no_cache true) and the DLL's
    (dat_load.cpp RefuseOriginalWithoutChinese). `game` is not asked: BOF3.exe
    beside the game says nothing of the install since the exe-retire round (X1);
    only the developer's BOF3X_EXEFILE=1 and the hosted build ask the file."""
    d = os.path.join(cache, "loc", "zh-CN", "dat") if cache else None
    if d and os.path.isdir(d) and any(f.upper().endswith(".DAT") for f in os.listdir(d)):
        return True
    return cache_kind(cache) != "disc"


# common/cache_manifest.cpp's answers on every install kind, as
# tools/launcher_host/config_tests.cpp TestInstallKind states them (its `kinds`
# table: the manifest, new and old, and whether a loc/zh-CN layer is on disk,
# to the Chinese text's answer at a player's start). `selftest` checks this
# mirror against them on manifests written as text: builds, layer names and
# made-up hashes only. The two rows of source-less loc/zh-CN slots are what
# cache_manifest.cpp should answer and today does not for the older manifest
# (cache_kind's docstring).
KIND_ROWS = (
    # what, install_kind written (None: an old manifest), source build, a loc/zh-CN asset row, the layer on disk,
    # cache_kind, original_available
    ("a PC folder used in place, no cache", "none", None, False, False, None, True),
    ("an install made from a PC source", "pc", "pc-zh", True, True, "pc", True),
    ("the same, old manifest", None, "pc-zh", True, True, "pc", True),
    ("the same, its loc/zh-CN layer gone", "pc", "pc-zh", True, False, "pc", True),
    ("the same, old manifest, layer gone", None, "pc-zh", True, False, "pc", True),
    ("a disc-built install", "disc", "psx-us", False, False, "disc", False),
    ("the same, old manifest", None, "psx-us", False, False, "disc", False),
    ("a PSP-only install", "disc", "psp-jp", False, False, "disc", False),
    ("the same, old manifest", None, "psp-jp", False, False, "disc", False),
    ("the recorded kind wins over the rows", "disc", "psx-us", True, False, "disc", False),
    # a disc-built cache's own loc/zh-CN rows: the slots, with no source (an older manifest has nothing else)
    ("a disc-built install, its source-less loc/zh-CN slot rows", "disc", "psx-us", "slot", False, "disc", False),
    ("the same, old manifest", None, "psx-us", "slot", False, "disc", False),
    ("a disc-built cache with a loc/zh-CN layer on disk", "disc", "psx-us", False, True, "disc", True),
)


def selftest():
    """The mirror against KIND_ROWS; a PC folder beside a disc-built cache is
    the disc-built row with a BOF3.exe in the game folder, which is not asked."""
    import tempfile
    bad = []
    with tempfile.TemporaryDirectory() as tmp:
        for i, (what, kind, build, zh_row, zh_dir, want_kind, want_zh) in enumerate(KIND_ROWS):
            for exe in (False, True):
                game, cache = os.path.join(tmp, "g%d%d" % (i, exe)), os.path.join(tmp, "c%d%d" % (i, exe))
                os.makedirs(os.path.join(game, "DAT"))
                open(os.path.join(game, "DAT", "FIRST.DAT"), "wb").close()
                if exe:
                    open(os.path.join(game, "BOF3.exe"), "wb").close()
                if kind != "none":
                    os.makedirs(cache)
                    text = '[meta]\ntarget = "pc-zh"\n\n[build]\nbase_policy = "psx"\n'
                    if kind:
                        text += 'install_kind = "%s"\n' % kind
                    text += '\n[[source]]\nbuild = "%s"\npath = "x"\n\n[cache]\nassets = [\n' % build
                    text += '  ["A.DAT", 0, "base", "%s:chunk", "h"],\n' % build
                    if zh_row == "slot":
                        text += '  ["A.DAT", 1, "loc/zh-CN", "", ""],\n'
                    elif zh_row:
                        text += '  ["A.DAT", 1, "loc/zh-CN", "pc-zh:chunk", "h"],\n'
                    text += "]\n"
                    with open(os.path.join(cache, "manifest.toml"), "w", encoding="utf-8", newline="\n") as fh:
                        fh.write(text)
                    if zh_dir:
                        os.makedirs(os.path.join(cache, "loc", "zh-CN", "dat"))
                        open(os.path.join(cache, "loc", "zh-CN", "dat", "A.DAT"), "wb").close()
                c = cache if kind != "none" else None
                got = (cache_kind(c), original_available(game, c))
                if got != (want_kind, want_zh):
                    bad.append("%s%s: %r, want %r" % (what, ", BOF3.exe beside it" if exe else "", got,
                                                       (want_kind, want_zh)))
    for b in bad:
        print("ERROR", b)
    print("language_tags selftest: %d install kinds, with and without a BOF3.exe beside the game, %d error(s)"
          % (len(KIND_ROWS), len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    import sys
    if sys.argv[1:] != ["selftest"]:
        sys.exit("usage: python tools/language_tags.py selftest - the install's kind and DIV-0102's test against "
                 "common/cache_manifest.cpp's answers (docs/exe-retire-round.md X3)")
    sys.exit(selftest())


def languages_held(game, cache=None):
    """The tags whose layer is installed in `game`'s DAT/ (<tag>.*.DAT) or
    built in `cache` (loc/<tag>/dat/), in TAGS' order - the launcher's
    ConfigLanguagesAvailable."""
    dat_dir = os.path.join(game, "DAT")
    names = os.listdir(dat_dir) if os.path.isdir(dat_dir) else []
    out = []
    for tag in TAGS:
        d = os.path.join(cache, "loc", tag, "dat") if cache else None
        if any(f.startswith(tag + ".") and f.upper().endswith(".DAT") for f in names) or \
                (d and os.path.isdir(d) and any(f.upper().endswith(".DAT") for f in os.listdir(d))):
            out.append(tag)
    return out
