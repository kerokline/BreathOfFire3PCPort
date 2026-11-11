#!/usr/bin/env python
"""A disc-patch mod's layers: what a romhack changes, as the PC's chunks.

docs/disc-patch-round.md section 3 (issue 216). A `disc-patch` mod carries its
author's .xdelta of a disc's data track (tools/mods.py checks the zip). This is
its build step, run by `mods.py build` and by setup on the player's machine:

  1. the player's disc of the mod's `build`, from <home>/sources.toml;
  2. the patch applied to it in a scratch file (tools/vcdiff.py), and the
     result checked against mod.toml's result_sha256;
  3. every section of every EMI compared between the two discs. A changed
     section a recipe row takes from this build goes through the row's own
     transform (importer.TRANSFORMS) on both sides; where the outputs differ,
     the patched one is a chunk of the layer `mod-<id>`;
  4. tools/loc_build.py run on a HYBRID disc - the original, with a patched
     section put in only where its load address is one the PC keeps as text
     (KEPT) - the way the install's language layer was built; every chunk that
     comes out different from the install's own layer is a chunk of the layer
     `mod-<id>-<tag>`, and after it that language's names for every enemy
     table of the first layer (the engine walks a mod's layer after the
     language's, so a whole table blanks the names the language laid);
  5. both layers into <cache>/opt/, and their manifest rows returned.

Nothing about a section's format is learned here: the importer's transforms
and loc_build.py do the reading. The patched disc is never handed to either
whole - loc_build.py finds its tables in the overlays by Capcom's numbers and
refuses a disc whose numbers a hack changed (docs/romhack-mods.md 8, M1).

What is NOT carried is said, never dropped in silence: a changed sound bank
(the owner's call 5), and every changed section the PC does not keep as a DAT
chunk (the overlays' tables and code: the round's group E and after).

    python tools/disc_patch_build.py selftest      # synthetic EMIs and containers, no game data

Everything written is derived game data and stays in the player's cache
(CLAUDE.md rule 1).
"""
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)

# The load addresses whose sections the hybrid disc takes from the patched
# disc: the ones the PC keeps as a language's chunks, which loc_build.py builds.
KEPT = {
    0x80010000: "an area's script and text block",
    0x8001A000: "the shared text pool of the battle, boss, field and first modules",
    0x0E001000: "an area page; where words are painted on it, it is a language's",
}
ENEMY_DEST = 0x800E4000            # an area's enemy table on the disc (importer.widen_enemies)
SOUND_TYPES = (6, 7, 8, 10)        # a sound bank's header, samples and cues; a sequence
RESULT_MAX = 800 << 20             # as tools/mods.py's: a CD's data track is under 800 MiB


class Bad(Exception):
    """The mod cannot be built; one sentence for the player, naming what and why."""


def sha(b):
    return hashlib.sha256(b).hexdigest()


# ---------------------------------------------------------------- EMIs

def emi_sections(blob):
    """[(index, type, load address, bytes)] of an EMI (region_diff.emi_sections_blob's rows)."""
    if blob[8:16] != b"MATH_TBL":
        raise ValueError("not an EMI")
    count, = struct.unpack_from("<I", blob, 0)
    out, pos = [], 0x800
    for i in range(count):
        size, dest = struct.unpack_from("<II", blob, 0x10 + 16 * i)
        kind, = struct.unpack_from("<H", blob, 0x1C + 16 * i)
        out.append((i, kind, dest, blob[pos:pos + size]))
        pos += ((size + 0x7FF) >> 11) * 0x800
    return out


def hybrid_emi(ob, nb, kept=KEPT):
    """The original EMI `ob` with the sections of the patched one `nb` put in
    where they differ and their load address is in `kept`: (bytes, [section
    index taken]). The header's sizes follow; nothing else of it changes."""
    so, sn = emi_sections(ob), emi_sections(nb)
    if len(so) != len(sn):
        raise Bad("the patch changes how many sections an EMI has (%d to %d), which this release cannot carry"
                  % (len(so), len(sn)))
    head, body, took = bytearray(ob[:0x800]), [], []
    for (i, kind, dest, da), (_, kind2, dest2, db) in zip(so, sn):
        take = dest in kept and da != db
        if take and (kind2 != kind or dest2 != dest):
            raise Bad("the patch changes the type or the load address of section %d of an EMI" % i)
        sec = db if take else da
        if take:
            took.append(i)
        struct.pack_into("<I", head, 0x10 + 16 * i, len(sec))
        body.append(sec + bytes(-len(sec) % 0x800))
    return bytes(head) + b"".join(body), took


class HybridDisc:
    """The original disc's file tree; an EMI both discs hold is hybrid_emi's.
    What loc_build.py is given in place of a disc (its `disc_opener`)."""

    def __init__(self, orig, patched, kept=KEPT):
        self.orig, self.patched, self.kept = orig, patched, kept
        self.files = orig.files
        self.took = {}

    def read(self, name):
        key = name.upper().replace("\\", "/")
        ob = self.orig.read(key)
        if not key.endswith(".EMI") or key not in self.patched.files or ob[8:16] != b"MATH_TBL":
            return ob
        nb = self.patched.read(key)
        if nb == ob:
            return ob
        try:
            blob, took = hybrid_emi(ob, nb, self.kept)
        except Bad as e:
            raise Bad("%s: %s" % (key, e))
        if took:
            self.took[key] = took
        return blob

    def find(self, basename):
        return self.orig.find(basename)

    def sector(self, lba):
        return self.orig.sector(lba)

    def extent(self, lba, size):
        return self.orig.extent(lba, size)


# ---------------------------------------------------------------- the two discs, section by section

def changed_sections(orig, patched):
    """{(EMI key, section index): (type, load address, original bytes, patched
    bytes)} for every section that differs between two region_diff.Build
    discs. Bad when the patched disc lacks an EMI or changes a section list."""
    out = {}
    for key in sorted(orig.emis):
        if key not in patched.emis:
            raise Bad("the patched disc has no %s, which this release cannot carry" % key)
        ob, nb = orig.disc.read(orig.emis[key]), patched.disc.read(patched.emis[key])
        if ob == nb:
            continue
        so, sn = emi_sections(ob), emi_sections(nb)
        if len(so) != len(sn):
            raise Bad("%s: the patch changes how many sections the file has (%d to %d), which this release "
                      "cannot carry" % (key, len(so), len(sn)))
        for (i, kind, dest, da), (_, kind2, dest2, db) in zip(so, sn):
            if da == db and kind == kind2 and dest == dest2:
                continue
            if kind != kind2 or dest != dest2:
                raise Bad("%s section %d: the patch changes its type or load address, which this release cannot "
                          "carry" % (key, i))
            out[(key, i)] = (kind, dest, da, db)
    return out


def collect_base(files, build, changed, transforms, sources_of):
    """The first layer's chunks: {DAT name: [(kind, tag, payload)]} in the
    recipe's slot order, and the set of (EMI key, section) they came from.
    `files`: the recipe's [{name, chunks}]. A changed section a row takes from
    `build` goes through the row's transform on both sides; where the outputs
    differ the patched one is the chunk."""
    out, used = {}, set()
    for f in files:
        for ch in f["chunks"]:
            for b, how, key, sec in sources_of(ch):
                if b != build or (key, sec) not in changed:
                    continue
                if how not in transforms:
                    raise Bad("%s: the recipe's step %r for %s section %d is not one this release has"
                              % (f["name"], how, key, sec))
                _, _, da, db = changed[(key, sec)]
                try:
                    o, n = transforms[how](da), transforms[how](db)
                except Exception as e:      # a transform's own refusal of bytes that are not its shape
                    raise Bad("%s section %d: the patched section is not what the step %r reads (%s)"
                              % (key, sec, how, e))
                if o is None:
                    continue                # not this row's shape on the original either: another row's section
                if n is None:
                    raise Bad("%s section %d: the patched section is not the shape the step %r reads (its length "
                              "changed), so %s cannot take it" % (key, sec, how, f["name"]))
                used.add((key, sec))
                if n != o and (ch["kind"], ch["tag"], n) not in out.get(f["name"], []):
                    out.setdefault(f["name"], []).append((ch["kind"], ch["tag"], n))
    return out, used


# ---------------------------------------------------------------- DAT containers

def read_chunks(blob, name):
    """[(kind, tag, fourth word, payload)] of a DAT container, in order."""
    out, pos = [], 0
    while pos < len(blob):
        if pos + 16 > len(blob):
            raise Bad("%s: a chunk header runs past the file" % name)
        kind, tag, size, word = struct.unpack_from("<4I", blob, pos)
        if pos + 16 + size > len(blob):
            raise Bad("%s: a chunk at 0x%X runs past the file" % (name, pos))
        out.append((kind, tag, word, blob[pos + 16:pos + 16 + size]))
        pos += 16 + size
    return out


def container(chunks):
    return b"".join(struct.pack("<4I", kind, tag, len(body), word) + body for kind, tag, word, body in chunks)


def diff_layer(old_dir, new_dir):
    """{DAT name: [(kind, tag, word, payload)]}: the chunks of `new_dir`'s
    containers that `old_dir`'s do not hold the same, in the new container's
    order. A corrected copy (kind 19) that differs brings the kind-0 block of
    its tag before it, since the engine takes it only after that block. Bad
    for a chunk the old layer has and the new one lacks: a layer over the
    language's cannot take a chunk away."""
    out = {}
    for name in sorted(os.listdir(new_dir)):
        if not name.upper().endswith(".DAT"):
            continue
        with open(os.path.join(new_dir, name), "rb") as f:
            new = read_chunks(f.read(), name)
        p = os.path.join(old_dir, name)
        old = []
        if os.path.isfile(p):
            with open(p, "rb") as f:
                old = read_chunks(f.read(), name)
        had = {(k, t): (w, b) for k, t, w, b in old}
        got = {(k, t) for k, t, _, _ in new}
        gone = sorted(set(had) - got)
        if gone:
            raise Bad("%s: with the patch the text builder writes no chunk of kind %d at 0x%X, and a mod's layer "
                      "cannot take a chunk away" % ((name,) + gone[0]))
        differ = {(k, t) for k, t, w, b in new if had.get((k, t)) != (w, b)}
        for k, t in list(differ):
            if k == 19:
                differ.add((0, t))
        mine = [c for c in new if (c[0], c[1]) in differ]
        if mine:
            out[name] = mine
    for name in sorted(os.listdir(old_dir)) if os.path.isdir(old_dir) else []:
        if name.upper().endswith(".DAT") and not os.path.isfile(os.path.join(new_dir, name)):
            raise Bad("%s: with the patch the text builder writes no such file, and a mod's layer cannot take a "
                      "file away" % name)
    return out


def write_layer(root, layer, files, where):
    """A layer's containers into root/<layer>/dat/; the manifest's rows
    [(name, chunk index, where, sha256)]."""
    d = os.path.join(root, layer, "dat")
    os.makedirs(d)
    rows = []
    for name in sorted(files):
        with open(os.path.join(d, name), "wb") as f:
            f.write(container(files[name]))
        rows += [(name, i, where, sha(body)) for i, (_, _, _, body) in enumerate(files[name])]
    return rows


# ---------------------------------------------------------------- the install

def source_row(home, build):
    import tomllib
    p = os.path.join(home, "sources.toml")
    if os.path.isfile(p):
        with open(p, "rb") as f:
            rows = tomllib.load(f).get("source", [])
        for r in rows:
            if r.get("build") == build:
                return r, rows
    raise Bad("it needs the disc %s, and the install's source list has none (add it on Data, Sources)" % build)


def loc_mode(cache, tag, build):
    """How the install's language layer was built: 'cache' (from the disc
    alone) or 'game' (against the PC's files), as the manifest records it."""
    p = os.path.join(cache, "manifest.toml")
    if not os.path.isfile(p):
        raise Bad("the cache has no manifest.toml: build the install first (Data, Sources)")
    with open(p, encoding="utf-8") as f:
        text = f.read()
    if '"loc/%s", "%s:loc_build-disc-only"' % (tag, build) in text:
        return "cache"
    if '"loc/%s", "%s:loc_build"' % (tag, build) in text:
        return "game"
    raise Bad("the install's %s text was not built from the disc %s, so a patch of that disc has nothing to lie "
              "over (build the install with that disc)" % (tag, build))


def _link_dir(src, dst):
    """`dst` as a view of the folder `src`: a junction, else a symbolic link, else a copy."""
    try:
        import _winapi
        _winapi.CreateJunction(src, dst)
        return
    except (ImportError, AttributeError, OSError):
        pass
    try:
        os.symlink(src, dst, target_is_directory=True)
        return
    except OSError:
        pass
    shutil.copytree(src, dst)


def _unlink_dir(dst):
    try:
        os.rmdir(dst)           # a junction or a link: the name alone
    except OSError:
        shutil.rmtree(dst, ignore_errors=True)


def run_loc(mode, tag, orig_path, patched_path, cache, pc_rows, work, enemy_names, say):
    """loc_build.py on the hybrid disc, the way the install's layer was built:
    the folder of its containers, and the folder of the names containers for
    `enemy_names` (DAT names). A child process: loc_build.py keeps a disc's
    state in its module."""
    import importer
    names_dir = os.path.join(work, "names")
    os.makedirs(names_dir)
    jobs = os.path.join(work, "names.json")
    with open(jobs, "w") as f:
        json.dump(sorted(enemy_names), f)
    cmd = [sys.executable, os.path.abspath(__file__), "_loc", orig_path, patched_path, jobs, names_dir,
           "all", "--disc", orig_path, "--lang", tag]
    view = None
    try:
        if mode == "cache":
            scratch = os.path.join(work, "cache")
            os.makedirs(scratch)
            view = os.path.join(scratch, "base")
            _link_dir(os.path.join(cache, "base"), view)
            cmd += ["--cache", scratch]
            out = os.path.join(scratch, "loc", tag, "dat")
        else:
            pc = next((r for r in pc_rows if r.get("kind") == "pc-folder"), None)
            if pc is None:
                raise Bad("the install's %s text was built against the PC game, and the source list has no PC game "
                          "folder to build it against again" % tag)
            exe = os.path.join(pc["path"], "BOF3.exe")
            if not os.path.isfile(exe):
                raise Bad("%s is not there" % exe)
            game = os.path.join(work, "game")
            os.makedirs(os.path.join(game, "DAT"))
            importer._place(exe, os.path.join(game, "BOF3.exe"))
            for name in importer._manifest_rows("fixtures/pc-zh.DAT.files.tsv"):
                importer._place(os.path.join(pc["path"], "DAT", name), os.path.join(game, "DAT", name))
            cmd += ["--game", game]
            out = os.path.join(work, "loc")
        p = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
        for line in (p.stdout + p.stderr).splitlines():
            say("  " + line, quiet=True)
        if p.returncode != 0:
            last = next((l for l in reversed((p.stderr + p.stdout).splitlines()) if l.strip()), "no output")
            raise Bad("the text builder refused the patched text (exit %d): %s" % (p.returncode, last.strip()))
        if mode == "game":
            os.makedirs(out)
            d = os.path.join(work, "game", "DAT")
            for f in sorted(os.listdir(d)):
                if f.startswith(tag + "."):
                    shutil.move(os.path.join(d, f), os.path.join(out, f[len(tag) + 1:]))
    finally:
        if view:
            _unlink_dir(view)
    if not os.path.isdir(out) or not os.listdir(out):
        raise Bad("the text builder wrote nothing")
    return out, names_dir


def cmd_loc(argv):
    """The child of run_loc: loc_build.py with the hybrid disc, then the names
    of the enemy tables the patch changed, made by loc_build's own converter
    from the patched disc while its language is still set up."""
    orig_path, patched_path, jobs, names_dir = argv[:4]
    import importer
    import loc_build
    import psx_disc
    orig, patched = psx_disc.Disc(orig_path), psx_disc.Disc(patched_path)
    loc_build.disc_opener = lambda path: HybridDisc(orig, patched)
    sys.argv = ["loc_build.py"] + argv[4:]
    try:
        rc = loc_build.main()
    except Bad as e:
        print("disc_patch_build: %s" % e)
        return 1
    if rc:
        return rc
    with open(jobs) as f:
        wanted = json.load(f)
    for name in wanted:
        found = patched.find(name[:-4] + ".EMI")
        if not found:
            print("disc_patch_build: the patched disc has no EMI for %s" % name)
            return 1
        secs = [s for dest, s in loc_build.emi_sections(patched.read(found[0])) if dest == ENEMY_DEST]
        if len(secs) != 1:
            print("disc_patch_build: %s holds %d enemy tables" % (found[0], len(secs)))
            return 1
        base = importer.widen_enemies(secs[0])
        if base is None:
            print("disc_patch_build: %s: the patched enemy table is not the table's length" % found[0])
            return 1
        try:
            chunks, _ = loc_build.convert_enemy_names(base, secs[0])
        except ValueError as e:
            print("disc_patch_build: %s: %s" % (found[0], e))
            return 1
        with open(os.path.join(names_dir, name), "wb") as f:
            f.write(container([(kind, tag, 0, body) for kind, tag, body in chunks]))
    print("disc_patch_build: names for %d enemy tables" % len(wanted))
    return 0


# ---------------------------------------------------------------- the build

def layer_names(row, tag):
    return "mod-%s" % row["id"], "mod-%s-%s" % (row["id"], tag)


def build(home, cache, mod_folder, row, tag, where, say):
    """Both layers of the disc-patch mod `row` into `cache`:
    {layer: [(name, chunk index, where, sha256)]}, a layer with no chunk left
    out and its old folder removed. Bad, with nothing changed in the cache,
    when the disc, the patch or what the patch makes is not what mod.toml
    names, or a builder refuses a patched section."""
    import importer
    import psx_disc
    import vcdiff
    build_id = row["build"]
    src, rows = source_row(home, build_id)
    work = os.path.join(home, "mods", ".build-" + row["id"])
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    unzipped = None
    try:
        disc_path = src["path"]
        if disc_path.lower().endswith(".zip"):
            import player_sources
            say("Unpacking %s" % os.path.basename(disc_path))
            try:
                disc_path, unzipped = player_sources.unpack_zip(disc_path, work)
            except (ValueError, OSError) as e:
                raise Bad("the disc %s cannot be unpacked: %s" % (src["path"], e))
        if not os.path.isfile(disc_path):
            raise Bad("the disc %s is not there (Data, Sources lists it)" % disc_path)
        try:
            disc = psx_disc.Disc(disc_path)
        except SystemExit as e:
            raise Bad("the disc %s cannot be read: %s" % (disc_path, e))
        if not disc.raw:
            raise Bad("the disc %s is not a raw image (2352 bytes a sector), and the patch is of a raw data track"
                      % disc_path)
        track = disc.f.name
        patched_path = os.path.join(work, "patched.bin")
        say("Applying %s to a scratch copy of the disc (about %d MiB, removed when the build ends)"
            % (row["patch"], os.path.getsize(track) >> 20))
        try:
            size, got = vcdiff.apply_file(os.path.join(mod_folder, row["patch"]), track, patched_path, RESULT_MAX)
        except vcdiff.PatchError as e:
            raise Bad("the patch does not apply to the disc %s: %s" % (os.path.basename(disc_path), e))
        except OSError as e:
            raise Bad("the scratch copy could not be written (%s)" % e)
        if got != row["result_sha256"]:
            raise Bad("the patch applied, and what it made is not the disc mod.toml's result_sha256 names; "
                      "nothing is built from it")
        import region_diff
        try:
            orig, patched = region_diff.Build(build_id, disc_path), region_diff.Build(build_id, patched_path)
        except SystemExit as e:
            raise Bad("the patched disc cannot be read: %s" % e)
        say("Comparing the two discs")
        changed = changed_sections(orig, patched)
        base, used = collect_base(importer.load_recipe()["file"], build_id, changed, importer.TRANSFORMS,
                                  importer.sources_of)
        sound = sorted(k for k, v in changed.items() if v[0] in SOUND_TYPES)
        text_secs = sorted(k for k, v in changed.items() if v[1] in KEPT)
        other = sorted(k for k, v in changed.items() if k not in used and v[0] not in SOUND_TYPES and v[1] not in KEPT)
        enemy = sorted(n for n, chunks in base.items() if any(t == importer.ENEMY_TAG for _, t, _ in chunks))

        mode = loc_mode(cache, tag, build_id)
        say("Building the %s text with the patch's text (the text builder, about two minutes)" % tag)
        orig.disc.f.close()
        patched.disc.f.close()
        new_loc, names_dir = run_loc(mode, tag, disc_path, patched_path, cache, rows, work, enemy, say)
        text = diff_layer(os.path.join(cache, "loc", tag, "dat"), new_loc)
        for name in enemy:
            with open(os.path.join(names_dir, name), "rb") as f:
                text.setdefault(name, []).extend(read_chunks(f.read(), name))

        first, second = layer_names(row, tag)
        staged = os.path.join(work, "layers")
        out = {}
        if base:
            out[first] = write_layer(staged, first, {n: [(k, t, 0, b) for k, t, b in c] for n, c in base.items()}, where)
        if text:
            out[second] = write_layer(staged, second, text, where)
        if not out:
            raise Bad("the patch changes nothing this release can carry (%d sections changed, none of them one the "
                      "PC keeps as a chunk)" % len(changed))
        opt = os.path.join(cache, "opt")
        os.makedirs(opt, exist_ok=True)
        for layer in (first, second):
            shutil.rmtree(os.path.join(opt, layer), ignore_errors=True)
            if layer in out:
                os.rename(os.path.join(staged, layer), os.path.join(opt, layer))
            else:
                out[layer] = []
        say("  %s: %d chunks in %d files (enemy tables and pictures, for any text)"
            % (first, len(out[first]), len(base)))
        say("  %s: %d chunks in %d files (%s text, and its names for %d enemy tables)"
            % (second, len(out[second]), len(text), tag, len(enemy)))
        if sound:
            say("  not carried: %d changed sound sections in %d files (a changed sound bank is not built)"
                % (len(sound), len({k for k, _ in sound})))
        if other:
            say("  not carried: %d changed sections in %d files that the PC does not keep as data chunks (the "
                "overlays' tables and code)" % (len(other), len({k for k, _ in other})))
        say("  %d text and page sections went through the text builder" % len(text_secs), quiet=True)
        return out
    finally:
        shutil.rmtree(work, ignore_errors=True)


# ---------------------------------------------------------------- the self-test

def _emi(sections):
    """A synthetic EMI: [(type, load address, bytes)]."""
    head = bytearray(0x800)
    struct.pack_into("<I", head, 0, len(sections))
    head[8:16] = b"MATH_TBL"
    body = b""
    for i, (kind, dest, data) in enumerate(sections):
        struct.pack_into("<II", head, 0x10 + 16 * i, len(data), dest)
        struct.pack_into("<H", head, 0x1C + 16 * i, kind)
        body += data + bytes(-len(data) % 0x800)
    return bytes(head) + body


class _Disc:
    def __init__(self, files):
        self.files = {k: (0, len(v)) for k, v in files.items()}
        self._b = files

    def read(self, name):
        return self._b[name.upper()]

    def find(self, basename):
        return [k for k in self._b if k.rsplit("/", 1)[-1] == basename.upper()]


class _Build:
    def __init__(self, files):
        self.disc = _Disc(files)
        self.emis = {k[4:]: k for k in files if k.endswith(".EMI")}


def selftest():
    import tempfile
    fails = []

    def expect(cond, what):
        print("%-78s %s" % (what, "ok" if cond else "FAIL"))
        if not cond:
            fails.append(what)

    def refused(fn, word, what):
        try:
            fn()
            expect(False, what + ": refused")
        except Bad as e:
            expect(word in str(e), "%s: refused (%s)" % (what, str(e)[:44]))

    # the bytes here are this test's own; no game data
    text, text2 = b"T" * 300, b"t" * 5000
    table, table2 = b"E" * 40, b"e" * 40
    code, code2 = b"C" * 64, b"c" * 64
    bank, bank2 = b"S" * 100, b"s" * 100
    a = _emi([(6, 2, bank), (0, 0x800E4000, table), (0, 0x80010000, text), (0, 0x801F2C00, code)])
    b = _emi([(6, 2, bank2), (0, 0x800E4000, table2), (0, 0x80010000, text2), (0, 0x801F2C00, code2)])
    blob, took = hybrid_emi(a, b)
    secs = emi_sections(blob)
    expect(took == [2] and [s[3] for s in secs] == [bank, table, text2, code] and len(blob) == 0x800 * 7,
           "hybrid EMI: the patched text section alone, resized, the rest the original's")
    expect(hybrid_emi(a, a) == (a, []), "hybrid EMI: an EMI the patch leaves is itself")
    refused(lambda: hybrid_emi(a, _emi([(6, 2, bank)])), "how many sections", "hybrid EMI: a section list that changed")
    refused(lambda: hybrid_emi(a, _emi([(6, 2, bank), (0, 0x800E4000, table), (3, 0x80010000, text2),
                                        (0, 0x801F2C00, code)])), "type or the load address",
            "hybrid EMI: a kept section whose type changed")

    o = _Build({"BIN/W/AREA001.EMI": a, "BIN/W/AREA002.EMI": a, "SYSTEM.CNF": b"x"})
    n = _Build({"BIN/W/AREA001.EMI": b, "BIN/W/AREA002.EMI": a, "SYSTEM.CNF": b"y"})
    ch = changed_sections(o, n)
    expect(sorted(ch) == [("W/AREA001.EMI", i) for i in range(4)] and ch[("W/AREA001.EMI", 1)][2:] == (table, table2),
           "changed sections: the four of the one EMI the patch touched")
    refused(lambda: changed_sections(o, _Build({"BIN/W/AREA001.EMI": b})), "has no W/AREA002.EMI",
            "changed sections: an EMI the patched disc lacks")
    h = HybridDisc(o.disc, n.disc)
    expect(h.read("bin/w/area001.emi") == blob and h.read("BIN/W/AREA002.EMI") == a and h.read("SYSTEM.CNF") == b"x"
           and h.took == {"BIN/W/AREA001.EMI": [2]}, "hybrid disc: the hybrid EMI, an unchanged EMI, a file as the original's")

    recipe = [{"name": "AREA001.DAT", "chunks": [
        {"kind": 2, "tag": 2, "class": "bank"},
        {"kind": 0, "tag": 0xC2000, "emi": "W/AREA001.EMI#1", "on": ["psx-xx:upper", "psx-yy:upper"]},
        {"kind": 0, "tag": 0x9000, "emi": "W/AREA001.EMI#3", "on": ["psx-yy:copy"]}]},
        {"name": "AREA002.DAT", "chunks": [{"kind": 0, "tag": 0xC2000, "emi": "W/AREA002.EMI#1", "on": ["psx-xx:upper"]}]}]

    def sources_of(c):
        if "emi" not in c:
            return []
        key, sec = c["emi"].rsplit("#", 1)
        return [tuple(x.split(":")) + (key, int(sec)) for x in c["on"]]

    tr = {"upper": lambda s: s.upper(), "copy": lambda s: s, "fixed": lambda s: s if len(s) == 40 else None}
    expect(collect_base(recipe, "psx-zz", ch, tr, sources_of) == ({}, set()),
           "collect: a build no row names gives nothing")
    base, used = collect_base(recipe, "psx-xx", ch, tr, sources_of)
    expect(base == {} and used == {("W/AREA001.EMI", 1)},
           "collect: a change the transform takes away is no chunk (the outputs are equal)")
    base, used = collect_base(recipe, "psx-yy", ch, tr, sources_of)
    expect(base == {"AREA001.DAT": [(0, 0x9000, code2)]} and used == {("W/AREA001.EMI", 1), ("W/AREA001.EMI", 3)},
           "collect: the patched section through the row's transform, in the recipe's order")
    recipe[0]["chunks"][1]["on"] = ["psx-xx:fixed"]
    ch2 = dict(ch)
    ch2[("W/AREA001.EMI", 1)] = (0, 0x800E4000, table, table2 + b"!")
    refused(lambda: collect_base(recipe, "psx-xx", ch2, tr, sources_of), "its length changed",
            "collect: a patched section the transform does not read")
    recipe[0]["chunks"][1]["on"] = ["psx-xx:gone"]
    refused(lambda: collect_base(recipe, "psx-xx", ch, tr, sources_of), "is not one this release has",
            "collect: a recipe step this release lacks")

    root = tempfile.mkdtemp(prefix="bof3x_dpb_")
    try:
        old, new = os.path.join(root, "old"), os.path.join(root, "new")
        os.makedirs(old)
        os.makedirs(new)

        def put(d, name, chunks):
            with open(os.path.join(d, name), "wb") as f:
                f.write(container(chunks))

        put(old, "A.DAT", [(0, 0, 0, b"block"), (19, 0, 1, b"fixed"), (0, 0xC2048, 0, b"name")])
        put(new, "A.DAT", [(0, 0, 0, b"block"), (19, 0, 1, b"FIXED"), (0, 0xC2048, 0, b"name")])
        put(old, "B.DAT", [(0, 0x4000, 0, b"pool"), (1, 0xE001000, 0, b"page")])
        put(new, "B.DAT", [(0, 0x4000, 0, b"a longer pool"), (1, 0xE001000, 0, b"page"), (5, 7, 0, b"new")])
        put(old, "C.DAT", [(0, 0, 0, b"same")])
        put(new, "C.DAT", [(0, 0, 0, b"same")])
        d = diff_layer(old, new)
        expect(d == {"A.DAT": [(0, 0, 0, b"block"), (19, 0, 1, b"FIXED")],
                     "B.DAT": [(0, 0x4000, 0, b"a longer pool"), (5, 7, 0, b"new")]},
               "diff: the changed and the new chunks; a changed corrected copy brings its block")
        put(new, "D.DAT", [(0, 0, 0, b"only here")])
        expect(diff_layer(old, new)["D.DAT"] == [(0, 0, 0, b"only here")], "diff: a file the old layer lacks, whole")
        put(new, "A.DAT", [(0, 0, 0, b"block")])
        refused(lambda: diff_layer(old, new), "cannot take a chunk away", "diff: a chunk the patch removes")
        put(new, "A.DAT", [(0, 0, 0, b"block"), (19, 0, 1, b"fixed"), (0, 0xC2048, 0, b"name")])
        os.remove(os.path.join(new, "C.DAT"))
        refused(lambda: diff_layer(old, new), "cannot take a file away", "diff: a file the patch removes")
        with open(os.path.join(new, "E.DAT"), "wb") as f:
            f.write(b"\0" * 20)
        refused(lambda: diff_layer(old, new), "runs past the file", "diff: a damaged container")

        rows = write_layer(os.path.join(root, "out"), "mod-x", d, "mod:x@1:psx-xx")
        with open(os.path.join(root, "out", "mod-x", "dat", "A.DAT"), "rb") as f:
            back = read_chunks(f.read(), "A.DAT")
        expect(back == d["A.DAT"] and [r[:3] for r in rows] == [("A.DAT", 0, "mod:x@1:psx-xx"), ("A.DAT", 1, "mod:x@1:psx-xx"),
                                                                ("B.DAT", 0, "mod:x@1:psx-xx"), ("B.DAT", 1, "mod:x@1:psx-xx")]
               and rows[1][3] == sha(b"FIXED"), "write: the containers read back, a manifest row a chunk")

        home, cache = os.path.join(root, "home"), os.path.join(root, "home", "cache")
        os.makedirs(cache)
        refused(lambda: source_row(home, "psx-xx"), "source list has none", "the install: no disc of the build")
        with open(os.path.join(home, "sources.toml"), "w") as f:
            f.write('[[source]]\npath = "nowhere.cue"\nbuild = "psx-xx"\nkind = "psx"\n')
        expect(source_row(home, "psx-xx")[0]["path"] == "nowhere.cue", "the install: the disc's row")
        refused(lambda: loc_mode(cache, "xx-XX", "psx-xx"), "no manifest.toml", "the install: no manifest")
        with open(os.path.join(cache, "manifest.toml"), "w") as f:
            f.write('[cache]\nassets = [\n  ["A.DAT", "loc/xx-XX", "psx-xx:loc_build-disc-only", "00"],\n'
                    '  ["A.DAT", "loc/yy-YY", "psx-yy:loc_build", "00"],\n]\n')
        expect(loc_mode(cache, "xx-XX", "psx-xx") == "cache" and loc_mode(cache, "yy-YY", "psx-yy") == "game",
               "the install: how each language layer was built, from the manifest")
        refused(lambda: loc_mode(cache, "xx-XX", "psx-yy"), "was not built from the disc",
                "the install: text built from another disc")
        row = {"id": "x", "build": "psx-xx", "patch": "p.xdelta", "result_sha256": "0" * 64}
        refused(lambda: build(home, cache, home, row, "xx-XX", "w", lambda t, quiet=False: None), "is not there",
                "build: a disc that is listed and gone")
        expect(not os.path.exists(os.path.join(home, "mods", ".build-x")), "  and no scratch folder left")
    finally:
        shutil.rmtree(root, ignore_errors=True)
    print("disc_patch_build selftest: %d failure(s)" % len(fails))
    return 1 if fails else 0


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "_loc":
        return cmd_loc(sys.argv[2:])
    if len(sys.argv) == 2 and sys.argv[1] == "selftest":
        return selftest()
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main() or 0)
