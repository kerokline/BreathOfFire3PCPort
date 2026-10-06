"""EDC and ECC (P/Q) for Mode 2 Form 1 raw sectors (ECMA-130), so a patched sector reads back
as written: Mednafen's drive applies L-EC correction, which silently repairs a few patched
bytes back to the original unless the sector's EDC/ECC are regenerated."""
import numpy as np
from bgm_paths import JP_BIN

F = [0] * 256; B = [0] * 256; EDC = [0] * 256
for i in range(256):
    j = ((i << 1) ^ (0x11D if i & 0x80 else 0)) & 0xFF
    F[i] = j; B[i ^ j] = i
    e = i
    for _ in range(8):
        e = (e >> 1) ^ (0xD8018001 if e & 1 else 0)
    EDC[i] = e


def edc(data):
    e = 0
    for b in data:
        e = (e >> 8) ^ EDC[(e ^ b) & 0xFF]
    return e


def _block(sec, src, major_count, minor_count, major_mult, minor_inc, dest):
    size = major_count * minor_count
    for major in range(major_count):
        index = (major >> 1) * major_mult + (major & 1)
        a = b = 0
        for _ in range(minor_count):
            t = sec[src + index]
            index += minor_inc
            if index >= size:
                index -= size
            a ^= t; b ^= t
            a = F[a]
        a = B[F[a] ^ b]
        sec[dest + major] = a
        sec[dest + major + major_count] = a ^ b


def fix(sector):
    """Regenerate EDC + ECC of a Mode 2 Form 1 raw sector (bytearray of 2352)."""
    s = sector
    assert s[15] == 2 and not (s[18] & 0x20), "not mode 2 form 1"
    e = edc(s[16:2072])
    s[2072:2076] = e.to_bytes(4, "little")
    hdr = bytes(s[12:16]); s[12:16] = b"\x00\x00\x00\x00"
    _block(s, 0xC, 86, 24, 2, 86, 0x81C)
    _block(s, 0xC, 52, 43, 86, 88, 0x8C8)
    s[12:16] = hdr
    return s


def fix_in_file(path, lbas):
    with open(path, "r+b") as f:
        for l in sorted(set(lbas)):
            f.seek(l * 2352); s = bytearray(f.read(2352))
            fix(s)
            f.seek(l * 2352); f.write(s)


if __name__ == "__main__":
    # self-test on untouched sectors of the held image: regenerate and compare
    SRC = JP_BIN
    with open(SRC, "rb") as f:
        ok = 0
        for l in (16, 61431, 61431 + 113, 60766, 187240):
            f.seek(l * 2352); s = f.read(2352)
            t = fix(bytearray(s))
            ok += t == bytearray(s)
            print(l, "same" if t == bytearray(s) else "DIFF")
