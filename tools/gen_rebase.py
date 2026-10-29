#!/usr/bin/env python
"""Generate the rebase table from recipes/exe-pointers.tsv.

    python tools/gen_rebase.py --pointers recipes/exe-pointers.tsv --out build/gen/bof3/rebase.gen.h

The relocatable image (docs/relocatable-image-design.md 3): when bof3x.exe
lies at another base than 0x400000, the engine adds the move to every pointer
word of .data after the copy (src/hook/exe_host.cpp, Rebase). The words are
recipes/exe-pointers.tsv's runs - `words` 4-byte words from `pc`, each
pointing into the section `to` names (data, bss, rdata, text) - less the
words of NOT_MOVED below, which the recipe holds as pointers and which are
not: each is listed with what it is. The engine checks each word's value lies
in its section's linked range before it moves it, so a layer or a cache that
wrote a number over a pointer is refused, not moved.

Why the recipe holds look-alikes: tools/exe_tables.py calls a word a pointer
when its value lies in the image and the discs do not all hold it equal - so
a word no disc places (a PC-only string, a port table) with a value in range
is taken for one. Moving it at another base changes a string or a number,
and the state hash, which hashes a word whose value lies in the image as its
offset, does not show it (design 3.4): a route shows it only as a garbled
string or a wrong number, so the list is the result of a sweep of every word
of the recipe against the player's BOF3.exe (the relocatable-image round's
stage C1, 2026-10-28): each word read with its neighbourhood, looked for as
(1) a word inside a NUL-terminated run of printable bytes whose start some
code operand or listed pointer uses as an address, (2) a word whose value is
not 4-aligned, (3) a word alone between unlisted neighbours of the same
shape (colours, coordinates, packed halves), and every bss and rdata word
read by hand. The words found are below; every other candidate was read and
is a pointer (code-pointer tables whose bytes happen to be printable, the C
runtime's ctype pointers 0x674E58 / 0x674E5C to its table + 2, its
self-linked list heads 0x672970..).

Addresses and counts only: the table is layout, as symbols.toml's addresses
are (CLAUDE.md rules 1 and 3).
"""
import argparse
import os
import sys

CLASSES = ('data', 'bss', 'rdata', 'text')

# Held as pointers by the recipe (its class first), not pointers by their
# reading: the reason for each.
NOT_MOVED = {
    # numbers the recipe calls bss pointers
    0x5E1AC0: ('bss', "area 9's entry-script halfwords: its descriptor 0x5E1AD0 names 0x5E1AB8 as its entries and "
                      "0x5E1ABC as its scripts, and the halfwords run through this word (docs/install-gap-labels.md)"),
    0x6542F0: ('bss', "EffectKind28's beam colours, three-byte values read in place from 0x6542EC "
                      "(effect_2a_callees.h kBeamColours28)"),
    0x65432C: ('bss', "EffectKind2A's beam colours, as kind 0x28's, from 0x654328 (kBeamColours2A)"),
    0x654C08: ('bss', "the last word of a byte table ending where EffectKind7F_States begins (0x654C0C); its "
                      "neighbours are bytes 0x82..0x87, the same shape"),
    # printf formats the recipe calls data pointers: each word is the whole
    # NUL-terminated format (three printable bytes and the NUL), its start
    # pushed by the code that formats with it; between unrelated tables
    0x64ADD8: ('data', "a decimal-count format after BattleExtra_TallyOpenSteps' entries; 0x64ADDC is a second one"),
    0x64ADDC: ('data', "a decimal-count format, the second after BattleExtra_TallyOpenSteps' entries"),
    0x64D3EC: ('data', "Boss26Fx_CountFormat, a two-digit count format (the one the 2026-10-28 proof run garbled in "
                       "the save list)"),
    0x64E324: ('data', "a three-digit count format after Battle_CommandBoxes' table"),
    0x65AB08: ('data', "a five-digit count format after IdentifyTint_Steps' entries"),
    0x66AF34: ('data', "a six-digit count format after Win2_TitleBoxStates' bytes"),
    # the last word of an error message of the port's MCI string table: the
    # message's final three letters and its NUL, the message's start pushed by
    # the code that reports it
    0x672488: ('data', "the end of an MCI error message (the table at 0x672470)"),
    0x6724B4: ('data', "the end of an MCI error message (from 0x67249C)"),
    0x672568: ('data', "the end of an MCI error message (from 0x672558)"),
    0x67257C: ('data', "the end of an MCI error message (from 0x67256C)"),
    0x672648: ('data', "the end of an MCI error message (from 0x672630)"),
    0x672674: ('data', "the end of an MCI error message (from 0x67265C)"),
    0x672728: ('data', "the end of an MCI error message (from 0x672718)"),
    0x67273C: ('data', "the end of an MCI error message (from 0x67272C)"),
    # a packed pair the recipe calls a data pointer
    0x617EB8: ('data', "two halfwords (5, 0x61) in a run of halfword records past Area96_Handlers' table (the same "
                       "four bytes 80 23 00 3B repeat either side); as a pointer it would be odd and land inside "
                       "another area's messages, Area81_ChoiceMessages + 0x231"),
}


def read_runs(path):
    runs = []
    for line in open(path, encoding='utf-8'):
        p = line.rstrip('\n').split('\t')
        if not p[0].startswith('0x'):
            continue
        if len(p) != 3 or p[2] not in CLASSES:
            sys.exit(f'gen_rebase: {path}: a row that is not pc, words, to: {line.rstrip()!r}')
        runs.append((int(p[0], 16), int(p[1]), p[2]))
    return runs


def split(runs):
    """The runs less the NOT_MOVED words, each such word cutting its run."""
    out, seen = [], set()
    for pc, n, to in runs:
        start = pc
        for k in range(n + 1):
            at = pc + 4 * k
            if k == n or at in NOT_MOVED:
                if at in NOT_MOVED and k < n:
                    if NOT_MOVED[at][0] != to:
                        sys.exit(f'gen_rebase: {at:#x} is held as {to}, not {NOT_MOVED[at][0]}')
                    seen.add(at)
                if at > start:
                    out.append((start, (at - start) // 4, to))
                start = at + 4
    missing = set(NOT_MOVED) - seen
    if missing:
        sys.exit('gen_rebase: not in the recipe any more: ' + ', '.join(f'{a:#x}' for a in sorted(missing)))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--pointers', required=True, help='recipes/exe-pointers.tsv')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    runs = split(read_runs(a.pointers))
    prev = 0
    for pc, n, to in runs:
        if pc % 4 or pc < prev or n < 1 or n > 0xFFFF:
            sys.exit(f'gen_rebase: a run {pc:#x} x{n} out of order, unaligned or too long')
        prev = pc + 4 * n
    words = {c: sum(n for _, n, to in runs if to == c) for c in CLASSES}
    lines = [
        '// GENERATED by tools/gen_rebase.py from recipes/exe-pointers.tsv; do not edit.',
        "// The pointer words of .data the engine moves with the image (src/hook/exe_host.cpp",
        '// Rebase; docs/relocatable-image-design.md 3). Addresses and counts only.',
        '#pragma once',
        '',
        '#include <cstdint>',
        '',
        'namespace bof3::rebase {',
        '',
        'enum To : std::uint8_t { kData, kBss, kRdata, kText };',
        'struct Run {',
        '    std::uint32_t pc;',
        '    std::uint16_t words;',
        '    To to;',
        '};',
        '',
        f'inline constexpr unsigned kRunCount = {len(runs)};',
        f'inline constexpr unsigned kWords[4] = {{{words["data"]}, {words["bss"]}, {words["rdata"]}, {words["text"]}}};',
        f'inline constexpr unsigned kNotMoved = {len(NOT_MOVED)};',
        'inline constexpr Run kRuns[kRunCount] = {',
    ]
    tag = {'data': 'kData', 'bss': 'kBss', 'rdata': 'kRdata', 'text': 'kText'}
    for pc, n, to in runs:
        lines.append(f'    {{{pc:#x}, {n}, {tag[to]}}},')
    lines += ['};', '', '}  // namespace bof3::rebase', '']
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    text = '\n'.join(lines)
    try:
        if open(a.out, encoding='utf-8').read() == text:
            return
    except OSError:
        pass
    with open(a.out, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    print(f'gen_rebase: {len(runs)} runs; words data {words["data"]}, bss {words["bss"]}, rdata {words["rdata"]}, '
          f'text {words["text"]}; {len(NOT_MOVED)} the recipe holds as pointers left alone: strings and numbers')


if __name__ == '__main__':
    main()
