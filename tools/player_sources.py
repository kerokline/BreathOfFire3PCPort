#!/usr/bin/env python
"""The player's sources: identify one for the launcher, keep the list, set up an install.

docs/install-sources.md. tools/importer.py's `identify --json`, `sources` and
`setup` commands are this module's; the launcher's Sources page runs them
with the bundled interpreter (tools/make_release.py) and reads their stdout.

What a source is: the 2001 PC port's game folder (BOF3.exe and DAT\\), a
PlayStation disc as .cue/.bin, a PSP disc as .iso, or a .zip holding any of
those. Each is identified against fixtures.toml exactly as importer.py
identify does (a catalogued per-file manifest whole, or a catalogued file's
sha256): an unknown or modified source imports nothing, and the player is
told why in one sentence. No game data is read into the repository; every
answer is a hash comparison.
"""
import contextlib
import datetime
import hashlib
import json
import os
import re
import shutil
import sys
import time
import uuid
import zipfile

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import bof3x_home   # noqa: E402

FFMPEG_LINK = "https://ffmpeg.org/download.html#build-windows"

# The short text the launcher shows beside a source.
LABELS = {
    "pc-zh": "PC, the 2001 Chinese release",
    "psx-jp": "PlayStation, Japan",
    "psx-us": "PlayStation, USA",
    "psx-eu-en": "PlayStation, Europe (English)",
    "psx-fr": "PlayStation, France",
    "psx-de": "PlayStation, Germany",
    "psp-jp": "PSP, Japan",
    "psp-eu": "PSP, Europe",
}
# The same, as "the ... disc" in a sentence.
DISC_NAMES = {
    "psx-jp": "Japanese PlayStation", "psx-us": "USA PlayStation", "psx-eu-en": "European (English) PlayStation",
    "psx-fr": "French PlayStation", "psx-de": "German PlayStation", "psp-jp": "Japanese PSP", "psp-eu": "European PSP",
}
# The build's preference by role (docs/install-sources.md section 3): the PC
# install first, whose chunks are the recipe's target; PlayStation discs next;
# a PSP disc last, where it fills only what the others leave (importer.py
# resolve_preset's rule for a PSP disc beside a preset).
ROLE_ORDER = {"pc-folder": 0, "pc-exe": 0, "psx": 1, "psp": 2}
SHIPPED_DAT = r"[A-Z0-9_]+\.DAT"
CD_EXT = (".cue", ".bin", ".iso")
# MCI fails to open a movie whose full path is around 160 characters
# (docs/disc-only-install.md section 1, docs/fmv-import.md).
MCI_PATH_LIMIT = 150


def _imp():
    import importer
    return importer


def result(path, ok=False, build=None, kind=None, label=None, problem=None):
    return {"path": path, "ok": ok, "build": build, "kind": kind, "label": label, "problem": problem}


def _sha_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest()


def _fixtures():
    return _imp()._fixtures()


def _child(folder, name, want_dir):
    """The entry of `folder` named `name` in any case, or None."""
    try:
        for e in os.listdir(folder):
            p = os.path.join(folder, e)
            if e.lower() == name.lower() and os.path.isdir(p) == want_dir:
                return p
    except OSError:
        pass
    return None


def _catalogue_diff(have, want):
    """How many of the catalogued files are missing or differ, and how many are extra."""
    bad = sum(1 for n, v in want.items() if have.get(n) != v)
    return bad, len(set(have) - set(want))


# ---------------------------------------------------------------- identify

def identify(path, tmp=None):
    """One identify-shaped dict for `path` (docs/install-sources.md section 4)."""
    shown = path
    try:
        if not os.path.exists(path):
            return result(shown, problem="There is nothing at this path.")
        if os.path.isdir(path):
            return identify_folder(path, shown)
        ext = os.path.splitext(path)[1].lower()
        with open(path, "rb") as f:
            magic = f.read(8)
        if ext == ".chd" or magic.startswith(b"MComprHD"):
            return result(shown, kind=None, problem="CHD images are not supported yet; convert it back to .cue/.bin "
                          "(chdman extractcd) and add the .cue.")
        if ext in (".cso", ".zso") or magic[:4] in (b"CISO", b"ZISO"):
            return result(shown, problem="Compressed PSP images (.cso) are not supported yet; convert it back to a "
                          "plain .iso and add that.")
        if ext in (".7z", ".rar", ".gz", ".xz", ".bz2", ".tar"):
            return result(shown, problem="Only .zip archives are read; unpack this one and add the .cue, .iso or "
                          "game folder inside it.")
        if ext in (".ecm", ".pbp", ".mdf", ".mds", ".ccd", ".nrg"):
            return result(shown, problem="This image format is not read; use a .cue/.bin (PlayStation) or .iso "
                          "(PSP) image.")
        if ext == ".zip" or magic[:4] == b"PK\x03\x04":
            return identify_zip(path, shown, tmp)
        if ext in CD_EXT:
            return identify_disc(path, shown)
        return identify_file(path, shown)
    except Exception as e:      # never a traceback for the launcher: one sentence
        return result(shown, problem="This could not be read (%s)." % _one_line(e))


def _one_line(e):
    s = str(e).strip().splitlines()
    return (s[0] if s else type(e).__name__)[:300]


def identify_file(path, shown):
    """A single file by whole-file sha256 against fixtures.toml's artifacts:
    BOF3.exe is the one a player adds this way."""
    h = _sha_file(path)
    for b in _fixtures()["build"]:
        for name, art in b.get("artifacts", {}).items():
            if art.get("sha256") == h:
                if (b["id"], name) == ("pc-zh", "exe"):
                    return result(shown, True, "pc-zh", "pc-exe", "PC BOF3.exe, the 2001 Chinese release")
                return result(shown, False, b["id"], None, LABELS.get(b["id"]),
                              "This file (%s) is part of a release, not a source on its own; add the whole game "
                              "folder or disc image instead." % art.get("name", name))
    if os.path.basename(path).lower() == "bof3.exe":
        return result(shown, False, None, "pc-exe", "PC BOF3.exe",
                      "This BOF3.exe is not the 2001 Chinese release's (patched, cracked or another version); "
                      "the unmodified one is needed.")
    return result(shown, problem="This is not a Breath of Fire III source: add the PC game folder, a PlayStation "
                                 ".cue/.bin, a PSP .iso, or a .zip of one.")


def identify_folder(path, shown):
    imp = _imp()
    import region_diff
    exe = _child(path, "BOF3.exe", False)
    dat = _child(path, "DAT", True)
    if exe is None:
        parent = os.path.dirname(os.path.abspath(path))
        if os.path.basename(os.path.abspath(path)).lower() == "dat" and _child(parent, "BOF3.exe", False):
            return result(shown, kind="pc-folder", problem="This is the game's DAT folder; choose the folder above "
                                                           "it, which holds BOF3.exe.")
        if dat is None:
            return result(shown, problem="This folder holds no BOF3.exe and DAT folder; for a disc image, choose "
                                         "the .cue, .iso or .zip file itself.")
        return result(shown, kind="pc-folder", problem="This folder has a DAT folder but no BOF3.exe; the PC "
                                                       "game's BOF3.exe is needed beside it.")
    if dat is None:
        return result(shown, kind="pc-folder", problem="This folder has BOF3.exe but no DAT folder; choose the "
                                                       "complete PC game folder.")
    pc = next(b for b in _fixtures()["build"] if b["id"] == "pc-zh")
    exe_ok = _sha_file(exe) == pc["artifacts"]["exe"]["sha256"]
    body = region_diff.manifest(dat, SHIPPED_DAT)[0]
    have = {n: (int(s), h) for n, s, h in (l.split("\t") for l in body.splitlines() if l)}
    want = imp._manifest_rows(pc["artifacts"]["dat_tree"]["manifest"])
    bad, _ = _catalogue_diff(have, want)
    label = LABELS["pc-zh"]
    if exe_ok and not bad:
        return result(shown, True, "pc-zh", "pc-folder", label)
    if not exe_ok and not bad:
        why = ("The DAT folder matches the 2001 Chinese release, but this BOF3.exe does not (patched, cracked or "
               "another version); the unmodified BOF3.exe is needed.")
    elif exe_ok:
        why = ("BOF3.exe is the 2001 Chinese release's, but %d of the %d files in DAT are missing or differ "
               "(an incomplete or modified copy)." % (bad, len(want)))
    else:
        why = ("Neither BOF3.exe nor the DAT folder matches the 2001 Chinese release (%d of the %d DAT files are "
               "missing or differ)." % (bad, len(want)))
    return result(shown, False, None, "pc-folder", "PC game folder", why)


def _disc_platform(d):
    """("psx" | "psp" | None, serial or None) from a disc's own files."""
    if "PSP_GAME/PARAM.SFO" in d.files:
        serial = None
        if "UMD_DATA.BIN" in d.files:
            serial = d.read("UMD_DATA.BIN").split(b"|", 1)[0].decode("latin1").strip() or None
        return "psp", serial
    if "SYSTEM.CNF" in d.files:
        m = re.search(rb"BOOT\s*=\s*cdrom:\\?([A-Z]{4})[_-](\d{3})\.(\d{2})", d.read("SYSTEM.CNF"), re.I)
        return "psx", ("%s-%s%s" % (m.group(1).decode().upper(), m.group(2).decode(), m.group(3).decode())
                       if m else None)
    return None, None


def identify_disc(path, shown):
    import psx_disc
    imp = _imp()
    if path.lower().endswith(".cue"):
        with open(path, "r", errors="replace") as f:
            m = re.search(r'FILE\s+"([^"]+)"\s+BINARY', f.read(), re.I)
        if not m:
            return result(shown, problem="This .cue names no BINARY track.")
        if not os.path.isfile(os.path.join(os.path.dirname(os.path.abspath(path)), m.group(1))):
            return result(shown, problem="This .cue names \"%s\", which is not in the same folder." % m.group(1))
    try:
        d = psx_disc.Disc(path)
    except (SystemExit, OSError, ValueError, IndexError):
        return result(shown, problem="This is not a disc image with a readable file system (an audio track such "
                                     "as Track 2, or a damaged file); choose the .cue or the data track.")
    platform, serial = _disc_platform(d)
    cooked_psx = platform == "psx" and not d.raw
    have = {}
    for n in sorted(d.files):
        b = d.read(n)
        have[n] = (len(b), hashlib.sha256(b).hexdigest())
    best = None
    for b in _fixtures()["build"]:
        for name, art in b.get("artifacts", {}).items():
            if "manifest" not in art or art.get("match") is not None:
                continue
            want = imp._manifest_rows(art["manifest"])
            if have == want:
                best = (b, 0, len(want))
                break
            bad, extra = _catalogue_diff(have, want)
            if b.get("serial") and b["serial"] == serial and (best is None or bad < best[1]):
                best = (b, bad + extra, len(want))
        if best and best[1] == 0:
            break
    what = {"psx": "PlayStation", "psp": "PSP"}.get(platform, "disc")
    if best and best[1] == 0:
        b = best[0]
        if cooked_psx:
            return result(shown, False, b["id"], "psx", LABELS.get(b["id"]),
                          "This is a 2048-byte .iso of the %s disc: its files match the catalogued release, but the "
                          "format has lost the disc's audio and movie sectors, which the import needs. Use the "
                          ".bin/.cue (or a .zip of them)." % DISC_NAMES.get(b["id"], b["id"]))
        return result(shown, True, b["id"], b["id"].split("-", 1)[0], LABELS.get(b["id"]))
    label = "%s disc%s" % (what, ", " + serial if serial else "")
    if cooked_psx:
        return result(shown, False, None, "psx", label,
                      "This is a 2048-byte .iso of a PlayStation disc; it has lost the disc's audio and movie "
                      "sectors. Use the .bin/.cue (or a .zip of them).")
    if best:
        b, n, tot = best
        return result(shown, False, None, platform, label,
                      "This looks like the %s disc (%s), but %d of its %d files differ from the catalogued release "
                      "(a patched or damaged image?); nothing would be imported from it."
                      % (DISC_NAMES.get(b["id"], b["id"]), serial, n, tot))
    if platform is None:
        return result(shown, False, None, None, None,
                      "This disc image is neither a PlayStation nor a PSP disc of Breath of Fire III.")
    return result(shown, False, None, platform, label,
                  "This %s disc%s is not a catalogued release of Breath of Fire III (a demo or another game?); "
                  "nothing would be imported from it." % (what, " (%s)" % serial if serial else ""))


# ---------------------------------------------------------------- .zip

def _zip_members(z):
    return [i for i in z.infolist() if not i.is_dir()]


def _whole_file_match(z, info):
    """(build, artifact) when a zip member is a catalogued file by size and
    sha256 (a PSX data track or a PSP .iso) - read without unpacking - else None."""
    cands = [(b, name, art) for b in _fixtures()["build"] for name, art in b.get("artifacts", {}).items()
             if art.get("size") == info.file_size and art.get("sha256") and name in ("data_track", "iso")]
    if not cands:
        return None
    h = hashlib.sha256()
    with z.open(info) as f:
        for blk in iter(lambda: f.read(1 << 22), b""):
            h.update(blk)
    h = h.hexdigest()
    return next(((b, name) for b, name, art in cands if art["sha256"] == h), None)


def unpack_zip(path, tmp):
    """Unpack what a .zip's source needs into a new folder under `tmp`:
    (the source's path inside it, the folder to delete). A .cue and the files
    it names; else a lone .iso or .bin; else a folder holding BOF3.exe, whole.
    Raises ValueError with the player's sentence when the zip holds none."""
    z = zipfile.ZipFile(path)
    mem = _zip_members(z)
    names = {i.filename.replace("\\", "/"): i for i in mem}
    low = {n.lower(): n for n in names}
    bad = [n for n in names if n.lower().endswith((".chd", ".cso", ".7z", ".rar"))]
    cues = sorted(n for n in names if n.lower().endswith(".cue"))
    isos = sorted(n for n in names if n.lower().endswith((".iso", ".bin")))
    exes = sorted(n for n in names if n.lower().rsplit("/", 1)[-1] == "bof3.exe")
    if len(cues) + (len(isos) if not cues else 0) + len(exes) == 0:
        if bad:
            raise ValueError("This .zip holds %s, which is not supported yet; unpack it and convert to "
                             ".cue/.bin or .iso." % bad[0].rsplit("/", 1)[-1])
        raise ValueError("This .zip holds no disc image (.cue/.bin or .iso) and no game folder with BOF3.exe.")
    out = os.path.join(tmp, "zip-" + uuid.uuid4().hex[:8])
    os.makedirs(out)
    try:
        if cues:
            if len(cues) > 1:
                raise ValueError("This .zip holds %d .cue files; one disc per .zip, please." % len(cues))
            cue = cues[0]
            folder = cue.rsplit("/", 1)[0] + "/" if "/" in cue else ""
            text = z.read(names[cue]).decode("latin1")
            want = [cue]
            for f in re.findall(r'FILE\s+"([^"]+)"\s+BINARY', text, re.I)[:1]:     # the data track; the build reads no other
                n = low.get((folder + f).lower())
                if n is None:
                    raise ValueError("The .cue in this .zip names \"%s\", which the .zip does not hold." % f)
                want.append(n)
            for n in want:
                z.extract(names[n], out)
            return os.path.join(out, *cue.split("/")), out
        if exes:
            if len(exes) > 1:
                raise ValueError("This .zip holds more than one BOF3.exe; one game folder per .zip, please.")
            z.extractall(out)
            folder = exes[0].rsplit("/", 1)[0] if "/" in exes[0] else ""
            return os.path.join(out, *folder.split("/")) if folder else out, out
        if len(isos) > 1:
            raise ValueError("This .zip holds several images and no .cue; add the .cue/.bin or .iso itself.")
        z.extract(names[isos[0]], out)
        return os.path.join(out, *isos[0].split("/")), out
    except BaseException:
        shutil.rmtree(out, ignore_errors=True)
        raise


def identify_zip(path, shown, tmp):
    try:
        z = zipfile.ZipFile(path)
    except zipfile.BadZipFile:
        return result(shown, problem="This .zip cannot be read (damaged or not a zip).")
    mem = _zip_members(z)
    # The quick road: one member is a catalogued data track or PSP image,
    # byte for byte (fixtures.toml's sha256) - no unpacking.
    for info in sorted(mem, key=lambda i: -i.file_size)[:2]:
        if info.filename.lower().endswith((".bin", ".iso")):
            m = _whole_file_match(z, info)
            if m:
                b = m[0]
                return result(shown, True, b["id"], b["id"].split("-", 1)[0], LABELS.get(b["id"]))
    tmp = tmp or bof3x_home.tmp_dir()
    os.makedirs(tmp, exist_ok=True)
    try:
        inner, folder = unpack_zip(path, tmp)
    except ValueError as e:
        return result(shown, problem=str(e))
    try:
        r = identify(inner, tmp)
    finally:
        shutil.rmtree(folder, ignore_errors=True)
    r["path"] = shown
    if r["problem"]:
        r["problem"] = "Inside this .zip: " + r["problem"][0].lower() + r["problem"][1:]
    return r


# ---------------------------------------------------------------- sources.toml

def _norm(p):
    return os.path.normcase(os.path.normpath(os.path.abspath(p)))


def load_sources(h):
    import tomllib
    p = bof3x_home.sources_file(h)
    if not os.path.isfile(p):
        return []
    with open(p, "rb") as f:
        return tomllib.load(f).get("source", [])


def save_sources(h, rows):
    os.makedirs(h, exist_ok=True)
    rows = sorted(rows, key=lambda r: ROLE_ORDER.get(r["kind"], 9))       # stable: added order within a role
    lines = ["# The player's sources, in the order the build prefers them: the PC game first, then",
             "# PlayStation discs (the first added is the base), then PSP discs. Written by",
             "# tools/importer.py sources (docs/install-sources.md); change it with that command.", ""]
    for r in rows:
        lines += ["[[source]]"] + ["%s = %s" % (k, json.dumps(r[k], ensure_ascii=False))
                                   for k in ("path", "build", "kind", "label", "added")] + [""]
    p = bof3x_home.sources_file(h)
    with open(p + ".tmp", "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    os.replace(p + ".tmp", p)
    return rows


def _emit(obj, as_json):
    if as_json:
        print(json.dumps(obj, ensure_ascii=False), flush=True)
    elif obj.get("ok"):
        print("%s: %s (%s)" % (obj["path"], obj["label"], obj["build"]), flush=True)
    else:
        print("%s: not usable - %s" % (obj["path"], obj["problem"]), flush=True)


def cmd_identify_json(paths, h=None):
    rc = 0
    tmp = bof3x_home.tmp_dir(h) if h else None
    for p in paths:
        r = identify(p, tmp)
        _emit(r, True)
        rc |= not r["ok"]
    return rc


def cmd_sources(a):
    h = os.path.abspath(a.home)
    rows = load_sources(h)
    if a.action == "list":
        for r in rows:
            gone = not os.path.exists(r["path"])
            obj = result(r["path"], not gone, r.get("build"), r.get("kind"), r.get("label"),
                         "This source is no longer at this path; add it again or remove it." if gone else None)
            if a.json:
                _emit(obj, True)
            else:
                print("%d. %s - %s%s" % (rows.index(r) + 1, r.get("label"), r["path"], "  (missing)" if gone else ""))
        if not rows and not a.json:
            print("no sources listed in %s" % bof3x_home.sources_file(h))
        return 0
    if a.action == "remove":
        keep = [r for r in rows if _norm(r["path"]) != _norm(a.path)]
        if len(keep) == len(rows):
            print("not in the list: %s" % a.path, flush=True)
            return 1
        save_sources(h, keep)
        print("removed: %s" % a.path, flush=True)
        return 0
    # add
    tmp = bof3x_home.tmp_dir(h)
    if os.path.isdir(a.path) and not _pc_folder_like(a.path):
        return _add_folder(a.path, h, rows, tmp, a.json)
    r = _add_one(a.path, rows, tmp)
    _emit(r, a.json)
    if r["ok"]:
        save_sources(h, rows)
    return 0 if r["ok"] else 1


# A folder that is not the PC game's is searched for sources (the owner,
# 2026-10-15): a folder of disc images, each in a folder of its own or not.
SCAN_DEPTH = 4                         # folders below the one chosen
SCAN_EXT = (".cue", ".iso", ".zip")    # and a .bin with no .cue beside it
# The order a scan adds discs in (the owner, 2026-10-15): the PlayStation's,
# the USA disc first so that it is a disc-only install's base (section 3),
# then the PSP's, Europe before Japan. A base already in the list stays the
# base; anything else (the PC game) comes before them.
SCAN_ORDER = ("psx-us", "psx-jp", "psx-eu-en", "psx-fr", "psx-de", "psp-eu", "psp-jp")


def _pc_folder_like(path):
    """The folder is, or is meant as, the PC game's: BOF3.exe or DAT in it, or
    it is a DAT folder. identify_folder's sentences answer those."""
    return (_child(path, "BOF3.exe", False) is not None or _child(path, "DAT", True) is not None
            or os.path.basename(os.path.abspath(path)).lower() == "dat")


def scan_folder(folder, skip=()):
    """The sources a folder may hold, in a stable order: each PC game folder
    (not searched further), and each image - .cue, .iso, .zip, and a .bin with
    no .cue beside it (a .cue names its own tracks) - down to SCAN_DEPTH
    folders. Folders under `skip` (the install's own) are left out."""
    skip = [_norm(x) for x in skip if x]
    base = len(os.path.abspath(folder).rstrip("\\/").split(os.sep))
    found = []
    for root, dirs, files in os.walk(folder):
        dirs.sort(key=str.lower)
        here = _norm(root)
        if any(here == k or here.startswith(k + os.sep) for k in skip):
            dirs[:] = []
            continue
        if _child(root, "BOF3.exe", False) is not None and _child(root, "DAT", True) is not None:
            found.append(root)
            dirs[:] = []
            continue
        if len(os.path.abspath(root).split(os.sep)) - base >= SCAN_DEPTH:
            dirs[:] = []
        cue = any(f.lower().endswith(".cue") for f in files)
        for f in sorted(files, key=str.lower):
            ext = os.path.splitext(f)[1].lower()
            if ext in SCAN_EXT or (ext == ".bin" and not cue):
                found.append(os.path.join(root, f))
    return found


def _add_folder(folder, h, rows, tmp, as_json):
    """Every source in the folder that identifies as a catalogued release is
    added - the first of each release, the rest refused as its duplicates -
    and each one found says so, a line each. Exit 0 when any was added."""
    # the install's own folders, never a source (a game\ laid out from the PC copy would look like one)
    found = scan_folder(folder, [os.path.join(h, d) for d in ("game", "cache", "runtime")])
    if not found:
        _emit(result(folder, problem="This folder holds no BOF3.exe and DAT folder, and no disc image (.cue, .iso, "
                                     ".bin or .zip) in it or the %d folders below it." % SCAN_DEPTH), as_json)
        return 1
    print("Found %d possible source%s in %s; checking each against the known releases."
          % (len(found), "" if len(found) == 1 else "s", folder), flush=True)
    # Identify every one, then add in SCAN_ORDER (the rest first, as found).
    ids = [identify(path, tmp) for path in found]
    rank = lambda i: (SCAN_ORDER.index(ids[i]["build"]) + 1 if ids[i]["build"] in SCAN_ORDER else 0, i)
    added = 0
    for i in sorted(range(len(found)), key=rank):
        r = _add_one(found[i], rows, tmp, ids[i])
        _emit(r, as_json)
        if r["ok"]:
            added += 1
            save_sources(h, rows)   # each one kept as it is added
    print("Added %d of the %d found." % (added, len(found)), flush=True)
    return 0 if added else 1


def _add_one(path, rows, tmp, r=None):
    """identify (unless `r` is its answer already), then add to `rows` (not
    saved) when usable and not a release already listed: the result as `add`
    emits it."""
    r = r or identify(path, tmp)
    if r["ok"] and r["kind"] == "pc-exe":
        # A BOF3.exe chosen in a file dialog: the folder that holds it is the source.
        parent = os.path.dirname(os.path.abspath(path))
        up = identify(parent, tmp)
        if not up["ok"]:
            r = result(path, False, "pc-zh", "pc-exe", r["label"],
                       "BOF3.exe alone cannot make an install, and the folder holding it is not a complete PC "
                       "game: %s" % up["problem"][0].lower() + up["problem"][1:])
        else:
            r = up
    if r["ok"]:
        same = next((x for x in rows if _norm(x["path"]) == _norm(r["path"]) or x.get("build") == r["build"]), None)
        if same:
            r = dict(r, ok=False, problem="%s is already a source (%s); remove that one first to use this copy "
                                          "instead." % (r["label"], same["path"]))
    if not r["ok"]:
        return r
    rows.append({"path": os.path.normpath(os.path.abspath(r["path"])), "build": r["build"], "kind": r["kind"],
                 "label": r["label"], "added": datetime.datetime.now().isoformat(timespec="seconds")})
    r["path"] = os.path.normpath(os.path.abspath(r["path"]))
    return r


# ---------------------------------------------------------------- setup

class _Log:
    """The build's own lines: into setup.log, and to the console unless the
    launcher is reading progress; a few become progress updates."""

    def __init__(self, fh, echo, on_line):
        self.fh, self.echo, self.on_line, self.buf = fh, echo, on_line, ""

    def write(self, s):
        self.fh.write(s)
        self.fh.flush()
        if self.echo:
            sys.__stdout__.write(s)
        self.buf += s
        while "\n" in self.buf:
            line, self.buf = self.buf.split("\n", 1)
            self.on_line(line)
        return len(s)

    def flush(self):
        self.fh.flush()


class SetupError(Exception):
    pass


def languages_for(rows, pc):
    """The language layers setup builds: every PlayStation disc's language;
    without the PC game also every PSP disc's (a disc-only build reads a PSP
    disc's text, docs/loc-build-psp.md; beside the PC's DAT\\ loc_build reads
    PSX discs only). In the sources' order, each tag once."""
    imp = _imp()
    out = []
    for r in rows:
        if r["kind"] == "psx" or (r["kind"] == "psp" and not pc):
            t = imp.tag_of(r["build"])
            if t not in out:
                out.append(t)
    return out


def _rmtree(p):
    if os.path.lexists(p):
        shutil.rmtree(p, ignore_errors=False)


def cmd_setup(a):
    h = os.path.abspath(a.home)
    total = [0]
    done = [0]
    out = sys.stdout        # the launcher's pipe; the build's own prints are redirected away from it

    def progress(text, step=True):
        if step:
            done[0] += 1
        out.write("progress %d %d %s\n" % (min(done[0], total[0]), total[0], text))
        out.flush()

    os.makedirs(h, exist_ok=True)
    logp = os.path.join(h, "setup.log")
    with open(logp, "w", encoding="utf-8") as log:
        try:
            game, cache = run_setup(a, h, log, progress, total)
        except SetupError as e:
            print("error %s" % e, flush=True)
            log.write("error %s\n" % e)
            return 1
        except SystemExit as e:        # the importer's own refusals
            msg = _one_line(e.code if isinstance(e.code, str) else e) or "the build stopped"
            print("error %s (details in %s)" % (msg, logp), flush=True)
            log.write("error %s\n" % e)
            return 1
        except Exception as e:
            import traceback
            traceback.print_exc(file=log)
            print("error %s: %s (details in %s)" % (type(e).__name__, _one_line(e), logp), flush=True)
            return 1
    print("done game=%s cache=%s" % (game, cache), flush=True)
    return 0


class _Times:
    """setup's own steps' wall times (IDEAS I42), written to setup.log as the
    build's `time` lines are, and all of them again at the end."""

    def __init__(self, log):
        self.log, self.rows, self.start = log, [], time.perf_counter()

    @contextlib.contextmanager
    def step(self, what):
        t = time.perf_counter()
        try:
            yield
        finally:
            self.rows.append((what, time.perf_counter() - t))
            self.log.write("  time      %-28s %7.1f s\n" % self.rows[-1])

    def summary(self):
        self.log.write("setup times:\n")
        for what, secs in self.rows:
            self.log.write("  %-34s %7.1f s\n" % (what, secs))
        self.log.write("  %-34s %7.1f s\n" % ("total", time.perf_counter() - self.start))


def run_setup(a, h, log, progress, total):
    import argparse
    import at3
    imp = _imp()
    times = _Times(log)
    rows = load_sources(h)
    if not rows:
        raise SetupError("No sources are listed: add the PC game folder, a PlayStation disc or a PSP disc first.")
    for r in rows:
        if not os.path.exists(r["path"]):
            raise SetupError("A listed source is no longer there (%s): add it again or remove it." % r["path"])
        if r["kind"] == "pc-exe":
            raise SetupError("BOF3.exe alone is listed (%s): add the PC game folder that holds it instead." % r["path"])
    if a.ffmpeg:
        os.environ["BOF3X_FFMPEG"] = a.ffmpeg
    try:
        ffmpeg = at3.find_ffmpeg()
    except SystemExit:
        raise SetupError("The ffmpeg given (%s) is not there." % os.environ.get("BOF3X_FFMPEG"))
    pc = next((r for r in rows if r["kind"] == "pc-folder"), None)
    zips = [r for r in rows if r["path"].lower().endswith(".zip")]
    langs = languages_for(rows, pc)
    total[0] = len(zips) + 1 + (1 if pc else 0) + 1 + 1
    game, cache = os.path.join(h, "game"), os.path.join(h, "cache")
    new_game, new_cache = game + ".new", cache + ".new"
    tmp = os.path.join(cache, "tmp")
    echo = not a.json_progress

    def on_line(line):        # importer.py build's own lines, as it prints them
        m = re.match(r"  loc/(\S+)\s+from ", line)
        if m:
            progress("Building the data cache: language layer %s done" % m.group(1), step=False)
        elif line.startswith("  base/fmv  converting"):
            progress("Building the data cache: converting the logo movie with ffmpeg", step=False)
        elif line.startswith("  base/exe  from"):
            progress("Building the data cache: the executable's tables done", step=False)

    if len(os.path.join(game, "CAPCOM.AVI")) > MCI_PATH_LIMIT:
        progress("Warning: the install folder's path is long (%d characters to the movie); Windows may not play "
                 "the logo movie from it. A shorter folder avoids that." % len(os.path.join(game, "CAPCOM.AVI")),
                 step=False)
    psp = [r for r in rows if r["kind"] == "psp"]
    if not ffmpeg:
        lost = []
        if psp and not any(r["kind"] in ("psx", "pc-folder") for r in rows):
            lost.append("the PSP's sound effects and voices")
        if psp or any(r["kind"] == "psx" for r in rows):
            lost.append("the disc's logo movie")
        if pc:
            lost.append("the PC logo movie's repair (DIV-0001)")
        if lost:
            progress("No ffmpeg found: skipping %s. Get ffmpeg from %s and run setup again to add them."
                     % (", ".join(lost), FFMPEG_LINK), step=False)
    log.write("sources: %s\nlanguages: %s\nffmpeg: %s\n" % ([r["path"] for r in rows], langs, ffmpeg))

    for p in (new_game, new_cache):
        _rmtree(p)
    unpacked, paths = [], {}
    try:
        for r in zips:
            progress("Unpacking %s" % r["label"])
            os.makedirs(tmp, exist_ok=True)
            try:
                with times.step("unpack " + r["build"]):
                    inner, folder = unpack_zip(r["path"], tmp)
            except ValueError as e:
                raise SetupError(str(e))
            unpacked.append(folder)
            paths[r["path"]] = inner
        src = []
        for r in rows:
            p = paths.get(r["path"], r["path"])
            if r["kind"] == "pc-folder":
                src += [_child(p, "DAT", True), _child(p, "BOF3.exe", False)]
            else:
                src.append(p)
        progress("Building the data cache from %s (several minutes)" % ", ".join(r["label"] for r in rows))
        # The install's current cache: what it already made the same way is taken, not made again (IDEAS I42).
        reuse = cache if os.path.isfile(os.path.join(cache, "manifest.toml")) else None
        ns = argparse.Namespace(source=src, out=new_cache, lang=list(langs), opt=[], no_opt=[], preset=None,
                                recipe=None, opt_recipe=None, prefer=None, reuse=reuse)
        with contextlib.redirect_stdout(_Log(log, echo, on_line)), times.step("build (importer.py build)"):
            imp.cmd_build(ns)
        os.makedirs(new_game)
        if pc:
            progress("Copying the PC game's files (the original folder is not changed)")
            with contextlib.redirect_stdout(_Log(log, echo, lambda line: None)), times.step("copy the PC game"):
                copy_pc(paths.get(pc["path"], pc["path"]), new_game, ffmpeg, log, progress)
        progress("Laying out the game folder" + (" with %s" % ", ".join(langs) if langs else ""))
        ns = argparse.Namespace(cache=new_cache, game=new_game, base=not pc, lang=list(langs), opt=[], no_opt=[])
        with contextlib.redirect_stdout(_Log(log, echo, lambda line: None)), times.step("install (lay out game)"):
            imp.cmd_install(ns)
    except BaseException:
        for p in unpacked:
            shutil.rmtree(p, ignore_errors=True)
        for p in (new_game, new_cache):
            shutil.rmtree(p, ignore_errors=True)
        raise
    for p in unpacked:
        shutil.rmtree(p, ignore_errors=True)
    shutil.rmtree(tmp, ignore_errors=True)
    progress("Replacing the previous install")
    with times.step("swap into place"):
        swap(h, (("game", new_game), ("cache", new_cache)))
    times.summary()
    return game, cache


def swap(h, pairs):
    """Each new folder into its place; the previous one kept until all are
    moved, and put back if one cannot be (the game still running, say)."""
    moved = []
    try:
        for name, new in pairs:
            cur, old = os.path.join(h, name), os.path.join(h, name + ".old")
            _rmtree(old)
            if os.path.exists(cur):
                os.rename(cur, old)
            moved.append((cur, old))
            os.rename(new, cur)
    except OSError as e:
        for cur, old in reversed(moved):
            if os.path.exists(old):
                if os.path.exists(cur):
                    shutil.rmtree(cur, ignore_errors=True)
                os.rename(old, cur)
        for _, new in pairs:
            shutil.rmtree(new, ignore_errors=True)
        raise SetupError("The previous install could not be replaced, and is left as it was (is the game still "
                         "running, or a file in it open?): %s." % (e.strerror or _one_line(e)))
    for _, old in moved:
        shutil.rmtree(old, ignore_errors=True)


# DIV-0001's command (docs/media-stack-survey.md section 3): the Indeo 5 logo
# re-encoded to Cinepak, the audio stream copied.
DIV0001_ARGS = ["-c:v", "cinepak", "-c:a", "copy", "-fflags", "+bitexact", "-flags:v", "+bitexact"]


def copy_pc(src, dst, ffmpeg, log, progress):
    """The PC game's files a game folder needs, copied: BOF3.exe, the 742
    shipped DAT containers (no overlay a tool left there), BGM\\, SND\\ and the
    two movies. capcom.avi as shipped (Indeo 5) is re-encoded by DIV-0001's
    command when ffmpeg is there, else copied as it is."""
    imp = _imp()
    pc = next(b for b in imp._fixtures()["build"] if b["id"] == "pc-zh")
    shutil.copy2(_child(src, "BOF3.exe", False), os.path.join(dst, "BOF3.exe"))
    dat = _child(src, "DAT", True)
    os.makedirs(os.path.join(dst, "DAT"))
    n = 0
    for f in sorted(os.listdir(dat)):
        if re.fullmatch(SHIPPED_DAT, f.upper()) and os.path.isfile(os.path.join(dat, f)):
            shutil.copy2(os.path.join(dat, f), os.path.join(dst, "DAT", f.upper()))
            n += 1
    print("copied BOF3.exe and %d DAT files" % n)
    for d in ("BGM", "SND"):
        s = _child(src, d, True)
        if s:
            shutil.copytree(s, os.path.join(dst, d))
            print("copied %s: %d files" % (d, len(os.listdir(s))))
        else:
            os.makedirs(os.path.join(dst, d))
            print("no %s in the PC folder: left empty" % d)
    lg = _child(src, "logos.avi", False)
    if lg:
        shutil.copy2(lg, os.path.join(dst, "logos.avi"))
    cap = _child(src, "capcom.avi", False)
    if cap is None:
        print("no capcom.avi in the PC folder")
        return
    out = os.path.join(dst, "capcom.avi")
    if _sha_file(cap) == pc["artifacts"]["capcom_avi"]["sha256"] and ffmpeg:
        import subprocess
        part = out + ".part.avi"
        r = subprocess.run([ffmpeg, "-v", "error", "-y", "-i", cap] + DIV0001_ARGS + [part], capture_output=True)
        if r.returncode == 0 and os.path.isfile(part):
            os.replace(part, out)
            print("capcom.avi: re-encoded to Cinepak (DIV-0001)")
            return
        print("capcom.avi: ffmpeg could not re-encode it (%s); copied as shipped"
              % r.stderr.decode(errors="replace").strip()[-200:])
        if os.path.exists(part):
            os.remove(part)
    elif _sha_file(cap) == pc["artifacts"]["capcom_avi"]["sha256"]:
        print("capcom.avi: Indeo 5 as shipped, copied (no ffmpeg for DIV-0001's repair)")
    shutil.copy2(cap, out)
