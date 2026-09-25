#!/usr/bin/env python
"""Check the repository's two ledgers against each other and against the code.

    python tools/ledger_check.py            # exit 1 on any error
    python tools/ledger_check.py --verbose  # also list the informational notes

Reads only files in this repository - no game data, no build tree - so it runs
anywhere, including CI (.github/workflows/checks.yml).

The divergence ledger, docs/DIVERGENCE.md (CLAUDE.md rule 2):
  * every entry under "## Entries" is a `### title` followed by exactly one
    `- **ID:** DIV-NNNN`, and carries the fields the entry format names;
  * IDs run DIV-0001, DIV-0002, ... with no gap and no repeat;
  * the Status line's "N entries, DIV-0001..NNNN" matches the entries;
  * every DIV-NNNN cited anywhere in the repository exists.

The ownership ledger, symbols.toml `impl` against the detours in src/:
  * every function detoured to ours (BOF3_INJECT, or a direct bof3::Inject
    with bof3::addr::<name>) has an `impl` line, and every `impl` is detoured -
    so "N impl" and the log's "N ours" are the same number;
  * no name is detoured twice.

Rule 4 (no stubs):
  * no TODO / FIXME / XXX / "for now" markers in src/.

A known gap that predates this check is listed in KNOWN below with its reason;
the check fails if a listed gap is ever closed, so the list cannot go stale.
"""
import argparse, os, re, sys, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.path.join('docs', 'DIVERGENCE.md')

# Fields every entry carries (docs/DIVERGENCE.md, "Entry format"). A field may
# carry a parenthetical qualifier: "Rationale (why this side)".
REQUIRED = ['ID', 'Date', 'Subsystem', 'Original behaviour', 'New behaviour',
            'Rationale', 'Also in the PSX version?', 'Reversible?']

# (DIV id, missing field) -> why it is accepted. Content only the author can
# write; the check will not invent it.
KNOWN = {}

# Where a DIV-NNNN may be cited. Everything tracked that is text.
CITE_EXTS = {'.md', '.py', '.cpp', '.h', '.toml', '.rc', '.txt', '.yml', '.cmake'}
CITE_SKIP_DIRS = {'.git', 'build', 'analysis', '_deps', '.claude'}

STUB_MARKERS = re.compile(r'\b(TODO|FIXME|XXX)\b|\bfor now\b', re.I)


class Report:
    def __init__(self):
        self.errors, self.notes = [], []

    def error(self, msg):
        self.errors.append(msg)

    def note(self, msg):
        self.notes.append(msg)


def read(rel):
    with open(os.path.join(ROOT, rel), encoding='utf-8') as f:
        return f.read()


def walk(top, exts):
    for d, dirs, files in os.walk(os.path.join(ROOT, top)):
        dirs[:] = sorted(x for x in dirs if x not in CITE_SKIP_DIRS)
        for name in sorted(files):
            if os.path.splitext(name)[1] in exts:
                p = os.path.join(d, name)
                yield os.path.relpath(p, ROOT).replace(os.sep, '/'), p


# --- the divergence ledger ---------------------------------------------------

def check_divergence(r):
    text = read(LEDGER)
    head, sep, body = text.partition('\n## Entries\n')
    if not sep:
        r.error(f'{LEDGER}: no "## Entries" section')
        return set()
    body_line0 = head.count('\n') + 2

    # Split into entries at each `### ` heading, keeping line numbers.
    entries, cur = [], None
    for i, line in enumerate(body.split('\n')):
        if line.startswith('### '):
            cur = {'title': line[4:].strip(), 'line': body_line0 + i, 'lines': []}
            entries.append(cur)
        elif line.startswith('## '):
            cur = None
        elif cur is not None:
            cur['lines'].append(line)

    ids, used_known = [], set()
    for e in entries:
        where = f'{LEDGER}:{e["line"]} "{e["title"]}"'
        chunk = '\n'.join(e['lines'])
        found = re.findall(r'^- \*\*ID:\*\*\s*(DIV-\d{4})\b', chunk, re.M)
        if len(found) != 1:
            r.error(f'{where}: {len(found)} ID lines, want exactly 1'
                    + (f' ({", ".join(found)} - a heading missing?)' if found else ''))
        if not found:
            continue
        did = found[0]
        ids.append((did, where))
        for field in REQUIRED:
            pat = r'^- \*\*' + re.escape(field) + r'(?: \([^)]*\))?:?\*\*'
            if re.search(pat, chunk, re.M):
                continue
            if (did, field) in KNOWN:
                used_known.add((did, field))
                r.note(f'{did}: no "{field}" field (known: {KNOWN[did, field]})')
            else:
                r.error(f'{where}: {did} has no "- **{field}:**" field')
        m = re.search(r'^- \*\*Date:\*\*\s*(\S+)', chunk, re.M)
        if m and not re.fullmatch(r'\d{4}-\d{2}-\d{2}', m.group(1)):
            r.error(f'{where}: {did} date {m.group(1)!r} is not YYYY-MM-DD')

    for key in KNOWN.keys() - used_known:
        r.error(f'ledger_check.py KNOWN lists {key[0]} missing "{key[1]}", '
                'but it is there now - remove the KNOWN line')

    seen = {}
    for n, (did, where) in enumerate(ids, 1):
        if did in seen:
            r.error(f'{where}: {did} reused (first at {seen[did]})')
            continue
        seen[did] = where
        want = f'DIV-{n:04d}'
        if did != want:
            r.error(f'{where}: {did} out of sequence, expected {want}')
    last = ids[-1][0] if ids else None

    m = re.search(r'^\*\*Status:\*\*.*?(\d+) entries, DIV-0001\.\.(\d{4})', head, re.M)
    if not m:
        r.error(f'{LEDGER}: Status line does not say "N entries, DIV-0001..NNNN"')
    elif ids:
        n, top = int(m.group(1)), f'DIV-{m.group(2)}'
        if n != len(ids) or top != last:
            r.error(f'{LEDGER}: Status line says {n} entries to {top}; '
                    f'the ledger has {len(ids)} to {last}')
    return set(seen)


def check_citations(r, defined):
    cited = {}
    for top in ('.',):
        for rel, path in walk(top, CITE_EXTS):
            if rel == LEDGER:
                continue
            try:
                with open(path, encoding='utf-8') as f:
                    lines = f.read().split('\n')
            except UnicodeDecodeError:
                continue
            for i, line in enumerate(lines, 1):
                for m in re.finditer(r'\bDIV-(\d{4})\b', line):
                    cited.setdefault(f'DIV-{m.group(1)}', f'{rel}:{i}')
    for did, where in sorted(cited.items()):
        if did not in defined:
            r.error(f'{where}: cites {did}, which the ledger does not have')
    code = set()
    for rel, path in walk('src', {'.cpp', '.h'}):
        with open(path, encoding='utf-8') as f:
            code.update(f'DIV-{x}' for x in re.findall(r'\bDIV-(\d{4})\b', f.read()))
    quiet = sorted(defined - code)
    if quiet:
        r.note(f'{len(quiet)} entries not cited in src/ (data-only or tooling '
               f'divergences are expected here): {", ".join(quiet)}')


# --- the ownership ledger ----------------------------------------------------

INJECT_MACRO = re.compile(r'\bBOF3_INJECT\(\s*(\w+)\s*\)')
INJECT_DIRECT = re.compile(r'\bInject\(\s*"(\w+)"\s*,\s*(?:::)?bof3::addr::(\w+)\s*,')


def check_ownership(r):
    with open(os.path.join(ROOT, 'symbols.toml'), 'rb') as f:
        t = tomllib.load(f)
    impl = {}
    for e in t.get('func', []):
        if 'impl' in e:
            impl[e['name']] = e['impl']
    for kind in ('data', 'block'):
        for e in t.get(kind, []):
            if 'impl' in e:
                r.error(f'symbols.toml: [[{kind}]] {e["name"]} has an impl; only functions can')

    injected = {}
    for rel, path in walk('src', {'.cpp', '.h'}):
        with open(path, encoding='utf-8') as f:
            src = f.read()
        # The macro's own #define line is not a use of it.
        src = re.sub(r'^\s*#define[^\n]*(?:\\\n[^\n]*)*', lambda m: '\n' * m.group(0).count('\n'),
                     src, flags=re.M)
        hits = [(m.start(), m.group(1)) for m in INJECT_MACRO.finditer(src)]
        for m in INJECT_DIRECT.finditer(src):   # the call may wrap lines
            if m.group(1) != m.group(2):
                r.error(f'{rel}:{src.count(chr(10), 0, m.start()) + 1}: Inject names '
                        f'"{m.group(1)}" but passes bof3::addr::{m.group(2)}')
            hits.append((m.start(), m.group(2)))
        for pos, name in sorted(hits):
            where = f'{rel}:{src.count(chr(10), 0, pos) + 1}'
            if name in injected:
                r.error(f'{where}: {name} detoured twice (also {injected[name]})')
            else:
                injected[name] = where

    for name, where in sorted(injected.items()):
        if name not in impl:
            r.error(f'{where}: {name} is detoured to ours but symbols.toml has no impl for it')
    for name, path in sorted(impl.items()):
        if name not in injected:
            r.error(f'symbols.toml: {name} has impl = "{path}" but nothing in src/ detours it')
        if not os.path.isfile(os.path.join(ROOT, path)):
            r.error(f'symbols.toml: {name} impl file {path} does not exist')
    r.note(f'{len(impl)} impl lines, {len(injected)} functions detoured')


# --- rule 4 ------------------------------------------------------------------

def check_stubs(r):
    for rel, path in walk('src', {'.cpp', '.h'}):
        with open(path, encoding='utf-8') as f:
            for i, line in enumerate(f, 1):
                m = STUB_MARKERS.search(line)
                if m:
                    r.error(f'{rel}:{i}: "{m.group(0)}" - CLAUDE.md rule 4: '
                            'implement it fully or abort loudly')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--verbose', '-v', action='store_true')
    a = ap.parse_args()

    r = Report()
    defined = check_divergence(r)
    check_citations(r, defined)
    check_ownership(r)
    check_stubs(r)

    if a.verbose:
        for n in r.notes:
            print(f'note: {n}')
    for e in r.errors:
        print(f'error: {e}')
    print(f'ledger_check: {len(defined)} ledger entries, '
          f'{len(r.errors)} error(s), {len(r.notes)} note(s)')
    return 1 if r.errors else 0


if __name__ == '__main__':
    sys.exit(main())
