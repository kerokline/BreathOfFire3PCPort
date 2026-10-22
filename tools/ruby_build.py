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

The layer it writes is derived game text: it stays in the install's DAT\ or
the cache (both gitignored), never in the tree (CLAUDE.md rule 1).
"""
import argparse
import collections
import glob
import os
import re
import struct
import sys
import tomllib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import dat        # noqa: E402
import loc_build  # noqa: E402

DATA = os.path.join(ROOT, "data", "furigana")
READINGS_TOML = os.path.join(DATA, "readings.toml")
CHARSET_TOML = os.path.join(DATA, "jp-charset.toml")

# ---------------------------------------------------------------- the contract (docs/reading-row.md 1)

SIDE_KIND = 18
# The side chunk's tag names the block it annotates. reading-row.md 1.1 fixes
# 0xC8000 for an area's script block; the kind-0 chunk that carries the area
# text in every held layer is tag 0 (the arena's first bytes, 0x803580 +
# 0 = what Msg_OpenScript indexes; 0xC8000 is AreaMap_Header's chunk). The
# contract's number is what RR keys on, so it is the one written; the
# discrepancy is in the hand-back and docs/reading-row-builder.md 6.
AREA_SIDE_TAG = 0xC8000
AREA_TEXT_TAG = 0x0           # the kind-0 chunk read: the area's script block
POOL_TAG = 0x4000             # FIRST.DAT's, BATTLE.DAT's (and the other pools') kind-0 chunk
INSERT_MARK = 0x12            # in a ruby row: the drawn width of the k-th insert below
GAP = 0x09                    # in a ruby row: one half-cell, nothing drawn
LAYER = "ruby-furigana-%s"

# ---------------------------------------------------------------- the PC's text codes (docs/msgbox.md 3)

NL, PAGE, CHOICE, TIMED, HEAD = 0x01, 0x02, 0x14, 0x16, 0x0C
ARG1 = loc_build.ARG1         # two-byte controls, as loc_build.message_end steps them
INSERTS = (0x03, 0x04, 0x07, 0x08)
COLOUR, COLOUR_OFF = 0x05, 0x06
EFFECTS = (0x0D, 0x0E, 0x0F)  # the offset pen and the effect preset (MsgBox_EffectDraw's spans)
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

PLAIN_REASONS = ("not Japanese", "narration (a row past the box)", "narration (a page past three rows)",
                 "own effect or offset pen", "timed page needing a split")


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
            for row in rows:
                if any(it[0] == "c" and it[1][0] in EFFECTS for it in row):
                    return 3
            if term[:1] == bytes([TIMED]) and len(rows) > PAIRS:
                return 4
        return None

    def compose(self, raw, where):
        """The composed message for one slot's bytes, or None when the slot keeps its own."""
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
        if not total:
            return None
        if reason is not None:
            self.plain[PLAIN_REASONS[reason]] += 1
            self.plain_readings[PLAIN_REASONS[reason]] += total
            return None
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
    reader = Reader(charset, load_overrides(), args.dict, ambiguous)
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
    print("messages left plain, with the readings they would have had: %s"
          % (", ".join("%s %d (%d)" % (r, comp.plain[r], comp.plain_readings[r]) for r in PLAIN_REASONS) or "none"))
    print("longest side chunk %d bytes (%s); longest composed message %d bytes (%s); longest block with its "
          "composed messages %d bytes (%s) against the arena's 0x%X for an area block (the side chunk is "
          "outside it, reading-row.md 1.1)"
          % (longest_chunk + longest_msg + longest_block + (loc_build.TEXT_ROOM,)))
    for kind in ("area", "pool"):
        k = kinds[kind]
        print("  %s blocks %d: messages %d (%d distinct by bytes), changed %d (%d distinct); readings placed %d, merged %d, no room %d (wall %d, "
              "crowded %d), pulled left %d; pages split %d; plain %s"
              % (kind, k["blocks"], k["messages"], len(distinct[kind][0]), k["messages_changed"],
                 len(distinct[kind][1]), k["readings_placed"], k["readings_merged"],
                 k["readings_no_room"], k["no_room_wall"], k["no_room_crowded"], k["readings_pulled_left"],
                 k["pages_split"], ", ".join("%d (%d)" % (k["plain: " + r], k["plain readings: " + r])
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
    f.add_argument("--selftest", action="store_true",
                   help="no readings, no re-flow: every message of every block must join back byte for byte")
    f.add_argument("--lint", metavar="FILE", help="simulate the renderer's x over every ruby row; list rows "
                   "whose readings would not land over their stems")
    f.add_argument("--review", metavar="FILE", help="every reading placed, with its message's text")
    f.add_argument("--ambiguous", metavar="FILE", help="every surface the lexicon reads two ways, most frequent first")
    f.add_argument("--dict", choices=["core", "full"], default="core", help="SudachiDict (default core)")
    args = ap.parse_args(argv)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    return cmd_furigana(args)


if __name__ == "__main__":
    sys.exit(main())
