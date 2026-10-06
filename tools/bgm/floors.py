"""Noise floor of every MP3: RMS of the lead-in before the first sound, and the quietest 50 ms
frame inside the file (after the first sound). Also the same for the three Mednafen renders.
Writes analysis/bgm/floors.json."""
import json
import numpy as np
from loops import decode, SR

from bgm_paths import PC
inv = json.load(open(PC + "/analysis/bgm/inventory.json"))
out = {}
fr = int(0.05 * SR)
for n, info in sorted(inv["pc"].items(), key=lambda kv: int(kv[0])):
    x = decode(PC + "/bof3/BGM/" + info["file"])
    a = np.abs(x).max(axis=1)
    first = int(np.nonzero(a > 1e-3)[0][0])
    lead = x[:max(first - 64, 1)]
    m = x.mean(axis=1)
    k = (len(m) - first) // fr
    fr_db = np.array([20 * np.log10(np.sqrt(np.mean(m[first + i * fr:first + (i + 1) * fr] ** 2)) + 1e-12) for i in range(k)])
    nz = np.nonzero(x[:first])[0]
    out[n] = dict(first_sound=first, lead_rms_db=float(20 * np.log10(np.sqrt(np.mean(lead ** 2)) + 1e-12)),
                  lead_exact_zero_fraction=float(np.mean(x[:first] == 0)) if first else None,
                  quietest_frame_db=float(fr_db.min()), p1_frame_db=float(np.percentile(fr_db, 1)))
vals = [v["quietest_frame_db"] for v in out.values()]
print("quietest interior frame dB: min %.1f median %.1f max %.1f" % (min(vals), np.median(vals), max(vals)))
print("lead rms dB median %.1f; lead exactly zero fraction median %.3f" % (
    np.median([v["lead_rms_db"] for v in out.values()]), np.median([v["lead_exact_zero_fraction"] for v in out.values()])))
print("files whose quietest frame is below -80 dB:", sum(1 for v in vals if v < -80), "below -60:", sum(1 for v in vals if v < -60))
json.dump(out, open(PC + "/analysis/bgm/floors.json", "w"), indent=1)
