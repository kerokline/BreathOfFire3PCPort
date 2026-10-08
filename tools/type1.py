#!/usr/bin/env python
"""The PSX EMI type-1 decompressor, and its check against every copy we hold.

docs/type1-compression.md. Type-1 sections (65 on each PSX disc, all bound
for 0x80033800: the PL, BPL*, BRT* arenas and START) are compressed; the PC
port and the PSP ship them decompressed. The format, measured on all 65:

    u32  size        the decompressed size
    u16  0xFFFF      in 65 of 65
    tokens, until `size` bytes are out, then one 0xFF byte (65 of 65)

    b0 in 0x11..0x1F  0x20 - b0 literal bytes follow (1..15)
    b0 == 0x10        n = next byte; 16 + n literal bytes follow (16..271)
    otherwise         a back-reference, b0 and b1:
                        distance = 0x800 - ((b0 >> 5) << 8 | b1)   (1..2048)
                        length   = (b0 & 0x0F) + 3; if that is 18, add the
                                   next byte (18..273)
                      bit 4 of b0 is never set in a reference; a copy may
                      overlap its own output (distance < length)

    python tools/type1.py check --disc JPDISC [--psp PSPISO] [--dat DAT] [--disc2 DISC ...]
    python tools/type1.py decode --disc DISC --emi PLCHAR/PL012.EMI --out analysis/type1/PL012.bin
    python tools/type1.py selftest

`check` prints names, sizes, counts and verdicts only - never section bytes.
`decode` writes game data: under analysis/ (gitignored), never committed
(CLAUDE.md rule 1).
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

TYPE1 = 1


class Type1Error(ValueError):
    pass


def decode(c):
    """Decompress one type-1 section payload; raises on anything the 65 never do."""
    if len(c) < 7:
        raise Type1Error("type 1: %d bytes, shorter than the header" % len(c))
    size, mark = struct.unpack_from("<IH", c)
    if mark != 0xFFFF:
        raise Type1Error("type 1: header word 0x%04X, not 0xFFFF" % mark)
    out = bytearray()
    p = 6
    try:
        while len(out) < size:
            b0 = c[p]
            p += 1
            if 0x10 <= b0 <= 0x1F:
                if b0 == 0x10:
                    n = 16 + c[p]
                    p += 1
                else:
                    n = 0x20 - b0
                if p + n > len(c):
                    raise Type1Error("type 1: literal run of %d past the end at +0x%X" % (n, p))
                out += c[p:p + n]
                p += n
                continue
            if b0 & 0x10:
                raise Type1Error("type 1: reference byte 0x%02X with bit 4 set at +0x%X" % (b0, p - 1))
            b1 = c[p]
            p += 1
            d = 0x800 - ((b0 >> 5) << 8 | b1)
            n = (b0 & 0x0F) + 3
            if n == 18:
                n += c[p]
                p += 1
            if d > len(out):
                raise Type1Error("type 1: distance %d before the start (%d out) at +0x%X" % (d, len(out), p))
            for _ in range(n):  # byte by byte: a copy may overlap its own output
                out.append(out[-d])
    except IndexError:
        raise Type1Error("type 1: stream ends at +0x%X with %d of %d bytes out" % (p, len(out), size))
    if len(out) != size:
        raise Type1Error("type 1: the last token overran the size by %d" % (len(out) - size))
    if p >= len(c) or c[p] != 0xFF:
        raise Type1Error("type 1: no 0xFF after the last token (+0x%X)" % p)
    return bytes(out)


# ---------------------------------------------------------------- the check

def _sections(build, t):
    out = {}
    for key in sorted(build.emis):
        for i, typ, dest, b in build.sections(key):
            if typ == t:
                out.setdefault(key, []).append((i, dest, b))
    return out


def _pc_chunks(dat_dir, key):
    import dat
    path = os.path.join(dat_dir, os.path.basename(key)[:-4] + ".DAT")
    if not os.path.exists(path):
        return None
    with open(path, "rb") as f:
        blob = f.read()
    return [blob[c.offset:c.offset + c.size] for c in dat.parse(blob, path) if c.kind == 0]


def cmd_check(a):
    import region_diff as rd
    jp = rd.Build("disc", a.disc)
    secs = _sections(jp, TYPE1)
    psp = _sections(rd.Build("psp", a.psp), TYPE1) if a.psp else {}
    dec, bad = {}, 0
    for key, v in secs.items():
        if len(v) != 1:
            print("?  %s: %d type-1 sections" % (key, len(v)))
        try:
            dec[key] = decode(v[0][2])
        except Type1Error as e:
            print("FAIL %s: %s" % (key, e))
            bad += 1
    print("%s: %d type-1 sections in %d files; %d decode" % (a.disc, sum(map(len, secs.values())), len(secs), len(dec)))
    if a.psp:
        eq = [k for k in dec if k in psp and psp[k][0][2] == dec[k]]
        print("  PSP: %d of %d byte-identical; differ: %s" % (len(eq), len(dec), " ".join(
            os.path.basename(k)[:-4] for k in dec if k not in eq) or "none"))
    if a.dat:
        eq, ne = [], []
        for k, o in dec.items():
            ch = _pc_chunks(a.dat, k)
            (eq if ch and o in ch else ne).append(k)
        print("  PC:  %d of %d byte-identical to a kind-0 chunk; differ: %s" % (len(eq), len(dec), " ".join(
            os.path.basename(k)[:-4] for k in ne) or "none"))
    if a.psp and a.dat:
        none = [k for k in dec if not (k in psp and psp[k][0][2] == dec[k])
                and not (_pc_chunks(a.dat, k) and dec[k] in _pc_chunks(a.dat, k))]
        print("  whole by neither: %s" % (" ".join(os.path.basename(k)[:-4] for k in none) or "none"))
        for k in none:
            # Each copy carries its own edits; ask whether every byte is
            # confirmed by one of them, where both are the decoded size.
            o = dec[k]
            p = psp[k][0][2] if k in psp else None
            x = next((c for c in _pc_chunks(a.dat, k) or [] if len(c) == len(o)), None)
            if p is None or x is None or len(p) != len(o):
                print("    %s: no same-size copy on both sides" % os.path.basename(k)[:-4])
                bad += 1
                continue
            vp = sum(1 for i in range(len(o)) if o[i] != p[i])
            vx = sum(1 for i in range(len(o)) if o[i] != x[i])
            vb = sum(1 for i in range(len(o)) if o[i] != p[i] and o[i] != x[i])
            print("    %s: %d bytes differ from the PSP, %d from the PC, %d from both" % (
                os.path.basename(k)[:-4], vp, vx, vb))
            bad += vb != 0
    for other in a.disc2 or []:
        b = rd.Build("other", other)
        osecs = _sections(b, TYPE1)
        same = diff = 0
        for k, v in osecs.items():
            try:
                o = decode(v[0][2])
            except Type1Error as e:
                print("FAIL %s %s: %s" % (other, k, e))
                bad += 1
                continue
            same, diff = (same + 1, diff) if dec.get(k) == o else (same, diff + 1)
        print("%s: %d type-1 sections; %d decode to the first disc's, %d differ" % (other, sum(map(len, osecs.values())), same, diff))
    return 1 if bad else 0


def cmd_decode(a):
    import region_diff as rd
    b = rd.Build("disc", a.disc)
    v = _sections(b, TYPE1).get(a.emi)
    if not v:
        sys.exit("%s: no type-1 section in %s" % (a.disc, a.emi))
    o = decode(v[0][2])
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "wb") as f:
        f.write(o)
    print("%s section %d: %d -> %d bytes, %s" % (a.emi, v[0][0], len(v[0][2]), len(o), a.out))


# ---------------------------------------------------------------- self-test (no game data)

def _stream(size, body):
    return struct.pack("<IH", size, 0xFFFF) + bytes(body) + b"\xFF"


def _ref(d, n):
    pos = 0x800 - d
    if n >= 18:
        return [(pos >> 8) << 5 | 0x0F, pos & 0xFF, n - 18]
    return [(pos >> 8) << 5 | (n - 3), pos & 0xFF]


def _lits(data):
    body = []
    for i in range(0, len(data), 271):
        run = data[i:i + 271]
        body += ([0x10, len(run) - 16] if len(run) >= 16 else [0x20 - len(run)]) + list(run)
    return body


def cmd_selftest(_a):
    lit = bytes(range(1, 21))
    far = b"\x01\x02\x03" + bytes(2045)
    cases = [
        ("short literals", _stream(3, [0x1D, 1, 2, 3]), b"\x01\x02\x03"),
        ("one literal", _stream(1, [0x1F, 9]), b"\x09"),
        ("16+n literals", _stream(20, [0x10, 4] + list(lit)), lit),
        ("overlapping copy", _stream(9, [0x1D, 7, 8, 9] + _ref(3, 6)), b"\x07\x08\x09" * 3),
        ("run from one byte", _stream(5, [0x1F, 0xAA] + _ref(1, 4)), b"\xAA" * 5),
        ("extended length", _stream(1 + 18 + 200, [0x1F, 0x55] + _ref(1, 218)), b"\x55" * 219),
        ("distance 2048", _stream(2048 + 3, _lits(far) + _ref(2048, 3)), far + far[:3]),
    ]
    bad = 0
    for name, s, want in cases:
        try:
            got = decode(s)
        except Type1Error as e:
            got = e
        ok = got == want
        bad += not ok
        print("%-4s %s" % ("ok" if ok else "FAIL", name))
    for name, s in [("bit 4 in a reference", _stream(4, [0x1F, 1, 0x30, 0xFF])),
                    ("before the start", _stream(4, [0x1F, 1] + _ref(2, 3))),
                    ("overrun", _stream(3, [0x1F, 1] + _ref(1, 3))),
                    ("no terminator", _stream(1, [0x1F, 1])[:-1]),
                    ("truncated", _stream(5, [0x1B, 1, 2]))]:
        try:
            decode(s)
            print("FAIL %s: accepted" % name)
            bad += 1
        except Type1Error:
            print("ok   %s: refused" % name)
    print("selftest: %s" % ("FAIL" if bad else "ok"))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    c = s.add_parser("check")
    c.add_argument("--disc", required=True, help="a PSX disc (.cue, .bin or .iso)")
    c.add_argument("--psp", help="a PSP disc, whose sections ship decompressed")
    c.add_argument("--dat", help="the PC port's DAT/ directory")
    c.add_argument("--disc2", action="append", help="another PSX disc, decoded and compared with --disc")
    d = s.add_parser("decode")
    d.add_argument("--disc", required=True)
    d.add_argument("--emi", required=True, help="path under the data root, e.g. PLCHAR/PL012.EMI")
    d.add_argument("--out", required=True)
    s.add_parser("selftest")
    a = ap.parse_args()
    sys.exit({"check": cmd_check, "decode": cmd_decode, "selftest": cmd_selftest}[a.cmd](a) or 0)


if __name__ == "__main__":
    main()
