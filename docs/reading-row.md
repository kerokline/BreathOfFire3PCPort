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
