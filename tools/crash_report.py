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

The image may lie elsewhere than 0x400000 (a developer's bof3x.exe written at
another base, docs/relocatable-image-design.md 8): the dump's module list
gives the main module's base, an address inside it is shown with its VA and
named by the VA, and --read / --u32 take VAs, as every other tool does
(tools/proc_image.py).

Both widths (docs/x64-build-design.md 11): a dump of the 32-bit process (an
x86 CONTEXT, 4-byte stack words) and one of the x86-64 bof3x.exe (the
SystemInfoStream says AMD64; an AMD64 CONTEXT, 8-byte stack words, addresses
printed with 16 digits). The game's image lies below 4 GB on both, so a VA is
read and named the same way.

A dump is game-derived: it holds the game's code and data. Keep it local
(CLAUDE.md rule 1).

    python tools/crash_report.py --log path/to/bof3x.log   # the CRASH lines of a log

The log's `CRASH n:` lines (src/hook/crash.cpp) are the report where there is
no dump - on Linux, whose catcher writes none (src/hook/crash_posix.cpp) - and
beside one on Windows: each report's lines are printed with the fault's
address and every stack word named as above, by the VA the line gives.
"""
import argparse, bisect, glob, json, os, re, struct, sys, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    from proc_image import LINKED_BASE, place, va  # noqa: E402
except (ImportError, AttributeError, OSError):   # not Windows: proc_image reads processes through psapi
    LINKED_BASE, _LINKED_END = 0x400000, 0x98F000

    def place(addr, move):
        return addr + move if LINKED_BASE <= addr < _LINKED_END else addr

    def va(addr, move):
        return addr - move if move and LINKED_BASE + move <= addr < _LINKED_END + move else addr
CODES = {0xC0000005: 'access violation', 0xC0000006: 'in-page error',
         0xC000001D: 'illegal instruction', 0xC0000096: 'privileged instruction',
         0xC00000FD: 'stack overflow', 0xC0000094: 'integer divide by zero',
         0x80000003: 'breakpoint', 0xC0000409: 'stack buffer overrun (fail-fast)'}
# The x86 CONTEXT's integer registers, in record order from +0x9C.
REGS32 = ['edi', 'esi', 'ebx', 'edx', 'ecx', 'eax', 'ebp', 'eip', 'cs', 'efl', 'esp']
# The AMD64 CONTEXT's: EFlags at +0x44, Rax..R15 at +0x78 (16 qwords), Rip at +0xF8.
REGS64 = ['rax', 'rcx', 'rdx', 'rbx', 'rsp', 'rbp', 'rsi', 'rdi',
          'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15']
PROCESSOR_AMD64 = 9                                         # SystemInfoStream.ProcessorArchitecture


class Dump:
    def __init__(self, path):
        self.d = d = open(path, 'rb').read()
        sig, _, n, rva = struct.unpack_from('<IIII', d, 0)
        if sig != 0x504D444D:
            sys.exit(f'{path}: not a minidump')
        self.regions, self.modules, self.exc = [], [], None
        # The width first: the SystemInfoStream (kind 7) may follow the
        # ExceptionStream in the directory, and the context's layout needs it.
        self.width = 32
        for i in range(n):
            kind, _, r = struct.unpack_from('<III', d, rva + 12 * i)
            if kind == 7 and struct.unpack_from('<H', d, r)[0] == PROCESSOR_AMD64:
                self.width = 64
        self.word, self.fmt, self.digits = (4, '<I', 8) if self.width == 32 else (8, '<Q', 16)
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
                if self.width == 32:
                    regs = dict(zip(REGS32, struct.unpack_from('<11I', d, ctx + 0x9C)))
                    regs['ip'], regs['sp'] = regs['eip'], regs['esp']
                else:
                    regs = dict(zip(REGS64, struct.unpack_from('<16Q', d, ctx + 0x78)))
                    regs['efl'], = struct.unpack_from('<I', d, ctx + 0x44)
                    regs['rip'], = struct.unpack_from('<Q', d, ctx + 0xF8)
                    regs['ip'], regs['sp'] = regs['rip'], regs['rsp']
                self.exc = dict(thread=tid, code=code, addr=addr, params=params, regs=regs)
        # The game's image: the executable among the modules (BOF3.exe or
        # bof3x*.exe), and its move from 0x400000.
        exe = [b for b, _, n in self.modules if n.lower().endswith('.exe') and n.lower().startswith(('bof3.', 'bof3x'))]
        self.move = (exe[0] - LINKED_BASE) if exe else 0

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
            # ImageBase: <I at opt + 28 in a PE32, <Q at opt + 24 in a PE32+
            # (tools/pe_read.py). BOF3.exe is a PE32; the branch is for a
            # PE32+ handed to this reader (docs/x64-build-design.md 11).
            magic, = struct.unpack_from('<H', d, pe + 24)
            if magic == 0x20B:
                base, = struct.unpack_from('<Q', d, pe + 24 + 24)
            else:
                base, = struct.unpack_from('<I', d, pe + 24 + 28)
            for i in range(nsec):
                vsz, va, rsz, raw = struct.unpack_from('<IIII', d, pe + 24 + optsz + i * 40 + 8)
                _cache['secs'].append((base + va, rsz, raw))
    for va, rsz, raw in _cache['secs']:
        if va <= a and a + n <= va + rsz:
            return _cache['d'][raw + a - va:raw + a - va + n]
    return None


CRASH_LINE = re.compile(r'^CRASH (\d+): (.*)$')


def read_log(path, name):
    """Each report's CRASH lines, the fault's address and the stack words named
    by the VA the line gives (" (VA 0x...)" where the image moved)."""
    reports, order = {}, []
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = CRASH_LINE.match(line.rstrip('\r\n'))
            if not m:
                continue
            n = int(m.group(1))
            if n not in reports:
                reports[n] = []
                order.append(n)
            reports[n].append(m.group(2))
    if not order:
        sys.exit(f'{path}: no CRASH lines')
    for n in order:
        print(f'{path}: report {n}')
        for text in reports[n]:
            label = None
            if text.startswith(('exception ', 'signal ')):
                at = re.search(r' at 0x([0-9A-Fa-f]+)(?: \(VA 0x([0-9A-Fa-f]+)\))?', text)
                if at:
                    label = name(int(at.group(2) or at.group(1), 16))
            elif text.startswith('stack '):
                sm = re.match(r'stack \+0x[0-9A-Fa-f]+\s+0x([0-9A-Fa-f]+)(.*)', text)
                if sm:
                    moved = re.search(r'\(VA 0x([0-9A-Fa-f]+)\)', sm.group(2))
                    label = name(int(moved.group(1) if moved else sm.group(1), 16))
            print('  ' + text + (f'   <- {label}' if label else ''))
    return 0


def after_call(dump, v):
    b = dump.read(v - 6, 6) or exe_bytes(va(v, dump.move) - 6, 6)
    return bool(b) and (b[1] == 0xE8 or b[4] == 0xFF or b[3] == 0xFF or b[0] == 0xFF)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('dump', nargs='?')
    ap.add_argument('--read', nargs=2, metavar=('ADDR', 'LEN'))
    ap.add_argument('--u32', nargs='+', metavar='ADDR')
    ap.add_argument('--stack', type=lambda x: int(x, 0), default=0x2000, help='bytes of stack to scan')
    ap.add_argument('--log', metavar='LOG', help="a log's CRASH lines in place of a dump")
    a = ap.parse_args()
    if a.log:
        return read_log(a.log, Names())
    path = a.dump
    if not path:
        found = sorted(glob.glob(os.path.join(ROOT, 'build', 'bof3x.crash-*.dmp')), key=os.path.getmtime)
        if not found:
            sys.exit('no build/bof3x.crash-*.dmp; name a dump')
        path = found[-1]
    dump, name = Dump(path), Names()

    def where(v):
        a = va(v, dump.move)   # a place in the moved image, as its VA
        n = name(a)
        if n:
            return n + (f' (VA 0x{a:08X})' if a != v else '')
        m = dump.module(v)
        return f'{m[0]}+0x{m[1]:X}' if m else ''

    if a.read:
        addr, ln = int(a.read[0], 0), int(a.read[1], 0)
        for off in range(0, ln, 16):
            m = dump.read(place(addr + off, dump.move), min(16, ln - off))
            print(f'{addr + off:08X}  {m.hex(" ") if m else "(not in dump)"}')
        return
    if a.u32:
        for x in a.u32:
            m = dump.read(place(int(x, 0), dump.move), 4)
            v = struct.unpack('<I', m)[0] if m else None
            print(f'[{int(x, 0):08X}] = ' + (f'0x{v:08X}  {where(v)}' if m else '(not in dump)'))
        return

    print(path)
    e = dump.exc
    if not e:
        sys.exit('no exception stream in this dump')
    w = dump.digits                                         # 8 hex digits for a 32-bit dump, 16 for a 64-bit one
    print(f'{"x86" if dump.width == 32 else "x86-64"} process; exception 0x{e["code"]:08X} ({CODES.get(e["code"], "?")}) '
          f'at 0x{e["addr"]:0{w}X}  {where(e["addr"])}  thread {e["thread"]}')
    if e['code'] in (0xC0000005, 0xC0000006) and len(e['params']) >= 2:
        kind = {0: 'reading', 1: 'writing', 8: 'executing'}.get(e['params'][0], '?')
        at = e['params'][1]
        print(f'  {kind} 0x{at:0{w}X}' + (f' (VA 0x{va(at, dump.move):08X})' if va(at, dump.move) != at else ''))
    r = e['regs']
    if dump.width == 32:
        print('  ' + '  '.join(f'{k} {r[k]:08X}' for k in ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi')))
        print('  ' + '  '.join(f'{k} {r[k]:08X}' for k in ('ebp', 'esp', 'eip', 'efl')))
    else:
        print('  ' + '  '.join(f'{k} {r[k]:016X}' for k in ('rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi')))
        print('  ' + '  '.join(f'{k} {r[k]:016X}' for k in ('r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15')))
        print('  ' + '  '.join(f'{k} {r[k]:016X}' for k in ('rbp', 'rsp', 'rip')) + f'  efl {r["efl"]:08X}')
    sp = 'esp' if dump.width == 32 else 'rsp'
    print('return addresses on the stack (a scan, not a walk - nearest first):')
    shown = 0
    for off in range(0, a.stack, dump.word):
        m = dump.read(r['sp'] + off, dump.word)
        if not m:
            break
        v, = struct.unpack(dump.fmt, m)
        if (name(va(v, dump.move)) or (dump.module(v) or ('',))[0].lower() == 'bof3x.dll') and after_call(dump, v):
            print(f'  {sp}+0x{off:04X}  0x{v:0{w}X}  {where(v)}')
            shown += 1
    if not shown:
        print('  none inside BOF3.exe or bof3x.dll')


if __name__ == '__main__':
    main()
