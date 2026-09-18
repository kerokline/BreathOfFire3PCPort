#!/usr/bin/env python
"""Decode the MSVC "Rich" header of BOF3.exe.

Undocumented but well understood: between the DOS stub and the PE header MSVC
link.exe writes an XOR-masked record of every tool that contributed an object
file, as (product id, build number, use count) triples. It identifies the exact
toolchain far more precisely than the linker version in the optional header.

    python tools/pe_rich.py [--exe bof3/BOF3.exe]
"""
import argparse, struct

ap = argparse.ArgumentParser()
ap.add_argument('--exe', default='bof3/BOF3.exe')
a = ap.parse_args()
d = open(a.exe, 'rb').read()
pe = struct.unpack_from('<I', d, 0x3C)[0]

end = d.find(b'Rich', 0, pe)
if end < 0:
    raise SystemExit('no Rich header (linker stripped it, or not MSVC)')
key = struct.unpack_from('<I', d, end + 4)[0]

# walk back to the XOR-masked "DanS"
start = None
for off in range(end - 4, 0x3F, -4):
    if struct.unpack_from('<I', d, off)[0] ^ key == 0x536E6144:   # 'DanS'
        start = off
        break
if start is None:
    raise SystemExit('Rich present but DanS not found')

print(f'Rich header at {start:#x}..{end+8:#x}, xor key {key:#010x}')
print(f'{"prodid":>8} {"build":>7} {"count":>7}   note')
recs = []
for off in range(start + 16, end, 8):
    v = struct.unpack_from('<I', d, off)[0] ^ key
    n = struct.unpack_from('<I', d, off + 4)[0] ^ key
    prodid, build = v >> 16, v & 0xFFFF
    recs.append((prodid, build, n))
    print(f'{prodid:>8} {build:>7} {n:>7}')
print(f'\n{len(recs)} tool records, {sum(r[2] for r in recs)} object contributions')
print('Build numbers are the decisive field: cross-reference against the known '
      'MSVC build table before naming a toolchain in docs.')
