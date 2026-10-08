"""Section 12's proof without the game: the PC's loop before (the decoder rewound
to 0) and after (the table's row), spliced exactly as Music_Decode does it -
the decoded stream cut at sample `end` and resumed at sample `start` of a
fresh decode from the file's first frame, which for a deterministic decoder is
the first pass's samples (ffmpeg's decode stands in for the game's) - and
the seam measured:

  continuity  the correlation of 0.25 s windows of what the PC plays after
              the seam against what the music really does there: the render
              of the disc (rows measured on a render) or the file's own
              continuation past `end` (in-file rows, whose music repeats)
  step        the largest sample-to-sample jump within 2 ms of the seam,
              over the 99th percentile of jumps in the surrounding second
              (a click shows as a ratio well above 1)
  gap         samples under -60 dBFS within 50 ms after the seam

    python prove_loops.py [TRACK ...]      default: every row of loops.json
Writes analysis/bgm/loop_proof.json and plots/trackNNN_seam.png; for 153 also
analysis/bgm/listen/153_loop_fixed.wav (section 9's window, the fix applied).
"""
import json, os, sys, wave
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bgm_paths import PC
from wavread import read as wavread
from loops import decode, SR

OUT = PC + "/analysis/bgm"
Q = SR // 4


def nccv(u, v):
    u = np.asarray(u, np.float64); v = np.asarray(v, np.float64)
    d = np.sqrt(np.dot(u, u) * np.dot(v, v))
    return float(np.dot(u, v) / d) if d > 1e-12 else 0.0


def played(x, start, end, length, fade=0):
    """The PC's output from sample 0 for `length` samples: x[:end], then x[start:end] for ever,
    the first `fade` samples of every pass after the first blended with x[end:end+fade] exactly as
    Music_Decode does it (16-bit integers, weight (2j+1)/(2 fade), truncated toward zero)."""
    q = np.round(np.clip(x, -1, 1 - 1 / 32768) * 32768).astype(np.int64)  # the decoder's 16-bit samples
    body = q[start:end].copy()
    if fade:
        j = np.arange(fade)[:, None]
        a, b = q[end:end + fade], q[start:start + fade]
        num = a * (2 * fade - (2 * j + 1)) + b * (2 * j + 1)
        body[:fade] = np.trunc(num / (2 * fade)).astype(np.int64)
    out = [q[:end]]
    total = end
    while total < length:
        out.append(body); total += end - start
    return (np.concatenate(out)[:length] / 32768.0).astype(np.float32)


def seam_stats(pc, seam, truth):
    """pc: the PC's output; seam: its index; truth: what the music does from the seam on."""
    m = pc.mean(axis=1) if pc.ndim == 2 else pc
    t = truth.mean(axis=1) if truth.ndim == 2 else truth
    cont = [nccv(m[seam + i * Q:seam + (i + 1) * Q], t[i * Q:(i + 1) * Q]) for i in range(min(8, len(t) // Q))]
    d = np.abs(np.diff(m[seam - SR // 2:seam + SR // 2]))
    near = d[SR // 2 - 88:SR // 2 + 88].max()
    ref = np.percentile(np.concatenate([d[:SR // 2 - 441], d[SR // 2 + 441:]]), 99)
    post = np.abs(pc[seam:seam + SR // 20]).max(axis=1) if pc.ndim == 2 else np.abs(m[seam:seam + SR // 20])
    return dict(continuity=[round(c, 3) for c in cont], continuity_first_s=float(np.mean(cont[:4])),
                step_ratio=float(near / max(ref, 1e-9)), gap_samples=int(np.sum(post < 1e-3)))


def main():
    table = json.load(open(OUT + "/loops.json"))
    tracks = [int(a) for a in sys.argv[1:]] or sorted(int(k) for k, r in table.items() if not r.get("excluded"))
    res = {}
    for tr in tracks:
        r = table[str(tr)]
        x = decode(PC + "/bof3/BGM/" + r["file"])
        n = len(x)
        row = dict(track=tr, file=r["file"], excluded=bool(r.get("excluded")))
        if r.get("render") and "align" in r:
            rec, _ = wavread(OUT + "/renders/" + r["render"])
            a = r["align"]
            to_rec = lambda s: int(round(s + a["offset"] + a["intercept"] + a["ppm"] * 1e-6 * s))
            truth_at = lambda s: rec[to_rec(s):to_rec(s) + 2 * SR]
            # the original: the file ends (sample n) and starts again at 0
            row["before"] = seam_stats(np.concatenate([x, x[:3 * SR]]), n, truth_at(n))
            if not r.get("excluded"):
                pc = played(x, r["start"], r["end"], r["end"] + 3 * SR, r.get("fade", 0))
                row["after"] = seam_stats(pc, r["end"], truth_at(r["end"]))
                # the render's own loop as the yardstick: its seam against its next pass
                P = r["render_loop"]["period"]; S = r["render_loop"]["start_rec"]
                row["render_self"] = seam_stats(np.concatenate([rec[:S + P], rec[S:S + 3 * SR]]), S + P,
                                                rec[S + P:S + P + 2 * SR])
        else:
            # in-file rows: the truth after `end` is the file's own continuation
            row["before"] = seam_stats(np.concatenate([x, x[:3 * SR]]), n,
                                       x[n - (r["end"] - r["start"]):n - (r["end"] - r["start"]) + 2 * SR])
            pc = played(x, r["start"], r["end"], r["end"] + 3 * SR, r.get("fade", 0))
            row["after"] = seam_stats(pc, r["end"], x[r["end"]:r["end"] + 2 * SR])
        res[tr] = row
        print(tr, json.dumps({k: v for k, v in row.items()}, default=float))
        if "after" in row:
            fig, ax = plt.subplots(2, 1, figsize=(10, 5), sharex=True)
            w = SR
            b = np.concatenate([x, x[:2 * SR]])[n - w:n + w].mean(axis=1)
            pc = played(x, r["start"], r["end"], r["end"] + 2 * SR, r.get("fade", 0))[r["end"] - w:r["end"] + w].mean(axis=1)
            tt = (np.arange(2 * w) - w) / SR
            ax[0].plot(tt, b, lw=0.4); ax[0].set_title("track %03d, before: the file's end, then its start (the original's rewind)" % tr)
            ax[1].plot(tt, pc, lw=0.4, color="C2"); ax[1].set_title("after: sample %d, then sample %d (the table's row)" % (r["end"], r["start"]))
            ax[1].set_xlabel("seconds from the seam")
            fig.tight_layout(); fig.savefig(OUT + "/plots/track%03d_seam.png" % tr, dpi=90); plt.close(fig)
    old = json.load(open(OUT + "/loop_proof.json")) if os.path.exists(OUT + "/loop_proof.json") else {}
    old.update({str(k): v for k, v in res.items()})
    json.dump(old, open(OUT + "/loop_proof.json", "w"), indent=1, default=float)


def listen_153():
    """153_loop_fixed.wav: section 9's 45 s window (render 70..115 s), the PC's playback with the row."""
    meta = json.load(open(OUT + "/listen/153_meta.json"))
    r = json.load(open(OUT + "/loops.json"))["153"]
    x = decode(PC + "/bof3/BGM/" + r["file"])
    w0, w1 = meta["window_rec_s"]
    off, slope = meta["offset_used"], meta["drift_slope_samples_per_s"]
    g = np.arange(int(w0 * SR), int(w1 * SR)) - off
    g = np.round(g - slope * (g / SR)).astype(np.int64)
    pc = played(x, r["start"], r["end"], int(g.max()) + 1, r.get("fade", 0))
    y = pc[g] * 10 ** (meta["common_scale_db"] / 20)
    with wave.open(OUT + "/listen/153_loop_fixed.wav", "wb") as f:
        f.setnchannels(2); f.setsampwidth(2); f.setframerate(SR)
        f.writeframes((np.clip(y, -1, 1) * 32767).astype("<i2").tobytes())
    seam = (r["end"] + off + slope * r["end"] / SR) / SR - w0
    print("153_loop_fixed.wav: the seam at %.2f s of the window" % seam)


if __name__ == "__main__":
    main()
    if not sys.argv[1:] or "153" in sys.argv[1:]:
        listen_153()
