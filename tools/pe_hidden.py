#!/usr/bin/env python
"""Function entries that pc_funcs.json folds into their neighbours.

pe_funcs.py takes entries from direct call targets only, so a function reached
solely through a pointer - a handler table, a task entry, a window procedure -
becomes the tail of whatever precedes it (docs/attract-remaining.md section 3).

    python tools/pe_hidden.py scan
        -> analysis/pc_hidden.json, analysis/calltrace/entries_hidden.txt
    python tools/pe_hidden.py classify analysis/calltrace/hidden_a/bof3x.calltrace.tsv
        -> analysis/pc_hidden_reached.json
    python tools/pe_hidden.py plus
        -> analysis/calltrace/entries_plus_hidden.txt

`scan`'s rule: an address inside a recorded extent, 16-aligned, preceded by a
ret or jmp and nothing but nop / int3 padding, that no branch inside the extent
targets, and whose first dword is not itself a .text address (a jump table).
`classify` reads a first-call trace of the scan's entry list and says how each
reached entry was entered, from the instruction before its first return
address. `plus` merges the reached ones into entries.txt, cutting each known
size at the next hidden start so the tracer's owned ranges stop covering them.

Output is derived from copyrighted game code: it lives under analysis/ and is
never committed (CLAUDE.md rule 1).
"""
import argparse, bisect, collections, json, struct
import capstone

TASK_RUNALL = 0x5A98A0  # the tracer counts frames by it and refuses a list without it


def parse_pe(data):
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    nsec, = struct.unpack_from('<H', data, pe + 6)
    optsz, = struct.unpack_from('<H', data, pe + 20)
    opt = pe + 24
    base, = struct.unpack_from('<I', data, opt + 28)
    image, = struct.unpack_from('<I', data, opt + 56)
    secs = []
    for i in range(nsec):
        s = opt + optsz + i * 40
        name = data[s:s + 8].rstrip(b'\0').decode('latin1')
        vsz, va, rsz, raw = struct.unpack_from('<IIII', data, s + 8)
        secs.append(dict(name=name, va=base + va, vsz=vsz, raw=raw, rsz=rsz))
    return base, base + image, secs


class Image:
    def __init__(self, exe):
        self.data = open(exe, 'rb').read()
        self.base, self.end, self.secs = parse_pe(self.data)
        self.text = next(s for s in self.secs if s['name'] == '.text')

    def off(self, va):
        for s in self.secs:
            if s['va'] <= va < s['va'] + s['rsz']:
                return s['raw'] + va - s['va']
        return None

    def in_text(self, va):
        return self.text['va'] <= va < self.text['va'] + self.text['vsz']

    def dword(self, va):
        return struct.unpack_from('<I', self.data, self.off(va))[0]


def cmd_scan(a):
    img = Image(a.exe)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    funcs = json.load(open(a.funcs))['functions']

    # Every dword anywhere in the image that points into .text, and where.
    ptrs = collections.defaultdict(list)
    for s in img.secs:
        blob = img.data[s['raw']:s['raw'] + s['rsz']]
        for i in range(len(blob) - 3):
            x, = struct.unpack_from('<I', blob, i)
            if img.in_text(x):
                ptrs[x].append((s['name'], s['va'] + i))

    found, tables = [], 0
    for f in funcs:
        e, size = f['entry'], f['size']
        code = img.data[img.off(e):img.off(e) + size]
        insns = list(md.disasm(code, e))
        targets = set()
        for ins in insns:
            if ins.mnemonic.startswith('j') or ins.mnemonic == 'call':
                try:
                    targets.add(int(ins.op_str, 16))
                except ValueError:
                    pass
        for ins in insns:
            if ins.mnemonic not in ('ret', 'jmp'):
                continue
            p = ins.address + ins.size
            while p < e + size and img.data[img.off(p)] in (0x90, 0xCC):
                p += 1
            if p >= e + size or p % 16 or p in targets:
                continue
            if img.in_text(img.dword(p)):
                tables += 1
                continue
            found.append(dict(entry=p, host=e, refs=[[n, v] for n, v in ptrs.get(p, [])]))

    # Size: to the next known or hidden start, within the host's extent.
    host_end = {f['entry']: f['entry'] + f['size'] for f in funcs}
    starts = sorted({h['entry'] for h in found} | set(host_end))
    for h in found:
        i = bisect.bisect_right(starts, h['entry'])
        end = host_end[h['host']]
        if i < len(starts) and starts[i] < end:
            end = starts[i]
        h['size'] = end - h['entry']
    json.dump(found, open(a.out, 'w'), indent=0)
    runall = next(f for f in funcs if f['entry'] == TASK_RUNALL)
    with open(a.entries, 'w', newline='\n') as f:
        f.write(f'# {len(found)} entries hidden inside {a.funcs} extents, plus Task_RunAll '
                f'(tools/pe_hidden.py scan)\n')
        for e, s in sorted([(h['entry'], h['size']) for h in found] + [(TASK_RUNALL, runall['size'])]):
            f.write(f'{e:08X} {s:X}\n')
    print(f'{len(found)} hidden entries in {len({h["host"] for h in found})} extents '
          f'({tables} jump tables dropped); {sum(1 for h in found if h["refs"])} have a pointer '
          f'in the image -> {a.out}, {a.entries}')


def cmd_classify(a):
    img = Image(a.exe)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    hidden = {h['entry']: h for h in json.load(open(a.hidden))}

    def after_call(c):
        if not img.in_text(c):
            return False
        for k in (2, 3, 5, 6, 7):
            ins = next(md.disasm(img.data[img.off(c - k):img.off(c - k) + k], c - k), None)
            if ins and ins.address + ins.size == c and ins.mnemonic == 'call':
                return True
        return False

    out = []
    for line in open(a.trace):
        if line[0] == '#' or not line.strip():
            continue
        frame, e, c, _ = line.split('\t')
        e, c = int(e, 16), int(c, 16)
        if e not in hidden:
            continue
        # A tail-jump target inherits its caller's return address, and is
        # classified as its caller was.
        if c == 0:
            how = 'task entry'
        elif not img.base <= c < img.end:
            how = 'OS callback'
        elif after_call(c):
            how = 'indirect call'
        else:
            how = 'jumped to'
        out.append(dict(hidden[e], frame=int(frame), caller=c, how=how))
    json.dump(out, open(a.out, 'w'), indent=0)
    print(f'{len(out)} hidden entries reached:',
          dict(collections.Counter(r['how'] for r in out)), '->', a.out)


def cmd_plus(a):
    known = {}
    for line in open(a.entries):
        if line[0] != '#' and line.strip():
            e, s = line.split()
            known[int(e, 16)] = int(s, 16)
    hidden = json.load(open(a.hidden))
    reached = json.load(open(a.reached))
    starts = sorted(h['entry'] for h in hidden)
    out, cut = {}, 0
    for e, s in known.items():
        i = bisect.bisect_right(starts, e)
        end = e + s
        if i < len(starts) and starts[i] < end:
            end, cut = starts[i], cut + 1
        out[e] = end - e
    for r in reached:
        out[r['entry']] = r['size']
    with open(a.out, 'w', newline='\n') as f:
        f.write(f'# {a.entries} ({len(known)}) plus {len(reached)} reached hidden entries; '
                f'{cut} known sizes cut at the next hidden start (tools/pe_hidden.py plus)\n')
        for e in sorted(out):
            f.write(f'{e:08X} {out[e]:X}\n')
    print(f'{len(out)} entries, {cut} sizes cut -> {a.out}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--exe', default='bof3/BOF3.exe')
    ap.add_argument('--funcs', default='analysis/pc_funcs.json')
    ap.add_argument('--hidden', default='analysis/pc_hidden.json')
    ap.add_argument('--reached', default='analysis/pc_hidden_reached.json')
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('scan')
    p.add_argument('--out', default='analysis/pc_hidden.json')
    p.add_argument('--entries', default='analysis/calltrace/entries_hidden.txt')
    p.set_defaults(fn=cmd_scan)
    p = sub.add_parser('classify')
    p.add_argument('trace')
    p.add_argument('--out', default='analysis/pc_hidden_reached.json')
    p.set_defaults(fn=cmd_classify)
    p = sub.add_parser('plus')
    p.add_argument('--entries', default='analysis/calltrace/entries.txt')
    p.add_argument('--out', default='analysis/calltrace/entries_plus_hidden.txt')
    p.set_defaults(fn=cmd_plus)
    a = ap.parse_args()
    a.fn(a)


if __name__ == '__main__':
    main()
