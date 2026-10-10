"""Step 3 of docs/bgm-comparison.md for every MP3: edges, loop period, loop start.

For each PC track: decode with ffmpeg (every frame's 1152 samples, no trim,
as the game's decoder hands them out - which holds because the files carry no
Xing/Info/LAME or ID3 tag for ffmpeg to take an encoder delay or padding
from: a decode is exactly frames * 1152 samples on every track, and
gen_loop_table.py refuses to write a row where it is not), measure the leading and trailing
silence, then find the loop body by self-correlation guided by the paired
sub-song's loop markers. Writes analysis/bgm/mp3_scan.json (named loops.json until 2026-10-06).
"""
import json, subprocess, sys
import numpy as np

from bgm_paths import PC
OUT = PC + "/analysis/bgm"
SR = 44100


def decode(path):
    r = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-acodec", "pcm_f32le", "-"],
                       capture_output=True, check=True)
    return np.frombuffer(r.stdout, dtype=np.float32).reshape(-1, 2)


def edges(x, thr):
    m = np.abs(x).max(axis=1)
    idx = np.nonzero(m > thr)[0]
    if len(idx) == 0:
        return len(m), len(m)
    return int(idx[0]), int(len(m) - 1 - idx[-1])


def ncc_search(ref, sig):
    """Normalised cross-correlation of ref against every offset in sig (FFT)."""
    n = len(sig) + len(ref)
    nf = 1 << (n - 1).bit_length()
    F = np.fft.rfft(sig, nf) * np.conj(np.fft.rfft(ref, nf))
    c = np.fft.irfft(F, nf)[:len(sig) - len(ref) + 1]
    cs = np.concatenate([[0.0], np.cumsum(sig.astype(np.float64) ** 2)])
    e = cs[len(ref):] - cs[:-len(ref)]
    er = np.sum(ref.astype(np.float64) ** 2)
    denom = np.sqrt(np.maximum(e, 1e-20) * er)
    out = c / denom
    out[e < 1e-6 * er] = 0.0
    return out


def loop_body(mono, a_nom, b_nom, lead):
    """Period P (samples) and the loop start in the MP3 (samples)."""
    body = (b_nom - a_nom) * SR
    W = int(min(3.0 * SR, body * 0.25))
    t0 = int(lead + a_nom * SR + min(2.0 * SR, body * 0.1))
    if t0 + W >= len(mono) or W < 0.5 * SR:
        return None
    ref = mono[t0:t0 + W]
    lo = int(t0 + 0.5 * body)
    hi = len(mono)
    if hi - lo <= W:
        return dict(period=None, why="mp3 ends before one body repeats", t0=t0)
    c = ncc_search(ref, mono[lo:hi])
    k = int(np.argmax(c))
    P = lo + k - t0
    peak = float(c[k])
    # refine the loop start: the earliest t where x[t:t+w] matches x[t+P:t+P+w]
    w = int(0.05 * SR)
    start, scan = None, []
    tmin = max(0, int(lead + a_nom * SR - 6 * SR))
    for t in range(tmin, min(t0 + W, len(mono) - P - w), w // 2):
        u, v = mono[t:t + w], mono[t + P:t + P + w]
        den = np.sqrt(np.dot(u, u) * np.dot(v, v))
        scan.append((t, float(np.dot(u, v) / den) if den > 1e-12 else 0.0))
    # first t from which every later window correlates > 0.9
    ok = [s for s in scan]
    for i in range(len(ok)):
        if all(v > 0.9 for _, v in ok[i:]):
            start = ok[i][0]
            break
    return dict(period=int(P), peak=peak, t0=t0, start=start) if peak > 0.9 else dict(period=None, why='no repeat of the body in the file', best_peak=peak, t0=t0)


def exact_start(mono, P, approx):
    """Sample-exact loop start: smallest t with mono[t+j]==mono[t+P+j] closely, near approx."""
    if approx is None:
        return None
    best = None
    w = 256
    for t in range(max(0, approx - SR // 10), approx + SR // 10):
        u, v = mono[t:t + w], mono[t + P:t + P + w]
        err = np.sqrt(np.mean((u - v) ** 2)) / (np.sqrt(np.mean(u ** 2)) + 1e-9)
        if err < 0.05:
            return t
    return best


def main():
    inv = json.load(open(OUT + "/inventory.json"))
    tab = {t["song"]: t for t in inv["table"]}
    only = set(int(a) for a in sys.argv[1:])
    res = {}
    for n, info in sorted(inv["pc"].items(), key=lambda kv: int(kv[0])):
        n = int(n)
        if only and n not in only:
            continue
        x = decode(PC + "/bof3/BGM/" + info["file"])
        lead80, trail80 = edges(x, 10 ** (-80 / 20))
        lead60, trail60 = edges(x, 10 ** (-60 / 20))
        mono = x.mean(axis=1)
        r = dict(file=info["file"], samples=len(x), seconds=len(x) / SR,
                 lead_80db=lead80, trail_80db=trail80, lead_60db=lead60, trail_60db=trail60,
                 peak=float(np.abs(x).max()), dc=[float(x[:, 0].mean()), float(x[:, 1].mean())],
                 rms_db=float(20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)),
                 first_sample_db=float(20 * np.log10(np.abs(x[:1152]).max() + 1e-12)),
                 last_frame_db=float(20 * np.log10(np.abs(x[-1152:]).max() + 1e-12)))
        if n in tab:
            t = tab[n]
            sub = inv["disc"][t["file"]]["subs"][t["sub"]]
            ls = [l for l in sub["loops"] if l["kind"] == "start"]
            le = [l for l in sub["loops"] if l["kind"] == "end"]
            r["seq"] = dict(file=t["file"], sub=t["sub"], seconds=sub["seconds"],
                            loop_start=ls[0]["seconds"] if ls else None,
                            loop_end=le[0]["seconds"] if le else None)
            if ls and le and not info["once"]:
                lb = loop_body(mono, ls[0]["seconds"], le[0]["seconds"], lead80)
                if lb and lb.get("period"):
                    lb["exact_start"] = exact_start(mono, lb["period"], lb["start"])
                    P = lb["period"]
                    s = lb["exact_start"] if lb["exact_start"] is not None else lb["start"]
                    lb["period_s"] = P / SR
                    lb["nominal_body_s"] = le[0]["seconds"] - ls[0]["seconds"]
                    lb["tempo_ratio"] = lb["period_s"] / lb["nominal_body_s"]
                    if s is not None:
                        lb["start_s"] = s / SR
                        lb["passes_in_file"] = (len(x) - s) / P
                r["loop"] = lb
        # does the opening reappear later? (H1: the intro replayed or not)
        W = int(2.0 * SR)
        a0 = lead80 + int(0.25 * SR)
        if a0 + W < len(mono) - W:
            c = ncc_search(mono[a0:a0 + W], mono[a0 + W:])
            k = int(np.argmax(c))
            r["intro_reappears"] = dict(peak=float(c[k]), at_s=(a0 + W + k) / SR)
        res[n] = r
        print(n, info["file"], round(r["seconds"], 2), "lead", lead80, "trail", trail80,
              "loop", {k: (round(v, 3) if isinstance(v, float) else v) for k, v in (r.get("loop") or {}).items()},
              "intro", r.get("intro_reappears"), flush=True)
    name = OUT + ("/mp3_scan.json" if not only else "/mp3_scan_part.json")
    json.dump(res, open(name, "w"), indent=1)


if __name__ == "__main__":
    main()
