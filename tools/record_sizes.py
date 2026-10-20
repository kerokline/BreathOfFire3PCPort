#!/usr/bin/env python
"""The size of every record struct, read from the record headers (typed-data
round stage C4, docs/typed-data-stage-c.md 6).

    python tools/record_sizes.py          # print `records::Name<TAB>size` for every struct
    python tools/record_sizes.py --fields records::SpriteObject   # its named fields, by offset

A symbols.toml [[data]] entry or [[block]] may take a record family as its
`ctype` (`ctype = "records::SpriteObject"`, `count = 30`): an array of
records, 0xA4 bytes each. The tools that size an entry (tools/data_widths.py,
tools/typed_convert.py, tools/data_census.py, tools/gen_symbols.py) need that
size without a compiler, so they read it here, from the one line every
struct in src/game/records*.h carries:

    static_assert(sizeof(SpriteObject) == 0xA4, "SpriteObject stride");

The headers assert each struct's size, and the build checks the assert, so
the number read here is the one the compiler lays out. A struct with no such
line is an error here, not a guess: a size the tools invent would size an
entry wrongly without a sound.

`fields(ctype)` gives a struct's named fields for tools/typed_convert.py,
each (offset, path, size, signed, kind): the offset from the header's
`static_assert(offsetof(...))` line (every named field has one), the type
from its declaration, a base struct's fields first, a named member of a
record type flattened into `member.field`, a scalar array into its elements
(`shade[1]`). Padding (`pad_*`) is not a field. The members of an anonymous
union are all listed, each at its offset: which one a site means is the
caller's choice (tools/typed_convert.py's UNION_NAMES).

Names only; no game data.
"""
import glob, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADERS = os.path.join('src', 'game', 'records*.h')
STRUCT = re.compile(r'^(?:struct|union)\s+([A-Za-z_]\w*)\s*(?::[^{;]*)?\{', re.M)
SIZEOF = re.compile(r'static_assert\s*\(\s*sizeof\s*\(\s*([A-Za-z_]\w*)\s*\)\s*==\s*(0[xX][0-9A-Fa-f]+|\d+)\s*,')

_cache = {}


def sizes(root=ROOT):
    """{'records::Name': size} for every top-level struct of the headers."""
    if root in _cache:
        return _cache[root]
    out, declared = {}, set()
    for path in sorted(glob.glob(os.path.join(root, HEADERS))):
        with open(path, encoding='utf-8') as f:
            text = f.read()
        declared.update(m.group(1) for m in STRUCT.finditer(text))
        for m in SIZEOF.finditer(text):
            out['records::' + m.group(1)] = int(m.group(2), 0)
    missing = sorted(n for n in declared if 'records::' + n not in out)
    if missing:
        sys.exit(f'record_sizes: no static_assert(sizeof(...)) for {", ".join(missing)} in {HEADERS}')
    _cache[root] = out
    return out


SCALARS = {
    'std::uint8_t': (1, False, 'int'), 'unsigned char': (1, False, 'int'), 'std::int8_t': (1, True, 'int'),
    'signed char': (1, True, 'int'), 'char': (1, True, 'int'),
    'std::uint16_t': (2, False, 'int'), 'unsigned short': (2, False, 'int'), 'std::int16_t': (2, True, 'int'),
    'short': (2, True, 'int'),
    'std::uint32_t': (4, False, 'int'), 'unsigned long': (4, False, 'int'), 'std::int32_t': (4, True, 'int'),
    'long': (4, True, 'int'), 'int': (4, True, 'int'), 'unsigned': (4, False, 'int'),
    'float': (4, True, 'float'),
}
_parsed = {}


def _strip(text):
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def _parse(root=ROOT):
    """{struct: (base or None, [(name, type, dims)])}, {(struct, name): offset}."""
    if root in _parsed:
        return _parsed[root]
    bodies, offs = {}, {}
    for path in sorted(glob.glob(os.path.join(root, HEADERS))):
        with open(path, encoding='utf-8') as f:
            text = _strip(f.read())
        for m in re.finditer(r'static_assert\s*\(\s*offsetof\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)\s*==\s*'
                             r'(0[xX][0-9A-Fa-f]+|\d+)', text):
            offs[(m.group(1), m.group(2))] = int(m.group(3), 0)
        for m in STRUCT.finditer(text):
            name = m.group(1)
            head = text[m.start():m.end()]
            bm = re.search(r':\s*(?:public\s+)?(\w+)', head)
            depth, i = 0, m.end() - 1
            while i < len(text):
                if text[i] == '{':
                    depth += 1
                elif text[i] == '}':
                    depth -= 1
                    if depth == 0:
                        break
                i += 1
            body = text[m.end():i]
            # anonymous unions / structs: their members are the struct's own
            flat = re.sub(r'\b(?:union|struct)\s*\{|\}\s*;', ' ', body)
            members = []
            for decl in flat.split(';'):
                decl = ' '.join(decl.split())
                if not decl or decl.startswith(('static_assert', 'using', 'friend')) or '(' in decl:
                    continue
                dm = re.fullmatch(r'((?:const\s+)?(?:unsigned\s+|signed\s+)?[\w:]+)\s+(.+)', decl)
                if not dm:
                    continue
                ty = dm.group(1)
                for d in dm.group(2).split(','):
                    d = d.strip()
                    am = re.fullmatch(r'(\w+)((?:\s*\[\s*(?:0[xX][0-9A-Fa-f]+|\d+)\s*\])*)', d)
                    if not am:
                        sys.exit(f'record_sizes: cannot read the member {d!r} of {name}')
                    dims = [int(x, 0) for x in re.findall(r'\[\s*(0[xX][0-9A-Fa-f]+|\d+)\s*\]', am.group(2))]
                    members.append((am.group(1), ty, dims))
            bodies[name] = (bm.group(1) if bm else None, members)
    _parsed[root] = (bodies, offs)
    return _parsed[root]


def fields(ctype, root=ROOT):
    """[(offset, path, size, signed, kind)] of a record ctype's named fields,
    by offset; kind 'int' or 'float'."""
    bodies, offs = _parse(root)
    out = []

    def walk(struct, base_off, prefix, owner_for_offsets):
        base, members = bodies[struct]
        if base:
            walk(base, base_off, prefix, base)
        for name, ty, dims in members:
            if name.startswith('pad'):
                continue
            off = offs.get((owner_for_offsets, name), offs.get((struct, name)))
            if off is None:
                sys.exit(f'record_sizes: no offsetof assert for {struct}::{name}')
            ty = ty.replace('records::', '')
            path = prefix + name
            if ty in bodies:
                sz = sizes(root)['records::' + ty]
                n = dims[0] if dims else 1
                for k in range(n):
                    walk(ty, base_off + off + k * sz, path + (f'[{k}].' if dims else '.'), ty)
                continue
            if ty not in SCALARS:
                sys.exit(f'record_sizes: {struct}::{name} has the unknown type {ty!r}')
            size, signed, kind = SCALARS[ty]
            if dims:
                total = 1
                for d in dims:
                    total *= d
                if len(dims) != 1:
                    continue      # a 2-D array: no element spelled here
                for k in range(dims[0]):
                    out.append((base_off + off + k * size, f'{path}[{k}]', size, signed, kind))
            else:
                out.append((base_off + off, path, size, signed, kind))

    t = ctype.strip().replace('records::', '')
    if t not in bodies:
        sys.exit(f'record_sizes: {ctype!r} names no struct of {HEADERS}')
    walk(t, 0, '', t)
    out.sort(key=lambda x: (x[0], -x[2]))
    return out


def bases(ctype, root=ROOT):
    """The records:: structs ctype derives from, nearest first."""
    bodies, _ = _parse(root)
    t = ctype.strip().replace('records::', '')
    out = []
    while t in bodies and bodies[t][0]:
        t = bodies[t][0]
        out.append('records::' + t)
    return out


def size_of(ctype, root=ROOT):
    """The size of a record ctype (`records::SpriteObject`), None for any other."""
    t = ctype.strip()
    if t.startswith('const '):
        t = t[6:].strip()
    if not t.startswith('records::') or '*' in t:
        return None
    s = sizes(root).get(t)
    if s is None:
        sys.exit(f'record_sizes: {ctype!r} names no struct of {HEADERS}')
    return s


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--fields':
        for off, path, size, signed, kind in fields(sys.argv[2]):
            print(f'+{off:#x}\t{path}\t{"s" if signed else "u"}{size * 8}{"f" if kind == "float" else ""}')
    else:
        for k, v in sorted(sizes().items()):
            print(f'{k}\t{v:#x}')
