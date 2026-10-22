# The reading row: the side chunk and the composed message (the FB / SB / RR contract)

**Status:** CONTRACT, fixed 2026-10-22 before stage A of the reading-row
round launched ([`reading-row-round.md`](reading-row-round.md) section 4
is the decision; this file is its exact bytes). Nothing here is built yet;
the sections after 1 are written by the groups as they hand back.

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

## 3. The renderer as built (RR, 2026-10-22)

**Status:** BUILT on `reading-row/rr` (stage B, group RR); played on the
nue, shop and inn routes under `ja-JP` with `ruby-furigana-ja-JP`, and once
with SB's `ruby-en-US-ja-JP`; the owner has not played it. Section 2 (PR's
numbers) is not in yet: the row table below is the staging arithmetic.

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

- **The row table** (`kRowTable`, one per gap, `kGap` the chosen one; the
  only place the geometry lives): pen y from `Msgbox_OriginY` of ruby 0,
  text 0, ruby 1, text 1 = `0, 6, 18 + gap, 24 + gap`; staged at gap 1.
  A ruby quad's top is its row's pen y + 1 (`Text_DrawAt`'s own + 1 for a
  text glyph), `kHalfCell` 6, the scale `1/2`.
- `MsgBox_Step` hands an annotated message to `ruby::Step` before its
  first instruction and returns; nothing else in it changed. `Step` keeps
  the original prologue (pen at the origin, nothing drawn at a count of 0),
  skips a leading `0x0C nn`, then per pair: finds the ruby row's end and
  its colour, walks the text row exactly as the original walks a row (the
  substitutions, DIV-0140's buttons, the hang, `g.draw_char` and
  DIV-0006) - drawing while the count allows and measuring to the row's
  end either way - and then draws the ruby row from the measurements.
- A ruby glyph is `Text_EmitGlyph`'s record of its whole 12-unit cell on a
  `12 s` square quad (6 x 6 at `s = 1/2`), the page scroll's clip pair
  `(u, v)` scaled with it, advancing its DIV-0006 advance times `s` (a
  kana 6, a Latin letter 4); a space advances and draws nothing. `0x09`
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
  ruby row, more than 15 inserts or 96 glyphs in a row.
- Untouched: `MsgBox_EffectDraw` (never reached: the offset pen is
  refused), choice options (`0x14`, drawn by the window code from the read
  position, verbatim), every other text path.

### 3.4 Identity with no layer

No side chunk, no change: both `DatLoad_` calls return before reading
anything, `ruby::Annotated` is false, `SkipRow` returns its argument. The
`msgbox` and `menu_windows` shadow fuzzes are at 0 mismatches, and the state
hash on the routes is the record (the hand-back has the numbers).
