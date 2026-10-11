#!/usr/bin/env python
"""Which of Capcom's functions does our code still call?

    python tools/owned_sweep.py            # the remainder, one line each
    python tools/owned_sweep.py --verbose  # and where each is named

Our code reaches a function of the game in ways that `inject: N ours` does
not count (it counts only `[[func]]` entries with an `impl`):

  1. by name through a harness's call macro (`AH_CALL(Name)`, and `BH_CALL` /
     `MH_CALL` / `SH_CALL`, the same macro for the other harnesses), which
     binds to Capcom's address whenever the name's `[[func]]` has no `impl`;
  2. as a callee a fuzz's clone is re-aimed from: the `{offset, 0x...}` rows
     of an array typed `ah::CallSite` (or `bh::` / `mh::` / `sh::`);
  3. by its bare name anywhere else in code - a module's table of callees
     (`kOriginals`), a direct call - which gen_symbols.py binds to Capcom's
     address the same way.

Every such callee that lies in `.text` (0x401000..0x5C4000) and whose
`[[func]]` has no `impl`, or (1 and 2) that has no `[[func]]` at all, is
reported. A call macro over a name that is no symbol - a function-pointer
variable, a template parameter - is a call through a pointer: counted, not
reported (`--verbose` lists them).

Only arrays typed `*::CallSite` are read for (2): the `{start, end}` pairs in
boss_harness.h, scenario_harness.cpp and mode_states.cpp are address bands,
not call sites, and a looser scan would report their ends. A row's target may
be a literal, `bof3::addr::Name`, or a constant the same file defines
(`kMove = 0x578C10`). Comments, string literals and `#define` lines are
skipped.

A function reported is not by itself an error - the MP3 decoder's eight
(DIV-0091, reached only under BOF3X_MP3=original) and the CRT's _ftol (which
ours inline; the clones' callees in fuzzes) are expected - but anything else
is a gap: Capcom's bytes still running under a caller that is ours.
docs/move-cmds.md section 8 has the run that introduced this tool.
"""
import argparse, os, re, sys, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEXT_LO, TEXT_HI = 0x401000, 0x5C4000

CALL_MACRO = re.compile(r'\b(AH|BH|MH|SH)_CALL\(\s*([A-Za-z_]\w*)\s*\)')
CALLSITE_ARRAY = re.compile(r'\b(?:ah|bh|mh|sh)::CallSite\s+\w+\s*\[\s*\w*\s*\]\s*=\s*\{')
ROW = re.compile(r'\{\s*(0[xX][0-9A-Fa-f]+|\d+)\s*,\s*((?:bof3::addr::)?[A-Za-z_]\w*|0[xX][0-9A-Fa-f]+)\s*[,}]')
CONST = re.compile(r'\b(k\w+)\s*=\s*(0[xX][0-9A-Fa-f]+)\b')
NOISE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|^[ \t]*#[ \t]*define\b(?:[^\n]*\\\n)*[^\n]*', re.S | re.M)


def load_symbols():
    with open(os.path.join(ROOT, 'symbols.toml'), 'rb') as f:
        t = tomllib.load(f)
    by_name = {e['name']: e for e in t.get('func', [])}
    by_pc = {e['pc']: e for e in t.get('func', [])}
    addr = {e['name']: e['pc'] for kind in ('func', 'data', 'block') for e in t.get(kind, [])}
    return by_name, by_pc, addr


def array_body(text, start):
    """The text of the brace group that opens just before text[start]."""
    depth, i = 1, start
    while depth and i < len(text):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
        i += 1
    return text[start:i - 1]


def sources():
    for base, _, files in os.walk(os.path.join(ROOT, 'src')):
        for name in sorted(files):
            if name.endswith(('.cpp', '.h', '.hpp', '.inc')):
                yield os.path.join(base, name)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--verbose', action='store_true', help="list each reported callee's files, and the pointer calls")
    a = ap.parse_args()
    by_name, by_pc, addr = load_symbols()
    theirs = {n: e for n, e in by_name.items() if 'impl' not in e and TEXT_LO <= e['pc'] < TEXT_HI}
    bare = re.compile(r'(?<![\w:])(?:::)?(' + '|'.join(map(re.escape, sorted(theirs))) + r')\b') if theirs else None

    gaps = {}          # key -> (what, set of files)
    pointers = set()   # call macros over a variable or template parameter
    unresolved = []    # CallSite rows whose target could not be read
    n_macro = n_rows = 0

    def note(key, what, place):
        gaps.setdefault(key, (what, set()))[1].add(place)

    def no_impl(e):
        return f'{e["pc"]:#08x} {e["name"]}: [[func]] without impl'

    for path in sources():
        rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
        with open(path, encoding='utf-8', errors='replace') as f:
            text = NOISE.sub(lambda m: '""' if m.group(0).startswith('"') else '', f.read())
        consts = {m.group(1): int(m.group(2), 16) for m in CONST.finditer(text)}

        for m in CALL_MACRO.finditer(text):
            n_macro += 1
            name = m.group(2)
            e = by_name.get(name)
            if e is None:
                if name in addr:
                    note(('name', name), f'{m.group(1)}_CALL({name}): a symbol, but no [[func]]', rel)
                else:
                    pointers.add(f'{rel}: {m.group(1)}_CALL({name})')
            elif 'impl' not in e and TEXT_LO <= e['pc'] < TEXT_HI:
                note(('pc', e['pc']), no_impl(e), rel)

        for m in CALLSITE_ARRAY.finditer(text):
            for row in ROW.finditer(array_body(text, m.end())):
                n_rows += 1
                target = row.group(2)
                if target.lower().startswith('0x'):
                    pc = int(target, 16)
                elif target.startswith('bof3::addr::') and target[12:] in addr:
                    pc = addr[target[12:]]
                elif target in consts:
                    pc = consts[target]
                else:
                    unresolved.append(f'{rel}: {{{row.group(1)}, {target}}}')
                    continue
                if not TEXT_LO <= pc < TEXT_HI:
                    continue
                e = by_pc.get(pc)
                if e is None:
                    note(('pc', pc), f'{pc:#08x}: no [[func]] starts here', rel)
                elif 'impl' not in e:
                    note(('pc', pc), no_impl(e), rel)

        if bare:
            for m in bare.finditer(text):
                e = theirs[m.group(1)]
                note(('pc', e['pc']), no_impl(e), rel)

    print(f'owned_sweep: {n_macro} harness call macros ({len(pointers)} through a pointer), '
          f'{n_rows} CallSite rows; {len(gaps)} callee(s) not ours')
    for key in sorted(gaps, key=lambda k: (k[0], str(k[1]))):
        what, places = gaps[key]
        print(f'  {what} ({len(places)} file(s))')
        if a.verbose:
            for p in sorted(places):
                print(f'      {p}')
    if a.verbose:
        for p in sorted(pointers):
            print(f'  through a pointer: {p}')
    for u in unresolved:
        print(f'  unresolved row: {u}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
