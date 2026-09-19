#!/usr/bin/env python
"""First-call trace of BOF3.exe: entry list in, report out (docs/call-trace.md).

    python tools/calltrace.py entries
        analysis/pc_funcs.json -> analysis/calltrace/entries.txt, the list the
        DLL arms when BOF3X_CALLTRACE points at it (src/hook/calltrace.cpp).

    python tools/calltrace.py report build/bof3x.calltrace.tsv [--out X.tsv]
        Join the hits with symbols.toml names and the function inventory:
        what was reached, when, from which function, and what never ran.

    python tools/calltrace.py compare A.tsv B.tsv
        Same set, same order, same frames? Exit 0 if so.

    python tools/calltrace.py volatile COUNTS_A COUNTS_B [--out LIST]
        Two bof3x.callcounts.tsv files taken to the same frame at different
        speeds. Entries whose totals differ depend on wall-clock time (drawing,
        the audio pump); the rest are logic. Writes the entry list without the
        former - arm that one for a per-frame hash comparable across runs.

    python tools/calltrace.py wallclock COUNTS [--out LIST] [--check OLDLIST]
        The structural version of volatile: drop everything reachable, by the
        call edges of a full-list run, from the call sites in WinMain's loop
        that are timed by GetTickCount. Preferred over volatile.

    python tools/calltrace.py queue COUNTS [--out CSV]
        Takeover work queue: reached functions not yet in symbols.toml, callees
        before callers, hottest first within a layer.

    python tools/calltrace.py frames A B
        Two bof3x.callframes.tsv files: same calls and hash every frame?

An entry is armed only if the instruction before it ends a function (ret, jmp,
int3 or nop padding). pe_funcs.py takes every direct call target as an entry,
and its linear sweep also decodes the jump tables that MSVC leaves in .text; an
int3 written into one of those would corrupt data rather than trap. The filter
costs a few real entries and the tool says how many.

Everything written here is derived from game code and stays under analysis/
(CLAUDE.md rule 1).
"""
import argparse, bisect, collections, json, os, struct, sys, tomllib
import capstone

ENDS_FUNCTION = ('ret', 'retf', 'jmp', 'int3', 'nop')


def load_funcs(path):
    d = json.load(open(path))
    return d, sorted(f['entry'] for f in d['functions'])


def text_section(exe):
    data = open(exe, 'rb').read()
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    nsec, = struct.unpack_from('<H', data, pe + 6)
    optsz, = struct.unpack_from('<H', data, pe + 20)
    base, = struct.unpack_from('<I', data, pe + 24 + 28)
    for i in range(nsec):
        s = pe + 24 + optsz + i * 40
        if data[s:s+8].rstrip(b'\0') == b'.text':
            vsz, va, rsz, raw = struct.unpack_from('<IIII', data, s + 8)
            return base + va, data[raw:raw + rsz]
    sys.exit('no .text in ' + exe)


def previous_mnemonics(exe):
    """address -> mnemonic of the instruction the linear sweep puts before it."""
    lo, code = text_section(exe)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    prev, last, pos = {}, None, 0
    while pos < len(code):
        got = False
        for addr, size, mnem, _ in md.disasm_lite(code[pos:], lo + pos):
            prev[addr] = last
            last = mnem
            pos = addr - lo + size
            got = True
        if not got:
            last = None
            pos += 1
    return prev


def names(toml_path):
    t = tomllib.load(open(toml_path, 'rb'))
    return {f['pc']: f['name'] for f in t.get('func', []) if 'pc' in f and 'name' in f}


def cmd_entries(a):
    d, entries = load_funcs(a.funcs)
    size = {f['entry']: f['size'] for f in d['functions']}
    prev = previous_mnemonics(a.exe)
    keep, drop = [], []
    for e in entries:
        p = prev.get(e, 'missing')
        (keep if p is None or p in ENDS_FUNCTION else drop).append((e, p))
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, 'w', newline='\n') as f:
        f.write(f'# {len(keep)} function entries of {d["exe"]}, from {a.funcs}\n')
        for e, _ in keep:
            f.write(f'{e:08X} {size[e]:X}\n')
    print(f'{len(keep)} entries -> {a.out}')
    print(f'{len(drop)} dropped, not preceded by a function end:')
    for e, p in drop:
        print(f'  0x{e:08X}  after {p}')


def read_hits(path):
    hits = []
    for line in open(path):
        if line.startswith('#') or not line.strip():
            continue
        frame, entry, caller, thread = line.split('\t')
        hits.append((int(frame), int(entry, 16), int(caller, 16), int(thread)))
    return hits


def cmd_report(a):
    d, entries = load_funcs(a.funcs)
    armed = {int(l.split()[0], 16) for l in open(a.entries) if l.strip() and l[0] != '#'}
    name = names(a.symbols)
    lo = int(d['text']['va'], 16)
    hi = lo + d['text']['size']

    def owner(addr):
        if not lo <= addr < hi:
            return None
        i = bisect.bisect_right(entries, addr) - 1
        return entries[i] if i >= 0 else None

    def label(e):
        return name.get(e, '') if e is not None else ''

    hits = read_hits(a.hits)
    out = a.out or os.path.splitext(a.hits)[0] + '.report.tsv'
    with open(out, 'w', newline='\n') as f:
        f.write('frame\tentry\tname\tcaller\tcaller_func\tcaller_name\tthread\n')
        for frame, e, c, t in hits:
            o = owner(c)
            f.write(f'{frame}\t0x{e:08X}\t{label(e)}\t0x{c:08X}\t'
                    f'{"0x%08X" % o if o else "(outside .text)"}\t{label(o)}\t{t}\n')

    reached = {h[1] for h in hits}
    threads = collections.Counter(h[3] for h in hits)
    print(f'{len(hits)} of {len(armed)} armed functions reached '
          f'({100 * len(hits) / len(armed):.1f}%); last frame seen {max(h[0] for h in hits)}')
    print('threads:', ', '.join(f'{t}: {n}' for t, n in threads.most_common()))
    named = sorted(e for e in name if e in armed)
    print(f'named in symbols.toml and armed: {len(named)}; '
          f'reached {sum(e in reached for e in named)}')
    for e in named:
        if e not in reached:
            print(f'  not reached  0x{e:08X}  {name[e]}')
    # New functions per 300-frame window: where in the run the code is.
    width = a.window
    buckets = collections.Counter(h[0] // width for h in hits)
    print(f'first calls per {width} logic frames:')
    for b in sorted(buckets):
        print(f'  {b * width:>6}  {buckets[b]}')
    print('report ->', out)


def cmd_compare(a):
    x, y = read_hits(a.a), read_hits(a.b)
    kx = [(h[0], h[1], h[2]) for h in x]
    ky = [(h[0], h[1], h[2]) for h in y]
    n = min(max(h[0] for h in x), max(h[0] for h in y))
    kx = [k for k in kx if k[0] <= n]
    ky = [k for k in ky if k[0] <= n]
    if kx == ky:
        print(f'identical: {len(kx)} first calls, same order, frame and caller, to frame {n}')
        return 0
    if [k[1:] for k in kx] == [k[1:] for k in ky]:
        late = [(p, q) for p, q in zip(kx, ky) if p[0] != q[0]]
        print(f'same {len(kx)} first calls, same order and callers, to frame {n}; '
              f'{len(late)} differ in frame only:')
        for p, q in late:
            print(f'  0x{p[1]:08X} from 0x{p[2]:08X}: frame {p[0]} vs {q[0]}')
        return 1
    sx, sy = {k[1] for k in kx}, {k[1] for k in ky}
    print(f'DIFFERENT to frame {n}: {len(kx)} vs {len(ky)} first calls; '
          f'only in A {len(sx - sy)}, only in B {len(sy - sx)}')
    for i, (p, q) in enumerate(zip(kx, ky)):
        if p != q:
            print(f'first difference at row {i}: A frame {p[0]} 0x{p[1]:08X} from 0x{p[2]:08X}; '
                  f'B frame {q[0]} 0x{q[1]:08X} from 0x{q[2]:08X}')
            break
    return 1


def read_counts(path):
    first = open(path).readline()
    frame = int(first.split()[3].rstrip(';'))
    totals = {}
    for line in open(path):
        if line[0] == '#':
            continue
        entry, caller, n = line.split()
        if int(caller, 16) == 0:
            totals[int(entry, 16)] = int(n)
    return frame, totals


def cmd_volatile(a):
    fa, ca = read_counts(a.a)
    fb, cb = read_counts(a.b)
    if fa != fb:
        sys.exit(f'counts end on different frames ({fa} vs {fb}); use BOF3X_CALLTRACE_STOP')
    name = names(a.symbols)
    moved = sorted(e for e in set(ca) | set(cb) if ca.get(e, 0) != cb.get(e, 0))
    # Entries whose totals agree but whose calls move between frames: found
    # with BOF3X_CALLTRACE_DETAIL, named by hand.
    also = sorted(int(x, 16) for x in a.also.split(',') if x)
    print(f'to frame {fa}: {len(ca)} / {len(cb)} functions called, '
          f'{sum(ca.values())} / {sum(cb.values())} calls; {len(moved)} totals differ:')
    for e in moved:
        print(f'  0x{e:08X}  {ca.get(e, 0):>9}  {cb.get(e, 0):>9}  {name.get(e, "")}')
    print(f'plus {len(also)} named with --also')
    moved = set(moved) | set(also)
    keep = [l for l in open(a.entries) if l[0] == '#' or int(l.split()[0], 16) not in moved]
    with open(a.out, 'w', newline='\n') as f:
        f.write(f'# speed-independent entries: {a.entries} minus {len(moved)} (calltrace.py volatile)\n')
        f.writelines(l for l in keep if l[0] != '#')
    print(f'{len(keep) - 1} entries -> {a.out}')


def cmd_queue(a):
    """Reached, unowned functions, ordered so that what a function calls comes
    before it. Layer 0 calls nothing unnamed; layer n calls only layers < n."""
    d, entries = load_funcs(a.funcs)
    F = {f['entry']: f for f in d['functions']}
    _, total = read_counts(a.counts)
    t = tomllib.load(open(a.symbols, 'rb'))
    known = {f['pc'] for f in t.get('func', [])}
    armed = {int(l.split()[0], 16) for l in open(a.logic) if l.strip() and l[0] != '#'}
    todo = {e for e in total if e not in known}
    layer, n = {}, 0
    while True:
        ready = {e for e in todo if e not in layer and
                 all(c in known or c in layer or c not in F or c == e for c in F[e]['callees'])}
        if not ready:
            break
        for e in ready:
            layer[e] = n
        n += 1
    rows = []
    for e in todo:
        f = F[e]
        unreached = [c for c in f['callees'] if c in F and c not in total and c not in known]
        rows.append((layer.get(e, 99), -total[e], e, f['size'], f['icall'], f['fp'],
                     len(unreached), 'logic' if e in armed else 'speed-dependent'))
    rows.sort()
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, 'w', newline='') as f:
        f.write('layer,calls,entry,size,indirect_calls,fp_insns,unreached_callees,kind\n')
        for l, c, e, size, ic, fp, ur, kind in rows:
            f.write(f'{l},{-c},0x{e:08X},{size},{ic},{fp},{ur},{kind}\n')
    by = collections.Counter(r[0] for r in rows)
    print(f'{len(rows)} reached functions not in symbols.toml; by layer (99 = in a call cycle '
          f'or above one):')
    print('  ' + ', '.join(f'{l}: {k}' for l, k in sorted(by.items())))
    print('queue ->', a.out)


# Return addresses inside WinMain's loop whose calls are timed by GetTickCount,
# not by the logic frame (docs/call-trace.md section 6): the rendered-frame
# branch, the audio pump in the pacing spin, and the once-a-second FPS sprintf.
WALLCLOCK_SITES = [(0x4FCE3F, 0x4FCEB9), (0x4FCEC1, 0x4FCEC2), (0x4FCF08, 0x4FCF09)]


def cmd_wallclock(a):
    """Everything reachable, by observed call edges, from a wall-clock-timed
    call site. COUNTS must come from a run that armed the full entry list."""
    d, entries = load_funcs(a.funcs)
    lo = int(d['text']['va'], 16)
    hi = lo + d['text']['size']
    down, roots = collections.defaultdict(set), set()
    for line in open(a.counts):
        if line[0] == '#':
            continue
        e, c, _ = (int(x, 16) if i < 2 else x for i, x in enumerate(line.split()))
        if c == 0 or not lo <= c < hi:
            continue
        if any(s <= c < t for s, t in WALLCLOCK_SITES):
            roots.add(e)
        else:
            down[entries[bisect.bisect_right(entries, c) - 1]].add(e)
    reach, stack = set(roots), list(roots)
    while stack:
        for e in down[stack.pop()]:
            if e not in reach:
                reach.add(e)
                stack.append(e)
    reach |= {int(x, 16) for x in a.also.split(',') if x}
    name = names(a.symbols)
    print(f'{len(roots)} roots, {len(reach)} functions reachable from wall-clock-timed call sites')
    if a.check:
        old = {int(l.split()[0], 16) for l in open(a.entries) if l.strip() and l[0] != '#'} - \
              {int(l.split()[0], 16) for l in open(a.check) if l.strip() and l[0] != '#'}
        print(f'against {a.check}: {len(reach & old)} in both, '
              f'{len(reach - old)} new here, {len(old - reach)} only there')
        for e in sorted(reach - old):
            print(f'  new   0x{e:08X}  {name.get(e, "")}')
        for e in sorted(old - reach):
            print(f'  only there  0x{e:08X}  {name.get(e, "")}')
    keep = [l for l in open(a.entries) if l[0] != '#' and int(l.split()[0], 16) not in reach]
    with open(a.out, 'w', newline='\n') as f:
        f.write(f'# {a.entries} minus {len(reach)} reachable from wall-clock call sites '
                f'(calltrace.py wallclock {a.counts})\n')
        f.writelines(keep)
    print(f'{len(keep)} entries -> {a.out}')


def cmd_frames(a):
    def rd(p):
        return [tuple(l.split()) for l in open(p)]
    x, y = rd(a.a), rd(a.b)
    n = min(len(x), len(y))
    bad = [i for i in range(n) if x[i] != y[i]]
    if not bad:
        print(f'identical: calls and hash agree on all {n} frames')
        return 0
    print(f'{len(bad)} of {n} frames differ; first ten:')
    for i in bad[:10]:
        print(f'  frame {x[i][0]}: {x[i][1]} calls {x[i][2]}  vs  {y[i][1]} calls {y[i][2]}')
    return 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--funcs', default='analysis/pc_funcs.json')
    ap.add_argument('--exe', default='bof3/BOF3.exe')
    ap.add_argument('--symbols', default='symbols.toml')
    ap.add_argument('--entries', default='analysis/calltrace/entries.txt')
    sub = ap.add_subparsers(dest='cmd', required=True)
    e = sub.add_parser('entries')
    e.add_argument('--out', default='analysis/calltrace/entries.txt')
    r = sub.add_parser('report')
    r.add_argument('hits')
    r.add_argument('--out')
    r.add_argument('--window', type=int, default=300)
    c = sub.add_parser('compare')
    c.add_argument('a')
    c.add_argument('b')
    v = sub.add_parser('volatile')
    v.add_argument('a')
    v.add_argument('b')
    v.add_argument('--out', default='analysis/calltrace/entries_logic.txt')
    v.add_argument('--also', default='', help='comma-separated hex entries to drop as well')
    q = sub.add_parser('queue')
    q.add_argument('counts')
    q.add_argument('--logic', default='analysis/calltrace/entries_logic.txt')
    q.add_argument('--out', default='analysis/calltrace/queue.csv')
    w = sub.add_parser('wallclock')
    w.add_argument('counts')
    w.add_argument('--out', default='analysis/calltrace/entries_logic.txt')
    w.add_argument('--check', help='an older reduced entry list to compare against')
    w.add_argument('--also', default='', help='comma-separated hex entries to drop as well')
    fr = sub.add_parser('frames')
    fr.add_argument('a')
    fr.add_argument('b')
    a = ap.parse_args()
    return {'entries': cmd_entries, 'report': cmd_report, 'compare': cmd_compare,
            'volatile': cmd_volatile, 'frames': cmd_frames,
            'queue': cmd_queue, 'wallclock': cmd_wallclock}[a.cmd](a)


if __name__ == '__main__':
    sys.exit(main())
