#!/usr/bin/env python
"""The .data code-pointer words whose target is not a [[func]] start (docs/exe-code-pointers-midfunc.md).

docs/exe-import-engine.md section 4.1 / 5 item 3. State 3's entry-stub page
plants a jump at every symbols.toml [[func]] start; a code pointer into the
middle of a function gets no stub, so each one must be read. This lists them:

  - every word of recipes/exe-pointers.tsv's `text` runs (the PC's .data words
    the recipe decided are pointers into .text), its value read from BOF3.exe;
  - those whose value is not a [[func]] start, with the enclosing [[func]]
    (the greatest start below it; its extent runs to the next start) and
    whether that function has `impl`;
  - whether some disc places the word: a byte of it inside any build's
    exe_maps/<build>.tsv segment, recipes/exe-places.tsv place, or a
    catalogued table's span with a place in that build (exe_tables.disc_image's
    ranges, computed from the recipe files alone - no disc is read);
  - and the .rdata words (0x5C4000..0x5DA000, 4-aligned) that point into .text,
    with how many targets are [[func]] starts.

    python tools/exe_midfunc_pointers.py [--exe bof3/BOF3.exe] [--all] [--disc CUE_OR_ISO ...]

Prints addresses and counts only (CLAUDE.md rule 1). --all also lists the
words no disc places (by default only their count, grouped by enclosing
function). --disc builds each disc's image as exe_tables.disc_image does and
says, for every placed mid-function word, whether that build's word equals the
PC's (what made `exe_tables.decide_pointers` call it a pointer) and which
range placed it - the file, the segment's PC start and the disc address -
never the disc's bytes.
"""
import argparse
import bisect
import collections
import os
import struct
import sys
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import exe_tables    # noqa: E402
import tables        # noqa: E402

TEXT = (0x401000, 0x5C4000)
RDATA = (0x5C4000, 0x5DA000)


def funcs():
    with open(os.path.join(ROOT, "symbols.toml"), "rb") as f:
        return sorted((e["pc"], e["name"], bool(e.get("impl"))) for e in tomllib.load(f).get("func", []))


def placed_bytes():
    """{pc byte address: {build}} that some disc's range places (maps, places, tables)."""
    cat, syms, _ = tables.load()
    places = exe_tables.load_places()
    out = collections.defaultdict(set)
    for bid in exe_tables.ORDER:
        spans = [(r["pc"], r["pc_end"]) for r in tables.read_map(os.path.join(tables.MAPS, bid + ".tsv"))]
        spans += [(r["pc"], r["pc_end"]) for r in places.get(bid, [])]
        spans += [exe_tables.table_span(t, syms) for t, _ in exe_tables.table_rows(cat, syms, bid)]
        for lo, hi in spans:
            for a in range(lo, hi):
                out[a].add(bid)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=os.path.join(ROOT, "bof3", "BOF3.exe"))
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--disc", action="append", default=[])
    a = ap.parse_args()
    with open(a.exe, "rb") as f:
        exe = f.read()
    secs = exe_tables.pe_sections(exe)
    dva, _, draw, drsz = secs[".data"]
    rva, rvsz, rraw, rrsz = secs[".rdata"]

    def word(va):
        if dva <= va < dva + drsz:
            return struct.unpack_from("<I", exe, draw + va - dva)[0]
        return struct.unpack_from("<I", exe, rraw + va - rva)[0]

    fs = funcs()
    starts = [f[0] for f in fs]
    startset = set(starts)

    def enclosing(v):
        i = bisect.bisect_right(starts, v) - 1
        if i < 0:
            return None, None
        nxt = starts[i + 1] if i + 1 < len(starts) else TEXT[1]
        return fs[i], nxt

    words = [pc + 4 * k for pc, n, cls in exe_tables.load_pointers() if cls == "text" for k in range(n)]
    targets = {w: word(w) for w in words}
    bad = [w for w in words if not TEXT[0] <= targets[w] < TEXT[1]]
    if bad:
        sys.exit("exe_midfunc_pointers: %d `text` words do not point into .text (first 0x%06X)" % (len(bad), bad[0]))
    mid = [w for w in words if targets[w] not in startset]
    placed = placed_bytes()
    at_start = len(words) - len(mid)
    print(".data words into .text (exe-pointers.tsv `text`): %d at %d targets" % (len(words), len(set(targets.values()))))
    print("  at a [[func]] start: %d words, %d targets (%d of them without impl)"
          % (at_start, len({targets[w] for w in words if targets[w] in startset}),
             len({targets[w] for w in words if targets[w] in startset} - {f[0] for f in fs if f[2]})))
    print("  inside a function:   %d words, %d targets" % (len(mid), len({targets[w] for w in mid})))
    pl = [w for w in mid if any(w + k in placed for k in range(4))]
    un = [w for w in mid if w not in pl]
    print("    a disc places the word: %d   no disc places it: %d" % (len(pl), len(un)))
    print("\nplaced, inside a function:")
    print("word      target    +off   enclosing [[func]]                   impl  extent   builds")
    for w in pl:
        v = targets[w]
        (fpc, fname, impl), nxt = enclosing(v)
        bids = sorted(set().union(*(placed.get(w + k, set()) for k in range(4))), key=exe_tables.ORDER.index)
        print("0x%06X  0x%06X  +0x%-4X %-36s %-5s 0x%X   %s" % (w, v, v - fpc, "%s 0x%06X" % (fname, fpc),
                                                            "yes" if impl else "no", nxt - fpc, ",".join(bids)))
    by = collections.Counter()
    for w in un:
        (fpc, fname, _), _ = enclosing(targets[w])
        by["%s 0x%06X" % (fname, fpc)] += 1
    print("\nno disc places it, by enclosing function (%d functions):" % len(by))
    for k, n in by.most_common(None if a.all else 10):
        print("  %4d  %s" % (n, k))
    if a.all:
        for w in un:
            print("  0x%06X -> 0x%06X" % (w, targets[w]))
    if a.disc:
        pcd = exe[draw:draw + drsz]
        places = exe_tables.load_places()
        print("\nper build (equal: the disc's word is the PC's; the range that placed it):")
        for bid, d in exe_tables.discs(a.disc):
            img, owner, ranges = exe_tables.disc_image(d, bid, places.get(bid, []))
            for w in pl:
                o = w - exe_tables.DATA_LO
                if any(owner[i] is None for i in range(o, o + 4)):
                    print("  %-9s 0x%06X  not wholly placed" % (bid, w))
                    continue
                r = ranges[owner[o]]
                print("  %-9s 0x%06X  %-6s  %s %s from 0x%06X, disc 0x%X" % (
                    bid, w, "equal" if img[o:o + 4] == pcd[o:o + 4] else "DIFFERS", r[2], r[3], r[0],
                    r[4] + w - r[0]))
    rd = [va for va in range(RDATA[0], RDATA[0] + min(rvsz, rrsz) - 3, 4) if TEXT[0] <= word(va) < TEXT[1]]
    rt = {word(va) for va in rd}
    print("\n.rdata words into .text: %d at %d targets, %d of them [[func]] starts"
          % (len(rd), len(rt), len(rt & startset)))


if __name__ == "__main__":
    main()
