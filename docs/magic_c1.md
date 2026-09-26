# The unfinished skills: MAGIC010, 080, 113, 145, 146, 213

**Status:** IN PROGRESS (2026-09-26) - sixty-two functions ours
(`src/game/magic_c1.cpp`, shadow name `magic_c1`), fuzzed headless through
the shared harness ([`magic_harness.md`](magic_harness.md)): 0 mismatches in
124,000 rounds; 211 of 214 negative controls refused by a count, the other three two equivalent mutants (near variants refused) and one refused by a fault (section 10). Fuzz only: no recorded route casts any of
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

Two hundred and fourteen, planted one at a time by a script (the scratch
`controls.py`, not committed: replace an anchor that occurs once, rebuild,
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=magic_c1`, read the log; at the end the
source restored and **rebuilt**), 2026-09-26, on the fuzz as committed.
**211 of 214 refused** by a count (exit 3), each in the function or
functions its plant touches; counts are this worktree's. The three not
refused:

- **P71, equivalent**: `PentagramBand_Draw`'s first height, `b << 5` below
  0x10 and 0x200 from there - at exactly 0x10 both give 0x200, so moving the
  bound to include 0x10 changes nothing any input can see. Its near variant
  P71b (0x201 when grown) is refused.
- **I4, equivalent**: `Ink_Start` copying CLUT entry 0x1A20 too - the next
  instruction clears it, so no state holds the copy. Its near variant I4b
  (the copy one entry too far, to 0x1A30) is refused; a first run of it was
  not, because the region then stopped at 0x1A2F, and the region was widened
  to 0x1A3F for it.
- **J30, refused by a fault, not a count**: `InkInkActor_Start` computing the
  actor's record after the sound call. The disturbance then moves `+4` (any
  byte) as well as the side bit, and an enemy index up to 255 reads past the
  image: the run faults (exit 0xC0000005) - once after 12 counted mismatches,
  once before any. Its variant J30b (only the side bit read late, `+4`
  before) is refused by a count.

Two plants of the first run were themselves wrong (I12 and I22 re-read in
the planted code at the same point ours reads, so planted nothing) and were
rewritten; and I10 (a puff's `dz` from the wrong dword) went unrefused until
the turn's stand-in logged the offsets it is handed - the fuzz was fixed, not
the plant. The table is the second run, after both.

The thinnest are the re-reads across one call: I18 (6 rounds: the puff's
radius word taken unsigned - only a negative radius word, which only the
disturbance sets, shows it), I23, J27 (the allocs' last record), P10 /
P11 (`NewChild` reading the owner or the task before the create), P21, I12;
each shows only when a disturbance lands on exactly that call.

| | Planted | Refused in (rounds of 2,000) |
|---|---|---|
| W1 | WhiteFlag_Task: entries 0 and 1 swapped | WhiteFlag_Task 995 |
| W2 | TintSource: tint blue 1 | WhiteFlag_TintSource 2,000 |
| W3 | TintSource: +9 = 1 | WhiteFlag_TintSource 2,000 |
| W4 | TintSource: the source not read again after the release | WhiteFlag_TintSource 45 |
| W5 | Untint: two colour bytes | WhiteFlag_Untint 2,000 |
| W6 | Untint: records of 16 bytes | WhiteFlag_Untint 1,995 |
| W7 | Untint: the actor flashed, not the target | WhiteFlag_Untint 647 |
| W8 | Untint: +1 on without the release | WhiteFlag_Untint 669 |
| M1 | Magic080_Task: its two entries swapped | Magic080_Task 2,000 |
| M2 | Start: x from Field_Kind2Z | Magic080_Start 2,000 |
| M3 | Start: Rand & 0x1F | Magic080_Start 1,036 |
| M4 | Start: +9 0x2F | Magic080_Start 2,000 |
| M5 | Start: elevation at (+0x38, +0x34) | Magic080_Start 2,000 |
| M6 | Run: x 0x23 to the left | Magic080_Run 2,000 |
| M7 | Run: glyphs and text swapped | Magic080_Run 2,000 |
| M8 | Run: done flag 2 | Magic080_Run 466 |
| M9 | Run: the screen update after the move | Magic080_Run 60 |
| M10 | DrawText: five characters | Magic080_DrawText 2,000 |
| M11 | DrawText: the first byte whole | Magic080_DrawText 2,000 |
| M12 | DrawText: CLUT y 0x1E1 | Magic080_DrawText 2,000 |
| M13 | DrawText: v 11 at the third corner | Magic080_DrawText 2,000 |
| M14 | DrawText: slot 3 | Magic080_DrawText 2,000 |
| M15 | DrawText: third corner 11 down | Magic080_DrawText 2,000 |
| M16 | DrawGlyphs: u from the second byte | Magic080_DrawGlyphs 2,000 |
| M17 | DrawGlyphs: blend 0 | Magic080_DrawGlyphs 2,000 |
| M18 | DrawGlyphs: 11 wide | Magic080_DrawGlyphs 2,000 |
| M19 | DrawGlyphs: the packet pointer read once | Magic080_DrawGlyphs 2,000 |
| M20 | DrawGlyphs: grey 0x7F in blue | Magic080_DrawGlyphs 2,000 |
| M21 | DrawGlyphs: the screen point read before SetPolyFT4 | Magic080_DrawGlyphs 386 |
| P1 | Pentagram_Task: WaitSprites and WaitChildren swapped | Pentagram_Task 494 |
| P2 | Pentagram_Task: the disc drawn when +0 is 0 | Pentagram_Task 2,000 |
| P3 | Start: the actor side at 2 | Pentagram_Start 416 |
| P4 | Start: the bail flags the actor | Pentagram_Start 654 |
| P5 | Start: height from +0x38 | Pentagram_Start 1,053 |
| P6 | Start: STP 0x4000 | Pentagram_Start 1,053 |
| P7 | Start: CLUT entries 0x1A00..0x1A0E | Pentagram_Start 1,053 |
| P8 | Start: Gfx_ClutStripDirty not set | Pentagram_Start 1,051 |
| P9 | NewChild: direction - 1 | Pentagram_Rings 434, Pentagram_Star 467, Pentagram_Band 452 |
| P10 | NewChild: the owner read before the create | Pentagram_Rings 19, Pentagram_Star 9, Pentagram_Band 9 |
| P11 | NewChild: the task read before the create | Pentagram_Rings 19, Pentagram_Star 10, Pentagram_Band 17 |
| P12 | NewChild: word +0x3E from +0x3C | Pentagram_Rings 434, Pentagram_Star 467, Pentagram_Band 452 |
| P13 | NewChild: parameter 4 | Pentagram_Rings 434, Pentagram_Star 467, Pentagram_Band 452 |
| P14 | Rings: at 0x11 | Pentagram_Rings 875 |
| P15 | Rings: sound 0x100 | Pentagram_Rings 434 |
| P16 | Rings: +0xB 1 and 2 | Pentagram_Rings 434 |
| P17 | Star: a child of kind 2 | Pentagram_Star 467 |
| P18 | Band: at +0xB 5 | Pentagram_Band 909 |
| P19 | Band: 0x803154 = 0x7E | Pentagram_Band 452 |
| P20 | Band: 0x92BF14 = 1 | Pentagram_Band 452 |
| P21 | Band: the sound before +1 on | Pentagram_Band 11 |
| P22 | Sprites: dropped call 0x15 | Pentagram_Sprites 2,000 |
| P23 | Sprites: +9 (i >> 1) * 8 + 2 | Pentagram_Sprites 407 |
| P24 | Sprites: +4 (i >> 1) & 1 | Pentagram_Sprites 407 |
| P25 | Sprites: the task read before each create | Pentagram_Sprites 61 |
| P26 | Sprites: at +0xB 4 | Pentagram_Sprites 836 |
| P27 | Sprites: five sprites | Pentagram_Sprites 407 |
| P28 | WaitSprites: at 7 | Pentagram_WaitSprites 883 |
| P29 | WaitSprites: +0xB 0x83 | Pentagram_WaitSprites 433 |
| P30 | WaitChildren: at 0x81 | Pentagram_WaitChildren 868 |
| P31 | DrawDisc: radius 0x1C1 | Pentagram_DrawDisc 1,997 |
| P32 | DrawDisc: shade x 11 | Pentagram_DrawDisc 2,000 |
| P33 | DrawDisc: tpage 0x56 | Pentagram_DrawDisc 2,000 |
| P34 | DrawDisc: 31 triangles | Pentagram_DrawDisc 2,000 |
| P35 | DrawDisc: the last rim point read before the set-ups | Pentagram_DrawDisc 79 |
| P36 | DrawDisc: the radius not read again | Pentagram_DrawDisc 581 |
| P37 | DrawDisc: a rim colour 2 | Pentagram_DrawDisc 2,000 |
| P38 | DrawDisc: the centre vertex not cleared | Pentagram_DrawDisc 2,000 |
| P39 | PentagramChild_Task: by +2 | PentagramChild_Task 1,513 |
| P40 | Ring_Run: the radius by +0xA | PentagramRing_Run 1,026 |
| P41 | Ring_Run: the radius read before the push | PentagramRing_Run 51 |
| P42 | Ring_Grow: at 0x20 | PentagramRing_Grow 915 |
| P43 | Ring_Grow: owner +0xB 2 | PentagramRing_Grow 490 |
| P44 | FxWaitRelease: at 0x85 | PentagramFx_WaitRelease 910 |
| P45 | FxFade: at 0xD | PentagramFx_Fade 871 |
| P46 | FxFade: the owner count up | PentagramFx_Fade 435 |
| P47 | Ring_Draw: a line while +9 >= i | PentagramRing_Draw 511 |
| P48 | LineColour: green 0xAB | PentagramRing_Draw 1,767, PentagramStar_Draw 1,358 |
| P49 | LineColour: red dimmed x 21 | PentagramRing_Draw 1,638, PentagramStar_Draw 1,319 |
| P50 | LineColour: dimmed at +2 1 | PentagramRing_Draw 1,866, PentagramStar_Draw 1,717 |
| P51 | Ring_Draw: semi-transparent at +2 1 | PentagramRing_Draw 1,867 |
| P52 | Ring_Draw: the second depth at +0x18 | PentagramRing_Draw 1,918 |
| P53 | Ring_Draw: the new point height not cleared | PentagramRing_Draw 120 |
| P54 | Ring_Draw: the closing commit to slot 4 | PentagramRing_Draw 2,000 |
| P55 | Ring_Draw: the first point from Sin(0) of the old radius | PentagramRing_Draw 1,999 |
| P56 | Star_Run: drawn whatever +0 | PentagramStar_Run 980 |
| P57 | Star_Grow: at 0x29 | PentagramStar_Grow 866 |
| P58 | Star_Grow: owner +0xB 5 | PentagramStar_Grow 433 |
| P59 | Star_Draw: capped at 7 | PentagramStar_Draw 1,746 |
| P60 | Star_Draw: a floor, not a truncation | PentagramStar_Draw 1,629 |
| P61 | Star_Draw: angle x 0x110 | PentagramStar_Draw 1,956 |
| P62 | Star_Draw: the points unsigned | PentagramStar_Draw 1,822 |
| P63 | Star_Draw: a line while 8 i <= +9 | PentagramStar_Draw 135 |
| P64 | Star_Draw: the far point radius 289 | PentagramStar_Draw 1,958 |
| P65 | Star_Draw: +9 read once for the loop | PentagramStar_Draw 954 |
| P66 | Star_Draw: the near point z from the far one | PentagramStar_Draw 1,958 |
| P67 | Band_Run: by +1 | PentagramBand_Run 1,309 |
| P68 | Band_Grow: at 0x30 | PentagramBand_Grow 864 |
| P69 | Band_WaitRelease: sound 0x104 | PentagramBand_WaitRelease 444 |
| P70 | Band_Fade: at 0x11 | PentagramBand_Fade 879 |
| P71 | Band_Draw: the first height at 0x10 too | **NOT REFUSED (exit 0)** |
| P72 | Band_Draw: the loop height at 0x10 too | PentagramBand_Draw 452 |
| P71b | Band_Draw: the first height 0x201 when grown (near variant of the equivalent P71) | PentagramBand_Draw 1,146 |
| P72b | Band_Draw: the loop height below 0x11 | PentagramBand_Draw 452 |
| P73 | Band_Draw: 0x200 exactly at i 31 | PentagramBand_Draw 565 |
| P74 | Band_Draw: Rand & 0x7F | PentagramBand_Draw 1,761 |
| P75 | Band_Draw: the rim radius 305 | PentagramBand_Draw 1,929 |
| P76 | Band_Draw: the height read before the Sin | PentagramBand_Draw 37 |
| P77 | Band_Draw: the foot red 2 | PentagramBand_Draw 1,929 |
| P78 | Band_Draw: the top red 0xC1 | PentagramBand_Draw 1,929 |
| P79 | Band_Draw: the quad linked with dy 1 | PentagramBand_Draw 1,929 |
| P80 | Band_Draw: sorted at the near rim point | PentagramBand_Draw 1,929 |
| P81 | Band_Draw: the closing commit to slot 5 | PentagramBand_Draw 2,000 |
| P82 | Band_Draw: the top colour byte from the shade x 3 | PentagramBand_Draw 1,929 |
| P83 | Band_Draw: the far top not cleared | PentagramBand_Draw 176 |
| P84 | Sprite_Run: the effects table 0x8E3581 | PentagramSprite_Run 2,000 |
| P85 | Sprite_Run: the battle table not put back | PentagramSprite_Run 2,000 |
| P86 | Sprite_Run: tpage 0xB6 | PentagramSprite_Run 2,000 |
| P87 | Sprite_Start: raised at +0xB 1 | PentagramSprite_Start 960 |
| P88 | Sprite_Start: +0x2A 0 | PentagramSprite_Start 1,005 |
| P89 | Sprite_Start: +0 bit 0x10 | PentagramSprite_Start 1,511 |
| P90 | Sprite_Start: sound for +0xB 1 | PentagramSprite_Start 984 |
| P91 | Sprite_Start: grey 0xC1 in blue | PentagramSprite_Start 2,000 |
| P92 | Sprite_Start: the +4 raise 0x1800000 | PentagramSprite_Start 1,005 |
| P93 | Animate: freed past 3 | PentagramSprite_Animate 196 |
| P94 | Animate: +0x2A not masked | PentagramSprite_Animate 453 |
| P95 | Animate: sound 0x104 at +9 2 | PentagramSprite_Animate 201 |
| P96 | Animate: no screen update while the script runs | PentagramSprite_Animate 644 |
| P97 | Animate: the owner count down | PentagramSprite_Animate 885 |
| I1 | Ink_Task: the wait and the end swapped | Ink_Task 1,340 |
| I2 | Ink_Start: delays step 3 | Ink_Start 2,000 |
| I3 | Ink_Start: parameter 0x6D | Ink_Start 2,000 |
| I4 | Ink_Start: the CLUT entries from 0x1A20 | **NOT REFUSED (exit 0)** |
| I4b | Ink_Start: the CLUT entries to 0x1A30 (near variant of the equivalent I4) | Ink_Start 2,000 |
| I5 | Ink_Start: entry 0x1A20 kept | Ink_Start 2,000 |
| I6 | Ink_Start: target flags 0x20 | Ink_Start 2,000 |
| I7 | Ink_Start: the child +0xB not set | Ink_Start 1,999 |
| I8 | Ink_Start: the flags before the sound | Ink_Start 2,000 |
| I9 | InkPuff_Run: drawn with +2 0 | InkPuff_Run 373 |
| I10 | PuffPlace: dz from the third dword | InkPuff_Start 662 |
| I11 | PuffPlace: +0xC not read again after the turn | InkPuff_Start 662 |
| I12 | PuffPlace: the source read again after the turn | InkPuff_Start 14 |
| I13 | PuffPlace: +0xA not cleared | InkPuff_Start 662 |
| I14 | InkPuff_Start: direction from +9 of the source | InkPuff_Start 655 |
| I15 | DrawPuff: radius x 3 | Ink_DrawPuff 1,985 |
| I16 | DrawPuff: grey x 7 | Ink_DrawPuff 1,962 |
| I17 | DrawPuff: corners 3 and 4 swapped | Ink_DrawPuff 2,000 |
| I18 | DrawPuff: the radius as unsigned | Ink_DrawPuff 6 |
| I19 | DrawPuff: CLUT x 0x21 | Ink_DrawPuff 2,000 |
| I20 | DrawPuff: v 0x41 at the far corners | Ink_DrawPuff 2,000 |
| I21 | DrawPuff: the second link dy 3 | Ink_DrawPuff 2,000 |
| I22 | DrawPuff: the packet read before the link | Ink_DrawPuff 2,000 |
| I23 | DrawPuff: the radius word read once, before the loop | Ink_DrawPuff 9 |
| I24 | DrawPuff: tpage y 0xF0 | Ink_DrawPuff 2,000 |
| J1 | InkInk_Task: the walk over 63 | InkInk_Task 1,002, Magic213_Task 994 |
| J2 | Walk: the owner not put back | InkInk_Task 1,627, Magic213_Task 1,621 |
| J3 | Walk: records with bit 1 | InkInk_Task 2,000, Magic213_Task 2,000 |
| J4 | Walk: the task saved before the phase | InkInk_Task 62 |
| J5 | ClearPool: +2 kept | InkInk_Start 2,000, Magic213_Start 2,000 |
| J6 | InkInk_Start: +9 7 | InkInk_Start 1,865 |
| J7 | SpawnOnActors: enemies 3..9 | InkInk_Start 985, Magic213_Start 1,023 |
| J8 | SpawnOnActors: the side bit 0x80 | InkInk_Start 985, Magic213_Start 1,023 |
| J9 | SpawnOnActors: delay x 8 | InkInk_Start 1,097, Magic213_Start 1,130 |
| J10 | SpawnOnActors: the task read before the create | InkInk_Start 124, Magic213_Start 113 |
| J11 | SpawnOnActors: the out actors too | InkInk_Start 1,961, Magic213_Start 1,961 |
| J12 | InkInkActor_Run: by +1 | InkInkActor_Run 967 |
| J13 | ActorRecord: the enemy stride 0x12C | InkInkActor_Start 304, Magic213Actor_Start 285 |
| J14 | ActorRecord: a member from 0x802D44 | InkInkActor_Start 260, Magic213Actor_Start 359 |
| J15 | InkInkActor_Start: the sound after the copy | InkInkActor_Start 50 |
| J16 | InkInkActor_Start: delays 1, 3 | InkInkActor_Start 658 |
| J17 | InkInkActor_Start: flags on the target byte | InkInkActor_Start 637 |
| J18 | SpawnRecords: a full pool not skipped | InkInkActor_Start 38, Magic213Actor_Start 74 |
| J19 | SpawnRecords: +0xB from 1 | InkInkActor_Start 658, Magic213Actor_Start 688 |
| J20 | SpawnRecords: the task read before the alloc | InkInkActor_Start 51, Magic213Actor_Start 118 |
| J21 | InkInkPuff_Start: direction from the task | InkInkPuff_Start 602 |
| J22 | InkInkPuff_Start: dz not read again after the turn | InkInkPuff_Start 698 |
| J23 | InkInkPuff_Start: the height row + 4 | InkInkPuff_Start 654 |
| J24 | InkInkPuff_Fade: +9 not counted | InkInkPuff_Fade 1,997 |
| J25 | InkInkPuff_Fade: the owner count kept | InkInkPuff_Fade 662 |
| J26 | PoolAlloc: bit 1 marked | InkInkPuff_Alloc 786, Magic213Mote_Alloc 814 |
| J27 | PoolAlloc: one record short | InkInkPuff_Alloc 9, Magic213Mote_Alloc 10 |
| J28 | InkInkPuff_Run: drawn with +2 0 | InkInkPuff_Run 346 |
| J29 | InkInk_Start: the CLUT without STP | InkInk_Start 2,000 |
| J30 | InkInkActor_Start: the side bit read after the sound | **NOT REFUSED (exit 3221225477)** |
| J30b | InkInkActor_Start: only the side bit read after the sound (+4 before) | InkInkActor_Start 26 |
| X1 | Magic213_Task: its entries swapped | Magic213_Task 2,000 |
| X2 | Magic213_Task: the walk to 0x4F4EC0 | Magic213_Task 1,993 |
| X3 | Magic213_Start: parameter 0x6C | Magic213_Start 1,644 |
| X4 | Magic213_Start: the child +0xB written | Magic213_Start 1,634 |
| X5 | Magic213_Start: the CLUT with STP | Magic213_Start 2,000 |
| X6 | Magic213_Start: sound 0x101 | Magic213_Start 2,000 |
| X7 | Magic213Actor_Run: three entries | Magic213Actor_Run 515 |
| X8 | Actor_Start: +9 0xF | Magic213Actor_Start 658 |
| X9 | Actor_Start: motes step 3 | Magic213Actor_Start 688 |
| X10 | Actor_Start: the tint (0, 0, 1, 1) | Magic213Actor_Start 688 |
| X11 | Actor_Start: the tint kept in +0xB | Magic213Actor_Start 688 |
| X12 | Actor_Start: the release skipped | Magic213Actor_Start 688 |
| X13 | Actor_Start: the height not copied | Magic213Actor_Start 688 |
| X14 | TintUp: two colour bytes | ActorFx_TintUp 2,000 |
| X15 | TintUp: +2 on at 1 | ActorFx_TintUp 1,348 |
| X16 | Untint: ended on the third byte | Magic213Actor_Untint 507 |
| X17 | Untint: flag 0x40 on the target byte | Magic213Actor_Untint 499 |
| X18 | Untint: the record not released | Magic213Actor_Untint 502 |
| X19 | Mote_Run: the table not switched | Magic213Mote_Run 2,000 |
| X20 | Mote_Run: updated whatever +2 | Magic213Mote_Run 505 |
| X21 | Mote_Run: the table put back before the update | Magic213Mote_Run 571 |
| X22 | Mote_Start: angle (+0xB & 3) x 0x200 | Magic213Mote_Start 335 |
| X23 | Mote_Start: radius 13 | Magic213Mote_Start 680 |
| X24 | Mote_Start: the angle not read again for the Cos | Magic213Mote_Start 23 |
| X25 | Mote_Start: raised 0x2000000 | Magic213Mote_Start 680 |
| X26 | Mote_Start: animation % 4 | Magic213Mote_Start 523 |
| X27 | Mote_Start: +2 on before the animation | Magic213Mote_Start 32 |
| X28 | Mote_Start: +0x2B 0 | Magic213Mote_Start 680 |
| X29 | Mote_End: the owner count kept | Magic213Mote_End 1,294 |
| X30 | Mote_End: freed while the script runs | Magic213Mote_End 2,000 |

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
