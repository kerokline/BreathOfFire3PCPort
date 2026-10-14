#!/usr/bin/env python
"""Render the frames of an arena's sprite bank, the PC's chunk against a disc's.

The disc-data inventory's Q1 and Q3 (docs/disc-data-inventory.md 6,
docs/arena-render-1013.md): the port hand-edited animation scripts in eleven
arenas (and START carries PL012's) and one byte of Ryu's form data. This draws
both versions the way the PC's own code draws them, so the owner can judge.

    python tools/arena_render.py arenas --source DAT --source JP.cue --out analysis/shots/arena-render-1013
    python tools/arena_render.py ryud   --source DAT --source JP.cue --out analysis/shots/arena-render-1013

The model, from code that is ours and read term for term:
- arena: three u32 offsets, one bank per member (docs/arena-edits.md 1);
- bank: u32 offsets to the image table, the palette table, the scripts, the
  3-byte frame entries (image, script, pose) - Sprite_SetFrameQueueUpload
  0x5894D0 uploads image entry[0] and runs script entry[1];
- image: s16 w (VRAM cells), s16 h, u32 kind, then kind 1 six 5-bit texels a
  dword (Gfx_UploadPacked5 0x461FC0) or kind 2 LZSS (Gfx_UploadLzss 0x462070),
  w * h * 2 bytes: eight-bit texels, 2w wide;
- palette: a u32 offset table, 32 colours (BGR555) each (Sprite_LoadPalette);
- script: frames, steps, 2 x steps, then u16 frame offsets from the table's
  start; a frame is a count and that many 5-byte parts (Sprite_Draw 0x5935B0):
  byte 0 low nibble the size (8 + 8 * (n % 4) by 8 + 8 * (n / 4),
  SpriteCell_Sizes), bit 7 mirrored; s8 x, s8 y from the sprite's origin; u, v
  in the uploaded image;
- each part's texels are copied over what is drawn, colour 0 transparent, later
  parts on top, a mirrored part's row reversed inside its own box
  (tex_cells.cpp DrawCells / CellUnpack).

What is NOT modelled: the whole-sprite mirror (+0x2A, the entry's pose byte),
the object's palette choice (palette 0 is used; the same for both versions),
scale and shade. Both versions go through the same simplifications.

Output is derived from the player's files: --out must be outside the
repository or under analysis/ (CLAUDE.md rule 1).
"""
import argparse
import difflib
import hashlib
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import importer         # noqa: E402
import cache_inventory  # noqa: E402

ARENAS = ["PL012", "PL025", "PL026", "PL247", "PL257", "PL267", "PL278", "PL27A",
          "BPLD012", "BPLD015", "BPLD016", "START"]
RED, GREEN, BLUE = (255, 40, 40), (40, 220, 40), (60, 120, 255)
BG = (48, 48, 56)


# --- the bank -------------------------------------------------------------------

def lzss(src, total, used=None):
    """Gfx_UploadLzss's stream to `total` bytes; with `used` (a list), the
    input bytes consumed and the bytes produced before the cut are appended."""
    win = bytearray(0x200)
    r, out, i, flags = 0x1EE, bytearray(), 4, 0
    while len(out) < total:
        flags >>= 1
        if not flags & 0x100:
            flags = src[i] | 0xFF00
            i += 1
        if flags & 1:
            c = src[i]
            i += 1
            out.append(c)
            win[r] = c
            r = (r + 1) & 0x1FF
        else:
            b0, b1 = src[i], src[i + 1]
            i += 2
            off = b0 | (b1 & 0xF0) << 4
            for k in range((b1 & 0xF) + 3):
                c = win[(off + k) & 0x1FF]
                out.append(c)
                win[r] = c
                r = (r + 1) & 0x1FF
    if used is not None:
        used += [i, len(out)]
    return bytes(out[:total])


def packed5(src, total):
    out = bytearray()
    for g in range((total + 5) // 6):
        v = struct.unpack_from("<I", src, 4 * g)[0]
        for s in (0, 5, 10, 16, 21, 26):
            out.append(v >> s & 0x1F)
    return bytes(out[:total])


class Bank:
    def __init__(self, b):
        self.b = b
        self.img, self.pal, self.scr, self.fr = struct.unpack_from("<4I", b, 0)
        n = struct.unpack_from("<I", b, self.img)[0] // 4
        self.image_offs = [self.img + o for o in struct.unpack_from("<%dI" % n, b, self.img)]
        n = struct.unpack_from("<H", b, self.scr)[0] // 2
        self.script_offs = [self.scr + o for o in struct.unpack_from("<%dH" % n, b, self.scr)]
        self.entries = []
        for i in range((len(b) - self.fr) // 3):
            e = b[self.fr + 3 * i:self.fr + 3 * i + 3]
            self.entries.append(e)
        while self.entries and self.entries[-1] == b"\0\0\0":
            self.entries.pop()
        self._img = {}

    def image(self, k):
        """(width in texels, height, texels) of image k, eight-bit."""
        if k not in self._img:
            o = self.image_offs[k]
            w, h, kind = struct.unpack_from("<hhI", self.b, o)
            total = w * h * 2
            if kind == 1:
                t = packed5(self.b[o + 8:], total)
            elif kind == 2:
                t = lzss(self.b[o + 8:], total)
            else:
                t = self.b[o + 8:o + 8 + total]
            self._img[k] = (2 * w, h, t)
        return self._img[k]

    def palette(self, k=0):
        n = struct.unpack_from("<I", self.b, self.pal)[0] // 4
        o = self.pal + struct.unpack_from("<%dI" % n, self.b, self.pal)[k]
        out = []
        for i in range(32):
            c = struct.unpack_from("<H", self.b, o + 2 * i)[0]
            out.append(None if c == 0 else ((c & 31) * 255 // 31, (c >> 5 & 31) * 255 // 31, (c >> 10 & 31) * 255 // 31))
        return out

    def script(self, s):
        p = self.script_offs[s]
        nframes, nsteps = self.b[p], self.b[p + 1]
        steps = self.b[p + 2:p + 2 + 2 * nsteps]
        fo = p + 2 + 2 * nsteps
        frames = []
        for w in struct.unpack_from("<%dH" % nframes, self.b, fo):
            q = fo + w
            frames.append([self.b[q + 1 + 5 * k:q + 6 + 5 * k] for k in range(self.b[q])])
        return steps, frames

    def images_of(self, s):
        return sorted({e[0] for e in self.entries if e[1] == s and e[0] < len(self.image_offs)})


def stepped(steps):
    """Frame numbers a script's steps name (jumps skipped)."""
    out, i = set(), 0
    while i + 1 < len(steps):
        if steps[i] >= 0x80:
            i += 2
            continue
        out.add(steps[i + 1])
        i += 2
    return out


def part_box(p):
    n = p[0] & 0xF
    x, y = struct.unpack("bb", p[1:3])
    return x, y, 8 + 8 * (n % 4), 8 + 8 * (n // 4)


# --- drawing ----------------------------------------------------------------------

def pixels(bank, image, parts):
    """{(x, y): colour} of the frame as drawn, at 1x; out-of-palette indices magenta."""
    pal = bank.palette(0)
    iw, ih, tex = bank.image(image) if image is not None else (0, 0, b"")
    px = {}
    for p in parts:
        x, y, w, h = part_box(p)
        u, v, flip = p[3], p[4], p[0] & 0x80
        for r in range(h):
            for c in range(w):
                tu, tv = u + (w - 1 - c if flip else c), v + r
                if tu >= iw or tv >= ih:
                    continue        # outside the bank's image: VRAM this upload does not write
                t = tex[tv * iw + tu]
                col = pal[t] if t < 32 else (255, 0, 255)
                if col is not None:
                    px[(x + c, y + r)] = col
    return px


def compose(bank, image, parts, box, scale, marks):
    """The frame at `scale`, nearest; marks: {part index: colour} outlined."""
    x0, y0, x1, y1 = box
    im = Image.new("RGB", (x1 - x0, y1 - y0), BG)
    px = im.load()
    for (x, y), col in pixels(bank, image, parts).items():
        if x0 <= x < x1 and y0 <= y < y1:
            px[x - x0, y - y0] = col
    im = im.resize(((x1 - x0) * scale, (y1 - y0) * scale), Image.NEAREST)
    d = ImageDraw.Draw(im)
    for i, colr in marks.items():
        x, y, w, h = part_box(parts[i])
        d.rectangle([(x - x0) * scale, (y - y0) * scale, (x - x0 + w) * scale - 1, (y - y0 + h) * scale - 1],
                    outline=colr, width=2)
    return im


def difference(a, b, box, scale):
    """(image, counts): green only the PC opaque, blue only the disc, red both and different, grey same."""
    x0, y0, x1, y1 = box
    im = Image.new("RGB", (x1 - x0, y1 - y0), BG)
    px = im.load()
    n = {"pc_only": 0, "disc_only": 0, "recoloured": 0}
    for k in set(a) | set(b):
        x, y = k
        if not (x0 <= x < x1 and y0 <= y < y1):
            continue
        if k in a and k not in b:
            px[x - x0, y - y0] = GREEN
            n["pc_only"] += 1
        elif k in b and k not in a:
            px[x - x0, y - y0] = BLUE
            n["disc_only"] += 1
        elif a[k] != b[k]:
            px[x - x0, y - y0] = RED
            n["recoloured"] += 1
        else:
            px[x - x0, y - y0] = (110, 110, 118)
    return im.resize(((x1 - x0) * scale, (y1 - y0) * scale), Image.NEAREST), n


def holes(px):
    """Transparent pixels with opaque neighbours on both sides (left/right or above/below): 1-px seams."""
    if not px:
        return []
    xs = [k[0] for k in px]
    ys = [k[1] for k in px]
    return [(x, y) for y in range(min(ys), max(ys) + 1) for x in range(min(xs), max(xs) + 1)
            if (x, y) not in px and (((x - 1, y) in px and (x + 1, y) in px) or ((x, y - 1) in px and (x, y + 1) in px))]


HOT = (255, 0, 200)


def seams_view(px, box, scale):
    """The frame on a magenta ground, so see-through pixels show; its 1-px holes outlined white;
    a yellow line at x = 0, the sprite's origin."""
    x0, y0, x1, y1 = box
    im = Image.new("RGB", (x1 - x0, y1 - y0), HOT)
    p2 = im.load()
    for (x, y), c in px.items():
        if x0 <= x < x1 and y0 <= y < y1:
            p2[x - x0, y - y0] = c
    im = im.resize(((x1 - x0) * scale, (y1 - y0) * scale), Image.NEAREST)
    d = ImageDraw.Draw(im)
    if x0 <= 0 < x1:
        d.line([-x0 * scale, 0, -x0 * scale, im.height], fill=(255, 255, 0), width=1)
    h = holes(px)
    for x, y in h:
        if x0 <= x < x1 and y0 <= y < y1:
            d.rectangle([(x - x0) * scale, (y - y0) * scale, (x - x0 + 1) * scale - 1, (y - y0 + 1) * scale - 1],
                        outline=(255, 255, 255), width=2)
    return im, len(h)


def match(pc_parts, jp_parts):
    """Marks for each side: moved/changed red, added (PC only) green, dropped (disc only) blue."""
    mp, mj = {}, {}
    sm = difflib.SequenceMatcher(a=[bytes(p) for p in jp_parts], b=[bytes(p) for p in pc_parts], autojunk=False)
    for op, a0, a1, b0, b1 in sm.get_opcodes():
        if op == "equal":
            continue
        if op == "replace" and a1 - a0 == b1 - b0:
            for k in range(a1 - a0):
                mj[a0 + k] = RED
                mp[b0 + k] = RED
            continue
        for k in range(a0, a1):
            mj[k] = BLUE
        for k in range(b0, b1):
            mp[k] = GREEN
    return mp, mj


def describe(pc_parts, jp_parts):
    """Words for a frame's change: per paired part the field that moved."""
    out = []
    sm = difflib.SequenceMatcher(a=[bytes(p) for p in jp_parts], b=[bytes(p) for p in pc_parts], autojunk=False)
    for op, a0, a1, b0, b1 in sm.get_opcodes():
        if op == "equal":
            continue
        if op == "replace" and a1 - a0 == b1 - b0:
            for k in range(a1 - a0):
                q, p = jp_parts[a0 + k], pc_parts[b0 + k]
                dx = struct.unpack("b", p[1:2])[0] - struct.unpack("b", q[1:2])[0]
                dy = struct.unpack("b", p[2:3])[0] - struct.unpack("b", q[2:3])[0]
                if p[0] == q[0] and p[3:] == q[3:]:
                    out.append("x%+d" % dx if dy == 0 else "x%+d y%+d" % (dx, dy))
                else:
                    out.append("part %d %s>%s" % (b0 + k, q.hex(), p.hex()))
        else:
            out += ["added %d" % k for k in range(b0, b1)] + ["dropped %d" % k for k in range(a0, a1)]
    return out


def label(im, text):
    f = ImageFont.load_default()
    out = Image.new("RGB", (max(im.width, 8 * len(text)), im.height + 14), (20, 20, 24))
    out.paste(im, (0, 14))
    ImageDraw.Draw(out).text((2, 1), text, fill=(230, 230, 230), font=f)
    return out


def bounds(lists):
    xs0, ys0, xs1, ys1 = [], [], [], []
    for parts in lists:
        for p in parts:
            x, y, w, h = part_box(p)
            xs0.append(x); ys0.append(y); xs1.append(x + w); ys1.append(y + h)
    if not xs0:
        return (-8, -8, 8, 8)
    return (min(xs0) - 2, min(ys0) - 2, max(xs1) + 2, max(ys1) + 2)


def sheet(rows, title, width_max=1800):
    """Stack row images (each a PC | disc pair, labelled) into one sheet."""
    pad = 8
    lines, line, lw, lh = [], [], 0, 0
    for im in rows:
        if line and lw + im.width + pad > width_max:
            lines.append((line, lh)); line, lw, lh = [], 0, 0
        line.append(im); lw += im.width + pad; lh = max(lh, im.height)
    if line:
        lines.append((line, lh))
    W = max(sum(i.width + pad for i in l) for l, _ in lines) + pad
    H = sum(h + pad for _, h in lines) + pad + 24
    out = Image.new("RGB", (max(W, 600), H), (12, 12, 14))
    d = ImageDraw.Draw(out)
    d.text((pad, 6), title, fill=(255, 255, 255), font=ImageFont.load_default())
    y = 24
    for l, h in lines:
        x = pad
        for im in l:
            out.paste(im, (x, y)); x += im.width + pad
        y += h + pad
    return out


# --- sources ----------------------------------------------------------------------

class Sources:
    def __init__(self, paths, recipe=None):
        self.rec = importer.load_recipe(recipe)
        srcs = [importer.open_source(p) for p in paths]
        self.pc = next(s for s in srcs if isinstance(s, importer.PcSource))
        self.disc = next(s for s in srcs if isinstance(s, importer.DiscSource))
        self.files = {f["name"]: f for f in self.rec["file"]}

    def pair(self, name, tag):
        f = self.files[name]
        for slot, ch in enumerate(f["chunks"]):
            if ch["layer"] == "base" and ch["kind"] == 0 and ch["tag"] == tag:
                body = self.pc.get(name, slot, ("chunk", None, None))
                form, raw = cache_inventory.disc_form(self.disc, f, slot, ch, False)
                return body, raw, form
        raise SystemExit("%s: no base kind-0 chunk tagged 0x%X" % (name, tag))


def arena_banks(a):
    offs = struct.unpack_from("<3I", a, 0)
    out = []
    for o in offs:
        end = min([x for x in offs if x > o] + [len(a)])
        out.append(a[o:end])
    return out


def out_dir(p):
    o = os.path.abspath(p)
    if o.startswith(ROOT + os.sep) and not o.startswith(os.path.join(ROOT, "analysis") + os.sep):
        raise SystemExit("--out inside the repository must be under analysis/ (CLAUDE.md rule 1)")
    os.makedirs(o, exist_ok=True)
    return o


# --- the arenas -------------------------------------------------------------------

def render_arena(src, name, out, scale, readme):
    pc, jp, form = src.pair(name + ".DAT", 0x10000)
    lines = ["## %s (disc form `%s`; PC %d bytes, disc %d)" % (name, form, len(pc), len(jp))]
    rows, seam_cells = [], []
    for k, (bp, bj) in enumerate(zip(arena_banks(pc), arena_banks(jp))):
        if bp == bj:
            continue
        lines.append("bank %d differs (PC sha256 %s, disc %s)" % (k, hashlib.sha256(bp).hexdigest()[:12],
                                                                    hashlib.sha256(bj).hexdigest()[:12]))
        P, J = Bank(bp), Bank(bj)
        for s in range(min(len(P.script_offs), len(J.script_offs))):
            sp, fp = P.script(s)
            sj, fj = J.script(s)
            if (sp, fp) == (sj, fj):
                continue
            imgs = P.images_of(s) or J.images_of(s)
            image = imgs[0] if imgs else None
            steps_p, steps_j = stepped(sp), stepped(sj)
            lines.append("- script %d: %d frames PC, %d disc; steps %s; image %s; frames stepped (PC) %s"
                         % (s, len(fp), len(fj), "equal" if sp == sj else "DIFFER", image, sorted(steps_p)))
            for j in range(max(len(fp), len(fj))):
                a = fp[j] if j < len(fp) else None
                b = fj[j] if j < len(fj) else None
                if a == b:
                    continue
                box = bounds([x for x in (a, b) if x])
                cells = []
                if a is not None and b is not None:
                    mp, mj = match(a, b)
                    words = describe(a, b)
                else:
                    mp, mj, words = {}, {}, ["frame only in the %s" % ("PC" if b is None else "disc")]
                drawn = j in (steps_p | steps_j)
                tag = "%s s%d f%d%s" % (name, s, j, "" if drawn else " (never stepped)")
                blank = Image.new("RGB", ((box[2] - box[0]) * scale, (box[3] - box[1]) * scale), BG)
                cells.append(label(compose(P, image, a, box, scale, mp) if a is not None else blank, "PC  " + tag))
                cells.append(label(compose(J, image, b, box, scale, mj) if b is not None else blank, "JP  " + tag))
                pa = pixels(P, image, a) if a is not None else {}
                pb = pixels(J, image, b) if b is not None else {}
                dim, n = difference(pa, pb, box, scale)
                cells.append(label(dim, "diff: green PC only, blue JP only, red recoloured"))
                ha, hb = holes(pa), holes(pb)
                pairim = Image.new("RGB", (sum(c.width + 4 for c in cells), max(c.height for c in cells)), (12, 12, 14))
                xx = 0
                for c in cells:
                    pairim.paste(c, (xx, 0))
                    xx += c.width + 4
                rows.append((0 if drawn else 1, len(rows), pairim))
                if drawn and a is not None and b is not None and a != b:
                    for side, px in (("PC", pa), ("JP", pb)):
                        im, nh = seams_view(px, box, scale)
                        seam_cells.append(label(im, "%s %s, holes %d (white)" % (side, tag, nh)))
                words.append("pixels: %d PC only, %d disc only, %d recoloured; 1-px holes PC %d, disc %d"
                             % (n["pc_only"], n["disc_only"], n["recoloured"], len(ha), len(hb)))
                summary = {}
                for w in words:
                    summary[w] = summary.get(w, 0) + 1
                lines.append("  - frame %d%s: %s" % (j, "" if drawn else " (never stepped)",
                                                     ", ".join("%s x%d" % (w, n) if n > 1 else w for w, n in summary.items())))
    fn = "%s.png" % name
    if rows:
        rows = [r[2] for r in sorted(rows, key=lambda r: r[:2])]
        sheet(rows, "%s - drawn frames first. PC (left, port-edited) | JP disc (right). Outlines: red part moved/changed, green PC-only part, blue disc-only part. %dx nearest." % (name, scale)).save(os.path.join(out, fn))
        lines.insert(1, "sheet `%s`, %d frame pairs" % (fn, len(rows)))
        if seam_cells:
            fs = "%s_seams.png" % name
            sheet(seam_cells, "%s - the drawn frames that differ, PC then JP, on a magenta ground (see-through pixels show); "
                  "white: 1-px holes; yellow: x = 0. %dx." % (name, scale), width_max=1400).save(os.path.join(out, fs))
            lines.insert(2, "sheet `%s`: the %d drawn frames that differ, on a magenta ground" % (fs, len(seam_cells) // 2))
    else:
        lines.insert(1, "no differing frame")
    readme.extend(lines + [""])


def cmd_arenas(a):
    out = out_dir(a.out)
    src = Sources(a.source, a.recipe)
    readme = ["# Arena renders, %s: the PC's chunk against %s" % (a.date, src.disc.id), "",
              "Made by `tools/arena_render.py arenas`. Game-derived: never commit. Each sheet pairs every",
              "frame that differs, PC left, disc right; outlines: red a part the port moved or changed, green",
              "a part only the PC has, blue a part only the disc has; the third panel is the pixel difference.",
              "Words per frame: x+1 means the PC's part sits one pixel right of the disc's. A frame is",
              "'never stepped' when no step of its script names it. 1-px holes: transparent pixels with",
              "opaque neighbours on both sides.", ""]
    for name in a.arena or ARENAS:
        render_arena(src, name, out, a.scale, readme)
        print(name, "done")
    with open(os.path.join(out, "README.md"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(readme))


# --- Ryu's byte -------------------------------------------------------------------

RYUD = {"RYUD00": 0x4F00C, "RYUD01": 0x3400C, "RYUD02": 0x4600C, "RYUD03": 0x2B00C}


def image_im(t, w, h, pal, x0=0, x1=None):
    x1 = w if x1 is None else x1
    im = Image.new("RGB", (x1 - x0, h), BG)
    px = im.load()
    for yy in range(h):
        for xx in range(x0, x1):
            v = t[yy * w + xx]
            c = pal[v] if v < 32 else (255, 0, 255)
            if c is not None:
                px[xx - x0, yy] = c
    return im


def outline_texels(im, cells, sc, x0=0):
    """A thin red square round each texel in `cells` ((x, y) in image texels)."""
    d = ImageDraw.Draw(im)
    for x, y in cells:
        d.rectangle([(x - x0) * sc, y * sc, (x - x0 + 1) * sc - 1, (y + 1) * sc - 1], outline=RED, width=max(1, sc // 8))
    return im


def stream_words(b, bank, k):
    o = bank.image_offs[k]
    end = bank.image_offs[k + 1] if k + 1 < len(bank.image_offs) else bank.pal
    w, h, kind = struct.unpack_from("<hhI", b, o)
    if kind != 2:
        return "kind %d" % kind
    used = []
    t = lzss(b[o + 8:], w * h * 2, used)
    return ("LZSS record of %d bytes after the header: stream consumed %d, produced %d of %d texels, %d texels above 31"
            % (end - o - 8, used[0], used[1], w * h * 2, sum(1 for v in t if v > 31)))


def cmd_ryud(a):
    out = out_dir(a.out)
    src = Sources(a.source, a.recipe)
    lines = []
    for name, tag in RYUD.items():
        pc, jp, form = src.pair(name + ".DAT", tag)
        at = [i for i in range(len(pc)) if pc[i] != jp[i]]
        P, J = Bank(pc), Bank(jp)
        k = max(i for i, o in enumerate(P.image_offs) if o <= at[0])
        wp, hp, tp = P.image(k)
        wj, hj, tj = J.image(k)
        diff = [i for i in range(len(tp)) if tp[i] != tj[i]]
        lines.append("%s (disc form `%s`): bytes differ at %s (PC %s, disc %s); image %d (%d x %d texels, starts 0x%X); "
                     "%d texels differ, the first (%d,%d)"
                     % (name, form, ", ".join("+0x%X" % i for i in at), " ".join("%02X" % pc[i] for i in at),
                        " ".join("%02X" % jp[i] for i in at), k, wp, hp, P.image_offs[k], len(diff),
                        diff[0] % wp if diff else -1, diff[0] // wp if diff else -1))
        lines.append("  PC: " + stream_words(pc, P, k))
        lines.append("  disc: " + stream_words(jp, J, k))
        others = [i for i in range(len(P.image_offs)) if i != k and P.image(i) != J.image(i)]
        lines.append("  other images differing PC against disc: %s" % (others or "none"))
        users = [s for s in range(len(P.script_offs)) if k in P.images_of(s)]
        lines.append("  scripts drawing image %d (frame entries): %s" % (k, users))
        if name != "RYUD00":
            continue
        pal = P.palette(0)
        sc = a.ryud_scale
        cells = [(i % wp, i // wp) for i in diff]
        ims = []
        for tag2, t in (("PC", tp), ("JP", tj)):
            im = image_im(t, wp, hp, pal).resize((wp * sc, hp * sc), Image.NEAREST)
            ims.append(label(outline_texels(im, cells, sc), "%s  %s image %d, %dx, the %d differing texels outlined"
                             % (tag2, name, k, sc, len(diff))))
        if diff:
            cx = diff[0] % wp
            x0, x1 = max(0, cx - 24), min(wp, cx + 40)
            for tag2, t in (("PC", tp), ("JP", tj)):
                im = image_im(t, wp, hp, pal, x0, x1).resize(((x1 - x0) * 16, hp * 16), Image.NEAREST)
                first = [(diff[0] % wp, diff[0] // wp)]
                d = ImageDraw.Draw(im)
                fx, fy = first[0]
                d.ellipse([(fx - x0) * 16 - 12, fy * 16 - 12, (fx - x0) * 16 + 28, fy * 16 + 28], outline=RED, width=3)
                ims.append(label(outline_texels(im, [c for c in cells if x0 <= c[0] < x1], 16, x0),
                                 "%s  columns %d..%d, 16x; circled: the first texel the stream changes" % (tag2, x0, x1 - 1)))
        for s in users:
            _, fr = P.script(s)
            _, frj = J.script(s)
            for j, parts in enumerate(fr):
                box = bounds([parts, frj[j]])
                hit = {}
                for n, p in enumerate(parts):
                    x, y, w, h = part_box(p)
                    if any(p[3] <= cx2 < p[3] + w and p[4] <= cy2 < p[4] + h for cx2, cy2 in cells):
                        hit[n] = RED
                for tag2, bank, ps in (("PC", P, parts), ("JP", J, frj[j])):
                    im = compose(bank, k, ps, box, sc, hit)
                    ims.append(label(im, "%s  script %d frame %d as drawn, %dx; red: parts sampling a changed texel"
                                     % (tag2, s, j, sc)))
        sheet(ims, "Ryu's form data %s image %d: PC (port) against the JP disc" % (name, k),
              width_max=2200).save(os.path.join(out, "RYUD_image%d.png" % k))
    with open(os.path.join(out, "RYUD.md"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("# Ryu's one byte (tools/arena_render.py ryud)\n\n" + "\n".join("- " + l if not l.startswith("  ") else l
                                                                                 for l in lines) + "\n")
    print("\n".join(lines))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    for n in ("arenas", "ryud"):
        p = s.add_parser(n)
        p.add_argument("--source", action="append", required=True, help="the PC's DAT/ and one disc")
        p.add_argument("--recipe")
        p.add_argument("--out", required=True)
        p.add_argument("--date", default="")
        p.add_argument("--scale", type=int, default=4)
        p.add_argument("--ryud-scale", type=int, default=8)
        p.add_argument("--arena", action="append", default=[])
    a = ap.parse_args()
    {"arenas": cmd_arenas, "ryud": cmd_ryud}[a.cmd](a)


if __name__ == "__main__":
    main()
