#!/usr/bin/env python
"""Read-only scans of the PSP release's BOOT.BIN (docs/psp-widescreen.md).

    python tools/psp_elf.py <BOOT.BIN> sections
    python tools/psp_elf.py <BOOT.BIN> dis 0xa0220 0xa0314        # capstone MIPS32 LE
    python tools/psp_elf.py <BOOT.BIN> imm 370 -50 384             # I-type immediates
    python tools/psp_elf.py <BOOT.BIN> ranges                      # slti lo / slti hi pairs
    python tools/psp_elf.py <BOOT.BIN> refs 0x9d22c 0x2df0e0       # jal targets, lui/lo16 pairs
    python tools/psp_elf.py <BOOT.BIN> find bc008200bc009e00       # bytes in the image

BOOT.BIN is extracted from the disc image into a scratch directory, never into
the tree (CLAUDE.md rule 1): it is a plain ELF (e_type 0xFFA0, MIPS) whose
first program header holds .text, .rodata and .data at virtual address 0 and
whose second holds .bss at 0x3737a0. Relocations are not applied, so every
address here is a link-time offset from 0; a lui/lo16 pair whose value lands
in .text but is used as data is a .bss reference and wants +0x3737a0 (the
`refs` command prints both). The PSX build's equivalents are in the sibling's
`SLPS_009.90` at 0x80093800 after its 0x800-byte header.
"""
import struct, sys

TEXT_END = 0x2cf710          # .text size, from the section table
BSS_BASE = 0x3737a0          # program header 1's virtual address
OPS = {0x08: 'addi', 0x09: 'addiu', 0x0A: 'slti', 0x0B: 'sltiu', 0x0C: 'andi', 0x0D: 'ori', 0x0E: 'xori'}
LO16 = {0x08, 0x09, 0x0D, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2B, 0x31, 0x39}


def load(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'\x7fELF' and struct.unpack_from('<H', d, 18)[0] == 8, 'not a MIPS ELF'
    phoff, = struct.unpack_from('<I', d, 28)
    t, off, va, pa, fs, ms, fl, al = struct.unpack_from('<8I', d, phoff)
    return d[off:off + fs]        # PH0: text+rodata+data, base 0


def words(img):
    return struct.unpack_from('<%dI' % (TEXT_END // 4), img, 0)


def simm(w):
    v = w & 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def cmd_sections(path):
    d = open(path, 'rb').read()
    e_shoff, = struct.unpack_from('<I', d, 32)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', d, 46)
    shs = [struct.unpack_from('<10I', d, e_shoff + i * e_shentsize) for i in range(e_shnum)]
    stroff = shs[e_shstrndx][4]
    for s in shs:
        name = d[stroff + s[0]:d.index(b'\0', stroff + s[0])].decode()
        if name and not name.startswith('.rel'):
            print('%-28s addr %#8x off %#8x size %#7x' % (name, s[3], s[4], s[5]))


def cmd_dis(img, a, b):
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_MIPS, capstone.CS_MODE_MIPS32 | capstone.CS_MODE_LITTLE_ENDIAN)
    md.skipdata = True           # Allegrex VFPU ops come out as .byte
    for i in md.disasm(img[a:b], a):
        print('%08x  %-8s %s' % (i.address, i.mnemonic, i.op_str))


def cmd_imm(img, vals):
    w = words(img)
    for v in vals:
        hits = []
        for i, x in enumerate(w):
            op = x >> 26
            if op in OPS and (simm(x) if op < 0x0C else x & 0xFFFF) == v:
                hits.append('%x(%s)' % (i * 4, OPS[op]))
        print('%6d x%-4d %s' % (v, len(hits), ' '.join(hits[:20])))


def cmd_ranges(img):
    """slti r, x, LO ... slti r, x, HI on the same x: the PSP compiler's range check."""
    w = words(img)
    for i, x in enumerate(w):
        if x >> 26 != 0x0A:
            continue
        rs, lo = (x >> 21) & 31, simm(x)
        if not -400 <= lo < 0:
            continue
        for j in range(i + 1, min(i + 8, len(w))):
            y = w[j]
            if y >> 26 == 0x0A and (y >> 21) & 31 == rs:
                hi = simm(y)
                if 100 <= hi <= 1200:
                    print('%08x slti %-5d %08x slti %-5d  keep [%d, %d]' % (i * 4, lo, j * 4, hi, lo, hi - 1))
                break


def cmd_refs(img, targets):
    w = words(img)
    t = set(targets) | {x - BSS_BASE for x in targets}
    for i, x in enumerate(w):
        if x >> 26 == 3 and (x & 0x3FFFFFF) << 2 in t:
            print('jal  %08x -> %08x' % (i * 4, (x & 0x3FFFFFF) << 2))
        if x >> 21 != 0x1E0:
            continue
        rt, hi = (x >> 16) & 31, (x & 0xFFFF) << 16
        for j in range(i + 1, min(i + 12, len(w))):
            y = w[j]
            op, rs = y >> 26, (y >> 21) & 31
            if op in LO16 and rs == rt:
                lo = y & 0xFFFF if op == 0x0D else simm(y)
                ea = (hi + lo) & 0xFFFFFFFF
                if ea in t or (ea & ~3) in t:
                    print('ref  %08x (lui %08x) -> %08x%s' % (j * 4, i * 4, ea,
                          '  (.bss %#x)' % (ea + BSS_BASE) if ea + BSS_BASE in targets else ''))
            if (y >> 16) & 31 == rt and op not in LO16 and op != 0:
                break


def cmd_find(img, hexbytes):
    pat, i = bytes.fromhex(hexbytes), 0
    while True:
        i = img.find(pat, i)
        if i < 0:
            break
        print('%08x' % i); i += 1


def main():
    path, cmd, args = sys.argv[1], sys.argv[2], sys.argv[3:]
    if cmd == 'sections':
        return cmd_sections(path)
    img = load(path)
    if cmd == 'dis':
        cmd_dis(img, int(args[0], 16), int(args[1], 16))
    elif cmd == 'imm':
        cmd_imm(img, [int(a, 0) for a in args])
    elif cmd == 'ranges':
        cmd_ranges(img)
    elif cmd == 'refs':
        cmd_refs(img, [int(a, 16) for a in args])
    elif cmd == 'find':
        cmd_find(img, args[0])
    else:
        sys.exit(__doc__)


main()
