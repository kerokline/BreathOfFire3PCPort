#!/usr/bin/env python
"""What the Linux ELF's link still lacks (docs/linux-round.md, stages LC and LD, group C1).

Links the `bof3x` target of a configured linux-x86_64 build (--preset linux-x86_64; cmake/linux_target.cmake),
reads every undefined name the linker reports, finds the objects that want each (llvm-nm), and sorts each into a
kind, the work order of wave B:

    windows  a Windows API: a name one of llvm-mingw's import libraries of a system DLL defines
    crt      Windows' C runtime: msvcrt / ucrt / mingw's own runtime libraries define it, glibc has no such name
    ours     ours, not yet written for Linux: the Windows 64-bit build defines it in one of our objects (the file
             that does is named; mostly a WINDOWS_ONLY file of tools/linux_probe/linux_tree.py)
    sdl      an SDL_ name: SDL3 is linked, so a subsystem this build left out
    dxvk     Direct3D 11 / DXGI: DXVK-native's libraries (cmake/dxvk_native.cmake), built on a Linux host only
    other    none of these: read by hand

    python tools/linux_probe/linux_link.py [--build build-linux-x86_64] [--windows build-x86_64]
                                           [--out analysis/linux_link.tsv]

Needs a configured Linux build (BOF3X_LINUX_SYSROOT on Windows, cmake/x86_64-linux.cmake), the Windows 64-bit
build's objects (--preset x86_64, for the `ours` kind), and llvm-nm beside clang. Reads objects and import
libraries only; writes the list. Names and file names only: no game data.
"""
import argparse, collections, os, re, shutil, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HEADER = ('# tools/linux_probe/linux_link.py: the names the linux-x86_64 link leaves undefined, the kind of each, '
          'and the objects that want it. Names and file names only.\n#name\tkind\tdetail\twanted_by\n')
CRT_LIBS = re.compile(r'^lib(msvcrt|msvcr\d+|ucrt\w*|api-ms-win-crt-\w+|mingwex|mingw32|moldname|mingwthrd|'
                      r'pthread|winpthread|gcc\w*|c\+\+\w*|unwind)\.a$', re.I)
DXVK = re.compile(r'^(D3D11\w*|D3D10\w*|CreateDXGIFactory\w*|DXGI\w*|D3DCompile\w*|D3DReflect\w*|IID_\w+|'
                  r'DXVK\w*)$')


def tool(name):
    clang = shutil.which('clang++') or shutil.which('clang')
    for d in ([os.path.dirname(clang)] if clang else []) + os.environ.get('PATH', '').split(os.pathsep):
        for n in (name + '.exe', name):
            if os.path.isfile(os.path.join(d, n)):
                return os.path.join(d, n)
    sys.exit(f'linux_link: {name} not found beside clang++ or on PATH')


def nm(nmtool, paths, *flags):
    """{file: [(kind, name)]} for objects and archives (an archive's members are reported under the archive)."""
    out = collections.defaultdict(list)
    for i in range(0, len(paths), 40):
        r = subprocess.run([nmtool, '-C', '-A', *flags, *paths[i:i + 40]], capture_output=True, text=True,
                           errors='replace')
        for line in r.stdout.splitlines():
            # "<file>[:<member>]: <addr> <T> <name>" or, for undefined, "<file>: U <name>" with padding
            m = re.match(r'^(.*?\.(?:o|obj|a))(?::[^:]+\.(?:o|obj))?:\s+(?:[0-9a-fA-F]+\s+)?([A-Za-z?])\s+(.+)$', line)
            if m:
                out[m.group(1)].append((m.group(2), m.group(3)))
    return out


def objects(build, sub):
    found = []
    for d, _, fs in os.walk(os.path.join(build, sub)):
        found += [os.path.join(d, f) for f in fs if f.endswith(('.o', '.obj'))]
    return sorted(found)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--build', default=os.path.join(ROOT, 'build-linux-x86_64'))
    ap.add_argument('--windows', default=os.path.join(ROOT, 'build-x86_64'))
    ap.add_argument('--out')
    ap.add_argument('--log', help='read this link log instead of running the link')
    a = ap.parse_args()
    nmtool = tool('llvm-nm')

    if a.log:
        text = open(a.log, encoding='utf-8', errors='replace').read()
    else:
        r = subprocess.run(['cmake', '--build', a.build, '--target', 'bof3x'], capture_output=True, text=True,
                           errors='replace')
        text = r.stdout + r.stderr
        if r.returncode == 0:
            print('linux_link: the link succeeded: no undefined name')
    names = sorted(set(re.findall(r'undefined symbol: (.+?)\s*$', text, re.M)))
    if not names and 'error:' in text:
        sys.exit('linux_link: the build failed before the link (no "undefined symbol" line):\n'
                 + '\n'.join(l for l in text.splitlines() if 'error' in l)[:4000])

    # Who wants each name: every object of the link that holds it undefined.
    ours = objects(a.build, os.path.join('CMakeFiles')) + objects(a.build, os.path.join('src'))
    sdl_lib = [os.path.join(d, f) for d, _, fs in os.walk(os.path.join(a.build, '_deps', 'sdl3-build'))
               for f in fs if f.endswith('.a')]
    wanted = collections.defaultdict(set)
    for f, syms in nm(nmtool, ours + sdl_lib, '-u').items():
        rel = os.path.relpath(f, a.build).replace('\\', '/')
        rel = re.sub(r'\.o$', '', re.sub(r'^CMakeFiles/[^/]+\.dir/', '', rel))
        for k, s in syms:
            wanted[s].add(rel)
    wanted['main'].add('crt1.o (_start)')

    # What the Windows build defines: the DLL's objects (ours) and the import libraries (windows, crt). A file the
    # Linux build compiles too has the name in its Windows half alone (#ifdef _WIN32).
    linux_list = set()
    gen = os.path.join(a.build, 'gen', 'linux_sources.gen.cmake')
    if os.path.isfile(gen):
        linux_list = set(re.findall(r'^\s+(src/\S+)$', open(gen, encoding='utf-8').read(), re.M))
    win_defs = {}
    for f, syms in nm(nmtool, objects(a.windows, os.path.join('CMakeFiles', 'bof3x.dir')), '--defined-only').items():
        rel = re.sub(r'^CMakeFiles/[^/]+\.dir/', '', os.path.relpath(f, a.windows).replace('\\', '/'))
        rel = re.sub(r'\.obj$', '', rel)
        for k, s in syms:
            win_defs.setdefault(s, rel)
    mingw = os.path.join(os.path.dirname(os.path.dirname(tool('clang++'))), 'x86_64-w64-mingw32', 'lib')
    libs = sorted(os.path.join(mingw, f) for f in os.listdir(mingw) if f.endswith('.a'))
    lib_defs = {}
    for f, syms in nm(nmtool, libs, '--defined-only').items():
        base = os.path.basename(f)
        for k, s in syms:
            s = s[6:] if s.startswith('__imp_') else s
            lib_defs.setdefault(s, base)

    rows, kinds = [], collections.Counter()
    for n in names:
        if n.startswith('SDL_'):
            kind, detail = 'sdl', ''
        elif DXVK.match(n):
            kind, detail = 'dxvk', ''
        elif n == 'main':
            kind, detail = 'ours', 'the start-up (exe_host.cpp is Windows-only; C2 writes the Linux one)'
        elif n in win_defs:
            kind, detail = 'ours', win_defs[n] + (' (its Windows half)' if win_defs[n] in linux_list else '')
        elif n in lib_defs:
            kind, detail = ('crt' if CRT_LIBS.match(lib_defs[n]) else 'windows'), lib_defs[n]
        elif n == '__ImageBase':
            kind, detail = 'windows', "the PE linker's own symbol at the image's first byte (image_base.h)"
        else:
            kind, detail = 'other', ''
        kinds[kind] += 1
        rows.append((n, kind, detail, ';'.join(sorted(wanted.get(n, ())))))

    print(f'linux_link: {len(names)} undefined names')
    for k, v in kinds.most_common():
        print(f'  {v:5d}  {k}')
    if a.out:
        with open(a.out, 'w', encoding='utf-8', newline='\n') as o:
            o.write(HEADER)
            for r in sorted(rows, key=lambda r: (r[1], r[0])):
                o.write('\t'.join(r) + '\n')
        print(f'linux_link: wrote {a.out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
