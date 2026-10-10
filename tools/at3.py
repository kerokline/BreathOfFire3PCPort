#!/usr/bin/env python
"""The PSP's sound effects, voice and jingles, decoded by the player's own ffmpeg.

docs/psp-only-build.md section 2.1.1. Where the PSX discs carry the port's
`SND/` clips as XA (tools/xa.py), the PSP discs carry each clip as its own
ATRAC3plus file under `PSP_GAME/USRDIR/<JPN|USA>/SCE_XA/`: `MAGIC/PLnn/NNN_KK.AT3`
the 875 numbered effects, `VOICE/NA01..05.AT3` the five `VOICE00..04`,
`MUSIC/NAME.AT3` the 11 jingles. RIFF `WAVE_FORMAT_EXTENSIBLE`, 44.1 kHz, the
`fact` chunk's two words the sample count and the encoder delay (2048).

No ATRAC3plus decoder is carried (LICENSING.md section 4: every known one is
LGPL or Sony's): the player's own ffmpeg decodes, at import time (the owner,
2026-10-10). Without one the effects are reported missing. The output can never
equal the PC's files byte for byte, so nothing here is hashed against them:
lengths are the PC's, and the import's provenance names the ffmpeg.

The PSP cut each clip on a 0.25 s grid of its XA stream, so a clip sits at an
offset inside its file: 785 of the 880 at the start sector's time mod 0.25 s
plus 368 samples (44.1 kHz) past the encoder delay, 94 a grid step earlier, one
292 samples later. No rule tells the step, so `recipe` measures every offset
against a PSX disc and `recipes/psp.snd.toml` holds them, numbers only.

    python tools/at3.py recipe --disc JP.cue --psp PSPJP.iso [--ffmpeg PATH] [--out recipes/psp.snd.toml]
    python tools/at3.py build  --psp PSP.iso --out CACHE/base/snd [--ffmpeg PATH]

`recipe` needs numpy and both discs (the owner's machine); `build` needs only
the PSP disc and ffmpeg. Decoded audio is game data: scratch or a cache only
(CLAUDE.md rule 1).
"""
import argparse
import array
import collections
import datetime
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import psx_disc  # noqa: E402
import xa  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RECIPE = os.path.join(ROOT, "recipes", "psp.snd.toml")
SND_RECIPE = xa.RECIPE
AT3_RATE, WAVE_RATE = 44100, xa.WAVE_RATE
STEREO_SECTOR = 2016          # samples a channel in a stereo 4-bit 37.8 kHz XA sector
JINGLE_KIND = 0


def find_ffmpeg(explicit=None):
    """The player's ffmpeg: `explicit`, else BOF3X_FFMPEG, else the PATH's; or None."""
    for p in (explicit, os.environ.get("BOF3X_FFMPEG")):
        if p:
            if not os.path.isfile(p):
                raise SystemExit("ffmpeg %s: no such file" % p)
            return p
    return shutil.which("ffmpeg")


def ffmpeg_version(ffmpeg):
    out = subprocess.run([ffmpeg, "-hide_banner", "-version"], capture_output=True, text=True).stdout
    return out.split("\n", 1)[0].strip()


def info(b):
    """(channels, rate, sample count, encoder delay) of an AT3 file."""
    if b[:4] != b"RIFF" or b[8:12] != b"WAVE":
        raise ValueError("not a RIFF WAVE")
    pos, ch = 12, {}
    while pos + 8 <= len(b):
        cid, n = b[pos:pos + 4], struct.unpack_from("<I", b, pos + 4)[0]
        ch[cid] = b[pos + 8:pos + 8 + n]
        pos += 8 + n + (n & 1)
    channels, rate = struct.unpack_from("<HI", ch[b"fmt "], 2)
    count, delay = struct.unpack_from("<II", ch[b"fact"])
    return channels, rate, count, delay


def decode(ffmpeg, b, rate, channels):
    """An AT3 file to 16-bit PCM at `rate` with `channels`, by ffmpeg (stdin to stdout)."""
    r = subprocess.run([ffmpeg, "-v", "error", "-f", "wav", "-i", "pipe:0", "-ac", str(channels),
                        "-ar", str(rate), "-f", "s16le", "pipe:1"], input=b, capture_output=True)
    if r.returncode or not r.stdout:
        raise SystemExit("ffmpeg could not decode an AT3 file: %s" % r.stderr.decode(errors="replace").strip()[-300:])
    return array.array("h", r.stdout)


def delay_trimmed(b, decoded, rate, channels):
    """Whether this ffmpeg dropped the encoder delay, by the decode's length:
    untrimmed it is whole 2048-sample frames, at least count + delay."""
    _, src_rate, count, delay = info(b)
    frames = len(decoded) // channels * src_rate / rate
    return frames < count + delay - 64, delay * rate // src_rate


def onset_base(b, decoded, rate, channels):
    """Where the delay-trimmed stream starts in `decoded`, in samples a channel."""
    trimmed, delay = delay_trimmed(b, decoded, rate, channels)
    return 0 if trimmed else delay


def psp_files(disc):
    """{path under SCE_XA/: the disc's full path} for every AT3 of a PSP disc."""
    out = {}
    for p in disc.files:
        if p.upper().endswith(".AT3") and "/SCE_XA/" in p.upper():
            out[p[p.upper().index("/SCE_XA/") + 8:].upper()] = p
    return out


def at3_name(row, files):
    """The AT3 a recipe row's clip is on the PSP: by kind and name."""
    if row["kind"] == 2:
        rel = "VOICE/NA%02d.AT3" % (row["id"] + 1)
    elif row["kind"] == JINGLE_KIND:
        rel = "MUSIC/%s.AT3" % row["name"].upper()
    else:
        rel = next((k for k in files if k.startswith("MAGIC/") and k.endswith("/%s.AT3" % row["name"].upper())), None)
    return rel if rel in files else None


def out_length(row):
    """Samples a channel the PC's file holds: the WAV's data, or for a jingle
    the XA clip's length at 44.1 kHz."""
    if row["kind"] == JINGLE_KIND:
        return row["sectors"] * STEREO_SECTOR * AT3_RATE // xa.RATE
    return (row["size"] - 44) // 2


def place(decoded, start, n, channels):
    """`n` frames of `decoded` from frame `start` (negative: zeros first), zero-padded."""
    out = array.array("h", bytes(2 * n * channels))
    lo, hi = max(0, -start), min(n, len(decoded) // channels - start)
    if hi > lo:
        out[lo * channels:hi * channels] = decoded[(start + lo) * channels:(start + hi) * channels]
    return out


# ---------------------------------------------------------------- recipe

def _xcorr_lag(a, ref):
    import numpy as np
    n = 1 << (len(a) + len(ref)).bit_length()
    c = np.fft.irfft(np.fft.rfft(a, n) * np.conj(np.fft.rfft(ref, n)), n)
    c = np.concatenate([c[-(len(ref) - 1):], c[:len(a)]])
    return int(np.argmax(c)) - (len(ref) - 1)


def _measure(a, ref, lag):
    import numpy as np
    if lag >= 0:
        A, B = a[lag:lag + len(ref)], ref[:max(0, len(a) - lag)]
    else:
        B, A = ref[-lag:], a[:len(ref) + lag]
    n = min(len(A), len(B))
    A, B = A[:n], B[:n]
    g = np.dot(A, B) / max(np.dot(A, A), 1e-9)
    e = B - g * A
    return float(np.corrcoef(A, B)[0, 1]), float(10 * np.log10(np.dot(B, B) / max(np.dot(e, e), 1e-9)))


def cmd_recipe(a):
    import numpy as np
    ffmpeg = find_ffmpeg(a.ffmpeg)
    if not ffmpeg:
        raise SystemExit("no ffmpeg: --ffmpeg, BOF3X_FFMPEG or the PATH")
    with open(SND_RECIPE, "rb") as fh:
        rows = tomllib.load(fh)["file"]
    cl = xa.disc_clips(a.disc)
    psp = psx_disc.Disc(a.psp)
    files = psp_files(psp)
    out, done, st = [], set(), collections.Counter()
    for r in rows:
        if r["kind"] not in (0, 1, 2) or r["name"] in done or "size" not in r:
            continue
        done.add(r["name"])
        rel = at3_name(r, files)
        if rel is None:
            st["no AT3"] += 1
            continue
        b = psp.read(files[rel])
        _, _, chans, _ = cl[r["stream"]]
        raws = chans[r["channel"]][r["start"]:r["start"] + r["sectors"]]
        if r["kind"] == JINGLE_KIND:
            rate = AT3_RATE
            L, R = xa.decode(raws)
            x = (np.asarray(L, float) + np.asarray(R, float)) / 2
            t = np.arange(int(len(x) * rate / xa.RATE)) * (xa.RATE / rate)
            ref = np.interp(t, np.arange(len(x)), x)
        else:
            rate = WAVE_RATE
            ref = np.asarray(xa.resample(xa.decode(raws)[0]), float)
        dec = decode(ffmpeg, b, rate, 1)
        lag = _xcorr_lag(np.asarray(dec, float), ref)
        corr, snr = _measure(np.asarray(dec, float), ref, lag)
        onset = lag - onset_base(b, dec, rate, 1)
        st["clips"] += 1
        st["corr < 0.98"] += corr < 0.98
        out.append({"name": r["name"], "kind": r["kind"], "at3": rel, "at3_sha256": hashlib.sha256(b).hexdigest(),
                    "rate": rate, "onset": onset, "length": out_length(r), "corr": round(corr, 4), "snr": round(snr, 1)})
        print("%-9s %-24s onset %6d  corr %.4f  snr %5.1f dB" % (r["name"], rel, onset, corr, snr), flush=True)
    lines = ["# recipes/psp.snd.toml - GENERATED by `tools/at3.py recipe`; do not edit by hand.",
             "# The PC port's SND/ clips from a PSP disc's ATRAC3plus files, decoded by the player's",
             "# ffmpeg (docs/psp-only-build.md 2.1.1): `at3` under SCE_XA/ and its hash; `onset`, where",
             "# the clip's first sample sits in the decode past the encoder delay, at `rate` (negative:",
             "# the PSP's file starts inside the clip); `length` the PC file's samples a channel. `corr`",
             "# and `snr` against the PSX disc's XA as the port converts it (its WAV, whose resampler",
             "# aliases: the low rows are that), a jingle against the XA itself. Names, numbers and",
             "# hashes only (rule 1).", "",
             "[meta]", "generated = \"%s\"" % datetime.date.today().isoformat(),
             "ffmpeg = %s" % json.dumps(ffmpeg_version(ffmpeg)), ""]
    for r in out:
        lines.append("[[file]]")
        lines += ["%s = %s" % (k, json.dumps(v)) for k, v in r.items()]
        lines.append("")
    with open(a.out or RECIPE, "w", newline="\n", encoding="utf-8") as fh:
        fh.write("\n".join(lines))
    print("%s: %s" % (a.out or RECIPE, dict(st)))


def check(recipe=None):
    """recipes/psp.snd.toml against recipes/pc-zh.snd.toml alone (CI, no game
    data): one row for every distinct clip of kinds 0..2 the PC ships, each its
    own AT3 under the expected folder, its length the PC file's, its rate the
    output's, a hash of the right shape. Returns the errors."""
    with open(recipe or RECIPE, "rb") as fh:
        rows = tomllib.load(fh)["file"]
    with open(SND_RECIPE, "rb") as fh:
        pc = {r["name"]: r for r in tomllib.load(fh)["file"] if r["kind"] in (0, 1, 2) and "size" in r}
    errs = []
    if sorted(r["name"] for r in rows) != sorted(pc):
        errs.append("psp.snd.toml: %d clips, pc-zh.snd.toml %d" % (len(rows), len(pc)))
    if len({r["at3"] for r in rows}) != len(rows):
        errs.append("psp.snd.toml: two clips share an AT3")
    for r in rows:
        p = pc.get(r["name"])
        if p is None:
            continue
        folder = {0: "MUSIC/", 1: "MAGIC/", 2: "VOICE/"}[p["kind"]]
        rate = AT3_RATE if p["kind"] == JINGLE_KIND else WAVE_RATE
        if (r["kind"] != p["kind"] or not r["at3"].startswith(folder) or r["rate"] != rate
                or r["length"] != out_length(p) or len(r["at3_sha256"]) != 64):
            errs.append("psp.snd.toml: %s" % r["name"])
    return errs


# ---------------------------------------------------------------- build

def build(psp_path, out, ffmpeg, recipe=None):
    """Every clip of the recipe from a PSP disc into `out`/NAME.DAT: the effects
    and voice as the port's WAVs (mono, 22.05 kHz), the jingles as MP3s (44.1 kHz
    stereo, 128 kbit/s, the PC's form) when this ffmpeg can encode one. Returns
    ({name: sha256 of what was written}, [why a clip was not written])."""
    with open(recipe or RECIPE, "rb") as fh:
        rows = tomllib.load(fh)["file"]
    psp = psx_disc.Disc(psp_path)
    files = psp_files(psp)
    os.makedirs(out, exist_ok=True)
    written, skipped = {}, []
    for r in rows:
        if r["at3"] not in files:
            skipped.append("%s: no %s on this disc" % (r["name"], r["at3"]))
            continue
        b = psp.read(files[r["at3"]])
        if hashlib.sha256(b).hexdigest() != r["at3_sha256"]:
            raise SystemExit("%s: %s is not the recipe's file" % (r["name"], r["at3"]))
        channels = 2 if r["kind"] == JINGLE_KIND else 1
        dec = decode(ffmpeg, b, r["rate"], channels)
        pcm = place(dec, onset_base(b, dec, r["rate"], channels) + r["onset"], r["length"], channels)
        if not any(pcm):
            raise SystemExit("%s: the decode is silent" % r["name"])
        if r["kind"] == JINGLE_KIND:
            enc = subprocess.run([ffmpeg, "-v", "error", "-f", "s16le", "-ar", str(r["rate"]), "-ac", "2",
                                  "-i", "pipe:0", "-b:a", "128k", "-f", "mp3", "pipe:1"],
                                 input=pcm.tobytes(), capture_output=True)
            if enc.returncode or not enc.stdout:
                skipped.append("%s: this ffmpeg has no MP3 encoder" % r["name"])
                continue
            body = enc.stdout
        else:
            body = xa.wave(list(pcm))
        with open(os.path.join(out, r["name"] + ".DAT"), "wb") as fh:
            fh.write(body)
        written[r["name"]] = hashlib.sha256(body).hexdigest()
    return written, skipped


def importer_snd(sources, out):
    """tools/importer.py build's psp-at3: `base/snd/` from the first PSP disc
    among the player's sources, by the player's ffmpeg. Returns (build id, how,
    {name: sha256}, [skipped]) or None; with no ffmpeg, (build id, None, {},
    [why])."""
    disc = next((s for s in sources if s.id.startswith("psp-") and hasattr(s, "build")), None)
    if disc is None:
        return None
    ffmpeg = find_ffmpeg()
    if not ffmpeg:
        return disc.id, None, {}, ["no ffmpeg on the PATH or in BOF3X_FFMPEG: the PSP's effects, voice and "
                                   "jingles are ATRAC3plus, which only the player's ffmpeg decodes"]
    written, skipped = build(disc.path, os.path.join(out, "base", "snd"), ffmpeg)
    return disc.id, ffmpeg_version(ffmpeg), written, skipped


def cmd_build(a):
    ffmpeg = find_ffmpeg(a.ffmpeg)
    if not ffmpeg:
        raise SystemExit("no ffmpeg: --ffmpeg, BOF3X_FFMPEG or the PATH")
    written, skipped = build(a.psp, a.out, ffmpeg)
    print("%d files written by %s" % (len(written), ffmpeg_version(ffmpeg)))
    for s in skipped:
        print("  not written: %s" % s)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    p = s.add_parser("recipe", help="measure every clip's onset in its AT3 against a PSX disc")
    p.add_argument("--disc", required=True, help="a PSX disc (the XA clips)")
    p.add_argument("--psp", required=True, help="a PSP disc")
    p.add_argument("--ffmpeg")
    p.add_argument("--out")
    p = s.add_parser("build", help="every clip from a PSP disc into a directory")
    p.add_argument("--psp", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--ffmpeg")
    a = ap.parse_args()
    {"recipe": cmd_recipe, "build": cmd_build}[a.cmd](a)


if __name__ == "__main__":
    main()
