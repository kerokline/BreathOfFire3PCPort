#!/usr/bin/env python
"""Find BOF3.exe's data tables in every other build, by their bytes (unified-data step 5).

The twin method in bulk (docs/exe-tables-by-build.md). BOF3.exe's .data is
aligned against one build's boot executable and every EMI code or data
section, and every symbols.toml [[data]] entry in it that src/ names is placed
by the result:

    python tools/exe_twins.py survey --game DIR --disc DISC [--disc DISC ...] --out analysis/exe_twins
    python tools/exe_twins.py summary analysis/exe_twins/*.json
    python tools/exe_twins.py show analysis/exe_twins/psx-us.json Shop_Records Char_ExpTable
    python tools/exe_twins.py map analysis/exe_twins/*.json       # exe_maps/<build>.tsv (committed)

A word of the PC's that points into the PC image is a wildcard (the other
build's pointer is its own address); everything else must be equal. Runs: a
16-byte key every 16 bytes of .data, a seed where its hits fall at no more
than eight addresses, extended both ways while the bytes agree. A table is
where the run holding its first byte puts it; a table no run reaches is
searched for alone (its wildcard-free head), several equal places decided by
the neighbours' displacement, their file, or the file its name names. A
row's `len` is how much of the table's extent (to the next symbol) the other
build holds verbatim from that address. `map` turns the runs into
exe_maps/<build>.tsv.

Sources searched, per build:
    PSX  the boot EXE SYSTEM.CNF names (addresses as loaded), and every EMI
         section of type 0 bound for main RAM (0x80xxxxxx destinations)
    PSP  PSP_GAME/SYSDIR/BOOT.BIN's first program header (link-time offsets
         from 0, tools/psp_elf.py), and the same EMI sections under USRDIR/
         (their destinations with bit 31 restored, as region_diff.nd does)
The type-1 arenas (art) and the audio and image sections are not searched.

The output holds names, addresses, lengths and counts - no table bytes - but
it is derived from the player's files, so it goes under analysis/ (CLAUDE.md
rule 1). Which build a disc is comes from its boot line (PSX SYSTEM.CNF, PSP
UMD_DATA.BIN) against fixtures.toml's serials.
"""
import argparse
import collections
import json
import os
import re
import struct
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import psx_disc      # noqa: E402

RDATA, DATA, BSS = 0x5C4000, 0x5DA000, 0x676000   # BOF3.exe: .rdata, .data, .data's raw end
IMAGE = (0x401000, 0x940000)                      # a word in here is a pointer into the PC image
KEY, MAX_EXTENT = 16, 0x10000


def fail(msg):
    sys.exit("exe_twins: " + msg)


# ------------------------------------------------------------------ the PC side

def pc_image(game):
    with open(os.path.join(game, "BOF3.exe"), "rb") as f:
        exe = f.read()
    pe = struct.unpack_from("<I", exe, 0x3C)[0]
    nsec, optsz = struct.unpack_from("<H", exe, pe + 6)[0], struct.unpack_from("<H", exe, pe + 20)[0]
    base = struct.unpack_from("<I", exe, pe + 24 + 28)[0]
    secs = []
    for i in range(nsec):
        vsz, rva, rsz, raw = struct.unpack_from("<IIII", exe, pe + 24 + optsz + i * 40 + 8)
        secs.append((base + rva, exe[raw:raw + rsz]))

    def read(va, size):
        for start, blob in secs:
            if start <= va < start + len(blob):
                return blob[va - start:va - start + size]
        return b""
    return read


def src_words():
    words = collections.Counter()
    for r, _, fs in os.walk(os.path.join(ROOT, "src")):
        for f in fs:
            if f.endswith((".cpp", ".h")):
                with open(os.path.join(r, f), encoding="utf-8", errors="replace") as fh:
                    words.update(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", fh.read()))
    return words


def candidates(read):
    """(entry, extent bytes, wildcard mask) for every named, initialised,
    non-zero, not-all-code-pointer [[data]] entry src/ uses."""
    with open(os.path.join(ROOT, "symbols.toml"), "rb") as f:
        data = sorted(tomllib.load(f)["data"], key=lambda e: e["pc"])
    words = src_words()
    out = []
    for i, e in enumerate(data):
        if not RDATA <= e["pc"] < BSS or not words[e["name"]]:
            continue
        end = data[i + 1]["pc"] if i + 1 < len(data) else BSS
        end = min(end, e["pc"] + MAX_EXTENT, BSS if e["pc"] >= DATA else DATA)
        b, wild = masks(read, e["pc"], end)
        size = CSIZE[e["ctype"]] * e["count"] if "count" in e and e.get("ctype") in CSIZE else len(b)
        if not b or not any(x for x, w in zip(b[:size], wild[:size]) if not w):
            continue                                  # zeros, or pointers and zeros: our code's tables
        out.append((e, b, wild))
    return out


# ------------------------------------------------------------- the other build

def build_of(disc, builds):
    if "SYSTEM.CNF" in disc.files:
        line = disc.read("SYSTEM.CNF").split(b"\n")[0].upper().replace(b".", b"")
    elif "UMD_DATA.BIN" in disc.files:
        line = disc.read("UMD_DATA.BIN").split(b"|")[0].upper().replace(b"-", b"_")
    else:
        fail("no SYSTEM.CNF or UMD_DATA.BIN: not a PSX or PSP disc of this game")
    for b in builds:
        serial = b.get("serial", "").upper().replace("-", "_").encode()
        if serial and serial in line:
            return b
    fail("the disc's boot line %r names no build in fixtures.toml" % line)


def emi_sections(blob):
    count, = struct.unpack_from("<I", blob, 0)
    pos = 0x800
    for i in range(count):
        size, dest = struct.unpack_from("<II", blob, 0x10 + 16 * i)
        kind, = struct.unpack_from("<H", blob, 0x1C + 16 * i)
        yield i, kind, dest, blob[pos:pos + size]
        pos += ((size + 0x7FF) >> 11) * 0x800


def sources(disc):
    """(file, section, base address, bytes) for everything searched."""
    out = []
    psp = "SYSTEM.CNF" not in disc.files
    if not psp:
        boot = disc.read("SYSTEM.CNF").split(b"\n")[0].split(b":")[-1].strip().lstrip(b"\\").split(b";")[0]
        name = boot.decode().replace("\\", "/")
        exe = disc.read(name)
        t_addr, t_size = struct.unpack_from("<II", exe, 0x18)
        out.append(("BOOT", None, t_addr, exe[0x800:0x800 + t_size]))
    else:
        elf = disc.read("PSP_GAME/SYSDIR/BOOT.BIN")
        if elf[:4] != b"\x7fELF":
            fail("BOOT.BIN is not a plain ELF")
        phoff, = struct.unpack_from("<I", elf, 28)
        _, off, va, _, fs, _, _, _ = struct.unpack_from("<8I", elf, phoff)
        out.append(("BOOT.BIN", None, va, elf[off:off + fs]))
    for p in sorted(disc.files):
        if not p.endswith(".EMI"):
            continue
        blob = disc.read(p)
        if blob[8:16] != b"MATH_TBL":
            continue
        rel = re.sub(r"^(BIN/|PSP_GAME/USRDIR/[^/]+/)", "", p)
        for i, kind, dest, sec in emi_sections(blob):
            if kind == 0 and 0x10000 <= dest < 0x800000 and psp:
                dest |= 0x80000000                    # the PSP clears bit 31 (region_diff.nd)
            if kind == 0 and dest >> 24 == 0x80 and sec:
                out.append((rel, i, dest, sec))
    return out


def agree(b, wild, blob, start):
    """How many bytes of the PC extent `b` the blob holds from `start`."""
    n = min(len(b), len(blob) - start)
    i = 0
    while i < n and (wild[i] or blob[start + i] == b[i]):
        i += 1
    return i


def solid(b, wild, n):
    """The non-zero, non-wildcard bytes among the first n: what a match rests on."""
    return sum(1 for x, w in zip(b[:n], wild[:n]) if x and not w)


def masks(read, lo, hi):
    """The PC bytes of [lo, hi) and their wildcard mask (words pointing into the image)."""
    b = read(lo, hi - lo)
    wild = bytearray(len(b))
    for o in range((-lo) % 4, len(b) - 3, 4):
        if IMAGE[0] <= struct.unpack_from("<I", b, o)[0] < IMAGE[1]:
            wild[o:o + 4] = b"\1\1\1\1"
    return b, bytes(wild)


SEED_STEP, SEED_MAX_HITS = 16, 8


def runs(pc, wild, srcs):
    """Every stretch of the PC's initialised data that a source holds verbatim
    (pointer words wild): seeded by 16-byte keys every 16 bytes that occur at
    most SEED_MAX_HITS times in the build, extended both ways while the bytes
    agree. [(pc0, pc1, source index, address of pc0)], longest first."""
    import ahocorasick
    lo, data = pc
    auto, keys = ahocorasick.Automaton(), collections.defaultdict(list)
    for o in range(0, len(data) - KEY + 1, SEED_STEP):
        k = data[o:o + KEY]
        if any(wild[o:o + KEY]) or len(set(k)) < 4:
            continue
        ks = k.decode("latin1")
        if ks not in keys:
            auto.add_word(ks, ks)
        keys[ks].append(o)
    auto.make_automaton()
    hits = collections.defaultdict(list)
    for si, (_, _, _, blob) in enumerate(srcs):
        for end, ks in auto.iter(blob.decode("latin1")):
            hits[ks].append((si, end - KEY + 1))
    # An overlay copied into many files (the battle engine is in BATTLE.EMI and
    # in every BOSS file, at one address) is one place: a key is a seed when
    # its hits fall at no more than SEED_MAX_HITS distinct addresses, and each
    # address is extended in its first source by `rank`.
    seeds = []
    for ks, hs in hits.items():
        by_addr = collections.defaultdict(list)
        for si, at in hs:
            by_addr[srcs[si][2] + at].append((si, at))
        if len(by_addr) > SEED_MAX_HITS:
            continue
        for group in by_addr.values():
            si, at = min(group, key=lambda h: rank(srcs[h[0]][0]))
            for o in keys[ks]:
                seeds.append((o, si, at, len(group)))
    seeds.sort()
    found, covered = [], set()
    for o, si, at, copies in seeds:
        delta = (si, at - o)
        if (delta, o // SEED_STEP) in covered:
            continue
        blob = srcs[si][3]
        a = o
        while a > 0 and at - (o - a) > 0 and (wild[a - 1] or data[a - 1] == blob[at - (o - a) - 1]):
            a -= 1
        b = o
        while b < len(data) and at + (b - o) < len(blob) and (wild[b] or data[b] == blob[at + (b - o)]):
            b += 1
        while a < b and wild[a]:                      # a run neither starts nor ends on a wildcard
            a += 1
        while b > a and wild[b - 1]:
            b -= 1
        for q in range(a // SEED_STEP, b // SEED_STEP + 1):
            covered.add((delta, q))
        found.append((lo + a, lo + b, si, srcs[si][2] + at - (o - a), copies))
    best = {}
    for r in found:
        k = r[:4]
        best[k] = max(best.get(k, 0), r[4])
    return sorted(((k + (c,)) for k, c in best.items()), key=lambda r: (-(r[1] - r[0]), r[0]))


def rank(file):
    """Which of several identical copies stands for them: the boot
    executable, then GAME.EMI, then BATTLE.EMI, then by name."""
    order = {"BOOT": 0, "BOOT.BIN": 0, "ETC/GAME.EMI": 1, "BATTLE/BATTLE.EMI": 2}
    return (order.get(file, 3), file)


CSIZE = {"char": 1, "signed char": 1, "unsigned char": 1, "short": 2, "unsigned short": 2,
         "const unsigned short": 2, "int": 4, "long": 4, "unsigned int": 4, "unsigned long": 4,
         "std::uint32_t": 4, "float": 4, "void *": 4, "unsigned char *": 4, "const unsigned char *": 4,
         "const char *": 4, "unsigned long *": 4}
NEIGHBOUR = 0x1000


def judge(row, cover, need):
    cover.sort(key=lambda h: (-h["len"], rank(h["file"]), h["addr"]))
    row["hits"] = cover[:12]
    row["n_hits"] = len(cover)
    best = [h for h in cover if h["len"] == cover[0]["len"]]
    row["best_len"] = cover[0]["len"]
    row["verdict"] = ("whole" if cover[0]["len"] >= need else "part") + \
        ("" if len({h["addr"] for h in best}) == 1 else "-multi")


def affinity(name):
    """The file a symbol's name ties it to: AreaNN_ / WorldMapNN_ to AREA0NN,
    ScenaNN_ to SCENANN, MagicNNN_ to MAGICNNN. A tie-break only."""
    m = re.match(r"(?:Area|WorldMap)(\d+)_", name)
    if m:
        return "AREA%03d.EMI" % int(m.group(1))
    m = re.match(r"Scena(\d+)_", name)
    if m:
        return "SCENA%02d.EMI" % int(m.group(1))
    m = re.match(r"Magic(\d+)", name)
    if m:
        return "MAGIC%03d.EMI" % int(m.group(1))
    return None


def resolve_multi(rows):
    """A table several places hold equally: the place its neighbours in the
    PC's .data predict (same source, same displacement), else the source both
    nearest single-placed neighbours are in, else the file its name names."""
    import bisect
    single = sorted((r["pc"], r["hits"][0]) for r in rows if r["verdict"] in ("whole", "part"))
    spcs = [x[0] for x in single]
    for r in rows:
        if not r["verdict"].endswith("-multi"):
            continue
        best = [h for h in r["hits"] if h["len"] == r["best_len"]]
        j = bisect.bisect_left(spcs, r["pc"])
        prev = single[j - 1] if j > 0 and r["pc"] - single[j - 1][0] <= 0x4000 else None
        nxt = single[j] if j < len(single) and single[j][0] - r["pc"] <= 0x4000 else None
        how, pick = None, []
        for nb in (prev, nxt):
            if nb and not pick:
                want = nb[1]["addr"] + r["pc"] - nb[0]
                pick = [h for h in best if h["addr"] == want and h["file"] == nb[1]["file"]]
                how = "neighbour displacement"
        if not pick and prev and nxt and prev[1]["file"] == nxt[1]["file"]:
            pick = [h for h in best if h["file"] == prev[1]["file"]]
            how = "neighbours' file"
        if not pick:
            aff = affinity(r["name"])
            pick = [h for h in best if aff and h["file"].endswith("/" + aff)]
            how = "name"
        if len(pick) == 1:
            r["hits"].remove(pick[0])
            r["hits"].insert(0, pick[0])
            r["verdict"] = r["verdict"][:-len("-multi")]
            r["picked_by"] = how
            r["alternatives"] = len(best) - 1


def survey(read, disc, build):
    import bisect
    cands = candidates(read)
    srcs = sources(disc)
    allruns = []
    # .data only: .rdata is the compiler's constants, strings and the import
    # table (its three named entries are DirectInput's), and an arithmetic
    # sequence there matched a spell file by chance in the first survey.
    b, w = masks(read, DATA, BSS)
    allruns += runs((DATA, b), w, srcs)
    allruns.sort(key=lambda r: r[0])
    starts = [r[0] for r in allruns]
    rows = []
    # Pass 1: the runs. A table is where the run covering its first byte puts it.
    for e, b, wild in cands:
        pc, ext = e["pc"], len(b)
        size = CSIZE[e["ctype"]] * e["count"] if "count" in e and e.get("ctype") in CSIZE else None
        need = min(size, ext) if size else ext
        row = {"name": e["name"], "pc": pc, "extent": ext, "size": size, "hits": []}
        cover = [{"file": srcs[r[2]][0], "section": srcs[r[2]][1], "addr": r[3] + pc - r[0],
                  "len": min(r[1] - pc, ext), "copies": r[4], "how": "run"}
                 for r in allruns[:bisect.bisect_right(starts, pc)] if r[0] <= pc < r[1]]
        if cover:
            judge(row, cover, need)
        rows.append((row, b, wild, need))
    # Pass 2: what no run reaches, searched for alone (its wildcard-free head),
    # and where several places hold it, the one a neighbour's run predicts.
    located = sorted((r["pc"], r["hits"][0]) for r, _, _, _ in rows
                     if r["hits"] and r["verdict"] in ("whole", "part"))
    lpcs = [x[0] for x in located]
    import ahocorasick
    todo, auto, places = [], ahocorasick.Automaton(), {}
    for row, b, wild, need in rows:
        if row["hits"]:
            continue
        k = 0
        while k < min(need, 64) and not wild[k]:
            k += 1
        key = b[:k] if k >= 4 and (k >= 8 or len(set(b[:k])) >= 2) else None
        todo.append((row, b, wild, need, key))
        if key is not None and key not in places:
            places[key] = collections.defaultdict(list)
            auto.add_word(key.decode("latin1"), key)
    if places:
        auto.make_automaton()
        for si, (f, sec, base, blob) in enumerate(srcs):
            for end, key in auto.iter(blob.decode("latin1")):
                p = places[key]
                if len(p) <= 256:
                    p[base + end - len(key) + 1].append((si, end - len(key) + 1))
    for row, b, wild, need, key in todo:
        pc = row["pc"]
        cover = []
        if key is not None and len(places[key]) <= 256:
            for addr, group in places[key].items():
                si, i = min(group, key=lambda h: rank(srcs[h[0]][0]))
                cover.append({"file": srcs[si][0], "section": srcs[si][1], "addr": addr,
                              "len": agree(b, wild, srcs[si][3], i), "copies": len(group), "how": "search"})
        j = bisect.bisect_left(lpcs, pc)
        near = [located[x] for x in (j - 1, j) if 0 <= x < len(located) and abs(located[x][0] - pc) <= NEIGHBOUR]
        predicted = {h["addr"] + pc - npc for npc, h in near}
        if len({c["addr"] for c in cover}) > 1:
            pick = [c for c in cover if c["addr"] in predicted]
            if len(pick) == 1:
                pick[0]["how"] = "search+neighbour"
                cover = pick
        if not cover:
            for npc, h in near:
                si = next(i for i, s_ in enumerate(srcs) if s_[0] == h["file"] and s_[1] == h["section"])
                at = h["addr"] + pc - npc - srcs[si][2]
                if 0 <= at < len(srcs[si][3]):
                    n = agree(b, wild, srcs[si][3], at)
                    if n >= need and solid(b, wild, n) >= 4:
                        cover.append({"file": h["file"], "section": h["section"], "addr": h["addr"] + pc - npc,
                                      "len": n, "copies": 1, "how": "neighbour"})
        cover = [c for c in cover if c["len"] >= min(need, 4) and solid(b, wild, c["len"]) >= 4]
        if cover:
            judge(row, cover, need)
        else:
            inside = [r for r in allruns if pc < r[0] < pc + row["extent"]]
            row["verdict"] = "inside" if inside else "none"
    resolve_multi([r for r, _, _, _ in rows])
    return {"build": build["id"], "sources": len(srcs), "source_bytes": sum(len(s[3]) for s in srcs),
            "runs": [{"pc": r[0], "len": r[1] - r[0], "file": srcs[r[2]][0], "section": srcs[r[2]][1],
                      "addr": r[3], "copies": r[4]} for r in allruns],
            "rows": [r for r, _, _, _ in rows]}


# ---------------------------------------------------------------- commands

def cmd_survey(a):
    with open(os.path.join(ROOT, "fixtures.toml"), "rb") as f:
        builds = tomllib.load(f)["build"]
    out = os.path.abspath(a.out)
    if out.startswith(ROOT) and not out.startswith(os.path.join(ROOT, "analysis")):
        fail("--out inside the repository must be under analysis/ (gitignored)")
    os.makedirs(out, exist_ok=True)
    read = pc_image(a.game)
    for path in a.disc:
        disc = psx_disc.Disc(path)
        b = build_of(disc, builds)
        res = survey(read, disc, b)
        with open(os.path.join(out, b["id"] + ".json"), "w") as f:
            json.dump(res, f, indent=1)
        print("%s: %d tables, %d sources (%d bytes): %s" % (b["id"], len(res["rows"]), res["sources"],
              res["source_bytes"], dict(collections.Counter(r["verdict"] for r in res["rows"]))))


MAP_GAP, MAP_MIN = 64, 16
MAP_HEAD = ("# exe_maps/%s.tsv - where BOF3.exe's initialised data is in this build, by bytes.\n"
            "# Generated by tools/exe_twins.py map (docs/exe-tables-by-build.md); do not edit.\n"
            "# Addresses and counts only (CLAUDE.md rule 1). One line per segment: the PC range\n"
            "# [pc, pc_end) is the build's `file` (BOOT = the boot EXE, BOOT.BIN = the PSP ELF;\n"
            "# else an EMI and its section) from `addr` on; `agree` bytes of it were compared\n"
            "# equal (pointer words wild), the rest are gaps of at most %d bytes between them.\n"
            "# `copies`: how many files hold the seed at that address (an overlay copied into many).\n"
            "# A segment of one table placed by a search or by its neighbours' displacement\n"
            "# rather than by a run says so in `how` (`name`: several places held it equally and\n"
            "# the file its symbol's name names was taken - the weakest).\n")


def segments(res):
    """The survey's runs as a partition of the PC's data: each byte to the
    longest run holding it (on a PSP disc the ELF's runs first: the copy the
    PSP's code reads), then neighbours with one displacement merged."""
    psp = res["build"].startswith("psp")
    order = sorted((r for r in res["runs"] if r["len"] >= MAP_MIN),
                   key=lambda r: (psp and r["file"] != "BOOT.BIN", -r["len"], rank(r["file"]), r["pc"]))
    owner = {}
    for i, r in enumerate(order):
        for p in range(r["pc"], r["pc"] + r["len"]):
            owner.setdefault(p, i)
    segs, cur = [], None
    for p in sorted(owner):
        r = order[owner[p]]
        key = (r["file"], r["section"], r["addr"] - r["pc"])
        if cur and cur["key"] == key and p - cur["end"] <= MAP_GAP and not crosses(cur["pc"], p):
            cur["end"], cur["agree"] = p + 1, cur["agree"] + 1
            cur["copies"] = max(cur["copies"], r.get("copies", 1))
        else:
            if cur:
                segs.append(cur)
            cur = {"key": key, "pc": p, "end": p + 1, "agree": 1, "copies": r.get("copies", 1)}
    if cur:
        segs.append(cur)
    # The tables no run reaches but a search or a neighbour placed alone, each
    # a segment of its own where nothing else claims those bytes.
    for row in res["rows"]:
        if row["verdict"] not in ("whole", "part") or row["hits"][0]["how"] == "run":
            continue
        h = row["hits"][0]
        lo, hi = row["pc"], row["pc"] + h["len"]
        if any(x["pc"] <= lo < x["end"] for x in segs):
            continue
        hi = min([hi] + [x["pc"] for x in segs if lo < x["pc"] < hi])   # up to what a segment already holds
        how = {"name": "name", "neighbour displacement": "neighbour",
               "neighbours' file": "neighbour-file"}.get(row.get("picked_by"), h["how"])
        segs.append({"key": (h["file"], h["section"], h["addr"] - lo), "pc": lo, "end": hi, "agree": hi - lo,
                     "copies": h.get("copies", 1), "how": how})
    segs.sort(key=lambda s_: s_["pc"])
    return segs


def crosses(a, b):
    return (a < DATA) != (b < DATA)


def cmd_map(a):
    os.makedirs(a.out, exist_ok=True)
    for path in a.json:
        with open(path) as f:
            res = json.load(f)
        segs = segments(res)
        out = os.path.join(a.out, res["build"] + ".tsv")
        with open(out, "w", newline="\n") as f:
            f.write(MAP_HEAD % (res["build"], MAP_GAP))
            f.write("pc\tpc_end\tfile\tsection\taddr\tagree\tcopies\thow\n")
            for s_ in segs:
                file, sec, delta = s_["key"]
                f.write("0x%06X\t0x%06X\t%s\t%s\t0x%08X\t%d\t%d\t%s\n" % (
                    s_["pc"], s_["end"], file, "-" if sec is None else sec, (s_["pc"] + delta) & 0xFFFFFFFF,
                    s_["agree"], s_["copies"], s_.get("how", "run")))
        print("%s: %d segments, %d bytes compared equal -> %s" % (res["build"], len(segs),
              sum(s_["agree"] for s_ in segs), out))


def cmd_summary(a):
    for path in a.json:
        with open(path) as f:
            res = json.load(f)
        c = collections.Counter(r["verdict"] for r in res["rows"])
        where = collections.Counter(r["hits"][0]["file"].split("/")[0] if r["hits"][0]["file"] not in
                                    ("BOOT", "BOOT.BIN") else r["hits"][0]["file"]
                                    for r in res["rows"] if r["hits"])
        print("%-10s %4d tables  %s" % (res["build"], len(res["rows"]), dict(sorted(c.items()))))
        print("           best hit by place: %s" % dict(where.most_common()))


def cmd_show(a):
    with open(a.json) as f:
        res = json.load(f)
    want = set(a.names)
    for r in res["rows"]:
        if want and r["name"] not in want:
            continue
        print("%-36s 0x%06X ext 0x%-5X %-12s %s" % (r["name"], r["pc"], r["extent"], r["verdict"],
              "  ".join("%s%s 0x%08X len 0x%X" % (h["file"], "" if h["section"] is None else "#%d" % h["section"],
                        h["addr"], h["len"]) for h in r["hits"][:4])))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("survey")
    p.add_argument("--game", required=True, help="the directory holding BOF3.exe")
    p.add_argument("--disc", action="append", required=True)
    p.add_argument("--out", required=True)
    p.set_defaults(fn=cmd_survey)
    p = sub.add_parser("summary")
    p.add_argument("json", nargs="+")
    p.set_defaults(fn=cmd_summary)
    p = sub.add_parser("map")
    p.add_argument("json", nargs="+")
    p.add_argument("--out", default=os.path.join(ROOT, "exe_maps"))
    p.set_defaults(fn=cmd_map)
    p = sub.add_parser("show")
    p.add_argument("json")
    p.add_argument("names", nargs="*")
    p.set_defaults(fn=cmd_show)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        os._exit(0)
