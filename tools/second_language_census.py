#!/usr/bin/env python
"""Second-language census for the reading-row round (group SL).

Reads the PSX language overlays a PC install holds (DAT\\<tag>.<NAME>.DAT,
DIV-0005) and the shipped files under them, pairs every area text block and
the two system pools slot by slot across every ordered pair of languages,
and measures how the second language fits into the reading rows over the
main language's pages. Reads only; writes counts, area names and slot
numbers (rule 1): no message bytes. Run it on a COPY of the install's DAT
folder, never on the folder the owner plays from.

    python -I tools/second_language_census.py run --dat COPY\\DAT --out census.json
    python -I tools/second_language_census.py tables census.json [--kind area|SYSPOOL|BATPOOL]

Model (docs/second-language-census.md section 1 states it in full):
  - a text row is W = 192 units (16 cells of 12) wide; a page with reading
    rows holds two text rows and two reading rows;
  - a glyph's width is its layer's advance (FIRST.DAT kind 4; the chunk's tag
    is the space) - 12 for Japanese, 8 for Latin (5 for ' and ,);
  - a reading glyph is that width times 1/2 or 2/3;
  - inserts are budgeted as the sibling's ruby_fit: 03 / 04 nn 5 glyphs,
    07 nn 8, 08 nn 11, at the layer's ordinary letter width;
  - Latin breaks at spaces only; Japanese between any two glyphs;
  - a row opening on 0x2A / 0x3C hangs that glyph into the margin.
"""
import argparse
import collections
import glob
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dat  # noqa: E402

LANGS = ["ja-JP", "en-US", "en-150", "fr-FR", "de-DE"]
W = 192
ROWS_ANNOTATED = 2
SCALES = {"1/2": 0.5, "2/3": 2.0 / 3.0}
ARG1 = {0x04, 0x05, 0x07, 0x08, 0x0A, 0x0C, 0x0F, 0x16}   # loc_build.ARG1 / message_end
INSERT_GLYPHS = {0x03: 5, 0x04: 5, 0x07: 8, 0x08: 11}
HANG = (0x2A, 0x3C)
JA_FIRST = 0x993
POOL_TAG = 0x4000


def chunk(path, kind, tag):
    if not os.path.exists(path):
        return None
    blob, cs = dat.load(path)
    for c in cs:
        if c.kind == kind and c.tag == tag:
            return blob[c.offset:c.offset + c.size]
    return None


def message_end(buf, i):
    """loc_build.message_end, verbatim in behaviour."""
    while True:
        b = buf[i] if i < len(buf) else 0
        if b == 0:
            return i + 1
        if b == 0x14:
            options = buf[i + 3] & 0xF
            i += 4
            for _ in range(options):
                while i < len(buf) and buf[i]:
                    i += 2 if buf[i] & 0x80 or buf[i] in ARG1 else 1
                i += 1
            return i
        i += 2 if (b & 0x80 or b in ARG1) else 1


def block_slots(block):
    n = struct.unpack_from("<H", block, 0)[0] // 2
    offs = struct.unpack_from("<%dH" % n, block, 0)
    return [(o, block[o:message_end(block, o)] if o < len(block) else b"\0") for o in offs]


class Font:
    def __init__(self, first_dat):
        blob, cs = dat.load(first_dat)
        adv = [c for c in cs if c.kind == 4][0]
        self.adv = blob[adv.offset:adv.offset + adv.size]
        self.space = adv.tag
        t = [c for c in cs if c.kind == 3][0]
        self.table = blob[t.offset:t.offset + t.size]
        self.glyphs = t.size // 288

    def width(self, g):
        return self.adv[g] if 0 <= g < len(self.adv) else 12


def tokens(msg):
    """[(kind, value, glyph)] over a PC message; kind 'g' glyph, 'sp', 'nl',
    'pg', 'ins', 'col', 'choice', 'ctl', 'end'. The choice block is one token
    holding its options' token lists."""
    out, i = [], 0
    while i < len(msg):
        b = msg[i]
        if b == 0:
            out.append(("end", 0, None))
            return out
        if b == 0x14:
            opts, k = [], msg[i + 3] & 0xF if i + 3 < len(msg) else 0
            j = i + 4
            for _ in range(k):
                e = j
                while e < len(msg) and msg[e]:
                    e += 2 if msg[e] & 0x80 or msg[e] in ARG1 else 1
                opts.append(tokens(msg[j:e] + b"\0"))
                j = e + 1
            out.append(("choice", tuple(msg[i + 1:i + 4]), opts))
            out.append(("end", 0, None))
            return out
        if b & 0x80:
            g = ((b & 0x7F) << 8) | (msg[i + 1] if i + 1 < len(msg) else 0)
            out.append(("g", b, g))
            i += 2
            continue
        if b == 0x01:
            out.append(("nl", b, None))
        elif b == 0x02:
            out.append(("pg", b, None))
        elif b == 0x03:
            out.append(("ins", 0x03, None))
        elif b in (0x04, 0x07, 0x08):
            out.append(("ins", b, msg[i + 1] if i + 1 < len(msg) else 0))
            i += 2
            continue
        elif b == 0x05:
            out.append(("col", msg[i + 1] if i + 1 < len(msg) else 0, None))
            i += 2
            continue
        elif b == 0x06:
            out.append(("col", 0, None))
        elif b in ARG1:
            out.append(("ctl", b, msg[i + 1] if i + 1 < len(msg) else 0))
            i += 2
            continue
        elif b == 0x20:
            out.append(("sp", b, None))
        elif b <= 0x16:
            out.append(("ctl", b, None))
        else:
            out.append(("g", b, b - 0x26))
        i += 1
    out.append(("end", 0, None))
    return out


def is_foreign(msg, base_msg):
    """The port's own message, kept by the layer (an untranslated or
    out-of-block slot): equal to the shipped slot, or a two-byte glyph below
    the appended range (the Chinese table)."""
    if len(msg) > 1 and msg == base_msg:
        return True
    for k, v, g in tokens(msg):
        if k == "g" and v & 0x80 and g < JA_FIRST:
            return True
        if k == "choice":
            for o in g:
                for k2, v2, g2 in o:
                    if k2 == "g" and v2 & 0x80 and g2 < JA_FIRST:
                        return True
    return False


def letter_width(font, ja):
    return 12 if ja else 8


def units(font, ja, tok):
    k, v, g = tok
    if k == "g":
        return font.width(g)
    if k == "sp":
        return font.space
    if k == "ins":
        return INSERT_GLYPHS[v] * letter_width(font, ja)
    return 0


def pages_of(toks):
    """Split the prompt (no choice) into pages of rows of tokens."""
    pages, rows, row = [], [], []
    for t in toks:
        k = t[0]
        if k in ("end", "choice"):
            break
        if k == "nl":
            rows.append(row); row = []
        elif k == "pg":
            rows.append(row); pages.append(rows); rows, row = [], []
        else:
            row.append(t)
    rows.append(row)
    pages.append(rows)
    # a trailing empty page (a message ending on 02) is not a page
    pages = [p for p in pages if any(any(t[0] in ("g", "ins") for t in r) for r in p)]
    return pages


def row_width(font, ja, row):
    w, first = 0, True
    for t in row:
        u = units(font, ja, t)
        if first and t[0] == "g" and not (t[1] & 0x80) and t[1] in HANG:
            u = 0          # hangs into the margin
        if u:
            first = False
        w += u
    return w


def words(font, ja, rows, scale=1.0):
    """The text as break units: [(width, is_space)]. Latin breaks at spaces
    (and authored row ends, which become spaces); Japanese between glyphs."""
    out = []
    for ri, row in enumerate(rows):
        if ri and out and not ja:
            out.append([font.space * scale, True])
        cur = None
        for t in row:
            u = units(font, ja, t) * scale
            if t[0] == "sp":
                out.append([u, True]); cur = None
            elif t[0] in ("g", "ins"):
                if ja:
                    out.append([u, False])
                else:
                    if cur is None:
                        cur = [0.0, False]; out.append(cur)
                    cur[0] += u
    return out


def wrap(units_list, width=W):
    """Greedy rows over break units; a space at a row end costs nothing;
    a unit wider than the row is cut across rows."""
    rows, x, gap = 0, 0.0, 0.0
    for u, sp in units_list:
        if sp:
            if x > 0:
                gap += u
            continue
        if rows == 0:
            rows = 1
        if x > 0 and x + gap + u > width + 1e-6:
            rows += 1
            x = 0.0
        elif x > 0:
            x += gap
        gap = 0.0
        if x == 0:
            while u > width + 1e-6:
                u -= width
                rows += 1
        x += u
    return rows


def glyph_weight(rows):
    return sum(1 for r in rows for t in r if t[0] in ("g", "ins"))


def share(font, ja, pages, n_main_pages, main_weights):
    """The second's text split over the main's pages: pagewise when the counts
    agree, otherwise proportionally by the main's glyph count, at break-unit
    granularity. Returns a list (per main page) of break-unit lists at scale 1."""
    if len(pages) == n_main_pages:
        return [words(font, ja, p) for p in pages]
    allrows = []
    for p in pages:
        allrows.extend(p)
    us = words(font, ja, allrows)
    total = sum(main_weights) or 1
    bounds, acc = [], 0
    for w in main_weights:
        acc += w
        bounds.append(acc / total)
    tw = sum(u for u, sp in us if not sp) or 1
    out = [[] for _ in range(n_main_pages)]
    acc, pi = 0.0, 0
    for u, sp in us:
        mid = (acc + u / 2) / tw if not sp else acc / tw
        while pi < n_main_pages - 1 and mid > bounds[pi]:
            pi += 1
        out[pi].append([u, sp])
        if not sp:
            acc += u
    return out


def scale_units(us, s):
    return [[u * s, sp] for u, sp in us]


def ctl_profile(toks):
    ins = collections.Counter()
    col = 0
    other = collections.Counter()
    choice = None
    for k, v, g in toks:
        if k == "ins":
            ins[v] += 1
        elif k == "col" and v:
            col += 1
        elif k == "ctl":
            other[v] += 1
        elif k == "choice":
            choice = (v[2] & 0xF)
    return ins, col, other, choice


class Stats:
    def __init__(self):
        self.c = collections.Counter()
        self.h = collections.defaultdict(collections.Counter)
        self.vals = collections.defaultdict(list)

    def to_json(self):
        def pct(v, q):
            if not v:
                return None
            v = sorted(v)
            return v[min(len(v) - 1, int(q * len(v)))]
        return {"counts": dict(self.c),
                "hist": {k: dict(sorted(v.items())) for k, v in self.h.items()},
                "dist": {k: {"n": len(v), "mean": round(sum(v) / len(v), 2) if v else None,
                             "p50": pct(v, 0.5), "p90": pct(v, 0.9), "p99": pct(v, 0.99),
                             "max": max(v) if v else None} for k, v in self.vals.items()}}


def load_blocks(root):
    """{block id: {lang or 'base': block bytes}} for the 200 areas and the two pools' four sub-blocks."""
    out = {}
    for path in sorted(glob.glob(os.path.join(root, "AREA*.DAT"))):
        name = os.path.basename(path)
        if name.count(".") != 1:
            continue
        stem = name[:-4]
        d = {"base": chunk(path, 0, 0)}
        for lang in LANGS:
            d[lang] = chunk(os.path.join(root, "%s.%s" % (lang, name)), 0, 0)
        out[stem] = d
    for pool, file in (("SYSPOOL", "FIRST.DAT"), ("BATPOOL", "BATTLE.DAT")):
        raw = {"base": chunk(os.path.join(root, file), 0, POOL_TAG)}
        for lang in LANGS:
            raw[lang] = chunk(os.path.join(root, "%s.%s" % (lang, file)), 0, POOL_TAG)
        for half in (0, 1):
            d = {}
            for k, p in raw.items():
                if p is None:
                    d[k] = None
                    continue
                d0, d1 = struct.unpack_from("<II", p, 0)
                d[k] = p[d0:d1] if half == 0 else p[d1:]
            out["%s.%d" % (pool, half)] = d
    return out


def per_lang(blocks, fonts):
    """Per language: slot census and its own page / row shape."""
    res = {}
    for lang in LANGS:
        st = Stats()
        ja = lang == "ja-JP"
        for bid, d in blocks.items():
            kind = "area" if bid.startswith("AREA") else bid.split(".")[0]
            blk, base = d[lang], d["base"]
            if blk is None:
                st.c[kind + ".block_missing"] += 1
                continue
            slots = block_slots(blk)
            bslots = block_slots(base) if base else [(0, b"")] * len(slots)
            st.c[kind + ".blocks"] += 1
            st.c[kind + ".slots"] += len(slots)
            if base and len(block_slots(base)) != len(slots):
                st.c[kind + ".slotcount_differs_from_pc"] += 1
            seen = set()
            for (o, m), (bo, bm) in zip(slots, bslots):
                if m[:1] == b"\0":
                    st.c[kind + ".slot_empty"] += 1
                    continue
                if is_foreign(m, bm):
                    st.c[kind + ".slot_kept_pc"] += 1
                    continue
                st.c[kind + ".slot_native"] += 1
                if o in seen:
                    continue
                seen.add(o)
                st.c[kind + ".msg_native"] += 1
                toks = tokens(m)
                pages = pages_of(toks)
                st.h[kind + ".pages_per_msg"][len(pages)] += 1
                for p in pages:
                    st.h[kind + ".rows_per_page"][len(p)] += 1
                    for r in p:
                        w = row_width(fonts[lang], ja, r)
                        if w:
                            st.vals[kind + ".row_width"].append(w)
                        if w > W:
                            st.c[kind + ".rows_wider_than_W"] += 1
                            if len(p) <= 3:
                                st.c[kind + ".rows_wider_than_W_in_pages_of_3_or_fewer"] += 1
                                st.h[kind + ".wide_row_cells"][w // 12] += 1
        res[lang] = st.to_json()
    return res


def pair(blocks, fonts, main, second):
    st = Stats()
    mja, sja = main == "ja-JP", second == "ja-JP"
    fm, fs = fonts[main], fonts[second]
    block_extra, block_extra_native, worst = {}, {}, {}
    for bid, d in blocks.items():
        kind = "area" if bid.startswith("AREA") else bid.split(".")[0]
        bm, bs, base = d[main], d[second], d["base"]
        if bm is None or bs is None:
            st.c[kind + ".block_unpaired"] += 1
            continue
        sm, ss = block_slots(bm), block_slots(bs)
        bslots = block_slots(base) if base else [(0, b"")] * len(sm)
        if len(sm) != len(ss):
            st.c[kind + ".slotcount_mismatch"] += 1
        seen = set()
        for slot in range(min(len(sm), len(ss))):
            (om, mm), (os_, ms) = sm[slot], ss[slot]
            bmsg = bslots[slot][1] if slot < len(bslots) else b""
            em, es = mm[:1] == b"\0", ms[:1] == b"\0"
            fm_, fs_ = (not em) and is_foreign(mm, bmsg), (not es) and is_foreign(ms, bmsg)
            if em and es:
                st.c[kind + ".slot_both_empty"] += 1
                continue
            if fm_ or fs_:
                st.c[kind + ".slot_kept_pc_" + ("main" if fm_ and not fs_ else "second" if fs_ and not fm_ else "both")] += 1
                continue
            if em or es:
                st.c[kind + ".slot_empty_" + ("main" if em else "second")] += 1
                continue
            st.c[kind + ".slot_paired"] += 1
            if (om, os_) in seen:
                continue
            seen.add((om, os_))
            st.c[kind + ".msg_paired"] += 1
            tm, ts = tokens(mm), tokens(ms)
            pm, ps = pages_of(tm), pages_of(ts)
            # controls
            im, cm, xm, chm = ctl_profile(tm)
            is_, cs, xs, chs = ctl_profile(ts)
            if im or is_:
                st.c[kind + ".ctl_insert_either"] += 1
                if im != is_:
                    st.c[kind + ".ctl_insert_differs"] += 1
                    if (sum(im.values()) == 0) != (sum(is_.values()) == 0):
                        st.c[kind + ".ctl_insert_one_side_only"] += 1
                for c in set(im) | set(is_):
                    if im[c] != is_[c]:
                        st.c[kind + ".ctl_insert_differs_%02X" % c] += 1
            if chm is not None or chs is not None:
                st.c[kind + ".ctl_choice_either"] += 1
                if (chm is None) != (chs is None):
                    st.c[kind + ".ctl_choice_one_side_only"] += 1
                elif chm != chs:
                    st.c[kind + ".ctl_choice_option_count_differs"] += 1
                else:
                    st.c[kind + ".ctl_choice_both_same_count"] += 1
                    if not pm or not ps:
                        st.c[kind + ".ctl_choice_bare_on_a_side"] += 1
            if cm or cs:
                st.c[kind + ".ctl_colour_either"] += 1
                if cm != cs:
                    st.c[kind + ".ctl_colour_span_count_differs"] += 1
                if (cm == 0) != (cs == 0):
                    st.c[kind + ".ctl_colour_one_side_only"] += 1
            for c in (0x0D, 0x0A, 0x0C, 0x0F, 0x0B, 0x09, 0x10, 0x11, 0x16):
                if xm[c] or xs[c]:
                    st.c[kind + ".ctl_%02X_either" % c] += 1
            if not pm:
                st.c[kind + ".msg_no_main_text"] += 1
                continue
            if not ps:
                st.c[kind + ".msg_no_second_text"] += 1
                continue
            st.h[kind + ".page_count_main_minus_second"][max(-3, min(3, len(pm) - len(ps)))] += 1
            if len(pm) == len(ps):
                st.c[kind + ".msg_page_counts_equal"] += 1
            nonbox = any(len(p) > 3 or any(row_width(fm, mja, r) > W for r in p) for p in pm)
            if nonbox:
                st.c[kind + ".msg_main_nonbox"] += 1
            weights = [glyph_weight(p) for p in pm]
            shares = share(fs, sja, ps, len(pm), weights)
            mtot_units = words(fm, mja, [r for p in pm for r in p])
            stot_units = words(fs, sja, [r for p in ps for r in p])
            mrows_total = wrap(mtot_units)
            # the composed block's growth: the second's glyphs at two bytes
            # (a relocated cell), its spaces at one, three bytes a page of rows
            sglyphs = sum(1 for p in ps for r in p for t in r if t[0] in ("g", "ins"))
            sspaces = sum(1 for p in ps for r in p for t in r if t[0] == "sp")
            st.c[kind + ".composed_extra_bytes"] += 2 * sglyphs + sspaces + 3 * len(pm)
            block_extra[bid] = block_extra.get(bid, 0) + 2 * sglyphs + sspaces + 3 * len(pm)
            snative = sum((2 if t[1] & 0x80 else 1) if t[0] == "g" else 2 if t[0] == "ins" else 1
                          for p in ps for r in p for t in r if t[0] in ("g", "ins", "sp"))
            block_extra_native[bid] = block_extra_native.get(bid, 0) + snative + 3 * len(pm)
            for sname, s in SCALES.items():
                grow_msg = 0
                for p, su in zip(pm, shares):
                    pre = kind + "." + sname + "."
                    sw = sum(u for u, sp in su if not sp) * s
                    srows = wrap(scale_units(su, s))
                    mw = [row_width(fm, mja, r) for r in p]
                    if len(p) > 3 or any(w > W for w in mw):
                        st.c[pre + "nonbox_pages"] += 1
                        if srows > ROWS_ANNOTATED:
                            st.c[pre + "nonbox_second_overflows_2_rows"] += 1
                        continue
                    st.vals[pre + "second_cells_per_page"].append(round(sw / 12, 2))
                    st.h[pre + "second_rows"][min(srows, 5)] += 1
                    st.h[pre + "main_authored_rows"][len(p)] += 1
                    if srows <= ROWS_ANNOTATED:
                        st.c[pre + "V3_three_text_rows_two_reading_fit"] += 1
                    mrows_re = wrap(words(fm, mja, p))
                    authored_ok = len(p) <= ROWS_ANNOTATED
                    if srows <= ROWS_ANNOTATED and authored_ok:
                        cls = "A_as_authored"
                        if srows > len(p):
                            st.c[pre + "A_needs_empty_text_row"] += 1
                    elif srows <= ROWS_ANNOTATED and mrows_re <= ROWS_ANNOTATED:
                        cls = "B_main_rebroken"
                    else:
                        if srows > ROWS_ANNOTATED and mrows_re > ROWS_ANNOTATED:
                            cls = "C_split_both"
                        elif srows > ROWS_ANNOTATED:
                            cls = "C_split_by_second"
                        else:
                            cls = "C_split_by_main_alone"
                    st.c[pre + cls] += 1
                    st.c[pre + "pages"] += 1
                    if srows > ROWS_ANNOTATED:
                        st.c[pre + "second_overflows_2_rows"] += 1
                    need = max(-(-mrows_re // ROWS_ANNOTATED), -(-srows // ROWS_ANNOTATED), 1)
                    grow_msg += need - 1
                    if srows > ROWS_ANNOTATED:
                        g2 = max(-(-srows // ROWS_ANNOTATED), 1) - max(-(-mrows_re // ROWS_ANNOTATED), 1)
                        if g2 > 0:
                            st.c[pre + "pages_added_by_second"] += g2
                st.h[kind + "." + sname + ".pages_added_per_msg"][min(grow_msg, 5)] += 1
                if grow_msg >= 3:
                    worst.setdefault(sname, []).append((grow_msg, bid, slot))
                st.c[kind + "." + sname + ".pages_added"] += grow_msg
                # message-level lower bound: both texts packed freely
                srows_tot = wrap(scale_units(stot_units, s))
                need_tot = max(-(-mrows_total // ROWS_ANNOTATED), -(-srows_tot // ROWS_ANNOTATED), 1)
                st.c[kind + "." + sname + ".pages_free_pack"] += need_tot
                st.c[kind + "." + sname + ".pages_main_authored"] += len(pm)
                # second alone: does it outnumber the main's reading-row room over the whole message?
                if srows_tot > ROWS_ANNOTATED * len(pm):
                    st.c[kind + "." + sname + ".msg_second_exceeds_room_whole_msg"] += 1
            # width ratio, at scale 1, second over main
            mwid = sum(u for u, sp in mtot_units if not sp)
            swid = sum(u for u, sp in stot_units if not sp)
            if mwid:
                st.vals[kind + ".second_over_main_width_ratio"].append(round(swid / mwid, 3))
            st.vals[kind + ".main_glyphs"].append(sum(weights))
    out = st.to_json()
    sizes = []
    for bid, extra in block_extra.items():
        if bid.startswith("AREA") and blocks[bid][main] is not None:
            sizes.append((len(blocks[bid][main]) + extra, bid))
    sizes.sort(reverse=True)
    out["composed_area_block_largest"] = sizes[:3]
    out["composed_area_blocks_over_0x8000"] = sum(1 for s, b in sizes if s > 0x8000)
    out["composed_area_blocks_over_0xFFFF"] = sum(1 for s, b in sizes if s > 0xFFFF)
    nat = sorted(((len(blocks[b][main]) + e, b) for b, e in block_extra_native.items()
                  if b.startswith("AREA") and blocks[b][main] is not None), reverse=True)
    out["composed_native_area_block_largest"] = nat[:3]
    out["composed_native_over_0x8000"] = sorted(b for s, b in nat if s > 0x8000)
    out["composed_native_area_blocks_over_0x8000"] = sum(1 for s, b in nat if s > 0x8000)
    out["main_area_block_largest"] = sorted(((len(blocks[b][main]), b) for b in blocks
                                             if b.startswith("AREA") and blocks[b][main] is not None), reverse=True)[:3]
    out["worst_messages"] = {k: sorted(v, reverse=True)[:5] for k, v in worst.items()}
    return out


def font_report(fonts):
    out = {}
    for lang, f in fonts.items():
        blank = bytes(288)
        nonblank = [g for g in range(f.glyphs) if f.table[g * 288:(g + 1) * 288] != blank]
        out[lang] = {"glyphs": f.glyphs, "advances": len(f.adv), "space": f.space,
                     "last_nonblank": nonblank[-1] if nonblank else None,
                     "nonblank_from_0x993": sum(1 for g in nonblank if g >= JA_FIRST)}
    # pairwise: glyphs >= 0x993 that both tables ink and that differ; and the single-byte slots
    pairs = {}
    for a in LANGS:
        for b in LANGS:
            if a >= b:
                continue
            fa, fb = fonts[a], fonts[b]
            n = min(fa.glyphs, fb.glyphs)
            blank = bytes(288)
            clash = same = 0
            for g in range(JA_FIRST, n):
                ga, gb = fa.table[g * 288:(g + 1) * 288], fb.table[g * 288:(g + 1) * 288]
                if ga != blank and gb != blank:
                    if ga != gb:
                        clash += 1
                    else:
                        same += 1
            low_diff = sum(1 for g in range(0, 0x5A) if fa.table[g * 288:(g + 1) * 288] != fb.table[g * 288:(g + 1) * 288])
            hang = {"0x%02X" % h: fa.table[(h - 0x26) * 288:(h - 0x25) * 288] == fb.table[(h - 0x26) * 288:(h - 0x25) * 288] for h in HANG}
            # the 100 Latin dialogue cells: shape (ink mask) differences, advances
            masks = adv = 0
            for g in range(JA_FIRST, JA_FIRST + 100):
                ia = [((x >> s) & 0xF) != 0 for x in fa.table[g * 288:(g + 1) * 288] for s in (0, 4)]
                ib = [((x >> s) & 0xF) != 0 for x in fb.table[g * 288:(g + 1) * 288] for s in (0, 4)]
                masks += ia != ib
                adv += fa.width(g) != fb.width(g)
            pairs["%s|%s" % (a, b)] = {"inked_in_both_from_0x993": clash + same, "differ": clash,
                                       "single_byte_slots_differ_of_90": low_diff, "hang_slot_same": hang,
                                       "first_100_from_0x993_ink_mask_differs": masks,
                                       "first_100_from_0x993_advance_differs": adv}
    out["pairs"] = pairs
    for lang, f in fonts.items():
        ranges, start = [], None
        blank = bytes(288)
        for g in range(JA_FIRST, f.glyphs + 1):
            ink = g < f.glyphs and f.table[g * 288:(g + 1) * 288] != blank
            if ink and start is None:
                start = g
            if not ink and start is not None:
                ranges.append("0x%X-0x%X" % (start, g - 1))
                start = None
        out[lang]["inked_ranges_from_0x993"] = ranges
    return out


def text_glyph_use(blocks, fonts):
    """Which glyph ranges each language's native area text uses."""
    out = {}
    for lang in LANGS:
        c = collections.Counter()
        for bid, d in blocks.items():
            if not bid.startswith("AREA") or d[lang] is None:
                continue
            slots = block_slots(d[lang])
            base = block_slots(d["base"]) if d["base"] else [(0, b"")] * len(slots)
            for (o, m), (bo, bm) in zip(slots, base):
                if m[:1] == b"\0" or is_foreign(m, bm):
                    continue
                for k, v, g in tokens(m):
                    if k != "g":
                        continue
                    if not v & 0x80:
                        c["single_hang" if v in HANG else "single"] += 1
                    elif g < 0x993:
                        c["two_byte_below_0x993"] += 1
                    elif g < 0xA00:
                        c["0x993-0x9FF"] += 1
                    elif g < 0xA80:
                        c["0xA00-0xA7F"] += 1
                    elif g < 0xD10:
                        c["0xA80-0xD0F"] += 1
                    else:
                        c["0xD10+"] += 1
        out[lang] = dict(c)
    return out


ORDER_PAIRS = [(s, m) for m in LANGS for s in LANGS if s != m]
CLASSES = ("A_as_authored", "B_main_rebroken", "C_split_by_main_alone", "C_split_by_second", "C_split_both")


def cmd_tables(args):
    """The doc's tables, from a run's JSON."""
    r = json.load(open(args.json))
    kind = args.kind
    print("Fit classes, %s, box pages (section 4)" % kind)
    print("| second over main | msgs paired | equal page counts | box pages | scale | A as authored | "
          "B main re-broken | C main alone | C by the second | C both | second > 2 rows | pages added | "
          "of them by the second | 3 + 2 fits |")
    print("|" + "---|" * 14)
    for s, m in ORDER_PAIRS:
        c = r["pairs"]["%s>%s" % (s, m)]["counts"]
        for sc in SCALES:
            g = lambda k: c.get("%s.%s.%s" % (kind, sc, k), 0)
            box = sum(g(k) for k in CLASSES)
            pc = lambda n: "%d (%.1f%%)" % (n, 100.0 * n / box if box else 0)
            head = ("| %s over %s | %d | %d | %d |" % (s, m, c.get(kind + ".msg_paired", 0),
                                                       c.get(kind + ".msg_page_counts_equal", 0), box)
                    if sc == "1/2" else "| | | | |")
            print(head + " %s | %s | %s | %s | %s | %s | %s | %d | %d | %s |" % (
                sc, pc(g("A_as_authored")), pc(g("B_main_rebroken")), pc(g("C_split_by_main_alone")),
                pc(g("C_split_by_second")), pc(g("C_split_both")), pc(g("second_overflows_2_rows")),
                g("pages_added"), g("pages_added_by_second"), pc(g("V3_three_text_rows_two_reading_fit"))))
    if kind != "area":
        return
    print("\nSlots, area (section 2)")
    print("| second over main | slots paired | distinct pairs | both empty | empty main only | empty second only | "
          "kept-PC main | kept-PC second | kept-PC both | second has more pages | fewer | non-box msgs |")
    print("|" + "---|" * 12)
    for s, m in ORDER_PAIRS:
        v = r["pairs"]["%s>%s" % (s, m)]
        c, h = v["counts"], v["hist"]["area.page_count_main_minus_second"]
        g = lambda k: c.get("area." + k, 0)
        print("| %s over %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d |" % (
            s, m, g("slot_paired"), g("msg_paired"), g("slot_both_empty"), g("slot_empty_main"),
            g("slot_empty_second"), g("slot_kept_pc_main"), g("slot_kept_pc_second"), g("slot_kept_pc_both"),
            sum(n for k, n in h.items() if int(k) < 0), sum(n for k, n in h.items() if int(k) > 0),
            g("msg_main_nonbox")))
    print("\nControls, area, per unordered pair (section 6)")
    print("| pair | msgs with an insert | insert counts differ | one side only | of 04 | of 07 | of 03 | "
          "choice either | choice one side | option count differs | colour either | colour one side | "
          "span count differs | 0D offset pen |")
    print("|" + "---|" * 14)
    done = set()
    for s, m in ORDER_PAIRS:
        if (m, s) in done:
            continue
        done.add((s, m))
        c = r["pairs"]["%s>%s" % (s, m)]["counts"]
        g = lambda k: c.get("area." + k, 0)
        print("| %s / %s | %s |" % (s, m, " | ".join(str(g(k)) for k in (
            "ctl_insert_either", "ctl_insert_differs", "ctl_insert_one_side_only", "ctl_insert_differs_04",
            "ctl_insert_differs_07", "ctl_insert_differs_03", "ctl_choice_either", "ctl_choice_one_side_only",
            "ctl_choice_option_count_differs", "ctl_colour_either", "ctl_colour_one_side_only",
            "ctl_colour_span_count_differs", "ctl_0D_either"))))
    print("\nGrowth, area (section 5): messages by pages added 0/1/2/3/4/5+ at 1/2 and 2/3; the second's width "
          "over the main's p50 / p90; the second's cells per box page p50 / p90 / max at 1/2 and 2/3; "
          "composed block over 0x8000 (two-byte / native)")
    print("| second over main | msgs by pages added, 1/2 | at 2/3 | width ratio p50 / p90 | cells a page at 1/2 | "
          "at 2/3 | blocks over 0x8000 |")
    print("|" + "---|" * 7)
    for s, m in ORDER_PAIRS:
        v = r["pairs"]["%s>%s" % (s, m)]
        h, d = v["hist"], v["dist"]
        f = lambda sc: "/".join(str(h["area.%s.pages_added_per_msg" % sc].get(str(i), 0)) for i in range(6))
        q = lambda sc: "%s / %s / %s" % tuple(d["area.%s.second_cells_per_page" % sc][k] for k in ("p50", "p90", "max"))
        rr = d["area.second_over_main_width_ratio"]
        print("| %s over %s | %s | %s | %s / %s | %s | %s | %d / %d |" % (
            s, m, f("1/2"), f("2/3"), rr["p50"], rr["p90"], q("1/2"), q("2/3"),
            v["composed_area_blocks_over_0x8000"], v["composed_native_area_blocks_over_0x8000"]))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("run")
    p.add_argument("--dat", required=True)
    p.add_argument("--out", required=True)
    p = sub.add_parser("tables")
    p.add_argument("json")
    p.add_argument("--kind", default="area")
    args = ap.parse_args()
    if args.cmd == "tables":
        return cmd_tables(args)
    fonts = {lang: Font(os.path.join(args.dat, "%s.FIRST.DAT" % lang)) for lang in LANGS}
    blocks = load_blocks(args.dat)
    res = {"install": args.dat, "W": W, "scales": list(SCALES), "fonts": font_report(fonts),
           "glyph_use": text_glyph_use(blocks, fonts), "langs": per_lang(blocks, fonts), "pairs": {}}
    for m in LANGS:
        for s in LANGS:
            if m != s:
                res["pairs"]["%s>%s" % (s, m)] = pair(blocks, fonts, m, s)
                print("done", s, "over", m, file=sys.stderr)
    with open(args.out, "w") as f:
        json.dump(res, f, indent=1, sort_keys=True)


if __name__ == "__main__":
    main()
