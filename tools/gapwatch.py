#!/usr/bin/env python
"""The gap watch's files (BOF3X_GAPWATCH, src/hook/gapwatch.cpp; docs/install-gap-reads.md), read together.

    python tools/gapwatch.py report --dll <launcher copy>/bof3x.dll FILE_OR_DIR... [--tsv OUT]

Each file is one run: a build (the disc-built image's), a route, every watched range
with whether it was touched and, for its first access, the reader's address and the
return addresses on its stack (bof3x.dll+offset). The report joins the runs by range:
which builds and routes touched it, and the reader named - llvm-symbolizer on the DLL
the runs used (its function, the source line), and symbols.toml's `pc` for a function
that is the owned twin of one of Capcom's. Addresses, classes, symbol names and counts
only (CLAUDE.md rule 1): the watch never reads a value.

    python tools/gapwatch.py pages FILE...     # the per-page fallback: faults, parks, busiest readers
"""
import argparse
import glob
import os
import re
import shutil
import struct
import subprocess
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_run(path):
    run = {'path': path, 'ranges': {}, 'pages': [], 'header': []}
    with open(path, encoding='utf-8', errors='replace') as fh:
        for line in fh:
            line = line.rstrip('\r\n')
            if line.startswith('#'):
                run['header'].append(line)
                m = re.search(r'build (\S+), route (\S+), classes (\S+), budget (\d+)', line)
                if m:
                    run['build'], run['route'], run['classes'], run['budget'] = m.group(1), m.group(2), m.group(3), int(m.group(4))
                m = re.search(r'latches (\d+), Frame_Counter (\d+)', line)
                if m:
                    run['latches'], run['frames'] = int(m.group(1)), int(m.group(2))
                m = re.search(r'([\d.]+) ms a latch', line)
                if m:
                    run['ms'] = float(m.group(1))
                m = re.search(r'(\d+) guard faults, (\d+) steps', line)
                if m:
                    run['faults'], run['steps'] = int(m.group(1)), int(m.group(2))
                continue
            f = line.split('\t')
            if f[0] == 'range' and f[1] != 'lo':
                r = dict(zip(['lo', 'hi', 'how', 'symbol', 'read', 'frame', 'rframe', 'area', 'via', 'thread', 'reader',
                              'callers', 'reads', 'rbw', 'writes', 'wframe', 'writer', 'edge', 'edge_reader', 'dr_hits',
                              'dr_latches'], f[1:]))
                run['ranges'][(r['lo'], r['hi'])] = r
            elif f[0] == 'page' and f[1] != 'base':
                run['pages'].append(dict(zip(['base', 'ranges', 'faults', 'range_faults', 'parks', 'readers'], f[1:])))
    if 'build' not in run:
        sys.exit(f'{path}: not a gap watch file (no header)')
    return run


def files_of(args):
    out = []
    for a in args:
        if os.path.isdir(a):
            out += sorted(glob.glob(os.path.join(a, '**', 'gapwatch_*.tsv'), recursive=True))
        else:
            out += sorted(glob.glob(a))
    if not out:
        sys.exit('no gap watch files')
    return out


def image_base(dll):
    with open(dll, 'rb') as fh:
        data = fh.read(4096)
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    return struct.unpack_from('<I', data, pe + 0x34)[0]


class Symbolizer:
    def __init__(self, dll):
        self.dll = dll
        self.base = image_base(dll)
        self.tool = shutil.which('llvm-symbolizer') or shutil.which('llvm-symbolizer.exe')
        if not self.tool:
            sys.exit('llvm-symbolizer is not on the PATH (the llvm-mingw toolchain has it)')
        self.cache = {}

    def names(self, refs):
        todo = sorted({r for r in refs if r.startswith('bof3x.dll+') and r not in self.cache})
        if todo:
            addrs = [hex(self.base + int(r.split('+')[1], 16)) for r in todo]
            out = subprocess.run([self.tool, f'--obj={self.dll}', '--functions=linkage', '--demangle', '--no-inlines'],
                                 input='\n'.join(addrs) + '\n', capture_output=True, text=True, check=True).stdout
            blocks = [b for b in out.split('\n\n') if b.strip()]
            for r, b in zip(todo, blocks):
                lines = b.strip().splitlines()
                fn = lines[0] if lines else '?'
                src = lines[1] if len(lines) > 1 else ''
                src = re.sub(r'^.*[\\/]src[\\/]', 'src/', src).replace('\\', '/')
                self.cache[r] = (fn, src)
        return {r: self.cache.get(r, (r, '')) for r in refs}


def capcom_names():
    with open(os.path.join(ROOT, 'symbols.toml'), 'rb') as fh:
        doc = tomllib.load(fh)
    return {f['name']: f['pc'] for f in doc.get('func', []) if 'name' in f and 'pc' in f}


def short(fn, pcs):
    base = re.sub(r'\(.*$', '', fn)
    base = base.split('::')[-1]
    pc = pcs.get(base)
    return f'{base} (0x{pc:06X})' if pc else base


def cmd_report(a):
    runs = [read_run(p) for p in files_of(a.files)]
    sym = Symbolizer(a.dll) if a.dll else None
    pcs = capcom_names()
    refs = set()
    for run in runs:
        for r in run['ranges'].values():
            refs.add(r['reader'])
            refs.update(r['callers'].split(','))
            refs.add(r['edge_reader'])
            refs.add(r['writer'])
    names = sym.names(refs) if sym else {}

    def nm(ref):
        if ref in ('-', ''):
            return '-'
        fn, src = names.get(ref, (ref, ''))
        return short(fn, pcs) if fn and fn != '??' else ref

    keys = sorted({k for run in runs for k in run['ranges']}, key=lambda k: int(k[0], 16))
    print(f'# {len(runs)} runs')
    for run in runs:
        print(f"#   {run['build']:10} {run['route']:28} classes {run['classes']}, {run.get('latches', 0)} latches, "
              f"{run.get('ms', 0):.2f} ms a latch, {run.get('faults', 0)} faults, "
              f"{sum(1 for r in run['ranges'].values() if r['read'] == 'yes')} of {len(run['ranges'])} ranges read")
    rows = []
    for k in keys:
        seen = [(run, run['ranges'][k]) for run in runs if k in run['ranges']]
        hits = [(run, r) for run, r in seen if r['read'] == 'yes']
        r0 = seen[0][1]
        builds = sorted({run['build'] for run, _ in seen})
        hit_builds = sorted({run['build'] for run, _ in hits})
        routes = sorted({run['route'] for run, _ in hits})
        readers = sorted({nm(r['reader']) for _, r in hits})
        callers = sorted({nm(c) for _, r in hits for c in r['callers'].split(',')[:2] if c != '-'})
        vias = sorted({r['via'] for _, r in hits})
        reads = sum(int(r['reads']) for _, r in seen)
        rbw = sum(int(r['rbw']) for _, r in seen)
        rbw_builds = sorted({run['build'] for run, r in seen if int(r['rbw'])})
        writes = sum(int(r['writes']) for _, r in seen)
        writers = sorted({nm(r['writer']) for _, r in seen if r['writer'] != '-'})
        wframes = [int(r['wframe']) for _, r in seen if r['wframe'] != '-']
        dr_hits = sum(int(r['dr_hits']) for _, r in seen)
        dr_latches = sum(int(r['dr_latches']) for _, r in seen)
        edge = sum(int(r['edge']) for _, r in seen)
        edge_readers = sorted({nm(r['edge_reader']) for _, r in seen if r['edge_reader'] != '-'})
        first = min(hits, key=lambda h: int(h[1]['frame'])) if hits else None
        rows.append({
            'lo': k[0], 'hi': k[1], 'how': r0['how'], 'symbol': r0['symbol'],
            'read': 'yes' if hits else 'no', 'builds': ','.join(builds), 'hit_builds': ','.join(hit_builds),
            'routes': ','.join(routes), 'via': ','.join(vias), 'readers': '; '.join(readers),
            'callers': '; '.join(callers), 'reads': reads, 'rbw': rbw, 'rbw_builds': ','.join(rbw_builds),
            'writes': writes, 'writers': '; '.join(writers), 'first_write': min(wframes) if wframes else '-',
            'dr_hits': dr_hits, 'dr_latches': dr_latches, 'edge': edge,
            'edge_readers': '; '.join(edge_readers),
            'first': f"{first[0]['route']}@{first[1]['frame']} area {first[1]['area']}" if first else '-'})
    cols = ['lo', 'hi', 'how', 'symbol', 'read', 'hit_builds', 'routes', 'via', 'readers', 'callers', 'reads', 'rbw',
            'rbw_builds', 'writes', 'writers', 'first_write', 'dr_hits', 'dr_latches', 'edge', 'edge_readers', 'first',
            'builds']
    text = '\t'.join(cols) + '\n' + ''.join('\t'.join(str(r[c]) for c in cols) + '\n' for r in rows)
    if a.tsv:
        with open(a.tsv, 'w', encoding='utf-8', newline='\n') as fh:
            fh.write(text)
    if not a.quiet:
        for r in rows:
            if r['read'] == 'yes' or r['writes'] or a.all:
                print(f"{r['lo']}..{r['hi']} {r['how']:8} {r['symbol']:34} read {r['read']:3} {r['hit_builds']:30} "
                      f"{r['routes']:40} {r['via']:8} {r['readers']} <- {r['callers']} (reads {r['reads']}, before a "
                      f"write {r['rbw']} {r['rbw_builds']}; writes {r['writes']} from frame {r['first_write']} by "
                      f"{r['writers']}; dr {r['dr_hits']} hits over {r['dr_latches']} latches; edge {r['edge']} "
                      f"{r['edge_readers']})")
    print(f"# {sum(1 for r in rows if r['read'] == 'yes')} of {len(rows)} ranges read in some run, "
          f"{sum(1 for r in rows if r['rbw'])} of them before any write")


def cmd_pages(a):
    runs = [read_run(p) for p in files_of(a.files)]
    sym = Symbolizer(a.dll) if a.dll else None
    pcs = capcom_names()
    for run in runs:
        print(f"# {run['build']} {run['route']}: {run.get('latches', 0)} latches, {run.get('ms', 0):.2f} ms a latch")
        for p in sorted(run['pages'], key=lambda p: -int(p['faults'])):
            if not int(p['faults']):
                continue
            readers = [x.rsplit(':', 1) for x in p['readers'].split(',') if x]
            names = sym.names([r for r, _ in readers]) if sym else {}
            top = ', '.join(f"{short(names.get(r, (r, ''))[0], pcs)}:{n}" for r, n in readers[:4])
            print(f"  {p['base']} ranges {p['ranges']:>3} faults {p['faults']:>8} in ranges {p['range_faults']:>6} "
                  f"parked {p['parks']:>6} latches  {top}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('report')
    r.add_argument('files', nargs='+')
    r.add_argument('--dll', help='the bof3x.dll the runs used (for the readers\' names)')
    r.add_argument('--tsv', help='write the joined table here')
    r.add_argument('--all', action='store_true', help='print the untouched ranges too')
    r.add_argument('--quiet', action='store_true')
    p = sub.add_parser('pages')
    p.add_argument('files', nargs='+')
    p.add_argument('--dll')
    a = ap.parse_args()
    {'report': cmd_report, 'pages': cmd_pages}[a.cmd](a)


if __name__ == '__main__':
    main()
