#!/usr/bin/env python
"""Generate the C++ symbol header from symbols.toml.

    python tools/gen_symbols.py --toml symbols.toml --out build/gen/bof3/symbols.gen.h

symbols.toml is the only place an address is typed (docs/SCAFFOLDING.md). This
emits, for every named function and data item:

  * an address constant, `bof3::addr::Name`;
  * for a function with a signature (`ret`/`params`), exactly ONE binding of
    the bare name:
      - no `impl`  -> a macro casting the literal address to a typed function
                      pointer, so our code calls Capcom's code;
      - `impl`     -> an ordinary prototype for OUR function, plus an
                      `orig::Name` pointer to the address it displaced.
    Call sites are the same in both cases, so taking over a function is: write
    it, add `impl` to its symbols.toml entry, add one BOF3_INJECT line.
  * for data with a `ctype`: a macro giving a typed lvalue (scalar) or a typed
    pointer to element 0 (`count` present).

Record fields (typed-data round stage C3, docs/typed-data-stage-c.md 5). A
[[block]] may give its records: `record_stride`, `record_count` and, when a
record is shorter than the stride, `record_size` (the block's records are
pc + stride * i, i < count, each record_size bytes). A [[data]] entry inside
those records carries `field_of = "<the block's name>"` and `field_offset`
(its offset in the record): it is a field of that record family (stage C1's
struct in src/game/records.h), not a global of its own. Its macro is still
emitted, with a comment naming the record and offset, while sites use it; a
field entry is removed once its sites read the struct. Checked here: the
block exists and gives its records, the entry lies inside them, and
`field_offset` is the entry's offset in its record. tools/data_widths.py
reads the same keys (its rule 0: an address inside a block's records is a
field, never measured as a scalar).

The output contains addresses and names only — our work product, not game data.
It is still generated into the build tree rather than committed, so it cannot
drift from symbols.toml.

    python tools/gen_symbols.py --toml symbols.toml --out ... --retirable analysis/retirable_constants.tsv

`--retirable` adds the address-to-name check (docs/typed-data-round.md 2,
group SG): every module constant (`constexpr ... kName = 0x...` in a
src/**/*_callees.h) whose value is the address of a [[data]] entry is written
as one row: address, header, constant, the entry's name. Those constants are
retirable, the typed global already naming their address. Addresses and names
only. Nothing is removed here; stage B retires them at the sites.
"""
import argparse, glob, os, re, sys, tomllib

IDENT = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*$')
CCS = {'__cdecl', '__stdcall', '__fastcall', '__thiscall'}
CONSTEXPR = re.compile(r'constexpr\b[^;]*?;', re.S)
CONST_ITEM = re.compile(r'\b(k[A-Za-z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+)[uUlL]*\b')


def module_constants(root):
    """(header, line, name, value) for every hex `kName = 0x...` inside a
    constexpr statement of a *_callees.h under root/src, path then line order."""
    out = []
    for path in sorted(glob.glob(os.path.join(root, 'src', '**', '*_callees.h'), recursive=True)):
        with open(path, encoding='utf-8') as f:
            text = f.read()
        rel = os.path.relpath(path, root).replace(os.sep, '/')
        for m in CONSTEXPR.finditer(text):
            stmt, line = m.group(0), text.count('\n', 0, m.start()) + 1
            for it in CONST_ITEM.finditer(stmt):
                out.append((rel, line + stmt.count('\n', 0, it.start()),
                            it.group(1), int(it.group(2), 16)))
    return out


def write_retirable(path, root, datas):
    """The address-to-name check: module constants a [[data]] entry names."""
    names = {e['pc']: e['name'] for e in datas}
    rows = sorted({(v, h, k, names[v]) for h, _, k, v in module_constants(root) if v in names})
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('address\theader\tconstant\tdata\n')
        for v, h, k, n in rows:
            f.write(f'0x{v:X}\t{h}\t{k}\t{n}\n')
    print(f'gen_symbols: {len(rows)} retirable module constants '
          f'({len({r[0] for r in rows})} addresses) -> {path}')


def fail(msg):
    sys.exit(f'gen_symbols: {msg}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--toml', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--retirable', metavar='TSV',
                    help='also write the module constants whose address a [[data]] entry names')
    a = ap.parse_args()

    with open(a.toml, 'rb') as f:
        t = tomllib.load(f)
    meta = t['meta']
    lo, hi = meta['image_base'], meta['image_base'] + meta['size_of_image']

    seen_name, seen_addr = {}, {}
    def check(kind, e):
        name, pc = e.get('name'), e.get('pc')
        if not name or not IDENT.match(name):
            fail(f'{kind} at {pc!r}: bad or missing name {name!r}')
        if not isinstance(pc, int) or not lo <= pc < hi:
            fail(f'{name}: pc {pc!r} is outside the image {lo:#x}..{hi:#x}')
        if name in seen_name:
            fail(f'{name}: name bound twice ({seen_name[name]:#x} and {pc:#x})')
        if pc in seen_addr:
            fail(f'{pc:#x}: address bound twice ({seen_addr[pc]} and {name})')
        seen_name[name], seen_addr[pc] = pc, name

    out = []
    w = out.append
    w('// GENERATED by tools/gen_symbols.py from symbols.toml. Do not edit.')
    w('// Every number in this file is an address inside BOF3.exe (CLAUDE.md rule 3).')
    w('#pragma once')
    w('#include <cstdint>')
    w('')
    w('namespace bof3::image {')
    w(f'inline constexpr std::uint32_t kBase = {meta["image_base"]:#x};')
    w(f'inline constexpr std::uint32_t kTimestamp = {meta["pe_timestamp"]:#x};')
    w(f'inline constexpr std::uint32_t kSizeOfImage = {meta["size_of_image"]:#x};')
    w(f'inline constexpr std::uint32_t kTextVa = {meta["text_va"]:#x};')
    w(f'inline constexpr std::uint32_t kTextSize = {meta["text_size"]:#x};')
    w(f'inline constexpr char kSha256[] = "{meta["sha256"]}";')
    w('}  // namespace bof3::image')
    w('')

    funcs = sorted(t.get('func', []), key=lambda e: e.get('pc', 0))
    datas = sorted(t.get('data', []), key=lambda e: e.get('pc', 0))
    blocks = sorted(t.get('block', []), key=lambda e: e.get('pc', 0))
    for e in funcs: check('func', e)
    for e in datas: check('data', e)
    for e in blocks: check('block', e)

    by_block = {b['name']: b for b in blocks}
    for b in blocks:
        keys = [k for k in ('record_stride', 'record_count', 'record_size') if k in b]
        if keys and not {'record_stride', 'record_count'} <= set(keys):
            fail(f'{b["name"]}: record_stride and record_count go together')
        if b.get('record_size', 0) > b.get('record_stride', 0):
            fail(f'{b["name"]}: record_size is larger than record_stride')

    def field_of(e):
        """(record, offset) of a field entry, checked against its block."""
        b = by_block.get(e['field_of'])
        if b is None or 'record_stride' not in b:
            fail(f'{e["name"]}: field_of {e["field_of"]!r} is not a [[block]] with record_stride')
        if 'field_offset' not in e:
            fail(f'{e["name"]}: field_of without field_offset')
        rel = e['pc'] - b['pc']
        rec, off = divmod(rel, b['record_stride'])
        if rel < 0 or rec >= b['record_count'] or off >= b.get('record_size', b['record_stride']):
            fail(f'{e["name"]}: {e["pc"]:#x} is outside {b["name"]}\'s records')
        if off != e['field_offset']:
            fail(f'{e["name"]}: field_offset {e["field_offset"]:#x}, but {e["pc"]:#x} is '
                 f'{b["name"]} record {rec} +{off:#x}')
        return rec, off

    for e in datas:
        if 'field_offset' in e and 'field_of' not in e:
            fail(f'{e["name"]}: field_offset without field_of')
        if 'field_of' in e:
            e['_field'] = field_of(e)

    w('namespace bof3::addr {')
    for e in blocks + datas + funcs:
        w(f'inline constexpr std::uint32_t {e["name"]} = {e["pc"]:#x};')
    w('}  // namespace bof3::addr')
    w('')

    ours, theirs = [], []
    for e in funcs:
        if 'impl' in e and 'ret' not in e:
            fail(f'{e["name"]}: has impl but no signature')
        if 'ret' not in e:
            continue
        if 'params' not in e:
            fail(f'{e["name"]}: ret without params (write "void" for none)')
        cc = e.get('cc', '__cdecl')
        if cc not in CCS:
            fail(f'{e["name"]}: unknown calling convention {cc!r}')
        if 'impl' in e:
            if not os.path.isfile(os.path.join(os.path.dirname(os.path.abspath(a.toml)), e['impl'])):
                fail(f'{e["name"]}: impl file {e["impl"]} does not exist')
            ours.append((e, cc))
        else:
            theirs.append((e, cc))

    w('// --- Functions still Capcom\'s: the name is the original address. ---')
    for e, cc in theirs:
        w(f'#define {e["name"]} (reinterpret_cast<{e["ret"]} ({cc} *)({e["params"]})>({e["pc"]:#x}))')
    w('')
    w('// --- Functions that are ours: the name is our implementation. ---')
    for e, cc in ours:
        w(f'// {e["impl"]}')
        w(f'extern "C" {e["ret"]} {cc} {e["name"]}({e["params"]});')
    w('')
    w('// The address each of ours displaced. After BOF3_INJECT its first five')
    w('// bytes are a jmp to ours, so this is for the detour layer, not for calling.')
    w('namespace bof3::orig {')
    for e, cc in ours:
        w(f'inline const auto {e["name"]} = reinterpret_cast<{e["ret"]} ({cc} *)({e["params"]})>({e["pc"]:#x});')
    w('}  // namespace bof3::orig')
    w('')

    w('// --- Data. Scalars are lvalues; arrays are pointers to element 0. ---')
    for e in datas:
        if 'ctype' not in e:
            continue
        ty = e['ctype']
        if '_field' in e:
            w(f'// a field of {e["field_of"]}: record {e["_field"][0]} +{e["_field"][1]:#x} (stage C1\'s struct)')
        if 'count' in e:
            w(f'#define {e["name"]} (reinterpret_cast<{ty} *>({e["pc"]:#x}))')
            w(f'inline constexpr unsigned {e["name"]}_count = {e["count"]};')
        else:
            w(f'#define {e["name"]} (*reinterpret_cast<{ty} *>({e["pc"]:#x}))')
        # The macro above rewrites `bof3::addr::<name>` wherever it is spelled,
        # so a typed entry's address is reached as <name>_addr instead (a
        # harness's Mem(<name>_addr), a pointer form At(<name>_addr)).
        w(f'inline constexpr std::uint32_t {e["name"]}_addr = {e["pc"]:#x};')
    w('')

    if a.retirable:
        write_retirable(a.retirable, os.path.dirname(os.path.abspath(a.toml)), datas)

    text = '\n'.join(out)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    # Leave the file alone when nothing changed so the build does not re-run.
    try:
        with open(a.out, encoding='utf-8') as f:
            if f.read() == text:
                return
    except FileNotFoundError:
        pass
    with open(a.out, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    print(f'gen_symbols: {len(ours)} ours, {len(theirs)} callable originals, '
          f'{len(funcs)} functions named -> {a.out}')


if __name__ == '__main__':
    main()
