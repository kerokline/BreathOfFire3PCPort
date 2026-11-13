#!/usr/bin/env python
"""A PPF patch (PlayStation Patch Format 3.0) read and applied in pure Python.

    python tools/ppf.py info PATCH                 the header and the record count
    python tools/ppf.py apply PATCH SOURCE OUT     OUT = SOURCE with the patch's records written over it
    python tools/ppf.py selftest                   on patches it makes itself (no game data)

The second patch format a romhack comes in (docs/mods.md 12): tools/vcdiff.py
reads `.xdelta`, this reads `.ppf`. A PPF 3.0 file is

    0   "PPF30"            the magic
    5   1 byte             the encoding method, 2 for 3.0
    6   50 bytes           a description, space padded
    56  1 byte             the image type: 0 a BIN (any raw image), 1 a GI (PrimoDVD)
    57  1 byte             block check: 1 when 1,024 bytes of the image follow the header
    58  1 byte             undo: 1 when each record also holds the bytes it replaces
    59  1 byte             unused
    60  1,024 bytes        (block check) the image's bytes at 0x9320 (BIN) or 0x80A0 (GI)
    then records, each     an 8-byte offset (little endian), a 1-byte count 1..255,
                           that many new bytes, and with undo that many old bytes
    at the end, optional   "@BEGIN_FILE_ID.DIZ" text "@END_FILE_ID.DIZ" and the text's
                           length in 2 bytes (some tools write 4)

A record writes inside the image: PPF does not make a file longer, and a
record past the image's end is refused. The block check is the format's own
test that the patch is of this image, and undo data is a second: the bytes a
record replaces must be the image's. A patch with neither is applied to any
image its records fit, and what it makes is the caller's to check (a sha256
from the release's list of known patches, tools/mods.py).

Only 3.0 is read. 1.0 and 2.0 patches (4-byte offsets, another header) are
refused by name: none has been seen for this game.
"""
import hashlib
import os
import shutil
import struct
import sys

MAGIC = b"PPF30"
HEADER = 60
BLOCK = 1024
BLOCK_AT = {0: 0x9320, 1: 0x80A0}
DIZ_BEGIN, DIZ_END = b"@BEGIN_FILE_ID.DIZ", b"@END_FILE_ID.DIZ"


class PatchError(Exception):
    """A file that is not a PPF 3.0 patch, or a patch that does not fit the image."""


def is_ppf(patch):
    return patch[:3] == b"PPF"


def header(patch):
    """{description, image_type, block (the 1,024 check bytes or None), undo,
    start (the first record's offset in the patch), end (past the last)}."""
    if patch[:3] != b"PPF":
        raise PatchError("not a PPF patch (a .ppf file starts PPF)")
    if patch[:5] != MAGIC or len(patch) < HEADER or patch[5] != 2:
        raise PatchError("a PPF patch of version %s: only 3.0 is read" % patch[3:5].decode("latin-1"))
    image_type, check, undo = patch[56], patch[57], patch[58]
    if image_type not in BLOCK_AT or check > 1 or undo > 1:
        raise PatchError("the PPF header's flags are not ones the format has")
    start = HEADER + (BLOCK if check else 0)
    if len(patch) < start:
        raise PatchError("the PPF patch ends inside its header")
    end = len(patch)
    for width in (2, 4):
        tail = end - width - len(DIZ_END)
        if tail >= start and patch[tail:tail + len(DIZ_END)] == DIZ_END:
            n = int.from_bytes(patch[end - width:end], "little")
            begin = tail - n - len(DIZ_BEGIN)
            if begin < start or patch[begin:begin + len(DIZ_BEGIN)] != DIZ_BEGIN:
                raise PatchError("the PPF patch's closing text block is damaged")
            end = begin
            break
    return {"description": patch[6:56].decode("latin-1").rstrip(" \0"), "image_type": image_type,
            "block": bytes(patch[HEADER:HEADER + BLOCK]) if check else None, "undo": bool(undo),
            "start": start, "end": end}


def records(patch, h=None):
    """Each record: (offset in the image, the new bytes, the bytes it replaces
    or None when the patch has no undo data)."""
    h = h or header(patch)
    p, end = h["start"], h["end"]
    while p < end:
        if p + 9 > end:
            raise PatchError("the PPF patch ends inside a record")
        off, n = struct.unpack_from("<QB", patch, p)
        p += 9
        if n == 0:
            raise PatchError("a PPF record of no bytes")
        step = n * (2 if h["undo"] else 1)
        if p + step > end:
            raise PatchError("the PPF patch ends inside a record")
        yield off, patch[p:p + n], (patch[p + n:p + 2 * n] if h["undo"] else None)
        p += step


def target_len(patch):
    """One past the last byte the patch writes."""
    return max((off + len(b) for off, b, _ in records(patch)), default=0)


def apply_file(patch_path, source_path, out_path, max_out=None):
    """(size, sha256) of OUT: SOURCE with the records written over it. OUT is
    written beside itself and renamed, so a patch that fails leaves no file."""
    with open(patch_path, "rb") as f:
        patch = f.read()
    h = header(patch)
    size = os.path.getsize(source_path)
    if size == 0:
        raise PatchError("the source file is empty")
    if max_out is not None and size > max_out:
        raise PatchError("the source file is %d bytes, over the %d the result may be" % (size, max_out))
    if h["block"] is not None:
        with open(source_path, "rb") as f:
            f.seek(BLOCK_AT[h["image_type"]])
            if f.read(BLOCK) != h["block"]:
                raise PatchError("the patch's block check does not match: it is a patch of another image "
                                 "(another version or region of the disc)")
    part = out_path + ".part"
    try:
        shutil.copyfile(source_path, part)
        with open(part, "r+b") as out:
            for off, data, old in records(patch, h):
                if off + len(data) > size:
                    raise PatchError("a record writes at 0x%X, past the image's end (0x%X)" % (off, size))
                out.seek(off)
                if old is not None and out.read(len(old)) != old:
                    # the undo data is the image's bytes before the patch: the format's second test of the image
                    raise PatchError("the bytes at 0x%X are not the ones the patch replaces: it is a patch of "
                                     "another image (another version or region of the disc)" % off)
                out.seek(off)
                out.write(data)
        d = hashlib.sha256()
        with open(part, "rb") as f:
            for b in iter(lambda: f.read(1 << 20), b""):
                d.update(b)
        os.replace(part, out_path)
    finally:
        if os.path.exists(part):
            os.remove(part)
    return size, d.hexdigest()


def make_patch(recs, description="", block=None, undo=None, diz=None, diz_width=2):
    """A PPF 3.0 patch of `recs` [(offset, bytes)], for the self-tests. `undo`:
    the source, when each record also carries the bytes it replaces."""
    out = bytearray(MAGIC + b"\x02" + description.encode("latin-1")[:50].ljust(50, b" "))
    out += bytes([0, 1 if block is not None else 0, 1 if undo is not None else 0, 0])
    if block is not None:
        out += block
    for off, data in recs:
        for i in range(0, len(data), 255):
            piece = data[i:i + 255]
            out += struct.pack("<QB", off + i, len(piece)) + piece
            if undo is not None:
                out += undo[off + i:off + i + len(piece)]
    if diz is not None:
        out += DIZ_BEGIN + diz + DIZ_END + len(diz).to_bytes(diz_width, "little")
    return bytes(out)


def selftest():
    import tempfile
    fails = []

    def expect(cond, what):
        print("%-78s %s" % (what, "ok" if cond else "FAIL"))
        if not cond:
            fails.append(what)

    def refused(fn, want):
        try:
            fn()
        except PatchError as e:
            return want in str(e)
        return False

    root = tempfile.mkdtemp(prefix="ppf_selftest_")
    try:
        src = bytes((i * 7 + (i >> 8)) & 0xFF for i in range(0xA000))
        recs = [(0x10, b"hello"), (0x9000, bytes(range(200)) * 3), (len(src) - 4, b"tail")]
        want = bytearray(src)
        for off, data in recs:
            want[off:off + len(data)] = data
        sp, pp, op = (os.path.join(root, n) for n in ("src.bin", "p.ppf", "out.bin"))
        with open(sp, "wb") as f:
            f.write(src)
        block = src[0x9320:0x9320 + BLOCK]
        for name, patch in (("plain", make_patch(recs, "a test")),
                            ("block check", make_patch(recs, block=block)),
                            ("undo data", make_patch(recs, undo=src)),
                            ("a closing text, 2-byte length", make_patch(recs, block=block, diz=b"some text")),
                            ("a closing text, 4-byte length", make_patch(recs, undo=src, diz=b"some text", diz_width=4))):
            with open(pp, "wb") as f:
                f.write(patch)
            size, sha = apply_file(pp, sp, op)
            with open(op, "rb") as f:
                got = f.read()
            expect(got == bytes(want) and size == len(src) and sha == hashlib.sha256(want).hexdigest()
                   and target_len(patch) == len(src), "%s: applied, the size and the sha256 of what it made" % name)
        expect(header(make_patch(recs, "a test"))["description"] == "a test", "the description read without its padding")
        expect(len(list(records(make_patch(recs)))) == 5, "a run over 255 bytes is that many records")
        with open(pp, "wb") as f:
            f.write(make_patch(recs, block=bytes(BLOCK)))
        expect(refused(lambda: apply_file(pp, sp, op), "a patch of another image") and not os.path.exists(op + ".part"),
               "a block check of another image: refused, no part file left")
        with open(pp, "wb") as f:
            f.write(make_patch(recs, undo=bytes(len(src))))
        expect(refused(lambda: apply_file(pp, sp, op), "not the ones the patch replaces"),
               "undo data of another image: refused")
        with open(pp, "wb") as f:
            f.write(make_patch([(len(src) - 2, b"long")]))
        expect(refused(lambda: apply_file(pp, sp, op), "past the image's end"), "a record past the image's end: refused")
        good = make_patch(recs)
        expect(refused(lambda: header(b"\xd6\xc3\xc4\0" + bytes(80)), "not a PPF patch"), "another file: not a PPF patch")
        expect(refused(lambda: header(b"PPF20\x01" + bytes(80)), "only 3.0 is read"), "a PPF 2.0 patch: refused by its version")
        expect(refused(lambda: list(records(good[:-3])), "ends inside a record"), "a cut patch: refused")
        expect(refused(lambda: header(good + DIZ_END + b"\x40\x00"), "closing text block is damaged"),
               "a closing text with no beginning: refused")
        expect(is_ppf(good) and not is_ppf(b"\xd6\xc3\xc4\0"), "is_ppf: by the first three bytes")
    finally:
        shutil.rmtree(root, ignore_errors=True)
    print("ppf selftest: %s" % ("ok" if not fails else "%d failure(s)" % len(fails)))
    return 1 if fails else 0


def main():
    a = sys.argv[1:]
    if a[:1] == ["selftest"]:
        return selftest()
    if a[:1] == ["info"] and len(a) == 2:
        with open(a[1], "rb") as f:
            patch = f.read()
        h = header(patch)
        n = sum(1 for _ in records(patch, h))
        print("PPF 3.0: %r, image type %d, block check %s, undo data %s, %d records, writes up to 0x%X"
              % (h["description"], h["image_type"], "yes" if h["block"] else "no", "yes" if h["undo"] else "no", n,
                 target_len(patch)))
        return 0
    if a[:1] == ["apply"] and len(a) == 4:
        size, sha = apply_file(a[1], a[2], a[3])
        print("%d bytes, sha256 %s" % (size, sha))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    try:
        sys.exit(main() or 0)
    except PatchError as e:
        sys.exit("ppf: %s" % e)
