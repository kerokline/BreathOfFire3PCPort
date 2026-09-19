#!/usr/bin/env python
"""Dump regions of a running BOF3.exe at a reproducible point of the attract
sequence, for byte-level A/B checks of a reimplemented loader.

    # terminal 1: a fresh, hands-off launch (ours, or --original LoadDatFile)
    python tools/attract_run.py --out analysis/attract/tmp.tsv --minutes 3
    # terminal 2, started right after: waits for the point, dumps, exits
    python tools/mem_dump.py --label ours
    python tools/mem_dump.py --compare orig ours

The point: the area word 0x904EFC first reads --area (default 4, the first
field scene of the attract sequence, docs/attract-mode.md), then --frames more
logic frames (flips of 0x905B89). The process is suspended (NtSuspendProcess)
for the read so the regions are one consistent snapshot, then resumed. Nothing
is written to the game.

The port is deterministic to the frame from launch (docs/attract-mode.md), so
two runs of the same code should dump identical bytes; this script counts
frames from outside and can be a frame late, which is why an original-vs-
original baseline belongs beside any ours-vs-original comparison.

Regions:
  arena  0x803580, 0xE0C4C bytes - where kind-0 DAT chunks land; the length is
         the extent the shipped data can write (symbols.toml, MessagePools).
  vram   0x6C9F44, 0x100000 bytes - the 16-bit 1024x512 shadow of PSX VRAM
         that kind-1 chunks land in (Gfx_LoadImage).

Output goes to analysis/memdump/, which is game-derived and gitignored.
"""
import argparse, ctypes, hashlib, os, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from attract_watch import find_game  # noqa: E402

k = ctypes.WinDLL('kernel32', use_last_error=True)
ntdll = ctypes.WinDLL('ntdll')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'analysis', 'memdump')
REGIONS = {'arena': (0x803580, 0xE0C4C), 'vram': (0x6C9F44, 0x100000)}
A_FLIP, A_AREA = 0x905B89, 0x904EFC


def read(h, addr, size):
    buf = ctypes.create_string_buffer(size)
    got = ctypes.c_size_t()
    if not k.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, size, ctypes.byref(got)) or got.value != size:
        sys.exit(f'ReadProcessMemory {addr:#x}+{size:#x} failed: {ctypes.get_last_error()}')
    return buf.raw


def dump(a):
    pid = find_game(wait=60)
    # VM_READ | QUERY_INFORMATION | SUSPEND_RESUME
    h = k.OpenProcess(0x0410 | 0x0800, False, pid)
    if not h:
        sys.exit(f'OpenProcess failed: {ctypes.get_last_error()}')
    t0 = time.time()
    while int.from_bytes(read(h, A_AREA, 2), 'little') != a.area:
        if time.time() - t0 > a.timeout:
            sys.exit(f'area never read {a.area}')
        time.sleep(0.002)
    frames, last = 0, read(h, A_FLIP, 1)
    while frames < a.frames:
        cur = read(h, A_FLIP, 1)
        if cur != last:
            frames, last = frames + 1, cur
        time.sleep(0.001)
    ntdll.NtSuspendProcess(h)
    try:
        data = {name: read(h, addr, size) for name, (addr, size) in REGIONS.items()}
        area = int.from_bytes(read(h, A_AREA, 2), 'little')
    finally:
        ntdll.NtResumeProcess(h)
    os.makedirs(OUT, exist_ok=True)
    for name, b in data.items():
        with open(os.path.join(OUT, f'{a.label}_{name}.bin'), 'wb') as f:
            f.write(b)
        print(f'{a.label} {name:5s} {len(b):#x} bytes  sha256 {hashlib.sha256(b).hexdigest()[:16]}')
    print(f'taken {a.frames} frames after area {a.area} first read; area now {area}')


def compare(x, y):
    same = True
    for name, (addr, _) in REGIONS.items():
        bx = open(os.path.join(OUT, f'{x}_{name}.bin'), 'rb').read()
        by = open(os.path.join(OUT, f'{y}_{name}.bin'), 'rb').read()
        diffs = [i for i in range(len(bx)) if bx[i] != by[i]]
        if not diffs:
            print(f'{name}: IDENTICAL ({len(bx):#x} bytes)')
            continue
        same = False
        runs, start, prev = [], diffs[0], diffs[0]
        for i in diffs[1:]:
            if i > prev + 16:
                runs.append((start, prev)); start = i
            prev = i
        runs.append((start, prev))
        print(f'{name}: {len(diffs)} bytes differ in {len(runs)} runs')
        for s, e in runs[:20]:
            print(f'    {addr + s:#x}..{addr + e:#x}  (offset {s:#x}, {e - s + 1} bytes)')
    return 0 if same else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--label')
    ap.add_argument('--area', type=int, default=4)
    ap.add_argument('--frames', type=int, default=300)
    ap.add_argument('--timeout', type=float, default=240)
    ap.add_argument('--compare', nargs=2, metavar=('A', 'B'))
    a = ap.parse_args()
    if a.compare:
        sys.exit(compare(*a.compare))
    if not a.label:
        ap.error('--label or --compare')
    dump(a)


if __name__ == '__main__':
    main()
