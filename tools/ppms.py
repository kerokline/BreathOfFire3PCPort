#!/usr/bin/env python
"""The PSP's songs (`pPMS`) as the cache's song files, base/bgm/NNN.DAT.

docs/psp-music.md. A PSP disc keeps every `BGM/*.EMI` of the PSX with the
same three music sections. The SEP (type 10) becomes `pPMS`: a table of four
Standard MIDI Files, one per sub-song. This module inverts each SMF into the
event list tools/seq.py's `read_sep` gives for a PSX sub-song. seq.py's
`song_bytes` then writes it in the form base/bgm/ stores (docs/seq-format.md 1).

`pPMS`, as read on both PSP discs (docs/psp-music.md 2):

    0x00  `pPMS`
    0x04  u32  the section's size
    0x08  u32  the end of the table: 0x10 + 16 x the sub-song count
    0x0C  u32  0xFFFFFFFF
    0x10  per sub-song: u16 id (its index), u16 0xFFFF, u32 offset, u32 size,
          u32 0xFFFFFFFF
    then  the SMFs, the first at the table's end: `MThd`, format 0, one
          `MTrk`, a division of 48 or 480

The inversion (`invert`; every rule counted by `measure`):

- **Ticks.** A division of 48 is the PSX's resolution. A division of 480 is
  ten times it, so every tick is divided by 10 and must divide exactly. Any
  other division is refused.
- **Metas at tick 0.** The tempo (FF 51) is the SEP header's tempo. The time
  signature (FF 58) gives the header's two rhythm bytes. The track name (03),
  port (21) and key signature (59) have no place in a SEP and are dropped.
  An FF 58 that is absent leaves the rhythm 04 02, the PSX's most common
  value. A later tempo is kept as an event, which seq.py's format carries. Any
  other meta is refused.
- **Note-offs.** An `8n` note-off is the SEP's `9n` with velocity 0.
- **The loop markers.** libsnd's NRPN pair 99 = 20 / 99 = 30, each followed
  by 6 = 127, is written on the PSP as 99 = 0 / 99 = 1, each followed by
  6 = 0. That pair is mapped back. A controller 6 that does not follow a
  loop's 99 is kept as it is.
- **The end of track** goes to the tick of the last channel event. That is
  where every PSX sub-song ends; the PSP moves some ends.

What remains is the PSP's own music, and that is what a PSP-only install
plays (psp-only-build.md 7, the owner's call 2). `measure` compares each
song with a PSX disc's and names its class (docs/psp-music.md 3).

The song table is the PSP's own: tables.toml's `songs` row for the build, in
`BOOT.BIN`. Its file ids index `BOOT.BIN`'s path table (64-byte records,
found by shape: the longest run of records that each name a file of the
disc's data folder). Song 165 is `BGMBAT00.EMI` sub 1, as on the PSX (seq.py).

    python tools/ppms.py build   --disc ISO --out CACHE         (CACHE/base/bgm/: the songs, and their banks by
                                                             vag.psp_music_vab; what importer.py build does
                                                             with a PSP disc and no PSX disc)
    python tools/ppms.py fixture --disc ISO [--disc ISO ...]   (fixtures/bgm-psp.tsv: what build writes, hashes)
    python tools/ppms.py dump    --disc ISO --song N            (the inverted song, as seq.py dump prints it)
    python tools/ppms.py measure --disc ISO --psx CUE [--json OUT]
                                                             (each song against the PSX disc's: the classes)
    python tools/ppms.py check                               (a synthetic round trip, no game data)

What it reads and writes is game data: scratch or a cache only (CLAUDE.md rule 1).
"""
import argparse
import collections
import json
import os
import re
import struct
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import psx_disc     # noqa: E402
import region_diff  # noqa: E402
import seq          # noqa: E402
import tables       # noqa: E402

MAGIC = b"pPMS"
ENTRY = 16
PSX_RESOLUTION = 48
SCALES = {48: 1, 480: 10}                   # SMF division -> ticks per SEP tick
META_TEMPO, META_RHYTHM, META_END = 0x51, 0x58, 0x2F
META_DROPPED = {0x03: "track name", 0x21: "port", 0x59: "key signature"}
DEFAULT_RHYTHM = b"\x04\x02"
LOOP_FROM = {0: seq.LOOP_START, 1: seq.LOOP_END}   # the PSP's NRPN 99 value -> libsnd's
LOOP_DATA_PSP, LOOP_DATA_PSX = 0, 127
PATH_RECORD = 64

# ---------------------------------------------------------------- pPMS and SMF


def _varlen(d, p):
    v = 0
    while True:
        c = d[p]
        p += 1
        v = (v << 7) | (c & 0x7F)
        if not c & 0x80:
            return v, p


def smf_track(d, p, end):
    """One MTrk's events [(tick, status, d1, d2, data)], running status
    resolved, ticks absolute. A meta is (tick, 0xFF, type, 0, bytes). Sysex
    and anything after the end of track is refused."""
    tick, run, out = 0, None, []
    while p < end:
        dt, p = _varlen(d, p)
        tick += dt
        st = d[p]
        if st & 0x80:
            p += 1
            if st < 0xF0:
                run = st
        elif run is None:
            raise ValueError("SMF: running status with no status at %d" % p)
        else:
            st = run
        if st == 0xFF:
            typ = d[p]
            n, q = _varlen(d, p + 1)
            out.append((tick, 0xFF, typ, 0, bytes(d[q:q + n])))
            p = q + n
            if typ == META_END:
                if p != end:
                    raise ValueError("SMF: %d bytes after the end of track" % (end - p))
                return out
            continue
        if st >= 0xF0:
            raise ValueError("SMF: status %02X (sysex) at %d" % (st, p))
        if st & 0xF0 in (0xC0, 0xD0):
            out.append((tick, st, d[p], 0, b""))
            p += 1
        else:
            out.append((tick, st, d[p], d[p + 1], b""))
            p += 2
    raise ValueError("SMF: no end of track")


def read_smf(s):
    """{division, events} of a format-0, one-track SMF; anything else refused."""
    if s[:4] != b"MThd" or struct.unpack_from(">I", s, 4)[0] != 6:
        raise ValueError("not an SMF")
    fmt, ntr, div = struct.unpack_from(">HHH", s, 8)
    if fmt != 0 or ntr != 1 or div & 0x8000:
        raise ValueError("SMF format %d, %d tracks, division 0x%04X: only format 0, one track, ticks" % (fmt, ntr, div))
    if s[14:18] != b"MTrk":
        raise ValueError("SMF: no MTrk after the header")
    n, = struct.unpack_from(">I", s, 18)
    if 22 + n != len(s):
        raise ValueError("SMF: the track is %d bytes, the sub-song %d" % (n, len(s) - 22))
    return dict(division=div, events=smf_track(s, 22, 22 + n))


def read_ppms(d):
    """A `pPMS` section: its sub-songs, each {id, division, events, size}."""
    if d[:4] != MAGIC:
        raise ValueError("not a pPMS")
    total, end, w12 = struct.unpack_from("<III", d, 4)
    if total != len(d) or w12 != 0xFFFFFFFF or end < 0x10 + ENTRY or (end - 0x10) % ENTRY:
        raise ValueError("pPMS: header words 0x%X 0x%X 0x%X for %d bytes" % (total, end, w12, len(d)))
    subs, at = [], end
    for i in range((end - 0x10) // ENTRY):
        sid, x, off, size, w = struct.unpack_from("<HHIIi", d, 0x10 + ENTRY * i)
        if sid != i or x != 0xFFFF or w != -1 or off != at or off + size > len(d):
            raise ValueError("pPMS: entry %d is id %d, 0x%04X, at 0x%X (expected 0x%X), %d bytes"
                             % (i, sid, x, off, at, size))
        sub = read_smf(d[off:off + size])
        sub.update(id=sid, size=size)
        subs.append(sub)
        at = off + size
    if at != len(d):
        raise ValueError("pPMS: the sub-songs end at %d of %d bytes" % (at, len(d)))
    return subs


# ---------------------------------------------------------------- the inversion


def invert(sub, st=None):
    """One of read_ppms's sub-songs as read_sep gives a PSX sub-song: {id,
    resolution, tempo, rhythm, events}. `st` (a Counter) counts each rule as
    it is applied."""
    st = st if st is not None else collections.Counter()
    div = sub["division"]
    if div not in SCALES:
        raise ValueError("SMF division %d: only 48 and 480 are known" % div)
    scale = SCALES[div]
    st["division %d" % div] += 1
    tempo, rhythm, out, loop_ch = None, None, [], {}
    for tick, s, d1, d2, data in sub["events"]:
        if tick % scale:
            raise ValueError("tick %d of division %d is not a whole SEP tick" % (tick, div))
        tick //= scale
        if s == 0xFF:
            if d1 == META_END:
                continue
            if d1 == META_TEMPO and len(data) == 3:
                v = int.from_bytes(data, "big")
                if tick == 0 and tempo is None:
                    tempo = v
                    st["tempo at tick 0: the header's"] += 1
                else:
                    out.append((tick, 0xFF, META_TEMPO, 0, v))
                    st["tempo past tick 0: an event"] += 1
            elif d1 == META_RHYTHM and tick == 0 and rhythm is None and len(data) == 4:
                rhythm = bytes(data[:2])
                st["time signature: the header's rhythm"] += 1
            elif d1 in META_DROPPED and tick == 0:
                st["meta %s dropped" % META_DROPPED[d1]] += 1
            else:
                raise ValueError("meta %02X (%d bytes) at tick %d: no SEP form" % (d1, len(data), tick))
            continue
        hi, ch = s & 0xF0, s & 0x0F
        after_loop = loop_ch.pop(ch, False)      # the channel's previous event was a loop's 99
        if hi == 0x80:
            s, d2 = 0x90 | ch, 0
            st["note-off as velocity 0"] += 1
        elif hi == 0xB0 and d1 == seq.CC_NRPN and d2 in LOOP_FROM:
            d2 = LOOP_FROM[d2]
            loop_ch[ch] = True
            st["loop marker"] += 1
        elif hi == 0xB0 and d1 == seq.CC_DATA and after_loop:
            if d2 != LOOP_DATA_PSP:
                raise ValueError("a loop marker's data entry is %d, not %d" % (d2, LOOP_DATA_PSP))
            d2 = LOOP_DATA_PSX
            st["loop data entry"] += 1
        elif hi == 0xB0 and d1 == seq.CC_DATA:
            st["data entry kept"] += 1
        out.append((tick, s, d1, d2, 0))
    if tempo is None:
        raise ValueError("no tempo at tick 0")
    if not out:
        raise ValueError("no channel event")
    last = max(e[0] for e in out)
    st["end of track moved" if sub["events"][-1][0] // scale != last else "end of track kept"] += 1
    if rhythm is None:
        rhythm = DEFAULT_RHYTHM
        st["no time signature: 04 02"] += 1
    out.append((last, 0xFF, seq.META_END, 0, 0))
    return dict(id=sub["id"], resolution=PSX_RESOLUTION, tempo=tempo, rhythm=rhythm, events=out)


# ---------------------------------------------------------------- the song table


def disc_build_id(disc):
    _, _, builds = tables.load()
    b = tables.disc_build(disc, builds)
    if not b["id"].startswith("psp-"):
        raise SystemExit("ppms.py reads PSP discs only (a PSX disc's songs are seq.py's)")
    return b["id"]


def data_root(disc):
    """PSP_GAME/USRDIR/<lang>/: the one folder that holds BGM/."""
    roots = {m.group(1) for p in disc.files for m in [re.match(r"(PSP_GAME/USRDIR/[^/]+/)BGM/", p)] if m}
    if len(roots) != 1:
        raise SystemExit("the PSP disc has %d data folders with a BGM/" % len(roots))
    return roots.pop()


def file_paths(disc, root=None):
    """BOOT.BIN's path table: file id -> path under the data folder. Found by
    shape: the longest run of 64-byte records, each a NUL-ended path of a file
    under `root`."""
    root = root or data_root(disc)
    names = {p[len(root):].encode() for p in disc.files if p.startswith(root)}
    elf = disc.read("PSP_GAME/SYSDIR/BOOT.BIN")
    at = [m.start() for m in re.finditer(rb"[A-Z0-9_]+/[A-Z0-9_./]+\x00", elf) if m.group()[:-1] in names]
    ok, best, run = set(at), (0, 0), {}
    for p in at:
        run[p] = run.get(p - PATH_RECORD, 0) + 1
        if run[p] > best[1]:
            best = (p - PATH_RECORD * (run[p] - 1), run[p])
    if best[1] < 2:
        raise SystemExit("BOOT.BIN: no table of paths")
    start, n = best
    assert all(start + PATH_RECORD * i in ok for i in range(n))
    return [elf[start + PATH_RECORD * i:start + PATH_RECORD * (i + 1)].split(b"\0")[0].decode() for i in range(n)]


def songs(disc, bid=None):
    """Every song: [{song, emi (key under the data folder), name, sub}] for
    0..164 from BOOT.BIN's song table, plus 165 = BGMBAT00.EMI sub 1."""
    bid = bid or disc_build_id(disc)
    cat, _, _ = tables.load()
    t = next(t for t in cat["table"] if t["key"] == "songs")
    where = next((r for r in t["psx"] if r["build"] == bid), None)
    if where is None or t["count"] != seq.SONGS or t["stride"] != 4:
        raise SystemExit("tables.toml records no song table of %d for %s" % (seq.SONGS, bid))
    raw = tables.psx_bytes(disc, where, seq.SONGS * 4)
    paths = file_paths(disc)
    out = []
    for n in range(seq.SONGS):
        fid, sq, sub = struct.unpack_from("<HBB", raw, 4 * n)
        if sq != 0 or sub > 3 or fid >= len(paths) or not paths[fid].startswith("BGM/"):
            raise SystemExit("song %d: file id 0x%X seq %d sub %d is not a BGM file's sub-song" % (n, fid, sq, sub))
        out.append(dict(song=n, emi=paths[fid], sub=sub))
    out.append(dict(song=seq.BATTLE_SONG[0], emi="BGM/%s.EMI" % seq.BATTLE_SONG[1], sub=seq.BATTLE_SONG[2]))
    for s in out:
        s["name"] = s["emi"].rsplit("/", 1)[-1][:-4]
    return out


def _music(build, key):
    """An EMI's three music sections: {6, 10, 7: (section index, bytes)}."""
    kinds = collections.defaultdict(list)
    for i, ty, _, body in build.sections(key):
        kinds[ty].append((i, body))
    if any(len(kinds[k]) != 1 for k in (6, 10, 7)):
        raise SystemExit("%s: not one bank header, song table and bank body" % key)
    return {k: kinds[k][0] for k in (6, 10, 7)}


def song_files(disc_path, bid=None, st=None, with_banks=False):
    """(build id, [(song number, bank name, song file bytes, the pPMS's section
    index)]) for the 166 songs of a PSP disc; with `with_banks`, a third item
    {bank name: (EMI key, its music sections)} for the EMIs the songs name."""
    build = region_diff.Build("psp", disc_path)
    bid = bid or disc_build_id(build.disc)
    st = st if st is not None else collections.Counter()
    secs, seps, out = {}, {}, []
    for s in songs(build.disc, bid):
        if s["emi"] not in seps:
            if s["emi"] not in build.emis:
                raise SystemExit("song %d: %s is not on the disc" % (s["song"], s["emi"]))
            secs[s["name"]] = (s["emi"], _music(build, s["emi"]))
            seps[s["emi"]] = read_ppms(secs[s["name"]][1][10][1])
        subs = seps[s["emi"]]
        if s["sub"] >= len(subs):
            raise SystemExit("song %d: %s has no sub-song %d" % (s["song"], s["emi"], s["sub"]))
        sub = invert(subs[s["sub"]], st)
        out.append((s["song"], s["name"], seq.song_bytes(s["song"], s["name"], s["sub"], sub),
                    secs[s["name"]][1][10][0]))
    return (bid, out, secs) if with_banks else (bid, out)


def bank_file(name, sections):
    """base/bgm/bank/NAME.DAT from an EMI's PSP music sections: MB's
    `vag.psp_music_vab` (the volume / pan merge inverted, psp-music-banks.md 3),
    then seq.py's reader and writer, as for a PSX disc's VAB."""
    import vag
    vh, vb = vag.psp_music_vab(sections[6][1], sections[7][1])
    return seq.bank_bytes(name, seq.parse_vab(vh, vb))


# ---------------------------------------------------------------- build (base/bgm/ from a PSP disc)

def build(disc_path, out, bid=None):
    """base/bgm/ under `out` from a PSP disc: the 166 songs and the 81 banks
    they name. Returns (build id, [(path under out, source, sha256)], stats),
    seq.build's shape, so the manifest's `bgm` rows and `verify` take either.
    A song's source is `<build>:ppms:NAME.EMI#i` (the pPMS section), a bank's
    `<build>:psp-vab:NAME.EMI#i,j` (PPHD, pBVC)."""
    st = collections.Counter()
    bid, files, secs = song_files(disc_path, bid, st, with_banks=True)
    os.makedirs(os.path.join(out, "base", "bgm", "bank"), exist_ok=True)
    rows = []
    for n, name, body, i in files:
        rel = "base/bgm/%03d.DAT" % n
        rows.append((rel, "%s:ppms:%s.EMI#%d" % (bid, name, i), seq._write(out, rel, body)))
    for name in sorted(secs):
        key, m = secs[name]
        try:
            body = bank_file(name, m)
        except ValueError as e:
            raise SystemExit("%s: %s" % (key, e))
        rel = "base/bgm/bank/%s.DAT" % name
        rows.append((rel, "%s:psp-vab:%s.EMI#%d,%d" % (bid, name, m[6][0], m[7][0]), seq._write(out, rel, body)))
    st["songs"], st["banks"] = len(files), len(secs)
    return bid, rows, st


def importer_bgm(sources, out):
    """tools/importer.py build's hook, after seq.importer_bgm: base/bgm/ from the
    first PSP disc among the player's sources when no PSX disc is among them
    (a PSX disc's music stays the base, psp-only-build.md 7 call 2). Returns
    (build id, rows) or None."""
    if any(s.id.startswith("psx-") and hasattr(s, "build") for s in sources):
        return None
    disc = next((s for s in sources if s.id.startswith("psp-") and hasattr(s, "build")), None)
    if disc is None:
        return None
    bid, rows, _ = build(disc.path, out, disc.id)
    return bid, rows


def cmd_build(a):
    bid, rows, st = build(a.disc, a.out)
    print("%s: %d songs, %d banks -> %s" % (bid, st["songs"], st["banks"], os.path.join(a.out, "base", "bgm")))


# ---------------------------------------------------------------- the fixture

# fixtures/bgm-psp.tsv: for each PSP build, every file `build` writes from its
# disc, in fixtures/bgm.tsv's format (seq.py): hashes and sizes only (rule 1),
# so that importer.py verify proves a PSP-built base/bgm/ with no disc.
FIXTURE = os.path.join(ROOT, "fixtures", "bgm-psp.tsv")


def cmd_fixture(a):
    import tempfile
    rows = []
    for path in a.disc:
        with tempfile.TemporaryDirectory() as tmp:
            bid, got, _ = build(path, tmp)
            for rel, where, h in got:
                rows.append((bid, rel, os.path.getsize(os.path.join(tmp, rel)), where.split(":", 2)[2], h))
        print("%s: %d files" % (bid, len(got)))
    builds = sorted({r[0] for r in rows})
    if len(builds) != len(a.disc):
        raise SystemExit("two discs of one build: %s" % ", ".join(builds))
    with open(a.out, "w", newline="\n", encoding="utf-8") as f:
        f.write("# The files tools/ppms.py build writes under base/bgm/ from each PSP disc (ppms.py fixture):\n"
                "# build, path, size, the EMI section(s) it is made from, sha256. importer.py verify\n"
                "# checks a PSP-built cache's base/bgm/ against its build's rows; check, this file's shape.\n")
        f.write(seq.FIXTURE_HEAD % (seq.SONG_VERSION, seq.BANK_VERSION) + "\n")
        for r in sorted(rows):
            f.write("%s\t%s\t%d\t%s\t%s\n" % r)
    print("%s: %d rows, %s" % (a.out, len(rows), ", ".join(builds)))


def load_fixture():
    """{build: {path: (size, source, sha256)}} of fixtures/bgm-psp.tsv ({} if absent)."""
    return seq.load_fixture(FIXTURE)[0] if os.path.exists(FIXTURE) else {}


def check_fixture(builds):
    """fixtures/bgm-psp.tsv's shape, no game data: seq.py's format versions,
    PSP builds of fixtures.toml, each the 166 songs and exactly the banks they
    name, every row well formed. Returns the errors."""
    if not os.path.exists(FIXTURE):
        return ["bgm-psp.tsv: missing"]
    rows, versions = seq.load_fixture(FIXTURE)
    errs = []
    if versions != (seq.SONG_VERSION, seq.BANK_VERSION):
        errs.append("bgm-psp.tsv: written for song / bank format %s, seq.py writes %d / %d - regenerate it "
                    "(ppms.py fixture)" % (versions, seq.SONG_VERSION, seq.BANK_VERSION))
    want = {"base/bgm/%03d.DAT" % i for i in range(seq.SONGS + 1)}
    for bid, files in sorted(rows.items()):
        if bid not in builds or not bid.startswith("psp-"):
            errs.append("bgm-psp.tsv: build %s is not a PSP build of fixtures.toml" % bid)
        got = {r for r in files if not r.startswith("base/bgm/bank/")}
        if got != want:
            errs.append("bgm-psp.tsv %s: %d song files, not the %d of 000..%03d" % (bid, len(got), len(want), seq.SONGS))
        banks = {r for r in files if r.startswith("base/bgm/bank/")}
        named = {"base/bgm/bank/%s.DAT" % files[r][1].split(".EMI")[0] for r in got}
        if banks != named:
            errs.append("bgm-psp.tsv %s: the banks are not the ones the songs name" % bid)
        for r, (size, where, h) in files.items():
            if size <= 0 or len(h) != 64 or any(c not in "0123456789abcdef" for c in h) or ".EMI#" not in where:
                errs.append("bgm-psp.tsv %s %s: a malformed row" % (bid, r))
    return errs


# ---------------------------------------------------------------- measure (needs a PSX disc too)

CLASSES = ("identical", "rhythm only", "order within a tick", "tempo only", "notes", "programs",
           "pitch bend only", "controllers")


def classify(psp, psx):
    """The class of a PSP-built song file against the PSX-built one (both
    seq.read_song dicts), and the details: (class, {tempo differs, counts})."""
    a, b = psx["events"], psp["events"]
    det = dict(tempo=(psx["tempo"], psp["tempo"]) if psx["tempo"] != psp["tempo"] else None,
               rhythm=(psx["rhythm"].hex(), psp["rhythm"].hex()) if psx["rhythm"] != psp["rhythm"] else None)
    ca, cb = collections.Counter(a), collections.Counter(b)
    only_psx, only_psp = ca - cb, cb - ca
    kinds = {e[1] & 0xF0 for e in list(only_psx) + list(only_psp)}
    det["psx_only"] = dict(collections.Counter("%X0" % (e[1] >> 4) for e in only_psx.elements()))
    det["psp_only"] = dict(collections.Counter("%X0" % (e[1] >> 4) for e in only_psp.elements()))
    if a == b and not det["tempo"] and not det["rhythm"]:
        return "identical", det
    if not kinds and a != b:
        return "order within a tick", det
    if not kinds:
        return ("tempo only" if det["tempo"] else "rhythm only"), det
    if 0x90 in kinds:
        return "notes", det
    if 0xC0 in kinds:
        return "programs", det
    if kinds == {0xE0}:
        return "pitch bend only", det
    return "controllers", det


def measure(psp_path, psx_path):
    """Each PSP-built song against the PSX-built one: {song: (class, details)},
    the inversion's rule counts, and the PSP build id."""
    st = collections.Counter()
    bid, mine = song_files(psp_path, st=st)
    disc = psx_disc.Disc(psx_path)
    theirs = {}
    for s in seq.songs(disc):
        secs = region_diff.emi_sections_blob(disc.read("BIN/" + s["emi"]))
        sub = seq.read_sep(secs[s["sep"]][3])[s["sub"]]
        theirs[s["song"]] = seq.song_bytes(s["song"], s["name"], s["sub"], sub)
    res = {}
    for n, name, body, _ in mine:
        if body == theirs[n]:
            res[n] = ("identical", {})
        else:
            res[n] = classify(seq.read_song(body), seq.read_song(theirs[n]))
    return bid, res, st


def cmd_measure(a):
    bid, res, st = measure(a.disc, a.psx)
    by = collections.defaultdict(list)
    for n, (c, _) in sorted(res.items()):
        by[c].append(n)
    print("%s against the PSX disc: %d songs" % (bid, len(res)))
    for c in CLASSES:
        if by[c]:
            print("  %-22s %3d  %s" % (c, len(by[c]), " ".join(map(str, by[c])) if c != "identical" else ""))
    tempo = [(n, d["tempo"]) for n, (c, d) in sorted(res.items()) if d.get("tempo")]
    print("  tempo differs in %d: %s" % (len(tempo), ", ".join("%d (%d -> %d us)" % (n, x, y) for n, (x, y) in tempo)))
    rh = [(n, d["rhythm"]) for n, (c, d) in sorted(res.items()) if d.get("rhythm")]
    print("  rhythm differs in %d: %s" % (len(rh), ", ".join("%d (%s -> %s)" % (n, x, y) for n, (x, y) in rh)))
    print("the inversion's rules, per sub-song used:")
    for k, v in sorted(st.items()):
        print("  %6d  %s" % (v, k))
    if a.json:
        with open(a.json, "w") as f:
            json.dump(dict(build=bid, songs={str(n): dict(cls=c, **d) for n, (c, d) in sorted(res.items())},
                           rules=dict(st)), f, indent=1)


def cmd_dump(a):
    _, rows = song_files(a.disc)
    body = next(r[2] for r in rows if r[0] == a.song)
    print("\n".join(seq.dump_lines(body)))


# ---------------------------------------------------------------- check (no game data)


def _vl(v):
    out = [v & 0x7F]
    v >>= 7
    while v:
        out.append(0x80 | (v & 0x7F))
        v >>= 7
    return bytes(reversed(out))


def to_smf(events, division, tempo, rhythm, metas=True, offs=()):
    """The PSP's form of read_sep's events, as the PSP made it (the test's
    model of the forward direction): ticks x division / 48, the tempo, time
    signature and the dropped metas at tick 0, the loop pair as 99 = 0 / 1
    with 6 = 0, the note-offs at the indices `offs` as 8n, the end of track
    a few ticks later."""
    sc = division // PSX_RESOLUTION
    body, last, run = bytearray(), 0, None
    head = [(0, bytes([0xFF, 0x03, 2]) + b"T0"), (0, bytes([0xFF, 0x51, 3]) + tempo.to_bytes(3, "big"))]
    if metas:
        head += [(0, bytes([0xFF, 0x58, 4]) + rhythm + b"\x18\x08"), (0, bytes([0xFF, 0x21, 1, 0])),
                 (0, bytes([0xFF, 0x59, 2, 0, 0]))]
    for t, raw in head:
        body += _vl(0) + raw
    loop = {}
    for i, (tick, s, d1, d2, val) in enumerate(events):
        t = tick * sc
        if s == 0xFF and d1 == seq.META_END:
            body += _vl(t - last + 5 * sc) + b"\xff\x2f\x00"
            break
        if s == 0xFF:
            raw = bytes([0xFF, d1, 3]) + val.to_bytes(3, "big")
        else:
            hi, ch = s & 0xF0, s & 0x0F
            if hi == 0xB0 and d1 == seq.CC_NRPN and d2 in (seq.LOOP_START, seq.LOOP_END):
                d2 = 0 if d2 == seq.LOOP_START else 1
                loop[ch] = True
            elif hi == 0xB0 and d1 == seq.CC_DATA and loop.pop(ch, False):
                d2 = 0
            if hi == 0x90 and d2 == 0 and i in offs:
                s, d2 = 0x80 | ch, 64
            raw = bytes([s, d1]) if hi in (0xC0, 0xD0) else bytes([s, d1, d2])
            if s == run:
                raw = raw[1:]
            run = s
        body += _vl(t - last) + raw
        last = t
    return b"MThd" + struct.pack(">IHHH", 6, 0, 1, division) + b"MTrk" + struct.pack(">I", len(body)) + bytes(body)


def to_ppms(smfs):
    end = 0x10 + ENTRY * len(smfs)
    table, at = bytearray(), end
    for i, s in enumerate(smfs):
        table += struct.pack("<HHIIi", i, 0xFFFF, at, len(s), -1)
        at += len(s)
    return MAGIC + struct.pack("<III", at, end, 0xFFFFFFFF) + bytes(table) + b"".join(smfs)


def check():
    """seq.py's synthetic SEP through the PSP's form (to_smf / to_ppms) and
    back: its first sub-song at division 480 and at 48, with note-offs, the
    metas and the loop pair, must invert to read_sep's events and the same
    song file; the refusals must refuse. No game data. Returns the errors."""
    errs = []
    sep, want, *_ = seq.synthetic()
    x = seq.read_sep(sep)[0]
    # The synthetic SEP is shaped as the discs' are (docs/psp-music.md 2): its tick-0
    # tempo event is the header's tempo, its one 8n note-off a 9n with velocity 0, and
    # its resolution 48 (no 8n and no other resolution in any of the 324 PSX sub-songs).
    events = [e if e[1] & 0xF0 != 0x80 else (e[0], 0x90 | (e[1] & 0x0F), e[2], 0, 0)
              for e in x["events"] if not (e[0] == 0 and e[1] == 0xFF and e[2] == META_TEMPO)]
    x = dict(x, events=events, tempo=want[0][4], resolution=PSX_RESOLUTION)
    offs = {i for i, e in enumerate(events) if e[1] & 0xF0 == 0x90 and e[3] == 0}
    for div in (480, 48):
        smf = to_smf(events, div, x["tempo"], x["rhythm"], offs=offs)
        st = collections.Counter()
        subs = read_ppms(to_ppms([smf, to_smf([(0, 0xFF, seq.META_END, 0, 0)], div, 500000, b"\x04\x02", False)]))
        got = invert(subs[0], st)
        if got["events"] != events or (got["tempo"], got["rhythm"], got["resolution"]) != (x["tempo"], x["rhythm"], 48):
            errs.append("division %d: the inverted sub-song is not the SEP's" % div)
        if seq.song_bytes(7, "SYNTH", 0, got) != seq.song_bytes(7, "SYNTH", 0, x):
            errs.append("division %d: the song file differs from the SEP's" % div)
        if st["note-off as velocity 0"] != len(offs) or st["loop marker"] != 2 or st["loop data entry"] != 1 \
                or st["end of track moved"] != 1:
            errs.append("division %d: the rule counts %s" % (div, dict(st)))
        try:
            invert(subs[1])
            errs.append("division %d: a sub-song with no channel event was not refused" % div)
        except ValueError:
            pass
    # no time signature: the rhythm 04 02
    if invert(read_smf(to_smf(events, 48, 500000, b"\x03\x02", metas=False)) | dict(id=0))["rhythm"] != DEFAULT_RHYTHM:
        errs.append("no time signature: not 04 02")
    bad = [("a division of 96", lambda s: s[:12] + struct.pack(">H", 96) + s[14:]),
           ("a tick not a whole SEP tick", lambda s: s[:22] + b"\x00" + s[23:].replace(b"\x00\xff\x51", b"\x03\xff\x51", 1)),
           ("an unknown meta", lambda s: s.replace(b"\xff\x03\x02T0", b"\xff\x7f\x02T0", 1)),
           ("format 1", lambda s: s[:8] + struct.pack(">H", 1) + s[10:])]
    good = to_smf(events, 480, x["tempo"], x["rhythm"], offs=offs)
    for why, f in bad:
        try:
            invert(read_smf(f(good)) | dict(id=0))
            errs.append("%s was not refused" % why)
        except ValueError:
            pass
    try:
        read_ppms(to_ppms([good])[:0x14] + b"\x05" + to_ppms([good])[0x15:])
        errs.append("a pPMS entry with the wrong offset was not refused")
    except ValueError:
        pass
    return errs


def cmd_check(a):
    errs = check()
    for e in errs:
        print("ERROR", e)
    print("ppms check: synthetic pPMS round trip, %d error(s)" % len(errs))
    return 1 if errs else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    p = s.add_parser("dump", help="one song of a PSP disc, inverted, as seq.py dump prints it")
    p.add_argument("--disc", required=True)
    p.add_argument("--song", type=int, required=True)
    p = s.add_parser("measure", help="every PSP-built song against a PSX disc's: the classes")
    p.add_argument("--disc", required=True, help="a PSP disc")
    p.add_argument("--psx", required=True, help="a PSX disc")
    p.add_argument("--json")
    p = s.add_parser("build", help="CACHE/base/bgm/ from a PSP disc: the songs and their banks")
    p.add_argument("--disc", required=True)
    p.add_argument("--out", required=True)
    p = s.add_parser("fixture", help="fixtures/bgm-psp.tsv: the hashes of what build writes from each PSP disc")
    p.add_argument("--disc", action="append", required=True)
    p.add_argument("--out", default=FIXTURE)
    s.add_parser("check", help="the synthetic round trip (no game data)")
    a = ap.parse_args()
    sys.exit({"dump": cmd_dump, "measure": cmd_measure, "check": cmd_check, "build": cmd_build,
              "fixture": cmd_fixture}[a.cmd](a) or 0)


if __name__ == "__main__":
    main()
