#!/usr/bin/env python
"""The quicksave proof by the state hash (docs/quicksave-routes.md,
docs/quicksave-round.md section 2 "RC").

    python tools/quicksave_check.py make  ROUTE.txt --at F --why TEXT --name N   (writes the three forms)
    python tools/quicksave_check.py run   RECIPE.txt --out DIR [--defaults] [--lock DIR] [--plain P.sh]
    python tools/quicksave_check.py inproc RUN.sh [--log bof3x.log] [--plain PLAIN.sh]
    python tools/quicksave_check.py cross LIVE.sh LOADED.sh [--log-live L] [--log-loaded L]
    python tools/quicksave_check.py same  A.sh B.sh          (the identity reference: keys pressed, nothing changed)
    python tools/quicksave_check.py selftest

make    From a recorded route (wait / hold / one-frame shot lines only) and
        a recipe frame F at which the route holds nothing, three recipes:
          quick_N.txt       the proof: the route to F, `key f6` in place of
                            frame F, the route's frames F+1..F+GAP-1, `key f7`,
                            then the route again from F+1 to its end - so
                            after a working restore the game is handed the
                            same inputs it had after the take, and once the
                            window is over it plays the route on to its end;
          quick_N_keys.txt  the keys alone: `key f6` in place of frame F and
                            `key f7` in place of frame F+GAP (both must be
                            idle frames), nothing replayed - the same length
                            and inputs as the route. While F6 and F7 do
                            nothing its hash equals the route's tick for tick;
          quick_N_cross.txt the cross-process form: `key f7` at recipe frame
                            0, then the route from F+1 - a second process
                            that loads the quicksave the live run took at F.
inproc  The in-process proof on one hashed run of quick_N.txt: finds the
        restore (the record where Frame_Counter jumps), the snapshot (the
        first earlier record with the restored Frame_Counter), and compares
        snapshot+k with restore+k for k over the window. Reports the first
        k whose hash differs, its ticks and recipe frames, and the pages that
        differ, each named by its symbols and by the skip list's rows on it.
        No jump at all is reported as "f7 did nothing", not an error.
        --plain: also compare the whole run with a hashed run of the route
        itself (before the restore tick for tick; after it, shifted by the
        window) - the route after a restore is the route.
cross   The cross-process proof: LOADED is a hashed run of quick_N_cross.txt,
        LIVE a hashed run that pressed f6 at F (quick_N.txt or _keys). The
        loaded run's restore record is matched with the live record holding
        the same Frame_Counter after the take (the restore's continuation
        if LIVE restored, else the take itself), and the two compared from
        there to the shorter run's end.
same    Two hashed runs compared tick for tick (statehash.py diff, with the
        pages named as above): the identity reference, quick_N_keys.txt
        against the route while F6 and F7 do nothing.
run     One live run through tools/input_run.py with BOF3X_STATEHASH and the
        skip list, the hash conditions (BOF3X_OPT=none BOF3X_LAYERING=0)
        unless --defaults, --slot0-hold and --no-front; then the analysis the
        recipe's form wants (inproc for quick_N, same against --plain for
        _keys). --lock DIR takes the machine's one-live-game lock (mkdir,
        retried a minute apart) for the run and gives it back after.
selftest  The analyses on synthetic hash files: an equal window, a window
        diverging at a known tick, a run with no restore, a cross pair.

Exit codes: 0 equal (or, for `same`, identical), 1 a difference, 2 the
quicksave keys did nothing (no restore found) - a report, not a crash.
"""
import argparse, os, re, shutil, struct, subprocess, sys, tempfile, time

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import statehash  # noqa: E402  (Run, names_in, the header checks)

SKIP_FILE = os.path.join(ROOT, 'tools', 'statehash_skip.txt')
GAP = 300
META = re.compile(r'^# quick (\w+) f6 (\d+) f7 (\d+) gap (\d+) from (\S+)')


# ---------------------------------------------------------------- recipes

def read_route(path):
    """(header lines, frames) of a recorded route: frames one entry per frame,
    (buttons or '', shot name or None) - recipe_shots.py's reading."""
    header, frames = [], []
    for line in open(path):
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
        elif t[0] == 'key' and len(t) == 2:      # a quick form's f6 / f7: one frame, nothing held
            frames.append(('KEY', t[1]))
        else:
            raise SystemExit(f'{path}: not a recorded recipe line: {line.strip()}')
    return header, frames


def emit(frames, out, suffix=''):
    """Recipe lines for frames: (buttons, shot) or ('KEY', 'f6')."""
    i = 0
    while i < len(frames):
        b, shot = frames[i]
        if b == 'KEY':
            out.append(f'key {shot}')
            i += 1
            continue
        if shot:
            out.append(f'shot {shot}{suffix} 1' + (f' {b}' if b else ''))
            i += 1
            continue
        j = i
        while j < len(frames) and frames[j] == (b, None):
            j += 1
        out.append(f'hold {b} {j - i}' if b else f'wait {j - i}')
        i = j


def idle_runs(frames, lo=0, hi=None, least=1):
    """(first, length) of each run of idle frames (nothing held, no shot)."""
    hi = len(frames) if hi is None else min(hi, len(frames))
    out, i = [], lo
    while i < hi:
        if frames[i] == ('', None):
            j = i
            while j < hi and frames[j] == ('', None):
                j += 1
            if j - i >= least:
                out.append((i, j - i))
            i = j
        else:
            i += 1
    return out


def cmd_make(a):
    header, r = read_route(a.route)
    f6, gap = a.at, a.gap
    f7 = f6 + gap
    src = os.path.basename(a.route)
    if f7 + 1 >= len(r):
        sys.exit(f'{src}: {len(r)} frames; f7 at {f7} is past its end')
    if r[f6] != ('', None):
        sys.exit(f'{src}: frame {f6} holds {r[f6][0] or "a shot"}: f6 must replace an idle frame '
                 f'(idle runs near it: {idle_runs(r, max(0, f6 - 60), f6 + 60)})')
    k7 = a.keys_f7 or f7      # the keys form's f7: the window's end unless that frame holds a button
    keys_ok = f6 < k7 < len(r) and r[k7] == ('', None)
    keep = [h for h in header if h.startswith('#')]
    save = [h for h in keep if re.match(r'#\s*save\s', h)]
    rest = [h for h in keep if h not in save]
    why = [f'# {w}' for w in a.why]
    d = a.dir
    name = a.name

    def write(fn, lines, want):
        p = os.path.join(d, fn)
        open(p, 'w', newline='\n').write('\n'.join(lines) + '\n')
        # read back: the same frames, frame for frame (a shot compared by its buttons, its name aside)
        got = [(b, None if b != 'KEY' else s) for b, s in read_route(p)[1]]
        exp = [(b, None if b != 'KEY' else s) for b, s in want]
        if got != exp:
            sys.exit(f'{p}: read back, the frames differ from those meant - a bug in make')
        print(f'{p}: {len(got)} frames, {sum(1 for x in lines if not x.startswith("#"))} lines, read back equal')

    # the proof: route to f6, the take, the window, the restore, the route again from f6+1
    q = r[:f6] + [('KEY', 'f6')] + r[f6 + 1:f7] + [('KEY', 'f7')] + r[f6 + 1:]
    out = [f'# quick replay f6 {f6} f7 {f7} gap {gap} from {src}',
           f'# The quicksave proof on {src} (tools/quicksave_check.py make; docs/quicksave-routes.md):',
           f'# f6 in place of idle frame {f6}, the route\'s frames {f6 + 1}..{f7 - 1}, f7 at {f7}, then the route',
           f'# again from frame {f6 + 1} to its end ({len(q)} frames, the route\'s {len(r)} + {gap}). After a working',
           f'# restore the game is handed the inputs it had after the take; while F6/F7 do nothing the replay',
           f'# drives the game from the wrong place and the route does not reach its end.'] + why + save + \
          ['# The recorded route\'s header:'] + rest
    lines = list(out)
    emit(q[:f7 + 1], lines)
    emit(q[f7 + 1:], lines, '_r')       # replayed shots get their own names
    write(f'quick_{name}.txt', lines, q)

    if keys_ok:
        k = list(r)
        k[f6], k[k7] = ('KEY', 'f6'), ('KEY', 'f7')
        out = [f'# quick keys f6 {f6} f7 {k7} gap {k7 - f6} from {src}',
               f'# The keys alone on {src}: f6 in place of idle frame {f6}, f7 in place of idle frame {k7}'
               + ('' if k7 == f7 else f' (frame {f7}, the proof\'s f7, holds a button)') + ',',
               f'# nothing replayed - the route\'s length and inputs. Its hash equals the route\'s while the',
               f'# keys do nothing (the identity reference, docs/quicksave-routes.md).'] + why + save + \
              ['# The recorded route\'s header:'] + rest
        lines = list(out)
        emit(k, lines)
        write(f'quick_{name}_keys.txt', lines, k)
    else:
        print(f'{src}: frame {k7} holds {r[k7][0] or "a shot"}: no _keys form (f7 must replace an idle frame; '
              f'--keys-f7 names another; idle runs after the window: {idle_runs(r, f7, f7 + 200, 2)[:6]})')

    c = [('KEY', 'f7')] + r[f6 + 1:]
    out = [f'# quick cross f6 {f6} f7 0 gap {gap} from {src}',
           f'# The cross-process form of quick_{name}.txt: f7 at recipe frame 0 loads the quicksave a live run',
           f'# took at frame {f6}, then the route from frame {f6 + 1}. No save header: nothing is loaded through',
           f'# slot 0 (run it with --no-save). Compare with `quicksave_check.py cross LIVE.sh LOADED.sh`.'] + why + \
          ['# The recorded route\'s header:'] + [h for h in rest]
    lines = list(out)
    emit(c, lines, '_x')
    write(f'quick_{name}_cross.txt', lines, c)


def recipe_meta(path):
    for line in open(path):
        m = META.match(line)
        if m:
            return m[1], int(m[2]), int(m[3]), int(m[4]), m[5]
        if line.strip() and not line.startswith('#'):
            break
    return None


# ---------------------------------------------------------------- analysis

_rows = None


def skip_rows():
    """The skip list's rows: (address, length, comment)."""
    global _rows
    if _rows is None:
        _rows = []
        for line in open(SKIP_FILE, encoding='utf-8'):
            body, _, why = line.partition('#')
            w = body.split()
            if len(w) >= 2:
                _rows.append((int(w[0], 0), int(w[1], 0), ' '.join(why.split())))
    return _rows


def page_name(run, p):
    lo = run.base + p * run.page
    hi = lo + run.page
    rows = [f'skip 0x{a:06X}+{n} {why}' for a, n, why in skip_rows() if a < hi and a + n > lo]
    syms = statehash.names_in(lo, hi) if os.path.exists(os.path.join(ROOT, 'symbols.toml')) else ''
    return lo, syms, rows


def print_pages(run, per, total, indent='  '):
    print(f'{indent}page       first    last   ticks  symbols')
    for p, (f, l, c) in sorted(per.items(), key=lambda kv: (kv[1][0], kv[0])):
        lo, syms, rows = page_name(run, p)
        print(f'{indent}0x{lo:06X} {f:7d} {l:7d} {c:7d}  {syms}')
        for row in rows[:6]:
            print(f'{indent}{"":33s}{row}')
        if len(rows) > 6:
            print(f'{indent}{"":33s}+{len(rows) - 6} more skip rows')


def states(run):
    """Per record: (tick, Frame_Counter, recipe frame, page tuple)."""
    out = []
    for i, (key, fc, st) in enumerate(run.frames()):
        out.append((key, fc, run.rf[i], tuple(st)))
    return out


class HashRun(statehash.Run):
    """statehash.Run keeping the recipe frame beside the tick."""

    def __init__(self, path):
        super().__init__(path, 'tick')
        d = open(path, 'rb').read()
        o = {1: 24, 2: 28, 3: 32}[self.version]
        self.rf = []
        while o + 16 <= len(d) and len(self.rf) < len(self.keys):
            tick, fc, rf, n = struct.unpack_from('<4I', d, o)
            self.rf.append(rf)
            o += 16 + 8 * n


def jumps(st):
    """Records whose Frame_Counter is not the previous one's plus one."""
    return [i for i in range(1, len(st)) if st[i][1] != st[i - 1][1] + 1]


def compare(run, a, b, sa, sb, n, out_pages=()):
    """Compare sa[a+k] with sb[b+k] for k < n: (first k or None, per page, compared)."""
    per, first, total = {}, None, 0
    for k in range(n):
        if a + k >= len(sa) or b + k >= len(sb):
            break
        total += 1
        x, y = sa[a + k][3], sb[b + k][3]
        if x == y:
            continue
        if first is None:
            first = k
        for p in range(run.pages):
            if x[p] != y[p] and p not in out_pages:
                v = per.get(p)
                per[p] = (k, k, 1) if v is None else (v[0], k, v[2] + 1)
    return first, per, total


def log_lines(path):
    """The quicksave keys and anything a quicksave logs, from a bof3x.log."""
    if not path or not os.path.exists(path):
        return []
    pat = re.compile(r'key f[67]|uicksave|uickload|uick-load|napshot|recipe (done|FAILED)|FATAL', re.I)
    return [l.rstrip() for l in open(path, encoding='utf-8', errors='replace') if pat.search(l)]


def cmd_inproc(a):
    run = HashRun(a.run)
    st = states(run)
    print(f'{a.run}: {len(st)} records, ticks {st[0][0]}..{st[-1][0]}, Frame_Counter {st[0][1]}..{st[-1][1]}')
    for l in log_lines(a.log):
        print(f'  log: {l}')
    back = [i for i in jumps(st) if st[i][1] <= st[i - 1][1]]
    if not back:
        fwd = jumps(st)
        print('RESULT: f7 did nothing - no record where Frame_Counter goes back'
              + (f' (it jumps forward at ticks {[st[i][0] for i in fwd][:5]})' if fwd else '')
              + '; nothing to compare (F6/F7 not effective in this build, or the take was refused)')
        return 2
    rc = 0
    for n, R in enumerate(back):
        fc = st[R][1]
        S = next((i for i in range(R) if st[i][1] == fc), None)
        if S is None:
            print(f'restore {n + 1} at tick {st[R][0]}: Frame_Counter {fc}, which no earlier record held - '
                  'a load of another run\'s quicksave? nothing to compare in this run')
            rc = max(rc, 2)
            continue
        win = R - S if a.window is None else a.window
        nxt = back[n + 1] if n + 1 < len(back) else len(st)
        win = min(win, nxt - R)
        print(f'restore {n + 1}: tick {st[R][0]} (recipe frame {st[R][2]}) holds Frame_Counter {fc}, '
              f'last held at tick {st[S][0]} (recipe frame {st[S][2]}): window {win} ticks')
        first, per, total = compare(run, S, R, st, st, win)
        if first is None:
            print(f'RESULT: equal - the {total} ticks after the restore equal the {total} after the take, tick for tick')
        else:
            rc = 1
            print(f'RESULT: differs - first at k={first}: tick {st[R + first][0]} (recipe frame {st[R + first][2]}) '
                  f'against tick {st[S + first][0]} (recipe frame {st[S + first][2]}); '
                  f'{len(per)} pages over {total} compared ticks (k = ticks after the restore)')
            print_pages(run, per, total)
    if a.plain:
        rc = max(rc, plain_check(run, st, back, a.plain))
    return rc


def plain_check(run, st, back, plain_path):
    """The quick run against the route itself: before the first restore tick
    for tick, after it shifted back by the window."""
    pl = HashRun(plain_path)
    out = statehash.same_headers([pl, run])
    sp = states(pl)
    R = back[0]
    fc = st[R][1]
    S = next(i for i in range(R) if st[i][1] == fc)
    shift = R - S
    f1, per1, t1 = compare(run, 0, 0, st, sp, R, out)
    f2, per2, t2 = compare(run, R, S, st, sp, len(st), out)
    print(f'against the route {plain_path}: before the restore {t1} ticks, '
          + ('equal' if f1 is None else f'first differs at tick {st[f1][0]}'))
    if per1:
        print_pages(run, per1, t1)
    print(f'  after it (shifted by {shift}): {t2} ticks, '
          + ('equal to the route\'s end' if f2 is None else
             f'first differs at tick {st[R + f2][0]} (route tick {sp[S + f2][0]})'))
    if per2:
        print_pages(run, per2, t2)
    return 0 if f1 is None and f2 is None else 1


def cmd_cross(a):
    live, loaded = HashRun(a.live), HashRun(a.loaded)
    out = statehash.same_headers([live, loaded])
    sl, sd = states(live), states(loaded)
    for tag, p in (('live', a.log_live), ('loaded', a.log_loaded)):
        for l in log_lines(p):
            print(f'  {tag} log: {l}')
    jd = jumps(sd)
    if not jd:
        print('RESULT: the loaded run never restored - Frame_Counter runs on without a jump '
              '(F7 did nothing at its first frame: no quicksave file, another build, or not effective yet)')
        return 2
    R = jd[0]
    fc = sd[R][1]
    back = [i for i in jumps(sl) if sl[i][1] <= sl[i - 1][1]]
    cands = [i for i in range(len(sl)) if sl[i][1] == fc]
    if not cands:
        print(f'RESULT: the loaded run restored Frame_Counter {fc} (tick {sd[R][0]}), which the live run never held '
              '- not this live run\'s quicksave')
        return 2
    # the live run's own restore continuation if it has one, else the take
    L = next((i for i in cands if back and i >= back[0]), cands[0])
    print(f'loaded: restore at tick {sd[R][0]} (recipe frame {sd[R][2]}), Frame_Counter {fc}; '
          f'live: tick {sl[L][0]} (recipe frame {sl[L][2]}, {"its own restore" if back and L >= back[0] else "the take"})')
    n = a.frames if a.frames else max(len(sl), len(sd))
    first, per, total = compare(live, L, R, sl, sd, n, out)
    if first is None:
        print(f'RESULT: equal - {total} ticks from the restore, tick for tick')
        return 0
    print(f'RESULT: differs - first at k={first}: loaded tick {sd[R + first][0]} against live tick {sl[L + first][0]}; '
          f'{len(per)} pages over {total} compared ticks')
    print_pages(live, per, total)
    return 1


def cmd_same(a):
    ra, rb = HashRun(a.a), HashRun(a.b)
    out = statehash.same_headers([ra, rb])
    sa, sb = states(ra), states(rb)
    first, per, total = compare(ra, 0, 0, sa, sb, max(len(sa), len(sb)), out)
    lens = f'{len(sa)} and {len(sb)} records'
    if first is None:
        print(f'RESULT: identical on all {total} compared ticks ({lens})')
        return 0
    print(f'RESULT: differs from tick {sa[first][0]} (recipe frame {sa[first][2]}); {len(per)} pages over {total} '
          f'ticks ({lens})')
    print_pages(ra, per, total)
    return 1


# ---------------------------------------------------------------- live runs

def take_lock(path):
    if not path:
        return
    while True:
        try:
            os.mkdir(path)
            print(f'lock {path} taken')
            return
        except FileExistsError:
            print(f'lock {path} held; waiting a minute')
            time.sleep(60)


def cmd_run(a):
    meta = recipe_meta(a.recipe)
    name = os.path.splitext(os.path.basename(a.recipe))[0]
    os.makedirs(a.out, exist_ok=True)
    sh = os.path.join(os.path.abspath(a.out), name + '.sh')
    if os.path.exists(sh):
        os.remove(sh)
    cmd = [sys.executable, os.path.join(TOOLS, 'input_run.py'), a.recipe, '--out', a.out, '--no-front',
           '--lang', a.lang, '--minutes', str(a.minutes),
           '--env', f'BOF3X_STATEHASH={sh}', '--env', f'BOF3X_STATEHASH_SKIP={SKIP_FILE}']
    if not a.defaults:
        cmd += ['--env', 'BOF3X_OPT=none', '--env', 'BOF3X_LAYERING=0']
    cmd += ['--no-save'] if meta and meta[0] == 'cross' else ['--slot0-hold']
    if a.launcher:
        cmd += ['--launcher', a.launcher]
    if a.game:
        cmd += ['--game', a.game]
    for kv in a.env:
        cmd += ['--env', kv]
    take_lock(a.lock)
    try:
        t0 = time.time()
        p = subprocess.run(cmd, capture_output=True, text=True)
        open(os.path.join(a.out, '_run.txt'), 'w').write(' '.join(cmd) + '\n\n' + p.stdout + p.stderr)
        print(p.stdout.strip().splitlines()[-1] if p.stdout.strip() else p.stderr.strip()[-400:])
        print(f'wall {time.time() - t0:.0f} s')
    finally:
        if a.lock:
            os.rmdir(a.lock)
            print(f'lock {a.lock} given back')
    launcher = a.launcher or os.path.join(ROOT, 'build', 'bof3x-launcher.exe')
    log = os.path.join(os.path.dirname(os.path.abspath(launcher)), 'bof3x.log')
    if os.path.exists(log):
        shutil.copy(log, os.path.join(a.out, 'bof3x.log'))
        log = os.path.join(a.out, 'bof3x.log')
    if not os.path.exists(sh):
        print(f'no state hash written ({sh})')
        return 1
    if meta and meta[0] == 'replay':
        return cmd_inproc(argparse.Namespace(run=sh, log=log, plain=a.plain, window=None))
    if a.plain:
        return cmd_same(argparse.Namespace(a=a.plain, b=sh))
    return 0


# ---------------------------------------------------------------- self-test

def write_sh(path, recs, pages=8):
    """A BOF3SH3 file: recs is a list of (Frame_Counter, recipe frame, [page hashes])."""
    with open(path, 'wb') as f:
        f.write(b'BOF3SH3\0' + struct.pack('<4I', statehash.BASE, pages, 0x1000, 0) + struct.pack('<2I', 0, 3))
        prev = None
        for t, (fc, rf, st) in enumerate(recs):
            ch = [(p, h) for p, h in enumerate(st) if prev is None or prev[p] != h]
            f.write(struct.pack('<4I', t, fc, rf, len(ch)))
            for p, h in ch:
                f.write(struct.pack('<2I', p, h))
            prev = st


def synthetic(n_before=50, gap=30, after=60, diverge=None, restore=True):
    """A quick run: frames 0..n_before (the take at n_before), gap more, a
    restore to the take's state, then `after` frames replaying the window's
    states; diverge = k after the restore at which page 5 differs."""
    def state(i):
        return [(i * 7 + p * 13) & 0xFFFF if p in (1, 2, 5) else p for p in range(8)]
    recs = [(i, i, state(i)) for i in range(n_before + gap + 1)]
    if restore:
        for k in range(after):
            s = state(n_before + k)
            if diverge is not None and k >= diverge:
                s = list(s)
                s[5] ^= 0xBEEF
            recs.append((n_before + k, n_before + gap + 1 + k, s))
    else:
        recs += [(i, i, state(i)) for i in range(n_before + gap + 1, n_before + gap + 1 + after)]
    return recs


def cmd_selftest(a):
    d = tempfile.mkdtemp(prefix='qcheck_')
    ok = True

    def expect(what, got, want):
        nonlocal ok
        print(f'{"ok  " if got == want else "FAIL"} {what}: exit {got}, want {want}')
        ok &= got == want

    p = os.path.join(d, 'eq.sh')
    write_sh(p, synthetic())
    expect('equal window', cmd_inproc(argparse.Namespace(run=p, log=None, plain=None, window=None)), 0)
    p = os.path.join(d, 'div.sh')
    write_sh(p, synthetic(diverge=17))
    import io, contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = cmd_inproc(argparse.Namespace(run=p, log=None, plain=None, window=None))
    print(buf.getvalue().rstrip())
    expect('diverging at k=17', rc, 1)
    m = re.search(r'first at k=(\d+): tick (\d+)', buf.getvalue())
    good = bool(m) and int(m[1]) == 17 and int(m[2]) == 50 + 30 + 1 + 17
    print(f'{"ok  " if good else "FAIL"} the reported first differing tick is k=17, tick {50 + 30 + 1 + 17}')
    ok &= good
    good = '0x5DF000' in buf.getvalue()
    print(f'{"ok  " if good else "FAIL"} the differing page is page 5, 0x5DF000')
    ok &= good
    p = os.path.join(d, 'none.sh')
    write_sh(p, synthetic(restore=False))
    expect('no restore (f7 did nothing)', cmd_inproc(argparse.Namespace(run=p, log=None, plain=None, window=None)), 2)
    # the route against the quick run: plain = the frames without the window's detour
    pl = os.path.join(d, 'plain.sh')
    write_sh(pl, synthetic(restore=False))
    expect('plain route against the equal quick run',
           cmd_inproc(argparse.Namespace(run=os.path.join(d, 'eq.sh'), log=None, plain=pl, window=None)), 0)
    # cross: a loaded run - title frames, then a restore to Frame_Counter 50 and the live run's
    # states from its own restore (record 81) on
    eq = synthetic()
    ld = [(i, i, [9] * 8) for i in range(5)] + [(50 + k, 5 + k, eq[81 + k][2]) for k in range(40)]
    lp = os.path.join(d, 'loaded.sh')
    write_sh(lp, ld)
    expect('cross equal', cmd_cross(argparse.Namespace(live=os.path.join(d, 'eq.sh'), loaded=lp, log_live=None,
                                                       log_loaded=None, frames=None)), 0)
    ld2 = [(i, i, [9] * 8) for i in range(45)]
    lp2 = os.path.join(d, 'loaded_none.sh')
    write_sh(lp2, ld2)
    expect('cross, the load did nothing', cmd_cross(argparse.Namespace(live=os.path.join(d, 'eq.sh'), loaded=lp2,
                                                                      log_live=None, log_loaded=None, frames=None)), 2)
    ld3 = [(i, i, [9] * 8) for i in range(5)] + [(50 + k, 5 + k, eq[81 + k][2][:3] + [k] + eq[81 + k][2][4:])
                                                 for k in range(40)]
    lp3 = os.path.join(d, 'loaded_div.sh')
    write_sh(lp3, ld3)
    expect('cross differing (page 3)',
           cmd_cross(argparse.Namespace(live=os.path.join(d, 'eq.sh'), loaded=lp3, log_live=None,
                                        log_loaded=None, frames=None)), 1)
    expect('same: a run against itself', cmd_same(argparse.Namespace(a=pl, b=pl)), 0)
    # a page is named by the skip list's rows on it: 0x6BC000 holds Sound_Banks' row
    fake = argparse.Namespace(base=statehash.BASE, page=0x1000)
    lo, syms, rows = page_name(fake, (0x6BC000 - statehash.BASE) // 0x1000)
    good = any('0x6BC928' in r and 'Sound_Banks' in r for r in rows)
    print(f'{"ok  " if good else "FAIL"} page 0x{lo:06X} named by its skip rows: {rows[:2]}')
    ok &= good
    shutil.rmtree(d, ignore_errors=True)
    print('selftest ' + ('passed' if ok else 'FAILED'))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('make')
    p.add_argument('route')
    p.add_argument('--at', type=int, required=True, help='the recipe frame f6 replaces (an idle frame)')
    p.add_argument('--gap', type=int, default=GAP)
    p.add_argument('--keys-f7', type=int, help='the frame f7 replaces in the _keys form, when the window end holds a button')
    p.add_argument('--name', required=True, help='quick_NAME.txt, quick_NAME_keys.txt, quick_NAME_cross.txt')
    p.add_argument('--why', action='append', default=[], help='a header line saying why this tick')
    p.add_argument('--dir', default=os.path.join(ROOT, 'tools', 'recipes'))
    p.set_defaults(fn=cmd_make)
    p = sub.add_parser('inproc')
    p.add_argument('run')
    p.add_argument('--log')
    p.add_argument('--plain')
    p.add_argument('--window', type=int)
    p.set_defaults(fn=cmd_inproc)
    p = sub.add_parser('cross')
    p.add_argument('live')
    p.add_argument('loaded')
    p.add_argument('--log-live')
    p.add_argument('--log-loaded')
    p.add_argument('--frames', type=int)
    p.set_defaults(fn=cmd_cross)
    p = sub.add_parser('same')
    p.add_argument('a')
    p.add_argument('b')
    p.set_defaults(fn=cmd_same)
    p = sub.add_parser('run')
    p.add_argument('recipe')
    p.add_argument('--out', required=True)
    p.add_argument('--defaults', action='store_true', help="the player's defaults: no BOF3X_OPT / BOF3X_LAYERING")
    p.add_argument('--lock', help='the machine lock directory (mkdir / rmdir)')
    p.add_argument('--plain', help='a hashed run of the route itself, to compare with')
    p.add_argument('--launcher')
    p.add_argument('--game')
    p.add_argument('--lang', default='en-US')
    p.add_argument('--minutes', type=float, default=10)
    p.add_argument('--env', action='append', default=[])
    p.set_defaults(fn=cmd_run)
    p = sub.add_parser('selftest')
    p.set_defaults(fn=cmd_selftest)
    a = ap.parse_args()
    sys.exit(a.fn(a) or 0)


if __name__ == '__main__':
    main()
