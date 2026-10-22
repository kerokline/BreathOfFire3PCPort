# The reading row: the side chunk and the composed message (the FB / SB / RR contract)

**Status:** CONTRACT (section 1, fixed 2026-10-22 before stage A of the
reading-row round launched; [`reading-row-round.md`](reading-row-round.md)
section 4 is the decision, this file its exact bytes), MEASURED (section 2,
PR) and BUILT (section 3, RR), 2026-10-22: the furigana layer is DIV-0155,
the second-language layer DIV-0156. Section 1.2 amended and section 3.6
added 2026-10-22 by group OS (timed boxes stop, effect pages composed).
Not yet played by the owner.

## 1. The side chunk and the string shape

Every number here was chosen on 2026-10-22 from measurements named beside
it; the groups build to it and do not change it. A change goes through the
coordinator and this file first.

### 1.1 The chunk: kind 18

A reading-row layer is an optional text layer (DIV-0086) whose overlay DAT
(`DAT\<layer>.<NAME>.DAT`, or `cache/opt/<layer>/dat/<NAME>.DAT`) carries,
per text block it annotates, **one chunk of kind 18** - the next after
kind 17 (DIV-0148), the last of ours in `src/game/dat_load.cpp`'s switch;
the original loader skips any kind above 3 by its size
([`DAT_CONTAINER.md`](DAT_CONTAINER.md)). The header is the container's:

```
0x00 u32 kind   18
0x04 u32 tag    the tag of the kind-0 text chunk the side chunk annotates:
                0 for an area's script block (arena offset 0 = MessagePools
                0x803580, what Msg_OpenScript indexes, src/game/msgbox.cpp;
                corrected 2026-10-22 by FB from the staged 0xC8000, which
                is AreaMap_Header's chunk), 0x4000 for FIRST.DAT's and
                BATTLE.DAT's pool
0x08 u32 size   payload bytes
0x0C u32 half   0; for the pool tag, the pool half (0 or 1) whose slot
                table the chunk's slots index (tools/second_language_census.py
                load_blocks splits the pool payload at its two leading
                offsets the same way)
```

The payload is a slot table and composed messages, **keyed by slot** so the
text block itself is never rewritten and a slot without a composed message
plays exactly as today:

```
0x00 u32 n          slots, the same count as the annotated block's table
0x04 u32 off[n]     payload offset of slot i's composed message, 0 = none
                    (the block's own message is played)
then the composed messages, each terminated as loc_build.message_end
reads a message (a 0x00, or a 0x14 choice block to its last option's NUL)
```

Offsets are 32-bit because the census found sixteen composed area blocks
past the arena's `0x8000` and the faerie village's block alone is 665
messages ([`second-language-census.md`](second-language-census.md) 8); a
side chunk has no arena limit, it lives in memory of ours.

The loader (RR, stage B): `LoadDatFile`'s switch gains `case 18`, keeping
the chunk's bytes in our own memory keyed by (tag, half); `Msg_OpenScript`
and `MsgBox_Reopen` take the composed copy's address for a slot that has
one, else the block's; a new area load drops the previous area's side
chunk as the arena drops its block. With no layer loaded nothing is
reached, which is what keeps the `msgbox` shadow fuzz and the state hash
meaningful.

### 1.2 The composed message

The composed message is the slot's message with each page re-laid as row
pairs. Its bytes are the PC's own text codes (two-byte kana from `0x80..`,
one-byte Latin, the controls of [`msgbox.md`](msgbox.md) 3), so the
builder's `--selftest` (no readings, no re-flow) reproduces the original
message byte for byte and the stepper's existing walk is unchanged for
every byte but the ones named below.

- **A page** is `ruby 0x01 text [0x01 ruby 0x01 text] 0x02`: a ruby row,
  then its text row, at most **two pairs per page** (the owner's decision
  of 2026-10-20; a third pair goes to a new page on `0x02`). Ruby rows come
  first so a page ends on a text row, where the next-page arrow places.
- **An empty text row** under a reading row is allowed (`0x01` straight
  after the ruby row's bytes): the second language may have text where the
  main has none. An empty ruby row is allowed the same way, so a page of
  the main text with nothing to read over it is still a pair.
- **Ruby row cells are half-cells**: a ruby glyph sits on a 6 x 6-unit quad
  at a 6-unit advance for the Japanese sheet (the 12 px cell texel for
  texel at the PC's 2x); the Latin reading glyph's quad is the size the
  owner picks from PR's captures (1/2: 4 x 6; or 2/3), recorded in section
  2 when chosen. `0x09` in a ruby row advances one half-cell and draws
  nothing (in a text row it stays what it is today: one byte, nothing
  drawn). A reading's first glyph starts at twice its stem's cell index.
- **Allowed in a ruby row, and nothing else:** glyph codes, `0x09`, the
  colour codes `0x05 nn` / `0x06`, and the two insert markers below. Every
  other control (`0x0A 0x0B 0x0C 0x0D 0x0E 0x0F 0x10 0x11 0x16`, the
  inserts `0x03 0x04 0x07 0x08`) stays in the text row exactly where the
  original had it. A ruby row never holds `0x02` or `0x14`.
- **The insert marker is `0x12`, one byte.** In a ruby row it stands for
  "the drawn width of the k-th insert of the text row below", k counting
  the row's inserts in order from 1 at the row's first marker; the stepper
  resolves it at draw time, so the reading row's x after a member name,
  an item record or a sub-pool message is right whatever the player named
  Ryu. Chosen because no shipped message in any held layer (`base`,
  `ja-JP`, `en-US`, `en-150`, `fr-FR`, `de-DE`, the 200 area blocks and
  both pool halves each) carries a `0x12`, `0x13` or `0x15` byte at all
  (census 2026-10-22 over `C:\Users\kerok\bof3x-runs\rr_sl\DAT` with
  `second_language_census.tokens`; the one-byte codes that do occur are
  `0x0A..0x11` and `0x16`), and the original stepper reads `0x12` as a
  glyph's first byte, which a composed message never asks it to draw. The
  builder emits the gap bytes up to the insert, the marker, then goes on;
  a reading over an inserted name is stage B's (a per-name table built with
  the layer), not stage A's - stage A leaves the half-cells over an insert
  empty after the marker.
- **The second-language marker is `0x13`, one byte** (added 2026-10-22 at
  SB's merge): in a ruby row of a `ruby-<other>-<main>` layer it stands for
  "draw the k-th insert of the text row below here, at reading size, from
  the main's name table" - the second script shows the name, where the
  furigana row only leaves room for it. `0x13` is unused in every held
  layer by the same census as `0x12`. A ruby row holds `0x12` or `0x13`,
  never both; the stepper keys on the byte, not on the layer.
- **A pair code** (DIV-0057) in an inserted name counts two cells; the
  marker's resolved width already includes that.
- **The hanging brackets** `0x2A` / `0x3C` are written as their two-byte
  codes in a ruby row so they do not hang at a row start
  ([`second-language-census.md`](second-language-census.md) 9).
- **A `0x14` choice block** is copied from the main's message verbatim from
  its first byte to its end, and nothing precedes it in the composed
  message that did not precede it in the original. Options get no reading
  row in this round.
- **A timed close is the box's stop** (amended 2026-10-22 by group OS, the
  owner's call of the same day, section 3.6): a composed copy carries no
  `0x16 nn`. `0x16` closes the box `nn` frames after it is reached and
  nothing walks the bytes after it again, so the composed copy is the
  box's own part - the slot's message cut at its first `0x16 nn` - with
  `0x00`, the box's own end, in its place: the wait every message end has
  (`MsgBox_State2Press`), then the same close. The pages after it are the
  next slots', each composed in its own copy. The offset pen `0x0D` /
  `0x0E` and the effect preset `0x0F nn` are composed too, kept in the text
  row where the original has them (the rule above).
- **No page-head marker.** The stepper knows a page is annotated because
  the message's base pointer came from a side chunk; no shipped byte
  changes meaning.
- **Row geometry** (PR measures, RR compiles in; the values here are the
  staging doc's arithmetic until PR's hand-back replaces them in section
  2): text rows are 12 units high on the Japanese sheet's 24 x 24 cells,
  a ruby row 6, so a pair is 18 units plus the gap the owner chooses from
  PR's captures (0, 1 or 2); the plain newline's 14 stays for every
  message not from a side chunk.

### 1.3 Who owns what

- FB writes kind-18 chunks for `ruby-furigana-ja-JP` (every area block;
  the pool halves where the `ja-JP` layer carries them).
- SB writes them for `ruby-<other>-<main>` by SL's pairing rule, with the
  composed font chunk of SL 7 beside them in the same overlay.
- RR loads kind 18, repoints the box, and walks the shape above; it draws
  the marker's width and the half-cell grid.
- ML never reads a side chunk; it lists layers by name.

## 2. Row geometry, measured (PR, 2026-10-22)

From [`reading-row-probe.md`](reading-row-probe.md) 4, which has the frames
and the arithmetic. Pen y per row from the box origin `O`
(`Msgbox_OriginY`, window y + 6), a glyph drawn at pen y + 1; ruby height
`r` = 6 (kana, and Latin at 1/2) or 8 (Latin at 2/3). **Recommended:
bottom-aligned**, the last text row on the plain page's third row:

| Row | r = 6 | r = 8 |
|---|---|---|
| ruby 1 | `O + 4 - gap` | `O - gap` |
| text 1 | `O + 10 - gap` | `O + 8 - gap` |
| ruby 2 | `O + 22` | `O + 20` |
| text 2 | `O + 28` | `O + 28` |

The gap (0, 1 or 2) and the Latin size (1/2 or 2/3) are the owner's calls
off the probe's sheets; the probe recommends gap 2 and leaves the size to
the eye. Ruby glyph quads: 6 x 6 units (r = 6), 8 x 8 (Latin at 2/3), the
whole 12-unit texture cell; the ruby pen advances the glyph's own kind-4
advance times 1/2 or 2/3, `0x09` 6 units. At scale 2 every 6-unit reading
glyph is its sheet cell texel for texel under both filters (probe 3).

**Settled at PR's merge (2026-10-22):** a space in a second-language ruby
row is the composed font's blank space cell, a glyph code carrying the
second's space advance - `0xE64` (advance 8) for Latin over Japanese,
`0xE40` (advance 12) for Japanese over Latin ([`reading-row-second.md`](reading-row-second.md)
3), which SB's chunk already carries and is what the probe drew. `0x20`
never appears in a ruby row; `0x09` is the furigana row's half-cell gap.

## 3. The renderer as built (RR, 2026-10-22)

**Status:** BUILT on `reading-row/rr` (stage B, group RR), merged into
`localization/second-script` 2026-10-22; played on the nue, shop and inn
routes and the dolphin fight under `ja-JP` with `ruby-furigana-ja-JP`, and
once with SB's `ruby-en-US-ja-JP`; the owner has not played it. The row
table is section 2's bottom-aligned one with the owner's gap (3.3).

### 3.1 The loader (`src/game/dat_load.cpp`)

- `TakeChunk`'s `case 18` copies the payload into memory of ours
  (`malloc`, sixteen zero bytes after it) under one of three keys: `(0, 0)`
  the area's script block at `MessagePools + 0`, `(0x4000, 0)` and
  `(0x4000, 1)` the system pool's halves (header word 3 is the half). It is
  refused loudly (`Fatal`) outside a `BOF3X_OPT` layer's walk (DIV-0086 is
  the only way one arrives), under any other key, with a slot table that
  does not fit, an offset outside the payload, a payload not ending on a
  NUL, or a slot count that is not the block's it lands over.
- On landing it records the FNV-1a hash of the block's slot table (2n bytes
  at `MessagePools + 0`, or at the pool's half wherever DIV-0007 put it).
  A lookup whose block no longer hashes the same - a path that rewrote the
  arena without `LoadDatFile`, a quickload into another area - logs once,
  retires the side chunk and plays the block's own message.
- A kind-0 chunk of the same tag retires the side chunk (the area's when a
  new area file's text block lands, the pool's with the pool). The retired
  copy is freed at the next retirement, so a box still open on it never
  walks freed memory. Only AREA files carry a tag-0 kind-0 chunk (scan of
  the held `DAT`, 2026-10-22).
- `DatLoad_ReadingRow(tag, half, slot)` answers the composed message or
  null; `DatLoad_ReadingRowHolds(p)` says whether `p` lies among the held
  (or retired) composed messages. Before the first kind-18 chunk both
  return at once.

### 3.2 The repoint (which paths open a composed message)

- `Msg_OpenScript(id)` (sign-extended index 0 and up) and `MsgBox_Reopen`
  (the choice answer and the menu's next message; unsigned id) take
  `(0, 0, slot)` from the area's side chunk.
- `Msg_OpenSystem(id)` (`src/game/menu_windows.cpp`, one call outside this
  group's file list) takes `(0x4000, id >> 14, id & 0x3FFF)` - the pickup
  lines ("got the item") are system pool half 0; seen live (half 0 slots 0
  and 2 in the shop route).
- Not repointed: `MsgBox_State3Step`'s page restart (it takes the read
  position, which stays inside the composed message), the battle and menu
  readers of the pool (`Msg_SystemPtr` callers that draw through
  `Text_DrawString`, out of scope, reading-row-round.md 7).
- "Annotated" is the base pointer lying in a side chunk
  (`ruby::Annotated`), so every writer of `0x7DEE4C` is covered without a
  flag to keep in step. Both openers repoint only when `MsgBox_Step` and
  `MsgBox_StatePrint` are ours (`bof3::IsEnabled` after the injects);
  under `BOF3X_ORIGINAL` of either, Capcom's body would count ruby glyphs
  and fault on `0x12` (`Text_DrawString`'s trap for glyph `0xFFEC`), so the
  box plays the block's own message and the log says so once.
- Each composed open is logged (`reading row: tag .. half .. slot ..
  opened composed`).

### 3.3 The walk (`src/game/msgbox.cpp`, namespace `msgbox::ruby`)

- **The row table** (`kRowTable`, the only place the geometry lives):
  pen y from the origin of ruby 1, text 1, ruby 2, text 2 = `2, 9, 21, 28`.
  Bottom-aligned as section 2 recommends (the last text row on the plain
  page's third row, `O + 28`), with the owner's gap of 2026-10-22: even, a
  reading's ink 2 units under the text row above it and 2 over its own text
  row - the ruby row 7 units over its text row and a pair pitch of 19
  (measured off the capture, hand-back). A glyph draws at pen y + 1; a
  one-pair page uses ruby 1 / text 1. The Latin row stays at 1/2.
- `MsgBox_Step` hands an annotated message to `ruby::Step` before its
  first instruction and returns; nothing else in it changed. `Step` keeps
  the original prologue (pen at the origin, nothing drawn at a count of 0),
  skips a leading `0x0C nn`, then per pair: finds the ruby row's end and
  its colour, walks the text row exactly as the original walks a row (the
  substitutions, DIV-0140's buttons, the hang, `g.draw_char` and
  DIV-0006) - drawing while the count allows and measuring to the row's
  end either way - and then draws the ruby row from the measurements.
- A ruby glyph is `Text_EmitGlyph`'s record of its whole 12-unit cell on a
  `12 s` square quad, the page scroll's clip pair `(u, v)` scaled with it,
  advancing its DIV-0006 advance times 1/2 (a kana 6 x 6 and 6, texel for
  texel; a Latin letter 6 x 6 and 4 - the owner kept 1/2). A space in a ruby row
  is the composed font's blank cell (section 2); a `0x20` is refused. `0x09`
  is one half-cell; `0x12` advances by the k-th insert's measured width;
  `0x13` draws the k-th insert's own characters (its substitution text,
  its count, a pair code as its two glyphs) at the reading scale and
  advances by that width; one row holds one kind of marker.
- **Reveal** (`ruby::Reveal`, one function; the owner's call 3): a
  reading - a run of glyphs with only colour codes between - appears with
  the first glyph of its segment whose cell is at or after the cell its
  first half-cell begins. The other rule is the one line `return
  row.first;`. A `0x13` insert appears with the insert's first character.
  The count `0x7DEE59` is text glyphs only: `MsgBox_StatePrint`, outside a
  substitution, steps over a whole ruby row (`ruby::SkipRow`, found by the
  row parity from the base) at no cost and no delay.
- The pen left behind: the x after the last drawn text glyph and the y of
  the last text row the walk entered, which `MsgBox_State3Arrow` places
  the next-page arrow off. `MsgBox_State3Step` needs nothing: it never
  reads a row, and its restart takes the read position (just past the
  `0x02`, a ruby row's start) as the new base.
- **Refused loudly** (rule 4, `Fatal` with tag, half, slot and the byte):
  a byte the contract does not allow in a ruby row, a ruby row ending on a
  page or message end with glyphs in it, a third pair on a page, the offset
  pen `0x0D` / `0x0E` in an annotated message (the builder leaves those
  plain), a row end inside a substitution, an insert inside an insert, a
  marker with no insert under it, a pair code or an out-of-table glyph in a
  ruby row, more than 15 inserts or 96 glyphs in a row. (The offset pen
  `0x0D` / `0x0E` was refused here until 2026-10-22; it is walked now,
  section 3.6.)
- Untouched: `MsgBox_EffectDraw` itself (called from the walk as
  `MsgBox_Step` calls it, section 3.6), choice options (`0x14`, drawn by the
  window code from the read position, verbatim), every other text path.

### 3.4 Identity with no layer

No side chunk, no change: both `DatLoad_` calls return before reading
anything, `ruby::Annotated` is false, `SkipRow` returns its argument. The
`msgbox` and `menu_windows` shadow fuzzes are at 0 mismatches, and the state
hash on the routes is the record (the hand-back has the numbers).

### 3.5 The study key

F4 (the owner, 2026-10-22, for language study) hides and shows the reading
rows: a DLL-side flag, "shown" at launch, toggled on the key's first
down (a held key's repeats do nothing), effective from the next frame's
walk. Hidden, `ruby::Emit` returns before building a primitive, so no
reading glyph and no `0x13` insert is drawn; the walk, the row table, the
reveal count, the pages and the arrow are exactly as shown. A message not
from a side chunk is untouched either way. `BOF3X_RUBY_KEY` names the key:
`f1`..`f12` in any case (default `f4`; F2, F3 and F4 are the keys Capcom's
`Game_WndProc` leaves unbound), `none` binds nothing; anything else stops
at start-up. The binding sits in `src/game/win_main.cpp`'s `WM_KEYDOWN`
beside F6 / F7, after the pause's "any key resumes". The launcher writes
`ruby_key=` in `bof3x.ini` (`f4` by default, `none`, or `f1`..`f12`) and
hands it over as `BOF3X_RUBY_KEY` unless the environment sets it
(`src/launcher/config.cpp`, the end batch;
[`launcher-settings.md`](launcher-settings.md) 3).

### 3.6 Timed boxes and the offset pen (group OS, 2026-10-22)

**Status:** BUILT on `reading-row/os`; captured on the new game's opening
scene under the furigana layer and English over Japanese (the hand-back);
not yet played by the owner. The owner's call (2026-10-22): "let's add an
explicit stop to the text; it will make the new game not comparable to
before, but it makes it better for study."

**What a timed close is, measured.** `0x16 nn` sends `MsgBox_StatePrint`
to state 7: `MsgBox_State7Delay` counts `nn` frames, then steps window 0 to
its shrink (state 3) and holds the printer; `MsgBox_State2Closed` reports
the box done once the window reaches 4, and `Window_Kind0Close` frees it.
The only writers of the walk's base `0x7DEE4C` are `Msg_OpenScript`
(`0x4976EA`), `Msg_OpenSystem` (`0x49771E`), `MsgBox_State3Step`
(`0x49802A`) and `MsgBox_Reopen` (`0x4982A9`) - the five absolute operands
at that address in `BOF3.exe`, the fifth the walk's read at `0x497859`
(capstone over the code section, 2026-10-22; symbols.toml's typed-data
census counts the same five) - so no byte after a `0x16 nn` is walked by
that box again. A scene of timed boxes is one message whose slots begin
after each `0x16 nn`: the new game's opening is `AREA024` slots 0..7, and a
probe of NEW GAME with the box's state, slot, base and read pointer
peeked every 6 frames (scratch recipe `probe0`, 2026-10-22) saw the script
open slots 1, 2, ..., 7 in turn, each box reaching state 7 at its `0x16`,
the window going 2 -> 3 -> 0, and the next `Msg_OpenScript` re-basing at
the next slot's first byte (slot 0, the same message from one page
earlier, was not opened on that route).

**The conversion (the builder, both modes, `box_extent`).** A slot's
composed copy is cut at its message's first `0x16 nn` (outside a choice
block), and `0x00` - the box's own end - takes its place: one confirm
press where the timer was, no arrow (state 2, like every message end in
the game; the next-page arrow is state 3's and stays where a page break
is). A three-row page before the stop splits on `0x02` as any other. Under
the furigana layer a timed box with no reading is composed all the same
(empty reading rows), so every timed box of a layer stops; a box of a
space alone (`0C 81 20 16 00`, a blank beat of three area scenes) and a
narration page past the box stay as they are, timer and all. In the second
mode the second's message is cut at its own first `0x16` too, so a scene
of timed boxes pairs box for box. With no layer nothing changes: the side
chunk is the only place the conversion exists.

**The offset pen in the walk (`ruby::WalkText`).** `0x0D` / `0x0E` set and
clear a pen flag that, as `MsgBox_Step`'s flag bit 5, carries from row to
row of a page (a newline leaves it) and starts off on each page's walk.
Under it a text glyph is drawn exactly as `MsgBox_Step` draws one: with
flag 8 of `0x7DEE44` (effects 2 and 3, the grow and the shrink) by
`MsgBox_EffectDraw` at `MsgBox_PenX` / `Msgbox_PenY` in memory - the walk
sets `Msgbox_PenY` to each text row's pen y before walking it, as the
original's newline does - else by `g.draw_char` at `MsgBox_PenX` + the s8
`0x7DEE5A` and the row's y + the s8 `0x7DEE5B`. Measured past the count,
the effect draw's advance is `12 + P` (`P` the word `0x7DEE68`), every
other glyph's DIV-0006's. `0x0F nn` is two bytes the walk steps over, as
before; `MsgBox_StatePrint` starts the effect when the typewriter reaches
it.

**The reading row under the pen (`ruby::DrawRuby`).** A reading whose stem
(`Stem`, the glyph its reveal waits for) was drawn under the offset pen is
drawn at the same offset - the s8 pair the shake (effect 1), the wander (4)
and the rise (5) move - so it moves with its word; a `0x13` insert takes
its insert's offset the same way. A row `MsgBox_EffectDraw` draws in shows
**no reading while the effect runs** (flag 8): the effect draw stays
unannotated, its glyphs a growing cell on a pen that moves by `P` a glyph,
which the half-cell grid does not follow; the readings come back when the
effect clears flag 8 (`MsgBox_GrowStep`'s stop). The F4 key, the reveal
count and the arrow are unchanged.

**A `0x16` still in a composed message** (a layer built before this
change): the walk ends the page on it as before and `MsgBox_StatePrint`
plays its timer; nothing refuses it.
