# The map view's scrolling and its reset

**Status:** IN PROGRESS (2026-09-22) - nine functions ours
(`src/game/map_scroll.cpp`): the four shifts of the view's cell ring,
`MapView_PlaceRuns` after them, `Field_ViewReset` and its three set-up
callees. Each was fuzzed against a copy of Capcom's with every call re-aimed
at a recording stand-in, 0 mismatches, and 48 of 50 negative controls refused
by a count (section 5). **Not yet through the live batch check** - that runs
centrally after the merge.

Handoff item 0000, the second parallel round, group F. Last round took
`AreaMap_Frame` `0x56E6C0` and the view's rebuild
([`map-layers.md`](map-layers.md)); this is what `AreaMap_Frame` calls when
the view actually moves, and what lays the view out in the first place.

## 1. The functions

Sizes are the disassembly's (capstone, 2026-09-22: each decodes to its last
`ret` with every jump inside it, none has a jump table). Call counts are the
call trace's `hidden_b` (16,128 frames, a whole attract cycle), with `all_b`'s
in brackets where they differ.

| Function | Address | Size | PSX twin (pair tier) | Calls |
|---|---|--:|---|--:|
| `MapView_ShiftColumnPrev` | `0x56E9A0` | 0x83 | `FUN_80153758` (call-anchored) | 196 (150) |
| `MapView_ShiftColumnNext` | `0x56EA30` | 0x71 | `FUN_80153858` (call-anchored) | 66 (44) |
| `MapView_ShiftRowsPrev` | `0x56EAB0` | 0x9D | `FUN_80153950` (call-anchored) | 164 (115) |
| `MapView_ShiftRowsNext` | `0x56EB50` | 0xA1 | `FUN_80153A6C` (call-anchored) | 117 (92) |
| `MapView_PlaceRuns` | `0x571FF0` | 0xCE | `FUN_801595E0` (call-anchored) | 543 (402) |
| `Field_ViewReset` | `0x56F670` | 0x291 | `func_0x801548A4` (call) | 12 |
| `AreaMap_BakePatches` | `0x56FAD0` | 0xF8 | `FUN_80154EF4` (call-anchored) | 12 |
| `AreaMap_SetupEntries` | `0x571720` | 0x83 | `FUN_80158A54` (call-anchored) | 12 |
| `AreaMap_ClutCycleStart` | `0x5717B0` | 0xC6 | `FUN_80158B60` (call-anchored) | 12 |

The pairs are `analysis/pairs_propagated.json`'s. Unlike last round, the PSX
twins were **read**, not only named: `SLPS_009.90` is at file offset 0x800 for
`0x80093800`, and capstone disassembles MIPS from it. All four shifts, the
set-up walk and the palette start match instruction for instruction in shape -
same wraps (0x1B, 0x37), same `-1` / `0x1C` / `-2` / `-3` / `0x38` arguments,
same "first top byte at or above the frame" scan, same table index mask
(0x3F, on the PSX table at `0x80180268`). Nothing in the port was rewritten
here.

**Sizes to add to `entries_logic.txt`** (the frame hash's trace list): the nine
above. Eight are already listed with these sizes; **`0x5717B0` is listed as
0x13F**, which is `pe_funcs.py` running on into the pointer-reached `0x571880`
after it - it is 0xC6. (`0x571110`'s 0x60F is the same artefact; its body is
0x1CC. It is not ours, so it only matters if it is ever taken over.)

### The ring

`MapView_CellItems` `0x937FA0` is a ring of 0x38 rows by 0x1C columns, four
bytes a cell - map x, map y, and a word whose low 12 bits are the cell's draw
item ([`map-layers.md`](map-layers.md) §1). `MapView_Row` `0x929F24` and
`MapView_Column` `0x929F20` are its heads: the slot at the head is the one
*just before* view row / column 0, which is why `MapView_Build` and
`DrawLayer_Open` advance the index **before** they read a cell. `MapView_Cells`
`0x904F20` is a second ring of the same shape, one word a cell, holding where
that cell's record run starts.

A scroll never moves a cell. It releases the ring slots that leave the view,
refills them through `MapView_CellToMap` with the cells that enter, and moves
the head and `MapView_Origin` by one map cell. The four are called by
`AreaMap_Frame` when `MapView_ScrollX` or `MapView_ElevationOffset` passes
`0x200` either way (`map-layers.md` §1); `0x4B5350`, the *other* view
initialiser, calls two of them as well and is unread.

### The four shifts

| Function | Refills | With | Then |
|---|---|---|---|
| `MapView_ShiftColumnPrev` | the head column's 56 cells, rows from head + 1 | `MapView_CellToMap(i, -1)`, i = 0..0x37 | head column - 1 (0 wraps to 0x1B), origin (-1, +1) |
| `MapView_ShiftColumnNext` | the column after the head (0x1B wraps to 0), same 56 rows | `MapView_CellToMap(i, 0x1C)` | origin (+1, -1), head column = the new one |
| `MapView_ShiftRowsPrev` | twice: the head row's 28 cells, columns from head + 1 | `MapView_CellToMap(-2, j)`, then `(-3, j)` | each time head row - 1 (0 wraps to 0x37); origin (-1, -1) |
| `MapView_ShiftRowsNext` | twice: the row after the head, 28 cells | `MapView_CellToMap(0x38, j)`, then `(0x39, j)` | each time head row = the new one; origin (+1, +1) |

Every cell is `DrawItemPool_ReleaseCell`d first, then refilled.
`MapView_CellToMap`'s first argument is the cell's place in the walk, not the
ring row - it is a *view* row, and view rows -1, -2, -3, 0x1C, 0x38, 0x39 are
exactly the lines just outside the 0x38 x 0x1C window.

Which head is re-read, and when, differs between the four, and ours follows
each (controls S3, S7, S11):

- `ColumnPrev` re-reads `MapView_Column` for every one of its 56 cells and
  reads `MapView_Row` once;
- `ColumnNext` reads both once, before the walk, and stores the new column
  only after it;
- `RowsPrev` re-reads `MapView_Row` for every cell and `MapView_Column` once
  a pass;
- `RowsNext` reads `MapView_Row` from memory for the first pass only - the
  second advances the value the first stored - and `MapView_Column` once a
  pass.

Nothing the shifts call writes either head, so only a stand-in can tell the
four apart; the fuzz's stand-ins do move them (section 5).

### `MapView_PlaceRuns` `0x571FF0`

`MapView_Cells`'s 0x310 dwords are zeroed, then the area block's run list is
walked from dword `AreaMap_CellBase` (its low word) until a zero dword. A run's
first dword holds map y in byte 0, map x in byte 1 and its length in dwords in
the high half - the step to the next run, re-read from memory after the store.
For a run at map (x, y), with `x' = x - MapView_Origin[0]` and
`y' = y - MapView_Origin[1]`:

```
s = x' + y'        the view row,   kept only if 0 <= s < 0x38
d = x' - y'        twice the view column (odd rows add one), kept if 0 <= d < 0x38
row    = (s + MapView_Row + 1)        wrapped once by 0x38
column = (d / 2 + MapView_Column + 1) wrapped once by 0x1C
MapView_Cells[column + row * 0x1C] = (the run's dword offset from the list) + 1
```

That is `MapView_CellToMap` turned round (`src/game/map_view.cpp`: x = origin x
+ col + (row + 1) / 2, y = origin y + row / 2 - col), and it is why
`DrawLayer_Open` can find a cell's records by its ring position. The "+ 1"
makes 0 mean "no run".

### `Field_ViewReset` `0x56F670`

Called at area set-up by `0x594E60` (`0x594F76`) and from 43 other sites - the
event script's `Kind2_Script` `0x573231` among them (an E8 scan of `.text`).
In order:

1. `Gte_SetGeomScreen(0x3E8)`, `Gte_SetGeomOffset(0xA0, 0x78)`,
   `Gte_SetBackColor(0x78, 0x78, 0x78)`.
2. All 0x800 quads of `DrawItems` (two per draw item, one per display buffer)
   through `Gpu_SetPolyFT4` and `Gpu_SetShadeTex(q, 0)`, with each quad's
   words `+0x36` and `+0x46` cleared after its two calls - for a draw item
   those are `+0x36` / `+0x46` and the side-item words `+0x7E` / `+0x8E`
   (`map-layers.md` §1).
3. `Camera_Angles` = (`0xFD56`, 0, `0x200`) and both dwords copied into
   `Camera_AnglesDrawn` - the word after the three angles too, as
   `AreaMap_Frame` does. Draw item 0 - the index that means "none" - has its
   eight screen points zeroed.
4. `MapView_FocusX` = `0x7FFF - (Field_Kind2X >> 8)`, `MapView_FocusZ` =
   `0x8000 - (Field_Kind2Z >> 8)`, `MapView_Origin` = (`Kind2X` high word -
   0x18, `Kind2Z` high word + 3). `Camera_Distance`, `Camera_ShiftX` and
   `Camera_ShiftY` 0.
5. `MapView_Elevation` = `AreaMap_Elevation(Kind2X, Kind2Z)` as an s16 -
   unless bit 3 of `Field_InputFlags` is set, when it is 0 and no call is
   made. `MapView_BuildFlags` = the input flags **read again after that call**
   `<< 7` (the whole byte, so only bit 0 survives).
6. `DrawTable_Count` 0; `MapView_ElevationOffset` = `AreaMap_Word1E`, or
   `-2 x` the elevation when that word is 0 - the same rule
   `MapView_SetElevation` has ([`sprite-draw-order.md`](sprite-draw-order.md)
   §16). `AreaMap_Bytes` = block dword `AreaMap_BytesBase` (header `+0x14`).
   `MapView_ScrollX`, `MoveScript_F3Divisor`, `MoveScript_FAWord`,
   `MapView_HeightScale` and `Cond_ByteFE` 0; `MapView_Redraw` 3.
7. All 0x620 ring cells: the draw-item word cleared **before**
   `MapView_CellToMap(row, column)` fills the position. The old draw items are
   not released one by one - the pool is reset instead:
   `DrawItemPool_Free[i] = i` for 0..0x3FF, `DrawItemPool_Top` 1.
8. Heads: `MapView_Column` 0x1B, `MapView_Row` 0x37 - so view row / column 0
   is ring slot 0.
9. `AreaMap_BakePatches`, `AreaMap_SetupEntries`, `MapView_PlaceRuns`.

### The area block's three lists

The area header holds three dword indices into its own block, all now named:
`AreaMap_SetupBase` `0x8CB5A0` (+0x20), `AreaMap_EntryBase` `0x8CB5A2` (+0x22,
`AreaMap_HeaderPass`'s, `map-layers.md` §1), `AreaMap_CellBase` `0x8CB5A4`
(+0x24, the runs) and `AreaMap_PatchBase` `0x8CB5A8` (+0x28).

**`AreaMap_SetupEntries` `0x571720`** is the set-up pass over two of them:

```
Cond_ByteFF = Cond_ByteFFSource & 1                       // 0x8034E1, bit 0
for each entry of the set-up list (AreaMap_SetupBase):
    AreaMap_SetupHandlers[(entry >> 24) & 0x3F](entry)    // 0x663290
    entry += entry[2] * 4                                 // the step, read after the call
AreaMap_ClutCycleStart()
for each entry of the patch list (AreaMap_PatchBase, re-read after the call above):
    AreaMap_ApplyPatch(entry)                             // 0x571110, Capcom's
    entry += (entry >> 16) * 4 + 4                        // read after the call
```

`AreaMap_SetupHandlers` `0x663290` holds three code pointers -`0x571880`,
`0x571A30`, `0x571A70`, all pointer-reached and unread - and the mask reaches
64 slots, so entry kinds 3 and up fall through into `AreaMap_EntryHandlers`
`0x66329C`, then `Kind2_RunTable`, then data. The same overlap
`map-layers.md` §2 found from the other side; `MapCell_WallTextures` indexes
into its first two entries for wall kinds `0x4B` / `0x4C`.

**`AreaMap_ClutCycleStart` `0x5717B0`** walks the *header entry* list
(`AreaMap_EntryBase`, the one `AreaMap_HeaderPass` walks every frame) and
puts every palette cycle at its current frame at once. For an entry whose top
byte is exactly `0x80` - kind 0, `AreaMap_ClutCycle`'s, with bit 7 set - with
`f = Frame_Counter mod (entry & 0xFF) + 1`, it takes the **first** dword after
the entry whose top byte is `f` or above and copies 16-colour row
`(d >> 12) & 0xFFF` of `Gfx_ClutStrip` over row `d & 0xFFF`, setting
`Gfx_ClutStripDirty`. Three differences from `AreaMap_ClutCycle`, all also in
the PSX twin: the period is the low **byte**, not the word; there is no
`Area_TestCondition` test; the scan takes the first top byte at or above `f`,
not an equal one (so it always lands on something, given the `0xFF` sentinel
the lists end with).

**`AreaMap_BakePatches` `0x56FAD0`** walks the patch list before any of that.
Each entry is a header dword - an `Area_TestCondition` code in the low word, a
length `n` in the high word - and `n` dwords of 3-dword records; the next
entry is `n + 1` dwords on. An entry whose code is `0x8001` is one that is
always true (kind `0x80` falls through `Area_TestCondition`'s table to "bit
0", `sprite-draw-order.md` §16). Those, and only those, are *baked*: the code
becomes `0x8000` (always false) and, if the first record's top byte is below
2, each record's **second** dword - the one `AreaMap_ApplyPatch` would choose
for a false condition - is written into the area's texture run at

```
dword (height * width + 1) / 2 + offset + tile + ((record >> 16) & 0xF)
```

where `tile` is the cell word of map (x = record byte 1, y = record byte 0),
addressed exactly as `MapView_CellTextures` addresses it (`map-layers.md` §1).
So a patch that is unconditional is applied once, at set-up, and disarmed; the
conditional ones stay for `AreaMap_ApplyPatch` to apply every set-up.

`AreaMap_ApplyPatch` `0x571110` stays Capcom's (read, not taken over): its
record kinds are 0 (the texture run **and** a live draw item's texture through
`0x572ED0` and `Prim_SetTexture`), 1 (the draw item only), 2 (a corner dword
of `AreaMap_Corners`, with `MapView_Redraw` 2) and anything else (a byte of
`AreaMap_Bytes`).

## 2. Newly named data

| Address | Name | What |
|---|---|---|
| `0x663290` | `AreaMap_SetupHandlers` | the set-up list's handler table, 3 entries, 64 reached by the mask |
| `0x8CB5A0` | `AreaMap_SetupBase` | header +0x20: the set-up list |
| `0x8CB5A8` | `AreaMap_PatchBase` | header +0x28: the patch list |
| `0x8CB594` | `AreaMap_BytesBase` | header +0x14: where `AreaMap_Bytes` points |
| `0x8034E1` | `Cond_ByteFFSource` | bit 0 goes to `Cond_ByteFF` at every set-up (hypothesis: nothing else about the byte is read here; 41 functions touch it) |

`MapView_CellItems`, `MapView_Cells`, `MapView_Row` and `MapView_Column` keep
their names; their entries now say what the ring is and which function moves
each head.

## 3. Callees

Every call goes through `map_scroll::g` (`src/game/map_scroll_callees.h`), so
the fuzz can stand recorders in: `DrawItemPool_ReleaseCell`,
`MapView_CellToMap`, `Gte_SetGeomScreen`, `Gte_SetGeomOffset`,
`Gte_SetBackColor`, `Gpu_SetPolyFT4`, `Gpu_SetShadeTex`, `AreaMap_Elevation`
(all ours already), the three set-up functions and `MapView_PlaceRuns` (ours,
in this file - so each is tested alone), and `AreaMap_ApplyPatch` `0x571110`,
which stays Capcom's. The one indirect call is
`AreaMap_SetupHandlers[(entry >> 24) & 0x3F]`.

## 4. Kept as the original has it

Each is in the code's comments; the ones a control refused are marked.

- **The re-reads.** The heads, as §1 lists them (S3, S7, S11);
  `Field_ViewReset`'s input flags after the elevation call (R5);
  `AreaMap_SetupEntries`' step bytes after each handler (E2, E4) and
  `AreaMap_PatchBase` after `AreaMap_ClutCycleStart` (E5);
  `AreaMap_BakePatches`' entry length after every record (B5) and its width,
  height, offset and tile for every record.
- **Store order around a call.** `Field_ViewReset` zeroes the camera's
  distance and shifts *before* the elevation call and sets the heads *before*
  the three set-up calls; ours does too (R17).
- **16-bit arithmetic.** The heads, the origin and the stored run offset are
  words and wrap as words; `MapView_ShiftColumnNext` compares the head column
  at 16 bits and then sign-extends it.
- **Nothing is bounded.** A head out of 0..0x37 / 0..0x1B walks out of the
  ring; `MapView_PlaceRuns` wraps its position once only, so an out-of-range
  head writes past `MapView_Cells`; a run or patch entry with a step of 0 never
  ends; `AreaMap_ClutCycleStart` divides by zero on a period byte of 0, runs
  on past a list with no top byte at or above `f`, and takes 12-bit rows
  against the strip's 512. All read from the code, not run: the fuzz keeps
  inside every one of them.
- **`AreaMap_BakePatches` ignores the third dword of a record** and, when the
  length is not a multiple of 3, still writes the whole last record (its reads
  run past the entry into the next one). Both read, not run - except that the
  fuzz does build lengths that are not multiples of 3.
- **`Field_ViewReset` releases no draw item** before it resets the pool, and
  leaves item 0's `+0x36` / `+0x46` words as the quad loop set them.

## 5. Checks

**Start-up fuzz**, `BOF3X_SHADOW=map_scroll` (`src/game/map_scroll_fuzz.cpp`,
the harness copied from `map_layers_fuzz.cpp`). Each original is cloned with
every call re-aimed at a recording stand-in - `Field_ViewReset`'s calls of the
three set-up functions and of `MapView_PlaceRuns` too, so each is tested
alone - and ours runs with the same stand-ins through `map_scroll::g`. For
`AreaMap_SetupEntries` all 64 slots of `AreaMap_SetupHandlers` become numbered
stand-ins, put back afterwards. Compared each round: every byte of the
function's state (the harness refuses overlapping regions), and the stand-ins'
log - a count, a hash of every entry, the first 48 kept.

The stand-ins give back what the caller reads and disturb what it reads again:
the ring heads during a walk (kept inside the ring), the input flags after the
elevation call, `AreaMap_PatchBase` after the palette call, a set-up entry's
step byte and a patch entry's length after their calls, the two quad words a
`Gpu_SetPolyFT4` / `Gpu_SetShadeTex` stand-in writes before the caller clears
them. The elevation stand-in returns results with noise above bit 15 and with
bit 15 set, so a caller that keeps 32 bits is seen (R11). Where a stand-in
records more than the real callee could observe - the heads, the watched
scalars at each `Field_ViewReset` call - the control it refuses is noted as
such below.

The run of 2026-09-22 (`build/bof3x.log`, the same under `BOF3X_SHADOW=*`,
69 self-test lines, 219 injects):

| Function | Rounds | Coverage | Mismatches |
|---|--:|---|--:|
| `MapView_ShiftColumnPrev` | 20,000 | 5,714 with the head column at an edge, 5,342 with the head row; 2.24 M stand-in calls | **0** |
| `MapView_ShiftColumnNext` | 20,000 | 5,692 / 5,314; 2.24 M | **0** |
| `MapView_ShiftRowsPrev` | 20,000 | 5,748 / 5,368; 2.24 M | **0** |
| `MapView_ShiftRowsNext` | 20,000 | 5,698 / 5,384; 2.24 M | **0** |
| `MapView_PlaceRuns` | 20,000 | 254,578 runs, 129,326 placed in the view, 144,977 aimed at an edge of it (no calls) | **0** |
| `Field_ViewReset` | 600 | 287 with the elevation call, 126 of those below 0; 310 with `AreaMap_Word1E` set; 3.4 M stand-in calls | **0** |
| `AreaMap_BakePatches` | 12,000 | 36,647 entries, 15,191 baked, 5,234 refused by the first record's top byte; 3,006 rounds with an entry that shortens itself (no calls) | **0** |
| `AreaMap_ClutCycleStart` | 30,000 | 29,604 cycle entries, 49,307 of other kinds; 13,668 rounds copied a row into a clean strip; 12,996 with the counter aimed at a listed frame (no calls) | **0** |
| `AreaMap_SetupEntries` | 30,000 | 118,828 set-up entries laid, 99,231 handler calls; 174,291 patch entries laid, 84,580 patch calls, 30,000 palette calls | **0** |

The area block the fuzz owns is laid out so that every write the original can
make stays inside it: the cell grid's words below 0x200, the texture run's
first 0x900 dwords small in both halves (a record just outside the grid reads
a run word as its tile), the patch list beyond all of that. One record per
round in a quarter of `AreaMap_BakePatches`' rounds is aimed at its own
entry's header and shortens the entry from 6 dwords to 3 - the only way to
see the length being re-read (B5), with the next entry laid where the shorter
length puts it.

**Negative controls.** Each a rebuild and a start-up run, reverted after
(a scratch script, one planted change per run). All refused with exit 3, by
comparison, except the two marked:

| Control | Mismatches |
|---|--:|
| S1 `ColumnPrev` wraps 0 to 0x1A | 725 |
| S2 `ColumnPrev` fills view column 0, not -1 | 20,000 |
| S3 `ColumnPrev` reads the head column once (stand-in) | 19,981 |
| S4 `ColumnNext` origin y + 1 | 20,000 |
| S5 `ColumnNext` stores the column before the walk (stand-in) | 19,999 |
| S6 `RowsPrev` view rows -3 then -2 | 20,000 |
| S7 `RowsPrev` reads the head row once a pass (stand-in) | 19,980 |
| S8 `RowsNext` wraps at 0x36 | 5,489 |
| S9 `RowsNext` re-reads the head row each pass | **0** |
| S10 `RowsNext` origin y unchanged | 20,000 |
| S11 `RowsNext` reads the head column once for both passes (stand-in) | 18,795 |
| P1 `PlaceRuns` refuses d = 0 | 9,549 |
| P2 `PlaceRuns` stores the offset without + 1 | 16,372 |
| P3 `PlaceRuns` origin x and y swapped | 16,277 |
| P4 `PlaceRuns` row without + 1 | 16,372 |
| R1 `ViewReset` GTE offset (0x78, 0xA0) | 600 |
| R2 `ViewReset` origin y + 2 | 600 |
| R3 `ViewReset` tests input bit 2 | 315 |
| R4 `ViewReset` elevation offset + 2e | 137 |
| R5 `ViewReset` input flags not re-read after the call | 45 |
| R6 `ViewReset` pool top 0 | 600 |
| R7 `ViewReset` quad word +0x46 not cleared | 600 |
| R8 `ViewReset` cell word cleared after `MapView_CellToMap` | 600 |
| R9 `ViewReset` head row 0x36 | 600 |
| R10 `ViewReset` set-up called before the bake | 600 |
| R11 `ViewReset` elevation kept at 32 bits | 213 |
| R12 `ViewReset` `AreaMap_Word1E` ignored | 310 |
| R13 `ViewReset` `Camera_AnglesDrawn` word 2 copied before the yaw is set | 600 |
| R14 `ViewReset` item 0's point +0x84 kept | 600 |
| R15 `ViewReset` focus z from 0x7FFF | 600 |
| R16 `ViewReset` shade-tex argument 1 | 600 |
| R17 `ViewReset` camera distance zeroed after the elevation call (stand-in) | 287 |
| B1 Bake on code 0x8000 | 8,862 |
| B2 Bake leaves bit 0 of the code | 7,206 |
| B3 Bake first record's top byte < 3 | 1,499 |
| B4 Bake nibble from bits 20..23 | 6,313 |
| B5 Bake length not re-read | 3,003 |
| B6 Bake half without + 1 | 1,632 |
| C1 ClutStart scan stops a frame early | 2,931 |
| C2 ClutStart takes kind 0 as well | 8,272 |
| C3 ClutStart f without + 1 | 2,931 |
| C4 ClutStart dirty flag not set | 13,668 |
| C5 ClutStart period a word | **crash** |
| C6 ClutStart period forced odd | 5,181 |
| E1 Setup handler index 5 bits | 22,260 |
| E2 Setup step read before the handler | 14,657 |
| E3 Setup `Cond_ByteFF` not masked | 29,773 |
| E4 Setup patch length read before the call | 2,408 |
| E5 Setup patch base read before the palette call | 7,314 |
| E6 Setup without the palette call | 30,000 |

Two did not end in a count:

- **S9 is a change that changes nothing.** Re-reading `MapView_Row` from
  memory at the start of each pass reads back exactly what the pass before
  stored, and the first pass reads it from memory anyway. No input can tell
  the two apart, so the "read once, then keep it" reading is the only
  observable one. (Trap: a control that is not refused may be a change that
  changes nothing - checked before calling the fuzz blind.)
- **C5 ended the process with an access violation** (0xC0000005) rather than a
  count: a period taken from the whole word makes `f` larger than any top byte
  in the list, the scan runs past the `0xFF` sentinel and off the block. A
  crash proves less than a count, so C6 - the period forced odd, which stays
  inside 1..0xFF - was run in its place and refused by comparison. It is also
  the original's own trap: the period **is** a byte here, and a word in
  `AreaMap_ClutCycle`.

Two more candidates were dropped before running as changes that change
nothing: `MapView_PlaceRuns` caching the run dword instead of re-reading it
for the step (nothing in the loop writes the list), and `Field_ViewReset`
reading `Field_Kind2X` / `Z` again after the elevation call (nothing between
writes them).

**What the fuzz does not reach.** The real callees: every one is a stand-in,
so the live batch check is the only test of the whole - and for
`Field_ViewReset` that means the view of an area as it is first drawn. The
unbounded cases in section 4 are read, not run.

## 6. In game: what the attract cycle reaches, and what it does not

All nine run in a whole attract cycle (`hidden_b`, counts in section 1), so the
batch check's `--original` list should carry all nine names. Inside them:

- **All four shifts are reached**, in both directions of both axes, and
  `MapView_PlaceRuns` after them (531 calls from `AreaMap_Frame`, 12 from
  `Field_ViewReset`). The attract presses nothing, so those 543 are the
  attract's own camera following the kind-2 object; **walking reaches them
  faster and in a chosen direction** - `tools/recipes/camera_rotate.txt`
  already holds a plain direction for 60 frames on save 5's field, and
  `field_view.txt` loads that field without walking. A camera turn (R1 + a
  direction) does **not** shift: it moves `Camera_Angles`, which
  `AreaMap_Frame` answers with a rebuild, not a scroll.
- **The two lists are empty in every attract area.** `AreaMap_SetupEntries`
  ran 12 times and made **no** handler call (no caller `0x571754` in
  `hidden_b`; the three handlers `0x571880`, `0x571A30`, `0x571A70` are in
  `entries_hidden.txt` and have no count) and **no** `AreaMap_ApplyPatch` call
  (`0x571110` is armed in `entries_plus_hidden.txt`, count absent). So
  `AreaMap_BakePatches` walked nothing either: its 12 calls did no work.
  What would reach them is an area whose header has a set-up or patch list -
  unknown; the way to find one is a scan of the area blocks in the `DAT`s for
  a non-zero header word +0x20 / +0x28, or a temporary probe counting the
  entries at each set-up.
- **`AreaMap_ClutCycleStart`'s copy is unmeasured.** It ran 12 times, but
  whether any attract area has an entry with top byte exactly `0x80` is not
  known from the trace (the per-frame `AreaMap_ClutCycle` runs 29,701 times,
  which says only that cycles of *some* kind exist). A probe on
  `Gfx_ClutStripDirty` at set-up, or a scan of the header entry lists, would
  answer it.
- **`Field_ViewReset`'s input-flag branch**: `AreaMap_Elevation` is called
  from `0x56F815` all 12 times, so bit 3 of `Field_InputFlags` was clear every
  time; the branch that skips the elevation is not reached.
- **Its other callers**: only `0x594F7B` (area set-up) calls it in the attract.
  The other 43 sites - `Kind2_Script`'s among them - and the second view
  initialiser `0x4B5350` (which calls `MapView_ShiftRows*`,
  `AreaMap_BakePatches` and tail-jumps to `AreaMap_SetupEntries`) are not
  reached.

## 7. Possible defects and open questions

Nothing here looks like a defect of the 2001 code: every quirk above is
Capcom's on both platforms (the PSX twins were read this time) or unobservable.
No `DIVERGENCE.md` entry was needed - no stack padding reaches memory in any of
the nine, and nothing was changed.

Open:

- `AreaMap_SetupHandlers`' three handlers `0x571880`, `0x571A30`, `0x571A70`
  are unread, and so is the set-up entry format they take.
- `AreaMap_ApplyPatch` `0x571110` is read only to the level section 1 states;
  its draw-item side (`0x572ED0`) is unread.
- `Cond_ByteFFSource` `0x8034E1`: what the byte means, and who writes it.
- `0x4B5350`, the other initialiser, is unread - it is the only other caller
  of the row shifts and of `AreaMap_BakePatches`.
- The 0x18 and +3 in `MapView_Origin`'s set-up are the view's half-size in
  cells; nothing confirms that reading beyond the arithmetic.
