#!/usr/bin/env python
"""CD-XA ADPCM audio: reading a disc's XA stream and decoding it.

docs/sound-import.md. The format, as the public descriptions give it. An XA
audio sector is a Mode 2 Form 2 sector: 12 sync bytes, 4 header bytes, an
8-byte subheader (file, channel, submode, coding - written twice), then 18
sound groups of 128 bytes. Submode bit 2 marks audio, bit 7 the end of the
file; the coding byte's bit 0 is stereo, bit 2 half rate (18.9 kHz, else
37.8 kHz), bit 4 eight-bit samples (else four).

A four-bit sound group is 16 parameter bytes and 28 four-byte words. Units
0..7 take their parameter from bytes 4..11 (bytes 0..3 and 12..15 are
copies): shift in the low nibble, filter (0..3) in the high one. Sample j of
unit u is word j's byte u // 2, its low nibble for an even unit and its
high nibble for an odd one. A mono group plays the units one after another,
8 x 28 samples; a stereo group alternates them, even units left. Each sample
is the nibble put in the top of a 16-bit word, shifted right by `shift`,
plus (s1 * k0 + s2 * k1) / 64 from the side's last two samples (floored),
clamped to 16 bits; the filter pairs are the SPU's first four.

The port's `SND/` is three of the disc's XA files cut into clips by the
boot EXE's stream table (docs/sound-import.md section 3): per channel, a list
of u16 start sectors, the last flagged 0x8000 and followed by the channel's
length; stream id n of a kind is the n-th clip counting channel by channel.
`BOF3.exe`'s table 0x6653B0 names them, kind by kind (Sound_LoadStream).

    python tools/xa.py ls     DISC BIN/BMAG_XA/MAGIC00.STR
    python tools/xa.py recipe --disc JP.cue --exe BOF3.exe --snd SND [--out recipes/pc-zh.snd.toml]
    python tools/xa.py build  --disc DISC --out CACHE/base/snd   (every clip, checked against the recipe)

Decoded audio is game data: scratch or analysis/ only (CLAUDE.md rule 1).
"""
import argparse
import array
import collections
import datetime
import hashlib
import json
import os
import re
import struct
import sys
import tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import psx_disc  # noqa: E402

FILTERS = ((0, 0), (60, 0), (115, -52), (98, -55))
RAW = psx_disc.RAW
GROUPS, GROUP = 18, 128
RATE = 37800
WAVE_RATE = 22050
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RECIPE = os.path.join(ROOT, "recipes", "pc-zh.snd.toml")
# Sound_LoadStream's kinds (id >> 12), each a table of names at BOF3.exe
# 0x6653B0 + 4 * kind, and the disc file whose clips they are: kind 0 the
# eleven MP3 jingles, kind 1 the numbered clips, kind 2 the five VOICE files.
STREAMS = ((0, "BIN/SCE_XA/S_XA00.STR"), (1, "BIN/BMAG_XA/MAGIC00.STR"), (2, "BIN/SCE_XA/VOICE.STR"))
PC_NAME_TABLES = 0x6653B0


def sectors(disc, name):
    """[(subheader bytes, raw sector)] for every sector of a file's extent,
    read from the raw image (the ISO size counts 2,048-byte sectors)."""
    if not disc.raw:
        raise SystemExit("XA needs a raw (2,352-byte sector) image")
    lba, size = disc.files[name]
    out = []
    for i in range(size // psx_disc.USER):
        disc.f.seek((lba + i) * RAW)
        raw = disc.f.read(RAW)
        out.append((raw[16:20], raw))
    return out


def is_audio(sub):
    return bool(sub[2] & 0x04)


def channels(secs):
    """channel -> [raw sector, ...] of the audio sectors, in disc order."""
    out = collections.defaultdict(list)
    for sub, raw in secs:
        if is_audio(sub):
            out[sub[1]].append(raw)
    return out


def decode(raws, state=None):
    """Four-bit XA sectors to 16-bit samples, one list per channel side
    (mono: [samples]; stereo: [left, right]). `state` carries each side's
    history [s1, s2] from one call to the next; a fresh one starts at zero.
    The prediction is floored (an arithmetic shift), as the SPU's is: the
    port's clips match that, not the +32 rounding some descriptions give."""
    if not raws:
        return [[]]
    stereo = raws[0][19] & 1
    sides = 2 if stereo else 1
    st = state if state is not None else [[0, 0] for _ in range(sides)]
    out = [[] for _ in range(sides)]
    for raw in raws:
        if raw[19] & 0x30 or (raw[19] & 1) != stereo:
            raise ValueError("coding 0x%02X: only four-bit, one layout per stream" % raw[19])
        for g in range(GROUPS):
            p = 24 + g * GROUP
            for u in range(8):
                side = u & 1 if stereo else 0
                s1, s2 = st[side]
                param = raw[p + 4 + u]
                shift = param & 0x0F
                k0, k1 = FILTERS[(param >> 4) & 3]
                hi = u & 1
                at = p + 16 + (u >> 1)
                dst = out[side]
                for j in range(28):
                    b = raw[at + 4 * j]
                    n = (b >> 4) if hi else (b & 0x0F)
                    if n & 8:
                        n -= 16
                    v = ((n << 12) >> shift) + ((s1 * k0 + s2 * k1) >> 6)
                    v = -32768 if v < -32768 else 32767 if v > 32767 else v
                    dst.append(v)
                    s1, s2 = v, s1
                st[side] = [s1, s2]
    return out


def stream_lists(exe, counts):
    """(file offset, [(channel, start, sectors)]) of the boot EXE's clip
    lists for an XA file whose channels hold `counts` audio sectors: the
    first place where, channel by channel from 0, u16 starts rise from 0 to
    a 0x8000-flagged one followed by a length equal to the channel's - for
    every channel, or for the first four or more (S_XA00 lists 4 of its 8).
    None if there is no such."""
    n = len(counts)
    for off in range(0, len(exe) - 4 * n, 2):
        if exe[off] or exe[off + 1] & 0x7F:
            continue                          # every list opens at sector 0
        p, clips, done = off, [], 0
        for ch in range(n):
            starts = []
            while p + 4 <= len(exe):
                v, = struct.unpack_from("<H", exe, p)
                p += 2
                if starts and (v & 0x7FFF) <= starts[-1]:
                    break
                starts.append(v & 0x7FFF)
                if v & 0x8000:
                    break
            total, = struct.unpack_from("<H", exe, p)
            p += 2
            if total != counts[ch] or starts[0] != 0 or total <= starts[-1]:
                break
            clips += [(ch, a, b - a) for a, b in zip(starts, starts[1:] + [total])]
            done = ch + 1
        else:
            return off, clips
        if clips and done >= min(n, 4):
            return off, clips
    return None


def resample(x):
    """The port's 37.8 to 22.05 kHz conversion, as measured on its 880 clips
    (docs/sound-import.md section 3.3): output 0 repeats input 0; output k
    is the straight line from input i to i + 1 at t = (k - 1) * 12 / 7,
    the position computed in double and stored as a float (24-bit
    mantissa), i its integer part; the value truncated toward zero."""
    n = len(x)
    count = n * 7 // 12
    pos = array.array("f", (k * (RATE / WAVE_RATE) for k in range(count)))
    x = list(x) + [0, 0]
    out = [x[0]]
    for t in pos:
        i = int(t)
        a = x[i]
        out.append(int(a + (x[i + 1] - a) * (t - i)))
    return out


def wave(pcm, rate=WAVE_RATE):
    data = struct.pack("<%dh" % len(pcm), *pcm)
    return (b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " +
            struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) +
            b"data" + struct.pack("<I", len(data)) + data)


def clip(raws):
    """A clip's sectors to the WAVE `SND/` holds for it: decoded from a
    fresh history, resampled as the port did."""
    mono, = decode(raws)
    return wave(resample(mono))


def disc_clips(disc_path):
    """{stream file: (boot EXE offset, [(channel, start, sectors)], {channel: [raw sectors]})}."""
    disc = psx_disc.Disc(disc_path)
    boot = next(k for k in disc.files if re.match(r"S[LC][PUE][SM]_\d{3}\.\d{2}$", k))
    exe = disc.read(boot)
    out = {}
    for _, name in STREAMS:
        chans = channels(sectors(disc, name))
        counts = [len(chans[c]) for c in range(len(chans))]
        found = stream_lists(exe, counts)
        if not found:
            raise SystemExit("%s: no clip list in %s matches its %d channels" % (name, boot, len(counts)))
        out[name] = (found[0], found[1], chans, boot)
    return out


def pc_names(exe_path, counts):
    """{kind: [name, ...]} from BOF3.exe's stream name tables, `counts[kind]` each."""
    import loc_build
    game = os.path.dirname(os.path.abspath(exe_path))
    out = {}
    for kind, n in counts.items():
        table, = struct.unpack("<I", loc_build.exe_bytes(game, PC_NAME_TABLES + 4 * kind, 4))
        names = []
        for i in range(n):
            ptr, = struct.unpack("<I", loc_build.exe_bytes(game, table + 4 * i, 4))
            names.append(loc_build.exe_bytes(game, ptr, 16).split(b"\0")[0].decode("ascii"))
        out[kind] = names
    return out


def sha(b):
    return hashlib.sha256(b).hexdigest()


def cmd_recipe(a):
    """recipes/pc-zh.snd.toml: every file of the PC's SND/ with the clip the
    disc's table gives it, the PC file's hash and size, and for the WAVs the
    hash of this tool's output and how far it is from the PC's."""
    cl = disc_clips(a.disc)
    names = pc_names(a.exe, {k: len(cl[f][1]) for k, f in STREAMS})
    files = {f.upper(): f for f in os.listdir(a.snd)}
    rows, seen, st = [], set(), collections.Counter()
    for kind, f in STREAMS:
        off, clips_, chans, boot = cl[f]
        for idx, ((ch, start, n), name) in enumerate(zip(clips_, names[kind])):
            pc = files.get(name.upper() + ".DAT")
            body = open(os.path.join(a.snd, pc), "rb").read() if pc else None
            row = {"name": name, "kind": kind, "id": idx, "stream": f, "channel": ch, "start": start, "sectors": n}
            if body is not None:
                row.update(size=len(body), sha256=sha(body))
            if body is not None and body[:4] == b"RIFF":
                ours = clip(chans[ch][start:start + n])
                if ours != body:
                    row["wave"] = sha(ours)
                if len(ours) == len(body):
                    d = [abs(p - q) for p, q in zip(struct.unpack_from("<%dh" % ((len(ours) - 44) // 2), ours, 44),
                                                     struct.unpack_from("<%dh" % ((len(body) - 44) // 2), body, 44))]
                    differ = sum(1 for x in d if x)
                    if differ:
                        row["max_diff"], row["differ"] = max(d), differ
                    st["same" if not differ else "differ"] += name not in seen
                    st["samples"] += len(d) if name not in seen else 0
                    st["samples differ"] += differ if name not in seen else 0
                    st["max"] = max(st["max"], max(d))
                else:
                    st["length differs"] += 1
                row["how"] = "wave-from-xa"
            elif body is not None:
                row["how"] = "pc"
                row["why"] = "an MP3 of the stereo clip (44.1 kHz, 128 kbit/s): no encoder here"
            else:
                row["how"] = "none"
            seen.add(name)
            rows.append(row)
        print("%s: table at %s file 0x%X, %d clips" % (f, boot, off, len(clips_)))
    listed = {r["name"].upper() + ".DAT" for r in rows}
    for k in sorted(files):
        if k not in listed:
            body = open(os.path.join(a.snd, files[k]), "rb").read()
            stem = files[k][:-4] if k.endswith(".DAT") else files[k]
            why = ("a list of SND/'s file names, CR LF, no stream reads it" if body.endswith(b".DAT\r\n")
                   else "no stream id names it, and no XA clip is it")
            rows.append({"name": stem, "kind": -1, "size": len(body), "sha256": sha(body), "how": "pc", "why": why})
    lines = ["# recipes/pc-zh.snd.toml - GENERATED by `tools/xa.py recipe`; do not edit by hand.",
             "# The PC port's SND/ files: each is clip `id` of Sound_LoadStream kind `kind`, the",
             "# `sectors` sectors of `stream`'s channel `channel` from its sector `start`, by the",
             "# boot EXE's clip lists (docs/sound-import.md section 3). `sha256` is the PC file's;",
             "# `how` wave-from-xa is tools/xa.py's conversion, which reproduces it - where it would",
             "# not, `wave` would hold its own hash and `max_diff` / `differ` the distance. Names,",
             "# numbers and hashes only (CLAUDE.md rule 1).", "",
             "[meta]", "target = \"pc-zh\"", "generated = \"%s\"" % datetime.date.today().isoformat(), ""]
    for r in rows:
        lines.append("[[file]]")
        lines += ["%s = %s" % (k, json.dumps(v)) for k, v in r.items()]
        lines.append("")
    out = a.out or RECIPE
    with open(out, "w", newline="\n", encoding="utf-8") as fh:
        fh.write("\n".join(lines))
    print("%s: %d rows; WAVs %d identical, %d differ (%d of %d samples, max |d| %d), %d of another length"
          % (out, len(rows), st["same"], st["differ"], st["samples differ"], st["samples"], st["max"],
             st["length differs"]))


def build(disc_path, out, recipe=None):
    """Every recipe file whose `how` is wave-from-xa, from a disc, into
    `out`/NAME.DAT; each output's hash must be the recipe's (`wave` where
    it has one, else the PC file's `sha256`). Returns the number written."""
    with open(recipe or RECIPE, "rb") as fh:
        rec = tomllib.load(fh)
    cl = disc_clips(disc_path)
    os.makedirs(out, exist_ok=True)
    done = set()
    for r in rec["file"]:
        if r["how"] != "wave-from-xa" or r["name"] in done:
            continue           # five names are two stream ids each, the same clip
        _, _, chans, _ = cl[r["stream"]]
        body = clip(chans[r["channel"]][r["start"]:r["start"] + r["sectors"]])
        if sha(body) != r.get("wave", r["sha256"]):
            raise SystemExit("%s: wave-from-xa output differs from the recipe's hash" % r["name"])
        with open(os.path.join(out, r["name"] + ".DAT"), "wb") as fh:
            fh.write(body)
        done.add(r["name"])
    return len(done)


def cmd_build(a):
    print("%d files written to %s" % (build(a.disc, a.out, a.recipe), a.out))


def cmd_ls(a):
    disc = psx_disc.Disc(a.disc)
    secs = sectors(disc, a.name)
    kinds = collections.Counter((sub[1], sub[2], sub[3]) for sub, _ in secs)
    print("%s: %d sectors" % (a.name, len(secs)))
    for (ch, mode, coding), n in sorted(kinds.items()):
        print("  channel %2d submode 0x%02X coding 0x%02X  %5d" % (ch, mode, coding, n))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    p = s.add_parser("ls", help="the sectors of an XA file by channel, submode and coding")
    p.add_argument("disc")
    p.add_argument("name")
    p = s.add_parser("recipe", help="generate the SND recipe from a disc, BOF3.exe and the PC's SND/")
    p.add_argument("--disc", required=True)
    p.add_argument("--exe", required=True)
    p.add_argument("--snd", required=True)
    p.add_argument("--out")
    p = s.add_parser("build", help="write SND/ from a disc, checked against the recipe")
    p.add_argument("--disc", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--recipe")
    a = ap.parse_args()
    sys.exit({"ls": cmd_ls, "recipe": cmd_recipe, "build": cmd_build}[a.cmd](a) or 0)


if __name__ == "__main__":
    main()
