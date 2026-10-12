#!/usr/bin/env python
"""PSX SPU ADPCM ("VAG") decoding, and the PC port's audio banks from a disc.

docs/sound-import.md. The SPU's sample format, as the public descriptions
give it: a stream of 16-byte blocks of 28 samples each.

    byte 0   shift (low nibble) and filter (high nibble, 0..4)
    byte 1   flags: bit 0 end, bit 1 repeat, bit 2 loop start
    2..15    28 four-bit signed nibbles, low nibble first

A sample is the nibble put in the top of a 16-bit word and shifted right by
`shift`, plus the filter's prediction from the two previous outputs,
(s1 * k0 + s2 * k1) / 64 with (k0, k1) one of five pairs.

A PSX `.EMI` sound bank is three sections in a row: type 6 the VAB header
(`pBAV`), type 8 the cue table (4 bytes a cue), type 7 the VAB body - the VAG
samples back to back, sizes in the header's table. The PC's kind-2 chunk is
the same bank converted: 24 cues of four voice words, 64 voice entries
(offset, size), one RIFF WAVE per sample a cue plays. `bank` builds it from
the three sections; `compare` measures it against a PC install.

A PSP disc's banks (`PPHD` / `pBVC`) are read by `psp_vab` for the effects and
`psp_music_vab` for the music, the latter into the VAB that tools/seq.py's
`parse_vab` reads (docs/psp-music-banks.md); `check` tests both on synthetic
banks.

    python tools/vag.py compare --disc JP.cue --dat DAT
    python tools/vag.py check

Decoded audio is game data: scratch or analysis/ only (CLAUDE.md rule 1).
"""
import argparse
import collections
import hashlib
import os
import struct
import sys

FILTERS = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))
CUES, VOICES = 24, 64
VOICE_TABLE, DATA = 0x180, 0x380
VAB_PROGRAMS, TONE, SE_VOICE = 128, 32, 16
SPU_RATE, WAVE_RATE = 44100, 22050


def _nibbles(body, p):
    for i in range(28):
        b = body[p + 2 + (i >> 1)]
        n = (b >> 4) if i & 1 else (b & 0x0F)
        yield n - 16 if n & 8 else n


def decode(body):
    """The SPU's integer decode: 16-bit PCM samples (a list), clamped. The
    prediction floors with no +32 - the SPU model's reading R1, which the
    renders settled on 2026-10-10 (docs/spu-model.md R1, music-open-ends.md 2);
    shift 13..15 is taken raw (R2 says 9), which no block of this game uses."""
    out, s1, s2 = [], 0, 0
    for p in range(0, len(body) - 15, 16):
        shift = body[p] & 0x0F
        k0, k1 = FILTERS[body[p] >> 4]
        for n in _nibbles(body, p):
            s = ((n << 12) >> shift) + ((s1 * k0 + s2 * k1) >> 6)
            s = -32768 if s < -32768 else 32767 if s > 32767 else s
            out.append(s)
            s1, s2 = s, s1
    return out


def decode_port(body):
    """The port's converter, as measured against all 4,779 of its WAVs
    (docs/sound-import.md section 2): the same recurrence in double precision
    on the unrounded, unclamped history; each output (int)(x + 0.5) - C's
    truncation toward zero - kept in 16 bits by wrapping, not clamping."""
    out, s1, s2 = [], 0.0, 0.0
    for p in range(0, len(body) - 15, 16):
        scale = 2.0 ** (12 - (body[p] & 0x0F))
        k0, k1 = FILTERS[body[p] >> 4]
        k0, k1 = k0 / 64, k1 / 64
        for n in _nibbles(body, p):
            s = n * scale + s1 * k0 + s2 * k1
            s1, s2 = s, s1
            out.append(((int(s + 0.5) + 0x8000) & 0xFFFF) - 0x8000)
    return out


def trim(body):
    """(the blocks the port converts, loops): from block 1 (block 0 is the
    zero block every VAG opens with) to the first block flagged end without
    repeat (1), which drops the flag-7 block after it; a sample with no such
    block repeats and is converted whole."""
    for j in range(1, len(body) // 16):
        if body[16 * j + 1] == 1:
            return body[16:16 * (j + 1)], False
    return body[16:], True


def wave(pcm):
    """A RIFF WAVE as the port writes them: PCM, mono, 16-bit, the header's
    rate 22,050 Hz whatever the sample's (the cue word carries the rate)."""
    data = struct.pack("<%dh" % len(pcm), *pcm)
    return (b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " +
            struct.pack("<IHHIIHH", 16, 1, 1, WAVE_RATE, WAVE_RATE * 2, 2, 16) +
            b"data" + struct.pack("<I", len(data)) + data)


def groups(sections):
    """The sound banks of an EMI's section list [(index, type, dest, bytes)],
    in order: a type 6 opens a group, the 8 and 7 after it join it. A group
    with a type 10 (a SEQ, dest 0) is music, which the port dropped
    (DAT_CONTAINER.md section 2)."""
    out = []
    for s in sections:
        if s[1] == 6:
            out.append({6: s})
        elif s[1] in (7, 8, 10) and out and s[1] not in out[-1]:
            out[-1][s[1]] = s
    return [g for g in out if 10 not in g]


def frequency(tone):
    """The rate a tone plays at: the SPU's 44.1 kHz moved by the tone's key
    (its lowest note, byte 6) less its centre note (4) plus the fine tune
    (5, 1/128 semitone), truncated."""
    return int(SPU_RATE * 2 ** ((tone[6] - tone[4] + tone[5] / 128) / 12))


def bank(vh, cues, vb, cache=None):
    """The PC's kind-2 payload from the VAB header, cue table and body."""
    if vh[:4] != b"pBAV":
        raise ValueError("not a VAB header")
    programs, = struct.unpack_from("<H", vh, 0x12)
    nvag, = struct.unpack_from("<H", vh, 0x16)
    tones = 32 + VAB_PROGRAMS * 16
    sizes = struct.unpack_from("<256H", vh, tones + programs * 16 * TONE)
    if nvag >= VOICES or len(cues) // 4 > CUES:
        raise ValueError("bank too large: %d samples, %d cues" % (nvag, len(cues) // 4))
    vags, pos = {}, 0
    for v in range(1, nvag + 1):
        vags[v] = vb[pos:pos + sizes[v] * 8]
        pos += sizes[v] * 8
    cache = {} if cache is None else cache
    waves = {}

    def converted(v):
        body = vags.get(v, b"")
        key = hashlib.sha256(body).digest()
        if key not in cache:
            blocks, loops = trim(body) if body else (b"", False)
            pcm = decode_port(blocks)
            cache[key] = (wave(pcm) if any(pcm) else None, loops)
        return cache[key]

    head = bytearray(DATA)
    for i in range(len(cues) // 4):
        flags, prog, tone, chord = cues[4 * i:4 * i + 4]
        voices = ((chord >> 4) >> 1) + 1 if chord >> 4 else 0
        if voices > 4:      # a cue's slot is 16 bytes, four words; more would write into the next cue's
            raise ValueError("cue %d: chord 0x%02X asks for %d voices, a cue holds 4" % (i, chord, voices))
        for j in range(voices):
            t = vh[tones + (prog & 0x7F) * 16 * TONE + ((tone >> 4) + j) * TONE:][:TONE]
            v, = struct.unpack_from("<H", t, 22)
            channel = (SE_VOICE + (chord & 0x0F) + j) << 8
            w, loops = converted(v)
            if w is None:            # a silent sample keys the voice off
                word = channel | 0xFF
            else:
                word = frequency(t) << 16 | (0x8000 if loops else 0) | channel | v
                waves[v] = w
            struct.pack_into("<I", head, 16 * i + 4 * j, word)
    body, off = [], DATA
    for v in sorted(waves):
        struct.pack_into("<II", head, VOICE_TABLE + 8 * v, off, len(waves[v]))
        body.append(waves[v])
        off += len(waves[v])
    return bytes(head) + b"".join(body)


def banks_of(sections, cache=None):
    """[(dest, payload)] for every sound bank of an EMI, in order."""
    return [(g[6][2], bank(*_vab_cues(g), cache)) for g in groups(sections)]


def _vab_cues(g):
    vh, vb = _vab(g)
    return vh, g[8][3] if 8 in g else b"", vb


# psp-unwrap for banks (docs/psp-only-build.md section 2.1, docs/psp-music-banks.md).
# The PSP keeps the EMI's sound bank as three sections of the same types, the VAB
# re-containered: type 6 a `PPHD` header, type 7 a `pBVC` body, type 8 the cue
# table unchanged. `PPHD` +0x10, +0x14, +0x18 are the offsets of its `PPPG`, `PPTN`
# and `PPVA` blocks. `PPPG` +0x20: 128 program offsets (-1, none), a program a tone
# count, three -1 words, then that many indices into `PPTN`. `PPTN`: the record size
# at +8, the count less one at +0x14, records from +0x18. `PPVA` +0x14 the count less
# one, from +0x20 one (offset in `pBVC`, rate, size, -1) per sample.
#
# A `PPTN` record is 24 words (0x60 bytes, every music bank of both PSPs). Read
# against the PSX-JP VAB of all 119 music banks, 2,939 tones (psp-music-banks.md 2):
#   2      the VAG less one (-1: sample 0, silent)
#   6, 7   lowest and highest note          8      the tone's mode (bit 2 reverb)
#   10     volume: tone x program x VAB master, (tv * pm * bm * 32) // 127**2
#   11     pan: (tone pan + program pan + 64) // 3
#   12, 13 words 10 and 11 again            14, 15 centre note, fine tune
#   18, 19 pitch-bend range down, up        22, 23 ADSR words 1 and 2
#   0, 1, 9, 17, 21 are -1 and 3, 4, 5, 16, 20 are 0 in every record.
PSP_HEAD, PSP_BODY = b"PPHD", b"pBVC"
PSP_TONE_WORDS = 24
PSP_TONE_FIXED = ((0, -1), (1, -1), (3, 0), (4, 0), (5, 0), (9, -1), (16, 0), (17, -1), (20, 0), (21, -1))
PSP_VOLUME_UNIT = 32            # word 10 is the merged volume (0..127) times 32, floored
VAB_HEAD, VAB_VERSION = 32, 7


def psp_bank(ph, pbvc):
    """The PSP's `PPHD` and `pBVC` read: ({program: (tone record, ...)}, [sample
    bytes]), a record the tuple of its words, a sample its ADPCM blocks."""
    if ph[:4] != PSP_HEAD or pbvc[:4] != PSP_BODY:
        raise ValueError("not a PSP sound bank")
    pg, tn, va = struct.unpack_from("<3I", ph, 0x10)
    if (ph[pg:pg + 4], ph[tn:tn + 4], ph[va:va + 4]) != (b"PPPG", b"PPTN", b"PPVA"):
        raise ValueError("PPHD: block tags not where its header says")
    size, = struct.unpack_from("<I", ph, tn + 8)
    ntones = struct.unpack_from("<i", ph, tn + 0x14)[0] + 1
    nvag = struct.unpack_from("<i", ph, va + 0x14)[0] + 1
    if size % 4 or size < 0x40:
        raise ValueError("PPTN: record size %d" % size)
    programs = {}
    for p in range(VAB_PROGRAMS):
        o, = struct.unpack_from("<i", ph, pg + 0x20 + 4 * p)
        if o == -1:
            continue
        n, = struct.unpack_from("<I", ph, o)
        if n > 16:
            raise ValueError("PPHD: program %d has %d tones, a VAB program holds 16" % (p, n))
        records = []
        for t in struct.unpack_from("<%dI" % n, ph, o + 16):
            if t >= ntones:
                raise ValueError("PPHD: program %d tone %d past the tone table" % (p, t))
            records.append(struct.unpack_from("<%di" % (size // 4), ph, tn + 0x18 + size * t))
        programs[p] = tuple(records)
    samples = []
    for i in range(nvag):
        off, _, n = struct.unpack_from("<3I", ph, va + 0x20 + 16 * i)
        if n % 8 or off + n > len(pbvc):
            raise ValueError("PPVA: sample %d (offset 0x%X, size %d) not in pBVC" % (i, off, n))
        samples.append(pbvc[off:off + n])
    return programs, samples


def psp_dropped_tone(programs, nsamples, cues):
    """The tone the PSP's converter dropped, derived from the bank's own data:
    (program, slot, sample index) or None. psp-music-banks.md 5.

    A cue's chord keys tones (tone >> 4) .. + voices - 1 of its program, whatever
    the program's count (`bank`, SsUtKeyOnV); the PSX kept tone slots past the count,
    the PSP's converter only the declared ones. When a chord reaches one slot past
    a program whose declared tones are all silent (sample 0), and the bank holds
    exactly one sample that no tone references, that sample is the dropped tone's,
    at the declared tone's pitch and range. Anything less determined is None."""
    used = {r[2] for ts in programs.values() for r in ts}
    orphans = [i for i in range(nsamples) if i not in used]
    wanted = set()
    for i in range(len(cues) // 4):
        _, prog, tone, chord = cues[4 * i:4 * i + 4]
        voices = ((chord >> 4) >> 1) + 1 if chord >> 4 else 0
        ts = programs.get(prog & 0x7F)
        if voices and ts and (tone >> 4) + voices > len(ts) and all(r[2] == -1 for r in ts):
            wanted.add((prog & 0x7F, (tone >> 4) + voices))
    if len(wanted) != 1 or len(orphans) != 1:
        return None
    p, reach = wanted.pop()
    if reach != len(programs[p]) + 1:
        return None
    return p, len(programs[p]), orphans[0]


def psp_vab(ph, pbvc, cues=b""):
    """A VAB header and body, as far as `bank` reads them (each tone's sample,
    centre, fine tune and range), from the PSP's `PPHD` and `pBVC` sections. With
    the bank's cue table, the tone `psp_dropped_tone` derives is put back."""
    programs, samples = psp_bank(ph, pbvc)
    count = max(programs) + 1 if programs else 0
    tones = 32 + VAB_PROGRAMS * 16
    vh = bytearray(tones + count * 16 * TONE + 512)
    vh[:4] = b"pBAV"
    struct.pack_into("<HH", vh, 0x12, count, 0)
    struct.pack_into("<H", vh, 0x16, len(samples))
    slots = [(p, j, r, r[2]) for p, ts in programs.items() for j, r in enumerate(ts)]
    dropped = psp_dropped_tone(programs, len(samples), cues) if cues else None
    if dropped:
        p, j, sample = dropped
        slots.append((p, j, programs[p][-1], sample))
    for p, j, r, sample in slots:
        o = tones + (p * 16 + j) * TONE
        vh[o + 4:o + 8] = bytes((r[14] & 0xFF, r[15] & 0xFF, r[6] & 0xFF, r[7] & 0xFF))
        struct.pack_into("<H", vh, o + 22, sample + 1)
    sizes = tones + count * 16 * TONE
    for i, s in enumerate(samples):
        struct.pack_into("<H", vh, sizes + 2 * (i + 1), len(s) // 8)
    return bytes(vh), b"".join(samples)


def psp_music_vab(pphd, pbvc):
    """A music bank's VAB header and body, as tools/seq.py's `parse_vab` reads a
    PSX disc's (`base/bgm/bank/`), from the PSP's `PPHD` and `pBVC` sections.

    The samples, mode, pitch (centre, fine), range, bend range and ADSR are the
    PSP's, which are the PSX's (psp-music-banks.md 2). Volume and pan are the
    inverse of the PSP's merge, a rule (section 3): the VAB master and program
    volumes 127, each tone's volume the merged one rounded, (2w + 33) // 64; the
    program pans 64, each tone's pan 3p - 128 clamped to 0..127. What the PSP
    does not carry is written as 0: the program priority and mode, the VAB's
    attributes, the tone priority and vibrato / portamento, and the tone slots
    past each program's count (section 4: the sequencer reads none of them)."""
    programs, samples = psp_bank(pphd, pbvc)
    if len(samples) > 255:
        raise ValueError("PPHD: %d samples, a VAB holds 255" % len(samples))
    for p, ts in programs.items():
        for j, r in enumerate(ts):
            if len(r) != PSP_TONE_WORDS or any(r[w] != v for w, v in PSP_TONE_FIXED):
                raise ValueError("PPTN: program %d tone %d is not the 24-word record this reader knows" % (p, j))
            if r[10] != r[12] or r[11] != r[13] or not 0 <= r[10] <= 127 * PSP_VOLUME_UNIT or not 0 <= r[11] <= 127:
                raise ValueError("PPTN: program %d tone %d volume %d / %d, pan %d / %d" % (p, j, r[10], r[12], r[11], r[13]))
            if not -1 <= r[2] < len(samples):
                raise ValueError("PPTN: program %d tone %d sample %d of %d" % (p, j, r[2] + 1, len(samples)))
            if any(not -128 <= r[w] <= 255 for w in (6, 7, 8, 14, 15, 18, 19)) or any(not 0 <= r[w] <= 0xFFFF for w in (22, 23)):
                raise ValueError("PPTN: program %d tone %d has a field out of its VAB byte" % (p, j))
    with_tones = [p for p in sorted(programs) if programs[p]]
    ps, ts = len(with_tones), sum(len(programs[p]) for p in with_tones)
    tone_at = VAB_HEAD + VAB_PROGRAMS * 16
    sizes_at = tone_at + ps * 16 * TONE
    vh = bytearray(sizes_at + 512)
    vb = b"".join(samples)
    struct.pack_into("<4sIIIHHHHBBBBI", vh, 0, b"pBAV", VAB_VERSION, 0, len(vh) + len(vb), 0, ps, ts,
                     len(samples), 127, 64, 0, 0, 0)
    for block, p in enumerate(with_tones):
        struct.pack_into("<6B", vh, VAB_HEAD + 16 * p, len(programs[p]), 127, 0, 0, 64, 0)
        for j, r in enumerate(programs[p]):
            vol = (2 * r[10] + PSP_VOLUME_UNIT + 1) // (2 * PSP_VOLUME_UNIT)
            pan = min(127, max(0, 3 * r[11] - 128))
            struct.pack_into("<14B2xHHhh", vh, tone_at + (block * 16 + j) * TONE,
                             0, r[8] & 0xFF, vol, pan, r[14] & 0xFF, r[15] & 0xFF, r[6] & 0xFF, r[7] & 0xFF,
                             0, 0, 0, 0, r[18] & 0xFF, r[19] & 0xFF, r[22], r[23], p, r[2] + 1)
    for i, s in enumerate(samples):
        if len(s) % 16:
            raise ValueError("PPVA: sample %d is %d bytes, not whole 16-byte blocks" % (i, len(s)))
        struct.pack_into("<H", vh, sizes_at + 2 * (i + 1), len(s) // 8)
    return bytes(vh), vb


def _vab(g):
    """(header, body) of a sound-bank group, unwrapping a PSP disc's."""
    if g[6][3][:4] == PSP_HEAD:
        return psp_vab(g[6][3], g[7][3], g[8][3] if 8 in g else b"")
    return g[6][3], g[7][3]


_CACHE = {}


def bank_from_disc(build, dat_name, ordinal):
    """The `ordinal`-th kind-2 chunk of the PC container `dat_name`, built
    from the same-named EMI of a disc (region_diff.Build): (EMI key, the VAB
    header's section index, payload), or None if the disc has no such bank.
    The PC's banks pair with the EMI's groups in order (sound-import.md 2)."""
    stem = dat_name[:-4]
    key = next((k for k in build.emis if k.rsplit("/", 1)[-1][:-4] == stem), None)
    if key is None:
        return None
    gs = groups(build.sections(key))
    if ordinal >= len(gs):
        return None
    g = gs[ordinal]
    vh, vb = _vab(g)
    return key, g[6][0], bank(vh, g[8][3] if 8 in g else b"", vb, _CACHE)


def importer_source(src, f, slot, ch):
    """tools/importer.py build's wave-from-vag: a recipe `bank` chunk from a
    PSX or PSP disc source, as importer's (build, (build, how, EMI, section),
    payload), or None - no such bank, or one that is not the recipe's (the
    PAL discs' eight swapped area banks, region-diff.md 8.4). The PSP's one
    bank with a dropped tone (16 chunks) is put back by `psp_dropped_tone`'s
    rule, psp-music-banks.md 5."""
    if ch["class"] != "bank" or not src.id.startswith(("psx-", "psp-")) or not hasattr(src, "build"):
        return None
    got = bank_from_disc(src.build, f["name"], sum(1 for c in f["chunks"][:slot] if c["kind"] == 2))
    if got is None or hashlib.sha256(got[2]).hexdigest() != ch["sha256"]:
        return None
    return src.id, (src.id, "wave-from-vag", got[0], got[1]), got[2]


def cmd_compare(a):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import dat
    import region_diff
    disc = region_diff.Build("disc", a.disc)
    by_stem = {k.rsplit("/", 1)[-1][:-4]: k for k in disc.emis}
    st, cache, diff = collections.Counter(), {}, []
    hw = collections.Counter()
    for name in sorted(os.listdir(a.dat)):
        stem = name[:-4]
        if a.names and stem not in a.names:
            continue
        blob, chunks = dat.load(os.path.join(a.dat, name))
        pcs = [c for c in chunks if c.kind == 2]
        if not pcs:
            continue
        secs = disc.sections(by_stem[stem])
        ours = banks_of(secs, cache)
        if len(ours) != len(pcs) or any(c.tag != d for c, (d, _) in zip(pcs, ours)):
            st["unpaired"] += len(pcs)
            diff.append("%s: %d banks on the disc, %d on the PC" % (stem, len(ours), len(pcs)))
            continue
        for c, (_, b) in zip(pcs, ours):
            same = blob[c.offset:c.offset + c.size] == b
            st["identical" if same else "differ"] += 1
            if not same:
                diff.append("%s chunk %d" % (stem, c.index))
        if a.hardware:
            for g in groups(secs):
                for blk in set(_vags(g)):
                    x, _ = trim(blk)
                    p, h = decode_port(x), decode(x)
                    d = max((abs(i - j) for i, j in zip(p, h)), default=0)
                    hw["samples"] += len(p)
                    hw["differ"] += sum(i != j for i, j in zip(p, h))
                    hw["max"] = max(hw["max"], d)
    print("banks: %d identical, %d differ, %d unpaired" % (st["identical"], st["differ"], st["unpaired"]))
    for d in diff[:20]:
        print("  ", d)
    if a.hardware:
        print("port's decode against the SPU's integer decode: %d of %d samples differ, max |d| %d"
              % (hw["differ"], hw["samples"], hw["max"]))
    return 1 if st["differ"] or st["unpaired"] else 0


def _vags(g):
    vh, vb = g[6][3], g[7][3]
    programs, = struct.unpack_from("<H", vh, 0x12)
    nvag, = struct.unpack_from("<H", vh, 0x16)
    sizes = struct.unpack_from("<256H", vh, 32 + VAB_PROGRAMS * 16 + programs * 16 * TONE)
    pos = 0
    for v in range(1, nvag + 1):
        yield vb[pos:pos + sizes[v] * 8]
        pos += sizes[v] * 8


def _psp_synthetic(programs, samples):
    """A `PPHD` and `pBVC` laid out as the PSP's (the comment above `PSP_HEAD`),
    from {program: [(tone fields), ...]} and [sample bytes]: for `check` only, no
    game data. A tone's fields: (sample index or -1, min, max, mode, vol, pan,
    centre, fine, pbmin, pbmax, adsr1, adsr2), vol and pan already merged."""
    pg, tn = 0x40, 0x40 + 0x20 + 4 * VAB_PROGRAMS + sum(16 + 4 * len(t) for t in programs.values())
    records = [t for p in sorted(programs) for t in programs[p]]
    va = tn + 0x18 + 0x60 * len(records)
    ph = bytearray(va + 0x20 + 16 * len(samples))
    ph[:4] = PSP_HEAD
    struct.pack_into("<3I", ph, 0x10, pg, tn, va)
    ph[pg:pg + 4], ph[tn:tn + 4], ph[va:va + 4] = b"PPPG", b"PPTN", b"PPVA"
    struct.pack_into("<%di" % VAB_PROGRAMS, ph, pg + 0x20, *([-1] * VAB_PROGRAMS))
    o, k = pg + 0x20 + 4 * VAB_PROGRAMS, 0
    for p in sorted(programs):
        struct.pack_into("<i", ph, pg + 0x20 + 4 * p, o)
        struct.pack_into("<I3i", ph, o, len(programs[p]), -1, -1, -1)
        struct.pack_into("<%dI" % len(programs[p]), ph, o + 16, *range(k, k + len(programs[p])))
        o, k = o + 16 + 4 * len(programs[p]), k + len(programs[p])
    struct.pack_into("<I", ph, tn + 8, 0x60)
    struct.pack_into("<i", ph, tn + 0x14, len(records) - 1)
    for i, (s, lo, hi, mode, vol, pan, centre, fine, bmin, bmax, a1, a2) in enumerate(records):
        w = [0] * PSP_TONE_WORDS
        for j, v in PSP_TONE_FIXED:
            w[j] = v
        w[2], w[6], w[7], w[8], w[10], w[11], w[12], w[13] = s, lo, hi, mode, vol, pan, vol, pan
        w[14], w[15], w[18], w[19], w[22], w[23] = centre, fine, bmin, bmax, a1, a2
        struct.pack_into("<24i", ph, tn + 0x18 + 0x60 * i, *w)
    struct.pack_into("<i", ph, va + 0x14, len(samples) - 1)
    body, off = bytearray(PSP_BODY + bytes(0x3C)), 0x40
    for i, s in enumerate(samples):
        struct.pack_into("<3Ii", ph, va + 0x20 + 16 * i, off, SPU_RATE, len(s), -1)
        body += s
        off += len(s)
    return bytes(ph), bytes(body)


def check():
    """The PSP readers on synthetic banks (no game data): the music bank's
    fields and the volume / pan inverse through tools/seq.py's parse_vab, and
    the dropped-tone rule's one case and its refusals. Returns the errors."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import seq
    errs = []
    blocks = [bytes(16) + bytes((0x12, 0)) + bytes(14) + bytes((0x12, 1)) + bytes(14),
              bytes(16) + bytes((0x23, 3)) + bytes(14) + bytes((0x23, 7)) + bytes(14) + bytes(32)]
    # tone volume 90 under program 123 and master 123 merges to 2,701; pan 110
    # under program 64 to 79, pan 0 to 42, pan 127 to 85 (the measured merge)
    def merged(tv, pm, bm):
        return tv * pm * bm * PSP_VOLUME_UNIT // (127 * 127)
    tones = {0: [(0, 36, 36, 0, merged(90, 123, 123), (110 + 64 + 64) // 3, 44, 0, 0, 0, 0x80FF, 0x584D),
                 (1, 0, 127, 4, merged(127, 127, 127), (0 + 64 + 64) // 3, 200 - 256, 65, 12, 2, 0x80FF, 0x5FCD)],
             5: [(-1, 0, 127, 4, merged(64, 127, 127), (127 + 64 + 64) // 3, 60, 0, 0, 0, 1, 2)]}
    ph, pb = _psp_synthetic(tones, blocks)
    vh, vb = psp_music_vab(ph, pb)
    v = seq.parse_vab(vh, vb)
    want = [(0, 0, 84, 109, 44, 0, 36, 36, 0, 0, 0x80FF, 0x584D, 1),     # 2,701 / 32 = 84.4; 3 x 79 - 128
            (0, 4, 127, 0, 200, 65, 0, 127, 12, 2, 0x80FF, 0x5FCD, 2),
            (5, 4, 64, 127, 60, 0, 0, 127, 0, 0, 1, 2, 0)]
    got = [(t["prog"], t["mode"], t["vol"], t["pan"], t["center"], t["shift"], t["min"], t["max"], t["pbmin"], t["pbmax"],
            t["adsr1"], t["adsr2"], t["vag"]) for t in (v["tones"][0], v["tones"][1], v["tones"][16])]
    if got != want:
        errs.append("psp_music_vab: tones %s, want %s" % (got, want))
    if (v["ps"], v["ts"], v["vs"], v["mvol"], v["samples"]) != (2, 3, 2, 127, blocks):
        errs.append("psp_music_vab: ps %d ts %d vs %d mvol %d, or the samples" % (v["ps"], v["ts"], v["vs"], v["mvol"]))
    if [(p["tones"], p["mvol"], p["mpan"], p["block"]) for p in (v["programs"][0], v["programs"][5])] != [(2, 127, 64, 0), (1, 127, 64, 1)]:
        errs.append("psp_music_vab: programs %s" % [v["programs"][0], v["programs"][5]])
    for vol in range(PSP_VOLUME_UNIT * 127 + 1):     # the inverse lands within half a step of the merge
        ph, pb = _psp_synthetic({0: [(0, 0, 127, 0, vol, 64, 60, 0, 0, 0, 0, 0)]}, blocks[:1])
        tv = seq.parse_vab(*psp_music_vab(ph, pb))["tones"][0]["vol"]
        if abs(tv * PSP_VOLUME_UNIT - (vol + 0.5)) > PSP_VOLUME_UNIT / 2:
            errs.append("psp_music_vab: merged volume %d inverted to %d" % (vol, tv))
            break
    bad = list(tones[0][0])
    bad[4] += 1
    ph, pb = _psp_synthetic({0: [tuple(bad)]}, blocks[:1])
    ph = bytearray(ph)
    struct.pack_into("<i", ph, struct.unpack_from("<I", ph, 0x14)[0] + 0x18 + 4 * 12, bad[4] - 1)
    try:
        psp_music_vab(bytes(ph), pb)
        errs.append("psp_music_vab: words 10 and 12 differ and it did not refuse")
    except ValueError:
        pass
    # the dropped tone: program 1 declares one silent tone, a cue chords two, one
    # sample (index 2) is referenced by no tone
    se = {0: [(0, 24, 24, 0, 3200, 64, 47, 59, 0, 0, 0x80FF, 0x1FC0), (1, 24, 24, 4, 640, 64, 47, 59, 0, 0, 0x80FF, 0x1FC0)],
          1: [(-1, 24, 24, 0, 3200, 64, 44, 0, 0, 0, 0x80FF, 0x1FC0)]}
    samples = blocks + [bytes(16) + bytes((0x34, 1)) + bytes(14)]
    ph, pb = _psp_synthetic(se, samples)
    cues = bytes((0, 0x80, 0x0A, 0x34, 0, 0x81, 0x0A, 0x32))
    if psp_dropped_tone(psp_bank(ph, pb)[0], 3, cues) != (1, 1, 2):
        errs.append("psp_dropped_tone: %s, want (1, 1, 2)" % (psp_dropped_tone(psp_bank(ph, pb)[0], 3, cues),))
    vh, _ = psp_vab(ph, pb, cues)
    t = vh[32 + VAB_PROGRAMS * 16 + 17 * TONE:][:TONE]
    if (t[4:8], struct.unpack_from("<H", t, 22)[0]) != (bytes((44, 0, 24, 24)), 3):
        errs.append("psp_vab: the restored tone is %s sample %d" % (t[4:8].hex(), struct.unpack_from("<H", t, 22)[0]))
    refusals = (("two orphans", se, samples + samples[2:], cues),
                ("a declared tone that sounds", {0: se[0], 1: [(2,) + se[1][0][1:]]}, samples, cues),
                ("a chord two past the count", se, samples, cues[:4] + bytes((0, 0x81, 0x0A, 0x54))))
    for why, progs, smp, cue in refusals:
        ph, pb = _psp_synthetic(progs, smp)
        if psp_dropped_tone(psp_bank(ph, pb)[0], len(smp), cue) is not None:
            errs.append("psp_dropped_tone: restored a tone with %s" % why)
    return errs


def cmd_check(a):
    errs = check()
    for e in errs:
        print("  ", e)
    print("vag check: %d error(s)" % len(errs))
    return 1 if errs else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    s.add_parser("check", help="the PSP readers on synthetic banks (no game data)")
    p = s.add_parser("compare", help="every PC bank against the one built from the disc's EMI of the same name")
    p.add_argument("--disc", required=True)
    p.add_argument("--dat", required=True)
    p.add_argument("--names", nargs="*", help="DAT stems to compare (default all)")
    p.add_argument("--hardware", action="store_true", help="also measure the port's decode against the SPU's")
    a = ap.parse_args()
    sys.exit({"compare": cmd_compare, "check": cmd_check}[a.cmd](a) or 0)


if __name__ == "__main__":
    main()
