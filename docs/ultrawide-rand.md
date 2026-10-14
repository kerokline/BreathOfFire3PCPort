# The map walk's random split (ultrawide round, group RN)

**Status:** BUILT (2026-10-13), branch `wide/rn` off the round tip
`a826c9cb`. Self-tested headless at `BOF3X_WIDE` unset, 1 and 2. Not run
live; nothing here has been seen in play. This builds the owner's call of
2026-10-13 on EB-2's cause 1 ([`ultrawide-census-2.md`](ultrawide-census-2.md)
section 3.1), which main's [`known-defects.md`](known-defects.md) records as
**D283**, "the wide picture changes the random sequence": **split the
random.** Cells the original walks keep Capcom's `Rand`; cells only the
lowered inset adds draw from a generator of ours. DIV-0041's inset lowering
stays as it is (it fixes the owner's bridge notch).

**Numbered at the round's fold-in (2026-10-14):** draft H (section 7) is DIV-0041's 2026-10-13 amendment "the map walk's random split"; the log tag `DIV H` quoted below is now `DIV-0041`; the patch-cell defect is D295.

## 0. The answer

- **The cause, restated.** DIV-0041 lowers `MapView_Build`'s cell inset by
  `widescreen_rule::InsetColumns(c) = ceil(c / 20)` (3 at 16:9, 6 at 21:9).
  `DrawLayer_Open` `0x56FD20` reads that inset and hands the cells of
  `(14 - inset) * 2` columns of each view row to `MapCell_Handlers`. So the
  wide walk hands over cells the original never does. `MapCell_DrawRising`
  `0x570660` makes eight `Rand` calls a cell and has no cull, so each such
  cell moves the CRT stream by 8 a frame (EB-2: whelpBoss 24 against 32 at
  frame 285).
- **The split** (section 2). `Inset()` now notes the inset Capcom's
  arithmetic gives before the lowering. `DrawLayer_Open` works out, per row,
  the columns the original's walk of that row would take, and marks each
  other cell it hands over as a **band cell**. `MapCell_DrawRising` takes
  its eight numbers from `Rand` for an original cell and from a draw-only
  generator of ours for a band cell. That generator is MSVC `rand()`'s
  recurrence on a seed of its own, which nothing else reads. Narrow, and
  under `BOF3X_WIDE_INSET=0`, the two insets are equal, no row is split, and
  every cell calls `Rand` exactly as before.
- **Every consumer a band cell can reach was scanned** (section 3). Only
  `MapCell_DrawRising` draws random numbers. **One other handler pair changes
  game state:** `MapCell_PatchThenStep` / `_PatchThenStop` (kinds `0x24` /
  `0x25`) apply an area patch list, which writes ground heights, map bytes and
  texture words. A band cell applies it at a different frame from narrow.
  That one is **listed, not changed** (section 3.3; the owner's call). The
  rest write draw state and scratch only.
- **Area 2's steam vent (D283):** the eye batch's "puffs" come from the one
  random consumer, `MapCell_DrawRising`, by elimination (section 3.2). That
  is an inference. The vent itself was not traced to its cell.
- **Tested** (section 4). A boundary test at 0, 53 and 120 columns over every
  original inset, input flag, row parity and view column, at 0 mismatches.
  A fuzz of whole frames shows Capcom's `Rand` calls equal to the narrow
  frame's, call for call and cell for cell, at every lowering, at 0
  mismatches. Its control, band cells sent back to Capcom's `Rand`, is
  refused at 53 and 120.

## 1. The original's inset, per frame

**Who reads and writes it.** `MapView_Inset` `0x905D80` has two references in
the image (`uw_rn/xref1.py <exe> 0x905D80`, a byte search of every `.text`
instruction):

- `0x56ECE6` `mov byte [0x905D80], al`, in `MapView_Build` (ours,
  `map_layers.cpp` `Inset()`);
- `0x56FD82` `mov dl, byte [0x905D80]`, in `DrawLayer_Open` (ours,
  `draw_emit.cpp`).

The handler table `MapCell_Handlers` `0x663008` is read only by
`DrawLayer_Open`'s `call [ecx*4 + 0x663008]` (`0x56FE44`). The only other hit,
`0x56E68C` `cmp eax, 0x663008`, is the end bound of the preceding 28-pair
table's search loop. No handler entry has a direct `call` / `jmp` or any
`.data` reference outside the table, except the shared bare `ret`
`0x437CC0` (`uw_rn/callers.py`).

**What changed in `Inset()`.** Its arithmetic moved into
`map_layers::ComputeInset(angle, distance, flags, FE, wide)`. That is a pure
function returning `{original, lowered}`: the value Capcom's steps give, and
the value after DIV-0041's lowering. `Inset()` stores the lowered value in
`Scratch_Swap` and its low byte in `MapView_Inset`, as before. The original
wrote `Scratch_Swap` after each step with no call between the stores, so
only the last store is ever seen. Then it calls
`wide_rand::NoteInset(original, MapView_Inset)`. The walk itself is
untouched: `MapView_Build` and `DrawLayer_Open` visit exactly the cells they
visited before. `map_layers`' own `MapView_Build` fuzz (which compares
`Scratch_Swap` and `MapView_Inset`) runs before the wide columns are set, and
passes at 0 mismatches (section 4).

**Per cell.** `MapView_Build` runs only on a redraw frame, and
`MapView_Inset` persists between them, so the noted pair is "this frame's" as
long as the byte `DrawLayer_Open` reads is the one `Inset()` wrote.
`wide_rand::Row` (built once per `DrawLayer_Open` call, from the same
`MapView_Inset`, `Field_InputFlags`, row and `MapView_Column` the walk uses)
repeats `DrawLayer_Open`'s arithmetic on the noted original inset:

- less 2 (not below 0) with input flag bit 0;
- first column `MapView_Column + inset + (1 for an even row)`, less `0x1C`
  once;
- `(14 - inset) * 2` columns, each advanced before it is read, `0x1B` to 0.

A cell is a band cell when its column is not among those. The row is not
split at all when no build has noted a pair, when the byte read is not the
one last written (`DrawLayer_Open`'s own fuzz, for one), or when the two
insets are equal (narrow, or `BOF3X_WIDE_INSET=0`). The test is by column set
rather than by position, so it holds for any view column the game stores. In
range, it is the same as "the first and last `d` cells of the wide walk",
where `d` is the difference of the two effective insets, and the boundary
test checks both (section 4).

`DrawLayer_Open` sets `wide_rand::SetBand(row.Band(column))` before a cell's
records go to their handlers, and clears it after the row. `MapCell_DrawRising`
asks `wide_rand::Band()` before each of its eight numbers
(`map_cells::RisingRand`).

## 2. What changed, file by file

| File | Change |
|---|---|
| `src/game/wide_rand.{h,cpp}` (new) | The noted pair, `Row`, the band flag, the draw-only generator `DrawOnly` (seed 1, `seed * 0x343FD + 0x269EC3`, bits 16..30: `Rand` `0x5B93D2`'s recurrence on a state of its own), the self-test (`BOF3X_SHADOW=wide_rand`) and the log line `DIV H`. |
| `src/game/map_layers.{h,cpp}` | `Inset()` through `ComputeInset`; `NoteInset` after the store. |
| `src/game/draw_emit.cpp` | `DrawLayer_Open`: the `Row`, and the band flag around each cell's records. Its walk, its reads and its order are unchanged. |
| `src/game/map_cells.{h,cpp}` | `DrawRising`'s `Rand` through `map_cells::RisingRand`; `SwapRand` for the self-test. The live shadow (`BOF3X_SHADOW=map_cells` in play) checks a band cell against a third clone of `0x570660`, whose `Rand` call (`+0x93`) is aimed at `DrawOnly`, and compares both seeds. |
| `src/hook/inject_all.cpp` | `wide_rand::Inject()` after `Widescreen_Inject`. |

## 3. Every consumer a band cell can reach

### 3.1 The scan

`uw_rn/cell_reach.py <exe> <symbols.toml>` reads the 77 entries of
`MapCell_Handlers`, which are 17 distinct functions. For each one it follows
every direct `call` and tail `jmp` by recursive descent, and lists:

- whether the function reaches `Rand` `0x5B93D2`;
- every store to an absolute address of `0x600000` or above;
- every indirect call or jump.

`uw_rn/cell_reach2.py` adds each handler body's stores through a register.
Output: `uw_rn/cell_reach.txt`, `cell_reach2.txt`. `MapView_Build`
`0x56EC00` and `DrawLayer_Open` `0x56FD20` were scanned the same way
(`reach_roots.py`, `reach_build.txt`). Our source for each handler was
grepped for `Rand` and `Sound_`.

| Handler | Kinds | Ours in | `Rand` | Writes outside the primitive, besides the GTE's own globals |
|---|---|---|---|---|
| `MapCell_DrawQuads` `0x570020` | 0 | `map_cells.cpp` | no | `Gfx_PacketNext` |
| `MapCell_DrawUprights` `0x570210` | 1..3, 5..0xE, 0x31..0x33, 0x35..0x3E | `map_field_objects.cpp` | no | the scratchpad `0x903850..0x903858` (`DamageScratch`'s page), `Prim_VertexScratch` `0x9037A4..0x9037BC`, `MapView_ScreenXY` (by the GTE call) |
| `MapCell_DrawDiagonalWall` `0x570530` | 4, 0x34 | `map_field_objects.cpp` | no | — |
| **`MapCell_DrawRising`** `0x570660` | **0xF** | `map_cells.cpp` | **8 a cell** | `Prim_VertexScratch` `0x9037A2..0x9037BC` |
| `MapCell_DrawFrames` `0x570870` | 0x10 | `field_e2.cpp` | no | — |
| `MapCell_DrawFlatFaces` `0x570A00` (+ `MapCell_FlatOverlay`) | 0x11..0x17, 0x19..0x1F | `map_field_objects.cpp`, `field_misc.cpp` | no | — (reads the cell's draw item) |
| `BareRet` `0x437CC0` | 0x18, 0x20, 0x23, 0x26, 0x2C..0x30 | — | no | — |
| `MapCell_DrawShaded` `0x570BC0` | 0x21 | `field_e2.cpp` | no | — |
| `MapCell_DrawSpinning` `0x570DE0` | 0x22 | `field_e2.cpp` | no | `Gte_MatrixDepth` (push, then pop: balanced) |
| **`MapCell_PatchThenStep`** `0x571090` | **0x24** | `map_field_objects.cpp` | no | **the record's kind byte; via `AreaMap_ApplyPatch` `0x571110`: `AreaMap_Header`'s texture run, `AreaMap_Corners`, `AreaMap_Bytes`, `MapView_Redraw`** |
| **`MapCell_PatchThenStop`** `0x5710D0` | **0x25** | `map_field_objects.cpp` | no | **the same** |
| `MapCell_DrawAnimated` `0x5712E0` | 0x27 | `map_field_objects.cpp` | no | `Prim_VertexScratch` `0x9037A2..A4`, `MapView_ScreenXY` |
| `MapCell_DrawTexQuads` `0x4CEB40` | 0x28 | `battle_e6.cpp` | no | — |
| `MapCell_DrawShadedQuads` `0x4CED60` | 0x29 | `battle_e6.cpp` | no | — |
| `MapCell_DrawSpinQuads` `0x4CEFC0` | 0x2A | `battle_e6.cpp` | no | `Gte_MatrixDepth` (balanced) |
| `MapCell_DrawGroundSprite` `0x4CF270` | 0x2B | `battle_e6.cpp` | no | `Prim_VertexScratch`, `MapView_ScreenXY` |
| `MapCell_DrawWalls` `0x571500` | 0x3F..0x4C | `map_layers.cpp` | no | — |

Every handler also writes `Gfx_PacketNext` and the ordering-table pointers
(`Gfx_CommitPrim`), and the GTE's globals `0x7DE428..0x7DE7A8` (`Gte_*`).

Notes on the scan:

- `AreaMap_Elevation`'s "write" to `MapView_HeightScale` is a false positive:
  `imul byte [MapView_HeightScale]` reads it.
- `Rand`'s own subtree (`Crt_GetPtd` and the C runtime's start-up under it)
  is the CRT's and is counted with `Rand`.
- No handler reaches a `Sound_*` function, by the image's call graph or by
  ours.
- The stores through a register in each body are to the primitive at
  `Gfx_PacketNext` or to vertex arrays on the stack (`lea ecx, [esp + n]`:
  read at `0x57012F`, `0x570F47` / `0x570F68`, `0x4CEC67`, `0x4CEE21`,
  `0x570C75`). The exception is the two patch handlers' store to the record.
- `battle_e6.cpp` does call `Rand` (lines 339, 341, 887), but from
  `DragonForm_Mix` `0x4517A0` and `Battle_MemberRollByAction` `0x453910`,
  which no handler reaches.

### 3.2 `Rand`: one consumer, routed

`MapCell_DrawRising` is the only handler that reaches `Rand`. It is routed
through the split (section 1). EB-2's diagnostic build already put every
extra call on the world-map route at `DrawRising`.

**The steam vent of D283** (area 2, outside the narrow view and inside the
band, "makes puffs"). Three facts point to `DrawRising`:

- whelpBoss's extra calls come 8 a frame, which is one `DrawRising` cell;
- `BOF3X_WIDE_INSET=0` removes them (EB-2), so they come from the cell walk;
- the scan finds no other random consumer under the handler table.

`DrawRising` draws eight squares stacked over a cell, rising 8 units a frame
and fading in shade. That fits "puffs", but this is an inference. Which
area 2 cell holds the vent, and that its record's kind is `0xF`, was not
read from the area data. Under the split the vent still draws in the band,
from our generator.

### 3.3 Other state a band cell changes: listed, not changed

- **`MapCell_PatchThenStep` / `_PatchThenStop` (kinds 0x24 / 0x25): game
  state.** `PatchThen` stores the record's next kind (`0x24` becomes `0x25`,
  which becomes `0x23`, a bare `ret`), then runs the area patch list entry
  through `AreaMap_ApplyPatch` `0x571110`. That function evaluates an
  `Area_TestCondition` code and writes, by record kind:
  - kind 0: a texture word into the area block;
  - kind 1: a draw item's texture;
  - kind 2: a dword of `AreaMap_Corners`, the ground heights that
    `AreaMap_Elevation` and the walk tests read, plus `MapView_Redraw = 2`;
  - other kinds: a byte of `AreaMap_Bytes`.

  So an `0x24` record applies its patch on its first two walked frames and
  then stops. In the wide view a patch cell in the band is walked, and so
  patched, on frames where the original's walk has not reached it yet. Its
  condition is read on that earlier frame. **Not changed here.** The owner
  ruled on `Rand` only. Whether any area holds an `0x24` / `0x25` record
  outside the original's inset at the moment it matters was not counted;
  we have no area-record parser. Splitting this the same way would mean a
  band cell skips the patch, and the original cell applies it when the
  original walk arrives. That is a logic change in the other direction, so
  it is the owner's call.
- **The scratchpad `0x903850..0x90385F`** (`DamageScratch` `0x903850`,
  `Scratch_Swap` `0x90385C`; PSX `0x1F800000`). `MapCell_DrawUprights`
  writes `+0`, `+4` and `+8` for each cell. After a row whose last cell is a
  band cell, the words hold that cell's values, not the last original cell's.
  `Battle_ScaleDamage` reads `0x903850`. Whether it, or any reader, reads the
  page before writing it after a field frame was not checked. EB-2 saw no
  game page differ under `BOF3X_WIDE_INSET=0` on ten routes, which does not
  rule it out.
- **`Prim_VertexScratch`, `MapView_ScreenXY`, the GTE's globals:** left
  holding the last cell's values, as above. These are draw state that the
  next draw rewrites. Not checked for a stale reader.
- **The packet pool and the ordering table:** more primitives a frame. A
  fuller pool can make `Gfx_CommitPrim` skip a later primitive. That affects
  only the picture.

The **build walk** (`MapView_Build`'s `BuildCell`, not a handler) also
reaches band columns under the lowered inset. It writes:

- `DrawItemPool_Alloc` / `_Release` items;
- `DrawTable` entries, whose byte count wraps at 256;
- `MapView_BuildFlags` bit 0. `AreaMap_DrawBackdrop` `0x571BE0` reads it,
  and that is a draw gate;
- `MapView_CornerPtr` and `DamageScratch`'s byte;
- `Scratch_Swap`.

None of these is a `Rand` call (`reach_build.txt`: no `Rand` under
`0x56EC00`). They are listed for completeness, not changed.

## 4. Measured

**Build:** `cmake --build build` at the branch tip, with no error lines.
`wide_rand.cpp`, `draw_emit.cpp`, `map_cells.cpp`, `map_layers.cpp` and
`inject_all.cpp` were rebuilt (`uw_rn/build1.log`, `build2.log`). Launcher
copy `uw_rn/L/` (ini-less: launcher, `bof3x.exe`, DLL).

**Runs:** `BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original BOF3X_SHADOW='*'
L/bof3x-launcher.exe --game <bof3> --no-config`, with `BOF3X_WIDE` unset,
`=1` and `=2`. Logs `uw_rn/log_w0.txt`, `log_w1.txt`, `log_w2.txt`.

All three runs **exit 0**. Each log has `inject: 10087 ours, 0 left
original by BOF3X_ORIGINAL`, and no non-zero mismatch line in any module.

| | narrow (`BOF3X_WIDE` unset) | 16:9 (`=1`) | 21:9 (`=2`) |
|---|---|---|---|
| `DIV-0041` line | the width rules only (no widescreen line) | `53 columns a side ... cell inset asked 3 lower a side ..., applied 3` | `120 columns a side ... applied 6 - ... the whole 28-column ring is walked` |
| `DIV H` line | `idle (the cell inset is not lowered), every cell on Capcom's Rand` | `cells only the inset lowered by 3 adds draw MapCell_DrawRising's squares from a generator of ours` | the same, `lowered by 6` |

**The `wide_rand` self-test** (the same in all three runs, plus one line for
the run's own lowering):

| Part | 0 columns (lowered by 0) | 53 (by 3) | 120 (by 6) |
|---|---|---|---|
| Boundary: rows (inset 0..14 x flag x parity x 28 columns) | 1,680 | 1,680 | 1,680 |
| ... cells handed over | 26,544 | 34,608 | 40,656 |
| ... band cells (rows with any) | 0 (0) | 8,064 (1,456) | 14,112 (1,456) |
| ... mismatches (set, walk-order edge, call counts) | **0** | **0** | **0** |
| Fuzz: whole frames (with a lowered inset) | 3,000 (0) | 3,000 (1,003) | 3,000 (1,003) |
| ... frames whose Capcom `Rand` calls differ from narrow's | **0** | **0** | **0** |
| ... draw-only calls by band cells | 0 | 319,696 | 470,984 |
| Control: frames that differ from narrow | 0 (not split: must be equal) | **419: refused** | **435: refused** |

Each boundary row checks three things:

- the cells handed over against the wide walk's column set;
- each band decision against the original's column set, and against the
  walk-order rule (the first `d` and last `d` cells);
- Capcom's calls at 8 per original cell, and the draw-only calls at 8 per
  band cell.

The modules this branch touched passed their own fuzzes at 0 mismatches in
all three runs:

- `map_cells`: `Area_TestCondition` 65,536 rounds, `AreaMap_Elevation`
  131,072, the handlers 24,000, of which 8,000 were `MapCell_DrawRising`;
- `DrawLayer_Open`: 12,000 rounds;
- `map_layers`: `AreaMap_Frame` 40,000, `MapView_Build` 1,500,
  `MapView_CellTextures` 60,000, `MapCell_DrawWalls` 60,000,
  `AreaMap_ClutCycle` 60,000, `AreaMap_HeaderPass` 30,000.

`python tools/ledger_check.py`: 114 entries, 0 errors, 2 notes. Draft H is
not in the ledger yet.

## 5. Not verified

- **No live run.** The randlog against narrow on whelpBoss, shop (21:9) and
  the world map, EB-2's check, is owed. The prediction is equal `Rand`
  counts on every frame where cause 1 was the only cause. The census's
  cause 2 (the slide-off bounds, group SL's) still parts dragonTransform and
  cutsceneAndNue.
- That the steam vent of D283 is a `DrawRising` cell (section 3.2: by
  elimination, not read from the area data).
- The patch handlers' effect in any real area (section 3.3): no area data
  was read.
- Stale readers of the scratchpad and the vertex scratch (section 3.3).
- The look of the band's rising squares on our generator. The recurrence is
  `Rand`'s, so they jitter as the original's do (0..3 on each half side), but
  the owner has not seen them.
- The live shadow's band clone (`BOF3X_SHADOW=map_cells` in play): built,
  not run (no live runs this round).

## 6. The owner's calls met

- **Split the random** (2026-10-13): built as asked. Narrow is untouched: no
  row is split when the insets are equal, and the self-test proves it at 0
  columns.
- **Keep the inset lowering:** kept; nothing in the walk changed.
- **New, for the owner:** the patch handlers (section 3.3) are a second
  logic consumer of the band cells, not `Rand`. Leave them, or split them,
  is the owner's call.

## 7. For the other files

**`docs/DIVERGENCE.md`, draft H: an amendment of DIV-0041:**

> **Amended (ultrawide round, group RN, [`ultrawide-rand.md`](ultrawide-rand.md)): the map walk's random split, 2026-10-13.** **The 2026-10-07 inset rule changed logic, not only the picture.** Lowering `MapView_Build`'s cell inset by `ceil(columns / 20)` (3 at 16:9, 6 at 21:9; `BOF3X_WIDE_INSET`) makes `DrawLayer_Open` `0x56FD20` hand `MapCell_Handlers` the cells of columns the original never walks. `MapCell_DrawRising` `0x570660` draws eight squares a cell with one `Rand` each, with no cull. From 2026-10-07 until this amendment, each such cell moved the CRT stream that encounters, battle damage and town walkers share by 8 calls a frame, and routes left the narrow one: whelpBoss at both widths from frame 285, shop at 21:9, the world map ([`ultrawide-census-2.md`](ultrawide-census-2.md) 3.1; [`known-defects.md`](known-defects.md) D283, area 2's steam vent). *Original behaviour:* every cell the walk reaches calls `Rand`. *New behaviour:* `Inset()` notes the inset Capcom's arithmetic gives before the lowering. `DrawLayer_Open` marks each cell outside the original's walk of its row as a band cell. `MapCell_DrawRising` takes a band cell's eight numbers from a draw-only generator of ours, `Rand`'s recurrence on a seed of its own that nothing else reads. The original's cells keep Capcom's `Rand`, in the original's order, so the band's rising squares still draw and the game's random stream is the narrow one. The inset lowering itself is unchanged. Narrow, and with `BOF3X_WIDE_INSET=0`, the insets are equal and nothing is split. *Why:* the owner, 2026-10-13: split the random; widen the picture, not the logic. *Scope:* `DrawRising` is the only random consumer under the handler table (image scan of all 17 handlers and their callees). The area patch handlers (kinds `0x24` / `0x25`) also change game state from a band cell and are left as they were, as the owner's call ([`ultrawide-rand.md`](ultrawide-rand.md) 3.3). *Verification:* `wide_rand`'s self-test runs ours alone, with stand-in handlers and a recording `Rand`. The boundary at 0 / 53 / 120 columns, over every original inset 0..14, input flag, row parity and view column, has 0 mismatches. Whole-frame fuzzes at four lowerings show Capcom's `Rand` calls equal to the narrow frame's, call for call and cell for cell, with 0 mismatches. The control (band cells on Capcom's `Rand`) is refused at 53 and 120. The counts are in the doc's section 4. *Reversible:* `BOF3X_WIDE_INSET=0` (no band). *Owed:* EB-2's randlog against narrow on whelpBoss, shop and the world map.

**`docs/README.md`, one row:**

> | [`ultrawide-rand.md`](ultrawide-rand.md) | The ultrawide round's random split (group RN, 2026-10-13; D283): cells only DIV-0041's lowered inset adds draw `MapCell_DrawRising`'s squares from a draw-only generator, the original's cells keep Capcom's `Rand`; every consumer under the 17 cell handlers scanned (`DrawRising` the one `Rand`; the patch handlers 0x24 / 0x25 change area state from a band cell, listed for the owner); boundary test at 0 / 53 / 120, whole-frame fuzz and its control. BUILT; not run live. |

**`docs/known-defects.md`:** D283 gains "fixed by DIV H for `Rand`
(2026-10-13, built, not run live); the patch handlers 0x24 / 0x25 are a
second logic path from the band (`ultrawide-rand.md` 3.3)".

**Defect for the coordinator to number:**

- *A band cell applies its area patch early* (ours, DIV-0041's inset;
  `MapCell_PatchThenStep` / `_PatchThenStop` via `AreaMap_ApplyPatch`). It
  writes ground heights, map bytes and textures on frames where the
  original's walk has not reached the cell. Not seen. Whether any area
  places one there was not counted.

**`docs/STATUS.md`:** "RN: the map walk's random split (DIV H): band cells
draw the rising squares from our generator, the original's cells keep
`Rand`; the self-test proves Capcom's calls equal to narrow's. Live randlog
owed."

**`docs/HANDOFF.md`:**

- EB-2's randlog on whelpBoss, shop and the world map at `=1` / `=2`
  against narrow. Expect equal `Rand` counts wherever cause 1 alone parted
  the route.
- The owner: the patch handlers (section 3.3).
