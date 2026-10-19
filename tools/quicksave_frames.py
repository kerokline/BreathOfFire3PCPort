#!/usr/bin/env python3
"""The quicksave's frame check (docs/quicksave.md, the graphics participant): what each frame
DRAWS after a quick-load against what it drew after the quicksave.

A run with BOF3X_FRAMEHASH=1 logs one line a walk of the lists (render_shim.cpp,
LogWalkHash; a present can carry two walks when two logic frames passed
between draws):

    framehash   drawn at Frame_Counter 731: 412 commands, 1650 vertices, 9 textures, 0123456789ABCDEF

the hash over the walk's commands in order - the pipe state, the vertices,
the CONTENT of each texture sampled - tagged with the Frame_Counter the
lists were left at. A proof recipe (quick_*.txt, tools/quicksave_check.py)
presses F6 at a frame and F7 300 frames later and replays the same inputs, so
after the restore the game draws the same lists again: the frame tagged C + k
after the F7 must hash as the frame tagged C + k after the F6.

    python tools/quicksave_frames.py bof3x.log [--show 8]

Reads the snapshot core's lines (`quicksave   F6 at Frame_Counter C`,
`F7 ... back to Frame_Counter C`). The first walks presented after the F7
were drawn before the load (Gpu_PutDispEnv presents the last draw first)
and are left out. Frames a late tick did not draw are missing on one
side or the other and are counted, not compared.

Exit 0: every frame drawn on both sides equal; 1: a difference (the first
named, k frames after the restore); 2: no take, no restore, or no frame hash.
"""
import argparse, re, sys

TAKE = re.compile(r'quicksave\s+F6 at Frame_Counter (\d+): BISLPS10')
LOAD = re.compile(r'quicksave\s+F7 \S+ restored at Frame_Counter (\d+): back to Frame_Counter (\d+)')
HASH = re.compile(r'framehash\s+drawn at Frame_Counter (\d+): (\d+) commands, (\d+) vertices, (\d+) textures, ([0-9A-F]{16})')


def read(path):
    take = load = None
    a, b = {}, {}
    side = None
    skipped_first = False
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = TAKE.search(line)
            if m and take is None:
                take = int(m.group(1))
                side = a
                continue
            m = LOAD.search(line)
            if m and take is not None and load is None:
                load = (int(m.group(1)), int(m.group(2)))
                side = b
                continue
            m = HASH.search(line)
            if not m or side is None:
                continue
            tag = int(m.group(1))
            if side is b and not skipped_first:
                # The walks drawn before the load and presented after it carry
                # the last tags of the window (Frame_Counter L - 1, L - 2):
                # passed over until the restored game's first walk.
                if not (load[1] <= tag < load[0] - 2):
                    continue
                skipped_first = True
            side.setdefault(tag, (m.group(5), int(m.group(2)), int(m.group(3)), int(m.group(4))))
    return take, load, a, b


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('log')
    ap.add_argument('--show', type=int, default=8, help='differing frames listed')
    args = ap.parse_args()
    take, load, a, b = read(args.log)
    if take is None or load is None:
        print('RESULT: no take or no restore in the log')
        return 2
    if not a or not b:
        print('RESULT: no framehash lines (BOF3X_FRAMEHASH=1 not set?)')
        return 2
    back_to = load[1]
    if back_to != take:
        print(f'note: the restore went back to {back_to}, the take was at {take}')
    window = load[0] - back_to
    tags = range(back_to, back_to + window)
    both = [t for t in tags if t in a and t in b]
    only = sum(1 for t in tags if (t in a) != (t in b))
    differ = [t for t in both if a[t][0] != b[t][0]]
    print(f'take at Frame_Counter {take}, restore at {load[0]} back to {back_to}: window {window} frames; '
          f'{len(both)} drawn on both sides, {only} on one side only (late ticks)')
    first = back_to if back_to in a and back_to in b else (both[0] if both else None)
    if first is not None:
        same = a[first][0] == b[first][0]
        print(f'first frame after the restore (drawn at {first}, k={first - back_to}): '
              f'{"EQUAL" if same else "DIFFERS"} - after F6 {a[first][0]} ({a[first][1]} commands, {a[first][3]} textures), '
              f'after F7 {b[first][0]} ({b[first][1]} commands, {b[first][3]} textures)')
    if not differ:
        print(f'RESULT: equal - all {len(both)} frames drawn on both sides draw the same')
        return 0
    print(f'RESULT: {len(differ)} of {len(both)} frames differ; the first at k={differ[0] - back_to} '
          f'(drawn at Frame_Counter {differ[0]}), the last at k={differ[-1] - back_to}')
    for t in differ[:args.show]:
        print(f'  k={t - back_to:4d} at {t}: after F6 {a[t][0]} {a[t][1]}c {a[t][2]}v {a[t][3]}t; '
              f'after F7 {b[t][0]} {b[t][1]}c {b[t][2]}v {b[t][3]}t')
    return 1


if __name__ == '__main__':
    sys.exit(main())
