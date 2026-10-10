#!/usr/bin/env python
"""The disc's music as the cache's containers: base/bgm/NNN.DAT and base/bgm/bank/NAME.DAT.

docs/seq-import.md; the format is docs/seq-format.md, the plan
docs/sequenced-music-plan.md section 3 (group IMP). Every `BIN/BGM/*.EMI` is
a VAB header (section type 6, `pBAV`), a SEP (type 10, `pQES` version 0: four
sub-songs of MIDI-style events) and a VAB body (type 7, the SPU's ADPCM
samples). The boot EXE's song table (tables.toml `songs`: 165 entries of
{u16 file id, u8 seq, u8 sub}) names each song's file and sub-song; a file id
is an index into the boot EXE's table of disc-file LBAs (the sibling's
tools/file_ids.py). Song 165 is `BGMBAT00.EMI` sub 1, the battle bundles' own
sub-song, which the PC plays as `BGM\\165.DAT` (docs/bgm-comparison.md 6.3).

The VAB header, as Sony lays it out:

    0x000  32 bytes  `pBAV`, version, id, fsize, u16 0, u16 ps (programs with
                     tones), ts (tones), vs (samples), u8 mvol, pan, attr1, attr2, u32 0
    0x020  128 x 16  ProgAtr: tones, mvol, prior, mode, mpan, 0, s16 attr, 2 x u32
    0x820  ps*16 x 32  VagAtr: prior, mode, vol, pan, center, shift, min, max,
                     vibW, vibT, porW, porT, pbmin, pbmax, 2 x u8, u16 adsr1,
                     adsr2, s16 prog, vag, 4 x s16; block k is the k-th program
                     with tones (all 81 VABs measured)
    then     256 x u16 the sample sizes in 8-byte units; entry 0 unused, sample i is entry i

The SEP's events: delta time (MIDI varlen), status (running status for
channel messages), data; meta `FF 51 tt tt tt` (tempo, no length byte) and
`FF 2F 00` (end) only.

    python tools/seq.py build  --disc CUE --out CACHE      (CACHE/base/bgm/...)
    python tools/seq.py dump   FILE                        (a song or a bank, as text)
    python tools/seq.py verify --disc CUE --cache CACHE    (against tools/bgm/inventory.py and tools/vag.py)
    python tools/seq.py check                              (a synthetic round trip, no game data)
    python tools/seq.py fixture --disc CUE [--disc CUE ...] [--out fixtures/bgm.tsv]
                                                           (what build writes from each disc: hashes)

What it writes is game data: the cache lives outside the repo (CLAUDE.md rule 1).
"""
import argparse
import collections
import hashlib
import os
import struct
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import psx_disc     # noqa: E402
import region_diff  # noqa: E402
import tables       # noqa: E402

SONG_MAGIC, SONG_VERSION = b"BF3S", 1
BANK_MAGIC, BANK_VERSION = b"BF3B", 2
NONE = 0xFFFFFFFF
SONGS = 165                     # the table's; 165 itself is the battle bundles' sub 1
BATTLE_SONG = (165, "BGMBAT00", 1)
SONG_HEAD, EVENT = 56, 12
BANK_HEAD, PROGRAM, TONE, SAMPLE = 36, 8, 24, 8
VAB_HEAD, VAB_PROG, VAB_TONE = 32, 16, 32
EVENT_TYPES = (0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0)
META_TEMPO, META_END = 0x51, 0x2F
CC_NRPN, CC_DATA = 99, 6
LOOP_START, LOOP_END = 20, 30

# ---------------------------------------------------------------- the VAB

PROG_FIELDS = ("tones", "mvol", "prior", "mode", "mpan", "attr")
TONE_FIELDS = ("prior", "mode", "vol", "pan", "center", "shift", "min", "max", "vibW", "vibT",
               "porW", "porT", "pbmin", "pbmax", "adsr1", "adsr2", "prog", "vag")


def parse_vab(vh, vb):
    """The VAB header and body: {ps, ts, vs, mvol, pan, attr1, attr2,
    programs: 128 dicts (+ block), tones: ps*16 dicts, samples: [bytes]}.
    Raises on anything the layout does not account for."""
    if vh[:4] != b"pBAV":
        raise ValueError("not a VAB header")
    _, ver, _, fsize, _, ps, ts, vs, mvol, pan, a1, a2, _ = struct.unpack_from("<4sIIIHHHHBBBBI", vh, 0)
    sizes_at = VAB_HEAD + 128 * VAB_PROG + ps * 16 * VAB_TONE
    if len(vh) != sizes_at + 512:
        raise ValueError("VAB header is %d bytes, %d programs make %d" % (len(vh), ps, sizes_at + 512))
    programs, block = [], 0
    for i in range(128):
        t, mv, pr, mo, mp, _, at, _, _ = struct.unpack_from("<BBBBBBhII", vh, VAB_HEAD + VAB_PROG * i)
        programs.append(dict(zip(PROG_FIELDS, (t, mv, pr, mo, mp, at & 0xFFFF)), block=block if t else 0xFF))
        block += 1 if t else 0
    if block != ps:
        raise ValueError("%d programs have tones, the header says %d" % (block, ps))
    tones = []
    for k in range(ps * 16):
        f = struct.unpack_from("<14B2xHHhh", vh, VAB_HEAD + 128 * VAB_PROG + VAB_TONE * k)
        tones.append(dict(zip(TONE_FIELDS, f[:16] + (f[16] & 0xFFFF, f[17] & 0xFFFF))))
    for p, prog in enumerate(programs):
        for t in tones[prog["block"] * 16:prog["block"] * 16 + prog["tones"]] if prog["tones"] else ():
            if t["prog"] != p:
                raise ValueError("program %d's block %d holds a tone of program %d" % (p, prog["block"], t["prog"]))
    if sum(p["tones"] for p in programs) != ts:
        raise ValueError("the programs' tones add to %d, the header says %d" % (sum(p["tones"] for p in programs), ts))
    sizes = struct.unpack_from("<256H", vh, sizes_at)
    if sizes[0] or any(sizes[vs + 1:]) or vs > 255:
        raise ValueError("size table: entry 0 or entries past %d are not 0" % vs)
    samples, pos = [], 0
    for i in range(1, vs + 1):
        n = sizes[i] * 8
        if n % 16:
            raise ValueError("sample %d is %d bytes, not whole 16-byte blocks" % (i, n))
        samples.append(vb[pos:pos + n])
        pos += n
    if pos != len(vb):
        raise ValueError("the size table makes %d bytes of body, the body is %d" % (pos, len(vb)))
    if fsize != len(vh) + len(vb) or ver != 7:
        raise ValueError("VAB version %d, fsize %d for %d bytes" % (ver, fsize, len(vh) + len(vb)))
    return dict(ps=ps, ts=ts, vs=vs, mvol=mvol, pan=pan, attr1=a1, attr2=a2,
                programs=programs, tones=tones, samples=samples)


def bank_bytes(name, v):
    """base/bgm/bank/NAME.DAT (docs/seq-format.md section 2) from parse_vab's dict."""
    out = bytearray(BANK_MAGIC + struct.pack("<I16sHHHBBBBH", BANK_VERSION, name.encode(), v["ps"], v["ts"],
                                             v["vs"], v["mvol"], v["pan"], v["attr1"], v["attr2"], 0))
    for p in v["programs"]:
        out += struct.pack("<6BH", p["tones"], p["mvol"], p["prior"], p["mode"], p["mpan"], p["block"], p["attr"])
    for t in v["tones"]:
        out += struct.pack("<14BHHHHH", *[t[f] for f in TONE_FIELDS[:14]], 0, t["adsr1"], t["adsr2"], t["prog"], t["vag"])
    pos = 0
    for s in v["samples"]:
        out += struct.pack("<II", pos, len(s))
        pos += len(s)
    return bytes(out + b"".join(v["samples"]))


def read_bank(b):
    """A bank file back into parse_vab's dict (with the name); refuses a wrong magic or version."""
    if b[:4] != BANK_MAGIC or struct.unpack_from("<I", b, 4)[0] != BANK_VERSION:
        raise ValueError("not a version %d bank" % BANK_VERSION)
    _, name, ps, ts, vs, mvol, pan, a1, a2, _ = struct.unpack_from("<I16sHHHBBBBH", b, 4)
    v = dict(name=name.rstrip(b"\0").decode(), ps=ps, ts=ts, vs=vs, mvol=mvol, pan=pan, attr1=a1, attr2=a2)
    p = BANK_HEAD
    v["programs"] = []
    for _ in range(128):
        f = struct.unpack_from("<6BH", b, p)
        v["programs"].append(dict(zip(PROG_FIELDS, f[:5] + (f[6],)), block=f[5]))
        p += PROGRAM
    v["tones"] = []
    for _ in range(ps * 16):
        f = struct.unpack_from("<14BHHHHH", b, p)
        v["tones"].append(dict(zip(TONE_FIELDS, f[:14] + f[15:])))
        p += TONE
    table = [struct.unpack_from("<II", b, p + SAMPLE * i) for i in range(vs)]
    bodies = p + SAMPLE * vs
    v["samples"] = [b[bodies + o:bodies + o + n] for o, n in table]
    if bodies + sum(n for _, n in table) != len(b):
        raise ValueError("bank: the sample table does not end the file")
    return v


# ---------------------------------------------------------------- the SEP

def _varlen(d, p):
    v = 0
    while True:
        c = d[p]
        p += 1
        v = (v << 7) | (c & 0x7F)
        if not c & 0x80:
            return v, p


def read_track(d, p, end):
    """One sub-song's events [(tick, status, d1, d2, value)], running status
    resolved, ticks absolute, in the file's order; and the position after
    the end-of-track. The walk is tools/bgm/inventory.py parse_track's;
    anything outside it is an error, never a skip."""
    tick, run, out = 0, None, []
    while p < end:
        dt, p = _varlen(d, p)
        tick += dt
        st = d[p]
        if st & 0x80:
            p += 1
            if st != 0xFF:
                run = st
        elif run is None:
            raise ValueError("running status with no status at %d" % p)
        else:
            st = run
        if st == 0xFF:
            typ = d[p]
            if typ == META_END:
                if d[p + 1] != 0:
                    raise ValueError("end of track FF 2F %02X at %d" % (d[p + 1], p))
                out.append((tick, 0xFF, META_END, 0, 0))
                return out, p + 2
            if typ == META_TEMPO:
                out.append((tick, 0xFF, META_TEMPO, 0, (d[p + 1] << 16) | (d[p + 2] << 8) | d[p + 3]))
                p += 4
                continue
            raise ValueError("meta %02X at %d" % (typ, p))
        hi = st & 0xF0
        if hi in (0xC0, 0xD0):
            out.append((tick, st, d[p], 0, 0))
            p += 1
        elif hi in EVENT_TYPES:
            out.append((tick, st, d[p], d[p + 1], 0))
            p += 2
        else:
            raise ValueError("status %02X at %d" % (st, p))
    raise ValueError("no end of track")


def read_sep(d):
    """A `pQES` version 0 SEP: its sub-songs, each {id, resolution, tempo,
    rhythm (the two header bytes), size, walked, events}. The list ends at
    the data's end or an id of 0xFFFF, as inventory.py's parse_sep."""
    if d[:4] != b"pQES" or struct.unpack_from(">H", d, 4)[0] != 0:
        raise ValueError("not a version 0 SEP")
    subs, p = [], 6
    while p + 13 <= len(d):
        sid, res = struct.unpack_from(">HH", d, p)
        tempo = (d[p + 4] << 16) | (d[p + 5] << 8) | d[p + 6]
        size, = struct.unpack_from(">I", d, p + 9)
        events, endp = read_track(d, p + 13, len(d))
        subs.append(dict(id=sid, resolution=res, tempo=tempo, rhythm=bytes(d[p + 7:p + 9]), size=size,
                         walked=endp - p - 13, events=events))
        p = p + 13 + size
        if p >= len(d) or d[p:p + 2] == b"\xff\xff":
            break
    return subs


def markers(events):
    """(loop start tick, loop end tick, end tick): controller 99 = 20 and 99 = 30
    (libsnd's NRPN loop convention), NONE where absent. The start is the
    first start marker: songs 37 and 80 carry a second before their one end
    (docs/seq-format.md section 1), which stays in the events for the player
    to treat as libsnd does. More than one end, or an end with no start
    before it, is an error the format has no room for."""
    starts = [e[0] for e in events if e[1] & 0xF0 == 0xB0 and e[2] == CC_NRPN and e[3] == LOOP_START]
    ends = [e[0] for e in events if e[1] & 0xF0 == 0xB0 and e[2] == CC_NRPN and e[3] == LOOP_END]
    if len(ends) > 1 or bool(starts) != bool(ends) or (ends and starts[-1] > ends[0]):
        raise ValueError("loop markers: starts at %s, ends at %s" % (starts, ends))
    if events[-1][1:3] != (0xFF, META_END):
        raise ValueError("the last event is not the end of track")
    return (starts[0] if starts else NONE), (ends[0] if ends else NONE), events[-1][0]


def song_bytes(n, bank, sub_index, sub):
    """base/bgm/NNN.DAT (docs/seq-format.md section 1) from one of read_sep's sub-songs."""
    ls, le, end = markers(sub["events"])
    out = bytearray(SONG_MAGIC + struct.pack("<II16sHHH", SONG_VERSION, 1 if ls != NONE else 0, bank.encode(),
                                             n, sub_index, sub["resolution"]))
    out += sub["rhythm"] + struct.pack("<IIIII", sub["tempo"], len(sub["events"]), ls, le, end)
    for tick, st, d1, d2, val in sub["events"]:
        out += struct.pack("<IBBBBI", tick, st, d1, d2, 0, val)
    return bytes(out)


def read_song(b):
    """A song file back into a dict with its events; refuses a wrong magic or version."""
    if b[:4] != SONG_MAGIC or struct.unpack_from("<I", b, 4)[0] != SONG_VERSION:
        raise ValueError("not a version %d song" % SONG_VERSION)
    _, flags, bank, n, sub, res = struct.unpack_from("<II16sHHH", b, 4)
    tempo, count, ls, le, end = struct.unpack_from("<IIIII", b, 36)
    if len(b) != SONG_HEAD + EVENT * count:
        raise ValueError("song: %d events do not fill %d bytes" % (count, len(b)))
    events = []
    for i in range(count):
        tick, st, d1, d2, z, val = struct.unpack_from("<IBBBBI", b, SONG_HEAD + EVENT * i)
        events.append((tick, st, d1, d2, val))
    return dict(flags=flags, bank=bank.rstrip(b"\0").decode(), song=n, sub=sub, resolution=res,
                rhythm=bytes(b[34:36]), tempo=tempo, loop_start=ls, loop_end=le, end=end, events=events)


# ---------------------------------------------------------------- the song table

def disc_build_id(disc):
    _, _, builds = tables.load()
    b = tables.disc_build(disc, builds)
    if not b["id"].startswith("psx-"):
        raise SystemExit("seq.py reads PSX discs only (the PSP's pPMS / PPHD are not read; "
                         "docs/sequenced-music-plan.md section 3)")
    return b["id"]


def file_paths(disc):
    """The boot EXE's disc-file table: file id -> path. The table is u32 LBAs,
    one per file in directory-walk order (the sibling's tools/file_ids.py,
    JP 0x80182DBC); found by shape on any PSX disc - the longest run of words
    that are each a file's LBA."""
    exe = boot_exe(disc)
    lbas = {lba: p for p, (lba, _) in disc.files.items()}
    words = struct.unpack_from("<%dI" % ((len(exe) - 0x800) // 4), exe, 0x800)
    best, i = (0, 0), 0
    while i < len(words):
        j = i
        while j < len(words) and words[j] in lbas:
            j += 1
        if j - i > best[1] - best[0]:
            best = (i, j)
        i = j + 1
    return [lbas[w] for w in words[best[0]:best[1]]]


def boot_exe(disc):
    boot = disc.read("SYSTEM.CNF").split(b"\n")[0].split(b":")[-1].strip().lstrip(b"\\").split(b";")[0]
    exe = disc.read(boot.decode().replace("\\", "/"))
    if exe[:8] != b"PS-X EXE":
        raise SystemExit("the boot file %s is not a PS-X EXE" % boot.decode())
    return exe


def songs(disc, bid=None):
    """Every song: [{song, emi (path under BIN/), name, vh, sep, vb (section
    indices), sub}] for 0..164 from the boot EXE's song table (where
    tables.toml's `songs` records it for this build), plus 165 =
    BGMBAT00.EMI sub 1."""
    bid = bid or disc_build_id(disc)
    cat, _, _ = tables.load()
    t = next(t for t in cat["table"] if t["key"] == "songs")
    where = next((r for r in t["psx"] if r["build"] == bid), None)
    if where is None or t["count"] != SONGS or t["stride"] != 4:
        raise SystemExit("tables.toml records no song table of %d for %s" % (SONGS, bid))
    raw = tables.psx_bytes(disc, where, SONGS * 4)
    paths = file_paths(disc)
    out, sections = [], {}
    rows = [struct.unpack_from("<HBB", raw, 4 * i) + (i,) for i in range(SONGS)]
    for fid, seq, sub, n in rows:
        if seq != 0 or sub > 3 or fid >= len(paths) or not paths[fid].startswith("BIN/BGM/"):
            raise SystemExit("song %d: file id 0x%X seq %d sub %d is not a BIN/BGM file's sub-song" % (n, fid, seq, sub))
        out.append(dict(song=n, emi=paths[fid][4:], sub=sub))
    out.append(dict(song=BATTLE_SONG[0], emi="BGM/%s.EMI" % BATTLE_SONG[1], sub=BATTLE_SONG[2]))
    for s in out:
        if s["emi"] not in sections:
            blob = disc.read("BIN/" + s["emi"])
            secs = region_diff.emi_sections_blob(blob)
            kinds = collections.defaultdict(list)
            for i, ty, _, _ in secs:
                kinds[ty].append(i)
            if any(len(kinds[k]) != 1 for k in (6, 10, 7)):
                raise SystemExit("%s: not one VAB header, SEP and VAB body (%s)" % (s["emi"], [x[1] for x in secs]))
            sections[s["emi"]] = (kinds[6][0], kinds[10][0], kinds[7][0])
        s["name"] = s["emi"].rsplit("/", 1)[-1][:-4]
        s["vh"], s["sep"], s["vb"] = sections[s["emi"]]
    return out


# ---------------------------------------------------------------- build

def build(disc_path, out, bid=None):
    """base/bgm/ under `out` from a PSX disc: every song and every bank a song
    names. Returns (build id, [(path under out, source, sha256)], stats)."""
    disc = psx_disc.Disc(disc_path)
    bid = bid or disc_build_id(disc)
    table = songs(disc, bid)
    d = os.path.join(out, "base", "bgm")
    os.makedirs(os.path.join(d, "bank"), exist_ok=True)
    rows, st, seps, banks = [], collections.Counter(), {}, {}
    st["empty"] = []
    for s in table:
        if s["emi"] not in seps:
            secs = region_diff.emi_sections_blob(disc.read("BIN/" + s["emi"]))
            seps[s["emi"]] = read_sep(secs[s["sep"]][3])
            banks[s["emi"]] = (secs[s["vh"]][3], secs[s["vb"]][3])
        subs = seps[s["emi"]]
        if s["sub"] >= len(subs) or subs[s["sub"]]["id"] != s["sub"]:
            raise SystemExit("song %d: %s has no sub-song %d" % (s["song"], s["emi"], s["sub"]))
        sub = subs[s["sub"]]
        if sub["walked"] != sub["size"]:
            raise SystemExit("song %d: the events end at %d of the sub-song's %d bytes" % (s["song"], sub["walked"], sub["size"]))
        body = song_bytes(s["song"], s["name"], s["sub"], sub)
        rel = "base/bgm/%03d.DAT" % s["song"]
        rows.append((rel, "%s:seq:%s.EMI#%d" % (bid, s["name"], s["sep"]), _write(out, rel, body)))
        st["songs"] += 1
        st["events"] += len(sub["events"])
        st["song_bytes"] += len(body)
        if not any(e[1] & 0xF0 == 0x90 and e[3] for e in sub["events"]):
            st["empty"].append(s["song"])
    for emi in sorted(banks):
        s = next(x for x in table if x["emi"] == emi)
        v = parse_vab(*banks[emi])
        body = bank_bytes(s["name"], v)
        rel = "base/bgm/bank/%s.DAT" % s["name"]
        rows.append((rel, "%s:vab:%s.EMI#%d,%d" % (bid, s["name"], s["vh"], s["vb"]), _write(out, rel, body)))
        st["banks"] += 1
        st["samples"] += v["vs"]
        st["bank_bytes"] += len(body)
    return bid, rows, st


def _write(out, rel, body):
    with open(os.path.join(out, rel), "wb") as f:
        f.write(body)
    return hashlib.sha256(body).hexdigest()


def importer_bgm(sources, out):
    """tools/importer.py build's hook: base/bgm/ from the first PSX disc among
    the player's sources, or nothing without one. Returns (build id, rows) or None."""
    disc = next((s for s in sources if s.id.startswith("psx-") and hasattr(s, "build")), None)
    if disc is None:
        return None
    bid, rows, _ = build(disc.path, out, disc.id)
    return bid, rows


# ---------------------------------------------------------------- the fixture

# fixtures/bgm.tsv: for each PSX build, every file `build` writes from its disc -
# path under the cache, size, the section it came from, sha256 - so that
# `importer.py verify` proves a cache's base/bgm/ is what this seq.py makes of
# that build's disc, with no disc. Hashes and sizes only (CLAUDE.md rule 1).
# The header names the two format versions: a change to either needs the
# fixture regenerated (`fixture`), and `check` says so.
FIXTURE = os.path.join(ROOT, "fixtures", "bgm.tsv")
FIXTURE_HEAD = "# format song %d bank %d"


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
        f.write("# The files tools/seq.py build writes under base/bgm/ from each PSX disc (seq.py fixture):\n"
                "# build, path, size, the EMI section(s) it is made from, sha256. importer.py verify\n"
                "# checks a cache's base/bgm/ against its build's rows; check, this file's shape.\n")
        f.write(FIXTURE_HEAD % (SONG_VERSION, BANK_VERSION) + "\n")
        for r in sorted(rows):
            f.write("%s\t%s\t%d\t%s\t%s\n" % r)
    print("%s: %d rows, %s" % (a.out, len(rows), ", ".join(builds)))


def load_fixture(path=None):
    """{build: {path: (size, source, sha256)}} and the header's (song, bank) versions."""
    rows, versions = collections.defaultdict(dict), None
    with open(path or FIXTURE, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("# format "):
                x = line.split()
                versions = (int(x[3]), int(x[5]))
            elif line and not line.startswith("#"):
                bid, rel, size, where, h = line.split("\t")
                rows[bid][rel] = (int(size), where, h)
    return dict(rows), versions


def check_fixture(builds, path=None):
    """fixtures/bgm.tsv's shape, no game data: the format versions seq.py writes,
    builds fixtures.toml holds, every build the same 166 songs, each song's EMI
    with its bank, nothing else. Returns the errors."""
    path = path or FIXTURE
    if not os.path.exists(path):
        return ["%s: missing" % os.path.relpath(path, ROOT)]
    rows, versions = load_fixture(path)
    errs = []
    if versions != (SONG_VERSION, BANK_VERSION):
        errs.append("bgm.tsv: written for song / bank format %s, seq.py writes %d / %d - regenerate it (seq.py fixture)"
                    % (versions, SONG_VERSION, BANK_VERSION))
    songs_want = {"base/bgm/%03d.DAT" % i for i in range(SONGS + 1)}
    for bid, files in sorted(rows.items()):
        if bid not in builds or not bid.startswith("psx-"):
            errs.append("bgm.tsv: build %s is not a PSX build of fixtures.toml" % bid)
        got = {r for r in files if not r.startswith("base/bgm/bank/")}
        if got != songs_want:
            errs.append("bgm.tsv %s: %d song files, not the %d of 000..%03d" % (bid, len(got), len(songs_want), SONGS))
        banks = {r for r in files if r.startswith("base/bgm/bank/")}
        named = {"base/bgm/bank/%s.DAT" % files[r][1].split(".EMI")[0] for r in got}
        if banks != named:
            errs.append("bgm.tsv %s: banks %s without a song, %s named and missing"
                        % (bid, sorted(banks - named)[:3], sorted(named - banks)[:3]))
        for r, (size, where, h) in files.items():
            if size <= 0 or len(h) != 64 or any(c not in "0123456789abcdef" for c in h) or ".EMI#" not in where:
                errs.append("bgm.tsv %s %s: a malformed row" % (bid, r))
    return errs


# ---------------------------------------------------------------- dump

STATUS = {0x80: "note-off", 0x90: "note-on", 0xA0: "key-pressure", 0xB0: "control", 0xC0: "program",
          0xD0: "pressure", 0xE0: "pitch-bend"}


def event_text(e):
    tick, st, d1, d2, val = e
    if st == 0xFF:
        return "%8d  meta  %s" % (tick, "tempo %d" % val if d1 == META_TEMPO else "end" if d1 == META_END else "%02X" % d1)
    hi = st & 0xF0
    data = "%d" % d1 if hi in (0xC0, 0xD0) else "%d %d" % (d1, d2)
    return "%8d  ch%-2d  %-12s %s" % (tick, st & 0x0F, STATUS[hi], data)


def dump_lines(b):
    if b[:4] == SONG_MAGIC:
        s = read_song(b)
        mark = lambda t: "none" if t == NONE else str(t)
        lines = ["song %d: bank %s, sub-song %d, %s" % (s["song"], s["bank"], s["sub"], "loops" if s["flags"] & 1 else "once"),
                 "resolution %d, tempo %d us, rhythm %s" % (s["resolution"], s["tempo"], s["rhythm"].hex()),
                 "%d events; loop start %s, loop end %s, end %d" % (len(s["events"]), mark(s["loop_start"]),
                                                                    mark(s["loop_end"]), s["end"])]
        return lines + [event_text(e) for e in s["events"]]
    v = read_bank(b)
    lines = ["bank %s: %d programs, %d tones, %d samples; mvol %d pan %d attr %d %d"
             % (v["name"], v["ps"], v["ts"], v["vs"], v["mvol"], v["pan"], v["attr1"], v["attr2"])]
    for i, p in enumerate(v["programs"]):
        if p["tones"]:
            lines.append("program %3d: block %d, %d tones, mvol %d prior %d mode %d mpan %d attr %d"
                         % (i, p["block"], p["tones"], p["mvol"], p["prior"], p["mode"], p["mpan"], p["attr"]))
            for t in v["tones"][p["block"] * 16:p["block"] * 16 + p["tones"]]:
                lines.append("    tone: " + " ".join("%s %d" % (f, t[f]) for f in TONE_FIELDS[:14]) +
                             " adsr1 %04X adsr2 %04X prog %d vag %d" % (t["adsr1"], t["adsr2"], t["prog"], t["vag"]))
    for i, s in enumerate(v["samples"]):
        lines.append("sample %3d: %d bytes, %d blocks, sha256 %s" % (i + 1, len(s), len(s) // 16,
                                                                    hashlib.sha256(s).hexdigest()[:16]))
    return lines


def cmd_dump(a):
    with open(a.file, "rb") as f:
        print("\n".join(dump_lines(f.read())))


def cmd_build(a):
    bid, rows, st = build(a.disc, a.out)
    print("%s: %d songs (%d events, %d bytes), %d banks (%d samples, %d bytes) -> %s"
          % (bid, st["songs"], st["events"], st["song_bytes"], st["banks"], st["samples"], st["bank_bytes"],
             os.path.join(a.out, "base", "bgm")))
    print("songs with no note: %s" % (", ".join(map(str, st["empty"])) or "none"))


# ---------------------------------------------------------------- verify (needs the disc)

def cmd_verify(a):
    """Each song file read back against tools/bgm/inventory.py's parse of the
    same SEP (an independent walk): the note-ons, the controllers by number,
    the programs, the channels, the loop markers' ticks, the tempo changes and
    the end tick; each bank against tools/vag.py's reading of the same VAB
    (its program and sample counts and its sample bodies) and the header's
    raw records. Needs the sibling checkout (inventory.py imports from it)."""
    sys.path.insert(0, os.path.join(TOOLS, "bgm"))
    import inventory
    import vag
    disc = psx_disc.Disc(a.disc)
    table = songs(disc)
    bad, n_songs, n_banks, seen, dropped = [], 0, 0, {}, collections.Counter()
    for s in table:
        secs = seen.get(s["emi"]) or seen.setdefault(s["emi"], region_diff.emi_sections_blob(disc.read("BIN/" + s["emi"])))
        _, subs = inventory.parse_sep(secs[s["sep"]][3])
        ref = subs[s["sub"]]
        with open(os.path.join(a.cache, "base", "bgm", "%03d.DAT" % s["song"]), "rb") as f:
            got = read_song(f.read())
        ev = got["events"]
        ccs = collections.Counter(e[2] for e in ev if e[1] & 0xF0 == 0xB0)
        loops = [(e[0], "start" if e[3] == LOOP_START else "end") for e in ev
                 if e[1] & 0xF0 == 0xB0 and e[2] == CC_NRPN and e[3] in (LOOP_START, LOOP_END)]
        mine = dict(
            notes=sum(1 for e in ev if e[1] & 0xF0 == 0x90 and e[3]),
            ccs={hex(k): v for k, v in sorted(ccs.items())},
            programs=sorted({e[2] for e in ev if e[1] & 0xF0 == 0xC0}),
            channels=sorted({e[1] & 0x0F for e in ev if e[1] != 0xFF}),
            loops=loops,
            tempos=[(e[0], e[4]) for e in ev if e[1] == 0xFF and e[2] == META_TEMPO],
            ticks=got["end"], resolution=got["resolution"], tempo=got["tempo"],
            head=[x for x in (got["loop_start"], got["loop_end"]) if x != NONE])
        theirs = dict(notes=ref["notes"], ccs=ref["ccs"], programs=ref["programs"], channels=ref["channels"],
                      loops=[(x["tick"], x["kind"]) for x in ref["loops"]], tempos=[tuple(t) for t in ref["tempos"]],
                      ticks=ref["ticks"], resolution=ref["resolution"], tempo=ref["tempo_us"],
                      head=[x["tick"] for x in ref["loops"]][:1] + [x["tick"] for x in ref["loops"] if x["kind"] == "end"])
        for k in mine:
            if mine[k] != theirs[k]:
                bad.append("song %d: %s %r, inventory.py %r" % (s["song"], k, mine[k], theirs[k]))
        if got["bank"] != s["name"] or got["sub"] != s["sub"] or got["song"] != s["song"]:
            bad.append("song %d: header names %s sub %d" % (s["song"], got["bank"], got["sub"]))
        n_songs += 1
    for emi, secs in sorted(seen.items()):
        name = emi.rsplit("/", 1)[-1][:-4]
        vh = next(x[3] for x in secs if x[1] == 6)
        vb = next(x[3] for x in secs if x[1] == 7)
        with open(os.path.join(a.cache, "base", "bgm", "bank", name + ".DAT"), "rb") as f:
            got = read_bank(f.read())
        programs, = struct.unpack_from("<H", vh, 0x12)
        nvag, = struct.unpack_from("<H", vh, 0x16)
        bodies = list(vag._vags({6: (0, 6, 0, vh), 7: (0, 7, 0, vb)}))
        if (got["ps"], got["vs"]) != (programs, nvag) or got["samples"] != bodies:
            bad.append("bank %s: %d programs, %d samples against vag.py's %d, %d (or a body differs)"
                       % (name, got["ps"], got["vs"], programs, nvag))
        tone_at = VAB_HEAD + 128 * VAB_PROG
        for k, t in enumerate(got["tones"]):
            raw = vh[tone_at + VAB_TONE * k:tone_at + VAB_TONE * (k + 1)]
            again = struct.pack("<14B2xHHHH8x", *[t[f] for f in TONE_FIELDS])
            if again[:14] != raw[:14] or again[16:24] != raw[16:24]:
                bad.append("bank %s tone %d: not the header's record" % (name, k))
            dropped[(raw[14:16] + raw[24:32]).hex()] += 1
        used = sum(1 for p in got["programs"] if p["tones"])
        if used != got["ps"] or sum(p["tones"] for p in got["programs"]) != got["ts"]:
            bad.append("bank %s: program / tone counts" % name)
        n_banks += 1
    print("verify: %d songs against inventory.py, %d banks against vag.py and the headers: %d problem(s)"
          % (n_songs, n_banks, len(bad)))
    print("the tone records' reserved bytes (not carried), by value: %s"
          % ", ".join("%s x %d" % kv for kv in sorted(dropped.items())))
    for x in bad[:30]:
        print("   ", x)
    return 1 if bad else 0


# ---------------------------------------------------------------- check (no game data)

def synthetic():
    """A tiny SEP and VAB made here, with the events they must read back as.
    The SEP has two sub-songs (the second empty), running status across a
    note-on with velocity 0, a tempo change, the loop markers, a program
    change, pitch bend and pressure; the VAB has programs 0 and 3 (a gap,
    as 27 of the disc's 81 banks have) and two samples."""
    ev = bytes([0x00, 0xFF, 0x51, 0x07, 0xA1, 0x20,     # tempo 500000
                0x00, 0xC1, 0x03,                       # program 3, channel 1
                0x00, 0xB1, 99, 20, 0x00, 6, 127,       # loop start, running status
                0x00, 0x91, 60, 100,
                0x81, 0x00, 60, 0,                      # running status, velocity 0, delta 128
                0x10, 0xE1, 0x00, 0x48,
                0x10, 0xD1, 0x40,
                0x10, 0xFF, 0x51, 0x0F, 0x42, 0x40,     # tempo 1000000
                0x00, 0x81, 62, 64,
                0x08, 0xB1, 99, 30,                     # loop end
                0x00, 0xFF, 0x2F, 0x00])
    empty = bytes([0x00, 0xFF, 0x2F, 0x00])
    sep = b"pQES" + struct.pack(">H", 0)
    for sid, body in ((0, ev), (1, empty)):
        sep += struct.pack(">HH", sid, 480) + bytes([0x07, 0xA1, 0x20, 0x04, 0x02]) + struct.pack(">I", len(body)) + body
    want = [(0, 0xFF, 0x51, 0, 500000), (0, 0xC1, 3, 0, 0), (0, 0xB1, 99, 20, 0), (0, 0xB1, 6, 127, 0),
            (0, 0x91, 60, 100, 0), (128, 0x91, 60, 0, 0), (144, 0xE1, 0, 0x48, 0), (160, 0xD1, 0x40, 0, 0),
            (176, 0xFF, 0x51, 0, 1000000), (176, 0x81, 62, 64, 0), (184, 0xB1, 99, 30, 0), (184, 0xFF, 0x2F, 0, 0)]
    progs = {0: (2, 100, 1, 0, 64, 0x1234), 3: (1, 90, 2, 0, 32, 7)}
    ps, ts, vs = 2, 3, 2
    bodies = [bytes(16) + bytes([0x12, 0x00] + list(range(14))) + bytes([0x00, 0x07] + [0] * 14),
              bytes(16) + bytes([0x31, 0x03] + [0x77] * 14)]
    vh = bytearray(struct.pack("<4sIIIHHHHBBBBI", b"pBAV", 7, 0, 0, 0, ps, ts, vs, 120, 64, 0, 0, 0))
    for i in range(128):
        t, mv, pr, mo, mp, at = progs.get(i, (0, 0, 0, 0, 0, 0))
        vh += struct.pack("<BBBBBBhII", t, mv, pr, mo, mp, 0, at, 0, 0)
    tones = []
    for k, (p, n) in enumerate(((0, 2), (3, 1))):
        for t in range(16):
            if t < n:
                f = (k + t, 4, 100 - t, 64, 60 + t, 3, 0, 127, 0, 0, 0, 0, 2, 2, 0x80FF, 0x5FC0 + t, p, 1 + (k + t) % 2)
            else:
                f = (0,) * 14 + (0, 0, 0, 0)
            tones.append(f)
            vh += struct.pack("<14B2xHHhh8x", *f)
    sizes = [0] + [len(b) // 8 for b in bodies] + [0] * (255 - vs)
    vh += struct.pack("<256H", *sizes)
    vb = b"".join(bodies)
    struct.pack_into("<I", vh, 12, len(vh) + len(vb))
    return sep, want, bytes(vh), vb, progs, tones, bodies


def check():
    """The synthetic round trip: SEP and VAB made in memory -> written in the
    cache's format -> read back -> equal to what was made, and dumped. No
    game data. Returns a list of errors."""
    sep, want, vh, vb, progs, tones, bodies = synthetic()
    errs = []
    subs = read_sep(sep)
    if len(subs) != 2 or subs[0]["events"] != want or subs[0]["walked"] != subs[0]["size"]:
        errs.append("SEP: the events read are not the ones made")
    song = read_song(song_bytes(7, "SYNTH", 0, subs[0]))
    if (song["events"], song["loop_start"], song["loop_end"], song["end"], song["flags"], song["tempo"],
            song["resolution"], song["rhythm"], song["bank"], song["song"]) != \
            (want, 0, 184, 184, 1, 500000, 480, b"\x04\x02", "SYNTH", 7):
        errs.append("song: the file does not read back as written")
    once = read_song(song_bytes(8, "SYNTH", 1, subs[1]))
    if (once["flags"], once["loop_start"], once["loop_end"], len(once["events"])) != (0, NONE, NONE, 1):
        errs.append("song: an empty sub-song does not read back as one that does not loop")
    v = parse_vab(vh, vb)
    back = read_bank(bank_bytes("SYNTH", v))
    if back["programs"] != v["programs"] or back["tones"] != v["tones"] or back["samples"] != bodies or \
            [back[k] for k in ("ps", "ts", "vs", "mvol", "pan", "name")] != [2, 3, 2, 120, 64, "SYNTH"]:
        errs.append("bank: the file does not read back as written")
    if [p["block"] for p in back["programs"][:4]] != [0, 0xFF, 0xFF, 1] or \
            [tuple(t[f] for f in TONE_FIELDS) for t in back["tones"]] != [tuple(t) for t in tones] or \
            {i: tuple(p[f] for f in PROG_FIELDS) for i, p in enumerate(back["programs"]) if p["tones"]} != progs:
        errs.append("bank: the programs, blocks or tones are not the ones made")
    for bad, why in ((vh[:4] + struct.pack("<I", 7) + vh[8:20] + struct.pack("<H", 5) + vh[22:], "a tone count"),
                     (vh, "a body size")):
        try:
            parse_vab(bad, vb if why != "a body size" else vb + bytes(16))
            errs.append("VAB: %s that disagrees was not refused" % why)
        except ValueError:
            pass
    for blob in (b"BF3S" + struct.pack("<I", 9) + bytes(48), b"BF3B" + struct.pack("<I", 1) + bytes(48)):
        try:
            dump_lines(blob)
            errs.append("a wrong version was not refused")
        except ValueError:
            pass
    try:
        read_sep(sep[:-4] + bytes([0x00, 0xFF, 0x01, 0x00]))
        errs.append("SEP: an unknown meta event was not refused")
    except ValueError:
        pass
    if len(dump_lines(song_bytes(7, "SYNTH", 0, subs[0]))) != 3 + len(want) or \
            len(dump_lines(bank_bytes("SYNTH", v))) != 1 + 2 + 3 + 2:
        errs.append("dump: not one line per event / program / tone / sample")
    return errs


def cmd_check(a):
    errs = check()
    for e in errs:
        print("ERROR", e)
    print("seq check: synthetic SEP and VAB round trip, %d error(s)" % len(errs))
    return 1 if errs else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    p = s.add_parser("build", help="write CACHE/base/bgm/ from a PSX disc")
    p.add_argument("--disc", required=True)
    p.add_argument("--out", required=True)
    p = s.add_parser("dump", help="a song's header and events, or a bank's programs, tones and samples")
    p.add_argument("file")
    p = s.add_parser("verify", help="a built cache against inventory.py's and vag.py's parse of the disc")
    p.add_argument("--disc", required=True)
    p.add_argument("--cache", required=True)
    s.add_parser("check", help="the synthetic round trip (no game data)")
    p = s.add_parser("fixture", help="fixtures/bgm.tsv: the hashes of what build writes from each disc")
    p.add_argument("--disc", action="append", required=True)
    p.add_argument("--out", default=FIXTURE)
    a = ap.parse_args()
    sys.exit({"build": cmd_build, "dump": cmd_dump, "verify": cmd_verify, "check": cmd_check,
              "fixture": cmd_fixture}[a.cmd](a) or 0)


if __name__ == "__main__":
    main()
