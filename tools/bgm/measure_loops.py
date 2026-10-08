"""The music loop table (docs/bgm-comparison.md section 11): for every looping PC
track, (loop start, loop end) in samples of the PC's own MP3, measured against
a Mednafen render of the disc's sequence.

    python measure_loops.py run [--workers N] [--songs 0,153,...] [--redo]
        renders every looping song not yet in analysis/bgm/loops.json (or
        whose render is missing), measures it, adds its row; resumable - a
        song with a row is skipped, a render already on disk is reused.
        --redo measures every song with a render on disk again, excluded
        or not; the log line carries the period, to diff against the last run.
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
import collections, json, os, subprocess, sys, time
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
# The PC renders' clock against Mednafen's, in rec samples per mp3 sample: -173..-177 ppm on every
# song the waveform aligns (003, 011, 017, 025 on 2026-10-08; 000 and 153 on 10-06). The envelope
# fallback fits its intercept with the slope pinned here - its few hop-resolution points cannot
# fit a slope (song 034: five points gave -1542).
DRIFT_PPM = -175.0
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


def match_windows(x, t0, t1, P, step):
    """[(t, best ncc of x[t:t+q] against x[t+P+d:...] for d in -3..3, rms)] for t in t0..t1 by step:
    the render's second pass against its first. A sequencer loop is not an integer number of SPU
    samples, hence the +-3; the SPU's noise voices are a fresh LFSR realisation every pass, so
    a window holding a noise hit does not match at all - the callers judge spans by the fraction
    of windows that match (>= 0.97), never by their minimum (2026-10-08: 89 of 153 songs refused
    on the minimum; song 011 matches in a third of its windows at its true period)."""
    q = SR // 4
    out = []
    for t in range(int(t0), int(min(t1, len(x) - P - q - 4)), step):
        u = x[t:t + q]
        best = max(nccv(u, x[t + P + d:t + P + d + q]) for d in range(-3, 4))
        out.append((t, best, float(np.sqrt(np.mean(u ** 2)))))
    return out


def match_fraction(sims):
    act = [s >= 0.97 for _, s, e in sims if e >= 1e-3]
    return float(np.mean(act)) if act else 0.0


def envelope(x, hop=256):
    """log-RMS per hop, first-differenced and half-wave rectified: an onset envelope, the same for
    two renders of one sequence whose voices differ in phase (song 034: spectrogram frames 0.94
    alike, 2 s waveform windows under 0.5)."""
    n = len(x) // hop
    e = np.log1p(np.sqrt(np.mean(x[:n * hop].reshape(n, hop) ** 2, axis=1)) * 1000)
    d = np.maximum(np.diff(e, prepend=e[0]), 0)
    return np.convolve(d, np.ones(3) / 3, mode="same")


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
    # the song's first note sits at 43.6-43.8 s in every render of 2026-10-08 (Mednafen's boot, the
    # Sony and Capcom intros, the title); a render whose first sound after t0 is far from that has
    # the guard inside the intros or a silent title - the log line shows it
    row["first_sound_s"] = m_first = (first_sound(rec, int(t0 * SR)) or 0) / SR

    # 1. alignment: first sounds, refined on the waveform, then the drift line
    fm = first_sound(mp3)
    fr = first_sound(rec, int(t0 * SR))
    off = fr - fm  # rec = mp3 + off (+ drift)
    pts, best_c = [], 0.0
    for t in range(fm + SR // 2, n - 3 * SR, SR):
        w = mm[t:t + 2 * SR]
        if np.sqrt(np.mean(w ** 2)) < 1e-3:
            continue
        lo = t + off - SR // 10
        if lo < 0 or lo + 2 * SR + SR // 5 > len(rm):
            break
        k, c = best_lag(w, rm[lo:lo + 2 * SR + SR // 5])
        best_c = max(best_c, c)
        if c > 0.5:
            pts.append((t, lo + k - t - off, c))
    align_method = "waveform"
    if len(pts) < 10:
        # the envelope fallback, at hop resolution: 4 s windows of the MP3's onset envelope
        # against the render's, +-0.25 s about the first-sound offset
        hop = 256
        em, er = envelope(mm, hop), envelope(rm, hop)
        pts2 = []
        for t in range(fm + SR // 2, n - 5 * SR, SR):
            a, b = t // hop, (t + 4 * SR) // hop
            if em[a:b].max() < 1e-3:
                continue
            lo = (t + off) // hop - SR // 4 // hop
            if lo < 0 or lo + (b - a) + SR // 2 // hop > len(er):
                break
            k, c = best_lag(em[a:b], er[lo:lo + (b - a) + SR // 2 // hop])
            if c > 0.3:     # onset envelopes are spiky and correlate low even when right (song 034:
                pts2.append((t, (lo + k) * hop - t - off, c))   # 0.44 with lags agreeing to a hop); the line fit guards
        if len(pts2) > len(pts):
            pts, align_method = pts2, "envelope"
    if len(pts) < 3:
        row.update(excluded=True, why="alignment: fewer than 3 confident windows (best window %.3f)" % best_c,
                   confidence=0.0)
        return row
    p = np.array(pts, np.float64)

    def fit(p):
        if align_method == "envelope":
            slope = DRIFT_PPM * 1e-6
        else:
            sl = [(p[j, 1] - p[i, 1]) / (p[j, 0] - p[i, 0]) for i in range(len(p)) for j in range(i + 1, len(p))]
            slope = float(np.median(sl))  # rec samples of offset per mp3 sample
        icpt = float(np.median(p[:, 1] - slope * p[:, 0]))
        return slope, icpt, p[:, 1] - (icpt + slope * p[:, 0])
    slope, icpt, resid = fit(p)
    # A window can lock a bar away in self-similar music (the lag search spans +-SR/10): those are
    # outliers of thousands of samples against a fit whose residuals are otherwise under ten. Drop
    # them, refit, and gate on the inliers; the count of outliers is reported.
    tol = 50 if align_method == "waveform" else 2 * 256
    inl = np.abs(resid) <= tol
    outliers = int((~inl).sum())
    if inl.sum() >= 3 and outliers:
        p = p[inl]
        slope, icpt, resid = fit(p)
    row["align"] = dict(offset=int(off), intercept=icpt, ppm=slope * 1e6, windows=len(p), outliers=outliers,
                        median_ncc=float(np.median(p[:, 2])), max_residual=float(np.abs(resid).max()),
                        method=align_method)
    align = row["align"]
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
    # The period and the loop start are found on the ONSET ENVELOPE, which the sequencer repeats
    # exactly every pass, and only then made sample-exact on the waveform. The waveform alone
    # cannot be trusted for it: the SPU's noise voices are a fresh LFSR realisation each pass -
    # as isolated hits (011: 0.99 between the beats, ~0 on them) or as a voice sounding throughout
    # (007: 0.3..0.7 wherever it plays, 1.000 where it rests; 089 everywhere) - and a phrase repeat
    # can outscore the loop in a single window (017: 48.14 s at 1.000 against the loop's 53.49 s).
    # An intro against the body, or a phrase against what follows it, has a different envelope.
    HOP = 256
    env = envelope(rm, HOP)
    eq = SR // HOP                     # envelope samples per second
    e_t, e_W = tref // HOP, 12 * eq    # 12 s of envelope from tref
    e_lo, e_hi = int(lo // HOP), min(int(hi // HOP) + e_W, len(env))
    if e_hi - e_lo <= e_W:              # the render ends inside the search: a shorter reference
        e_W = max(3 * eq, e_hi - e_lo - 1)
    ce = ncc_search(env[e_t:e_t + e_W], env[e_lo:e_hi])
    cw = ncc_search(rm[tref:tref + W], rm[lo:hi])
    # candidates: the envelope search's local peaks (hop-exact) and the waveform search's (sample-
    # exact, 0.5 and up); each scored on the envelope at the best of its three nearest hops - the
    # envelope is spiky and a sample-exact lag between hops scores low on it (005: the loop at
    # 56.898 s dropped from the shortlist for a bar multiple at 56.882)
    peaks_e = [i for i in range(1, len(ce) - 1) if ce[i] >= ce[i - 1] and ce[i] >= ce[i + 1] and ce[i] > 0.3]
    cands = {int(i) * HOP for i in sorted(peaks_e, key=lambda i: -ce[i])[:8]}
    cands |= {int(i) for i in range(1, len(cw) - 1) if cw[i] > 0.5 and cw[i] >= cw[i - 1] and cw[i] >= cw[i + 1]}

    def env_score(Pc):     # mean envelope correlation of four 3 s spans from tref, at lag Pc
        a = tref // HOP; n3 = 3 * eq; best = 0.0
        for b in ((tref + Pc) // HOP - 1, (tref + Pc) // HOP, (tref + Pc) // HOP + 1):
            vals = [nccv(env[a + j * n3:a + (j + 1) * n3], env[b + j * n3:b + (j + 1) * n3]) for j in range(4)
                    if b + (j + 1) * n3 <= len(env)]
            best = max(best, float(np.mean(vals)) if vals else 0.0)
        return best
    scored = sorted(((env_score(lo + i - tref), i) for i in cands), reverse=True)
    if scored[0][0] < 0.6:
        row.update(excluded=True, why="no repeat found in the render (best 12 s envelope correlation %.2f)"
                   % scored[0][0], confidence=scored[0][0])
        return row
    # the envelope cannot tell a lag of whole bars from the loop (011: 149.77 s at 0.79 against the
    # loop's 142.63 s at 0.74 - the drum pattern repeats every bar); the waveform's matching windows
    # can. Among the candidates within 0.1 of the best envelope score, the one the most windows match
    short = [(e, i) for e, i in scored if e >= scored[0][0] - 0.1][:4]
    short = [(match_fraction(match_windows(rm, tref, tref + 12 * SR, lo + i - tref, SR // 8)), e, i)
             for e, i in short]
    if max(f for f, _, _ in short) >= 0.05:
        short.sort(reverse=True)
    else:
        # no waveform window matches at any candidate (a noise-dominated song): the envelope's bar
        # ambiguity is broken by the sequence's own body length, good to +-3 % on every song the
        # waveform settled (the timing_ratio of the rows)
        short.sort(key=lambda x: abs((lo + x[2] - tref) / SR - body))
    f0, ep, k = short[0]
    P_env = lo + k - tref
    row["period_candidates"] = [dict(period_s=(lo + i - tref) / SR, env=float(e), fraction=float(f))
                                for f, e, i in short]
    # sample-exact: the lag the matching waveform windows vote for, within the envelope's +-HOP
    votes = collections.Counter()
    q = SR // 4
    for t in range(tref, min(tref + 12 * SR, len(rm) - P_env - 2 * HOP - q), q):
        u = rm[t:t + q]
        if np.sqrt(np.mean(u ** 2)) < 1e-3:
            continue
        c = ncc_search(u, rm[t + P_env - HOP:t + P_env + HOP + q])
        j = int(np.argmax(c))
        if c[j] >= 0.9:
            votes[j - HOP] += 1
    if votes:
        d = votes.most_common(1)[0][0]
        P, exact = P_env + d, int(sum(votes.values()))
    else:                              # no window matches in waveform (089): the envelope's lag
        P, exact = P_env, 0
    sims = match_windows(rm, tref, tref + 12 * SR, P, SR // 8)
    frac = match_fraction(sims)
    # the render's own confidence in the loop: its envelope over 12 s, or the share of waveform
    # windows matching, whichever says more (a clean song 1.0 by the windows; a noisy one by the
    # envelope, 0.8-0.9)
    cp = max(ep, frac)
    row["period_exactness"] = dict(windows_voting=exact, env=ep)
    # the loop start: the first 3 s span from the song's start whose envelope repeats at P (half the
    # best span's, and 0.5 at least); then the first waveform window in it that matches, if any
    # The loop start. An intro with the body's drum pattern repeats on the envelope too (007), so the
    # waveform decides where it can: the first 0.25 s window that matches at P and is followed by
    # 3 s matching at half the song's own rate or better (a chance match in the intro is not
    # followed). Only a song no window of which matches (089 nearly) takes the envelope's first span.
    early = match_windows(rm, int(s0) - SR // 2, tref + W, P, SR // 16)
    sp = 3 * SR // (SR // 16)
    S = None
    for i, (t, s_, e) in enumerate(early):
        if s_ >= 0.97 and match_fraction(early[i:i + sp]) >= max(0.1, 0.5 * frac):
            S = t
            break
    best3 = 0.0
    if S is None:
        a0 = max(0, (int(s0) - SR // 2) // HOP); n3 = 3 * eq; pe = P // HOP
        spans = [(a, nccv(env[a:a + n3], env[a + pe:a + pe + n3]))
                 for a in range(a0, tref // HOP + W // HOP, eq // 4) if a + pe + n3 <= len(env)]
        best3 = max((v for _, v in spans), default=0.0)
        S = next((a * HOP for a, v in spans if v >= max(0.5, 0.5 * best3)), None)
    if S is None:
        row.update(excluded=True, why="no loop start found in the render (best 3 s envelope %.2f)" % best3,
                   confidence=ep)
        return row
    row["match_fraction"] = frac
    row["start_by"] = "waveform" if best3 == 0.0 else "envelope"
    row["render_loop"] = dict(period=int(P), period_s=P / SR, ncc=cp, start_rec=int(S),
                              intro_s=(S - s0) / SR, nominal_intro_s=a_nom, nominal_body_s=body,
                              timing_ratio=(P / SR) / body)
    lead = fm
    # a start found in the window straddling the song's first note (up to 0.25 s before it) maps
    # before the MP3's first sample; the loop starts at the note - any start inside the matching
    # region loops the same audio (song 020: Li negative sliced an empty refinement reference)
    L = max(to_mp3(S), float(lead))
    Pm = P / (1 + slope)
    E = L + Pm
    row["body"] = int(round(Pm))
    row["body_s"] = Pm / SR
    last = (n // FRAME) * FRAME - FRAME  # keep off the file's last frame
    if E <= last:
        # full: refine P inside the file, around the clock-derived value
        Li = int(round(L)); Wn = SR
        r = 300 if align["method"] == "waveform" else 2500      # the envelope's resolution
        lo2 = Li + int(round(Pm)) - r
        if lo2 + Wn + 2 * r <= n:
            k2, c2 = best_lag(mm[Li:Li + Wn], mm[lo2:lo2 + Wn + 2 * r])
            Pi = lo2 + k2 - Li
        else:   # no room to refine inside the file: the render's own measure stands
            Pi, c2 = int(round(Pm)), cp
        # the MP3's own second pass against its first, in waveform (c2) and in onset envelope
        # (c2e): a noise voice that differs between passes lowers the first and not the second,
        # and the loop is right either way - the engine crossfades FADE samples at it
        em = envelope(mm, HOP); a, b = Li // HOP, (Li + Pi) // HOP; n12 = min(12 * eq, len(em) - b - 1)
        c2e = nccv(em[a:a + n12], em[b:b + n12]) if n12 > eq else 0.0
        # the confidence: the MP3's own repeat in waveform or envelope - but where the render's
        # waveform never confirmed the period (no window matched: a bar multiple can still score
        # 0.9 in a 1 s window of repetitive music, 047), the envelope alone speaks
        conf = max(c2, c2e) if exact else c2e
        row.update(case="full", start=Li, end=Li + Pi, short_by=0, holds_whole_body=True,
                   confidence=conf, waveform_ncc=c2, envelope_ncc=c2e, passes_after_start=(n - Li) / Pi)
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
        # near-full: intro + exactly one body, trimmed to a frame - a loop over the whole file from
        # the loop start would slip by short_by samples a pass (017: 18 ms in 53.5 s; 034: 31 ms in
        # 21.9 s). Flagged, still excluded: whether such a slip beats a rewind is the owner's call
        # (DIV-0081), to be taken on the count
        row.update(case="shortened", start=Li, end=Ei, short_by=int(round(E - n)), holds_whole_body=False,
                   confidence=cp, best_continuation_ncc=c3, loop_shorter_by_s=(Pm - (Ei - Li)) / SR,
                   near_full=bool(E - n <= 2 * FRAME + FADE), near_full_start=int(round(L)),
                   excluded=True, why="the file is shorter than one loop period from its start: no phase-correct "
                   "loop exists in it; it rewinds as the original does")
        return row
    a = row["align"]
    bad = []
    if row["confidence"] < MIN_CONFIDENCE:
        bad.append("loop correlation %.3f under %.2f" % (row["confidence"], MIN_CONFIDENCE))
    if a["windows"] < 10 or a["max_residual"] > (50 if a["method"] == "waveform" else 512):
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


def run(workers, songs, redo=False):
    import concurrent.futures as cf
    inv = json.load(open(OUT + "/inventory.json"))
    done = load_table()
    # a render-measured row is final; an in-file row is replaced by the render's measurement; with
    # --redo every song whose render is on disk is measured again (no new render) - after a change
    # to measure(), every row, since a row that passed may have passed at a wrong period (017)
    todo = [s for s in songs if str(s) not in done or done[str(s)].get("method") == "in-file"
            or (redo and os.path.exists(RENDERS + "/song%03d_mednafen.wav" % s))]
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
                    rl = row.get("render_loop", {})
                    log("song %03d: %s conf %.3f P %.3f s intro %.2f s match %.2f align %s first %.1f s%s%s"
                        % (s, row.get("case", "-"), row.get("confidence", 0), rl.get("period_s", 0),
                           rl.get("intro_s", 0), row.get("match_fraction", 0),
                           row.get("align", {}).get("method", "-"), row.get("first_sound_s", 0),
                           " NEAR-FULL (short by %d samples)" % row["short_by"] if row.get("near_full") else "",
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
        run(workers, songs, redo="--redo" in args)


if __name__ == "__main__":
    main()
