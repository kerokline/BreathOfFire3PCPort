#!/usr/bin/env python
"""Rename the faeries in a save file to the names a language overlay gives them.

A faerie village birth copies five bytes of the trait record's name into the
save's name table (block +0xF10, 60 x 5 bytes, beside the 60 entries of 8 at
+0xCF0; docs/save-interchange.md). A save made under the shipped Chinese
holds the Chinese bytes, and a Chinese name is two bytes a glyph, so the five
bytes hold two and a half glyphs - the overlay cannot reach them. This writes
each in-use entry's name from the overlay's own list (DIV-0064 group 16, the
kind-15 chunk of tag 16 in DAT\\<lang>.FIRST.DAT: entry r is trait record r,
so the name is exact) and recomputes the checksum the loader checks (the low
word of the byte sum of the 0x10B0-byte block with the checksum word zeroed;
src/game/save_menu.cpp, LoadMenu_Read).

    python tools/faerie_names.py --lang en-US --game bof3 tools/recipe_saves/fairyVillage.DAT bof3/BISLPS00.DAT ...
    python tools/faerie_names.py --lang en-US --game bof3 --dry-run bof3/BISLPS0*.DAT

The files are game data and are rewritten in place: back them up first. A
save whose checksum does not verify is left alone.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dat  # noqa: E402

ENTRIES, ENTRY_COUNT, ENTRY_SIZE = 0xCF0, 60, 8
NAMES, NAME_SIZE = 0xF10, 5
CHECKSUM, BLOCK, FILE_SIZE = 0x70, 0x10B0, 0x12B0
KIND_LABELS, TAG_FAERIES = 15, 16


def overlay_names(game, lang):
    """The sixty names as the overlay encodes them (PC glyph codes), b'' where kept as shipped."""
    path = os.path.join(game, "DAT", "%s.FIRST.DAT" % lang)
    blob, chunks = dat.load(path)
    for c in chunks:
        if c.kind == KIND_LABELS and c.tag == TAG_FAERIES:
            payload = blob[c.offset:c.offset + c.size]
            count = payload[0]
            names = payload[1:].split(b"\0")[:count]
            if count != ENTRY_COUNT or len(names) != count:
                raise SystemExit("%s: the faerie names chunk holds %d names, not %d" % (path, len(names), ENTRY_COUNT))
            return names
    raise SystemExit("%s has no faerie names chunk (kind %d tag %d): rebuild the overlay" % (path, KIND_LABELS, TAG_FAERIES))


def checksum(block):
    b = bytearray(block[:BLOCK])
    b[CHECKSUM:CHECKSUM + 2] = b"\0\0"
    return sum(b) & 0xFFFF


def rename(path, names, dry_run):
    data = bytearray(open(path, "rb").read())
    if len(data) != FILE_SIZE:
        print("%s: %d bytes, not a save (%d)" % (path, len(data), FILE_SIZE))
        return
    stored = struct.unpack_from("<H", data, CHECKSUM)[0]
    if checksum(data) != stored:
        print("%s: checksum 0x%04X does not verify (0x%04X): left alone" % (path, stored, checksum(data)))
        return
    changed = []
    for r in range(ENTRY_COUNT):
        if data[ENTRIES + ENTRY_SIZE * r] == 0:
            continue
        old = bytes(data[NAMES + NAME_SIZE * r:NAMES + NAME_SIZE * (r + 1)])
        new = names[r]
        if not new or len(new) > NAME_SIZE:
            changed.append((r, old, None))
            continue
        new = new + b"\0" * (NAME_SIZE - len(new))
        if new != old:
            data[NAMES + NAME_SIZE * r:NAMES + NAME_SIZE * (r + 1)] = new
            changed.append((r, old, new))
    for r, old, new in changed:
        print("  entry %2d: %s -> %s" % (r, old.hex(" "), new.hex(" ") if new else "kept (no name in the overlay)"))
    if not changed:
        print("%s: no faeries to rename" % path)
        return
    struct.pack_into("<H", data, CHECKSUM, checksum(data))
    if dry_run:
        print("%s: %d names would change (dry run)" % (path, sum(1 for c in changed if c[2])))
        return
    open(path, "wb").write(data)
    print("%s: %d names written, checksum 0x%04X" % (path, sum(1 for c in changed if c[2]), checksum(data)))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("saves", nargs="+")
    p.add_argument("--lang", required=True, help="the overlay tag, e.g. en-US")
    p.add_argument("--game", default="bof3")
    p.add_argument("--dry-run", action="store_true")
    args = p.parse_args()
    names = overlay_names(args.game, args.lang)
    for path in args.saves:
        rename(path, names, args.dry_run)


if __name__ == "__main__":
    main()
