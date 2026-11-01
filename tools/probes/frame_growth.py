#!/usr/bin/env python
"""How much larger the 64-bit build's stack frames are, for the task slices
(docs/x64-build-design.md 7; the 64-bit build round, group XA2).

Compiles a sample of src/game files with both drivers (-S, the build's -O2
-g -std=c++20 and include dirs; the generated header from a configured
build/) and reads each function's prologue: the pushes of callee-saved
registers plus the largest `sub $N, %esp` / `sub $N, %rsp`, which is the
frame the function takes below its return address. Functions matched by
mangled name (the i686 driver prefixes `_`). Reports the per-function
ratio's distribution and the sum over the sample.

    python tools/probes/frame_growth.py [--all] [--files N] [--out TSV] [--gen64 DIR]

Only files without inline asm compile under the x86_64 driver unchanged
(stage B1 rewrites the rest); a file that fails to compile is skipped and
counted. Runs no game; compiles only.
"""
import argparse
import glob
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FLAGS = ['-S', '-O2', '-g', '-std=c++20', '-Wall', '-Wextra', '-falign-functions=16', '-fno-strict-aliasing',
         '-I', os.path.join(ROOT, 'src'), '-I', os.path.join(ROOT, 'third_party'), '-I', os.path.join(ROOT, 'build', 'gen')]
DRIVERS = {'i686': 'i686-w64-mingw32-clang++', 'x86_64': 'x86_64-w64-mingw32-clang++'}
PUSH = {'i686': 4, 'x86_64': 8}


def frames(asm, width):
    """{mangled name: frame bytes} from an AT&T listing: pushes before the
    first call or the frame's sub, plus the largest sub of rsp/esp in the
    prologue region (up to the first call, jump or ret)."""
    out = {}
    name = None
    pushes = 0
    sub = 0
    done = False
    for line in asm.splitlines():
        # a function's label: clang writes `name:   # @name`; the debug and
        # block labels (Lfunc_begin0, Ltmp3, .LBB0_2) have no such comment
        m = re.match(r'^(_?[A-Za-z_][\w.$@]*):\s*# @', line)
        if m:
            if name is not None:
                out[name] = pushes * PUSH[width] + sub
            name = m.group(1)[1:] if width == 'i686' and m.group(1).startswith('_') else m.group(1)
            pushes = sub = 0
            done = False
            continue
        if name is None or done:
            continue
        s = line.strip()
        if re.match(r'^push[lq]?\s', s):
            pushes += 1
        elif re.match(r'^sub[lq]?\s+\$(\d+),\s*%[er]sp', s):
            sub = max(sub, int(re.match(r'^sub[lq]?\s+\$(\d+)', s).group(1)))
        elif re.match(r'^(call|jmp|ret|j[a-z]+)\b', s):
            done = True
    if name is not None:
        out[name] = pushes * PUSH[width] + sub
    return out


def compile_one(path, width, gen64=None):
    with tempfile.NamedTemporaryFile(suffix='.s', delete=False) as t:
        out = t.name
    extra = ['-I', gen64] if (gen64 and width == 'x86_64') else []
    r = subprocess.run([DRIVERS[width]] + extra + FLAGS + ['-o', out, path], capture_output=True, text=True)
    if r.returncode != 0:
        if os.path.exists(out):
            os.unlink(out)
        return None, r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'failed'
    asm = open(out, encoding='utf-8', errors='replace').read()
    os.unlink(out)
    return asm, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--all', action='store_true', help='every asm-free live file of src/game, not a sample')
    ap.add_argument('--files', type=int, default=40, help='sample size (every k-th file)')
    ap.add_argument('--out', help='a TSV of every matched function')
    ap.add_argument('--gen64', help='an include dir searched first for the x86_64 compile: a copy of build/gen whose '
                                    'symbols.gen.h declares the 111 pointer-typed [[data]] slots as std::uint32_t, '
                                    'since the generated GameData does not keep its size with 8-byte pointers '
                                    '(stage B3 makes that change in tools/gen_symbols.py)')
    a = ap.parse_args()
    files = sorted(f for f in glob.glob(os.path.join(ROOT, 'src', 'game', '*.cpp'))
                   if 'fuzz' not in os.path.basename(f).lower() and 'harness' not in os.path.basename(f).lower()
                   and not re.search(r'\basm\b|__asm__', open(f, encoding='utf-8', errors='replace').read()))
    if not a.all and len(files) > a.files:
        step = len(files) / a.files
        files = [files[int(i * step)] for i in range(a.files)]
    rows = []
    skipped = []
    for f in files:
        asm32, e32 = compile_one(f, 'i686')
        asm64, e64 = compile_one(f, 'x86_64', a.gen64)
        if asm32 is None or asm64 is None:
            skipped.append((os.path.basename(f), e32 or e64))
            continue
        f32, f64 = frames(asm32, 'i686'), frames(asm64, 'x86_64')
        for n, b32 in f32.items():
            if n in f64 and b32 > 0:
                rows.append((os.path.basename(f), n, b32, f64[n]))
        print(f'{os.path.basename(f)}: {len(f32)} functions, {len([n for n in f32 if n in f64])} matched', file=sys.stderr)
    if a.out:
        with open(a.out, 'w') as o:
            o.write('file\tfunction\tframe_i686\tframe_x86_64\n')
            for r in rows:
                o.write('\t'.join(str(x) for x in r) + '\n')
    s32 = sum(r[2] for r in rows)
    s64 = sum(r[3] for r in rows)
    ratios = sorted(r[3] / r[2] for r in rows)
    big = sorted(rows, key=lambda r: -r[3])[:8]
    print(f'{len(files) - len(skipped)} files compiled both ways, {len(skipped)} skipped, {len(rows)} functions matched with a frame')
    if rows:
        print(f'sum of frames: i686 {s32} bytes, x86_64 {s64} bytes, ratio {s64 / s32:.3f}')
        print(f'per-function ratio: median {ratios[len(ratios) // 2]:.2f}, p90 {ratios[int(len(ratios) * 0.9)]:.2f}, '
              f'max {ratios[-1]:.2f}, min {ratios[0]:.2f}')
        print('largest 64-bit frames:')
        for r in big:
            print(f'  {r[0]} {r[1]}: {r[2]} -> {r[3]}')
    for f, why in skipped:
        print(f'skipped {f}: {why}')


if __name__ == '__main__':
    main()
