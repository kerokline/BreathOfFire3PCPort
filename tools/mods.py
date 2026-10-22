#!/usr/bin/env python
"""The player's mods: a .zip of data files, checked, unpacked under the home and built into its cache.

    python tools/mods.py add ZIP --home H [--json]     check, unpack to H/mods/<id>/, record in H/mods.toml
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
its name. Today there is one, `reading-row` (the reading-row round,
docs/reading-row-round.md section 2 ML): a row above each text row of the
dialogue box, built on the player's machine by tools/ruby_build.py into the
optional text layer `ruby-<id>-<base>` (docs/reading-row.md section 1) - a layer
the launcher's *Row above the text* box offers and adds to BOF3X_OPT.

The home's `mods.toml` is written only by this tool, in the shape of
`sources.toml` (docs/install-sources.md section 3): one `[[mod]]` table a mod, a
`key = value` line each, the values JSON-quoted. The launcher reads it for the
Settings page's box (src/launcher/config.cpp ConfigModsInstalled) and runs this
tool for the Mods tab (src/launcher/sources_dialog.cpp, the Mods page), as the
Sources page runs importer.py: `--json` lines, one child at a time.

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
}


class Refused(Exception):
    """A zip that is not a mod the tools take; the message is one sentence for the player."""


class BuildError(Exception):
    """A mod's step could not run or failed; the message names the mod and why."""


def layer_name(meta):
    """The optional layer a mod's step builds (docs/reading-row.md 1.3)."""
    if meta["kind"] == "reading-row":
        return "ruby-%s-%s" % (meta["id"], meta["base"])
    raise BuildError("no layer is defined for kind %r" % meta["kind"])


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
    extra = sorted(set(mod) - {"id", "name", "version", "kind", "requires"})
    if extra:
        raise Refused("mod.toml: [mod] has %s, which the tools do not know (id, name, version, kind, requires)"
                      % extra[0])
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
    req = mod.get("requires")
    if not isinstance(req, list):
        raise Refused("mod.toml: mod.requires must be a list of language tags, e.g. [\"ja-JP\"]")
    meta["requires"] = [_tag(t, "mod.requires") for t in req]
    if len(set(meta["requires"])) != len(meta["requires"]):
        raise Refused("mod.toml: mod.requires names a tag twice")
    t = doc.get(k["table"])
    if not isinstance(t, dict):
        raise Refused("mod.toml: a %s mod needs its [%s] table" % (kind, k["table"]))
    extra = sorted(set(t) - set(k["keys"]))
    if extra:
        raise Refused("mod.toml: [%s] has %s, which the tools do not know (%s)"
                      % (k["table"], extra[0], ", ".join(k["keys"])))
    files = []
    for key, how in k["keys"].items():
        where = "%s.%s" % (k["table"], key)
        if how == "tag":
            meta[key] = _tag(t.get(key), where)
        else:
            v = _string(t, key, k["table"], 64)
            if not FILE_RE.fullmatch(v) or v == MOD_TOML:
                raise Refused("mod.toml: %s %r must be a plain file name at the zip's root" % (where, v))
            want = k.get("file_ext", {}).get(key)
            if want and not v.lower().endswith(want):
                raise Refused("mod.toml: %s %r must be a %s file" % (where, v, want))
            meta[key] = v
            files.append(v)
    if kind == "reading-row":
        if meta["base"] not in meta["requires"]:
            raise Refused("mod.toml: reading-row.base %s must be among mod.requires" % meta["base"])
        if meta["base"] not in k["bases"]:
            raise Refused("mod.toml: reading-row.base %s - the reading-row builder reads %s text only"
                          % (meta["base"], " and ".join(k["bases"])))
    meta["files"] = files
    meta["layer"] = layer_name(meta)
    if len(meta["layer"]) > LAYER_MAX:
        raise Refused("mod.toml: the layer %s is longer than the %d characters a layer name may have; "
                      "a shorter mod.id fits" % (meta["layer"], LAYER_MAX))
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
    return meta, data


def sha_bytes(b):
    return hashlib.sha256(b).hexdigest()


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


ROW_KEYS = ("id", "name", "version", "kind", "requires", "base", "readings", "files", "zip", "sha256", "added")


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
    return _has_dat(os.path.join(cache, "opt", layer, "dat")) and manifest_lists(cache, layer)


def state_of(h, r, cache=None):
    """(state, needs, problem): built, not-built, needs (a required language layer
    not held: `needs` names them) or problem (the mod's files are not there)."""
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
    if built(cache, layer_name(r)):
        return "built", None, None
    return "not-built", None, None


def obj(r, path=None, ok=True, problem=None, state=None, needs=None, **more):
    o = {"path": path, "ok": ok, "id": r.get("id"), "name": r.get("name"), "version": r.get("version"),
         "kind": r.get("kind"), "requires": r.get("requires"), "base": r.get("base"),
         "layer": layer_name(r) if r.get("kind") in KINDS and r.get("id") else None,
         "state": state, "needs": needs, "problem": problem}
    o.update(more)
    return o


def _emit(o, as_json):
    if as_json:
        print(json.dumps(o, ensure_ascii=False), flush=True)
    elif o["ok"]:
        extra = {"built": "built", "not-built": "not built yet", "needs": "needs %s" % o.get("needs")}.get(o["state"], "")
        print("%s %s (%s, %s)%s" % (o["id"], o["version"], o["name"], o["kind"], ": " + extra if extra else ""),
              flush=True)
    else:
        print("%s: %s" % (o.get("path") or o.get("id"), o["problem"]), flush=True)


# ---------------------------------------------------------------- the commands

def cmd_check(a):
    try:
        meta, _ = read_zip(a.zip)
    except Refused as e:
        _emit(obj({}, a.zip, False, str(e)), a.json)
        return 1
    _emit(obj(meta, a.zip), a.json)
    return 0


def cmd_add(a):
    h = os.path.abspath(a.home)
    path = os.path.abspath(a.zip)
    try:
        meta, data = read_zip(path)
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
    row.update({k: meta[k] for k in KINDS[meta["kind"]]["keys"]})
    row.update(files=meta["files"], zip=path, sha256=sha_file(path),
               added=datetime.datetime.now().isoformat(timespec="seconds"))
    rows = [row if r is old else r for r in rows] if old else rows + [row]
    save_rows(h, rows)
    st, needs, problem = state_of(h, row)
    _emit(obj(row, path, True, problem, st, needs, replaced=old["version"] if old else None), a.json)
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
        remove_layer(cache, layer_name(gone))
    save_rows(h, keep)
    print("removed: %s" % a.id, flush=True)
    return 0


def cmd_list(a):
    h = os.path.abspath(a.home)
    rows = load_rows(h)
    for r in rows:
        st, needs, problem = state_of(h, r)
        _emit(obj(r, mod_dir(h, r["id"]), st != "problem", problem, st, needs), a.json)
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


def build_layer(cache, layer, kind, base, readings, where, what, say, builder=None):
    """A reading-row layer into `cache`: the builder into a new folder, swapped
    in and listed in the manifest. BuildError when the builder or its readings
    are not there, or the builder fails or writes nothing."""
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
    # --out is the directory the builder writes its DATs into: the layer's
    # dat/ (docs/reading-row.md 1.1: cache/opt/<layer>/dat/<NAME>.DAT).
    cmd = [sys.executable, builder, k["mode"], "--cache", cache, "--lang", base, "--readings", readings,
           "--out", os.path.join(work, "dat")]
    say("Building %s into opt/%s" % (what, layer))
    p = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    for line in (p.stdout + p.stderr).splitlines():
        say("  " + line, quiet=True)
    if p.returncode != 0:
        shutil.rmtree(work, ignore_errors=True)
        last = next((l for l in reversed((p.stderr + p.stdout).splitlines()) if l.strip()), "no output")
        raise BuildError("The reading-row builder failed for %s (exit %d): %s" % (what, p.returncode, last.strip()))
    dd = os.path.join(work, "dat")
    dats = sorted(n for n in os.listdir(dd) if n.upper().endswith(".DAT")) if os.path.isdir(dd) else []
    if not dats:
        shutil.rmtree(work, ignore_errors=True)
        raise BuildError("The reading-row builder wrote no .DAT into opt/%s/dat for %s" % (layer, what))
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


def build_one(h, cache, r, say, builder=None):
    """One mod's layer into `cache` (its readings from H/mods/<id>/)."""
    return build_layer(cache, layer_name(r), r["kind"], r["base"], os.path.join(mod_dir(h, r["id"]), r["readings"]),
                       "mod:%s@%s:%s" % (r["id"], r["version"], r["base"]), "the mod %s (%s)" % (r["id"], r["name"]),
                       say, builder)


def setup_step(h, cache, say, only=None, builder=None):
    """The reading-row layers into `cache`, after its language layers (setup's
    new cache, or the home's own for `build`): every listed mod's. A mod whose `requires` the install lacks is skipped with a
    line naming the layers; anything else that cannot be built raises
    BuildError. Returns (built layers, skipped mod ids)."""
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
        done.append(build_one(h, cache, r, say, builder))
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
    p = s.add_parser("add", help="check a mod zip, unpack it under HOME/mods/<id>/ and list it in HOME/mods.toml")
    p.add_argument("zip")
    p.add_argument("--home", required=True)
    p.add_argument("--json", action="store_true")
    p = s.add_parser("remove", help="a mod's folder, its row and the layer built from it")
    p.add_argument("id")
    p.add_argument("--home", required=True)
    p = s.add_parser("list")
    p.add_argument("--home", required=True)
    p.add_argument("--json", action="store_true")
    p = s.add_parser("build", help="build the mods' layers into HOME/cache in place (setup does this too)")
    p.add_argument("id", nargs="?")
    p.add_argument("--home", required=True)
    p = s.add_parser("check", help="the checks of add alone")
    p.add_argument("zip")
    p.add_argument("--json", action="store_true")
    p = s.add_parser("selftest")
    p.add_argument("--keep", help="make the zips here and keep them")
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if a.cmd == "selftest":
        return selftest(os.path.abspath(a.keep) if a.keep else None)
    return {"add": cmd_add, "remove": cmd_remove, "list": cmd_list, "build": cmd_build, "check": cmd_check}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main() or 0)
