#!/usr/bin/env python
"""Read a 32-bit PE's headers, sections, imports, exports and resources.

The project's PE parser without capstone: tools/pe_funcs.py takes its
parse_pe from here, and tools/make_exe.py verifies the executable it writes
with it (docs/state3-design.md section 1.3). Reads layout only; prints
nothing of a file's content.

    python tools/pe_read.py build/bof3x.exe     # headers, sections, imports
"""
import struct
import sys


def parse_pe(data):
    """(image base, entry VA, sections) - the shape pe_funcs.py has always used."""
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    assert data[pe:pe + 4] == b'PE\0\0', 'not a PE'
    nsec, = struct.unpack_from('<H', data, pe + 6)
    optsz, = struct.unpack_from('<H', data, pe + 20)
    opt = pe + 24
    entry, = struct.unpack_from('<I', data, opt + 16)
    base, = struct.unpack_from('<I', data, opt + 28)
    secs = []
    off = opt + optsz
    for i in range(nsec):
        s = off + i * 40
        name = data[s:s + 8].rstrip(b'\0').decode('latin1')
        vsz, va, rsz, raw = struct.unpack_from('<IIII', data, s + 8)
        secs.append(dict(name=name, va=base + va, vsz=vsz, raw=raw, rsz=rsz))
    return base, base + entry, secs


class Pe:
    """Every header field make_exe.py's verification reads, by name."""

    def __init__(self, data):
        self.data = data
        pe = struct.unpack_from('<I', data, 0x3C)[0]
        if data[:2] != b'MZ' or data[pe:pe + 4] != b'PE\0\0':
            raise ValueError('not a PE')
        (self.machine, nsec, self.timestamp, _, _, optsz,
         self.characteristics) = struct.unpack_from('<HHIIIHH', data, pe + 4)
        opt = pe + 24
        self.magic, = struct.unpack_from('<H', data, opt)
        if self.magic != 0x10B:
            raise ValueError('not PE32')
        self.entry_rva, = struct.unpack_from('<I', data, opt + 16)
        self.image_base, self.section_alignment, self.file_alignment = struct.unpack_from('<III', data, opt + 28)
        (self.os_major, self.os_minor, _, _, self.subsys_major,
         self.subsys_minor) = struct.unpack_from('<HHHHHH', data, opt + 40)
        self.size_of_image, self.size_of_headers, self.checksum = struct.unpack_from('<III', data, opt + 56)
        self.subsystem, self.dll_characteristics = struct.unpack_from('<HH', data, opt + 68)
        (self.stack_reserve, self.stack_commit, self.heap_reserve,
         self.heap_commit) = struct.unpack_from('<IIII', data, opt + 72)
        ndirs, = struct.unpack_from('<I', data, opt + 92)
        self.dirs = [struct.unpack_from('<II', data, opt + 96 + 8 * i) for i in range(ndirs)]
        self.sections = []
        for i in range(nsec):
            s = opt + optsz + i * 40
            name = data[s:s + 8].rstrip(b'\0').decode('latin1')
            vsz, va, rsz, raw = struct.unpack_from('<IIII', data, s + 8)
            ch, = struct.unpack_from('<I', data, s + 36)
            self.sections.append(dict(name=name, va=va, vsz=vsz, raw=raw, rsz=rsz, ch=ch))

    def offset(self, rva):
        for s in self.sections:
            if s['va'] <= rva < s['va'] + s['rsz']:
                return s['raw'] + rva - s['va']
        raise ValueError(f'rva {rva:#x} has no file bytes')

    def cstr(self, rva):
        o = self.offset(rva)
        return self.data[o:self.data.index(b'\0', o)].decode('latin1')

    def imports(self):
        """[(dll, [name or '#ordinal', ...]), ...] from the import directory."""
        rva, size = self.dirs[1]
        out = []
        if not rva:
            return out
        p = self.offset(rva)
        while True:
            ilt, _, _, name, iat = struct.unpack_from('<IIIII', self.data, p)
            p += 20
            if not name:
                return out
            q = self.offset(ilt or iat)
            fns = []
            while True:
                e, = struct.unpack_from('<I', self.data, q)
                q += 4
                if not e:
                    break
                fns.append(f'#{e & 0xFFFF}' if e & 0x80000000 else self.cstr(e + 2))
            out.append((self.cstr(name), fns))

    def exports(self):
        """The export directory's names."""
        rva, size = self.dirs[0]
        if not rva:
            return []
        o = self.offset(rva)
        n_names, = struct.unpack_from('<I', self.data, o + 24)
        names_rva, = struct.unpack_from('<I', self.data, o + 32)
        q = self.offset(names_rva)
        return [self.cstr(struct.unpack_from('<I', self.data, q + 4 * i)[0]) for i in range(n_names)]

    def resources(self):
        """{(type, name, lang): bytes} - ids as ints, names as str."""
        rva, size = self.dirs[2]
        out = {}
        if not rva:
            return out
        base = self.offset(rva)

        def entries(o):
            named, ids = struct.unpack_from('<HH', self.data, o + 12)
            for i in range(named + ids):
                name, target = struct.unpack_from('<II', self.data, o + 16 + 8 * i)
                if name & 0x80000000:
                    so = base + (name & 0x7FFFFFFF)
                    n, = struct.unpack_from('<H', self.data, so)
                    name = self.data[so + 2:so + 2 + 2 * n].decode('utf-16-le')
                yield name, target

        for t, t_off in entries(base):
            for n, n_off in entries(base + (t_off & 0x7FFFFFFF)):
                for lang, d_off in entries(base + (n_off & 0x7FFFFFFF)):
                    drva, dsize, _, _ = struct.unpack_from('<IIII', self.data, base + d_off)
                    o = self.offset(drva)
                    out[(t, n, lang)] = self.data[o:o + dsize]
        return out


def main():
    pe = Pe(open(sys.argv[1], 'rb').read())
    print(f'base {pe.image_base:#x} entry {pe.image_base + pe.entry_rva:#x} timestamp {pe.timestamp:#x} '
          f'characteristics {pe.characteristics:#x} dll characteristics {pe.dll_characteristics:#x} '
          f'subsystem {pe.subsystem} {pe.subsys_major}.{pe.subsys_minor} '
          f'stack {pe.stack_reserve:#x}/{pe.stack_commit:#x} size {pe.size_of_image:#x}')
    for s in pe.sections:
        print(f"  {s['name']:8} {pe.image_base + s['va']:#010x} +{s['vsz']:#08x}  raw {s['rsz']:#07x}  {s['ch']:#010x}")
    for dll, fns in pe.imports():
        print(f'  import {dll}: {", ".join(fns)}')
    for name in pe.exports():
        print(f'  export {name}')
    for key, blob in pe.resources().items():
        print(f'  resource {key}: {len(blob)} bytes')


if __name__ == '__main__':
    main()
