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
  6. both layers into <cache>/opt/, and their manifest rows returned;
  7. for a mod whose `needs` names TownWarp (DIV-0225, docs/town-warp.md 4),
     the town warp's table and text, read from its file outside the disc's
     directory, as kind 21 of each layer's FIRST.DAT.

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


def relocation(da, db, a0, a1, b0, b1, shifts, data=False):
    """When a same-length run inside one word is only an address that follows
    moved bytes - a jump target, a 16-bit offset or a pointer word that differs
    by one of `shifts` - that distance; else None. The edit is the patch tool's
    relinking, not a change the author made by hand.

    `data`: the word lies in bytes the PC's image takes as a table. A table
    holds no instruction, so only a whole pointer word counts there, and both
    words must be addresses (0x80......): a number the author changed by an
    amount that happens to equal a distance bytes moved is his edit (D417: the
    Definitive Edition 1.4 moves bytes by 220 and lowers one EXP step by 220)."""
    if not shifts or a1 - a0 != b1 - b0 or a1 - a0 > 4 or (a0 & ~3) != ((a1 - 1) & ~3) or (a0 & 3) != (b0 & 3):
        return None
    if (a0 & ~3) + 4 > len(da) or (b0 & ~3) + 4 > len(db):
        return None
    w0, = struct.unpack_from("<I", da, a0 & ~3)
    w1, = struct.unpack_from("<I", db, b0 & ~3)
    d32 = (w1 - w0) & 0xFFFFFFFF
    d32 -= (d32 & 0x80000000) << 1
    if data:
        return d32 if d32 in shifts and (w0 >> 24) == 0x80 and (w1 >> 24) == 0x80 else None
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
      its own. Inside a `fixed` table only a pointer word can be that; a
      number there is the author's, whatever it differs by.
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

    def table(p, q):
        return any(f0 <= p and q <= f1 for f0, f1 in fixed)

    for a0, a1, b0, b1 in runs:
        if a1 - a0 == b1 - b0:
            for p, q, bp in _words(a0, a1, b0):
                r = relocation(da, db, p, q, bp, bp + q - p, shifts, table(p, q))
                if r is not None:
                    relinked[r] += 1
    u, unplaced, n = bytearray(da), [], collections.Counter()

    def put(a0, a1, b0):
        for p, q, bp in _words(a0, a1, b0):
            if relocation(da, db, p, q, bp, bp + q - p, shifts, table(p, q)) is None:
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


def parse_skip(value, cat=None):
    """A disc-patch mod's `skip` (docs/mods.md 11): what of a table the build
    must not carry, which no rule of the build can see. In mod.toml a list of
    inline tables, {table = "<a tables.toml key>", records = [first, last]};
    out, [[key, first, last]] (the form <home>/mods.toml keeps, which a JSON
    list writes as TOML). Bad, naming the entry, for a table the catalogue
    does not have or that is no table file, or records outside the table."""
    if cat is None:
        import tables
        cat, _, _ = tables.load()
    by_key = {t["key"]: t for t in cat["table"]}
    if not isinstance(value, list) or len(value) > 64:
        raise Bad("skip must be a list of at most 64 entries, each {table = \"...\", records = [first, last]}")
    out = []
    for n, e in enumerate(value, 1):
        where = "skip entry %d" % n
        if isinstance(e, list) and len(e) == 3:            # the home's own form, read back
            e = {"table": e[0], "records": e[1:]}
        if not isinstance(e, dict) or set(e) != {"table", "records"}:
            raise Bad("%s must have exactly two keys, table and records" % where)
        key, rg = e["table"], e["records"]
        if not isinstance(key, str) or key not in by_key:
            raise Bad("%s: table %s is not a table of the catalogue (tables.py list names them)" % (where, json.dumps(key)))
        t = by_key[key]
        if "symbol" not in t or "in_image" in t:
            raise Bad("%s: table %s is not a table file of the PC's data, so no cell of it is carried" % (where, key))
        if not (isinstance(rg, list) and len(rg) == 2 and all(isinstance(v, int) and not isinstance(v, bool) for v in rg)
                and 0 <= rg[0] <= rg[1] < t["count"]):
            raise Bad("%s: records must be [first, last] inside %s (0 to %d)" % (where, key, t["count"] - 1))
        out.append([key, rg[0], rg[1]])
    return out


def write_image_part(cache, root, layer, changes, where, skip=(), cat=None):
    """The mod's image part into root/<layer>/exe/: a byte in a table that is
    a file as a cell of tables.toml (the install's whole table file with the
    mod's bytes, docs/image-layer-round.md 3.2), any other as a raw range of
    place.toml where the install's image is of a class a layer may write.
    ([(file name, sha256)], {"cells", "tables", "raw", "kept": bytes the
    install's image keeps as the PC's own, "zero_bytes" / "zero_records":
    bytes not carried because they fill a record the install leaves all zero
    and the filled record is outside the table's field bounds, "skipped":
    bytes of the records `skip` names ([[key, first, last]], parse_skip)}).

    The zero-record rule (docs/mods.md 11; D415): a record the original
    leaves entirely zero is not the mod's to fill unless every field of the
    patched record is inside its `range` (tables.toml; tables.out_of_bounds).
    A hack that keeps code or its own tables in a table's empty records is
    the case it sees; a field with no range takes any value."""
    import tomllib
    import tables
    if cat is None:
        cat, _, _ = tables.load()
    by_cat = {t["key"]: t for t in cat["table"]}
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
    # the records a mod does not fill: `skip`'s, then the zero-record rule
    zero_bytes, zero_records, skipped = 0, 0, 0
    by_row = {r[0]: r for r in rows}
    for key in list(bodies):
        stride = by_row[key][4]
        with open(os.path.join(exe, "tables", key + ".bin"), "rb") as f:
            orig = f.read()
        for rec in sorted({off // stride for off in cells[key]}):
            lo, hi = rec * stride, (rec + 1) * stride
            mine = [off for off in cells[key] if lo <= off < hi]
            skipped_here = any(k == key and a <= rec <= b for k, a, b in skip)
            zero_fill = not any(orig[lo:hi]) and key in by_cat and \
                tables.out_of_bounds(by_cat[key], bytes(bodies[key][lo:hi])) is not None
            if not (skipped_here or zero_fill):
                continue
            for off in mine:
                bodies[key][off] = orig[off]
                cells[key].discard(off)
            if skipped_here:
                skipped += len(mine)
            else:
                zero_bytes += len(mine)
                zero_records += 1
        if not cells[key]:
            del bodies[key]
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
    return wrote, {"cells": ncells, "tables": len(bodies), "raw": len(raw), "kept": kept,
                   "zero_bytes": zero_bytes, "zero_records": zero_records, "skipped": skipped}


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


# ---------------------------------------------------------------- files the directory does not list

SECTOR = 2048


def disc_sectors(disc):
    """How many sectors the image holds (a raw track 2352 bytes a sector, a cooked one 2048)."""
    return os.path.getsize(disc.f.name) // (2352 if disc.raw else SECTOR)


def owned_sectors(disc):
    """Every sector the disc's ISO 9660 tree accounts for, as a set of LBAs:
    the system area and the volume descriptors (LBA 0 to the set terminator),
    the path tables, every directory's extent and every file's."""
    owned = set(range(16))
    lba = 16
    while True:
        s = disc.sector(lba)
        owned.add(lba)
        if s[1:6] != b"CD001" or s[0] == 255 or lba > 64:
            break
        lba += 1
    pvd = disc.sector(16)
    size, = struct.unpack_from("<I", pvd, 132)
    n = max(1, -(-size // SECTOR))
    for at, fmt in ((140, "<I"), (144, "<I"), (148, ">I"), (152, ">I")):
        t, = struct.unpack_from(fmt, pvd, at)
        if t:
            owned.update(range(t, t + n))

    def walk(lba, size, depth):
        if depth > 32:
            raise Bad("the disc's directory tree is deeper than 32 folders")
        owned.update(range(lba, lba + max(1, -(-size // SECTOR))))
        d, pos = disc.extent(lba, size), 0
        while pos < len(d):
            n = d[pos]
            if n == 0:
                pos = (pos // SECTOR + 1) * SECTOR
                continue
            e_lba, e_size = struct.unpack_from("<I", d, pos + 2)[0], struct.unpack_from("<I", d, pos + 10)[0]
            flags, nlen = d[pos + 25], d[pos + 32]
            name = d[pos + 33:pos + 33 + nlen]
            pos += n
            if name in (b"\x00", b"\x01"):
                continue
            if flags & 2:
                walk(e_lba, e_size, depth + 1)
            else:
                owned.update(range(e_lba, e_lba + max(1, -(-e_size // SECTOR))))
    root = pvd[156:190]
    walk(struct.unpack_from("<I", root, 2)[0], struct.unpack_from("<I", root, 10)[0], 0)
    return owned


def outside_files(orig, patched):
    """The files of the patched disc that its directory does not list
    (docs/mods.md 11): sectors that are not zero, belong to no directory
    entry of the patched disc, and are not the original's sector at that LBA
    (absent there, or other bytes: a rebuilt disc lays the original's files
    elsewhere, so on the Definitive Edition 3 of the 67 such sectors hold the
    original's bytes of another file, not zeros) - read as EMIs where they are
    one. A sector the original has the same is the mastering tool's, not the
    patch's. [{"lba", "sectors", "sections": [(index, type, load address,
    size)] or None for sectors that are not an EMI}], in LBA order. Read
    only; nothing here is carried into a layer."""
    owned, total, o_total = owned_sectors(patched), disc_sectors(patched), disc_sectors(orig)

    def free(lba):
        """Outside the directory, and not the original's sector at that LBA."""
        return lba < total and lba not in owned and (lba >= o_total or orig.sector(lba) != patched.sector(lba))

    out, lba = [], 0
    while lba < total:
        if lba in owned:
            lba += 1
            continue
        s = patched.sector(lba)
        if not any(s) or not free(lba):
            lba += 1
            continue
        if s[8:16] == b"MATH_TBL":
            count, = struct.unpack_from("<I", s, 0)
            if 0 < count <= 126:
                sizes = [struct.unpack_from("<I", s, 0x10 + 16 * i)[0] for i in range(count)]
                n = 1 + sum(-(-z // SECTOR) for z in sizes)
                if all(free(lba + k) for k in range(n)):
                    blob = patched.extent(lba, n * SECTOR)
                    secs = [(i, kind, dest, len(b)) for i, kind, dest, b in emi_sections(blob)]
                    out.append({"lba": lba, "sectors": n, "sections": secs})
                    lba += n
                    continue
        if out and out[-1]["sections"] is None and out[-1]["lba"] + out[-1]["sectors"] == lba:
            out[-1]["sectors"] += 1
        else:
            out.append({"lba": lba, "sectors": 1, "sections": None})
        lba += 1
    return out


def outside_sections(disc, lba):
    """One file outside the disc's directory, read as an EMI from its first
    sector `lba` (an outside_files() row's): [(index, type, load address,
    bytes)], as emi_sections gives them. For the code that carries such a file
    later (docs/mods.md 11: the camp's masters and the town warp of the
    Definitive Edition read their text from files like these). Bad when the
    sector is not an EMI's header. The bytes are game data: never committed."""
    head = disc.sector(lba)
    if head[8:16] != b"MATH_TBL":
        raise Bad("the sector at LBA 0x%X is not the start of an EMI" % lba)
    count, = struct.unpack_from("<I", head, 0)
    sizes = [struct.unpack_from("<I", head, 0x10 + 16 * i)[0] for i in range(count)]
    n = 1 + sum(-(-z // SECTOR) for z in sizes)
    return emi_sections(disc.extent(lba, n * SECTOR))


# ---------------------------------------------------------------- TownWarp's data (DIV-0225)
#
# docs/town-warp.md 4. The Definitive Edition romhack keeps its town warp's
# question, labels, place names and destination table in a file outside the
# disc's directory whose one section loads at 0x80016000, and the towns' area
# ids in the patched GAME.EMI section 0. Behaviour TownWarp (ours) needs them
# from the player's own build, never from this repository: kind 21 of the
# mod's FIRST.DAT, tag 0 the table (the first layer), tag 1 the text (the
# language's layer). The reader is bound to that hack's code: each address is
# taken from the instruction that loads it, at a fixed offset, with the
# instruction's opcode and registers checked, and the town count from both
# loops' bounds; anything else is refused. Numbers only here; the bytes read
# are the hack author's and stay in the player's cache.

WARP_DEST = 0x80016000              # the warp file's section (its +0 opens the menu, +8 is its frame)
WARP_KIND = 21                      # the chunk kind (src/game/battle/dat_load.cpp TakeChunk)
WARP_TABLE_TAG, WARP_TEXT_TAG = 0, 1
WARP_VERSION = 1
WARP_ROW = 16                       # a table row as carried: town area, destination area, flags, 0, x, z
WARP_MAX_TOWNS = 31                 # the visited word's bits 0..30 (bit 31 marks "forgotten")
WARP_NAME = "TownWarp"              # the behaviour that reads it (mods.md 11)
GAME_TOWNS = 0x8019C244             # GAME section 0: the visited-town recorder
CHOICE_CODE = 0x14                  # the message's choice control (tools/loc_build.py CHOICE)
_A0, _T3, _T4, _T5, _T6, _T7, _A1, _A2, _A3 = 4, 11, 12, 13, 14, 15, 5, 6, 7
_LUI, _ADDIU, _SLTIU, _LW, _LHU = 0x0F, 0x09, 0x0B, 0x23, 0x25
# (offset in the warp section, opcode, rt, rs) of each load the reader takes a
# number from; a `lui` is checked by its rt alone.
WARP_LOADS = {
    "header": ((0x8C, _LUI, _A0, 0), (0x94, _ADDIU, _A0, _A0)),     # the question
    "names": ((0x100, _LUI, _A0, 0), (0x108, _LW, _A0, _A0)),       # the names' pointer table
    "more": ((0x130, _LUI, _A0, 0), (0x138, _ADDIU, _A0, _A0)),
    "cancel": ((0x150, _LUI, _A0, 0), (0x158, _ADDIU, _A0, _A0)),
    "rows": ((0x27C, _LUI, _T3, 0), (0x284, _ADDIU, _T3, _T3)),     # the destination table
}
WARP_COUNT = (0xB0, _SLTIU, _T5, _T3)                               # the page loop's bound
WARP_FIELDS = ((0x288, _LHU, _A0, 0), (0x28C, _LHU, _A3, 2), (0x290, _LW, _A1, 4), (0x294, _LW, _A2, 8))
WARP_STRIDE = ((0x270, 1), (0x278, 2))                              # sll by 1, add, sll by 2: 12 a row
GAME_LOADS = ((0x14, _LUI, _T4, 0), (0x18, _ADDIU, _T4, _T4))       # from GAME_TOWNS: the towns' area ids
GAME_COUNT = (0x34, _SLTIU, _T7, _T5)


def _word(data, base, at):
    if not base <= at <= base + len(data) - 4:
        raise Bad("the town warp's code does not reach 0x%08X" % at)
    return struct.unpack_from("<I", data, at - base)[0]


def _insn(data, base, at, op, rt, rs):
    """The immediate of the I-type instruction at `at`, after its opcode and
    registers are checked (a `lui`'s rs is not)."""
    w = _word(data, base, at)
    if w >> 26 != op or (w >> 16) & 31 != rt or (op != _LUI and (w >> 21) & 31 != rs):
        raise Bad("the town warp's code at 0x%08X is not the shape this release reads" % at)
    imm = w & 0xFFFF
    return imm if op in (_LUI, _SLTIU) else imm - 0x10000 if imm & 0x8000 else imm


def _address(data, base, pair):
    (o1, op1, rt1, rs1), (o2, op2, rt2, rs2) = pair
    return ((_insn(data, base, base + o1, op1, rt1, rs1) << 16) + _insn(data, base, base + o2, op2, rt2, rs2)) & 0xFFFFFFFF


def _cstr(data, base, at, what):
    if not base <= at < base + len(data):
        raise Bad("the town warp's %s at 0x%08X is outside its file" % (what, at))
    end = data.find(b"\0", at - base)
    if end < 0:
        raise Bad("the town warp's %s at 0x%08X has no end" % (what, at))
    return data[at - base:end]


def town_warp_parts(warp_sections, game_section):
    """The town warp's parts as the hack keeps them: {"towns": [area id] *
    n, "rows": [(area, flags, x, z)] * n, "header", "more", "cancel": the
    disc's text bytes, "names": [bytes] * n}. `warp_sections`: the outside
    file's [(index, type, load address, bytes)]; `game_section`: (load
    address, bytes) of the patched GAME.EMI section 0. Bad when either is not
    the shape of the Definitive Edition's code."""
    secs = [s for s in warp_sections if s[2] == WARP_DEST]
    if len(warp_sections) != 1 or len(secs) != 1:
        raise Bad("the town warp's file is not one section at 0x%08X" % WARP_DEST)
    data = secs[0][3]
    base = WARP_DEST
    n = _insn(data, base, base + WARP_COUNT[0], *WARP_COUNT[1:])
    g_base, g_data = game_section
    if _insn(g_data, g_base, GAME_TOWNS + GAME_COUNT[0], *GAME_COUNT[1:]) != n:
        raise Bad("the town warp's two town counts differ")
    if not 1 <= n <= WARP_MAX_TOWNS:
        raise Bad("the town warp names %d towns; the visited word holds %d" % (n, WARP_MAX_TOWNS))
    for at, op, rt, off in WARP_FIELDS:
        if _insn(data, base, base + at, op, rt, _T3) != off:
            raise Bad("the town warp's table row is not the shape this release reads")
    for at, sa in WARP_STRIDE:
        w = _word(data, base, base + at)
        if w >> 26 != 0 or w & 0x3F != 0 or (w >> 6) & 31 != sa:
            raise Bad("the town warp's table stride is not the one this release reads")
    towns_at = ((_insn(g_data, g_base, GAME_TOWNS + GAME_LOADS[0][0], *GAME_LOADS[0][1:]) << 16)
                + _insn(g_data, g_base, GAME_TOWNS + GAME_LOADS[1][0], *GAME_LOADS[1][1:])) & 0xFFFFFFFF
    if not g_base <= towns_at <= g_base + len(g_data) - 2 * n:
        raise Bad("the town warp's town list is outside GAME's section")
    towns = list(struct.unpack_from("<%dH" % n, g_data, towns_at - g_base))
    rows_at = _address(data, base, WARP_LOADS["rows"])
    if not base <= rows_at <= base + len(data) - 12 * n:
        raise Bad("the town warp's table is outside its file")
    rows = [struct.unpack_from("<HHii", data, rows_at - base + 12 * i) for i in range(n)]
    names_at = _address(data, base, WARP_LOADS["names"])
    if not base <= names_at <= base + len(data) - 4 * n:
        raise Bad("the town warp's name list is outside its file")
    names = [_cstr(data, base, p, "name %d" % i)
             for i, p in enumerate(struct.unpack_from("<%dI" % n, data, names_at - base))]
    out = {"towns": towns, "rows": rows, "names": names}
    for key in ("header", "more", "cancel"):
        out[key] = _cstr(data, base, _address(data, base, WARP_LOADS[key]), key)
    head = out["header"]
    if len(head) < 5 or head[-4] != CHOICE_CODE or head[-1] & 0xF:
        raise Bad("the town warp's question does not end in an empty choice list")
    for key, raw in [("more", out["more"]), ("cancel", out["cancel"])] + [("name %d" % i, s) for i, s in enumerate(names)]:
        if not raw or any(b <= 0x16 for b in raw):
            raise Bad("the town warp's %s is empty or holds a control code" % key)
    return out


def town_warp_table(parts):
    """Kind 21 tag 0's payload: the version, n, then n rows of WARP_ROW bytes."""
    out = bytearray([WARP_VERSION, len(parts["towns"])])
    for town, (area, flags, x, z) in zip(parts["towns"], parts["rows"]):
        out += struct.pack("<HHHHii", town, area, flags, 0, x, z)
    return bytes(out)


def town_warp_text(parts, convert):
    """Kind 21 tag 1's payload: the version, n, then the question, "more",
    "cancel" and the n names, each a u16 length and its bytes in the PC's
    encoding. `convert(raw, nul_only)` is tools/loc_build.py's convert_run
    as (bytes or None, the index it stopped at): the question converts up to
    its choice control (no NUL), the others to and with their NUL. Bad for a
    string with a code the language does not have."""
    out = bytearray([WARP_VERSION, len(parts["names"])])
    strings = [("question", parts["header"], False), ("more", parts["more"], True),
               ("cancel", parts["cancel"], True)] + [("name %d" % i, s, True) for i, s in enumerate(parts["names"])]
    for what, raw, nul_only in strings:
        got, stop = convert(raw + b"\0", nul_only)
        if got is None:
            raise Bad("the town warp's %s holds a code the %s text does not have" % (what, "disc's"))
        if not nul_only and (stop != len(raw) or len(got) < 4 or got[-4] != CHOICE_CODE):
            raise Bad("the town warp's question did not convert to text and its choice control")
        if nul_only and (not got.endswith(b"\0") or b"\0" in got[:-1]):
            raise Bad("the town warp's %s did not convert to one string" % what)
        out += struct.pack("<H", len(got)) + got
    return bytes(out)


WARP_TEXT_FILE = "_townwarp"         # the child's text payload, beside the names containers


def game_section0(disc):
    """(load address, bytes) of GAME.EMI's section 0 on `disc`."""
    found = disc.find("GAME.EMI")
    if len(found) != 1:
        raise Bad("the patched disc holds %d GAME.EMI files" % len(found))
    secs = [(dest, b) for i, _, dest, b in emi_sections(disc.read(found[0])) if i == 0]
    if not secs:
        raise Bad("the patched GAME.EMI has no section 0")
    return secs[0]


def town_warp_file(outside):
    """The outside_files() row of the town warp's file (one section at
    WARP_DEST), or None."""
    hits = [x for x in outside if x["sections"] and any(s[2] == WARP_DEST for s in x["sections"])]
    if len(hits) > 1:
        raise Bad("the patched disc holds %d files with a section at 0x%08X" % (len(hits), WARP_DEST))
    return hits[0] if hits else None


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


def run_loc(mode, tag, build_id, orig_path, patched_path, cache, pc_rows, work, enemy_names, say, warp_lba=None,
            port=None):
    """loc_build.py on the hybrid disc, the way the install's layer was built:
    the folder of its containers, and the folder of the names containers for
    `enemy_names` (DAT names) - with, when `warp_lba` names the town warp's
    file (DIV-0225), its text payload as the file WARP_TEXT_FILE there. A
    child process: loc_build.py keeps a disc's state in its module.

    `port` (another language's text, port_text): {"disc": that language's
    disc, "dir": write_ported's folder}. The builder then reads that disc with
    the ported sections in it, and the image as the install has it."""
    import importer
    names_dir = os.path.join(work, "names")
    os.makedirs(names_dir)
    jobs = os.path.join(work, "names.json")
    with open(jobs, "w") as f:
        json.dump({"enemy": sorted(enemy_names), "warp": warp_lba, "port": port}, f)
    cmd = [sys.executable, os.path.abspath(__file__), "_loc", orig_path, patched_path, jobs, names_dir, build_id,
           "all", "--disc", port["disc"] if port else orig_path, "--lang", tag]
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
    with open(jobs) as f:
        job = json.load(f)
    if job.get("port"):
        # another language's text: its own disc with the ported sections, and the image as it is - that
        # disc's tables hold Capcom's numbers, which is what the builder finds them by
        ported = read_ported(job["port"]["dir"])

        def opener(path):
            return PortedDisc(psx_disc.Disc(path), ported)
    else:
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
    wanted = job["enemy"]
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
    if job.get("warp") is not None:
        # DIV-0225: the town warp's text in the PC's encoding, by the converter
        # the language's text went through, while it is still set up.
        try:
            parts = town_warp_parts(outside_sections(patched, job["warp"]), game_section0(patched))
            payload = town_warp_text(parts, lambda raw, nul_only: loc_build.convert_run(raw, 0, nul_only))
        except Bad as e:
            print("disc_patch_build: %s" % e)
            return 1
        with open(os.path.join(names_dir, WARP_TEXT_FILE), "wb") as f:
            f.write(payload)
        print("disc_patch_build: the town warp's text, %d places" % len(parts["names"]))
    return 0


# ---------------------------------------------------------------- the patch's text over another disc's
#
# docs/disc-patch-round.md 11. A patch of the US disc changes en-US text. The
# European English text (en-150: the European PlayStation disc's, the European
# PSP disc's) is the same script with a few lines worded or broken another way,
# so the patch's lines are carried over to it line by line. A text block is a
# table of 16-bit offsets, one a slot, and the messages they point at;
# loc_build.convert_block reads a block slot by slot and nothing else of it, so
# a block made again from each slot's message is the same block to the builder.
#
#   the patch left the slot's message alone              the other disc's message
#   the patch changed it, the other disc has the US's    the patch's message ("as is")
#   the patch changed it, the other disc words it        the patch's message, counted and
#     differently                                          named in the build's log
#   the patch changed it, the other disc has no message  the other disc's (a slot it does not use)
#
# A block that is not this shape on one of the three sides is not carried, and
# said. Nothing here reads a language: the bytes are compared, never decoded.

ALSO_TEXT = {"psx-us": ("en-150",)}     # build -> the other tags its patch's text is carried over to
LAYER_MAX = 23                          # tools/mods.py LAYER_MAX (src/game/dat_load.cpp kOptName - 1)
DONOR_ARG1 = frozenset((0x04, 0x05, 0x07, 0x08, 0x0A, 0x0C, 0x0F, 0x16))   # loc_build.ARG1: a code and its one argument
DONOR_CHOICE = 0x14                     # loc_build.CHOICE: four bytes, then as many runs as its fourth byte's low nibble
POOL_DEST, BLOCK_DEST = 0x8001A000, 0x80010000
NAME_KIND, NAME_RECORD = 5, 16          # loc_build.KIND_NAMES: a table's names, 16 bytes a record


class Unmerged(Exception):
    """A text section the patch changed cannot be carried over to another disc's text; why, in a few words."""


def donor_message(buf, i):
    """The disc message at offset i of a block, through its end
    (loc_build.convert_run's walk for a Latin disc): to its NUL, a code of
    DONOR_ARG1 taking the byte after it, a choice taking its options' runs. A
    message the block's end cuts short is what is there."""
    n, start = len(buf), i

    def run(i):
        while i < n and buf[i]:
            i += 2 if buf[i] in DONOR_ARG1 else 1
        return i + 1

    while i < n:
        b = buf[i]
        if b == 0:
            i += 1
            break
        if b == DONOR_CHOICE:
            options = buf[i + 3] & 0xF if i + 3 < n else 0
            i += 4
            for _ in range(options):
                i = run(i)
            break
        i += 2 if b in DONOR_ARG1 else 1
    return bytes(buf[start:min(i, n)])


def text_slots(block):
    """A text block's messages, one a slot, or None when the block does not
    start with a table of offsets that all lie inside it."""
    if len(block) < 2:
        return None
    n = struct.unpack_from("<H", block, 0)[0] // 2
    if not n or 2 * n > len(block):
        return None
    offs = struct.unpack_from("<%dH" % n, block, 0)
    if any(o < 2 * n or o > len(block) for o in offs):
        return None
    seen = {}
    for o in offs:
        if o not in seen:
            seen[o] = donor_message(block, o)
    return [seen[o] for o in offs]


def slots_block(msgs):
    """A text block from its slots' messages; the same message is laid once."""
    at, body, base = {}, bytearray(), 2 * len(msgs)
    for m in msgs:
        if m not in at:
            at[m] = base + len(body)
            body += m
    if base + len(body) > 0xFFFF:
        raise Unmerged("the block would be longer than its 16-bit offsets reach")
    return struct.pack("<%dH" % len(msgs), *[at[m] for m in msgs]) + bytes(body)


def _blank(m):
    return m in (b"", b"\0")


def merge_text_block(uo, up, other):
    """The block `other` (another disc's) with the lines the patch changed
    (`uo` the patch's disc's block, `up` the same block patched): (bytes,
    {"as_is", "lines", "worded": [slot]}). `as_is`: the other disc's block is
    the patch's disc's, byte for byte, and the patched block is taken whole.
    `lines`: the distinct lines taken; `worded`: the slots whose line the
    other disc words differently and the patch's line replaced. A merged
    block's dict also holds the lines themselves, for a count over sections
    that share them (a pool is in forty files): `taken`, and `other`, the
    ones of `worded`. Unmerged when
    a side is not a slot table, the patch changes the slot count, the other
    disc has fewer slots, or most of the changed lines are worded differently
    (the two blocks are then not the same script)."""
    if other == uo:
        return up, {"as_is": True, "lines": 0, "worded": []}
    mo, mp, me = text_slots(uo), text_slots(up), text_slots(other)
    if mo is None or mp is None:
        raise Unmerged("the patch's block is not a table of lines")
    if me is None:
        raise Unmerged("the other disc's block is not a table of lines")
    if len(mo) != len(mp):
        raise Unmerged("the patch changes how many lines the block has")
    if len(me) < len(mo):
        raise Unmerged("the other disc's block has fewer lines")
    out, taken, worded = list(me), set(), []
    for s in range(len(mo)):
        if mo[s] == mp[s] or (_blank(me[s]) and not _blank(mo[s])):
            continue
        out[s] = mp[s]
        if me[s] == mo[s]:
            taken.add(mp[s])
        else:
            worded.append(s)
    lines_worded = {mp[s] for s in worded}
    if len(lines_worded) > 3 and len(lines_worded) > len(taken):
        raise Unmerged("most of the lines the patch changes are other lines on the other disc")
    return slots_block(out), {"as_is": False, "lines": len(taken | lines_worded), "worded": worded,
                              "taken": taken | lines_worded, "other": lines_worded}


def merge_text_section(dest, uo, up, other):
    """merge_text_block for a section the PC keeps as text: an area's block,
    a pool (a header of two offsets and two blocks: loc_build.convert_pool),
    or an area page, which is carried only where the other disc's is the
    patch's disc's."""
    if other == uo:
        return up, {"as_is": True, "lines": 0, "worded": []}
    if dest == BLOCK_DEST:
        return merge_text_block(uo, up, other)
    if dest == POOL_DEST:
        heads = []
        for x in (uo, up, other):
            if len(x) < 8:
                raise Unmerged("a pool is shorter than its header")
            first, second = struct.unpack_from("<II", x, 0)
            if first != 8 or not 8 <= second <= len(x):
                raise Unmerged("a pool's header is not (8, the second block)")
            heads.append(second)
        a, sa = merge_text_block(uo[8:heads[0]], up[8:heads[1]], other[8:heads[2]])
        b, sb = merge_text_block(uo[heads[0]:], up[heads[1]:], other[heads[2]:])
        return struct.pack("<II", 8, 8 + len(a)) + a + b, {
            "as_is": False, "lines": sa["lines"] + sb["lines"],
            "worded": sa["worded"] + [s + 0x10000 for s in sb["worded"]],     # the second block's slots, marked
            "taken": sa.get("taken", set()) | sb.get("taken", set()), "other": sa.get("other", set()) | sb.get("other", set())}
    raise Unmerged("the other disc's page is another picture")


def twin_section(mine, theirs, i):
    """The index in `theirs` ([(type, load address)] of the other disc's EMI)
    of the section that is section i of `mine`, or None: the same index where
    the lists are as long and it has the type and address, else the section
    of that type and address by its place among them. A PSP disc's address
    has bit 31 clear (loc_build.psp_emi), so that bit is not compared."""
    def same(a, b):
        return a[0] == b[0] and a[1] | 0x80000000 == b[1] | 0x80000000
    if len(mine) == len(theirs) and same(mine[i], theirs[i]):
        return i
    a = [k for k, x in enumerate(mine) if same(x, mine[i])]
    b = [k for k, x in enumerate(theirs) if same(x, mine[i])]
    return b[a.index(i)] if len(a) == len(b) else None


def emi_with(blob, sections):
    """The EMI `blob` with {index: bytes} in place of those sections; the header's sizes follow."""
    head, body = bytearray(blob[:0x800]), []
    for i, _, _, data in emi_sections(blob):
        data = sections.get(i, data)
        struct.pack_into("<I", head, 0x10 + 16 * i, len(data))
        body.append(data + bytes(-len(data) % 0x800))
    return bytes(head) + b"".join(body)


class PortedDisc:
    """Another disc's file tree with port_text's sections in its EMIs: what
    loc_build.py is given in place of that disc (its `disc_opener`)."""

    def __init__(self, disc, sections):
        self.disc, self.sections = disc, sections      # {the disc's path of an EMI: {index: bytes}}
        self.files = disc.files

    def read(self, name):
        key = name.upper().replace("\\", "/")
        blob = self.disc.read(key)
        return emi_with(blob, self.sections[key]) if key in self.sections else blob

    def find(self, basename):
        return self.disc.find(basename)

    def sector(self, lba):
        return self.disc.sector(lba)

    def extent(self, lba, size):
        return self.disc.extent(lba, size)


def port_text(changed, lists, other):
    """The patch's text sections over the disc `other` (a region_diff.Build):
    ({the other disc's EMI path: {section index: bytes}}, stats, notes).
    `changed`: changed_sections' rows; `lists`: {EMI key: [(type, load
    address)]} of the patch's disc. stats counts the sections taken as is,
    merged and not carried, and over the merged sections the distinct lines
    taken (`lines`) and, of them, those the other disc words differently
    (`worded`); notes is one line a section that is not plainly as is, for
    the log - places, never text."""
    out, notes = {}, []
    stats = collections.Counter()
    taken, worded = set(), set()
    for (key, i), (kind, dest, da, db) in sorted(changed.items()):
        if dest not in KEPT:
            continue
        why, at, theirs = None, None, None
        if key not in other.emis:
            why = "the other disc has no such file"
        else:
            theirs = other.sections(key)
            at = twin_section(lists[key], [(k, d) for _, k, d, _ in theirs], i)
            if at is None:
                why = "the other disc's file has no such section"
        if why is None:
            try:
                data, st = merge_text_section(dest, da, db, theirs[at][3])
            except Unmerged as e:
                why = str(e)
        if why is not None:
            stats["refused"] += 1
            notes.append("%s section %d: not carried (%s)" % (key, i, why))
            continue
        stats["as_is" if st["as_is"] else "merged"] += 1
        taken |= st.get("taken", set())
        worded |= st.get("other", set())
        if st["worded"]:
            notes.append("%s section %d: the patch's line replaces a line the other disc words differently, at %s%s"
                         % (key, i, ", ".join("slot %d%s" % (s & 0xFFFF, " of the second block" if s >> 16 else "")
                                              for s in st["worded"][:12]),
                            " and %d more" % (len(st["worded"]) - 12) if len(st["worded"]) > 12 else ""))
        if data != theirs[at][3]:
            out.setdefault(other.emis[key], {})[at] = data
    stats["lines"], stats["worded"] = len(taken), len(worded)
    return out, stats, notes


def port_names(us_diff, us_dir, other_dir):
    """The names the patch changed (the kind-5 chunks of the text layer's
    difference `us_diff`), over the other language's names: ({DAT name:
    [(kind, tag, word, payload)]}, records taken, [why a chunk is not
    carried]). A record the patch changed is the patch's; every other record
    is the other language's."""
    out, taken, notes = {}, 0, []
    for name, chunks in sorted(us_diff.items()):
        mine = [c for c in chunks if c[0] == NAME_KIND]
        if not mine:
            continue
        old, theirs = {}, {}
        for d, into in ((us_dir, old), (other_dir, theirs)):
            p = os.path.join(d, name)
            if os.path.isfile(p):
                with open(p, "rb") as f:
                    into.update({(k, t): (w, b) for k, t, w, b in read_chunks(f.read(), name)})
        for kind, tag, word, new in mine:
            a, e = old.get((kind, tag)), theirs.get((kind, tag))
            if a is None or e is None or len(a[1]) != len(new) or len(e[1]) != len(new) or len(new) % NAME_RECORD:
                notes.append("%s: the names at 0x%X are not carried (the other language's table is another shape)"
                             % (name, tag))
                continue
            body = bytearray(e[1])
            for at in range(0, len(new), NAME_RECORD):
                if new[at:at + NAME_RECORD] != a[1][at:at + NAME_RECORD]:
                    body[at:at + NAME_RECORD] = new[at:at + NAME_RECORD]
                    taken += 1
            if bytes(body) != e[1]:
                out.setdefault(name, []).append((kind, tag, e[0], bytes(body)))
    return out, taken, notes


PORT_INDEX = "sections.json"


def write_ported(folder, sections):
    """port_text's sections as files of `folder`, for the text builder's child."""
    os.makedirs(folder)
    index = []
    for path in sorted(sections):
        for i in sorted(sections[path]):
            index.append({"path": path, "index": i, "file": "%04d.bin" % len(index)})
            with open(os.path.join(folder, index[-1]["file"]), "wb") as f:
                f.write(sections[path][i])
    with open(os.path.join(folder, PORT_INDEX), "w") as f:
        json.dump(index, f)


def read_ported(folder):
    with open(os.path.join(folder, PORT_INDEX)) as f:
        index = json.load(f)
    out = {}
    for row in index:
        with open(os.path.join(folder, row["file"]), "rb") as f:
            out.setdefault(row["path"], {})[row["index"]] = f.read()
    return out


def loc_source(cache, tag):
    """(build, 'cache' or 'game') of the install's `tag` text as its manifest
    records it (loc_mode's two forms), or None when the install has none."""
    import re
    p = os.path.join(cache, "manifest.toml")
    if not os.path.isfile(p):
        return None
    with open(p, encoding="utf-8") as f:
        m = re.search(r'"loc/%s", "([a-z0-9-]+):loc_build(-disc-only)?"' % re.escape(tag), f.read())
    return (m.group(1), "cache" if m.group(2) else "game") if m else None


# ---------------------------------------------------------------- the build

def layer_names(row, tag):
    return "mod-%s" % row["id"], "mod-%s-%s" % (row["id"], tag)


def port_layer(cache, rows, work, tag, build_id, changed, lists, us_diff, us_tag, enemy, warp_lba, disc_path,
               patched_path, say):
    """The patch's text over the install's `tag` text (ALSO_TEXT): ({DAT name:
    [(kind, tag, word, payload)]}, the report's counts), or None when the
    install has no `tag` text. Bad when that text's disc is not in the source
    list or cannot be read, or the text builder refuses a ported section."""
    import region_diff
    found = loc_source(cache, tag)
    if found is None:
        return None
    other_id, mode = found
    src = next((r for r in rows if r.get("build") == other_id), None)
    if src is None:
        raise Bad("the install's %s text was built from the disc %s, which is not in the source list" % (tag, other_id))
    os.makedirs(work)
    other_path = src["path"]
    if other_path.lower().endswith(".zip"):
        import player_sources
        say("Unpacking %s" % os.path.basename(other_path))
        try:
            other_path, _ = player_sources.unpack_zip(other_path, work)
        except (ValueError, OSError) as e:
            raise Bad("the disc %s cannot be unpacked: %s" % (src["path"], e))
    if not os.path.isfile(other_path):
        raise Bad("the disc %s is not there (Data, Sources lists it)" % other_path)
    try:
        other = region_diff.Build(other_id, other_path)
    except SystemExit as e:
        raise Bad("the disc %s cannot be read: %s" % (other_path, e))
    try:
        sections, stats, notes = port_text(changed, lists, other)
    finally:
        other.disc.f.close()
    for line in notes:
        say("    " + line, quiet=True)
    port_dir = os.path.join(work, "ported")
    write_ported(port_dir, sections)
    say("Building the %s text with the patch's lines (the text builder, about two minutes)" % tag)
    new_loc, names_dir = run_loc(mode, tag, build_id, disc_path, patched_path, cache, rows, work, enemy, say,
                                 warp_lba, port={"disc": other_path, "dir": port_dir})
    old_loc = os.path.join(cache, "loc", tag, "dat")
    text = diff_layer(old_loc, new_loc)
    names, records, why = port_names(us_diff, os.path.join(cache, "loc", us_tag, "dat"), old_loc)
    for line in why:
        say("    " + line, quiet=True)
    for name, chunks in names.items():
        mine = {(k, t) for k, t, _, _ in chunks}
        text[name] = [c for c in text.get(name, []) if (c[0], c[1]) not in mine] + chunks
    for name in enemy:
        with open(os.path.join(names_dir, name), "rb") as f:
            text.setdefault(name, []).extend(read_chunks(f.read(), name))
    if warp_lba is not None:
        with open(os.path.join(names_dir, WARP_TEXT_FILE), "rb") as f:
            text.setdefault("FIRST.DAT", []).append((WARP_KIND, WARP_TEXT_TAG, 0, f.read()))
    return text, {"sections_as_is": stats["as_is"], "sections_merged": stats["merged"],
                  "sections_not_carried": stats["refused"], "lines_merged": stats["lines"], "lines_worded": stats["worded"],
                  "name_records": records, "names_not_carried": len(why)}


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
        import ppf
        patch_path = os.path.join(mod_folder, row["patch"])
        with open(patch_path, "rb") as f:
            reader = ppf if ppf.is_ppf(f.read(3)) else vcdiff     # by the file's first bytes (mods.py patch_format)
        try:
            size, got = reader.apply_file(patch_path, track, patched_path, RESULT_MAX)
        except (vcdiff.PatchError, ppf.PatchError) as e:
            raise Bad("the patch does not apply to the disc %s: %s" % (os.path.basename(disc_path), e))
        except OSError as e:
            raise Bad("the scratch copy could not be written (%s)" % e)
        if not row.get("result_sha256"):
            # a patch the release's list does not have (mods.py read_patch): no disc is known that it must
            # make. The patch's own window checksums passed, so it is the patch of this disc.
            say("The patch is not in the list of known mods: what it made is not compared with a known disc")
        elif got != row["result_sha256"]:
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
        say("Looking for files the patched disc's directory does not list")
        outside = outside_files(orig.disc, patched.disc)
        out_emis = [x for x in outside if x["sections"] is not None]
        # DIV-0225: the town warp's data, carried when the mod needs TownWarp
        # (docs/town-warp.md 4); its file then leaves the "not carried" count.
        warp, warp_file = None, None
        if WARP_NAME in (row.get("needs") or []):
            warp_file = town_warp_file(outside)
            if warp_file is None:
                raise Bad("the mod needs %s, and the patched disc holds no town-warp file (one section at 0x%08X, "
                          "outside the disc's directory)" % (WARP_NAME, WARP_DEST))
            say("Reading the town warp's table")
            warp = town_warp_parts(outside_sections(patched.disc, warp_file["lba"]), game_section0(patched.disc))
            out_emis = [x for x in out_emis if x is not warp_file]

        mode = loc_mode(cache, tag, build_id)
        lists = {key: [(k, d) for _, k, d, _ in orig.sections(key)] for key in {k for k, _ in text_secs}}
        say("Building the %s text with the patch's text (the text builder, about two minutes)" % tag)
        orig.disc.f.close()
        patched.disc.f.close()
        new_loc, names_dir = run_loc(mode, tag, build_id, disc_path, patched_path, cache, rows, work, enemy, say,
                                     warp_file["lba"] if warp else None)
        text = diff_layer(os.path.join(cache, "loc", tag, "dat"), new_loc)
        us_diff = {name: list(chunks) for name, chunks in text.items()}
        for name in enemy:
            with open(os.path.join(names_dir, name), "rb") as f:
                text.setdefault(name, []).extend(read_chunks(f.read(), name))
        first_files = {n: [(k, t, 0, b) for k, t, b in c] for n, c in base.items()}
        if warp:
            with open(os.path.join(names_dir, WARP_TEXT_FILE), "rb") as f:
                text.setdefault("FIRST.DAT", []).append((WARP_KIND, WARP_TEXT_TAG, 0, f.read()))
            first_files.setdefault("FIRST.DAT", []).append((WARP_KIND, WARP_TABLE_TAG, 0, town_warp_table(warp)))

        # the patch's text over the install's other text of the same script (ALSO_TEXT); a refusal there
        # leaves that language without the mod and is said - the mod's own language is built all the same
        ports = {}
        for also in ALSO_TEXT.get(build_id, ()):
            if len(layer_names(row, also)[1]) > LAYER_MAX:
                continue                    # mods.also_tags: a layer's name holds 23 characters
            try:
                got = port_layer(cache, rows, os.path.join(work, "port-" + also), also, build_id, changed, lists,
                                 us_diff, tag, enemy, warp_file["lba"] if warp else None, disc_path, patched_path, say)
            except Bad as e:
                say("  not built for %s text: %s" % (also, e))
                continue
            if got is not None and got[0]:
                ports[also] = got

        first, second = layer_names(row, tag)
        staged = os.path.join(work, "layers")
        out = {}
        if first_files:
            out[first] = write_layer(staged, first, first_files, where)
        wrote, placed = write_image_part(cache, staged, first, image, where, parse_skip(row.get("skip") or [])) if image else ([], None)
        if wrote:
            out[first] = out.get(first, []) + [("exe/" + n, i, where, h) for i, (n, h) in enumerate(wrote)]
        if text:
            out[second] = write_layer(staged, second, text, where)
        if not out:
            raise Bad("the patch changes nothing this release can carry (%d sections changed, none of them one the "
                      "PC keeps as a chunk or a table)" % len(changed))
        for also, (files, _) in ports.items():
            out[layer_names(row, also)[1]] = write_layer(staged, layer_names(row, also)[1], files, where)
        opt = os.path.join(cache, "opt")
        os.makedirs(opt, exist_ok=True)
        for layer in (first, second) + tuple(layer_names(row, also)[1] for also in ALSO_TEXT.get(build_id, ())):
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
            if placed["zero_bytes"]:
                say("  not carried: %d bytes in %d records that the original leaves empty, outside the table's field "
                    "bounds (the patch keeps code or tables of its own there)" % (placed["zero_bytes"], placed["zero_records"]))
            if placed["skipped"]:
                say("  not carried: %d bytes of the records mod.toml's skip names" % placed["skipped"])
        if warp:
            say("  %s: the town warp's table of %d places (FIRST.DAT kind %d); %s: its text"
                % (first, len(warp["towns"]), WARP_KIND, second))
        if out_emis or any(x["sections"] is None for x in outside):
            say("  not carried: %d files outside the disc's directory (%d bytes), and %d other sectors there"
                % (len(out_emis), sum(x["sectors"] for x in out_emis) * SECTOR,
                   sum(x["sectors"] for x in outside if x["sections"] is None)))
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
        for also, (files, counts) in ports.items():
            third = layer_names(row, also)[1]
            say("  %s: %d chunks in %d files (%s text: %d text sections as the patch has them, %d with the patch's "
                "lines put into that text, %d renamed things)"
                % (third, len(out[third]), len(files), also, counts["sections_as_is"], counts["sections_merged"],
                   counts["name_records"]))
            if counts["lines_worded"]:
                say("    %d lines that the %s text words differently are the patch's lines now (the log names them)"
                    % (counts["lines_worded"], also))
            if counts["sections_not_carried"] or counts["names_not_carried"]:
                say("    not carried for %s text: %d text sections, %d name tables (the log names them)"
                    % (also, counts["sections_not_carried"], counts["names_not_carried"]))
        if sound:
            say("  not carried: %d changed sound sections in %d files (a changed sound bank is not built)"
                % (len(sound), len({k for k, _ in sound})))
        say("  %d changed overlay sections in %d files were read for their tables; their code is not carried (the PC "
            "does not run it)" % (len(overlays), len({k for k, _ in overlays})))
        if other:
            say("  not carried: %d changed sections in %d files of a kind this release does not read"
                % (len(other), len({k for k, _ in other})))
        say("  %d text and page sections went through the text builder" % len(text_secs), quiet=True)
        write_report(mod_folder, where, {
            "base_chunks": sum(len(c) for c in base.values()), "enemy_tables": len(enemy),
            "text_chunks": len(out[second]), "text_sections": len(text_secs),
            "table_bytes": placed["cells"] if placed else 0, "tables": placed["tables"] if placed else 0,
            "other_table_bytes": placed["raw"] if placed else 0,
            "town_warp_towns": len(warp["towns"]) if warp else 0,
        }, {
            "program_sections": len(overlays), "program_files": len({k for k, _ in overlays}),
            "sound_sections": len(sound), "boot_places": boot, "other_sections": len(other),
            "table_bytes": placed["kept"] if placed else 0, "pointer_bytes": notes["pointer"],
            "length_changes": len(notes["length"]),
            "zero_record_bytes": placed["zero_bytes"] if placed else 0,
            "zero_records": placed["zero_records"] if placed else 0,
            "skipped_bytes": placed["skipped"] if placed else 0,
            "outside_files": len(out_emis), "outside_bytes": sum(x["sectors"] for x in out_emis) * SECTOR,
            "outside_sectors": sum(x["sectors"] for x in outside if x["sections"] is None),
        }, {also: dict(counts, text_chunks=len(out[layer_names(row, also)[1]])) for also, (_, counts) in ports.items()})
        return out
    finally:
        shutil.rmtree(work, ignore_errors=True)


REPORT = "report.toml"


def write_report(mod_folder, where, carried, not_carried, also=None):
    """<the mod's folder>/report.toml: what this build of the mod has and what
    of the patch it does not have, as counts (docs/disc-patch-round.md 3 step
    6). Counts only: no byte of the game or of the patch. `also`: {tag:
    port_layer's counts} for each other language the text was carried over
    to, as a table [also."<tag>"]."""
    lines = ["# What this build of the mod has, and what of the author's patch it does not have.",
             "# Written by tools/disc_patch_build.py when the mod is built; counts only.",
             'source = "%s"' % where, "", "[carried]"]
    lines += ["%s = %d" % (k, v) for k, v in carried.items()]
    lines += ["", "[not_carried]"]
    lines += ["%s = %d" % (k, v) for k, v in not_carried.items()]
    for tag, counts in sorted((also or {}).items()):
        lines += ["", '[also."%s"]' % tag]
        lines += ["%s = %d" % (k, v) for k, v in counts.items()]
    tmp = os.path.join(mod_folder, REPORT + ".tmp")
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    os.replace(tmp, os.path.join(mod_folder, REPORT))


def read_report(mod_folder):
    """(carries, lacks): two sentences for the Mods tab from report.toml, each
    None when there is nothing to say; (None, None) when the file is not
    there or is not one this tool wrote."""
    import tomllib
    try:
        with open(os.path.join(mod_folder, REPORT), "rb") as f:
            rep = tomllib.load(f)
        c, n = rep["carried"], rep["not_carried"]
        def count(v, word):
            return "%d %s%s" % (v, word, "" if v == 1 else "s")

        def num(d, k):
            v = d.get(k, 0)
            if not isinstance(v, int) or isinstance(v, bool) or v < 0:
                raise ValueError(k)
            return v
        has = []
        if num(c, "enemy_tables"):
            has.append(count(num(c, "enemy_tables"), "enemy table"))
        if num(c, "base_chunks") > num(c, "enemy_tables"):
            has.append(count(num(c, "base_chunks") - num(c, "enemy_tables"), "picture"))
        if num(c, "text_sections"):
            has.append(count(num(c, "text_sections"), "text section"))
        if num(c, "table_bytes") + num(c, "other_table_bytes"):
            has.append(count(num(c, "table_bytes") + num(c, "other_table_bytes"), "byte") + " of table numbers")
        if num(c, "town_warp_towns"):
            has.append("a town warp to " + count(num(c, "town_warp_towns"), "town"))
        lack = []
        if num(n, "program_sections"):
            lack.append("the program changes in " + count(num(n, "program_sections"), "section"))
        if num(n, "boot_places"):
            lack.append("the changes to the boot program")
        if num(n, "sound_sections"):
            lack.append(count(num(n, "sound_sections"), "sound section"))
        if num(n, "table_bytes"):
            lack.append(count(num(n, "table_bytes"), "byte") + " of table numbers")
        if num(n, "pointer_bytes"):
            lack.append(count(num(n, "pointer_bytes"), "byte") + " of pointers")
        if num(n, "length_changes"):
            lack.append(count(num(n, "length_changes"), "change") + " to the length of a table")
        if num(n, "other_sections"):
            lack.append(count(num(n, "other_sections"), "other section"))
        if num(n, "zero_record_bytes"):
            lack.append("%s that the patch writes into %s of a table (they are not table numbers)"
                        % (count(num(n, "zero_record_bytes"), "byte"), count(num(n, "zero_records"), "empty record")))
        if num(n, "skipped_bytes"):
            lack.append(count(num(n, "skipped_bytes"), "byte") + " of table records that the mod skips")
        if num(n, "outside_files"):
            lack.append(count(num(n, "outside_files"), "file") + " that the disc's directory does not list")
        if num(n, "outside_sectors"):
            lack.append(count(num(n, "outside_sectors"), "other sector") + " that the disc's directory does not list")
        for tag, a in sorted(rep.get("also", {}).items()):
            got = num(a, "sections_as_is") + num(a, "sections_merged")
            has.append("its text for %s also (%s%s)" % (
                tag, count(got, "text section"),
                ", %d not" % num(a, "sections_not_carried") if num(a, "sections_not_carried") else ""))
    except (OSError, KeyError, ValueError, AttributeError, tomllib.TOMLDecodeError):
        return None, None
    return ("This build of the mod has these changes: %s." % ", ".join(has) if has else None,
            "It does not have these changes of the patch: %s." % ", ".join(lack) if lack else None)


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


def _iso(path, files, extra, total):
    """A cooked (2048-byte sector) ISO 9660 image of this test's own: the
    volume descriptor at 16, the set terminator at 17, a path table at 18, the
    root directory at 19 and `files` ({name: bytes}) from 20; `extra` ({lba:
    bytes}) laid where no directory entry names it; `total` sectors in all."""
    sec = [bytearray(SECTOR) for _ in range(total)]

    def rec(lba, size, flags, name):
        r = bytearray(33 + len(name) + (1 - len(name) % 2))
        r[0] = len(r)
        struct.pack_into("<I", r, 2, lba)
        struct.pack_into(">I", r, 6, lba)
        struct.pack_into("<I", r, 10, size)
        struct.pack_into(">I", r, 14, size)
        r[25], r[32] = flags, len(name)
        r[33:33 + len(name)] = name
        return bytes(r)

    root = rec(19, SECTOR, 2, b"\x00") + rec(19, SECTOR, 2, b"\x01")
    lba = 20
    for name, body in files.items():
        root += rec(lba, len(body), 0, name.encode() + b";1")
        for k in range(-(-len(body) // SECTOR)):
            sec[lba + k][:] = body[k * SECTOR:(k + 1) * SECTOR].ljust(SECTOR, b"\0")
        lba += -(-len(body) // SECTOR)
    sec[19][:len(root)] = root
    sec[16][0:6] = b"\x01CD001"
    struct.pack_into("<I", sec[16], 132, 10)
    struct.pack_into("<I", sec[16], 140, 18)
    struct.pack_into(">I", sec[16], 148, 18)
    sec[16][156:190] = rec(19, SECTOR, 2, b"\x00")
    sec[17][0:6] = b"\xffCD001"
    sec[18][0:10] = b"\x01\x00\x13\x00\x00\x00\x01\x00\x00\x00"
    for at, body in extra.items():
        for k in range(-(-len(body) // SECTOR)):
            sec[at + k][:] = body[k * SECTOR:(k + 1) * SECTOR].ljust(SECTOR, b"\0")
    with open(path, "wb") as f:
        f.write(b"".join(sec))


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
    # D417: a number in a table that the author changed by the distance bytes moved (8) is his edit.
    num = da.index(table) + 20                         # a word of the filler after the table, not an address
    w, = struct.unpack_from("<I", da, num)
    lowered = bytearray(both)
    lowered[num:num + 4] = word((w - 8) & 0xFFFFFFFF)
    u, unplaced, n = unmoved_section(da, bytes(lowered), fixed=[(0, 64), (num, num + 4)])
    expect(u[num:num + 4] == word((w - 8) & 0xFFFFFFFF) and n["relinked"] == 1,
           "unmoved: a number in a table lowered by the distance bytes moved is taken; the pointer outside it is still relinked")
    u, unplaced, n = unmoved_section(da, bytes(lowered), fixed=[(0, 64)])
    expect(u[num:num + 4] == da[num:num + 4] and n["relinked"] == 2,
           "unmoved: the same word outside every table is still read as a relinked address")
    u, unplaced, n = unmoved_section(da, both, fixed=[(0, 68)])
    expect(u[64:68] == da[64:68] and n["relinked"] == 1,
           "unmoved: a pointer word inside a table is still a relinked address")
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
        expect(placed == {"cells": 4, "tables": 1, "raw": 3, "kept": 1, "zero_bytes": 0, "zero_records": 0,
                          "skipped": 0} and [w[0] for w in wrote] ==
               ["things.bin", "tables.toml", "place.bin", "place.toml"],
               "image part: a byte on a class that must equal the PC's is kept and counted")
        wrote, placed = write_image_part(ic, os.path.join(root, "iout2"), "mod-x", {0x600130: 1}, "w")
        expect(wrote == [] and placed["kept"] == 1 and not os.path.exists(os.path.join(root, "iout2", "mod-x")),
               "image part: nothing a layer may write leaves no folder")

        # the zero-record rule and skip (docs/mods.md 11; D415): a catalogue of this test's own.
        # "things": 5 records of 8 bytes, +0 a count 0..3, +1 four kinds 0..2 two bytes apart, +2 free.
        tcat = {"table": [{"key": "things", "symbol": "Things", "count": 5, "stride": 8, "field": [
            {"at": 0, "type": "u8", "name": "count", "range": [0, 3], "status": "evidence"},
            {"at": 1, "type": "u8", "repeat": [3, 2], "name": "kind", "range": [0, 2], "status": "evidence"},
            {"at": 2, "type": "u8", "repeat": [3, 2], "name": "id", "status": "evidence"}]}]}
        with open(os.path.join(ic, "base", "exe", "tables", "things.bin"), "wb") as f:
            f.write(bytes(8) + bytes([2, 1, 9, 0, 0, 0, 0, 0]) + bytes(24))       # record 1 stocked, the rest empty
        with open(os.path.join(ic, "base", "exe", "tables", "tables.toml"), "w") as f:
            f.write('build = "test"\ntables = [\n  ["things", 0x600100, 0x600128, 5, 8, "%s", "test"],\n]\n'
                    % sha(bytes(8) + bytes([2, 1, 9, 0, 0, 0, 0, 0]) + bytes(24)))

        def part(out, ch, skip=()):
            wrote, placed = write_image_part(ic, os.path.join(root, out), "mod-x", ch, "w", skip, tcat)
            p = os.path.join(root, out, "mod-x", "exe", "tables.toml")
            cells = []
            if os.path.isfile(p):
                with open(p, "rb") as f:
                    cells = tomllib.load(f)["cells"]
            return cells, placed

        # record 0 filled as a list would be (count 1, kind 2, id 7): inside the bounds, carried
        cells, placed = part("z1", {0x600100: 1, 0x600101: 2, 0x600102: 7})
        expect(cells == [["things", 0, 0, 3]] and placed["zero_bytes"] == 0,
               "zero records: an empty record filled inside every field's range is carried")
        # record 2 filled with what no reader takes (a count of 0x27, a kind of 0x24): refused, counted
        cells, placed = part("z2", {0x600110: 0x27, 0x600111: 0x24, 0x600112: 0x10, 0x600101: 1})
        expect(cells == [["things", 0, 1, 1]] and placed["zero_bytes"] == 3 and placed["zero_records"] == 1,
               "zero records: an empty record filled outside a field's range is not carried, and counted")
        # a stocked record (1) changed out of bounds is the mod's: the rule is for empty records only
        cells, placed = part("z3", {0x600109: 7})
        expect(cells == [["things", 1, 1, 1]] and placed["zero_bytes"] == 0,
               "zero records: a record the original stocks is carried whatever its value")
        # one byte out of range in a later copy of a repeated field
        cells, placed = part("z4", {0x600118: 1, 0x60011D: 3})
        expect(cells == [] and placed["zero_bytes"] == 2 and placed["zero_records"] == 1,
               "zero records: the third copy of a repeated field out of range refuses the record")
        # skip: what the rule cannot see (an in-bounds fill of records 2..3), named in mod.toml
        cells, placed = part("z5", {0x600110: 1, 0x600118: 1, 0x600120: 1}, [["things", 2, 3]])
        expect(cells == [["things", 4, 0, 1]] and placed["skipped"] == 2 and placed["zero_bytes"] == 0,
               "skip: the records it names are not carried, and counted; the record after them is")
        cells, placed = part("z6", {0x600110: 0x27}, ())
        p = os.path.join(root, "z6", "mod-x")
        expect(cells == [] and not os.path.exists(p) and placed["tables"] == 0,
               "zero records: a table left with no cell writes no table file")
        refused(lambda: parse_skip([{"table": "hats", "records": [0, 1]}], tcat), "not a table of the catalogue",
                "skip: a table the catalogue does not have")
        refused(lambda: parse_skip([{"table": "things", "records": [3, 5]}], tcat), "inside things (0 to 4)",
                "skip: records past the table")
        refused(lambda: parse_skip([{"table": "things", "records": [3, 2]}], tcat), "[first, last]",
                "skip: first after last")
        refused(lambda: parse_skip([{"table": "things", "records": [1, 2], "why": "x"}], tcat), "exactly two keys",
                "skip: a key an entry does not have")
        refused(lambda: parse_skip({"table": "things"}, tcat), "must be a list", "skip: not a list")
        expect(parse_skip([{"table": "things", "records": [1, 2]}, ["things", 4, 4]], tcat) == [["things", 1, 2], ["things", 4, 4]],
               "skip: the mod.toml form and the home's own form, both read")
        with open(os.path.join(ic, "base", "exe", "tables", "things.bin"), "wb") as f:
            f.write(zeros)
        with open(os.path.join(ic, "base", "exe", "tables", "tables.toml"), "w") as f:
            f.write('build = "test"\ntables = [\n  ["things", 0x600100, 0x600128, 5, 8, "%s", "test"],\n]\n' % sha(zeros))
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

        # the report: counts in, two sentences out
        rd = os.path.join(root, "report")
        os.makedirs(rd)
        expect(read_report(rd) == (None, None), "report: none before a build")
        write_report(rd, "mod:x@1:psx-xx",
                     {"base_chunks": 4, "enemy_tables": 3, "text_chunks": 9, "text_sections": 5, "table_bytes": 20,
                      "tables": 2, "other_table_bytes": 6},
                     {"program_sections": 7, "program_files": 4, "sound_sections": 1, "boot_places": 2,
                      "other_sections": 0, "table_bytes": 0, "pointer_bytes": 0, "length_changes": 0})
        expect(read_report(rd) == (
            "This build of the mod has these changes: 3 enemy tables, 1 picture, 5 text sections, 26 bytes of table "
            "numbers.",
            "It does not have these changes of the patch: the program changes in 7 sections, the changes to the boot "
            "program, 1 sound section."), "report: what the build has and what it does not have, as sentences")
        write_report(rd, "w", {"enemy_tables": 1}, {})
        expect(read_report(rd) == ("This build of the mod has these changes: 1 enemy table.", None),
               "report: nothing left out, no second sentence")
        write_report(rd, "w", {"enemy_tables": 1},
                     {"zero_record_bytes": 149, "zero_records": 10, "skipped_bytes": 2, "outside_files": 15,
                      "outside_bytes": 67 * SECTOR, "outside_sectors": 1})
        expect(read_report(rd)[1] ==
               "It does not have these changes of the patch: 149 bytes that the patch writes into 10 empty records of "
               "a table (they are not table numbers), 2 bytes of table records that the mod "
               "skips, 15 files that the disc's directory does not list, 1 other sector that the disc's directory does "
               "not list.", "report: the empty records, the skip and the files outside the directory, as a sentence")
        with open(os.path.join(rd, REPORT), "w") as f:
            f.write('[carried]\nenemy_tables = "many"\n[not_carried]\n')
        expect(read_report(rd) == (None, None), "report: a file this tool did not write says nothing")

        # files outside the directory: two images of this test's own
        import psx_disc
        listed = _emi([(0, 0x80010000, b"L" * 3000)])                     # 3 sectors at 20..22
        hidden = _emi([(0, 0x80010000, b"H" * 100), (0, 0x80016000, b"W" * 2100)])   # 1 + 1 + 2 sectors
        op, pp = os.path.join(root, "o.iso"), os.path.join(root, "p.iso")
        _iso(op, {"A.EMI": listed}, {33: b"left by the mastering tool", 28: b"another file of the original's"}, 40)
        _iso(pp, {"A.EMI": listed}, {26: hidden, 33: b"left by the mastering tool", 35: b"not an EMI",
                                    41: _emi([(0, 0x80010000, b"Z" * 10)])}, 43)
        od, pd = psx_disc.Disc(op), psx_disc.Disc(pp)
        try:
            own = owned_sectors(pd)
            expect({16, 17, 18, 19, 20, 21, 22} <= own and 26 not in own and 23 not in own,
                   "outside: the directory's sectors - descriptors, path table, root, the file - are owned")
            got = outside_files(od, pd)
            expect([(x["lba"], x["sectors"], x["sections"]) for x in got] == [
                (26, 4, [(0, 0, 0x80010000, 100), (1, 0, 0x80016000, 2100)]),
                (35, 1, None),
                (41, 2, [(0, 0, 0x80010000, 10)])],
                "outside: an EMI no entry lists (one of its sectors over other bytes of the original's), a sector "
                "that is no EMI, one past the original's end; a sector the original has the same is not counted")
            secs = outside_sections(pd, 26)
            expect([(s[2], s[3]) for s in secs] == [(0x80010000, b"H" * 100), (0x80016000, b"W" * 2100)],
                   "outside: one file's sections read back by its first sector")
            refused(lambda: outside_sections(pd, 35), "not the start of an EMI", "outside: a sector that is no EMI")
            expect(outside_files(od, od) == [], "outside: the original against itself has none")
            expect(town_warp_file(got) is got[0] and town_warp_file(got[1:]) is None,
                   "town warp: the outside file with a section at 0x80016000, or none")
        finally:
            od.f.close()
            pd.f.close()

        # DIV-0225: the town warp's parts from code and text of this test's own
        import loc_build

        def ins(op, rs, rt, imm):
            return struct.pack("<I", (op << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF))

        def place(buf, base, at, word):
            buf[at - base:at - base + 4] = word

        def load_pair(buf, base, anchor, pair, addr):
            (o1, _, rt1, _), (o2, op2, rt2, rs2) = pair
            lo = addr & 0xFFFF
            place(buf, base, anchor + o1, ins(_LUI, 0, rt1, ((addr >> 16) + (1 if lo & 0x8000 else 0)) & 0xFFFF))
            place(buf, base, anchor + o2, ins(op2, rs2, rt2, lo))

        def warp_fixture(n=3, header=b"Go\xffwhere\x14\x02\x01\x10", names=(b"Aa", b"Bb", b"Cc"), game_n=None):
            w, d = bytearray(0x480), WARP_DEST
            load_pair(w, d, d, WARP_LOADS["header"], d + 0x300)
            load_pair(w, d, d, WARP_LOADS["more"], d + 0x320)
            load_pair(w, d, d, WARP_LOADS["cancel"], d + 0x330)
            load_pair(w, d, d, WARP_LOADS["names"], d + 0x380)
            load_pair(w, d, d, WARP_LOADS["rows"], d + 0x3A0)
            place(w, d, d + WARP_COUNT[0], ins(_SLTIU, _T3, _T5, n))
            for at, op, rt, off in WARP_FIELDS:
                place(w, d, d + at, ins(op, _T3, rt, off))
            for at, sa in WARP_STRIDE:
                place(w, d, d + at, struct.pack("<I", (10 << 11) | (sa << 6)))
            w[0x300:0x300 + len(header) + 1] = header + b"\0"
            w[0x320:0x325] = b"Next\0"
            w[0x330:0x335] = b"Back\0"
            at = 0x340
            for i, nm in enumerate(names):
                struct.pack_into("<I", w, 0x380 + 4 * i, d + at)
                w[at:at + len(nm) + 1] = nm + b"\0"
                at += len(nm) + 1
            for i in range(len(names)):
                struct.pack_into("<HHii", w, 0x3A0 + 12 * i, 10 + i, 2 * i + 1, 0x10000 * (i + 1), -0x8000 * i)
            g_base = 0x8019C000
            g = bytearray(0x400)
            load_pair(g, g_base, GAME_TOWNS, GAME_LOADS, 0x8019C300)   # a low half >= 0x8000: lui takes one more
            place(g, g_base, GAME_TOWNS + GAME_COUNT[0], ins(_SLTIU, _T5, _T7, n if game_n is None else game_n))
            for i in range(len(names)):
                struct.pack_into("<H", g, 0x300 + 2 * i, 20 + i)
            return [(0, 0, d, bytes(w))], (g_base, bytes(g))

        ws, gs = warp_fixture()
        parts = town_warp_parts(ws, gs)
        expect(parts["towns"] == [20, 21, 22] and parts["rows"] == [(10, 1, 0x10000, 0), (11, 3, 0x20000, -0x8000),
                                                                    (12, 5, 0x30000, -0x10000)]
               and parts["names"] == [b"Aa", b"Bb", b"Cc"] and parts["more"] == b"Next" and parts["cancel"] == b"Back"
               and parts["header"] == b"Go\xffwhere\x14\x02\x01\x10",
               "town warp: towns, rows, names, labels and question read through the code's own loads")
        table = town_warp_table(parts)
        expect(table[:2] == bytes([1, 3]) and len(table) == 2 + 3 * WARP_ROW
               and struct.unpack_from("<HHHHii", table, 2 + WARP_ROW) == (21, 11, 3, 0, 0x20000, -0x8000),
               "town warp: kind 21 tag 0, the version, n and 16-byte rows")
        text = town_warp_text(parts, lambda raw, nul_only: loc_build.convert_run(raw, 0, nul_only))
        strings, pos = [], 2
        while pos < len(text):
            k, = struct.unpack_from("<H", text, pos)
            strings.append(text[pos + 2:pos + 2 + k])
            pos += 2 + k
        expect(text[:2] == bytes([1, 3]) and strings == [b"Go where\x14\x02\x01\x10",b"Next\0", b"Back\0", b"Aa\0",
                                                          b"Bb\0", b"Cc\0"],
               "town warp: kind 21 tag 1, the question with its control and no NUL, then NUL-ended strings")
        refused(lambda: town_warp_text(parts, lambda raw, nul_only: (None, 0)), "a code the",
                "town warp: a string the language cannot convert")
        refused(lambda: town_warp_parts(*warp_fixture(game_n=4)), "two town counts differ",
                "town warp: the page loop and the recorder disagree on the count")
        refused(lambda: town_warp_parts(*warp_fixture(header=b"Go where")), "empty choice list",
                "town warp: a question with no choice control")
        refused(lambda: town_warp_parts(*warp_fixture(names=(b"Aa", b"B\x01", b"Cc"))), "control code",
                "town warp: a name with a control code")
        bent = bytearray(ws[0][3])
        bent[WARP_LOADS["rows"][1][0] + 2] ^= 0x20                     # another register
        refused(lambda: town_warp_parts([(0, 0, WARP_DEST, bytes(bent))], gs), "not the shape",
                "town warp: code of another shape")
        refused(lambda: town_warp_parts(ws + [(1, 0, 0x80010000, b"x")], gs), "not one section",
                "town warp: a file of more than one section")
        write_report(rd, "w", {"enemy_tables": 1, "town_warp_towns": 11}, {})
        expect(read_report(rd)[0] == "This build of the mod has these changes: 1 enemy table, a town warp to 11 towns.",
               "report: the town warp's places, as words")
    finally:
        shutil.rmtree(root, ignore_errors=True)

    # ---- the patch's text over another disc's (docs/disc-patch-round.md 11); the bytes are this test's own
    def block(msgs, offsets=None):
        """A text block: `msgs` laid in order after the table; `offsets` the slots as indexes into msgs."""
        slots = list(range(len(msgs))) if offsets is None else offsets
        at, pos = [], 2 * len(slots)
        for m in msgs:
            at.append(pos)
            pos += len(m)
        return struct.pack("<%dH" % len(slots), *[at[k] for k in slots]) + b"".join(msgs)

    expect(donor_message(b"ab\0cd\0", 0) == b"ab\0" and donor_message(b"ab\0cd\0", 3) == b"cd\0",
           "port: a message runs to its NUL")
    expect(donor_message(b"a\x07\0b\0c\0", 0) == b"a\x07\0b\0", "port: a code's argument byte is not the end, a NUL or not")
    expect(donor_message(b"q\x14\0\0\x02yes\0no\0z\0", 0) == b"q\x14\0\0\x02yes\0no\0",
           "port: a choice takes the runs of its options")
    expect(donor_message(b"abc", 1) == b"bc" and donor_message(b"abc", 3) == b"",
           "port: a message the block's end cuts short is what is there")
    us_b = block([b"one\0", b"two\0", b"three\0", b"four\0"], [0, 1, 2, 3, 0])
    expect(text_slots(us_b) == [b"one\0", b"two\0", b"three\0", b"four\0", b"one\0"], "port: a block's messages, one a slot")
    expect(text_slots(struct.pack("<2H", 4, 99) + b"x\0") is None and text_slots(b"\0") is None
           and text_slots(struct.pack("<H", 0x3000) + b"x\0") is None,
           "port: an offset outside the block, or no table: not a table of lines")
    de_b = block([b"one\0", b"TWO!\0", b"three\0", b"four\0"], [0, 1, 2, 3, 0])
    got, st = merge_text_block(us_b, de_b, us_b)
    expect(got == de_b and st["as_is"], "port: the other disc's block is the patch's disc's: the patched block whole")
    eu_b = block([b"one\0", b"two\0", b"thr-ee\0", b"four\0", b"five\0"], [0, 1, 2, 3, 0, 4])     # worded, one slot more
    got, st = merge_text_block(us_b, de_b, eu_b)
    expect(text_slots(got) == [b"one\0", b"TWO!\0", b"thr-ee\0", b"four\0", b"one\0", b"five\0"]
           and (st["as_is"], st["lines"], st["worded"], st["other"]) == (False, 1, [], set()),
           "port: the patch's line taken, the other disc's wording and its own slot kept")
    de_c = block([b"one\0", b"two\0", b"THREE\0", b"four\0"], [0, 1, 2, 3, 0])
    got, st = merge_text_block(us_b, de_c, eu_b)
    expect(text_slots(got)[2] == b"THREE\0" and st["worded"] == [2] and st["lines"] == 1,
           "port: a line the other disc words differently is the patch's line, and its slot is named")
    eu_blank = block([b"one\0", b"two\0", b"three\0", b"four\0", b""], [0, 1, 2, 3, 4])
    de_d = block([b"ONE\0", b"two\0", b"three\0", b"four\0"], [0, 1, 2, 3, 0])
    got, st = merge_text_block(us_b, de_d, eu_blank)
    expect(text_slots(got) == [b"ONE\0", b"two\0", b"three\0", b"four\0", b""] and st["worded"] == [],
           "port: a slot the other disc leaves without a message stays so")
    de_e = block([b"one\0", b"same\0", b"four\0"], [0, 1, 1, 2, 0])          # the patch points two slots at one message
    got, st = merge_text_block(us_b, de_e, eu_b)
    expect(text_slots(got)[:4] == [b"one\0", b"same\0", b"same\0", b"four\0"] and st["lines"] == 1 and st["worded"] == [2]
           and got.count(b"same\0") == 1, "port: slots the patch points at one message share it, laid once")

    def unmerged(fn, word, what):
        try:
            fn()
            expect(False, what + ": not carried")
        except Unmerged as e:
            expect(word in str(e), "%s: not carried (%s)" % (what, str(e)[:40]))

    unmerged(lambda: merge_text_block(us_b, block([b"one\0"]), eu_b), "how many lines", "port: the patch changes the slot count")
    unmerged(lambda: merge_text_block(us_b, de_b, block([b"one\0", b"two\0"])), "fewer lines", "port: the other disc has fewer slots")
    unmerged(lambda: merge_text_block(us_b, de_b, b"\0\0zz"), "other disc's block", "port: the other disc's block is no table")
    many_u = block([bytes([65 + k]) + b"\0" for k in range(8)])
    many_d = block([bytes([97 + k]) + b"!\0" for k in range(8)])
    many_e = block([bytes([48 + k]) + b"?\0" for k in range(8)])
    unmerged(lambda: merge_text_block(many_u, many_d, many_e), "other lines", "port: another script altogether")
    unmerged(lambda: slots_block([bytes([k & 0xFF, k >> 8, 1]) * 30 + b"\0" for k in range(1, 800)]), "16-bit",
             "port: a block past its offsets' reach")
    pool = lambda a, b: struct.pack("<II", 8, 8 + len(a)) + a + b
    got, st = merge_text_section(POOL_DEST, pool(us_b, us_b), pool(de_b, de_c), pool(eu_b, eu_b))
    first, second = struct.unpack_from("<II", got, 0)
    expect(first == 8 and text_slots(got[8:second])[1] == b"TWO!\0" and text_slots(got[second:])[2] == b"THREE\0"
           and st["worded"] == [0x10002] and st["lines"] == 2,
           "port: a pool's two blocks, each merged; a slot of the second is marked")
    unmerged(lambda: merge_text_section(POOL_DEST, pool(us_b, us_b), pool(de_b, de_c), b"\x09\0\0\0\0\0\0\0"), "header",
             "port: a pool with another header")
    unmerged(lambda: merge_text_section(0x0E001000, b"page", b"PAGE", b"pag3"), "another picture",
             "port: a page the other disc paints differently")
    expect(merge_text_section(0x0E001000, b"page", b"PAGE", b"page")[0] == b"PAGE", "port: a page the two discs share: the patch's")
    mine = [(6, 2), (0, 0x80010000), (0, 0x8001A000), (0, 0x8001A000)]
    expect(twin_section(mine, mine, 2) == 2 and twin_section(mine, [(6, 2), (0, 0x10000), (0, 0x1A000), (0, 0x1A000)], 3) == 3,
           "port: the same section by index; a PSP disc's address without bit 31")
    expect(twin_section(mine, [(9, 1)] + mine, 3) == 4 and twin_section(mine, [(9, 1), (0, 0x8001A000)] + mine[:2], 3) is None,
           "port: by its place among its kind in a longer list; none when the counts differ")
    us_e = _emi([(6, 2, bank), (0, 0x80010000, us_b), (0, 0x801F2C00, code)])
    de_e2 = _emi([(6, 2, bank), (0, 0x80010000, de_b), (0, 0x801F2C00, code2)])
    eu_e = _emi([(9, 1, b"extra"), (6, 2, bank), (0, 0x10000, eu_b), (0, 0x801F2C00, code)])
    expect([s[3] for s in emi_sections(emi_with(eu_e, {2: b"new" * 900}))] == [b"extra", bank, b"new" * 900, code],
           "port: an EMI with one section replaced, the sizes following")

    class _Other:                                      # region_diff.Build's two members port_text reads
        def __init__(self, files):
            self.files = files
            self.emis = {k[4:]: k for k in files}

        def sections(self, key):
            return emi_sections(self.files[self.emis[key]])

        def read(self, name):
            return self.files[name]

    po, pn = _Build({"BIN/W/AREA001.EMI": us_e, "BIN/W/AREA002.EMI": us_e, "BIN/W/AREA003.EMI": us_e}), \
        _Build({"BIN/W/AREA001.EMI": de_e2, "BIN/W/AREA002.EMI": de_e2, "BIN/W/AREA003.EMI": de_e2})
    pch = changed_sections(po, pn)
    plists = {key: [(k, d) for _, k, d, _ in emi_sections(us_e)] for key, _ in pch}
    other = _Other({"BIN/W/AREA001.EMI": eu_e, "BIN/W/AREA002.EMI": us_e,
                    "BIN/W/AREA003.EMI": _emi([(6, 2, bank), (0, 0x80010000, b"\0\0zz"), (0, 0x801F2C00, code)])})
    secs, stats, notes = port_text(pch, plists, other)
    expect(sorted(secs) == ["BIN/W/AREA001.EMI", "BIN/W/AREA002.EMI"] and list(secs["BIN/W/AREA001.EMI"]) == [2]
           and secs["BIN/W/AREA002.EMI"] == {1: de_b} and dict(stats) == {"merged": 1, "lines": 1, "worded": 0, "as_is": 1,
                                                                         "refused": 1}
           and len(notes) == 1 and "W/AREA003.EMI section 1: not carried" in notes[0],
           "port: three EMIs - one merged at its twin section, one as is, one not carried and named; code is not text")
    ported = PortedDisc(other, secs)
    expect(text_slots(emi_sections(ported.read("BIN/W/AREA001.EMI"))[2][3])[1] == b"TWO!\0"
           and ported.read("BIN/W/AREA003.EMI") == other.files["BIN/W/AREA003.EMI"],
           "port: the other disc with the sections put in; a file with none is itself")
    root = tempfile.mkdtemp(prefix="bof3x-dp-port-")
    try:
        write_ported(os.path.join(root, "p"), secs)
        expect(read_ported(os.path.join(root, "p")) == secs, "port: the sections written for the text builder's child read back")
        rec = lambda *names: b"".join(n.ljust(NAME_RECORD, b"\0") for n in names)
        for d, body in (("us", rec(b"Sword", b"Cap", b"Ring")), ("eu", rec(b"Sword", b"Hat", b"Ring"))):
            os.makedirs(os.path.join(root, d))
            with open(os.path.join(root, d, "FIRST.DAT"), "wb") as f:
                f.write(container([(NAME_KIND, 0x1000, 0, body), (NAME_KIND, 0x2000, 0, body if d == "us" else body[:16])]))
        diff = {"FIRST.DAT": [(0, 7, 0, b"text"), (NAME_KIND, 0x1000, 0, rec(b"Blade", b"Cap", b"Ring")),
                              (NAME_KIND, 0x2000, 0, rec(b"Sword", b"Cap", b"Band"))]}
        names, taken, why = port_names(diff, os.path.join(root, "us"), os.path.join(root, "eu"))
        expect(names == {"FIRST.DAT": [(NAME_KIND, 0x1000, 0, rec(b"Blade", b"Hat", b"Ring"))]} and taken == 1
               and len(why) == 1 and "0x2000" in why[0],
               "port: a renamed record is the patch's, the others the other language's; a table of another shape is named")
        with open(os.path.join(root, "manifest.toml"), "w") as f:
            f.write('overlays = [\n  ["A.DAT", "loc/en-US", "psx-us:loc_build-disc-only", "00"],\n'
                    '  ["A.DAT", "loc/en-150", "psp-eu:loc_build", "00"],\n]\n')
        expect(loc_source(root, "en-US") == ("psx-us", "cache") and loc_source(root, "en-150") == ("psp-eu", "game")
               and loc_source(root, "fr-FR") is None, "port: the disc and the way a language's text was built, from the manifest")
        write_report(root, "w", {"enemy_tables": 1}, {}, {"en-150": {"sections_as_is": 3, "sections_merged": 2,
                                                                     "sections_not_carried": 1, "lines_merged": 7, "lines_worded": 4,
                                                                     "name_records": 0, "names_not_carried": 0,
                                                                     "text_chunks": 9}})
        expect(read_report(root)[0] == "This build of the mod has these changes: 1 enemy table, its text for en-150 also "
               "(5 text sections, 1 not).", "report: the text carried over to another language, as words")
    finally:
        shutil.rmtree(root, ignore_errors=True)
    import mods
    expect(mods.KINDS["disc-patch"]["also"] == ALSO_TEXT and mods.LAYER_MAX == LAYER_MAX,
           "port: tools/mods.py lists the same other languages, and the same bound on a layer's name")
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
