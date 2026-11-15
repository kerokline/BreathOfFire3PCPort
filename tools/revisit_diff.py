#!/usr/bin/env python
"""Reach one point of the game twice in ONE process and diff the image's
writable data at the two arrivals: what survives from the first showing into
the second (docs/stale-overlay-state.md).

    python tools/revisit_diff.py run --same-rand --save desertToManmo --lang en-US \
        --body tools/recipes/revisit_manmo_body.txt --after tools/recipes/revisit_manmo_after.txt \
        --watch 0x65EF0C:12 --out C:/Users/kerok/bof3x-runs/stale1105/r3 \
        --game C:/Users/kerok/bof3x-runs/stale1105/game \
        --launcher C:/Users/kerok/bof3x-runs/stale1105/L/bof3x-launcher.exe
    python tools/revisit_diff.py report C:/Users/kerok/bof3x-runs/stale1105/r3 [--all] [--runs]

The defect class: the PlayStation reloaded an overlay's file image with each
area or battle, so state its code wrote came back to the disc's bytes the next
time; the PC holds the same bytes once in its image, so the write carries into
a later showing in the same run. A fresh process starts from the exe's bytes,
so a replay or a clean load never shows it - only a revisit inside one process
does (D226, the Manmo sand hill; the Volt row-done byte; issue #149's cart).

`run` writes one recipe (OUT/revisit.txt) and plays it through
tools/input_run.py (its launch, slot-0 swap with --slot0-hold, frozen shots):

    until the title (Title_LogoState 0x66C7FA == 2)   shot rv_title1
    BODY: the title to the load and on to X           shot rv_x1
    AFTER: play on (the scene runs to its end)        shot rv_end1
    F9, F9: the restart to the title (Game_RestartFlag)
    until the title again                             shot rv_title2
    BODY again                                        shot rv_x2

At each `rv_*` shot the game is frozen (BOF3X_SHOT_WAIT) and this script reads
the image's .data and .bss (0x5DA000..0x93E000, the state hash's span) from
outside - read only, nothing is written to the game - into OUT/snap/NAME.bin.
The default BODY is the title's load of slot 0 (tools/recipes/cartReload.txt's
presses), then `until` the area word 0x904EFC reads --area, then --x-frames
frames; --body FILE replaces it (title-relative lines: both visits play it from
the title). AFTER is --after-frames frames of nothing, or --after FILE.

--same-rand is the one write: a route through random encounters (the desert's)
desyncs on the second visit because Rand has drawn on. The input driver's
randlog gives Rand's draw count at each frozen shot; at rv_x1 the script finds
the one word of the process holding the seed that count implies (the CRT's LCG
from 1, crt_rest.cpp; the C runtime's per-thread data, or bof3x.dll's when Rand
is ours) and at rv_title2 writes rv_title1's seed there, so the second visit
draws what the first drew. Rand's own state then compares equal;
anything Rand-driven that still differs differs for another reason. Without it
the bodies must not depend on Rand.

`report` compares the five snapshots. A byte that differs between rv_x1 and
rv_x2, outside tools/statehash_skip.txt's ranges, is:

    stale    rv_x2 == rv_end1 and rv_x1 == rv_title1: a fresh process holds at X
             what it held before the load, and the revisit holds what the first
             showing left - the class this hunts (timers and Rand move on, so
             they fail rv_x2 == rv_end1);
    carried  rv_x2 == rv_end1 != rv_title1, rv_x1 != rv_title1: the first
             showing's value at X again where the fresh process's X had its own
             - read it (a buffer emptied back to the title's bytes is not here);
    other    anything else: timers, Rand, play time, legitimately different state.

Each run of bytes is named by symbols.toml and, where tools/stale_census.py's
ranges hold it, by the overlay its PlayStation twin lived in and when the
PlayStation reloaded it. Snapshots are game memory: they stay in OUT (a
harness root under bof3x-runs, never the tree; CLAUDE.md rule 1).
"""
import argparse, bisect, os, re, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SPAN_LO, SPAN_HI = 0x5DA000, 0x93E000     # .data and its .bss (BOF3.exe's section table; statehash.py BASE, END)
SNAPS = ('rv_title1', 'rv_x1', 'rv_end1', 'rv_title2', 'rv_x2')

TITLE = ['until 0x66C7FA u8 == 2 timeout 3600', 'wait 30']
# The title's load of slot 0 on the PC install (tools/recipes/cartReload.txt, 2026-10-25): Start to the
# menu, Start twice to the load list, the confirm presses.
LOAD_SLOT0 = ['hold start 6', 'wait 52', 'hold start 7', 'wait 5', 'hold start 5', 'wait 40',
              'hold circle 6', 'wait 29', 'hold circle 4', 'wait 4', 'hold circle 3', 'wait 61']
RESTART = ['hold f9 4', 'wait 42', 'hold f9 4']


def read_lines(path):
    with open(path, encoding='utf-8') as f:
        return [l.rstrip('\n') for l in f if l.strip() and not l.lstrip().startswith('#')]


def compose(a):
    body = read_lines(a.body) if a.body else LOAD_SLOT0 + (
        [f'until 0x904EFC u16 == {a.area} timeout 3600'] if a.area is not None else []) + (
        [f'wait {a.x_frames}'] if a.x_frames else [])
    after = read_lines(a.after) if a.after else [f'wait {a.after_frames}']
    watches = []
    for w in a.watch:
        addr, _, n = w.partition(':')
        watches.append(f'watch {addr} {n or 1} w{addr[2:] if addr.startswith("0x") else addr}')
    lines = [f'# save {a.save}', '# tools/revisit_diff.py: one process, the same point twice'] + watches
    lines += TITLE + ['shot rv_title1 1'] + body + ['mark rv_x1', 'peek 0x904EFC u16 area', 'shot rv_x1 1']
    lines += after + ['shot rv_end1 1'] + RESTART + TITLE + ['shot rv_title2 1']
    lines += body + ['mark rv_x2', 'peek 0x904EFC u16 area', 'shot rv_x2 1', 'end']
    return '\n'.join(lines) + '\n'


def seed_after(draws):
    """Rand's seed after `draws` draws: the MSVC CRT's holdrand from 1 (crt_rest.cpp, the same LCG)."""
    x = 1
    for _ in range(draws):
        x = (x * 0x343FD + 0x269EC3) & 0xFFFFFFFF
    return x


def last_draws(log):
    """Rand's draw count by the input driver's randlog (`randlog frame F rand K`), the last line."""
    k = None
    with open(log, 'rb') as f:
        for line in f:
            m = re.match(rb'randlog +frame \d+ rand (\d+)', line)
            if m:
                k = int(m[1])
    return k


def find_words(pid, values):
    """[(va, value)]: every aligned dword of the process's committed read-write memory equal to one
    of `values` (Rand's seed lives in the C runtime's per-thread data under the randlog, a heap
    block, or in bof3x.dll when Rand is ours)."""
    import ctypes
    from ctypes import wintypes as w

    class MBI(ctypes.Structure):
        _fields_ = [('BaseAddress', ctypes.c_void_p), ('AllocationBase', ctypes.c_void_p),
                    ('AllocationProtect', w.DWORD), ('RegionSize', ctypes.c_size_t), ('State', w.DWORD),
                    ('Protect', w.DWORD), ('Type', w.DWORD)]
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.OpenProcess.restype = w.HANDLE
    k.VirtualQueryEx.argtypes = [w.HANDLE, ctypes.c_void_p, ctypes.POINTER(MBI), ctypes.c_size_t]
    k.VirtualQueryEx.restype = ctypes.c_size_t
    h = k.OpenProcess(0x0410, False, pid)
    want = {struct.pack('<I', v): v for v in values}
    out, at, mbi = [], 0, MBI()
    while at < 0x7FFF0000 and k.VirtualQueryEx(h, ctypes.c_void_p(at), ctypes.byref(mbi), ctypes.sizeof(mbi)):
        size = mbi.RegionSize
        if mbi.State == 0x1000 and mbi.Protect in (0x04, 0x40) and size <= 0x10000000:   # COMMIT, RW / XRW
            buf = ctypes.create_string_buffer(size)
            got = ctypes.c_size_t()
            if k.ReadProcessMemory(h, ctypes.c_void_p(at), buf, size, ctypes.byref(got)):
                raw = buf.raw[:got.value]
                for key, v in want.items():
                    i = raw.find(key)
                    while i >= 0:
                        if i % 4 == 0:
                            out.append((at + i, v))
                        i = raw.find(key, i + 1)
        at += size
    k.CloseHandle(h)
    return out


def poke(pid, at, data):
    import ctypes
    from ctypes import wintypes as w
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.OpenProcess.restype = w.HANDLE
    h = k.OpenProcess(0x0028, False, pid)             # PROCESS_VM_WRITE | PROCESS_VM_OPERATION
    got = ctypes.c_size_t()
    ok = k.WriteProcessMemory(h, ctypes.c_void_p(at), data, len(data), ctypes.byref(got))
    k.CloseHandle(h)
    return bool(ok)


def cmd_run(a):
    import input_run
    import task_stacks
    os.makedirs(os.path.join(a.out, 'snap'), exist_ok=True)
    recipe = os.path.join(a.out, 'revisit.txt')
    with open(recipe, 'w', encoding='utf-8', newline='\n') as f:
        f.write(compose(a))
    grab = input_run.grab

    def snap_then_grab(path):
        name = os.path.splitext(os.path.basename(path))[0]
        if name in SNAPS:
            pid = input_run.game_pid()
            if not pid:
                print(f'  {name}: no game to read')
            else:
                g = task_stacks.Game(pid)
                buf = bytearray()
                for at in range(SPAN_LO, SPAN_HI, 0x10000):
                    n = min(0x10000, SPAN_HI - at)
                    b = g.read(at, n)
                    if b is None or len(b) != n:
                        sys.exit(f'{name}: read failed at 0x{at:X}')
                    buf += b
                with open(os.path.join(a.out, 'snap', name + '.bin'), 'wb') as f:
                    f.write(buf)
                print(f'  {name}: 0x{SPAN_LO:X}..0x{SPAN_HI:X} read (move 0x{g.move:X})')
        if a.same_rand and name in ('rv_title1', 'rv_x1', 'rv_title2'):
            # Rand's draw count by the randlog at the frozen shot; the seed it implies is searched for
            # at rv_x1 (Rand has drawn by then, so the value is rare) and rv_title1's written back at
            # rv_title2. A count a frame stale is allowed for (+-2 draws).
            pid = input_run.game_pid()
            k = last_draws(input_run.LOG)
            if k is None:
                sys.exit('--same-rand: no randlog lines in the log')
            if name == 'rv_title1':
                rand['title'] = k
            elif name == 'rv_x1':
                vals = {seed_after(d): d for d in range(max(0, k - 2), k + 3)}
                hits = find_words(pid, vals)
                if len(hits) != 1:
                    sys.exit(f'--same-rand: {len(hits)} words hold the seed of draw {k} +-2 '
                             f'({[hex(x) for x, _ in hits[:4]]}); run without it')
                rand['at'] = hits[0][0]
                print(f'  Rand: seed at 0x{rand["at"]:X}, {vals[hits[0][1]]} draws (randlog {k}); '
                      f'{rand["title"]} at rv_title1')
            else:
                if not poke(pid, rand['at'], struct.pack('<I', seed_after(rand['title']))):
                    sys.exit('--same-rand: the write failed')
                print(f'  Rand: seed at 0x{rand["at"]:X} from {k} draws back to rv_title1\'s {rand["title"]}')
        grab(path)

    rand = {}
    input_run.grab = snap_then_grab
    argv = ['input_run.py', recipe, '--out', os.path.join(a.out, 'shots'), '--save', a.save, '--slot0-hold',
            '--no-front', '--minutes', str(a.minutes)]
    for k in ('game', 'launcher', 'lang'):
        if getattr(a, k):
            argv += ['--' + k, getattr(a, k)]
    if a.slot0_shared:
        argv.append('--slot0-shared')
    for kv in a.env:
        argv += ['--env', kv]
    sys.argv = argv
    status = None
    try:
        input_run.main()
    except SystemExit as e:
        status = e.code
    missing = [n for n in SNAPS if not os.path.exists(os.path.join(a.out, 'snap', n + '.bin'))]
    if missing:
        sys.exit(f'run ended ({status}) without {" ".join(missing)}')
    a.dir = a.out
    cmd_report(a)


def skip_mask():
    import statehash
    mask = bytearray(SPAN_HI - SPAN_LO)
    with open(os.path.join(ROOT, 'tools', 'statehash_skip.txt'), encoding='utf-8') as f:
        for at, n in statehash.parse_skips(f.read()):
            lo, hi = max(at, SPAN_LO), min(at + n, SPAN_HI)
            if lo < hi:
                mask[lo - SPAN_LO:hi - SPAN_LO] = b'\1' * (hi - lo)
    return mask


def overlay_of():
    import stale_census
    rs = stale_census.ranges('psx-jp')
    starts = [r[0] for r in rs]

    def find(x):
        i = bisect.bisect_right(starts, x) - 1
        if i >= 0 and x < rs[i][1]:
            lo, hi, fil, sec = rs[i]
            return f'{fil}#{sec} ({stale_census.reload_class(fil)})'
        return '-'
    return find


def cmd_report(a):
    import statehash
    d = os.path.join(a.dir, 'snap')
    s = {n: open(os.path.join(d, n + '.bin'), 'rb').read() for n in SNAPS}
    t1, x1, e1, x2 = s['rv_title1'], s['rv_x1'], s['rv_end1'], s['rv_x2']
    mask = skip_mask()
    where = overlay_of()
    runs, cur = [], None
    for i in range(len(x1)):
        if x1[i] == x2[i] or mask[i]:
            cls = None
        elif x2[i] == e1[i] and x1[i] == t1[i]:
            cls = 'stale'
        elif x2[i] == e1[i] and x2[i] != t1[i]:
            cls = 'carried'
        else:
            cls = 'other'
        if cls and cur and cur[0] == cls and i - cur[2] <= 4:
            cur[2] = i + 1
        elif cls:
            cur = [cls, i, i + 1]
            runs.append(cur)
    order = {'stale': 0, 'carried': 1, 'other': 2}
    out = []
    counts = {k: [0, 0] for k in order}
    for cls, lo, hi in runs:
        counts[cls][0] += 1
        counts[cls][1] += hi - lo
    out.append(f'revisit diff {a.dir}: rv_x1 against rv_x2 over 0x{SPAN_LO:X}..0x{SPAN_HI:X}, '
               f'tools/statehash_skip.txt left out')
    for k in order:
        out.append(f'  {k:8s} {counts[k][0]:6d} runs {counts[k][1]:8d} bytes')
    out.append('')
    # One row per (class, symbol): a scratch buffer's hundred runs read as one line. --runs lists each run.
    groups = {}
    for cls, lo, hi in runs:
        if cls == 'other' and not a.all:
            continue
        va = SPAN_LO + lo
        sym = statehash.at(va).split('+')[0]
        g = groups.setdefault((cls, sym), [va, 0, 0, where(va), lo, hi])
        g[1] += 1
        g[2] += hi - lo
    out.append('class    first      runs  bytes symbol                         overlay (psx-jp)                '
               'title1/x1/end1/x2 at the first run (8 bytes)')
    key = lambda kv: (order[kv[0][0]], kv[1][3] == '-', kv[1][0])
    for (cls, sym), (va, n, nb, ov, lo, hi) in sorted(groups.items(), key=key):
        vals = '/'.join(b[lo:min(hi, lo + 8)].hex() for b in (t1, x1, e1, x2))
        out.append(f'{cls:8s} 0x{va:06X} {n:5d} {nb:6d} {sym[:30]:30s} {ov[:31]:31s} {vals}')
    if a.runs:
        out.append('')
        for cls, lo, hi in sorted(runs, key=lambda r: (order[r[0]], r[1])):
            if cls == 'other' and not a.all:
                continue
            va = SPAN_LO + lo
            vals = '/'.join(b[lo:min(hi, lo + 8)].hex() for b in (t1, x1, e1, x2))
            out.append(f'{cls:8s} 0x{va:06X} {hi - lo:6d} {statehash.at(va)[:40]:40s} {where(va)[:31]:31s} {vals}')
    if not a.all and counts['other'][0]:
        out.append(f'({counts["other"][0]} `other` runs not shown: --all)')
    text = '\n'.join(out) + '\n'
    with open(os.path.join(a.dir, 'report.txt'), 'w', encoding='utf-8') as f:
        f.write(text)
    print(text)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('run')
    r.add_argument('--save', required=True, help='the recipe save loaded both times (tools/recipe_saves/NAME.DAT, '
                                                 'or BOF3X_RECIPE_SAVES)')
    r.add_argument('--out', required=True, help='a harness directory (snapshots are game memory)')
    r.add_argument('--body', help='title-relative recipe lines from the title to X (default: load slot 0)')
    r.add_argument('--area', type=int, help='with the default body: wait for this area number after the load')
    r.add_argument('--x-frames', type=int, default=60, help='with the default body: frames after the area to X')
    r.add_argument('--after', help='recipe lines played after X on the first visit (default: --after-frames)')
    r.add_argument('--after-frames', type=int, default=1800)
    r.add_argument('--watch', action='append', default=[], metavar='ADDR:LEN',
                   help='log every change of these bytes (the recipe op `watch`, reads only)')
    r.add_argument('--game')
    r.add_argument('--launcher')
    r.add_argument('--lang')
    r.add_argument('--slot0-shared', action='store_true')
    r.add_argument('--env', action='append', default=[], metavar='K=V')
    r.add_argument('--minutes', type=float, default=15)
    r.add_argument('--all', action='store_true', help='also list the `other` runs in the report')
    r.add_argument('--runs', action='store_true', help='also list every run')
    r.add_argument('--same-rand', action='store_true',
                   help='a harness write: at rv_title2 put Rand\'s seed (bof3x.dll\'s, found by value at '
                        'rv_title1) back to rv_title1\'s, so a route with random encounters replays alike')
    p = sub.add_parser('report')
    p.add_argument('dir')
    p.add_argument('--all', action='store_true')
    p.add_argument('--runs', action='store_true', help='also list every run, not only one row per symbol')
    a = ap.parse_args()
    if a.cmd == 'run':
        cmd_run(a)
    else:
        cmd_report(a)


if __name__ == '__main__':
    main()
