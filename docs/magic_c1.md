# The unfinished skills: MAGIC010, 080, 113, 145, 146, 213

**Status:** IN PROGRESS (2026-09-26) - sixty-two functions ours
(`src/game/magic_c1.cpp`, shadow name `magic_c1`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)): 0 mismatches in
124,000 rounds; CONTROLS_SUMMARY. Fuzz only: no recorded route casts any of
them (section 10).

Group C1 of round nine's second spell wave
([`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §4):
the rows TCRF's "Entirely Unused" skills load
([`cut-content.md`](cut-content.md) §2). TCRF's names and descriptions are
third-party claims about the **PlayStation** release; this page says what the
**PC's code** does for each, read to the last instruction and fuzzed against
the original, and does not assert what the game shows. The function names
are this project's, from what the code does.

## 1. The six overlays

| Overlay | Row | TCRF ids (their names) | Extent | Functions |
|---|--:|---|---|--:|
| MAGIC010 | 9 | `0x19` (White Flag) | `0x49DEF0..0x49DFF4` | 3 |
| MAGIC080 | 27 | `0x50` (Raaku), `0x6F` (The World), `0x70` (Again), `0x93` (Death Bomb), `0x94` (Roulette), and ids players meet | `0x4FC330..0x4FC696` | 5 |
| MAGIC113 | 10 | `0x71` (Pentagram) | `0x4D67F0..0x4D7952` | 26 |
| MAGIC145 | 149 | `0x91` (Ink) | `0x4E9A70..0x4E9EE0` | 6 |
| MAGIC146 | 150 | `0x92` (Ink Ink) | `0x4E9EF0..0x4EA446` | 10 |
| MAGIC213 | 148 | `0xD5` (TCRF: PurifyAll; the shifted label reads Miyakuri) | `0x4F4A60..0x4F50A6` | 12 |

Extents from `tools/magic_rows.py --unit MAGIC0NN --clones` (capstone
recursive descent; every jump internal, no jump table, nothing `REFUSED`).
None was ours; none was found inside or missing from the extents: 62
functions, 9,226 bytes, as the queue counted. Each function's own evidence
line is in `symbols.toml`.

## 2. Row 9, MAGIC010 (TCRF's White Flag)

Three functions and two borrowed phases. The kind-2 task `WhiteFlag_Task`
(`0x49DEF0`) runs a four-entry stack table by `+1`:

1. `WhiteFlag_TintSource` (`0x49DF30`): the source sprite's (`0x904B4C`)
   tints released and a tint (0, 0, 0, 1) set on it, its record in `+0xB`;
2. MAGIC117's `0x4D9FD0` (group S26's): that tint record's colour bytes up by
   one a frame until `+9` reaches 0x10;
3. `WhiteFlag_Untint` (`0x49DF70`): the colour bytes down by one a frame for
   16 frames; then the tint released and `BattleActor_Flash(target)`;
4. the engine's `0x43F460` (row 123's last phase, group E's): the target's
   flag 0x40, the effect-done flag, the task freed.

So on the PC the code brightens the caster's sprite over 16 frames, dims it
back over 16, flashes the target actor, and ends. It calls nothing that
applies a result: the effect code of every row ends with the target's flag
0x40 and the done bit, and what an ability does to its target is the
battle engine's, not this row's (not read here).

## 3. Row 27, MAGIC080 (a shared row)

Five functions. **The code reads no ability id**: every id that loads row 27
runs exactly this, so what it draws is the same for The World, Again, Death
Bomb, Roulette and the ids players meet. The task `Magic080_Task`
(`0x4FC330`), two entries by `+1`:

- `Magic080_Start` (`0x4FC360`): the task placed at `Field_Kind2X` /
  `Field_Kind2Z` (`0x905E64` / `0x905E60`, the field's kind-2 point - named
  as hypotheses in `symbols.toml`), its height the map's elevation there
  (`AreaMap_Elevation`); `+0xB` = `Rand & 0x3F`; `+9` = 0x30;
- `Magic080_Run` (`0x4FC3C0`), every frame for 0x30 frames: the screen
  point, 0x24 to the left; then **one of two draws, chosen once by `+0xB`**:
  - `+0xB` not 0 (63 casts in 64): `Magic080_DrawGlyphs` (`0x4FC540`), seven
    12 x 12 textured quads side by side, grey, from the texture page at
    (0x3C0, 0) with CLUT (0, 0x1E0), each cell's u / v a byte pair of
    `Magic080_GlyphCells` (`0x65DA28`, 28 bytes);
  - `+0xB` 0 (one cast in 64): `Magic080_DrawText` (`0x4FC420`), six
    characters, each the first two-byte character of one of six strings
    `Magic080_TextPointers` (`0x66A3C8`) points at, as code-0x6C primitives
    (`Gpu_SetCode6C`) with the character's code at `+0x16` and CLUT (0,
    0x1E0).

  At the end, the target's flag 0x40, the done flag, the task freed.

What the seven cells and the six characters are is data in the exe and is
not reproduced here; which glyphs they select is for the owner to look at
(a live cast, or `BOF3X_TEXTLOG`). Two things the code does make clear: the
text is drawn at the field's kind-2 point, not at the caster or target -
in a battle, wherever the field last left that point (not measured) - and it
is the same text whatever the ability. TCRF's "prints text" and "field only"
fit that shape; whether they are this is not measured.

## 4. Row 10, MAGIC113 (TCRF's Pentagram)

Twenty-six functions: a task, four kinds of child, and a disc. The task
`Pentagram_Task` (`0x4D67F0`) runs an eight-entry stack table by `+1` and,
while `+0` is set, draws `Pentagram_DrawDisc` under the actor's matrix every
frame:

| # | Phase | Does |
|--:|---|---|
| 0 | `Pentagram_Start` `0x4D6860` | **Target and actor on the same side** (both below 3, or both 3 and up): the target's flag 0x40, the done flag, the task freed - nothing drawn. Otherwise the task at the source sprite; row 26's first CLUT (`Gfx_ClutStrip` 0x1A00..0x1A0F) from its source with the STP bit; `+0xB`, `+9` 0. |
| 1 | `Pentagram_Rings` `0x4D6900` | `+9` up to 0x10; then two rings (children of kind 0, `+0xB` 0 and 1), sound 0x101 |
| 2 | `Pentagram_Star` `0x4D6A30` | once the rings set `+0xB` to 3: the five-line figure (kind 1) |
| 3 | `Pentagram_Band` `0x4D6AC0` | once the figure sets `+0xB` to 4: the band (kind 2), sound 0x102; bytes `0x803154` = 0x7F and `0x92BF14` = 0 (unnamed) |
| 4 | `Pentagram_Sprites` `0x4D6B70` | two dropped calls a frame (`Port_DroppedCall(0x14, 0x10)`, `(0x14)`: bare `ret`s); once the band sets `+0xB` to 5: six sprites (kind 3), `+0xB` 0 |
| 5 | `Pentagram_WaitSprites` `0x4D6C60` | once the six sprites have counted `+0xB` up to 6: `+0xB` 0x84 - the release |
| 6 | `Pentagram_WaitChildren` `0x4D6C80` | once the rings, figure and band have counted it down to 0x80 |
| 7 | MAGIC114's `0x4D7BD0` (group S26's) | `+9` (0x10 since phase 1) down; at 0 the target's flag 0x40, the done flag, the task freed |

Every child is `BattleTask_Create(1, 3)`: `PentagramChild_Task` (`0x4D6E30`)
jumps through `PentagramChild_Kinds` (`0x65B9F0`) by its `+1`:

- **the rings** (`PentagramRing_Run` `0x4D6E50`, phases `PentagramRing_Phases`
  `0x65BA08`): `PentagramRing_Grow` counts `+9` to 0x21, `PentagramRing_Draw`
  drawing up to 32 flat lines round a circle of radius
  `PentagramRing_Radii[+0xB]` (`0x65BA00`), one more a frame; at 0x21 the owner's
  `+0xB` = 3; `PentagramFx_WaitRelease` until the owner's `+0xB` is 0x84;
  `PentagramFx_Fade` dims them over 12 frames (semi-transparent) and counts the
  owner down;
- **the figure** (`PentagramStar_Run` `0x4D7080`): the same, 0x28 frames of
  growth (owner `+0xB` = 4), `PentagramStar_Draw` drawing five flat lines between
  six points at the angles `PentagramStar_Points` (`0x65BA14`, six signed bytes)
  x 0x111, radius 288 / 4096 of the unit, each line extended an eighth a frame;
- **the band** (`PentagramBand_Run` `0x4D7380`): 0x31 frames of growth (owner
  `+0xB` = 5), `PentagramBand_Draw` drawing up to 32 gouraud quads standing on a
  circle of radius 304 / 4096, black at the foot, pale at the top, the tops
  jittering (`Rand & 0x3F`) once grown; `PentagramBand_WaitRelease` (sound
  0x103) and `PentagramBand_Fade` (16 frames);
- **the sprites** (`PentagramSprite_Run` `0x4D7760`, with the sprite
  frame-offset table `0x9039D8` switched to the effects' 0x8E3580 around its
  phase): MAGIC130's count-down of `+9`, `PentagramSprite_Start` (the sprite
  raised by +0xB and +4, colour 0xC0, animation `+0xB`),
  `PentagramSprite_Animate` (three runs of its animation script, then the
  owner's `+0xB` up and the task freed).

So on the PC the code draws, in turn, a translucent disc at the caster, two
rings, a five-line figure, a standing band of light and six animated sprites,
then releases and fades them - but **only when the target is on the other
side from the actor**. A player casting it on the party (or an enemy on the
enemies) gets nothing drawn. TCRF's "works, with an animation seen nowhere
else" fits a cast at the other side; that it is this animation is not
measured.

## 5. Rows 149 and 150, MAGIC145 / 146 (TCRF's Ink, Ink Ink)

**Ink** (`Ink_Task` `0x4E9A70`, three phases: `Ink_Start`, MAGIC009's
`0x49DA50` - wait for `+0xB` 0 -, the engine's end `0x43F460`). `Ink_Start`
(`0x4E9AA0`) makes three puffs (`BattleTask_Create(1, 0x6C)`: `+0xB` 0..2,
delays 1, 5, 9); row 26's entries 0x1A21..0x1A2F from their source with the
STP bit and 0x1A20 cleared; sound 0x100; the **target's flags 0x10**. A puff
(`InkPuff_Task` / `InkPuff_Run`, phases `InkPuff_Phases` `0x65BE04`):
`InkPuff_Start` places it at the source sprite offset by `InkPuff_Offsets`
row `+0xB` (turned by the sprite's direction, `0x446770`); MAGIC009's
`0x49DB90` grows it (`+9`, `+0xA` up by 2 to 0x10); MAGIC040's `0x4A5D50`
fades it and counts the task down. `Ink_DrawPuff` (`0x4E9C90`, shared with
146) draws a puff: one semi-transparent textured quad of radius `+9` x 2 and
grey `+0xA` x 8, page (0x340, 0x100), CLUT (0x20, 0x1FA).

**Ink Ink** (`InkInk_Task` `0x4E9EF0`, the same three phases, then its puff
pool walked): `InkInk_Start` (`0x4E9F70`) clears the 64-record pool
`InkInkPuff_Pool` (`0x6A6F40`), puts a child (`BattleTask_Create(1, 0x6D)`)
on every actor of the target's side that `Battle_ActorIsOut` does not
answer for (enemies 3..10 with the target byte's 0x40, else the party
0..2), delays 16 apart, and the same CLUT change. Each actor child
(`InkInkActor_Run`, phases `InkInkActor_Start`, MAGIC063's `0x4B3320`):
`InkInkActor_Start` (`0x4EA130`) takes the actor's record, sound 0x100,
three puff records from the pool (`InkInkPuff_Alloc`), and **that actor's
flags 0x10**; MAGIC063's phase waits for its puffs, then the actor's flag
0x40 and the task counted down. The pool's records run as tasks under
`InkInk_Task`'s walk: `InkInkPuff_Start` (at the actor child, offset by
`InkInkPuff_Offsets` row `+0xB`), `0x49DB90` (grow), `InkInkPuff_Fade`
(fade, count the actor child down, free the record through MAGIC219's
`0x4F6290`); drawn by `Ink_DrawPuff`.

So on the PC both draw smoke puffs - over the caster (145) or over each
living actor of the target's side (146) - and set **flag 0x10** on the
target (145) or on each of those actors (146), a flag the other spells of
this round set too (Blizzard's 0x200, Magic073's 0x40, ...). What 0x10 means
to the engine is not read here; TCRF's "no damage" is not measured.

## 6. Row 148, MAGIC213

Twelve functions. `Magic213_Task` (`0x4F4A60`: `Magic213_Start`, then
MAGIC131's end once `+0xB` is 0, and its mote pool walked).
`Magic213_Start` (`0x4F4AE0`) clears the pool `Magic213Mote_Pool`
(`0x6B2958`), puts a child (`BattleTask_Create(1, 0x6B)`) on each living
actor of the target's side as Ink Ink does, copies row 26's first CLUT from
its source **without** the STP bit (putting back what MAGIC113 and others
set), and plays sound 0x100. Each actor child (`Magic213Actor_Run`, four
phases `Magic213Actor_Phases` `0x65C1E8`):

1. `Magic213Actor_Start` (`0x4F4C80`): at the actor's position, seven motes
   from the pool (delays 1, 5, .. 25), the actor's tints released and a tint
   (0, 0, 0, 1) set on it;
2. `ActorFx_TintUp` (`0x4F4DA0`, also MAGIC073's and 074's -
   [`magic_s16.md`](magic_s16.md) §3): the tint up by one a frame for 16;
3. `Magic213Actor_Untint` (`0x4F4E10`): down again; at 0 the tint record
   released and the actor's flag 0x40;
4. `MagicFx_EndWithChildren` (group S24's, ours).

The motes (`Magic213Mote_Run`, with the frame-offset table switched as the
Pentagram sprites'): `Magic213Mote_Start` places a sprite on a circle of 12
round the actor at one of eight angles, 0x180 up, animation `+0xB % 3`;
`Magic213Mote_End` waits for its script's end, counts the child down, frees
the record.

So on the PC: each living actor of a side glows up and back over 32 frames
with seven sprites round it, and gets flag 0x40. TCRF's "an animation, no
effect" is not measured.

## 7. How ours is written

As the first wave's: every call through the harness (`MH_CALL`, `MH_AT`,
`Phase`), Sprite_Current and the owner read again after every call the
original reads them after, registers the original holds across a call held
too (the source in `InkPuff_Start`, the actor record in the two `_Start`s,
the last task read in `Pentagram_Sprites`). The shared shapes are written
once: `NewChild` (MAGIC113's children), `SpawnOnActors` (146 and 213),
`SpawnRecords` and `PoolAlloc` (the pools), `WalkPool`, `PuffPlace`,
`LineColour`, `ClutCopy`. The `.data` dispatchers read their cells in place
(`CellCall`), as S16's do; like every stack dispatcher (`StackCall`) they
abort past their table's own entries - where the original would call what
follows (the next table, or data). No DIVERGENCE entry: nothing else
differs.

## 8. Calls into other units (raw addresses)

| Address | Unit (group) | Called as | By |
|---|---|---|---|
| `0x4D9FD0` | MAGIC117 (S26) | stack-table phase | `WhiteFlag_Task` |
| `0x43F460` | engine row 123 (E) | stack-table phase | `WhiteFlag_Task`, `Ink_Task`, `InkInk_Task` |
| `0x49DA50` | MAGIC009 (S03) | stack-table phase | `Ink_Task`, `InkInk_Task` |
| `0x4E5200` | MAGIC131 (S30) | stack-table phase | `Magic213_Task` |
| `0x4D7BD0` | MAGIC114 (S26) | stack-table phase | `Pentagram_Task` |
| `0x4E47F0` | MAGIC130 (S30) | `.data` phase | `PentagramSprite_Run` |
| `0x49DB90` | MAGIC009 (S03) | `.data` phase | `InkPuff_Run`, `InkInkPuff_Run` |
| `0x4A5D50` | MAGIC040 (S07) | `.data` phase | `InkPuff_Run` |
| `0x4B3320` | MAGIC063 (S13) | `.data` phase | `InkInkActor_Run` |
| `0x4F6290` | MAGIC219 (S37) | tail jump | `InkInkPuff_Fade`, `Magic213Mote_End` |
| `0x446770` | engine, unnamed (in no group) | call | `InkPuff_Start`, `InkInkPuff_Start` |
| `0x4DF820` | `Port_DroppedCall` (MAGIC124's extent, a bare `ret`) | call | `Pentagram_Sprites` |

And ours by name: `MagicFx_EndWithChildren` (S24, through
`Magic213Actor_Phases`), `MagicFx_PushActorMatrix`, and the engine's.
Reached from outside: `ActorFx_TintUp` is entry 2 of MAGIC073's and 074's
child tables (group S16's, by address).

## 9. The fuzz

`BOF3X_SHADOW=magic_c1`, `magic_c1_fuzz.cpp`, one `magic_harness::Run`; no
harness edit. Beyond the standard set:

- **the clones**: 62, the clone tables as `magic_rows.py --clones` printed
  them; `ret_mask` 0xFF on the two allocs;
- **callees**: the GPU set-ups; `Gte_RotTransPers` / `3` / `4` with `deref`
  (the six bytes of each vertex in `Prim_VertexScratch`); `Gfx_CommitPrim`
  and `MapView_LinkPrimAt` with an `effect` that logs the fuzz's 0x800-byte
  packet buffer and advances `Gfx_PacketNext` by the size, as the real ones
  do - so every primitive of a draw is compared; `0x446770` with an `effect`
  logging the task's `+0xC..+0x13` and direction as the caller left them and writing new ones (the turn), which the puffs read again;
  `Sprite_UpdateScreen` noting the frame-offset table pointer; the five phases
  called with that pointer switched (`0x4E47F0`, `0x4D77D0`, `0x4D78D0`,
  `0x4F4F00`, `0x4F5030`) listed as `kPhase` callees logging it, ahead of
  their `.data` tables; the group's nine directly called functions as
  `kPhase`; the allocs answering 0..0x3F or 0xFF;
- **fifteen `.data` tables**, each cell once (abutting tables list their own);
- **regions** (33,812 bytes with the standard ones): the scratch and vertex
  words, `Gfx_PacketNext` and the buffer, `MoveScript_TintRecords`, CLUT
  entries 0x1A00..0x1A3F (one row past the last written, so an overrun shows) and their source, `Field_Kind2Z` / `X`, `0x9039D8`,
  `0x803154`, `0x92BF14`, both pools whole, and the overlays' `.data` the
  functions read (radii, points, offsets, glyph cells) less the handler
  cells;
- **`phase_span` 3**: `PentagramSprite_Run` reads `+2` after two calls and
  calls through a three-entry table; a disturbed `+2` past it sends the
  original through data (the first run crashed there);
- **the seed**: the packet pointer into the buffer; both pools' live bits
  (full, filled to a point, or random) and every record's `+0x80` a real slot
  or record (the walks make it the owner, which the recorders write
  through); the target's side bit half the time; each dispatcher's phase
  inside its table; each count-down at, below and past its end (`+9`
  thresholds 0x10, 0x21, 0x28, 0x31; the owner's `+0xB` at 3..6, 0x80, 0x84;
  the fades at 0xC and 0x10); `Pentagram_Start`'s target and actor across
  the side boundary (2 / 3); `+4` an actor of the right side two rounds in
  three; the draws' `+9` across their loop bounds (`PentagramBand_Draw`'s
  0x10 and 0x20); a Rand hint for the band's jitter; the untint's end byte;
- **the disturbance** of the group's cells: the packet pointer, a scratch or
  vertex word, the owner's count near its thresholds, a pool record's live
  bit, the target's side bit.

**Result** (2026-09-26; this worktree):

    shadow      magic_c1 self-test: 124000 rounds over 62 functions (2000 each), 1833294 calls to the stand-ins,
                0 MISMATCHES; 33812 bytes of state (26 regions) and the stand-ins' log compared

Every callee and every handler the clones name is reached (the coverage
lines; e.g. `Battle_ActorIsOut` 21,875, `BattleTask_Create` 17,576,
`Gte_RotTransPers` 95,960, `0x4EA260` 80,893 - the walk -, the allocs 2,046
and 4,669, every phase 219..2,000). Counts depend on the build directory
([`takeover-queue-round9.md`](takeover-queue-round9.md) §6).
`BOF3X_SHADOW='*'`: exit 0.

## 10. The controls

CONTROLS_TABLE

## 11. What nothing reached

No recorded route casts any of these rows (the queue's §5); nothing here has
run live. The live check is the owner's: DIV-0045's cheat puts a skill in a
list. What to look at, by the reading above: White Flag's caster glow and
target flash; row 27's seven glyphs (or, one cast in 64, six characters) and
where they appear; Pentagram cast at the other side (and nothing at one's
own); the Ink puffs; row 148's glow and motes on each actor of a side. And
what none of these functions decides: what the ability does to its target.

## 12. Latent defects (Capcom's, kept)

For the coordinator to number; nothing is fixed.

- **Unbounded dispatch** - every task's stack table and every `.data`
  dispatcher: a phase past its entries calls what follows (the next table,
  or data). Nothing here steps a phase past; ours aborts.
- **The target byte's side bit is read late.** `InkInkActor_Start` and
  `Magic213Actor_Start` find their actor's record from the target byte's
  0x40 **when the child starts** (after its delay of up to 16 x 7 + 1
  frames), not when it was spawned; if the byte changes in between, a party
  index is read as an enemy (`0x93B960 - 0x378` for member 0: inside the task
  slots) or an enemy index as a party member (past the three members). Whether
  the target byte changes during an effect is not measured.
- **Pentagram waits on exact counts.** Phases 5 and 6 wait for `+0xB` to be
  exactly 6 and exactly 0x80; phase 4 makes six sprites and phases 1..3 four
  counted children, each from `BattleTask_Create` whose answer is never
  checked. A create that fails (no slot free - what it answers then was not
  read here) would leave the count short and the task waiting for ever.
  Whether a battle ever runs out of task slots is not measured.
- **Pentagram leaves row 26 changed.** `Pentagram_Start` sets the STP bit on
  row 26's first sixteen CLUT entries; nothing in MAGIC113 puts them back
  (MAGIC213's start copies them back from the source, and others may). Not
  measured in play.
- **Unbounded `.data` reads**: `PentagramRing_Radii[+0xB]`, the offset rows
  `[+0xB]` of both puffs, and the tint record `+0xB` / `+0xA` (inside
  `MoveScript_TintRecords`' 3,072 bytes for any byte) - reads, not crashes.
- **Row 27 in battle** draws at the field's kind-2 point, whatever it holds
  (section 3) - by design or not, not read.
