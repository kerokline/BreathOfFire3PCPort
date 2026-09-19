#!/usr/bin/env python
"""Read a BOF3.exe minidump: what faulted, where, and what memory held.

    python tools/crash_report.py                       # newest build/bof3x.crash-*.dmp
    python tools/crash_report.py path/to/file.dmp
    python tools/crash_report.py DUMP --read 0x903880 0x120   # hex dump
    python tools/crash_report.py DUMP --u32 0x929EDC 0x937F84 # dwords

Works on the dumps the injected DLL writes (src/hook/crash.cpp) and on the
ones Windows Error Reporting leaves in %LOCALAPPDATA%/CrashDumps. Addresses in
BOF3.exe are named from symbols.toml, else given as the nearest function entry
of analysis/pc_funcs.json plus an offset.

A dump is game-derived: it holds the game's code and data. Keep it local
(CLAUDE.md rule 1).
"""
import argparse, bisect, glob, json, os, struct, sys, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODES = {0xC0000005: 'access violation', 0xC0000006: 'in-page error',
         0xC000001D: 'illegal instruction', 0xC0000096: 'privileged instruction',
         0xC00000FD: 'stack overflow', 0xC0000094: 'integer divide by zero',
         0x80000003: 'breakpoint', 0xC0000409: 'stack buffer overrun (fail-fast)'}
REGS = ['edi', 'esi', 'ebx', 'edx', 'ecx', 'eax', 'ebp', 'eip', 'cs', 'efl', 'esp']


class Dump:
    def __init__(self, path):
        self.d = d = open(path, 'rb').read()
        sig, _, n, rva = struct.unpack_from('<IIII', d, 0)
        if sig != 0x504D444D:
            sys.exit(f'{path}: not a minidump')
        self.regions, self.modules, self.exc = [], [], None
        for i in range(n):
            kind, _, r = struct.unpack_from('<III', d, rva + 12 * i)
            if kind == 9:                                   # Memory64ListStream
                cnt, off = struct.unpack_from('<QQ', d, r)
                for k in range(cnt):
                    a, s = struct.unpack_from('<QQ', d, r + 16 + 16 * k)
                    self.regions.append((a, s, off))
                    off += s
            elif kind == 5:                                 # MemoryListStream
                cnt, = struct.unpack_from('<I', d, r)
                for k in range(cnt):
                    a, s, o = struct.unpack_from('<QII', d, r + 4 + 16 * k)
                    self.regions.append((a, s, o))
            elif kind == 4:                                 # ModuleListStream
                cnt, = struct.unpack_from('<I', d, r)
                for k in range(cnt):
                    base, size, _, _, name_rva = struct.unpack_from('<QIIII', d, r + 4 + 108 * k)
                    ln, = struct.unpack_from('<I', d, name_rva)
                    name = d[name_rva + 4:name_rva + 4 + ln].decode('utf-16-le')
                    self.modules.append((base, size, os.path.basename(name)))
            elif kind == 6:                                 # ExceptionStream
                tid, _, code, _, _, addr, nparm, _ = struct.unpack_from('<IIIIQQII', d, r)
                params = struct.unpack_from('<15Q', d, r + 40)[:nparm]
                _, ctx = struct.unpack_from('<II', d, r + 8 + 152)
                regs = dict(zip(REGS, struct.unpack_from('<11I', d, ctx + 0x9C)))
                self.exc = dict(thread=tid, code=code, addr=addr, params=params, regs=regs)

    def read(self, a, n):
        for b, s, o in self.regions:
            if b <= a and a + n <= b + s:
                return self.d[o + a - b:o + a - b + n]
        return None

    def module(self, a):
        for base, size, name in self.modules:
            if base <= a < base + size:
                return name, a - base
        return None


class Names:
    def __init__(self):
        self.named, self.entries = {}, []
        try:
            t = tomllib.load(open(os.path.join(ROOT, 'symbols.toml'), 'rb'))
            self.named = {f['pc']: f['name'] for f in t.get('func', []) if 'pc' in f}
        except OSError:
            pass
        try:
            j = json.load(open(os.path.join(ROOT, 'analysis', 'pc_funcs.json')))
            self.entries = sorted(f['entry'] for f in j['functions'])
            self.lo = int(j['text']['va'], 16)
            self.hi = self.lo + j['text']['size']
        except OSError:
            self.lo = self.hi = 0

    def __call__(self, a):
        if not self.lo <= a < self.hi:
            return None
        e = self.entries[bisect.bisect_right(self.entries, a) - 1]
        label = self.named.get(e, f'0x{e:08X}')
        return label if a == e else f'{label}+0x{a - e:X}'


def exe_bytes(a, n, _cache={}):
    """Code the dump left out (WER dumps carry almost no .text): take it from
    the player's BOF3.exe, which is /FIXED at its preferred base."""
    if 'd' not in _cache:
        try:
            d = open(os.path.join(ROOT, 'bof3', 'BOF3.exe'), 'rb').read()
        except OSError:
            d = b''
        _cache['d'], _cache['secs'] = d, []
        if d:
            pe = struct.unpack_from('<I', d, 0x3C)[0]
            nsec, = struct.unpack_from('<H', d, pe + 6)
            optsz, = struct.unpack_from('<H', d, pe + 20)
            base, = struct.unpack_from('<I', d, pe + 24 + 28)
            for i in range(nsec):
                vsz, va, rsz, raw = struct.unpack_from('<IIII', d, pe + 24 + optsz + i * 40 + 8)
                _cache['secs'].append((base + va, rsz, raw))
    for va, rsz, raw in _cache['secs']:
        if va <= a and a + n <= va + rsz:
            return _cache['d'][raw + a - va:raw + a - va + n]
    return None


def after_call(dump, v):
    b = dump.read(v - 6, 6) or exe_bytes(v - 6, 6)
    return bool(b) and (b[1] == 0xE8 or b[4] == 0xFF or b[3] == 0xFF or b[0] == 0xFF)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('dump', nargs='?')
    ap.add_argument('--read', nargs=2, metavar=('ADDR', 'LEN'))
    ap.add_argument('--u32', nargs='+', metavar='ADDR')
    ap.add_argument('--stack', type=lambda x: int(x, 0), default=0x2000, help='bytes of stack to scan')
    a = ap.parse_args()
    path = a.dump
    if not path:
        found = sorted(glob.glob(os.path.join(ROOT, 'build', 'bof3x.crash-*.dmp')), key=os.path.getmtime)
        if not found:
            sys.exit('no build/bof3x.crash-*.dmp; name a dump')
        path = found[-1]
    dump, name = Dump(path), Names()

    def where(v):
        n = name(v)
        if n:
            return n
        m = dump.module(v)
        return f'{m[0]}+0x{m[1]:X}' if m else ''

    if a.read:
        addr, ln = int(a.read[0], 0), int(a.read[1], 0)
        for off in range(0, ln, 16):
            m = dump.read(addr + off, min(16, ln - off))
            print(f'{addr + off:08X}  {m.hex(" ") if m else "(not in dump)"}')
        return
    if a.u32:
        for x in a.u32:
            m = dump.read(int(x, 0), 4)
            v = struct.unpack('<I', m)[0] if m else None
            print(f'[{int(x, 0):08X}] = ' + (f'0x{v:08X}  {where(v)}' if m else '(not in dump)'))
        return

    print(path)
    e = dump.exc
    if not e:
        sys.exit('no exception stream in this dump')
    print(f'exception 0x{e["code"]:08X} ({CODES.get(e["code"], "?")}) at 0x{e["addr"]:08X}  '
          f'{where(e["addr"])}  thread {e["thread"]}')
    if e['code'] in (0xC0000005, 0xC0000006) and len(e['params']) >= 2:
        kind = {0: 'reading', 1: 'writing', 8: 'executing'}.get(e['params'][0], '?')
        print(f'  {kind} 0x{e["params"][1]:08X}')
    r = e['regs']
    print('  ' + '  '.join(f'{k} {r[k]:08X}' for k in ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi')))
    print('  ' + '  '.join(f'{k} {r[k]:08X}' for k in ('ebp', 'esp', 'eip', 'efl')))
    print('return addresses on the stack (a scan, not a walk - nearest first):')
    shown = 0
    for off in range(0, a.stack, 4):
        m = dump.read(r['esp'] + off, 4)
        if not m:
            break
        v, = struct.unpack('<I', m)
        if (name(v) or (dump.module(v) or ('',))[0].lower() == 'bof3x.dll') and after_call(dump, v):
            print(f'  esp+0x{off:04X}  0x{v:08X}  {where(v)}')
            shown += 1
    if not shown:
        print('  none inside BOF3.exe or bof3x.dll')


if __name__ == '__main__':
    main()
