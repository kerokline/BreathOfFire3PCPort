#!/usr/bin/env python
"""Disassemble one function (or N instructions) of BOF3.exe by virtual address.

    python tools/pe_disasm.py 0x497740          # to the first ret past the entry
    python tools/pe_disasm.py 0x497740:40       # exactly 40 instructions
"""
import sys, struct, capstone
exe = 'bof3/BOF3.exe'
data = open(exe,'rb').read()
pe = struct.unpack_from('<I', data, 0x3C)[0]
nsec, = struct.unpack_from('<H', data, pe+6); optsz, = struct.unpack_from('<H', data, pe+20)
opt = pe+24; base, = struct.unpack_from('<I', data, opt+28)
secs=[]
for i in range(nsec):
    s = opt+optsz+i*40
    nm = data[s:s+8].rstrip(b'\0').decode('latin1')
    vsz,va,rsz,raw = struct.unpack_from('<IIII', data, s+8)
    secs.append((nm, base+va, vsz, raw, rsz))
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail=True
for arg in sys.argv[1:]:
    addr, _, cnt = arg.partition(':')
    addr = int(addr, 16); cnt = int(cnt) if cnt else 0
    sec = next(s for s in secs if s[1] <= addr < s[1]+s[2])
    off = sec[3] + (addr - sec[1])
    print(f'--- {addr:#x} ({sec[0]}) ---')
    n = 0
    for ins in md.disasm(data[off:off+4096], addr):
        print(f'{ins.address:08x}  {ins.bytes.hex():<16} {ins.mnemonic:<7} {ins.op_str}')
        n += 1
        if cnt and n >= cnt: break
        if not cnt and ins.mnemonic in ('ret','retf'): break
