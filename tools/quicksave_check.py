#!/usr/bin/env python
"""The quicksave proof by the state hash (docs/quicksave-routes.md,
docs/quicksave-round.md section 2 "RC").

    python tools/quicksave_check.py make  ROUTE.txt --at F --why TEXT --name N   (writes the three forms)
    python tools/quicksave_check.py run   RECIPE.txt --out DIR [--defaults] [--lock DIR] [--plain P.sh]
    python tools/quicksave_check.py inproc RUN.sh [--log bof3x.log] [--plain PLAIN.sh]
    python tools/quicksave_check.py cross LIVE.sh LOADED.sh [--log-live L] [--log-loaded L]
    python tools/quicksave_check.py same  A.sh B.sh [--refb A2.sh]   (the identity reference: keys pressed, nothing changed)
    python tools/quicksave_check.py tier0 ROUTE.txt --at F --out DIR [--host linux --elf E --direct] [--env K=V]
                                          [--take-elf E1 --load-elf E2 --no-doctor] [--take-env K=V] [--load-env K=V]
    python tools/quicksave_check.py matrix TAKES.txt --take T=ELF --load U=ELF [--load 'T!=ELF'] --game G --out DIR
    python tools/quicksave_check.py qsinfo BISLPS10.DAT ...
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
        against the route while F6 and F7 do nothing. --refb A2: a second run
        of the route; a page A and A2 disagree on at a tick is noise there
        (statehash.py check's rule), listed apart and not a difference.
run     One live run through tools/input_run.py with BOF3X_STATEHASH and the
        skip list, the hash conditions (BOF3X_OPT=none BOF3X_LAYERING=0)
        unless --defaults, --slot0-hold and --no-front; then the analysis the
        recipe's form wants (inproc for quick_N, same against --plain for
        _keys). --lock DIR takes the machine's one-live-game lock (mkdir,
        retried a minute apart) for the run and gives it back after.
tier0   A refused quicksave loaded as its save (the state-by-name round's
        A4, docs/state-by-name-tier0.md 5). Four steps, one game at a time:
          take   the route to idle frame F and `key f6` there, then a few
                 frames: BISLPS10.DAT in the run's game folder;
          doctor one byte of the file's build stamp (the version tag)
                 changed, so the build that wrote it refuses it as
                 another build; its save half written apart as a recipe
                 save (T0HALF.DAT) - the bytes the title's load reads;
          load   from the title, F5, confirm on slot 0, Yes: the load is
                 refused and the menu asks whether to load the save; Yes:
                 the restart and the title's load of the save half; then,
                 once Game_Mode is past 1 (the field, a mode-8 area),
                 --after frames and the peeks;
          ref    the same save half as slot 0 through the title's own load
                 menu (combat.txt's presses: start, start, r2 three times), then the
                 same wait and peeks.
        The two runs' peeks (the area, the leader's position, the came-from
        place, the play clock) and their last shots are compared; the load
        run's log must show the refusal, the question, the restart, the
        title's state 7 and the load menu's read of the save half.
        --take-elf / --load-elf: the take by one build, the load and the reference
        by another; with --no-doctor the stamp is left as taken, so the refusal
        is the loading build's own (the state-by-name proof's older build).
matrix  The state-by-name proof (docs/state-by-name-proof.md): each take of
        TAKES (NAME ROUTE FC [keep=N] [lang=TAG|none] [# why]: the route to
        recipe frame FC-1, made idle when it holds a button, the take there,
        KEEP frames more) played by the --take build with the state hash, the
        file it wrote read (portable or not, the TASKS record), and loaded at
        frame 0 by each --load build in a copy of the take's run folder, its
        hash compared with the live run's from the restore (cross). A label
        ending in ! loads forced portable (BOF3X_QSPORTABLE=1). Each row of
        --results (tab-separated, a header line): the take, the builds, the
        file, the answer the loading build logged (EXACT, PORTABLE, REFUSED and
        why), the comparison. A build is an ELF (--host linux --direct) or a
        launcher copy's .exe (--host exe); --take-env / --load-env cross a
        setting. --resume skips rows the results file has.
qsinfo  A quicksave's header, format-2 extension, TASKS record and settings.
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
        o = self.header   # where the records start, by version (a BOF3SH4 or BOF3SH5 file raised KeyError here)
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
    out = []
    for l in open(path, encoding='utf-8', errors='replace'):
        if not pat.search(l):
            continue
        l = l.rstrip()
        if out and out[-1][0] == l:      # the overlay logs its line every frame it shows
            out[-1][1] += 1
        else:
            out.append([l, 1])
    return [l if n == 1 else f'{l}   (x{n})' for l, n in out]


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
        exp, per = split_expected(run, per, getattr(a, 'expect', None))
        if exp:
            print(f'expected (by the --expect rules, each a page and a window of k): {len(exp)} pages')
            print_pages(run, exp, total)
        first = min((v[0] for v in per.values()), default=None)
        if first is None:
            print(f'RESULT: equal - the {total} ticks after the restore equal the {total} after the take, tick for tick'
                  + (', but for the expected pages above' if exp else ''))
        else:
            rc = 1
            print(f'RESULT: differs - first at k={first}: tick {st[R + first][0]} (recipe frame {st[R + first][2]}) '
                  f'against tick {st[S + first][0]} (recipe frame {st[S + first][2]}); '
                  f'{len(per)} pages over {total} compared ticks (k = ticks after the restore)')
            print_pages(run, per, total)
    if a.plain:
        rc = max(rc, plain_check(run, st, back, a.plain))
    return rc


# What a correct restore still changes, by design, measured by raw dumps
# (docs/quicksave-routes.md, stage C 2): outside the skip list only
# Overlay_Frames (0x6BC630) differs on page 0x6BC000 - SN's "Quickload" line
# is set after the span, its count one to three behind the take's "Quicksave
# OK" and counted down on drawn frames only, so late ticks stretch it past 120
# (132 seen with 13 late ticks) - and only Gpu_DispEnv+2 / Gpu_DrawEnv+2
# (0x7DECE2, 0x7DED02, the display's buffer parity) on page 0x7DE000 at k=1.
# A page rule, not a byte rule - the hash is per page - so each holds only
# inside its window of k, and a dump is the way to see more.
EXPECT_DEFAULT = ['0x6BC000:0-140', '0x7DE000:0-1']


def parse_expect(rules):
    out = []
    for r in rules or []:
        page, _, win = r.partition(':')
        lo, _, hi = win.partition('-')
        out.append((int(page, 0), int(lo), int(hi or lo)))
    return out


def split_expected(run, per, rules):
    """(expected, the rest) of per: a page whose every differing k lies in
    one rule's window for that page is expected."""
    exp, rest = {}, {}
    rules = parse_expect(rules)
    for p, (f, l, c) in per.items():
        addr = run.base + p * run.page
        if any(addr == pa and lo <= f and l <= hi for pa, lo, hi in rules):
            exp[p] = (f, l, c)
        else:
            rest[p] = (f, l, c)
    return exp, rest


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
    exp, per = split_expected(live, per, getattr(a, 'expect', None))
    if exp:
        print(f'expected (by the --expect rules): {len(exp)} pages')
        print_pages(live, exp, total)
    first = min((v[0] for v in per.values()), default=None)
    if first is None:
        print(f'RESULT: equal - {total} ticks from the restore, tick for tick'
              + (', but for the expected pages above' if exp else ''))
        return 0
    print(f'RESULT: differs - first at k={first}: loaded tick {sd[R + first][0]} against live tick {sl[L + first][0]}; '
          f'{len(per)} pages over {total} compared ticks')
    print_pages(live, per, total)
    return 1


def cmd_same(a):
    ra, rb = HashRun(a.a), HashRun(a.b)
    rr = HashRun(a.refb) if getattr(a, 'refb', None) else None
    out = statehash.same_headers([ra, rr, rb] if rr else [ra, rb])
    sa, sb = states(ra), states(rb)
    sr = states(rr) if rr else None
    n = min(len(sa), len(sb), len(sr) if sr else len(sb))
    per, noise, first = {}, {}, None
    for t in range(n):
        x, y = sa[t][3], sb[t][3]
        if x == y:
            continue
        for p in range(ra.pages):
            if x[p] == y[p] or p in out:
                continue
            # with --refb: a page the two runs of A's side disagree on at this tick is noise there
            d = noise if sr is not None and sr[t][3][p] != x[p] else per
            v = d.get(p)
            d[p] = (t, t, 1) if v is None else (v[0], t, v[2] + 1)
            if d is per and first is None:
                first = t
    lens = f'{len(sa)} and {len(sb)} records' + (f', the second reference {len(sr)}' if sr else '')
    if noise:
        print(f'noise (the two references disagree there too): {len(noise)} pages')
        print_pages(ra, noise, n)
    if first is None:
        print(f'RESULT: identical on all {n} compared ticks ({lens})'
              + (' where the references agree' if sr else ''))
        return 0
    print(f'RESULT: differs from tick {sa[first][0]} (recipe frame {sa[first][2]}); {len(per)} pages over {n} '
          f'ticks ({lens})')
    print_pages(ra, per, n)
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
    if a.save:      # a cross run given the live run's recipe save, held in slot 0 as the live run held it
        cmd += ['--save', a.save, '--slot0-hold']
    else:
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
    if not meta or meta[0] == 'replay':      # a proof form, or a recipe of its own (quick_menu.txt)
        return cmd_inproc(argparse.Namespace(run=sh, log=log, plain=a.plain, window=None, expect=a.expect))
    if a.plain:
        return cmd_same(argparse.Namespace(a=a.plain, b=sh, refb=None))
    return 0


# ---------------------------------------------------------------- tier 0

# Peeked once the game is up (docs/state-by-name-tier0.md 5): u16 Game_AreaNumber;
# the leader's x and z (ObjTrio +0x34, +0x38, what Save_BuildBlock saves);
# the came-from place (AreaW2c_LastArea, 0x7E091C, 0x7E0920); the play clock.
TIER0_PEEKS = [('0x904EFC', 'u16', 'area'), ('0x802D74', 'u32', 'x'), ('0x802D78', 'u32', 'z'),
               ('0x802290', 'u16', 'came_from'), ('0x7E091C', 'u32', 'came_x'), ('0x7E0920', 'u32', 'came_z'),
               ('0x6BC63C', 'u8', 'restart_flag')]
GAME_MODE = '0x66C7E8'
STAMP_AT = 0x12B0 + 8 + 4 * 4      # the version tag in the header (docs/quicksave.md 4)
SAVE_HALF = 0x12B0


def tier0_tail(after):
    # any mode past the start and the entry (0, 1): the field (2), a mode-8 area (8), ...
    out = [f'until {GAME_MODE} u16 & 0xE timeout 5400', f'wait {after}']
    out += [f'peek {a} {t} t0_{n}' for a, t, n in TIER0_PEEKS]
    out += ['shot t0_up 1', 'wait 2']
    return out


def run_input(a, recipe, out, extra, saves=None, elf=None, env=()):
    elf = elf or a.elf
    cmd = [sys.executable, os.path.join(TOOLS, 'input_run.py'), recipe, '--out', out, '--no-front',
           '--lang', a.lang, '--minutes', str(a.minutes)]
    if a.host:
        cmd += ['--host', a.host]
    if elf:
        cmd += ['--elf', elf]
    if a.direct:
        cmd += ['--direct']
    for kv in list(a.env) + list(env):
        cmd += ['--env', kv]
    cmd += extra
    os.makedirs(out, exist_ok=True)
    log = linux_log(elf) if a.host == 'linux' else None
    if log and os.path.exists(log):
        os.remove(log)      # the run's own log, never the last run's
    env = dict(os.environ)
    if saves:               # the harness reads it for the recipe save (input_run.py linux_save)
        env['BOF3X_RECIPE_SAVES'] = saves
    t0 = time.time()
    p = subprocess.run(cmd, capture_output=True, text=True, env=env)
    open(os.path.join(out, '_run.txt'), 'w').write(' '.join(cmd) + '\n\n' + p.stdout + p.stderr)
    tail = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else p.stderr.strip()[-400:]
    print(f'  {os.path.basename(recipe)}: exit {p.returncode}, {time.time() - t0:.0f} s - {tail}')
    if log and os.path.exists(log):
        shutil.copy(log, os.path.join(out, 'bof3x.log'))
    return p.returncode, os.path.join(out, 'bof3x.log')


def linux_log(elf):
    import linux_run
    return linux_run.local_path(linux_run.log_of(elf))


def cmd_tier0(a):
    header, r = read_route(a.route)
    f = a.at
    if f >= len(r) or r[f] != ('', None):
        sys.exit(f'{a.route}: frame {f} is not an idle frame (idle runs near it: {idle_runs(r, max(0, f - 60), f + 60)})')
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    save = [h for h in header if re.match(r'#\s*save\s', h)]
    # 1. the take
    take = os.path.join(out, 't0_take.txt')
    lines = [f'# tier0 take at {f} from {os.path.basename(a.route)} (quicksave_check.py tier0)'] + save
    emit(r[:f], lines)
    lines += [f'peek {GAME_MODE} u16 t0_take_mode', 'peek 0x904EFC u16 t0_take_area']   # no frame of their own
    emit([('KEY', 'f6')], lines)
    lines += ['wait 30']
    open(take, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('take:')
    inst = ['--game', a.game] if a.game else []
    rc, log = run_input(a, take, os.path.join(out, 'take'), inst + (['--slot0-hold'] if save else ['--no-save']),
                        elf=a.take_elf, env=a.take_env)
    game = os.path.join(out, 'take', 'game')
    qs = os.path.join(game, 'BISLPS10.DAT')
    if not os.path.exists(qs):
        print(f'no quicksave in {game}: the take did not happen')
        return 1
    # 2. the stamp doctored; the save half apart
    b = bytearray(open(qs, 'rb').read())
    shutil.copy(qs, qs + '.orig')
    if not a.no_doctor:
        b[STAMP_AT] ^= 0x20
        open(qs, 'wb').write(b)
    halves = os.path.join(out, 'half')
    os.makedirs(halves, exist_ok=True)
    open(os.path.join(halves, 'T0HALF.DAT'), 'wb').write(bytes(b[:SAVE_HALF]))
    print(f'doctor: {qs}, ' + ('left as taken (--no-doctor: the loading build refuses it by itself)' if a.no_doctor
                               else f'byte 0x{STAMP_AT:X} of the build stamp changed')
          + f'; save half {len(b[:SAVE_HALF])} bytes')
    # 3. the load: F5, confirm on slot 0, Yes (refused), Yes (the save)
    load = os.path.join(out, 't0_load.txt')
    lines = ['# tier0 load: F5 on the title, slot 0, Yes, then Yes to the save (quicksave_check.py tier0)',
             'wait 150', 'key f5', 'wait 20', 'hold r2 3', 'wait 20', 'hold r2 3', 'wait 30', 'shot t0_question 1',
             'wait 10', 'hold r2 3', 'every 300'] + tier0_tail(a.after)
    open(load, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('load:')
    rc_load, log_load = run_input(a, load, os.path.join(out, 'load'), ['--run-game', 'none', '--game', game, '--no-save'],
                                  elf=a.load_elf, env=a.load_env)
    ans, why = load_answer(log_load)
    print(f'  the answer: {ans} {why}')
    # 4. the reference: the save half through the title's menu
    ref = os.path.join(out, 't0_ref.txt')
    lines = ['# tier0 reference: the save half as slot 0, through the title\'s load menu (quicksave_check.py tier0)',
             '# save T0HALF', 'wait 59', 'hold start 5', 'wait 95', 'hold start 6', 'wait 43', 'hold r2 3', 'wait 59',
             'hold r2 5', 'wait 25', 'hold r2 5'] + tier0_tail(a.after)
    open(ref, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('ref:')
    rc_ref, log_ref = run_input(a, ref, os.path.join(out, 'ref'),
                                ['--slot0-hold'] + inst, saves=halves, elf=a.load_elf, env=a.load_env)
    # the comparison
    want = ['refused', 'its save half is whole', 'the restart asked', 'state 7, the title', 'load menu\'s read',
            'loaded the save half']
    seen = open(log_load, errors='replace').read() if os.path.exists(log_load) else ''
    road = [(w, w in seen) for w in want]
    pl, pr = peeks_of(log_load), peeks_of(log_ref)
    pt = peeks_of(log)
    print(f'the take: mode {pt.get("t0_take_mode")}, area {pt.get("t0_take_area")}')
    print('the road (the load run\'s log):')
    for w, ok in road:
        print(f'  {"yes" if ok else "NO "}  {w}')
    print('where it stands:            load            ref')
    same = True
    for _, _, n in TIER0_PEEKS:
        x, y = pl.get('t0_' + n), pr.get('t0_' + n)
        # equal and read; an area 0 is no game (a take on the title comes up there)
        same = same and x is not None and x == y and not (n == 'area' and x == '0x0')
        print(f'  {n:12s} {str(x):>16s} {str(y):>16s}  {"missing" if x is None or y is None else "equal" if x == y else "DIFFERENT"}')
    shots = [os.path.join(out, k) for k in ('load', 'ref')]
    pics = [shot_bytes(d, 't0_up') for d in shots]
    pic = pics[0] is not None and pics[0] == pics[1]
    print(f'  the picture t0_up: {"equal" if pic else "different" if all(pics) else "missing"}')
    ok = all(o for _, o in road) and same
    print('tier0: ' + ('the save half came up where the title\'s load puts it' if ok else 'NOT as the title\'s load'))
    return 0 if ok else 1


def peeks_of(log):
    """{label: value} of the log's `input       peek LABEL = 0xV (D) recipe frame N` lines (input_script.cpp)."""
    got = {}
    if not os.path.exists(log):
        return got
    for line in open(log, errors='replace'):
        m = re.search(r'input\s+peek (t0_\w+) = (0x[0-9A-Fa-f]+)', line)
        if m:
            got[m[1]] = m[2]
    return got


def shot_bytes(d, name):
    for ext in ('.png', '.bmp'):
        p = os.path.join(d, name + ext)
        if os.path.exists(p):
            return open(p, 'rb').read()
    return None


# ---------------------------------------------------------------- the matrix (state by name, B3)
#
# The state-by-name round's proof (docs/state-by-name-proof.md): a quicksave
# taken by one build, loaded at frame 0 by another, both runs hashed and
# compared tick for tick. One take a line of a takes file; each take played
# once by the taking build and loaded by every loading build named.

# Peeked on the take's frame, before the key (a peek has no frame of its own):
# where the game is, for the results (symbols.toml's names).
TAKE_PEEKS = [('0x66C7E8', 'u16', 'mode'), ('0x66C7EA', 'u16', 'step'), ('0x66C828', 'u16', 'kind'),
              ('0x904EFC', 'u16', 'area'), ('0x905B60', 'u8', 'camp')]
QS_HALF = 0x12B0
TASK_KIND = {0: 'free', 1: 'new', 2: 'site', 3: 'nowhere'}
NOT_PORTABLE = {0: 'portable', 1: 'taken by the hosted build', 2: 'the code places lie under an unmeasured top byte',
                3: 'a task stands at no named place', 4: "a new task's entry names no VA",
                5: 'a slot holds a function of ours', 6: 'a message slot names our module'}
# The proof's skip list: the state hash's, and the words the proof measured as
# naming the build that wrote them (docs/state-by-name-proof.md 3). Both sides of
# every comparison of the matrix hash under this list, so their headers agree.
PROOF_SKIP_EXTRA = [
    (0x6BC62C, 4, 'ours   Overlay_Text: after F6 / F7 / the quick menu it names our line buffer (snapshot.cpp Show, '
                  'quick_menu.cpp g_f7_line), an address in our module that each build links elsewhere'),
    (0x7DEE4C, 12, 'ours   Msgbox_Base, MenuWindows_ListText, Msgbox_Resume: text in our message pool (DIV-0007), '
                   'which the quicksave writes by its offset (span_words.h kTextWords) and the hash reads raw'),
    (0x93B8FC, 4, 'ours   BattleFlow_TaskCurrent+0x38: the battle block\'s text word, as above (kTextWords)'),
]


def proof_skip(out):
    """The skip list for the matrix's runs: tools/statehash_skip.txt and PROOF_SKIP_EXTRA, written into out."""
    p = os.path.join(out, 'proof_skip.txt')
    text = open(SKIP_FILE, encoding='utf-8').read().rstrip('\n') + '\n# The proof (quicksave_check.py matrix):\n'
    text += ''.join(f'0x{a:06X} {n:<8d}# {why}\n' for a, n, why in PROOF_SKIP_EXTRA)
    open(p, 'w', encoding='utf-8', newline='\n').write(text)
    return p


RESULT_COLS = ['take', 'route', 'fc', 'by', 'loaded_by', 'forced', 'taken_fc', 'format', 'portable', 'why_not',
               'tasks', 'settings_fnv', 'bound', 'peeks', 'answer', 'reason', 'cross', 'ticks', 'first_k', 'pages',
               'exit']


def read_quicksave(path):
    """A quicksave's header, format-2 extension, TASKS record and settings
    lines (src/hook/snapshot.cpp's Header, HeaderExt, TaskRec; little-endian).
    The game's own bytes are not read: the header, our record and our lines."""
    b = open(path, 'rb').read()
    h = QS_HALF
    out = {'bytes': len(b)}
    if len(b) < h + 0xA0 or b[h:h + 8] != b'BOF3XQS\0':
        out['format'] = None
        return out
    fmt, hb, fb, host = struct.unpack_from('<4I', b, h + 8)
    out.update(format=fmt, file_bytes=fb, host=host,
               tag=b[h + 24:h + 56].split(b'\0')[0].decode('ascii', 'replace'),
               sha=b[h + 56:h + 120].decode('ascii', 'replace'))
    out['dll_base'], out['fc'] = struct.unpack_from('<2I', b, h + 120)
    sb, sn, pb, pn, at, an = struct.unpack_from('<6I', b, h + 128)
    if fmt != 2:
        return out
    x = struct.unpack_from('<12I', b, h + 0xA0)
    out.update(revision=x[0], portable=x[1], why_not=x[2], settings_fnv=x[3], words=x[6], table_lo=x[7],
               bind_rows=x[8], bind_fnv=x[9], image_base=x[10], pointer_bytes=x[11])
    o = h + hb + sn + pn + an
    tasks = []
    for k in range(4):
        kind, name, nb = struct.unpack_from('<3I', b, o + 28 * k)
        tasks.append((TASK_KIND.get(kind, str(kind)), name, b[o + 28 * k + 12:o + 28 * k + 12 + nb].hex()))
    out['tasks'] = tasks
    s = o + x[5]
    out['settings'] = b[s:s + x[4]].decode('utf-8', 'replace').splitlines()
    return out


def tasks_text(q):
    """'0 site 9 (01000000 00000000), 1 free, ...' - the TASKS record in short."""
    if not q.get('tasks'):
        return '-'
    parts = []
    for k, (kind, name, loc) in enumerate(q['tasks']):
        if kind == 'site':
            parts.append(f'{k} site {name}' + (f' ({loc})' if loc else ''))
        elif kind == 'new':
            parts.append(f'{k} new 0x{name:06X}')
        else:
            parts.append(f'{k} {kind}')
    return ', '.join(parts)


def why_not_text(q):
    w = q.get('why_not')
    if w is None:
        return '-'
    if not w:
        return '-'
    t = NOT_PORTABLE.get(w & 0xFF, f'code {w & 0xFF}')
    return t + (f' (task {w >> 8})' if (w & 0xFF) in (3, 4) else '')


def load_answer(log):
    """(answer, reason) from a loaded run's log: EXACT, PORTABLE, or REFUSED and
    the reason as the log has it (with whether the save half is offered)."""
    if not log or not os.path.exists(log):
        return 'no log', ''
    for line in open(log, encoding='utf-8', errors='replace'):
        m = re.search(r'quicksave\s+F7 \S+ restored PORTABLE at Frame_Counter \d+: back to Frame_Counter (\d+).*'
                      r'tasks (.*)\)', line)
        if m:
            return 'PORTABLE', f'FC {m[1]}; tasks {m[2]}'
        m = re.search(r'quicksave\s+F7 \S+ restored at Frame_Counter \d+: back to Frame_Counter (\d+)', line)
        if m:
            return 'EXACT', f'FC {m[1]}'
        m = re.search(r'quicksave\s+F7 (?:\S+ )?refused: (.*)', line)
        if m:
            return 'REFUSED', m[1].strip()
        m = re.search(r'FATAL[: ](.*)', line)
        if m:
            return 'FATAL', m[1].strip()[:300]
    return 'none', 'no F7 line in the log'


def parse_build(spec):
    """LABEL=PATH, LABEL!=PATH (the load forced portable, BOF3X_QSPORTABLE=1).
    A PATH ending in .exe is a Windows launcher copy (input_run --launcher); any
    other path a Linux ELF (--host linux --direct --elf; from Windows inside the
    WSL), or with @launcher appended, the ELF started by the bof3x-launcher
    beside it (--no-config: its defaults and preset, as a Windows launcher copy's)."""
    label, _, path = spec.partition('=')
    if not path:
        raise SystemExit(f'a build is LABEL=PATH: {spec}')
    forced = label.endswith('!')
    label = label.rstrip('!')
    launcher = path.endswith('@launcher')     # an ELF started by the bof3x-launcher beside it (--no-config)
    path = path[:-len('@launcher')] if launcher else path
    exe = path.lower().endswith('.exe')
    return argparse.Namespace(label=label + ('!' if forced else ''), name=label, path=path, forced=forced, exe=exe,
                              launcher=launcher)


class HeavyLock:
    """--lock-file: an flock held for one game run and the copy of its log (a
    shared ELF's log is rewritten by the next run of it); --lock DIR: the
    Windows runs' mkdir lock (take_lock)."""

    def __init__(self, a):
        self.a, self.f = a, None

    def __enter__(self):
        if getattr(self.a, 'lock_file', None):
            import fcntl
            self.f = open(self.a.lock_file, 'a')
            fcntl.flock(self.f, fcntl.LOCK_EX)
        take_lock(getattr(self.a, 'lock', None))
        return self

    def __exit__(self, *exc):
        if getattr(self.a, 'lock', None):
            os.rmdir(self.a.lock)
        if self.f:
            self.f.close()


def game_run(a, build, recipe, out, extra, env, lang):
    """One hashed run of recipe by build (a parse_build). Returns (rc, log)."""
    cmd = [sys.executable, os.path.join(TOOLS, 'input_run.py'), recipe, '--out', out, '--no-front',
           '--minutes', str(a.minutes)]
    if lang and lang != 'none':
        cmd += ['--lang', lang]
    if build.exe:
        cmd += ['--host', 'exe', '--launcher', build.path]
        log = os.path.join(os.path.dirname(os.path.abspath(build.path)), 'bof3x.log')
    else:
        import linux_run
        cmd += ['--host', 'linux', '--elf', build.path]
        # The ELF's path is the WSL's when this is Windows: its folder is cut at '/', never joined with '\'.
        cmd += ['--launcher', build.path.rpartition('/')[0] + '/bof3x-launcher'] if build.launcher else ['--direct']
        log = linux_run.local_path(linux_run.log_of(build.path))
    for kv in env:
        cmd += ['--env', kv]
    cmd += extra
    os.makedirs(out, exist_ok=True)
    with HeavyLock(a):
        if os.path.exists(log):
            os.remove(log)
        t0 = time.time()
        p = subprocess.run(cmd, capture_output=True, text=True)
        open(os.path.join(out, '_run.txt'), 'w').write(' '.join(cmd) + '\n\n' + p.stdout + p.stderr)
        mine = os.path.join(out, 'bof3x.log')
        if os.path.exists(log):
            shutil.copy(log, mine)
    tail = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else p.stderr.strip()[-300:]
    print(f'    {build.label} {os.path.basename(recipe)}: exit {p.returncode}, {time.time() - t0:.0f} s - {tail}')
    return p.returncode, mine


def read_takes(path):
    """NAME ROUTE FC [keep=N] [lang=TAG|none] [# why]: one take a line. FC is the
    Frame_Counter the take holds (recipe frame FC-1, which is made idle)."""
    takes = []
    for line in open(path, encoding='utf-8'):
        body, _, why = line.partition('#')
        w = body.split()
        if not w:
            continue
        if len(w) < 3:
            raise SystemExit(f'{path}: a take is NAME ROUTE FC [keep=N] [lang=TAG]: {line.strip()}')
        t = argparse.Namespace(name=w[0], route=w[1], fc=int(w[2]), keep=None, lang=None, why=why.strip())
        for kv in w[3:]:
            k, _, v = kv.partition('=')
            if k == 'keep':
                t.keep = int(v)
            elif k == 'lang':
                t.lang = v
            else:
                raise SystemExit(f'{path}: {kv}: keep= or lang=')
        takes.append(t)
    return takes


def take_recipes(route, f, keep, d, name):
    """The take form (the route to recipe frame f, made idle if it held a button,
    the peeks, `key f6` in place of it, the route on for keep frames) and the
    cross form (`key f7` at frame 0, then the same frames from f+1)."""
    header, r = read_route(route)
    if f >= len(r):
        raise SystemExit(f'{route}: {len(r)} frames; the take at recipe frame {f} is past its end')
    r = r[:f + keep]
    made_idle = r[f] != ('', None)
    if r[f][0] == 'KEY' or r[f][1]:
        raise SystemExit(f'{route}: recipe frame {f} is a shot or a key: take another frame')
    r[f] = ('', None)
    save = [h for h in header if re.match(r'#\s*save\s', h)]
    src = os.path.basename(route)
    note = ' (it held a button, made idle on both sides)' if made_idle else ''
    lines = [f'# quick take f6 {f} f7 0 gap 0 from {src}',
             f'# The state-by-name proof (quicksave_check.py matrix): {src} to recipe frame {f}{note}, the peeks,',
             f'# f6 in place of frame {f}, then the route on to frame {f + keep - 1}.'] + save
    emit(r[:f], lines)
    lines += [f'peek {addr} {ty} qs_{n}' for addr, ty, n in TAKE_PEEKS]
    emit([('KEY', 'f6')] + r[f + 1:], lines)
    tp = os.path.join(d, f'take_{name}.txt')
    open(tp, 'w', newline='\n').write('\n'.join(lines) + '\n')
    lines = [f'# quick cross f6 {f} f7 0 gap 0 from {src}',
             f'# The loaded side of take_{name}.txt: f7 at recipe frame 0, then {src} from frame {f + 1} to {f + keep - 1}.']
    emit([('KEY', 'f7')] + r[f + 1:], lines, '_x')
    cp = os.path.join(d, f'cross_{name}.txt')
    open(cp, 'w', newline='\n').write('\n'.join(lines) + '\n')
    return tp, cp, made_idle


def read_route_peeks(path):
    """read_route of a take form: its peek lines (no frame of their own) left out."""
    text = ''.join(l for l in open(path) if not l.startswith('peek '))
    tmp = tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False)
    tmp.write(text)
    tmp.close()
    try:
        return read_route(tmp.name)[1]
    finally:
        os.remove(tmp.name)


def take_peeks(log):
    got = {}
    if log and os.path.exists(log):
        for line in open(log, errors='replace'):
            m = re.search(r'input\s+peek qs_(\w+) = (0x[0-9A-Fa-f]+)', line)
            if m:
                got[m[1]] = int(m[2], 16)
    return ' '.join(f'{k}={v}' for k, v in got.items())


def cross_quiet(live, loaded, log_live, log_loaded, out):
    """cmd_cross into a file: (exit, RESULT line, ticks, first k, pages)."""
    import io, contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        try:
            rc = cmd_cross(argparse.Namespace(live=live, loaded=loaded, log_live=log_live, log_loaded=log_loaded,
                                              frames=None, expect=[]))
        except Exception as e:      # a run that wrote no hash file
            print(f'RESULT: no comparison ({type(e).__name__}: {e})')
            rc = 3
    open(out, 'w').write(buf.getvalue())
    res = next((l for l in buf.getvalue().splitlines() if l.startswith('RESULT')), 'RESULT: none')
    m = re.search(r'equal - (\d+) ticks', res)
    ticks = int(m[1]) if m else ''
    m = re.search(r'first at k=(\d+).* (\d+) pages over (\d+)', res)
    first, pages = (int(m[1]), int(m[2])) if m else ('', '')
    if m:
        ticks = int(m[3])
    return rc, res, ticks, first, pages


def results_rows(path):
    if not os.path.exists(path):
        return []
    rows = [l.rstrip('\n').split('\t') for l in open(path, encoding='utf-8')]
    return [dict(zip(rows[0], r)) for r in rows[1:]] if rows else []


def cmd_matrix(a):
    takes = read_takes(a.takes)
    if a.only:
        want = set(a.only.split(','))
        takes = [t for t in takes if t.name in want]
    tb = parse_build(a.take)
    loads = [parse_build(s) for s in a.load]
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    res = os.path.join(out, a.results)
    done = {(r['take'], r['by'], r['loaded_by']) for r in results_rows(res)} if a.resume else set()
    if not os.path.exists(res) or not os.path.getsize(res) or not a.resume:
        open(res, 'w', encoding='utf-8').write('\t'.join(RESULT_COLS) + '\n')
    hashenv = [] if a.defaults else ['BOF3X_OPT=none', 'BOF3X_LAYERING=0']
    skip = SKIP_FILE if a.hash_skip_only else proof_skip(out)
    common = [f'BOF3X_STATEHASH_SKIP={skip}'] + hashenv + a.env
    worst = 0
    for t in takes:
        name = f'{t.name}_{tb.name}'
        d = os.path.join(out, name)
        os.makedirs(d, exist_ok=True)
        route = t.route if os.path.exists(t.route) else os.path.join(ROOT, 'tools', 'recipes', t.route + '.txt')
        lang = t.lang or a.lang
        keep = t.keep or a.keep
        try:
            tp, cp, idle = take_recipes(route, t.fc - 1, keep, d, t.name)
        except SystemExit as e:     # a route this cannot cut (a hand-written recipe: press / until), a frame past its end
            print(f'take {t.name}: not made - {e}')
            with open(res, 'a', encoding='utf-8') as f:
                f.write('\t'.join(str(x) for x in (t.name, os.path.basename(route)[:-4], t.fc, tb.label, '', '', '', '',
                                                  '', '', '', '', '', '', 'no take', str(e).replace('\t', ' '), '', '',
                                                  '', '', 3)) + '\n')
            continue
        todo = [lb for lb in loads if (t.name, tb.label, lb.label) not in done]
        if not todo:
            continue
        print(f'take {t.name}: {os.path.basename(route)} at Frame_Counter {t.fc} by {tb.label}'
              + (' (the frame made idle)' if idle else '') + (f' - {t.why}' if t.why else ''))
        live_sh = os.path.join(d, 'live.sh')
        qs = os.path.join(d, 'BISLPS10.DAT')
        if not (a.resume and os.path.exists(live_sh) and os.path.exists(qs)):
            game = a.game_take or a.game
            extra = ['--game', game]
            if not tb.exe:
                extra += ['--only-slot0', '--run-game', os.path.join(d, 'live_game')]
            else:
                extra += ['--slot0-hold']
            rc, log = game_run(a, tb, tp, os.path.join(d, 'live'), extra,
                               common + a.take_env + [f'BOF3X_STATEHASH={live_sh}'], lang)
            made = os.path.join(d, 'live_game', 'BISLPS10.DAT') if not tb.exe else os.path.join(game, 'BISLPS10.DAT')
            if os.path.exists(made):
                shutil.move(made, qs) if tb.exe else shutil.copy(made, qs)
        log_live = os.path.join(d, 'live', 'bof3x.log')
        peeks = take_peeks(log_live)
        q = read_quicksave(qs) if os.path.exists(qs) else {}
        if not q:
            print(f'  no quicksave written (see {log_live})')
        else:
            print(f'  the file: format {q.get("format")}, FC {q.get("fc")}, '
                  f'{"portable" if q.get("portable") else "not portable: " + why_not_text(q)}; {tasks_text(q)}; {peeks}')
        for lb in todo:
            ld = os.path.join(d, f'load_{lb.label.rstrip("!")}{"_forced" if lb.forced else ""}')
            sh = ld + '.sh'
            row = dict(take=t.name, route=os.path.basename(route)[:-4], fc=t.fc, by=tb.label, loaded_by=lb.label,
                       forced=int(lb.forced), taken_fc=q.get('fc', ''), format=q.get('format', ''),
                       portable=q.get('portable', ''), why_not=why_not_text(q), tasks=tasks_text(q),
                       settings_fnv=f'{q["settings_fnv"]:08X}' if 'settings_fnv' in q else '',
                       bound=f'{q.get("bind_rows")}/{q["bind_fnv"]:08X}' if 'bind_fnv' in q else '', peeks=peeks)
            if q:
                env = common + a.load_env + [f'BOF3X_STATEHASH={sh}'] + (['BOF3X_QSPORTABLE=1'] if lb.forced else [])
                if lb.exe:
                    gdir = a.game_load or a.game
                    shutil.copy(qs, os.path.join(gdir, 'BISLPS10.DAT'))
                    extra = ['--game', gdir, '--no-save']
                else:
                    import linux_run
                    src = ld + '_game'
                    live_game = os.path.join(d, 'live_game')
                    if linux_run.ON_WINDOWS:
                        # From Windows the folder's links are the WSL's to make (os.symlink wants a privilege
                        # here; measured 2026-11-26, WinError 1314), and a folder of them is the WSL's to remove:
                        # so a run folder from the install through the helper, as input_run.py makes the take's,
                        # for a Linux take's load too (the take's own folder is not copied).
                        helper = linux_run.LinuxGame(lb.path, a.game_load or a.game, {}, os.devnull)
                        got = helper._ask('mkgame', linux_run.to_linux(a.game_load or a.game), linux_run.to_linux(src),
                                          None, True)
                        helper.close()
                        if not got:
                            raise SystemExit(f'the WSL could not make the game folder of the load, {src}')
                    else:
                        if os.path.lexists(src):
                            shutil.rmtree(src)
                        if os.path.isdir(live_game):
                            shutil.copytree(live_game, src, symlinks=True,
                                            ignore=shutil.ignore_patterns('BISLPS1*', 'bof3x.log'))
                        else:      # a take by a Windows build: a run folder from the install, as --only-slot0 makes it
                            linux_run.make_run_game(a.game_load or a.game, src, None, True)
                    shutil.copy(qs, os.path.join(src, 'BISLPS10.DAT'))
                    extra = ['--game', src, '--run-game', 'none', '--no-save']
                if os.path.exists(sh):
                    os.remove(sh)
                rc, log = game_run(a, lb, cp, ld, extra, env, lang)
                if lb.exe:
                    try:
                        os.remove(os.path.join(gdir, 'BISLPS10.DAT'))
                    except OSError:
                        pass
                ans, why = load_answer(log)
                crc, cres, ticks, first, pages = cross_quiet(live_sh, sh, log_live, log, ld + '_cross.txt')
                row.update(answer=ans, reason=why, cross=cres.replace('RESULT: ', ''), ticks=ticks, first_k=first,
                           pages=pages, exit=crc)
                print(f'  loaded by {lb.label}: {ans} {why[:160]}')
                print(f'    cross: {cres}')
                if crc == 1:
                    worst = 1
            else:
                row.update(answer='no take', exit=3)
            with open(res, 'a', encoding='utf-8') as f:
                f.write('\t'.join(str(row.get(c, '')).replace('\t', ' ') for c in RESULT_COLS) + '\n')
    print(f'results: {res}')
    return worst


def cmd_qsinfo(a):
    """A quicksave's header, TASKS record and settings lines."""
    for p in a.files:
        q = read_quicksave(p)
        if not q.get('format'):
            print(f'{p}: not a quicksave ({q["bytes"]} bytes)')
            continue
        print(f'{p}: format {q["format"]}, {q["file_bytes"]} bytes, Frame_Counter {q["fc"]}, built {q["tag"]} '
              f'({q["sha"][:12]}...) at 0x{q["dll_base"]:08X}')
        if q['format'] == 2:
            print(f'  revision {q["revision"]}, {"portable" if q["portable"] else "not portable: " + why_not_text(q)}, '
                  f'{q["words"]} words by their names, {q["pointer_bytes"] * 8}-bit, image at 0x{q["image_base"]:08X}, '
                  f'code places at 0x{q["table_lo"]:08X}, {q["bind_rows"]} bound VAs (fnv {q["bind_fnv"]:08X})')
            print(f'  tasks: {tasks_text(q)}')
            print(f'  settings {q["settings_fnv"]:08X}: ' + ' | '.join(q['settings']))
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
    divp = os.path.join(d, 'div.sh')
    expect('--expect covering the page and its window (k 17..30 in 0..40)',
           cmd_inproc(argparse.Namespace(run=divp, log=None, plain=None, window=None, expect=['0x5DF000:0-40'])), 0)
    expect('--expect whose window ends too soon (0..20)',
           cmd_inproc(argparse.Namespace(run=divp, log=None, plain=None, window=None, expect=['0x5DF000:0-20'])), 1)
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
    expect('same: a run against itself', cmd_same(argparse.Namespace(a=pl, b=pl, refb=None)), 0)
    # --refb: a page the two references disagree on is noise, not a difference
    base = synthetic(restore=False)
    noisy = [(fc, rf, st[:2] + [st[2] ^ 1] + st[3:]) if fc == 5 else (fc, rf, st) for fc, rf, st in base]
    other = [(fc, rf, st[:6] + [st[6] ^ 1] + st[7:]) if fc == 9 else (fc, rf, st) for fc, rf, st in noisy]
    a2, b1, b2 = (os.path.join(d, f) for f in ('refb.sh', 'noisy.sh', 'other.sh'))
    write_sh(a2, base); write_sh(b1, noisy); write_sh(b2, other)
    expect('same --refb, the difference only where the references disagree',
           cmd_same(argparse.Namespace(a=pl, b=b1, refb=b1)), 0)
    expect('same --refb, a difference where they agree', cmd_same(argparse.Namespace(a=pl, b=b2, refb=b1)), 1)
    # a page is named by the skip list's rows on it: 0x6BC000 holds Sound_Banks' row
    fake = argparse.Namespace(base=statehash.BASE, page=0x1000)
    lo, syms, rows = page_name(fake, (0x6BC000 - statehash.BASE) // 0x1000)
    good = any('0x6BC928' in r and 'Sound_Banks' in r for r in rows)
    print(f'{"ok  " if good else "FAIL"} page 0x{lo:06X} named by its skip rows: {rows[:2]}')
    ok &= good
    # matrix: the take form and the cross form hold the route's frames, frame f made idle
    rt = os.path.join(d, 'route.txt')
    open(rt, 'w').write('# save town\nwait 10\nhold cross 5\nwait 20\nhold up 30\n')
    tp, cp, idle = take_recipes(rt, 12, 15, d, 'st')
    _, r0 = read_route(rt)
    want = r0[:12] + [('KEY', 'f6')] + r0[13:27]
    got = read_route_peeks(tp)
    good = idle and got == want and read_route(cp)[1] == [('KEY', 'f7')] + r0[13:27] and \
        open(tp).read().count('peek ') == len(TAKE_PEEKS) and '# save town' in open(tp).read()
    print(f'{"ok  " if good else "FAIL"} the take form: the route to frame 12 (a held frame made idle), the peeks, '
          f'f6, the route to frame 26; the cross form f7 then frames 13..26')
    ok &= good
    # the quicksave reader on a synthetic format-2 header, one task at a site
    hb, sn, an = 0xD0, 0x100, 0x40
    qb = bytearray(QS_HALF + hb + sn + an + 0x70 + 9)
    struct.pack_into('<8s4I', qb, QS_HALF, b'BOF3XQS\0', 2, hb, len(qb), 1)
    qb[QS_HALF + 24:QS_HALF + 30] = b'v0.1.0'
    qb[QS_HALF + 56:QS_HALF + 120] = b'a' * 64
    struct.pack_into('<2I6I', qb, QS_HALF + 120, 0x10000000, 337, 0x5DA000, sn, 0, 0, 0x1000, an)
    struct.pack_into('<12I', qb, QS_HALF + 0xA0, 1, 1, 0, 0x114FA51B, 9, 0x70, 7, 0x7C401000, 3, 0xABCD, 0x400000, 8)
    o = QS_HALF + hb + sn + an
    struct.pack_into('<3I8s', qb, o, 2, 9, 8, bytes([1, 0, 0, 0, 0, 0, 0, 0]))
    qb[o + 0x70:o + 0x79] = b'lang=x\non'
    qf = os.path.join(d, 'q.dat')
    open(qf, 'wb').write(qb)
    q = read_quicksave(qf)
    good = q['format'] == 2 and q['fc'] == 337 and q['portable'] == 1 and tasks_text(q).startswith(
        '0 site 9 (0100000000000000), 1 free') and q['settings'] == ['lang=x', 'on'] and q['tag'] == 'v0.1.0'
    print(f'{"ok  " if good else "FAIL"} a synthetic format-2 quicksave read back: {tasks_text(q)}')
    ok &= good
    lg = os.path.join(d, 'l.log')
    open(lg, 'w').write('quicksave   F7 BISLPS10.DAT refused: another graphics record - v3 (the save half is whole: '
                        'it can be loaded as a save)\n')
    good = load_answer(lg) == ('REFUSED', 'another graphics record - v3 (the save half is whole: it can be loaded as a save)')
    print(f'{"ok  " if good else "FAIL"} a refusal read from a log: {load_answer(lg)}')
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
    p.add_argument('--expect', action='append', default=None, metavar='PAGE:K0-K1',
                   help='a page allowed to differ inside a window of k (default: ' + ' '.join(EXPECT_DEFAULT)
                   + '; --expect none for no rule)')
    p.set_defaults(fn=cmd_inproc)
    p = sub.add_parser('cross')
    p.add_argument('live')
    p.add_argument('loaded')
    p.add_argument('--log-live')
    p.add_argument('--log-loaded')
    p.add_argument('--frames', type=int)
    p.add_argument('--expect', action='append', default=None, metavar='PAGE:K0-K1')
    p.set_defaults(fn=cmd_cross)
    p = sub.add_parser('same')
    p.add_argument('a')
    p.add_argument('b')
    p.add_argument('--refb', help="a second run of A's side: where it and A disagree is noise, not reported")
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
    p.add_argument('--expect', action='append', default=None, metavar='PAGE:K0-K1')
    p.add_argument('--save', help='the recipe save to hold in slot 0 (input_run --save); for a cross run, the live '
                   'run, so a save list read later in the route lists the same file on both sides')
    p.set_defaults(fn=cmd_run)
    p = sub.add_parser('tier0')
    p.add_argument('route')
    p.add_argument('--at', type=int, required=True, help='the idle recipe frame of the route where f6 takes')
    p.add_argument('--out', required=True)
    p.add_argument('--after', type=int, default=120, help='frames after Game_Mode 2 before the peeks and the shot')
    p.add_argument('--host')
    p.add_argument('--elf')
    p.add_argument('--take-elf', help='the build that takes, if not --elf (a real other build: with --no-doctor)')
    p.add_argument('--load-elf', help='the build that loads, and plays the reference, if not --elf')
    p.add_argument('--no-doctor', action='store_true', help='leave the build stamp as taken: the loading build is another')
    p.add_argument('--take-env', action='append', default=[], metavar='K=V', help='the take only')
    p.add_argument('--load-env', action='append', default=[], metavar='K=V', help='the load and the reference only')
    p.add_argument('--direct', action='store_true')
    p.add_argument('--game', help='the install for the take and the reference run (input_run --game)')
    p.add_argument('--lang', default='en-US')
    p.add_argument('--minutes', type=float, default=10)
    p.add_argument('--env', action='append', default=[])
    p.set_defaults(fn=cmd_tier0)
    p = sub.add_parser('matrix')
    p.add_argument('takes', help='NAME ROUTE FC [keep=N] [lang=TAG|none] [# why], one take a line')
    p.add_argument('--take', required=True, metavar='LABEL=PATH', help='the build that takes (an ELF, or a launcher .exe)')
    p.add_argument('--load', action='append', required=True, metavar='LABEL=PATH',
                   help='a build that loads each take; LABEL!=PATH loads it forced portable (BOF3X_QSPORTABLE=1)')
    p.add_argument('--out', required=True)
    p.add_argument('--results', default='results.tsv', help='the results file, one row a take and a loading build')
    p.add_argument('--game', required=True, help='the install (Linux: a run folder is made from it for the take)')
    p.add_argument('--game-take', help='Windows: the take build\'s game folder, if not --game')
    p.add_argument('--game-load', help='Windows: the loading builds\' game folder, if not --game')
    p.add_argument('--keep', type=int, default=500, help='frames of the route after the take (default 500)')
    p.add_argument('--lang', default='en-US', help="the takes' language unless a line says lang= ('none': no overlay)")
    p.add_argument('--minutes', type=float, default=10)
    p.add_argument('--defaults', action='store_true', help="no BOF3X_OPT=none / BOF3X_LAYERING=0")
    p.add_argument('--env', action='append', default=[], metavar='K=V', help='both sides')
    p.add_argument('--take-env', action='append', default=[], metavar='K=V', help='the take only (a setting crossed)')
    p.add_argument('--load-env', action='append', default=[], metavar='K=V', help='the loads only')
    p.add_argument('--only', help='the takes of these names, comma-separated')
    p.add_argument('--hash-skip-only', action='store_true',
                   help="hash under the state hash's skip list alone, without PROOF_SKIP_EXTRA")
    p.add_argument('--resume', action='store_true', help='keep the results file and skip the rows it has')
    p.add_argument('--lock-file', help='an flock held for each game run (POSIX)')
    p.add_argument('--lock', help='a mkdir lock held for each game run (the Windows runs)')
    p.set_defaults(fn=cmd_matrix)
    p = sub.add_parser('qsinfo')
    p.add_argument('files', nargs='+')
    p.set_defaults(fn=cmd_qsinfo)
    p = sub.add_parser('selftest')
    p.set_defaults(fn=cmd_selftest)
    a = ap.parse_args()
    if hasattr(a, 'expect'):
        a.expect = EXPECT_DEFAULT if a.expect is None else [] if a.expect == ['none'] else a.expect
    sys.exit(a.fn(a) or 0)


if __name__ == '__main__':
    main()
