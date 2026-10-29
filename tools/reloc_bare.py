#!/usr/bin/env python
"""The bare-address rule: no new unplaced image address in src/game, src/input, src/render.

    python tools/reloc_bare.py              # write analysis/reloc_bare.tsv, print the counts
    python tools/reloc_bare.py --out FILE   # write elsewhere
    python tools/reloc_bare.py --check      # fail on a site the committed file does not list
    python tools/reloc_bare.py --list       # print every site, write nothing

Stage D of the relocatable-image round (docs/relocatable-image-round.md 5).
Since stage C the image may lie elsewhere than 0x400000 (a developer's
BOF3X_IMAGEBASE), and in bof3x.exe a code VA is a name, not a function: a VA
becomes a place only through ::bof3::Img (data), ::bof3::Code (code) or
::bof3::CodePlace (a code word known at run time), common/image_base.h. A VA
used as a place without them reads or calls at the linked address, which
works at 0x400000 and faults (or reads the wrong bytes) anywhere else - the
class the stage C proof found four times, one route at a time. This rule
refuses a new one at review time instead. Reads src/ only; writes our own
source lines, addresses and names - no game data.

Scope: every .cpp / .h under src/game, src/input and src/render except the
shadow fuzz (a file name with `_fuzz.` or `fuzz_`, hosted only: BOF3.exe is at
0x400000 by construction and the exe host refuses BOF3X_SHADOW). The harness
files are in scope: their live helpers are called by game code (stage C1's
magic_harness PartyOf). Comments and strings are blanked first.

A **site** is one of four shapes, outside the argument of Img( / Code( /
CodePlace( / VaOf( and outside a static_assert:

  literal   a hex literal of six digits or more whose value lies in the
            linked span 0x400000..0x98F000, not the initialiser of an
            image::Va / CodeVa definition (`constexpr Va kX{0x6C3A4C};` is
            a name; `0x6C3A4C` anywhere else may be a place). Most existing
            ones are world coordinates (`x == 0x480000`) or names (a Log
            argument, a VA a test compares): listed, not changed.
  accessor  a VA name - `Name_addr`, `bof3::addr::Name`, `kX.v`, or an
            integer k-constant defined (in the file or a header it includes)
            from one of those or from a literal in the span - inside the
            argument list of a memory accessor that does not place (its
            parameter is an integer address cast straight to a pointer:
            discovered in the scope, plus reinterpret_cast<T*>(...) itself).
            Stage C1's shell.cpp:237 `At(kBlocks)` and magic_harness's
            `Mem(ObjTrio_addr + ...)` were this shape (c1 sweep3 / sweep6).
  fncast    a cast to a function-pointer type (Fn<>, Raw<>, As<>, H<>, a
            template argument or reinterpret_cast<> naming a function-pointer
            type or an alias of one) whose argument begins with a code VA
            (`bof3::addr::`, `kX`, `kX.v`, a literal in 0x401000..0x5C4000)
            not through Code( / CodePlace( (stage C1's field_event.cpp:43-47,
            c1 castsweep).
  taskarg   an argument of Task_Create / Task_Restart (any spelling) that is
            a bare code VA - a task entry the game will call (castsweep's
            second half).

Each site is keyed by (shape, file, token, the line's text with whitespace
collapsed) - not the line number, so an edit elsewhere in a file moves
nothing - and counted. `--check` fails when a key's count rises above the
committed file's or a key is new; a key that fell is reported as stale (run
without --check to rewrite the file, which then lists only what is there).
So a new bare address is refused until someone either places it (the
normal answer) or decides it is a name and commits the regenerated file -
a reviewer then sees the line in the diff of analysis/reloc_bare.tsv.

What the rule cannot see: a VA that reaches a place at run time - read from
a table of VAs of ours, passed through a parameter or a variable, computed
from a code word (a word the game holds is already a place: placing it again
is the opposite error, which no spelling rule tells from a VA); a literal
below six hex digits; an accessor defined so that the rule does not find it
(a parameter of another type, a body over several lines it does not read);
a place the generated headers give (tools/gen_symbols.py places every typed
name itself). The run-time checks cover those: the fence (a moved image's
linked span no-access, exe_host.cpp Fence), the trap (.stub no-access), and
CodeToCall's Fatal.
"""
import argparse, bisect, collections, glob, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from data_census import strip_code  # noqa: E402  (the census's reader: the same blanking)

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
DEFAULT_OUT = 'analysis/reloc_bare.tsv'
SPAN_LO, SPAN_HI = 0x400000, 0x98F000          # the linked span: bof3x.exe's SizeOfImage at 0x400000
CODE_LO, CODE_HI = 0x401000, 0x5C4000          # Capcom's code, image_base.h kCodeLo..kCodeHi
SHAPES = ('literal', 'accessor', 'fncast', 'taskarg')

HEX = re.compile(r'(?<![\w.])0x([0-9A-Fa-f]{6,8})(?:[uU][lL]{0,2}|[lL]{1,2}[uU]?)?\b')
PLACERS = re.compile(r'(?<![\w.])(?:(?:::)?bof3::)?(?:Img|Code|CodePlace|CodeToCall|CodePlaceToCall|VaOf)\s*\(')
STATIC_ASSERT = re.compile(r'\bstatic_assert\s*\(')
VA_DEF = re.compile(r'\b(?:image::)?(?:Va|CodeVa)\b\s*(?:\w+\s*)?\{')
FNPTR = r'\(\s*(?:__cdecl|__stdcall|__fastcall|__thiscall|WINAPI|CALLBACK)?\s*\*\s*\w*\s*\)'
VA_NAME = r'(?:(?:::)?bof3::addr::\w+|\b\w+_addr\b|(?:\b\w+::)*\bk[A-Z]\w*\.v\b)'
TASK = re.compile(r'(?<![\w.])(?:\w+(?:\.|->|::))?(?:Task_Create|Task_Restart|task_create|task_restart)\s*\(')
CODE_START = re.compile(r'\s*(?:\(\s*)*(?:static_cast<\s*(?:std::)?u?int(?:ptr|32)_t\s*>\s*\(\s*)*'
                        r'((?:::)?bof3::addr::\w+|(?:\w+::)*k[A-Z]\w*(?:\.v)?|0x[0-9A-Fa-f]{6,8})\b')


# a task argument that is a code VA: the name alone, or through the casts that
# make it a pointer (reinterpret_cast<void*>(static_cast<std::uintptr_t>(...)))
TASK_ARG = re.compile(r'\s*(?:(?:\(|(?:reinterpret|static)_cast\s*<[^<>;]*>\s*\()\s*)*'
                      r'((?:::)?bof3::addr::\w+|(?:\w+::)*k[A-Z]\w*\.v|0x[0-9A-Fa-f]{6,8})\b[\s)]*$')


def in_scope(rel):
    rel = rel.replace('\\', '/')
    if not re.match(r'src/(game|input|render)/', rel) or not rel.endswith(('.cpp', '.h')):
        return False
    return not re.search(r'_fuzz\.|fuzz_', os.path.basename(rel))


def close_paren(t, i, open_='(', close=')'):
    """The index just past the bracket that closes the one before i."""
    depth = 1
    while i < len(t) and depth:
        c = t[i]
        if c == open_:
            depth += 1
        elif c == close:
            depth -= 1
        i += 1
    return i


def template_end(t, i):
    depth = 1
    while i < len(t) and depth:
        c = t[i]
        if c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
        elif c in ';{}':
            return -1
        i += 1
    return i


def split_args(s):
    parts, depth, cur = [], 0, ''
    for ch in s:
        if ch in '(<[{':
            depth += 1
        elif ch in ')>]}':
            depth -= 1
        if ch == ',' and depth == 0:
            parts.append(cur)
            cur = ''
        else:
            cur += ch
    parts.append(cur)
    return parts


class Source:
    def __init__(self, rel):
        self.rel = rel.replace('\\', '/')
        with open(os.path.join(ROOT, rel), encoding='utf-8', errors='replace') as f:
            self.raw = f.read()
        self.text = strip_code(self.raw)
        self.lines = self.text.split('\n')
        self.nl = [m.start() for m in re.finditer('\n', self.text)]
        # positions covered by a placer's argument, a static_assert or a Va definition's braces
        cov = bytearray(len(self.text))
        for rx, op, cl in ((PLACERS, '(', ')'), (STATIC_ASSERT, '(', ')'), (VA_DEF, '{', '}')):
            for m in rx.finditer(self.text):
                e = close_paren(self.text, m.end(), op, cl)
                cov[m.start():e] = b'\x01' * (e - m.start())
        self.cov = cov

    def line_of(self, pos):
        return bisect.bisect_right(self.nl, pos - 1) + 1 if pos else 1

    def key_line(self, pos):
        return ' '.join(self.lines[self.line_of(pos) - 1].split())


def includes(src, by_rel, seen=None):
    seen = set() if seen is None else seen
    if src.rel in seen:
        return seen
    seen.add(src.rel)
    for m in re.finditer(r'#include\s+"([^"]+)"', src.raw):
        for cand in ('src/' + m.group(1), os.path.dirname(src.rel) + '/' + m.group(1)):
            if cand in by_rel:
                includes(by_rel[cand], by_rel, seen)
                break
    return seen


INT_DEF = re.compile(r'\b(?:constexpr|const)\s+(?:static\s+)?(?:U|std::uint32_t|uint32_t|unsigned(?:\s+int|\s+long)?|'
                     r'std::uintptr_t)\s+(k[A-Z]\w*)\s*=\s*([^;,]+)')


def va_ints(src):
    """The integer k-constants a file defines from a VA."""
    out = set()
    for m in INT_DEF.finditer(src.text):
        v = m.group(2)
        if re.search(VA_NAME, v):
            out.add(m.group(1))
            continue
        h = re.match(r'\s*\(?\s*0x([0-9A-Fa-f]+)', v)
        if h and SPAN_LO <= int(h.group(1), 16) < SPAN_HI:
            out.add(m.group(1))
    return out


IDENT_DEF = re.compile(r'^[ \t]*(?:template\s*<[^>\n]*>\s*)?(?:(?:inline|static|constexpr|extern "C")\s+)*'
                       r'[\w:<>\s\*&]*?\b(\w+)\s*\(\s*(?:const\s+)?(?:U|std::uint32_t|uint32_t|unsigned(?:\s+long)?|'
                       r'std::uintptr_t)\s+(\w+)\s*(?:,[^)]*)?\)\s*(?:const\s*)?\{', re.M)


def identity_accessors(srcs):
    """Names of functions in the scope whose integer parameter is cast straight
    to a pointer (or memcpy'd from, or handed to At) with no placer in the body."""
    names = set()
    for s in srcs:
        for m in IDENT_DEF.finditer(s.text):
            body = s.text[m.end():close_paren(s.text, m.end(), '{', '}')]
            if len(body) > 600 or PLACERS.search(body):
                continue
            p = re.escape(m.group(2))
            if (re.search(r'reinterpret_cast<[^>]*\*[^>]*>\(\s*(?:static_cast<\s*std::uintptr_t\s*>\(\s*)?' + p + r'\b', body)
                    or re.search(r'memcpy\([^;]*\b' + p + r'\b', body) or re.search(r'\bAt\(\s*' + p + r'\b', body)):
                names.add(m.group(1))
    return names


def fnptr_aliases(srcs):
    out = set()
    for s in srcs:
        for m in re.finditer(r'\busing\s+(\w+)\s*=\s*[^;]*?' + FNPTR, s.text):
            out.add(m.group(1))
        for m in re.finditer(r'\btypedef\s+[^;]*?\(\s*(?:__cdecl|__stdcall|__fastcall)?\s*\*\s*(\w+)\s*\)', s.text):
            out.add(m.group(1))
    return out


def is_code_token(tok):
    if tok.startswith('0x'):
        return CODE_LO <= int(tok, 16) < CODE_HI
    return True


def scan(src, ints, accessors, aliases, sites):
    t, cov = src.text, src.cov

    def add(shape, pos, token):
        sites.append((shape, src.rel, src.line_of(pos), token, src.key_line(pos)))

    # literal
    for m in HEX.finditer(t):
        if cov[m.start()]:
            continue
        v = int(m.group(1), 16)
        if SPAN_LO <= v < SPAN_HI:
            add('literal', m.start(), '0x' + m.group(1).upper())

    # accessor: a VA name in a non-placing accessor's arguments
    if accessors:
        call = re.compile(r'(?<![\w.>:])(?:' + '|'.join(map(re.escape, sorted(accessors, key=len, reverse=True)))
                          + r')\s*\(|\breinterpret_cast<[^<>;]*\*\s*>\s*\(')
        names_rx = re.compile(VA_NAME + (r'|(?<![\w.:])(?:at::)?(?:' + '|'.join(map(re.escape, sorted(ints))) + r')\b(?![\w.(\[])'
                                         if ints else ''))
        for m in call.finditer(t):
            e = close_paren(t, m.end())
            # a name inside another call's arguments (Index(who, i, n, kX.v),
            # SH_AT(F, kX.v)) is that call's, not this accessor's address; a
            # nested accessor is matched on its own
            nested = bytearray(e - m.end())
            for c in re.finditer(r'\b\w+\s*(?:<[^<>;()]*>)?\s*\(', t[m.end():e - 1]):
                ce = close_paren(t, m.end() + c.end())
                nested[c.end():ce - m.end()] = b'\x01' * (ce - m.end() - c.end())
            for n in names_rx.finditer(t, m.end(), e - 1):
                if cov[n.start()] or nested[n.start() - m.end()]:
                    continue
                if t[n.end():n.end() + 1] == '(':
                    continue   # a call: Name_addr() does not exist; k...() is a function
                add('accessor', n.start(), n.group(0).strip())

    # fncast
    for m in re.finditer(r'\b(\w+)\s*<', t):
        name = m.group(1)
        if name in ('static_cast', 'const_cast', 'dynamic_cast', 'template', 'operator'):
            continue
        te = template_end(t, m.end())
        if te < 0 or t[te:te + 1] != '(':
            continue
        targ = t[m.end():te - 1]
        fn = re.search(FNPTR, targ) is not None or targ.strip() in aliases
        if not (name in ('Fn', 'Raw', 'As', 'H') or fn):
            continue
        if name == 'reinterpret_cast' and not fn:
            continue
        if cov[te]:
            continue
        ae = close_paren(t, te + 1)
        arg = t[te + 1:ae - 1]
        if re.match(r'\s*(?:\(\s*)*' + PLACERS.pattern, arg):
            continue
        c = CODE_START.match(arg)
        if c and is_code_token(c.group(1)):
            add('fncast', te + 1 + c.start(1), c.group(1))

    # taskarg
    for m in TASK.finditer(t):
        head = t[t.rfind('\n', 0, m.start()) + 1:m.start()]
        if re.search(r'\b(?:void|U|unsigned|int|static|extern|inline|auto)\b[\w\s\*&:<>]*$', head):
            continue   # a declaration or definition
        ae = close_paren(t, m.end())
        pos = m.end()
        for part in split_args(t[m.end():ae - 1]):
            start = pos + len(part) - len(part.lstrip())
            pos += len(part) + 1
            if cov[start] if start < len(cov) else False:
                continue
            c = TASK_ARG.match(part)
            if c and is_code_token(c.group(1)):
                add('taskarg', start, c.group(1))


def collect():
    rels = []
    for d in ('game', 'input', 'render'):
        for f in glob.glob(os.path.join(ROOT, 'src', d, '**', '*.*'), recursive=True):
            rel = os.path.relpath(f, ROOT).replace('\\', '/')
            if in_scope(rel):
                rels.append(rel)
    srcs = [Source(r) for r in sorted(rels)]
    by_rel = {s.rel: s for s in srcs}
    # headers outside the scope a file includes (common/image_base.h, hook/*.h) add constants only
    for s in list(srcs):
        for m in re.finditer(r'#include\s+"([^"]+)"', s.raw):
            r = 'src/' + m.group(1)
            if r not in by_rel and os.path.exists(os.path.join(ROOT, r)) and r.endswith('.h'):
                by_rel[r] = Source(r)
    # image_base.h's kBase / kCodeLo / kCodeHi describe the image; their
    # names collide with files' own offsets (char_stats.cpp at::kBase)
    own_ints = {r: (va_ints(s) if r != 'src/common/image_base.h' else set()) for r, s in by_rel.items()}
    accessors = identity_accessors(srcs)
    aliases = fnptr_aliases(by_rel.values())
    sites = []
    for s in srcs:
        ints = set()
        for r in includes(s, by_rel):
            ints |= own_ints.get(r, set())
        scan(s, ints, accessors, aliases, sites)
    return sites, len(srcs), len(accessors)


def keyed(sites):
    c = collections.Counter((sh, f, tok, line) for sh, f, ln, tok, line in sites)
    return c


HEADER = ('# tools/reloc_bare.py: the bare image addresses in src/game, src/input, src/render (not the fuzz) - '
          'our own source lines, addresses and names only (docs/relocatable-image-round.md 5).\n'
          '#shape\tfile\ttoken\tcount\tline\n')


def write(path, sites):
    c = keyed(sites)
    with open(os.path.join(ROOT, path), 'w', encoding='utf-8', newline='\n') as o:
        o.write(HEADER)
        for (sh, f, tok, line), n in sorted(c.items()):
            o.write(f'{sh}\t{f}\t{tok}\t{n}\t{line}\n')


def read(path):
    c = collections.Counter()
    with open(os.path.join(ROOT, path), encoding='utf-8') as f:
        for row in f:
            if row.startswith('#') or not row.strip():
                continue
            sh, fl, tok, n, line = row.rstrip('\n').split('\t', 4)
            c[(sh, fl, tok, line)] += int(n)
    return c


def counts(sites, nfiles, nacc):
    by = collections.Counter(s[0] for s in sites)
    return (f'reloc_bare: {nfiles} files, {nacc} non-placing accessors found; sites: '
            + ', '.join(f'{sh} {by[sh]}' for sh in SHAPES) + f'; {len(sites)} in all')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', default=DEFAULT_OUT)
    ap.add_argument('--check', action='store_true', help='fail on a site the committed --out file does not list')
    ap.add_argument('--list', action='store_true', help='print every site, write nothing')
    a = ap.parse_args()
    sites, nfiles, nacc = collect()
    if a.list:
        for sh, f, ln, tok, line in sorted(sites, key=lambda s: (s[1], s[2], s[0])):
            print(f'{sh}\t{f}:{ln}\t{tok}\t{line}')
        print(counts(sites, nfiles, nacc))
        return 0
    if not a.check:
        write(a.out, sites)
        print(counts(sites, nfiles, nacc) + f'; written to {a.out}')
        return 0
    if not os.path.exists(os.path.join(ROOT, a.out)):
        print(f'reloc_bare --check: {a.out} is missing; run tools/reloc_bare.py')
        return 1
    have, now = read(a.out), keyed(sites)
    new = [(k, n - have.get(k, 0)) for k, n in now.items() if n > have.get(k, 0)]
    stale = [(k, have[k] - now.get(k, 0)) for k in have if now.get(k, 0) < have[k]]
    print(counts(sites, nfiles, nacc))
    if stale:
        print(f'reloc_bare --check: {len(stale)} listed key(s) fewer or gone (fine; rerun tools/reloc_bare.py to drop them)')
    if new:
        lines = {}
        for sh, f, ln, tok, line in sites:
            lines.setdefault((sh, f, tok, line), ln)
        print(f'reloc_bare --check: {sum(n for _, n in new)} new bare image address(es) - place each through '
              f'::bof3::Img (data), ::bof3::Code (code) or ::bof3::CodePlace (a run-time code word), '
              f'common/image_base.h; if it is a name, not a place (a coordinate, a Log argument, a VA a test '
              f'compares), rerun tools/reloc_bare.py and commit {a.out}:')
        for (sh, f, tok, line), n in sorted(new):
            print(f'  {sh:8} {f}:{lines[(sh, f, tok, line)]}  {tok}  (x{n})  {line[:140]}')
        return 1
    print(f'reloc_bare --check: no new bare image address; ok')
    return 0


if __name__ == '__main__':
    sys.exit(main())
