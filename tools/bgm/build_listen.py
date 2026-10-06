"""Step 5's files: analysis/bgm/listen/<track>_mp3.wav, <track>_disc.wav, <track>_seam.wav.

    python build_listen.py TRACK REC.wav DISC_WIN_START DISC_WIN_END

The window is given on the recording's timeline (seconds). The MP3 file is the
PC's playback over the same musical span: the decoded stream, and for a looping
track the stream again from sample 0 after the end (the decoder rewind,
Music_Decode -> Mp3_Seek(decoder, 0)), mapped onto the recording by the
alignment in analysis/bgm/compare_TRACK.json (offset + linear drift).
Both are loudness-matched by RMS over the window (the disc file is scaled to
the MP3's level; both scaled down together if a peak would pass -1 dBFS).
"""
import json, os, sys, wave
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wavread import read as wavread
from loops import decode, SR

from bgm_paths import PC
OUT = PC + "/analysis/bgm/listen"


def write(path, x):
    x = np.clip(x, -1, 1)
    with wave.open(path, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype("<i2").tobytes())


def main():
    track, rec, w0, w1 = int(sys.argv[1]), sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
    cmp_ = json.load(open(PC + "/analysis/bgm/compare_%03d.json" % track))
    inv = json.load(open(PC + "/analysis/bgm/inventory.json"))
    info = inv["pc"][str(track)]
    m = decode(PC + "/bof3/BGM/" + info["file"])
    r, _ = wavread(rec)
    off = cmp_["align"]["offset_samples"]
    d = np.array(cmp_["drift"])
    good = d[d[:, 2] > 0.5]
    # a robust line through the confident local offsets: median slope of pairs, median intercept
    if len(good) >= 3:
        sl = [(good[j, 1] - good[i, 1]) / (good[j, 0] - good[i, 0]) for i in range(len(good)) for j in range(i + 1, len(good)) if good[j, 0] > good[i, 0]]
        slope = float(np.median(sl)); icpt = float(np.median(good[:, 1] - slope * good[:, 0]))
    else:
        slope, icpt = 0.0, 0.0
    off = off + int(round(icpt))
    a, b = int(w0 * SR), int(w1 * SR)
    disc = r[a:b]
    # PC playback timeline: game sample g -> file sample g mod len (looping) or silence after the end
    n = len(m)
    g = np.arange(a, b) - off  # first-order map rec -> mp3, refined by drift
    g = np.round(g - slope * (g / SR)).astype(np.int64)
    pc = np.zeros((b - a, 2), dtype=np.float32)
    if info["once"]:
        ok = (g >= 0) & (g < n)
        pc[ok] = m[g[ok]]
    else:
        ok = g >= 0
        pc[ok] = m[g[ok] % n]
    rm = lambda x: float(np.sqrt(np.mean(x.astype(np.float64) ** 2)))
    gain = rm(pc) / max(rm(disc), 1e-9)
    disc = disc * gain
    peak = max(np.abs(pc).max(), np.abs(disc).max())
    s = min(1.0, 10 ** (-1 / 20) / peak)
    os.makedirs(OUT, exist_ok=True)
    write(OUT + "/%03d_mp3.wav" % track, pc * s)
    write(OUT + "/%03d_disc.wav" % track, disc * s)
    meta = dict(track=track, window_rec_s=[w0, w1], window_len_s=w1 - w0, disc_gain_db=20 * np.log10(gain),
                common_scale_db=20 * np.log10(s), drift_slope_samples_per_s=slope, offset_used=int(off),
                pc_wraps_in_window=[int(x) for x in np.nonzero(np.diff(g % n) < 0)[0]] if not info["once"] else [],
                pc_end_in_window=bool(info["once"] and (g >= n).any()))
    if not info["once"]:
        # the seam alone: the last 8 s of the file, then the first 8 s again, as the decoder rewinds
        seam = np.concatenate([m[n - 8 * SR:], m[:8 * SR]])
        write(OUT + "/%03d_seam.wav" % track, seam * s)
        meta["seam_at_s"] = 8.0
    json.dump(meta, open(OUT + "/%03d_meta.json" % track, "w"), indent=1)
    print(meta)


if __name__ == "__main__":
    main()
