#!/usr/bin/env python
"""Read initialised data out of BOF3.exe by virtual address.

    python tools/pe_data.py 0x64e97c:8:s16       # 8 signed halfwords
    python tools/pe_data.py 0x64e95c:16:u16 0x64e97c:8:s16

Format is COUNT:u8|s8|u16|s16|u32. Addresses past the end of a section's raw
data are BSS and read back as "(bss)".
"""
import sys, struct
data = open('bof3/BOF3.exe','rb').read()
pe = struct.unpack_from('<I', data, 0x3C)[0]
nsec, = struct.unpack_from('<H', data, pe+6); optsz, = struct.unpack_from('<H', data, pe+20)
opt = pe+24; base, = struct.unpack_from('<I', data, opt+28)
secs=[]
for i in range(nsec):
    s = opt+optsz+i*40
    nm = data[s:s+8].rstrip(b'\0').decode('latin1')
    vsz,va,rsz,raw = struct.unpack_from('<IIII', data, s+8)
    secs.append((nm, base+va, vsz, raw, rsz))
FMT = {'u8':('<B',1),'s8':('<b',1),'u16':('<H',2),'s16':('<h',2),'u32':('<I',4)}
for arg in sys.argv[1:]:
    a, n, f = arg.split(':')
    a = int(a,16); n = int(n); code, w = FMT[f]
    sec = next((s for s in secs if s[1] <= a < s[1]+s[2]), None)
    if sec is None: print(f'{a:#x}: not mapped'); continue
    off = a - sec[1]
    if off >= sec[4]:
        print(f'{a:#x} ({sec[0]}): (bss — no initialised bytes)'); continue
    vals = [struct.unpack_from(code, data, sec[3]+off+i*w)[0] for i in range(n)]
    print(f'{a:#x} ({sec[0]}) {f} x{n}: ' + ', '.join(str(v) for v in vals))
