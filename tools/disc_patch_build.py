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
  5. the overlays' data (docs/disc-patch-round.md 3b): every other changed
     section of an overlay is put back at the original's addresses - an edit
     in place taken, an address the patch tool only relinked left alone, a
     block that moved followed - and tools/exe_tables.py builds the .data
     image from the original disc and from that one. The bytes that differ
     are the mod's: a byte in a table that is a file becomes a cell of
     `mod-<id>`'s exe/tables.toml, any other a raw range of its exe/place.toml
     where the install's image lets a layer write there;
  6. both layers into <cache>/opt/, and their manifest rows returned.

Nothing about a section's format is learned here: the importer's transforms
and loc_build.py do the reading. The patched disc is never handed to either
whole - loc_build.py finds its tables in the overlays by Capcom's numbers and
refuses a disc whose numbers a hack changed (docs/romhack-mods.md 8, M1).

What is NOT carried is said, never dropped in silence: a changed sound bank
(the owner's call 5), a table that changed its length, a pointer the hack
aimed elsewhere, a byte the install's image keeps as the PC's own, and the
overlays' code, which the PC does not run (each a behaviour of ours to offer).

    python tools/disc_patch_build.py selftest      # synthetic EMIs and containers, no game data

Everything written is derived game data and stays in the player's cache
(CLAUDE.md rule 1).
"""
import collections
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
    What loc_build.py is given in place of a disc (its `disc_opener`).
    `unmoved`: an UnmovedDisc, whose overlay sections are taken in place of
    the original's - the hack's table numbers and the names beside them, at
    the original's addresses, so that loc_build.py reads the names the hack
    changed."""

    def __init__(self, orig, patched, kept=KEPT, unmoved=None):
        self.orig, self.patched, self.kept, self.unmoved = orig, patched, kept, unmoved
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
        if self.unmoved is not None:
            ob = self.unmoved.read(key)         # the same section list and sizes as the original
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


# ---------------------------------------------------------------- one section against its patched self

ANCHOR = 12          # bytes that must agree for two streams to be back in step


def edits(a, b):
    """[(a0, a1, b0, b1)]: the runs where a and b differ, found by walking both
    and, at a difference, taking the nearest place the next ANCHOR bytes agree
    again. Greedy, so not minimal, but an insertion or a deletion stays one run."""
    out = []
    i = j = 0
    na, nb = len(a), len(b)
    while i < na and j < nb:
        if a[i] == b[j]:
            while i + 4096 <= na and j + 4096 <= nb and a[i:i + 4096] == b[j:j + 4096]:
                i += 4096
                j += 4096
            while i < na and j < nb and a[i] == b[j]:
                i += 1
                j += 1
            continue
        best = None
        for da in range(0, 8192):
            if i + da + ANCHOR > na or (best is not None and da >= best[0] + best[1]):
                break
            k = b.find(a[i + da:i + da + ANCHOR], j, j + 16384)
            if k >= 0 and (best is None or da + (k - j) < best[0] + best[1]):
                best = (da, k - j)
        if best is None:
            out.append((i, na, j, nb))
            return out
        out.append((i, i + best[0], j, j + best[1]))
        i += best[0]
        j += best[1]
    if i < na or j < nb:
        out.append((i, na, j, nb))
    return out


def moved_by(runs):
    """The distances bytes of a section moved by: each running total of what
    the edit runs inserted and deleted, 0 left out."""
    out, d = set(), 0
    for a0, a1, b0, b1 in runs:
        d += (b1 - b0) - (a1 - a0)
        if d:
            out.add(d)
        if (b1 - b0) != (a1 - a0):
            out.add((b1 - b0) - (a1 - a0))
    return out


def relocation(da, db, a0, a1, b0, b1, shifts):
    """When a same-length run inside one word is only an address that follows
    moved bytes - a jump target, a 16-bit offset or a pointer word that differs
    by one of `shifts` - that distance; else None. The edit is the patch tool's
    relinking, not a change the author made by hand."""
    if not shifts or a1 - a0 != b1 - b0 or a1 - a0 > 4 or (a0 & ~3) != ((a1 - 1) & ~3) or (a0 & 3) != (b0 & 3):
        return None
    if (a0 & ~3) + 4 > len(da) or (b0 & ~3) + 4 > len(db):
        return None
    w0, = struct.unpack_from("<I", da, a0 & ~3)
    w1, = struct.unpack_from("<I", db, b0 & ~3)
    d32 = (w1 - w0) & 0xFFFFFFFF
    d32 -= (d32 & 0x80000000) << 1
    if d32 in shifts:                                   # a pointer word, or an immediate with no carry
        return d32
    if (w0 >> 26) == (w1 >> 26) and (w0 >> 26) in (2, 3):   # j, jal: a word index
        d = ((w1 - w0) & 0x3FFFFFF)
        d -= (d & 0x2000000) << 1
        if d * 4 in shifts:
            return d * 4
    if (w0 >> 16) == (w1 >> 16):                        # the low half of an address
        d = (w1 - w0) & 0xFFFF
        d -= (d & 0x8000) << 1
        if d in shifts:
            return d
    return None


def _words(a0, a1, b0):
    """A same-length run cut at the original's word boundaries: (a from, a to, b from)."""
    p = a0
    while p < a1:
        q = min(a1, (p | 3) + 1)
        yield p, q, b0 + p - a0
        p = q


def unmoved_section(da, db, fixed=()):
    """The patched section `db` at the original `da`'s addresses: (bytes of
    da's length, [(a0, a1, b0, b1) not placed], counts). `fixed`: the
    [(from, to)] offsets of the section that the PC holds as tables at
    addresses of its own.

    - An edit of the same length is taken, where it is now.
    - A word that differs only by a distance bytes moved (`relocation`) is the
      patch tool's relinking and stays the original's: the PC's pointers are
      its own.
    - A run that changes length opens a stretch that ends where the running
      shift is what it was. If the stretch lies inside one `fixed` table, or
      nothing in the section was relinked by a distance inside the stretch,
      nothing in it moved as far as any reader knows: it was rewritten in
      place (a record with new text and numbers, a list whose entries were
      reshuffled) and is taken address for address. Otherwise bytes moved: the
      same-length edits follow them, and the bytes put in or taken out have no
      address in the original and are not placed.
    - A stretch that never closes moves the rest of the section; likewise."""
    runs = edits(da, db)
    shifts = moved_by(runs)
    relinked = collections.Counter()
    for a0, a1, b0, b1 in runs:
        if a1 - a0 == b1 - b0:
            for p, q, bp in _words(a0, a1, b0):
                r = relocation(da, db, p, q, bp, bp + q - p, shifts)
                if r is not None:
                    relinked[r] += 1
    u, unplaced, n = bytearray(da), [], collections.Counter()

    def put(a0, a1, b0):
        for p, q, bp in _words(a0, a1, b0):
            if relocation(da, db, p, q, bp, bp + q - p, shifts) is None:
                u[p:q] = db[bp:bp + q - p]
            else:
                n["relinked"] += 1

    i, d = 0, 0
    while i < len(runs):
        a0, a1, b0, b1 = runs[i]
        if a1 - a0 == b1 - b0:
            put(a0, a1, b0)
            n["edits"] += 1
            i += 1
            continue
        j, dd, inner, closed = i, d, set(), False
        while j < len(runs):
            x0, x1, y0, y1 = runs[j]
            dd += (y1 - y0) - (x1 - x0)
            inner.add((y1 - y0) - (x1 - x0))
            j += 1
            if dd == d:
                closed = True
                break
            inner.add(dd)
        lo, hi = runs[i][0], runs[j - 1][1]
        if closed and (any(f0 <= lo and hi <= f1 for f0, f1 in fixed) or not any(relinked[x] for x in inner)):
            put(lo, hi, lo + d)
            n["rewritten in place"] += 1
        else:
            for x0, x1, y0, y1 in runs[i:j]:
                if x1 - x0 == y1 - y0:
                    put(x0, x1, y0)
                    n["edits"] += 1
                else:
                    unplaced.append((x0, x1, y0, y1))
                    n["length changes"] += 1
        i, d = j, dd
    return bytes(u), unplaced, n


class UnmovedDisc:
    """The original disc's file tree; every section of an overlay that the
    patch changed is unmoved_section's (the sections the PC keeps as chunks
    stay the original's here: they are the DAT layers' work). What
    tools/exe_tables.py is given to build the patched .data image from."""

    def __init__(self, orig, patched, fixed=None):
        """`fixed`: {(EMI path under the data root, section): [(from, to)]},
        the offsets the PC's image takes as tables (unmoved_section)."""
        self.orig, self.patched, self.fixed = orig, patched, fixed or {}
        self.files = orig.files
        self.unplaced, self.counts, self._memo = {}, collections.Counter(), {}

    @staticmethod
    def overlay(kind, dest):
        return kind == 0 and dest >> 24 == 0x80 and dest not in KEPT and dest != ENEMY_DEST

    def read(self, name):
        key = name.upper().replace("\\", "/")
        if key in self._memo:
            return self._memo[key]
        out = ob = self.orig.read(key)
        if key.endswith(".EMI") and key in self.patched.files and ob[8:16] == b"MATH_TBL":
            nb = self.patched.read(key)
            if nb != ob:
                so, sn = emi_sections(ob), emi_sections(nb)
                if len(so) != len(sn):
                    raise Bad("%s: the patch changes how many sections the file has" % key)
                body = []
                for (i, kind, dest, da), (_, _, _, db) in zip(so, sn):
                    sec = da
                    if da != db and self.overlay(kind, dest):
                        rel = key.split("/", 1)[1] if key.startswith("BIN/") else key
                        sec, unplaced, n = unmoved_section(da, db, self.fixed.get((rel, i), ()))
                        self.counts.update(n)
                        if unplaced:
                            self.unplaced[(key, i)] = (dest, unplaced)
                    body.append(sec + bytes(-len(sec) % 0x800))
                out = ob[:0x800] + b"".join(body)
        self._memo[key] = out
        return out

    def find(self, basename):
        return self.orig.find(basename)

    def sector(self, lba):
        return self.orig.sector(lba)

    def extent(self, lba, size):
        return self.orig.extent(lba, size)


# ---------------------------------------------------------------- the .data image

# The classes of an image's ranges a layer may write (src/hook/exe_image.cpp
# kClasses: the others must equal the PC's).
LAYER_CLASSES = ("map", "place", "table", "widen", "language", "unread", "gap", "file")


def image_changes(orig, patched, build_id):
    """{PC address: byte} - where the .data image built from the patched
    disc's overlays differs from the original disc's - what was not placed:
    {"pointer": bytes in pointer words, "length": [(file, section, disc
    address, old length, new length, PC address)] runs that change length on
    bytes the image takes, "counts": unmoved_section's}, and the UnmovedDisc
    the image was built from."""
    import exe_tables
    import exe_twins
    places = exe_tables.load_places().get(build_id, ())
    pointers = exe_tables.load_pointers()
    try:
        img0, owner0, ranges0 = exe_tables.disc_image(orig, build_id, places)
        fixed = collections.defaultdict(list)
        bases = {(f, sec): base for f, sec, base, _ in exe_twins.sources(orig)}
        for r_lo, r_hi, how, where, addr in ranges0:
            f, _, sec = where.split(":")[0].partition("#")
            if addr is not None and sec and (f, int(sec)) in bases:
                fixed[(f, int(sec))].append((addr - bases[(f, int(sec))], addr - bases[(f, int(sec))] + r_hi - r_lo))
        hybrid = UnmovedDisc(orig, patched, fixed)
        img1, owner1, ranges1 = exe_tables.disc_image(hybrid, build_id, places)
    except SystemExit as e:
        raise Bad("the image builder refused the patched overlays: %s" % e)
    if [r[:4] for r in ranges0] != [r[:4] for r in ranges1]:
        raise Bad("with the patch the image builder places a table somewhere else, which this release cannot carry")
    before = sum(1 for x, y in zip(img0, img1) if x != y)
    exe_tables.mask(img0, owner0, pointers)
    exe_tables.mask(img1, owner1, pointers)
    lo = exe_tables.DATA_LO
    changes = {lo + k: img1[k] for k in range(len(img0)) if img0[k] != img1[k]}
    segs = collections.defaultdict(list)
    for r_lo, r_hi, how, where, addr in ranges0:
        if addr is not None:
            segs[where.split(":")[0]].append((addr, addr + r_hi - r_lo, r_lo))
    length = []
    for (key, i), (dest, runs) in sorted(hybrid.unplaced.items()):
        rel = key.split("/", 1)[1] if key.startswith("BIN/") else key
        for a0, a1, b0, b1 in runs:
            for s0, s1, pc in segs.get("%s#%d" % (rel, i), ()):
                if min(dest + max(a1, a0 + 1), s1) > max(dest + a0, s0):
                    length.append((rel, i, dest + a0, a1 - a0, b1 - b0, pc + max(0, dest + a0 - s0)))
    return changes, {"pointer": before - len(changes), "length": length, "counts": hybrid.counts}, hybrid


def write_image_part(cache, root, layer, changes, where):
    """The mod's image part into root/<layer>/exe/: a byte in a table that is
    a file as a cell of tables.toml (the install's whole table file with the
    mod's bytes, docs/image-layer-round.md 3.2), any other as a raw range of
    place.toml where the install's image is of a class a layer may write.
    ([(file name, sha256)], {"cells", "tables", "raw", "kept": bytes the
    install's image keeps as the PC's own})."""
    import tomllib
    exe = os.path.join(cache, "base", "exe")
    try:
        with open(os.path.join(exe, "data.toml"), "rb") as f:
            image = tomllib.load(f)["image"]
        with open(os.path.join(exe, "tables", "tables.toml"), "rb") as f:
            rows = sorted(tomllib.load(f)["tables"], key=lambda r: r[1])
    except (OSError, KeyError, tomllib.TOMLDecodeError):
        raise Bad("this install's game data has no table files (base\\exe\\tables): it was built by an older "
                  "release. Build the install again on the Sources page")
    classes = sorted((r[0], r[1], r[2]) for r in image["ranges"])
    bodies, cells, raw, kept = {}, collections.defaultdict(set), [], 0
    for pc in sorted(changes):
        row = next((r for r in rows if r[1] <= pc < r[2]), None)
        if row is not None:
            key, t_lo, t_hi, count, stride = row[:5]
            if key not in bodies:
                with open(os.path.join(exe, "tables", key + ".bin"), "rb") as f:
                    body = f.read()
                if len(body) != t_hi - t_lo or sha(body) != row[5]:
                    raise Bad("the install's table file %s.bin is not the one its tables.toml records; build the "
                              "install again on the Sources page" % key)
                bodies[key] = bytearray(body)
            bodies[key][pc - t_lo] = changes[pc]
            cells[key].add(pc - t_lo)
            continue
        how = next((h for c_lo, c_hi, h in classes if c_lo <= pc < c_hi), None)
        if how in LAYER_CLASSES:
            raw.append(pc)
        else:
            kept += 1
    d = os.path.join(root, layer, "exe")
    os.makedirs(d)
    wrote, ncells = [], 0
    if bodies:
        lines = ["# opt/%s/exe/tables.toml - written by tools/disc_patch_build.py (docs/disc-patch-round.md 3b)." % layer,
                 "# One file a table the mod changes: <key>.bin is the install's whole table with the mod's",
                 "# bytes in it. `cells` are the bytes that are the mod's - [key, record, at, length] -",
                 "# and only those are laid over .data (src/hook/exe_image.cpp).",
                 "", "build = %s" % json.dumps(where), "", "tables = ["]
        cell_lines = []
        by_key = {r[0]: r for r in rows}
        for key in sorted(bodies, key=lambda k: by_key[k][1]):
            r, body = by_key[key], bytes(bodies[key])
            with open(os.path.join(d, key + ".bin"), "wb") as f:
                f.write(body)
            lines.append('  ["%s", 0x%06X, %d, %d, "%s"],' % (key, r[1], r[3], r[4], sha(body)))
            wrote.append((key + ".bin", sha(body)))
            run = None                              # [record, at, length]: a run of the mod's bytes inside one record
            for off in sorted(cells[key]):
                rec, at = divmod(off, r[4])
                if run and run[0] == rec and run[1] + run[2] == at:
                    run[2] += 1
                else:
                    if run:
                        cell_lines.append('  ["%s", %d, %d, %d],' % (key, run[0], run[1], run[2]))
                    run = [rec, at, 1]
            cell_lines.append('  ["%s", %d, %d, %d],' % (key, run[0], run[1], run[2]))
            ncells += len(cells[key])
        text = ("\n".join(lines + ["]", "", "cells = ["] + cell_lines + ["]"]) + "\n").encode("utf-8")
        with open(os.path.join(d, "tables.toml"), "wb") as f:
            f.write(text)
        wrote.append(("tables.toml", sha(text)))
    if raw:
        ranges = []
        for pc in raw:
            if ranges and ranges[-1][1] == pc:
                ranges[-1][1] = pc + 1
            else:
                ranges.append([pc, pc + 1])
        blob = bytes(changes[pc] for pc in raw)
        with open(os.path.join(d, "place.bin"), "wb") as f:
            f.write(blob)
        text = ("\n".join(
            ["# opt/%s/exe/place.toml - written by tools/disc_patch_build.py (docs/disc-patch-round.md 3b)." % layer,
             "# The mod's bytes of .data outside every table that is a file, by address; place.bin holds",
             "# them in this order (src/hook/exe_image.cpp).",
             "", "build = %s" % json.dumps(where), 'kind = "raw"', 'file = "place.bin"', 'sha256 = "%s"' % sha(blob),
             "bytes = %d" % len(blob), "", "ranges = ["] +
            ['  [0x%06X, 0x%06X, "mod"],' % (a, b) for a, b in ranges] + ["]"]) + "\n").encode("utf-8")
        with open(os.path.join(d, "place.toml"), "wb") as f:
            f.write(text)
        wrote += [("place.bin", sha(blob)), ("place.toml", sha(text))]
    if not wrote:
        os.rmdir(d)
        if not os.listdir(os.path.join(root, layer)):
            os.rmdir(os.path.join(root, layer))
    return wrote, {"cells": ncells, "tables": len(bodies), "raw": len(raw), "kept": kept}


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
    os.makedirs(d, exist_ok=True)
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


def run_loc(mode, tag, build_id, orig_path, patched_path, cache, pc_rows, work, enemy_names, say):
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
    cmd = [sys.executable, os.path.abspath(__file__), "_loc", orig_path, patched_path, jobs, names_dir, build_id,
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
    orig_path, patched_path, jobs, names_dir, build_id = argv[:5]
    import importer
    import loc_build
    import psx_disc
    orig, patched = psx_disc.Disc(orig_path), psx_disc.Disc(patched_path)
    try:
        changes, _, unmoved = image_changes(orig, patched, build_id)
    except Bad as e:
        print("disc_patch_build: %s" % e)
        return 1

    def opener(path):
        # the mod's numbers over the image from here on: loc_build.py opens the disc after it has
        # checked that a PC install's BOF3.exe is the one it knows, which must see the exe as it is
        loc_build.image_overlay = changes
        return HybridDisc(orig, patched, unmoved=unmoved)
    loc_build.disc_opener = opener
    sys.argv = ["loc_build.py"] + argv[5:]
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
        other = sorted(k for k, v in changed.items() if k not in used and v[0] not in SOUND_TYPES and v[1] not in KEPT
                       and not UnmovedDisc.overlay(v[0], v[1]))
        overlays = sorted(k for k, v in changed.items() if k not in used and UnmovedDisc.overlay(v[0], v[1]))
        enemy = sorted(n for n, chunks in base.items() if any(t == importer.ENEMY_TAG for _, t, _ in chunks))

        say("Building the overlays' tables with the patch's numbers")
        image, notes, _ = image_changes(orig.disc, patched.disc, build_id)
        boot = 0
        if orig.boot and orig.boot in patched.disc.files:
            boot = len(edits(orig.disc.read(orig.boot), patched.disc.read(orig.boot)))

        mode = loc_mode(cache, tag, build_id)
        say("Building the %s text with the patch's text (the text builder, about two minutes)" % tag)
        orig.disc.f.close()
        patched.disc.f.close()
        new_loc, names_dir = run_loc(mode, tag, build_id, disc_path, patched_path, cache, rows, work, enemy, say)
        text = diff_layer(os.path.join(cache, "loc", tag, "dat"), new_loc)
        for name in enemy:
            with open(os.path.join(names_dir, name), "rb") as f:
                text.setdefault(name, []).extend(read_chunks(f.read(), name))

        first, second = layer_names(row, tag)
        staged = os.path.join(work, "layers")
        out = {}
        if base:
            out[first] = write_layer(staged, first, {n: [(k, t, 0, b) for k, t, b in c] for n, c in base.items()}, where)
        wrote, placed = write_image_part(cache, staged, first, image, where) if image else ([], None)
        if wrote:
            out[first] = out.get(first, []) + [("exe/" + n, i, where, h) for i, (n, h) in enumerate(wrote)]
        if text:
            out[second] = write_layer(staged, second, text, where)
        if not out:
            raise Bad("the patch changes nothing this release can carry (%d sections changed, none of them one the "
                      "PC keeps as a chunk or a table)" % len(changed))
        opt = os.path.join(cache, "opt")
        os.makedirs(opt, exist_ok=True)
        for layer in (first, second):
            shutil.rmtree(os.path.join(opt, layer), ignore_errors=True)
            if layer in out:
                os.rename(os.path.join(staged, layer), os.path.join(opt, layer))
            else:
                out[layer] = []
        say("  %s: %d chunks in %d files (enemy tables and pictures, for any text)"
            % (first, sum(len(c) for c in base.values()), len(base)))
        if placed:
            say("  %s: %d bytes of %d tables that are files, and %d bytes of other tables (the overlays' numbers)"
                % (first, placed["cells"], placed["tables"], placed["raw"]))
            if placed["kept"]:
                say("  not carried: %d bytes of the overlays' numbers outside the tables that are files; this install's "
                    "image keeps them as the PC's own" % placed["kept"])
        if notes["length"]:
            say("  not carried: %d places where the patch changes a table's length" % len(notes["length"]))
            for rel, i, addr, old, new, pc in notes["length"]:
                say("    %s section %d at 0x%08X: %d bytes become %d (PC 0x%06X)" % (rel, i, addr, old, new, pc), quiet=True)
        n = notes["counts"]
        say("  the overlays: %d edits taken, %d rewrites in place, %d relinked addresses left as the PC's, %d length "
            "changes followed" % (n["edits"], n["rewritten in place"], n["relinked"], n["length changes"]), quiet=True)
        if boot:
            say("  not carried: the boot program, which differs in %d places (its file table follows the disc's "
                "layout; its other changes are not read)" % boot)
        if notes["pointer"]:
            say("  not carried: %d bytes of pointers the patch aims elsewhere (the PC's pointers are its own)"
                % notes["pointer"])
        say("  %s: %d chunks in %d files (%s text, and its names for %d enemy tables)"
            % (second, len(out[second]), len(text), tag, len(enemy)))
        if sound:
            say("  not carried: %d changed sound sections in %d files (a changed sound bank is not built)"
                % (len(sound), len({k for k, _ in sound})))
        say("  %d changed overlay sections in %d files were read for their tables; their code is not carried (the PC "
            "does not run it)" % (len(overlays), len({k for k, _ in overlays})))
        if other:
            say("  not carried: %d changed sections in %d files of a kind this release does not read"
                % (len(other), len({k for k, _ in other})))
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

    # one section against its patched self. 0x80100000 is where the section loads.
    def word(v):
        return struct.pack("<I", v)

    filler = bytes((7 * i + 3) & 0xFF for i in range(64))
    da = filler + word(0x80100060) + bytes(range(100, 132)) + b"TABLE-OF-NUMBERS" + filler[::-1]
    db = bytearray(da)
    db[70] ^= 0xFF                                    # an edit in place
    u, unplaced, n = unmoved_section(da, bytes(db))
    expect(u == bytes(db) and not unplaced and n["edits"] == 1, "unmoved: an edit of the same length is taken")
    grown = da[:16] + b"\xAA" * 8 + da[16:64] + word(0x80100068) + da[68:]     # 8 bytes put in; the pointer relinked
    u, unplaced, n = unmoved_section(da, grown)
    expect(u == da and unplaced == [(16, 16, 16, 24)] and n["relinked"] == 1 and n["length changes"] == 1,
           "unmoved: bytes put in are not placed, and the pointer the tool relinked stays the original's")
    grown2 = bytearray(grown)
    grown2[8 + 100] ^= 0x55                           # and an edit after the insertion: old offset 100
    u, unplaced, n = unmoved_section(da, bytes(grown2))
    expect(u == da[:100] + bytes([da[100] ^ 0x55]) + da[101:] and len(unplaced) == 1,
           "unmoved: an edit after bytes that moved lands at the original's address")
    table = b"TABLE-OF-NUMBERS"
    at = da.index(table)
    shuffled = da[:at] + b"NEW!" + table[:12] + da[at + 16:]                    # put in 4, take out 4: no pointer moved
    u, unplaced, n = unmoved_section(da, shuffled)
    expect(u == shuffled and not unplaced and n["rewritten in place"] == 1,
           "unmoved: a list rewritten in place (put in and taken out, nothing relinked) is taken address for address")
    both = da[:16] + b"\xAA" * 8 + da[16:40] + da[48:64] + word(0x80100068) + da[68:]   # +8 then -8, a pointer moved by 8
    u, unplaced, n = unmoved_section(da, both)
    expect(len(unplaced) == 2 and n["rewritten in place"] == 0,
           "unmoved: the same shape with a pointer relinked by that distance is bytes that moved, not placed")
    u, unplaced, n = unmoved_section(da, both, fixed=[(0, 64)])
    expect(not unplaced and u[:64] == both[:64] and n["rewritten in place"] == 1,
           "unmoved: inside a table the PC holds at its own address it is a rewrite in place")
    o2 = _Disc({"BIN/W/AREA001.EMI": _emi([(0, 0x80010000, text), (0, 0x80100000, da)])})
    n2 = _Disc({"BIN/W/AREA001.EMI": _emi([(0, 0x80010000, text2), (0, 0x80100000, bytes(db))])})
    um = UnmovedDisc(o2, n2)
    got = emi_sections(um.read("BIN/W/AREA001.EMI"))
    expect([x[3] for x in got] == [text, bytes(db)] and um.counts["edits"] == 1,
           "unmoved disc: the overlay's section with the edit, the text section the original's")
    hy = HybridDisc(o2, n2, unmoved=um)
    expect([x[3] for x in emi_sections(hy.read("BIN/W/AREA001.EMI"))] == [text2, bytes(db)],
           "hybrid disc over it: the patched text and the overlay's edit together")

    root = tempfile.mkdtemp(prefix="bof3x_dpb_")
    try:
        # the image part: a cache of this test's own, a table of zeros
        ic = os.path.join(root, "icache")
        os.makedirs(os.path.join(ic, "base", "exe", "tables"))
        zeros = bytes(40)
        with open(os.path.join(ic, "base", "exe", "tables", "things.bin"), "wb") as f:
            f.write(zeros)
        with open(os.path.join(ic, "base", "exe", "tables", "tables.toml"), "w") as f:
            f.write('build = "test"\ntables = [\n  ["things", 0x600100, 0x600128, 5, 8, "%s", "test"],\n]\n' % sha(zeros))
        with open(os.path.join(ic, "base", "exe", "data.toml"), "w") as f:
            f.write('[image]\nranges = [\n  [0x600000, 0x600100, "map", "x"],\n  [0x600100, 0x600128, "file", "things"],\n'
                    '  [0x600128, 0x600200, "exe", "x"],\n]\n')
        ch = {0x600101: 1, 0x600102: 2, 0x600107: 7, 0x600108: 8, 0x600010: 0x10, 0x600011: 0x11, 0x600020: 0x20,
              0x600130: 0x30}
        wrote, placed = write_image_part(ic, os.path.join(root, "iout"), "mod-x", ch, "mod:x@1:psx-xx")
        import tomllib
        d = os.path.join(root, "iout", "mod-x", "exe")
        with open(os.path.join(d, "tables.toml"), "rb") as f:
            tt = tomllib.load(f)
        with open(os.path.join(d, "place.toml"), "rb") as f:
            pt = tomllib.load(f)
        with open(os.path.join(d, "things.bin"), "rb") as f:
            body = f.read()
        with open(os.path.join(d, "place.bin"), "rb") as f:
            blob = f.read()
        expect(tt["cells"] == [["things", 0, 1, 2], ["things", 0, 7, 1], ["things", 1, 0, 1]]
               and tt["tables"] == [["things", 0x600100, 5, 8, sha(body)]] and body[1:3] == b"\x01\x02" and body[7:9] == b"\x07\x08"
               and sum(body) == 18, "image part: a table's bytes as cells, a run cut at the record's end")
        expect(pt["kind"] == "raw" and pt["ranges"] == [[0x600010, 0x600012, "mod"], [0x600020, 0x600021, "mod"]]
               and pt["bytes"] == 3 and blob == b"\x10\x11\x20" and pt["sha256"] == sha(blob),
               "image part: the other bytes as raw ranges, in order")
        expect(placed == {"cells": 4, "tables": 1, "raw": 3, "kept": 1} and [w[0] for w in wrote] ==
               ["things.bin", "tables.toml", "place.bin", "place.toml"],
               "image part: a byte on a class that must equal the PC's is kept and counted")
        wrote, placed = write_image_part(ic, os.path.join(root, "iout2"), "mod-x", {0x600130: 1}, "w")
        expect(wrote == [] and placed["kept"] == 1 and not os.path.exists(os.path.join(root, "iout2", "mod-x")),
               "image part: nothing a layer may write leaves no folder")
        with open(os.path.join(ic, "base", "exe", "tables", "things.bin"), "wb") as f:
            f.write(bytes(39) + b"\1")
        refused(lambda: write_image_part(ic, os.path.join(root, "iout3"), "mod-x", ch, "w"), "is not the one its tables.toml",
                "image part: a table file that is not the install's own")
        os.remove(os.path.join(ic, "base", "exe", "tables", "tables.toml"))
        refused(lambda: write_image_part(ic, os.path.join(root, "iout4"), "mod-x", ch, "w"), "no table files",
                "image part: an install from before the table files")

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
