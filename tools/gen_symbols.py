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

Record arrays (stage C4, docs/typed-data-stage-c.md 6). A `ctype` may be a
record family of src/game/records*.h (`ctype = "records::SpriteObject"`,
`count = 30`): the macro is then a pointer to record 0, so a site reads
`Sprite_Objects[i].x`, and `<Name>_count` counts records, not bytes. A
[[block]] that gives its records may carry such a `ctype` as well, which must
be a record of exactly `record_stride` bytes (tools/record_sizes.py reads each
struct's size from its header's static_assert): it is emitted as a typed
array of `record_count` records (`WindowRecords[16].state`), or, with one
record, as that record (`SharedScratch.w_0c`), with `<Name>_addr` (and
`<Name>_count` for an array) as for a [[data]] entry. A field entry stays a
field of its block either way. The generated header includes
game/records.h before any macro, so a record-typed macro expands to a
complete type wherever it is spelled, and the record headers themselves are
read before a macro of the same name exists (the block SharedScratch and
the struct records::SharedScratch share their name: past this header,
`records::SharedScratch` is not spellable, and nothing in src/ spells it).

GameData (stage C4 part 2, docs/typed-data-stage-c.md 6). The typed entries
past .text - every [[data]] with a `ctype` that is not a field of a block,
and every typed [[block]] - are laid out as the members of one struct,
`bof3::GameData`, in address order with named padding between
(`pad_<address>`). GameData starts at the first page past .text
(`bof3::kGameDataVa`) and ends at the end of the image; its size is
static_asserted. `BOF3_GAME` is the one pointer to it. A member's
`<Name>_addr` is computed from the struct (`kGameDataVa + offsetof(GameData,
Name)`) and static_asserted equal to symbols.toml's pc, with the member's
size; the member's macro casts that `<Name>_addr`, so every spelling keeps
its meaning and compiles to the same absolute address as before (a macro
spelled `BOF3_GAME->Name` would let the optimiser see one object and change
the code; the codegen gate refuses that). An entry that cannot be a member
keeps its own literal: one inside .text, one that starts inside an earlier
member (two views of the same bytes: Area00_Choices and Area00_Handlers),
and a field entry (`field_of`, a field of its block's records).

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
import argparse, collections, glob, os, re, sys, tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import record_sizes  # noqa: E402

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


SCALAR_SIZES = {'char': 1, 'signed char': 1, 'unsigned char': 1, 'short': 2, 'unsigned short': 2,
                'int': 4, 'unsigned int': 4, 'long': 4, 'unsigned long': 4, 'float': 4, 'double': 8,
                'std::uint8_t': 1, 'std::int8_t': 1, 'std::uint16_t': 2, 'std::int16_t': 2,
                'std::uint32_t': 4, 'std::int32_t': 4}


def ctype_size(ct):
    """(size, alignment) of one element of a ctype; None for one this does
    not know (the entry is then not a GameData member)."""
    c = ct.strip()
    if c.endswith('*'):
        return 4, 4
    if c.startswith('const '):
        c = c[6:].strip()
    if c.startswith('records::'):
        n = record_sizes.size_of(c)
        return (n, 4) if n else None
    n = SCALAR_SIZES.get(c)
    return (n, n) if n else None


def game_data_layout(meta, typed):
    """(start, end, members, others): GameData's extent, its members
    (pc, size, name, ctype, count or None, align) and the typed entries that
    stay outside it, each with the reason."""
    text_end = meta['text_va'] + meta['text_size']
    start = (text_end + 0xFFF) & ~0xFFF
    end = meta['image_base'] + meta['size_of_image']
    members, others, cur = [], [], start
    for e in sorted(typed, key=lambda e: e['pc']):
        sz = ctype_size(e['ctype'])
        if e['pc'] < start:
            others.append((e, 'inside .text'))
            continue
        if sz is None:
            others.append((e, 'ctype size unknown'))
            continue
        n = e.get('count')
        size = sz[0] * (n or 1)
        if e['pc'] < cur:
            others.append((e, 'starts inside an earlier member'))
            continue
        if e['pc'] + size > end:
            others.append((e, 'past the image'))
            continue
        members.append((e['pc'], size, e['name'], e['ctype'], n, sz[1]))
        cur = e['pc'] + size
    return start, end, members, others


def emit_game_data(w, start, end, members):
    w("// --- GameData: the typed entries past .text as one struct (stage C4). ---")
    w("// Members in address order, pad_<address> between; GameData starts at")
    w("// bof3::kGameDataVa and ends at the end of the image (tools/gen_symbols.py).")
    w("namespace bof3 {")
    w(f"inline constexpr std::uint32_t kGameDataVa = {start:#x};")
    w(f"inline constexpr std::uint32_t kGameDataSize = {end - start:#x};")
    w("struct GameData {")
    cur = start
    for pc, size, name, ct, n, al in members:
        if pc > cur:
            w(f"    unsigned char pad_{cur:x}[{pc - cur:#x}];")
        packed = " __attribute__((packed, aligned(1)))" if pc % al else ""
        dim = f"[{n}]" if n is not None else ""
        w(f"    using {name}_type = {ct};")
        w(f"    {name}_type {name}{dim}{packed};")
        cur = pc + size
    if end > cur:
        w(f"    unsigned char pad_{cur:x}[{end - cur:#x}];")
    w("};")
    w(f'static_assert(sizeof(GameData) == {end - start:#x}, "GameData size");')
    w("}  // namespace bof3")
    w("// The one pointer to the image's data, for code that wants the struct")
    w("// itself (BOF3_GAME->Sprite_Objects[3].x).")
    w("#define BOF3_GAME (reinterpret_cast<::bof3::GameData *>(::bof3::kGameDataVa))")
    w("// Each member's address, from the struct, checked against symbols.toml's")
    w("// pc: the macros below cast these, so every spelling compiles to the same")
    w("// absolute address as before.")
    w("#pragma clang diagnostic push")
    w('#pragma clang diagnostic ignored "-Winvalid-offsetof"')
    for pc, size, name, ct, n, al in members:
        w(f"inline constexpr std::uint32_t {name}_addr = ::bof3::kGameDataVa + "
          f"__builtin_offsetof(::bof3::GameData, {name});")
        w(f"static_assert({name}_addr == {pc:#x} && sizeof(::bof3::GameData::{name}) == {size:#x}, "
          f'"GameData::{name}");')
    w("#pragma clang diagnostic pop")
    w("")


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
    w('// The record families a typed entry may name (records::SpriteObject, ...),')
    w('// read before any macro below exists (tools/gen_symbols.py, stage C4).')
    w('#include "game/records.h"')
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
        if 'ctype' in b:
            if 'record_stride' not in b:
                fail(f'{b["name"]}: a [[block]] ctype needs record_stride and record_count')
            if b.get('record_size', b['record_stride']) != b['record_stride']:
                fail(f'{b["name"]}: a [[block]] ctype types whole records; record_size differs')
            size = record_sizes.size_of(b['ctype'])
            if size != b['record_stride']:
                fail(f'{b["name"]}: ctype {b["ctype"]} is {size!r} bytes, record_stride {b["record_stride"]:#x}')

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
        if 'ctype' in e:
            record_sizes.size_of(e['ctype'])     # a records:: ctype names a struct, or this fails
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
    typed_blocks = []
    for b in blocks:
        if 'ctype' not in b:
            continue
        e = {'name': b['name'], 'pc': b['pc'], 'ctype': b['ctype']}
        if b['record_count'] != 1:
            e['count'] = b['record_count']
        typed_blocks.append(e)
    gd_start, gd_end, gd_members, gd_others = game_data_layout(
        meta, typed_blocks + [e for e in datas if 'ctype' in e and '_field' not in e])
    emit_game_data(w, gd_start, gd_end, gd_members)
    in_gd = {m[2] for m in gd_members}

    def data_macro(e, ty):
        # a GameData member casts its <Name>_addr (from the struct); any other
        # typed entry its literal pc
        at = f'{e["name"]}_addr' if e['name'] in in_gd else f'{e["pc"]:#x}'
        if 'count' in e:
            return f'#define {e["name"]} (reinterpret_cast<{ty} *>({at}))'
        return f'#define {e["name"]} (*reinterpret_cast<{ty} *>({at}))'
    for e in typed_blocks:
        w(f'// [[block]] {e["name"]}: its records as {e["ctype"]}')
        w(data_macro(e, e['ctype']))
        if 'count' in e:
            w(f'inline constexpr unsigned {e["name"]}_count = {e["count"]};')
        if e['name'] not in in_gd:
            w(f'inline constexpr std::uint32_t {e["name"]}_addr = {e["pc"]:#x};')
    for e in datas:
        if 'ctype' not in e:
            continue
        ty = e['ctype']
        if '_field' in e:
            w(f'// a field of {e["field_of"]}: record {e["_field"][0]} +{e["_field"][1]:#x} (stage C1\'s struct)')
        w(data_macro(e, ty))
        if 'count' in e:
            w(f'inline constexpr unsigned {e["name"]}_count = {e["count"]};')
        # The macro above rewrites `bof3::addr::<name>` wherever it is spelled,
        # so a typed entry's address is reached as <name>_addr instead (a
        # harness's Mem(<name>_addr), a pointer form At(<name>_addr)).
        if e['name'] not in in_gd:
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
    print(f'gen_symbols: GameData {gd_end - gd_start:#x} bytes at {gd_start:#x}, {len(gd_members)} members; '
          f'{len(gd_others)} typed entries outside it ('
          + ', '.join(f'{v} {k}' for k, v in sorted(collections.Counter(r for _, r in gd_others).items())) + ')')


if __name__ == '__main__':
    main()
