# The map and draw layers under the field's frame

**Status:** IN PROGRESS (2026-09-22) - six functions ours
(`src/game/map_layers.cpp`), each fuzzed alone against a copy of Capcom's
with every callee a recording stand-in, 0 mismatches, and every planted bug
refused. **Not yet through the batch check in game**: the frozen-shot A/B is
what checks these pixel for pixel, and it runs after the merge.

Handoff item 0000, target 2. Everything here feeds the draw: the view's
camera and scroll every field frame, the rebuild of the view's cell draw
items, two table-reached handlers, and the area header's pass.

## 1. The functions

Sizes are the disassembly's (capstone, 2026-09-22: each decodes exactly to its
last `ret`, and every jump stays inside). Call counts are the call trace's:
`all_b` (12,800 frames, a whole attract cycle) did not arm the two
pointer-reached handlers, so their counts are `hidden_b`'s (16,128 frames).

| Function | Address | Size | PSX twin (pair tier) | Calls `all_b` / `hidden_b` |
|---|---|--:|---|--:|
| `AreaMap_Frame` | `0x56E6C0` | 0x2DA | `FUN_801532A0` (call-anchored) | 8,846 / 12,165 |
| `MapView_Build` | `0x56EC00` | 0x9A1 | `FUN_80153B8C` (callers) | 5,007 / 6,839 |
| `MapView_CellTextures` | `0x56F9B0` | 0x11B | `FUN_80154D50` (gap4) | 21,435 / 28,357 |
| `MapCell_DrawWalls` | `0x571500` | 0x21F | `FUN_801587A0` (gap4) | - / 33,550 |
| `AreaMap_ClutCycle` | `0x571B40` | 0x98 (+8 padding) | `FUN_8015900C` (gap4) | - / 29,701 |
| `AreaMap_HeaderPass` | `0x571AF0` | 0x46 | `FUN_80158F60` (gap4) | 8,846 / 12,165 |

The pairs are `analysis/pairs_propagated.json`'s. The sibling's Ghidra export
of `SLPS_009.90` lists the twins with no decompilation, so the check here is
shape only: `FUN_801532A0` calls the four shift twins, `FUN_80153B8C`,
`FUN_801595E0` and `FUN_801F2C04`, as `AreaMap_Frame` calls theirs; the
handlers' twins sit in the same order in the same block. Treat the gap4 tier
as a few points less sure (`attract-remaining.md` section 5.1).

`pe_funcs.py`'s sizes were not used. `0x571B40`'s 160 bytes in the handoff
include 8 bytes of padding before `0x571BE0`.

### `AreaMap_Frame` `0x56E6C0`

Called with no arguments by the field frame entries `0x517200` / `0x517240`
(caller sites `0x517219`, 6,709 calls, and `0x517263`, 2,137). In area
`0xBD` it calls `AreaMap_FrameAreaBD` `0x510630` and does nothing else - that
function is a fixed-camera copy of the rest, and the attract cycle never
calls it. Otherwise:

1. **Follow.** `MapView_FocusX` `0x929F14` steps toward the kind-2 object:
   the difference is `((0x7FFF - x) << 8) - (Field_Kind2X & 0xFFFF8000)`, and
   a non-zero one moves x by `MoveScript_F3Divisor` with its sign, moves
   `MapView_ScrollX` `0x92BEE0` and `MapView_ElevationOffset` back by the same
   step, and sets `Field_Kind2Hold`. `MapView_FocusZ` `0x929F18` does the same
   against `Field_Kind2Z` with `0x8000`, moving `MapView_ScrollX` forward
   instead. While either moved, or the hold is still set,
   `MoveScript_FAWord` is added to `MapView_Elevation` (and twice it taken
   from the offset unless `AreaMap_Word1E` `0x8CB59E` is set), the redraw byte
   is 2, and the hold is cleared once both differences are 0.
2. **Scroll.** `MapView_ScrollX` at `0x200` or more shifts the view a column
   forward (`MapView_ShiftColumnNext` `0x56EA30`) and takes `0x200` off; at
   `-0x200` or less a column back (`0x56E9A0`). `MapView_ElevationOffset`
   does the same with two rows (`0x56EB50` / `0x56EAB0`). After any shift,
   `MapView_PlaceRuns` `0x571FF0` re-places the area's cell runs in the view.
3. **Camera.** If a word of `Camera_Angles` `0x929EC8` differs from
   `Camera_AnglesDrawn` `0x905D88`, the redraw byte is 3 and both dwords are
   copied. Then `Gte_RotMatrix` builds `Camera_Matrix` from the angles, its
   translation is `Camera_Matrix x (focus x, focus z, elevation) / 2` plus
   `Camera_ShiftX` `0x903800`, `Camera_ShiftY` `0x903802` and
   `Camera_Distance` `0x903840` + `0x1194`, and both halves go to the GTE.
4. **Rebuild.** While `MapView_Redraw` is non-zero, `MapView_Build`, then one
   less. So a camera turn rebuilds three frames running, a scroll two.

### `MapView_Build` `0x56EC00`

The view's 56 x 28 cells (`MapView_CellItems` `0x937FA0`, 4 bytes each: map
x, map y, and a word whose low 12 bits are a draw item) become draw items in
the layer lists that `Sprite_DrawPass` merges with the sprites
([`sprite-draw-order.md`](sprite-draw-order.md) sections 2, 7, 15).

- **Set-up.** `DrawLayers_Reset`, `DrawTable_Count` 0, bit 0 of
  `MapView_BuildFlags` `0x929ED8` cleared.
- **Inset** - columns off each side, by the camera: `(0x160 - |s16
  Cond_AngleFB - 0x200|) / 50`, less 1 (not below 0), less `Camera_Distance /
  650` (0 if that is more), less 2 more (not below 0) with bit 0 of
  `Field_InputFlags`, and 0 outright when `Cond_ByteFE` is `0x23`. Written to
  `Scratch_Swap` and `MapView_Inset`, the one `DrawLayer_Open` reads.
- **Walk.** Layer j = 0..0x36 is view row `(MapView_Row + 1 + j) mod 0x38`;
  its `(14 - inset) * 2` columns run from `MapView_Column + inset`, each
  advanced (0x1B wraps to 0) before it is read - `DrawLayer_Open`'s walk.
- **A cell.** Map x or y 0: released (`DrawItemPool_ReleaseCell`) and flags
  bit 0 set. Otherwise its first corner - `((x << 7) - 0x4040, (y << 7) -
  0x4040, -(s8 corner byte 0) * 16)`, the corner dword at `AreaMap_Corners`
  `+ (y * width + x) * 4` - goes through `Gte_Rtps` into `MapView_ScreenXY`
  `0x903820`, and the cell is released unless `-50 <= x <= 370` and `-200 <= y
  <= 290` (x87 compares: a NaN is culled). A kept cell's item is its word's
  low 12 bits; for none, `DrawItemPool_Alloc` gives one, with a side item at
  `+0x7E` when the next cell's near corners stand lower and one at `+0x8E` for
  the cell below, and `MapView_CellTextures` fills them and ORs its four flag
  bits into the cell word. The item's quad is the four corners, 128 square;
  its first screen point is the one just culled.
- **Where it goes**, by the cell word:

  | Bits 15 / 14 | List |
  |---|---|
  | 1 / 1 | layer j - 1's list 0 (layer 0's for j = 0) |
  | 1 / 0 | layer j's list 1 |
  | 0 / 1, `Draw_OtSlot` 6, or 4 with j above a row threshold | `DrawTable`: j in bits 31..24, the lowest corner in 23..16, the item in 11..0 |
  | otherwise | layer j's list 0 |

  The threshold is `Field_Kind2Z.hi - MapView_Origin.y - MapView_Origin.x +
  Field_Kind2X.hi + 8` - the kind-2 object's own row, by its shape. The
  lowest corner is the smallest of the four bytes with their sign bits
  flipped, and passes through `DamageScratch`'s byte `0x903850` on the way.
- **Side triangles.** The `+0x7E` item (the next cell's edge) and the `+0x8E`
  item (the edge below) are triangles from two corners of the neighbour and
  the quad's own third vertex, in layer j's list 0 when cell word bit 13 /
  12 is set, else list 1.
- **End.** `DrawTable_Sort`.

### `MapView_CellTextures` `0x56F9B0`

`(x, y, item, buffer)`, called twice by `MapView_Build`. The cell's tile word
is `AreaMap_Header` word `x + width * y + offset * 2` (`offset` the header's
u16 `+2`), and 0 means none: the result is 0. Otherwise the texture dwords run
from `AreaMap_Header` dword `(height * width + 1) / 2 + offset + tile`: the
item's quad for `buffer` through `Prim_SetTexture`, then the `+0x8E` side
item's, then the `+0x7E` one's - where a zero dword instead releases that item
(`DrawItemPool_Release`) and clears its word. The result is bit 30 of each
dword as `0x8000`, `0x1000` and `0x2000`, with bit 14 of the first. An item's
word `+0x36` is how `MapView_Build` asks for the other buffer next frame.

### `MapCell_DrawWalls` `0x571500`

`MapCell_Handlers` entries `0x3F..0x4C` - all fourteen are this function
(`0x663104..0x663138`, byte scan of the exe). With `n = (kind - 0x3F) >> 1`:
below 4, one quad with `MapCell_WallTextures[n]` `0x663280`; from 4, two with
entries `2n - 8` and the next. Each quad is a wall 128 high hanging from one
edge of the cell - the top edge for an even texture index, the left for an
odd one - with its top at the heights of the cell's own two corners on that
edge or the neighbour's two across it, whichever pair has a corner higher
(signed). `Gte_RotTransPers4`, `Gte_PrimDepths4_10`, `Prim_SetTexture`,
`Gfx_CommitPrim` to `Draw_OtSlot`. What the walls look like in game is not
established; the name describes the geometry.

### `AreaMap_ClutCycle` `0x571B40`

`AreaMap_EntryHandlers` entry 0: a palette cycle. The entry's low word is a
period; `f = Frame_Counter mod period + 1`. If `Area_TestCondition` of the
next word is true, the dwords after it are scanned in order for top byte
`f`, stopping at one above `f`; on a match, 16-colour row `(d >> 12) & 0xFFF`
of `Gfx_ClutStrip` is copied over row `d & 0xFFF` and `Gfx_ClutStripDirty` is
set.

### `AreaMap_HeaderPass` `0x571AF0`

The first call of `0x592F00` (group A's). With `Draw_PassFlags` bit 2, walks
the area block's dword entries from `AreaMap_EntryBase` `0x8CB5A2`, calling
`AreaMap_EntryHandlers[(entry >> 24) & 0x7F]` `0x66329C` with the entry's
address, and moves on by the entry's byte `+2` in dwords, read after the call,
until a zero dword.

## 2. The handler tables

| Table | Entries | What |
|---|---|---|
| `MapCell_Handlers` `0x663008` | `0x3F..0x4C` | all `MapCell_DrawWalls` |
| `AreaMap_EntryHandlers` `0x66329C` | 0 | `AreaMap_ClutCycle` |
| | 1, 2, 3 | `0x571BE0`, `0x571D30`, `0x571E20` - named `AreaMap_EntryKind1..3`, not read; none is called in `hidden_b` |
| | 4.. | `Kind2_Run`'s table `0x6632AC` (group B), then data: the mask reaches 128 entries |
| `MapCell_WallTextures` `0x663280` | 0..3 | `0xB1800007`, `0xB1810007`, `0xBD800006`, `0xBD810006` |
| | 4..6 (`0x663290`) | the code addresses `0x571880`, `0x571A30`, `0x571A70` - another table, not textures |

## 3. Callees

Every call goes through `map_layers::g` (`src/game/map_layers_callees.h`), so
the fuzz can stand recorders in. Newly typed in `symbols.toml` (`# group D
callees`, all `hypothesis`): `AreaMap_FrameAreaBD` `0x510630`,
`MapView_ShiftColumnNext` / `Prev` `0x56EA30` / `0x56E9A0`,
`MapView_ShiftRowsNext` / `Prev` `0x56EB50` / `0x56EAB0`, `MapView_PlaceRuns`
`0x571FF0`, `AreaMap_EntryKind1..3`. Their call counts (`all_b`): 44, 150,
92, 115, 402; the area-0xBD function and the three kinds are never called.
The other callees were already typed and are ours: the GTE and GPU library,
`Prim_SetTexture`, `Gfx_CommitPrim`, the draw-item pool, `DrawLayers_Reset`,
`DrawTable_Sort`, `Area_TestCondition`.

## 4. Kept as the original has it

Each is in the code's comments; the ones a control refused are marked.

- **Re-reads after calls.** `AreaMap_Frame` reads the scroll words, the
  redraw byte, the angles, the focus, the elevation and the translation's
  shifts after the calls that precede them (controls F7, F9);
  `MapView_Build` re-reads the inset from `Scratch_Swap` after every cell -
  the game's all-purpose temporary - for the column count and the next row's
  first column (B10), and the corner pointer after an allocation (B15).
- **Globals as temporaries.** The corner pointer lives in `MapView_CornerPtr`
  `0x92A0C0`, the draw-table key's minimum in `DamageScratch`'s byte (B21) -
  both left there, both compared.
- **The follow overshoots.** A difference not a multiple of the step is never
  landed on: the focus swings across it, the hold stays set, and the view is
  rebuilt every frame (`MapView_Redraw` 2). Whether any area's step and the
  object's rounding allow that is unread.
- **Camera_AnglesDrawn** takes both dwords, the word after the three angles
  too (F13).
- **`MapView_CellTextures`**: a zero third dword releases the `+0x7E` item;
  a zero second one is given to the `+0x8E` item as a texture word, not
  released.
- **`MapCell_DrawWalls`** uses its arguments at 32 bits and does not check
  the kind (read, not run: only `0x3F..0x4C` reach it).
- **`AreaMap_ClutCycle`**: a period of 0 divides by zero, a list without a
  top byte at or above the frame runs on, rows are 12 bits against the
  strip's 512 - all three read, not run.
- **`AreaMap_HeaderPass`**: the kind masked to 7 bits and not checked against
  the four handlers (all 128 entries run by the fuzz); a step of 0 never ends
  (read, not run).
- **Vertex padding.** `MapCell_DrawWalls` and `AreaMap_Frame` build vertices
  on the stack and never write their fourth words, as `MapCell_DrawQuads`
  does (DIV-0023) - but here the callee is `Gte_RotTransPers4` /
  `Gte_ApplyMatrix`, which read three words, so nothing reaches memory and no
  ledger entry is needed. `MapView_Build`'s vertices are in
  `Prim_VertexScratch`, a global whose fourth words it leaves as they were,
  and so does ours.

## 5. Checks

**Start-up fuzz**, `BOF3X_SHADOW=map_layers` (`src/game/map_layers_fuzz.cpp`).
Each original is cloned with every call re-aimed at a recording stand-in -
the Frame's call of `MapView_Build` and the Build's of `MapView_CellTextures`
too, so each is tested alone - and ours runs with the same stand-ins through
`map_layers::g`. For `AreaMap_HeaderPass` all 128 entries of
`AreaMap_EntryHandlers` become numbered stand-ins, put back afterwards.
Compared each round: every byte of the function's state, the stand-ins' log
(count, a hash of every entry, the first 48 kept) and the result. The clone
runs under control word `0x027F`.

The stand-ins give back what the caller reads: screen points drawn from each
cull bound, a step past it, NaNs (quiet and signalling, written as bits),
infinities and the range around; allocations of 0 a quarter of the time; the
`+0x7E` / `+0x8E` words cleared as the real texture call may; the packet
pointer moved by `Gfx_CommitPrim`'s byte size. Some also disturb what the
caller reads again: the scroll words and the redraw byte after a shift, the
angles after `MapView_PlaceRuns`, the focus and elevation after
`Gte_RotMatrix`, the shifts after `Gte_ApplyMatrix`, `Scratch_Swap` after a
cell's textures, the corner pointer after an allocation, the step byte after
a header entry's handler.

The run of 2026-09-22 (`build/bof3x.log`, the same under `BOF3X_SHADOW=*`,
43 self-test lines, 164 injects):

| Function | Rounds | Coverage | Mismatches |
|---|--:|---|--:|
| `AreaMap_Frame` | 40,000 | 2,659 in area 0xBD; 21,137 stepping x, 21,134 z, 4,783 held still, 7,571 reaching the object; 42,901 shifts, 19,869 angle changes, 34,222 rebuilds | **0** |
| `MapView_Build` | 1,500 | 686,458 cells to the cull, 743,445 released (off the map or culled), 144,500 quads, 109,363 allocations, 123,916 side triangles, 255,888 list links, 12,528 draw-table entries; 252 rounds with `Cond_ByteFE` 0x23, 126 with no columns | **0** |
| `MapView_CellTextures` | 60,000 | 12,009 without a tile, 23,967 with a `+0x8E` item, 8,968 with all three textures, 5,954 `+0x7E` items released | **0** |
| `MapCell_DrawWalls` | 60,000 | 4,286 per kind `0x3F..0x4C`; 29,052 with the left neighbour higher | **0** |
| `AreaMap_ClutCycle` | 60,000 | of rounds starting clean, 13,830 with a copy and 31,114 without; 60,000 condition tests | **0** |
| `AreaMap_HeaderPass` | 30,000 | 7,488 with the pass off, 2,795 empty; 89,697 handler calls, 72,241 kinds with bit 7 set | **0** |

**Negative controls.** Each a rebuild and a start-up run, reverted after
(`controls.py`, a scratch script: one planted change per run). All refused
with exit 3, by comparison:

| Control | Mismatches |
|---|--:|
| F1 the z step moves `MapView_ScrollX` the wrong way | 16,130 |
| F2 the hold never released | 7,571 |
| F3 `AreaMap_Word1E` test inverted | 27,457 |
| F4 column shift at `> 0x200` | 1,047 |
| F5 no `MapView_PlaceRuns` after rows back | 11,926 |
| F6 translation `+ 0x1190` | 37,341 |
| F7 focus x not re-read after `Gte_RotMatrix` | 9,302 |
| F8 angle z not compared | 4,123 |
| F9 redraw byte not re-read after the build | 11,356 |
| F10 area `0xBC` for `0xBD` | 5,280 |
| F11 elevation offset `+ 2 * rise` | 13,918 |
| F13 `Camera_AnglesDrawn` word 3 not copied | 9,988 |
| B1 inset `/ 51` | 320 |
| B2 distance `/ 640` | 196 |
| B3 the input flag narrows by 1 | 302 |
| B4 `Cond_ByteFE` `0x24` | 186 |
| B5 cull `x > -50` | 1,374 |
| B6 cull `y < 290` | 1,374 |
| B7 bits 15 + 14 to this layer | 1,373 |
| B8 draw-table threshold `>=` | 130 |
| B9 draw-table key the highest corner | 626 |
| B10 inset not re-read after a cell | 1,371 |
| B11 `+0x36` values swapped | 1,373 |
| B12 `+0x7E` list by bit 12 | 1,373 |
| B13 `+0x7E` side from the cell, not the next | 1,374 |
| B14 `+0x7E` test unsigned | 1,359 |
| B15 corner pointer not re-read after the allocation | 979 |
| B16 row wraps after `0x36` | 1,374 |
| B17 column start wraps by `0x1B` | 781 |
| B17b column advanced after it is read | 1,374 |
| B18 first screen y from x | 1,374 |
| B19 the flags replace the cell word | 1,373 |
| B20 `+0x8E` side from the cell, not the one below | 1,374 |
| B21 the key kept off `DamageScratch` | 621 |
| T1 texture run at `h * w / 2` | 11,806 |
| T2 `0x1000` from bit 29 | 9,054 |
| T3 a zero texture never releases | 5,954 |
| T4 run not advanced past the `+0x8E` texture | 11,225 |
| T5 tile at `offset`, not `offset * 2` | 59,999 |
| W1 one quad for every kind | 25,712 |
| W2 texture index `t - 4` | 17,140 |
| W3 edge x offset by parity, not its complement | 60,000 |
| W4 left heights compared unsigned | 11,104 |
| W5 wall top `+ 0x80` | 60,000 |
| W6 second left corner not compared | 10,695 |
| W7 upper neighbour one cell left | 37,489 |
| C1 the frame without `+ 1` | 19,216 |
| C3 rows swapped | 18,513 |
| C4 condition ignored | 9,342 |
| C5 dirty flag not set | 13,830 |
| C6 source row's low nibble from bits 8..11 | 17,355 |
| H1 step read before the call | 12,265 |
| H2 pass flag bit 1 | 13,246 |
| H3 kind masked to 6 bits | 17,311 |

A first B17 - the column start not wrapped at exactly `0x1C` - ended the
process (139) instead: the unwrapped column walks past the last row into
memory the fuzz does not own. A crash proves less than a count, so it was
replaced by the two B17 controls above, both refused by comparison. Two
candidate controls were dropped as changes that change nothing: the step's
sign at `dx = 0` (never reached - a zero difference does not step) and the
input flag's floor at exactly 2 (`2 - 2` and the floor are both 0).

**What the fuzz does not reach.** The real callees: every one is a stand-in,
so the live batch check - frozen-shot pixel A/B above all, since all six
feed the draw - is the only test of the whole. Area `0xBD`'s path is a call
and a return; `AreaMap_EntryKind1..3` are not called in the attract cycle,
and neither is area `0xBD`. The rows past the clut strip, a zero period and
a runaway scan (above) are read, not run.

**For the batch check** (`--original`):
`AreaMap_Frame,MapView_Build,MapView_CellTextures,MapCell_DrawWalls,AreaMap_ClutCycle,AreaMap_HeaderPass`
- all six reached by the attract cycle (counts above).

## 6. Possible defects of the 2001 code

Written down, not fixed (the owner's call; each would want a `DIVERGENCE.md`
entry).

- **Kinds `0x4B` / `0x4C` draw with code addresses as texture words.**
  `MapCell_DrawWalls` indexes `MapCell_WallTextures` at 4 and 5 for them, past
  the four textures into `0x571880` / `0x571A30`. Whether any area's cell
  records use those two kinds is unread (a scan of the area files' cell runs
  would answer it), and whether the PlayStation's table had six entries is
  unchecked.
- **The follow can oscillate** (section 4): a rebuild every frame while
  held. Harmless if every step divides 128, the object's rounding; unread.
- **The `+0x8E` item keeps a zero texture** where the `+0x7E` one is released
  (section 4). May be deliberate.
