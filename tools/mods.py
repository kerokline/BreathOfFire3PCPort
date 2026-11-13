#!/usr/bin/env python
"""The player's mods: a .zip of data files, checked, unpacked under the home and built into its cache.

    python tools/mods.py add ZIP --home H [--json]     check, unpack to H/mods/<id>/, record in H/mods.toml
    python tools/mods.py add PATCH --home H [--json]   a romhack's patch file as its author released it (.xdelta)
    python tools/mods.py remove ID --home H            the folder, the row and the layer built from it
    python tools/mods.py list --home H [--json]        one flat JSON object a line, in the list's order
    python tools/mods.py build [ID] --home H           build each mod's layer into H/cache (in place)
    python tools/mods.py check ZIP [--json]            the checks of `add` alone; nothing written
    python tools/mods.py selftest [--keep DIR]         every refusal and the add/list/build/remove cycle,
                                                       on zips it makes in a temporary folder (no game data)

docs/mods.md is the contract. A mod is a zip holding `mod.toml` at its root and the
data files it names, **never code**: nothing in a zip is executed, imported or
evaluated, and a zip holding anything mod.toml does not name is refused whole.
The kinds are a closed set (KINDS); a kind the tools do not know is refused with
its name. Today there are two. `reading-row` (the reading-row round,
docs/reading-row-round.md section 2 ML): a row above each text row of the
dialogue box, built on the player's machine by tools/ruby_build.py into the
optional text layer `ruby-<id>-<base>` (docs/reading-row.md section 1) - a layer
the launcher's *Row above the text* box offers and adds to BOF3X_OPT.
`table-edit` (the image-layer round, docs/image-layer-round.md 4): numbers of the
game's tables, as text rows in `edits.toml`, built by tools/table_edit_build.py
into the layer `mod-<id>` - one file a table over the install's own table files
- which the launcher adds to BOF3X_OPT after the other layers, in this list's
order (the later mod wins a cell two mods both set).

The home's `mods.toml` is written only by this tool, in the shape of
`sources.toml` (docs/install-sources.md section 3): one `[[mod]]` table a mod, a
`key = value` line each, the values JSON-quoted. The launcher reads it for the
Settings page's box (src/launcher/config.cpp ConfigModsInstalled) and runs this
tool for the Mods tab (src/launcher/sources_dialog.cpp, the Mods page), as the
Sources page runs importer.py: `--json` lines, one child at a time.

A romhack's patch is added as the file its author released, with no zip of
ours around it (docs/mods.md 12): the release's list `data/mods/known.toml`
names the patches the project has measured, by the sha256 of the file. A patch
in the list is a disc-patch mod with the list's name, author and behaviours. A
patch that is not in the list and that the tools can read is added too, as
`unverified`: no author, no behaviour, only the data and text its build can
carry. A file the tools cannot read as a patch is refused.

`importer.py setup` (tools/player_sources.py run_setup) calls setup_step() after
the language layers are built and before the game folder is laid out: each mod's
layer is built into the new cache, or the mod is skipped with a line naming the
language layer it needs. A mod whose builder is not in the release stops the
setup with an error naming the builder (CLAUDE.md rule 4: no silent pass).
"""
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import zipfile

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import language_tags   # noqa: E402  (the engine's tags, src/game/language_tags.h)

MOD_TOML = "mod.toml"
MODS_FILE = "mods.toml"
MODS_DIR = "mods"

# The size bounds (docs/mods.md 2). A reading-row mod is a few hundred lines of
# TOML; the bounds leave room for every mod kind the round can foresee and stop
# a zip bomb before it is unpacked: each member is read at most its bound + 1.
ZIP_MAX = 8 << 20          # the .zip file itself
MEMBER_MAX = 4 << 20       # one member, unpacked
TOTAL_MAX = 8 << 20        # every member together, unpacked
MEMBERS_MAX = 16

# The DLL's longest layer name (src/game/dat_load.cpp kOptName - 1; the
# launcher's ConfigOptValid): a mod's layer must fit it.
LAYER_MAX = 23

ID_RE = re.compile(r"[a-z0-9]+(-[a-z0-9]+)*")
VERSION_RE = re.compile(r"[0-9]+(\.[0-9]+){0,3}([-+][0-9A-Za-z.]+)?")
FILE_RE = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,63}")
# Who made the mod (docs/mods.md 2; issue 216): shown on the Mods tab, so the
# author of a repackaged romhack is credited where the player chooses it. All
# three are text the launcher shows and never opens, runs or follows.
CREDIT_KEYS = ("author", "url", "credit")
URL_RE = re.compile(r"https?://[A-Za-z0-9._~:/?#@!$&'()*+,;=%\[\]-]{1,190}")
# Kinds of file a mod never holds, whatever mod.toml says: game data (rule 1;
# a text layer is derived game data, built on the player's machine) and programs.
GAME_DATA_EXT = (".dat", ".emi", ".bin", ".iso", ".cue", ".str", ".xa", ".avi", ".mp3", ".at3", ".vag", ".vab")
CODE_EXT = (".exe", ".dll", ".py", ".pyc", ".pyd", ".pyw", ".bat", ".cmd", ".ps1", ".psm1", ".vbs", ".js",
            ".jse", ".wsf", ".com", ".scr", ".msi", ".lnk", ".cpl", ".sh", ".jar", ".hta", ".reg")

# The kinds the tools know. Each: its table in mod.toml, that table's keys
# (`tag` a language tag, `file` a file in the zip), what its layer is called,
# and the builder its step runs. Anything else is refused with its name.
KINDS = {
    "reading-row": {
        "table": "reading-row",
        "keys": {"base": "tag", "readings": "file"},
        "file_ext": {"readings": ".toml"},
        # tools/ruby_build.py's furigana mode reads the ja-JP text alone (the
        # reading-row round, group FB); a reading row over another base needs
        # another builder mode first.
        "bases": ("ja-JP",),
        "builder": "ruby_build.py",
        "mode": "furigana",
    },
    "table-edit": {
        "table": "table-edit",
        "keys": {"edits": "file"},
        "file_ext": {"edits": ".toml"},
        "builder": "table_edit_build.py",
    },
    # A romhack as its author released it (docs/disc-patch-round.md): the
    # .xdelta of a disc's data track, untouched, and the two hashes that say
    # which file it is and what it must make. `builds`: the discs the builder
    # has been run on, each with the language its text is and the name the
    # Sources page gives it - a closed list, widened when another is measured.
    "disc-patch": {
        "table": "disc-patch",
        "keys": {"build": "build", "patch": "file", "patch_sha256": "sha256", "result_sha256": "sha256"},
        # skip = [{table = "<a tables.toml key>", records = [first, last]}]: records the build does not carry
        # (docs/mods.md 11; disc_patch_build.parse_skip)
        "optional": {"skip": "skip"},
        "file_ext": {"patch": (".xdelta", ".ppf")},
        "builds": {"psx-us": ("en-US", "PlayStation, USA")},
        # the other text the patch's lines are carried over to, where the install holds it and its disc
        # (docs/disc-patch-round.md 11; disc_patch_build.ALSO_TEXT, which the self-test holds this equal to)
        "also": {"psx-us": ("en-150",)},
        "author": True,
        "builder": "disc_patch_build.py",
    },
}

# The patches the project has measured (docs/mods.md 12): shipped beside the
# tools (build_reuse.SHIPPED_DATA). One [[patch]] a version of a romhack.
KNOWN_FILE = os.path.join(os.path.dirname(TOOLS), "data", "mods", "known.toml")
KNOWN_KEYS = ("id", "name", "version", "author", "url", "credit", "requires", "needs", "build", "format",
              "patch_sha256", "result_sha256", "skip")
# The patch formats the tools read: the name the list gives each, its file extension, its reader.
PATCH_FORMATS = {"xdelta": ".xdelta", "ppf": ".ppf"}
# A patch file added by itself: the bound of a mod's zip.
PATCH_MAX = 8 << 20
# What the player is told about a patch the list does not have (the owner's words, 2026-11-13).
UNVERIFIED = ("This patch is not in the list of known mods of this release (data/mods/known.toml). Its author and "
              "its function are not verified. The game takes only the data and text changes its build can carry.")

# What a disc-patch mod's patch may write: a CD's data track is under 800 MiB.
RESULT_MAX = 800 << 20
SHA_RE = re.compile(r"[0-9a-f]{64}")

# The engine's behaviours a mod may need (docs/mods.md 11): `needs` in [mod],
# for the kinds that are on or off as a whole. A name is a ledger code name
# (CamelCase, as BOF3X_OFF's), at most this many a mod.
NEEDS_KINDS = ("disc-patch", "table-edit")
NEED_RE = re.compile(r"[A-Z][A-Za-z0-9_]{0,47}")
NEEDS_MAX = 32
# The release's behaviours come from the header the game and the launcher are
# built with, src/hook/code_names.h (generated by tools/ledger_switches.py and
# shipped beside the tools: build_reuse.SHIPPED_DATA), so this list and theirs
# cannot differ.
CODE_NAMES_H = os.path.join(os.path.dirname(TOOLS), "src", "hook", "code_names.h")


def release_behaviours(path=None):
    """The behaviours this release has: the names in code_names.h's
    `namespace bof3::behaviours` kNames, in its order. A release without the
    header has none, and a mod that needs one is listed as missing it."""
    try:
        with open(path or CODE_NAMES_H, encoding="utf-8") as f:
            text = f.read()
    except OSError:
        return ()
    m = re.search(r"namespace bof3::behaviours \{(.*?)\n\}  // namespace bof3::behaviours", text, re.S)
    if not m:
        return ()
    k = re.search(r"kNames\[kCount \+ 1\] = \{(.*?)\};", m.group(1), re.S)
    return tuple(re.findall(r'"(\w+)"', k.group(1))) if k else ()


class Refused(Exception):
    """A zip that is not a mod the tools take; the message is one sentence for the player."""


class BuildError(Exception):
    """A mod's step could not run or failed; the message names the mod and why."""


def layer_name(meta):
    """The optional layer a mod's step builds (docs/reading-row.md 1.3)."""
    if meta["kind"] == "reading-row":
        return "ruby-%s-%s" % (meta["id"], meta["base"])
    if meta["kind"] in ("table-edit", "disc-patch"):
        return "mod-%s" % meta["id"]
    raise BuildError("no layer is defined for kind %r" % meta["kind"])


def layers_of(meta):
    """Every optional layer a mod's step may build. A disc-patch mod has two
    (docs/disc-patch-round.md 3a): `mod-<id>`, what no language owns, and
    `mod-<id>-<tag>`, its edits to the text of its disc's language - and after
    them one `mod-<id>-<tag>` for each other text its lines are carried over
    to (docs/disc-patch-round.md 11), where that name fits a layer's."""
    if meta["kind"] == "disc-patch":
        tag = KINDS["disc-patch"]["builds"][meta["build"]][0]
        return [layer_name(meta), "%s-%s" % (layer_name(meta), tag)] + \
            ["%s-%s" % (layer_name(meta), t) for t in also_tags(meta)]
    return [layer_name(meta)]


def also_tags(meta):
    """The other languages a disc-patch mod's text may be built for: KINDS'
    `also` of its disc, less a tag whose layer name is too long for a layer."""
    k = KINDS["disc-patch"]
    return [t for t in k["also"].get(meta.get("build"), ()) if len("%s-%s" % (layer_name(meta), t)) <= LAYER_MAX]


def text_languages(h, r, cache=None):
    """The languages a disc-patch mod's text is built for in this install: its
    disc's own, then each of also_tags() whose layer is in the cache. What the
    launcher plays the mod over (config.cpp ConfigModLayers)."""
    cache = cache or os.path.join(h, "cache")
    own = KINDS["disc-patch"]["builds"][r["build"]][0]
    return [own] + [t for t in also_tags(r) if built(cache, "%s-%s" % (layer_name(r), t))]


# ---------------------------------------------------------------- the zip

def _member_problem(info):
    """Why one member cannot be in a mod, or None."""
    n = info.filename
    if not n or "\x00" in n:
        return "an empty or unreadable member name"
    if n.startswith(("/", "\\")) or re.match(r"[A-Za-z]:", n) or "\\" in n \
            or any(part in ("..", ".") for part in n.split("/")):
        return "%s: a path that leaves the mod's folder" % n
    if (info.external_attr >> 16) and stat.S_ISLNK(info.external_attr >> 16):
        return "%s: a link, not a file" % n
    if n.endswith("/") or info.is_dir():
        return "%s: a folder - a mod's files sit at the zip's root, beside mod.toml" % n
    if "/" in n:
        return "%s: a file in a folder - a mod's files sit at the zip's root, beside mod.toml" % n
    low = n.lower()
    if low.endswith(GAME_DATA_EXT):
        return ("%s: game data (a .DAT or other game file) - a mod carries only its own data; the game's text "
                "and pictures are built from the player's discs, never shipped (CLAUDE.md rule 1)" % n)
    if low.endswith(CODE_EXT):
        return "%s: a program or script - a mod holds data files only, never code" % n
    if info.flag_bits & 0x1:
        return "%s: encrypted" % n
    if info.file_size > MEMBER_MAX:
        return "%s: %d bytes unpacked, over the %d a member may hold" % (n, info.file_size, MEMBER_MAX)
    return None


def _read_member(z, info):
    import zlib
    try:
        with z.open(info) as f:
            data = f.read(MEMBER_MAX + 1)
    except (zipfile.BadZipFile, zlib.error, EOFError, OSError, NotImplementedError) as e:
        raise Refused("%s: damaged or not readable in the zip (%s)" % (info.filename, e))
    if len(data) > MEMBER_MAX:
        raise Refused("%s: over %d bytes unpacked, whatever its header says" % (info.filename, MEMBER_MAX))
    return data


def _toml(data, what):
    import tomllib
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        raise Refused("%s is not UTF-8 text" % what)
    try:
        return tomllib.loads(text)
    except tomllib.TOMLDecodeError as e:
        raise Refused("%s is not valid TOML (%s)" % (what, e))


def _string(t, key, where, maxlen):
    v = t.get(key)
    if not isinstance(v, str) or not v.strip():
        raise Refused("mod.toml: %s.%s must be a non-empty string" % (where, key))
    if len(v) > maxlen or any(ord(c) < 32 for c in v):
        raise Refused("mod.toml: %s.%s is longer than %d characters or holds a control character"
                      % (where, key, maxlen))
    return v


def _tag(v, where):
    if not isinstance(v, str):
        raise Refused("mod.toml: %s must be a language tag (%s)" % (where, ", ".join(language_tags.TAGS)))
    old = language_tags.retired(v, where)
    if old:
        raise Refused("mod.toml: " + old)
    if v not in language_tags.TAGS:
        raise Refused("mod.toml: %s is %r, not a language tag the game knows (%s)"
                      % (where, v, ", ".join(language_tags.TAGS)))
    return v


def check_meta(doc):
    """mod.toml's tables, checked against KINDS: the flat meta dict, or Refused."""
    kinds = ", ".join(sorted(KINDS))
    mod = doc.get("mod")
    if not isinstance(mod, dict):
        raise Refused("mod.toml has no [mod] table")
    kind = mod.get("kind")
    if not isinstance(kind, str) or kind not in KINDS:
        raise Refused("mod.toml: kind %s is not one the tools know (they know: %s)"
                      % (json.dumps(kind) if kind is not None else "(missing)", kinds))
    k = KINDS[kind]
    extra = sorted(set(doc) - {"mod", k["table"]})
    if extra:
        raise Refused("mod.toml: table %s is not one a %s mod has ([mod], [%s])" % (extra[0], kind, k["table"]))
    known = {"id", "name", "version", "kind", "requires"} | set(CREDIT_KEYS) | ({"needs"} if kind in NEEDS_KINDS else set())
    extra = sorted(set(mod) - known)
    if extra:
        if extra[0] == "needs":
            raise Refused("mod.toml: a %s mod cannot need behaviours (mod.needs is for %s mods)"
                          % (kind, " and ".join(NEEDS_KINDS)))
        raise Refused("mod.toml: [mod] has %s, which the tools do not know (id, name, version, kind, requires, "
                      "author, url, credit%s)" % (extra[0], ", needs" if kind in NEEDS_KINDS else ""))
    meta = {"kind": kind}
    meta["id"] = _string(mod, "id", "mod", LAYER_MAX)
    if not ID_RE.fullmatch(meta["id"]):
        raise Refused("mod.toml: mod.id %r must be lowercase letters, digits and single hyphens (it is the "
                      "mod's folder name)" % meta["id"])
    meta["name"] = _string(mod, "name", "mod", 64)
    meta["version"] = _string(mod, "version", "mod", 32)
    if not VERSION_RE.fullmatch(meta["version"]):
        raise Refused("mod.toml: mod.version %r is not a version (1.2.3, optionally -beta or +build)"
                      % meta["version"])
    if "author" in mod:
        meta["author"] = _string(mod, "author", "mod", 64)
    if "credit" in mod:
        meta["credit"] = _string(mod, "credit", "mod", 200)
    if "url" in mod:
        meta["url"] = _string(mod, "url", "mod", 200)
        if not URL_RE.fullmatch(meta["url"]):
            raise Refused("mod.toml: mod.url %r must be an http:// or https:// address with no spaces" % meta["url"])
    req = mod.get("requires")
    if not isinstance(req, list):
        raise Refused("mod.toml: mod.requires must be a list of language tags, e.g. [\"ja-JP\"]")
    meta["requires"] = [_tag(t, "mod.requires") for t in req]
    if len(set(meta["requires"])) != len(meta["requires"]):
        raise Refused("mod.toml: mod.requires names a tag twice")
    if "needs" in mod:
        # the engine's behaviours the mod turns on (docs/mods.md 11): names, checked for their form here;
        # whether this release has them is the list's `missing`, not a refusal - a newer release may
        needs = mod["needs"]
        if not isinstance(needs, list) or not all(isinstance(n, str) for n in needs):
            raise Refused("mod.toml: mod.needs must be a list of behaviour names, e.g. [\"SmokeSkill\"]")
        if len(needs) > NEEDS_MAX:
            raise Refused("mod.toml: mod.needs names %d behaviours, over the %d a mod may need" % (len(needs), NEEDS_MAX))
        for n in needs:
            if not NEED_RE.fullmatch(n):
                raise Refused("mod.toml: mod.needs names %s, which is not a behaviour name (a capital letter, then "
                              "letters, digits or _, 48 at most)" % json.dumps(n))
        if len(set(needs)) != len(needs):
            raise Refused("mod.toml: mod.needs names a behaviour twice")
        meta["needs"] = list(needs)
    t = doc.get(k["table"])
    if not isinstance(t, dict):
        raise Refused("mod.toml: a %s mod needs its [%s] table" % (kind, k["table"]))
    extra = sorted(set(t) - set(k["keys"]) - set(k.get("optional", ())))
    if extra:
        raise Refused("mod.toml: [%s] has %s, which the tools do not know (%s)"
                      % (k["table"], extra[0], ", ".join(list(k["keys"]) + list(k.get("optional", ())))))
    if "skip" in t:
        # what of a table the build must not carry, which its rules cannot see (docs/mods.md 11)
        import disc_patch_build
        try:
            meta["skip"] = disc_patch_build.parse_skip(t["skip"])
        except disc_patch_build.Bad as e:
            raise Refused("mod.toml: %s.%s" % (k["table"], e))
    files = []
    for key, how in k["keys"].items():
        where = "%s.%s" % (k["table"], key)
        if how == "tag":
            meta[key] = _tag(t.get(key), where)
        elif how == "build":
            v = t.get(key)
            if not isinstance(v, str) or v not in k["builds"]:
                raise Refused("mod.toml: %s is %s, not a disc a %s mod can be made for (%s)"
                              % (where, json.dumps(v) if v is not None else "(missing)", kind, ", ".join(k["builds"])))
            meta[key] = v
        elif how == "sha256":
            v = t.get(key)
            if not isinstance(v, str) or not SHA_RE.fullmatch(v):
                raise Refused("mod.toml: %s must be a sha256, 64 lowercase hex digits" % where)
            meta[key] = v
        else:
            v = _string(t, key, k["table"], 64)
            if not FILE_RE.fullmatch(v) or v == MOD_TOML:
                raise Refused("mod.toml: %s %r must be a plain file name at the zip's root" % (where, v))
            want = k.get("file_ext", {}).get(key)
            if want and not v.lower().endswith(want):
                raise Refused("mod.toml: %s %r must be a %s file"
                              % (where, v, want if isinstance(want, str) else " or ".join(want)))
            meta[key] = v
            files.append(v)
    if kind == "reading-row":
        if meta["base"] not in meta["requires"]:
            raise Refused("mod.toml: reading-row.base %s must be among mod.requires" % meta["base"])
        if meta["base"] not in k["bases"]:
            raise Refused("mod.toml: reading-row.base %s - the reading-row builder reads %s text only"
                          % (meta["base"], " and ".join(k["bases"])))
    if k.get("author") and "author" not in meta:
        raise Refused("mod.toml: a %s mod must name its author (mod.author): it carries another person's work"
                      % kind)
    if kind == "disc-patch":
        tag = k["builds"][meta["build"]][0]
        if tag not in meta["requires"]:
            raise Refused("mod.toml: a patch of the %s disc changes %s text, so mod.requires must name %s"
                          % (meta["build"], tag, tag))
    meta["files"] = files
    meta["layer"] = layer_name(meta)
    longest = max(layers_of(meta)[:2], key=len)     # its own layers; a carried-over text's is left out when too long
    if len(longest) > LAYER_MAX:
        raise Refused("mod.toml: the layer %s is longer than the %d characters a layer name may have; "
                      "a shorter mod.id fits" % (longest, LAYER_MAX))
    return meta


def read_zip(path):
    """(meta, {file name: bytes}) for a mod zip, or Refused. Reads; writes nothing."""
    if not os.path.isfile(path):
        raise Refused("there is no file at %s" % path)
    if not path.lower().endswith(".zip"):
        raise Refused("a mod is a .zip file")
    size = os.path.getsize(path)
    if size > ZIP_MAX:
        raise Refused("the zip is %d bytes, over the %d a mod may be" % (size, ZIP_MAX))
    try:
        z = zipfile.ZipFile(path)
    except (zipfile.BadZipFile, OSError) as e:
        raise Refused("not a readable .zip (%s)" % e)
    with z:
        infos = z.infolist()
        if len(infos) > MEMBERS_MAX:
            raise Refused("the zip holds %d members, over the %d a mod may have" % (len(infos), MEMBERS_MAX))
        seen = set()
        for info in infos:
            why = _member_problem(info)
            if why:
                raise Refused(why)
            key = info.filename.lower()
            if key in seen:
                raise Refused("%s: in the zip twice" % info.filename)
            seen.add(key)
        if sum(i.file_size for i in infos) > TOTAL_MAX:
            raise Refused("the zip unpacks to more than the %d bytes a mod may hold" % TOTAL_MAX)
        names = {i.filename: i for i in infos}
        if MOD_TOML not in names:
            raise Refused("no mod.toml at the zip's root: not a mod")
        meta = check_meta(_toml(_read_member(z, names[MOD_TOML]), MOD_TOML))
        named = [MOD_TOML] + meta["files"]
        for n in meta["files"]:
            if n not in names:
                raise Refused("mod.toml names %s, which the zip does not hold" % n)
        for n in names:
            if n not in named:
                raise Refused("the zip holds %s, which mod.toml does not name" % n)
        data = {n: _read_member(z, names[n]) for n in named}
    if sum(len(b) for b in data.values()) > TOTAL_MAX:
        raise Refused("the zip unpacks to more than the %d bytes a mod may hold" % TOTAL_MAX)
    for n in meta["files"]:
        if n.lower().endswith(".toml"):
            _toml(data[n], n)
    if meta["kind"] == "table-edit":
        # every row against the catalogue now, so that a mod that cannot be built
        # is refused when it is added and not at the next setup
        import table_edit_build
        try:
            table_edit_build.parse(_toml(data[meta["edits"]], meta["edits"]))
        except table_edit_build.Bad as e:
            raise Refused("%s: %s" % (meta["edits"], e))
    if meta["kind"] == "disc-patch":
        # the author's file, byte for byte, and a patch this release can read:
        # refused now, not after the player's disc has been copied
        patch = data[meta["patch"]]
        if sha_bytes(patch) != meta["patch_sha256"]:
            raise Refused("%s is not the file mod.toml's patch_sha256 names" % meta["patch"])
        try:
            fmt, total = patch_format(patch)
        except PatchFormat as e:
            raise Refused("%s: %s" % (meta["patch"], e))
        if not meta["patch"].lower().endswith(PATCH_FORMATS[fmt]):
            raise Refused("%s is a %s patch: mod.toml must name it as a %s file" % (meta["patch"], fmt, PATCH_FORMATS[fmt]))
        if total > RESULT_MAX:
            raise Refused("%s writes %d bytes, over the %d a disc's data track may be" % (meta["patch"], total, RESULT_MAX))
    return meta, data


class PatchFormat(Exception):
    """A file that is not a patch in a format the tools read; the message says what it is not."""


def patch_format(patch):
    """(format, the bytes it writes up to) for a patch's bytes: "xdelta"
    (VCDIFF, tools/vcdiff.py) or "ppf" (PPF 3.0, tools/ppf.py), by the file's
    first bytes and not by its name. PatchFormat for anything else."""
    import ppf
    import vcdiff
    if ppf.is_ppf(patch):
        try:
            ppf.header(patch)
            return "ppf", ppf.target_len(patch)
        except ppf.PatchError as e:
            raise PatchFormat(str(e))
    try:
        vcdiff.header(patch)
        return "xdelta", sum(w["target_len"] for w in vcdiff.windows(patch))
    except vcdiff.PatchError as e:
        raise PatchFormat(str(e))


def sha_bytes(b):
    return hashlib.sha256(b).hexdigest()


# ---------------------------------------------------------------- a patch by itself

def load_known(path=None):
    """The release's known patches: {patch sha256: meta}, each meta as
    check_meta makes it for a disc-patch mod, with `patch` the file name the
    home keeps it under. Refused when the list is there and is not valid: a
    release with a broken list says so and adds nothing by it. A release
    without the file knows no patch."""
    path = path or KNOWN_FILE
    if not os.path.isfile(path):
        return {}
    with open(path, "rb") as f:
        doc = _toml(f.read(), os.path.basename(path))
    rows = doc.get("patch", [])
    if set(doc) - {"patch"} or not isinstance(rows, list):
        raise Refused("%s: only [[patch]] tables belong in it" % os.path.basename(path))
    out = {}
    for i, row in enumerate(rows, 1):
        where = "%s, patch %d" % (os.path.basename(path), i)
        if not isinstance(row, dict):
            raise Refused("%s: not a table" % where)
        extra = sorted(set(row) - set(KNOWN_KEYS))
        if extra:
            raise Refused("%s: %s is not a key of the list (%s)" % (where, extra[0], ", ".join(KNOWN_KEYS)))
        mod = {k: row[k] for k in ("id", "name", "version", "author", "url", "credit", "requires", "needs") if k in row}
        mod["kind"] = "disc-patch"
        tab = {k: row[k] for k in ("build", "patch_sha256", "result_sha256", "skip") if k in row}
        fmt = row.get("format", "xdelta")
        if fmt not in PATCH_FORMATS:
            raise Refused("%s: format %s is not one the tools read (%s)" % (where, json.dumps(fmt), ", ".join(PATCH_FORMATS)))
        tab["patch"] = "%s-%s%s" % (row.get("id"), row.get("version"), PATCH_FORMATS[fmt])
        try:
            meta = check_meta({"mod": mod, "disc-patch": tab})
        except Refused as e:
            raise Refused("%s: %s" % (where, str(e).replace("mod.toml: ", "")))
        if meta["patch_sha256"] in out:
            raise Refused("%s: the list has that patch_sha256 twice" % where)
        out[meta["patch_sha256"]] = meta
    return out


def known_toml(meta):
    """The mod.toml the home keeps beside a patch added by itself: the list's
    entry in a mod zip's form (a patch the list has), so that the folder is a
    mod's folder like any other."""
    q = lambda v: json.dumps(v, ensure_ascii=False)   # noqa: E731
    lines = ["# Written by tools/mods.py from the release's list of known patches (data/mods/known.toml).", "", "[mod]"]
    lines += ["%s = %s" % (k, q(meta[k])) for k in ("id", "name", "version", "author", "url", "credit") if k in meta]
    lines += ['kind = "disc-patch"', "requires = %s" % q(meta["requires"])]
    if "needs" in meta:
        lines.append("needs = %s" % q(meta["needs"]))
    lines += ["", "[disc-patch]"] + ["%s = %s" % (k, q(meta[k])) for k in ("build", "patch", "patch_sha256", "result_sha256")]
    if "skip" in meta:
        lines.append("skip = [%s]" % ", ".join("{table = %s, records = [%d, %d]}" % (q(t), a, b) for t, a, b in meta["skip"]))
    return ("\n".join(lines) + "\n").encode("utf-8")


def read_patch(path, known=None):
    """(meta, {file name: bytes}) for a patch file added by itself, or Refused.
    A patch of the release's list: the list's meta. Another patch the tools
    can read: a disc-patch mod of the one disc the builder knows, marked
    `unverified`, with no author, no behaviour and no result to check against;
    its id is made from its sha256 and its name from the file's. Reads; writes
    nothing."""
    if not os.path.isfile(path):
        raise Refused("there is no file at %s" % path)
    if path.lower().endswith((".7z", ".rar", ".gz", ".xz")):
        raise Refused("%s is an archive the tools do not open: unpack it, then add the patch file that is in it "
                      "(.xdelta or .ppf)" % os.path.basename(path))
    size = os.path.getsize(path)
    if size > PATCH_MAX:
        raise Refused("the file is %d bytes, over the %d a patch may be" % (size, PATCH_MAX))
    with open(path, "rb") as f:
        patch = f.read(PATCH_MAX + 1)
    sha = sha_bytes(patch)
    try:
        fmt, total = patch_format(patch)
    except PatchFormat as e:
        raise Refused("%s cannot be applied: it is not a .zip mod and not a patch in a format the tools read "
                      "(.xdelta or .ppf; unknown format: %s)" % (os.path.basename(path), e))
    if total > RESULT_MAX:
        raise Refused("%s writes %d bytes, over the %d a disc's data track may be" % (os.path.basename(path), total, RESULT_MAX))
    meta = (load_known() if known is None else known).get(sha)
    if meta is not None:
        meta = dict(meta)
        return meta, {MOD_TOML: known_toml(meta), meta["patch"]: patch}
    k = KINDS["disc-patch"]
    build = next(iter(k["builds"]))
    stem = os.path.splitext(os.path.basename(path))[0]
    name = "".join(c for c in stem if ord(c) >= 32).strip()[:64] or "patch"
    meta = {"kind": "disc-patch", "id": "patch-" + sha[:6], "name": name, "version": "0", "requires": [k["builds"][build][0]],
            "build": build, "patch": "patch-%s%s" % (sha[:6], PATCH_FORMATS[fmt]), "patch_sha256": sha,
            "unverified": True}
    meta["files"] = [meta["patch"]]
    meta["layer"] = layer_name(meta)
    note = ("# Written by tools/mods.py for a patch that is not in the release's list of known patches.\n"
            "# Its author and its function are not verified.\n"
            "patch = %s\npatch_sha256 = %s\nbuild = %s\n" % (json.dumps(meta["patch"]), json.dumps(sha), json.dumps(build)))
    return meta, {MOD_TOML: note.encode("utf-8"), meta["patch"]: patch}


def read_any(path, known=None):
    """A mod zip, or a patch file by itself (anything that is not a .zip). A
    .zip with no mod.toml is an author's archive of his patch files, not a
    mod: the sentence says to unpack it."""
    if path.lower().endswith(".zip"):
        try:
            return read_zip(path)
        except Refused as e:
            if "no mod.toml" in str(e):
                raise Refused("%s has no mod.toml, so it is not a mod's zip. When it is the archive of a romhack, unpack "
                              "it, then add the patch file that is in it (.xdelta or .ppf)" % os.path.basename(path))
            raise
    return read_patch(path, known)


def sha_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


# ---------------------------------------------------------------- the home's list

def mods_file(h):
    return os.path.join(h, MODS_FILE)


def mod_dir(h, mid):
    return os.path.join(h, MODS_DIR, mid)


ROW_KEYS = ("id", "name", "version", "author", "url", "credit", "kind", "requires", "base", "readings", "edits",
            "needs", "build", "patch", "patch_sha256", "result_sha256", "skip", "unverified", "files", "zip", "sha256",
            "added")


def load_rows(h):
    import tomllib
    p = mods_file(h)
    if not os.path.isfile(p):
        return []
    with open(p, "rb") as f:
        return tomllib.load(f).get("mod", [])


def save_rows(h, rows):
    os.makedirs(h, exist_ok=True)
    lines = ["# The player's mods (docs/mods.md). Written by tools/mods.py (the launcher's Mods tab);",
             "# change it with that tool. Each mod's files are in mods/<id>/.", ""]
    for r in rows:
        lines += ["[[mod]]"] + ["%s = %s" % (k, json.dumps(r[k], ensure_ascii=False)) for k in ROW_KEYS if k in r] + [""]
    p = mods_file(h)
    with open(p + ".tmp", "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    os.replace(p + ".tmp", p)


def _has_dat(d):
    return os.path.isdir(d) and any(n.upper().endswith(".DAT") for n in os.listdir(d))


def held(h, tag, cache=None):
    """Whether the install holds language layer `tag`: the cache's loc/<tag>/dat/ or
    the game folder's DAT\\<tag>.*.DAT (the launcher's ConfigLanguagesAvailable)."""
    cache = cache or os.path.join(h, "cache")
    if _has_dat(os.path.join(cache, "loc", tag, "dat")):
        return True
    g = os.path.join(h, "game", "DAT")
    return os.path.isdir(g) and any(n.startswith(tag + ".") and n.upper().endswith(".DAT") for n in os.listdir(g))


def manifest_lists(cache, layer):
    """The cache's manifest lists opt/<layer> (common/cache_manifest.cpp
    ManifestOffersLayer: a cache with no manifest offers every layer)."""
    p = os.path.join(cache, "manifest.toml")
    if not os.path.isfile(p):
        return True
    with open(p, encoding="utf-8") as f:
        return '"opt/%s"' % layer in f.read()


def built(cache, layer):
    """The layer is in the cache and listed: its DAT containers, or its image
    part - exe/tables.toml (a table layer's), exe/place.toml (raw ranges)."""
    there = _has_dat(os.path.join(cache, "opt", layer, "dat")) or \
        os.path.isfile(os.path.join(cache, "opt", layer, "exe", "tables.toml")) or \
        os.path.isfile(os.path.join(cache, "opt", layer, "exe", "place.toml"))
    return there and manifest_lists(cache, layer)


def holds_build(h, build):
    """Whether the player's source list, H/sources.toml, has a disc of `build`
    (docs/install-sources.md): a disc-patch mod's step reads that disc."""
    import tomllib
    p = os.path.join(h, "sources.toml")
    if not os.path.isfile(p):
        return False
    try:
        with open(p, "rb") as f:
            rows = tomllib.load(f).get("source", [])
    except (OSError, tomllib.TOMLDecodeError):
        return False
    return any(isinstance(x, dict) and x.get("build") == build for x in rows)


def state_of(h, r, cache=None):
    """(state, needs, problem): built, not-built, needs (a required language layer
    not held: `needs` names them), needs-disc (a disc-patch mod whose disc is not
    in the source list: `needs` is the disc's name, `problem` the sentence) or
    problem (the mod's files are not there)."""
    cache = cache or os.path.join(h, "cache")
    d = mod_dir(h, r["id"])
    missing = [n for n in [MOD_TOML] + list(r.get("files", [])) if not os.path.isfile(os.path.join(d, n))]
    if r.get("kind") not in KINDS:
        return "problem", None, "kind %r is not one this release knows; remove the mod" % r.get("kind")
    if missing:
        return "problem", None, "its files are gone from %s (%s); add the zip again" % (d, ", ".join(missing))
    lack = [t for t in r.get("requires", []) if not held(h, t, cache)]
    if lack:
        return "needs", ", ".join(lack), None
    if r["kind"] == "disc-patch":
        k = KINDS["disc-patch"]
        if r.get("build") not in k["builds"]:
            return "problem", None, "it is for the disc %r, which this release cannot build from; remove the mod" % r.get("build")
        if not holds_build(h, r["build"]):
            disc = k["builds"][r["build"]][1]
            return "needs-disc", disc, "it needs the disc %s: add it on Data, Sources" % disc
    if any(built(cache, layer) for layer in layers_of(r)):
        return "built", None, None
    return "not-built", None, None


def _languages(h, r):
    if r.get("kind") != "disc-patch" or r.get("build") not in KINDS["disc-patch"]["builds"]:
        return None
    return text_languages(h, r)


def obj(r, path=None, ok=True, problem=None, state=None, needs=None, **more):
    o = {"path": path, "ok": ok, "id": r.get("id"), "name": r.get("name"), "version": r.get("version"),
         "kind": r.get("kind"), "requires": r.get("requires"), "base": r.get("base"),
         "layer": layer_name(r) if r.get("kind") in KINDS and r.get("id") else None,
         "text_layer": layers_of(r)[1] if r.get("kind") == "disc-patch" and r.get("id")
         and r.get("build") in KINDS["disc-patch"]["builds"] else None,
         "state": state, "needs": needs, "problem": problem,
         "author": r.get("author"), "url": r.get("url"), "credit": r.get("credit")}
    # the engine's behaviours the mod turns on (mod.toml's needs; docs/mods.md 11), and those this release
    # does not have: the launcher greys such a mod and never hands it over
    known = more.pop("known", None)
    languages = more.pop("languages", None)
    o["behaviours"] = list(r.get("needs") or [])
    # a patch added by itself that the release's list does not have (docs/mods.md 12): the Mods tab says so.
    # `known_now`: the list of this release has it after all (a newer release) - added again, it is that mod
    o["unverified"] = bool(r.get("unverified"))
    listed = more.pop("listed", None)
    if o["unverified"]:
        try:
            hit = (load_known() if listed is None else listed).get(r.get("patch_sha256"))
        except Refused:
            hit = None
        o["known_now"] = "%s %s" % (hit["name"], hit["version"]) if hit else None
    have = set(release_behaviours() if known is None else known)
    o["missing"] = [n for n in o["behaviours"] if n not in have]
    if o["text_layer"]:
        # the language of its text, and what its build has and does not have (the Mods tab's log)
        o["language"] = KINDS["disc-patch"]["builds"][r["build"]][0]
        # every language its text is built for here (text_languages); the caller that knows the home gives it
        o["languages"] = languages or [o["language"]]
        if state == "built" and path:
            try:
                import disc_patch_build
                o["carries"], o["lacks"] = disc_patch_build.read_report(path)
            except ImportError:      # a release without the builder: the mod is listed, its report is not read
                pass
    o.update(more)
    return o


def _emit(o, as_json):
    if as_json:
        print(json.dumps(o, ensure_ascii=False), flush=True)
    elif o["ok"]:
        extra = {"built": "built", "not-built": "not built yet", "needs": "needs %s" % o.get("needs"),
                 "needs-disc": "needs the disc %s" % o.get("needs")}.get(o["state"], "")
        by = ", by %s" % o["author"] if o.get("author") else ""
        if o.get("unverified"):
            extra += ("; " if extra else "") + "not in the list of known mods: its author and its function are not verified"
        if o.get("missing"):
            extra += ("; " if extra else "") + "needs behaviours this release does not have: %s" % ", ".join(o["missing"])
        print("%s %s (%s%s, %s)%s" % (o["id"], o["version"], o["name"], by, o["kind"], ": " + extra if extra else ""),
              flush=True)
    else:
        print("%s: %s" % (o.get("path") or o.get("id"), o["problem"]), flush=True)


# ---------------------------------------------------------------- the commands

def _known(a):
    """The list `--known` names (the self-test's), or None for the release's."""
    return load_known(a.known) if getattr(a, "known", None) else None


def cmd_check(a):
    try:
        meta, _ = read_any(a.zip, _known(a))
    except Refused as e:
        _emit(obj({}, a.zip, False, str(e)), a.json)
        return 1
    _emit(obj(meta, a.zip, listed=_known(a)), a.json)
    return 0


def cmd_add(a):
    h = os.path.abspath(a.home)
    path = os.path.abspath(a.zip)
    try:
        meta, data = read_any(path, _known(a))
    except Refused as e:
        _emit(obj({}, path, False, str(e)), a.json)
        return 1
    rows = load_rows(h)
    old = next((r for r in rows if r["id"] == meta["id"]), None)
    root = os.path.join(h, MODS_DIR)
    os.makedirs(root, exist_ok=True)
    new = os.path.join(root, meta["id"] + ".new")
    shutil.rmtree(new, ignore_errors=True)
    os.makedirs(new)
    for n, b in data.items():
        with open(os.path.join(new, n), "wb") as f:
            f.write(b)
    dst = mod_dir(h, meta["id"])
    if os.path.exists(dst):
        shutil.rmtree(dst)
    os.rename(new, dst)
    row = {k: meta[k] for k in ("id", "name", "version", "kind", "requires")}
    row.update({k: meta[k] for k in CREDIT_KEYS if k in meta})
    row.update({k: meta[k] for k in KINDS[meta["kind"]]["keys"] if k in meta})
    row.update({k: meta[k] for k in ("needs", "skip", "unverified") if k in meta})
    row.update(files=meta["files"], zip=path, sha256=sha_file(path),
               added=datetime.datetime.now().isoformat(timespec="seconds"))
    rows = [row if r is old else r for r in rows] if old else rows + [row]
    save_rows(h, rows)
    st, needs, problem = state_of(h, row)
    _emit(obj(row, path, True, problem, st, needs, replaced=old["version"] if old else None, listed=_known(a),
              languages=_languages(h, row)), a.json)
    return 0


def remove_layer(cache, layer):
    """The layer's folder and its manifest rows out of `cache` (a removed mod's)."""
    shutil.rmtree(os.path.join(cache, "opt", layer), ignore_errors=True)
    _manifest_rows(cache, layer, [])


def cmd_remove(a):
    h = os.path.abspath(a.home)
    rows = load_rows(h)
    keep = [r for r in rows if r["id"] != a.id]
    if len(keep) == len(rows):
        print("not installed: %s" % a.id, flush=True)
        return 1
    gone = next(r for r in rows if r["id"] == a.id)
    shutil.rmtree(mod_dir(h, a.id), ignore_errors=True)
    cache = os.path.join(h, "cache")
    if gone.get("kind") in KINDS and os.path.isdir(cache):
        try:
            layers = layers_of(gone)
        except KeyError:            # a row for a disc this release does not know
            layers = [layer_name(gone)]
        for layer in layers:
            remove_layer(cache, layer)
    save_rows(h, keep)
    print("removed: %s" % a.id, flush=True)
    return 0


def cmd_list(a):
    h = os.path.abspath(a.home)
    rows = load_rows(h)
    for r in rows:
        st, needs, problem = state_of(h, r)
        _emit(obj(r, mod_dir(h, r["id"]), st != "problem", problem, st, needs, listed=_known(a),
                  languages=_languages(h, r)), a.json)
    if not rows and not a.json:
        print("no mods listed in %s" % mods_file(h))
    return 0


# ---------------------------------------------------------------- the build step

def _chunks(blob, name):
    """(kind, tag, body) per chunk of a DAT container, the container's own header
    walk (docs/DAT_CONTAINER.md); any kind, so a layer's kind-18 side chunks too."""
    out, pos = [], 0
    while pos < len(blob):
        if pos + 16 > len(blob):
            raise BuildError("%s: a truncated chunk header at 0x%X" % (name, pos))
        kind, tag, size, word3 = struct.unpack_from("<4I", blob, pos)
        # word 3 is zero in every shipped chunk; a kind-18 side chunk carries
        # the pool half (0 or 1) there (docs/reading-row.md 1.1, tools/dat.py).
        ok3 = word3 == 0 or (kind == 18 and word3 == 1)
        if not ok3 or pos + 16 + size > len(blob):
            raise BuildError("%s: a bad chunk header at 0x%X" % (name, pos))
        out.append((kind, tag, blob[pos + 16:pos + 16 + size]))
        pos += 16 + size
    return out


def _manifest_rows(cache, layer, rows):
    """The cache's manifest with every [cache] opt row of opt/<layer> replaced by
    `rows` (the launcher and the DLL offer a cache's layer only when its manifest
    lists it, common/cache_manifest.cpp). The rest of the file is kept byte for byte."""
    p = os.path.join(cache, "manifest.toml")
    if not os.path.isfile(p):
        return
    with open(p, encoding="utf-8") as f:
        lines = f.read().split("\n")
    mine = '"opt/%s"' % layer
    lines = [l for l in lines if not (l.startswith("  [") and mine in l)]
    toml = lambda s: json.dumps(s, ensure_ascii=False)
    new = ["  [%s, %s, %d, %s, %s]," % (toml(n), toml("opt/" + layer), i, toml(w), toml(hh)) for n, i, w, hh in rows]
    table, start, end = None, None, None
    for i, l in enumerate(lines):
        s = l.strip()
        if not l.startswith(" ") and s.startswith("[") and s.endswith("]") and "=" not in s and '"' not in s:
            if table == "cache" and end is None:
                end = i
            table = s.strip("[]").strip()
        if table == "cache" and s.startswith("opt = ["):
            start = i
    if new:
        if start is not None:
            close = next(i for i in range(start + 1, len(lines)) if lines[i].strip() == "]")
            lines[close:close] = new
        else:
            at = end if end is not None else len(lines)
            while at > 0 and not lines[at - 1].strip():
                at -= 1
            lines[at:at] = ["", "# Optional layers (opt/<name>/, docs/opt-layers.md): every chunk, its layer, its",
                            "# index in that layer's container, what built it, its hash.", "opt = ["] + new + ["]"]
    with open(p + ".tmp", "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    os.replace(p + ".tmp", p)


def builder_path(kind):
    return os.path.join(TOOLS, KINDS[kind]["builder"])


def build_layer(cache, layer, kind, base, readings, where, what, say, builder=None, reuse=None):
    """A reading-row layer into `cache`: the builder into a new folder, swapped
    in and listed in the manifest. BuildError when the builder or its readings
    are not there, or the builder fails or writes nothing.

    A measured step named for its layer (build_reuse.ChildStep): `reuse`, a
    previous cache whose layer is taken when its record holds - the builder's
    code and data/furigana the same, the readings, the base's text layer (and
    zh-CN's) the same bytes, the same SudachiPy."""
    from build_reuse import ChildStep, packages, take, tree_digest
    k = KINDS[kind]
    builder = builder or builder_path(kind)
    if not os.path.isfile(builder):
        raise BuildError("%s needs the reading-row builder, tools/%s, and this release does not have it, so it "
                         "cannot be built" % (what[0].upper() + what[1:], k["builder"]))
    if not os.path.isfile(readings):
        raise BuildError("%s needs its readings, %s, and they are not there" % (what[0].upper() + what[1:], readings))
    final = os.path.join(cache, "opt", layer)
    work = os.path.join(cache, "opt", "." + layer + ".new")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    with open(readings, "rb") as f:
        readings_sha = sha_bytes(f.read())
    say("Building %s into opt/%s" % (what, layer))
    try:
        with ChildStep(cache, reuse, layer, "reading-row", k["mode"], base, where, readings_sha,
                       tree_digest(os.path.join(cache, "loc", base, "dat")),
                       tree_digest(os.path.join(cache, "loc", "zh-CN", "dat")), packages("sudachi")) as st:
            kept = st.previous(os.path.join("opt", layer))
            if kept:
                take(kept, work)
                say("  %s: the previous cache's, built the same way" % layer, quiet=True)
            else:
                # --out is the directory the builder writes its DATs into: the layer's
                # dat/ (docs/reading-row.md 1.1: cache/opt/<layer>/dat/<NAME>.DAT).
                cmd = [sys.executable, builder, k["mode"], "--cache", cache, "--lang", base, "--readings", readings,
                       "--out", os.path.join(work, "dat")]
                p = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace",
                                   env=st.env())
                for line in (p.stdout + p.stderr).splitlines():
                    say("  " + line, quiet=True)
                if p.returncode != 0:
                    last = next((l for l in reversed((p.stderr + p.stdout).splitlines()) if l.strip()), "no output")
                    raise BuildError("The reading-row builder failed for %s (exit %d): %s"
                                     % (what, p.returncode, last.strip()))
            dd = os.path.join(work, "dat")
            dats = sorted(n for n in os.listdir(dd) if n.upper().endswith(".DAT")) if os.path.isdir(dd) else []
            if not dats:      # raised inside the step, so it keeps no record of a failed build
                raise BuildError("The reading-row builder wrote no .DAT into opt/%s/dat for %s" % (layer, what))
    except BuildError:
        shutil.rmtree(work, ignore_errors=True)
        raise
    rows = []
    for n in dats:
        with open(os.path.join(dd, n), "rb") as f:
            blob = f.read()
        for i, (_, _, body) in enumerate(_chunks(blob, n)):
            rows.append((n, i, where, sha_bytes(body)))
    shutil.rmtree(final, ignore_errors=True)
    os.rename(work, final)
    _manifest_rows(cache, layer, rows)
    say("Built %s: opt/%s, %d container(s), %d chunk(s)" % (what, layer, len(dats), len(rows)))
    return layer


def build_table_layer(h, cache, r, say, builder=None):
    """A table-edit mod's layer into `cache`: the install's own table files with
    the mod's numbers, one file a table (tools/table_edit_build.py), listed in
    the manifest. BuildError when the builder is not in the release, or the
    edits or the install's tables are not what it needs."""
    layer, what = layer_name(r), "the mod %s (%s)" % (r["id"], r["name"])
    builder = builder or builder_path(r["kind"])
    if not os.path.isfile(builder):
        raise BuildError("%s needs the table builder, tools/%s, and this release does not have it, so it cannot "
                         "be built" % (what[0].upper() + what[1:], KINDS[r["kind"]]["builder"]))
    import table_edit_build
    where = "mod:%s@%s" % (r["id"], r["version"])
    say("Building %s into opt/%s" % (what, layer))
    try:
        wrote = table_edit_build.build(cache, layer, os.path.join(mod_dir(h, r["id"]), r["edits"]), where)
    except table_edit_build.Bad as e:
        raise BuildError("%s cannot be built: %s" % (what[0].upper() + what[1:], e))
    _manifest_rows(cache, layer, [("exe/" + n, i, where, hh) for i, (n, hh) in enumerate(wrote)])
    say("Built %s: opt/%s, %d table file(s)" % (what, layer, len(wrote) - 1))
    return layer


def build_patch_layer(h, cache, r, say, builder=None):
    """A disc-patch mod's layers into `cache`: the chunks the builders make
    differently from the player's disc with the author's patch on it
    (tools/disc_patch_build.py, docs/disc-patch-round.md 3 and 3a), listed in
    the manifest; the layers that hold a chunk. BuildError when the builder is
    not in the release, or the disc, the patch or what the patch makes is not
    what mod.toml names."""
    layer, what = layer_name(r), "the mod %s (%s)" % (r["id"], r["name"])
    tag = KINDS["disc-patch"]["builds"][r["build"]][0]
    builder = builder or builder_path(r["kind"])
    if not os.path.isfile(builder):
        raise BuildError("%s needs the disc-patch builder, tools/%s, and this release does not have it, so it "
                         "cannot be built" % (what[0].upper() + what[1:], KINDS[r["kind"]]["builder"]))
    import disc_patch_build
    where = "mod:%s@%s:%s" % (r["id"], r["version"], r["build"])
    say("Building %s into opt/%s" % (what, layer))
    try:
        layers = disc_patch_build.build(h, cache, mod_dir(h, r["id"]), r, tag, where, say)
    except disc_patch_build.Bad as e:
        raise BuildError("%s cannot be built: %s" % (what[0].upper() + what[1:], e))
    for name in layers_of(r):
        _manifest_rows(cache, name, layers.get(name, []))
    done = [name for name in layers_of(r) if layers.get(name)]
    say("Built %s: %s" % (what, ", ".join("opt/%s, %d chunk(s)" % (name, len(layers[name])) for name in done)))
    return done


def build_one(h, cache, r, say, builder=None, reuse=None):
    """One mod's layer into `cache` (its data from H/mods/<id>/)."""
    if r["kind"] == "disc-patch":
        return build_patch_layer(h, cache, r, say, builder if builder and builder.endswith("disc_patch_build.py") else None)
    if r["kind"] == "table-edit":
        return build_table_layer(h, cache, r, say, builder if builder and builder.endswith("table_edit_build.py") else None)
    return build_layer(cache, layer_name(r), r["kind"], r["base"], os.path.join(mod_dir(h, r["id"]), r["readings"]),
                       "mod:%s@%s:%s" % (r["id"], r["version"], r["base"]), "the mod %s (%s)" % (r["id"], r["name"]),
                       say, builder, reuse)


def setup_step(h, cache, say, only=None, builder=None, reuse=None):
    """The mods' layers into `cache`, after its language layers (setup's
    new cache, or the home's own for `build`): every listed mod's. A mod whose `requires` the install lacks is skipped with a
    line naming the layers, and a disc-patch mod whose disc is not in the source
    list with a line naming the disc; anything else that cannot be built raises
    BuildError. `reuse`: setup's previous cache, whose layer is taken when
    built the same way (build_layer). Returns (built layers, skipped mod ids)."""
    done = []
    rows = load_rows(h)
    if only:
        rows = [r for r in rows if r["id"] == only]
        if not rows:
            raise BuildError("no mod %s is installed" % only)
    skipped = []
    for r in rows:
        st, needs, problem = state_of(h, r, cache)
        if st == "problem":
            raise BuildError("The mod %s cannot be built: %s" % (r["id"], problem))
        if st == "needs":
            say("The mod %s (%s) is skipped: it needs the %s language layer, which this install does not hold "
                "(add a disc with that language on the Sources page)" % (r["id"], r["name"], needs))
            skipped.append(r["id"])
            continue
        if st == "needs-disc":
            say("The mod %s (%s) is skipped: it needs the disc %s, which is not in this install's sources (add it on "
                "the Sources page)" % (r["id"], r["name"], needs))
            skipped.append(r["id"])
            continue
        got = build_one(h, cache, r, say, builder, reuse)
        done += got if isinstance(got, list) else [got]
    return done, skipped


def cmd_build(a):
    h = os.path.abspath(a.home)
    cache = os.path.join(h, "cache")
    if not os.path.isfile(os.path.join(cache, "manifest.toml")):
        print("No install has been built yet (no cache\\manifest.toml in %s): the mods are built with it, on the "
              "Sources page." % h, flush=True)
        return 2

    def say(text, quiet=False):
        print(text, flush=True)

    try:
        done, skipped = setup_step(h, cache, say, a.id)
    except BuildError as e:
        print("error %s" % e, flush=True)
        return 1
    print("built %d mod layer(s)%s" % (len(done), ", skipped %d" % len(skipped) if skipped else ""), flush=True)
    return 0


# ---------------------------------------------------------------- the self-test

def make_zip(path, members, comment=b""):
    """A zip of (name, bytes) members, deterministic (fixed times), for make_mod.py
    and the self-test."""
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in members:
            info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, data)
        z.comment = comment
    return path


GOOD_MOD = b'''[mod]
id = "furigana"
name = "Japanese (Furigana)"
version = "0.1.0"
author = "A. Tester"
kind = "reading-row"
requires = ["ja-JP"]

[reading-row]
base = "ja-JP"
readings = "readings.toml"
'''
GOOD_READINGS = '# a test sidecar: no game data\n[[reading]]\nsurface = "漢字"\nreading = "かんじ"\n'.encode()


def selftest(keep=None):
    """Each refusal on a zip made for it, then add / list / build / remove in a
    temporary home. No game data: the caches are empty marker files."""
    root = keep or tempfile.mkdtemp(prefix="bof3x_mods_")
    os.makedirs(root, exist_ok=True)
    fails = []

    def expect(cond, what):
        print("%-78s %s" % (what, "ok" if cond else "FAIL"))
        if not cond:
            fails.append(what)

    def mod_toml(**edit):
        t = GOOD_MOD.decode()
        for k, v in edit.items():
            t = re.sub(r"(?m)^%s = .*$" % k, "%s = %s" % (k, v) if v is not None else "", t)
        return t.encode()

    cases = [
        ("good", [("mod.toml", GOOD_MOD), ("readings.toml", GOOD_READINGS)], None),
        ("no_author", [("mod.toml", mod_toml(author=None)), ("readings.toml", GOOD_READINGS)], None),
        ("credited", [("mod.toml", mod_toml(author='"Ben"\nurl = "https://example.org/hacks/1/"\ncredit = "Made by Ben. '
                                                   'Shared with his permission."')), ("readings.toml", GOOD_READINGS)], None),
        ("empty_author", [("mod.toml", mod_toml(author='"  "')), ("readings.toml", GOOD_READINGS)],
         "mod.author must be a non-empty string"),
        ("author_control", [("mod.toml", GOOD_MOD.replace(b'"A. Tester"', b'"Ben\\u001b[2J"')),
                            ("readings.toml", GOOD_READINGS)],
         "control character"),
        ("url_not_http", [("mod.toml", mod_toml(author='"Ben"\nurl = "file:///C:/Windows/x"')),
                          ("readings.toml", GOOD_READINGS)], "must be an http:// or https:// address"),
        ("url_spaces", [("mod.toml", mod_toml(author='"Ben"\nurl = "https://example.org/a b"')),
                        ("readings.toml", GOOD_READINGS)], "must be an http:// or https:// address"),
        ("unknown_kind", [("mod.toml", mod_toml(kind='"art-swap"')), ("readings.toml", GOOD_READINGS)],
         'kind "art-swap" is not one the tools know'),
        ("no_kind", [("mod.toml", mod_toml(kind=None)), ("readings.toml", GOOD_READINGS)], "kind (missing)"),
        ("holds_dat", [("mod.toml", GOOD_MOD), ("readings.toml", GOOD_READINGS), ("AREA014.DAT", b"\0" * 64)],
         "game data"),
        ("holds_text_layer", [("mod.toml", GOOD_MOD), ("readings.toml", GOOD_READINGS),
                              ("ruby-furigana-ja-JP.AREA014.DAT", b"\0" * 64)], "game data"),
        ("holds_code", [("mod.toml", GOOD_MOD), ("readings.toml", GOOD_READINGS), ("setup.py", b"print(1)\n")],
         "never code"),
        ("holds_unnamed", [("mod.toml", GOOD_MOD), ("readings.toml", GOOD_READINGS), ("notes.txt", b"hi\n")],
         "which mod.toml does not name"),
        ("lacks_named", [("mod.toml", GOOD_MOD)], "names readings.toml, which the zip does not hold"),
        ("escapes_dotdot", [("mod.toml", GOOD_MOD), ("readings.toml", GOOD_READINGS), ("../evil.toml", b"a=1\n")],
         "leaves the mod's folder"),
        ("escapes_absolute", [("mod.toml", GOOD_MOD), ("readings.toml", GOOD_READINGS), ("/evil.toml", b"a=1\n")],
         "leaves the mod's folder"),
        ("in_a_folder", [("furigana/mod.toml", GOOD_MOD), ("furigana/readings.toml", GOOD_READINGS)],
         "a file in a folder"),
        ("no_mod_toml", [("readings.toml", GOOD_READINGS)], "no mod.toml"),
        ("too_big_member", [("mod.toml", GOOD_MOD), ("readings.toml", b"#" * (MEMBER_MAX + 1))],
         "over the %d a member may hold" % MEMBER_MAX),
        ("bad_id", [("mod.toml", mod_toml(id='"Furigana!"')), ("readings.toml", GOOD_READINGS)],
         "must be lowercase letters"),
        ("long_id", [("mod.toml", mod_toml(id='"a-very-long-mod-id"')), ("readings.toml", GOOD_READINGS)],
         "longer than the 23 characters"),
        ("retired_tag", [("mod.toml", mod_toml(requires='["ja"]')), ("readings.toml", GOOD_READINGS)],
         "is retired; use ja-JP"),
        ("base_not_required", [("mod.toml", mod_toml(requires='["en-US"]')), ("readings.toml", GOOD_READINGS)],
         "must be among mod.requires"),
        ("base_unsupported", [("mod.toml", mod_toml(base='"en-US"', requires='["en-US"]')),
                              ("readings.toml", GOOD_READINGS)], "reads ja-JP text only"),
        ("readings_escape", [("mod.toml", mod_toml(readings='"../readings.toml"')), ("readings.toml", GOOD_READINGS)],
         "must be a plain file name"),
        ("readings_not_toml", [("mod.toml", mod_toml(readings='"readings.txt"')), ("readings.txt", b"x\n")],
         "must be a .toml file"),
        ("readings_bad_toml", [("mod.toml", GOOD_MOD), ("readings.toml", b"this is = = not toml\n")],
         "readings.toml is not valid TOML"),
        ("extra_key", [("mod.toml", GOOD_MOD + b'\n[mod.run]\ncommand = "x"\n'), ("readings.toml", GOOD_READINGS)],
         "which the tools do not know"),
        ("extra_table", [("mod.toml", GOOD_MOD + b'\n[hooks]\npost = "x"\n'), ("readings.toml", GOOD_READINGS)],
         "is not one a reading-row mod has"),
        ("bad_version", [("mod.toml", mod_toml(version='"soon"')), ("readings.toml", GOOD_READINGS)],
         "is not a version"),
        ("duplicate_member", None, "in the zip twice"),
        ("not_a_zip", None, "not a readable .zip"),
        ("too_many_members", [("mod.toml", GOOD_MOD), ("readings.toml", GOOD_READINGS)]
         + [("r%02d.toml" % i, b"a=1\n") for i in range(MEMBERS_MAX)], "members, over the"),
    ]
    zips = {}
    for name, members, _ in cases:
        p = os.path.join(root, name + ".zip")
        if name == "duplicate_member":
            import warnings
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                make_zip(p, [("mod.toml", GOOD_MOD), ("readings.toml", GOOD_READINGS), ("readings.toml", b"a=1\n")])
        elif name == "not_a_zip":
            with open(p, "wb") as f:
                f.write(b"PK\x03\x04 not really")
        else:
            make_zip(p, members)
        zips[name] = p
    # A zip bomb's shape: a member whose header claims a small size but holds more.
    p = os.path.join(root, "lying_size.zip")
    make_zip(p, [("mod.toml", GOOD_MOD), ("readings.toml", b"#" * (MEMBER_MAX + 10))])
    with open(p, "r+b") as f:     # rewrite both headers' uncompressed size to 100
        b = bytearray(f.read())
        for sig, off in ((b"PK\x03\x04", 22), (b"PK\x01\x02", 24)):
            i = -1
            while True:
                i = b.find(sig, i + 1)
                if i < 0:
                    break
                n = struct.unpack_from("<H", b, i + (26 if sig == b"PK\x03\x04" else 28))[0]
                if b[i + (30 if sig == b"PK\x03\x04" else 46):][:n] == b"readings.toml":
                    struct.pack_into("<I", b, i + off, 100)
        f.seek(0)
        f.write(b)
    zips["lying_size"] = p
    # zipfile reads only as far as the header says; the CRC then fails
    cases.append(("lying_size", None, "damaged or not readable"))

    for name, _, want in cases:
        try:
            read_zip(zips[name])
            got = None
        except Refused as e:
            got = str(e)
        if want is None:
            expect(got is None, "%s: accepted" % name)
        else:
            expect(got is not None and want in got, "%s: refused (%s)" % (name, got))

    # The cycle in a home with no ja-JP, then with one.
    h = os.path.join(root, "home")
    shutil.rmtree(h, ignore_errors=True)
    cache = os.path.join(h, "cache")
    os.makedirs(os.path.join(cache, "base", "dat"))
    with open(os.path.join(cache, "manifest.toml"), "w", newline="\n") as f:
        f.write('[meta]\ntarget = "pc-zh"\n\n[[source]]\nbuild = "psx-us"\npath = "x.cue"\n\n[cache]\nassets = [\n'
                '  ["AREA000.DAT", 0, "base", "psx-us:copy", "00"],\n]\n\n[exe]\nbuild = "psx-us"\n')
    run = lambda *args: _run_self(list(args))
    rc, out = run("add", zips["good"], "--home", h, "--json")
    o = json.loads(out.splitlines()[-1])
    expect(rc == 0 and o["ok"] and o["state"] == "needs" and o["needs"] == "ja-JP",
           "add into a home without ja-JP: added, state needs ja-JP")
    expect(o["author"] == "A. Tester" and o["url"] is None and load_rows(h)[0].get("author") == "A. Tester",
           "  its author in the add's line and in mods.toml")
    expect(os.path.isfile(os.path.join(h, "mods", "furigana", "readings.toml")), "  unpacked to mods/furigana/")
    rc, out = run("add", zips["holds_dat"], "--home", h, "--json")
    expect(rc == 1 and not json.loads(out)["ok"] and len(load_rows(h)) == 1, "a refused zip: exit 1, the list unchanged")
    lines = []
    done, skipped = setup_step(h, cache, lambda t, quiet=False: lines.append(t))
    expect(not done and skipped == ["furigana"] and "needs the ja-JP language layer" in lines[0],
           "setup without ja-JP: skipped with the layer named")
    os.makedirs(os.path.join(cache, "loc", "ja-JP", "dat"))
    open(os.path.join(cache, "loc", "ja-JP", "dat", "AREA000.DAT"), "wb").close()
    rc, out = run("list", "--home", h, "--json")
    expect(json.loads(out)["state"] == "not-built", "with loc/ja-JP: state not-built")
    try:
        setup_step(h, cache, lambda t, quiet=False: None, builder=os.path.join(root, "absent", "ruby_build.py"))
        expect(False, "the builder absent: refused")
    except BuildError as e:
        expect("ruby_build.py" in str(e) and "does not have it" in str(e), "the builder absent: refused loudly (%s)"
               % str(e)[:60])
    if not os.path.isfile(builder_path("reading-row")):
        rc, out = run("build", "--home", h)
        expect(rc == 1 and out.startswith("error ") and "ruby_build.py" in out,
               "mods.py build with this checkout's builder absent: exit 1, the error line")
    # A stand-in builder (written here, run by path): one container of one kind-18 chunk.
    fake = os.path.join(root, "fake_builder", "ruby_build.py")
    os.makedirs(os.path.dirname(fake), exist_ok=True)
    with open(fake, "w") as f:
        f.write("import argparse, os, struct\np = argparse.ArgumentParser()\np.add_argument('mode')\n"
                "for k in ('--cache', '--lang', '--readings', '--out'): p.add_argument(k)\na = p.parse_args()\n"
                "assert a.mode == 'furigana' and os.path.isfile(a.readings)\nbody = b'\\x01\\0\\0\\0\\0\\0\\0\\0'\n"
                "os.makedirs(a.out)\n"
                "open(os.path.join(a.out, 'AREA000.DAT'), 'wb').write(struct.pack('<4I', 18, 0, len(body), 0)"
                " + body)\nprint('stand-in: 1 message')\n")
    done, _ = setup_step(h, cache, lambda t, quiet=False: None, builder=fake)
    with open(os.path.join(cache, "manifest.toml"), encoding="utf-8") as f:
        man = f.read()
    import tomllib
    rows = tomllib.loads(man)["cache"]["opt"]
    expect(done == ["ruby-furigana-ja-JP"] and rows and rows[0][1] == "opt/ruby-furigana-ja-JP"
           and rows[0][3] == "mod:furigana@0.1.0:ja-JP", "a stand-in builder: the layer built and listed in the manifest")
    rc, out = run("list", "--home", h, "--json")
    expect(json.loads(out)["state"] == "built", "  list: state built")
    done, _ = setup_step(h, cache, lambda t, quiet=False: None, builder=fake)
    with open(os.path.join(cache, "manifest.toml"), encoding="utf-8") as f:
        expect(f.read() == man, "  built again: the manifest byte for byte")
    rc, out = run("add", zips["good"], "--home", h, "--json")
    expect(rc == 0 and json.loads(out)["replaced"] == "0.1.0" and len(load_rows(h)) == 1, "added again: replaced, one row")
    rc, out = run("remove", "furigana", "--home", h)
    with open(os.path.join(cache, "manifest.toml"), encoding="utf-8") as f:
        man = f.read()
    expect(rc == 0 and not load_rows(h) and not os.path.exists(os.path.join(h, "mods", "furigana"))
           and not os.path.exists(os.path.join(cache, "opt", "ruby-furigana-ja-JP")) and "ruby-furigana" not in man,
           "remove: the folder, the row, the layer and its manifest rows gone")
    rc, out = run("remove", "furigana", "--home", h)
    expect(rc == 1 and "not installed" in out, "remove again: exit 1")

    # A table-edit mod (docs/image-layer-round.md 4): refusals, then the cycle
    # over a table file of zeros - the catalogue's key, count and stride, no value.
    import tables
    import tomllib
    cat, _, _ = tables.load()
    wt = next(x for x in cat["table"] if x["key"] == "weapons")
    TE_MOD = (b'[mod]\nid = "sharper"\nname = "Sharper"\nversion = "1.0"\nauthor = "A. Tester"\n'
              b'kind = "table-edit"\nrequires = []\n\n[table-edit]\nedits = "edits.toml"\n')
    TE_EDITS = ('[[set]]\ntable = "weapons"\nrecord = 2\nat = %d\ntype = "u8"\nvalue = 9\n' % (wt["stride"] - 1)).encode()
    te = {}
    for name, members in (
            ("te_good", [("mod.toml", TE_MOD), ("edits.toml", TE_EDITS)]),
            ("te_no_rows", [("mod.toml", TE_MOD), ("edits.toml", b"# nothing\n")]),
            ("te_unknown_table", [("mod.toml", TE_MOD), ("edits.toml", TE_EDITS.replace(b"weapons", b"hats"))]),
            ("te_name", [("mod.toml", TE_MOD), ("edits.toml", b'[[set]]\ntable = "weapons"\nrecord = 2\nfield = "name"\nvalue = 1\n')]),
            ("te_holds_table", [("mod.toml", TE_MOD), ("edits.toml", TE_EDITS), ("weapons.bin", b"\0" * 16)]),
            ("te_long_id", [("mod.toml", TE_MOD.replace(b'"sharper"', b'"a-mod-id-of-20-chars"')), ("edits.toml", TE_EDITS)])):
        te[name] = make_zip(os.path.join(root, name + ".zip"), members)
    for name, want in (("te_good", None), ("te_no_rows", "edits.toml: no [[set]] row"),
                       ("te_unknown_table", "is not a table of the catalogue"), ("te_name", "a name is not a table's number"),
                       ("te_holds_table", "game data"), ("te_long_id", "longer than the 23 characters")):
        try:
            meta, _ = read_zip(te[name])
            got = None
        except Refused as e:
            got = str(e)
        if want is None:
            expect(got is None and meta["layer"] == "mod-sharper", "%s: accepted, layer mod-sharper" % name)
        else:
            expect(got is not None and want in got, "%s: refused (%s)" % (name, (got or "")[:60]))
    rc, out = run("add", te["te_good"], "--home", h, "--json")
    o = json.loads(out.splitlines()[-1])
    expect(rc == 0 and o["ok"] and o["state"] == "not-built" and o["layer"] == "mod-sharper",
           "a table-edit mod added: state not-built (it needs no language)")
    try:
        setup_step(h, cache, lambda t, quiet=False: None)
        expect(False, "an install with no table files: refused")
    except BuildError as e:
        expect("no table files" in str(e), "an install with no table files: the build refused, with the rebuild sentence")
    td = os.path.join(cache, "base", "exe", "tables")
    os.makedirs(td)
    zeros = bytes(wt["count"] * wt["stride"])
    with open(os.path.join(td, "weapons.bin"), "wb") as f:
        f.write(zeros)
    with open(os.path.join(td, "tables.toml"), "w", newline="\n") as f:
        f.write('build = "test"\n\ntables = [\n  ["weapons", 0x%06X, 0x%06X, %d, %d, "%s", "test"],\n]\n'
                % (0x600000, 0x600000 + len(zeros), wt["count"], wt["stride"], sha_bytes(zeros)))
    done, _ = setup_step(h, cache, lambda t, quiet=False: None)
    d = os.path.join(cache, "opt", "mod-sharper", "exe")
    with open(os.path.join(cache, "manifest.toml"), encoding="utf-8") as f:
        man = f.read()
    rows = [r for r in tomllib.loads(man)["cache"]["opt"] if r[1] == "opt/mod-sharper"]
    with open(os.path.join(d, "weapons.bin"), "rb") as f:
        body = f.read()
    expect(done == ["mod-sharper"] and len(rows) == 2 and rows[0][3] == "mod:sharper@1.0" and
           body[3 * wt["stride"] - 1] == 9 and sum(body) == 9,
           "the table layer built: one byte of the table changed, both files listed in the manifest")
    with open(os.path.join(d, "tables.toml"), "rb") as f:
        expect(tomllib.load(f)["cells"] == [["weapons", 2, wt["stride"] - 1, 1]], "  its cells: the one byte the mod set")
    rc, out = run("list", "--home", h, "--json")
    expect(json.loads(out)["state"] == "built", "  list: state built")
    rc, out = run("remove", "sharper", "--home", h)
    with open(os.path.join(cache, "manifest.toml"), encoding="utf-8") as f:
        expect(rc == 0 and not os.path.exists(os.path.join(cache, "opt", "mod-sharper")) and "mod-sharper" not in f.read(),
               "remove: the table layer and its manifest rows gone")
    # A disc-patch mod (docs/disc-patch-round.md 2): a patch tools/vcdiff.py makes
    # here over bytes of its own, so no game data; refusals, then its states.
    import vcdiff
    src = bytes(range(256)) * 16
    patch, _ = vcdiff.make_patch(src, [([("copy", 0, 1024, 0), ("add", b"a romhack"), ("copy", 2048, 512, 0)],
                                        len(src), 0, 0, True, 1)])
    big, _ = vcdiff.make_patch(b"", [([("run", 0, 1 << 20)], 0, 0, 0, False, 1)] * 2)   # 2 MiB, against a bound of 1 here

    def dp_toml(**over):
        mod = {"id": '"definitive"', "name": '"Definitive Edition"', "version": '"1.0"', "author": '"A. Hacker"',
               "kind": '"disc-patch"', "requires": '["en-US"]'}
        tab = {"build": '"psx-us"', "patch": '"hack.xdelta"', "patch_sha256": '"%s"' % sha_bytes(patch),
               "result_sha256": '"%s"' % ("0" * 64)}
        for key, v in over.items():
            (mod if key in mod or key == "needs" else tab)[key] = v
        return ("[mod]\n" + "".join("%s = %s\n" % kv for kv in mod.items() if kv[1] is not None) + "\n[disc-patch]\n" +
                "".join("%s = %s\n" % kv for kv in tab.items() if kv[1] is not None)).encode()

    dp = {}
    for name, members, want in (
            ("dp_good", [("mod.toml", dp_toml()), ("hack.xdelta", patch)], None),
            ("dp_no_author", [("mod.toml", dp_toml(author=None)), ("hack.xdelta", patch)], "must name its author"),
            ("dp_other_disc", [("mod.toml", dp_toml(build='"psx-xx"')), ("hack.xdelta", patch)],
             "not a disc a disc-patch mod can be made for (psx-us)"),
            ("dp_no_text", [("mod.toml", dp_toml(requires="[]")), ("hack.xdelta", patch)], "mod.requires must name en-US"),
            ("dp_not_xdelta", [("mod.toml", dp_toml(patch='"hack.toml"')), ("hack.toml", b"a = 1\n")],
             "must be a .xdelta or .ppf file"),
            ("dp_other_file", [("mod.toml", dp_toml()), ("hack.xdelta", patch + b"\0")],
             "is not the file mod.toml's patch_sha256 names"),
            ("dp_bad_sha", [("mod.toml", dp_toml(result_sha256='"ABC"')), ("hack.xdelta", patch)],
             "must be a sha256, 64 lowercase hex digits"),
            ("dp_not_vcdiff", [("mod.toml", dp_toml(patch_sha256='"%s"' % sha_bytes(b"PATCH" * 40))),
                               ("hack.xdelta", b"PATCH" * 40)], "not a VCDIFF patch"),
            ("dp_too_big", [("mod.toml", dp_toml(patch_sha256='"%s"' % sha_bytes(big))), ("hack.xdelta", big)],
             "a disc's data track may be"),
            ("dp_holds_track", [("mod.toml", dp_toml()), ("hack.xdelta", patch), ("track1.bin", b"\0" * 16)], "game data"),
            ("dp_long_id", [("mod.toml", dp_toml(id='"a-mod-id-of-20-chars"')), ("hack.xdelta", patch)],
             "longer than the 23 characters"),
            ("dp_id_14", [("mod.toml", dp_toml(id='"fourteen-chars"')), ("hack.xdelta", patch)],
             "the layer mod-fourteen-chars-en-US is longer"),
            ("te_holds_patch", [("mod.toml", TE_MOD), ("edits.toml", TE_EDITS), ("hack.xdelta", patch)],
             "which mod.toml does not name"),
            # the behaviours a mod needs and the records it skips (docs/mods.md 11)
            ("dp_needs", [("mod.toml", dp_toml(needs='["SelfTestBeta", "SelfTestAlpha"]',
                                               skip='[{table = "shop_records", records = [40, 49]}]')),
                          ("hack.xdelta", patch)], None),
            ("dp_needs_not_list", [("mod.toml", dp_toml(needs='"SmokeSkill"')), ("hack.xdelta", patch)],
             "mod.needs must be a list of behaviour names"),
            ("dp_needs_bad_name", [("mod.toml", dp_toml(needs='["smoke skill"]')), ("hack.xdelta", patch)],
             "is not a behaviour name"),
            ("dp_needs_twice", [("mod.toml", dp_toml(needs='["SelfTestAlpha", "SelfTestAlpha"]')), ("hack.xdelta", patch)],
             "names a behaviour twice"),
            ("dp_skip_table", [("mod.toml", dp_toml(skip='[{table = "hats", records = [0, 1]}]')), ("hack.xdelta", patch)],
             "disc-patch.skip entry 1: table \"hats\" is not a table of the catalogue"),
            ("dp_skip_records", [("mod.toml", dp_toml(skip='[{table = "shop_records", records = [40, 50]}]')),
                                 ("hack.xdelta", patch)], "records must be [first, last] inside shop_records (0 to 49)"),
            ("dp_skip_shape", [("mod.toml", dp_toml(skip='["shop_records"]')), ("hack.xdelta", patch)],
             "must have exactly two keys"),
            ("te_needs", [("mod.toml", TE_MOD.replace(b'requires = []', b'requires = []\nneeds = ["SelfTestAlpha"]')),
                          ("edits.toml", TE_EDITS)], None),
            ("rr_needs", [("mod.toml", GOOD_MOD.replace(b'kind = "reading-row"', b'kind = "reading-row"\nneeds = ["SelfTestAlpha"]')),
                          ("readings.toml", GOOD_READINGS)], "a reading-row mod cannot need behaviours")):
        dp[name] = make_zip(os.path.join(root, name + ".zip"), members)
        bound = RESULT_MAX
        try:
            if name == "dp_too_big":
                globals()["RESULT_MAX"] = 1 << 20
            meta, _ = read_zip(dp[name])
            got = None
        except Refused as e:
            got = str(e)
        finally:
            globals()["RESULT_MAX"] = bound
        if name == "te_needs":
            expect(got is None and meta["layer"] == "mod-sharper" and meta["needs"] == ["SelfTestAlpha"],
                   "%s: a table-edit mod may need behaviours too" % name)
        elif name == "dp_needs":
            expect(got is None and meta["needs"] == ["SelfTestBeta", "SelfTestAlpha"]
                   and meta["skip"] == [["shop_records", 40, 49]],
                   "%s: accepted; its needs in their order, its skip in the home's form" % name)
        elif want is None:
            expect(got is None and meta["layer"] == "mod-definitive" and meta["build"] == "psx-us",
                   "%s: accepted, layer mod-definitive" % name)
        else:
            expect(got is not None and want in got, "%s: refused (%s)" % (name, (got or "")[:60]))
    # the release's behaviours: the generated header's list, read as the launcher's is built from it
    hdr = os.path.join(root, "code_names.h")
    with open(hdr, "w", newline="\n") as f:
        f.write('namespace bof3::code_names {\nconstexpr const char* kNames[kCount] = {\n    "NotABehaviour",\n};\n}\n'
                'namespace bof3::behaviours {\n\nenum class Id : int {\n    SelfTestAlpha,\n    SelfTestGamma,\n};\n'
                'constexpr int kCount = 2;\nconstexpr const char* kNames[kCount + 1] = {\n    "SelfTestAlpha",\n'
                '    "SelfTestGamma",\n    nullptr,\n};\n\n}  // namespace bof3::behaviours\n')
    expect(release_behaviours(hdr) == ("SelfTestAlpha", "SelfTestGamma"),
           "behaviours: the names of code_names.h's behaviours block, not its code names")
    expect(release_behaviours(os.path.join(root, "none.h")) == (), "behaviours: no header, no behaviour")
    expect(isinstance(release_behaviours(), tuple) and "SelfTestAlpha" not in release_behaviours(),
           "behaviours: this checkout's header is read, and no test name is in it")
    o = obj({"id": "x", "kind": "disc-patch", "needs": ["SelfTestBeta", "SelfTestAlpha"]}, known=("SelfTestAlpha",))
    expect(o["behaviours"] == ["SelfTestBeta", "SelfTestAlpha"] and o["missing"] == ["SelfTestBeta"],
           "behaviours: missing is what the release does not have, in the mod's order")
    o = obj({"id": "x", "kind": "disc-patch"}, known=())
    expect(o["behaviours"] == [] and o["missing"] == [], "behaviours: a mod with no needs misses none")
    h2 = os.path.join(root, "home_needs")
    rc, out = run("add", dp["dp_needs"], "--home", h2, "--json")
    o = json.loads(out.splitlines()[-1])
    with open(mods_file(h2), encoding="utf-8") as f:
        row2 = tomllib.loads(f.read())["mod"][0]
    expect(rc == 0 and o["ok"] and o["behaviours"] == ["SelfTestBeta", "SelfTestAlpha"]
           and o["missing"] == ["SelfTestBeta", "SelfTestAlpha"],
           "add: the needs in the JSON; this release has neither, so both are missing")
    expect(row2["needs"] == ["SelfTestBeta", "SelfTestAlpha"] and row2["skip"] == [["shop_records", 40, 49]],
           "  mods.toml keeps the needs and the skip (TOML the launcher reads)")
    rc, out = run("list", "--home", h2)
    expect(rc == 0 and "needs behaviours this release does not have: SelfTestBeta, SelfTestAlpha" in out,
           "  list: the missing behaviours named on the mod's line")
    rc, out = run("add", dp["dp_good"], "--home", h, "--json")
    o = json.loads(out.splitlines()[-1])
    expect(rc == 0 and o["ok"] and o["state"] == "needs" and o["needs"] == "en-US",
           "a disc-patch mod added with no en-US text: state needs en-US")
    os.makedirs(os.path.join(cache, "loc", "en-US", "dat"))
    with open(os.path.join(cache, "loc", "en-US", "dat", "AREA000.DAT"), "wb") as f:
        f.write(b"\0" * 16)
    rc, out = run("list", "--home", h, "--json")
    o = json.loads(out.splitlines()[-1])
    expect(o["ok"] and o["state"] == "needs-disc" and o["needs"] == "PlayStation, USA" and "Data, Sources" in o["problem"],
           "  with the text and no disc in sources.toml: state needs-disc, the disc named")
    said = []
    done, skipped = setup_step(h, cache, lambda t, quiet=False: said.append(t))
    expect(done == [] and skipped == ["definitive"] and any("needs the disc PlayStation, USA" in t for t in said),
           "  the build step skips it with a line that names the disc")
    with open(os.path.join(h, "sources.toml"), "w", newline="\n") as f:
        f.write('[[source]]\npath = "C:\\\\nowhere\\\\us.cue"\nbuild = "psx-us"\nkind = "psx"\n')
    rc, out = run("list", "--home", h, "--json")
    o = json.loads(out.splitlines()[-1])
    expect(o["state"] == "not-built" and o["problem"] is None and o["layer"] == "mod-definitive"
           and o["text_layer"] == "mod-definitive-en-US", "  with the disc listed: state not-built; its two layers named")
    with open(os.path.join(mods_file(h)), encoding="utf-8") as f:
        row = [r for r in tomllib.loads(f.read())["mod"] if r["id"] == "definitive"][0]
    expect(row["build"] == "psx-us" and row["patch"] == "hack.xdelta" and row["patch_sha256"] == sha_bytes(patch)
           and row["author"] == "A. Hacker", "  mods.toml keeps the disc, the patch and both hashes")
    try:
        build_patch_layer(h, cache, row, lambda t, quiet=False: None, builder=os.path.join(root, "no_such_builder.py"))
        expect(False, "a release without the builder: refused")
    except BuildError as e:
        expect("does not have it" in str(e), "  a release without the disc-patch builder: the build refused, by name")
    try:
        setup_step(h, cache, lambda t, quiet=False: None, only="definitive")
        expect(False, "a disc that is listed and not there: refused")
    except BuildError as e:
        expect("is not there" in str(e), "  the builder run: a disc that is listed and not there is refused, by path")
    # the text layer alone is a built mod (a hack that changes text and no table)
    td = os.path.join(cache, "opt", "mod-definitive-en-US", "dat")
    os.makedirs(td)
    with open(os.path.join(td, "AREA000.DAT"), "wb") as f:
        f.write(b"\0" * 16)
    _manifest_rows(cache, "mod-definitive-en-US", [("AREA000.DAT", 0, "mod:definitive@1.0:psx-us", "00")])
    rc, out = run("list", "--home", h, "--json")
    o = json.loads(out.splitlines()[-1])
    expect(o["state"] == "built" and o["language"] == "en-US" and o.get("carries") is None,
           "  its text layer alone in the cache: state built; its text's language named; no report, no sentence")
    # the text carried over to another language (docs/disc-patch-round.md 11): a third layer, and `languages`
    expect(o["languages"] == ["en-US"] and layers_of(row) == ["mod-definitive", "mod-definitive-en-US",
                                                              "mod-definitive-en-150"]
           and layers_of(dict(row, id="thirteen-char")) == ["mod-thirteen-char", "mod-thirteen-char-en-US"],
           "  languages: its own alone; the en-150 layer is its third, left out where the name is too long")
    te = os.path.join(cache, "opt", "mod-definitive-en-150", "dat")
    os.makedirs(te)
    with open(os.path.join(te, "AREA000.DAT"), "wb") as f:
        f.write(b"\0" * 16)
    _manifest_rows(cache, "mod-definitive-en-150", [("AREA000.DAT", 0, "mod:definitive@1.0:psx-us", "00")])
    rc, out = run("list", "--home", h, "--json")
    expect(json.loads(out.splitlines()[-1])["languages"] == ["en-US", "en-150"],
           "  with the en-150 layer in the cache: languages names both")
    import disc_patch_build
    disc_patch_build.write_report(mod_dir(h, "definitive"), "w", {"enemy_tables": 2, "text_sections": 3},
                                  {"program_sections": 4})
    rc, out = run("list", "--home", h, "--json")
    o = json.loads(out.splitlines()[-1])
    expect(o["carries"] == "This build of the mod has these changes: 2 enemy tables, 3 text sections."
           and o["lacks"] == "It does not have these changes of the patch: the program changes in 4 sections.",
           "  with its build's report.toml: what it has and what it does not have, for the Mods tab")
    rc, out = run("remove", "definitive", "--home", h)
    with open(os.path.join(cache, "manifest.toml"), encoding="utf-8") as f:
        expect(rc == 0 and not os.path.exists(os.path.join(h, "mods", "definitive"))
               and not os.path.exists(os.path.join(cache, "opt", "mod-definitive-en-US")) and "mod-definitive" not in f.read(),
               "remove: the patch gone from the home, both layers and their manifest rows from the cache")
    # a patch added by itself (docs/mods.md 12): one the list has, one it does not, a file that is no patch
    other, _ = vcdiff.make_patch(src, [([("copy", 0, 512, 0), ("add", b"another romhack")], len(src), 0, 0, True, 1)])
    klist = os.path.join(root, "known.toml")
    entry = ('[[patch]]\nid = "definitive"\nname = "Definitive Edition"\nversion = "1.4"\nauthor = "A. Hacker"\n'
             'url = "https://example.org/hack"\nrequires = ["en-US"]\nneeds = ["SelfTestAlpha"]\nbuild = "psx-us"\n'
             'patch_sha256 = "%s"\nresult_sha256 = "%s"\nskip = [{table = "shop_records", records = [40, 49]}]\n'
             % (sha_bytes(patch), "1" * 64))
    with open(klist, "w", newline="\n") as f:
        f.write(entry)
    files = {}
    for name, blob in (("As Released 1.4.xdelta", patch), ("Some Other Hack.xdelta", other), ("notes.xdelta", b"PATCH" * 40),
                       ("hack.xdelta.7z", b"7z\xbc\xaf\x27\x1c" + b"\0" * 32)):
        files[name] = os.path.join(root, name)
        with open(files[name], "wb") as f:
            f.write(blob)
    kn = load_known(klist)
    meta, data = read_patch(files["As Released 1.4.xdelta"], kn)
    expect(meta["id"] == "definitive" and meta["version"] == "1.4" and meta["author"] == "A. Hacker"
           and meta["needs"] == ["SelfTestAlpha"] and meta["patch"] == "definitive-1.4.xdelta" and "unverified" not in meta
           and data[meta["patch"]] == patch, "a patch of the list: the list's mod, its author and its needs")
    again = check_meta(tomllib.loads(data[MOD_TOML].decode()))
    expect(all(again[k] == meta[k] for k in ("id", "name", "version", "author", "url", "requires", "needs", "build",
                                              "patch", "patch_sha256", "result_sha256", "skip")),
           "  the mod.toml written beside it is a mod zip's, and reads back the same")
    meta, data = read_patch(files["Some Other Hack.xdelta"], kn)
    uid = "patch-" + sha_bytes(other)[:6]
    expect(meta["id"] == uid and meta["name"] == "Some Other Hack" and meta.get("unverified") is True
           and "author" not in meta and "needs" not in meta and "result_sha256" not in meta and meta["build"] == "psx-us"
           and len(max(layers_of(meta), key=len)) <= LAYER_MAX,
           "a patch the list does not have: unverified, no author, no behaviour, an id from its sha256")
    for name, want in (("notes.xdelta", "unknown format"), ("hack.xdelta.7z", "unpack it")):
        try:
            read_patch(files[name], kn)
            got = None
        except Refused as e:
            got = str(e)
        expect(got is not None and want in got, "%s: refused (%s)" % (name, (got or "")[:60]))
    for text, want in ((entry + entry, "twice"), (entry.replace('author = "A. Hacker"\n', ""), "must name its author"),
                       (entry + 'kind = "table-edit"\n', "kind is not a key of the list"),
                       ("[mod]\nid = 1\n", "only [[patch]] tables")):
        bad = os.path.join(root, "bad_known.toml")
        with open(bad, "w", newline="\n") as f:
            f.write(text)
        try:
            load_known(bad)
            got = None
        except Refused as e:
            got = str(e)
        expect(got is not None and want in got, "a broken list: refused (%s)" % (got or "")[-50:])
    expect(load_known(os.path.join(root, "no_list.toml")) == {}, "a release without the list knows no patch")
    try:
        mine = load_known()
        expect(all(m["author"] for m in mine.values()), "this checkout's list reads: %d patch(es), each with its author" % len(mine))
    except Refused as e:
        expect(False, "this checkout's list reads (%s)" % e)
    import ppf
    pp1, pp2 = ppf.make_patch([(16, b"a description")], undo=src), ppf.make_patch([(64, b"another")])
    with open(klist, "a", newline="\n") as f:
        f.write('\n[[patch]]\nid = "descriptions"\nname = "Better Words"\nversion = "1.1.1"\nauthor = "B. Writer"\n'
                'requires = ["en-US"]\nbuild = "psx-us"\nformat = "ppf"\npatch_sha256 = "%s"\nresult_sha256 = "%s"\n'
                % (sha_bytes(pp1), "2" * 64))
    for name, blob in (("Words (This Disc).ppf", pp1), ("Words (Other).ppf", pp2), ("old.ppf", b"PPF20\x01" + bytes(80))):
        files[name] = os.path.join(root, name)
        with open(files[name], "wb") as f:
            f.write(blob)
    kn = load_known(klist)
    meta, data = read_patch(files["Words (This Disc).ppf"], kn)
    expect(meta["id"] == "descriptions" and meta["patch"] == "descriptions-1.1.1.ppf" and meta["author"] == "B. Writer"
           and "needs" not in meta and check_meta(tomllib.loads(data[MOD_TOML].decode()))["patch"] == meta["patch"],
           "a PPF patch of the list: the list's mod, its file kept as .ppf")
    meta, _ = read_patch(files["Words (Other).ppf"], kn)
    expect(meta.get("unverified") is True and meta["patch"].endswith(".ppf"), "a PPF patch the list does not have: unverified")
    try:
        read_patch(files["old.ppf"], kn)
        got = None
    except Refused as e:
        got = str(e)
    expect(got is not None and "only 3.0 is read" in got, "a PPF 2.0 patch: refused by its version")
    expect(patch_format(patch)[0] == "xdelta" and patch_format(pp1) == ("ppf", 29), "the format is the file's first bytes")
    zz = make_zip(os.path.join(root, "authors_archive.zip"), [("Words.ppf", pp1), ("README.txt", b"read me")])
    try:
        read_any(zz, kn)
        got = None
    except Refused as e:
        got = str(e)
    expect(got is not None and "unpack it" in got, "an author's zip with no mod.toml: refused, the sentence says to unpack it")
    bad = os.path.join(root, "bad_known.toml")
    with open(bad, "w", newline="\n") as f:
        f.write(entry + 'format = "ips"\n')
    try:
        load_known(bad)
        got = None
    except Refused as e:
        got = str(e)
    expect(got is not None and "is not one the tools read" in got, "a list entry of another format: refused")
    h3 = os.path.join(root, "home_patch")
    rc, out = run("add", files["As Released 1.4.xdelta"], "--home", h3, "--json", "--known", klist)
    o = json.loads(out.splitlines()[-1])
    expect(rc == 0 and o["ok"] and o["id"] == "definitive" and o["author"] == "A. Hacker" and o["unverified"] is False
           and o["behaviours"] == ["SelfTestAlpha"] and os.path.isfile(os.path.join(h3, "mods", "definitive", "definitive-1.4.xdelta")),
           "add: a patch of the list is the list's mod, its file kept under the list's name")
    rc, out = run("add", files["Some Other Hack.xdelta"], "--home", h3, "--json", "--known", klist)
    o = json.loads(out.splitlines()[-1])
    expect(rc == 0 and o["ok"] and o["id"] == uid and o["unverified"] is True and o["author"] is None
           and o["behaviours"] == [] and o["known_now"] is None and o["state"] == "needs",
           "add: a patch the list does not have is added unverified, with no author and no behaviour")
    with open(mods_file(h3), encoding="utf-8") as f:
        text3 = f.read()
    row3 = [r for r in tomllib.loads(text3)["mod"] if r["id"] == uid][0]
    expect(row3["unverified"] is True and "result_sha256" not in row3 and "author" not in row3
           and "unverified = true" in text3, "  mods.toml keeps unverified = true, and no result hash")
    rc, out = run("list", "--home", h3, "--known", klist)
    expect(rc == 0 and "its author and its function are not verified" in out, "  list: the line says it is not verified")
    with open(klist, "a", newline="\n") as f:
        f.write(entry.replace("definitive", "other").replace(sha_bytes(patch), sha_bytes(other)))
    rc, out = run("list", "--home", h3, "--json", "--known", klist)
    o = [json.loads(x) for x in out.splitlines() if json.loads(x)["id"] == uid][0]
    expect(o["unverified"] is True and o["known_now"] == "Definitive Edition 1.4",
           "  a list that has it after all: known_now names the mod it is")
    rc, out = run("add", files["notes.xdelta"], "--home", h3, "--json", "--known", klist)
    o = json.loads(out.splitlines()[-1])
    expect(rc == 1 and not o["ok"] and "unknown format" in o["problem"], "add: a file that is no patch is refused: unknown format")
    rc, out = run("remove", uid, "--home", h3)
    expect(rc == 0 and not os.path.exists(os.path.join(h3, "mods", uid)), "remove: the unverified patch gone from the home")
    if not keep:
        shutil.rmtree(root, ignore_errors=True)
    else:
        print("zips and the test home kept in %s" % root)
    print("selftest: %d failure(s)" % len(fails))
    return 1 if fails else 0


def _run_self(args):
    p = subprocess.run([sys.executable, os.path.abspath(__file__)] + args, capture_output=True, text=True,
                       encoding="utf-8")
    return p.returncode, p.stdout.strip()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    p = s.add_parser("add", help="check a mod zip or a patch file, unpack it under HOME/mods/<id>/ and list it in "
                                 "HOME/mods.toml")
    p.add_argument("zip", help="a mod's .zip, or a romhack's patch file as its author released it (.xdelta)")
    p.add_argument("--home", required=True)
    p.add_argument("--json", action="store_true")
    p.add_argument("--known", help=argparse.SUPPRESS)   # another list of known patches (the self-test's)
    p = s.add_parser("remove", help="a mod's folder, its row and the layer built from it")
    p.add_argument("id")
    p.add_argument("--home", required=True)
    p = s.add_parser("list")
    p.add_argument("--home", required=True)
    p.add_argument("--json", action="store_true")
    p.add_argument("--known", help=argparse.SUPPRESS)
    p = s.add_parser("build", help="build the mods' layers into HOME/cache in place (setup does this too)")
    p.add_argument("id", nargs="?")
    p.add_argument("--home", required=True)
    p = s.add_parser("check", help="the checks of add alone")
    p.add_argument("zip")
    p.add_argument("--json", action="store_true")
    p.add_argument("--known", help=argparse.SUPPRESS)
    p = s.add_parser("selftest")
    p.add_argument("--keep", help="make the zips here and keep them")
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if a.cmd == "selftest":
        return selftest(os.path.abspath(a.keep) if a.keep else None)
    return {"add": cmd_add, "remove": cmd_remove, "list": cmd_list, "build": cmd_build, "check": cmd_check}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main() or 0)
