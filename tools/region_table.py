#!/usr/bin/env python3
"""The region table: every run of the quicksave's span, with a name or an
address, its length and a class (docs/region-table.md; docs/state-by-name-round.md
3.4).

    python3 tools/region_table.py                # the counts by class
    python3 tools/region_table.py --tsv FILE     # the table, one run a row
    python3 tools/region_table.py --largest 20   # the largest unknown runs
    python3 tools/region_table.py --check        # CI: the unknown bytes may only fall

Reads only files in this repository - symbols.toml, tools/region_classes.txt,
tools/statehash_skip.txt, src/hook/snapshot.cpp's kKept, src/hook/exe_layout.h -
so it runs anywhere, CI included, with no game data and no build tree.
tools/gen_symbols.py calls build() and writes the table into the build as
bof3/region_table.gen.h, beside symbols.gen.h; src/hook/region_table.cpp checks
it at start-up.

The span is the exe host's: exe_layout.h kDataVa..kOwnVa (.data and .bss, the
end of .game, .pool), the bytes a quicksave copies (docs/quicksave.md 3).

A run is a piece of the span between two boundaries: the start and end of a
named member, of a row of tools/region_classes.txt. Its name is the member
that covers it (with "+0x.." when the run starts inside it), else the label
of the nearest name before it in the same gap (a symbols.toml entry without a
length, "~Name+0x.."), else none. Its class:

  * the class of the tools/region_classes.txt row that covers it;
  * else `state` when a member with a length names it (the quicksave writes
    it, and a name says someone read it as the game's);
  * else `unknown`.

Members with a length are GameData's (tools/gen_symbols.py: every typed
[[data]] that is not a field of a block, every typed [[block]]) and the
untyped [[block]] entries that give their records (record_stride *
record_count). An entry with no length - a [[data]] without a ctype, a
[[block]] without records - labels the gap it starts.

Refused here, so the generated header and CI cannot hold a table that
disagrees with the lists it stands beside:
  * two rows of tools/region_classes.txt over one byte, a row outside the
    span, a class that is not one of the five;
  * a `process` row that is not a kKept range, or a kKept range without its
    `process` row (kKept parsed out of src/hook/snapshot.cpp's source: it is
    in an unnamed namespace, so nothing outside that file can read it at run
    time);
  * a byte of tools/statehash_skip.txt that no row covers: every range the
    state hash cannot see has been read, and its row says what was found.

--check compares the unknown bytes with tools/region_unknown.txt, the
committed count: a rise fails, a fall passes and says to lower the number.
"""
import argparse, os, re, sys, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

CLASSES = ('state', 'process', 'derived', 'stack', 'unknown')
CLASS_FILE = os.path.join('tools', 'region_classes.txt')
SKIP_FILE = os.path.join('tools', 'statehash_skip.txt')
KEPT_FILE = os.path.join('src', 'hook', 'snapshot.cpp')
LAYOUT_FILE = os.path.join('src', 'hook', 'exe_layout.h')
UNKNOWN_FILE = os.path.join('tools', 'region_unknown.txt')
# The frame the round doc's 2.3 measured in: .data and .bss to the page past
# kBssEnd (BOF3.exe's section, before .rsrc), so its numbers compare.
FRAME_23 = (0x5DA000, 0x93E000)


class TableError(Exception):
    pass


def _read(root, rel):
    with open(os.path.join(root, rel), encoding='utf-8') as f:
        return f.read()


def layout(root):
    """exe_layout.h's constants by name (the `kName = 0x...;` lines make_exe.py reads too)."""
    return {m.group(1): int(m.group(2), 16)
            for m in re.finditer(r'\b(k\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;', _read(root, LAYOUT_FILE))}


def ranges_file(root, rel, want_class):
    """[(va, bytes, class or kind, why, line)] of an ADDRESS LENGTH WORD # why file."""
    out = []
    for n, line in enumerate(_read(root, rel).split('\n'), 1):
        body, _, why = line.partition('#')
        f = body.split()
        if not f:
            continue
        if len(f) != (3 if want_class else 2):
            raise TableError(f'{rel}:{n}: want ADDRESS LENGTH' + (' CLASS' if want_class else '') + f', got {body.strip()!r}')
        try:
            va, size = int(f[0], 0), int(f[1], 0)
        except ValueError:
            raise TableError(f'{rel}:{n}: {f[0]!r} {f[1]!r} is not ADDRESS LENGTH')
        if size <= 0:
            raise TableError(f'{rel}:{n}: an empty range')
        word = f[2] if want_class else None
        if want_class and word not in CLASSES:
            raise TableError(f'{rel}:{n}: class {word!r} is not one of {", ".join(CLASSES)}')
        # the skip list's kind is the comment's first word (noise / ours / both)
        kind = word if want_class else (why.split() or [''])[0]
        out.append((va, size, kind, why.strip(), n))
    return out


KEPT_ROW = re.compile(r'\{\s*(0x[0-9A-Fa-f]+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*"(\w+)"\s*\}')


def kept(root):
    """[(va, bytes, name)] of snapshot.cpp's `constexpr Kept kKept[] = { ... };`, in its order."""
    text = _read(root, KEPT_FILE)
    m = re.search(r'constexpr\s+Kept\s+kKept\[\]\s*=\s*\{(.*?)\n\};', text, re.S)
    if not m:
        raise TableError(f'{KEPT_FILE}: no `constexpr Kept kKept[] = {{ ... }};` to read')
    body = re.sub(r'//[^\n]*', '', m.group(1))
    rows = [(int(a, 16), int(b, 0), n) for a, b, n in KEPT_ROW.findall(body)]
    if len(rows) != body.count('{'):
        raise TableError(f'{KEPT_FILE}: kKept has {body.count("{")} rows, {len(rows)} of them read as {{VA, bytes, "Name"}}')
    return rows


def members(t):
    """([(va, bytes, name)] the named members with a length, [(va, name)] the
    names without one), from symbols.toml as gen_symbols.py lays out GameData."""
    import gen_symbols   # beside this file; imported here, since gen_symbols imports this module
    blocks = sorted(t.get('block', []), key=lambda e: e['pc'])
    datas = sorted(t.get('data', []), key=lambda e: e['pc'])
    typed_blocks = []
    for b in blocks:
        if 'ctype' not in b:
            continue
        e = {'name': b['name'], 'pc': b['pc'], 'ctype': b['ctype']}
        if b['record_count'] != 1:
            e['count'] = b['record_count']
        typed_blocks.append(e)
    typed = typed_blocks + [e for e in datas if 'ctype' in e and 'field_of' not in e]
    _, _, gd, _ = gen_symbols.game_data_layout(t['meta'], typed)
    out = sorted((pc, size, name) for pc, size, name, _, _, _ in gd)
    # An untyped block's records may lie over GameData members (two views of
    # one place: MsgBoxState holds typed members); such a block is a label of
    # its place, and the members name the bytes. One that overlaps none is a
    # member of its own (EnemyWorkingRecords).
    blocks_as_labels = []
    for b in blocks:
        if 'ctype' in b or 'record_stride' not in b:
            continue
        pc, size = b['pc'], b['record_stride'] * b['record_count']
        if any(m_pc < pc + size and pc < m_pc + m_size for m_pc, m_size, _ in out):
            blocks_as_labels.append((pc, b['name']))
        else:
            out.append((pc, size, b['name']))
    out.sort()
    labels = sorted([(e['pc'], e['name']) for e in datas if 'ctype' not in e and 'field_of' not in e]
                    + [(b['pc'], b['name']) for b in blocks if 'ctype' not in b and 'record_stride' not in b]
                    + blocks_as_labels)
    return out, labels


def build(root=ROOT, toml=None):
    """The table: a dict with span, runs [(va, bytes, name, cls, source)], the
    kept rows, the skip rows and the counts. source: `classes` (a row of
    tools/region_classes.txt), `named` (state by its name), `none` (unknown)."""
    with open(toml or os.path.join(root, 'symbols.toml'), 'rb') as f:
        t = tomllib.load(f)
    lay = layout(root)
    lo, hi = lay['kDataVa'], lay['kOwnVa']
    named, labels = members(t)
    named = [(pc, size, n) for pc, size, n in named if pc + size > lo and pc < hi]
    rows = ranges_file(root, CLASS_FILE, True)
    skips = ranges_file(root, SKIP_FILE, False)
    keep = kept(root)

    # The class rows: inside the span, no byte twice.
    rows.sort()
    for va, size, cls, why, n in rows:
        if va < lo or va + size > hi:
            raise TableError(f'{CLASS_FILE}:{n}: 0x{va:X} + 0x{size:X} is outside the span 0x{lo:X}..0x{hi:X}')
    for a, b in zip(rows, rows[1:]):
        if a[0] + a[1] > b[0]:
            raise TableError(f'{CLASS_FILE}:{b[4]}: 0x{b[0]:X} lies inside the row of line {a[4]} (0x{a[0]:X} + 0x{a[1]:X})')

    # process == kKept, row for row.
    proc = sorted((va, size) for va, size, cls, _, _ in rows if cls == 'process')
    want = sorted((va, size) for va, size, _ in keep)
    if proc != want:
        extra = [f'0x{v:X} {s}' for v, s in sorted(set(proc) - set(want))]
        lack = [f'0x{v:X} {s} ({n})' for v, s, n in keep if (v, s) not in set(proc)]
        raise TableError(f'{CLASS_FILE} and {KEPT_FILE} kKept disagree:'
                         + (f' process rows not in kKept: {", ".join(extra)};' if extra else '')
                         + (f' kKept ranges with no process row: {", ".join(lack)}' if lack else ''))

    # Every skip-list byte covered by a class row.
    for va, size, kind, why, n in skips:
        at = va
        for rva, rsize, _, _, _ in rows:
            if rva <= at < rva + rsize:
                at = rva + rsize
                if at >= va + size:
                    break
        if at < va + size:
            raise TableError(f'{SKIP_FILE}:{n}: 0x{at:X} (in 0x{va:X} {size}) has no row in {CLASS_FILE}: '
                             'read it and give it a class')

    # The partition.
    cuts = {lo, hi}
    for pc, size, _ in named:
        cuts.add(max(pc, lo))
        cuts.add(min(pc + size, hi))
    for va, size, _, _, _ in rows:
        cuts.update((va, va + size))
    # a label cuts the gap it starts; inside a member it names nothing here
    import bisect
    starts = [pc for pc, _, _ in named]
    for pc, _ in labels:
        i = bisect.bisect_right(starts, pc) - 1
        if lo < pc < hi and not (i >= 0 and pc < named[i][0] + named[i][1]):
            cuts.add(pc)
    cuts = sorted(cuts)
    runs, mi, ri, li = [], 0, 0, 0
    label = None
    for a, b in zip(cuts, cuts[1:]):
        while mi < len(named) and named[mi][0] + named[mi][1] <= a:
            mi += 1
        while ri < len(rows) and rows[ri][0] + rows[ri][1] <= a:
            ri += 1
        member = named[mi] if mi < len(named) and named[mi][0] <= a else None
        row = rows[ri] if ri < len(rows) and rows[ri][0] <= a else None
        while li < len(labels) and labels[li][0] <= a:
            label = labels[li]
            li += 1
        if member:
            name = member[2] + (f'+0x{a - member[0]:X}' if a > member[0] else '')
            label = None   # a label names only the gap it starts
        elif label:
            name = f'~{label[1]}' + (f'+0x{a - label[0]:X}' if a > label[0] else '')
        else:
            name = ''
        if row:
            cls, source = row[2], 'classes'
        elif member:
            cls, source = 'state', 'named'
        else:
            cls, source = 'unknown', 'none'
        runs.append((a, b - a, name, cls, source))

    counts = {c: [0, 0] for c in CLASSES}
    for _, size, _, cls, _ in runs:
        counts[cls][0] += size
        counts[cls][1] += 1
    return {'span': (lo, hi), 'runs': runs, 'kept': keep, 'skips': skips, 'rows': rows, 'counts': counts,
            'named': named, 'labels': labels}


def frame_23(tab):
    """The round doc 2.3's measure in its own frame: named bytes, unnamed runs and bytes."""
    lo, hi = FRAME_23
    named = [(max(pc, lo), min(pc + s, hi)) for pc, s, _ in tab['named'] if pc + s > lo and pc < hi]
    covered = sum(b - a for a, b in named)
    gaps, cur = [], lo
    for a, b in named:
        if a > cur:
            gaps.append((cur, a - cur))
        cur = max(cur, b)
    if hi > cur:
        gaps.append((cur, hi - cur))
    return {'members': len(named), 'bytes': hi - lo, 'named': covered, 'gaps': gaps}


def unknown_bytes(tab):
    return tab['counts']['unknown'][0]


def summary(tab, out=sys.stdout):
    lo, hi = tab['span']
    total = hi - lo
    w = out.write
    w(f'region_table: span 0x{lo:X}..0x{hi:X}, {total:,} bytes, {len(tab["runs"]):,} runs\n')
    for c in CLASSES:
        b, n = tab['counts'][c]
        w(f'  {c:8} {b:>10,} bytes {100 * b / total:5.1f} %  {n:>5,} runs\n')
    f = frame_23(tab)
    w(f'  in 2.3\'s frame 0x{FRAME_23[0]:X}..0x{FRAME_23[1]:X}: {f["members"]:,} named members cover '
      f'{f["named"]:,} of {f["bytes"]:,} bytes ({100 * f["named"] / f["bytes"]:.1f} %); '
      f'{len(f["gaps"]):,} unnamed runs, {sum(s for _, s in f["gaps"]):,} bytes\n')


def largest(tab, n, out=sys.stdout):
    # adjacent unknown runs are one stretch for this list (a label may cut a gap)
    merged = []
    for va, size, name, cls, src in tab['runs']:
        if cls != 'unknown':
            continue
        if merged and merged[-1][0] + merged[-1][1] == va:
            merged[-1][1] += size
            merged[-1][3].append(name)
        else:
            merged.append([va, size, name, [name]])
    for va, size, name, names in sorted(merged, key=lambda r: -r[1])[:n]:
        out.write(f'  0x{va:06X} {size:#9x} {size:>9,}  {name or "-"}\n')


def write_tsv(tab, path):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('va\tbytes\tclass\tsource\tname\n')
        for va, size, name, cls, src in tab['runs']:
            f.write(f'0x{va:06X}\t{size}\t{cls}\t{src}\t{name}\n')


def committed_unknown(root):
    for line in _read(root, UNKNOWN_FILE).split('\n'):
        body = line.split('#', 1)[0].strip()
        if body:
            return int(body)
    raise TableError(f'{UNKNOWN_FILE}: no number')


def check(tab, root=ROOT):
    """0 when the unknown bytes did not rise above the committed count."""
    have, ceiling = unknown_bytes(tab), committed_unknown(root)
    if have > ceiling:
        print(f'region_table: {have:,} unknown bytes, above the committed {ceiling:,} ({UNKNOWN_FILE}): '
              'the count may only fall - class the new bytes in tools/region_classes.txt or name them in '
              'symbols.toml (docs/region-table.md)')
        return 1
    if have < ceiling:
        print(f'region_table: {have:,} unknown bytes, below the committed {ceiling:,}: lower {UNKNOWN_FILE} '
              f'to {have} to keep the gain')
    else:
        print(f'region_table: {have:,} unknown bytes, the committed count')
    return 0


def emit_header(tab, sym_expr):
    """The generated header's text. sym_expr(name) is the C++ expression of a
    symbols.toml name's VA (gen_symbols.py knows which spelling each takes)."""
    lo, hi = tab['span']
    out = []
    w = out.append
    w('// GENERATED by tools/gen_symbols.py (tools/region_table.py) from symbols.toml,')
    w('// tools/region_classes.txt, tools/statehash_skip.txt and src/hook/snapshot.cpp\'s kKept.')
    w('// Do not edit. Every number is an address inside BOF3.exe or bof3x.exe (CLAUDE.md rule 3).')
    w('// docs/region-table.md; checked at start-up by src/hook/region_table.cpp.')
    w('#pragma once')
    w('#include <cstdint>')
    w('')
    w('#include "bof3/symbols.gen.h"')
    w('')
    w('namespace bof3::region_gen {')
    w('')
    w('enum class Class : std::uint8_t { State, Process, Derived, Stack, Unknown };')
    w('// Where a run\'s class came from: a row of tools/region_classes.txt, a name')
    w('// (state), or nothing (unknown).')
    w('enum class Source : std::uint8_t { Classes, Named, None };')
    w(f'inline constexpr unsigned kClasses = {len(CLASSES)};')
    w('inline constexpr const char* kClassName[kClasses] = {' + ', '.join(f'"{c}"' for c in CLASSES) + '};')
    w('')
    w(f'inline constexpr std::uint32_t kSpanVa = {lo:#x};    // exe_layout.h kDataVa')
    w(f'inline constexpr std::uint32_t kSpanEnd = {hi:#x};   // exe_layout.h kOwnVa')
    w('')
    w('// One run a row, in address order, end to end over the span. name: the member,')
    w('// "Member+0x.." part-way in, "~Name+0x.." a gap a length-less entry starts, "" none.')
    w('struct Run {')
    w('    std::uint32_t va;')
    w('    std::uint32_t bytes;')
    w('    Class cls;')
    w('    Source from;')
    w('    const char* name;')
    w('};')
    runs = tab['runs']
    w(f'inline constexpr unsigned kRuns = {len(runs)};')
    w('inline constexpr Run kRun[kRuns] = {')
    cname = {c: c.capitalize() for c in CLASSES}
    sname = {'classes': 'Classes', 'named': 'Named', 'none': 'None'}
    for va, size, name, cls, src in runs:
        w(f'    {{{va:#x}u, {size:#x}u, Class::{cname[cls]}, Source::{sname[src]}, "{name}"}},')
    w('};')
    w('')
    w('// Each run that starts a symbols.toml name: its index in kRun and the name\'s VA')
    w('// as this build\'s symbols.gen.h spells it (a GameData member\'s <Name>_addr is')
    w('// the compiler\'s offsetof, so the check compares the table with the layout).')
    w('struct NamedStart {')
    w('    unsigned run;')
    w('    std::uint32_t va;')
    w('};')
    starts = [(i, name) for i, (_, _, name, _, _) in enumerate(runs)
              if name and '+' not in name]
    w(f'inline constexpr unsigned kNamedStarts = {len(starts)};')
    w('inline constexpr NamedStart kNamedStart[kNamedStarts] = {')
    for i, name in starts:
        w(f'    {{{i}u, {sym_expr(name.lstrip("~"))}}},')
    w('};')
    w('')
    w('// The keep list as kKept had it when this was generated (tools/region_table.py')
    w('// refused the build had it differed from the process rows).')
    w('struct Range {')
    w('    std::uint32_t va;')
    w('    std::uint32_t bytes;')
    w('};')
    w(f'inline constexpr unsigned kKeptRows = {len(tab["kept"])};')
    w('inline constexpr Range kKept[kKeptRows] = {')
    for va, size, name in tab['kept']:
        w(f'    {{{va:#x}u, {size:#x}u}},   // {name}')
    w('};')
    w('// The skip list\'s rows (tools/statehash_skip.txt), each read and classed.')
    w(f'inline constexpr unsigned kSkipRows = {len(tab["skips"])};')
    w('inline constexpr Range kSkip[kSkipRows] = {')
    for va, size, _, _, _ in tab['skips']:
        w(f'    {{{va:#x}u, {size:#x}u}},')
    w('};')
    w('')
    w('// The counts by class, bytes and runs (tools/region_table.py).')
    w('inline constexpr std::uint32_t kBytes[kClasses] = {'
      + ', '.join(f'{tab["counts"][c][0]}u' for c in CLASSES) + '};')
    w('inline constexpr unsigned kRunsOf[kClasses] = {'
      + ', '.join(f'{tab["counts"][c][1]}u' for c in CLASSES) + '};')
    w('')
    w('}  // namespace bof3::region_gen')
    return '\n'.join(out) + '\n'


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--tsv', metavar='FILE', help='write the table, one run a row')
    ap.add_argument('--largest', type=int, metavar='N', help='list the N largest unknown stretches')
    ap.add_argument('--check', action='store_true', help=f'fail when the unknown bytes rise above {UNKNOWN_FILE}')
    a = ap.parse_args()
    try:
        tab = build()
    except TableError as e:
        print(f'region_table: {e}')
        return 1
    summary(tab)
    if a.largest:
        largest(tab, a.largest)
    if a.tsv:
        write_tsv(tab, a.tsv)
        print(f'region_table: {len(tab["runs"]):,} runs -> {a.tsv}')
    return check(tab) if a.check else 0


if __name__ == '__main__':
    sys.exit(main())
