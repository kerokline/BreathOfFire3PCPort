#!/usr/bin/env python
"""Audit of the owned starts that have no `analysis/calltrace/entries_logic.txt`
line (round twelve's debt 4, docs/takeover-queue-round12.md section 7).

    python tools/entries_audit.py [--symbols symbols.toml]
        [--entries analysis/calltrace/entries_logic.txt]
        [--funcs analysis/pc_funcs.json] [--hidden analysis/pc_hidden.json]
        [--exclude analysis/calltrace/wallclock_reach.json ...]
        [--reach <bof3x.calltrace.tsv | bof3x.callcounts.tsv> ...]
        [--exe bof3/BOF3.exe] [--tsv <path>] [--append] [--dedupe]

Why a missing line matters (src/hook/calltrace.cpp, CallTrace_Start): the
tracer registers an owned range [addr, addr + size) only for a listed entry
that Inject owns; a call made from inside such a range is logged with
kOwnedCaller in place of its return address, which is what makes the frame
hash of an all-original run comparable with ours. An owned function with no
line has no range, so on the original side the calls its body makes carry a
real return address, and on ours kOwnedCaller: the hash differs at the first
frame a route enters it (HANDOFF's trap, "an owned function missing from
entries_logic.txt breaks the hash"; the dragon route and Sparkle_Launch,
round twelve section 5). It is harmless only while one of two things holds:

  covered   its address lies inside the registered range of another owned,
            listed function (a hidden start inside its host, a case cut out
            of a switch): the host's range already claims its calls; or
  unreached no route enters it. The tool cannot know this by itself; give
            the reach runs (--reach, BOF3X_CALLTRACE_REACH=1 output files)
            and it says which starts those runs entered.

For each owned start without a line the tool prints: the name, the impl file,
the host whose listed range holds it (and whether that host is owned - a
listed range of a function Capcom still runs is not registered), whether an
--exclude list names it (the wall-clock exclusions: `calltrace.py wallclock`
drops them on purpose), which --reach runs entered it and how often, and the
verdict. For a start that wants a line it proposes one, `AAAAAAAA SIZE`,
from pc_funcs.json's size, or from the code's own extent when --exe is given
(tools/band_rows.py's read_extent, capstone); the size is a hypothesis until
the group's doc or a read confirms it (battle_flow.md section 7 lists sizes
the catalogue got wrong).

A start without a line is never armed (the tracer arms the listed entries
only), so no reach run can have seen it enter: the reach runs settle only the
listed starts of --all (a line with a size of 0) and say nothing about the
missing ones, whose verdict is always "add a line". --append writes the
proposed lines to the entries file under a dated comment, as the groups did
(the sizes from the code when --exe is given; it refuses a start whose size
is unknown). --dedupe rewrites the entries file keeping, of each address
listed more than once, the line with the smallest non-zero size (the
consolidation the group docs deferred: "duplicates keep the smaller"). The
tracer registers a range per line, duplicates included (calltrace.cpp,
CallTrace_Start), so a host's over-long extent beside its cut-down line
still claims the calls of whatever Capcom code lies past the host - on both
sides alike, so the hash holds, but a Capcom caller is logged as owned; the
dedupe is attribution, not the hash. Both writes are made on the machine
that holds analysis/ (the file is derived from the exe, CLAUDE.md rule 1);
a run with neither writes nothing but --tsv. Everything it prints is
addresses and our own names: fine for a doc. Addresses are load-bearing
(rule 3).
"""
import argparse, bisect, collections, csv, json, os, re, sys, tomllib

HEX = re.compile(r'(?<![0-9A-Za-z_])(?:0x)?([0-9A-Fa-f]{6,8})(?![0-9A-Za-z_])')


def load_symbols(path):
    with open(path, 'rb') as fh:
        t = tomllib.load(fh)
    funcs = {f['pc']: f for f in t.get('func', []) if 'pc' in f}
    owned = {pc: f for pc, f in funcs.items() if 'impl' in f}
    return funcs, owned


def load_entries(path):
    """[(addr, size, line number)] of the entry list; size 0 when the line
    gives none (the tracer reads a size only for an owned entry)."""
    out = []
    with open(path, encoding='utf-8', errors='replace') as fh:
        for n, line in enumerate(fh, 1):
            s = line.strip()
            if not s or s[0] == '#':
                continue
            parts = s.split()
            try:
                addr = int(parts[0], 16)
            except ValueError:
                sys.exit('%s:%d: not a hex address: %s' % (path, n, s))
            size = int(parts[1], 16) if len(parts) > 1 else 0
            out.append((addr, size, n))
    return out


def load_addresses(path):
    """Every address a file names, however it is shaped: a JSON list or dict
    (ints, hex strings, or objects with an 'entry' / 'pc' / 'addr' field),
    or a text file with hex tokens (an entry list, a report)."""
    text = open(path, encoding='utf-8', errors='replace').read()
    found = set()
    try:
        obj = json.loads(text)
    except ValueError:
        obj = None
    if obj is not None:
        def walk(x):
            if isinstance(x, bool):
                return
            if isinstance(x, int):
                found.add(x)
            elif isinstance(x, str):
                try:
                    found.add(int(x, 16))
                except ValueError:
                    pass
            elif isinstance(x, dict):
                for k in ('entry', 'pc', 'addr', 'address'):
                    if k in x:
                        walk(x[k])
                        break
                else:
                    for k, v in x.items():
                        walk(k)
                        walk(v)
            elif isinstance(x, (list, tuple)):
                for v in x:
                    walk(v)
        walk(obj)
    else:
        for m in HEX.finditer(text):
            found.add(int(m.group(1), 16))
    return found


def load_reach(path):
    """{entry: calls} from a tracer output: bof3x.calltrace.tsv (first calls,
    frame\\tentry\\tcaller\\tthread) or bof3x.callcounts.tsv (entry caller n;
    the caller-0 line is the total). Comment lines start with #."""
    counts = collections.Counter()
    with open(path, encoding='utf-8', errors='replace') as fh:
        for line in fh:
            if not line.strip() or line[0] == '#':
                continue
            parts = line.split('\t') if '\t' in line else line.split()
            if len(parts) < 3:
                continue
            if '\t' in line:                       # calltrace.tsv: frame, entry, caller, thread
                try:
                    counts[int(parts[1], 16)] += 1
                except ValueError:
                    continue
            else:                                  # callcounts.tsv: entry caller n
                try:
                    e, c, n = int(parts[0], 16), int(parts[1], 16), int(parts[2])
                except ValueError:
                    continue
                if c == 0:
                    counts[e] += n
    return counts


def extent_reader(exe):
    """band_rows.read_extent over the exe, when --exe is given and capstone
    imports; None otherwise."""
    if not exe:
        return None
    if not os.path.exists(exe):
        sys.exit('--exe %s: no such file' % exe)
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import band_rows, magic_rows                   # noqa: E402  capstone
    img = magic_rows.Image(exe)

    def read(start, limit):
        d = band_rows.read_extent(img, start, limit)
        return d['end'] - start, d
    return read


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--symbols', default=os.path.join(repo, 'symbols.toml'))
    ap.add_argument('--entries', default=os.path.join(repo, 'analysis', 'calltrace', 'entries_logic.txt'))
    ap.add_argument('--funcs', default=os.path.join(repo, 'analysis', 'pc_funcs.json'),
                    help="pe_funcs.py's inventory, for the recorded sizes (optional)")
    ap.add_argument('--hidden', default=os.path.join(repo, 'analysis', 'pc_hidden.json'),
                    help='pe_hidden.py\'s pointer-reached starts, for their hosts (optional)')
    ap.add_argument('--exclude', action='append', default=[],
                    help='a file naming starts left out on purpose (the wall-clock exclusions); repeatable')
    ap.add_argument('--reach', action='append', default=[],
                    help='a bof3x.calltrace.tsv or bof3x.callcounts.tsv of a BOF3X_CALLTRACE_REACH=1 run; repeatable')
    ap.add_argument('--exe', help="BOF3.exe, to read each start's extent for the proposed line (capstone)")
    ap.add_argument('--all', action='store_true', help='also list the owned starts that have a line but a size of 0')
    ap.add_argument('--tsv', help='write the table to this path')
    ap.add_argument('--append', action='store_true',
                    help='append the proposed lines (the starts to add, with a known size) to the entries file')
    ap.add_argument('--dedupe', action='store_true',
                    help='rewrite the entries file keeping the smallest non-zero size of each address listed twice')
    a = ap.parse_args()

    funcs, owned = load_symbols(a.symbols)
    entries = load_entries(a.entries)
    listed = {}
    dups = []
    for addr, size, n in entries:
        if addr in listed:
            dups.append((addr, listed[addr][1], n))
        listed[addr] = (size, n)
    # the registered ranges: a listed entry that is owned, with its size
    ranges = sorted((addr, addr + size, addr) for addr, (size, n) in listed.items() if addr in owned and size)
    range_lo = [r[0] for r in ranges]
    # every listed extent, owned or not, for the "listed but not owned" host case
    spans = sorted((addr, addr + size, addr) for addr, (size, n) in listed.items() if size)
    span_lo = [r[0] for r in spans]

    recorded = {}
    if a.funcs and os.path.exists(a.funcs):
        for f in json.load(open(a.funcs))['functions']:
            recorded[f['entry']] = f['size']
    hidden = {}
    if a.hidden and os.path.exists(a.hidden):
        for h in json.load(open(a.hidden)):
            hidden[h['entry']] = h
    excluded = {}
    for p in a.exclude:
        for x in load_addresses(p):
            excluded.setdefault(x, []).append(os.path.basename(p))
    reach = [(os.path.basename(p), load_reach(p)) for p in a.reach]
    read = extent_reader(a.exe)
    all_starts = sorted(set(funcs) | set(listed) | set(recorded) | set(hidden))

    def hosts_of(s):
        """The registered range holding s (owned host), and any listed span
        holding it (a host Capcom still runs: listed, no range)."""
        out = []
        for los, rs, kind in ((range_lo, ranges, 'owned'), (span_lo, spans, 'listed')):
            i = bisect.bisect_right(los, s) - 1
            while i >= 0 and rs[i][0] <= s:
                lo, hi, h = rs[i]
                if lo < s < hi and h != s and not any(h == o[1] for o in out):
                    out.append((kind, h, lo, hi))
                i -= 1
        return out

    missing = sorted(pc for pc in owned if pc not in listed)
    zero = sorted(pc for pc in owned if pc in listed and listed[pc][0] == 0)
    rows = []
    for s in missing + (zero if a.all else []):
        f = owned[s]
        hs = hosts_of(s)
        owned_host = next((h for h in hs if h[0] == 'owned'), None)
        other_host = next((h for h in hs if h[0] == 'listed' and not owned_host), None)
        hit = [(name, c[s]) for name, c in reach if c.get(s)]
        excl = excluded.get(s)
        size, how = None, ''
        if read is not None:
            i = bisect.bisect_right(all_starts, s)
            lim = all_starts[i] if i < len(all_starts) else s + 0x10000
            size, _ = read(s, lim)
            how = 'extent read from the code'
        elif s in recorded:
            size, how = recorded[s], 'pc_funcs.json'
        if owned_host:
            verdict = 'covered: inside the registered range of %s' % owned[owned_host[1]]['name']
        elif excl:
            verdict = 'left out on purpose (%s)' % ', '.join(sorted(set(excl)))
        elif hit:
            verdict = 'UNCOVERED and entered by a route: add a line'
        elif s in listed:
            verdict = ('uncovered, no given route enters it: add a line before a route does' if reach
                       else 'uncovered (no --reach given): add a line')
        else:
            # never armed (no line), so the reach runs could not have seen it
            verdict = 'uncovered: add a line (a start without a line is never armed, so no reach run could see it)'
        if other_host:
            verdict += "; lies in the listed span of %s, which Capcom still runs (no range)" % (
                funcs.get(other_host[1], {}).get('name', '0x%X' % other_host[1]))
        hid = hidden.get(s)
        rows.append(dict(
            pc=s, name=f['name'], impl=f.get('impl', ''), status=f.get('status', ''),
            listed='size 0' if s in listed else '',
            host=('%s 0x%X [0x%X..0x%X)' % (owned[owned_host[1]]['name'], owned_host[1], owned_host[2], owned_host[3])
                  if owned_host else ''),
            hidden=('hidden, host 0x%X' % hid['host']) if hid and 'host' in hid else ('hidden' if hid else ''),
            excluded=', '.join(sorted(set(excl))) if excl else '',
            reached='; '.join('%s %d' % h for h in hit),
            line=('%08X %X' % (s, size)) if size else '',
            size_from=how if size else 'size unknown: read the extent (--exe)',
            verdict=verdict))

    print('%d owned starts (impl) in %s; %d lines in %s (%d registered as ranges); '
          '%d owned starts without a line%s' % (
              len(owned), os.path.relpath(a.symbols, repo), len(listed), os.path.relpath(a.entries, repo),
              len(ranges), len(missing), ', %d with a size of 0' % len(zero) if zero else ''))
    if dups:
        print('%d addresses listed twice: %s' % (len(dups), ', '.join('0x%X (lines %d, %d)' % d for d in dups[:12])))
    if reach:
        print('reach: %s' % ', '.join('%s (%d entries)' % (n, len(c)) for n, c in reach))
    print()
    for r in rows:
        print('0x%X %s  (%s%s)' % (r['pc'], r['name'], r['impl'], ', ' + r['hidden'] if r['hidden'] else ''))
        if r['listed']:
            print('    listed with a size of 0: the tracer would Fatal on it')
        if r['host']:
            print('    host: %s' % r['host'])
        if r['excluded']:
            print('    excluded by: %s' % r['excluded'])
        if r['reached']:
            print('    entered by: %s' % r['reached'])
        print('    %s' % r['verdict'])
        if not r['verdict'].startswith('covered') and not r['excluded']:
            print('    line: %s  (%s)' % (r['line'] or '-', r['size_from']))
    by = collections.Counter(r['verdict'].split(':')[0].split(' (')[0] for r in rows)
    print('\nby verdict: %s' % ', '.join('%s %d' % (k, n) for k, n in sorted(by.items())))

    if a.tsv:
        keys = ['pc', 'name', 'impl', 'status', 'hidden', 'host', 'excluded', 'reached', 'line', 'size_from', 'verdict']
        with open(a.tsv, 'w', newline='', encoding='utf-8') as fh:
            w = csv.writer(fh, delimiter='\t')
            w.writerow(keys)
            for r in rows:
                w.writerow(['0x%X' % r['pc']] + [r[k] for k in keys[1:]])
        print('wrote %s (%d rows)' % (a.tsv, len(rows)), file=sys.stderr)

    if a.dedupe:
        dedupe_entries(a.entries)
    if a.append:
        append_entries(a.entries, rows, read is not None)


def dedupe_entries(path):
    """Of each address listed more than once, keep the line with the smallest
    non-zero size (a size of 0 only when no line has one); every other line of
    that address goes. Comments and order are kept."""
    with open(path, encoding='utf-8', errors='replace') as fh:
        lines = fh.read().split('\n')
    best = {}                                       # addr -> (size key, line index)
    for i, line in enumerate(lines):
        s = line.strip()
        if not s or s[0] == '#':
            continue
        parts = s.split()
        addr = int(parts[0], 16)
        size = int(parts[1], 16) if len(parts) > 1 else 0
        key = (0, size) if size else (1, 0)         # a sized line beats an unsized one; then the smaller
        if addr not in best or key < best[addr][0]:
            best[addr] = (key, i)
    keep = {i for _, i in best.values()}
    dropped = []
    out = []
    for i, line in enumerate(lines):
        s = line.strip()
        if s and s[0] != '#' and i not in keep:
            dropped.append((int(s.split()[0], 16), i + 1, s))
            continue
        out.append(line)
    if not dropped:
        print('dedupe: no address is listed twice; %s unchanged' % path)
        return
    with open(path, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(out))
    print('dedupe: %d line(s) dropped from %s (the smallest non-zero size of each address kept):' % (len(dropped), path))
    for addr, n, s in dropped:
        print('    line %d: %s' % (n, s))


def append_entries(path, rows, sized_from_code):
    """The proposed lines of the rows that want one, appended under a dated
    comment. Refuses when a start's size is unknown (give --exe)."""
    import datetime
    want = [r for r in rows if r['verdict'].startswith('uncovered') or r['verdict'].startswith('UNCOVERED')]
    want = [r for r in want if not r['listed']]     # a listed line with a size of 0 is edited by hand
    if not want:
        print('append: nothing to add')
        return
    missing = [r for r in want if not r['line']]
    if missing:
        sys.exit('append: %d start(s) have no size (%s): give --exe so the extent is read from the code' % (
            len(missing), ', '.join('0x%X' % r['pc'] for r in missing)))
    today = datetime.date.today().isoformat()
    with open(path, 'a', encoding='utf-8', newline='\n') as fh:
        fh.write('\n# tools/entries_audit.py %s: the %d owned starts without a line, each at its code extent%s '
                 '(docs/takeover-queue-round12.md section 7 item 4)\n' % (
                     today, len(want), ' read with capstone' if sized_from_code else ''))
        for r in want:                              # the name on its own comment line: every reader
            fh.write('# %s\n%s\n' % (r['name'], r['line']))   # of the list splits a bare "ADDR SIZE"
    print('append: %d line(s) added to %s:' % (len(want), path))
    for r in want:
        print('    %s  # %s' % (r['line'], r['name']))


if __name__ == '__main__':
    main()
