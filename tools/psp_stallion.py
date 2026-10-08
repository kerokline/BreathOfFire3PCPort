#!/usr/bin/env python
"""The PSP's Stallion and Holy Mantle measurements (docs/psp-stallion.md).

    python tools/psp_stallion.py clut   --jp CUE --psp ISO              # area 67 / 166 palette rows decoded
    python tools/psp_stallion.py render --jp CUE --psp ISO --out DIR    # the recolour as PNGs (DIR under analysis/)
    python tools/psp_stallion.py tables --us CUE --jp CUE --psp-eu ISO  # item / ability tables: GAME.EMI vs the PSP-EU ELF
    python tools/psp_stallion.py boot   --psp ISO                       # is BOOT.BIN a plain ELF?

Reads the player's own discs through tools/psx_disc.py (cue/bin and ISO). Prints
counts, hue families, ids, offsets and field names - never section bytes, names
or palettes (CLAUDE.md rule 1); `render` writes PNGs, which belong in the
gitignored analysis/ tree only.

The constants this rests on, each measured 2026-10-06 (docs/psp-stallion.md):
  - an area's palette section is type 0 at 0x8002D800: 32-byte rows of 16
    RGB555 entries; a sprite's palette is 32 entries ("CLUT number" n = rows
    2n, 2n+1; Sprite_RestoreClut, docs/sprite-draw-order.md);
  - the sprite sheet is the type-3 section at 0x0E001000, 8 bits a texel,
    0x800-byte tiles of 32 x 32 halfwords, 16 tiles across;
  - the US disc's GAME.EMI section 0 loads at 0x80195800, the JP's at
    0x80196800; the record layouts are the sibling's names/items.toml and
    abilities.toml [meta]; the PSP-EU ELF holds the US layout, located here by
    content, never by address.
"""
import argparse, colorsys, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import region_diff  # noqa: E402

PAL, SHEET = 0x8002D800, 0x0E001000
AREAS = ("WORLD01/AREA067.EMI", "WORLD04/AREA166.EMI")
JP_TABLES = {"consumable": (0x801C995C, 14, 92), "key": (0x801C9E64, 12, 16), "weapon": (0x801C9F24, 20, 83),
             "armour": (0x801CA5A0, 18, 68), "accessory": (0x801CAA68, 16, 52), "ability": (0x801CB230, 16, 227)}
US_TABLES = {"consumable": (0x801C8964, 18), "key": (0x801C8FDC, 16), "weapon": (0x801C90DC, 24),
             "armour": (0x801C98A4, 22), "accessory": (0x801C9E7C, 20), "ability": (0x801CA718, 20)}


def section(build, key, dest):
    for _, t, d, x in build.sections(key):
        if (d & 0x7FFFFFFF if t == 0 else d & ~1) == (dest & 0x7FFFFFFF if dest & 0x80000000 else dest):
            return x
    raise KeyError(hex(dest))


def rgb(c):
    return (c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3


def hue(c):
    r, g, b = rgb(c)
    h, l, s = colorsys.rgb_to_hls(r / 255, g / 255, b / 255)
    if s < 0.15 or l < 0.06:
        return "grey"
    h *= 360
    for lim, n in ((15, "red"), (45, "brown/orange"), (70, "yellow"), (160, "green"), (200, "cyan"),
                   (260, "blue"), (300, "purple"), (345, "magenta"), (361, "red")):
        if h < lim:
            return n


def cmd_clut(a):
    jp, psp = region_diff.Build("psx-jp", a.jp), region_diff.Build("psp-jp", a.psp)
    for key in AREAS:
        x, y = section(jp, key, PAL), section(psp, key, PAL)
        for r in range(len(x) // 32):
            X, Y = struct.unpack_from("<16H", x, 32 * r), struct.unpack_from("<16H", y, 32 * r)
            mv = [k for k in range(16) if X[k] != Y[k]]
            if not mv:
                continue
            fam = lambda T: dict(sorted(__import__("collections").Counter(hue(T[k]) for k in mv).items()))
            lum = lambda T: round(sum(sum(rgb(T[k])) / 3 for k in mv) / len(mv))
            print("%s row %2d (palette %d): %2d of 16 move; JP %s mean %d -> PSP %s mean %d"
                  % (key.split("/")[1], r, r // 2, len(mv), fam(X), lum(X), fam(Y), lum(Y)))


def cmd_render(a):
    from PIL import Image
    os.makedirs(a.out, exist_ok=True)
    for bid, path in (("psx-jp", a.jp), ("psp-jp", a.psp)):
        b = region_diff.Build(bid, path)
        for key in AREAS:
            sheet, pal = section(b, key, SHEET), section(b, key, PAL)
            n, W = len(sheet) // 0x800, 16 * 64
            H = n // 16 * 32
            img = bytearray(W * H)
            for t in range(n):
                for y in range(32):
                    o = ((t // 16) * 32 + y) * W + (t % 16) * 64
                    img[o:o + 64] = sheet[t * 0x800 + y * 64: t * 0x800 + y * 64 + 64]
            out = Image.new("RGBA", (W, H * 4), (24, 24, 32, 255))
            for p in range(4):
                cols = [(0, 0, 0, 0)] * 256
                for i in range(1, 32):
                    c, = struct.unpack_from("<H", pal, 64 * p + 2 * i)
                    cols[i] = rgb(c) + (255 if c else 0,)
                im = Image.new("RGBA", (W, H))
                im.putdata([cols[v] for v in img])
                out.alpha_composite(im, (0, H * p))
            name = "%s_%s_sheet_palettes0to3.png" % (key.split("/")[1][:-4], bid)
            out.save(os.path.join(a.out, name))
            print("wrote", name)


def cmd_tables(a):
    us_b, jp_b = region_diff.Build("psx-us", a.us), region_diff.Build("psx-jp", a.jp)
    us = us_b.sections("ETC/GAME.EMI")[0][3]
    jp = jp_b.sections("ETC/GAME.EMI")[0][3]
    p = region_diff.Build("psp-eu", a.psp_eu)
    elf = p.disc.read([f for f in p.disc.files if f.endswith("SYSDIR/BOOT.BIN")][0])
    for cat, (ja, js, n) in JP_TABLES.items():
        ua, us_ = US_TABLES[cat]
        J = [jp[ja - 0x80196800 + js * i:][:js] for i in range(n)]
        U = [us[ua - 0x80195800 + us_ * i:][:us_] for i in range(n)]
        num = (lambda r: r[:8]) if cat == "ability" else None
        jn = [r[:8] if cat == "ability" else r[8:] for r in J]
        un = [r[:8] if cat == "ability" else r[12:] for r in U]
        at = elf.find(b"".join(U[1:4]))
        P = [elf[at - us_ + us_ * i:][:us_] for i in range(n)] if at >= 0 else None
        pn = [r[:8] if cat == "ability" else r[12:] for r in P] if P else None
        print("%-10s %3d records; JP vs US numbers differ %s; PSP-EU ELF copy at %s: numbers differ %s, names differ %s"
              % (cat, n, [i for i in range(n) if jn[i] != un[i]], hex(at - us_) if P else None,
                 [i for i in range(n) if pn[i] != un[i]] if P else "-",
                 [i for i in range(n) if P[i] != U[i] and pn[i] == un[i]] if P else "-"))
        if cat == "accessory":
            print("   accessory 21: JP == US numbers %s; PSP-EU == US %s" % (jn[21] == un[21], pn[21] == un[21] if P else None))


def cmd_boot(a):
    p = region_diff.Build("psp", a.psp)
    for f in p.disc.files:
        if f.endswith("SYSDIR/BOOT.BIN"):
            d = p.disc.read(f)
            print(f, len(d), "plain ELF" if d[:4] == b"\x7fELF" else "not an ELF, magic %r" % d[:4])


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name, fn in (("clut", cmd_clut), ("render", cmd_render)):
        s = sub.add_parser(name); s.add_argument("--jp", required=True); s.add_argument("--psp", required=True)
        if name == "render":
            s.add_argument("--out", required=True)
        s.set_defaults(fn=fn)
    s = sub.add_parser("tables"); s.add_argument("--us", required=True); s.add_argument("--jp", required=True)
    s.add_argument("--psp-eu", required=True); s.set_defaults(fn=cmd_tables)
    s = sub.add_parser("boot"); s.add_argument("--psp", required=True); s.set_defaults(fn=cmd_boot)
    a = ap.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
