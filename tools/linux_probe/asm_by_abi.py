#!/usr/bin/env python
"""Every inline-asm block in src/, by the width half it belongs to, and which of the x86-64 ones
name the Win64 calling convention (docs/linux-round.md section 4, LA3).

    python tools/linux_probe/asm_by_abi.py            # the counts and the list of Win64-dependent blocks
    python tools/linux_probe/asm_by_abi.py --all      # every x86-64 block, with its verdict

A block is an `asm(` / `__asm__(` statement (tools/width_census.py's ASM). Its half comes from the
`#if defined(__i386__)` / `#elif defined(__x86_64__)` branch it sits in: `i686` (inside an
i386 branch, or game/task_switch_i686.h), `x86_64` (inside an x86_64 branch, or a file only the
64-bit build includes: game/task_switch_x86_64.h), else `common` (no width in the text: the x87 and
control-word blocks, cltd, the fs: reads' users). A harness file (name holding `fuzz`) is counted apart.
A negated test (`#if !defined(__x86_64__)`, `#ifndef __i386__`) names the other half: the build
has only these two widths.

An x86-64 block is `win64` when its text names a Win64 argument register as an incoming or outgoing
argument (rcx, ecx, cx, cl, rdx, edx, dl, r8, r9), argument (rcx, ecx, cx, cl, rdx, edx, dl, r8, r9), the 32-byte home space (an immediate of 32, 0x20, 40, 0x68, 160 or 168: a frame sized for it),
or the Win64 callee-saved set beyond System V's (rsi, rdi, xmm6..xmm15). Every other x86-64 block is
`neutral`, e.g. a `jmp`-only forwarder or pure memory work: one text serves both ABIs when caller and
callee share one. The verdict is by the text alone; the doc says which blocks were then read by hand.
"""
import os, re, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import data_census as dc  # noqa: E402

ASM = re.compile(r'(?<![\w.])(?:__asm__|__asm|asm)\s*(?:(?:volatile|__volatile__|goto|inline)\s*)*\(')
COND = re.compile(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$')
WIN64_REG = re.compile(r'%%?(?:rcx|ecx|cx|cl|rdx|edx|dx|dl|r8d?|r9d?|rsi|esi|rdi|edi|xmm(?:6|7|8|9|1[0-5]))\b')
HOME = re.compile(r'\$(?:32|40|0x68|168|160)\b|\$0x20\b')


I686 = re.compile(r'__i386__|_M_IX86')
X64 = re.compile(r'__x86_64__|_WIN64|_M_X64')
OTHER = {'i686': 'x86_64', 'x86_64': 'i686'}


def width_of(test, negated):
    """The half a condition selects: the width it names, or the other one when negated."""
    kind = 'i686' if I686.search(test) else ('x86_64' if X64.search(test) else 'other')
    if kind != 'other' and (negated or re.match(r'\s*!', test)):
        kind = OTHER[kind]
    return kind


def half_at(stack):
    """stack: list of (cond_text, taken_branch_kind). A branch is 'i686' under __i386__, 'x86_64' under __x86_64__."""
    for text, kind in reversed(stack):
        if kind in ('i686', 'x86_64'):
            return kind
    return None


def main():
    allrows = '--all' in sys.argv
    rows = []
    for f in dc.load_sources():
        if not f.rel.endswith(('.cpp', '.h')):
            continue
        base = os.path.basename(f.rel)
        lines = f.code.split('\n')
        stack = []   # one (condition text, kind) per open #if: the kind of the branch being read
        starts = f.starts
        # asm statement positions by line
        asm_lines = {}
        for m in ASM.finditer(f.code):
            ln = f.code.count('\n', 0, m.start()) + 1
            # the statement's text: up to the matching close paren
            depth, i = 0, m.end() - 1
            while i < len(f.code):
                c = f.code[i]
                if c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
                    if depth == 0:
                        break
                i += 1
            # strings were blanked by strip_code; read the raw text over the same span
            asm_lines[ln] = f.raw[m.start():i + 1]
        for ln, line in enumerate(lines, 1):
            m = COND.match(line)
            if m:
                k, rest = m.group(1), m.group(2)
                if k in ('if', 'ifdef', 'ifndef'):
                    stack.append((rest, width_of(rest, k == 'ifndef')))
                elif k == 'elif':
                    if stack:
                        stack[-1] = (rest, width_of(rest, False))
                elif k == 'else':
                    if stack:
                        t, prev = stack[-1]
                        stack[-1] = (t, OTHER.get(prev, 'other'))
                elif k == 'endif':
                    if stack:
                        stack.pop()
            if ln in asm_lines:
                half = half_at(stack)
                if base == 'task_switch_x86_64.h':
                    half = 'x86_64'
                if base == 'task_switch_i686.h':
                    half = 'i686'
                text = asm_lines[ln]
                fuzz = 'fuzz' in base
                if half is None:
                    half = 'common'
                verdict = ''
                if half == 'x86_64':
                    verdict = 'win64' if (WIN64_REG.search(text) or HOME.search(text)) else 'neutral'
                rows.append((f.rel, ln, half, 'fuzz' if fuzz else 'live', verdict))
    import collections
    c = collections.Counter((r[3], r[2]) for r in rows)
    print('asm blocks (statements), by standing and half:')
    for k in sorted(c):
        print(f'  {k[0]:5s} {k[1]:7s} {c[k]}')
    print('  total', len(rows))
    x = [r for r in rows if r[2] == 'x86_64' and r[3] == 'live']
    print(f'live x86-64 blocks: {len(x)}; win64 {sum(1 for r in x if r[4] == "win64")}, neutral {sum(1 for r in x if r[4] == "neutral")}')
    for r in x:
        if allrows or r[4] == 'win64':
            print(f'  {r[4]:8s} {r[0]}:{r[1]}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
