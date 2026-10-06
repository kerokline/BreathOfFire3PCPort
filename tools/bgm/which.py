"""Which PC MP3 is playing in a stretch of a recording? Scores every MP3's 8 s windows against it."""
import sys, json
from bgm_paths import PC
import numpy as np
from wavread import read
from loops import decode, SR
from feat import bands, best_lag

rec, t0, t1 = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
inv = json.load(open(PC + "/analysis/bgm/inventory.json"))
r, sr = read(rec); r = r.mean(axis=1)[int(t0 * SR):int(t1 * SR)]
_, Dr = bands(r)
scores = []
for n, info in inv["pc"].items():
    m = decode(PC + "/bof3/BGM/" + info["file"]).mean(axis=1)
    _, Dm = bands(m)
    best = (0, 0, 0)
    for a in range(0, max(1, Dm.shape[1] - 800), 1500):
        if a + 800 > Dm.shape[1] or 800 > Dr.shape[1]:
            break
        k, c, _ = best_lag(Dm[:, a:a + 800], Dr)
        if c > best[0]:
            best = (c, a / 100, t0 + k / 100)
    scores.append((best[0], int(n), best[1], best[2]))
scores.sort(reverse=True)
for s in scores[:6]:
    print("track %03d score %.3f  (mp3 %.1f s at rec %.2f s)" % (s[1], s[0], s[2], s[3]))
