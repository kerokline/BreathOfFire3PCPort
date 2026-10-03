# Group E4C: effect kinds 0x8D, 0x8E, 0x8F, 0x90, 0x93 and 0x99

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..13),
wave four, from the round branch's tip `89c25e1`. **50 functions ours**
(`src/game/effect_4c.cpp`, shadow name `effect_4c`): the cut table's 49 rows
for E4C (`analysis/round13_cut.tsv`, the band `0x48B200..0x48C7F0`) and one
start no list of the cut has - kind 0x8E's dispatcher `0x48B300`,
`Effect_KindHandlers[0x8E]` (section 5). Each read to its last instruction
with capstone and fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
200,000 rounds, 0 mismatches; 58 of 61 controls refused, the other three equivalent mutants whose near variants are refused. **Fuzz only**: no recorded
route enters any of the 50 (section 9). Every row is effect code: the four
`hypothesis` rows of the cut (`0x48B850`, `0x48B870`, `0x48BFB0`, `0x48C0E0`)
are kind 0x8E's dot pool and kinds 0x93's and 0x99's dispatchers - taken.

| Kind | Functions | Reached through |
|---|--:|---|
| 0x8D: a camera pull - `Camera_Distance` 0x1C80, a full-screen tint (E4D's `0x48CA90`) held, then drawn in by 0x2A a frame as the tint fades; its end steps the byte after `MoveScript_Var7` | 5 | `Effect_KindHandlers[0x8D]` (`0x655584`), `EffectKind8D_States` `0x654FA4` (4) |
| 0x8E: two pools of 128 falling particles - flat triangles ("chips", 0x28 at `EffectKind30_Shards`) and one-pixel tiles ("dots", 0x24 at `0x92D380`) - set up in three boxes and thrown, bouncing off a floor | 11 | `Effect_KindHandlers[0x8E]` (`0x655588`), `EffectKind8E_States` `0x654FB4` (3) |
| 0x8F: sixteen shards spawned at fixed cells from two tables - three, then two every fifteen frames, then by five stages of a frame word - each brightening, then fading, drifting on the ground | 15 | `Effect_KindHandlers[0x8F]` (`0x65558C`), `EffectKind8F_States` `0x655080` (9) |
| 0x90: thirty-two shards emitted one every fourth frame at the record's point, each tagged with the record's `+6` so several records share the pool | 9 | `Effect_KindHandlers[0x90]` (`0x655590`), `EffectKind90_States` `0x6550B4` (3) |
| 0x93: E3C's kind 0x74 column at grey 0xC0, the shard pool cleared first | 5 | `Effect_KindHandlers[0x93]` (`0x65559C`), `EffectKind93_States` `0x6550C0` (4) |
| 0x99: a disc of sixteen triangles in four fans on the ground a unit back from the record's point, its radius growing for ten frames | 5 | `Effect_KindHandlers[0x99]` (`0x6555B4`), `EffectKind99_States` `0x6550D0` (4) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`; the six dispatchers, the clears, the finder and the one-line
states `evidence`). "Pull", "tint", "chips", "dots", "shards", "column",
"disc" name the code's shape - the primitives it commits and the cells it
steps - not a play-tested fact: where the game shows these kinds and what they
look like was not traced (section 9; the owner's word, not this doc's).
**Spawners found** (a raw scan of the image for `mov byte [reg + 5], kind`,
and a grep of `src/game`): area 134's handlers 12..15 (`area_w3c.cpp`:
`Area134_Effect90AtObject` kind 0x90 with `+1` 0, `Area134_Effect93AtObject`,
`Area134_Effect99AtObject`) - and chapter fourteen's runs
(`scena_sc13.cpp`): `Scena14_Run4` step 0xF stores kind 0x8D, `Scena14_Run5`
step 0xC kind 0x8E (`0x566255`). No spawner of kind 0x8F was found by either
(scena_sc13's control Z7 plants it, which is not a spawner); it is likely
stored by data (an event script's effect op).

## 1. What each function does

Every function runs with `Sprite_Current` an `Effect_Objects` record
(`Effect_RunObjects`) or is a cdecl helper called from one; `+1` is the kind's
state, `+9` a frame count, `+0x34 / +0x38 / +0x3C` the record's point (x, z,
height as 16.16). `Sprite_Current` is read again wherever the original reads
`[0x937F88]` again.

### 1.1 Kind 0x8D (`EffectKind8D_Run` `0x48B200`)

- `EffectKind8D_Run` `0x48B200` - `jmp [EffectKind8D_States + +1 * 4]`, unbounded.
- `_Start` `0x48B220` (state 0) - `Camera_Distance` 0x1C80, `MapView_Redraw`
  2, the byte `+0x48` of the three `ObjTrio` records (`0x802D88`, `0x802ED4`,
  `0x803020`) 1; the tint `+0x5D..+0x5F` 0x7F, `+9` 0x96, `+1` = 1.
- `_Hold` `0x48B280` (state 1) - a tail jump to E4D's tint `0x48CA90`. Nothing
  in the band moves `+1` from 1 to 2: whatever does is outside it (the
  spawning run's next steps, not traced).
- `_Pull` `0x48B290` (state 2) - `+9` down; not 0: `Camera_Distance` - 0x2A
  and, while `+0x5D` is not 0, the tint's three bytes down one; at 0:
  `Camera_Distance` 0x400, `+1` = 3. `MapView_Redraw` 2 and the tint.
- `_End` `0x48B2F0` (state 3) - the byte `0x8034E5` (after `MoveScript_Var7`;
  other groups' `kVar7Step`) up, `Effect_Release`.

### 1.2 Kind 0x8E (`EffectKind8E_Run` `0x48B300`)

- `EffectKind8E_Run` `0x48B300` - the dispatcher the cut does not list.
- `_Start` `0x48B320` (state 0) - both pools cleared (`_ClearChips`,
  `_ClearDots`); three boxes, each set up in both pools over its slice of the
  128 records (0..0x1F, 0x20..0x5F, 0x60..0x7F; x 0x20000..0x50000..0xA0000..
  0xD0000, z 0x770000, height 0xFF000000..0x2000000 - two stack vectors, all
  six dwords written again before each box); sound 0x207, `+2` 0, `+1` up.
- `_Fall` `0x48B450` (state 1) - the first time (`+2` 0) `Cond_ByteFE` 1 and
  `MoveCmd_TestFB(2, 0x77)` (its answer unused), `+2` up; the map camera; both
  pools moved; neither live: `+1` up (state 2 is `Effect_StateRelease`).
- `_MoveChips` `0x48B4A0` / `_MoveDots` `0x48B730` - each live record falls:
  x and z on by their speeds, the height on by its speed, the speed down
  0x40000; below the floor 0xFE000000 the height held there and the new speed
  negated (a bounce). `+3` down 2, a chip's shape `+4` `Rand & 7`, the life
  `+2` down (0 frees it), drawn. Answer al: 1 when any was live.
- `_DrawChip` `0x48B530` - E3C's `EffectKind6D_DrawParticle` byte for byte but
  the first link's dy (-2, that one 0) and the shape table (`0x654FC0`).
- `_DrawDot` `0x48B7C0` - a draw mode linked at the dot; a `TILE_1`
  (`Gpu_SetTile1`) semi-transparent at its projected point, colour (0, `+3`,
  `+3`), linked (dy -2, 0x14).
- `_InitChips` `0x48B660` / `_InitDots` `0x48B870` `(first, last, lo, hi)` -
  records first..last - 1 (low words; none when first >= last): x and height
  `((Rand & 0xFFF) << 8 / << 16) % (hi - lo) + lo` by `idiv` (lo read before
  the `Rand`, hi after), z `lo[1]`, the speeds, `+0` 1, `+1` 0, `+3` 0x48, the
  life `(Rand & 7) + 0x18`, a chip's shape `Rand & 7`.
- `_ClearChips` `0x48B640` / `_ClearDots` `0x48B850` - `+0` 0 in the 128.

### 1.3 Kind 0x8F (`EffectKind8F_Run` `0x48B930`)

States 0..8: `_Start` (the sixteen shards cleared), `_Burst` (rows 6, 9, 0xD
spawned, `+9` 0x3C), `_Rise` (moved for 0x3C frames), `_Arm` (`+9` 0x96),
`_Trickle` (two shuffled spawns every fifteenth count, moved), `_Settle`
(moved until none live), `_ResetClock` (`+0x2E`, `+0x30` 0), `_Sequence`,
`_Fade` (moved; none live: `Effect_Release`). Each steps `+1` by one when its
work is done.

- `_Sequence` `0x48BA70` (state 7) - the frame word `+0x2E` (s16) at 0x1A4
  or more: `+1` up. Below: the stage byte `0x676290` by it (below 0x3C 0,
  0xB4 1, 0x12C 2, 0x168 3, else 4); the wait word `+0x30` 0: `0x6550A4[stage]`
  shards spawned shuffled and the wait `0x6550AC[stage]` (the stage read back
  from its cell for each), else the wait down; the frame word up. A tail jump
  to the move.
- `_SpawnShard` `0x48BB40` `(row byte)` - a free shard (E3C's
  `EffectKind6E_FindShard`); `+3` 0x20, the size `+4` 0x80, x / z the row of
  `0x655000` << 8, the speeds the row of `0x655040` << 5.
- `_SpawnShuffled` `0x48BBB0` `(count byte)` - rows 0..15 in a local,
  shuffled by sixteen swaps of two `Rand & 0xF`; the first count spawned.
- `_MoveShards` `0x48BC60` - the map camera; each live shard by its phase
  `+1`: 0 brightens (`+3` up 4) for 16 frames, then 1 fades (down 4) and is
  freed at frame 0x20; x / z on by their speeds, the size up 8, the height
  `AreaMap_Elevation << 16`; drawn. Answer al.
- `_DrawShard` `0x48BD10` - a sized `POLY_FT4`: the point projected, the size
  scaled at its depth (`EffectGte_ProjectSize`), the corners on the x87 - the
  shape of E3C's `EffectKind73_DrawSpark` (that one with a draw mode first, its
  point at `+4`, size at `+0x14`, page abr 2); colour (`+3`,
  `(+3 >> 2) * 3`, `+3 >> 1`); linked (dy 3, 0x48).
- `_ClearShards` `0x48BC40` - `+0` 0 in the sixteen.

### 1.4 Kind 0x90 (`EffectKind90_Run` `0x48BF00`)

- `_Start` `0x48BF20` (state 0) - when the tag `0x676294` is 0 (only before
  the first kind-0x90 record of the session: nothing else writes it) the 32
  shards cleared; `+9` 0x80; the tag up, its low byte to `+6`.
- `_Emit` `0x48BF60` (state 1) - every fourth count a shard emitted
  (`_EmitOne`); `+9` down, at 0 `+1` up; a tail jump to the move.
- `_Fade` `0x48BFA0` (state 2) - moved; none of this record's live:
  `Effect_Release`.
- `_EmitOne` `0x48C1C0` - a free shard (`_FindShard`): `+0` the record's tag,
  `+3` 0x80, size 0x80, at the record's x / z, rising speed `+0x18` 0x1800.
- `_FindShard` `0x48C220` - the first of 32 whose `+0` is 0, or null.
- `_MoveShards` `0x48C260` - the map camera; each shard whose `+0` is
  `Sprite_Current`'s `+6` (read again for each) by its phase through a bounded
  four-case switch (`0x48C354`): growing, fading, shrinking twice (freed at
  frame 0x20); moved, the size up 8, the height from the ground, drawn. Answer al.
- `_DrawShard` `0x48C370` - kind 0x8F's quad, colour `+3` in all three,
  committed (`Gfx_CommitPrim(1, 0x48)`), not linked.
- `_ClearShards` `0x48C240` - `+0` 0 in the 32 (kind 0x93's start too).

### 1.5 Kind 0x93 (`EffectKind93_Run` `0x48BFB0`)

E3C's kind 0x74 state for state: `_Start` `0x48BFD0` is `EffectKind74_Start`
with `EffectKind90_ClearShards` first and sound 0x208 (that one 0x207);
`_Rise` `0x48C040` and `_Fade` `0x48C0A0` are `_Rise` and `_Fade` with the
draw `EffectKind93_Draw` `0x48C550` - `EffectKind74_Draw` at grey 0xC0 (that
one 0x80); the instructions differ only in register choice and store order
(a diff of the two listings). State 3 is `Effect_StateRelease`. Ours shares
one body (`Column`) between them in `effect_4c.cpp`; E3C's is left as it is.

### 1.6 Kind 0x99 (`EffectKind99_Run` `0x48C0E0`)

- `_Start` `0x48C100` (state 0) - the centre `+0xC` / `+0x10` the record's
  point less 0x10000 each, its height `+0x14` the ground's, the radius `+0x1C`
  0; `+9` 0xA.
- `_Spread` `0x48C160` (state 1) - the disc drawn, the radius up 0x1199; `+9`
  down, at 0 sound 0x20F and `+1` up (state 2 `Effect_StateRelease`).
- `_DrawDisc` `0x48C7A0` `(point, radius)` - the map camera; four fans at the
  angles 0, 0x400, 0x800, 0xC00, each linked at the point offset by the
  dwords of `0x6550E0` / `0x6550F0` (1 or -1: a unit of the 16.16 fraction).
- `_DrawFan` `0x48C7F0` `(point, radius, angle, dx, dz)` - the centre and the
  edge at the angle projected; four flat triangles (`0x5A7570`) of 0x100 each,
  grey 0x80, semi-transparent (page (0x380, 0x100), abr 2), each linked twice
  at the offset cell. The angle's argument slot is stepped in place.

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed (`DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no address of
the band nor its tables; checked 2026-10-03). Where the original indexes past
a table or pool, reads its own frame past a local or faults, ours aborts with
a `Fatal` naming the function (the round-nine rule; nothing in the fuzz
reaches it): the six dispatchers past their tables; `_DrawChip` past its eight
shapes; `_InitChips` / `_InitDots` past the 128 records and on a box whose
corners are equal (`idiv` by 0); `_SpawnShard` past its sixteen rows;
`_SpawnShuffled` past its sixteen-entry local; `_Sequence` with a stage byte
past five. No path of the game reaches any of them as far as the band's code
shows: every writer of `+1` steps it inside its table, `+4` is `Rand & 7`,
`_Start`'s three boxes are 0..0x80 with spans of 0x30000..0x50000, the rows
come from fixed pushes and the shuffle, the counts from `0x6550A4` (at most 4)
and the stage from its own compare.

The x87 is used as the original uses it (inline `fild` / `fadd` / `fsubr` /
`fiadd` / `fld`-`fst`-`fstp`) under the game's own control word.

## 3. The tables

**The state tables** (`symbols.toml` `[[data]]`): each the table's own length
to the next table a dispatcher indexes, or to the first dword that is not code
- checked by a raw scan of the image for every table address (each named by
exactly one `jmp [eax * 4 + T]`, its dispatcher's) and against what the
states store into `+1`:

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind8D_States` `0x654FA4` | 4 | `0x654FB4`, kind 0x8E's (the tool's run from E4B's `0x654F38` reads on into both: the cut's "T654F38 / kind 0x8B" hint for 0x48B220..0x48B7C0 is that run's, not the code's) |
| `EffectKind8E_States` `0x654FB4` | 3 | `0x654FC0`, the chip shapes (data); its third `Effect_StateRelease` |
| `EffectKind8F_States` `0x655080` | 9 | `0x6550A4`, the stage bytes (data) |
| `EffectKind90_States` `0x6550B4` | 3 | `0x6550C0`, kind 0x93's (the tool's run says 11) |
| `EffectKind93_States` `0x6550C0` | 4 | `0x6550D0`, kind 0x99's; its fourth `Effect_StateRelease` |
| `EffectKind99_States` `0x6550D0` | 4 | `0x6550E0`, the fan offsets (data); its third and fourth `Effect_StateRelease` |

**The data the band reads in place** (raw constants in `effect_4c_callees.h`,
not named): the chip shapes `0x654FC0` (eight rows of four s16), the shard
cells `0x655000` and speeds `0x655040` (sixteen rows of two s16 each), the
stage counts `0x6550A4` and waits `0x6550AC` (bytes, five used of eight), the
fan offsets `0x6550E0` / `0x6550F0` (four dwords each).

**The cells**: kind 0x8F's stage byte `0x676290` and kind 0x90's tag
`0x676294` (only this band's code names either: a scan of the image for the
two addresses finds the band's nine sites), kind 0x8E's dot pool `0x92D380`
(E2A's kinds 0x2C / 0x2E keep their centre there - section 7).

## 4. The fuzz (`effect_4c_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_4c`, effect mode (`g.effect`; kinds
0x8D, 0x8E, 0x8F, 0x90, 0x93, 0x99, each clone its own), 4,000 rounds a
function (`BOF3X_E4C_ONLY=<name>` runs the clones whose name holds it).
Shapes: 41 `kEffect` (the six dispatchers' `state_span` their table's length,
section 3), nine `kCall` (the draws handed a record of their pool, the inits
handed a slice and two corners in the harness's scratch, the spawners a row or
count over garbage above the byte, the disc and fan the record's `+0xC`).
`ret_mask` 0xFF on the four movers (al), 0xFFFFFFFF on `_FindShard` (eax).

**Stand-ins** (`kCallees`): the group's own by name (the movers `kFlag`, the
clears / emit / draws `kPhase` or hashed through their record, the two inits'
corners hashed 12 bytes with the pointers not compared - they are the caller's
locals - and their first and last masked to the word, the spawn row and count
to the byte, the fan's angle to the word: each the width the callee reads);
E3C's `EffectKind6E_FindShard` and our `_FindShard` answering the first free
of their 16 / 32 or null (null a quarter of the time); E4D's `0x48CA90` raw,
logging `Sprite_Current` and `+0x5C`; `EffectGte_ProjectPoint` re-listed with
E3C's fill (one value in eight a NaN, quiet or signalling, or past 2^63),
because `_DrawChip` copies the depth through the FPU (which quiets a
signalling NaN) and the others by `mov` (control 6 shows the difference). The
other callees are the effect-standard rows unchanged
(`EffectGte_ProjectSize`: every caller here writes both words of the size).

**Regions** beyond effect mode's: the two cells `0x676290..0x676297`, and kind
0x8E's pools past the standard 0x644 bytes of `EffectKind30_Shards`, to
`0x92E580`.

**Seeds** (`Seed`): every record's `+9` at its boundaries (1, 2, 0, 0xF,
0x10, 0x3C, 0x96, 0xFF ...); kind 0x90's `+6` small on every record (the
shards' tags drawn from it, its own and others'); the pools' `+0` free a third
of the time (some rounds full) and the lives at their boundaries; chips' and
dots' heights at and around the floor with speeds round 0x40000; the chips'
shape below eight for the draw; kind 0x8F's phases 0..2 and frames at 0xF /
0x10 / 0x1F / 0x20; `_Sequence`'s frame word at every stage boundary (0x3B /
0x3C .. 0x1A3 / 0x1A4, negative) and its wait at 0 / 1; kind 0x90's tag 0 or
not, its shards' phases 0..4 and 0xFF and frames at each case's limit; kind
0x93's top round its cap and its column a few steps tall (E3C's seed of the
twin: a random height runs the step loop to 4,096); the inits' corners never
equal. **Disturbance** (`Disturb`, from the hash only): `+9`, the tint, the
stage byte (inside its five), `+6`, the tag, the two clock words, kind 0x93's
top near its foot, kind 0x99's centre - each a byte or word the states read
again after a call.

**Result, in this worktree**: 200,000 rounds over 50 functions, 2,864,833
calls to the stand-ins, **0 mismatches**; 32,888 bytes of state in 47 regions.
Every entry of the six tables was reached (`phase` lines of the coverage:
428..1,377 a state; `Effect_StateRelease` 4,306).

**Under `BOF3X_SHADOW='*'`** (this worktree, at this tip): exit 0, `inject:
7837 ours, 0 left original`, 1,001 self-test summary lines, every one 0
mismatches (effect_4c's own: 200,000 rounds, 2,887,420 calls - its stream
differs from the lone run's); and again with `BOF3X_WIDE=1`: exit 0, the same
1,001 lines, 0 mismatches. Neither run died silently.

## 5. What the cut and the tool said, settled

- **Added: `0x48B300`**, kind 0x8E's dispatcher - `Effect_KindHandlers[0x8E]`
  (`0x655588`) points at it, it sits between the cut's `0x48B2F0` and
  `0x48B320`, and the catalog lists it (`remaining_catalog.tsv`, hidden) but
  the cut does not. In the band, so this group's (the second addendum).
- **No start dropped**: `band_rows.py` printed no "code no list has" row and
  no conditional jump into another's code; every extent the tool read is the
  code's (28 differ from the cut's sizes by padding only).
- **One jump table inside a clone**: `_MoveShards` `0x48C260`'s four-case
  switch, its table `0x48C354..0x48C363` inside the extent 0x104 (the clone
  moves it, `kTables48C260`).
- **The four `hypothesis` rows** are effect code (section 1): taken.
- **The cut's `unit` / `label` columns**: rows `0x48B220..0x48B7C0` say
  "Fn_48AB30 kind 0x8B, T654F38" - the tool reached them as entries 27..32 of
  the run at `0x654F38` (E4B's kind 0x8B table), which runs on into kind 0x8D's
  and 0x8E's tables; the code says kinds 0x8D and 0x8E (section 3). E4B's
  table count is E4B's to settle.
- **Hosts**: thirteen starts are hidden in E4B's `0x48A8E0` (its catalog
  extent 0xBBB runs to `0x48B49B`), nine in `0x48B870` (catalog 0x2CF), eight
  in `0x48BD10` (catalog 0x4A3); the hosts' true ends are their `ret`s
  (section 11).

## 6. Controls

Planted one at a time by a scratch script (`controls.py`: each plant anchored
on a unique string of `effect_4c.cpp`, rebuilt, run under
`BOF3X_E4C_ONLY=<filter>`, the file restored and rebuilt at the end; the
committed file has no switch). Counts are rounds refused of 4,000 per function
run, in this worktree; every refused run exited 3.

**58 of 61 refused** (56 by a count; 27 and 34, a dispatch through the wrong
table, mismatch and then reach ours' abort past the planted table). The three
not refused are equivalent mutants, each with a near variant that is refused:
9 (the two `Rand` draws of a swap exchanged - a swap is symmetric; 26 refused),
24 (x and z of the emitted shard read in the other order - no call between the
reads; 25, the tag read before the finder, refused) and 56 (`Cond_ByteFE` set
after `MoveCmd_TestFB` - the callee never reads it, by its listing
`0x572650..0x572789`; 61, the value 2, refused). Controls 13, 17, 19 and 25
(a cell read before or after a call) are refused only in the rounds where the
harness's disturbance moved `Sprite_Current` or the cell after that call
(29..130 of 4,000); control 46 (the finder's 32nd record never looked at) in
2, the rounds whose 31 first records were all in use.

| # | Run (`_ONLY`) | Plant | Refused |
|--:|---|---|---|
| 1 | `EffectKind8D_Pull` | the distance - 0x2B | 3718 |
| 2 | `EffectKind8D_Start` | `+1` stepped, not set to 1 | 3984 |
| 3 | `EffectKind8D_End` | `MoveScript_Var7` stepped, not the byte after | 4000 |
| 4 | `EffectKind8E_Run` | through kind 0x8D's table | 4000 |
| 5 | `EffectKind8E_MoveChips` | the bounce negates the old speed | 4000 |
| 6 | `EffectKind8E_DrawChip` | the depth copied by `mov` (no NaN quieting) | 85 |
| 7 | `EffectKind8E_InitChips` | first masked to 17 bits | 888 |
| 8 | `EffectKind8E_MoveDots` | shade - 4 | 4000 |
| 9 | `EffectKind8F_SpawnShuffled` | the two `Rand` draws of a swap in the other order | **not refused** (exit 0, equivalent) |
| 10 | `EffectKind8F_SpawnShard` | the z speed << 4 | 2121 |
| 11 | `EffectKind8F_MoveShards` | freed at frame 0x1F | 1959 |
| 12 | `EffectKind8F_Sequence` | stage 4 from 0x167 | 247 |
| 13 | `EffectKind8F_Sequence` | the wait written through the record read at entry | 54 |
| 14 | `EffectKind90_MoveShards` | the tag read once before the walk | 2428 |
| 15 | `EffectKind90_MoveShards` | case 2 ends at 0x17 | 550 |
| 16 | `EffectKind8F_DrawShard` | green `(+3 * 3) >> 2` | 1984 |
| 17 | `EffectKind90_Start` | the record read before the clear | 51 |
| 18 | `EffectKind93_Draw` | grey 0x80 (kind 0x74's) | 3815 |
| 19 | `EffectKind99_DrawFan` | the point's z read before `Math_Sin` | 29 |
| 20 | `EffectKind99_Start` | z - 0x20000 | 4000 |
| 21 | `EffectKind8F_ResetClock` | the wait cleared as a byte | 3981 |
| 22 | `EffectKind8F_MoveShards` | `AreaMap_Elevation(z, x)` | 4000 |
| 23 | `EffectKind8E_InitDots` | the speed + 0x1000 | 1753 |
| 24 | `EffectKind90_EmitOne` | x and z read in the other order | **not refused** (exit 0, equivalent) |
| 25 | `EffectKind90_EmitOne` | the tag read before the finder | 130 |
| 26 | `EffectKind8F_SpawnShuffled` | the second draw & 7 | 3136 |
| 27 | `EffectKind8D_Run` | through kind 0x8E's table (three) | refused: ours' abort (`+1` past the planted table) after mismatching rounds |
| 28 | `EffectKind8D_Hold` | the tint dropped | 4000 |
| 29 | `EffectKind8E_Start` | the dots' corners swapped | 4000 |
| 30 | `EffectKind8E_Fall` | `||` for `&&` | 1781 |
| 31 | `EffectKind8E_ClearChips` | 127 cleared | 3979 |
| 32 | `EffectKind8E_DrawDot` | the second link dy -1 | 4000 |
| 33 | `EffectKind8E_ClearDots` | the dots stepped by 0x28 | 4000 |
| 34 | `EffectKind8F_Run` | through its table one entry on (eight) | refused: ours' abort (`+1` past the planted table) after mismatching rounds |
| 35 | `EffectKind8F_Start` | `+1` not stepped | 4000 |
| 36 | `EffectKind8F_Burst` | row 0xC for 0xD | 4000 |
| 37 | `EffectKind8F_Rise` | the count test inverted | 4000 |
| 38 | `EffectKind8F_Arm` | `+9` 0x95 | 4000 |
| 39 | `EffectKind8F_Trickle` | every sixteenth | 1974 |
| 40 | `EffectKind8F_Settle` | the test inverted | 4000 |
| 41 | `EffectKind8F_Fade` | the test inverted | 4000 |
| 42 | `EffectKind8F_ClearShards` | fifteen cleared | 3982 |
| 43 | `EffectKind90_Run` | through kind 0x93's table | 4000 |
| 44 | `EffectKind90_Emit` | every eighth | 635 |
| 45 | `EffectKind90_Fade` | the test inverted | 4000 |
| 46 | `EffectKind90_FindShard` | thirty-one looked at | 2 |
| 47 | `EffectKind90_ClearShards` | thirty-one cleared | 3979 |
| 48 | `EffectKind90_DrawShard` | committed to slot 2 | 4000 |
| 49 | `EffectKind93_Run` | through kind 0x99's table | 2974 |
| 50 | `EffectKind93_Start` | sound 0x207 | 4000 |
| 51 | `EffectKind93_Rise` | the cap + 1 | 1712 |
| 52 | `EffectKind93_Fade` | the width step 0xFFF00001 | 3723 |
| 53 | `EffectKind99_Run` | through kind 0x93's table | 2974 |
| 54 | `EffectKind99_Spread` | the radius + 0x1198 | 4000 |
| 55 | `EffectKind99_DrawDisc` | the fans at i << 9 | 4000 |
| 56 | `EffectKind8E_Fall` | `Cond_ByteFE` set after `MoveCmd_TestFB` | **not refused** (exit 0, equivalent) |
| 57 | `EffectKind8D_Pull` | `+0x5E` down 2 | 2780 |
| 58 | `EffectKind8E_MoveChips` | the shape `Rand & 3` | 4000 |
| 59 | `EffectKind90_MoveShards` | the shade - 7 | 2823 |
| 60 | `EffectKind8F_DrawShard` | linked dy 2 | 4000 |
| 61 | `EffectKind8E_Fall` | `Cond_ByteFE` 2 (56's near variant) | 2049 |

## 7. Latent defects (Capcom's, described, not fixed)

- **Kind 0x90's tag wraps onto free shards.** `_Start` hands each kind-0x90
  record the next value of the dword `0x676294` as its tag (`+6`, the low
  byte), and `_MoveShards` / `_EmitOne` treat a shard whose `+0` equals the
  tag as the record's. Nothing resets the dword (a scan of the image: only
  `_Start` names it), so the 256th kind-0x90 record of a session gets tag 0 -
  and every **free** shard (`+0` 0) is then "its own": it moves and draws all
  of them (stale positions, whatever the pool last held), and its own emits
  are written with `+0` 0, so they are free records the next emit may take.
  The pool is also cleared only before the first record of the session (the
  dword is 0 only then). The nearest sensible value is to skip tag 0 (and
  clear the pool when no kind-0x90 record is live); the fix and its ledger
  entry are the owner's. Ordinary play reaching it needs 256 kind-0x90
  records (area 134's handlers 12 / 13 spawn one each) in one session - not
  measured.
- **The pools overlap.** Kind 0x8E's two pools cover `0x92BF80..0x92E580`:
  over the first 0x500 bytes kinds 0x8F, 0x90 and 0x93 use, E3C's kinds 0x6D /
  0x6E / 0x72 / 0x73 and every other user of `EffectKind30_Shards`, and over
  E2A's centre `0x92D380`. Two such kinds live at once trample each other;
  kind 0x93's start clears kind 0x90's shards outright. Whether the game ever
  runs two together was not traced.
- **The six dispatchers do not bound their state bytes**; every writer of
  `+1` in the band steps it inside its table. Kind 0x8D's state 1 is never
  left by the band's code (something outside writes 2).
- **The column's step count is unbounded** (E3C's section 7 for the twin):
  `EffectKind93_Draw` steps 0x100000 at a time from the foot to the top +
  0x1000000 with signed compares; the states keep the top within 0x8000000 of
  the foot.
- **Leftovers**: kind 0x8D sets the party records' byte `+0x48` and never puts
  it back (whoever reads it is outside the band); `_Fall` ignores
  `MoveCmd_TestFB`'s answer; `_DrawFan` steps its own argument slot; kind
  0x8F's stage byte is one cell for every kind-0x8F record (harmless: each
  computes it before reading it).

## 8. Calls across groups

**Outbound, raw** (`band_rows.py --edges`; `SH_AT`, listed in the fuzz):
E4D's `0x48CA90` (kind 0x8D's states 1 and 2, the tint) - E4D is this wave's,
so the coordinator rebinds it after both merge. By address and nobody's:
`0x5A7570` (the flat triangle; an effect-standard row). **By name, already
ours**: E3C's `EffectKind6E_FindShard` (the brief's inbound note: `0x48BB40`
calls `0x4841C0`), EGT's `EffectGte_LoadMapCamera`, `_ProjectPoint`,
`_ProjectSize`, `Effect_Release`, `Sound_PlayEffect`, `AreaMap_Elevation`,
`MoveCmd_TestFB`, `MapView_LinkPrimAt`, `Gfx_CommitPrim`, `Rand`, the `Gpu_*`
and `Math_*` primitives, and FC1's `Effect_StateRelease` through three tables.

**Inbound from outside the group**: none by call. `Effect_RunObjects` reaches
the six dispatchers through `Effect_KindHandlers` (read in place); a raw scan
of the image finds no other cell or immediate holding an address of the band
(each table address once, at its dispatcher; each dispatcher once, in
`Effect_KindHandlers`).

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 50 rows (`0x48B300` included), and no first-call
trace under `analysis/calltrace` names any of the 50 addresses (a grep of
every file but the entries lists). **Fuzz only.** No live run was made (the
brief). A recorded walk through area 134 (kinds 0x90, 0x93, 0x99) or chapter
fourteen's runs 4 and 5 (kinds 0x8D, 0x8E) would let the coordinator's
frame-hash A/B cover them; which scene shows which kind is the owner's to say.

## 10. The rebinding

`grep -rn -i` of the 50 addresses and the six tables in `src/` (and
`band_rows.py --refs`: 0 references): **nothing to rebind** - no file of ours
names an address of the band. The only mentions are E3C's `symbols.toml`
evidence and `docs/effect_3c.md` citing `0x48BB40` as a caller (text, still
right). Left raw here: E4D's `0x48CA90` (section 8).

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): 32 lines, the read extents of the 50 less eighteen already listed right. Three
of them correct host lines left in place: `0048B870 2CF` (the code is 0xBF; the
old line spans kind 0x8F's dispatcher and states after its `ret`),
`0048BD10 4A3` (0x1ED; kinds 0x90, 0x93 and 0x99's) and `0048C7F0 295` (0x195;
the old line runs into E4D's `0x48CA90`). E4B's host line `0048A8E0 BBB` spans
thirteen of this band's starts (to `0x48B49B`); its extent is E4B's to fix.

