#!/usr/bin/env python
r"""ruby_build.py - the reading-row builder: furigana over the Japanese text layer.

The reading-row round (docs/reading-row-round.md, group FB). Reads the
install's `ja-JP` text layer (DIV-0056) - every kind-0 area script block and
every message pool the layer carries - and writes the optional text layer
`ruby-furigana-ja-JP` (DIV-0086): per annotated block, one kind-18 side
chunk of composed messages keyed by slot, in the shape docs/reading-row.md
section 1 fixes (the contract with the renderer, RR). The text blocks
themselves are never rewritten. docs/reading-row-builder.md is this tool's
own account: the rules, the calls made, the numbers.

    python tools/ruby_build.py furigana --game G --lang ja-JP
    python tools/ruby_build.py furigana --cache C --lang ja-JP [--only AREA014] [--out DIR]
    python tools/ruby_build.py furigana --game G --lang ja-JP --selftest
    python tools/ruby_build.py furigana --game G --lang ja-JP --lint FILE --review FILE --ambiguous FILE

`--game G` reads G\DAT\ja-JP.<NAME>.DAT and writes
G\DAT\ruby-furigana-ja-JP.<NAME>.DAT; `--cache C` reads
C/loc/ja-JP/dat/<NAME>.DAT and writes C/opt/ruby-furigana-ja-JP/dat/<NAME>.DAT.

How a message is rebuilt: it is walked into glyph and control items that
keep their ORIGINAL bytes; nothing is decoded and re-encoded for output. The
glyphs are decoded only to be read (data/furigana/jp-charset.toml), the
readings come from data/furigana/readings.toml first and SudachiPy (mode C)
second, and each reading's kana are the PC's own two-byte codes
(loc_build.ja_encode). `--selftest` joins every message back with no
readings and no re-flow and compares it with the layer's bytes.

`second` (group SB, docs/reading-row-second.md) writes ruby-<second>-<main>:
another held language's same slot re-flowed into the reading rows by the
census's pairing rule (docs/second-language-census.md 9), with a composed
font chunk in the layer's FIRST.DAT so the second's glyphs draw under the
main's table:

    python tools/ruby_build.py second --game G --lang ja-JP --over en-US [--lint FILE] [--json FILE]
    python tools/ruby_build.py second --cache C --lang en-US --over ja-JP [--only AREA014,FIRST] [--out DIR]
    python tools/ruby_build.py second --game G --lang ja-JP --over en-US --selftest

The layer it writes is derived game text: it stays in the install's DAT\ or
the cache (both gitignored), never in the tree (CLAUDE.md rule 1).
"""
import argparse
import collections
import glob
import json
import os
import re
import struct
import sys
import tomllib
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import dat        # noqa: E402
import loc_build  # noqa: E402
import language_tags  # noqa: E402

DATA = os.path.join(ROOT, "data", "furigana")
READINGS_TOML = os.path.join(DATA, "readings.toml")
CHARSET_TOML = os.path.join(DATA, "jp-charset.toml")

# ---------------------------------------------------------------- the contract (docs/reading-row.md 1)

SIDE_KIND = 18
# The side chunk's tag names the block it annotates: the kind-0 chunk that
# carries the area text in every held layer is tag 0 (the arena's first
# bytes, 0x803580 + 0 = what Msg_OpenScript indexes), so the side chunk is
# tag 0 too (reading-row.md 1.1, corrected 2026-10-22 from a staged 0xC8000,
# which is AreaMap_Header's chunk).
AREA_SIDE_TAG = 0x0
AREA_TEXT_TAG = 0x0           # the kind-0 chunk read: the area's script block
POOL_TAG = 0x4000             # FIRST.DAT's, BATTLE.DAT's (and the other pools') kind-0 chunk
INSERT_MARK = 0x12            # in a ruby row: skip the drawn width of the k-th insert below (furigana)
INSERT_DRAW = 0x13            # in a ruby row: draw the k-th insert below at reading size (a second language)
GAP = 0x09                    # in a ruby row: one half-cell, nothing drawn
LAYER = "ruby-furigana-%s"

# ---------------------------------------------------------------- the PC's text codes (docs/msgbox.md 3)

NL, PAGE, CHOICE, TIMED, HEAD = 0x01, 0x02, 0x14, 0x16, 0x0C
ARG1 = loc_build.ARG1         # two-byte controls, as loc_build.message_end steps them
INSERTS = (0x03, 0x04, 0x07, 0x08)
COLOUR, COLOUR_OFF = 0x05, 0x06
EFFECTS = (0x0D, 0x0E, 0x0F)  # the offset pen and the effect preset (MsgBox_EffectDraw's spans):
                              # composed since 2026-10-22, kept in the text row verbatim (DIV-0155 / 0156)
STOP = 0x00                   # the box's own end: what a timed close becomes in a composed copy
HANG = (0x2A, 0x3C)           # a first byte at the line start hangs 12 units left (0x4979A0)
SPACE = 0x20

# ---------------------------------------------------------------- the box (the layout's bounds)

BOX_CELLS = 16                # second_language_census.W = 192 units: rows wider are narration
BOX_ROWS = 3                  # a page taller is the full-screen narration path
PAIRS = 2                     # row pairs per page (the owner, 2026-10-20)

# ---------------------------------------------------------------- characters

KANJI_RE = re.compile(r"[一-鿿々]")
HIRA_RE = re.compile(r"[ぁ-ゖー]+")
STRETCH = set("ーァィゥェォぁぃぅぇぉ")   # glued to a kanji: dialect stretching (気ィ)
UNKNOWN = "〓"


def kata_to_hira(s):
    return "".join(chr(ord(c) - 0x60) if "ァ" <= c <= "ヶ" else c for c in s)


class Charset:
    """PC glyph index -> character, for the ja-JP layer's codes (loc_build.ja_cell_of
    read backwards), and hiragana -> the PC's bytes for a reading."""

    def __init__(self, path=CHARSET_TOML):
        with open(path, "rb") as f:
            t = tomllib.load(f)
        self.single = {int(k, 16): v for k, v in t["single"].items()}
        self.page15 = {int(k, 16): v for k, v in t["page15"].items()}
        self.kanji = {int(k, 16): v for k, v in t["kanji"].items()}
        self.kana = {}
        for ch, code in loc_build.JA_CODE.items():
            if ch != " ":
                self.kana.setdefault(code, ch)
        self.kana[0xFC] = "ー"
        self.encode_memo = {}

    def char(self, item):
        """The character a glyph item draws, or None when it is not a
        Japanese-layer code (the port's own table: a kept Chinese message)."""
        raw = item[1]
        if len(raw) == 1:
            return {SPACE: " ", 0x2A: self.single.get(0x2A), 0x3C: self.single.get(0x3B)}.get(raw[0])
        g = item[2]
        ja, kanji = loc_build.JA_AT, loc_build.JA_KANJI_AT
        if ja <= g < kanji:
            c = g - ja
            if c < 0x5B:
                return self.single.get(c, UNKNOWN)
            if c < 0x5B + 0x23:
                return self.page15.get(c - 0x5B, UNKNOWN)
            code = c - 0x23
            return self.kana.get(code) or self.single.get(code, UNKNOWN)
        if kanji <= g < kanji + loc_build.JA_CELLS:
            return self.kanji.get(0x1200 + g - kanji, UNKNOWN)
        if g >= kanji + loc_build.JA_CELLS:
            return UNKNOWN            # past the sheets (the shoulders, a pair code): Japanese, unread
        return None

    def encode(self, reading):
        """A hiragana reading -> the PC's bytes, or None when a kana has no code."""
        if reading not in self.encode_memo:
            out = bytearray()
            for ch in reading:
                code = loc_build.JA_CODE.get(ch)
                e = loc_build.ja_encode(code) if code is not None and ch != " " else None
                if e is None or len(e) != 2:      # a ruby glyph is a two-byte code (never a hang byte)
                    out = None
                    break
                out += e
            self.encode_memo[reading] = None if out is None else bytes(out)
        return self.encode_memo[reading]


# ---------------------------------------------------------------- a message as items

class Message:
    """head (a leading 0x0C nn, which MsgBox_Reset eats), pages [(rows, term)],
    tail (the 0x00, or the 0x14 choice block to its end). A row is a list of
    items: ('g', bytes, glyph index) or ('c', bytes). Every byte of the
    message is in exactly one of them."""
    __slots__ = ("head", "pages", "tail")

    def __init__(self, raw):
        i, n = 0, len(raw)
        self.head = b""
        if n >= 2 and raw[0] == HEAD:
            self.head, i = raw[:2], 2
        pages, rows = [], [[]]
        self.tail = b""
        while i < n:
            b = raw[i]
            if b == 0 or b == CHOICE:
                self.tail = raw[i:]
                break
            if b == NL:
                rows.append([])
                i += 1
            elif b == PAGE:
                pages.append((rows, raw[i:i + 1]))
                rows, i = [[]], i + 1
            elif b == TIMED:
                pages.append((rows, raw[i:i + 2]))
                rows, i = [[]], i + 2
            elif b & 0x80:
                g = ((b & 0x7F) << 8) | (raw[i + 1] if i + 1 < n else 0)
                rows[-1].append(("g", raw[i:i + 2], g))
                i += 2
            elif b in ARG1:
                rows[-1].append(("c", raw[i:i + 2]))
                i += 2
            elif b <= 0x16 and b not in (0x12, 0x13, 0x15):
                rows[-1].append(("c", raw[i:i + 1]))
                i += 1
            else:
                rows[-1].append(("g", raw[i:i + 1], b - 0x26))
                i += 1
        pages.append((rows, b""))
        self.pages = pages

    @staticmethod
    def row_bytes(row):
        return b"".join(it[1] for it in row)

    def join(self):
        """The message as it was: no readings, no re-flow."""
        out = bytearray(self.head)
        for rows, term in self.pages:
            out += bytes([NL]).join(self.row_bytes(r) for r in rows) + term
        return bytes(out + self.tail)


def box_extent(raw):
    """The bytes ONE box plays of a slot's message, and whether it closes on a timer.

    `0x16 nn` (MsgBox_StatePrint's state 7, docs/msgbox.md 2) closes the box nn
    frames after it is reached: MsgBox_State7Delay starts window 0's shrink and
    MsgBox_State2Closed reports the box done; nothing reads the bytes after it
    again (the only writers of the walk's base 0x7DEE4C are the two openers,
    MsgBox_Reopen and the page restart). A scene of timed pages is one message
    whose slots start after each `0x16 nn` - AREA024's slots 0..7 - and the script
    opens them one by one (measured 2026-10-22, docs/reading-row.md 3.6).

    Under a reading-row layer the timed close is an explicit stop (the owner,
    2026-10-22): the composed copy is the box's own part, cut at its first
    `0x16 nn`, with the box's own end `0x00` in its place - MsgBox_State2Press,
    the wait every message end has, then the same close. Returns (bytes, timed);
    a message with no `0x16` before its end or its choice block is returned whole."""
    i, n = 0, len(raw)
    while i < n:
        b = raw[i]
        if b == 0 or b == CHOICE:
            break
        if b == TIMED:
            live = raw[:i] + bytes([STOP])
            assert raw.startswith(live[:-1]) and raw[len(live) - 1] == TIMED
            return live, True
        i += 2 if (b & 0x80 or b in ARG1) else 1
    return raw, False


def shows_text(msg):
    """Whether a message draws anything: a glyph other than the space, or an insert.
    A box of a space alone that closes on its timer (`0C 81 20 16 00`, three
    area slots) is a blank beat of a scene, not a page to stop on."""
    return any(it[0] == "g" and it[1] != b" " or it[0] == "c" and it[1][0] in INSERTS
               for rows, _ in msg.pages for r in rows for it in r)


def row_width(row):
    return sum(1 for it in row if it[0] == "g")


def colour_after(row, c):
    for it in row:
        if it[0] == "c" and it[1][0] == COLOUR and len(it[1]) == 2:
            c = it[1][1]
        elif it[0] == "c" and it[1][0] == COLOUR_OFF:
            c = 0
    return c


def colour_code(c):
    return bytes([COLOUR_OFF]) if c == 0 else bytes([COLOUR, c])


# ---------------------------------------------------------------- readings

def load_overrides(path=READINGS_TOML):
    """surface -> [(next set | None, prev tuple | None, reading)], the sibling's
    schema (BreathOfFire3Recomp tools/build_ruby_script.py load_overrides)."""
    with open(path, "rb") as f:
        doc = tomllib.load(f)
    out = {}
    for row in doc.get("reading", []):
        surface, reading = row["surface"], kata_to_hira(row["reading"])
        if not HIRA_RE.fullmatch(reading):
            raise SystemExit("%s: the reading for %s is not kana: %r" % (path, surface, row["reading"]))
        nxt = frozenset(row["next"]) if "next" in row else None
        prv = tuple(row["prev"]) if "prev" in row else None
        rules = out.setdefault(surface, [])
        if any((n, p) == (nxt, prv) for n, p, _ in rules):
            raise SystemExit("%s: %s listed twice for the same context" % (path, surface))
        rules.append((nxt, prv, reading))
    for rules in out.values():
        rules.sort(key=lambda r: r[0] is None and r[1] is None)   # context rules first
    return out


def override_lookup(overrides, surface, next_surface, prev_text):
    for nxt, prv, reading in overrides.get(surface, ()):
        if nxt is not None and next_surface not in nxt:
            continue
        if prv is not None and not any(prev_text.endswith(p) for p in prv):
            continue
        return reading
    return None


def inner_kana(surface):
    """Kana between the first and last kanji (最後の夜): the reading is the whole token's."""
    ks = [i for i, c in enumerate(surface) if KANJI_RE.match(c)]
    return bool(ks) and any(not KANJI_RE.match(c) for c in surface[ks[0]:ks[-1] + 1])


def ruby_for(surface, reading, trim=True):
    """The kana to print over `surface`: the reading with the kana the page
    already shows at either end trimmed off (起きる -> お over 起)."""
    r = kata_to_hira(reading or "")
    if not r or not trim:
        return r or None
    sf = kata_to_hira(surface)
    tail = 0
    while tail < len(sf) and tail < len(r) - 1 and sf[-1 - tail] == r[-1 - tail]:
        if KANJI_RE.match(sf[-1 - tail]):
            break
        tail += 1
    head = 0
    while head < len(sf) and head < len(r) - 1 and sf[head] == r[head]:
        if KANJI_RE.match(sf[head]):
            break
        head += 1
    core = r[head:len(r) - tail] if tail else r[head:]
    return core or r


class Reader:
    """Readings for a run of glyphs: [(first, last, kana bytes, kana text, surface)],
    indices into the run, the stem the reading stands over."""

    def __init__(self, charset, overrides, dict_name="core", ambiguous=None):
        from sudachipy import dictionary as sudachi_dictionary
        from sudachipy import tokenizer as sudachi_tokenizer
        self.charset, self.overrides = charset, overrides
        self.dict_name = dict_name
        self.dictionary = sudachi_dictionary.Dictionary(dict=dict_name)
        self.tok = self.dictionary.create()
        self.mode = sudachi_tokenizer.Tokenizer.SplitMode.C
        self.stats = collections.Counter()
        self.override_hits = collections.Counter()
        self.ambiguous = ambiguous
        self.memo = {}
        self._lexicon = {}

    def lexicon_readings(self, surface):
        if surface not in self._lexicon:
            seen = []
            for m in self.dictionary.lookup(surface):
                r = kata_to_hira(m.reading_form())
                if r not in seen:
                    seen.append(r)
            self._lexicon[surface] = seen
        return self._lexicon[surface]

    def override_for(self, surface, base, next_surface, prev_text):
        """The sidecar's reading: by the surface, or by the dictionary form with
        its kana tail swapped for the conjugated surface's (言う covers 言っ)."""
        r = override_lookup(self.overrides, surface, next_surface, prev_text)
        if r is not None:
            return r
        r = override_lookup(self.overrides, base, next_surface, prev_text)
        if r is None or base == surface:
            return r
        cut = max((k for k, c in enumerate(base) if KANJI_RE.match(c)), default=-1) + 1
        scut = max((k for k, c in enumerate(surface) if KANJI_RE.match(c)), default=-1) + 1
        if not cut or base[:cut] != surface[:scut]:
            return None
        base_tail = base[cut:]
        if base_tail and not r.endswith(base_tail):
            return None
        stem = r[:len(r) - len(base_tail)] if base_tail else r
        return stem + surface[scut:]

    def run(self, chars):
        """chars: the run's characters. Memoised by text: the corpus repeats."""
        key = "".join(chars)
        if key in self.memo:
            out, counts, amb = self.memo[key]
            self.stats.update(counts)
            if self.ambiguous is not None:
                for s, r in amb:
                    self.ambiguous.setdefault(s, collections.Counter())[r] += 1
            return out
        counts, amb = collections.Counter(), []
        out = self._run(chars, counts, amb)
        self.memo[key] = (out, counts, amb)
        self.stats.update(counts)
        if self.ambiguous is not None:
            for s, r in amb:
                self.ambiguous.setdefault(s, collections.Counter())[r] += 1
        return out

    def _run(self, chars, counts, amb):
        if not any(KANJI_RE.match(c) for c in chars):
            return []
        # Dialect stretching glued to a kanji (気ィ, 設備ーい) is in no lexicon and
        # breaks the token; tokenise without it and hand it back after the word.
        keep = []
        for k, c in enumerate(chars):
            if k and c in STRETCH and (KANJI_RE.match(chars[k - 1]) or (k - 1) not in keep):
                counts["stretch_glyphs"] += 1
                continue
            keep.append(k)
        text = "".join(chars[k] for k in keep)
        tokens = list(self.tok.tokenize(text, self.mode))
        out, pos = [], 0
        for ti, t in enumerate(tokens):
            surface = t.surface()
            span = keep[pos:pos + len(surface)]
            prev_text = text[:pos]
            pos += len(surface)
            if not span or not KANJI_RE.search(surface):
                continue
            next_surface = tokens[ti + 1].surface() if ti + 1 < len(tokens) else ""
            whole = inner_kana(surface)
            reading = self.override_for(surface, t.dictionary_form(), next_surface, prev_text)
            if reading is not None:
                self.override_hits[surface] += 1
            else:
                reading = t.reading_form()
            ruby = ruby_for(surface, reading, trim=not whole)
            counts["words"] += 1
            if not ruby or not HIRA_RE.fullmatch(ruby):
                counts["unread"] += 1           # out of the lexicon: the reading is the surface
                continue
            enc = self.charset.encode(ruby)
            if enc is None:
                counts["unencodable"] += 1
                continue
            amb.append((surface, ruby))
            if whole:
                first, last = 0, len(surface) - 1
            else:
                ks = [k for k, c in enumerate(surface) if KANJI_RE.match(c)]
                first, last = ks[0], ks[-1]
            out.append((span[first], span[last], enc, ruby, surface))
        return out


# ---------------------------------------------------------------- one ruby row

class RowLayout:
    """The ruby row over one text row (docs/reading-row.md 1.2): half-cells;
    a reading starts at twice its stem's cell, hangs right when wider, takes
    a free half-cell on the left past half a cell of overhang, never touches
    another reading, and stays inside its insert segment."""

    def __init__(self, row):
        self.row = row
        self.x = []          # per item: (segment, cell offset in the segment) or None
        self.seg_cells = []  # per segment: its cells up to the next insert, None for the last
        self.seg_start = [0]  # per segment: its start cell with inserts at zero width (for the wall)
        seg, x, drawn = 0, 0, False
        for it in row:
            if it[0] == "g":
                if not drawn and len(it[1]) == 1 and it[1][0] in HANG:
                    x -= 1
                drawn = True
                self.x.append((seg, x))
                x += 1
            elif it[1][0] in INSERTS:
                self.x.append(None)
                self.seg_cells.append(x)
                self.seg_start.append(self.seg_start[-1] + x)
                seg, x, drawn = seg + 1, 0, True
            else:
                self.x.append(None)
        self.seg_cells.append(None)
        self.segments = seg + 1

    def limit(self, seg):
        """Half-cells a reading of this segment may reach: the next insert, or the box's wall."""
        if self.seg_cells[seg] is not None:
            return 2 * self.seg_cells[seg]
        return 2 * (BOX_CELLS - self.seg_start[seg])

    def place(self, readings, stats):
        """readings: [(first item, last item, kana bytes, colour)] in row order.
        Returns the placements [(segment, start, n, stem start, stem width,
        first item, last item, kana bytes, colour)], half-cells within the segment.

        Each reading wants twice its stem's cell (one half-cell less when it
        overhangs its stem by more than one); it must stand over its stem (at
        least one half-cell shared), keep one free half-cell after the reading
        before it, and stay inside the segment (0 .. the wall). When the one
        before pushes it off its stem, the ones before move left as far as
        their own stems allow; when that is not enough it is dropped, counted
        as no room (rule 4: never silently)."""
        segs = [[] for _ in range(self.segments)]    # per segment: [start, n, lo, ...]
        placed = []
        todo = list(readings)
        while todo:
            first, last, kana, colour = todo.pop(0)
            seg, x0 = self.x[first]
            n = len(kana) // 2
            stem_s, stem_w = 2 * x0, 2 * (self.x[last][1] - x0 + 1)
            lim = self.limit(seg)
            lo, hi = max(0, stem_s - n + 1), min(stem_s + stem_w - 1, lim - n)
            if lo > hi:
                stats["readings_no_room"] += 1
                stats["no_room_wall"] += 1
                continue
            row = segs[seg]
            free = row[-1][0] + row[-1][1] + 1 if row else 0
            want = stem_s - 1 if n - stem_w > 1 and stem_s - 1 >= max(free, 0) else stem_s
            s = max(want, free, lo)
            if s > hi:
                # Pull the readings before it left, each no further than its own stem allows.
                s, starts, need, ok = hi, [r[0] for r in row], hi - 1, True
                for k in range(len(row) - 1, -1, -1):
                    if starts[k] + row[k][1] <= need:
                        break
                    starts[k] = need - row[k][1]
                    if starts[k] < row[k][2]:
                        ok = False
                        break
                    need = starts[k] - 1
                if not ok:
                    # Crowded: when the reading before stands over the cells just
                    # before this stem (in the same colour), the two words take one
                    # reading over both stems (東西: とうざい), which is how
                    # furigana is set over a compound; else it is dropped.
                    prev = placed[-1] if placed and row and row[-1][3] == len(placed) - 1 else None
                    if prev is not None and prev[0] == seg and self.x[prev[6]][1] + 1 == x0 and prev[8] == colour:
                        row.pop()
                        placed.pop()
                        todo.insert(0, (prev[5], last, prev[7] + kana, colour))
                        stats["readings_merged"] += 1
                        continue
                    stats["readings_no_room"] += 1
                    stats["no_room_crowded"] += 1
                    continue
                for k, st in enumerate(starts):
                    if st != row[k][0]:
                        stats["readings_pulled_left"] += 1
                        row[k][0] = st
            row.append([s, n, lo, len(placed)])
            placed.append([seg, s, n, stem_s, stem_w, first, last, kana, colour])
        for seg, row in enumerate(segs):
            for st, n, lo, idx in row:
                placed[idx][1] = st
        return [tuple(p) for p in placed]

    def emit(self, placed, start_colour):
        """The ruby row's bytes: gaps up to each reading; at each insert the gaps
        up to it and the marker; colour codes give a reading its stem's colour
        and the row ends on the colour it began with."""
        if not placed:
            return b""
        out, cur = bytearray(), start_colour
        last_seg = max(p[0] for p in placed)
        for seg in range(last_seg + 1):
            if seg:
                out.append(INSERT_MARK)
            at = 0
            mine = [p for p in placed if p[0] == seg]
            for _, s, n, _, _, _, _, kana, colour in mine:
                out += bytes([GAP]) * (s - at)
                if colour != cur:
                    out += colour_code(colour)
                    cur = colour
                out += kana
                at = s + n
            if seg < last_seg:
                out += bytes([GAP]) * (self.limit(seg) - at)
        if cur != start_colour:
            out += colour_code(start_colour)
        return bytes(out)


# ---------------------------------------------------------------- one message

PLAIN_REASONS = ("not Japanese", "narration (a row past the box)", "narration (a page past three rows)")
# Retired 2026-10-22 (the owner's call, DIV-0155): "own effect or offset pen" (now composed, the codes kept
# in the text row) and "timed page needing a split" (a timed close is now the box's stop, box_extent).


class Composer:
    def __init__(self, charset, reader, lint=None, review=None):
        self.charset, self.reader = charset, reader
        self.stats = collections.Counter()
        self.plain = collections.Counter()
        self.plain_readings = collections.Counter()
        self.lint, self.review = lint, review
        self.review_words = []

    def readings_of_row(self, row, colours):
        """[(first item, last item, kana bytes, colour, kana text, surface)] for one text row."""
        out, run = [], []

        def flush():
            if run:
                chars = [self.charset.char(row[k]) or UNKNOWN for k in run]
                for f, l, enc, ruby, surface in self.reader.run(chars):
                    out.append((run[f], run[l], enc, colours[run[f]], ruby, surface))
                run.clear()

        for k, it in enumerate(row):
            if it[0] == "g":
                run.append(k)
            else:
                flush()
        flush()
        return out

    def plain_reason(self, msg):
        for rows, term in msg.pages:
            for row in rows:
                for it in row:
                    if it[0] == "g" and self.charset.char(it) is None:
                        return 0
        for rows, term in msg.pages:
            if any(row_width(r) > BOX_CELLS for r in rows):
                return 1
            if len(rows) > BOX_ROWS:
                return 2
        return None

    def compose(self, raw, where):
        """The composed message for one slot's bytes, or None when the slot keeps its own.
        A timed close is composed as the box's stop (box_extent): the copy is the
        box's own part, so it is composed even with no reading in it - under a
        layer every timed close waits for a press."""
        raw, timed = box_extent(raw)
        msg = Message(raw)
        reason = self.plain_reason(msg)
        # Every row's readings first: a message with none keeps its own bytes.
        plan, total = [], 0
        for rows, term in msg.pages:
            page_plan, c = [], 0             # the walk's colour is 0 at a page's start (MsgBox_Step)
            for row in rows:
                colours, cc = [], c
                for it in row:
                    if it[0] == "c" and it[1][0] == COLOUR and len(it[1]) == 2:
                        cc = it[1][1]
                    elif it[0] == "c" and it[1][0] == COLOUR_OFF:
                        cc = 0
                    colours.append(cc)
                rd = self.readings_of_row(row, colours) if reason != 0 else []
                total += len(rd)
                page_plan.append((row, c, rd))
                c = colour_after(row, c)
            plan.append((page_plan, term))
        if not total and not (timed and shows_text(msg)):
            return None
        if reason is not None:
            self.plain[PLAIN_REASONS[reason]] += 1
            self.plain_readings[PLAIN_REASONS[reason]] += total
            if timed:
                self.stats["timed_left_plain"] += 1
            return None
        if timed:
            self.stats["timed_stops"] += 1
            if not total:
                self.stats["timed_stops_no_reading"] += 1
        if any(it[0] == "c" and it[1][0] in EFFECTS for rows, _ in msg.pages for r in rows for it in r):
            self.stats["effect_messages"] += 1
        out = bytearray(msg.head)
        placements_all, added_pages = [], 0
        for page_plan, term in plan:
            if len(page_plan) == 1 and not page_plan[0][0]:
                out += term          # an empty page (a message ending on its break): nothing to pair
                continue
            for p0 in range(0, len(page_plan), PAIRS):
                chunk = page_plan[p0:p0 + PAIRS]
                if p0:
                    out.append(PAGE)
                    added_pages += 1
                for ri, (row, c, rd) in enumerate(chunk):
                    if ri:
                        out.append(NL)
                    lay = RowLayout(row)
                    placed = lay.place([(f, l, enc, col) for f, l, enc, col, _, _ in rd], self.stats)
                    # A page split off a longer one starts its walk at colour 0
                    # (MsgBox_Step's local), so it takes back the colour its
                    # first row had in the original.
                    carry = colour_code(c) if p0 and ri == 0 and c else b""
                    ruby = carry + lay.emit(placed, c)
                    out += ruby + bytes([NL]) + Message.row_bytes(row)
                    self.stats["readings_placed"] += len(placed)
                    self.stats["readings_wanted"] += len(rd)
                    self.stats["insert_markers"] += max(p[0] for p in placed) if placed else 0
                    placements_all.append((row, ruby, placed, lay))
                    if self.review is not None:
                        for p in placed:
                            parts = [(s, r) for f, l, _, _, r, s in rd if p[5] <= f and l <= p[6]]
                            self.review_words.append("%s(%s)" % ("".join(s for s, _ in parts),
                                                                 "".join(r for _, r in parts)))
            out += term
        # A message the block ends without its NUL (message_end reads the end as
        # one) is terminated here: in a side chunk the next message follows it.
        out += msg.tail or b"\0"
        if not msg.tail:
            self.stats["terminated"] += 1
        composed = bytes(out)
        self.stats["pages_split"] += sum(1 for pp, _ in plan if len(pp) > PAIRS)
        self.stats["confirm_presses_added"] += added_pages
        self.check(raw, composed, where)
        if self.lint is not None:
            for row, ruby, placed, lay in placements_all:
                self.lint_row(row, ruby, placed, lay, where)
        return composed

    # -- checks ---------------------------------------------------------------
    def check(self, raw, composed, where):
        """The composed message, its ruby rows taken out, is the original's text:
        the same text rows in the same order, the same terminators with only the
        split's 0x02s added, the same head and tail. Raises on a difference."""
        orig, comp = Message(raw), Message(composed)
        if orig.head != comp.head or (orig.tail or b"\0") != comp.tail:
            raise SystemExit("%s: the composed message's head or tail differs" % where)
        o_rows = [r for rows, _ in orig.pages if rows != [[]] for r in rows]
        c_rows, c_terms = [], []
        for rows, term in comp.pages:
            if rows == [[]]:
                c_terms.append(term)
                continue
            if len(rows) % 2 or len(rows) > 2 * PAIRS:
                raise SystemExit("%s: a composed page of %d rows" % (where, len(rows)))
            for k in range(0, len(rows), 2):
                for it in rows[k]:
                    ok = (it[0] == "g" and (len(it[1]) == 2 or it[1] == bytes([INSERT_MARK]))
                          or it[0] == "c" and it[1][0] in (GAP, COLOUR, COLOUR_OFF))
                    if not ok:
                        raise SystemExit("%s: a ruby row holds %s" % (where, it[1].hex()))
                c_rows.append(rows[k + 1])
            c_terms.append(term)
        if [Message.row_bytes(r) for r in o_rows] != [Message.row_bytes(r) for r in c_rows]:
            raise SystemExit("%s: the composed text rows differ from the original's" % where)
        o_terms = [t for _, t in orig.pages]
        if [t for t in c_terms if t != bytes([PAGE])] != [t for t in o_terms if t != bytes([PAGE])]:
            raise SystemExit("%s: the composed page terminators differ" % where)
        self.stats["messages_checked"] += 1

    def lint_row(self, row, ruby, placed, lay, where):
        """Simulate the renderer's x over the ruby row (half-cells: a kana 6
        units, 0x09 one half-cell, colour codes nothing, 0x12 the k-th insert's
        width) and the text row's cells (12 units, the hang, the inserts), and
        report a reading that does not land where it was laid, does not stand
        over its stem, or reaches past its segment's wall."""
        self.stats["lint_rows"] += 1
        # Text: each glyph's (segment, cell), walked again from the bytes.
        tx, seg, x, drawn = {}, 0, 0, False
        for k, it in enumerate(row):
            if it[0] == "g":
                if not drawn and len(it[1]) == 1 and it[1][0] in HANG:
                    x -= 1
                drawn = True
                tx[k] = (seg, x)
                x += 1
            elif it[1][0] in INSERTS:
                seg, x, drawn = seg + 1, 0, True
        # Ruby: each run of kana's (segment, half-cell, length).
        runs, seg, h, i, prev_kana = [], 0, 0, 0, False
        while i < len(ruby):
            b = ruby[i]
            if b == GAP:
                h, i, prev_kana = h + 1, i + 1, False
            elif b == INSERT_MARK:
                seg, h, i, prev_kana = seg + 1, 0, i + 1, False
            elif b == COLOUR:
                i += 2
            elif b == COLOUR_OFF:
                i += 1
            elif b & 0x80:
                if prev_kana:
                    runs[-1][2] += 1
                else:
                    runs.append([seg, h, 1])
                h, i, prev_kana = h + 1, i + 2, True
            else:
                raise SystemExit("%s: lint: byte %02X in a ruby row" % (where, b))
        bad = []
        if len(runs) != len(placed):
            bad.append("%d kana runs for %d readings (two readings touch)" % (len(runs), len(placed)))
        for k, (run, p) in enumerate(zip(runs, placed)):
            pseg, s, n, _, _, first, last, _, _ = p
            sseg, sx = tx[first]
            stem_s, stem_e = 2 * sx, 2 * (tx[last][1] + 1)
            if (run[0], run[1], run[2]) != (pseg, s, n):
                bad.append("reading %d drawn at %d:%d x%d, laid at %d:%d x%d" % (k, run[0], run[1], run[2], pseg, s, n))
            if run[0] != sseg or run[1] >= stem_e or run[1] + run[2] <= stem_s:
                bad.append("reading %d at %d:%d does not stand over its stem %d:%d..%d" % (k, run[0], run[1], sseg, stem_s, stem_e))
            elif abs(run[1] - stem_s) > 1:
                self.stats["lint_off_stem"] += 1
            if run[1] + run[2] > lay.limit(run[0]):
                bad.append("reading %d reaches past its wall" % k)
        if bad:
            self.stats["lint_bad_rows"] += 1
            self.lint.append((where, bad))


# ---------------------------------------------------------------- blocks and files

def block_slots(block):
    """[(slot, offset or None)]: offset None for a slot past the block (an empty message)."""
    n = struct.unpack_from("<H", block, 0)[0] // 2 if len(block) >= 2 else 0
    offs = struct.unpack_from("<%dH" % n, block, 0) if n and 2 * n <= len(block) else ()
    return n, [(i, o if 2 * n <= o < len(block) else None) for i, o in enumerate(offs)]


def side_payload(n, messages):
    """u32 n, u32 off[n], the composed messages (reading-row.md 1.1); identical
    composed messages are stored once."""
    body, at, offs = bytearray(), {}, []
    base = 4 + 4 * n
    for slot in range(n):
        m = messages.get(slot)
        if m is None:
            offs.append(0)
            continue
        if m not in at:
            at[m] = base + len(body)
            body += m
        offs.append(at[m])
    return struct.pack("<I%dI" % n, n, *offs) + bytes(body)


def write_side_dat(path, chunks):
    """chunks: [(tag, half, payload)] -> one overlay of kind-18 chunks, read back by dat.load."""
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        for tag, half, payload in chunks:
            f.write(struct.pack("<4I", SIDE_KIND, tag, len(payload), half))
            f.write(payload)
    dat.load(tmp)
    os.replace(tmp, path)


class Source:
    """Where the ja-JP layer is read and the reading layer written."""

    def __init__(self, args):
        self.lang = args.lang
        self.layer = LAYER % args.lang
        if args.game:
            self.inp = os.path.join(args.game, "DAT")
            self.prefix_in = "%s." % args.lang
            self.out = args.out or self.inp
            self.prefix_out = "%s." % self.layer
        else:
            self.inp = os.path.join(args.cache, "loc", args.lang, "dat")
            self.prefix_in = ""
            self.out = args.out or os.path.join(args.cache, "opt", self.layer, "dat")
            self.prefix_out = ""
        if not os.path.isdir(self.inp):
            raise SystemExit("%s: no %s layer here (loc_build.py builds it)" % (self.inp, args.lang))

    def names(self):
        out = []
        for p in sorted(glob.glob(os.path.join(self.inp, self.prefix_in + "*.DAT"))):
            name = os.path.basename(p)[len(self.prefix_in):]
            if name.count(".") == 1:
                out.append(name)
        return out

    def blocks(self, name):
        """[(label, side tag, half, block bytes)] the file carries."""
        blob, chunks = dat.load(os.path.join(self.inp, self.prefix_in + name))
        out = []
        for c in chunks:
            if c.kind != 0:
                continue
            payload = blob[c.offset:c.offset + c.size]
            if c.tag == AREA_TEXT_TAG:
                out.append(("%s" % name[:-4], AREA_SIDE_TAG, 0, payload))
            elif c.tag == POOL_TAG:
                d0, d1 = struct.unpack_from("<II", payload, 0)
                if not (8 <= d0 <= d1 <= len(payload)):
                    raise SystemExit("%s: the pool's two half offsets 0x%X 0x%X do not split its 0x%X bytes"
                                     % (name, d0, d1, len(payload)))
                out.append(("%s pool.0" % name[:-4], POOL_TAG, 0, payload[d0:d1]))
                out.append(("%s pool.1" % name[:-4], POOL_TAG, 1, payload[d1:]))
        return out

    def out_path(self, name):
        return os.path.join(self.out, self.prefix_out + name)


# ---------------------------------------------------------------- the second-language layer (group SB)
#
# `ruby_build.py second --lang <main> --over <second>` writes the optional text
# layer ruby-<second>-<main>: the same slot of another held language, re-flowed
# into the reading rows by the pairing rule of docs/second-language-census.md 9,
# and a composed font chunk (census 7) so the second's glyphs draw under the
# main's table. docs/reading-row-second.md is this mode's own account.

# The Latin reading glyph's size: one constant until the owner picks from PR's
# captures (reading-row-round.md 6 call 1; 1/2 or 2/3). At 1/2 an 8 x 12 letter
# is drawn 4 x 6 units, texel for texel at the PC's 2x. The renderer (RR) draws
# every glyph of a Latin second's ruby row on a (12 s) x (12 s) quad and moves
# the pen by its kind-4 advance times s.
LATIN_READING_SCALE = Fraction(1, 2)
# A Japanese second's reading glyph: the sheet's 12 px cell on a 6-unit quad,
# texel for texel (reading-row.md 1.2); not the owner's to pick.
JA_READING_SCALE = Fraction(1, 2)
ROW_UNITS = 192               # a row, text or reading: second_language_census.W, 16 cells of 12
SECOND_LAYER = "ruby-%s-%s"   # (second, main)
# Inserts are budgeted at the widest each can print (census 1, the sibling's
# INSERT_WIDTH), in the main's letters: the name RR draws is the main's.
INSERT_GLYPHS = {0x03: 5, 0x04: 5, 0x07: 8, 0x08: 11}
JA_RELOC_AT = 0xAC0           # the Japanese cells under a Latin main: past every Latin table (census 7)
ROUND = 0x10                  # a relocated block starts on a round index
LATIN_CELLS = loc_build.LAST_CODE - loc_build.FIRST_CODE + 1      # the 100 dialogue cells, codes 0x30..0x93
LATIN_SPACE_CELL = 0x64       # under a Japanese main: base + 0x64, a blank cell for the Latin space
LATIN_EXT_CELL = 0x70         # under a Japanese main: base + 0x70 .. the accented block (FR, DE)
EXT_CELLS = loc_build.EXT_MAX - loc_build.EXT_FIRST + 1
GLYPH_BYTES = 288
JA_AT_FIRST = loc_build.APPEND_AT   # the first glyph past the shipped table: where every layer's own cells start
BLANK_GLYPH = bytes(GLYPH_BYTES)
# DT-2's button code (docs/button-dialogue.md 1.1, 4.1): 05 (0xB0 + n) 06 05 cc G.
BUTTON_ARG0 = 0xB0
BUTTON_OF_BIT = {4: 2, 5: 0, 6: 1, 7: 3}      # Japan's bit index n -> circle 0, cross 1, triangle 2, square 3
ICONS = {False: 0x9E3, True: 0x9F6}           # a Latin layer's circle .. square / ja-JP's (button_text.h)
# Kinsoku for a Japanese second's reading rows (units): glyphs that do not start a row, and that do not end one.
NO_START = set("、。，．・：；？！ー」』）】〕…‥ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ〜")
NO_END = set("「『（【〔")

SECOND_PLAIN = (
    # 2 and 3 are retired (2026-10-22, the owner's call, DIV-0156): the offset pen and the effect preset
    # are composed with their codes in the text row, and a timed close is the box's stop (box_extent).
    "narration (a row past the box)", "narration (a page past three rows)", "own effect or offset pen (retired)",
    "timed page needing a split (retired)", "the second has no text", "the main has no text",
    "a glyph of the second with no cell in the composed font",
    "a button code naming no face button",
    "an insert of the second the text row below does not carry",
)


def round_up(n, k=ROUND):
    return -(-n // k) * k


def two_byte(g):
    return bytes([0x80 | (g >> 8), g & 0xFF])


def is_hang(raw):
    return len(raw) == 1 and raw[0] in HANG


class LayerFont:
    """A layer's glyph table and advances: its FIRST.DAT's kind-3 and kind-4 chunks."""

    def __init__(self, path):
        blob, chunks = dat.load(path)
        k3 = [c for c in chunks if c.kind == 3]
        k4 = [c for c in chunks if c.kind == 4]
        if len(k3) != 1 or len(k4) != 1:
            raise SystemExit("%s: %d kind-3 and %d kind-4 chunks, expected one each (a language layer's font)"
                             % (path, len(k3), len(k4)))
        self.table = blob[k3[0].offset:k3[0].offset + k3[0].size]
        self.adv = blob[k4[0].offset:k4[0].offset + k4[0].size]
        self.space = k4[0].tag
        self.count = len(self.table) // GLYPH_BYTES
        if len(self.table) % GLYPH_BYTES or len(self.adv) != self.count:
            raise SystemExit("%s: a table of 0x%X bytes and %d advances" % (path, len(self.table), len(self.adv)))

    def cell(self, g):
        return self.table[g * GLYPH_BYTES:(g + 1) * GLYPH_BYTES]

    def inked(self, g):
        return 0 <= g < self.count and self.cell(g) != BLANK_GLYPH


class FontPlan:
    """The composed font chunk (census 7, the drafted answers of its section 10):
    the main's whole table and advances unchanged, the second's cells added past
    it, and the map from the second's glyph codes to the composed table's.

    - a Latin second over a Japanese main: the 100 dialogue cells (codes
      0x30..0x93) at base .. base + 0x63, a blank space cell at base + 0x64, the
      accented block (French, German) at base + 0x70 ..; base = the main's
      table rounded up to 0x10 (0xE00 over the held ja-JP);
    - a Japanese second over a Latin main: the 882 sheet cells (single sheet,
      kanji sheet) at 0xAC0 + (g - 0x993), a blank space cell after them;
    - Latin over Latin: the main's own letter shapes at the same codes, the
      second's accented cells copied in where the main's table leaves the
      index blank (French or German over English), a blank space cell.
    Every glyph of a ruby row is a code of this table; the hanging bytes 0x2A
    and 0x3C are written two-byte (reading-row.md 1.2)."""

    def __init__(self, main, second, main_ja, second_ja):
        self.main, self.second = main, second
        self.main_ja, self.second_ja = main_ja, second_ja
        self.table = bytearray(main.table)
        self.adv = bytearray(main.adv)
        self.map = {}          # second's glyph index -> composed index
        self.single = {}       # second's single byte -> composed index
        self.keep = not main_ja and not second_ja    # Latin over Latin: the main's codes as they are
        self.blocks = []       # (first, last, what) for the report
        if self.keep:
            self.kind = "Latin over Latin: the main's letter shapes"
            got = []
            for g in range(JA_AT_FIRST, second.count):
                if second.inked(g) and not main.inked(g):
                    self.put(g, second, g)
                    got.append(g)
            if got:
                self.blocks.append((got[0], got[-1], "the second's cells the main leaves blank, at their own codes"))
        elif second_ja:
            self.kind = "Japanese over Latin: the Japanese sheets relocated"
            base = max(JA_RELOC_AT, round_up(main.count))
            n = 2 * loc_build.JA_CELLS
            for k in range(n):
                self.put(base + k, second, loc_build.JA_AT + k)
            for b, code in ((0x2A, 0x2A), (0x3C, 0x3B)):        # loc_build.JA_HANG: the two hangs' cells
                self.single[b] = base + code
            self.blocks.append((base, base + n - 1, "the single sheet and the kanji sheet, g - 0x%X + 0x%X"
                                % (loc_build.JA_AT, base)))
        else:
            self.kind = "Latin over Japanese: the Latin cells relocated"
            base = round_up(main.count)
            for k in range(LATIN_CELLS):
                self.put(base + k, second, loc_build.APPEND_AT + k)
            for code, b in loc_build.ASCII_OF.items():
                self.single[b] = base + code - loc_build.FIRST_CODE
            self.blocks.append((base, base + LATIN_CELLS - 1, "the 100 dialogue cells, codes 0x30..0x93"))
            ext = [k for k in range(EXT_CELLS) if second.inked(loc_build.EXT_AT + k)]
            if ext:
                for k in range(ext[-1] + 1):
                    self.put(base + LATIN_EXT_CELL + k, second, loc_build.EXT_AT + k)
                self.blocks.append((base + LATIN_EXT_CELL, base + LATIN_EXT_CELL + ext[-1],
                                    "the accented cells, codes 0x94..0x%X" % (loc_build.EXT_FIRST + ext[-1])))
            self.space_at = base + LATIN_SPACE_CELL
        if not hasattr(self, "space_at"):
            self.space_at = round_up(len(self.table) // GLYPH_BYTES)
        self.put(self.space_at, None, None, advance=second.space)
        self.blocks.append((self.space_at, self.space_at, "a blank cell: the second's space, advance %d" % second.space))
        self.blocks.sort()
        self.space_code = two_byte(self.space_at)
        self.count = len(self.table) // GLYPH_BYTES
        if self.count - 1 > loc_build.GLYPH_LIMIT:
            raise SystemExit("the composed table holds %d glyphs, past DIV-0016's limit 0x%X"
                             % (self.count, loc_build.GLYPH_LIMIT))

    def put(self, at, src, g, advance=None):
        """Glyph `g` of `src` (None: a blank) at index `at` of the composed table."""
        n = len(self.table) // GLYPH_BYTES
        if self.main.inked(at):
            raise SystemExit("composed font: glyph 0x%X would overwrite the main's ink" % at)
        while n <= at:
            self.table += BLANK_GLYPH
            self.adv.append(loc_build.PC_ADVANCE)
            n += 1
        self.table[at * GLYPH_BYTES:(at + 1) * GLYPH_BYTES] = src.cell(g) if src is not None else BLANK_GLYPH
        self.adv[at] = src.adv[g] if src is not None else advance
        if src is not None:
            self.map[g] = at

    def encode(self, raw, g):
        """A glyph item of the second -> its bytes in a ruby row, or None."""
        if len(raw) == 1:
            b = raw[0]
            if b in self.single:
                return two_byte(self.single[b])
            if self.keep:
                return two_byte(b - 0x26) if b in HANG else raw
            return None
        if g in self.map:
            return two_byte(self.map[g])
        if self.keep and g < self.count:
            return raw
        return None

    def glyph_of(self, code):
        return ((code[0] & 0x7F) << 8) | code[1] if len(code) == 2 else code[0] - 0x26

    def advance(self, code):
        g = self.glyph_of(code)
        return self.adv[g] if 0 <= g < len(self.adv) else loc_build.PC_ADVANCE

    def icon(self, n):
        """The main's own icon for DT-2's button code 0xB0 + n (the main's family, so
        the renderer's ResolveCode reads it as this layer's; button-dialogue.md 4.1)."""
        k = BUTTON_OF_BIT.get(n)
        return None if k is None else two_byte(ICONS[self.main_ja] + k)

    def chunks(self):
        return [(3, 0, 0, bytes(self.table)), (4, self.main.space, 0, bytes(self.adv))]


class Atom:
    """One piece of the second's text in a reading row: a glyph ('g', its ruby-row
    bytes, its colour), an insert ('ins', the control's bytes), or a button code
    ('btn', its bytes, the colour it leaves)."""
    __slots__ = ("kind", "out", "w", "colour", "char")

    def __init__(self, kind, out, w, colour, char=None):
        self.kind, self.out, self.w, self.colour, self.char = kind, out, w, colour, char


class Unit:
    """A break unit: atoms that stay on one row, the width they take, and whether
    a space separates it from the unit before (Latin words; Japanese glyphs)."""
    __slots__ = ("atoms", "w", "gap")

    def __init__(self, atoms, w, gap):
        self.atoms, self.w, self.gap = atoms, w, gap


def share_out(units, weights):
    """The units shared over pages in reading order, in proportion to each page's
    weight (census 9 rule 3; second_language_census.share): a unit goes to the
    page its midpoint falls in."""
    n = len(weights)
    if n == 1:
        return [list(units)]
    total = sum(weights) or 1
    bounds, acc = [], 0
    for w in weights:
        acc += w
        bounds.append(acc / total)
    tw = sum(u.w for u in units) or 1
    out = [[] for _ in range(n)]
    acc, pi = 0, 0
    for u in units:
        mid = (acc + u.w / 2) / tw
        while pi < n - 1 and mid > bounds[pi]:
            pi += 1
        out[pi].append(u)
        acc += u.w
    return out


class SecondComposer:
    """One ordered pair: compose a main message and the second's message of the
    same slot into the reading-row shape of reading-row.md 1.2."""

    def __init__(self, plan, main_ja, second_ja, main_charset, second_charset, lint=None):
        self.plan = plan
        self.main_ja, self.second_ja = main_ja, second_ja
        self.mcs, self.scs = main_charset, second_charset
        s = JA_READING_SCALE if second_ja else LATIN_READING_SCALE
        self.scale = s
        self.num, self.limit = s.numerator, ROW_UNITS * s.denominator   # reading widths in units x denominator
        self.space_w = plan.second.space * self.num
        self.letter = 12 if main_ja else 8          # the main's letter: what RR draws for an insert's name
        self.madv, self.mspace = plan.main.adv, plan.main.space
        self.stats = collections.Counter()
        self.plain = collections.Counter()
        self.lint = lint

    # -- the main's rows ------------------------------------------------------
    def mwidth(self, it):
        """A main item's width in units (inserts at their budget)."""
        if it[0] == "g":
            if it[1] == b" ":
                return self.mspace
            g = it[2]
            return self.madv[g] if 0 <= g < len(self.madv) else loc_build.PC_ADVANCE
        if it[1][0] in INSERTS:
            return INSERT_GLYPHS[it[1][0]] * self.letter
        return 0

    def row_units(self, row, inserts=False):
        """A main row's drawn width: a first 0x2A / 0x3C hangs into the margin (msgbox.md 3)."""
        w, first = 0, True
        for it in row:
            if it[0] == "c" and not (inserts and it[1][0] in INSERTS):
                continue
            u = self.mwidth(it)
            if first and it[0] == "g" and is_hang(it[1]):
                u = 0
            first = False
            w += u
        return w

    # -- the second's text ----------------------------------------------------
    def second_atoms(self, raw):
        """The second's prompt as pages of atoms (its page and row breaks kept as
        'pg' / 'br' markers), or (None, reason). Its own controls but colour,
        the button code and the inserts are dropped (census 9 rule 8)."""
        msg = Message(raw)
        pages = []
        for rows, term in msg.pages:
            atoms, colour = [], 0                 # MsgBox_Step's colour starts at 0 on each page
            for ri, row in enumerate(rows):
                if ri:
                    atoms.append("br")
                k = 0
                while k < len(row):
                    it = row[k]
                    if it[0] == "g":
                        if it[1] == b" ":
                            atoms.append("sp")
                        else:
                            code = self.plan.encode(it[1], it[2])
                            if code is None:
                                return None, 6
                            ch = self.scs.char(it) if self.scs is not None else None
                            atoms.append(Atom("g", code, self.plan.advance(code) * self.num, colour, ch))
                    else:
                        b = it[1][0]
                        if b == COLOUR and len(it[1]) == 2 and BUTTON_ARG0 <= it[1][1] <= BUTTON_ARG0 + 7:
                            # 05 Bn 06 05 cc G: one atom, the icon the main's own (button-dialogue.md 4.1)
                            nxt = row[k + 1:k + 4]
                            if (len(nxt) == 3 and nxt[0][0] == "c" and nxt[0][1] == bytes([COLOUR_OFF])
                                    and nxt[1][0] == "c" and nxt[1][1][0] == COLOUR and len(nxt[1][1]) == 2
                                    and nxt[2][0] == "g"):
                                icon = self.plan.icon(it[1][1] - BUTTON_ARG0)
                                if icon is None:
                                    return None, 7
                                cc = nxt[1][1][1]
                                atoms.append(Atom("btn", it[1] + nxt[0][1] + nxt[1][1] + icon,
                                                  self.plan.advance(icon) * self.num, cc))
                                colour = cc
                                k += 4
                                continue
                            colour = it[1][1]
                        elif b == COLOUR and len(it[1]) == 2:
                            colour = it[1][1]
                        elif b == COLOUR_OFF:
                            colour = 0
                        elif b in INSERTS:
                            atoms.append(Atom("ins", it[1], INSERT_GLYPHS[b] * self.letter * self.num, None))
                    k += 1
            if any(isinstance(a, Atom) for a in atoms):
                pages.append(atoms)
        return pages, None

    def units(self, pages):
        """Break units over the second's pages, its page and row breaks dissolved
        (Latin: a break is a space; Japanese: nothing)."""
        out, gap, ja = [], False, self.second_ja
        for pi, atoms in enumerate(pages):
            if pi and not ja:
                gap = True
            for a in atoms:
                if a == "sp":
                    gap = True
                    continue
                if a == "br":
                    gap = gap or not ja
                    continue
                if ja:
                    last = out[-1] if out else None
                    if last is not None and not gap and (
                            (a.char and a.char in NO_START)
                            or (last.atoms[-1].char and last.atoms[-1].char in NO_END)):
                        last.atoms.append(a)
                        last.w += a.w
                    else:
                        out.append(Unit([a], a.w, gap))
                elif out and not gap:
                    out[-1].atoms.append(a)
                    out[-1].w += a.w
                else:
                    out.append(Unit([a], a.w, gap))
                gap = False
        return out

    def insert_rows(self, units, text_rows):
        """{unit index: reading row} for the units that hold an insert: the k-th
        marker of a reading row is the k-th insert of the text row below
        (reading-row.md 1.2), so each insert of the second goes over the first
        row, from the last one used, whose next insert is the same control.
        None when an insert has no such row."""
        T = [self.inserts_of(r[0]) if r else [] for r in text_rows]
        r, p, out = 0, 0, {}
        for k, u in enumerate(units):
            for a in u.atoms:
                if a.kind != "ins":
                    continue
                while r < len(T) and not (p < len(T[r]) and T[r][p] == a.out):
                    r, p = r + 1, 0
                if r == len(T) or out.get(k, r) != r:
                    return None
                out[k] = r
                p += 1
        return out

    def wrap(self, units, text_rows=None):
        """Greedy reading rows: [[(unit, space before)]]; a unit wider than a row
        is cut at a glyph. Given the text rows below, a unit holding an insert is
        put over the row that carries it (insert_rows), breaking early when it
        must; when that cannot be done the plain greedy rows are returned, and
        compose's insert check leaves the message plain."""
        want = self.insert_rows(units, text_rows) if text_rows is not None else None
        if want:
            rows = self.wrap_to(units, want)
            if rows is not None:
                return rows
        return self.wrap_to(units, {})

    def wrap_to(self, units, want):
        rows, cur, x = [], [], 0
        for k, u in enumerate(units):
            t = want.get(k)
            if t is not None:
                if len(rows) > t:
                    return None
                while len(rows) < t:
                    rows.append(cur)
                    cur, x = [], 0
            sw = self.space_w if (u.gap and cur) else 0
            if cur and x + sw + u.w > self.limit:
                if t is not None:
                    return None
                rows.append(cur)
                cur, x, sw = [], 0, 0
            if not cur and u.w > self.limit:
                piece, pw = [], 0
                for a in u.atoms:
                    if piece and pw + a.w > self.limit:
                        rows.append([(Unit(piece, pw, False), False)])
                        piece, pw = [], 0
                        self.stats["second units cut across rows"] += 1
                    piece.append(a)
                    pw += a.w
                u = Unit(piece, pw, False)
            cur.append((u, sw > 0))
            x += sw + u.w
        if cur:
            rows.append(cur)
        return rows

    # -- a page ---------------------------------------------------------------
    def weight(self, rows):
        return sum(1 for items, _ in rows for it in items if it[0] == "g" or it[1][0] in INSERTS)

    def layout(self, rows, units, top=True):
        """A main page (its rows, [(items, start colour)]) and its share of the
        second -> sub-pages of pairs [(reading row | None, (items, start colour) | None)]
        and the page's class. Census 9 rules 4 and 5 as amended 2026-10-22 (the
        owner: pages are re-flowed, rows never): every authored row is kept as it
        is, as the furigana mode keeps them; a page of one or two rows stays one
        page when the second fits its two reading rows; else it splits on 0x02,
        never the second squeezed: three authored rows as rows 1-2 and 3, two as
        1 and 2, the second re-shared over the parts by their glyphs."""
        free = self.wrap(units)              # the second's own rows, inserts anywhere: its length alone
        if len(rows) <= PAIRS:
            R = self.wrap(units, rows)
            if len(R) <= PAIRS:
                return [self.pairs(rows, self.balance(units, rows, R))], "A"
        over, three = len(free) > PAIRS, len(rows) > PAIRS
        cls = "C both" if over and three else "C by the second" if over else (
            "C main alone" if three else "C by the inserts")
        if top and over:
            self.stats["second needs a third reading row"] += 1
        if len(rows) == 1:
            R = self.wrap(units, rows)
            pages = []
            for i in range(0, len(R), PAIRS):
                part = R[i:i + PAIRS]
                pages.append(self.pairs(rows if i == 0 else [], part))
            return pages, cls
        groups = [rows[:PAIRS], rows[PAIRS:]] if len(rows) > PAIRS else [rows[:1], rows[1:]]
        shares = self.share_split(units, groups)
        out = []
        for g, u in zip(groups, shares):
            sub, _ = self.layout(g, u, False)
            out += sub
        return out, cls

    def inserts_fit(self, units, rows):
        """Every insert of the second's units has a row of these text rows to stand
        over (insert_rows), or there is none."""
        if not any(a.kind == "ins" for u in units for a in u.atoms):
            return True
        return self.insert_rows(units, rows) is not None

    def share_split(self, units, groups):
        """The second shared over the two parts of a split page by their glyphs
        (share_out); when that puts an insert of the second over a part whose text
        rows do not carry it, the boundary moves to the nearest unit where each
        part's inserts are its own rows' (NR, 2026-10-22), else the share stands
        and compose's insert check leaves the message plain."""
        shares = share_out(units, [self.weight(g) for g in groups])
        if all(self.inserts_fit(u, g) for u, g in zip(shares, groups)):
            return shares
        k0 = len(shares[0])
        for d in range(1, len(units) + 1):
            for k in (k0 - d, k0 + d):
                if 0 <= k <= len(units):
                    parts = [units[:k], units[k:]]
                    if all(self.inserts_fit(u, g) for u, g in zip(parts, groups)):
                        self.stats["splits whose share moved to its inserts' rows"] += 1
                        return parts
        return shares

    def balance(self, units, rows, R):
        """The second over a two-row page: shared over the two text rows by their
        glyphs (reading order, word boundaries) when each part fits its one
        reading row and its inserts are the row's, so each reading row stands
        over the text it reads with; else the greedy rows R (census 9 rule 4)."""
        if len(rows) != PAIRS or not R:
            return R
        parts = share_out(units, [self.weight([r]) for r in rows])
        out = []
        for part, row in zip(parts, rows):
            w = self.wrap(part, [row])
            if len(w) > 1:
                return R
            have = [a.out for u, _ in (w[0] if w else []) for a in u.atoms if a.kind == "ins"]
            if have != self.inserts_of(row[0])[:len(have)]:
                return R
            out.append(w[0] if w else [])
        self.stats["pages balanced over their two text rows"] += 1
        return out

    @staticmethod
    def pairs(rows, R):
        n = max(len(rows), len(R), 1)
        return [(R[i] if i < len(R) else None, rows[i] if i < len(rows) else None) for i in range(n)]

    # -- bytes ----------------------------------------------------------------
    def ruby_bytes(self, rrow, walk, want):
        """The reading row's bytes: the second's glyphs in its own colours, a space
        cell between units, 0x12 for an insert; it starts on the walk's colour
        `walk` and ends on `want`, the colour the text row below starts in."""
        out, cur = bytearray(), walk
        if rrow:
            for u, sp in rrow:
                if sp:
                    out += self.plan.space_code
                for a in u.atoms:
                    if a.kind == "g":
                        if a.colour != cur:
                            out += colour_code(a.colour)
                            cur = a.colour
                        out += a.out
                    elif a.kind == "ins":
                        out.append(INSERT_DRAW)
                    else:
                        out += a.out
                        cur = a.colour
        if cur != want:
            out += colour_code(want)
        return bytes(out)

    @staticmethod
    def inserts_of(items):
        return [it[1] for it in items if it[0] == "c" and it[1][0] in INSERTS]

    def compose(self, main_raw, second_raw, where):
        """The composed message, or (None, reason index) when the main stays plain.
        The numbers count only messages that end composed."""
        before = collections.Counter(self.stats)
        out = self._compose(main_raw, second_raw, where)
        if out[0] is None:
            self.stats = before
        return out

    def _compose(self, main_raw, second_raw, where):
        # One box's part of each side (box_extent): the main's timed close becomes
        # the box's stop, and the second is cut at its own first timed close, so a
        # scene of timed boxes pairs box for box (the pages after belong to the
        # next slots, which are composed in their own copies).
        main_raw, timed = box_extent(main_raw)
        second_raw, second_timed = box_extent(second_raw)
        msg = Message(main_raw)
        reason = self.plain_reason(msg)
        if reason is not None:
            return None, reason
        content = [k for k, (rows, term) in enumerate(msg.pages)
                   if any(it[0] == "g" or it[1][0] in INSERTS for r in rows for it in r)]
        if timed and not shows_text(msg):
            return None, 5      # a box that is only a timed beat: nothing to read, nothing to stop for
        spages, why = self.second_atoms(second_raw)
        if spages is None:
            return None, why
        if not spages:
            return None, 4
        if not content:
            return None, 5
        # The rows with the colour each starts in on the original walk (0 at a page's start).
        mpages = []
        for rows, term in msg.pages:
            rr, c = [], 0
            for r in rows:
                rr.append((r, c))
                c = colour_after(r, c)
            mpages.append((rr, term))
        if len(spages) == len(content):
            per = {k: self.units([spages[i]]) for i, k in enumerate(content)}
            self.stats["messages paired page by page"] += 1
        else:
            shares = share_out(self.units(spages), [self.weight(mpages[k][0]) for k in content])
            per = dict(zip(content, shares))
            self.stats["messages shared in proportion"] += 1
        plan, classes = [], collections.Counter()
        for k, (rows, term) in enumerate(mpages):
            if len(rows) == 1 and not rows[0][0]:
                plan.append((None, term))
                continue
            sub, cls = self.layout(rows, per.get(k, []))
            for sp in sub:
                for rrow, trow in sp:
                    have = [a.out for u, _ in (rrow or []) for a in u.atoms if a.kind == "ins"]
                    below = self.inserts_of(trow[0]) if trow else []
                    if have != below[:len(have)]:
                        return None, 8
            classes[cls] += 1
            plan.append((sub, term))
        out = bytearray(msg.head)
        added = 0
        for sub, term in plan:
            if sub is None:
                out += term
                continue
            for si, sp in enumerate(sub):
                if si:
                    out.append(PAGE)
                    added += 1
                walk = 0
                for pi, (rrow, trow) in enumerate(sp):
                    if pi:
                        out.append(NL)
                    items, start = trow if trow else ([], walk)
                    out += self.ruby_bytes(rrow, walk, start) + bytes([NL]) + Message.row_bytes(items)
                    walk = colour_after(items, start)
                    if trow is None:
                        self.stats["empty text rows under a reading row"] += 1
                    if not rrow:
                        self.stats["empty reading rows"] += 1
                    else:
                        self.stats["reading rows"] += 1
                        self.stats["insert markers"] += sum(1 for u, _ in rrow for a in u.atoms if a.kind == "ins")
            out += term
        out += msg.tail or b"\0"
        composed = bytes(out)
        for cls, n in classes.items():
            self.stats["box pages: " + cls] += n
        self.stats["box pages"] += sum(classes.values())
        self.stats["pages added"] += added
        if timed:
            self.stats["timed closes made the box's stop"] += 1
            if not second_timed:
                self.stats["timed in the main, not in the second"] += 1
        elif second_timed:
            self.stats["timed in the second, not in the main"] += 1
        if any(it[0] == "c" and it[1][0] in EFFECTS for rows, _ in msg.pages for r in rows for it in r):
            self.stats["messages with the offset pen or an effect preset"] += 1
        self.check(main_raw, composed, where)
        if self.lint is not None:
            self.lint_message(composed, where)
        return composed, None

    def plain_reason(self, msg):
        """Census 9 rule 2, per message (every page of a composed message is pairs,
        reading-row.md 1.2): a row past the box or a page past three rows leaves
        the whole message plain. The offset pen and the effect preset no longer
        do (2026-10-22, DIV-0156): their codes stay in the text row verbatim."""
        for rows, term in msg.pages:
            if any(self.row_units(r) > ROW_UNITS for r in rows):
                return 0
            if len(rows) > BOX_ROWS:
                return 1
        return None

    # -- checks ---------------------------------------------------------------
    def canon(self, rows_groups):
        """The main's text with its row and page breaks normalised: Latin, a break
        is a space and runs of spaces are one; Japanese, a break is nothing. Empty
        rows are dropped (an empty text row under a reading row is the layout's).
        Each glyph carries the colour the walk draws it in."""
        sp = (b" ", None)
        out = []
        for rows in rows_groups:
            toks = []
            for r in rows:
                if not r:
                    continue
                if toks and not self.main_ja:
                    toks.append(sp)
                toks += r
            if not self.main_ja:
                toks = [t for i, t in enumerate(toks) if not (t == sp and i and toks[i - 1] == sp)]
                while toks and toks[-1] == sp:
                    toks.pop()
                while toks and toks[0] == sp:
                    toks.pop(0)
            out.append(toks)
        return out

    @staticmethod
    def walk_colours(rows):
        """Rows of items -> rows of (bytes, the colour a glyph is drawn in | None),
        the walk's colour starting at 0 (a page, MsgBox_Step)."""
        c, out = 0, []
        for r in rows:
            toks = []
            for it in r:
                if it[0] == "c":
                    if it[1][0] == COLOUR and len(it[1]) == 2:
                        c = it[1][1]
                    elif it[1][0] == COLOUR_OFF:
                        c = 0
                    toks.append((it[1], None))
                else:
                    toks.append((it[1], None if it[1] == b" " else c))
            out.append(toks)
        return out

    def check(self, raw, composed, where):
        """The composed message, its reading rows taken out and its breaks
        normalised, is the original's text, every glyph in the colour the
        original drew it in; the same head, tail and non-page terminators; every
        page two pairs at most; a reading row holds only what 1.2 allows. Raises
        on a difference (rule 4: never written wrong)."""
        orig, comp = Message(raw), Message(composed)
        if orig.head != comp.head or (orig.tail or b"\0") != comp.tail:
            raise SystemExit("%s: the composed message's head or tail differs" % where)

        def groups(m, composed_side):
            gs, cur, terms = [], [], []
            for rows, term in m.pages:
                coloured = self.walk_colours(rows)
                if composed_side and rows != [[]]:
                    if len(rows) % 2 or len(rows) > 2 * PAIRS:
                        raise SystemExit("%s: a composed page of %d rows" % (where, len(rows)))
                    for k in range(0, len(rows), 2):
                        for it in rows[k]:
                            ok = (it[0] == "g" and not is_hang(it[1]) and it[1] != b" "
                                  and (len(it[1]) == 2 or it[1][0] > SPACE or it[1] == bytes([INSERT_DRAW]))
                                  or it[0] == "c" and it[1][0] in (GAP, COLOUR, COLOUR_OFF))
                            if not ok:
                                raise SystemExit("%s: a reading row holds %s" % (where, it[1].hex()))
                    coloured = [coloured[k + 1] for k in range(0, len(rows), 2)]
                cur += coloured
                if term != bytes([PAGE]):
                    gs.append(cur)
                    terms.append(term)
                    cur = []
            gs.append(cur)
            return gs, terms

        og, ot = groups(orig, False)
        cg, ct = groups(comp, True)
        if ot != ct:
            raise SystemExit("%s: the composed message's terminators differ" % where)
        if self.canon(og) != self.canon(cg):
            raise SystemExit("%s: the composed text rows differ from the original's" % where)
        # Rows are never re-flowed (the owner, 2026-10-22): the composed text rows,
        # empty ones dropped, are the authored rows one for one, and none is wider
        # than the authored row it stands for, so nothing reaches the frame that the
        # original did not.
        authored = [r for rows, _ in orig.pages for r in rows if r]
        text = [rows[k + 1] for rows, _ in comp.pages if rows != [[]]
                for k in range(0, len(rows), 2) if rows[k + 1]]
        reflowed = sum(1 for a, t in zip(authored, text) if a != t) + abs(len(authored) - len(text))
        self.stats["text rows re-flowed"] += reflowed
        if reflowed:
            raise SystemExit("%s: %d composed text rows are not the authored rows" % (where, reflowed))
        for a, t in zip(authored, text):
            if self.row_units(t, inserts=True) > self.row_units(a, inserts=True):
                raise SystemExit("%s: a composed text row is wider than its authored row" % where)
        self.stats["text rows checked as authored"] += len(text)
        self.stats["messages checked"] += 1

    def lint_message(self, composed, where):
        """Walk every reading row as the renderer will (reading-row.md 1.2): each
        glyph a code of the composed table, inked or the space cell, at its
        advance times the reading scale; 0x12 the next insert of the text row
        below at its budget; the row inside ROW_UNITS; no hanging first byte.
        And every text row inside the box. (The colours are check()'s.)"""
        bad = []
        for rows, term in Message(composed).pages:
            if rows == [[]]:
                continue
            for k in range(0, len(rows), 2):
                ruby, text = rows[k], rows[k + 1]
                self.stats["lint reading rows"] += 1
                x, marks = 0, 0
                below = self.inserts_of(text)
                glyphs = [it for it in ruby if it[0] == "g"]
                if glyphs and is_hang(glyphs[0][1]):
                    bad.append("a reading row starts on a hanging byte")
                for it in ruby:
                    if it[0] == "c":
                        continue
                    if it[1] == bytes([INSERT_DRAW]):
                        if marks >= len(below):
                            bad.append("a marker with no insert below")
                        else:
                            x += INSERT_GLYPHS[below[marks][0]] * self.letter * self.num
                        marks += 1
                        continue
                    g = self.plan.glyph_of(it[1])
                    if not (0 <= g < self.plan.count) or (g != self.plan.space_at and self.plan.table[
                            g * GLYPH_BYTES:(g + 1) * GLYPH_BYTES] == BLANK_GLYPH):
                        bad.append("glyph 0x%X has no ink in the composed table" % g)
                    x += self.plan.advance(it[1]) * self.num
                if x > self.limit:
                    bad.append("a reading row %.1f units wide" % (x / self.scale.denominator))
                if self.row_units(text, inserts=True) > ROW_UNITS:
                    self.stats["lint text rows past the box with inserts at their budget"] += 1
                if self.row_units(text) > ROW_UNITS:
                    bad.append("a text row past the box")
        if bad:
            self.stats["lint bad messages"] += 1
            self.lint.append((where, bad))


class SecondSource:
    """Where the two text layers are read (and the shipped text, for the port's kept
    messages) and the layer is written."""

    def __init__(self, args):
        self.main, self.over = args.lang, args.over
        self.layer = SECOND_LAYER % (args.over, args.lang)
        if args.game:
            d = os.path.join(args.game, "DAT")
            self.dirs = {t: (d, "%s." % t) for t in (args.lang, args.over)}
            self.base = (d, "")
            self.out = args.out or d
            self.prefix_out = "%s." % self.layer
        else:
            self.dirs = {t: (os.path.join(args.cache, "loc", t, "dat"), "") for t in (args.lang, args.over)}
            self.base = (os.path.join(args.cache, "loc", "zh-CN", "dat"), "")
            self.out = args.out or os.path.join(args.cache, "opt", self.layer, "dat")
            self.prefix_out = ""
        for t, (d, p) in self.dirs.items():
            if not os.path.isfile(os.path.join(d, p + "FIRST.DAT")):
                raise SystemExit("%s: no %s layer here (loc_build.py builds it)" % (d, t))

    def path(self, tag, name):
        d, p = self.dirs[tag] if tag != "base" else self.base
        return os.path.join(d, p + name)

    def names(self):
        d, p = self.dirs[self.main]
        out = []
        for f in sorted(glob.glob(os.path.join(d, p + "*.DAT"))):
            name = os.path.basename(f)[len(p):]
            if name.count(".") == 1:
                out.append(name)
        return out

    def blocks(self, tag, name):
        """{(side tag, half): block bytes} of one layer's file (None when it has none)."""
        path = self.path(tag, name)
        if not os.path.isfile(path):
            return None
        blob, chunks = dat.load(path)
        out = {}
        for c in chunks:
            if c.kind != 0:
                continue
            payload = blob[c.offset:c.offset + c.size]
            if c.tag == AREA_TEXT_TAG:
                out[(AREA_SIDE_TAG, 0)] = payload
            elif c.tag == POOL_TAG:
                d0, d1 = struct.unpack_from("<II", payload, 0)
                if not (8 <= d0 <= d1 <= len(payload)):
                    raise SystemExit("%s: the pool's two half offsets 0x%X 0x%X do not split its 0x%X bytes"
                                     % (path, d0, d1, len(payload)))
                out[(POOL_TAG, 0)] = payload[d0:d1]
                out[(POOL_TAG, 1)] = payload[d1:]
        return out

    def out_path(self, name):
        return os.path.join(self.out, self.prefix_out + name)


def slot_messages(block):
    """[message bytes or None (past the block, or empty: a leading NUL)] per slot, and the offsets."""
    n, slots = block_slots(block)
    msgs, offs = [], []
    for slot, off in slots:
        if off is None or block[off] == 0:
            msgs.append(None)
        else:
            msgs.append(block[off:loc_build.message_end(block, off)])
        offs.append(off)
    return n, msgs, offs


def kept_pc(msg, base_msg):
    """The port's own message, kept by a layer (second_language_census.is_foreign):
    equal to the shipped slot, or a two-byte glyph below the appended range."""
    if base_msg is not None and len(msg) > 1 and msg == base_msg:
        return True
    i, n = 0, len(msg)
    while i < n:
        b = msg[i]
        if b & 0x80:
            g = ((b & 0x7F) << 8) | (msg[i + 1] if i + 1 < n else 0)
            if g < loc_build.APPEND_AT:
                return True
            i += 2
        elif b in ARG1 or b == CHOICE:
            i += 2 if b in ARG1 else 4
        else:
            i += 1
    return False


def write_dat(path, chunks):
    """chunks: [(kind, tag, word3, payload)] -> one overlay, read back by dat.load."""
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        for kind, tag, half, payload in chunks:
            f.write(struct.pack("<4I", kind, tag, len(payload), half))
            f.write(payload)
    dat.load(tmp)
    os.replace(tmp, path)


def cmd_second(args):
    tags = language_tags.TAGS
    for t in (args.lang, args.over):
        language_tags.refuse_retired(t, "--lang" if t == args.lang else "--over")
        if t not in tags:
            raise SystemExit("%s is not a language tag the engine plays (%s)" % (t, ", ".join(tags)))
    if args.lang == args.over:
        raise SystemExit("--over names the main language itself")
    src = SecondSource(args)
    names = [n for n in src.names() if not args.only or n[:-4] in args.only]
    if args.only and not names:
        raise SystemExit("--only %s: no such file of %s" % (",".join(args.only), args.lang))
    main_ja, second_ja = args.lang == "ja-JP", args.over == "ja-JP"
    charset = Charset() if (main_ja or second_ja) else None

    if args.selftest:
        # FB's gate for this mode: with no second (empty), every main message
        # comes back byte for byte - and every message of the second walks into
        # items that join back, so nothing of it is lost before it is re-flowed.
        st = collections.Counter()
        for name in names:
            for tag in (args.lang, args.over):
                blocks = src.blocks(tag, name) or {}
                for key, block in blocks.items():
                    st["blocks"] += 1
                    n, msgs, offs = slot_messages(block)
                    seen = set()
                    for slot, (m, off) in enumerate(zip(msgs, offs)):
                        st["slots"] += 1
                        if off is None or off in seen:
                            continue
                        seen.add(off)
                        raw = block[off:loc_build.message_end(block, off)]
                        st["messages"] += 1
                        if Message(raw).join() != raw:
                            st["mismatch"] += 1
                            print("ROUND-TRIP MISMATCH %s %s %s/%d slot %d" % (tag, name, hex(key[0]), key[1], slot))
        print("selftest: %d files of %s and %s, %d blocks, %d slots, %d messages checked, %d mismatches"
              % (len(names), args.lang, args.over, st["blocks"], st["slots"], st["messages"], st["mismatch"]))
        return 1 if st["mismatch"] else 0

    mfont = LayerFont(src.path(args.lang, "FIRST.DAT"))
    sfont = LayerFont(src.path(args.over, "FIRST.DAT"))
    plan = FontPlan(mfont, sfont, main_ja, second_ja)
    comp = SecondComposer(plan, main_ja, second_ja, charset, charset if second_ja else None,
                          lint=[] if args.lint else None)
    os.makedirs(src.out, exist_ok=True)
    slots = collections.Counter()
    totals = collections.Counter()
    written = set()
    longest = (0, "")
    kinds = {"area": collections.Counter(), "pool": collections.Counter()}
    for name in names:
        mblocks = src.blocks(args.lang, name) or {}
        sblocks = src.blocks(args.over, name) or {}
        bblocks = src.blocks("base", name) or {}
        chunks = []
        for key, mblock in mblocks.items():
            totals["blocks"] += 1
            kind = "pool" if key[0] == POOL_TAG else "area"
            before = collections.Counter(comp.stats)
            n, mm, moffs = slot_messages(mblock)
            sblock = sblocks.get(key)
            if sblock is None:
                slots["%s: the second has no such block" % kind] += sum(1 for m in mm if m is not None)
                continue
            sn, sm, soffs = slot_messages(sblock)
            if sn != n:
                totals["blocks whose slot counts differ"] += 1
            bm = slot_messages(bblocks[key])[1] if key in bblocks else []
            composed, memo = {}, {}
            for slot in range(n):
                m = mm[slot]
                s = sm[slot] if slot < sn else None
                b = bm[slot] if slot < len(bm) else None
                slots[kind + ": slots"] += 1
                if m is None:
                    slots[kind + ": main empty (nothing to show)"] += 1
                    continue
                if s is None:
                    slots[kind + ": plain, the second empty"] += 1
                    continue
                km, ks = kept_pc(m, b), kept_pc(s, b)
                if km or ks:
                    slots[kind + ": plain, the port's kept message " + (
                        "on both sides" if km and ks else "in the main" if km else "in the second")] += 1
                    continue
                mk = (moffs[slot], soffs[slot])
                if mk not in memo:
                    memo[mk] = comp.compose(m, s, "%s %s/%d slot %d" % (name[:-4], hex(key[0]), key[1], slot))
                    totals[kind + " distinct pairs"] += 1
                c, why = memo[mk]
                if c is None:
                    slots[kind + ": plain, " + SECOND_PLAIN[why]] += 1
                    continue
                slots[kind + ": paired"] += 1
                composed[slot] = c
                if len(c) > longest[0]:
                    longest = (len(c), "%s slot %d" % (name[:-4], slot))
            kinds[kind].update(comp.stats - before)
            if composed:
                chunks.append((SIDE_KIND, key[0], key[1], side_payload(n, composed)))
        if name == "FIRST.DAT":
            chunks = plan.chunks() + chunks
        if chunks:
            path = src.out_path(name)
            write_dat(path, chunks)
            written.add(os.path.basename(path))
            totals["files"] += 1
            totals["chunks"] += len(chunks)
            totals["bytes"] += os.path.getsize(path)
    if not args.only and not args.out:
        for p in glob.glob(os.path.join(src.out, src.prefix_out + "*.DAT")):
            if os.path.basename(p) not in written:
                os.remove(p)
                totals["stale removed"] += 1

    st = comp.stats
    print("%s: %d files, %d chunks, %d bytes -> %s" % (src.layer, totals["files"], totals["chunks"], totals["bytes"],
                                                      src.out))
    print("composed font: %s; %d glyphs (the main's %d), reading scale %s"
          % (plan.kind, plan.count, mfont.count, comp.scale))
    for a, b, what in plan.blocks:
        print("  0x%X..0x%X  %s" % (a, b, what))
    for kind in ("area", "pool"):
        keys = sorted(k for k in slots if k.startswith(kind + ":"))
        if keys:
            print("%s slots: %s" % (kind, "; ".join("%s %d" % (k[len(kind) + 2:], slots[k]) for k in keys)))
    for kind in ("area", "pool"):
        k = kinds[kind]
        box = k["box pages"]
        if not box:
            continue
        pct = lambda n: "%d (%.1f%%)" % (n, 100.0 * n / box)
        print("%s box pages %d: as authored %s, main re-broken %d (text rows, check()); split by the main alone %s, by the second %s, by "
              "both %s, by the inserts' rows %s; pages added %d; the second needed a third reading row on %s"
              % (kind, box, pct(k["box pages: A"]), k["text rows re-flowed"], pct(k["box pages: C main alone"]),
                 pct(k["box pages: C by the second"]), pct(k["box pages: C both"]), pct(k["box pages: C by the inserts"]),
                 k["pages added"], pct(k["second needs a third reading row"])))
        print("  messages paired page by page %d, shared in proportion %d; reading rows %d (empty %d), empty text "
              "rows under a reading row %d, insert markers %d, second units cut across rows %d"
              % (k["messages paired page by page"], k["messages shared in proportion"], k["reading rows"],
                 k["empty reading rows"], k["empty text rows under a reading row"], k["insert markers"],
                 k["second units cut across rows"]))
        print("  timed closes made the box's stop %d (the second not timed there %d; the second timed, the main "
              "not %d); messages with the offset pen or an effect preset composed %d"
              % (k["timed closes made the box's stop"], k["timed in the main, not in the second"],
                 k["timed in the second, not in the main"], k["messages with the offset pen or an effect preset"]))
    print("%d composed messages checked against their originals (%d text rows, every one an authored row, "
          "%d re-flowed); longest %d bytes (%s)"
          % (st["messages checked"], st["text rows checked as authored"], st["text rows re-flowed"], longest[0],
             longest[1]))
    if args.json:
        with open(args.json, "w") as f:
            json.dump({"layer": src.layer, "font": {"kind": plan.kind, "glyphs": plan.count, "main": mfont.count,
                                                    "blocks": [["0x%X" % a, "0x%X" % b, w] for a, b, w in plan.blocks],
                                                    "scale": str(comp.scale)},
                       "slots": dict(slots), "area": dict(kinds["area"]), "pool": dict(kinds["pool"]),
                       "totals": dict(totals), "longest": list(longest)}, f, indent=1, sort_keys=True)
    if totals["blocks whose slot counts differ"]:
        print("blocks whose slot counts differ: %d" % totals["blocks whose slot counts differ"])
    if totals["stale removed"]:
        print("removed %d stale %s files" % (totals["stale removed"], src.layer))
    if args.lint:
        with open(args.lint, "w", encoding="utf-8") as f:
            f.write("# ruby_build.py second --lint: reading rows the renderer would draw wrong\n\n")
            for where, bad in comp.lint:
                f.write("%s\n  %s\n" % (where, "\n  ".join(bad)))
        print("lint: %d reading rows walked, %d messages with a fault (%s); %d text rows past the box only with "
              "their inserts at the widest budget"
              % (st["lint reading rows"], st["lint bad messages"], args.lint,
                 st["lint text rows past the box with inserts at their budget"]))
        if st["lint bad messages"]:
            return 1
    return 0


# ---------------------------------------------------------------- the command

def cmd_furigana(args):
    if args.lang != "ja-JP":
        raise SystemExit("ruby_build.py furigana reads a Japanese layer; --lang %s is not one" % args.lang)
    src = Source(args)
    names = [n for n in src.names() if not args.only or n[:-4] in args.only]
    if args.only and not names:
        raise SystemExit("--only %s: no such file in %s" % (",".join(args.only), src.inp))
    charset = Charset()

    if args.selftest:
        st = collections.Counter()
        for name in names:
            for label, tag, half, block in src.blocks(name):
                st["blocks"] += 1
                n, slots = block_slots(block)
                seen = set()
                for slot, off in slots:
                    st["slots"] += 1
                    if off is None:
                        st["empty"] += 1
                        continue
                    if off in seen:
                        continue
                    seen.add(off)
                    raw = block[off:loc_build.message_end(block, off)]
                    st["messages"] += 1
                    if Message(raw).join() != raw:
                        st["mismatch"] += 1
                        print("ROUND-TRIP MISMATCH %s slot %d (%d bytes)" % (label, slot, len(raw)))
        print("selftest: %d files, %d blocks, %d slots (%d past their block), %d messages checked, %d mismatches"
              % (len(names), st["blocks"], st["slots"], st["empty"], st["messages"], st["mismatch"]))
        return 1 if st["mismatch"] else 0

    ambiguous = {} if args.ambiguous else None
    reader = Reader(charset, load_overrides(args.readings), args.dict, ambiguous)
    comp = Composer(charset, reader, lint=[] if args.lint else None, review=[] if args.review else None)
    review = open(args.review, "w", encoding="utf-8") if args.review else None
    os.makedirs(src.out, exist_ok=True)
    totals = collections.Counter()
    longest_chunk, longest_msg, longest_block = (0, ""), (0, ""), (0, "")
    written = set()
    kinds = {"area": collections.Counter(), "pool": collections.Counter()}
    distinct = {"area": (set(), set()), "pool": (set(), set())}   # by bytes, across blocks
    for name in names:
        chunks = []
        for label, tag, half, block in src.blocks(name):
            totals["blocks"] += 1
            kind = "pool" if tag == POOL_TAG else "area"
            before = (collections.Counter(comp.stats), collections.Counter(comp.plain),
                      collections.Counter(comp.plain_readings))
            n, slots = block_slots(block)
            composed, by_off = {}, {}
            for slot, off in slots:
                totals["slots"] += 1
                if off is None:
                    continue
                if off not in by_off:
                    raw = block[off:loc_build.message_end(block, off)]
                    totals["messages"] += 1
                    comp.review_words = []
                    m = comp.compose(raw, "%s slot %d" % (label, slot))
                    by_off[off] = m
                    kinds[kind]["messages"] += 1
                    distinct[kind][0].add(raw)
                    if m is not None:
                        distinct[kind][1].add(raw)
                        kinds[kind]["messages_changed"] += 1
                        totals["messages_changed"] += 1
                        if len(m) > longest_msg[0]:
                            longest_msg = (len(m), "%s slot %d" % (label, slot))
                        if review is not None:
                            review.write("== %s slot %d: %d -> %d bytes\n%s\n%s\n\n" % (
                                label, slot, len(raw), len(m), decode(charset, raw), "  ".join(comp.review_words)))
                if by_off[off] is not None:
                    composed[slot] = by_off[off]
                    totals["slots_changed"] += 1
            kinds[kind]["blocks"] += 1
            kinds[kind].update(comp.stats - before[0])
            for r in PLAIN_REASONS:
                kinds[kind]["plain: " + r] += comp.plain[r] - before[1][r]
                kinds[kind]["plain readings: " + r] += comp.plain_readings[r] - before[2][r]
            if composed:
                payload = side_payload(n, composed)
                chunks.append((tag, half, payload))
                if len(payload) > longest_chunk[0]:
                    longest_chunk = (len(payload), label)
                annotated = len(block) + sum(len(m) for m in set(composed.values()))
                if annotated > longest_block[0]:
                    longest_block = (annotated, label)
        if chunks:
            path = src.out_path(name)
            write_side_dat(path, chunks)
            written.add(os.path.basename(path))
            totals["files"] += 1
            totals["chunks"] += len(chunks)
    if not args.only and not args.out:   # a whole build into the layer's own place: a file it did not write is stale
        for p in glob.glob(os.path.join(src.out, src.prefix_out + "*.DAT")):
            if os.path.basename(p) not in written:
                os.remove(p)
                totals["stale_removed"] += 1
    if review is not None:
        review.close()

    st, rs = comp.stats, reader.stats
    print("%s: %d files, %d chunks -> %s" % (src.layer, totals["files"], totals["chunks"], src.out))
    print("blocks %d, slots %d, distinct messages %d; messages changed %d (%d slots); %d composed messages "
          "checked against their originals"
          % (totals["blocks"], totals["slots"], totals["messages"], totals["messages_changed"],
             totals["slots_changed"], st["messages_checked"]))
    print("readings wanted %d: placed %d, merged with the word before into one reading %d, dropped for no "
          "room %d; insert markers %d"
          % (st["readings_wanted"], st["readings_placed"], st["readings_merged"], st["readings_no_room"],
             st["insert_markers"]))
    print("kanji words %d: unread (out of the lexicon) %d, a kana with no code %d; dictionary %s mode C; "
          "readings.toml applied %d times to %d surfaces (%d listed)"
          % (rs["words"], rs["unread"], rs["unencodable"], args.dict, sum(reader.override_hits.values()),
             len(reader.override_hits), len(reader.overrides)))
    print("pages split for a third row %d (confirm presses added %d)"
          % (st["pages_split"], st["confirm_presses_added"]))
    print("timed closes made the box's stop %d (composed for the stop alone, no reading: %d; timed but left plain: "
          "%d); messages with the offset pen or an effect preset composed %d"
          % (st["timed_stops"], st["timed_stops_no_reading"], st["timed_left_plain"], st["effect_messages"]))
    print("messages left plain, with the readings they would have had: %s"
          % (", ".join("%s %d (%d)" % (r, comp.plain[r], comp.plain_readings[r]) for r in PLAIN_REASONS) or "none"))
    print("longest side chunk %d bytes (%s); longest composed message %d bytes (%s); longest block with its "
          "composed messages %d bytes (%s) against the arena's 0x%X for an area block (the side chunk is "
          "outside it, reading-row.md 1.1)"
          % (longest_chunk + longest_msg + longest_block + (loc_build.TEXT_ROOM,)))
    for kind in ("area", "pool"):
        k = kinds[kind]
        print("  %s blocks %d: messages %d (%d distinct by bytes), changed %d (%d distinct); readings placed %d, merged %d, no room %d (wall %d, "
              "crowded %d), pulled left %d; pages split %d; timed stops %d, effect composed %d; plain %s"
              % (kind, k["blocks"], k["messages"], len(distinct[kind][0]), k["messages_changed"],
                 len(distinct[kind][1]), k["readings_placed"], k["readings_merged"],
                 k["readings_no_room"], k["no_room_wall"], k["no_room_crowded"], k["readings_pulled_left"],
                 k["pages_split"], k["timed_stops"], k["effect_messages"], ", ".join("%d (%d)" % (k["plain: " + r], k["plain readings: " + r])
                                             for r in PLAIN_REASONS)))
    if st["terminated"]:
        print("%d composed messages given the NUL their block ends without" % st["terminated"])
    if totals["stale_removed"]:
        print("removed %d stale %s files" % (totals["stale_removed"], src.layer))
    if args.lint:
        with open(args.lint, "w", encoding="utf-8") as f:
            f.write("# ruby_build.py --lint: ruby rows whose readings would not land over their stems\n"
                    "# (segment:half-cell; segment k is after the row's k-th insert)\n\n")
            for where, bad in comp.lint:
                f.write("%s\n  %s\n" % (where, "\n  ".join(bad)))
        print("lint: %d ruby rows simulated, %d with a misplaced reading (%s); %d readings more than a "
              "half-cell right or left of their stem's start (moved by a neighbour or the wall)"
              % (st["lint_rows"], st["lint_bad_rows"], args.lint, st["lint_off_stem"]))
    if args.ambiguous:
        write_ambiguous(args.ambiguous, reader)
    if args.lint and st["lint_bad_rows"]:
        return 1
    return 0


def decode(charset, raw):
    """A message for the review file: text, controls as <hh>."""
    out = []
    for rows, term in Message(raw).pages:
        for k, row in enumerate(rows):
            if k:
                out.append("<01>")
            for it in row:
                out.append((charset.char(it) or "<%s>" % it[1].hex()) if it[0] == "g" else "<%s>" % it[1].hex())
        out.append("<%s>" % term.hex() if term else "")
    return "".join(out)


def write_ambiguous(path, reader):
    rows = []
    for surface, used in reader.ambiguous.items():
        cands = reader.lexicon_readings(surface)
        if len(cands) < 2 and surface not in reader.overrides:
            continue
        rows.append((sum(used.values()), surface, cands, used))
    rows.sort(key=lambda r: (-r[0], r[1]))
    with open(path, "w", encoding="utf-8") as f:
        f.write("# Ambiguous readings: %d surfaces the %s lexicon reads more than one way\n"
                "# (readings placed, surface, the lexicon's candidates | printed reading x count | override)\n"
                "# Choose in data/furigana/readings.toml: [[reading]] surface = \"...\" reading = \"...\"\n\n"
                % (len(rows), reader.dict_name))
        for n, surface, cands, used in rows:
            printed = " ".join("%s x%d" % (r, c) for r, c in used.most_common())
            ov = "  override" if surface in reader.overrides else ""
            f.write("%6d  %s\t%s\t| %s%s\n" % (n, surface, " / ".join(cands), printed, ov))
    print("ambiguity list: %d surfaces -> %s" % (len(rows), path))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("furigana", help="the ruby-furigana-<lang> layer from the install's Japanese text")
    where = f.add_mutually_exclusive_group(required=True)
    where.add_argument("--game", help="the PC install: reads DAT\\<lang>.<NAME>.DAT")
    where.add_argument("--cache", help="an importer cache: reads loc/<lang>/dat/")
    f.add_argument("--lang", required=True, help="the main language's tag (ja-JP)")
    f.add_argument("--only", type=lambda s: s.split(","), help="file stems, e.g. AREA014,FIRST")
    f.add_argument("--out", help="the directory written (default: beside the layer, see the docstring)")
    f.add_argument("--readings", default=READINGS_TOML, metavar="FILE",
                   help="the readings sidecar (default data/furigana/readings.toml; a mod's, tools/mods.py)")
    f.add_argument("--selftest", action="store_true",
                   help="no readings, no re-flow: every message of every block must join back byte for byte")
    f.add_argument("--lint", metavar="FILE", help="simulate the renderer's x over every ruby row; list rows "
                   "whose readings would not land over their stems")
    f.add_argument("--review", metavar="FILE", help="every reading placed, with its message's text")
    f.add_argument("--ambiguous", metavar="FILE", help="every surface the lexicon reads two ways, most frequent first")
    f.add_argument("--dict", choices=["core", "full"], default="core", help="SudachiDict (default core)")
    s = sub.add_parser("second", help="the ruby-<second>-<main> layer: another held language in the reading rows")
    where = s.add_mutually_exclusive_group(required=True)
    where.add_argument("--game", help="the PC install: reads DAT\\<tag>.<NAME>.DAT")
    where.add_argument("--cache", help="an importer cache: reads loc/<tag>/dat/")
    s.add_argument("--lang", required=True, help="the main language's tag: the text played")
    s.add_argument("--over", required=True, help="the second language's tag: the text in the reading rows")
    s.add_argument("--only", type=lambda v: v.split(","), help="file stems, e.g. AREA014,FIRST")
    s.add_argument("--out", help="the directory written (default: beside the layers, see the docstring)")
    s.add_argument("--selftest", action="store_true",
                   help="every message of both layers' blocks must join back byte for byte")
    s.add_argument("--lint", metavar="FILE", help="walk every reading row as the renderer will; list the faults")
    s.add_argument("--json", metavar="FILE", help="the numbers (counts only, no text) as JSON")
    args = ap.parse_args(argv)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    return cmd_second(args) if args.cmd == "second" else cmd_furigana(args)


if __name__ == "__main__":
    sys.exit(main())
