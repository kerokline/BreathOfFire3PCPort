#!/usr/bin/env python
"""Read files out of a PSX or PSP disc image's ISO9660 tree.

Accepts a .cue (first BINARY track), a raw MODE2/2352 .bin, or a cooked
2048-byte .iso - the PSP discs in fixtures.toml are the last kind. Read-only.

    python tools/psx_disc.py ls   "CDImage/Breath of Fire III (USA).cue"
    python tools/psx_disc.py get  DISC BIN/WORLD00/AREA000.EMI --out <scratch>/AREA000.EMI

Extracted files are game data: scratch or analysis/ only, never committed
(CLAUDE.md rule 1).
"""
import argparse
import os
import re
import struct
import sys

RAW, USER, USER_OFF = 2352, 2048, 24
SYNC = b"\x00" + b"\xff" * 10 + b"\x00"


class Disc:
    def __init__(self, path):
        if path.lower().endswith(".cue"):
            with open(path, "r", errors="replace") as f:
                m = re.search(r'FILE\s+"([^"]+)"\s+BINARY', f.read(), re.I)
            if not m:
                raise SystemExit("%s: no BINARY FILE line" % path)
            path = os.path.join(os.path.dirname(os.path.abspath(path)), m.group(1))
        self.f = open(path, "rb")
        self.f.seek(16 * RAW)
        self.raw = self.f.read(12) == SYNC
        if self.sector(16)[1:6] != b"CD001":
            raise SystemExit("%s: no ISO9660 volume descriptor at LBA 16" % path)
        self.files = {}
        root = self.sector(16)[156:190]
        self._walk(struct.unpack_from("<I", root, 2)[0], struct.unpack_from("<I", root, 10)[0], "")

    def sector(self, lba):
        if self.raw:
            self.f.seek(lba * RAW + USER_OFF)
        else:
            self.f.seek(lba * USER)
        return self.f.read(USER)

    def extent(self, lba, size):
        out = bytearray()
        while len(out) < size:
            out += self.sector(lba)
            lba += 1
        return bytes(out[:size])

    def _walk(self, lba, size, prefix):
        d, pos = self.extent(lba, size), 0
        while pos < len(d):
            n = d[pos]
            if n == 0:                       # records do not span sectors
                pos = (pos // USER + 1) * USER
                continue
            e_lba, e_size = struct.unpack_from("<I", d, pos + 2)[0], struct.unpack_from("<I", d, pos + 10)[0]
            flags, nlen = d[pos + 25], d[pos + 32]
            name = d[pos + 33:pos + 33 + nlen]
            pos += n
            if name in (b"\x00", b"\x01"):
                continue
            name = name.decode("latin1").split(";")[0]
            if flags & 2:
                self._walk(e_lba, e_size, prefix + name + "/")
            else:
                self.files[(prefix + name).upper()] = (e_lba, e_size)

    def read(self, name):
        lba, size = self.files[name.upper().replace("\\", "/")]
        return self.extent(lba, size)

    def find(self, basename):
        """Full paths whose last component is basename (case-insensitive)."""
        b = basename.upper()
        return sorted(p for p in self.files if p.rsplit("/", 1)[-1] == b)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("ls")
    s.add_argument("disc")
    s = sub.add_parser("get")
    s.add_argument("disc")
    s.add_argument("name")
    s.add_argument("--out", required=True)
    args = ap.parse_args()
    disc = Disc(args.disc)
    if args.cmd == "ls":
        for p, (lba, size) in sorted(disc.files.items()):
            print("%10d %8d  %s" % (size, lba, p))
    else:
        data = disc.read(args.name)
        with open(args.out, "wb") as f:
            f.write(data)
        print("%s: %d bytes -> %s" % (args.name, len(data), args.out))


if __name__ == "__main__":
    sys.exit(main())
