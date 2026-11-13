#!/usr/bin/env python
"""A .7z archive read in pure Python (the standard library's lzma does the unpacking).

    python tools/sevenzip.py list ARCHIVE          each member's size and name
    python tools/sevenzip.py read ARCHIVE NAME OUT one member written to OUT
    python tools/sevenzip.py selftest              on archives it makes itself (no game data)

The archive a romhack's author releases is often a .7z of his patch file and
a readme (docs/mods.md 12.2). tools/mods.py reads the patch out of it with
this, so the player adds the file as it was downloaded. Nothing here writes a
member to a path the archive names: `read` gives bytes, and the caller says
where they go.

The layout (7-Zip's 7zFormat.txt):

    0   "7z" BC AF 27 1C    the signature
    6   2 bytes             the format version
    8   4 bytes             the CRC of the next 20 bytes
    12  8 + 8 + 4 bytes     where the header is (from byte 32), its size, its CRC
    32  the packed streams, then the header

The header is a tree of one-byte ids. It names the packed streams, the
*folders* (each a chain of coders that turns packed streams into one unpacked
stream), how each folder's stream is cut into files, and the files' names.
The header is most often packed itself ("encoded header": a small streams
description, whose one folder unpacks to the real header).

What is read: the coders Copy, LZMA, LZMA2, Deflate and BZip2, the Delta
filter on LZMA or LZMA2 and the x86 (BCJ) filter on LZMA2. 7-Zip puts a filter
on programs only, which no caller here reads. What is refused, each by name: an
encrypted archive, PPMd, BCJ2 and any other coder, an archive in more than one
volume, and one whose header names streams outside itself. Every size the
header gives is checked against a limit before anything of that size is made.
"""
import bz2
import lzma
import os
import struct
import sys
import zlib

SIG = b"7z\xbc\xaf\x27\x1c"
HEADER_MAX = 4 << 20        # the header, unpacked
FILES_MAX = 4096            # members
FOLDERS_MAX = 4096
DICT_MAX = 128 << 20        # an LZMA dictionary: the memory unpacking takes
FOLDER_MAX = 64 << 20       # one folder unpacked, when the caller gives no limit

COPY, LZMA1, LZMA2, DELTA, DEFLATE, BZIP2 = b"\x00", b"\x03\x01\x01", b"\x21", b"\x03", b"\x04\x01\x08", b"\x04\x02\x02"
X86 = (b"\x03\x03\x01\x03", b"\x04")
NAMED = {b"\x06\xf1\x07\x01": "it is encrypted", b"\x03\x04\x01": "it is packed with PPMd",
         b"\x03\x03\x01\x1b": "it is packed with BCJ2", b"\x04\x01\x09": "it is packed with Deflate64"}


class ArchiveError(Exception):
    """A file that is not a .7z archive, a damaged one, or one packed in a way this does not read."""


class _Reader:
    def __init__(self, b):
        self.b, self.p = b, 0

    def byte(self):
        if self.p >= len(self.b):
            raise ArchiveError("the .7z header ends early")
        self.p += 1
        return self.b[self.p - 1]

    def take(self, n):
        if n < 0 or self.p + n > len(self.b):
            raise ArchiveError("the .7z header ends early")
        self.p += n
        return self.b[self.p - n:self.p]

    def num(self):
        """The format's number: the first byte's leading 1 bits say how many bytes follow."""
        first, mask, v = self.byte(), 0x80, 0
        for i in range(8):
            if not first & mask:
                return v | ((first & (mask - 1)) << (8 * i))
            v |= self.byte() << (8 * i)
            mask >>= 1
        return v

    def count(self, most, what):
        n = self.num()
        if n > most:
            raise ArchiveError("the .7z header names %d %s, over the %d this reads" % (n, what, most))
        return n

    def bits(self, n):
        out, b, mask = [], 0, 0
        for _ in range(n):
            if not mask:
                b, mask = self.byte(), 0x80
            out.append(bool(b & mask))
            mask >>= 1
        return out

    def digests(self, n):
        defined = [True] * n if self.byte() else self.bits(n)
        return [int.from_bytes(self.take(4), "little") if d else None for d in defined]


def _folder(r):
    n = r.count(8, "coders in a folder")
    if n == 0:
        raise ArchiveError("a .7z folder with no coder")
    coders, n_in, n_out = [], 0, 0
    for _ in range(n):
        flag = r.byte()
        if flag & 0xC0:
            raise ArchiveError("a .7z coder with flags this does not read")
        cid = bytes(r.take(flag & 15))
        c_in, c_out = (r.count(8, "streams of a coder"), r.count(8, "streams of a coder")) if flag & 0x10 else (1, 1)
        props = bytes(r.take(r.count(256, "bytes of a coder's properties"))) if flag & 0x20 else b""
        coders.append({"id": cid, "props": props, "simple": (c_in, c_out) == (1, 1)})
        n_in, n_out = n_in + c_in, n_out + c_out
    if n_out == 0:
        raise ArchiveError("a .7z folder with no output")
    binds = [(r.num(), r.num()) for _ in range(n_out - 1)]              # (in stream, the out stream it reads)
    n_packed = n_in - len(binds)
    if n_packed < 1:
        raise ArchiveError("a .7z folder with no packed stream")
    bound = {i for i, _ in binds}
    packed = [r.num() for _ in range(n_packed)] if n_packed > 1 else [i for i in range(n_in) if i not in bound][:1]
    final = [o for o in range(n_out) if o not in {o for _, o in binds}]
    if len(final) != 1 or len(packed) != n_packed:
        raise ArchiveError("a .7z folder whose coders are not one chain")
    return {"coders": coders, "binds": binds, "packed": packed, "final": final[0], "n_out": n_out, "crc": None}


def _streams(r):
    """{pack_pos, pack_sizes, folders (each with size, crc, files: [(size, crc)])}
    for a streams description; the reader is left after its end byte."""
    out = {"pack_pos": 0, "pack_sizes": [], "folders": []}
    t = r.byte()
    if t == 6:                                                          # the packed streams
        out["pack_pos"] = r.num()
        n = r.count(FOLDERS_MAX, "packed streams")
        t = r.byte()
        while t != 0:
            if t == 9:
                out["pack_sizes"] = [r.num() for _ in range(n)]
            elif t == 0xA:
                r.digests(n)
            else:
                raise ArchiveError("the .7z header has a part this does not read (0x%02X)" % t)
            t = r.byte()
        if len(out["pack_sizes"]) != n:
            raise ArchiveError("the .7z header does not give its packed streams' sizes")
        t = r.byte()
    folders = out["folders"]
    if t == 7:                                                          # the folders
        if r.byte() != 0xB:
            raise ArchiveError("the .7z header's folders are not where they must be")
        n = r.count(FOLDERS_MAX, "folders")
        if r.byte() != 0:
            raise ArchiveError("the .7z header names streams outside the archive")
        folders.extend(_folder(r) for _ in range(n))
        if r.byte() != 0xC:
            raise ArchiveError("the .7z header does not give its folders' sizes")
        for f in folders:
            f["sizes"] = [r.num() for _ in range(f["n_out"])]
            f["size"] = f["sizes"][f["final"]]
        t = r.byte()
        if t == 0xA:
            for f, c in zip(folders, r.digests(n)):
                f["crc"] = c
            t = r.byte()
        if t != 0:
            raise ArchiveError("the .7z header has a part this does not read (0x%02X)" % t)
        t = r.byte()
    for f in folders:
        f["files"] = [(f["size"], f["crc"])]
    if t == 8:                                                          # how each folder is cut into files
        t = r.byte()
        counts = [1] * len(folders)
        if t == 0xD:
            counts = [r.count(FILES_MAX, "files in a folder") for _ in folders]
            t = r.byte()
        sizes = []
        for f, n in zip(folders, counts):
            got = []
            for _ in range(n - 1):
                if t != 9:
                    raise ArchiveError("the .7z header does not give its files' sizes")
                got.append(r.num())
            if n:
                got.append(f["size"] - sum(got))
                if got[-1] < 0:
                    raise ArchiveError("the .7z header's file sizes are more than their folder")
            sizes.append(got)
        if t == 9:
            t = r.byte()
        need = sum(n for f, n in zip(folders, counts) if not (n == 1 and f["crc"] is not None))
        crcs = [None] * need
        while t != 0:
            if t == 0xA:
                if need > FILES_MAX:
                    raise ArchiveError("the .7z header names %d files, over the %d this reads" % (need, FILES_MAX))
                crcs = r.digests(need)
            else:
                r.take(r.num())
            t = r.byte()
        crcs = iter(crcs)
        for f, n, got in zip(folders, counts, sizes):
            f["files"] = [(got[0], f["crc"])] if n == 1 and f["crc"] is not None else [(s, next(crcs)) for s in got]
        t = r.byte()
    if t != 0:
        raise ArchiveError("the .7z header has a part this does not read (0x%02X)" % t)
    packs = sum(len(f["packed"]) for f in folders)
    if packs != len(out["pack_sizes"]):
        raise ArchiveError("the .7z header's folders and packed streams do not agree")
    return out


def _lzma_dict(n):
    if n > DICT_MAX:
        raise ArchiveError("it is packed with a dictionary of %d bytes, over the %d this reads" % (n, DICT_MAX))
    return max(n, 4096)


def _unpack(folder, packed, limit):
    """A folder's unpacked bytes from its one packed stream."""
    coders = folder["coders"]
    for c in coders:
        if c["id"] in NAMED:
            raise ArchiveError(NAMED[c["id"]])
    if len(folder["packed"]) != 1 or not all(c["simple"] for c in coders):
        raise ArchiveError("it is packed with a coder of more than one stream")
    size = folder["size"]
    if size > limit:
        raise ArchiveError("a part of it unpacks to %d bytes, over the %d this reads" % (size, limit))
    order, cur = [], folder["packed"][0]                                # the coders, from the packed stream out
    while len(order) <= len(coders):
        order.append(cur)
        nxt = [i for i, o in folder["binds"] if o == cur]
        if not nxt:
            break
        cur = nxt[0]
    if len(order) != len(coders) or order[-1] != folder["final"]:
        raise ArchiveError("a .7z folder whose coders are not one chain")
    chain = [coders[i] for i in order if coders[i]["id"] != COPY]
    if not chain:
        data = bytes(packed)
    else:
        head, rest = chain[0], chain[1:]
        cid, props = head["id"], head["props"]
        try:
            if cid in (LZMA1, LZMA2):
                if cid == LZMA1:
                    if len(props) != 5 or props[0] >= 225:
                        raise ArchiveError("an LZMA coder with properties this does not read")
                    main = {"id": lzma.FILTER_LZMA1, "lc": props[0] % 9, "lp": props[0] // 9 % 5, "pb": props[0] // 45,
                            "dict_size": _lzma_dict(int.from_bytes(props[1:5], "little"))}
                else:
                    if len(props) != 1 or props[0] > 40:
                        raise ArchiveError("an LZMA2 coder with properties this does not read")
                    main = {"id": lzma.FILTER_LZMA2,
                            "dict_size": _lzma_dict(0xFFFFFFFF if props[0] == 40 else (2 | (props[0] & 1)) << (props[0] // 2 + 11))}
                filters = [main]                                        # lzma's order: the filter applied last comes first
                for c in rest:
                    if c["id"] == DELTA and len(c["props"]) == 1:
                        filters.insert(0, {"id": lzma.FILTER_DELTA, "dist": c["props"][0] + 1})
                    elif c["id"] in X86 and not c["props"] and cid == LZMA2:
                        # (on LZMA, whose stream has no end mark, lzma's x86 filter keeps the last bytes back)
                        filters.insert(0, {"id": lzma.FILTER_X86})
                    else:
                        raise ArchiveError("it is packed with a coder this does not read (%s)" % c["id"].hex())
                d = lzma.LZMADecompressor(lzma.FORMAT_RAW, filters=filters)
                data = d.decompress(bytes(packed), size)
                while len(data) < size and not d.eof:                   # an LZMA stream with no end mark keeps its last bytes back
                    more = d.decompress(b"", size - len(data))
                    if not more:
                        break
                    data += more
            elif cid == DEFLATE and not rest:
                data = zlib.decompressobj(-15).decompress(bytes(packed), size)
            elif cid == BZIP2 and not rest:
                data = bz2.BZ2Decompressor().decompress(bytes(packed), size)
            else:
                raise ArchiveError("it is packed with a coder this does not read (%s)" % (rest[0]["id"] if rest else cid).hex())
        except (lzma.LZMAError, zlib.error, OSError, ValueError, EOFError, MemoryError) as e:
            raise ArchiveError("damaged: a part of it does not unpack (%s)" % e)
    if len(data) != size:
        raise ArchiveError("damaged: a part of it unpacks to %d bytes and its header says %d" % (len(data), size))
    if folder["crc"] is not None and zlib.crc32(data) != folder["crc"]:
        raise ArchiveError("damaged: a part of it does not have the CRC its header gives")
    return data


class Archive:
    """A .7z archive's members. `files`: [{name, size, dir}], in the archive's
    order; `read(i)` gives member i's bytes."""

    def __init__(self, blob, limit=FOLDER_MAX):
        self.blob, self.limit, self._cache = blob, limit, (None, None)
        if blob[:6] != SIG:
            raise ArchiveError("not a .7z archive")
        if len(blob) < 32:
            raise ArchiveError("the .7z archive ends inside its first header")
        crc, off, size, hcrc = struct.unpack_from("<IQQI", blob, 8)
        if zlib.crc32(blob[12:32]) != crc:
            raise ArchiveError("damaged: the .7z archive's first header does not have its CRC")
        self.files, self.folders, self.pack = [], [], []
        if size == 0:
            return
        if 32 + off + size > len(blob):
            raise ArchiveError("the .7z archive is cut short (or it is one volume of several)")
        if size > HEADER_MAX:
            raise ArchiveError("the .7z header is %d bytes, over the %d this reads" % (size, HEADER_MAX))
        head = blob[32 + off:32 + off + size]
        if zlib.crc32(head) != hcrc:
            raise ArchiveError("damaged: the .7z header does not have its CRC")
        for _ in range(4):
            r = _Reader(head)
            t = r.byte()
            if t != 0x17:
                break
            s = _streams(r)                                             # the header is packed: unpack it
            if len(s["folders"]) != 1:
                raise ArchiveError("the .7z archive's packed header is not one folder")
            head = _unpack(s["folders"][0], self._packed(s, 0), HEADER_MAX)
        else:
            raise ArchiveError("the .7z header is packed too many times")
        if t != 1:
            raise ArchiveError("the .7z header does not start as one")
        t = r.byte()
        if t == 2:                                                      # archive properties: nothing here uses one
            while r.byte() != 0:
                r.take(r.num())
            t = r.byte()
        if t == 3:
            raise ArchiveError("the .7z header names streams outside the archive")
        s = {"pack_pos": 0, "pack_sizes": [], "folders": []}
        if t == 4:
            s = _streams(r)
            t = r.byte()
        self._s, self.folders = s, s["folders"]
        streams = [(fi, at, size, crc) for fi, f in enumerate(self.folders)
                   for at, (size, crc) in zip(_starts(f["files"]), f["files"])]
        if t == 5:
            self._files(r, streams)
            t = r.byte()
        if t != 0:
            raise ArchiveError("the .7z header has a part this does not read (0x%02X)" % t)

    def _files(self, r, streams):
        n = r.count(FILES_MAX, "files")
        empty, empty_file, names, attrs = [False] * n, None, None, [None] * n
        while True:
            t = r.byte()
            if t == 0:
                break
            body = _Reader(r.take(r.num()))
            if t == 0xE:
                empty = body.bits(n)
            elif t == 0xF:
                empty_file = body.bits(sum(empty))
            elif t == 0x11:
                if body.byte() != 0:
                    raise ArchiveError("the .7z header names streams outside the archive")
                try:
                    names = bytes(body.take(len(body.b) - 1)).decode("utf-16-le").split("\0")[:-1]
                except UnicodeDecodeError:
                    raise ArchiveError("the .7z header's file names are not text")
            elif t == 0x15:
                defined = [True] * n if body.byte() else body.bits(n)
                if body.byte() != 0:
                    raise ArchiveError("the .7z header names streams outside the archive")
                attrs = [int.from_bytes(body.take(4), "little") if d else None for d in defined]
        if names is None or len(names) != n:
            raise ArchiveError("the .7z header does not name each of its files")
        if n - sum(empty) != len(streams):
            raise ArchiveError("the .7z header's files and streams do not agree")
        streams, k = iter(streams), 0
        for i in range(n):
            if empty[i]:
                is_file = bool(empty_file and empty_file[k])
                k += 1
                is_dir = not is_file or bool(attrs[i] is not None and attrs[i] & 0x10)
                self.files.append({"name": names[i], "size": 0, "dir": is_dir, "at": None})
            else:
                fi, at, size, crc = next(streams)
                self.files.append({"name": names[i], "size": size, "dir": False, "at": (fi, at), "crc": crc})

    def _packed(self, s, fi):
        """Folder fi's packed stream, as a view of the archive."""
        k = sum(len(f["packed"]) for f in s["folders"][:fi])
        start = 32 + s["pack_pos"] + sum(s["pack_sizes"][:k])
        end = start + s["pack_sizes"][k]
        if end > len(self.blob):
            raise ArchiveError("the .7z archive is cut short (or it is one volume of several)")
        return memoryview(self.blob)[start:end]

    def read(self, i):
        f = self.files[i]
        if f["dir"]:
            raise ArchiveError("%s is a folder" % f["name"])
        if f["at"] is None:
            return b""
        fi, at = f["at"]
        if self._cache[0] != fi:                                        # a folder holds many files: unpacked once
            self._cache = (fi, _unpack(self.folders[fi], self._packed(self._s, fi), self.limit))
        data = self._cache[1][at:at + f["size"]]
        if len(data) != f["size"] or (f["crc"] is not None and zlib.crc32(data) != f["crc"]):
            raise ArchiveError("damaged: %s does not have the size and CRC its header gives" % f["name"])
        return data


def _starts(files):
    at, out = 0, []
    for size, _ in files:
        out.append(at)
        at += size
    return out


def is_7z(blob):
    return blob[:6] == SIG


# -- a writer, for the self-tests ----------------------------------------------

def _num(v):
    for i in range(8):
        if v < 1 << (7 * (i + 1)):
            return bytes([((0xFF << (8 - i)) & 0xFF) | (v >> (8 * i))]) + (v & ((1 << (8 * i)) - 1)).to_bytes(i, "little")
    return b"\xff" + v.to_bytes(8, "little")


def _pack(data, method):
    """(the coders' bytes of a folder, the packed stream, the coders' count)."""
    if method == "copy":
        return b"\x01" + COPY, data, 1
    if method == "lzma2":
        c = lzma.LZMACompressor(lzma.FORMAT_RAW, filters=[{"id": lzma.FILTER_LZMA2, "dict_size": 1 << 20}])
        return b"\x21" + LZMA2 + b"\x01\x10", c.compress(data) + c.flush(), 1
    if method == "lzma":
        c = lzma.LZMACompressor(lzma.FORMAT_RAW, filters=[{"id": lzma.FILTER_LZMA1, "dict_size": 1 << 20, "lc": 3, "lp": 0, "pb": 2}])
        return b"\x23" + LZMA1 + b"\x05\x5d" + (1 << 20).to_bytes(4, "little"), c.compress(data) + c.flush(), 1
    if method == "delta+lzma2":                                         # coder 0 the filter, coder 1 LZMA2 on the packed stream
        c = lzma.LZMACompressor(lzma.FORMAT_RAW, filters=[{"id": lzma.FILTER_DELTA, "dist": 2},
                                                           {"id": lzma.FILTER_LZMA2, "dict_size": 1 << 20}])
        return b"\x21" + DELTA + b"\x01\x01" + b"\x21" + LZMA2 + b"\x01\x10", c.compress(data) + c.flush(), 2
    if method == "deflate":
        c = zlib.compressobj(9, zlib.DEFLATED, -15)
        return b"\x03" + DEFLATE, c.compress(data) + c.flush(), 1
    if method == "bzip2":
        return b"\x03" + BZIP2, bz2.compress(data), 1
    if method in ("aes", "ppmd"):
        return (b"\x04\x06\xf1\x07\x01" if method == "aes" else b"\x03\x03\x04\x01"), data, 1
    raise ValueError(method)


def make_archive(files, method="lzma2", solid=True, pack_header=True, dirs=()):
    """A .7z of `files` [(name, bytes)] and empty folders `dirs`, for the
    self-tests: one folder for them all (solid) or one for each."""
    full = [(n, d) for n, d in files if d]
    empties = [(n, True) for n in dirs] + [(n, False) for n, d in files if not d]
    groups = [full] if solid and full else [[f] for f in full]

    def streams(groups, packs):
        s = b"\x06" + _num(0) + _num(len(packs)) + b"\x09" + b"".join(_num(len(p)) for _, p, _ in packs) + b"\x00"
        s += b"\x07\x0b" + _num(len(groups)) + b"\x00"
        for (coders, _, n), g in zip(packs, groups):
            s += _num(n) + coders
            if n == 2:
                s += _num(0) + _num(1)                                  # coder 0 reads coder 1's output
        s += b"\x0c"
        for (_, _, n), g in zip(packs, groups):
            total = sum(len(d) for _, d in g)
            s += _num(total) * n
        s += b"\x00"
        s += b"\x08\x0d" + b"".join(_num(len(g)) for g in groups)
        if any(len(g) > 1 for g in groups):
            s += b"\x09" + b"".join(_num(len(d)) for g in groups for _, d in g[:-1])
        n = sum(len(g) for g in groups)
        s += b"\x0a\x01" + b"".join(zlib.crc32(d).to_bytes(4, "little") for g in groups for _, d in g) + b"\x00"
        return s + b"\x00", n

    def bitvec(bits):
        out = bytearray((len(bits) + 7) // 8)
        for i, b in enumerate(bits):
            if b:
                out[i // 8] |= 0x80 >> (i % 8)
        return bytes(out)

    packs = [_pack(b"".join(d for _, d in g), method) for g in groups]
    body = b"".join(p for _, p, _ in packs)
    head = b"\x01"
    if groups:
        head += b"\x04" + streams(groups, packs)[0]
    names = [n for g in groups for n, _ in g] + [n for n, _ in empties]
    head += b"\x05" + _num(len(names))
    if empties:
        v = bitvec([False] * (len(names) - len(empties)) + [True] * len(empties))
        head += b"\x0e" + _num(len(v)) + v
        v = bitvec([not is_dir for _, is_dir in empties])
        head += b"\x0f" + _num(len(v)) + v
    text = b"\x00" + "".join(n + "\0" for n in names).encode("utf-16-le")
    head += b"\x11" + _num(len(text)) + text + b"\x00\x00"
    if pack_header:
        c = lzma.LZMACompressor(lzma.FORMAT_RAW, filters=[{"id": lzma.FILTER_LZMA1, "dict_size": 1 << 16, "lc": 3, "lp": 0, "pb": 2}])
        packed = c.compress(head) + c.flush()
        enc = (b"\x17\x06" + _num(len(body)) + _num(1) + b"\x09" + _num(len(packed)) + b"\x00"
               + b"\x07\x0b" + _num(1) + b"\x00" + _num(1) + b"\x23" + LZMA1 + b"\x05\x5d" + (1 << 16).to_bytes(4, "little")
               + b"\x0c" + _num(len(head)) + b"\x0a\x01" + zlib.crc32(head).to_bytes(4, "little") + b"\x00\x00")
        body, head = body + packed, enc
    start = struct.pack("<QQI", len(body), len(head), zlib.crc32(head))
    return SIG + b"\x00\x04" + zlib.crc32(start).to_bytes(4, "little") + start + body + head


def selftest():
    fails = []

    def expect(cond, what):
        print("%-78s %s" % (what, "ok" if cond else "FAIL"))
        if not cond:
            fails.append(what)

    def refused(fn, want):
        try:
            fn()
        except ArchiveError as e:
            return want in str(e)
        return False

    big = bytes((i * 31 + (i >> 7)) & 0xFF for i in range(70000))
    files = [("hack.xdelta", big), ("Read Me.txt", b"read me first\r\n"), ("notes/été.txt", b"x" * 300), ("empty.txt", b"")]
    want = dict(files)
    for method in ("copy", "lzma", "lzma2", "delta+lzma2", "deflate", "bzip2"):
        for solid in (True, False):
            for pack_header in (True, False):
                a = Archive(make_archive(files, method, solid, pack_header, dirs=("notes",)))
                got = {f["name"]: a.read(i) for i, f in enumerate(a.files) if not f["dir"]}
                ok = got == want and [f["name"] for f in a.files if f["dir"]] == ["notes"] \
                    and all(f["size"] == len(want[f["name"]]) for f in a.files if not f["dir"])
                if not ok or (solid and pack_header):
                    expect(ok, "%s%s: every member read back, the folder a folder" % (method, "" if ok else
                                                                                       " (solid %s, header packed %s)" % (solid, pack_header)))
    expect(Archive(make_archive([])).files == [], "an archive of nothing: no members")
    good = make_archive(files)
    expect(is_7z(good) and not is_7z(b"PK\x03\x04"), "is_7z: by the signature")
    expect(refused(lambda: Archive(b"PK\x03\x04" + bytes(40)), "not a .7z archive"), "another file: not a .7z archive")
    expect(refused(lambda: Archive(good[:-5]), "cut short"), "a cut archive: refused")
    bad = bytearray(good)
    bad[-3] ^= 0x40
    expect(refused(lambda: Archive(bytes(bad)), "does not have its CRC"), "a changed header: refused by its CRC")
    bad = bytearray(make_archive(files, "lzma2", True, False))
    bad[40] ^= 0x55
    expect(refused(lambda: Archive(bytes(bad)).read(0), "damaged"), "a changed packed stream: refused when it is read")
    bad = bytearray(good)
    bad[13] ^= 1
    expect(refused(lambda: Archive(bytes(bad)), "first header does not have its CRC"), "a changed first header: refused")
    for method, say in (("aes", "encrypted"), ("ppmd", "PPMd")):
        a = Archive(make_archive(files, method, True, False))
        expect(refused(lambda: a.read(0), say), "an archive that is %s: its members are listed, a read is refused" % say)
    expect(refused(lambda: Archive(make_archive(files, "copy"), limit=1000).read(0), "over the 1000"),
           "a folder over the caller's limit: refused before it is unpacked")
    r = _Reader(b"".join(_num(v) for v in (0, 127, 128, 16383, 16384, 1 << 40, (1 << 64) - 1)))
    expect([r.num() for _ in range(7)] == [0, 127, 128, 16383, 16384, 1 << 40, (1 << 64) - 1], "the format's numbers, each width")
    print("sevenzip selftest: %s" % ("ok" if not fails else "%d failure(s)" % len(fails)))
    return 1 if fails else 0


def main():
    a = sys.argv[1:]
    if a[:1] == ["selftest"]:
        return selftest()
    if a[:1] in (["list"], ["read"]) and len(a) == (2 if a[0] == "list" else 4):
        with open(a[1], "rb") as f:
            arc = Archive(f.read())
        if a[0] == "list":
            for f in arc.files:
                print("%12s  %s" % ("folder" if f["dir"] else f["size"], f["name"]))
            return 0
        hit = [i for i, f in enumerate(arc.files) if f["name"] == a[2] and not f["dir"]]
        if not hit:
            raise ArchiveError("the archive has no member %s" % a[2])
        with open(a[3], "wb") as f:
            f.write(arc.read(hit[0]))
        print("%d bytes" % os.path.getsize(a[3]))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    try:
        sys.exit(main() or 0)
    except ArchiveError as e:
        sys.exit("sevenzip: %s" % e)
