#!/usr/bin/env python
"""List every story-flag site in src/game: who sets, clears or tests which flag
of which row (docs/story-flags.md).

    python tools/story_flags.py census            every site, one TSV line each
    python tools/story_flags.py summary           per row: flags set / cleared / only tested
    python tools/story_flags.py chapter N         the sites of chapter N's row
    python tools/story_flags.py file NAME.cpp     every site in one file

A site is a call of Flags_Set / Flags_Clear / Flags_Test, direct or through a
file's own one-line wrapper (SetFlag(n), Flag(n), ...). The row is read from the
first argument: the current chapter's row (Chapter_FlagRow), a fixed chapter row
(Cond_Flags + 8 * c), or the event flags (Cond_Flags + 0xA0, save block +0x650).

This reads our source only. It prints flag numbers and function names, no game
data.
"""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'game')
OPS = {'Set': 'set', 'Clear': 'clear', 'Test': 'test'}
CALL = re.compile(r'Flags_(Set|Clear|Test)\)?\(')
FUNC = re.compile(r'^(?:extern "C" |static |inline )*[\w:<>\*& ]+?[ \*&](?:__cdecl |__stdcall |__fastcall )?(\w+)\([^;]*\)\s*(?:const\s*)?\{\s*$')
ONE_LINE = re.compile(r'^(?:static |inline |const )*[\w:<>\*& ]+?[ \*&](\w+)\(([^)]*)\)\s*\{(.*)\}\s*(?://.*)?$')
CASE = re.compile(r'^\s*(case [^:]+|default):')


def classify(expr):
    """('cur'|'event'|'row', chapter or None) for an expression naming a flag row, else None."""
    e = expr.replace(' ', '')
    if 'Chapter_FlagRow' in e:
        return ('cur', None)
    m = re.search(r'Cond_Flags(?:_addr\))?\[?\+?(0[xX][0-9a-fA-F]+|\d+)?', e)
    if m and 'Cond_Flags' in e:
        off = int(m.group(1), 0) if m.group(1) else 0
        if off == 0xA0:
            return ('event', None)
        if off % 8 == 0 and off < 0xA0:
            return ('row', off // 8)
        return ('other', off)
    return None


def split_args(text, start):
    """The arguments of the call whose '(' is at start-1; (args, end index)."""
    depth, args, cur, i = 1, [], '', start
    while i < len(text) and depth:
        c = text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                break
        if c == ',' and depth == 1:
            args.append(cur.strip())
            cur = ''
        else:
            cur += c
        i += 1
    args.append(cur.strip())
    return args, i


def num(s):
    s = s.strip().rstrip('uU')
    try:
        return int(s, 0)
    except ValueError:
        return None


def scan_file(path):
    name = os.path.basename(path)
    lines = open(path, encoding='utf8', errors='replace').read().split('\n')
    helpers, wrappers = {}, {}
    # helpers: a function of no arguments that returns a row; wrappers: one-liners around Flags_*
    for i, l in enumerate(lines):
        m = ONE_LINE.match(l)
        body, fname, params = (m.group(3), m.group(1), m.group(2)) if m else (None, None, None)
        if not m:
            m2 = re.match(r'^(?:static |inline |const )*unsigned char\s*\*\s*(\w+)\(\)\s*\{\s*$', l)
            if m2:
                fname, params, body = m2.group(1), '', ' '.join(lines[i + 1:i + 6])
        if body is None:
            continue
        c = CALL.search(body)
        if c and params.strip():
            args, _ = split_args(body, c.end())
            pname = params.split()[-1].lstrip('*&')
            if len(args) >= 2 and args[1] == pname:
                wrappers[fname] = (OPS[c.group(1)], args[0])
        elif not params.strip() and 'return' in body:
            k = classify(body)
            if k:
                helpers[fname] = k

    def resolve(expr, func_start, at):
        k = classify(expr)
        if k:
            return k
        m = re.match(r'^(\w+)\(\)$', expr)
        if m and m.group(1) in helpers:
            return helpers[m.group(1)]
        v = re.match(r'^(?:At|Mem|P|Ptr|&B)?\(?(\w+)\)?$', expr)
        if v:
            var = v.group(1)
            for j in range(at, func_start - 1, -1):
                a = re.search(r'\b' + re.escape(var) + r'\s*=\s*([^;]+);', lines[j])
                if a and j != at or (a and lines[j].index(a.group(0)) < lines[j].find('Flags_')):
                    return resolve(a.group(1).strip(), func_start, j - 1) if a.group(1).strip() != expr else None
        return None

    out, func, func_start = [], None, 0
    for i, l in enumerate(lines):
        if l[:1] not in (' ', '\t', '/', '#', '}', ''):
            m = FUNC.match(l)
            if m:
                func, func_start = m.group(1), i
        if func is None or ONE_LINE.match(l) and (l[:1] not in (' ', '\t')):
            continue
        code = l.split('//')[0]
        sites = []
        for c in CALL.finditer(code):
            args, _ = split_args(code, c.end())
            if len(args) >= 2:
                sites.append((OPS[c.group(1)], args[0], args[1]))
        for w, (op, bank) in wrappers.items():
            for c in re.finditer(r'(?<![\w>.])' + re.escape(w) + r'\(', code):
                args, _ = split_args(code, c.end())
                sites.append((op, bank, args[0]))
        if not sites:
            continue
        case = next((CASE.match(lines[k]).group(1) for k in range(i, func_start, -1) if CASE.match(lines[k])), '')
        for op, bank, idx in sites:
            k = resolve(bank, func_start, i)
            out.append(dict(file=name, line=i + 1, func=func, case=case, op=op,
                            bank=k[0] if k else '?', row=k[1] if k else None,
                            flag=num(idx), flag_text=idx, bank_text=bank))
    return out


def chapter_of(func):
    """The chapter a function belongs to, by its ScenaNN_ name. A file is not one chapter:
    scena_sc3.cpp holds chapters 3 and 4, scena_sc15.cpp 15, 17, 18 and 19."""
    m = re.match(r'Scena(\d+)_', func)
    return int(m.group(1)) if m else None


def all_sites():
    out = []
    for f in sorted(os.listdir(ROOT)):
        if f.endswith('.cpp') and not re.search(r'_fuzz|_callees|harness', f):
            for s in scan_file(os.path.join(ROOT, f)):
                if s['bank'] == 'cur':
                    s['row'] = chapter_of(s['func'])   # None for a function not named for a chapter
                out.append(s)
    return out


def rng(b):
    b, out, i = sorted(b), [], 0
    while i < len(b):
        j = i
        while j + 1 < len(b) and b[j + 1] == b[j] + 1:
            j += 1
        out.append('%d' % b[i] if i == j else '%d..%d' % (b[i], b[j]))
        i = j + 1
    return ' '.join(out) or '-'


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'summary'
    sites = all_sites()
    if cmd == 'census':
        print('file\tline\tfunction\tcase\top\tbank\trow\tflag\tbank_text')
        for s in sites:
            print('\t'.join(str(s[k]) if s[k] is not None else '' for k in
                            ('file', 'line', 'func', 'case', 'op', 'bank', 'row')) +
                  '\t%s\t%s' % (s['flag'] if s['flag'] is not None else s['flag_text'], s['bank_text']))
    elif cmd in ('chapter', 'file'):
        for s in sites:
            if cmd == 'file':
                if s['file'] != sys.argv[2]:
                    continue
                print('%-6s %-9s ' % (s['bank'], '' if s['row'] is None else 'row %d' % s['row']), end='')
            if cmd == 'file' or (s['bank'] in ('cur', 'row') and s['row'] == int(sys.argv[2])):
                print('%-5s %-4s %-16s %5d  %-28s %s' % (s['op'], s['flag'] if s['flag'] is not None else s['flag_text'],
                                                        s['file'], s['line'], s['func'], s['case']))
    else:
        print('%d sites; unresolved row: %d; flag number not a literal: %d' % (
            len(sites), sum(1 for s in sites if s['bank'] == '?'), sum(1 for s in sites if s['flag'] is None)))
        rows = {}
        for s in sites:
            key = ('event',) if s['bank'] == 'event' else ('row', s['row']) if s['bank'] in ('cur', 'row') else ('?',)
            rows.setdefault(key, []).append(s)
        for key in sorted(rows, key=lambda k: (k[0] != 'row', k[1] if len(k) > 1 and k[1] is not None else 99)):
            ss = rows[key]
            st = {s['flag'] for s in ss if s['op'] == 'set' and s['flag'] is not None}
            cl = {s['flag'] for s in ss if s['op'] == 'clear' and s['flag'] is not None}
            te = {s['flag'] for s in ss if s['op'] == 'test' and s['flag'] is not None}
            label = 'event flags' if key[0] == 'event' else 'unresolved' if key[0] == '?' else (
                'current row, function not named for a chapter' if key[1] is None else 'chapter %d' % key[1])
            print('%s: %d sites in %s' % (label, len(ss), ', '.join(sorted({s['file'] for s in ss}))))
            print('   set     %s\n   cleared %s\n   tested, never set here %s' % (rng(st), rng(cl), rng(te - st)))


if __name__ == '__main__':
    main()
