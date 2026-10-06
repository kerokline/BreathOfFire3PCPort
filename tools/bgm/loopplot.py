"""Plot of the disc render's self-similarity at its loop period, with the PC MP3's end marked.
loopplot.py TRACK REC.wav  -> analysis/bgm/plots/trackNNN_loop.png"""
import json, sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from wavread import read

from bgm_paths import PC
track, rec = int(sys.argv[1]), sys.argv[2]
c = json.load(open(PC + "/analysis/bgm/compare_%03d.json" % track))
dl = c["disc_loop"]
r, _ = read(rec); rm = r.mean(axis=1); S = 44100
P = int(round(dl["period_s"] * S))
s0 = dl["song_start_rec_s"]
ts, vs = [], []
for t in np.arange(s0 - 1, s0 + dl["period_s"] - 0.25, 0.25):
    a = int(t * S); u = rm[a:a + S // 4]; v = rm[a + P:a + P + S // 4]
    if a + P + S // 4 > len(rm):
        break
    den = np.sqrt(np.dot(u, u) * np.dot(v, v))
    ts.append(t - s0); vs.append(np.dot(u, v) / den if den > 1e-12 else 0)
fig, ax = plt.subplots(figsize=(10, 4))
ax.plot(ts, vs, lw=0.8)
ax.axvline(dl["start_after_song_start_s"], color="g", ls="--", label="disc loop start (repeats exactly from here)")
end_song = dl["mp3_end_rec_s"] - s0
ax.axvline(end_song - dl["period_s"], color="r", ls=":", label="PC MP3 end, one period back")
ax.set_xlabel("seconds after the song's first sound (disc render)")
ax.set_ylabel("NCC of 0.25 s windows, t vs t + period")
ax.set_title("track %03d: disc render vs itself one loop period (%.3f s) later" % (track, dl["period_s"]))
ax.set_ylim(-0.2, 1.05); ax.legend(loc="lower right")
fig.tight_layout(); fig.savefig(PC + "/analysis/bgm/plots/track%03d_loop.png" % track, dpi=90)
print("ok", len(ts))
