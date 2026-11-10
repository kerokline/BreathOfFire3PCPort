#!/usr/bin/env python
"""Apply a VCDIFF patch (RFC 3284), the format xdelta3 writes: a romhack's .xdelta.

    python tools/vcdiff.py info PATCH                 the header and every window's sizes; decodes nothing
    python tools/vcdiff.py apply PATCH SOURCE OUT     SOURCE patched into OUT; prints OUT's size and sha256
    python tools/vcdiff.py selftest                   patches it makes itself (no game data)

docs/romhack-mods.md (issue 216, IDEAS I58). A PlayStation hack is shared as an
xdelta of the disc's data track; this reads one in pure Python so a mod's build
step can apply the author's own file to the player's own disc. It reads:

  - the standard code table and address cache (near 4, same 3);
  - windows that copy from the source file (VCD_SOURCE), each with its Adler-32
    when the patch carries one (xdelta3 does) - a window whose sum differs is an
    error, so a patch applied to the wrong source is caught at its first window;
  - sections under xdelta3's secondary compressor 1 (DJW, a move-to-front and
    multi-table Huffman stage), written from a reading of xdelta3's
    xdelta3-djw.h (Apache-2.0; docs/THIRD_PARTY.md).

It refuses, by name: a custom code table, a VCD_TARGET window, and the secondary
compressors it does not read (2 LZMA, 16 FGK). A patch is untrusted input: every
length is checked against the patch, the source and the bounds below before it
is used, nothing in a patch names a path, and the output goes only where the
caller says.

The output of `apply` on a game disc is game data: scratch only (CLAUDE.md rule 1).
"""
import argparse
import hashlib
import mmap
import os
import sys
import zlib

MAGIC = b"\xd6\xc3\xc4"
VCD_SECONDARY, VCD_CODETABLE, VCD_APPHEADER = 1, 2, 4
VCD_SOURCE, VCD_TARGET, VCD_ADLER32 = 1, 2, 4
SECONDARY = {1: "DJW", 2: "LZMA", 16: "FGK"}

# The bounds. xdelta3's default window is 8 MiB; 64 MiB leaves room for any
# setting a hack is likely to use and stops a patch that names a huge window
# before anything is allocated.
WINDOW_MAX = 64 << 20
WINDOWS_MAX = 1 << 16
APPHEADER_MAX = 4096

ALPHABET = 256
DJW_MAX_CODELEN = 20
DJW_TOTAL_CODES = DJW_MAX_CODELEN + 2      # RUN_0, RUN_1, then code lengths 1..20
DJW_MAX_CLCLEN = 15
DJW_MAX_GBCLEN = 7
DJW_SECTOR_MULT = 5
# the code-length move-to-front list's first order (xdelta3-djw.h: 0, then
# djw_encode_12basic, then djw_encode_12extra)
DJW_CLEN_MTF = (0, 4, 5, 6, 7, 8, 9, 10, 3, 11, 2, 12, 13, 1, 14, 15, 16, 17, 18, 19, 20)


class PatchError(Exception):
    """The patch cannot be applied; the message says why in one sentence."""


# ---------------------------------------------------------------- DJW

class _Bits:
    """Bits of a byte range, each byte from its low bit up."""

    def __init__(self, buf, pos, end):
        self.b, self.p, self.end, self.cur, self.mask = buf, pos, end, 0, 0x100

    def bit(self):
        if self.mask == 0x100:
            if self.p >= self.end:
                raise PatchError("a compressed section ends inside a code")
            self.cur = self.b[self.p]
            self.p += 1
            self.mask = 1
        v = 1 if self.cur & self.mask else 0
        self.mask <<= 1
        return v

    def bits(self, n):
        v = 0
        for _ in range(n):
            v = (v << 1) | self.bit()
        return v


def _build_decoder(clen, abs_max):
    """(inorder, base, limit, min, max) for a canonical prefix code given each
    symbol's code length (0: no code)."""
    nr = [0] * (abs_max + 2)
    for l in clen:
        nr[l] += 1
    mn = 1
    while mn <= abs_max and not nr[mn]:
        mn += 1
    mx = abs_max
    while mx and not nr[mx]:
        mx -= 1
    if mn > mx:
        raise PatchError("a compressed section holds a code table with no codes")
    base, limit, tmp = [0] * (abs_max + 2), [0] * (abs_max + 2), [0] * (abs_max + 2)
    limit[mn] = nr[mn] - 1
    for i in range(mn + 1, mx + 1):
        last = (limit[i - 1] + 1) << 1
        tmp[i] = tmp[i - 1] + nr[i - 1]
        limit[i] = last + nr[i] - 1
        base[i] = last - tmp[i]
    inorder = [0] * sum(1 for l in clen if l)
    for i, l in enumerate(clen):
        if l:
            inorder[tmp[l]] = i
            tmp[l] += 1
    return inorder, base, limit, mn, mx


def _symbol(bs, dec):
    inorder, base, limit, mn, mx = dec
    code = n = 0
    while True:
        if n == mx:
            raise PatchError("a compressed section holds a code that is in no table")
        n += 1
        code = (code << 1) | bs.bit()
        if n >= mn and code <= limit[n]:
            break
    off = code - base[n]
    if not 0 <= off < len(inorder):
        raise PatchError("a compressed section holds a code that is in no table")
    return inorder[off]


def _decode_1_2(bs, dec, mtf, count, skip, values):
    """`count` values, move-to-front coded with two run codes (as bzip2's RUNA
    and RUNB). `skip`: a value is 0 without a code when the value `skip` places
    before it is 0 (a later table's lengths under the first table's)."""
    n = rep = pending = s = 0
    while n < count:
        if skip and n >= skip and values[n - skip] == 0:
            values[n] = 0
            n += 1
        elif rep:
            values[n] = mtf[0]
            n += 1
            rep -= 1
        elif pending:
            v = mtf.pop(pending)
            mtf.insert(0, v)
            values[n] = v
            n += 1
            pending = 0
        else:
            sym = _symbol(bs, dec)
            if sym <= 1:
                rep = (sym + 1) << s
                s += 1
            else:
                pending = sym - 1
                if pending >= len(mtf):
                    raise PatchError("a compressed section holds a table value out of range")
                s = 0
    if rep:
        raise PatchError("a compressed section holds a run longer than its table")


def djw_decode(buf, pos, end, size):
    """`size` bytes from the DJW stream buf[pos:end]."""
    bs = _Bits(buf, pos, end)
    groups = bs.bits(3) + 1
    sector = (bs.bits(5) + 1) * DJW_SECTOR_MULT if groups > 1 else size
    sectors = 1 + (size - 1) // sector
    count = bs.bits(4) + 7
    cl_clen = [bs.bits(4) for _ in range(count)] + [0] * (DJW_TOTAL_CODES - count)
    cl_dec = _build_decoder(cl_clen, DJW_MAX_CLCLEN)
    clen = [0] * (ALPHABET * groups)
    _decode_1_2(bs, cl_dec, list(DJW_CLEN_MTF), ALPHABET * groups, ALPHABET, clen)
    decs = [_build_decoder(clen[g * ALPHABET:(g + 1) * ALPHABET], DJW_MAX_CODELEN) for g in range(groups)]
    sel = [0] * sectors
    if groups > 1:
        sel_dec = _build_decoder([bs.bits(3) for _ in range(groups + 1)], DJW_MAX_GBCLEN)
        _decode_1_2(bs, sel_dec, list(range(groups)), sectors, 0, sel)
    out = bytearray(size)
    b, p, cur, mask = bs.b, bs.p, bs.cur, bs.mask      # the symbol loop, inlined: it is the hot one
    o = 0
    try:
        for g in sel:
            inorder, base, limit, mn, mx = decs[g]
            for _ in range(min(sector, size - o)):
                code = k = 0
                while True:
                    if mask == 0x100:
                        if p >= end:
                            raise PatchError("a compressed section ends inside a code")
                        cur = b[p]
                        p += 1
                        mask = 1
                    k += 1
                    code <<= 1
                    if cur & mask:
                        code |= 1
                    mask <<= 1
                    if k >= mn and code <= limit[k]:
                        break
                    if k == mx:
                        raise PatchError("a compressed section holds a code that is in no table")
                out[o] = inorder[code - base[k]]
                o += 1
    except IndexError:
        raise PatchError("a compressed section holds a code that is in no table")
    if p != end:
        raise PatchError("a compressed section holds %d bytes after its last code" % (end - p))
    return bytes(out)


# ---------------------------------------------------------------- VCDIFF

def _varint(b, p, end):
    v = 0
    for _ in range(10):
        if p >= end:
            raise PatchError("the patch ends inside a number")
        c = b[p]
        p += 1
        v = (v << 7) | (c & 0x7F)
        if not c & 0x80:
            return v, p
    raise PatchError("the patch holds a number longer than 10 bytes")


def _code_table():
    """The default instruction table (RFC 3284 section 5.6): 256 pairs of
    (type, size, mode); type 0 none, 1 ADD, 2 RUN, 3 COPY; size 0: read it."""
    t = [(2, 0, 0, 0, 0, 0)]
    t += [(1, s, 0, 0, 0, 0) for s in range(18)]
    for m in range(9):
        t += [(3, 0, m, 0, 0, 0)] + [(3, s, m, 0, 0, 0) for s in range(4, 19)]
    for m in range(6):
        t += [(1, a, 0, 3, c, m) for a in range(1, 5) for c in range(4, 7)]
    for m in range(6, 9):
        t += [(1, a, 0, 3, 4, m) for a in range(1, 5)]
    t += [(3, 4, m, 1, 1, 0) for m in range(9)]
    assert len(t) == 256
    return t


TABLE = _code_table()


def header(patch):
    """(secondary compressor id or None, application header bytes, offset of the first window)."""
    if len(patch) < 5 or patch[:3] != MAGIC:
        raise PatchError("not a VCDIFF patch (an .xdelta file starts D6 C3 C4)")
    if patch[3] != 0:
        raise PatchError("VCDIFF version %d; this reads version 0" % patch[3])
    ind, p, n = patch[4], 5, len(patch)
    if ind & ~7:
        raise PatchError("the patch header has a flag this does not know (0x%02X)" % ind)
    sec = None
    if ind & VCD_SECONDARY:
        if p >= n:
            raise PatchError("the patch ends inside its header")
        sec = patch[p]
        p += 1
    if ind & VCD_CODETABLE:
        raise PatchError("the patch carries its own instruction table; this reads the standard table only")
    app = b""
    if ind & VCD_APPHEADER:
        ln, p = _varint(patch, p, n)
        if ln > APPHEADER_MAX or p + ln > n:
            raise PatchError("the patch's application header is %d bytes, over %d or past the end" % (ln, APPHEADER_MAX))
        app = bytes(patch[p:p + ln])
        p += ln
    if sec is not None and sec != 1:
        raise PatchError("the patch's sections are compressed with %s (id %d); this reads DJW (xdelta3 -S djw) and "
                         "uncompressed sections only" % (SECONDARY.get(sec, "an unknown compressor"), sec))
    return sec, app, p


def windows(patch):
    """Each window's header, read without decoding: a dict of its numbers and
    the offsets of its three sections."""
    _, _, p = header(patch)
    n = len(patch)
    count = 0
    while p < n:
        count += 1
        if count > WINDOWS_MAX:
            raise PatchError("the patch holds more than %d windows" % WINDOWS_MAX)
        w = {"index": count - 1, "indicator": patch[p], "source_len": 0, "source_pos": 0}
        p += 1
        if w["indicator"] & ~7:
            raise PatchError("window %d has a flag this does not know (0x%02X)" % (w["index"], w["indicator"]))
        if w["indicator"] & VCD_TARGET:
            raise PatchError("window %d copies from the patched file itself (VCD_TARGET); this reads windows that "
                             "copy from the source file only" % w["index"])
        if w["indicator"] & VCD_SOURCE:
            w["source_len"], p = _varint(patch, p, n)
            w["source_pos"], p = _varint(patch, p, n)
        dlen, p = _varint(patch, p, n)
        end = p + dlen
        if end > n:
            raise PatchError("window %d runs past the end of the patch" % w["index"])
        w["target_len"], p = _varint(patch, p, end)
        if p >= end:
            raise PatchError("window %d ends inside its header" % w["index"])
        w["delta_indicator"] = patch[p]
        p += 1
        if w["delta_indicator"] & ~7:
            raise PatchError("window %d has a section flag this does not know" % w["index"])
        lens = []
        for _ in range(3):
            v, p = _varint(patch, p, end)
            lens.append(v)
        w["adler32"] = None
        if w["indicator"] & VCD_ADLER32:
            if p + 4 > end:
                raise PatchError("window %d ends inside its checksum" % w["index"])
            w["adler32"] = int.from_bytes(patch[p:p + 4], "big")
            p += 4
        if p + sum(lens) != end:
            raise PatchError("window %d's sections are %d bytes, its body %d" % (w["index"], sum(lens), end - p))
        if w["target_len"] > WINDOW_MAX or w["source_len"] > (1 << 40):
            raise PatchError("window %d writes %d bytes, over the %d a window may" % (w["index"], w["target_len"], WINDOW_MAX))
        w["sections"] = []
        for ln in lens:
            w["sections"].append((p, p + ln))
            p += ln
        yield w


def _section(patch, span, compressed, what, w):
    p, end = span
    if not compressed:
        return bytes(patch[p:end])
    size, q = _varint(patch, p, end)
    if size == 0 or size > WINDOW_MAX + (WINDOW_MAX >> 2):
        raise PatchError("window %d's %s section unpacks to %d bytes" % (w["index"], what, size))
    return djw_decode(patch, q, end, size)


def decode_window(patch, w, source):
    """The bytes window `w` writes. `source`: bytes, a bytearray or an mmap."""
    di = w["delta_indicator"]
    data = _section(patch, w["sections"][0], di & 1, "data", w)
    inst = _section(patch, w["sections"][1], di & 2, "instruction", w)
    addr = _section(patch, w["sections"][2], di & 4, "address", w)
    slen, spos, tlen, i = w["source_len"], w["source_pos"], w["target_len"], w["index"]
    if spos + slen > len(source):
        raise PatchError("window %d copies from bytes 0x%X..0x%X of the source, which is 0x%X bytes: the patch was "
                         "made against another file" % (i, spos, spos + slen, len(source)))
    seg = source[spos:spos + slen] if slen else b""
    tgt = bytearray(tlen)
    t = dp = ip = ap = nx = 0
    near, same = [0, 0, 0, 0], [0] * 768
    nd, ni, na = len(data), len(inst), len(addr)
    try:
        while ip < ni:
            e = TABLE[inst[ip]]
            ip += 1
            for k in (0, 3):
                typ = e[k]
                if not typ:
                    continue
                size, mode = e[k + 1], e[k + 2]
                if size == 0:
                    size, ip = _varint(inst, ip, ni)
                if t + size > tlen:
                    raise PatchError("window %d writes past its own length" % i)
                if typ == 1:
                    if dp + size > nd:
                        raise PatchError("window %d reads past its data section" % i)
                    tgt[t:t + size] = data[dp:dp + size]
                    dp += size
                elif typ == 2:
                    if dp >= nd:
                        raise PatchError("window %d reads past its data section" % i)
                    tgt[t:t + size] = data[dp:dp + 1] * size
                    dp += 1
                else:
                    here = slen + t
                    if mode < 6:
                        a, ap = _varint(addr, ap, na)
                        if mode == 1:
                            a = here - a
                        elif mode > 1:
                            a += near[mode - 2]
                    else:
                        a = same[(mode - 6) * 256 + addr[ap]]
                        ap += 1
                    if a < 0 or a >= here:
                        raise PatchError("window %d copies from an address outside what exists" % i)
                    near[nx] = a
                    nx = (nx + 1) & 3
                    same[a % 768] = a
                    if a + size <= slen:
                        tgt[t:t + size] = seg[a:a + size]
                    elif a >= slen:
                        o = a - slen
                        if o + size <= t:
                            tgt[t:t + size] = tgt[o:o + size]
                        else:                       # the copy overlaps what it writes: a repeating run
                            period = t - o
                            run = bytes(tgt[o:t])
                            tgt[t:t + size] = (run * (size // period + 1))[:size]
                    else:
                        raise PatchError("window %d has a copy that runs from the source into the target" % i)
                t += size
    except IndexError:
        raise PatchError("window %d's instructions run past a section" % i)
    if t != tlen:
        raise PatchError("window %d writes %d bytes, its header says %d" % (i, t, tlen))
    if w["adler32"] is not None and zlib.adler32(tgt) & 0xFFFFFFFF != w["adler32"]:
        raise PatchError("window %d does not come out as the patch's checksum says: the source is not the file the "
                         "patch was made against" % i)
    return tgt


def apply(patch, source, write, max_out=None):
    """Every window of `patch` over `source`, each handed to write(bytes) in
    order; the total written. `max_out`: refuse a patch that writes more."""
    total = 0
    for w in windows(patch):
        if max_out is not None and total + w["target_len"] > max_out:
            raise PatchError("the patch writes more than %d bytes" % max_out)
        write(decode_window(patch, w, source))
        total += w["target_len"]
    return total


def apply_file(patch_path, source_path, out_path, max_out=None):
    """(size, sha256) of OUT. OUT is written beside itself and renamed, so a
    patch that fails leaves no file."""
    with open(patch_path, "rb") as f:
        patch = f.read()
    h = hashlib.sha256()
    part = out_path + ".part"
    try:
        with open(source_path, "rb") as sf, open(part, "wb") as out:
            if os.fstat(sf.fileno()).st_size == 0:
                raise PatchError("the source file is empty")
            with mmap.mmap(sf.fileno(), 0, access=mmap.ACCESS_READ) as source:
                def write(b):
                    out.write(b)
                    h.update(b)
                total = apply(patch, source, write, max_out)
        os.replace(part, out_path)
    finally:
        if os.path.exists(part):
            os.remove(part)
    return total, h.hexdigest()


# ---------------------------------------------------------------- the self-test's encoder

class _BitsOut:
    def __init__(self):
        self.out, self.cur, self.n = bytearray(), 0, 0

    def bits(self, v, n):
        for i in range(n - 1, -1, -1):
            if (v >> i) & 1:
                self.cur |= 1 << self.n
            self.n += 1
            if self.n == 8:
                self.out.append(self.cur)
                self.cur = self.n = 0

    def done(self):
        if self.n:
            self.out.append(self.cur)
        return bytes(self.out)


def _lengths(freq, maxlen):
    """Huffman code lengths for the symbols with a count; flat when the tree is too deep."""
    import heapq
    live = [s for s, f in enumerate(freq) if f]
    out = [0] * len(freq)
    if len(live) == 1:
        out[live[0]] = 1
        return out
    heap = [(freq[s], s, (s,)) for s in live]
    heapq.heapify(heap)
    while len(heap) > 1:
        fa, ka, a = heapq.heappop(heap)
        fb, kb, b = heapq.heappop(heap)
        for s in a + b:
            out[s] += 1
        heapq.heappush(heap, (fa + fb, min(ka, kb), a + b))
    if max(out) > maxlen:
        flat = max(1, (len(live) - 1).bit_length())
        out = [flat if f else 0 for f in freq]
    return out


def _codes(clen, abs_max):
    inorder, base, limit, mn, mx = _build_decoder(clen, abs_max)
    codes = {}
    for pos, sym in enumerate(inorder):
        l = clen[sym]
        codes[sym] = (base[l] + pos, l)
    return codes


def _encode_1_2(values, mtf, skip):
    """The symbols (0, 1: run codes; i + 1: list place i) for `values`."""
    mtf = list(mtf)
    out, run = [], 0

    def flush(run):
        while run:
            if run & 1:
                out.append(0)
                run -= 1
            else:
                out.append(1)
                run -= 2
            run >>= 1

    for n, v in enumerate(values):
        if skip and n >= skip and values[n - skip] == 0:
            assert v == 0
            continue
        i = mtf.index(v)
        if i == 0:
            run += 1
        else:
            flush(run)
            run = 0
            out.append(i + 1)
            mtf.insert(0, mtf.pop(i))
    flush(run)
    return out


def djw_encode(data, groups=1, sector=10):
    """A DJW stream djw_decode reads back as `data` (the self-test's; not xdelta3's choices)."""
    assert data and 1 <= groups <= 8 and sector % DJW_SECTOR_MULT == 0
    w = _BitsOut()
    w.bits(groups - 1, 3)
    if groups > 1:
        w.bits(sector // DJW_SECTOR_MULT - 1, 5)
    else:
        sector = len(data)
    parts = [data[i:i + sector] for i in range(0, len(data), sector)]
    sel = [i % groups for i in range(len(parts))]
    clen = []
    for g in range(groups):       # table 0 covers every byte; a later one only its own sectors'
        freq = [0] * ALPHABET
        for part, s in zip(parts, sel):
            if g == 0 or s == g:
                for c in part:
                    freq[c] += 1
        if not any(freq):
            freq[data[0]] = 1
        clen += _lengths(freq, DJW_MAX_CODELEN)
    w.bits(DJW_TOTAL_CODES - 7, 4)
    for _ in range(DJW_TOTAL_CODES):
        w.bits(5, 4)                           # every code-length symbol: 5 bits, its own number
    for sym in _encode_1_2(clen, DJW_CLEN_MTF, ALPHABET):
        w.bits(sym, 5)
    tables = [_codes(clen[g * ALPHABET:(g + 1) * ALPHABET], DJW_MAX_CODELEN) for g in range(groups)]
    if groups > 1:
        for _ in range(groups + 1):
            w.bits(4, 3)
        for sym in _encode_1_2(sel, range(groups), 0):
            w.bits(sym, 4)
    for part, s in zip(parts, sel):
        for c in part:
            w.bits(*tables[s][c])
    return w.done()


def _enc_varint(v):
    out = [v & 0x7F]
    v >>= 7
    while v:
        out.append(0x80 | (v & 0x7F))
        v >>= 7
    return bytes(reversed(out))


def encode_window(ops, source_len, source_pos, compress=0, adler=True, groups=1):
    """One window's parts from `ops`, one instruction an opcode: ("add", bytes),
    ("run", byte, n), ("copy", addr, size, mode) with addr in the window's
    address space (the source segment, then the target), ("add1copy4", byte,
    addr) the double opcode 163. (target length, the three sections, the
    sections compressed, adler)."""
    data, inst, addr = bytearray(), bytearray(), bytearray()
    near, same, nx, t = [0, 0, 0, 0], [0] * 768, 0, 0
    for op in ops:
        if op[0] == "add":
            b = op[1]
            if 1 <= len(b) <= 17:
                inst.append(1 + len(b))
            else:
                inst.append(1)
                inst += _enc_varint(len(b))
            data += b
            t += len(b)
        elif op[0] == "run":
            inst.append(0)
            inst += _enc_varint(op[2])
            data.append(op[1])
            t += op[2]
        elif op[0] == "add1copy4":           # opcode 163: ADD 1 then COPY 4, mode 0
            inst.append(163)
            data += op[1]
            addr += _enc_varint(op[2])
            near[nx] = op[2]
            nx = (nx + 1) & 3
            same[op[2] % 768] = op[2]
            t += 5
        else:
            _, a, size, mode = op
            here = source_len + t
            if 4 <= size <= 18:
                inst.append(19 + 16 * mode + size - 3)
            else:
                inst.append(19 + 16 * mode)
                inst += _enc_varint(size)
            if mode == 0:
                addr += _enc_varint(a)
            elif mode == 1:
                addr += _enc_varint(here - a)
            elif mode < 6:
                assert a >= near[mode - 2]
                addr += _enc_varint(a - near[mode - 2])
            else:
                assert same[a % 768] == a and (a % 768) // 256 == mode - 6
                addr.append(a % 256)
            near[nx] = a
            nx = (nx + 1) & 3
            same[a % 768] = a
            t += size
    secs = []
    for bit, b in ((1, bytes(data)), (2, bytes(inst)), (4, bytes(addr))):
        if compress & bit and b:
            b = _enc_varint(len(b)) + djw_encode(b, groups)
        else:
            compress &= ~bit
        secs.append(b)
    return t, secs, compress, adler


def make_patch(source, wins, secondary=None, app=b""):
    """A patch and the target it makes. wins: [(ops, source_len, source_pos, compress, adler, groups)]."""
    out = bytearray(MAGIC + b"\x00")
    out.append((VCD_SECONDARY if secondary is not None else 0) | (VCD_APPHEADER if app else 0))
    if secondary is not None:
        out.append(secondary)
    if app:
        out += _enc_varint(len(app)) + app
    target = bytearray()
    for ops, slen, spos, compress, adler, groups in wins:
        seg = source[spos:spos + slen]
        tgt = bytearray()
        for op in ops:                       # the reference result, by the instructions' meaning
            if op[0] == "add":
                tgt += op[1]
            elif op[0] == "run":
                tgt += bytes([op[1]]) * op[2]
            elif op[0] == "add1copy4":
                tgt += op[1]
                tgt += seg[op[2]:op[2] + 4]
            else:
                for j in range(op[2]):
                    a = op[1] + j
                    tgt.append(seg[a] if a < slen else tgt[a - slen])
        tlen, secs, compress, adler = encode_window(ops, slen, spos, compress, adler, groups)
        assert tlen == len(tgt)
        body = _enc_varint(tlen) + bytes([compress]) + b"".join(_enc_varint(len(s)) for s in secs)
        if adler:
            body += (zlib.adler32(tgt) & 0xFFFFFFFF).to_bytes(4, "big")
        body += b"".join(secs)
        out.append((VCD_SOURCE if slen else 0) | (VCD_ADLER32 if adler else 0))
        if slen:
            out += _enc_varint(slen) + _enc_varint(spos)
        out += _enc_varint(len(body)) + body
        target += tgt
    return bytes(out), bytes(target)


def selftest():
    import random
    fails = []

    def expect(cond, what):
        print("%-78s %s" % (what, "ok" if cond else "FAIL"))
        if not cond:
            fails.append(what)

    def run(patch, source, max_out=None):
        got = bytearray()
        apply(patch, source, got.extend, max_out)
        return bytes(got)

    def refused(patch, source, word, max_out=None):
        try:
            run(patch, source, max_out)
        except PatchError as e:
            return word in str(e), str(e)
        return False, "applied"

    rng = random.Random(216)
    source = bytes(rng.randrange(256) for _ in range(5000))
    text = b"".join(bytes([rng.choice(b"abcdeeeeeefg   \n")]) for _ in range(900))
    ops1 = [("copy", 100, 700, 0),              # SELF, from the source
            ("add", b"hello, patch"),
            ("run", 0x2A, 300),
            ("copy", 1500, 18, 1),              # HERE
            ("copy", 1600, 40, 2),              # NEAR 0 (after address 1500)
            ("copy", 1500, 5, 6 + (1500 % 768) // 256),   # SAME
            ("add", text),
            ("copy", 4000 + 700, 64, 0),        # from the target written so far (the first copy's bytes)
            ("add", b"xyz"),
            ("copy", 4000 + 700 + 12 + 300 + 18 + 40 + 5 + 900 + 64, 50, 0),   # overlaps what it writes: "xyz" repeated
            ("add1copy4", b"Q", 77)]
    ops2 = [("copy", 0, 2000, 0), ("add", text[:400]), ("copy", 1000, 1000, 1)]
    for name, comp, sec, groups in (("no secondary compression", 0, None, 1),
                                    ("DJW on all three sections, one table", 7, 1, 1),
                                    ("DJW, two tables", 7, 1, 2),
                                    ("DJW on the data section alone", 1, 1, 1)):
        patch, target = make_patch(source, [(ops1, 4000, 0, comp, True, groups), (ops2, 3000, 2000, comp, True, groups)],
                                   sec, b"selftest//source/")
        try:
            got = run(patch, source)
        except PatchError as e:
            got = str(e)
        expect(got == target, "two windows, every instruction and address mode: %s" % name)
    patch, target = make_patch(source, [(ops1, 4000, 0, 7, True, 2), (ops2, 3000, 2000, 7, False, 1)], 1)
    expect(header(patch)[0] == 1 and [w["target_len"] for w in windows(patch)] == [len(target) - 3400, 3400],
           "the header and the windows read without decoding")
    for n in (1, 2, 7, 3000):
        d = bytes(rng.choice(b"\x00\x00\x00\x01\x02\xff") for _ in range(n))
        for g in (1, 2, 3):
            e = djw_encode(d, g)
            expect(djw_decode(e, 0, len(e), n) == d, "DJW round trip: %d bytes, %d table(s)" % (n, g))
    d = bytes(range(256)) * 3
    e = djw_encode(d, 2, 160)
    expect(djw_decode(e, 0, len(e), len(d)) == d, "DJW round trip: every byte value, sectors of 160")

    # the refusals
    good, target = make_patch(source, [(ops1, 4000, 0, 7, True, 1)], 1)
    cases = [
        ("not a patch", b"PK\x03\x04" + good[4:], source, "not a VCDIFF patch"),
        ("another source: the window's checksum", good, bytes(reversed(source)), "checksum"),
        ("a source shorter than the window copies from", good, source[:1000], "made against another file"),
        ("cut short", good[:len(good) - 40], source, "past the end"),
        ("a secondary compressor it does not read (LZMA)", good[:5] + b"\x02" + good[6:], source, "LZMA"),
        ("its own instruction table", good[:4] + bytes([good[4] | 2]) + good[5:], source, "instruction table"),
        ("more output than the caller allows", good, source, "writes more than", ),
    ]
    for what, patch, src, word in cases:
        ok, said = refused(patch, src, word, 100 if "allows" in what else None)
        expect(ok, "refused: %s (%s)" % (what, said[:40]))
    bad, _ = make_patch(source, [([("copy", 3990, 20, 0)], 4000, 0, 0, False, 1)])
    ok, said = refused(bad, source, "from the source into the target")
    expect(ok, "refused: a copy across the end of the source segment")
    w0 = bytearray(make_patch(source, [([("add", b"abcd")], 0, 0, 0, False, 1)])[0])
    w0[5] |= VCD_TARGET
    ok, said = refused(bytes(w0), source, "VCD_TARGET")
    expect(ok, "refused: a window that copies from the patched file")
    junk = good[:len(good) - 300] + bytes(rng.randrange(256) for _ in range(300))
    try:
        run(junk, source)
        ok = False
    except PatchError:
        ok = True
    except Exception as e:      # anything else is a hole in the checks
        ok = False
        print("   ", type(e).__name__, e)
    expect(ok, "damaged sections: an error of this tool's, not a crash")
    for seed in range(200):
        r = random.Random(seed)
        b = bytearray(good)
        for _ in range(r.randrange(1, 6)):
            b[r.randrange(5, len(b))] = r.randrange(256)
        try:
            run(bytes(b), source, 1 << 20)
        except PatchError:
            pass
        except Exception as e:
            fails.append("fuzz seed %d: %s %s" % (seed, type(e).__name__, e))
    expect(not any(f.startswith("fuzz") for f in fails), "200 patches with random bytes changed: applied or refused, no crash")
    print("vcdiff selftest: %s" % ("FAILED (%d)" % len(fails) if fails else "ok"))
    for f in fails:
        print("  " + f)
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("info")
    s.add_argument("patch")
    s = sub.add_parser("apply")
    s.add_argument("patch")
    s.add_argument("source")
    s.add_argument("out")
    sub.add_parser("selftest")
    a = ap.parse_args()
    if a.cmd == "selftest":
        return selftest()
    try:
        if a.cmd == "info":
            with open(a.patch, "rb") as f:
                patch = f.read()
            sec, app, _ = header(patch)
            print("%s: %d bytes, sha256 %s" % (a.patch, len(patch), hashlib.sha256(patch).hexdigest()))
            print("secondary compressor: %s" % ("none" if sec is None else SECONDARY.get(sec, sec)))
            print("application header: %s" % (app.decode("latin1") if app else "(none)"))
            total = n = 0
            for w in windows(patch):
                print("window %3d  writes 0x%X at 0x%X  source 0x%X..0x%X  sections %d/%d/%d  checksum %s" % (
                    w["index"], w["target_len"], total, w["source_pos"], w["source_pos"] + w["source_len"],
                    *(e - s for s, e in w["sections"]), "yes" if w["adler32"] is not None else "no"))
                total += w["target_len"]
                n += 1
            print("%d windows, %d bytes written" % (n, total))
        else:
            size, sha = apply_file(a.patch, a.source, a.out)
            print("%s: %d bytes, sha256 %s" % (a.out, size, sha))
    except PatchError as e:
        print("error %s" % e)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
