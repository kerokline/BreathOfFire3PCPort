#!/usr/bin/env python
"""Compile src/ files for x86-64 Linux with clang, for two questions of stage LA
(docs/linux-round.md section 4a). Needs clang++ and `python`; reads src/ and
symbols.toml only. Writes nothing into the repository (a work directory).

    python tools/linux_probe/linux_check.py stub FILE...    # LA1: what does FILE take from windows.h?
    python tools/linux_probe/linux_check.py asm FILE...     # LA3: do the asm texts assemble, and are the naked ones
                                                            #      the same with __cdecl as ms_abi?
    options: --work DIR   (default: a temporary directory)

stub: each FILE is compiled `-fsyntax-only` with an EMPTY windows.h, dinput.h, io.h and malloc.h first on the include
path. The errors name what the file takes from them. The generated symbols.gen.h has layout `static_assert`s about the
32-bit `long` of Windows (LP64 Linux makes `long` 8 bytes): they fail here, and are counted apart from the other
errors (printed as `static_asserts`; the count is one of linux-round.md's findings).

asm: each FILE is compiled to assembly (`-S -O1`) as it is and with `-D__cdecl=__attribute__((ms_abi))`, against a
small windows.h stub (the few names the files use); the generated headers' `static_assert`s are turned off, since the
stub case is about the asm. Prints per file whether either compile failed and, for
the functions whose body is asm only (`naked`), whether the emitted text is identical in the two.
"""
import argparse, os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MSABI = '-D__cdecl=__attribute__((ms_abi))'
STUB_WINDOWS = '''#pragma once
#include <cstddef>
#include <cstdint>
#include <strings.h>
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
typedef unsigned long DWORD; typedef int BOOL; typedef void* HANDLE; typedef void* LPVOID; typedef const char* LPCSTR;
typedef unsigned char BYTE; typedef unsigned short WORD; typedef DWORD* PDWORD; typedef DWORD* LPDWORD; typedef void* HMODULE;
#define WINAPI
#define CALLBACK
#define PAGE_NOACCESS 1
#define PAGE_READWRITE 4
#define PAGE_EXECUTE_READWRITE 0x40
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define MAX_PATH 260
extern "C" {
DWORD GetEnvironmentVariableA(LPCSTR, char*, DWORD);
LPVOID VirtualAlloc(LPVOID, size_t, DWORD, DWORD);
BOOL VirtualProtect(LPVOID, size_t, DWORD, DWORD*);
BOOL VirtualFree(LPVOID, size_t, DWORD);
DWORD GetLastError();
}
'''


def gen_headers(work, patch_asserts):
    gen = os.path.join(work, 'gen')
    os.makedirs(os.path.join(gen, 'bof3'), exist_ok=True)
    sym = os.path.join(gen, 'bof3', 'symbols.gen.h')
    if not os.path.exists(sym):
        subprocess.run([sys.executable, 'tools/gen_symbols.py', '--toml', 'symbols.toml', '--out', sym, '--bind-out',
                        os.path.join(gen, 'bof3', 'bind.gen.h')], check=True, cwd=ROOT, capture_output=True)
        subprocess.run([sys.executable, 'tools/gen_rebase.py', '--pointers', 'recipes/exe-pointers.tsv', '--out',
                        os.path.join(gen, 'bof3', 'rebase.gen.h')], check=True, cwd=ROOT, capture_output=True)
    if not patch_asserts:
        return gen
    gen2 = os.path.join(work, 'gen_noassert')
    os.makedirs(os.path.join(gen2, 'bof3'), exist_ok=True)
    for n in ('symbols.gen.h', 'bind.gen.h', 'rebase.gen.h'):
        s = open(os.path.join(gen, 'bof3', n)).read()
        open(os.path.join(gen2, 'bof3', n), 'w').write('#define BOF3_NOSA(...)\n' + s.replace('static_assert(', 'BOF3_NOSA('))
    return gen2


def clang(args, cwd=ROOT):
    # -w: only errors answer the questions. -ferror-limit=0: the counts are complete. -fdeclspec: some files use __declspec.
    return subprocess.run(['clang++', '-target', 'x86_64-linux-gnu', '-std=c++20', '-ferror-limit=0', '-w', '-fdeclspec', *args],
                          capture_output=True, text=True, cwd=cwd)


def mode_stub(work, files):
    stub = os.path.join(work, 'stub_empty')
    os.makedirs(stub, exist_ok=True)
    for n in ('windows.h', 'dinput.h', 'io.h', 'malloc.h'):
        open(os.path.join(stub, n), 'w').close()
    gen = gen_headers(work, False)
    for f in files:
        r = clang(['-fsyntax-only', '-I', stub, '-I', 'src', '-I', 'third_party', '-I', gen, f])
        errs = re.findall(r'error: (.*)', r.stderr)
        sa = sum(1 for e in errs if e.startswith('static assertion'))
        other = [e for e in errs if not e.startswith('static assertion')]
        names = {}
        for e in other:
            for m in re.finditer(r"'([A-Za-z_][\w:]*)'", e):
                names[m.group(1)] = names.get(m.group(1), 0) + 1
        top = sorted(names.items(), key=lambda kv: -kv[1])[:6]
        print(f'{f}\tstatic_asserts {sa}\tother_errors {len(other)}\t{dict(top)}')


def funcs(path):
    out, cur, buf = {}, None, []
    for line in open(path):
        m = re.match(r'^(\w+):\s*(#.*)?$', line)
        if m and not line.startswith('.'):
            cur, buf = m.group(1), []
            continue
        m = re.match(r'^\s*\.size\s+(\w+),', line)
        if m and cur == m.group(1):
            out[cur] = [l.strip() for l in buf if l.strip() and not l.strip().startswith(('.cfi', '.p2align', '.type', '.globl', '.Lfunc', '#'))]
            cur = None
            continue
        if cur:
            buf.append(line)
    return out


def mode_asm(work, files):
    stub = os.path.join(work, 'stub_win')
    os.makedirs(stub, exist_ok=True)
    open(os.path.join(stub, 'windows.h'), 'w').write(STUB_WINDOWS)
    for n in ('dinput.h', 'io.h'):
        open(os.path.join(stub, n), 'w').close()
    gen = gen_headers(work, True)
    for f in files:
        res = {}
        for mode, defs in (('asis', []), ('msabi', [MSABI])):
            out = os.path.join(work, f'{mode}_{os.path.basename(f)}.s')
            r = clang(['-S', '-O1', *defs, '-I', stub, '-I', 'src', '-I', 'third_party', '-I', gen, f, '-o', out])
            res[mode] = (r.returncode, out, [l for l in r.stderr.split('\n') if 'error' in l])
        if res['asis'][0] or res['msabi'][0]:
            print(f'{f}\tCOMPILE FAILED asis={res["asis"][0]} msabi={res["msabi"][0]}\t{(res["asis"][2] or res["msabi"][2])[:1]}')
            continue
        a, m = funcs(res['asis'][1]), funcs(res['msabi'][1])
        # a naked function's emitted text is its asm alone: no prologue, so the text equals the asm statement
        names = re.findall(r'__attribute__\(\(naked\)\)[^\n(]*?\b(\w+)\s*\(', open(os.path.join(ROOT, f)).read() +
                           open(os.path.join(ROOT, 'src/game/task_switch_x86_64.h')).read())   # task.cpp's naked entries live there
        naked = [n for n in names if n in a]
        same = [n for n in naked if a[n] == m.get(n)]
        diff = [n for n in naked if a[n] != m.get(n)]
        print(f'{f}\tassembles as is and as ms_abi\tnaked {len(naked)}: identical {len(same)}, different {len(diff)} {diff}\t'
              f'functions {len(a)}, emitted differently under ms_abi {sum(1 for n in a if a[n] != m.get(n))}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('mode', choices=('stub', 'asm'))
    ap.add_argument('files', nargs='+')
    ap.add_argument('--work')
    a = ap.parse_args()
    work = a.work or tempfile.mkdtemp(prefix='bof3_linux_check_')
    os.makedirs(work, exist_ok=True)
    (mode_stub if a.mode == 'stub' else mode_asm)(work, a.files)
    return 0


if __name__ == '__main__':
    sys.exit(main())
