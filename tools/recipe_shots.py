#!/usr/bin/env python
"""Put a frame-exact `shot` into a recorded recipe every N frames.

    python tools/recipe_shots.py tools/recipes/shop.txt --every 90 --out tools/recipes/shop_ab.txt

For a recipe BOF3X_RECORD wrote (only `wait N` / `hold BUTTONS N` / `shot NAME 1
[BUTTONS]` lines after the header comments). A shot of one frame holding that
frame's buttons takes the place of the frame, so the inputs the game sees are
unchanged and the recipe keeps its timing (src/hook/input_script.cpp).
"""
import argparse


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('recipe')
    ap.add_argument('--every', type=int, default=90, help='frames between shots')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()

    header, frames = [], []          # frames: one entry per frame, (buttons or '', shot name or None)
    for line in open(a.recipe):
        t = line.split('#')[0].split()
        if not t:
            if not frames:
                header.append(line.rstrip('\n'))
            continue
        if t[0] == 'wait':
            frames += [('', None)] * int(t[1])
        elif t[0] == 'hold':
            frames += [(t[1], None)] * int(t[2])
        elif t[0] == 'shot' and len(t) >= 3 and t[2] == '1':
            frames.append((t[3] if len(t) == 4 else '', t[1]))
        else:
            raise SystemExit(f'{a.recipe}: not a recorded recipe line: {line.strip()}')

    n = 0
    for f in range(a.every, len(frames), a.every):
        if frames[f][1] is None:
            n += 1
            frames[f] = (frames[f][0], f'f{f:05d}')

    out = header + [f'# Shots every {a.every} frames added by tools/recipe_shots.py from {a.recipe}.']
    i = 0
    while i < len(frames):
        b, shot = frames[i]
        if shot:
            out.append(f'shot {shot} 1' + (f' {b}' if b else ''))
            i += 1
            continue
        j = i
        while j < len(frames) and frames[j] == (b, None):
            j += 1
        out.append(f'hold {b} {j - i}' if b else f'wait {j - i}')
        i = j
    open(a.out, 'w', newline='\n').write('\n'.join(out) + '\n')
    print(f'{len(frames)} frames, {n} shots added -> {a.out}')


if __name__ == '__main__':
    main()
