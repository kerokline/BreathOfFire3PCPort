#!/usr/bin/env python
"""The relocation census: every site in src/ that depends on where the game image is.

    python tools/reloc_census.py                 # write analysis/reloc_census.tsv, print counts
    python tools/reloc_census.py --out FILE      # write elsewhere
    python tools/reloc_census.py --full          # every site row, the fuzz's too (about 25 MB)
    python tools/reloc_census.py --counts        # print the counts only, write nothing
    python tools/reloc_census.py --check         # fail if a gated count rose above the committed file's
    python tools/reloc_census.py --ranges        # print the comparisons to read
    python tools/reloc_census.py --groups N [--groups-tsv FILE]
                                                 # propose N stage B file groups of about equal size

Group RA1 of the relocatable-image round (docs/relocatable-image-round.md
section 2). Reads src/, symbols.toml and src/hook/exe_layout.h only - no game
data - and writes addresses, names and our own source lines only. It reuses
tools/data_census.py's reader (comment stripping, the constant map, the at::
resolution, the include closure), so a constant means here what it means
there.

**The image** is symbols.toml's [meta]: image_base 0x400000 to image_base +
size_of_image (0x93F000), .text from text_va for text_size. Our executable's
draw pool (exe_layout.h kPoolVa, kPoolSize) is counted beside it.

**A site** is one token of our C++ that names an address in the image.

  token kinds
    accessor      a generated typed name (tools/gen_symbols.py): a [[data]]
                  entry's or typed [[block]]'s macro (`Party_Zenny`,
                  `Sprite_Objects[i].x`), `BOF3_GAME`. `detail` says `value`
                  (a scalar or an element read: the bytes, not the address)
                  or `address` (`&Name`, an array's name unsubscripted).
    accessor_addr `Name_addr`, `bof3::addr::Name` of a data entry, `kGameDataVa`
    image         gen_symbols' `bof3::image::kBase` / `kTextVa`
    at            a reference to an `at::` constant whose value is in the image
    module        a reference to any other k-constant whose value is in the
                  image, as the file sees it (its own or its includes'; never
                  data_census's src-wide fallback, see resolve_module)
    gen_func      `bof3::addr::F` / `bof3::orig::F` of a [[func]], the bare
                  macro of a function still Capcom's (no `impl`), or a macro
                  pasting a name after `addr::` (`BOF3_INJECT(F)`)
    literal       a hex literal of six digits or more in the image
  the value's kind (`kind`): base (the image base itself), header, code
  (.text), data (past .text), end (the image's end, exclusive), pool.

  classes (the `class` column), the first that applies:
    inject        `BOF3_INJECT(F)` or `Inject(name, F's address, ours)`: a
                  stub-page write (state3-design.md 2)
    asm_operand   the token inside an inline-asm operand `"m"(...)`, `"r"(...)`
    asm_text      a literal in an inline-asm template string (read from the
                  raw text: a naked function's `movl 0x660BD4(,%eax,4)`)
    abs_call      a code address called, or made callable: inside a
                  function-pointer cast (`reinterpret_cast<void (__cdecl*)()>(k)`),
                  the address argument of a call-through helper (find_callers:
                  `SH_AT(Fn, 0x4410B0)(1)`, `Call0(0x5A99AD)`, `Phase(a)`), a
                  Capcom-function macro, a `bof3::orig::F(...)` call. `detail`
                  names the way (`called`, `fnptr`, `via=Name`, `theirs`, `orig`).
                  In the exe host each lands on the stub page's jmp.
    range_cmp     an operand of < <= > >= (also a `range` row)
    addr_eq       an operand of == != or a `case` label (also a `range` row)
    ptr_sub       the right operand of binary - or -= : an image address
                  subtracted. `detail`: `two_constants` (both sides name the
                  image: a size, unchanged by a move) or `runtime_left`.
    const_def     the definition of a k-constant whose value is in the image;
                  `detail` says `literal` (its right side spells an image
                  literal: an edit point) or `derived` (from other names only:
                  it follows them). The tokens inside a definition are not
                  sites of their own.
    value_literal a literal that is an operand of & | ^ * / % << >> : a mask or
                  a scale, not an address. Counted, not a site.
    base_offset   a literal added to the base by name (`image::kBase + 0x504058`,
                  any name whose value is 0x400000) or inside a BASE_HELPERS
                  call: stage B's converted spelling. Counted, not a site.
    otherwise the token kind names the class: accessor, accessor_addr,
    image_meta (image), at_global (at), module_const (module), code_const
    (at/module/gen_func of kind code), raw_literal (a literal of any other
    kind), code_literal (a literal of kind code).

  ctx: the innermost enclosing construct - `accessor` (a memory accessor of
  data_census.ACCESSORS), `cast`, `call` (another call's argument), `init` (a
  brace initialiser), `subscript`, `template`, `static_assert`, `macro`, `-`.
  how: for a module constant `file` or `include` (where it was found); for a
  literal a hint for the reader, never a decision - `symbol` (a symbols.toml
  start: likely an address) or `aligned_8000` (on a 0x8000 boundary and no
  start: likely a value; every world position and height in the samples read
  on 2026-10-28 was one).

**Scope** (the rule that counts sites apart):
  fuzz      the file name matches data_census's rule `_fuzz\\.|_harness|fuzz_`:
            the shadow fuzz and its harnesses, which run only under
            BOF3X_SHADOW, refused by the exe host (state3-design.md 6.1), so
            only ever with BOF3.exe at its fixed base.
  hosted    a site in any other file that only the hosted build executes:
            inside a CloneOriginal(...) call (the clone self-tests, under
            BOF3X_SHADOW only), inside a StubFor / StubAt body (a clone's
            callee table, Capcom's callee address to its recording
            stand-in), or a code address inside PatchBytes(...) /
            RetargetCall(...) (a patch of Capcom's code, dead in the exe host,
            state3-design.md 7).
  launcher  src/launcher/: another process; it reads BOF3.exe as a file and
            never the game's memory, so the image's place does not reach it.
  platform  src/hook, src/render, src/audio, src/input, src/common: the hosts
            and the platform under the game - the loader's own territory
            (stage C).
  game      src/game, the rest.

The TSV (write_tsv) holds `site`, `range`, `helper` and `file` rows, then
`#count` and `#gate` lines. `--check` reads the committed file's `#gate` lines
and fails when a gated count (GATED: the literal, call and reading classes,
game and platform scopes) is higher today: the number may only fall. Stage B
lowers them; a new literal address, absolute call or range comparison in the
engine is refused until the census is regenerated on purpose.
"""
import argparse, collections, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import data_census as dc  # noqa: E402

ROOT = dc.ROOT
DEFAULT_OUT = os.path.join('analysis', 'reloc_census.tsv')

FUZZ_RULE = re.compile(r'_fuzz\.|_harness|fuzz_')
PLATFORM_DIRS = ('src/hook/', 'src/render/', 'src/audio/', 'src/input/', 'src/common/')

# Names of the base and of a helper that adds it. A literal right of `+` on a
# name whose value is the image base is the converted spelling already; RB0
# adds its helper's name here if RA3's choice keeps addresses behind one.
BASE_HELPERS = frozenset()
# gen_symbols.py's `namespace bof3::image` names that are addresses
IMAGE_META = ('kBase', 'kTextVa')

# The classes --check gates (non-fuzz, non-launcher): each a spelling of the
# image's address that stage B removes, or a reading that needs a person.
GATED = ('asm_operand', 'asm_text', 'abs_call', 'range_cmp', 'addr_eq', 'ptr_sub',
         'const_def_literal', 'raw_literal', 'code_literal')

HEX = re.compile(r"(?<![\w.'])0[xX][0-9A-Fa-f][0-9A-Fa-f']*[uUlL]*\b")
IDENT = re.compile(r'(?<![\w.])((?:[A-Za-z_]\w*\s*::\s*)*)([A-Za-z_]\w*)\b')
ASM = re.compile(r'(?<![\w.])(?:__asm__|__asm|asm)\s*(?:(?:volatile|__volatile__|goto|inline)\s*)*\(')
CAST_TAIL = re.compile(r'(?:static_cast|reinterpret_cast|const_cast)\s*<[^;{}<>]*(?:<[^;{}<>]*>[^;{}<>]*)*>\s*\(\s*$')
FP_TYPE = re.compile(r'\(\s*(?:__\w+\s*)?\*\s*\)')
KEYWORDS = frozenset('''if while for switch return sizeof alignof decltype case static_assert
    static_cast reinterpret_cast const_cast dynamic_cast'''.split())

FAMILY_PREFIX = [
    ('magic', ('magic',)),
    ('effect', ('effect',)),
    ('area', ('area',)),
    ('battle', ('battle', 'boss', 'enemy')),
    ('scenario', ('scena',)),
    ('field', ('field', 'map', 'move', 'mode', 'event', 'sprite', 'sprt', 'world', 'worldmap',
               'object', 'task', 'frame', 'kind2')),
    ('rest', ('rest',)),
    ('ui', ('menu', 'window', 'text', 'title', 'save', 'shop', 'yes', 'msg', 'msgbox', 'name',
            'config', 'pause', 'quick', 'glyph', 'labels', 'language', 'lang', 'list', 'item',
            'inventory', 'fishing', 'cheats', 'button', 'band', 'shoulder', 'char', 'member')),
]


def scope_of(rel):
    if FUZZ_RULE.search(rel):
        return 'fuzz'
    if rel.startswith('src/launcher/'):
        return 'launcher'
    if rel.startswith(PLATFORM_DIRS):
        return 'platform'
    return 'game'


def family_of(rel):
    if not rel.startswith('src/game/'):
        return rel.split('/')[1]
    stem = os.path.basename(rel).split('.')[0]
    head = re.match(r'[a-z0-9]+', stem)
    head = head.group(0) if head else stem
    for fam, prefixes in FAMILY_PREFIX:
        if head in prefixes:
            return fam
    return 'engine'


# --------------------------------------------------------------------------
# The image and the generated names
# --------------------------------------------------------------------------

class Image:
    def __init__(self, syms, layout_path):
        self.syms = syms
        self.lo, self.hi = syms.lo, syms.hi
        text = open(layout_path, encoding='utf-8').read()
        vals = {m.group(1): int(m.group(2), 16) for m in
                re.finditer(r'\b(k\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;', text)}
        self.pool_lo = vals['kPoolVa']
        self.pool_hi = vals['kPoolVa'] + vals['kPoolSize']

    def kind(self, v):
        if v is None:
            return None
        if v == self.lo:
            return 'base'
        if self.lo < v < self.syms.text_lo:
            return 'header'
        if self.syms.in_text(v):
            return 'code'
        if self.lo <= v < self.hi:
            return 'data'
        if v == self.hi:
            return 'end'
        if self.pool_lo <= v < self.pool_hi:
            return 'pool'
        return None


def generated_names(path):
    """The names tools/gen_symbols.py emits, by what they are."""
    import tomllib
    with open(path, 'rb') as fh:
        t = tomllib.load(fh)
    data = t.get('data', [])
    blocks = t.get('block', [])
    funcs = t.get('func', [])
    typed = {e['name']: e['pc'] for e in data + blocks if 'ctype' in e}
    arrays = ({e['name'] for e in data if 'ctype' in e and 'count' in e} |
              {e['name'] for e in blocks if 'ctype' in e and e.get('record_count', 1) != 1})
    addr_names = {e['name'] + '_addr': e['pc'] for e in data + blocks if 'ctype' in e}
    data_pcs = {e['name']: e['pc'] for e in data + blocks}
    func_pcs = {e['name']: e['pc'] for e in funcs}
    theirs = {e['name']: e['pc'] for e in funcs if 'ret' in e and 'impl' not in e}
    return typed, arrays, addr_names, data_pcs, func_pcs, theirs


# --------------------------------------------------------------------------
# Context
# --------------------------------------------------------------------------

def enclosing(code, pos, limit=4000):
    """(bracket, text before it) of the innermost bracket around pos."""
    depth = {')': 0, ']': 0, '}': 0}
    lo = max(0, pos - limit)
    k = pos - 1
    while k >= lo:
        c = code[k]
        if c in ')]}':
            depth[c] += 1
        elif c in '([{':
            close = {'(': ')', '[': ']', '{': '}'}[c]
            if depth[close]:
                depth[close] -= 1
            else:
                return c, code[max(0, k - 200):k]
        elif c == ';' and not any(depth.values()):
            return None, ''
        k -= 1
    return None, ''


def ctx_of(f_acc, code, pos):
    br, before = enclosing(code, pos)
    if br == '(':
        if re.search(r'(?:static_cast|reinterpret_cast|const_cast)\s*<[^;{}]*>\s*$', before):
            return 'cast'
        m = re.search(r'([A-Za-z_]\w*)\s*$', before)
        if m:
            name = m.group(1)
            if name == 'static_assert':
                return 'static_assert'
            if name in BASE_HELPERS:
                return 'base_helper'
            if name in KEYWORDS:
                return '-'
            if f_acc(name):
                return 'accessor'
            return 'call'
        return '-'
    if br == '{':
        if re.search(r'(=|\]|[A-Za-z_]\w*|,|\{)\s*$', before) and not re.search(r'\)\s*(const\s*)?$', before):
            return 'init'
        return '-'
    if br == '[':
        return 'subscript'
    return '-'


def strip_left(code, s):
    """The text before s, less trailing whitespace, plain or cast parentheses
    and a unary & or *; whether a call's parenthesis stopped it; and how many
    parentheses were stripped (the right side may close only those)."""
    t = code[max(0, s - 300):s]
    nopen = 0
    while True:
        t2 = t.rstrip()
        m = CAST_TAIL.search(t2)
        if m:
            t = t2[:m.start()]
            nopen += 1
            continue
        if t2.endswith('('):
            pm = re.search(r'([A-Za-z_]\w*|[>\]])\s*\($', t2)
            if pm and pm.group(1) not in KEYWORDS:
                return t2, True, nopen
            t = t2[:-1]
            nopen += 1
            continue
        um = re.search(r'([&*])\s*$', t2)
        if um:
            before = t2[:um.start()].rstrip()
            if not before or before[-1] in '=(,<>!|&?:+-*/%^~{[':
                t = before
                continue
        return t2, False, nopen


def _balanced(t, i, open_c, close_c):
    """The index past the bracket that closes t[i] (== open_c), or -1."""
    depth = 0
    for k in range(i, len(t)):
        c = t[k]
        if c == open_c:
            depth += 1
        elif c == close_c:
            depth -= 1
            if depth == 0:
                return k + 1
        elif c in ';{}':
            return -1
    return -1


def skip_term(t):
    """The length of one operand at the start of t (after `+`, `-` or `*`):
    a number, a parenthesised expression, or a name - with template
    arguments when a call or `::` follows (`static_cast<U>(e)`,
    `rv::As<records::T>(p)`) - and its postfixes `(...)`, `[...]`, `->f`,
    `.f`; 0 if t does not start with one."""
    m = re.match(r"\s*(?:0[xX][0-9A-Fa-f']+|\d+)[uUlL]*", t)
    if m:
        return m.end()
    m = re.match(r'\s*', t)
    i = m.end()
    if t[i:i + 1] == '(':
        j = _balanced(t, i, '(', ')')
        return j if j > 0 else 0
    m = re.match(r'(?:[A-Za-z_]\w*\s*::\s*)*[A-Za-z_]\w*', t[i:])
    if not m:
        return 0
    i += m.end()
    if t[i:i + 1] == '<':
        j = _balanced(t, i, '<', '>')
        if j > 0 and re.match(r'\s*(\(|::)', t[j:]):
            i = j
    while True:
        m = re.match(r'\s*', t[i:])
        k = i + m.end()
        c = t[k:k + 1]
        if c == '(':
            j = _balanced(t, k, '(', ')')
        elif c == '[':
            j = _balanced(t, k, '[', ']')
        elif t[k:k + 2] == '->' or (c == '.' and re.match(r'\.[A-Za-z_]', t[k:k + 2])):
            n = re.match(r'(?:->|\.)\s*[A-Za-z_]\w*', t[k:])
            j = k + n.end() if n else -1
        else:
            return i
        if j < 0:
            return i
        i = j


def right_op(code, e, nopen):
    """The operator right of the token at e, past an offset term or two and
    the nopen parentheses the left side opened (a call's own parenthesis
    ends the search: `B(kX) != 0` compares the value read, not kX)."""
    t = code[e:e + 300]
    m = re.match(r"[uUlL']*", t)
    t = t[m.end():]
    while True:
        m = re.match(r'\s*\)', t)
        if m:
            if nopen <= 0:
                return None, t
            nopen -= 1
            t = t[m.end():]
            continue
        m = re.match(r'\s*[+\-*](?![+\-=>])', t)
        if m:
            n = skip_term(t[m.end():])
            if n:
                t = t[m.end() + n:]
                continue
        break
    m = re.match(r'\s*(<=|>=|==|!=|<<|>>|<|>|&&|&|\|\||\||\^|\*|/|%|-|\+)', t)
    if not m:
        return None, t
    op = m.group(1)
    rest = t[m.end():]
    if op == '>' and not re.match(r'\s|[\w&*\-]', rest[:1] or ' '):
        return 'tmpl>', rest
    if op == '-' and rest[:1] == '>':
        return None, t
    return op, rest


def left_op(code, s):
    """(operator, text before, parentheses opened) left of the token at s."""
    t, called, nopen = strip_left(code, s)
    if called:
        return None, t, 0
    if re.search(r'\bcase\s*$', t):
        return 'case', t, nopen
    m = re.search(r'(<=|>=|==|!=|<<=|>>=|<<|>>|->|-=|--|\+=|\|=|&=|&&|\|\||<|>|-|&|\||\^|\*|/|%|\+|=)\s*$', t)
    if not m:
        return None, t, nopen
    op = m.group(1)
    if op in ('->', '--', '<<=', '>>=', '+=', '|=', '&=', '=', '&&', '||', '+'):
        return op, t, nopen
    before = t[:m.start()].rstrip()
    if op in ('-', '&', '*', '<', '>', '|', '^', '/', '%', '<<', '>>'):
        # binary only: an operand before it
        if not before or not re.search(r'[\w)\]]$', before):
            return None, t, nopen
    if op == '<' and re.search(r'\b(?:template|static_cast|reinterpret_cast|const_cast)\s*$', before):
        return None, t, nopen
    return op, t, nopen


CMP_OPS = ('<', '<=', '>', '>=')
EQ_OPS = ('==', '!=', 'case')
ARITH_OPS = ('&', '|', '^', '*', '/', '%', '<<', '>>')


# --------------------------------------------------------------------------
# The scan
# --------------------------------------------------------------------------

def fp_cast_spans(code, fp_alias):
    """(arg_start, arg_end, called) of every cast to a function pointer:
    `reinterpret_cast<void (__cdecl*)()>(x)`, or to an alias of one."""
    out = []
    for m in re.finditer(r'\breinterpret_cast\s*<', code):
        depth, k = 1, m.end()
        while k < len(code) and depth:
            c = code[k]
            if c == '<':
                depth += 1
            elif c == '>':
                depth -= 1
            elif c in ';{}':
                break
            k += 1
        targ = code[m.end():k - 1]
        if not (FP_TYPE.search(targ) or targ.strip().rstrip('*').strip() in fp_alias):
            continue
        pm = re.match(r'\s*\(', code[k:])
        if not pm:
            continue
        ostart = k + pm.end() - 1
        r = dc.balanced_args(code, ostart)
        if r is None:
            continue
        out.append((ostart, r[1], code[r[1]:r[1] + 3].lstrip().startswith('(')))
    return out


INT_PARAM = re.compile(r'^(?:const\s+)?(?:std::)?(?:uint32_t|uintptr_t|U|unsigned(?:\s+(?:int|long))?|'
                       r'unsigned\s+long|int|long)\s+(\w+)$')


def find_callers(files, fp_alias, by_rel):
    """The call-through helpers: a macro whose argument is cast to its type
    argument (`#define SH_AT(type, address) ...reinterpret_cast<type>(...address...)`)
    -> {name: index}; and a function whose integral parameter reaches a
    function-pointer cast or another helper's address argument
    (`void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }`, `Phase`)
    -> {name: [(file, index)]}, to a fixed point. A code address passed there
    is called at that address (through the stub page in the exe host)."""
    macros = {}
    for f in files:
        for m in re.finditer(r'#[ \t]*define[ \t]+(\w+)\(([^)]*)\)((?:[^\n]*\\\n)*[^\n]*)', f.raw):
            params = [p.strip() for p in m.group(2).split(',')]
            body = m.group(3)
            for ti, t in enumerate(params):
                if not t or not re.search(r'reinterpret_cast\s*<\s*' + re.escape(t) + r'\s*>', body):
                    continue
                for ai, a in enumerate(params):
                    if ai != ti and a and re.search(
                            r'reinterpret_cast\s*<\s*' + re.escape(t) + r'\s*>\s*\([^;]*\b' + re.escape(a) + r'\b', body):
                        macros[m.group(1)] = ai
    funcs = collections.defaultdict(list)
    defs = []
    head = re.compile(r'(?:^|[;}\n])[ \t]*(?:(?:static|inline|constexpr|extern\s+"C")\s+)*'
                      r'[A-Za-z_][\w:<>]*(?:\s*[*&])?\s+(?:__\w+\s+)?(\w+)\s*\(([^(){};]*)\)\s*(?:const\s*)?\{')
    for f in files:
        for m in head.finditer(f.code):
            params = [p.strip() for p in m.group(2).split(',')] if m.group(2).strip() else []
            ints = {}
            for i, p in enumerate(params):
                pm = INT_PARAM.match(' '.join(p.split('=')[0].split()))
                if pm:
                    ints[pm.group(1)] = i
            if not ints:
                continue
            depth, j = 0, m.end() - 1
            while j < len(f.code) and j < m.end() + 3000:
                if f.code[j] == '{':
                    depth += 1
                elif f.code[j] == '}':
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            if depth:
                continue
            defs.append((f, m.group(1), ints, m.end() - 1, j + 1))
    for _ in range(4):
        changed = False
        for f, name, ints, a, b in defs:
            body = f.code[a:b]
            spans = [(s, e) for s, e, _ in fp_cast_spans(body, fp_alias)]
            known = list(macros) + [n for n in funcs]
            if known:
                for m in re.finditer(r'(?<![\w.>:])(' + '|'.join(map(re.escape, known)) + r')\s*\(', body):
                    r = dc.balanced_args(body, m.end() - 1)
                    if r is None:
                        continue
                    idxs = {macros[m.group(1)]} if m.group(1) in macros else {i for _, i in funcs[m.group(1)]}
                    pos = m.end()
                    for i, arg in enumerate(r[0]):
                        if i in idxs:
                            spans.append((pos, pos + len(arg)))
                        pos += len(arg) + 1
            for pname, i in ints.items():
                if (f.rel, i) in funcs.get(name, []):
                    continue
                if any(re.search(r'\b' + re.escape(pname) + r'\b', body[s:e]) for s, e in spans):
                    funcs[name].append((f.rel, i))
                    changed = True
        if not changed:
            break
    return macros, dict(funcs)


def scan(toml='symbols.toml', layout='src/hook/exe_layout.h'):
    syms = dc.Symbols(toml)
    img = Image(syms, layout)
    typed, arrays, addr_names, data_pcs, func_pcs, theirs = generated_names(toml)
    files = dc.load_sources()
    by_rel = {f.rel: f for f in files}
    env = dc.collect_constants(files, {**syms.names, **syms.addr_names})
    defs_by_name = collections.defaultdict(list)
    for f in files:
        for name, value, ns, line in f.consts:
            defs_by_name[name].append((f, value, ns, line))
    acc_defs = {f.rel: dc.accessor_defs(f) for f in files}

    def is_accessor(f, name):
        if name not in dc.ACCESSORS:
            return False
        if name in acc_defs[f.rel]:
            return acc_defs[f.rel][name]
        for g in dc.include_closure(f, by_rel):
            if name in acc_defs[g.rel]:
                return acc_defs[g.rel][name]
        return name not in dc.CAST_BY_DEFAULT

    # function-pointer aliases: `using XFn = void (__cdecl*)(int);`, typedefs
    fp_alias = set()
    for f in files:
        for m in re.finditer(r'\busing\s+(\w+)\s*=\s*([^;]*);', f.code):
            if FP_TYPE.search(m.group(2)):
                fp_alias.add(m.group(1))
        for m in re.finditer(r'\btypedef\b[^;]*\(\s*(?:__\w+\s*)?\*\s*(\w+)\s*\)\s*\(', f.code):
            fp_alias.add(m.group(1))

    # function-like macros that paste a parameter after `addr::` or `orig::`
    # (`BOF3_INJECT(name)` -> `::bof3::addr::name`): name -> {index: kind}
    addr_macros = {}
    for f in files:
        for m in re.finditer(r'#[ \t]*define[ \t]+(\w+)\(([^)]*)\)((?:[^\n]*\\\n)*[^\n]*)', f.raw):
            params = [p.strip() for p in m.group(2).split(',')]
            hits = {}
            for i, p in enumerate(params):
                if p and re.search(r'\b(?:addr|orig)\s*::\s*' + re.escape(p) + r'\b', m.group(3)):
                    hits[i] = 'orig' if re.search(r'\borig\s*::\s*' + re.escape(p) + r'\b', m.group(3)) else 'addr'
            if hits:
                addr_macros[m.group(1)] = hits

    call_macros, call_funcs = find_callers(files, fp_alias, by_rel)

    sites = []
    extra = collections.Counter()

    for f in files:
        code, raw, rel = f.code, f.raw, f.rel
        scope, fam = scope_of(rel), family_of(rel)

        def resolve_module(name):
            """(value, how) of a module constant as f sees it: its own, else
            its includes'. Not data_census's src-wide fallback (the one value
            src/ gives the name): outside an accessor that fallback reads an
            enumerator or a local table as another module's address (measured
            2026-10-28: 1,286 such references; a sample of 35 rows read by
            hand held no real one), so a name f cannot see is `srcwide` and
            not a site."""
            if f.locals.get(name) is not None:
                return f.locals[name], 'file'
            for g in dc.include_closure(f, by_rel):
                if g.locals.get(name) is not None:
                    return g.locals[name], 'include'
            return None, ('srcwide' if env.get(name) is not None else '-')

        def base_name(spelled):
            """Whether a name spelled left of `+` is the image base."""
            q = spelled.split('::')
            if len(q) > 1 and q[-2] == 'image' and q[-1] == 'kBase':
                return True
            return resolve_module(q[-1])[0] == img.lo

        # spans: constant definitions, asm statements and operands, fp casts
        defs = []   # (start, end, name, value, scope_at, rhs)
        for m in dc.CONST_DECL.finditer(code):
            for name, rhs in dc.split_decls(m.group(1)):
                defs.append((m.start(), m.end(), name, rhs, f.in_at_namespace(m.start())))
        for m in dc.CONST_BRACE.finditer(code):
            defs.append((m.start(), m.end(), m.group(1), m.group(2), f.in_at_namespace(m.start())))
        def_spans = sorted({(a, b) for a, b, *_ in defs})

        asm_spans, asm_ops = [], []
        for m in ASM.finditer(code):
            depth, k = 0, m.end() - 1
            while k < len(code):
                c = code[k]
                if c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            asm_spans.append((m.start(), k + 1))
            seg = code[m.end():k]
            for om in re.finditer(r'"[^"\n]*"\s*\(', seg):
                ostart = m.end() + om.end() - 1
                d2, j = 0, ostart
                while j < k:
                    c = code[j]
                    if c == '(':
                        d2 += 1
                    elif c == ')':
                        d2 -= 1
                        if d2 == 0:
                            break
                    j += 1
                cons = raw[m.end() + om.start() + 1:m.end() + om.end() - 1].split('"')[0]
                asm_ops.append((ostart, j, cons.strip()))
                extra['asm_operands_all'] += 1

        fp_spans = fp_cast_spans(code, fp_alias)
        # a call-through helper's address argument: `SH_AT(Fn, 0x4410B0)(1)`,
        # `Call0(0x5A99AD)` (find_callers)
        via_spans = []   # (start, end, via)
        names = [n for n in call_macros] + [n for n, defs in call_funcs.items()
                                            if any(d == rel or by_rel.get(d) in dc.include_closure(f, by_rel)
                                                   for d, _ in defs)]
        if names:
            for m in re.finditer(r'(?<![\w.>:])(?:\w+::)*(' + '|'.join(map(re.escape, names)) + r')\s*\(', code):
                if re.match(r'[ \t]*#[ \t]*define\b', f.lines[dc.line_of(f.starts, m.start()) - 1]):
                    continue
                r = dc.balanced_args(code, m.end() - 1)
                if r is None:
                    continue
                name = m.group(1)
                idxs = {call_macros[name]} if name in call_macros else \
                    {i for d, i in call_funcs[name] if d == rel or by_rel.get(d) in dc.include_closure(f, by_rel)}
                pos = m.end()
                for i, arg in enumerate(r[0]):
                    if i in idxs:
                        via_spans.append((pos, pos + len(arg), name))
                    pos += len(arg) + 1

        # hosted-only arguments: a clone of Capcom's code (CloneOriginal, the
        # shadow fuzz's, refused by the exe host) and a patch or call re-aim
        # into .text (PatchBytes, RetargetCall: dead in the exe host,
        # state3-design.md 7)
        hosted_spans = []   # (start, end, api)
        for m in re.finditer(r'(?<![\w.>])(?:\w+::)*(CloneOriginal|PatchBytes|RetargetCall)\s*\(', code):
            r = dc.balanced_args(code, m.end() - 1)
            if r is not None and not re.match(r'\s*(?:const\s+)?[\w:]+[\s*&]+\w+\s*$', r[0][0] if r[0] else ''):
                hosted_spans.append((m.end() - 1, r[1], m.group(1)))
        # and a clone's callee table: `StubFor(target)` / `StubAt(target)`,
        # Capcom's callee address to the recording stand-in a clone calls
        for m in re.finditer(r'\b(StubFor|StubAt)\s*\([^(){};]*\)\s*\{', code):
            depth, j = 0, m.end() - 1
            while j < len(code):
                if code[j] == '{':
                    depth += 1
                elif code[j] == '}':
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            hosted_spans.append((m.start(), j + 1, 'StubFor'))

        inject_spans = []
        for m in re.finditer(r'(?<![\w.>])(?:\w+::)*Inject\s*\(', code):
            r = dc.balanced_args(code, m.end() - 1)
            if r is not None:
                inject_spans.append((m.end() - 1, r[1]))

        def inside(spans, pos):
            for a, b, *rest in spans:
                if a <= pos < b:
                    return (a, b, *rest)
            return None

        line_text = f.lines
        raw_lines = raw.split('\n')

        def expr_at(ln):
            return ' '.join(line_text[ln - 1].split())[:160]

        def add(pos, end, klass, detail, token, value, kind, ctx, how='-', raw_expr=False):
            site_scope = scope
            if scope in ('game', 'platform'):
                h = inside(hosted_spans, pos)
                if h and (h[2] in ('CloneOriginal', 'StubFor') or kind == 'code'):
                    site_scope = 'hosted'
            ln = dc.line_of(f.starts, pos)
            cov = '-'
            if value is not None and kind in ('data', 'header'):
                c, dname, _ = syms.cover(value)
                cov = dname if c != 'none' else '-'
            elif value is not None and kind == 'code':
                hit = syms.by_pc.get(value)
                cov = hit[1]['name'] if hit else '-'
            sites.append({
                'file': rel, 'line': ln, 'class': klass, 'detail': detail, 'token': token,
                'value': f'{value:#x}' if value is not None else '-', 'kind': kind or '-',
                'scope': site_scope, 'family': fam, 'ctx': ctx, 'symbol': cov, 'how': how,
                'expr': ' '.join(raw_lines[ln - 1].split())[:160] if raw_expr else expr_at(ln),
            })

        # constant definitions with an image value
        for a, b, name, rhs, in_at in defs:
            v = f.locals.get(name)
            if v is None:
                v = dc.eval_expr(rhs, {**env, **f.locals})
            kind = img.kind(v)
            if kind is None:
                continue
            lit = any(img.kind(int(h.group(0).rstrip('uUlL').replace("'", ''), 16)) for h in HEX.finditer(rhs))
            add(a, b, 'const_def', 'literal' if lit else 'derived', ('at::' if in_at else '') + name,
                v, kind, 'at' if in_at else 'module')

        # a macro that pastes a name after addr:: / orig:: (BOF3_INJECT): one
        # site per use, the function's (or entry's) address; BOF3_INJECT's
        # are the stub page's writes (class inject)
        for m in re.finditer(r'(?<![\w.>:])(' + '|'.join(map(re.escape, addr_macros)) + r')\s*\(', code) \
                if addr_macros else ():
            if re.match(r'[ \t]*#[ \t]*define\b', line_text[dc.line_of(f.starts, m.start()) - 1]):
                continue
            r = dc.balanced_args(code, m.end() - 1)
            if r is None:
                continue
            for i, how in addr_macros[m.group(1)].items():
                if i >= len(r[0]):
                    continue
                arg = r[0][i].strip()
                v = func_pcs.get(arg, data_pcs.get(arg))
                kind = img.kind(v)
                if not kind:
                    continue
                if m.group(1) == 'BOF3_INJECT':
                    add(m.start(), r[1], 'inject', '-', arg, v, kind, 'macro')
                elif kind == 'code':
                    add(m.start(), r[1], 'code_const', f'macro={m.group(1)}', arg, v, kind, 'macro', how)
                else:
                    add(m.start(), r[1], 'accessor_addr', f'macro={m.group(1)}', arg, v, kind, 'macro', how)

        # tokens: hex literals and names
        tokens = []
        for m in HEX.finditer(code):
            v = int(m.group(0).rstrip('uUlL').replace("'", ''), 16)
            kind = img.kind(v)
            if kind:
                # a hint for the reader, never a decision: a literal that is a
                # symbols.toml start is likely an address (`symbol`); one on a
                # 0x8000 boundary that is not is likely a value - every world
                # position and height read in the 2026-10-28 samples was one
                # (`aligned_8000`)
                hint = 'symbol' if v in syms.by_pc else 'aligned_8000' if v % 0x8000 == 0 else '-'
                tokens.append((m.start(), m.end(), 'literal', m.group(0), v, kind, hint))
        for m in IDENT.finditer(code):
            prefix = re.sub(r'\s+', '', m.group(1))
            name = m.group(2)
            s, e = m.start(), m.end()
            if code[max(0, s - 2):s] == '->':
                continue
            quals = [q for q in prefix.split('::') if q]
            last_q = quals[-1] if quals else ''
            if last_q == 'addr' or (not prefix and name in addr_names) or name in ('kGameDataVa', 'BOF3_GAME'):
                # bof3::addr::Name / Name_addr / kGameDataVa
                # BOF3_GAME and kGameDataVa: the first page past .text
                # (gen_symbols.py's GameData), named, not valued here
                if name == 'BOF3_GAME':
                    tokens.append((s, e, 'accessor', name, None, 'data', '-'))
                    continue
                if name == 'kGameDataVa':
                    tokens.append((s, e, 'accessor_addr', prefix + name, None, 'data', '-'))
                    continue
                if name in addr_names and not prefix:
                    v = addr_names[name]
                    tokens.append((s, e, 'accessor_addr', name, v, img.kind(v), '-'))
                    continue
                if last_q == 'addr':
                    if name in data_pcs:
                        v = data_pcs[name]
                        tokens.append((s, e, 'accessor_addr', prefix + name, v, img.kind(v), '-'))
                    elif name in func_pcs:
                        v = func_pcs[name]
                        tokens.append((s, e, 'gen_func', prefix + name, v, img.kind(v), '-'))
                    continue
            if last_q == 'image' and name in IMAGE_META:
                # gen_symbols' bof3::image::kBase / kTextVa: the base named
                v = {'kBase': img.lo, 'kTextVa': syms.text_lo}[name]
                tokens.append((s, e, 'image', prefix + name, v, img.kind(v), '-'))
                continue
            if last_q == 'orig' and name in func_pcs:
                v = func_pcs[name]
                tokens.append((s, e, 'gen_func', prefix + name, v, img.kind(v), 'orig'))
                continue
            if not prefix and name in typed:
                if code[max(0, s - 1):s] in ('.',):
                    continue
                v = typed[name]
                # the address itself only as &Name or an array's name not
                # subscripted (a pointer to element 0); else the value
                amp = re.search(r'(?:^|[^&\w)\]])&\s*$', code[max(0, s - 40):s])
                after = code[e:e + 3].lstrip()
                addr = amp or (name in arrays and not after.startswith(('[', '->', '.')))
                tokens.append((s, e, 'accessor', name, v, img.kind(v), 'address' if addr else 'value'))
                continue
            if not prefix and name in theirs:
                v = theirs[name]
                tokens.append((s, e, 'gen_func', name, v, img.kind(v), 'theirs'))
                continue
            if not re.match(r'k[A-Z0-9_]', name):
                continue
            if prefix and dc.is_at_prefix(prefix):
                at_pre = prefix[:-len('at::')]
                v, cands = dc.resolve_at(f, at_pre, name, s, by_rel, defs_by_name)
                if v is None:
                    # a typed alias, a struct constant, or ambiguous between
                    # modules: ambiguous with an image candidate is listed
                    if cands and any(img.kind(c[1]) for c in cands if c[1] not in (None, 'REF')):
                        tokens.append((s, e, 'at', prefix + name, None, 'ambiguous', 'ambiguous'))
                    continue
                kind = img.kind(v)
                if kind:
                    tokens.append((s, e, 'at', prefix + name, v, kind, '-'))
                continue
            # a module constant: not an array or a function
            if re.match(r'\s*[\[(]', code[e:e + 2]):
                continue
            v, how = resolve_module(name)
            if how == 'srcwide' and img.kind(env.get(name)):
                extra['module_srcwide_not_counted'] += 1
            kind = img.kind(v)
            if kind:
                tokens.append((s, e, 'module', prefix + name, v, kind, how))
        tokens.sort()

        # ends of the tokens that are constants (a typed value read is not)
        img_ends = {t[1] for t in tokens if not (t[2] == 'accessor' and t[6] == 'value')}

        for s, e, tk, text, v, kind, how in tokens:
            if inside(def_spans, s):
                continue
            ctx = ctx_of(lambda n: is_accessor(f, n), code, s)
            # asm
            op = inside(asm_ops, s)
            if op:
                add(s, e, 'asm_operand', op[2], text, v, kind, ctx, how)
                continue
            if inside(asm_spans, s):
                add(s, e, 'asm_operand', 'statement', text, v, kind, ctx, how)
                continue
            if tk == 'gen_func':
                called = code[e:e + 3].lstrip().startswith('(')
                if how == 'theirs' or (how == 'orig' and called):
                    add(s, e, 'abs_call', how, text, v, kind, ctx)
                    continue
            if kind == 'code' and inside(inject_spans, s):
                # Inject(name, address, ours) spelled out (BOF3_INJECT's body)
                add(s, e, 'inject', 'direct', text, v, kind, ctx, how)
                continue
            fp = inside(fp_spans, s)
            if fp and kind == 'code':
                add(s, e, 'abs_call', 'called' if fp[2] else 'fnptr', text, v, kind, ctx, how)
                continue
            via = inside(via_spans, s)
            if via and kind == 'code':
                add(s, e, 'abs_call', f'via={via[2]}', text, v, kind, ctx, how)
                continue
            if via and not fp:
                fp = via   # a data token in a call through a word it reads
            if tk == 'accessor' and how == 'value':
                # a typed scalar or an element read: the value at the
                # address, not the address (`Game_Mode == 2`)
                add(s, e, 'accessor', 'value', text, v, kind, ctx)
                continue
            lop, ltext, nopen = left_op(code, s)
            rop, _ = right_op(code, e, nopen)
            if rop == 'tmpl>' or (lop == '<' and rop in ('tmpl>', ',')):
                ctx = 'template'
                lop = None if lop == '<' else lop
                rop = None
            if tk == 'literal' and (lop == '+' or ctx == 'base_helper'):
                lm = re.search(r'((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)\s*\+\s*$', ltext)
                if ctx == 'base_helper' or (lm and base_name(lm.group(1))):
                    add(s, e, 'base_offset', '-', text, v, kind, ctx)
                    continue
            if lop in CMP_OPS or rop in CMP_OPS:
                add(s, e, 'range_cmp', lop if lop in CMP_OPS else rop, text, v, kind, ctx, how)
                continue
            if lop in EQ_OPS or rop in ('==', '!='):
                add(s, e, 'addr_eq', lop if lop in EQ_OPS else rop, text, v, kind, ctx, how)
                continue
            if lop in ('-', '-='):
                # the left operand: an image token too (kEnd - kStart, a size
                # that moves with nothing) or not (an offset into the image)
                lm = re.search(r'(\w+)[uUlL]*\s*\)*\s*-=?\s*(?:\(\s*)*$', code[max(0, s - 300):s])
                both = 'runtime_left'
                if lm and max(0, s - 300) + lm.end(1) in img_ends:
                    both = 'two_constants'
                add(s, e, 'ptr_sub', both, text, v, kind, ctx, how)
                continue
            if tk == 'literal' and (lop in ARITH_OPS or rop in ARITH_OPS):
                add(s, e, 'value_literal', lop if lop in ARITH_OPS else rop, text, v, kind, ctx)
                continue
            if fp:
                # a data token inside a call through a word read from .data
                detail = 'call_through'
            else:
                detail = '-'
            if tk == 'accessor':
                add(s, e, 'accessor', detail, text, v, kind, ctx)
            elif tk == 'image':
                add(s, e, 'image_meta', detail, text, v, kind, ctx)
            elif tk == 'accessor_addr':
                add(s, e, 'accessor_addr', detail, text, v, kind, ctx)
            elif kind == 'code':
                add(s, e, 'code_literal' if tk == 'literal' else 'code_const', detail if detail != '-' else tk,
                    text, v, kind, ctx, how)
            elif tk == 'at':
                add(s, e, 'at_global', detail, text, v, kind, ctx, how)
            elif tk == 'module':
                add(s, e, 'module_const', detail, text, v, kind, ctx, how)
            elif tk == 'gen_func':
                add(s, e, 'code_const', how, text, v, kind, ctx)
            else:
                add(s, e, 'raw_literal', detail, text, v, kind, ctx, how)

        # literals in asm template strings (blanked in code; read from raw)
        for a, b in asm_spans:
            for sm in re.finditer(r'"((?:[^"\\\n]|\\.)*)"', raw[a:b]):
                for h in HEX.finditer(sm.group(1)):
                    v = int(h.group(0).rstrip('uUlL').replace("'", ''), 16)
                    kind = img.kind(v)
                    if kind:
                        add(a + sm.start(), a + sm.end(), 'asm_text', '-', h.group(0), v, kind, 'asm',
                            raw_expr=True)

    sites.sort(key=lambda r: (r['file'], r['line'], r['class'], r['token']))

    # range helpers: a small function whose body holds a range comparison;
    # its call sites are readings too
    helpers = {}
    for r in sites:
        if r['class'] != 'range_cmp':
            continue
        f = by_rel[r['file']]
        # the comparison's own line or one of the five above it opens a
        # `bool Name(` function, with no function's end between
        for k in range(r['line'] - 1, max(-1, r['line'] - 7), -1):
            hm = re.match(r'\s*(?:static\s+|inline\s+|constexpr\s+)*bool\s+(\w+)\s*\(', f.lines[k])
            if hm:
                helpers.setdefault((r['file'], hm.group(1)), r)
                break
            if f.lines[k].startswith('}'):
                break
    helper_calls = []
    for (hfile, hname), r in sorted(helpers.items()):
        hf = by_rel[hfile]
        pool = [hf] + [g for g in files if hf in dc.include_closure(g, by_rel)] if hfile.endswith('.h') else [hf]
        for g in pool:
            for m in re.finditer(r'(?<![\w.>:])' + re.escape(hname) + r'\s*\(', g.code):
                ln = dc.line_of(g.starts, m.start())
                if g.rel == hfile and ln == r['line']:
                    continue
                if re.match(r'\s*(?:static\s+|inline\s+|constexpr\s+)*bool\s+' + re.escape(hname), g.lines[ln - 1]):
                    continue
                helper_calls.append({'file': g.rel, 'line': ln, 'helper': hname, 'defined': f'{hfile}:{r["line"]}',
                                     'scope': scope_of(g.rel), 'expr': ' '.join(g.lines[ln - 1].split())[:160]})
    return sites, helper_calls, extra


# --------------------------------------------------------------------------
# Counts, output, check
# --------------------------------------------------------------------------

SITE_CLASSES = ('accessor', 'accessor_addr', 'at_global', 'module_const', 'code_const',
                'raw_literal', 'code_literal', 'asm_operand', 'asm_text', 'abs_call',
                'range_cmp', 'addr_eq', 'ptr_sub', 'const_def', 'inject', 'image_meta')
SCOPES = ('game', 'platform', 'hosted', 'fuzz', 'launcher')
# What a stage B file group edits: every class but the generated names
# (RB0's, through tools/gen_symbols.py: accessor, accessor_addr, inject,
# image_meta) and the readings (range_cmp, addr_eq: the coordinator's).
EDIT_CLASSES = ('at_global', 'module_const', 'code_const', 'raw_literal', 'code_literal', 'asm_operand',
                'asm_text', 'abs_call', 'ptr_sub', 'const_def')


def gate_key(r):
    if r['class'] == 'const_def':
        return 'const_def_literal' if r['detail'] == 'literal' else 'const_def_derived'
    return r['class']


def counts_of(sites, helper_calls, extra):
    c = collections.OrderedDict()
    real = [r for r in sites if r['class'] in SITE_CLASSES]
    c['sites'] = len(real)
    for sc in SCOPES:
        c[f'sites_{sc}'] = sum(1 for r in real if r['scope'] == sc)
    for k in SITE_CLASSES:
        c[f'class_{k}'] = sum(1 for r in real if r['class'] == k)
        for sc in SCOPES:
            c[f'class_{k}_{sc}'] = sum(1 for r in real if r['class'] == k and r['scope'] == sc)
    c['const_def_literal'] = sum(1 for r in sites if r['class'] == 'const_def' and r['detail'] == 'literal')
    c['const_def_derived'] = sum(1 for r in sites if r['class'] == 'const_def' and r['detail'] == 'derived')
    c['value_literal'] = sum(1 for r in sites if r['class'] == 'value_literal')
    c['base_offset'] = sum(1 for r in sites if r['class'] == 'base_offset')
    c['module_srcwide_not_counted'] = extra['module_srcwide_not_counted']
    c['asm_operands_all'] = extra['asm_operands_all']
    c['at_ambiguous'] = sum(1 for r in real if r['kind'] == 'ambiguous')
    for k in ('raw_literal', 'code_literal'):
        for cx in ('accessor', 'cast', 'call', 'init', 'subscript', 'template', 'static_assert', '-'):
            c[f'{k}_ctx_{cx}'] = sum(1 for r in real if r['class'] == k and r['ctx'] == cx and r['scope'] != 'fuzz')
    c['literal_with_symbol_nonfuzz'] = sum(1 for r in real if r['class'] in ('raw_literal', 'code_literal')
                                           and r['symbol'] != '-' and r['scope'] != 'fuzz')
    c['abs_call_theirs'] = sum(1 for r in real if r['class'] == 'abs_call' and r['detail'] == 'theirs')
    c['call_through_data_word'] = sum(1 for r in real if r['detail'] == 'call_through')
    c['constexpr_context_refs_nonfuzz'] = sum(
        1 for r in real if r['ctx'] in ('template', 'static_assert') or r['detail'] == 'case'
        if r['scope'] != 'fuzz')
    c['ptr_sub_two_constants'] = sum(1 for r in real if r['class'] == 'ptr_sub' and r['detail'] == 'two_constants')
    c['range_helper_calls'] = len(helper_calls)
    c['files_with_sites'] = len({r['file'] for r in real})
    c['files_with_sites_nonfuzz'] = len({r['file'] for r in real if r['scope'] in ('game', 'platform')})
    for k in GATED:
        c[f'gate_{k}'] = sum(1 for r in sites if gate_key(r) == k and r['scope'] in ('game', 'platform'))
    return c


SITE_HDR = ['rec', 'file', 'line', 'class', 'detail', 'token', 'value', 'kind', 'scope', 'family',
            'ctx', 'symbol', 'how', 'expr']
RANGE_HDR = ['rec', 'file', 'line', 'class', 'op', 'token', 'value', 'kind', 'scope', 'expr']
HELPER_HDR = ['rec', 'file', 'line', 'helper', 'defined', 'scope', 'expr']
FILE_HDR = ['rec', 'file', 'scope', 'family', 'class', 'n']
COMPACT_LEFT_OUT = ('accessor', 'accessor_addr', 'inject')


def write_tsv(path, sites, helper_calls, counts, full=False):
    """Five tables, each under a `#` header line: `site` rows, `range` rows
    (the comparisons to read, every scope), `helper` rows (calls of a range
    helper), `file` rows (per file, scope and class, the count of sites), then
    `#count` and `#gate` lines. Without --full the site rows leave out the
    fuzz scope and the generated names (COMPACT_LEFT_OUT): RB0 converts those
    in tools/gen_symbols.py, the fuzz keeps the fixed base, and the `file`
    rows still count them - the committed file stays a few MB, not 25."""
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    clean = lambda s: str(s).replace('\t', ' ')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('# tools/reloc_census.py: every site in src/ that names the game image\'s address. '
                'Addresses and names only (docs/relocatable-image-round.md 2, RA1).'
                + ('' if full else ' Site rows: game, platform, hosted and launcher scopes, '
                   'less ' + ', '.join(COMPACT_LEFT_OUT) + ' (counted in the file rows).') + '\n')
        f.write('#' + '\t'.join(SITE_HDR) + '\n')
        for r in sites:
            if not full and (r['scope'] == 'fuzz' or r['class'] in COMPACT_LEFT_OUT):
                continue
            f.write('site\t' + '\t'.join(clean(r[k]) for k in SITE_HDR[1:]) + '\n')
        f.write('#' + '\t'.join(RANGE_HDR) + '\n')
        for r in sites:
            if r['class'] in ('range_cmp', 'addr_eq'):
                f.write('range\t' + '\t'.join(clean(x) for x in (
                    r['file'], r['line'], r['class'], r['detail'], r['token'], r['value'], r['kind'],
                    r['scope'], r['expr'])) + '\n')
        f.write('#' + '\t'.join(HELPER_HDR) + '\n')
        for h in helper_calls:
            f.write('helper\t' + '\t'.join(clean(h[k]) for k in HELPER_HDR[1:]) + '\n')
        f.write('#' + '\t'.join(FILE_HDR) + '\n')
        per = collections.Counter((r['file'], r['scope'], r['family'], gate_key(r)) for r in sites)
        for (fl, sc, fam, k), n in sorted(per.items()):
            f.write(f'file\t{fl}\t{sc}\t{fam}\t{k}\t{n}\n')
        for k, v in counts.items():
            f.write(f'#count {k}\t{v}\n')
        for k in GATED:
            f.write(f'#gate {k}\t{counts["gate_" + k]}\n')


def committed_gates(path):
    out = {}
    with open(path, encoding='utf-8') as f:
        for line in f:
            m = re.match(r'#gate (\w+)\t(\d+)', line)
            if m:
                out[m.group(1)] = int(m.group(2))
    if not out:
        sys.exit(f'reloc_census: no "#gate" lines in {path}')
    return out


def propose_groups(sites, n):
    """Stage B's file groups: the game and platform sites a group edits
    (EDIT_CLASSES), by family, packed into n groups of about equal size. A
    family larger than the target is split by stem in name order, a .cpp
    and its headers (`x.cpp`, `x.h`, `x_callees.h`) always together."""
    per_file = collections.Counter()
    fam_of = {}
    for r in sites:
        if r['scope'] in ('game', 'platform') and r['class'] in EDIT_CLASSES:
            per_file[r['file']] += 1
            fam_of[r['file']] = r['family'] if r['scope'] == 'game' else 'platform'
    total = sum(per_file.values())
    target = total / n
    by_fam = collections.defaultdict(lambda: collections.defaultdict(list))
    for fl, k in sorted(per_file.items()):
        stem = re.sub(r'(_callees)?\.\w+$', '', fl)
        by_fam[fam_of[fl]][stem].append((fl, k))
    units = []
    for fam, stems in sorted(by_fam.items()):
        size = sum(k for fls in stems.values() for _, k in fls)
        if size <= target * 1.15:
            units.append((size, fam, [x for fls in stems.values() for x in fls]))
            continue
        cur, part = [], 1
        for stem, fls in sorted(stems.items()):
            k = sum(x for _, x in fls)
            if cur and sum(x for _, x in cur) + k > target:
                units.append((sum(x for _, x in cur), f'{fam}{part}', cur))
                cur, part = [], part + 1
            cur.extend(fls)
        if cur:
            units.append((sum(x for _, x in cur), f'{fam}{part}', cur))
    groups = [[0, [], []] for _ in range(n)]
    for size, name, fls in sorted(units, key=lambda u: -u[0]):
        g = min(groups, key=lambda g: g[0])
        g[0] += size
        g[1].append(name)
        g[2].extend(fls)
    return total, groups


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', default=DEFAULT_OUT)
    ap.add_argument('--toml', default='symbols.toml')
    ap.add_argument('--counts', action='store_true', help='print the counts, write nothing')
    ap.add_argument('--full', action='store_true',
                    help='write every site row (the fuzz and the generated names too; about 25 MB)')
    ap.add_argument('--check', action='store_true',
                    help='fail if a gated count is above the committed --out file\'s')
    ap.add_argument('--ranges', action='store_true', help='print the range comparisons and helper calls')
    ap.add_argument('--groups', type=int, default=0, help='propose N stage B file groups')
    ap.add_argument('--groups-tsv', help='with --groups: write each file\'s group here')
    a = ap.parse_args()
    os.chdir(ROOT)
    sites, helper_calls, extra = scan(a.toml)
    counts = counts_of(sites, helper_calls, extra)
    if a.check:
        was = committed_gates(a.out)
        bad = [(k, was.get(k, 0), counts['gate_' + k]) for k in GATED if counts['gate_' + k] > was.get(k, 0)]
        if bad:
            for k, w, now in bad:
                print(f'reloc_census: {k}: {now} in src/game and the platform, above the committed {w}')
            sys.exit('reloc_census: a new spelling of the image\'s address (docs/relocatable-image-round.md). '
                     'Name it through the base, or regenerate the census if the rise is deliberate.')
        print('reloc_census: ' + ', '.join(f'{k} {counts["gate_" + k]} (committed {was.get(k, 0)})'
                                          for k in GATED) + '; ok')
        return
    if a.ranges:
        for r in sites:
            if r['class'] in ('range_cmp', 'addr_eq'):
                print(f"{r['scope']}\t{r['file']}:{r['line']}\t{r['class']}\t{r['detail']}\t{r['token']}\t{r['expr']}")
        for h in helper_calls:
            print(f"{h['scope']}\t{h['file']}:{h['line']}\thelper\t{h['helper']}\t{h['defined']}\t{h['expr']}")
        return
    if a.groups:
        total, groups = propose_groups(sites, a.groups)
        print(f'{total} edit sites in src/game and the platform, {a.groups} groups, target {total / a.groups:.0f}')
        by_file = collections.defaultdict(collections.Counter)
        for r in sites:
            if r['scope'] in ('game', 'platform'):
                by_file[r['file']][gate_key(r)] += 1
        rows = []
        for i, (size, names, fls) in enumerate(groups, 1):
            mix = collections.Counter()
            for fl, _ in fls:
                mix.update(by_file[fl])
            print(f'group {i}: {size} sites, {len(fls)} files: {" ".join(sorted(names))}')
            print('    ' + ', '.join(f'{k} {mix[k]}' for k in EDIT_CLASSES + ('const_def_literal', 'const_def_derived', 'range_cmp', 'addr_eq')
                                    if mix[k]))
            rows.extend((i, fl, k) for fl, k in sorted(fls))
        if a.groups_tsv:
            with open(a.groups_tsv, 'w', encoding='utf-8', newline='\n') as fh:
                fh.write('#group\tfile\tedit_sites\n')
                for i, fl, k in rows:
                    fh.write(f'{i}\t{fl}\t{k}\n')
            print(f'wrote {a.groups_tsv}: {len(rows)} files')
        return
    for k, v in counts.items():
        print(f'{k}\t{v}')
    if not a.counts:
        write_tsv(a.out, sites, helper_calls, counts, a.full)
        print(f'wrote {a.out}: {len(sites)} rows')


if __name__ == '__main__':
    main()
