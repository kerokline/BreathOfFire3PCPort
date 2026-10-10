#!/usr/bin/env python
"""Score psx::MusicSynth's renders against the Mednafen renders of the disc.

    python tools/bgm/synth_check.py [--songs 0,3,153] [--renders DIR ...] [--cache DIR]
                                    [--synth PATH] [--work DIR] [--json OUT]

For each song: synth_render renders it from the importer's cache; both WAVs
are aligned on the song's first sound (the Mednafen render's after its t0 of
40 s: the boot and the title's intro come first) and refined by
cross-correlation; then

- **windows**: the waveform correlation (both channels) in 0.25 s windows
  over the overlap, windows quieter than -50 dBFS left out; the fraction at
  >= 0.99 and the median. The SPU noise voices would differ every run
  (bgm-comparison.md 11.1), so a song with noise is read by its envelope.
- **env**: the onset envelope's correlation (log-RMS per 256 samples,
  differenced, rectified) over the overlap.
- **drift**: the best offset in 10 s windows along the song, fitted to a line
  (samples per minute) - zero if the tick is right.
- **loop**: each render's loop period, measured the same way on both (the
  lag near the sequencer's own period, from synth_render --trace, at which
  the waveform after the loop start repeats best), and the difference.

- **oracle** (with --oracle): the same windows after each key-on VSync of
  ours is moved by the whole number of samples (-4..+4) that best matches
  the render - estimated per VSync from per-voice stems (synth_render --solo)
  - and the song rendered again with those delays (--offsets). The renders'
  key-ons sit off a uniform VSync grid by the game's interrupt latency
  (libsnd-reading.md section 9); this column says how much of a song's
  mismatch is that and nothing else.

Renders are game-derived audio: scratch only (CLAUDE.md rule 1).
"""
import argparse, json, os, subprocess, sys
import numpy as np
from scipy.signal import fftconvolve

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wavread  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
RATE = 44100
TICK = 263 * 6825 * 65536 / (103896 * 768 * 2)  # samples per VSync, as seq.h


def first_sound(x, start=0, thresh=10 ** (-60 / 20)):
    a = np.abs(x[start:]).max(axis=1)
    i = np.argmax(a > thresh)
    return start + int(i)


def xcorr_lag(a, b, maxlag):
    """lag such that a[n] ~ b[n + lag], searched in +-maxlag (mono float arrays)."""
    n = len(a)
    bb = b[: n + 2 * maxlag] if len(b) >= n + 2 * maxlag else np.pad(b, (0, n + 2 * maxlag - len(b)))
    c = fftconvolve(bb, a[::-1], mode="valid")  # c[k] = sum a[i] b[i+k], k = 0..2*maxlag
    k = int(np.argmax(c))
    return k - maxlag


def ncc(a, b):
    a = a - a.mean(); b = b - b.mean()
    d = np.sqrt((a * a).sum() * (b * b).sum())
    return float((a * b).sum() / d) if d > 0 else 0.0


def envelope(x, hop=256):
    m = x.mean(axis=1)
    n = len(m) // hop
    r = np.sqrt((m[: n * hop].reshape(n, hop) ** 2).mean(axis=1) + 1e-12)
    e = np.diff(20 * np.log10(r))
    return np.maximum(e, 0)


def period(x, start, approx, search=0.01, seg=6.0):
    """The lag near approx at which x repeats from start (envelope, then waveform)."""
    lo, hi = int(approx * (1 - search)), int(approx * (1 + search))
    m = x.mean(axis=1)
    L = int(seg * RATE)
    if start + hi + L > len(m):
        return None, 0.0
    a = m[start:start + L]
    region = m[start + lo:start + hi + L]
    c = fftconvolve(region, a[::-1], mode="valid")
    k = int(np.argmax(c))
    best = lo + k
    # refine +-8 on normalised correlation
    scores = [(ncc(a, m[start + best + d:start + best + d + L]), best + d) for d in range(-8, 9)]
    s, p = max(scores)
    return p, s


def window_stats(o, t, f, n):
    w = RATE // 4
    vals = []
    for s in range(f, n - w, w):
        a = o[s:s + w]; b = t[s:s + w]
        if 20 * np.log10(np.sqrt((b * b).mean()) + 1e-12) < -50:
            continue
        vals.append(ncc(a.ravel(), b.ravel()))
    return np.array(vals)


def oracle_offsets(synth, cache, song, secs, ours, theirs, off, ticks_file, work, base_args, window_max=2048):
    """Per key-on VSync, the whole-sample delay that best matches the render."""
    kons = []  # (tick, sample, [voices])
    for line in open(ticks_file):
        x = line.split()
        kon = int(x[2], 16)
        if kon:
            kons.append((int(x[0]), int(x[1]), [v for v in range(24) if kon >> v & 1]))
    voices = sorted(set(v for k in kons for v in k[2]))
    stems = {}
    for v in voices:
        p = os.path.join(work, "stem%02d.wav" % v)
        subprocess.run([synth, "--cache", cache, "--song", str(song), "--out", p, "--seconds", "%.2f" % secs, "--solo", str(v)]
                       + base_args, check=True, capture_output=True)
        stems[v] = wavread.read(p)[0]
        os.remove(p)
    n = min(len(ours), len(theirs) - off)
    t = theirs[off:off + n]
    o = ours[:n]
    g = float((o * t).sum() / max((t * t).sum(), 1e-12))
    r = t * g - o
    nxt = {}  # (voice, tick) -> end sample
    last = {}
    for tick, S, vs in reversed(kons):
        for v in vs:
            nxt[(v, tick)] = last.get(v, n)
            last[v] = S
    M = 4
    best_d = {}
    for _ in range(2):
        for tick, S, vs in kons:
            # the window: from the key on to the voices' next key on, at least
            # 2048 samples and at most window_max - a pad's slow attack is
            # silent for the first 2048 (libsnd-reading.md 9.6, song 146)
            W = max(2048, min(window_max, max(nxt[(v, tick)] for v in vs) - S, n - 2 * M - 1 - S))
            if S + W + 2 * M >= n or S < M:
                continue
            c = np.zeros((W + 2 * M, 2), np.float32)
            for v in vs:
                end = min(nxt[(v, tick)], S + W) - S
                c[M:M + end] += stems[v][S:S + end]
            d0 = best_d.get(tick, 0)
            # undo the current choice, then pick again
            seg = r[S - M:S + W + M].copy()
            seg += np.roll(c, d0, axis=0) - c
            scores = []
            for d in range(-M, M + 1):
                e = seg + c - np.roll(c, d, axis=0)
                scores.append(((e * e).sum(), d))
            e0 = dict((d_, e_) for e_, d_ in scores)[0]
            e, d = min(scores)
            if e > 0.97 * e0:  # not clearly better than where the grid puts it
                d = 0
            best_d[tick] = d
            r[S - M:S + W + M] = seg + c - np.roll(c, d, axis=0)
    path = os.path.join(work, "offsets%03d.txt" % song)
    with open(path, "w") as fh:
        for tick, d in sorted(best_d.items()):
            if d:
                fh.write("%d %d\n" % (tick, d * 256))
    return path, best_d


def analyse(song, ours, theirs, our_jumps):
    """our_jumps: VSync ticks of the loop jumps in our render (from --trace)."""
    t0 = int(40 * RATE)
    f_their = first_sound(theirs, t0)
    f_our = first_sound(ours, 0)
    off = f_their - f_our  # theirs[n + off] ~ ours[n]
    mo, mt = ours.mean(axis=1), theirs.mean(axis=1)
    # refine on the first 8 s of sound
    L = int(8 * RATE)
    seg = mo[f_our:f_our + L]
    lag = xcorr_lag(seg, mt[f_our + off - 2000:], 2000) - 2000 if False else None
    c_lo = f_our + off - 3000
    region = mt[c_lo:c_lo + L + 6000]
    c = fftconvolve(region, seg[::-1], mode="valid")
    k = int(np.argmax(c))
    off = c_lo + k - f_our
    # overlap
    n = min(len(ours), len(theirs) - off)
    o = ours[:n]
    t = theirs[off:off + n]
    # drift: best offset per 10 s window
    W = 10 * RATE
    drifts = []
    for s in range(f_our, n - W - 200, W):
        a = o[s:s + W].mean(axis=1)
        if np.sqrt((a * a).mean()) < 1e-3:
            continue
        b = t[s - 100:s + W + 100].mean(axis=1)
        cc = fftconvolve(b, a[::-1], mode="valid")
        drifts.append((s, int(np.argmax(cc)) - 100))
    if len(drifts) >= 2:
        xs = np.array([d[0] for d in drifts]); ys = np.array([d[1] for d in drifts])
        slope = np.polyfit(xs, ys, 1)[0] * 60 * RATE
    else:
        slope = 0.0
    # 0.25 s windows
    w = RATE // 4
    vals = []
    for s in range(f_our, n - w, w):
        a = o[s:s + w]; b = t[s:s + w]
        if 20 * np.log10(np.sqrt((b * b).mean()) + 1e-12) < -50:
            continue
        vals.append(ncc(a.ravel(), b.ravel()))
    vals = np.array(vals)
    env_o = envelope(o[f_our:]); env_t = envelope(t[f_our:])
    m = min(len(env_o), len(env_t))
    env = ncc(env_o[:m], env_t[:m])
    # level
    rms_o = 20 * np.log10(np.sqrt((o[f_our:] ** 2).mean()) + 1e-12)
    rms_t = 20 * np.log10(np.sqrt((t[f_our:] ** 2).mean()) + 1e-12)
    row = dict(song=int(song), offset=int(off), first_their_s=f_their / RATE, first_our_s=f_our / RATE,
               windows=int(len(vals)), frac99=float((vals >= 0.99).mean()) if len(vals) else 0.0,
               frac95=float((vals >= 0.95).mean()) if len(vals) else 0.0,
               median=float(np.median(vals)) if len(vals) else 0.0, env=env,
               drift_per_min=float(slope), rms_our=rms_o, rms_their=rms_t, seconds=n / RATE)
    # loop period, measured the same way on both, after the first jump
    if len(our_jumps) >= 1:
        P_seq = (our_jumps[1] - our_jumps[0]) * TICK if len(our_jumps) >= 2 else None
        if P_seq:
            start = int(our_jumps[0] * TICK) + RATE  # 1 s into the second pass... measured from a body point
            start = max(f_our, int(our_jumps[0] * TICK - P_seq) + RATE)
            po, so = period(ours, start, P_seq)
            pt, st = period(theirs[off:], start, P_seq)
            row.update(loop_seq=P_seq, loop_our=po, loop_our_ncc=so, loop_their=pt, loop_their_ncc=st,
                       loop_delta=(po - pt) if (po and pt) else None)
        else:
            row.update(loop_seq=None)
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--songs", default="0,3,7,11,17,25,34,89,153")
    ap.add_argument("--renders", nargs="+", default=["/workspace/scratch/renders", "/workspace/scratch/renders_cloud"])
    ap.add_argument("--cache", default="/workspace/scratch/cache")
    ap.add_argument("--synth", default="/workspace/scratch/seq/build/synth_render")
    ap.add_argument("--work", default="/workspace/scratch/seq/check")
    ap.add_argument("--json", default=None)
    ap.add_argument("--volume", type=int, default=100)
    ap.add_argument("--frames", type=int, default=8)
    ap.add_argument("--keep", action="store_true", help="keep the oracle-timed render in --work")
    ap.add_argument("--oracle", action="store_true", help="also score with per-VSync key-on delays fitted to the render")
    ap.add_argument("--oracle-window", type=float, default=2048 / RATE,
                    help="the longest stretch after a key on the oracle fits its delay over, in seconds (default 2048"
                         " samples, the 2026-10-08 table; 1.0 for the slow-attack pads of music-open-ends.md 1)")
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    songs = [int(s) for s in a.songs.split(",")] if a.songs != "all" else list(range(166))
    rows = []
    for song in songs:
        ref = None
        for d in a.renders:
            p = os.path.join(d, "song%03d_mednafen.wav" % song)
            if os.path.exists(p) and (d == a.renders[0] or os.path.exists(p[:-4] + ".done") or os.path.exists(p + ".done")):
                ref = p
                break
        if not ref:
            print("song %3d: no render" % song)
            continue
        theirs, sr = wavread.read(ref)
        assert sr == RATE
        secs = len(theirs) / RATE - 40.0
        out = os.path.join(a.work, "song%03d_ours.wav" % song)
        base_args = ["--volume", str(a.volume), "--frames", str(a.frames)]
        ticks_file = os.path.join(a.work, "ticks%03d.txt" % song)
        # ours runs on to two loop jumps where the song has them (the period)
        r = subprocess.run([a.synth, "--cache", a.cache, "--song", str(song), "--out", out, "--seconds", "%.2f" % max(secs, 420.0),
                            "--trace", "--ticks", ticks_file] + base_args,
                           capture_output=True, text=True)
        if r.returncode:
            print("song %3d: synth_render failed: %s" % (song, r.stderr.strip()[-300:]))
            rows.append(dict(song=song, error=r.stderr.strip()[-300:]))
            continue
        jumps = [int(l.split()[-1]) for l in r.stderr.splitlines() if l.startswith("loop jump")]
        ours, _ = wavread.read(out)
        row = analyse(song, ours, theirs, jumps)
        row["render"] = ref
        if a.oracle:
            path, dmap = oracle_offsets(a.synth, a.cache, song, secs, ours, theirs, row["offset"], ticks_file, a.work, base_args,
                                        window_max=int(a.oracle_window * RATE))
            out2 = os.path.join(a.work, "song%03d_oracle.wav" % song)
            subprocess.run([a.synth, "--cache", a.cache, "--song", str(song), "--out", out2, "--seconds", "%.2f" % secs,
                            "--offsets", path] + base_args, check=True, capture_output=True)
            o2, _ = wavread.read(out2)
            n2 = min(len(o2), len(theirs) - row["offset"])
            vals = window_stats(o2[:n2], theirs[row["offset"]:row["offset"] + n2], first_sound(o2), n2)
            moved = [d for d in dmap.values() if d]
            row.update(oracle_frac99=float((vals >= 0.99).mean()), oracle_median=float(np.median(vals)),
                       oracle_moved=len(moved), oracle_kon_ticks=len(dmap),
                       oracle_spread=float(np.std(list(dmap.values()))) if dmap else 0.0)
            print("          oracle timing: >=0.99 %.3f median %.4f (%d of %d key-on VSyncs moved, sd %.2f samples)"
                  % (row["oracle_frac99"], row["oracle_median"], len(moved), len(dmap), row["oracle_spread"]))
            if not a.keep:
                os.remove(out2)
        rows.append(row)
        print("song %3d: windows %4d  >=0.99 %.3f  >=0.95 %.3f  median %.4f  env %.3f  drift %+.2f/min  rms %.1f/%.1f  loop %s"
              % (song, row["windows"], row["frac99"], row["frac95"], row["median"], row["env"], row["drift_per_min"],
                 row["rms_our"], row["rms_their"],
                 ("ours %s theirs %s delta %s" % (row.get("loop_our"), row.get("loop_their"), row.get("loop_delta")))
                 if row.get("loop_seq") else "-"), flush=True)
        os.remove(out) if os.path.getsize(out) > 200e6 else None
    if a.json:
        json.dump(rows, open(a.json, "w"), indent=1, default=float)


if __name__ == "__main__":
    main()
