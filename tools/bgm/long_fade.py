#!/usr/bin/env python
"""A loop at the sequence's period with a longer crossfade, measured (docs/music-open-ends.md 4).

    python tools/bgm/long_fade.py --tracks 83,95 [--fades 128,1024,4096,11025,22050] [--json OUT]

For a song whose waveform never repeats (a pad whose held voices and sample
loops run free across the sequence's loop point, so no two passes match - even
our own sequencer's do not), no sample-exact loop exists in its MP3; what a
loop at the sequence's period could do is splice the file's end of a pass onto
its start with a crossfade long enough to hide the change of phase. This plays
that splice offline from loops.json's row (start, end = start + the period, the
row measured with the period pinned to the sequence's, measure_loops.py) for
each crossfade length, linear (the engine's weights) and equal-power
(sin / cos), and scores the 3 s after the seam against what the disc really
plays there (the row's Mednafen render, mapped by the row's alignment):

  env     correlation of the log-RMS in 10 ms steps, splice against render
  dev     the largest level difference in 50 ms windows over the crossfade and
          0.5 s after it, in dB, less the median difference over the 3 s (the
          MP3's own level offset): a dip or a bump at the seam
  step    the largest sample-to-sample jump within 2 ms of the seam over the
          99th percentile of the second around it (prove_loops.py's)

and the same for the original's rewind (the file's end onto its start) and for
the file playing on past the loop end with no seam at all (the yardstick of
how alike the MP3 and the render are there; needs the file to hold 3 s more).
A near-full shortened row (intro + one body, short of it by `short_by`
samples, bgm-comparison.md 11.1) is played as the loop it could be: the file
to its last whole frame, then on from the loop start - each pass `short_by`
samples short of the disc's, the music slipping that much at the seam.
Renders and MP3 decodes are game data: read only, nothing written but --json.
"""
import argparse, json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bgm_paths import PC, LOOPS_JSON  # noqa: E402
from loops import decode, SR          # noqa: E402
from wavread import read as wavread   # noqa: E402

OUT = PC + "/analysis/bgm"


def splice(x, start, end, n, fade, power):
    """x[:end], then x[start:] - the first `fade` samples after end blended into those from start."""
    body = x[start:start + n].copy()
    if fade:
        j = (np.arange(fade) + 0.5) / fade
        w_in, w_out = (np.sin(j * np.pi / 2), np.cos(j * np.pi / 2)) if power else (j, 1 - j)
        body[:fade] = x[end:end + fade] * w_out[:, None] + x[start:start + fade] * w_in[:, None]
    return np.concatenate([x[:end], body])


def logrms(m, hop):
    k = len(m) // hop
    return 20 * np.log10(np.sqrt((m[:k * hop].reshape(k, hop) ** 2).mean(axis=1)) + 1e-6)


def score(y, seam, truth, fade):
    m = y.mean(axis=1); t = truth.mean(axis=1)
    L = 3 * SR
    a, b = m[seam:seam + L], t[:L]
    ea, eb = logrms(a, 441), logrms(b, 441)
    env = float(np.corrcoef(ea, eb)[0, 1])
    wa, wb = logrms(a, SR // 20), logrms(b, SR // 20)
    k = int((fade + SR // 2) // (SR // 20)) + 1
    off = np.median(wa - wb)
    dev = float(np.max(np.abs((wa - wb - off)[:k])))
    d = np.abs(np.diff(m[seam - SR // 2:seam + SR // 2]))
    near = d[SR // 2 - 88:SR // 2 + 88].max()
    ref = np.percentile(np.concatenate([d[:SR // 2 - 441], d[SR // 2 + 441:]]), 99)
    return dict(env=round(env, 3), dev=round(dev, 2), step=round(float(near / max(ref, 1e-9)), 2))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tracks", required=True)
    ap.add_argument("--fades", default="128,1024,4096,11025,22050")
    ap.add_argument("--json", default=None)
    a = ap.parse_args()
    table = json.load(open(LOOPS_JSON))
    fades = [int(f) for f in a.fades.split(",")]
    res = {}
    for tr in [int(t) for t in a.tracks.split(",")]:
        r = table[str(tr)]
        near = r.get("case") == "shortened" and r.get("near_full")
        if (r.get("case") != "full" and not near) or "align" not in r:
            print("%3d: %s row, not a full one - no loop at the period inside the file" % (tr, r.get("case")))
            continue
        x = decode(PC + "/bof3/BGM/" + r["file"])
        rec, _ = wavread(OUT + "/renders/" + r["render"])
        al = r["align"]
        to_rec = lambda s: int(round(s + al["offset"] + al["intercept"] + al["ppm"] * 1e-6 * s))
        start, end, n = r["start"], r["end"], len(x)
        if near:   # intro + one body less short_by: the whole file from the loop start, a slip of short_by a pass
            start = r["near_full_start"]
        truth = rec[to_rec(end):to_rec(end) + 3 * SR]
        if len(truth) < 3 * SR:
            print("%3d: the render ends within 3 s of the loop end" % tr)
            continue
        row = dict(start=start, end=end, body_s=(end - start) / SR, file_after_end_s=(n - end) / SR)
        back = rec[to_rec(n):to_rec(n) + 3 * SR]
        if len(back) == 3 * SR:   # the render runs 3 s past the file's end (else the rewind is unmeasured)
            row["rewind"] = score(np.concatenate([x, x[:4 * SR]]), n, back, 0)
        if n - end >= 3 * SR:
            row["no_seam"] = score(x, end, truth, 0)
        for f in fades:
            for power in (False, True):
                if end + f > n:
                    continue
                y = splice(x, start, end, 4 * SR, f, power)
                row["%s%d" % ("p" if power else "l", f)] = score(y, end, truth, f)
        res[tr] = row
        cols = ["rewind", "no_seam"] + ["%s%d" % (p, f) for f in fades for p in "lp"]
        print("%3d: body %.2f s, %.2f s of file after the loop end" % (tr, row["body_s"], row["file_after_end_s"]))
        for c in cols:
            if c in row:
                print("     %-8s env %6.3f  dev %6.2f dB  step %5.2f" % (c, row[c]["env"], row[c]["dev"], row[c]["step"]))
    if a.json:
        json.dump(res, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
