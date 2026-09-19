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

Image uploads are queued by game logic but reach the VRAM shadow on the next
RENDERED frame, which is a wall-clock matter (docs/call-trace.md section 6). So
at the target frame the dump waits until the upload queue count 0x9035A0 and
the dirty-strip flag 0x937F90 both read zero under suspension, and only then
snapshots. What was pending at the target, and how many logic frames the wait
cost, go to <label>_meta.json; --compare prints both and warns when the waits
differ, since the two dumps are then not at the same logic frame. --no-drain
snapshots at the target regardless (the old behaviour).

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
import argparse, ctypes, hashlib, json, os, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from attract_watch import find_game  # noqa: E402

k = ctypes.WinDLL('kernel32', use_last_error=True)
ntdll = ctypes.WinDLL('ntdll')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'analysis', 'memdump')
REGIONS = {'arena': (0x803580, 0xE0C4C), 'vram': (0x6C9F44, 0x100000)}
A_FLIP, A_AREA = 0x905B89, 0x904EFC
# Gfx_UploadQueueCount and the dirty-strip flag (symbols.toml). Each is cleared
# only after its flush has finished (0x461FA7, 0x45499A), so both zero means
# nothing is queued and no flush is half done.
A_QCOUNT, A_STRIP = 0x9035A0, 0x937F90


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
    # Wait for a quiet moment: both pending-upload indicators zero, read while
    # the process is suspended so the check and the snapshot are one instant.
    first, extra, t1, data = None, 0, time.time(), None
    while data is None:
        ntdll.NtSuspendProcess(h)
        try:
            pending = (read(h, A_QCOUNT, 1)[0], read(h, A_STRIP, 1)[0])
            if first is None:
                first = pending
            if pending == (0, 0) or a.no_drain:
                data = {name: read(h, addr, size) for name, (addr, size) in REGIONS.items()}
                area = int.from_bytes(read(h, A_AREA, 2), 'little')
        finally:
            ntdll.NtResumeProcess(h)
        if data is None:
            if time.time() - t1 > a.drain_timeout:
                sys.exit(f'upload queues never drained: count {pending[0]}, strip flag {pending[1]}')
            time.sleep(0.001)
            cur = read(h, A_FLIP, 1)
            if cur != last:
                extra, last = extra + 1, cur
    os.makedirs(OUT, exist_ok=True)
    for name, b in data.items():
        with open(os.path.join(OUT, f'{a.label}_{name}.bin'), 'wb') as f:
            f.write(b)
        print(f'{a.label} {name:5s} {len(b):#x} bytes  sha256 {hashlib.sha256(b).hexdigest()[:16]}')
    meta = {'area': a.area, 'frames': a.frames, 'extra_frames': extra,
            'pending_at_target': {'queue_count': first[0], 'strip_flag': first[1]},
            'pending_at_dump': {'queue_count': pending[0], 'strip_flag': pending[1]}}
    with open(os.path.join(OUT, f'{a.label}_meta.json'), 'w') as f:
        json.dump(meta, f, indent=1)
    print(f'at the target frame: queue count {first[0]}, strip flag {first[1]}; '
          f'waited ~{extra} more logic frames, {time.time() - t1:.3f} s')
    print(f'taken {a.frames}+{extra} frames after area {a.area} first read; area now {area}')


def compare(x, y):
    same = True
    metas = []
    for lab in (x, y):
        p = os.path.join(OUT, f'{lab}_meta.json')
        metas.append(json.load(open(p)) if os.path.exists(p) else None)
        print(f'{lab}: {metas[-1] if metas[-1] else "no meta (dumped before the drain wait existed)"}')
    if None not in metas:
        for key in ('area', 'frames', 'extra_frames'):
            if metas[0][key] != metas[1][key]:
                print(f'WARNING: {key} differs ({metas[0][key]} vs {metas[1][key]}) - '
                      'the dumps are not at the same logic frame')
        for lab, m in zip((x, y), metas):
            if any(m['pending_at_dump'].values()):
                print(f'WARNING: {lab} was dumped with uploads pending - its vram region is not comparable')
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
    ap.add_argument('--drain-timeout', type=float, default=10,
                    help='seconds to wait for the upload queues to empty')
    ap.add_argument('--no-drain', action='store_true',
                    help='snapshot at the target frame even with uploads pending')
    ap.add_argument('--compare', nargs=2, metavar=('A', 'B'))
    a = ap.parse_args()
    if a.compare:
        sys.exit(compare(*a.compare))
    if not a.label:
        ap.error('--label or --compare')
    dump(a)


if __name__ == '__main__':
    main()
