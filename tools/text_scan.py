#!/usr/bin/env python
"""Static scan of BOF3.exe for the exe's own text: every NUL-ended string of
glyph codes in .data (pair codes 0x80.. two bytes a glyph, 0x21..0x7F one, the
controls 0x01/0x05/0x06/0x07 and the space 0x20) that a .data pointer word or
a .text immediate reaches, rendered with the port's font (FIRST.DAT's kind-3
table) into sheets so the Chinese can be read, with a TSV of the addresses
and their referrers. The way to find the strings no route has drawn yet
(docs/village-text-scan.md, 2026-10-10): the runtime text log
(BOF3X_TEXTLOG) sees only what a route draws.

    python tools/text_scan.py analysis/textscan            # the table region 0x640000..0x670000
    python tools/text_scan.py analysis/textscan --all      # every .data address
    python tools/text_scan.py analysis/textscan --all --ledger [--disc CUE ...] [--singles]

Strings of one glyph are left out (a count's suffix, a unit) unless
--singles is given; BOF3X_TEXTLOG finds those on a route.

--ledger joins the scan with what replaces each string
(docs/exe-chinese-remaining.md). For every string: the functions that read it
(symbols.toml, else analysis/pc_funcs.json / pc_hidden.json starts; a .data
referrer is followed back through its pointer table to the code that reads
the table); the src/ files that name its address; and one class -

  cover     a language overlay writes or repoints it: COVER below, kind 15's
            groups read out of src/game/labels.cpp, kind 5's name tables
            (symbols.toml's NameTable_* with name_tables.cpp's strides). Each
            COVER row names a constant its source must still hold.
  left      read and recorded in LEFT: real text nothing replaces, with what
            the discs have for it.
  pointer   not text: the string's bytes and its NUL are a little-endian
            address in the image (a code or data pointer word).
  disc      not text: the bytes equal a PlayStation disc's own bytes at the
            same address in that disc's image of .data (exe_tables.disc_image;
            needs --disc) - Capcom's data, which parses as glyph codes.
  data      not text: read and recorded in NOT_TEXT.
  NEW       none of these: printed, and the run exits 1, so the list cannot go
            stale without someone reading the new string.

It writes <out>/ledger.tsv and prints the counts. --game DIR (default bof3)
is the install holding BOF3.exe and DAT/FIRST.DAT; --analysis DIR (default
analysis) holds pc_funcs.json and pc_hidden.json.

Output is pictures of Capcom's font and bytes of the executable: analysis/
only (gitignored), never committed (CLAUDE.md rule 1). The tables below hold
addresses, keys and English words of ours or the discs', no Chinese.
"""
import json
import os
import re
import struct
import sys
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)


def arg(name, default=None):
    if name in sys.argv:
        return sys.argv[sys.argv.index(name) + 1]
    return default


def args_all(name):
    return [sys.argv[i + 1] for i, a in enumerate(sys.argv) if a == name and i + 1 < len(sys.argv)]


class Image:
    def __init__(self, path):
        exe = open(path, "rb").read()
        pe = struct.unpack_from("<I", exe, 0x3C)[0]
        nsec = struct.unpack_from("<H", exe, pe + 6)[0]
        opt = struct.unpack_from("<H", exe, pe + 20)[0]
        base = struct.unpack_from("<I", exe, pe + 24 + 28)[0]
        secs = {}
        for i in range(nsec):
            o = pe + 24 + opt + 40 * i
            name = exe[o:o + 8].rstrip(b"\0").decode()
            vs, va, rs, ro = struct.unpack_from("<IIII", exe, o + 8)
            secs[name] = (base + va, vs, ro, rs)
        self.text_va, _, tro, trs = secs[".text"]
        self.data_va, dvs, dro, drs = secs[".data"]
        self.data = exe[dro:dro + drs]
        self.text = exe[tro:tro + trs]
        self.data_end = self.data_va + drs
        self.image_end = self.data_va + dvs      # .bss included

    def dword(self, va):
        return struct.unpack_from("<I", self.data, va - self.data_va)[0]

    def is_address(self, v):
        return self.text_va <= v < self.text_va + len(self.text) or self.data_va <= v < self.image_end


def is_pair(b):
    return 0x80 <= b <= 0x8A


def parse(data, off, min_units=2):
    """units from data[off]: list of glyph codes (int) or None if not a string; returns (units, length_bytes)."""
    units = []
    i = off
    pairs = 0
    while i < len(data) and len(units) < 40:
        b = data[i]
        if b == 0:
            break
        if is_pair(b):
            if i + 1 >= len(data):
                return None
            units.append(((b & 0x7F) << 8) | data[i + 1])
            i += 2
            pairs += 1
        elif b == 0x20:
            units.append(-1)
            i += 1
        elif 0x21 <= b <= 0x7F:
            units.append(b - 0x26)
            i += 1
        elif b in (0x01, 0x06):
            units.append(-2)
            i += 1
        elif b in (0x05, 0x07):
            i += 2
            units.append(-2)
        else:
            return None
    if i >= len(data) or data[i] != 0:
        return None
    if pairs == 0 or len(units) < min_units:
        return None
    if len(units) > 24:
        return None
    return units, i - off


def references(img):
    """{target: [("data"|"text", where)]}: .data pointer words and .text imm32 into .data."""
    refs = {}
    d, t = img.data, img.text
    for i in range(0, len(d) - 3, 4):
        v = struct.unpack_from("<I", d, i)[0]
        if img.data_va <= v < img.data_end:
            refs.setdefault(v, []).append(("data", img.data_va + i))
    for i in range(len(t) - 4):
        v = struct.unpack_from("<I", t, i)[0]
        if img.data_va <= v < img.data_end:
            refs.setdefault(v, []).append(("text", img.text_va + i))
    return refs


def scan(img, refs, everything, min_units=2):
    found = []
    for va in sorted(refs):
        if not everything and not (0x640000 <= va < 0x670000):
            continue
        r = parse(img.data, va - img.data_va, min_units)
        if r:
            found.append((va, r[0], r[1], refs[va]))
    return found


def write_sheets(game, found, out):
    import font_pc
    from PIL import Image as PImage, ImageDraw
    table = font_pc.font_chunk(os.path.join(game, "DAT", "FIRST.DAT"))
    nglyph = len(table) // font_pc.GLYPH_BYTES

    def draw_units(units, im, x, y):
        for u in units:
            if u < 0:
                x += 12
                continue
            if 0 <= u < nglyph:
                rows = font_pc.glyph_pixels(table, u)
                for yy in range(24):
                    for xx in range(24):
                        n = rows[yy][xx]
                        if n:
                            c = 255 - n * 16 if n < 8 else 0
                            im.putpixel((x + xx, y + yy), (c, c, c))
            x += 24
        return x

    rows_per = 40
    for s in range(0, len(found), rows_per):
        chunk = found[s:s + rows_per]
        im = PImage.new("RGB", (1100, 26 * len(chunk) + 4), (40, 40, 40))
        dr = ImageDraw.Draw(im)
        for j, (va, units, n, rf) in enumerate(chunk):
            y = 2 + 26 * j
            kinds = ",".join(sorted(set(k for k, _ in rf)))
            dr.text((2, y + 6), f"{va:06X} {kinds:9} n{len(rf)}", fill=(255, 255, 0))
            draw_units(units, im, 230, y)
        im.save(f"{out}/scan{s // rows_per:02d}.png")


# ---------------------------------------------------------------------------
# The ledger: what replaces each string.
#
# Which overlays carry a key (tools/loc_build.py build_all): kinds 7..12, 15
# and 16 are built from a Western PlayStation disc only ("Latin": en-US,
# en-150, fr-FR, de-DE; a Japanese disc leaves them as shipped, a PSP disc
# builds part of kind 15 and no Config); kind 5 and kind 14 from every disc.
LATIN = "Latin"
ALL = "all"

# COVER: the strings a language overlay writes or repoints, other than kind
# 15's groups and kind 5's tables. (lo, hi, key, languages, source, witness):
# every scanned string starting in [lo, hi) is that key's; `witness` must
# appear in `source`, so a table that moved in src/ stops the run.
COVER = [
    (0x6536F8, 0x653808, "kind 7 Config options (DIV-0015)", LATIN, "src/game/config_text.cpp", "0x6536F8"),
    (0x669F0C, 0x669F44, "kind 7 Config row labels (DIV-0015)", LATIN, "src/game/config_text.cpp", "0x669F3C"),
    (0x66A338, 0x66A368, "kind 7 Config controller names (DIV-0015, DIV-0026)", LATIN, "src/game/config_text.cpp", "0x66A338"),
    (0x66A228, 0x66A2D8, "kind 8 menu verbs (DIV-0018)", LATIN, "src/game/menu_verbs.cpp", "0x66A228"),
    (0x669D28, 0x669D60, "kind 9 battle commands (DIV-0019)", LATIN, "src/game/menu_verbs.cpp", "0x669D28"),
    (0x64B390, 0x64B8B0, "kind 10 default names (DIV-0020)", LATIN, "src/game/char_names.cpp", "0x64B390"),
    (0x669CE0, 0x669CE8, "kind 10 the whelp's name slot (DIV-0020)", LATIN, "src/game/char_names.cpp", "0x669CE0"),
    (0x669CD8, 0x669CE0, "kind 11 Manillo (DIV-0020)", LATIN, "src/game/char_names.cpp", "0x669CD8"),
    (0x669D7C, 0x669DE0, "kind 12 battle banner messages (DIV-0052)", LATIN, "src/game/battle_text.cpp", "0x669D7C"),
    (0x669FC0, 0x66A070, "kind 16 fishing lines (DIV-0069)", LATIN, "src/game/fishing_text.cpp", "0x669FC0"),
    (0x66A070, 0x66A088, "kind 16 fishing tabs (DIV-0069)", LATIN, "src/game/fishing_text.cpp", "0x66A070"),
    (0x66A3F0, 0x66A448, "kind 14 F9 pause lines (DIV-0038, the port's own)", ALL, "src/game/pause_text.cpp", "0x66A3F0"),
    (0x66A1F8, 0x66A200, "not drawn under a Latin overlay (DIV-0083)", LATIN, "src/game/rest_2h.cpp", "0x66A1F8"),
]

# LEFT: real text nothing replaces, read one by one
# (docs/exe-chinese-remaining.md section 3). (lo, hi, what it is, what the discs have).
LEFT = [
    (0x653200, 0x653210, "faerie trait record 0's name (CommuSim_AddRecord reads 0x653200 + 20r, 0x455FB3)",
     "US COMMU00.EMI record 0's name; label group 16 writes name r at 0x653214 + 20r, one record late "
     "(loc-build-disc-only.md 5.4, owner-review.md), so this field is never written"),
    (0x669CCC, 0x669CD8, "the empty save slot's word (Menu_DrawSaveSlot)",
     "no word on the US disc: its slot is blank (the staged localisation PR, HANDOFF item 5; D254)"),
    (0x669CE8, 0x669CF0, "the name BattleExtra_EquipOpen copies over record 7's (EquipLeave restores 0x669CE0's)",
     "US BATE.EMI: 'Beyd' at 0x801D86F8, right after 'Whelp' at 0x801D86F0 (code 0x801D0F34 / 0x801D1608)"),
    (0x669D10, 0x669D20, "fight 26's turn counter, a word before and a word after the count (Boss26Fx_DrawCount)",
     "US BOSS025.EMI and BOSS027.EMI: 'turns left' at 0x800C32A8, drawn after the count '%2d' "
     "(code 0x800C30B0 / 0x800C30F0) - one word after, none before"),
    (0x669F44, 0x669F54, "the hi-lo game's three choices behind 0x669F54 (CommuHiLo_DrawChoice, count 2)",
     "US COMMU02.EMI: 'HIGH' (4 bytes, no NUL), 'LOW', 'SET' at 0x801DAC58, before the 72 bytes of sprite "
     "rectangles the PC has at 0x652D04"),
    (0x669F70, 0x669FA4, "the seven member names behind 0x669FA4 (CommuName_MemberEntryOut; the renamer DIV-0075 cut)",
     "no table on the US disc: 'Nina' is in no COMMU*.EMI (only START / STATUS's default records and area text)"),
    (0x66A094, 0x66A098, "a one-glyph word after a count (EffectKind55_DrawCount)",
     "not looked for (effect_2g; no route enters it)"),
    (0x66A098, 0x66A0B0, "the parts menu's title (EffectKind5E_DrawMenu, rest_3e)",
     "not found: no code-referenced string on the US disc names parts or Momo (GAME.EMI's Part A..G are the items); "
     "the reader has no PSX twin"),
    # one glyph each (--singles)
    (0x66A380, 0x66A398, "six element glyphs behind 0x66A398, drawn as 12 x 12 quads by glyph code (Identify_DrawElements)",
     "not read: the PlayStation routine's element draw (MAGIC060.EMI, beside its WEAK / POINT draw at 0x801EF1E0)"),
    (0x66A3B0, 0x66A3C8, "six glyphs behind Magic080_TextPointers 0x66A3C8, drawn as quads by glyph code (Magic080_DrawText)",
     "not read"),
]

# NOT_TEXT: bytes that parse as glyph codes and are not text, read one by one.
# (lo, hi, what they are).
NOT_TEXT = [
    (0x64D158, 0x64D164, "BossGarr_Enter's byte table 01 81 02 82 03 83 .., the shape of the other bosses' "
                         "tables the discs carry (BossBully_Enter's, BossMutant_Enter's: class disc)"),
    (0x65EF0C, 0x65EF18, "EffectKind18Sub59's run of 0x80 bytes just before its code-pointer table "
                         "(0x512B40, 0x512B70, ..): a fade ramp the Fade / DrawRing reads index"),
    (0x675310, 0x675330, "the C runtime's code-page 932 ranges (0x8260..0x8279 the full-width A..Z, then "
                         "0xA6..0xDF and the lead-byte ranges 0x81..0x9F, 0xE0..0xFC), read by the runtime at 0x5C13CE"),
    # one glyph each (--singles)
    (0x62160F, 0x621611, "the bytes 85 00 Area117_SwitchHook reads, inside area 117's data"),
    (0x6542EE, 0x6542F0, "the halfword 0x8080 (two shade bytes) EffectKind28's draws read"),
    (0x65432A, 0x65432C, "the halfword 0x8080 (two shade bytes) EffectKind2A's draws read"),
    (0x654A9C, 0x654A9E, "the shade bytes 80 80 / 80 EffectKind6F's draws read"),
    (0x66B710, 0x66B712, "the word 0x280 (640, the display width) Display_Setup reads"),
    (0x6751D8, 0x6751DA, "a C runtime number (0x80) the runtime at 0x5BFAC1 reads"),
    (0x6751F0, 0x6751F2, "a C runtime number (0x7080) the runtime at 0x5BCA55 / 0x5C040D reads"),
]


def symbols_toml():
    with open(os.path.join(ROOT, "symbols.toml"), "rb") as f:
        return tomllib.load(f)


def starts(analysis, syms):
    s = {e["pc"]: e["name"] for e in syms.get("func", [])}
    for name in ("pc_funcs.json", "pc_hidden.json"):
        p = os.path.join(analysis, name)
        if not os.path.exists(p):
            continue
        d = json.load(open(p))
        rows = d["functions"] if isinstance(d, dict) else d
        for r in rows:
            s.setdefault(r["entry"], "sub_%06X" % r["entry"])
    return sorted(s.items())


def owner(starts_, at):
    lo, hi = 0, len(starts_) - 1
    best = None
    while lo <= hi:
        mid = (lo + hi) // 2
        if starts_[mid][0] <= at:
            best = starts_[mid]
            lo = mid + 1
        else:
            hi = mid - 1
    return best


def readers(img, refs, starts_, kind, where, depth=0):
    """Names of the functions that reach a string through one referrer."""
    if kind == "text":
        o = owner(starts_, where)
        return {o[1] if o else "?"}
    # a word in .data: walk back through the table that holds it (pointer
    # words, or records holding them) to the first word code or another table
    # reaches, at most 64 words, stopping at a code pointer (another table's).
    # A heuristic: a table read only from a base outside it (a negative
    # index) is reported with no reader.
    out = set()
    w = where
    for _ in range(64):
        for k, a in refs.get(w, []):
            if k == "text":
                o = owner(starts_, a)
                out.add((o[1] if o else "?") + " (table 0x%06X)" % w)
            elif depth < 1:
                out |= {r + " (via 0x%06X)" % w for r in readers(img, refs, starts_, k, a, depth + 1)}
        if out:
            return out
        w -= 4
        if w < img.data_va or img.text_va <= img.dword(w) < img.text_va + len(img.text):
            break
    return out or {"(table 0x%06X, no code reference found)" % where}


def label_groups():
    """[(lo, size, key)] from src/game/labels.cpp's group table."""
    src = open(os.path.join(ROOT, "src", "game", "labels.cpp"), encoding="utf-8").read()
    consts = {}
    for m in re.finditer(r"\b(k\w+) = (0x[0-9A-Fa-f]+)", src):
        consts[m.group(1)] = int(m.group(2), 16)

    def value(expr):
        total = 0
        for term in expr.split("+"):
            term = term.strip()
            total += consts[term] if term in consts else int(term, 0)
        return total

    table = src[src.index("constexpr Table kTables[] = {"):]
    table = table[:table.index("\n};")]
    heads = list(re.finditer(r'\{(\d+), "([^"]+)", (\d+), \{', table))
    out = []
    for i, m in enumerate(heads):
        g, name, count = int(m.group(1)), m.group(2), int(m.group(3))
        body = table[m.end():heads[i + 1].start() if i + 1 < len(heads) else len(table)]
        slots = re.findall(r"\{([\w ]+?(?:\s*\+\s*\w+)?), (\d+), (?:0x[0-9A-Fa-f]+|k\w+)", body)
        if len(slots) != count:
            raise SystemExit("labels.cpp: group %d lists %d slots, its count is %d" % (g, len(slots), count))
        for lo, size in slots:
            out.append((value(lo), int(size), "kind 15 group %d %s (DIV-0064)" % (g, name)))
    if len(out) < 60:
        raise SystemExit("labels.cpp: only %d slots read from the group table - its shape changed" % len(out))
    return out


def name_tables(syms):
    """[(first name field, stride, count, key)] - name_tables.cpp's kTables over symbols.toml's addresses."""
    src = open(os.path.join(ROOT, "src", "game", "name_tables.cpp"), encoding="utf-8").read()
    data = {e["name"]: e["pc"] for e in syms.get("data", [])}
    out = []
    for m in re.finditer(r'\{"([^"]+)", bof3::addr::(NameTable_\w+)(?: \+ (\d+))?, (\d+), (\d+)\}', src):
        what, sym, plus, stride, count = m.groups()
        out.append((data[sym] + int(plus or 0), int(stride), int(count), "kind 5 %s names (DIV-0008)" % what))
    if len(out) != 6:
        raise SystemExit("name_tables.cpp: %d tables read, 6 expected - its shape changed" % len(out))
    return out


def src_addresses():
    named = {}
    for dp, _, fs in os.walk(os.path.join(ROOT, "src")):
        for f in fs:
            if not f.endswith((".cpp", ".h")):
                continue
            p = os.path.join(dp, f)
            for m in re.finditer(r"0x([0-9A-Fa-f]{6})\b", open(p, encoding="utf-8", errors="replace").read()):
                named.setdefault(int(m.group(1), 16), set()).add(os.path.relpath(p, ROOT).replace("\\", "/"))
    return named


def check_cover():
    for lo, hi, key, langs, src, witness in COVER:
        text = open(os.path.join(ROOT, src), encoding="utf-8").read()
        if witness not in text:
            raise SystemExit("COVER %s: %s no longer names %s" % (key, src, witness))


def disc_images(cues):
    """[(build id, image, owner)] - each disc's image of .data (exe_tables.disc_image, with the places)."""
    if not cues:
        return []
    import exe_tables
    import psx_disc
    import tables
    _, _, builds = tables.load()
    places = exe_tables.load_places()
    out = []
    for cue in cues:
        d = psx_disc.Disc(cue)
        bid = tables.disc_build(d, builds)["id"]
        img, own, _ = exe_tables.disc_image(d, bid, places.get(bid, []))
        out.append((bid, img, own, exe_tables.DATA_LO))
    return out


def classify(img, va, n, key, discs):
    raw = img.data[va - img.data_va:va - img.data_va + n + 1]
    for lo, hi, what, discs_have in LEFT:
        if lo <= va < hi:
            return "left", what + " | " + discs_have
    if key:
        return "cover", key
    if len(raw) == 4 and img.is_address(struct.unpack("<I", raw)[0]):
        return "pointer", "0x%08X" % struct.unpack("<I", raw)[0]
    for bid, dimg, own, lo in discs:
        o = va - lo
        if 0 <= o and o + n + 1 <= len(dimg) and all(x is not None for x in own[o:o + n + 1]) \
                and bytes(dimg[o:o + n + 1]) == raw:
            return "disc", bid
    for lo, hi, what in NOT_TEXT:
        if lo <= va < hi:
            return "data", what
    return "NEW", ""


def ledger(img, refs, found, out, analysis, cues):
    check_cover()
    syms = symbols_toml()
    starts_ = starts(analysis, syms)
    named = src_addresses()
    labels = label_groups()
    names = name_tables(syms)
    discs = disc_images(cues)
    rows = []
    counts = {}
    named_n = 0
    for va, units, n, rf in found:
        fn = set()
        for k, a in rf:
            fn |= readers(img, refs, starts_, k, a)
        key = ""
        for lo, size, k in labels:
            if lo <= va < lo + size:
                key = k + " [%s]" % LATIN
        for first, stride, count, k in names:
            if not key and first <= va < first + stride * count and (va - first) % stride < 16:
                key = k + " [%s]" % ALL
        for lo, hi, k, langs, _, _ in COVER:
            if not key and lo <= va < hi:
                key = k + " [%s]" % langs
        state, why = classify(img, va, n, key, discs)
        counts[state] = counts.get(state, 0) + 1
        if va in named:
            named_n += 1
        rows.append((va, n, len(units), state, why, ";".join(sorted(fn)),
                     ",".join(sorted(named.get(va, ()))), " ".join("%s:%06X" % r for r in rf[:6])))
    with open(os.path.join(out, "ledger.tsv"), "w", encoding="utf-8") as f:
        f.write("addr\tbytes\tglyphs\tclass\tkey_or_reading\treaders\tsrc_files\trefs\n")
        for r in rows:
            f.write("%06X\t%d\t%d\t%s\t%s\t%s\t%s\t%s\n" % r)
    print("ledger: %d strings, %d at addresses src/ names; %s" % (
        len(rows), named_n, ", ".join("%s %d" % (k, counts.get(k, 0))
                                      for k in ("cover", "left", "pointer", "disc", "data", "NEW"))))
    for r in rows:
        if r[3] in ("left", "NEW"):
            print("%-5s %06X %2d bytes  %s  <- %s" % (r[3], r[0], r[1], r[4][:90], r[5][:80]))
    return counts.get("NEW", 0) == 0


def main():
    pos = [a for i, a in enumerate(sys.argv[1:], 1)
           if not a.startswith("--") and sys.argv[i - 1] not in ("--game", "--analysis", "--disc")]
    out = pos[0]
    game = arg("--game", "bof3")
    os.makedirs(out, exist_ok=True)
    img = Image(os.path.join(game, "BOF3.exe"))
    refs = references(img)
    found = scan(img, refs, "--all" in sys.argv, 1 if "--singles" in sys.argv else 2)
    print("strings", len(found))
    if "--no-sheets" not in sys.argv:
        write_sheets(game, found, out)
    with open(f"{out}/scan.tsv", "w") as f:
        for va, units, n, rf in found:
            o = va - img.data_va
            f.write(f"{va:06X}\t{n}\t{len(units)}\t{' '.join(f'{k}:{a:06X}' for k, a in rf[:6])}\t{img.data[o:o + n].hex(' ')}\n")
    if "--ledger" in sys.argv:
        ok = ledger(img, refs, found, out, arg("--analysis", "analysis"), args_all("--disc"))
        sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
