"""MP3 pairs made from identical sequence and bank data: how alike are the two files' PCM?
A sample-exact match after alignment means a deterministic (digital) source; a residual near
the noise floor that is not zero means two captures. Writes analysis/bgm/twins.json."""
import json
import numpy as np
from loops import decode, ncc_search, SR

from bgm_paths import PC
inv = json.load(open(PC + "/analysis/bgm/inventory.json"))
out = []
for a, b in ((151, 153), (23, 52), (54, 71), (115, 118)):
    x = decode(PC + "/bof3/BGM/" + inv["pc"][str(a)]["file"])
    y = decode(PC + "/bof3/BGM/" + inv["pc"][str(b)]["file"])
    xm, ym = x.mean(axis=1), y.mean(axis=1)
    t = int(5 * SR); W = int(3 * SR)
    c = ncc_search(xm[t:t + W], ym[:t + W + 2 * SR]); k = int(np.argmax(c)); lag = k - t
    n = min(len(x), len(y) - lag) if lag >= 0 else min(len(x) + lag, len(y))
    xs = x[max(0, -lag):max(0, -lag) + n]; ys = y[max(0, lag):max(0, lag) + n]
    n = min(len(xs), len(ys)); xs, ys = xs[:n], ys[:n]
    res = ys - xs
    rr = 20 * np.log10(np.sqrt(np.mean(res ** 2)) / np.sqrt(np.mean(xs ** 2)))
    # residual after a best gain too
    g = np.sum(xs * ys) / np.sum(xs * xs)
    rg = 20 * np.log10(np.sqrt(np.mean((ys - g * xs) ** 2)) / np.sqrt(np.mean(xs ** 2)))
    # drift: lag at the end
    t2 = n - 4 * SR
    c2 = ncc_search(xm[max(0, -lag) + t2:max(0, -lag) + t2 + W], ym[max(0, lag) + t2 - SR // 10:max(0, lag) + t2 + W + SR // 10])
    k2 = int(np.argmax(c2)) - SR // 10
    row = dict(a=a, b=b, lag_samples=int(lag), ncc=float(c[k]), overlap_s=n / SR, residual_db=float(rr),
               residual_after_gain_db=float(rg), gain=float(g), lag_change_at_end=int(k2),
               identical_samples=float(np.mean(np.abs(res) < 1e-6)))
    out.append(row); print(row)
json.dump(out, open(PC + "/analysis/bgm/twins.json", "w"), indent=1)
