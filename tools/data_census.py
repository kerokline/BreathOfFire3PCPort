#!/usr/bin/env python
"""The typed-data census: every raw data access in src/, and every `at::k` constant.

    python tools/data_census.py                      # write analysis/data_census.tsv, print counts
    python tools/data_census.py --out FILE           # write elsewhere
    python tools/data_census.py --counts             # print the counts only, write nothing
    python tools/data_census.py --check              # fail if the site count rose above the committed file's

Group CS of the typed-data round (docs/typed-data-round.md section 2; the
counts and the method in docs/typed-data-census.md). Reads src/ and
symbols.toml only - no game data - and writes addresses and names only.

A **site** is one raw access in our C++, in one of three shapes:

  global    a reference to a constant of an `at` namespace (`at::kX`,
            `mh::at::kX`, or `kX` inside the namespace) whose value is an
            address inside the image - `B(at::kX)`, `At(at::kX + 4)`,
            `Pointer(at::kTable)`. One site per reference, whatever wraps it.
  record    a memory accessor (ACCESSORS below, bare or namespace-qualified)
            whose address argument is a local pointer or address plus or
            minus an offset - `Word(p + 0x1C)`, `Byte(s + n)`, `SetW(r - 2, v)`
            - or a symbols.toml data name plus an offset (`Sprite_Current +
            0x38`). The family is looked up from the local's last assignment
            in the same function: the helper that made it (`via=Sc`), the
            `at::` table or symbols.toml name it came from (`table=`, `base=`),
            the stride (`* 0xA4`, `* kXStride`, `p += 0x84`), or `param`.
  raw       a memory accessor whose address argument starts with a literal
            inside the image - `B(0x904002)`.

A bare local with no offset (`Word(p)`) is not counted: in many files the
same names are value casts (`Byte(v)`, `S16(x)`), so the bare form cannot be
told apart by syntax. Per file, an accessor name the file itself (or a local
header it includes) defines as a cast - a body with no memory access - is not
an accessor there at all.

The **constant map** is every constant defined in a namespace named `at`:
its value, every module (file and namespace) defining that name with that
value, whether the value lies in the image, and the symbols.toml entry at
that address (data or func) or `-`.

Coverage of a global site: `exact` when a [[data]] entry has that pc,
`inside` when it lies within an entry's extent (sizeof ctype * count),
`none` otherwise; the entry's name and ctype beside it.

The output TSV holds two tables, each under a `#` header line: `site` rows,
then `const` rows, then `#count` lines. `--check` reads the committed file's
`#count sites` line and fails when today's measured sites exceed it, so the
number may only fall (docs/typed-data-census.md section 3).
"""
import argparse, ast, collections, os, re, sys, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_OUT = os.path.join('analysis', 'data_census.tsv')
EXTS = ('.cpp', '.h', '.hpp', '.cc', '.inl', '.ixx')

# Memory accessor names: read, write, address-to-pointer. The census's
# vocabulary, measured 2026-10-19 by `grep -oE "\b\w+\(at::k"` over src/ (the
# names wrapping an at:: constant at least 40 times), less the generic ones
# (Put, Get, M, P, Entry, Text, Jump, Vertex) whose meaning differs by file.
ACCESSORS = frozenset('''
    B W D L Byte Word Dword Long S8 S16 S32 U8 U16 U32 UL
    SetB SetW SetD SetL SetByte SetWord SetDword SetLong SetUL
    Mem At Ptr PtrAt SetPtr Pointer SetPointer
    PutByte PutWord PutLong GetByte GetWord GetLong
'''.split())
# Names that are value casts unless the file says otherwise.
CAST_BY_DEFAULT = frozenset('S8 S16 S32 U8 U16 U32'.split())

CTYPE_SIZE = {
    'unsigned long': 4, 'long': 4, 'int': 4, 'unsigned int': 4, 'float': 4,
    'std::uint32_t': 4, 'unsigned char': 1, 'signed char': 1, 'char': 1,
    'unsigned short': 2, 'short': 2, 'const unsigned short': 2,
}


# --------------------------------------------------------------------------
# Source reading
# --------------------------------------------------------------------------

def strip_code(text):
    """Blank comments and string / char literals, keeping every newline and
    every column, so line numbers and brace depth survive."""
    out = list(text)
    i, n = 0, len(text)

    def blank(a, b):
        for k in range(a, b):
            if out[k] != '\n':
                out[k] = ' '

    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            j = n if j < 0 else j
            blank(i, j)
            i = j
        elif c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            blank(i, j)
            i = j
        elif c == 'R' and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == '_')):
            m = re.match(r'R"([^(\s]{0,16})\(', text[i:])
            if not m:
                i += 1
                continue
            end = text.find(')' + m.group(1) + '"', i)
            j = n if end < 0 else end + len(m.group(1)) + 2
            blank(i + 1, j)
            i = j
        elif c in '"\'':
            if c == "'":
                # a digit separator (0x80'0000) is not a char literal
                k = i - 1
                while k >= 0 and (text[k].isalnum() or text[k] in "_'"):
                    k -= 1
                if k + 1 < i and text[k + 1].isdigit():
                    i += 1
                    continue
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            blank(i + 1, min(j, n))
            i = j + 1
        else:
            i += 1
    return ''.join(out)


def line_starts(text):
    starts = [0]
    for m in re.finditer('\n', text):
        starts.append(m.end())
    return starts


def line_of(starts, pos):
    lo, hi = 0, len(starts) - 1
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if starts[mid] <= pos:
            lo = mid
        else:
            hi = mid - 1
    return lo + 1


def balanced_args(text, open_pos):
    """The top-level comma-separated arguments of the call whose '(' is at
    open_pos, and the position after its ')'; None if unbalanced."""
    depth, args, start = 0, [], open_pos + 1
    i, n = open_pos, len(text)
    while i < n:
        c = text[i]
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                args.append(text[start:i])
                return args, i + 1
        elif c == ',' and depth == 1:
            args.append(text[start:i])
            start = i + 1
        elif c == ';' and depth <= 1:
            return None
        i += 1
    return None


# --------------------------------------------------------------------------
# Constants
# --------------------------------------------------------------------------

NUM = re.compile(r'\b(0[xX][0-9A-Fa-f]+|\d+)[uUlL]*\b')
IDENT_RE = re.compile(r'[A-Za-z_]\w*')


def eval_expr(expr, env):
    """A constant expression of literals, names in env, + - * << >> | & ~ ( );
    None when anything else is in it."""
    s = expr.strip().replace("'", '')
    s = re.sub(r'static_cast<[^<>]*>', '', s)
    s = re.sub(r'\b(std::)?u?int(8|16|32|64)_t\s*\{([^{}]*)\}', r'(\3)', s)
    s = re.sub(r'(?<![\w:])::', '', s)           # ::bof3::addr::X
    s = re.sub(r'(?<!/)/(?!/)', '//', s)        # C's integer division

    def name(m):
        w = m.group(0)
        if re.fullmatch(r'0[xX][0-9A-Fa-f]+|\d+', w):
            return w
        key = w.split('::')[-1]
        if key in env and env[key] is not None:
            return str(env[key])
        raise KeyError(w)
    s = NUM.sub(lambda m: str(int(m.group(1), 0)), s)
    try:
        s = re.sub(r'(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*', name, s)
        tree = ast.parse(s, mode='eval')
    except (KeyError, SyntaxError, ValueError):
        return None
    ok = (ast.Expression, ast.BinOp, ast.UnaryOp, ast.Constant, ast.Add, ast.Sub,
          ast.Mult, ast.LShift, ast.RShift, ast.BitOr, ast.BitAnd, ast.BitXor,
          ast.Invert, ast.USub, ast.UAdd, ast.FloorDiv)
    for node in ast.walk(tree):
        if not isinstance(node, ok):
            return None
        if isinstance(node, ast.Constant) and not isinstance(node.value, int):
            return None
    try:
        return int(eval(compile(tree, '<c>', 'eval'), {'__builtins__': {}})) & 0xFFFFFFFF
    except Exception:
        return None


CONST_DECL = re.compile(
    r'\b(?:inline\s+)?(?:static\s+)?(?:constexpr|const)\s+'
    r'(?:std::)?(?:u?int(?:8|16|32|64)_t|unsigned(?:\s+(?:int|long|short|char))?|int|long|'
    r'short|char|signed\s+char|U|auto|size_t|std::size_t)\s+'
    r'(k\w*\s*=[^;{}]*);')


def split_decls(body):
    """'kA = 1, kB = kA + 2' -> [(kA, '1'), (kB, 'kA + 2')], top-level commas."""
    parts, depth, cur = [], 0, ''
    for c in body:
        if c in '([{<':
            depth += 1
        elif c in ')]}>':
            depth -= 1
        if c == ',' and depth == 0:
            parts.append(cur)
            cur = ''
        else:
            cur += c
    parts.append(cur)
    out = []
    for p in parts:
        m = re.match(r'\s*(k\w*)\s*=(.*)$', p, re.S)
        if m:
            out.append((m.group(1), m.group(2).strip()))
    return out


class SourceFile:
    def __init__(self, rel, text):
        self.rel = rel
        self.raw = text
        self.code = strip_code(text)
        self.starts = line_starts(self.code)
        self.lines = self.code.split('\n')
        self.includes = [m.group(1) for m in re.finditer(r'#\s*include\s*"([^"]+)"', text)]
        # namespace spans: (start, end, path) for every namespace block
        self.ns_spans = self._namespaces()
        self.aliases = {m.group(1): m.group(2).strip() for m in re.finditer(
            r'\bnamespace\s+(\w+)\s*=\s*([\w:]+)\s*;', self.code)}
        self.consts = []          # (name, value_or_None, ns_path, line, start, end)
        self.locals = {}          # every k-constant by name -> value (any namespace)

    def _namespaces(self):
        spans, stack = [], []
        code = self.code
        tok = re.compile(r'\bnamespace\s+([\w:]+)\s*\{|\bnamespace\s*\{|[{}]')
        for m in tok.finditer(code):
            s = m.group(0)
            if s == '{':
                stack.append(None)
            elif s == '}':
                if stack:
                    top = stack.pop()
                    if top is not None:
                        spans.append((top[0], m.end(), top[1]))
            else:
                name = m.group(1) or '(anon)'
                outer = [t[1] for t in stack if t is not None]
                path = '::'.join(([outer[-1]] if outer else []) + [name])
                stack.append((m.start(), path))
        return spans

    def ns_at(self, pos):
        best = None
        for a, b, p in self.ns_spans:
            if a <= pos < b and (best is None or a > best[0]):
                best = (a, b, p)
        return best[2] if best else ''

    def in_at_namespace(self, pos):
        for a, b, p in self.ns_spans:
            if a <= pos < b and p.split('::')[-1] == 'at':
                return True
        return False


def load_sources():
    files = []
    src = os.path.join(ROOT, 'src')
    for dirpath, dirnames, filenames in os.walk(src):
        dirnames.sort()
        for fn in sorted(filenames):
            if fn.endswith(EXTS):
                p = os.path.join(dirpath, fn)
                rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
                with open(p, encoding='utf-8', errors='replace') as f:
                    files.append(SourceFile(rel, f.read()))
    files.sort(key=lambda f: f.rel)
    return files


CONST_BRACE = re.compile(r'\b(?:inline\s+)?(?:static\s+)?constexpr\s+[\w:]+\s+(k\w*)\s*\{([^{},;]*)\}\s*;')


def collect_constants(files, sym_env):
    """Every k-constant in every file; values resolved in file order, a few
    passes so a header's constants serve the files that include it. sym_env
    holds symbols.toml's names, for `bof3::addr::Name` on the right."""
    by_rel = {f.rel: f for f in files}
    decls = []
    for f in files:
        for m in CONST_DECL.finditer(f.code):
            for name, rhs in split_decls(m.group(1)):
                decls.append((f, name, rhs, m.start()))
        for m in CONST_BRACE.finditer(f.code):
            decls.append((f, m.group(1), m.group(2), m.start()))
    decls.sort(key=lambda d: (d[0].rel, d[3]))
    globals_env = {}
    for _ in range(4):
        for f in files:
            f.locals = {}
        for f, name, rhs, pos in decls:
            env = dict(sym_env)
            env.update(globals_env)
            for inc in include_closure(f, by_rel):
                env.update(inc.locals)
            env.update(f.locals)
            v = eval_expr(rhs, env)
            f.locals[name] = v
        new = {}
        for f, name, rhs, pos in decls:
            v = f.locals.get(name)
            if v is not None:
                new.setdefault(name, set()).add(v)
        globals_env = {k: next(iter(v)) for k, v in new.items() if len(v) == 1}
    for f, name, rhs, pos in decls:
        if f.in_at_namespace(pos):
            ns = f.ns_at(pos)
            f.consts.append((name, f.locals.get(name), ns, line_of(f.starts, pos)))
    return globals_env


_closure_cache = {}


def include_closure(f, by_rel):
    if f.rel in _closure_cache:
        return _closure_cache[f.rel]
    seen, order, todo = {f.rel}, [], [f]
    while todo:
        cur = todo.pop()
        base = os.path.dirname(cur.rel)
        for inc in cur.includes:
            cand = [os.path.normpath(os.path.join(base, inc)).replace(os.sep, '/'),
                    'src/' + inc, 'src/game/' + inc]
            for c in cand:
                if c in by_rel and c not in seen:
                    seen.add(c)
                    order.append(by_rel[c])
                    todo.append(by_rel[c])
                    break
    _closure_cache[f.rel] = order
    return order


# --------------------------------------------------------------------------
# symbols.toml
# --------------------------------------------------------------------------

class Symbols:
    def __init__(self, path):
        with open(path, 'rb') as fh:
            t = tomllib.load(fh)
        meta = t['meta']
        self.lo = meta['image_base']
        self.hi = meta['image_base'] + meta['size_of_image']
        self.text_lo = meta['text_va']
        self.text_hi = meta['text_va'] + meta['text_size']
        self.data = sorted(t.get('data', []), key=lambda e: e['pc'])
        self.by_pc = {e['pc']: ('data', e) for e in self.data}
        self.names = {e['name']: e['pc'] for e in self.data}
        self.names.update({e['name']: e['pc'] for e in t.get('func', [])})
        # [[block]]: a named region (a PSX block's PC image) with no ctype
        for e in t.get('block', []):
            self.by_pc.setdefault(e['pc'], ('block', e))
            self.names.setdefault(e['name'], e['pc'])
        for e in t.get('func', []):
            self.by_pc.setdefault(e['pc'], ('func', e))
        self.extents = []
        for e in self.data:
            size = CTYPE_SIZE.get(e.get('ctype', ''), 4 if '*' in e.get('ctype', '') else 0)
            if size:
                self.extents.append((e['pc'], e['pc'] + size * max(1, e.get('count', 1)), e))
        self.extents.sort(key=lambda x: x[0])
        self.n_data = len(self.data)
        self.n_ctype = sum(1 for e in self.data if 'ctype' in e)
        self.n_count = sum(1 for e in self.data if 'count' in e)

    def in_image(self, v):
        return v is not None and self.lo <= v < self.hi

    def in_text(self, v):
        return v is not None and self.text_lo <= v < self.text_hi

    def in_data(self, v):
        """In the image and outside .text: .rdata, .data and the rest."""
        return self.in_image(v) and not self.in_text(v)

    def kind(self, v):
        return 'data' if self.in_data(v) else 'code' if self.in_text(v) else 'value'

    def name_at(self, v):
        hit = self.by_pc.get(v)
        if hit:
            return hit[1]['name'], hit[0]
        return '-', '-'

    def cover(self, v):
        """(cover, name, ctype) of the [[data]] entry covering address v."""
        hit = self.by_pc.get(v)
        if hit and hit[0] == 'data':
            return 'exact', hit[1]['name'], hit[1].get('ctype', '-')
        best = None
        for a, b, e in self.extents:
            if a > v:
                break
            if a <= v < b and (best is None or a > best['pc']):
                best = e
        if best:
            return 'inside', f"{best['name']}+{v - best['pc']:#x}", best.get('ctype', '-')
        return 'none', '-', '-'


# --------------------------------------------------------------------------
# Accessor definitions, per file
# --------------------------------------------------------------------------

MEM_BODY = re.compile(r'\b(At|Mem|memcpy|memset|Word|Long|Byte|Dword|B|W|D|L|Read\w*|Get\w*|Ptr|Pointer)\s*\(|'
                      r'reinterpret_cast|\[|->|\*\s*\(|\*\s*[A-Za-z_]\w*\s*[;)]')


def accessor_defs(f):
    """name -> True (memory) / False (cast) for the accessors f defines."""
    out = {}
    pat = re.compile(r'(?:^|[;}\n])\s*(?:static\s+|inline\s+|constexpr\s+|\[\[[^\]]*\]\]\s*)*'
                     r'[A-Za-z_][\w:<>]*(?:\s+[A-Za-z_][\w:<>]*)?[\s\*&]+(' +
                     '|'.join(sorted(ACCESSORS)) + r')\s*\(([^()]*)\)\s*(?:const\s*)?(?:noexcept\s*)?\{')
    for m in pat.finditer(f.code):
        name = m.group(1)
        r = balanced_args(f.code, m.end() - 1)
        if r is None:
            continue
        body = f.code[m.end() - 1:r[1]]
        mem = bool(MEM_BODY.search(body[1:-1]))
        out[name] = out.get(name, False) or mem
    return out


# --------------------------------------------------------------------------
# The scan
# --------------------------------------------------------------------------

AT_REF = re.compile(r'(?<![\w:])((?:\w+::)*)at::(k\w+)\b')
LOCAL_OFF = re.compile(r'^\s*(?:\(\s*)?([a-z_]\w*(?:(?:\.|->)\w+)*)\s*([+-])\s*(.+?)\s*$', re.S)
GLOBAL_OFF = re.compile(r'^\s*(?:\(\s*)?(?:::)?(?:bof3::)?([A-Z]\w*)\s*[+-]\s*(.+?)\s*$', re.S)
LIT = re.compile(r'^\s*\(?\s*(0[xX][0-9A-Fa-f]{6,8})[uUlL]*\b')
STRIDE_NAME = re.compile(r'Stride')


def resolve_at(f, prefix, name, pos, by_rel, defs_by_name):
    """The value of `[prefix::]at::name` as seen from f at pos, and the
    candidate definitions it was picked from."""
    cands = defs_by_name.get(name, [])
    if not cands:
        return None, []
    want_ns = None
    if prefix:
        p = prefix.rstrip(':').split('::')[-1]
        p = f.aliases.get(p, p)
        want_ns = p.split('::')[-1] + '::at'
    elif 'at' in f.aliases:
        want_ns = '::'.join(f.aliases['at'].split('::')[-2:])
    tiers = []
    if want_ns:
        tiers.append([c for c in cands if c[2].endswith(want_ns) or c[2] == want_ns])
    tiers.append([c for c in cands if c[0] is f])
    stem = re.sub(r'(_fuzz)?\.\w+$', '', os.path.basename(f.rel))
    tiers.append([c for c in cands if re.sub(r'(_callees|_fuzz)?\.\w+$', '', os.path.basename(c[0].rel)) == stem])
    closure = {g.rel for g in include_closure(f, by_rel)}
    tiers.append([c for c in cands if c[0].rel in closure])
    enc = f.ns_at(pos).split('::')[0]
    tiers.append([c for c in cands if c[2].split('::')[0] == enc])
    tiers.append(cands)
    for t in tiers:
        vals = {c[1] for c in t if c[1] is not None}
        if len(vals) == 1:
            return vals.pop(), t
        if len(vals) > 1:
            return None, t
    return None, cands


def function_start(f, line):
    """The first line of the function containing `line`: back to the nearest
    line that starts at column 0 with '}' (the previous function's end) or
    that opens a namespace, at most 400 lines."""
    raw_lines = f.lines
    lo = max(0, line - 400)
    for k in range(line - 2, lo - 1, -1):
        s = raw_lines[k]
        if s.startswith('}') or re.match(r'\s*namespace\b', s):
            return k + 1
    return lo


def stride_family(f, ident, line, env, at_value, syms):
    """What the local was made from, from its last assignment in the function:
    'table=0x...' (an at:: table) or 'base=Name' (a symbols.toml name), and
    'stride=0x..' (the multiplier); 'param' for an argument, '-' unknown."""
    base = ident.split('.')[0].split('->')[0]
    lines = f.lines
    start = function_start(f, line)
    assign = re.compile(r'(?<![\w.])' + re.escape(base) + r'\s*(?:=(?!=)|\{|\()\s*(.*)')
    for k in range(line - 1, start - 1, -1):
        s = lines[k]
        m = assign.search(s)
        if not m:
            continue
        before = s[:m.start()]
        decl_ok = re.search(r'[\w*&>]\s*$', before) or before.strip() == '' or before.rstrip().endswith((',', '('))
        if not decl_ok:
            continue
        rhs = m.group(1)
        j = k + 1
        while ';' not in rhs and j < len(lines) and j < k + 6:
            rhs += ' ' + lines[j]
            j += 1
        rhs = rhs.split(';')[0]
        stride = None
        sm = re.search(r'\*\s*\(?\s*((?:\w+::)*\w+|0[xX][0-9A-Fa-f]+|\d+)', rhs) or \
            re.search(r'((?:\w+::)*k\w*Stride\w*|0[xX][0-9A-Fa-f]+|\d+)\s*\*', rhs)
        if sm:
            tok = sm.group(1)
            if re.fullmatch(r'0[xX][0-9A-Fa-f]+|\d+', tok):
                stride = int(tok, 0)
            else:
                key = tok.split('::')[-1]
                stride = f.locals.get(key) if f.locals.get(key) is not None else env.get(key)
        tm = AT_REF.search(rhs)
        table = at_value(tm, k) if tm else None
        parts = []
        if table is not None and syms.in_data(table):
            parts.append(f'table={table:#x}')
        else:
            # a symbols.toml data name on the right: `= Sprite_Current`
            for w in re.findall(r'(?<![\w:])(?:bof3::)?(?:addr::)?([A-Z]\w*_\w+)', rhs):
                pc = syms.names.get(w)
                if pc is not None and syms.in_data(pc):
                    parts.append(f'base={w}')
                    break
        if not stride:
            # a walk: `p += 0xA4` / `p += kObjectStride` in the same function
            step = re.compile(r'(?<![\w.])' + re.escape(base) + r'\s*\+=\s*((?:\w+::)*k\w+|0[xX][0-9A-Fa-f]+|\d+)\s*[;)]')
            for t in lines[start:min(len(lines), line + 200)]:
                if t.startswith('}'):
                    break
                wm = step.search(t)
                if wm:
                    tok = wm.group(1)
                    key = tok.split('::')[-1]
                    stride = int(tok, 0) if tok[0].isdigit() else (
                        f.locals.get(key) if f.locals.get(key) is not None else env.get(key))
                    break
        if stride and 2 <= stride <= 0x10000:
            parts.append(f'stride={stride:#x}')
        if '(' in s and k == start and not parts:
            return 'param'
        if not any(p.startswith(('table=', 'base=')) for p in parts):
            # made by a helper: `= Sc()`, `= Party(actor[5])`
            for cm in re.finditer(r'(?<![\w:.>])((?:\w+::)*[A-Za-z_]\w*)\s*\(', rhs):
                fn = cm.group(1)
                if not re.search(r'(^|::)(static_cast|reinterpret_cast|const_cast|sizeof|U|UC|' +
                                 '|'.join(ACCESSORS) + r')$', fn):
                    parts.insert(0, f'via={fn.split("::")[-1]}')
                    break
        return ';'.join(parts) if parts else '-'
    sig = ' '.join(lines[start:min(line, start + 6)])
    if re.search(r'\b' + re.escape(base) + r'\b', sig):
        return 'param'
    return '-'


def scan(files, syms):
    by_rel = {f.rel: f for f in files}
    env = collect_constants(files, syms.names)
    defs_by_name = collections.defaultdict(list)
    for f in files:
        for name, value, ns, line in f.consts:
            defs_by_name[name].append((f, value, ns, line))

    acc_defs = {f.rel: accessor_defs(f) for f in files}

    def is_accessor(f, name):
        if name in acc_defs[f.rel]:
            return acc_defs[f.rel][name]
        for g in include_closure(f, by_rel):
            if name in acc_defs[g.rel]:
                return acc_defs[g.rel][name]
        return name not in CAST_BY_DEFAULT

    sites, unresolved_refs = [], []
    # bare or namespace-qualified (`move_script::SetLong(`, `sh::Mem(`), never a member call
    call_re = re.compile(r'(?<![\w.>:])(?:\w+::)*(' + '|'.join(sorted(ACCESSORS, key=len, reverse=True)) +
                         r')\s*\(')
    for f in files:
        code = f.code
        fuzz = '1' if re.search(r'_fuzz\.|_harness|fuzz_', f.rel) else '0'

        def at_value(m, k=None, f=f):
            v, _ = resolve_at(f, m.group(1), m.group(2), m.start(), by_rel, defs_by_name)
            return v
        # globals by constant
        for m in AT_REF.finditer(code):
            if f.in_at_namespace(m.start()):
                continue
            value, cands = resolve_at(f, m.group(1), m.group(2), m.start(), by_rel, defs_by_name)
            if not cands:
                # a constant of a struct or array type (the world-map tables'
                # `kWm87`, `kBannerStates`): its members, not it, hold addresses
                unresolved_refs.append((f.rel, m.group(2)))
                continue
            if value is None and not any(syms.in_data(c[1]) for c in cands):
                continue
            if value is not None and not syms.in_data(value):
                continue
            ln = line_of(f.starts, m.start())
            ls = f.starts[ln - 1]
            pre = code[ls:m.start()]
            wrap = re.search(r'(\w+)\s*\(\s*$', pre)
            acc = wrap.group(1) if wrap else '-'
            name = m.group(1) + 'at::' + m.group(2)
            if value is None:
                sites.append((f.rel, ln, 'global', acc, name, 'ambiguous', '-', '-', '-', '-', fuzz))
                continue
            cov, dname, ctype = syms.cover(value)
            sites.append((f.rel, ln, 'global', acc, name, f'{value:#x}', '-', cov, dname, ctype, fuzz))
        # record and raw through an accessor
        for m in call_re.finditer(code):
            name = m.group(1)
            if not is_accessor(f, name):
                continue
            r = balanced_args(code, m.end() - 1)
            if r is None or not r[0]:
                continue
            arg = r[0][0]
            # skip the accessor's own definition: 'Word(const unsigned char* p)'
            if re.search(r'\b(const|unsigned|std::|int|char|void|U)\b[\s\*&]+\w+\s*$', arg.strip()) and \
                    re.match(r'\s*(const\s+)?[\w:]+[\s\*&]+\w+\s*$', arg):
                continue
            if AT_REF.search(arg):
                continue
            ln = line_of(f.starts, m.start())
            lm = LIT.match(arg)
            if lm:
                v = int(lm.group(1), 16)
                if syms.in_data(v):
                    cov, dname, ctype = syms.cover(v)
                    sites.append((f.rel, ln, 'raw', name, ' '.join(arg.split())[:60], f'{v:#x}', '-',
                                  cov, dname, ctype, fuzz))
                continue
            om = LOCAL_OFF.match(arg)
            if not om:
                # a symbols.toml pointer plus an offset: `Sprite_Current + 0x38`
                gm = GLOBAL_OFF.match(arg)
                if gm and syms.in_data(syms.names.get(gm.group(1))):
                    sites.append((f.rel, ln, 'record', name, ' '.join(arg.split())[:60], '-',
                                  f'base={gm.group(1)}', '-', '-', '-', fuzz))
                continue
            ident = om.group(1)
            if re.match(r'k[A-Z0-9_]', ident) or ident in ('this', 'sizeof'):
                continue
            fam = stride_family(f, ident, ln, env, lambda tm, k: at_value(tm), syms)
            sites.append((f.rel, ln, 'record', name, ' '.join(arg.split())[:60], '-', fam,
                          '-', '-', '-', fuzz))
    sites.sort(key=lambda s: (s[0], s[1], s[2], s[4]))

    # constant map
    groups = collections.OrderedDict()
    for f in files:
        for name, value, ns, line in f.consts:
            key = (name, value)
            groups.setdefault(key, []).append(f'{f.rel}:{line}({ns})')
    consts = []
    for (name, value), mods in sorted(groups.items(), key=lambda kv: (kv[0][1] if kv[0][1] is not None else -1, kv[0][0])):
        if value is None:
            consts.append((name, 'unresolved', 'unknown', len(mods), ';'.join(mods), '-', '-', '-', '-'))
            continue
        kind = syms.kind(value)
        sname, skind = syms.name_at(value) if kind != 'value' else ('-', '-')
        cov, dname, ctype = syms.cover(value) if kind == 'data' else ('-', '-', '-')
        consts.append((name, f'{value:#x}', kind, len(mods), ';'.join(mods), sname, skind, cov, ctype))
    return sites, consts, files, unresolved_refs


def counts_of(sites, consts, files, syms, unresolved_refs):
    c = collections.OrderedDict()
    shapes = collections.Counter(s[2] for s in sites)
    c['sites'] = len(sites)
    c['sites_global'] = shapes['global']
    c['sites_record'] = shapes['record']
    c['sites_raw'] = shapes['raw']
    c['sites_in_fuzz'] = sum(1 for s in sites if s[10] == '1')
    c['files_with_sites'] = len({s[0] for s in sites})
    c['files_with_record_sites'] = len({s[0] for s in sites if s[2] == 'record'})
    rec = [s for s in sites if s[2] == 'record']
    c['record_sites_with_stride'] = sum(1 for s in rec if 'stride=' in s[6])
    c['record_sites_with_table_or_base'] = sum(1 for s in rec if 'table=' in s[6] or 'base=' in s[6])
    c['record_sites_on_named_pointer'] = sum(1 for s in rec if not re.match(r'\(?\s*[a-z_]', s[4]))
    c['record_sites_via_helper'] = sum(1 for s in rec if 'via=' in s[6])
    c['record_sites_param'] = sum(1 for s in rec if s[6] == 'param')
    c['record_sites_family_unknown'] = sum(1 for s in rec if s[6] == '-')
    c['record_strides'] = ' '.join(f'{k}:{v}' for k, v in sorted(
        collections.Counter(re.search(r'stride=(\w+)', s[6]).group(1) for s in rec if 'stride=' in s[6]).items(),
        key=lambda kv: -kv[1])[:12])
    c['global_sites_covered_exact'] = sum(1 for s in sites if s[2] == 'global' and s[7] == 'exact')
    c['global_sites_covered_inside'] = sum(1 for s in sites if s[2] == 'global' and s[7] == 'inside')
    c['global_sites_uncovered'] = sum(1 for s in sites if s[2] == 'global' and s[7] == 'none')
    c['global_sites_ambiguous'] = sum(1 for s in sites if s[2] == 'global' and s[5] == 'ambiguous')
    c['at_refs_struct_or_array'] = len(unresolved_refs)
    ndefs = sum(len(f.consts) for f in files)
    c['at_constant_definitions'] = ndefs
    c['at_constant_definitions_in_callees_h'] = sum(len(f.consts) for f in files if f.rel.endswith('_callees.h'))
    c['at_constants_name_value'] = len(consts)
    data_consts = [k for k in consts if k[2] == 'data']
    c['at_constants_data'] = len(data_consts)
    c['at_constants_code'] = sum(1 for k in consts if k[2] == 'code')
    c['at_constants_value'] = sum(1 for k in consts if k[2] == 'value')
    c['at_constants_unresolved'] = sum(1 for k in consts if k[2] == 'unknown')
    c['distinct_code_addresses'] = len({k[1] for k in consts if k[2] == 'code'})
    distinct = {k[1] for k in data_consts}
    c['distinct_addresses'] = len(distinct)
    by_addr = collections.defaultdict(set)
    for k in data_consts:
        by_addr[k[1]].add(k[0])
    c['max_names_one_address'] = max((len(v) for v in by_addr.values()), default=0)
    c['addresses_with_several_names'] = sum(1 for v in by_addr.values() if len(v) > 1)
    c['distinct_addresses_symbol_data'] = len({k[1] for k in data_consts if k[6] == 'data'})
    c['distinct_addresses_symbol_block'] = len({k[1] for k in data_consts if k[6] == 'block'})
    c['distinct_addresses_symbol_func'] = len({k[1] for k in data_consts if k[6] == 'func'})
    c['distinct_addresses_symbol_data_no_ctype'] = len({k[1] for k in data_consts if k[6] == 'data' and k[8] == '-'})
    c['distinct_addresses_name_missing'] = len({k[1] for k in data_consts if k[5] == '-'})
    c['distinct_addresses_name_missing_inside_entry'] = len(
        {k[1] for k in data_consts if k[5] == '-' and k[7] == 'inside'})
    # the round doc's other rows
    stride_defs = []
    for f in files:
        for m in re.finditer(r'\b(k\w*Stride\w*)\s*=\s*([^,;]+)', f.code):
            stride_defs.append((m.group(1), eval_expr(m.group(2), {})))
    c['stride_definitions'] = len(stride_defs)
    c['stride_names'] = len({n for n, v in stride_defs})
    c['stride_values'] = len({v for n, v in stride_defs if v is not None})
    c['static_assert_offsetof_sizeof'] = sum(
        len(re.findall(r'static_assert\s*\(\s*(?:offsetof|sizeof)\b', f.code)) for f in files)
    c['symbols_data'] = syms.n_data
    c['symbols_data_ctype'] = syms.n_ctype
    c['symbols_data_count'] = syms.n_count
    return c


SITE_HDR = ['rec', 'file', 'line', 'shape', 'accessor', 'expr', 'address', 'family', 'cover', 'data_name', 'ctype', 'fuzz']
CONST_HDR = ['rec', 'name', 'value', 'kind', 'n_modules', 'modules', 'symbol', 'symbol_kind', 'cover', 'ctype']


def write_tsv(path, sites, consts, counts):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('# tools/data_census.py: raw data accesses in src/ and the at:: constants. '
                'Addresses and names only (docs/typed-data-census.md).\n')
        f.write('#' + '\t'.join(SITE_HDR) + '\n')
        for s in sites:
            f.write('site\t' + '\t'.join(str(x) for x in s) + '\n')
        f.write('#' + '\t'.join(CONST_HDR) + '\n')
        for k in consts:
            f.write('const\t' + '\t'.join(str(x) for x in k) + '\n')
        for k, v in counts.items():
            f.write(f'#count {k}\t{v}\n')


def committed_sites(path):
    with open(path, encoding='utf-8') as f:
        for line in f:
            m = re.match(r'#count sites\t(\d+)', line)
            if m:
                return int(m.group(1))
    sys.exit(f'data_census: no "#count sites" line in {path}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', default=DEFAULT_OUT)
    ap.add_argument('--toml', default='symbols.toml')
    ap.add_argument('--counts', action='store_true', help='print the counts, write nothing')
    ap.add_argument('--check', action='store_true',
                    help='fail if the site count is above the committed --out file\'s')
    a = ap.parse_args()
    os.chdir(ROOT)
    syms = Symbols(a.toml)
    files = load_sources()
    sites, consts, files, unresolved_refs = scan(files, syms)
    counts = counts_of(sites, consts, files, syms, unresolved_refs)
    if a.check:
        was = committed_sites(a.out)
        now = counts['sites']
        if now > was:
            sys.exit(f'data_census: {now} raw data sites in src/, above the committed {was} '
                     f'({a.out}). Use a typed access (docs/typed-data-round.md), or '
                     f'regenerate the census if the rise is deliberate.')
        print(f'data_census: {now} raw data sites (committed {was}); ok')
        return
    for k, v in counts.items():
        print(f'{k}\t{v}')
    if not a.counts:
        write_tsv(a.out, sites, consts, counts)
        print(f'wrote {a.out}: {len(sites)} sites, {len(consts)} constants')


if __name__ == '__main__':
    main()
