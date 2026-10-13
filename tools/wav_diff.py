#!/usr/bin/env python
"""Where two WAVs differ, second by second - so a listen goes only there.

    python tools/wav_diff.py A.wav B.wav [--out diff.tsv] [--threshold -50] [--window 1.0]

Both files must be 16-bit PCM of one rate and channel count (the BOF3X_WAV_DIR
dumps are 44,100 Hz stereo, docs/wav-dump.md). For each window (a second by
default) the TSV has:

    second      the window's start, in seconds
    rms_a_db    A's level, dBFS (RMS over both channels; -inf printed as -120)
    rms_b_db    B's level
    diff_db     the level of A - B sample by sample: how much is different
    delta_db    rms_a_db - rms_b_db: a level change, blind to small time shifts
    differs     1 when diff_db is above --threshold (default -50 dBFS) and
                within 30 dB of the louder side - a difference you could hear

and the summary line on stdout names the first window that differs, how many
do, and any length difference (the shorter file's end is where the TSV
stops). A one-frame shift of identical audio shows as `differs` from where it
starts with delta_db near 0; that reads "the same music, moved", the cue to
listen for the timing rather than the sound.
"""
import argparse, math, sys, wave

import numpy as np


def read(path):
    with wave.open(path, 'rb') as w:
        if w.getsampwidth() != 2:
            sys.exit(f'{path}: {8 * w.getsampwidth()}-bit, only 16-bit PCM is read')
        rate, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
        data = np.frombuffer(w.readframes(n), dtype='<i2').astype(np.float64) / 32768.0
    return rate, ch, data.reshape(-1, ch)


def db(x):
    return 20 * math.log10(x) if x > 1e-6 else -120.0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('a')
    ap.add_argument('b')
    ap.add_argument('--out', default=None, help='the TSV (default: stdout after the summary)')
    ap.add_argument('--threshold', type=float, default=-50.0, help='diff_db above this is a difference (dBFS)')
    ap.add_argument('--window', type=float, default=1.0, help='seconds a row')
    a = ap.parse_args()

    ra, ca, A = read(a.a)
    rb, cb, B = read(a.b)
    if (ra, ca) != (rb, cb):
        sys.exit(f'{a.a} is {ra} Hz x{ca}, {a.b} is {rb} Hz x{cb}: not comparable')
    n = min(len(A), len(B))
    step = max(1, int(round(a.window * ra)))
    rows, first, count = [], None, 0
    for start in range(0, n, step):
        x, y = A[start:start + step], B[start:start + step]
        if len(x) == 0:
            break
        la, lb = db(float(np.sqrt(np.mean(x * x)))), db(float(np.sqrt(np.mean(y * y))))
        ld = db(float(np.sqrt(np.mean((x - y) ** 2))))
        differs = ld > a.threshold and ld > max(la, lb) - 30.0
        if differs:
            count += 1
            if first is None:
                first = start / ra
        rows.append((start / ra, la, lb, ld, la - lb, int(differs)))

    secs_a, secs_b = len(A) / ra, len(B) / rb
    summary = (f'{a.a}: {secs_a:.2f} s; {a.b}: {secs_b:.2f} s; compared {n / ra:.2f} s in {len(rows)} windows of '
               f'{a.window:g} s; ' + (f'first difference at {first:.1f} s, {count} windows differ'
                                     if first is not None else 'no window differs') +
               (f'; lengths differ by {abs(secs_a - secs_b):.2f} s' if len(A) != len(B) else ''))
    print(summary)
    lines = ['second\trms_a_db\trms_b_db\tdiff_db\tdelta_db\tdiffers']
    lines += [f'{s:.1f}\t{la:.1f}\t{lb:.1f}\t{ld:.1f}\t{dd:+.1f}\t{d}' for s, la, lb, ld, dd, d in rows]
    if a.out:
        with open(a.out, 'w', encoding='utf-8', newline='\n') as f:
            f.write('# ' + summary + '\n')
            f.write('\n'.join(lines) + '\n')
    else:
        print('\n'.join(lines))
    return 0


if __name__ == '__main__':
    sys.exit(main())
