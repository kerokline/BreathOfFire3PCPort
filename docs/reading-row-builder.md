# The furigana builder: `tools/ruby_build.py furigana`

**Status:** BUILT 2026-10-22 (the reading-row round's group FB, branch
`reading-row/fb`; nothing played - the renderer is stage B's RR). Every
number below is from one run over a copy of the owner's held `ja-JP` layer
(`C:\Users\kerok\bof3x-runs\rr_sl\DAT`, copied to `rr_fb`), named in
section 7. The contract it builds to is [`reading-row.md`](reading-row.md)
section 1; the round is [`reading-row-round.md`](reading-row-round.md).

## 1. What it does

`ruby_build.py furigana --game G | --cache C --lang ja-JP` reads the
Japanese text layer (DIV-0056) - the kind-0 area script block (tag `0x0`)
of every `ja-JP.AREA*.DAT` and the two halves of every message pool (kind
0, tag `0x4000`) the layer carries - and writes the optional text layer
`ruby-furigana-ja-JP` (DIV-0086): per block with anything to read, one
**kind-18 side chunk** of composed messages keyed by slot, in an overlay
`G\DAT\ruby-furigana-ja-JP.<NAME>.DAT` (or
`C/opt/ruby-furigana-ja-JP/dat/<NAME>.DAT`; `--out DIR` elsewhere). The text
blocks themselves are never touched. A whole build into the layer's own
place removes the layer's files it did not write; `--only` builds do not.

    python tools/ruby_build.py furigana --game G --lang ja-JP
    python tools/ruby_build.py furigana --game G --lang ja-JP --selftest
    python tools/ruby_build.py furigana --game G --lang ja-JP --lint L --review R --ambiguous A
    python tools/ruby_build.py furigana --cache C --lang ja-JP --only AREA014,FIRST

A message is walked into glyph and control items that keep their original
bytes (`Message`); the glyphs are decoded only to be read
(`data/furigana/jp-charset.toml`); a composed message is the original's own
text rows with a ruby row before each, so **nothing of the text is
re-encoded**. `--selftest` joins every message back with no readings and no
re-flow; every composed message is also checked as it is written (`check`):
with its ruby rows taken out it is the original's rows, head, tail and page
terminators, with only the split's `0x02`s added.

## 2. The data it reads

- `data/furigana/readings.toml` - the sibling's `names/readings.toml`
  (BreathOfFire3Recomp, the owner's own, commit `f047091`) carried over
  unchanged under a header. Same schema: `surface`, `reading`, `next`,
  `prev`, `note`; keyed on the surface or on a verb's dictionary form
  (言う covers 言っ). The one file meant to ship (the furigana mod's data).
- `data/furigana/jp-charset.toml` - the code-to-character identification
  the decode needs: the sibling's `names/kanji.toml` (441 cells, its
  2026-09-10 corrections included) with `names/font.toml`'s `[single]`,
  `[page15]` and `[kanji_extra]`, same commit. It is the owner's reading of
  the glyph sheets, no bytes of the game. A PC glyph index maps back
  through `loc_build.ja_cell_of`'s rule: `JA_AT + cell` is the single sheet
  (cell `b` for `b < 0x5B`, `nn + 0x5B` for `0x15 nn`, `b + 0x23` for the
  kana), `JA_KANJI_AT + k` the kanji sheet; the PC's one-byte `0x20`, `0x2A`,
  `0x3C` are the space and the two hanging brackets (`loc_build.JA_HANG`).
  The kana run is `loc_build.JA_CODE`'s gojuon order.
- SudachiPy mode C, `sudachidict_core` by default (`--dict full` merges
  compounds, as the sibling's every-word table did; both installed here).

## 3. Readings

Per run of glyphs (a run ends at any control; rows are never joined):

1. A long-vowel bar or small kana right after a kanji (dialect stretching,
   気ィ) is taken out before tokenising and stays in the text after the
   word; the reading's stem never includes it.
2. Each token with a kanji takes `readings.toml` first (surface, then the
   dictionary form with the conjugated kana tail swapped in; `next` is the
   following token, `prev` a suffix of the run before the word), then the
   dictionary's reading.
3. The kana the page already shows at either end are trimmed (起きる: お
   over 起); a token with kana between its kanji (最後の夜) keeps its whole
   reading over the whole token.
4. A reading that is not all hiragana (an out-of-lexicon token reads as its
   own surface) is counted `unread`; one with a kana that has no PC code is
   counted `unencodable`. Both were 0 on the held layer.

Every occurrence is read (the sibling's shipped `jp_furigana` scope).

## 4. Layout

**Rows are never re-flowed.** A reading lives in its own row, so it never
widens its word's row and every authored `0x01` is kept; "a word and its
reading one unit" and "closers cling" hold because a text row is copied
byte for byte. What the layout decides is pages and the ruby row.

**Pages.** Every page of a composed message is row pairs,
`ruby 0x01 text [0x01 ruby 0x01 text]`, ruby first. A page of three
authored rows becomes two pages, rows 1-2 and row 3, on a new `0x02` (the
owner's two pairs, 2026-10-20). A page with nothing to read over a row
still gets an empty ruby row (reading-row.md 1.2: the stepper knows an
annotated message by its source, so every page of it is pairs). A page
that is a single empty row (a message ending on its break) is left as it
was. A message with no reading anywhere gets no composed copy (offset 0:
it plays as today).

**Left plain** (no composed copy, counted with the readings it would have
had - rule 4):

| reason | rule | why |
|---|---|---|
| not Japanese | a glyph below `JA_AT` (the port's own table) | nothing to read; none on the held layer |
| narration, a row past the box | a row wider than 16 cells | `second_language_census.W` = 192 units; the full-screen path, not the box |
| narration, a page past three rows | more than 3 rows on a page | the same, by height (the sibling's `is_box_page`) |
| own effect or offset pen | `0x0D`, `0x0E` or `0x0F` | `MsgBox_EffectDraw` is unannotated this round (round doc 1) |
| timed page needing a split | a 3-row page ending `0x16 nn` | a `0x02` would make a self-closing page wait for a press |

**The ruby row.** Half-cells (6 units). A text glyph is one cell; a first
`0x2A` / `0x3C` at the row start hangs one cell left, as `MsgBox_Step`
draws it. A runtime insert (`0x03`, `0x04 nn`, `0x07 nn`, `0x08 nn`) splits
the row into segments: segment 0 is the row up to the first insert,
segment k the row after the k-th. Each reading:

- wants twice its stem's first cell in its segment; one half-cell further
  left when it overhangs its stem by more than one half-cell and that
  half-cell is free;
- must stand over its stem (at least one half-cell shared), keep one free
  half-cell after the reading before it (two readings never touch), and
  stay inside its segment: up to the next insert, or for the last segment
  the box's 32 half-cells counted from the segment's start with the inserts
  before it at zero width (their width is unknown at build time);
- when the reading before pushes it off its stem, the readings before move
  left as far as their own stems allow (`readings_pulled_left`); when that
  is not enough and the reading before stands over the cells just before
  this stem in the same colour, the two take **one reading over both stems**
  (東西: とうざい, `readings_merged`), which is how furigana is set over a
  compound; otherwise it is dropped (`readings_no_room`). No reading was
  dropped on the held layer.

Bytes: `0x09` up to each reading, the reading's two-byte kana; before the
k-th segment the gaps up to its insert and the marker `0x12`; segments
after the last reading are not written. A reading takes its stem's colour
(`0x05 nn` / `0x06` before it when the walk's colour differs) and the row
ends on the colour it began with, so the text row below draws as before.
A page split off a longer one begins with `0x05 nn` when its first row
began coloured: `MsgBox_Step`'s colour is a local that starts at 0 on each
page's walk.

**Kept where the original had them:** a leading `0x0C nn` stays first in
the message (MsgBox_Reset eats only a leading one) and so comes before the
first ruby row; every other control is in its text row; a `0x14` choice
block is copied from its first byte; a message the block ends without its
NUL is given one (none on the held layer).

## 5. The checks

- `--selftest`: every message of every held block joined back with no
  readings and no re-flow, byte for byte.
- `check`, on every composed message as it is written (above).
- `--lint FILE`: walks each ruby row's bytes as the renderer will
  (`0x09` one half-cell, a kana one half-cell, `0x05 nn` / `0x06` nothing,
  `0x12` the next segment) and the text row's bytes for the stems' cells,
  and lists a row where a reading is not drawn where it was laid, does not
  stand over its stem, touches another reading, or passes its wall. Also
  counts readings more than a half-cell from their stem's start (moved by a
  neighbour), which is the collision rule working, not an error.
- `--review FILE`: per composed message, its text and every reading placed
  (`surface(reading)`), for the proofreading pass (owner's call 4).
- `--ambiguous FILE`: every surface placed whose lexicon entry reads two or
  more ways, most frequent first, with what was printed and whether
  `readings.toml` names it.

Both files are game text: they go in a scratch folder, never the tree.

## 6. Where the contract was silent, and the reading taken

1. **The area side chunk's tag.** reading-row.md 1.1 says `0xC8000`, "what
   Msg_OpenScript indexes". The area script block every held layer carries
   is the kind-0 chunk of tag `0x0` (arena offset 0 = `MessagePools`
   `0x803580`, what `Msg_OpenScript` indexes); `0xC8000` is the area map's
   chunk (`AreaMap_Header`, `g_area_block_loaded`). The side chunk is
   written with `0xC8000` as the contract fixes (`AREA_SIDE_TAG`; it is only
   a key RR matches), the coordinator to confirm.
2. **The leading `0x0C nn`** is ahead of the first ruby row (above).
3. **Colour** in ruby rows and at a split page's start (above).
4. **Pools past FIRST and BATTLE.** The layer carries `0x4000` pools in
   FIRST, BATTLE, BATTLE2, AFLDKWA and 40 BOSS files; each gets its chunks
   the same way (tag `0x4000`, header word 3 the half).
5. **`tools/dat.py`** knows kind 18 and lets its fourth header word be 0
   or 1 (`WORD3`); every other kind still needs it 0.
6. **The last segment's wall** after an insert is approximate (above); no
   reading reached it on the held layer.

## 7. Numbers (2026-10-22, the held `ja-JP` layer)

Selftest: 245 files, 288 blocks (200 area, 88 pool halves), 83,367 slots,
41,664 messages, **0 mismatches**.

| | area blocks (200) | pool halves (88) |
|---|---:|---|
| messages (by block / distinct by bytes) | 15,714 / 6,784 | 25,950 / 757 |
| messages changed | 10,324 / 5,454 | 17,652 / 465 |
| readings placed | 33,385 | 41,912 |
| readings merged into the word before | 179 | 136 |
| readings dropped for no room | 0 | 0 |
| pages split for a third row (= confirm presses added) | 4,559 | 6 |
| left plain: past the box / past three rows / effect / timed | 62 / 203 / 60 / 131 messages (103 / 1,006 / 158 / 1,214 readings) | 5,442 / 30 / 0 / 0 (20,298 / 118) |

All 288 blocks: 98,509 kanji words, 0 unread, 0 unencodable; 75,612
readings wanted, 75,297 placed, 315 merged, 0 dropped; 1,721 insert
markers; `readings.toml` applied 839 times. Lint: 51,031 ruby rows,
**0 misplaced**, 631 readings more than a half-cell off their stem's start.
Layer: 238 files, 282 chunks, 2.05 MB. Longest side chunk 26,327 bytes
(AREA090); longest composed message 823 bytes (AREA175 slot 112); the
largest block plus its composed messages 42,909 bytes (AREA090), past the
arena's `0x8000` - which is why the side chunk lives outside it. Build time
about 5 s. The ambiguity list holds 584 surfaces (力 りょく / ちから at
the top).

Most pool messages are menu and battle strings that never reach the box;
their composed copies are inert unless RR repoints the system box opens.
