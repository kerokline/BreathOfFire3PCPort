#!/usr/bin/env python
"""Every loop table row's period against the sequence's own (docs/music-open-ends.md 3).

    python tools/bgm/seq_periods.py --synth <host build>/synth_render --cache <cache> [--json OUT] [--songs 3,64]

measure_loops.py measures each row's period P on a Mednafen render by envelope
and waveform; the sequence itself says what the period is. Here each looping
song is played by psx::MusicSynth (synth_render --trace, the player of
libsnd-reading.md, whose loop periods equal the renders' to the VSync on all
156 songs, 9.1) far enough for three loop jumps; the passes' lengths are the
jumps' differences in VSyncs, times the VSync's 737.137 samples (seq.h's
tick, synth_check.TICK). libsnd's integer tick makes consecutive passes differ
by one VSync on some songs (4212 / 4213 on song 23), so a row agrees when its
P is within one VSync and four samples of a pass length. Prints each row that
does not, with its case and whether the engine's table ships it, and writes
every song's pass lengths to --json (default bgm_paths.SEQ_PERIODS,
analysis/bgm/seq_periods.json), which measure_loops.py pins each period to
and gates on. Reads analysis/bgm/loops.json (bgm_paths.LOOPS_JSON).
"""
import argparse, json, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bgm_paths import LOOPS_JSON, SEQ_PERIODS  # noqa: E402
from synth_check import TICK      # noqa: E402

SLACK = TICK + 4


def passes(synth, cache, song, seconds):
    out = os.path.join(os.environ.get("TEMP", "/tmp"), "seq_periods_%03d.wav" % song)
    r = subprocess.run([synth, "--cache", cache, "--song", str(song), "--out", out, "--seconds", "%.0f" % seconds,
                        "--trace"], capture_output=True, text=True, check=True)
    os.remove(out)
    jumps = [int(l.split()[-1]) for l in r.stderr.splitlines() if l.startswith("loop jump")]
    return jumps


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--synth", required=True)
    ap.add_argument("--cache", required=True)
    ap.add_argument("--json", default=SEQ_PERIODS)
    ap.add_argument("--songs", default=None)
    a = ap.parse_args()
    table = json.load(open(LOOPS_JSON))
    songs = sorted(int(k) for k in table) if not a.songs else [int(s) for s in a.songs.split(",")]
    rows, bad = {}, 0
    for song in songs:
        r = table[str(song)]
        body = max(r.get("body_s") or 0, r["render_loop"].get("nominal_body_s") or 0)
        secs = min(1500.0, 60.0 + 3.3 * body)
        jumps = passes(a.synth, a.cache, song, secs)
        while len(jumps) < 3 and secs < 1500:
            secs = min(1500.0, secs * 2)
            jumps = passes(a.synth, a.cache, song, secs)
        lens = sorted({j - i for i, j in zip(jumps, jumps[1:])})
        P = r["render_loop"]["period"]
        near = min(lens, key=lambda v: abs(v * TICK - P)) if lens else None
        delta = P - near * TICK if near is not None else None
        ok = delta is not None and abs(delta) <= SLACK
        ships = not r.get("excluded")
        rows[song] = dict(case=r["case"], ships=ships, period=P, pass_vsyncs=lens, delta_samples=delta,
                          delta_vsyncs=delta / TICK if delta is not None else None, agrees=ok, jumps=jumps[:4])
        if not ok:
            bad += 1
            print("%3d  %-9s %-7s row %9d  sequence %s VSyncs (%s samples)  off by %+.0f samples (%+.2f VSyncs)"
                  % (song, r["case"], "SHIPS" if ships else "refused", P, "/".join(map(str, lens)),
                     "/".join("%.0f" % (v * TICK) for v in lens), delta if delta is not None else float("nan"),
                     delta / TICK if delta is not None else float("nan")), flush=True)
    shipped = [s for s, x in rows.items() if x["ships"]]
    print("%d rows: %d agree with the sequence within one VSync; %d do not (%d of the %d the table ships)"
          % (len(rows), len(rows) - bad, bad, sum(1 for s in shipped if not rows[s]["agrees"]), len(shipped)))
    if a.json:
        json.dump(rows, open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
