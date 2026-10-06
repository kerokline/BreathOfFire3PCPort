#!/usr/bin/env python
"""Diff two builds of the game section by section, and classify every difference.

docs/region-diff.md. For two builds, pairs every EMI of one with the other's
(same path under the data root), aligns the sections of each pair by content,
order-preserving, never by address (as tools/dat_census.py does for PC against
JP; analysis/census/audit.md is why), and says of each section pair: identical,
differs, or unpaired on one side. Every differing pair is classified:

    text    the language: message blocks, the system pool, name fields, the
            item / ability name window in GAME.EMI, and image sections at the
            destinations the Western discs repaint (the glyph atlas, the kanji
            sheet, area pages with words painted on them)
    layout  the same content re-laid: code or data whose only differences are
            relocated addresses (jump targets, lui/lo pairs, pointer words), a
            size change with the same normalised content, compression
    logic   anything else: bytecode, tables of numbers, stats, code that
            differs beyond relocation. Split into logic-code and logic-data.
    art     an image section that differs at a destination not known to carry
            language - content, not text (the PSP's repaints land here)

How the classifier decides is in `classify` and docs/region-diff.md section 3:
by section kind first (type, destination, file), then by the shape of the
byte-level difference; the residue is read by hand and recorded in the doc.

    python tools/region_diff.py pair  psx-jp=DISC psx-us=DISC --out analysis/region/psx-jp_vs_psx-us.json
    python tools/region_diff.py pc    --census analysis/dat_census.json --dat bof3/DAT --disc JPDISC --out ...
    python tools/region_diff.py files DISC --out <manifest.tsv>     # per-file hashes, for fixtures.toml

DISC is a .cue, a raw .bin or a cooked .iso (tools/psx_disc.py); a PSP disc's
data root is PSP_GAME/USRDIR/<region>/, a PSX disc's BIN/. Output holds
indices, destinations, sizes, offsets, counts and truncated hashes - never
section bytes - but is derived from game data, so it goes under analysis/
(CLAUDE.md rule 1). The manifests from `files` are hashes and names only.
"""
import argparse
import collections
import difflib
import hashlib
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dat        # noqa: E402
import psx_disc   # noqa: E402

AUDIO = (6, 7, 8, 10)

# --- section kinds, from what dat.py, loc_build.py and the sibling already know

SCRIPT_DEST = 0x80010000                    # the area message block; never moves (regional-builds.md)
POOL_DESTS = (0x80014000, 0x8001A000)       # the system pool: JP layout, Western layout
ENEMY_DEST, ENEMY_HEAD, ENEMY_COUNT = 0x800E4000, 0x48, 8
ENEMY_STRIDES = (0x88, 0x8C)                # PSX, PC (the port widened the name 8 -> 12)
ENEMY_NAMES = {0x88: 8, 0x8C: 12}
# Image destinations the Western discs repaint (regional-builds.md, "Language-bearing
# artwork", the 13/2/10/11/1 split DAT_CONTAINER.md section 5 reproduces), plus the
# title menu page loc_build.py reads (DIV-0014).
LANG_ART = {0x1C080200: "the glyph atlas", 0x1E000200: "the ending / kanji font sheet",
            0x0E001000: "an area page (words painted on)", 0x0A081000: "an area page (words painted on)",
            0x1A080400: "DEMO's language page", 0x1C000200: "the title menu page"}
JR_RA = 0x03E00008
TILE = 2048                                 # a 32 x 32 halfword tile (DAT_CONTAINER.md section 2)


def h16(b):
    return hashlib.sha256(b).hexdigest()[:16]


def is_code(b):
    """The sibling's emi_survey.classify: jr ra and addiu sp,sp,-N at a code's density."""
    n = len(b) // 4
    if not n:
        return False
    jr = pro = 0
    for (w,) in struct.iter_unpack("<I", b[:n * 4]):
        if w == JR_RA:
            jr += 1
        elif (w >> 16) == 0x27BD and (w & 0x8000):
            pro += 1
    return jr >= 4 and pro >= 4 and 1000.0 * (jr + pro) / n >= 2.0


# ---------------------------------------------------------------- reading builds

class Build:
    """EMIs of one disc, keyed by path under the data root."""

    def __init__(self, bid, path):
        self.id, self.path = bid, path
        self.disc = psx_disc.Disc(path)
        self.emis = {}
        for p in self.disc.files:
            if not p.endswith(".EMI"):
                continue
            m = re.match(r"(?:BIN/|PSP_GAME/USRDIR/[^/]+/)(.*)$", p)
            if m:
                self.emis[m.group(1)] = p
        boots = [p for p in self.disc.files if re.match(r"S[LC][PUE][SM]_\d{3}\.\d{2}$", p)]
        self.boot = boots[0] if boots else None

    def sections(self, key):
        blob = self.disc.read(self.emis[key])
        return emi_sections_blob(blob)


def emi_sections_blob(blob):
    if blob[8:16] != b"MATH_TBL":
        raise ValueError("not an EMI")
    count, = struct.unpack_from("<I", blob, 0)
    out, pos = [], 0x800
    for i in range(count):
        size, dest = struct.unpack_from("<II", blob, 0x10 + 16 * i)
        type_id, = struct.unpack_from("<H", blob, 0x1C + 16 * i)
        out.append((i, type_id, dest, blob[pos:pos + size]))
        pos += ((size + 0x7FF) >> 11) * 0x800
    return out


# ---------------------------------------------------------------- alignment

def align(a, b):
    """Order-preserving alignment of two section lists, skips allowed on both
    sides. Legal pair: same type. Score: identical content 8, equal size 2,
    equal destination 1, any pairing 1. Decided by content first; destinations
    shift between regions, so they only break ties."""
    n, m = len(a), len(b)
    best = [[0] * (m + 1) for _ in range(n + 1)]
    back = [[None] * (m + 1) for _ in range(n + 1)]
    ha = [hashlib.sha256(s[3]).digest() for s in a]
    hb = [hashlib.sha256(s[3]).digest() for s in b]
    for i in range(n + 1):
        for j in range(m + 1):
            if i == 0 and j == 0:
                continue
            cands = []
            if i:
                cands.append((best[i - 1][j], "a"))
            if j:
                cands.append((best[i][j - 1], "b"))
            if i and j and a[i - 1][1] == b[j - 1][1]:
                s = 1 + (8 if ha[i - 1] == hb[j - 1] else 0) + (2 if len(a[i - 1][3]) == len(b[j - 1][3]) else 0) \
                    + (1 if a[i - 1][2] == b[j - 1][2] else 0)
                cands.append((best[i - 1][j - 1] + s, "p"))
            best[i][j], back[i][j] = max(cands, key=lambda c: c[0])
    out, i, j = [], n, m
    while i or j:
        k = back[i][j]
        if k == "p":
            out.append((i - 1, j - 1)); i -= 1; j -= 1
        elif k == "a":
            out.append((i - 1, None)); i -= 1
        else:
            out.append((None, j - 1)); j -= 1
    return out[::-1]


# ---------------------------------------------------------------- the classifier

def norm(w):
    """A MIPS word with its address-bearing fields masked: J/JAL targets, and
    every I-type immediate. R-type (op 0) and COP words are kept whole."""
    op = w >> 26
    if op in (2, 3):
        return op << 26
    if op in (0, 0x10, 0x11, 0x12, 0x13):
        return w
    return w & 0xFFFF0000


def word_kind(x, y):
    """Why two aligned words with the same normalised form differ."""
    op = x >> 26
    if 0x80000000 <= x < 0x80200000 and 0x80000000 <= y < 0x80200000:
        return "pointer"
    if op in (2, 3):
        return "jump"
    if op == 0x0F:                                   # lui
        return "hi"
    rs = (x >> 21) & 31
    if op in (0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2B, 0x22, 0x26, 0x2A, 0x2E, 0x32, 0x3A):
        return "lo" if rs else "value"              # loads / stores off a base register
    if op in (0x08, 0x09, 0x0D):                     # addi, addiu, ori
        return "lo" if rs else "value"
    if op in (1, 4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17):
        return "branch"
    return "value"                                   # andi, slti, sltiu, xori, ...


def code_diff(x, y):
    """Classify two code sections. Returns (class, detail)."""
    wa = [w for (w,) in struct.iter_unpack("<I", x[:len(x) // 4 * 4])]
    wb = [w for (w,) in struct.iter_unpack("<I", y[:len(y) // 4 * 4])]
    na, nb = [norm(w) for w in wa], [norm(w) for w in wb]
    kinds = collections.Counter()
    changed = 0
    blocks = []
    if na == nb:
        for p, q in zip(wa, wb):
            if p != q:
                kinds[word_kind(p, q)] += 1
    else:
        sm = difflib.SequenceMatcher(None, na, nb, autojunk=False)
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == "equal":
                for p, q in zip(wa[i1:i2], wb[j1:j2]):
                    if p != q:
                        kinds[word_kind(p, q)] += 1
            else:
                changed += max(i2 - i1, j2 - j1)
                blocks.append((tag, i1 * 4, (i2 - i1) * 4, j1 * 4, (j2 - j1) * 4))
    real = kinds["value"] + kinds["branch"]
    detail = {"reloc_words": {k: v for k, v in kinds.items() if k not in ("value", "branch")},
              "value_words": kinds["value"], "branch_words": kinds["branch"],
              "changed_words": changed, "blocks": blocks[:40], "n_blocks": len(blocks)}
    if not changed and not real:
        return "layout", detail
    return "logic-code", detail


def text_block_detail(x, y):
    """A message block: the u16 slot table's lengths."""
    def slots(b):
        return struct.unpack_from("<H", b, 0)[0] // 2 if len(b) >= 2 else 0
    return {"slots": [slots(x), slots(y)]}


def enemy_diff(x, y):
    """Enemy records: compare everything but the name fields."""
    sa = (len(x) - ENEMY_HEAD) // ENEMY_COUNT
    sb = (len(y) - ENEMY_HEAD) // ENEMY_COUNT
    if sa not in ENEMY_STRIDES or sb not in ENEMY_STRIDES or len(x) != ENEMY_HEAD + 8 * sa or len(y) != ENEMY_HEAD + 8 * sb:
        return None
    if x[:ENEMY_HEAD] != y[:ENEMY_HEAD]:
        return "logic-data", {"what": "enemy table header differs",
                              "header_bytes": sum(p != q for p, q in zip(x[:ENEMY_HEAD], y[:ENEMY_HEAD]))}
    names = stats = 0
    bad_records = []
    for k in range(ENEMY_COUNT):
        ra = x[ENEMY_HEAD + k * sa:][:sa]
        rb = y[ENEMY_HEAD + k * sb:][:sb]
        na, nb = ENEMY_NAMES[sa], ENEMY_NAMES[sb]
        if ra[:na] != rb[:nb]:
            names += 1
        d = sum(p != q for p, q in zip(ra[na:], rb[nb:]))
        if d:
            stats += d
            bad_records.append({"record": k, "bytes": d,
                                "offsets": [i + na for i, (p, q) in enumerate(zip(ra[na:], rb[nb:])) if p != q][:16]})
    detail = {"names_differ": names, "stat_bytes_differ": stats, "records": bad_records}
    if stats:
        return "logic-data", detail
    return ("text" if sa == sb else "layout+text"), detail


def classify(sa, sb, file_key):
    """(class, what, detail) for two differing sections of the same type."""
    _, t, da, x = sa
    _, _, db, y = sb
    if t == 3:
        ndiff = sum(x[i:i + TILE] != y[i:i + TILE] for i in range(0, max(len(x), len(y)), TILE))
        what = LANG_ART.get(da) or LANG_ART.get(db)
        det = {"tiles_differ": ndiff, "tiles": (max(len(x), len(y)) + TILE - 1) // TILE}
        if len(x) != len(y):
            det["size_change"] = True
        return ("text", what, det) if what else ("art", "an image page", det)
    if t in AUDIO:
        what = {6: "a sound bank header (VH)", 7: "sound samples (VB)", 8: "a sound bank table",
                10: "a sequence (SEQ)"}[t]
        return "logic-data", what, {"bytes_differ": sum(p != q for p, q in zip(x, y)),
                                    "size": [len(x), len(y)]}
    if da == SCRIPT_DEST and db == SCRIPT_DEST:
        return "text", "the area message block", text_block_detail(x, y)
    if da in POOL_DESTS and db in POOL_DESTS:
        return "text", "the system message pool", {}
    if da == ENEMY_DEST and db == ENEMY_DEST:
        r = enemy_diff(x, y)
        if r:
            return r[0], "the enemy / formation table", r[1]
    if t == 1:
        return "layout", "compressed data", {"size": [len(x), len(y)]}
    if is_code(x) and is_code(y):
        cls, det = code_diff(x, y)
        what = "overlay code"
        if file_key.endswith("GAME.EMI"):
            what = "GAME.EMI code (holds the item / ability name window)"
        return cls, what, det
    # data: pointer-relocation only?
    if len(x) == len(y) and len(x) % 4 == 0:
        kinds = collections.Counter()
        for (p,), (q,) in zip(struct.iter_unpack("<I", x), struct.iter_unpack("<I", y)):
            if p != q:
                kinds["pointer" if (0x80000000 <= p < 0x80200000 and 0x80000000 <= q < 0x80200000) else "other"] += 1
        if kinds and not kinds["other"]:
            return "layout", "data with relocated pointers", {"pointer_words": kinds["pointer"]}
    nb = sum(p != q for p, q in zip(x, y))
    det = {"bytes_differ": nb, "size": [len(x), len(y)]}
    if len(x) == len(y):
        offs = [i for i, (p, q) in enumerate(zip(x, y)) if p != q]
        det["first"], det["last"] = (offs[0], offs[-1]) if offs else (None, None)
    return "logic-data", "data", det


# ---------------------------------------------------------------- the pair

def diff_pair(A, B):
    res = {"a": {"id": A.id, "path": os.path.basename(A.path), "boot": A.boot},
           "b": {"id": B.id, "path": os.path.basename(B.path), "boot": B.boot},
           "files": {}, "only_a": sorted(set(A.emis) - set(B.emis)), "only_b": sorted(set(B.emis) - set(A.emis))}
    for key in sorted(set(A.emis) & set(B.emis)):
        sa, sb = A.sections(key), B.sections(key)
        rows = []
        for i, j in align(sa, sb):
            if j is None:
                s = sa[i]
                rows.append({"a": s[0], "b": None, "type": s[1], "a_dest": "%08X" % s[2], "a_size": len(s[3]),
                             "a_hash": h16(s[3]), "verdict": "only_a", "zero": not any(s[3])})
                continue
            if i is None:
                s = sb[j]
                rows.append({"a": None, "b": s[0], "type": s[1], "b_dest": "%08X" % s[2], "b_size": len(s[3]),
                             "b_hash": h16(s[3]), "verdict": "only_b", "zero": not any(s[3])})
                continue
            x, y = sa[i], sb[j]
            row = {"a": x[0], "b": y[0], "type": x[1], "a_dest": "%08X" % x[2], "b_dest": "%08X" % y[2],
                   "a_size": len(x[3]), "b_size": len(y[3]), "a_hash": h16(x[3])}
            if x[3] == y[3]:
                row["verdict"] = "identical"
            else:
                row["b_hash"] = h16(y[3])
                row["verdict"] = "differs"
                row["class"], row["what"], row["detail"] = classify(x, y, key)
            rows.append(row)
        res["files"][key] = rows
    return res


def summarise(res):
    c = collections.Counter()
    for rows in res["files"].values():
        for r in rows:
            c["sections"] += 1
            if r["verdict"] == "differs":
                c[r["class"]] += 1
            else:
                c[r["verdict"]] += 1
    return dict(c)


def cmd_pair(a):
    (ia, pa), (ib, pb) = (s.split("=", 1) for s in (a.a, a.b))
    A, B = Build(ia, pa), Build(ib, pb)
    res = diff_pair(A, B)
    res["summary"] = summarise(res)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w") as f:
        json.dump(res, f, indent=1)
    print("%s vs %s: %d common EMIs, only %s %d, only %s %d"
          % (ia, ib, len(res["files"]), ia, len(res["only_a"]), ib, len(res["only_b"])))
    print("  ", res["summary"])


def cmd_pc(a):
    """The PC DAT tree against the JP disc: the census's own pairing, classified."""
    census = json.load(open(a.census))
    J = Build("psx-jp", a.disc)
    res = {"a": {"id": "psx-jp", "path": os.path.basename(a.disc), "boot": J.boot},
           "b": {"id": "pc-zh", "path": "DAT/"}, "files": {}, "census": os.path.basename(a.census)}
    for stem, rec in sorted(census.items()):
        keys = [k for k in J.emis if k.rsplit("/", 1)[-1] == stem + ".EMI"]
        if not rec["pairs"] or len(keys) != 1:
            continue
        sec = {s[0]: s for s in J.sections(keys[0])}
        blob, chunks = dat.load(os.path.join(a.dat, stem + ".DAT"))
        rows = []
        for p in rec["pairs"]:
            x = sec[p["emi"]]
            c = chunks[p["dat"]]
            yb = blob[c.offset:c.offset + c.size]
            row = {"a": p["emi"], "b": p["dat"], "type": x[1], "a_dest": p["dest"], "b_tag": p["tag"],
                   "a_size": len(x[3]), "b_size": c.size}
            if p["identical"]:
                row["verdict"] = "identical"
            else:
                row["verdict"] = "differs"
                y = (p["dat"], x[1] if c.kind == 1 else 0, x[2], yb)
                if x[1] == 1:
                    row["class"], row["what"], row["detail"] = "layout", "compressed on the disc, decompressed on the PC", {}
                else:
                    row["class"], row["what"], row["detail"] = classify(x, (y[0], x[1], x[2], yb), keys[0])
            rows.append(row)
        for d in rec["dropped"]:
            rows.append({"a": d["emi"], "b": None, "type": d["type"], "a_dest": d["dest"], "a_size": d["size"],
                         "verdict": "only_a", "zero": d["all_zero"]})
        res["files"][keys[0]] = rows
    res["summary"] = summarise(res)
    with open(a.out, "w") as f:
        json.dump(res, f, indent=1)
    print("pc-zh vs psx-jp:", res["summary"])


def cmd_files(a):
    d = psx_disc.Disc(a.disc)
    n = tot = 0
    lines = []
    for p in sorted(d.files):
        b = d.read(p)
        lines.append("%s\t%d\t%s" % (p, len(b), hashlib.sha256(b).hexdigest()))
        n += 1; tot += len(b)
    body = "\n".join(lines) + "\n"
    with open(a.out, "w", newline="\n") as f:
        f.write(body)
    print("%d files, %d bytes; manifest sha256 %s" % (n, tot, hashlib.sha256(body.encode()).hexdigest()))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("pair"); p.add_argument("a"); p.add_argument("b"); p.add_argument("--out", required=True)
    p.set_defaults(fn=cmd_pair)
    p = sub.add_parser("pc"); p.add_argument("--census", required=True); p.add_argument("--dat", required=True)
    p.add_argument("--disc", required=True); p.add_argument("--out", required=True); p.set_defaults(fn=cmd_pc)
    p = sub.add_parser("files"); p.add_argument("disc"); p.add_argument("--out", required=True)
    p.set_defaults(fn=cmd_files)
    a = ap.parse_args()
    return a.fn(a) or 0


if __name__ == "__main__":
    sys.exit(main())
