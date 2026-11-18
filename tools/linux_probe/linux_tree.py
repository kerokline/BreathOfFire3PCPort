#!/usr/bin/env python
"""Which source files compile for x86-64 Linux today (docs/linux-round.md, unit LB0).

Every `.cpp` under src/ but the launcher's, the Win32 backend's and the fuzz files (`*_fuzz.cpp`: the 64-bit host
drops them, CMakeLists.txt, and a Linux build is the 64-bit host) is compiled `-fsyntax-only` for
`x86_64-linux-gnu` with clang, with no stub for any Windows header: a file that needs one fails on the include,
and that is the answer. `__cdecl` is `ms_abi`, the road stage LA3 chose. Reads src/ and symbols.toml; writes
nothing into the repository but the list, and only when asked.

    python tools/linux_probe/linux_tree.py                 # the counts, and the files that fail with their reason
    python tools/linux_probe/linux_tree.py --write         # also rewrite analysis/linux_tree.tsv
    python tools/linux_probe/linux_tree.py --check         # fail when a file not in analysis/linux_tree.tsv fails
                                                           # (the list may only shrink: a listed file that now
                                                           # compiles fails the check too, until --write)
    options: --sysroot DIR   a Linux root with libstdc++'s headers (on Windows: //wsl.localhost/<distribution>)
             --sdl DIR       SDL3's include folder (default: the first build*/_deps/sdl3-src/include)
             --work DIR      where the generated headers go (default: a temporary directory)
             --jobs N

Needs `clang++` (any host: the target is named) and the headers of a Linux C++ library. On a Linux machine no
option is needed. The reasons in the list: `windows.h` and the other header names (the file includes it), `long`
(a layout static_assert of the generated header fails: `long` is 8 bytes here), `va_start` (a variadic function
under ms_abi), `name` (a name only Windows' C library has), `type` (a type that differs on LP64, `size_t` mostly).
"""
import argparse, collections, concurrent.futures, glob, os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LIST = 'analysis/linux_tree.tsv'
SKIP = ('src/launcher/', 'src/os/win32/')
# Files that are Windows by design and leave the Linux source list, each beside its Linux counterpart.
WINDOWS_ONLY = (
    'src/hook/crash_win32.cpp',       # the fault catcher: a vectored handler, MiniDumpWriteDump (crash_posix.cpp)
    'src/hook/stackwatch_win32.cpp',  # the main thread's TEB and guard page (stackwatch_posix.cpp refuses)
)
HEADER = ('# tools/linux_probe/linux_tree.py: the source files that do not compile for x86_64-linux-gnu, and why. '
          'File names and reasons only.\n#file\treason\terrors\n')


def gen_headers(work):
    gen = os.path.join(work, 'gen')
    os.makedirs(os.path.join(gen, 'bof3'), exist_ok=True)
    run = lambda *a: subprocess.run([sys.executable, *a], check=True, cwd=ROOT, capture_output=True)
    run('tools/gen_symbols.py', '--toml', 'symbols.toml', '--out', os.path.join(gen, 'bof3', 'symbols.gen.h'),
        '--bind-out', os.path.join(gen, 'bof3', 'bind.gen.h'))
    run('tools/gen_rebase.py', '--pointers', 'recipes/exe-pointers.tsv', '--out', os.path.join(gen, 'bof3', 'rebase.gen.h'))
    return gen


def sources():
    out = []
    for d, _, fs in os.walk(os.path.join(ROOT, 'src')):
        for f in fs:
            rel = os.path.relpath(os.path.join(d, f), ROOT).replace('\\', '/')
            if f.endswith('.cpp') and not f.endswith('_fuzz.cpp') and not rel.startswith(SKIP) and rel not in WINDOWS_ONLY:
                out.append(rel)
    return sorted(out)


def reason(errs):
    """One word for why a file fails: the first cause, in the order a port meets them."""
    for e in errs:
        m = re.search(r"'([\w./]+\.h)' file not found", e)
        if m:
            return m.group(1)
    if any('static assertion failed' in e for e in errs):
        return 'long'
    if any("'va_start' used in Win64 ABI function" in e for e in errs):
        return 'va_start'
    if any('use of undeclared identifier' in e for e in errs):
        return 'name'
    return 'type'


def compile_one(args):
    f, flags = args
    r = subprocess.run(['clang++', '--target=x86_64-linux-gnu', '-std=c++20', '-fsyntax-only', '-ferror-limit=0', '-w',
                        '-fdeclspec', '-D__cdecl=__attribute__((ms_abi))', *flags, f],
                       capture_output=True, text=True, cwd=ROOT)
    errs = [l for l in r.stderr.splitlines() if ': error: ' in l or ': fatal error: ' in l]
    if r.returncode and not errs:
        errs = ['fatal error: ' + (r.stderr.strip().splitlines() or ['clang++ failed'])[-1]]
    return f, errs


def read_list():
    path = os.path.join(ROOT, LIST)
    if not os.path.exists(path):
        return {}
    rows = [l.rstrip('\n').split('\t') for l in open(path, encoding='utf-8') if l.strip() and not l.startswith('#')]
    return {r[0]: r[1] for r in rows}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--sysroot')
    ap.add_argument('--sdl')
    ap.add_argument('--work')
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()

    work = a.work or tempfile.mkdtemp(prefix='linux_tree_')
    sdl = a.sdl or next(iter(sorted(glob.glob(os.path.join(ROOT, 'build*', '_deps', 'sdl3-src', 'include')))), None)
    flags = ['-I', 'src', '-I', 'third_party', '-I', gen_headers(work)]
    if sdl:
        flags += ['-I', sdl]
        rev = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(sdl))), '_deps', 'sdl3-build', 'include-revision')
        if os.path.isdir(rev):
            flags += ['-I', rev]
    if a.sysroot:
        flags.append('--sysroot=' + a.sysroot)

    files = sources()
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        res = dict(ex.map(compile_one, [(f, flags) for f in files]))
    bad = {f: (reason(e), len(e)) for f, e in res.items() if e}
    if len(bad) == len(files):
        sys.exit('linux_tree: no file compiled; clang++ does not find the Linux C++ headers (pass --sysroot), or is absent')
    why = collections.Counter(r for r, _ in bad.values())
    print(f'linux_tree: {len(files)} files, {len(files) - len(bad)} compile for x86_64-linux-gnu, {len(bad)} do not; '
          f'{len(WINDOWS_ONLY)} Windows-only files left out'
          + (' (no SDL3 headers found: files that include them fail on that)' if not sdl else ''))
    for k, v in why.most_common():
        print(f'  {v:4d}  {k}')

    if a.write:
        with open(os.path.join(ROOT, LIST), 'w', encoding='utf-8', newline='\n') as o:
            o.write(HEADER)
            for f in sorted(bad):
                o.write(f'{f}\t{bad[f][0]}\t{bad[f][1]}\n')
        print(f'linux_tree: wrote {LIST}')
        return 0
    if a.check:
        listed = read_list()
        new = sorted(f for f in bad if f not in listed)
        fixed = sorted(f for f in listed if f not in bad)
        for f in new:
            print(f'{f}: does not compile for Linux ({bad[f][0]}) and is not in {LIST}')
            for e in res[f][:3]:
                print('    ' + e)
        for f in fixed:
            print(f'{f}: listed in {LIST} but compiles for Linux now: remove the row (--write)')
        if new or fixed:
            return 1
        print(f'linux_tree --check: ok ({len(bad)} files wait)')
        return 0
    for f in sorted(bad):
        print(f'{f}\t{bad[f][0]}\t{bad[f][1]}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
