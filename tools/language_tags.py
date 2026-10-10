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
