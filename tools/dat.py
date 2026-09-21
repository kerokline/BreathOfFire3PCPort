#!/usr/bin/env python
"""Parse, extract and cross-check the PC port's DAT/*.DAT containers.

Format (docs/DAT_CONTAINER.md; measured on all 742 files, 2026-09-19):

    A .DAT is a flat stream of chunks, no file header, no trailer.
    chunk header (16 bytes)
        0x00 u32  kind   0 data, 1 image, 2 audio bank, 3 (one instance);
                         4 is OURS, never in a shipped file: glyph advances,
                         in language overlays only (DIVERGENCE.md DIV-0006);
                         5 is OURS too: item / ability names, tag = the
                         table's address in BOF3.exe (DIV-0008)
        0x04 u32  tag    kind 0: PC-side buffer offset
                         kind 1: the PSX EMI `RAM destination` word, verbatim
                         kind 2: small integer 1..6
        0x08 u32  size   payload bytes, header excluded
        0x0C u32  zero
    payload follows immediately; next header at +16+size. No alignment.

    kind-2 payload (offsets relative to the payload start)
        0x000  descriptor area (not decoded here)
        0x188  61 x (u32 offset, u32 size); size 0 = empty slot
        0x380  first subfile; subfiles are RIFF WAVE, contiguous, and the
               last one ends exactly at the end of the payload

Nothing is encrypted or compressed at the container level.

    python tools/dat.py list    DAT/FIRST.DAT
    python tools/dat.py survey  DAT/
    python tools/dat.py extract DAT/FIRST.DAT --out analysis/dat/FIRST
    python tools/dat.py compare DAT/FIRST.DAT path/to/FIRST.EMI

Extracted output is game data: write it under analysis/ (gitignored), never
commit it (CLAUDE.md rule 1).
"""
import argparse
import collections
import glob
import os
import struct
import sys

KINDS = {0: "data", 1: "image", 2: "audio bank", 3: "kind3", 4: "advances (ours, DIV-0006)",
         5: "names (ours, DIV-0008)", 6: "title menu widths (ours, DIV-0014)",
         7: "config screen text (ours, DIV-0015)",
         8: "menu verbs (ours, DIV-0018)"}
BANK_TOC = 0x188
BANK_SLOTS = 61
BANK_DATA = 0x380

Chunk = collections.namedtuple("Chunk", "index kind tag size offset")


def parse(blob, name="<mem>"):
    """Return the chunk list. Raises unless the walk lands exactly on EOF."""
    chunks, pos = [], 0
    while pos < len(blob):
        if pos + 16 > len(blob):
            raise ValueError("%s: truncated chunk header at 0x%X" % (name, pos))
        kind, tag, size, zero = struct.unpack_from("<4I", blob, pos)
        if kind not in KINDS or zero != 0 or pos + 16 + size > len(blob):
            raise ValueError("%s: bad chunk header at 0x%X (kind=%d size=0x%X pad=0x%X)"
                             % (name, pos, kind, size, zero))
        chunks.append(Chunk(len(chunks), kind, tag, size, pos + 16))
        pos += 16 + size
    return chunks


def bank_subfiles(blob, chunk, name="<mem>"):
    """(slot, absolute offset, size) per live subfile of a kind-2 chunk, verified."""
    out, expect = [], BANK_DATA
    for slot in range(BANK_SLOTS):
        off, size = struct.unpack_from("<II", blob, chunk.offset + BANK_TOC + 8 * slot)
        if not size:
            continue
        if off != expect:
            raise ValueError("%s: chunk %d slot %d at 0x%X, expected 0x%X"
                             % (name, chunk.index, slot, off, expect))
        out.append((slot, chunk.offset + off, size))
        expect = off + size
    if out and expect != chunk.size:
        raise ValueError("%s: chunk %d bank ends at 0x%X, payload is 0x%X"
                         % (name, chunk.index, expect, chunk.size))
    return out


def load(path):
    with open(path, "rb") as f:
        blob = f.read()
    return blob, parse(blob, os.path.basename(path))


def cmd_list(args):
    blob, chunks = load(args.dat)
    print("%s: %d bytes, %d chunks" % (args.dat, len(blob), len(chunks)))
    for c in chunks:
        extra = ""
        if c.kind == 2:
            extra = "  %d wav" % len(bank_subfiles(blob, c, args.dat))
        print("  #%-3d %-10s tag=%08X size=%7X @%7X%s"
              % (c.index, KINDS[c.kind], c.tag, c.size, c.offset, extra))


def cmd_survey(args):
    files = sorted(glob.glob(os.path.join(args.dir, "*.DAT")))
    kinds, wavs, bad = collections.Counter(), 0, []
    for p in files:
        try:
            blob, chunks = load(p)
            for c in chunks:
                kinds[c.kind] += 1
                if c.kind == 2:
                    for _, off, _ in bank_subfiles(blob, c, p):
                        if blob[off:off + 4] != b"RIFF":
                            raise ValueError("%s: non-RIFF bank subfile at 0x%X" % (p, off))
                        wavs += 1
        except ValueError as e:
            bad.append(str(e))
    print("%d files, %d parsed clean, %d failed" % (len(files), len(files) - len(bad), len(bad)))
    print("chunks by kind:", ", ".join("%s=%d" % (KINDS[k], n) for k, n in sorted(kinds.items())))
    print("RIFF subfiles:", wavs)
    for b in bad:
        print("  FAIL", b)
    return 1 if bad else 0


def cmd_extract(args):
    blob, chunks = load(args.dat)
    os.makedirs(args.out, exist_ok=True)
    for c in chunks:
        stem = os.path.join(args.out, "%03d_k%d_%08X" % (c.index, c.kind, c.tag))
        with open(stem + ".bin", "wb") as f:
            f.write(blob[c.offset:c.offset + c.size])
        if c.kind == 2:
            for slot, off, size in bank_subfiles(blob, c, args.dat):
                with open("%s_s%02d.wav" % (stem, slot), "wb") as f:
                    f.write(blob[off:off + size])
    print("wrote %d chunks to %s" % (len(chunks), args.out))


def emi_sections(path):
    """(index, type, dest, bytes) per section of a PSX .EMI (format: sibling tools/emi.py)."""
    with open(path, "rb") as f:
        blob = f.read()
    if blob[8:16] != b"MATH_TBL":
        raise ValueError("%s: not an EMI" % path)
    count, = struct.unpack_from("<I", blob, 0)
    out, pos = [], 0x800
    for i in range(count):
        size, dest = struct.unpack_from("<II", blob, 0x10 + 16 * i)
        type_id, = struct.unpack_from("<H", blob, 0x1C + 16 * i)
        out.append((i, type_id, dest, blob[pos:pos + size]))
        pos += ((size + 0x7FF) >> 11) * 0x800
    return out


def cmd_compare(args):
    """Pair EMI non-audio sections with DAT kind-0/1 chunks, in order, and diff them.

    Audio (EMI types 6/7/8) has no byte-level counterpart: the port merged each
    VH/VB group into one kind-2 bank of decoded WAVs.
    """
    blob, chunks = load(args.dat)
    emi = [s for s in emi_sections(args.emi) if s[1] not in (6, 7, 8, 10)]
    dat = [c for c in chunks if c.kind in (0, 1)]
    if len(emi) != len(dat):
        print("WARNING: %d EMI sections vs %d DAT chunks; pairing in order anyway"
              % (len(emi), len(dat)))
    for (i, t, dest, data), c in zip(emi, dat):
        pc = blob[c.offset:c.offset + c.size]
        n = min(len(data), len(pc))
        diff = sum(1 for a, b in zip(data, pc) if a != b)
        tagok = "" if c.kind != 1 else ("  tag==dest" if c.tag == dest else "  TAG MISMATCH")
        verdict = "IDENTICAL" if diff == 0 and len(data) == len(pc) else "%d/%d bytes differ" % (diff, n)
        print("  emi#%-2d t%-2d %08X len %6X | dat#%-2d k%d %08X len %6X | %s%s"
              % (i, t, dest, len(data), c.index, c.kind, c.tag, len(pc), verdict, tagok))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("list"); p.add_argument("dat"); p.set_defaults(fn=cmd_list)
    p = sub.add_parser("survey"); p.add_argument("dir"); p.set_defaults(fn=cmd_survey)
    p = sub.add_parser("extract"); p.add_argument("dat"); p.add_argument("--out", required=True)
    p.set_defaults(fn=cmd_extract)
    p = sub.add_parser("compare"); p.add_argument("dat"); p.add_argument("emi")
    p.set_defaults(fn=cmd_compare)
    args = ap.parse_args()
    return args.fn(args) or 0


if __name__ == "__main__":
    sys.exit(main())
