#!/usr/bin/env python
"""The typed-data round's left-raw list: every raw data site the census
counts, with the reason it is left raw (docs/typed-data-round.md section 3,
"C0 as run").

    python tools/typed_left.py              # regenerate analysis/typed_left.tsv from the tree
    python tools/typed_left.py --check      # fail if the committed file is not what regeneration writes
    python tools/typed_left.py import       # one-time: stage B's eight line-keyed lists (see below)

The sites are tools/data_census.py's, measured on the tree, so the list is
the census's own universe: one row per census site (global, record and raw
shapes). A row is keyed by where the site is, not by its line:

    file, function, shape, accessor, expr, n

`function` is the name of the function the site sits in (the census's own
function_start, then the first name called on its signature line; `-` at
file scope), `expr` the census's address expression (base and offset), and
`n` the site's ordinal among the sites of that file and function with the
same shape, accessor and expression. An edit elsewhere in the file moves no
key; an edit to the function that adds or removes an identical site renumbers
only that expression's ordinals in that function.

Regeneration carries each key's reason, evidence and source from the file
as it stands, so the reasons survive any edit that keeps the key. A site
whose key is new gets reason `-` (nobody has looked at it yet); a key that
no longer measures is dropped (converted, or moved to another function), and
both are counted on stderr. A reason is changed by editing its row.

`import` builds the file once from the line-keyed lists stage B wrote
(analysis/typed_b0_left.tsv, analysis/typed_b1_<group>_left.tsv): each row's
line is carried from the tree the list was written against (the last commit
that touched the list, from git) to today's tree by a line diff of its file,
and the row is matched to the census site at or near that line with the
same offset (B1) or constant (B0). Rows that match no census site are
written to --unmatched, and counted.

Addresses, names and reasons only (CLAUDE.md rule 1); no game data.
"""
from __future__ import annotations

import argparse
import collections
import difflib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import data_census as dc  # noqa: E402

ROOT = dc.ROOT
OUT = os.path.join('analysis', 'typed_left.tsv')
HDR = ['file', 'function', 'shape', 'accessor', 'expr', 'n', 'address', 'family', 'reason', 'evidence', 'source']
KEY = 6
B0_LIST = 'analysis/typed_b0_left.tsv'
B1_LISTS = ['analysis/typed_b1_%s_left.tsv' % g for g in
            ('magic', 'effect', 'rest', 'area', 'battle', 'field', 'scen_ui')]
CALLED = re.compile(r'([A-Za-z_~][\w:~]*)\s*\(')


def function_name(f, line):
    """The function the 1-based `line` of f sits in, by name, or '-'."""
    start = dc.function_start(f, line)
    for k in range(start, line):
        s = f.lines[k]
        if not s.strip() or s.lstrip().startswith('#'):
            continue
        m = CALLED.search(s)
        if m:
            return m.group(1)
    return '-'


def measure():
    """Today's census sites, each with its key: [(key tuple, line, site)]."""
    os.chdir(ROOT)
    syms = dc.Symbols('symbols.toml')
    files = dc.load_sources()
    by_rel = {f.rel: f for f in files}
    sites, _consts, _files, _unres = dc.scan(files, syms)
    out = []
    seen = collections.Counter()
    for s in sites:  # sorted by file, line
        rel, ln, shape, acc, expr = s[0], s[1], s[2], s[3], s[4]
        fn = function_name(by_rel[rel], ln)
        base = tuple(clean(x) for x in (rel, fn, shape, acc, expr))
        n = seen[base]
        seen[base] += 1
        out.append((base + (str(n),), ln, s))
    return out


def read_list(path):
    rows = {}
    if not os.path.exists(path):
        return rows
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            if line.startswith('#') or not line.strip():
                continue
            c = line.rstrip('\n').split('\t')
            rows[tuple(c[:KEY])] = c
    return rows


def render(measured, carried):
    lines = ['# tools/typed_left.py: the raw data sites the census counts, with the reason each is left raw, '
             'keyed by place, not line (docs/typed-data-round.md 3, "C0 as run"). Addresses, names and '
             'reasons only.\n',
             '#' + '\t'.join(HDR) + '\n']
    stats = collections.Counter()
    moved = moved_keys(measured, carried)
    for key, _ln, s in measured:
        old = carried.get(key)
        if old is None and key in moved:
            old = moved[key]
            stats['carried by place'] += 1
        if old is not None:
            reason, evidence, source = old[8], old[9], old[10]
            stats['carried' if reason != '-' else 'unreviewed'] += 1
        else:
            reason, evidence, source = '-', '-', '-'
            stats['new'] += 1
        row = list(key) + [s[5], s[6], reason, evidence, source]
        lines.append('\t'.join(clean(x) for x in row) + '\n')
    keys = {k for k, _, _ in measured}
    taken = {id(v) for v in moved.values()}
    stats['dropped'] = sum(1 for k, v in carried.items() if k not in keys and id(v) not in taken)
    return ''.join(lines), stats


def moved_keys(measured, carried):
    """{new key: old row} for sites whose key changed only in `expr`: an edit
    inside the census's 60-character expression (a renamed name, a typed
    sub-expression) changes the key of a site that is still raw, and its
    reason would be dropped (stage C3 carried five by hand; stage C4). Within
    one file, function, shape, accessor and address, the new keys and the
    gone ones are paired in order when they are as many and each pair's
    expressions still resemble each other."""
    keys = {k for k, _, _ in measured}
    new = collections.defaultdict(list)
    for k, _ln, s in measured:
        if k not in carried:
            new[k[:4] + (clean(s[5]),)].append(k)
    gone = collections.defaultdict(list)
    for k, row in carried.items():
        if k not in keys:
            gone[k[:4] + (row[6],)].append(row)
    out = {}
    for g, ks in new.items():
        rows = gone.get(g, [])
        if not rows or len(rows) != len(ks):
            continue
        rows = sorted(rows, key=lambda r: int(r[5]) if r[5].isdigit() else 0)
        pairs = list(zip(ks, rows))
        if all(difflib.SequenceMatcher(None, k[4], r[4]).ratio() >= 0.5 for k, r in pairs):
            out.update(pairs)
    return out


def clean(x):
    return ' '.join(str(x).replace('\t', ' ').split()) or '-'


def regenerate(check):
    measured = measure()
    carried = read_list(OUT)
    text, stats = render(measured, carried)
    reasons = collections.Counter(r.split('\t')[8] for r in text.splitlines() if not r.startswith('#'))
    if check:
        with open(OUT, encoding='utf-8') as fh:
            if fh.read() != text:
                sys.exit(f'typed_left: {OUT} is stale; run tools/typed_left.py '
                         f'({stats["new"]} new sites, {stats["dropped"]} keys gone)')
        print(f'typed_left: {OUT} current ({len(measured)} sites)')
        return
    with open(OUT, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(text)
    print(f'typed_left: {len(measured)} sites; {stats["carried"]} with a reason carried, '
          f'{stats["unreviewed"]} still without one, {stats["new"]} new; {stats["dropped"]} keys gone',
          file=sys.stderr)
    for k, v in reasons.most_common():
        print(f'{k}\t{v}', file=sys.stderr)


# ---- import: stage B's line-keyed lists ------------------------------------

def git(*args):
    return subprocess.run(['git', *args], cwd=ROOT, capture_output=True, text=True, encoding='utf-8',
                          errors='replace').stdout


def line_map(old, new):
    """1-based old line -> 1-based new line, by a diff of the two texts."""
    sm = difflib.SequenceMatcher(None, old, new, autojunk=False)
    m = {}
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        for i in range(i1, i2):
            if tag == 'equal':
                m[i + 1] = j1 + (i - i1) + 1
            elif j2 > j1:
                m[i + 1] = j1 + min(i - i1, j2 - j1 - 1) + 1
            else:
                m[i + 1] = j1 + 1
    return m


def num(s):
    s = s.strip().lstrip('+')
    s = re.sub(r'(?<=[0-9A-Fa-f])[uUlL]+$', '', s)
    try:
        return int(s, 0)
    except ValueError:
        try:
            return int(s, 16) if re.fullmatch(r'[0-9A-Fa-f]+', s) else None
        except ValueError:
            return None


def norm(s):
    return re.sub(r'\s+', '', s.replace('rv::', ''))


def site_offset(expr):
    """The constant offset at the end of a site's expression, or None."""
    m = re.search(r'([+-])\s*(0[xX][0-9A-Fa-f]+|\d+)[uUlL]*\s*$', expr)
    if m:
        v = num(m.group(2))
        if v is not None:
            return -v if m.group(1) == '-' else v
    return num(expr)


def list_commit(path):
    """The last commit that wrote a stage B list (they are removed after it)."""
    return git('log', '-1', '--format=%H', '--diff-filter=AM', '--', path).strip()


def read_old(path, commit):
    out = []
    for line in git('show', f'{commit}:{path}').splitlines():
        if line.startswith('#') or line.startswith('file\t') or not line.strip():
            continue
        out.append(line.split('\t'))
    return out


def import_lists(unmatched_path):
    measured = measure()
    by_file = collections.defaultdict(list)
    for i, (key, ln, s) in enumerate(measured):
        by_file[key[0]].append((i, ln, s))
    cur_text = {}

    def current_lines(rel):
        if rel not in cur_text:
            with open(os.path.join(ROOT, rel), encoding='utf-8', errors='replace') as fh:
                cur_text[rel] = fh.read().split('\n')
        return cur_text[rel]

    maps = {}

    def mapped(commit, rel, line):
        k = (commit, rel)
        if k not in maps:
            old = git('show', f'{commit}:{rel}').replace('\r\n', '\n').split('\n')
            maps[k] = line_map(old, current_lines(rel)) if old != [''] else {}
        return maps[k].get(line)

    assigned_b1, assigned_b0 = {}, {}
    unmatched = []
    stats = collections.Counter()

    def assign(rows, assigned, level_of, source):
        # rows: (rel, line_now, row, label); every candidate pair scored, then
        # taken best first, one row to one site
        pairs = []
        for r_i, (rel, ln, row, label) in enumerate(rows):
            if ln is None:
                continue
            for s_i, s_ln, s in by_file.get(rel, []):
                d = abs(s_ln - ln)
                if d > 60:
                    continue
                lv = level_of(row, s)
                if lv is None or (lv == 2 and d > 3) or (lv == 3 and d > 0):
                    continue
                # a row whose evidence quotes another accessor ranks after
                # one quoting this one (a read and a write of one field)
                q = re.match(r'\s*(\w+)\(', ' '.join(row[4:]) if source == 'b1' else row[4])
                other = 1 if q and q.group(1) != s[3] else 0
                pairs.append((lv, other, d, r_i, s_i))
        pairs.sort()
        taken_r, taken_s = set(), set()
        for lv, _other, d, r_i, s_i in pairs:
            if r_i in taken_r or s_i in taken_s:
                continue
            taken_r.add(r_i)
            taken_s.add(s_i)
            assigned[s_i] = (rows[r_i][2], rows[r_i][3])
            stats[f'{source} level {lv}'] += 1
        for r_i, (rel, ln, row, label) in enumerate(rows):
            if r_i not in taken_r:
                unmatched.append((label, row))
                stats[f'{source} unmatched'] += 1

    # B1: file, line, offset, reason, evidence
    rows = []
    for path in B1_LISTS:
        commit = list_commit(path)
        group = path.split('typed_b1_')[1].rsplit('_left', 1)[0]
        for row in read_old(path, commit):
            rel, line = row[0], int(row[1])
            rows.append((rel, mapped(commit, rel, line), row, f'b1-{group}'))

    def b1_level(row, s):
        # 0: the evidence names this accessor and expression; 1: the offset
        # agrees; 2: the row has no offset (near lines only); 3: the offset
        # disagrees (the same line only). An evidence match counts only
        # where the offset agrees: a row's evidence often quotes the
        # neighbouring site that made its pointer.
        acc, expr = s[3], s[4]
        ev = norm(' '.join(row[4:]))
        off = row[2].strip()
        o = num(off)
        so = site_offset(expr)
        if off in ('-', ''):
            agrees = None
        elif o is not None:
            agrees = so is not None and o == so
        else:
            agrees = bool(norm(off)) and norm(off) in norm(expr)
        if agrees is not False and norm(f'{acc}({expr}') in ev:
            return 0
        if agrees:
            return 1
        if agrees is None:
            return 2
        return 3

    assign(rows, assigned_b1, b1_level, 'b1')

    # B0: file, line, address, entry, accessor, constant, reason
    rows = []
    commit = list_commit(B0_LIST)
    for row in read_old(B0_LIST, commit):
        rel, line = row[0], int(row[1])
        rows.append((rel, mapped(commit, rel, line), row, 'b0'))

    def b0_level(row, s):
        const = row[5].split('::')[-1]
        if not re.search(r'\b' + re.escape(const) + r'\b', s[4]) and f'base={const}' not in s[6]:
            return None
        return 0 if row[4] == s[3] else 1

    assign(rows, assigned_b0, b0_level, 'b0')

    # A list row names its line once where the census counts a read and a
    # write of one field on it (`SetLong(c + 0x70, Long(c + 0x70) | 1)`):
    # the other site on that line with the same expression shares the row.
    by_line = {}
    for i, (key, ln, s) in enumerate(measured):
        if i in assigned_b1:
            by_line.setdefault((key[0], ln, s[4]), assigned_b1[i])
    for i, (key, ln, s) in enumerate(measured):
        if i not in assigned_b1 and (key[0], ln, s[4]) in by_line:
            assigned_b1[i] = by_line[(key[0], ln, s[4])]
            stats['b1 same line'] += 1

    carried = {}
    both = 0
    for i, (key, ln, s) in enumerate(measured):
        b1 = assigned_b1.get(i)
        b0 = assigned_b0.get(i)
        if not b1 and not b0:
            continue
        if b1:
            row, label = b1
            reason, evidence, source = row[3], ' '.join(row[4:]) or '-', label
            if b0:
                both += 1
                r0, _ = b0
                evidence += f' | b0: {r0[6]} ({r0[5]} {r0[2]} {r0[3]})'
                source += ';b0'
        else:
            row, label = b0
            reason, evidence, source = row[6], f'{row[5]} {row[2]} {row[3]}', label
        carried[key] = list(key) + [s[5], s[6], reason, evidence, source]
    text, rstats = render(measured, carried)
    with open(OUT, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(text)
    with open(unmatched_path, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('# tools/typed_left.py import: stage B list rows that match no census site today. '
                 'Addresses, names and reasons only.\n')
        for label, row in unmatched:
            fh.write(label + '\t' + '\t'.join(row) + '\n')
    for k, v in sorted(stats.items()):
        print(f'{k}\t{v}', file=sys.stderr)
    print(f'typed_left import: {len(measured)} sites, {len(carried)} with a reason '
          f'({both} from both lists), {rstats["new"]} without; {len(unmatched)} rows unmatched '
          f'(in {unmatched_path})', file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('command', nargs='?', choices=['import'])
    ap.add_argument('--check', action='store_true', help='fail if the committed list is stale')
    ap.add_argument('--unmatched', default='typed_left_unmatched.tsv',
                    help='import: where the rows that match no site go')
    a = ap.parse_args()
    if a.command == 'import':
        import_lists(os.path.abspath(a.unmatched))
    else:
        regenerate(a.check)
    return 0


if __name__ == '__main__':
    sys.exit(main())
