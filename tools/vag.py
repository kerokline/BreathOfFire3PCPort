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

    python tools/vag.py compare --disc JP.cue --dat DAT

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
    """The SPU's integer decode: 16-bit PCM samples (a list), clamped."""
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


# psp-unwrap for banks (docs/psp-only-build.md section 2.1). The PSP keeps the
# EMI's sound bank as three sections of the same types, the VAB re-containered:
# type 6 a `PPHD` header, type 7 a `pBVC` body, type 8 the cue table unchanged.
# `PPHD` +0x10, +0x14, +0x18 are the offsets of its `PPPG`, `PPTN` and `PPVA`
# blocks. `PPPG` +0x20: 128 program offsets (-1, none), a program a tone count,
# three -1 words, then that many indices into `PPTN`. `PPTN`: the record size at
# +8, the count less one at +0x14, records from +0x18; a record's words 2 (the VAG
# less one), 6 and 7 (lowest and highest note), 14 and 15 (centre note, fine tune).
# `PPVA` +0x14 the count less one, from +0x20 one (offset in `pBVC`, rate, size, -1)
# per sample. Read against the PSX-JP VH of every bank; what `bank` needs of a VH
# rebuilds 885 of the PC's 901 chunks. The other 16 are one bank whose program 1
# a chord plays two tones of: the PSP declares one and dropped the second slot.
PSP_HEAD, PSP_BODY = b"PPHD", b"pBVC"


def psp_vab(ph, pbvc):
    """A VAB header and body, as far as `bank` reads them, from the PSP's
    `PPHD` and `pBVC` sections."""
    if ph[:4] != PSP_HEAD or pbvc[:4] != PSP_BODY:
        raise ValueError("not a PSP sound bank")
    pg, tn, va = struct.unpack_from("<3I", ph, 0x10)
    if (ph[pg:pg + 4], ph[tn:tn + 4], ph[va:va + 4]) != (b"PPPG", b"PPTN", b"PPVA"):
        raise ValueError("PPHD: block tags not where its header says")
    programs = {}
    for p in range(VAB_PROGRAMS):
        o, = struct.unpack_from("<i", ph, pg + 0x20 + 4 * p)
        if o != -1:
            n, = struct.unpack_from("<I", ph, o)
            programs[p] = struct.unpack_from("<%dI" % n, ph, o + 16)
    size, = struct.unpack_from("<I", ph, tn + 8)
    ntones = struct.unpack_from("<i", ph, tn + 0x14)[0] + 1
    nvag = struct.unpack_from("<i", ph, va + 0x14)[0] + 1
    count = max(programs) + 1 if programs else 0
    tones = 32 + VAB_PROGRAMS * 16
    vh = bytearray(tones + count * 16 * TONE + 512)
    vh[:4] = b"pBAV"
    struct.pack_into("<HH", vh, 0x12, count, 0)
    struct.pack_into("<H", vh, 0x16, nvag)
    for p, ts in programs.items():
        if len(ts) > 16:
            raise ValueError("PPHD: program %d has %d tones, a VAB program holds 16" % (p, len(ts)))
        for j, t in enumerate(ts):
            if t >= ntones:
                raise ValueError("PPHD: program %d tone %d past the tone table" % (p, t))
            r = tn + 0x18 + size * t
            vag, = struct.unpack_from("<i", ph, r + 8)
            low, high = struct.unpack_from("<ii", ph, r + 0x18)
            centre, fine = struct.unpack_from("<ii", ph, r + 0x38)
            o = tones + (p * 16 + j) * TONE
            vh[o + 4:o + 8] = bytes((centre & 0xFF, fine & 0xFF, low & 0xFF, high & 0xFF))
            struct.pack_into("<H", vh, o + 22, vag + 1)
    sizes, vb = tones + count * 16 * TONE, []
    for i in range(nvag):
        off, _, n = struct.unpack_from("<3I", ph, va + 0x20 + 16 * i)
        if n % 8 or off + n > len(pbvc):
            raise ValueError("PPVA: sample %d (offset 0x%X, size %d) not in pBVC" % (i, off, n))
        struct.pack_into("<H", vh, sizes + 2 * (i + 1), n // 8)
        vb.append(pbvc[off:off + n])
    return bytes(vh), b"".join(vb)


def _vab(g):
    """(header, body) of a sound-bank group, unwrapping a PSP disc's."""
    if g[6][3][:4] == PSP_HEAD:
        return psp_vab(g[6][3], g[7][3])
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
    PAL discs' eight swapped area banks, region-diff.md 8.4; the PSP's one
    bank with a dropped tone, 16 chunks, psp-only-build.md 2.1)."""
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


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    p = s.add_parser("compare", help="every PC bank against the one built from the disc's EMI of the same name")
    p.add_argument("--disc", required=True)
    p.add_argument("--dat", required=True)
    p.add_argument("--names", nargs="*", help="DAT stems to compare (default all)")
    p.add_argument("--hardware", action="store_true", help="also measure the port's decode against the SPU's")
    a = ap.parse_args()
    sys.exit({"compare": cmd_compare}[a.cmd](a) or 0)


if __name__ == "__main__":
    main()
