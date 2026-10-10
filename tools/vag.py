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
    return [(g[6][2], bank(g[6][3], g[8][3] if 8 in g else b"", g[7][3], cache)) for g in groups(sections)]


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
    return key, g[6][0], bank(g[6][3], g[8][3] if 8 in g else b"", g[7][3], _CACHE)


def importer_source(src, f, slot, ch):
    """tools/importer.py build's wave-from-vag: a recipe `bank` chunk from a
    PSX disc source, as importer's (build, (build, how, EMI, section),
    payload), or None - no such bank, or one that is not the recipe's (the
    PAL discs' eight swapped area banks, region-diff.md 8.4)."""
    if ch["class"] != "bank" or not src.id.startswith("psx-") or not hasattr(src, "build"):
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
