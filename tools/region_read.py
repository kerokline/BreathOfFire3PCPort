#!/usr/bin/env python
"""Decode the Western discs' four data changes (docs/region-diff.md section 8).

    python tools/region_read.py --jp DISC --later DISC [--pal DISC]

Reads the rows region_diff.py found through the structures the game's code
reads them with, and prints what changed as fields, cells and values:

  area4   AREA004 section 8 (0x80104000, the area block at AreaMap_Header,
          PC tag 0xC8000): header, the corner heights at +0x30, the cell
          bytes AreaMap_Bytes (header dword +0x14, x 4), the tile words at
          4 x the header's word +2, the texture dwords after them - which
          cells' byte, tile or texture changed, from what to what
  area4n  AREA004 section 10 (0x8002A000, PC tag 0xC0800, the cell nibble map
          AreaMap_CellNibble 0x592890 reads for the battle placement)
  cues    every BPLCHAR type-8 record: the cue entries' fields (the sibling's
          SOUND_CUES.md: flags, pan|program, tone|priority, chord|voice)
  fix     src/game/area4_walls.cpp's tables applied to JP's sections 8 and 10, against --later's
  pal     the eight area banks where --pal differs from --later: samples
          dropped and added (by md5), and the tones that point at them

Prints coordinates, field values, counts and 12-hex md5s - never section
bytes (CLAUDE.md rule 1).
"""
import argparse, collections, hashlib, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import region_diff  # noqa: E402

AREA4 = "WORLD00/AREA004.EMI"


def area_layout(m):
    w, d = m[0], m[1]
    base = struct.unpack_from("<H", m, 2)[0]
    n = w * d
    plane = struct.unpack_from("<I", m, 0x14)[0] * 4
    return dict(w=w, d=d, n=n, corners=0x30, plane=plane, tiles=4 * base,
                tex=4 * (base + (n + 1) // 2), tex_end=4 * struct.unpack_from("<H", m, 4)[0])


def cmd_area4(J, L):
    x, y = J.sections(AREA4)[8][3], L.sections(AREA4)[8][3]
    L_ = area_layout(x)
    w, n = L_["w"], L_["n"]
    print("map %d x %d cells; corners +0x%X, cell bytes +0x%X, tile words +0x%X, textures +0x%X..+0x%X"
          % (w, L_["d"], L_["corners"], L_["plane"], L_["tiles"], L_["tex"], L_["tex_end"]))
    print("header equal:", x[:0x30] == y[:0x30], "; corner heights equal:", x[0x30:L_["plane"]] == y[0x30:L_["plane"]])
    p = L_["plane"]
    cells = [(i % w, i // w, x[p + i], y[p + i]) for i in range(n) if x[p + i] != y[p + i]]
    print("cell bytes changed: %d, values %s" % (len(cells), dict(collections.Counter(("0x%02X" % a, "0x%02X" % b) for _, _, a, b in cells))))
    cols = collections.defaultdict(list)
    for cx, cz, _, _ in cells:
        cols[cx].append(cz)
    for cx in sorted(cols):
        print("   x %2d: z %s" % (cx, runs(sorted(cols[cx]))))
    jp_walls = [(cx, cz) for cx in sorted(cols) for cz in range(0, L_["d"]) if x[p + cz * w + cx] & 0xF0 == 0x10
                and min(cols[cx]) - 3 <= cz <= max(cols[cx]) + 3]
    print("   the same columns' cells already 0x1? on JP:", jp_walls)

    def h(buf, cx, cz):
        return sum(struct.unpack_from("<4b", buf, 0x30 + 4 * (cz * w + cx))) / 4
    print("   mean corner height across x 24..31 at z 13, 29, 49:")
    for cz in (13, 29, 49):
        print("      z %d: %s" % (cz, [round(h(x, cx, cz)) for cx in range(24, 32)]))
    t = L_["tiles"]
    dt = collections.Counter()
    moved = []
    for i in range(n):
        a, b = struct.unpack_from("<H", x, t + 2 * i)[0], struct.unpack_from("<H", y, t + 2 * i)[0]
        if a != b:
            dt[b - a] += 1
            if b - a != 1:
                moved.append((i % w, i // w))
    print("tile words changed: %d, by delta %s; re-pointed cells: %s" % (sum(dt.values()), dict(dt), moved))
    import difflib
    ta, tb = x[L_["tex"]:L_["tex_end"]], y[L_["tex"]:L_["tex_end"]]
    wa = [ta[i:i + 4] for i in range(0, len(ta), 4)]
    wb = [tb[i:i + 4] for i in range(0, len(tb), 4)]
    ops = [op for op in difflib.SequenceMatcher(None, wa, wb, autojunk=False).get_opcodes() if op[0] != "equal"]
    print("texture records: %d JP, %d later; edits %s" % (len(wa), len(wb), ops))


def runs(v):
    out, s = [], v[0]
    for a, b in zip(v, v[1:] + [None]):
        if b != a + 1:
            out.append("%d" % s if s == a else "%d..%d" % (s, a))
            if b is not None:
                s = b
    return ", ".join(out)


def nib(buf, i):
    b = buf[i >> 1]
    return (b & 15) if i & 1 else (b >> 4)


def cmd_area4n(J, L):
    m = J.sections(AREA4)[8][3]
    mL = L.sections(AREA4)[8][3]
    w, n, p = m[0], m[0] * m[1], area_layout(m)["plane"]
    x, y = J.sections(AREA4)[10][3], L.sections(AREA4)[10][3]
    print("nibble map: %d cells in %d bytes of %d; past the map equal and zero: %s"
          % (n, (n + 1) // 2, len(x), x[(n + 1) // 2:] == y[(n + 1) // 2:] and not any(x[(n + 1) // 2:])))
    for i in range(n):
        if nib(x, i) != nib(y, i):
            print("   cell (%d, %d): nibble %d -> %d; its cell byte JP 0x%02X, later 0x%02X"
                  % (i % w, i // w, nib(x, i), nib(y, i), m[p + i], mL[p + i]))


def cmd_cues(J, L):
    pat = collections.Counter()
    for key in sorted(J.emis):
        if not key.startswith("BPLCHAR/") or key not in L.emis:
            continue
        for s, t in zip(J.sections(key), L.sections(key)):
            if s[1] != 8:
                continue
            fa = tuple((c[1] & 0x7F, c[2] >> 4, c[2] & 15, c[3] & 0x1F, (c[3] >> 5) & 3)
                       for c in (s[3][i:i + 4] for i in range(0, len(s[3]), 4)))
            pb = tuple(c & 15 for c in t[3][2::4])
            fam = key.split("/")[1].rstrip(".EMI").rstrip("0123456789_A")
            pat[(fam, fa, tuple(c[2] for c in fa), pb)] += 1
    print("(file family, bank) count: cue (program, tone, priority, voice, chord) on JP; priorities later")
    for (fam, fa, pa, pb), k in sorted(pat.items()):
        print("   %-5s x%-3d %s  priorities JP %s later %s%s" % (fam, k, fa, pa, pb, "" if pa == pb else "  <- differs"))


def vab(vh, vb):
    ps, ts, vs = struct.unpack_from("<HHH", vh, 0x12)
    tbl = len(vh) - 512
    sizes = [struct.unpack_from("<H", vh, tbl + 2 * i)[0] * 8 for i in range(vs + 1)]
    samples, pos = {}, 0
    for i in range(1, vs + 1):
        pos += sizes[i - 1] if i > 1 else sizes[0]
        samples[i] = vb[pos:pos + sizes[i]]
    tones = {}
    for pr in range(ps):
        for tn in range(16):
            o = 0x820 + pr * 0x200 + tn * 32
            if o + 32 <= tbl:
                v = struct.unpack_from("<H", vh, o + 0x16)[0]
                if v:
                    tones[(pr, tn)] = v
    return vs, samples, tones


def cmd_pal(L, P):
    md = lambda b: hashlib.md5(b).hexdigest()[:12]  # noqa: E731
    for key in sorted(L.emis):
        if "/AREA" not in key or key not in P.emis:
            continue
        sl, sp = L.sections(key), P.sections(key)
        if not sl or sl[0][1] != 6 or (sl[0][3] == sp[0][3] and sl[2][3] == sp[2][3]):
            continue
        vl, sml, tl = vab(sl[0][3], sl[2][3])
        vp, smp, tp = vab(sp[0][3], sp[2][3])
        hl = {md(v): i for i, v in sml.items()}
        hp = {md(v): i for i, v in smp.items()}
        gone = [(i, len(v), md(v)) for i, v in sml.items() if md(v) not in hp]
        new = [(i, len(v), md(v)) for i, v in smp.items() if md(v) not in hl]
        gi = {g[0] for g in gone}
        ts = sorted(k for k, v in tl.items() if v in gi)
        cues = [i for i in range(len(sl[1][3]) // 4)
                if (sl[1][3][4 * i + 1] & 0x7F, sl[1][3][4 * i + 2] >> 4) in ts]
        print("%s: samples %d -> %d; dropped %s; added %s; tones %s now -> %s; cue entries on them %s; cue table equal %s"
              % (key, vl, vp, gone, new, ts, sorted({tp.get(k) for k in ts}), cues, sl[1][3] == sp[1][3]))


def cmd_fix(J, L):
    """Apply src/game/area4_walls.cpp's own tables (parsed from the source, so the
    check is of the code's table, not a copy) to the JP disc's AREA004 sections 8
    and 10, and compare with the later disc's."""
    import re
    src = open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                            "src", "game", "area4_walls.cpp"), encoding="utf-8").read()
    walls = src[src.index("kWalls[]"):src.index("};", src.index("kWalls[]"))]
    place = src[src.index("kPlacement[]"):src.index("};", src.index("kPlacement[]"))]
    runs_ = [tuple(map(int, m)) for m in re.findall(r"\{(\d+), (\d+), (\d+), (\d+)\}", walls)]
    nibs = [tuple(map(int, m)) for m in re.findall(r"\{(\d+), (\d+), (\d+)\}", place)]
    wall = int(re.search(r"kWall = 0x([0-9A-Fa-f]+)", src).group(1), 16)
    m = bytearray(J.sections(AREA4)[8][3])
    n = bytearray(J.sections(AREA4)[10][3])
    ml, nl = L.sections(AREA4)[8][3], L.sections(AREA4)[10][3]
    w = m[0]
    p = area_layout(m)["plane"]
    cells = 0
    for x0, z0, x1, z1 in runs_:
        for z in range(z0, z1 + 1):
            for x in range(x0, x1 + 1):
                assert m[p + w * z + x] == 0, (x, z)
                m[p + w * z + x] = wall
                cells += 1
    for x, z, jp in nibs:
        i = w * z + x
        assert nib(n, i) == jp, (x, z)
        n[i >> 1] = (n[i >> 1] & 0xF0) if i & 1 else (n[i >> 1] & 0x0F)
    plane_same = m[p:p + w * m[1]] == ml[p:p + w * m[1]]
    rest = [i for i in range(len(m)) if m[i] != ml[i]]
    print("table: %d runs, %d wall cells (0x%02X), %d placement cells" % (len(runs_), cells, wall, len(nibs)))
    print("section 8 cell bytes after the fix == later disc's: %s" % plane_same)
    print("section 8 bytes still differing: %d, all in the tile words and texture records: %s"
          % (len(rest), all(area_layout(m)["tiles"] <= i < area_layout(m)["tex_end"] for i in rest)))
    print("section 10 after the fix == later disc's: %s" % (bytes(n) == nl))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--jp", required=True)
    ap.add_argument("--later", required=True, help="a Western PSX disc (US)")
    ap.add_argument("--pal", help="a PAL disc (FR or DE)")
    ap.add_argument("what", nargs="*", default=["area4", "area4n", "cues", "pal"])
    a = ap.parse_args()
    J, L = region_diff.Build("jp", a.jp), region_diff.Build("later", a.later)
    for w in a.what:
        print("==", w)
        if w == "pal":
            if a.pal:
                cmd_pal(L, region_diff.Build("pal", a.pal))
        else:
            {"area4": cmd_area4, "area4n": cmd_area4n, "cues": cmd_cues, "fix": cmd_fix}[w](J, L)


if __name__ == "__main__":
    sys.exit(main())
