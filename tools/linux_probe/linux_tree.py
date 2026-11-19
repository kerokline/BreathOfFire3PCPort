#!/usr/bin/env python
"""Which source files compile for x86-64 Linux today (docs/linux-round.md, unit LB0).

Every `.cpp` under src/ but the launcher's, the Win32 backend's and the fuzz files (`*_fuzz.cpp`: the 64-bit host
drops them, CMakeLists.txt, and a Linux build is the 64-bit host) and the Windows-only files by design
(WINDOWS_ONLY below, each with its reason) is compiled `-fsyntax-only` for
`x86_64-linux-gnu` with clang, with no stub for any Windows header: a file that needs one fails on the include,
and that is the answer. `__cdecl` is `ms_abi`, the road stage LA3 chose. Reads src/ and symbols.toml; writes
nothing into the repository but the list, and only when asked.

    python tools/linux_probe/linux_tree.py                 # the counts, and the files that fail with their reason
    python tools/linux_probe/linux_tree.py --write         # also rewrite analysis/linux_tree.tsv
    python tools/linux_probe/linux_tree.py --check         # fail when a file not in analysis/linux_tree.tsv fails
                                                           # (the list may only shrink: a listed file that now
                                                           # compiles fails the check too, until --write)
    python tools/linux_probe/linux_tree.py --cmake-list IN OUT   # the Linux build's source list (cmake_list below;
                                                                 # cmake/linux_target.cmake runs it)
    options: --sysroot DIR   a Linux root with libstdc++'s headers (on Windows: //wsl.localhost/<distribution>)
             --sdl DIR       SDL3's include folder (default: the first build*/_deps/sdl3-src/include)
             --dxvk DIR      DXVK-native's headers (docs/linux-round.md section 6, call 1): a DXVK source tree's
                             include/native (default: the first build*/_deps/dxvk-src/include/native), or an
                             installed include/dxvk
             --work DIR      where the generated headers go (default: a temporary directory)
             --jobs N

Needs `clang++` (any host: the target is named) and the headers of a Linux C++ library. On a Linux machine no
option is needed. The reasons in the list: `windows.h` and the other header names (the file includes it), `long`
(a layout static_assert of the generated header fails: `long` is 8 bytes here), `va_start` (a variadic function
under ms_abi), `name` (a name only Windows' C library has), `type` (a type that differs on LP64, `size_t` mostly).

DXVK-native's headers (--dxvk) carry a windows.h of their own, which d3d11.h includes. So that a file's own
windows.h is not answered by it, a file is compiled with them only when it fails without them on a Direct3D or DXGI
header, and then any windows.h (or other header of DXVK's windows folder) that a file outside DXVK's folders
includes is still that file's failure: `windows.h`, as without them.
"""
import argparse, collections, concurrent.futures, glob, os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LIST = 'analysis/linux_tree.tsv'
SKIP = ('src/launcher/', 'src/os/win32/')
# Files that are Windows files by design and leave the Linux source list (docs/linux-round.md, "The 38 files, staged
# as one round"): exact names, one reason each. Not compiled; --check and --write do not list them.
WINDOWS_ONLY = (
    ('src/hook/gapwatch.cpp', 'R2: the gap watch - PAGE_GUARD pages, debug registers and a vectored handler'),
    ('src/hook/write_watch.cpp', 'R2: the write watch - debug registers through a thread CONTEXT and a vectored handler'),
    ('src/hook/calltrace.cpp', 'R2: the call trace - int3 at entries, single steps and a vectored handler'),
    ('src/hook/exe_host.cpp', 'R3: the PE exe host - maps, rebases and binds bof3x.exe in the process (Engine_Main)'),
    ('src/hook/exe_icon.cpp', 'R3: the window icon through HICON, WIC and COM'),
    ('src/hook/dllmain.cpp', 'R3: the DLL entry point'),
    ('src/hook/host.cpp', "R3: tells the hosts apart by the main module's PE headers (DetectHost, VerifyImage)"),
    ('src/hook/crash_win32.cpp', 'R8: the fault catcher - a vectored handler, MiniDumpWriteDump (crash_posix.cpp is its counterpart)'),
    ('src/hook/stackwatch_win32.cpp', "R8: the main thread's TEB and guard page (stackwatch_posix.cpp refuses)"),
    ('src/game/platform/game_window_win32.cpp', 'R6: the Win32 window path and the SDL window\'s HWND steps (game_window_other.cpp is its counterpart)'),
    ('src/game/platform/fmv_play_win32.cpp', 'R6m: the movie\'s GDI picture, and Fmv_WndProc and its pump on the Win32 window path (fmv_play.cpp refuses or stops elsewhere)'),
)
# The headers DXVK-native stands in for: a file that fails on one of these first is compiled again with --dxvk.
DXVK_HEADERS = ('d3d11.h', 'dxgi.h', 'd3dcompiler.h', 'd3dcommon.h')
# DXVK-native's include/native/windows at v3.1.1: its stand-ins for Windows' own headers, which only its Direct3D
# headers may reach.
WINDOWS_STAND_INS = ('windows.h', 'windows_base.h', 'unknwn.h', 'objbase.h', 'ole2.h', 'oaidl.h', 'ocidl.h', 'rpc.h',
                     'rpcndr.h', 'poppack.h', 'pshpack4.h')
HEADER = ('# tools/linux_probe/linux_tree.py: the source files that do not compile for x86_64-linux-gnu, and why. '
          'File names and reasons only.\n#file\treason\terrors\n')


def gen_headers(work):
    gen = os.path.join(work, 'gen')
    os.makedirs(os.path.join(gen, 'bof3'), exist_ok=True)
    run = lambda *a: subprocess.run([sys.executable, *a], check=True, cwd=ROOT, capture_output=True)
    run('tools/gen_symbols.py', '--toml', 'symbols.toml', '--out', os.path.join(gen, 'bof3', 'symbols.gen.h'),
        '--bind-out', os.path.join(gen, 'bof3', 'bind.gen.h'))
    run('tools/gen_rebase.py', '--pointers', 'recipes/exe-pointers.tsv', '--out', os.path.join(gen, 'bof3', 'rebase.gen.h'))
    # CMake's configure_file of src/version.h.in (snapshot.cpp reads the tag): the numbers do not matter to a compile.
    with open(os.path.join(ROOT, 'src', 'version.h.in'), encoding='utf-8') as f:
        text = re.sub(r'@PROJECT_VERSION_(MAJOR|MINOR|PATCH)@', '0', f.read())
    text = text.replace('@PROJECT_VERSION@', '0.0.0').replace('@BOF3X_VERSION_TAG@', 'linux-probe')
    with open(os.path.join(gen, 'version.gen.h'), 'w', encoding='utf-8') as f:
        f.write(text)
    return gen


def sources():
    out = []
    windows_only = {f for f, _ in WINDOWS_ONLY}
    for d, _, fs in os.walk(os.path.join(ROOT, 'src')):
        for f in fs:
            rel = os.path.relpath(os.path.join(d, f), ROOT).replace('\\', '/')
            if f.endswith('.cpp') and not f.endswith('_fuzz.cpp') and not rel.startswith(SKIP) and rel not in windows_only:
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


def dxvk_dirs(d):
    """The include folders of DXVK-native's headers under `d`: a source tree's include/native has windows/ and
    directx/ (the mingw-directx-headers submodule); an installed include/dxvk holds both flat."""
    split = [os.path.join(d, 'windows'), os.path.join(d, 'directx')]
    if all(os.path.isfile(os.path.join(x, h)) for x, h in zip(split, ('windows.h', 'd3d11.h'))):
        return split
    if os.path.isfile(os.path.join(d, 'windows.h')) and os.path.isfile(os.path.join(d, 'd3d11.h')):
        return [d]
    sys.exit(f'linux_tree: --dxvk {d}: neither windows/windows.h and directx/d3d11.h (the include/native of a source '
             'tree, with its directx submodule) nor windows.h and d3d11.h (an installed include/dxvk)')


def clang(f, flags):
    r = subprocess.run(['clang++', '--target=x86_64-linux-gnu', '-std=c++20', '-fsyntax-only', '-ferror-limit=0', '-w',
                        '-fdeclspec', '-D__cdecl=__attribute__((ms_abi))', *flags, f],
                       capture_output=True, text=True, cwd=ROOT)
    errs = [l for l in r.stderr.splitlines() if ': error: ' in l or ': fatal error: ' in l]
    if r.returncode and not errs:
        errs = ['fatal error: ' + (r.stderr.strip().splitlines() or ['clang++ failed'])[-1]]
    return r, errs


def windows_leak(f, tree, dirs):
    """A header of DXVK's windows folder (WINDOWS_STAND_INS) that a file outside DXVK's folders includes, from
    clang's -H tree: the includer and the header, or None."""
    norm = lambda p: os.path.normcase(os.path.normpath(os.path.join(ROOT, p)))
    roots = [norm(x) + os.sep for x in dirs]
    inside = lambda p: any(norm(p).startswith(r) for r in roots)
    stack = [f]
    for line in tree.splitlines():
        m = re.match(r'^([.]+) (.+)$', line)
        if not m:
            continue
        depth, path = len(m.group(1)), m.group(2).strip()
        del stack[depth:]
        parent = stack[-1] if stack else f
        if os.path.basename(path) in WINDOWS_STAND_INS and inside(path) and not inside(parent):
            return parent, os.path.basename(path)
        stack.append(path)
    return None


def compile_one(args):
    f, flags, dxvk = args
    r, errs = clang(f, flags)
    if errs and dxvk and reason(errs) in DXVK_HEADERS:
        r, errs = clang(f, flags + [x for d in dxvk for x in ('-I', d)] + ['-H'])
        leak = windows_leak(f, r.stderr, dxvk)
        if leak:   # DXVK's windows.h answered the file's own include: the file still needs the seam
            errs = [f"{leak[0]}: fatal error: '{leak[1]}' file not found (DXVK-native's own stood in for it)"] + errs
    return f, errs


def read_list():
    path = os.path.join(ROOT, LIST)
    if not os.path.exists(path):
        return {}
    rows = [l.rstrip('\n').split('\t') for l in open(path, encoding='utf-8') if l.strip() and not l.startswith('#')]
    return {r[0]: r[1] for r in rows}


def cmake_list(src, out):
    """The Linux build's source list from a Windows one (cmake/linux_target.cmake runs this at configure time, so
    the build and this probe read one table: WINDOWS_ONLY and SKIP above). `src` holds the 64-bit Windows list, one
    path per line, in link order; `out` gets a CMake file that sets BOF3X_LINUX_SOURCES. A resource script, a fuzz
    file, a file under SKIP and a WINDOWS_ONLY file leave the list; a `<x>_win32.cpp` that leaves is replaced, in its
    place, by `<x>_posix.cpp` or `<x>_other.cpp` when one exists and the list does not hold it already (a file with
    neither has its Linux half in `<x>.cpp`, under #ifndef _WIN32). Every file of the result is one this probe
    compiles (sources()): the build cannot take a file the probe does not check."""
    windows_only = {f for f, _ in WINDOWS_ONLY}
    paths = [l.strip().replace('\\', '/') for l in open(src, encoding='utf-8') if l.strip()]
    paths = [os.path.relpath(os.path.join(ROOT, p), ROOT).replace('\\', '/') if os.path.isabs(p) else p for p in paths]
    have, out_list, dropped = set(paths), [], []
    for p in paths:
        if (p.endswith('.rc') or p.endswith('_fuzz.cpp') or p.startswith(SKIP) or p in windows_only):
            dropped.append(p)
            m = re.match(r'^(.*)_win32\.cpp$', p)
            if m:
                for alt in (m.group(1) + '_posix.cpp', m.group(1) + '_other.cpp'):
                    if os.path.isfile(os.path.join(ROOT, alt)) and alt not in have:
                        out_list.append(alt)
                        have.add(alt)
                        break
            continue
        out_list.append(p)
    probed = set(sources())
    unprobed = [p for p in out_list if p not in probed]
    if unprobed:
        sys.exit('linux_tree --cmake-list: the Linux list would hold files this probe does not compile: '
                 + ', '.join(unprobed))
    with open(out, 'w', encoding='utf-8', newline='\n') as o:
        o.write('# Written by tools/linux_probe/linux_tree.py --cmake-list at configure time: do not edit.\n')
        o.write('# Left out: ' + ' '.join(dropped) + '\n')
        o.write('set(BOF3X_LINUX_SOURCES\n' + ''.join(f'  {p}\n' for p in out_list) + ')\n')
    print(f'linux_tree --cmake-list: {len(out_list)} files for Linux, {len(dropped)} of the Windows list left out')
    return 0


def main():
    if len(sys.argv) == 4 and sys.argv[1] == '--cmake-list':
        return cmake_list(sys.argv[2], sys.argv[3])
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--sysroot')
    ap.add_argument('--sdl')
    ap.add_argument('--dxvk')
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
    dxvk_root = a.dxvk or next(iter(sorted(glob.glob(os.path.join(ROOT, 'build*', '_deps', 'dxvk-src', 'include', 'native')))), None)
    dxvk = dxvk_dirs(dxvk_root) if dxvk_root else None

    files = sources()
    gone = [f for f, _ in WINDOWS_ONLY if not os.path.isfile(os.path.join(ROOT, f))]
    if gone:
        sys.exit('linux_tree: WINDOWS_ONLY names a file that does not exist: ' + ', '.join(gone))
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        res = dict(ex.map(compile_one, [(f, flags, dxvk) for f in files]))
    bad = {f: (reason(e), len(e)) for f, e in res.items() if e}
    if len(bad) == len(files):
        sys.exit('linux_tree: no file compiled; clang++ does not find the Linux C++ headers (pass --sysroot), or is absent')
    why = collections.Counter(r for r, _ in bad.values())
    print(f'linux_tree: {len(files)} files, {len(files) - len(bad)} compile for x86_64-linux-gnu, {len(bad)} do not; '
          f'{len(WINDOWS_ONLY)} Windows-only files left out'
          + (' (no SDL3 headers found: files that include them fail on that)' if not sdl else '')
          + (' (no DXVK-native headers found: files that include d3d11.h fail on that; --dxvk)' if not dxvk else ''))
    for k, v in why.most_common():
        print(f'  {v:4d}  {k}')
    print(f'linux_tree: {len(WINDOWS_ONLY)} files are Windows-only by design and not compiled (WINDOWS_ONLY)')

    if a.write:
        with open(os.path.join(ROOT, LIST), 'w', encoding='utf-8', newline='\n') as o:
            o.write(HEADER)
            for f in sorted(bad):
                o.write(f'{f}\t{bad[f][0]}\t{bad[f][1]}\n')
        print(f'linux_tree: wrote {LIST}')
        return 0
    if a.check:
        windows_only = {f for f, _ in WINDOWS_ONLY}
        listed = {f: r for f, r in read_list().items() if f not in windows_only}   # not compiled: a row of one says nothing
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
