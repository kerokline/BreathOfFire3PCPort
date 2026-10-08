"""Step 4 of docs/bgm-comparison.md: a Mednafen render of a song against the PC's MP3.

    python compare.py TRACK REC.wav

Finds the song in the recording, measures its loop on the disc side, aligns the
MP3 to it, measures drift, level, DC, noise floor, high-frequency cutoff, and
draws the plots into analysis/bgm/plots/. Writes analysis/bgm/compare_TRACK.json.
"""
import json, os, subprocess, sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy import signal
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wavread import read as wavread
from loops import decode, ncc_search, SR

from bgm_paths import PC
OUT = PC + "/analysis/bgm"


def db(x):
    return float(20 * np.log10(max(x, 1e-12)))


def rms(x):
    return float(np.sqrt(np.mean(np.asarray(x, dtype=np.float64) ** 2)))


def main():
    track, recpath = int(sys.argv[1]), sys.argv[2]
    inv = json.load(open(OUT + "/inventory.json"))
    info = inv["pc"][str(track)]
    mp3 = decode(PC + "/bof3/BGM/" + info["file"])
    rec, sr = wavread(recpath)
    assert sr == SR, sr
    mm, rm = mp3.mean(axis=1), rec.mean(axis=1)
    res = dict(track=track, mp3=info["file"], rec_seconds=len(rec) / SR)

    # 1. where the MP3's opening sits in the recording: band-energy onsets (10 ms), then the waveform
    from feat import bands, best_lag
    lead = int(np.nonzero(np.abs(mm) > 1e-4)[0][0])
    _, Dm = bands(mm); _, Dr = bands(rm)
    a = lead // 441
    k, score, _ = best_lag(Dm[:, a:a + 800], Dr)
    coarse = k * 441 - a * 441
    if len(sys.argv) > 3:
        # the song's first sound in the recording after T0 against the MP3's first sound: the music
        # repeats phrases, so a feature match alone can land a phrase late
        T0 = int(float(sys.argv[3]) * SR)
        ar = np.abs(rec).max(axis=1); am = np.abs(mp3).max(axis=1)
        coarse = (T0 + int(np.nonzero(ar[T0:] > 1e-3)[0][0])) - int(np.nonzero(am > 1e-3)[0][0])
    def fine(t, coarse, W=2 * SR, R=SR // 20):
        w = mm[t:t + W]
        lo = max(0, t + coarse - R); hi = min(len(rm), t + coarse + W + R)
        if hi - lo < W or rms(w) < 1e-3:
            return None
        cc = ncc_search(w, rm[lo:hi]); j = int(np.argmax(cc))
        return lo + j - t, float(cc[j])
    off, ncc0 = fine(lead + SR, coarse, R=SR // 10)
    res["align"] = dict(mp3_lead=lead, feature_score=score, offset_samples=off, offset_s=off / SR, ncc=ncc0)

    # 2. drift: local waveform offsets of 2 s windows along the MP3
    drift = []
    for t in range(lead, len(mm) - 3 * SR, SR):
        if t + off + 3 * SR > len(rm):
            break
        r = fine(t, off, R=SR // 10)
        if r:
            drift.append((t / SR, r[0] - off, r[1]))
    res["drift"] = drift

    # 3. disc-side loop: self-similarity of the recording after the song start
    seq = None
    tab = [t for t in inv["table"] if t["song"] == track]
    if tab:
        t = tab[0]
        sub = inv["disc"][t["file"]]["subs"][t["sub"]]
        seq = sub
        ls = [l for l in sub["loops"] if l["kind"] == "start"]
        le = [l for l in sub["loops"] if l["kind"] == "end"]
        if ls and le:
            a, b = ls[0]["seconds"], le[0]["seconds"]
            s0 = off + lead  # song start in the recording (approx; the MP3's first sound)
            t0 = int(s0 + (a + min(2.0, 0.1 * (b - a))) * SR)
            ref = rm[t0:t0 + 3 * SR]
            lo = int(t0 + 0.5 * (b - a) * SR)
            if lo + 3 * SR < len(rm):
                cc = ncc_search(ref, rm[lo:])
                j = int(np.argmax(cc))
                P = lo + j - t0
                # loop start: earliest t such that rec[t:t+w] ~ rec[t+P:t+P+w] onward
                w = int(0.05 * SR); start = None; scan = []
                for tt in range(int(s0), t0 + 3 * SR, w // 2):
                    u, v = rm[tt:tt + w], rm[tt + P:tt + P + w]
                    den = np.sqrt(np.dot(u, u) * np.dot(v, v))
                    scan.append((tt, float(np.dot(u, v) / den) if den > 1e-12 else 0.0))
                for i in range(len(scan)):
                    if all(v > 0.9 for _, v in scan[i:]):
                        start = scan[i][0]; break
                res["disc_loop"] = dict(period_s=P / SR, ncc=float(cc[j]), nominal_body_s=b - a,
                                        tempo_ratio=(P / SR) / (b - a),
                                        start_rec_s=start / SR if start else None,
                                        start_after_song_start_s=(start - s0) / SR if start else None,
                                        nominal_start_s=a, song_start_rec_s=s0 / SR)
                if start:
                    # where the MP3's end falls on the disc timeline
                    end_rec = len(mm) + off
                    res["disc_loop"]["mp3_end_rec_s"] = end_rec / SR
                    res["disc_loop"]["mp3_end_minus_loop_start_over_period"] = (end_rec - start) / P

    # 4. level, DC, noise floor and spectra over the aligned overlap
    a0 = lead + int(1 * SR)
    a1 = min(len(mm), len(rm) - off) - int(0.5 * SR)
    seg_m = mp3[a0:a1]; seg_r = rec[a0 + off:a1 + off]
    n = min(len(seg_m), len(seg_r)); seg_m, seg_r = seg_m[:n], seg_r[:n]
    res["overlap_s"] = n / SR
    res["rms_db"] = dict(mp3=db(rms(seg_m)), disc=db(rms(seg_r)))
    res["peak"] = dict(mp3=float(np.abs(seg_m).max()), disc=float(np.abs(seg_r).max()))
    res["dc"] = dict(mp3=[float(v) for v in mp3.mean(axis=0)], disc=[float(v) for v in seg_r.mean(axis=0)])
    fr = int(0.05 * SR)
    def frames_db(x):
        m = x.mean(axis=1); k = len(m) // fr
        return np.array([db(rms(m[i * fr:(i + 1) * fr])) for i in range(k)])
    fm, fd = frames_db(seg_m), frames_db(seg_r)
    res["quiet_frames_db_p5"] = dict(mp3=float(np.percentile(fm, 5)), disc=float(np.percentile(fd, 5)))
    # the recording before the song (BIOS / logo silence) as the emulator's own floor
    pre = rec[max(0, off + lead - int(1.5 * SR)):off + lead - int(0.5 * SR)]
    res["disc_floor_before_song_db"] = db(rms(pre)) if len(pre) else None
    res["mp3_lead_samples_rms_db"] = db(rms(mp3[:lead])) if lead else None
    g = rms(seg_m) / max(rms(seg_r), 1e-12)
    f, Pm = signal.welch(seg_m.mean(axis=1), SR, nperseg=8192)
    _, Pr = signal.welch(seg_r.mean(axis=1) * g, SR, nperseg=8192)
    Lm, Lr = 10 * np.log10(Pm + 1e-20), 10 * np.log10(Pr + 1e-20)
    ref_band = (f > 200) & (f < 2000)
    def cutoff(L):
        top = np.median(L[ref_band])
        above = np.nonzero(L > top - 50)[0]
        return float(f[above[-1]])
    res["cutoff_hz_-50db"] = dict(mp3=cutoff(Lm), disc=cutoff(Lr))
    bands = [(20, 200), (200, 2000), (2000, 8000), (8000, 12000), (12000, 16000), (16000, 19000), (19000, 22000)]
    res["band_db_rel_disc"] = {"%d-%d" % b: float(10 * np.log10(Pm[(f >= b[0]) & (f < b[1])].sum() / max(Pr[(f >= b[0]) & (f < b[1])].sum(), 1e-30))) for b in bands}
    res["band_db_abs"] = {"%d-%d" % b: [float(10 * np.log10(Pm[(f >= b[0]) & (f < b[1])].sum() + 1e-30)), float(10 * np.log10(Pr[(f >= b[0]) & (f < b[1])].sum() + 1e-30))] for b in bands}
    # residual after alignment and gain (how alike the waveforms are)
    m1, r1 = seg_m.mean(axis=1), seg_r.mean(axis=1) * g
    res["residual_db_rel"] = db(rms(m1 - r1) / max(rms(m1), 1e-12))
    res["gain_disc_to_mp3_db"] = db(g)

    os.makedirs(OUT + "/plots", exist_ok=True)
    fig, ax = plt.subplots(3, 1, figsize=(11, 11))
    ax[0].plot(f, Lm, lw=0.7, label="PC MP3"); ax[0].plot(f, Lr, lw=0.7, label="disc render (Mednafen), gain-matched")
    ax[0].set_xlabel("Hz"); ax[0].set_ylabel("dB"); ax[0].legend(); ax[0].set_title("track %03d: power spectral density over the aligned overlap" % track)
    for i, (x, name) in enumerate(((m1, "PC MP3"), (r1, "disc render"))):
        pass
    ff, tt, S1 = signal.spectrogram(m1[:SR * 30], SR, nperseg=2048)
    _, _, S2 = signal.spectrogram(r1[:SR * 30], SR, nperseg=2048)
    ax[1].pcolormesh(tt, ff, 10 * np.log10(S1 + 1e-14), shading="auto", vmin=-140, vmax=-40); ax[1].set_title("PC MP3, first 30 s of overlap"); ax[1].set_ylabel("Hz")
    ax[2].pcolormesh(tt, ff, 10 * np.log10(S2 + 1e-14), shading="auto", vmin=-140, vmax=-40); ax[2].set_title("disc render, same 30 s"); ax[2].set_ylabel("Hz"); ax[2].set_xlabel("s")
    fig.tight_layout(); fig.savefig(OUT + "/plots/track%03d_spectra.png" % track, dpi=90); plt.close(fig)
    if drift:
        d = np.array(drift)
        fig, ax = plt.subplots(1, 1, figsize=(9, 4))
        ax.plot(d[:, 0], d[:, 1], ".-"); ax.set_xlabel("MP3 time (s)"); ax.set_ylabel("offset change (samples)")
        ax.set_title("track %03d: MP3 vs disc render, local alignment along the MP3" % track)
        fig.tight_layout(); fig.savefig(OUT + "/plots/track%03d_drift.png" % track, dpi=90); plt.close(fig)
    json.dump(res, open(OUT + "/compare_%03d.json" % track, "w"), indent=1)
    print(json.dumps({k: v for k, v in res.items() if k != "drift"}, indent=1))
    print("drift (t, d_off, ncc):", [(round(a, 1), int(b), round(c, 3)) for a, b, c in drift[::4]])


if __name__ == "__main__":
    main()
