"""The music loop table (docs/bgm-comparison.md section 11): for every looping PC
track, (loop start, loop end) in samples of the PC's own MP3, measured against
a Mednafen render of the disc's sequence.

    python measure_loops.py run [--workers N] [--songs 0,153,...]
        renders every looping song not yet in analysis/bgm/loops.json (or
        whose render is missing), measures it, adds its row; resumable - a
        song with a row is skipped, a render already on disk is reused.
        Progress: analysis/bgm/measure.log. Needs BGM_SCRATCH (disc copies,
        about 480 MB a worker, and Mednafen's base directories).
    python measure_loops.py measure TRACK RENDER.wav [T0]
        measures one song from a render already made (T0: seconds before
        which the recording holds no song, default 40).

The method (section 7): align the MP3 to the render on the song's first sound
and refine on the waveform, fitting the linear clock drift; the render's loop
period P is where it repeats itself, its loop start S the first point from
which it repeats (0.25 s windows correlating >= 0.97 for 3 s on). Both are
mapped into the MP3's samples. Then one of three cases:

  full     the MP3 holds a whole body after S: start = S, end = S + P, P
           refined inside the file (its own repeat), confidence = that
           correlation.
  shifted  the MP3 ends before S + P but holds a whole period from its
           start (n - P >= the first sound): end = the last whole frame
           boundary before the file's end, start = end - P - the loop is
           phase-correct, its first stretch is intro material standing in for
           the body's missing tail; confidence = the render's own correlation
           of that intro stretch against the body's tail it replaces.
  shortened the MP3 is shorter than one period: no phase-correct loop exists
           in the file. start is searched for where the MP3's material best
           matches the render's true continuation after the file's end
           (2 s windows); the loop is shorter than the disc's by the reported
           amount; confidence = that correlation.

Rows under 0.8 confidence are kept in loops.json with "excluded": true and
are not written into the engine's table (tools/bgm/gen_loop_table.py).
"""
import json, os, subprocess, sys, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bgm_paths import PC, SCRATCH
from wavread import read as wavread
from loops import decode, ncc_search, SR

OUT = PC + "/analysis/bgm"
TABLE = OUT + "/loops.json"
LOG = OUT + "/measure.log"
RENDERS = OUT + "/renders"
FRAME = 1152
MIN_CONFIDENCE = 0.8
FADE = 256  # samples (5.8 ms): the crossfade of a shifted row, whose stand-in is not the body's own tail


def log(msg):
    line = time.strftime("%Y-%m-%d %H:%M:%S ") + msg
    print(line, flush=True)
    with open(LOG, "a") as f:
        f.write(line + "\n")


def nccv(u, v):
    u = np.asarray(u, np.float64); v = np.asarray(v, np.float64)
    d = np.sqrt(np.dot(u, u) * np.dot(v, v))
    return float(np.dot(u, v) / d) if d > 1e-12 else 0.0


def first_sound(x, after=0, thr=1e-3):
    a = np.abs(x).max(axis=1) if x.ndim == 2 else np.abs(x)
    i = np.nonzero(a[after:] > thr)[0]
    return after + int(i[0]) if len(i) else None


def best_lag(ref, sig):
    c = ncc_search(ref, sig)
    k = int(np.argmax(c))
    return k, float(c[k])


def measure(track, recpath, t0=40.0):
    inv = json.load(open(OUT + "/inventory.json"))
    info = inv["pc"][str(track)]
    mp3 = decode(PC + "/bof3/BGM/" + info["file"])
    rec, sr = wavread(recpath)
    assert sr == SR
    mm, rm = mp3.mean(axis=1), rec.mean(axis=1)
    n = len(mm)
    row = dict(track=track, file=info["file"], bytes=info["bytes"], frames=info["frames"], samples=n,
               render=os.path.basename(recpath))

    # 1. alignment: first sounds, refined on the waveform, then the drift line
    fm = first_sound(mp3)
    fr = first_sound(rec, int(t0 * SR))
    off = fr - fm  # rec = mp3 + off (+ drift)
    pts = []
    for t in range(fm + SR // 2, n - 3 * SR, SR):
        w = mm[t:t + 2 * SR]
        if np.sqrt(np.mean(w ** 2)) < 1e-3:
            continue
        lo = t + off - SR // 10
        if lo < 0 or lo + 2 * SR + SR // 5 > len(rm):
            break
        k, c = best_lag(w, rm[lo:lo + 2 * SR + SR // 5])
        if c > 0.5:
            pts.append((t, lo + k - t - off, c))
    if len(pts) < 3:
        row.update(excluded=True, why="alignment: fewer than 3 confident windows", confidence=0.0)
        return row
    p = np.array(pts, np.float64)
    sl = [(p[j, 1] - p[i, 1]) / (p[j, 0] - p[i, 0]) for i in range(len(p)) for j in range(i + 1, len(p))]
    slope = float(np.median(sl))  # rec samples of offset per mp3 sample
    icpt = float(np.median(p[:, 1] - slope * p[:, 0]))
    resid = p[:, 1] - (icpt + slope * p[:, 0])
    row["align"] = dict(offset=int(off), intercept=icpt, ppm=slope * 1e6, windows=len(p),
                        median_ncc=float(np.median(p[:, 2])), max_residual=float(np.abs(resid).max()))
    to_rec = lambda m: m + off + icpt + slope * m
    to_mp3 = lambda r: (r - off - icpt) / (1 + slope)

    # 2. the render's loop: period and start
    tab = [t for t in inv["table"] if t["song"] == track]
    if tab:
        sub = inv["disc"][tab[0]["file"]]["subs"][tab[0]["sub"]]
    else:  # 165: the battle bundles' sub 1
        sub = inv["disc"]["BIN/BGM/BGMBAT00.EMI"]["subs"][1]
    ls = [l for l in sub["loops"] if l["kind"] == "start"]
    le = [l for l in sub["loops"] if l["kind"] == "end"]
    if not ls or not le:
        row.update(excluded=True, why="no loop markers in the sequence", confidence=0.0)
        return row
    a_nom, b_nom = ls[0]["seconds"], le[0]["seconds"]
    body = b_nom - a_nom
    s0 = fr
    tref = int(s0 + (a_nom + min(2.0, 0.1 * body)) * SR)
    W = int(min(3 * SR, 0.3 * body * SR))
    lo = int(tref + 0.9 * body * SR); hi = int(tref + 1.1 * body * SR) + W
    if hi > len(rm):
        row.update(excluded=True, why="render too short for one period", confidence=0.0)
        return row
    k, cp = best_lag(rm[tref:tref + W], rm[lo:hi])
    P = lo + k - tref
    # loop start: first t from which 0.25 s windows repeat at P (>= 0.97) for 3 s
    q = SR // 4
    sims = []
    for t in range(int(s0) - SR // 2, tref + W, q // 4):
        u, v = rm[t:t + q], rm[t + P:t + P + q]
        sims.append((t, nccv(u, v), np.sqrt(np.mean(u ** 2))))
    S = None
    span = 3 * SR // (q // 4)
    for i in range(len(sims) - span):
        if all(s >= 0.97 or e < 1e-3 for _, s, e in sims[i:i + span]):
            S = sims[i][0]
            break
    if S is None:
        row.update(excluded=True, why="no exact repeat found in the render", confidence=cp)
        return row
    row["render_loop"] = dict(period=int(P), period_s=P / SR, ncc=cp, start_rec=int(S),
                              intro_s=(S - s0) / SR, nominal_intro_s=a_nom, nominal_body_s=body,
                              timing_ratio=(P / SR) / body)
    L = to_mp3(S)
    Pm = P / (1 + slope)
    E = L + Pm
    lead = fm
    row["body"] = int(round(Pm))
    row["body_s"] = Pm / SR
    last = (n // FRAME) * FRAME - FRAME  # keep off the file's last frame
    if E <= last:
        # full: refine P inside the file, around the clock-derived value
        Li = int(round(L)); Wn = SR
        lo2 = Li + int(round(Pm)) - 300
        if lo2 + Wn + 600 <= n:
            k2, c2 = best_lag(mm[Li:Li + Wn], mm[lo2:lo2 + Wn + 600])
            Pi = lo2 + k2 - Li
        else:
            Pi, c2 = int(round(Pm)), 0.0
        row.update(case="full", start=Li, end=Li + Pi, short_by=0, holds_whole_body=True,
                   confidence=c2, passes_after_start=(n - Li) / Pi)
    elif last - Pm >= lead:
        # the end as late as the file allows with FADE samples after it inside the same frame (the
        # engine crossfades the body's last FADE samples into the stand-in), and the start with
        # FADE samples after it inside its frame as well
        Pr = int(round(Pm))
        Ei = n - FADE
        while (Ei - Pr) % FRAME > FRAME - FADE or Ei % FRAME > FRAME - FADE or Ei % FRAME == 0:
            Ei -= 1
        Li = Ei - Pr
        # refine on the pre-context inside the file: mp3[E-w:E] against mp3[L-w:L]
        w = SR // 2
        c2 = 0.0
        if Li - w - 200 >= 0:
            k2, c2 = best_lag(mm[Ei - w:Ei], mm[Li - w - 200:Li + 200])
            if c2 > 0.9:
                Li = Li - w - 200 + k2 + w
        fade = FADE if Li % FRAME <= FRAME - FADE else 0
        # confidence: the render's intro stretch against the body's tail it stands in for
        rs = int(round(to_rec(Li))); re_ = int(round(S))
        seg = max(q, re_ - rs)
        conf = nccv(rm[rs:rs + seg], rm[rs + P:rs + P + seg]) if re_ > rs else 1.0
        row.update(case="shifted", start=Li, end=Ei, short_by=int(round(E - n)), holds_whole_body=False,
                   confidence=cp, stand_in_ncc=conf, refine_ncc=c2, fade=fade,
                   stand_in_s=(S - rs) / SR)
    else:
        Ei = last
        er = int(round(to_rec(Ei)))
        ref = rm[er:er + 2 * SR]
        # one FFT search over the MP3 for the continuation
        k3, c3 = best_lag(ref, mm[lead:Ei])
        Li = lead + k3
        row.update(case="shortened", start=Li, end=Ei, short_by=int(round(E - n)), holds_whole_body=False,
                   confidence=cp, best_continuation_ncc=c3, loop_shorter_by_s=(Pm - (Ei - Li)) / SR,
                   excluded=True, why="the file is shorter than one loop period from its start: no phase-correct "
                   "loop exists in it; it rewinds as the original does")
        return row
    a = row["align"]
    bad = []
    if row["confidence"] < MIN_CONFIDENCE:
        bad.append("loop correlation %.3f under %.2f" % (row["confidence"], MIN_CONFIDENCE))
    if a["windows"] < 10 or a["max_residual"] > 50:
        bad.append("alignment: %d windows, residual %.1f samples" % (a["windows"], a["max_residual"]))
    row["excluded"] = bool(bad)
    if bad:
        row["why"] = "; ".join(bad)
    return row


def measure_infile(track):
    """A file that repeats its loop body inside itself (mp3_scan.json): the start and period
    measured in the MP3 alone - the same repeat test as the render's, on the file."""
    inv = json.load(open(OUT + "/inventory.json"))
    scan = json.load(open(OUT + "/mp3_scan.json"))[str(track)]
    info = inv["pc"][str(track)]
    mm = decode(PC + "/bof3/BGM/" + info["file"]).mean(axis=1)
    n = len(mm)
    row = dict(track=track, file=info["file"], bytes=info["bytes"], frames=info["frames"], samples=n,
               method="in-file")
    lp = scan.get("loop") or {}
    if not lp.get("period"):
        row.update(excluded=True, why="no repeat inside the file", confidence=0.0)
        return row
    body = lp["nominal_body_s"] * SR
    ratio = lp["period"] / body
    k = round(ratio)
    if k < 1 or abs(ratio / k - 1) > 0.05:
        row.update(excluded=True, why="in-file period %.2f s is not a whole number of bodies (%.2f s)" % (
            lp["period"] / SR, body / SR), confidence=lp["peak"])
        return row
    # refine the period on the waveform, then the earliest point the file repeats from
    t0 = lp["t0"]; W = SR
    lo = t0 + lp["period"] - 300
    kk, cp = best_lag(mm[t0:t0 + W], mm[lo:lo + W + 600])
    P = lo + kk - t0
    q = SR // 4
    span = 3 * SR // (q // 4)
    lead = first_sound(mm.reshape(-1, 1))
    sims = []
    for t in range(lead, min(t0 + W + 4 * SR, n - P - q), q // 4):
        u, v = mm[t:t + q], mm[t + P:t + P + q]
        sims.append((t, nccv(u, v), np.sqrt(np.mean(u ** 2))))
    S = None
    for i in range(len(sims) - span):
        # 0.95, not the render's 0.97: two passes of an MP3 carry different coding noise
        if all(sv >= 0.95 or e < 1e-3 for _, sv, e in sims[i:i + span]):
            S = sims[i][0]
            break
    last = (n // FRAME) * FRAME - FRAME
    if S is None or S + P > last:
        row.update(excluded=True, why="no start from which a whole period fits in the file", confidence=cp)
        return row
    row.update(case="full", start=int(S), end=int(S + P), body=int(P), body_s=P / SR, bodies=k,
               short_by=0, holds_whole_body=True, confidence=cp, passes_after_start=(n - S) / P,
               nominal_intro_s=lp.get("nominal_start_s"))
    row["excluded"] = bool(cp < MIN_CONFIDENCE)
    if row["excluded"]:
        row["why"] = "period correlation %.3f under %.2f" % (cp, MIN_CONFIDENCE)
    return row


def load_table():
    return json.load(open(TABLE)) if os.path.exists(TABLE) else {}


FULL_FADE = 128  # samples (2.9 ms): a full row's crossfade, the same music on both sides


def fit_fade(row):
    """A full row gets a short crossfade across its seam: two passes of an MP3 carry different
    coding noise, which can leave a small step at a hard cut (loop_proof.json: up to 8 times the
    file's own step at that point). The engine takes the faded samples from the frames holding the
    end and the start, so the loop window moves forward by the fewest samples (< 1,152) that put
    both points at least FULL_FADE samples before their frames' ends; the music repeats across the
    window, so moving it changes nothing else."""
    if row.get("excluded") or row.get("case") != "full" or row.get("fade"):
        return row
    last = (row["samples"] // FRAME) * FRAME - FRAME
    for d in range(FRAME):
        s, e = row["start"] + d, row["end"] + d
        if e + FULL_FADE > last:
            break
        if 0 < e % FRAME <= FRAME - FULL_FADE and s % FRAME <= FRAME - FULL_FADE:
            row.update(start=s, end=e, fade=FULL_FADE, moved_for_fade=d)
            return row
    row["fade"] = 0
    return row


def save_row(row):
    row = fit_fade(row)
    t = load_table()
    t[str(row["track"])] = row
    json.dump(t, open(TABLE, "w"), indent=1, default=float)


def looping_songs():
    inv = json.load(open(OUT + "/inventory.json"))
    return sorted(int(k) for k, v in inv["pc"].items() if not v["once"] and int(k) != 166)


def render_plan(track, inv):
    """(file id to load, song id to play, seconds) for the title patch of patch_disc.py."""
    if track == 165:
        fid = [t for t in inv["table"] if t["file"].endswith("BGMBAT00.EMI")][0]["file_id"]
        song = [t for t in inv["table"] if t["sub"] == 1][0]["song"]  # any song whose table sub is 1
        sub = inv["disc"]["BIN/BGM/BGMBAT00.EMI"]["subs"][1]
    else:
        t = [t for t in inv["table"] if t["song"] == track][0]
        fid, song, sub = t["file_id"], track, inv["disc"][t["file"]]["subs"][t["sub"]]
    secs = 44 + 1.05 * sub["seconds"] + 0.3 * sub["seconds"] + 15
    return fid, song, int(secs)


def run(workers, songs):
    import concurrent.futures as cf
    inv = json.load(open(OUT + "/inventory.json"))
    done = load_table()
    # a render-measured row is final; an in-file row is replaced by the render's measurement
    todo = [s for s in songs if str(s) not in done or done[str(s)].get("method") == "in-file"]
    log("run: %d songs to measure (%d already in loops.json), %d workers" % (len(todo), len(songs) - len(todo), workers))
    here = os.path.dirname(os.path.abspath(__file__))
    os.makedirs(RENDERS, exist_ok=True)

    def one(widx, track):
        out = RENDERS + "/song%03d_mednafen.wav" % track
        fid, song, secs = render_plan(track, inv)
        if not os.path.exists(out):
            disc = SCRATCH + "/w%d" % widx
            subprocess.run([sys.executable, here + "/patch_disc.py", str(song), disc, hex(fid)], check=True,
                           capture_output=True)
            env = dict(os.environ, BGM_SCRATCH=SCRATCH + "/home%d" % widx)
            subprocess.run(["bash", here + "/mrun.sh", str(secs), out, disc + "/bof3jp.cue"], env=env,
                           capture_output=True)
        return measure(track, out)

    queue = list(todo)
    with cf.ThreadPoolExecutor(workers) as ex:
        free = list(range(workers))
        futs = {}
        while queue or futs:
            while queue and free:
                w = free.pop(); s = queue.pop(0)
                futs[ex.submit(one, w, s)] = (w, s)
            fin, _ = cf.wait(list(futs), return_when=cf.FIRST_COMPLETED)
            for f in fin:
                w, s = futs.pop(f); free.append(w)
                try:
                    row = f.result()
                    save_row(row)
                    log("song %03d: %s conf %.3f%s" % (s, row.get("case", "-"), row.get("confidence", 0),
                                                      " EXCLUDED (%s)" % row.get("why") if row.get("excluded") else ""))
                except Exception as e:
                    log("song %03d: FAILED %r" % (s, e))
    log("run: done")


def main():
    if sys.argv[1] == "measure":
        row = measure(int(sys.argv[2]), sys.argv[3], float(sys.argv[4]) if len(sys.argv) > 4 else 40.0)
        save_row(row)
        log("song %03d: %s" % (row["track"], json.dumps(row, default=float)))
    elif sys.argv[1] == "infile":
        tracks = [int(t) for t in sys.argv[2].split(",")]
        table = load_table()
        for t in tracks:
            if str(t) in table and table[str(t)].get("method") != "in-file":
                log("song %03d: has a render-measured row; in-file measure skipped" % t)
                continue
            row = measure_infile(t)
            save_row(row)
            log("song %03d: in-file %s conf %.3f%s" % (t, row.get("case", "-"), row.get("confidence", 0),
                                                       " EXCLUDED (%s)" % row.get("why") if row.get("excluded") else ""))
    elif sys.argv[1] == "run":
        args = sys.argv[2:]
        workers = int(args[args.index("--workers") + 1]) if "--workers" in args else 3
        songs = [int(s) for s in args[args.index("--songs") + 1].split(",")] if "--songs" in args else looping_songs()
        run(workers, songs)


if __name__ == "__main__":
    main()
