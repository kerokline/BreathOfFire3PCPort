#!/usr/bin/env python
"""Hold the game's FMV decoders (src/video, DIV-0205) against ffmpeg's decode.

    python tools/fmv_compare.py MOVIE.avi [--dump build/bof3x-fmvdump.exe] [--ffmpeg PATH] [--work DIR]

Runs bof3x-fmvdump on the file (every frame as rgb24, a Video 1 stream also
in its own depth, the PCM stream whole) and ffmpeg on the same file (-f
rawvideo -fps_mode passthrough, rgb24 for Cinepak, rgb555le for a 16-bit
Video 1 stream, the audio as s16le / u8 of the stream's channels and rate,
-c:a copy), then compares frame by frame: the count, the pixels that differ
at all, the largest channel difference, and the PSNR of each frame; the
audio byte for byte. An empty video chunk (logos.avi has 60 of 120) is a
repeat of the previous frame on our side and no frame at all from ffmpeg,
so each is checked as a repeat and the next non-empty chunk against
ffmpeg's next frame. Prints a summary and exits 1 on a frame count or audio
mismatch, an empty chunk that is not a repeat, or any pixel differing beyond
--tolerance (default 0: bit-exact).

The reference decode happens outside the repository, by the player's ffmpeg
(BOF3X_FFMPEG or the PATH), and nothing of the movie is written under the
tree: --work is a scratch directory (default a temporary one).
"""
import argparse
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile


def ffmpeg_path(given):
    for p in (given, os.environ.get("BOF3X_FFMPEG"), shutil.which("ffmpeg")):
        if p and os.path.exists(p):
            return p
    sys.exit("ffmpeg not found: --ffmpeg, BOF3X_FFMPEG or the PATH")


def read_header(avi):
    """The video stream's compression and bit depth, the audio format, and each video chunk's size, from the file."""
    data = open(avi, "rb").read()
    vid = aud = None
    sizes = []
    pos = 12
    def walk(off, end):
        nonlocal vid, aud
        while off + 8 <= end:
            fcc = data[off:off + 4]; size = struct.unpack_from("<I", data, off + 4)[0]; body = off + 8
            if fcc in (b"RIFF", b"LIST"):
                walk(body + 4, body + size)
            elif fcc[:2].isdigit() and fcc[2:] in (b"dc", b"db") and vid is not None:
                sizes.append(size)
            elif fcc == b"strf":
                if size >= 40 and data[body + 16:body + 20] in (b"cvid", b"CRAM", b"MSVC", b"msvc", b"cram") and vid is None:
                    vid = (data[body + 16:body + 20].decode(), struct.unpack_from("<H", data, body + 14)[0])
                elif size >= 16 and aud is None and vid is not None:
                    aud = struct.unpack_from("<HHIIHH", data, body)
            off = body + size + (size & 1)
    walk(pos, len(data))
    return vid, aud, sizes


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("avi")
    ap.add_argument("--dump", default=os.path.join("build", "bof3x-fmvdump.exe"))
    ap.add_argument("--ffmpeg")
    ap.add_argument("--work")
    ap.add_argument("--tolerance", type=int, default=0, help="largest channel difference allowed (8-bit units)")
    a = ap.parse_args()
    ff = ffmpeg_path(a.ffmpeg)
    vid, aud, sizes = read_header(a.avi)
    if vid is None:
        sys.exit("no Cinepak or Video 1 stream in " + a.avi)
    compression, bits = vid
    work = a.work or tempfile.mkdtemp(prefix="fmv_compare_")
    os.makedirs(work, exist_ok=True)
    ours = os.path.join(work, "ours.rgb"); ours_raw = os.path.join(work, "ours.raw"); ours_pcm = os.path.join(work, "ours.pcm")
    r = subprocess.run([a.dump, a.avi, ours, "--raw", ours_raw, "--pcm", ours_pcm], capture_output=True, text=True)
    print(r.stdout.strip())
    if r.returncode != 0:
        print(r.stderr.strip()); sys.exit("bof3x-fmvdump exit %d" % r.returncode)
    # ffmpeg: the same depth as ours where the stream has one
    own_depth = compression.lower() in ("cram", "msvc") and bits == 16
    pix = "rgb555le" if own_depth else "rgb24"
    ref = os.path.join(work, "ref." + pix)
    cmd = [ff, "-v", "error", "-y", "-i", a.avi, "-an", "-fps_mode", "passthrough", "-f", "rawvideo", "-pix_fmt", pix, ref]
    subprocess.run(cmd, check=True)
    ref_pcm = None
    if aud and aud[0] == 1:
        ref_pcm = os.path.join(work, "ref.pcm")
        subprocess.run([ff, "-v", "error", "-y", "-i", a.avi, "-vn", "-c:a", "copy", "-f", "s16le" if aud[5] == 16 else "u8", ref_pcm], check=True)
    # frame size: from ffprobe-free arithmetic - the dump's header line says w x h
    w, h = [int(x) for x in r.stdout.split(" x ")[0].split()[-1:] + [r.stdout.split(" x ")[1].split()[0]]]
    bpp = 2 if own_depth else 3
    mine = open(ours_raw if own_depth else ours, "rb").read()
    theirs = open(ref, "rb").read()
    fsz = w * h * bpp
    n_mine, n_ref = len(mine) // fsz, len(theirs) // fsz
    empties = [i for i, s in enumerate(sizes) if s == 0]
    print("frames: ours %d (%d empty chunks, repeats), ffmpeg %d, %d x %d %s" % (n_mine, len(empties), n_ref, w, h, pix))
    bad = 0
    worst = 0
    psnr_min = float("inf")
    exact = 0
    repeats_bad = 0
    j = 0   # ffmpeg's next frame
    for i in range(n_mine):
        m = mine[i * fsz:(i + 1) * fsz]
        if i < len(sizes) and sizes[i] == 0:
            if i == 0 or m != mine[(i - 1) * fsz:i * fsz]:
                repeats_bad += 1
                print("frame %d: an empty chunk that is not a repeat of frame %d" % (i, i - 1))
            continue
        if j >= n_ref:
            break
        t = theirs[j * fsz:(j + 1) * fsz]
        j += 1
        if m == t:
            exact += 1
            continue
        if own_depth:
            mv = struct.unpack("<%dH" % (w * h), m); tv = struct.unpack("<%dH" % (w * h), t)
            diffs = [max(abs(((x >> 10) & 31) - ((y >> 10) & 31)), abs(((x >> 5) & 31) - ((y >> 5) & 31)), abs((x & 31) - (y & 31))) * 8 for x, y in zip(mv, tv) if (x & 0x7FFF) != (y & 0x7FFF)]
            se = sum(d * d for d in diffs)
        else:
            diffs = [abs(x - y) for x, y in zip(m, t) if x != y]
            se = sum(d * d for d in diffs)
        if not diffs:
            exact += 1   # differs only in rgb555's unused bit 15
            continue
        n = len(diffs)
        mx = max(diffs)
        worst = max(worst, mx)
        mse = se / (w * h * 3)
        psnr = 10 * math.log10(255 * 255 / mse) if mse else float("inf")
        psnr_min = min(psnr_min, psnr)
        if mx > a.tolerance:
            bad += 1
        if bad <= 5 and mx > a.tolerance:
            print("frame %d: %d channel values differ, largest %d, PSNR %.1f dB" % (i, n, mx, psnr))
    print("exact frames %d of %d coded; frames over the tolerance %d; largest channel difference %d; lowest PSNR %s; "
          "repeats wrong %d" % (exact, n_mine - len(empties), bad, worst,
                                 "inf" if psnr_min == float("inf") else "%.1f dB" % psnr_min, repeats_bad))
    ok = n_mine - len(empties) == n_ref and bad == 0 and repeats_bad == 0
    if ref_pcm:
        mp = open(ours_pcm, "rb").read(); tp = open(ref_pcm, "rb").read()
        same = mp == tp
        print("audio: ours %d bytes, ffmpeg %d bytes, %s" % (len(mp), len(tp), "identical" if same else "DIFFERENT"))
        ok = ok and same
    print("RESULT " + ("PASS" if ok else "FAIL"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
