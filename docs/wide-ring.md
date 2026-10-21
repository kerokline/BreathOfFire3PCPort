# The field's view ring grown for the wide picture (wide-ring round, group RG)

**Status:** BUILT (2026-10-21), branch `wide-ring/rg` off the round branch
`wide-ring/round` (`83345294`, RM's read). IDEAS I37. Built to
[`wide-ring-read.md`](wide-ring-read.md) section 6 with the owner's four
answers to its section 7 (chat, 2026-10-21). The checks are section 4;
the ledger text, drafted for the orchestrator, section 6.

## 0. The answer

- **At 16:9 and 21:9 the field's draw walks the PSP's 33 x 58 ring; at 4:3
  nothing changed.** Capcom's 28 x 56 ring (`MapView_CellItems`,
  `MapView_Cells`, the heads, `MapView_Origin`) is kept exactly as it is at
  every width, so every logic reader of its window - `MoveCmd_TestFB` / `FC`,
  `AreaMap_ClearCell`, `Area40_TileLit`, the Quake walk - and DIV-0071's
  `layering.cpp` see Capcom's window. Beside it, a **rim** of ours holds the
  346 cells the PSP's ring has beyond Capcom's: one row above, one below,
  two and a half columns either side. Only the draw reads the rim.
- **Sprites in the grown rows stay hidden** (owner's answer 1): the
  off-screen flag and the draw list stay at `0..0x36`; the pass draws
  terrain and frame nodes at the two new layers, never a sprite.
- **The walk is today's plus one column** (answer 3): grown columns
  `g .. 32 - g`, `g = max(0, i + 2 - L)`.
- **Every cell only the grown walk adds is a band cell**: its
  `MapCell_DrawRising` numbers come from the draw-only generator. Capcom's
  cells are walked in Capcom's order inside the grown walk, so Capcom's
  `Rand` is the narrow frame's call for call (section 4.1).
- **D295's rule is kept** (answer 2): the patch handlers 0x24 / 0x25 run in
  band cells as before; the gate decides (section 4.3).
- **A quickload across widths rebuilds the rim** (answer 4), never refuses.
- **`BOF3X_WIDE_RING=0`** turns the rim off at a wide picture (Capcom's ring
  alone, the 2026-10-13 build's picture), for A/B and as the reversal.

## 1. What was built

### 1.1 The rim: `src/game/map_rim.{h,cpp}` (new)

| What | Where | |
|---|---|---|
| The gate | `map_rim.cpp:61` `Active()` | armed by `Arm()` (`:672`) after every module's self-test, when `Widescreen_InsetColumns() != 0` and `BOF3X_WIDE_RING` is not 0; forced by the self-tests |
| The geometry | `:63` `InWindow`, `:70` `RingRow`, `:72` `RingColumn`, `:77` `Cell`, `:83` `RunWord` | grown `(gr, gc)`, `gr` 0..57, `gc` 0..32; Capcom's view cell `(r, c)` is grown `(r + 1, c + 2 + (r odd))` (read 2) |
| The grown map position | `:88` `GrownCellToMap` | `MapView_CellToMap`'s arithmetic on the PSP's frame, origin `+(-3, +2)`, never Capcom's function at row -1 |
| The shifts' rim half | `:120` `RimShift` | one function, four directions: content moves with the map (`new(p) = old(p + step)`), a rim cell leaving the grown ring or entering Capcom's window releases its items, a rim position fed from Capcom's window or past the edge is filled fresh |
| The fill | `:153` `RimFill` | reset (items 0) or refill in place (items kept), then the runs |
| The runs | `:169` `RimRuns` | `MapView_PlaceRuns`' arithmetic, a run at `s' = s + 1` in 0..0x39 and `d' = d + 5` in 0..0x41 outside Capcom's window to the rim's word |
| The walk's inset | `:191` `NoteWalk`, `:197` `Walk` | noted by `MapView_Build` beside the inset byte; `DrawLayer_Open` asks with the byte it read |
| Layer -1 | `:203` `ExtraLayer`, `:205` `ExtraReset`, `:217..238` the table | one `0x30`-byte record shaped as `DrawLayers`' and the row's draw-table cells, sorted as `DrawTable_Sort` sorts |
| Draw items by map cell | `:240` `RimItemAt` | for `MapView_ItemAt` and `MapCell_DrawFlatFaces` outside Capcom's window |
| Peaks | `:248` `NoteBuildPeaks` | `wide-ring   peak at frame F: draw items N of 2048 in use, draw table T of 256 (area A)`, a line at each new peak |
| The quicksave | `:271` `QuickSave`, `:292` `QuickRestore` | section 3 |
| The model test | `:353..670` | section 4.1 |

### 1.2 The sites (each changed only when `map_rim::Active()`)

| Site | File:line | Change |
|---|---|---|
| The four shifts | `src/game/map_scroll.cpp:98`, `:118`, `:141`, `:166` | `RimShift` after Capcom's body |
| `MapView_PlaceRuns` | `map_scroll.cpp:201` | `RimRuns` after Capcom's half |
| `Field_ViewReset` | `map_scroll.cpp:285` | `RimFill(false)` after the 56 x 28 fill |
| `Weretiger_ResetMapView` | `src/game/magic_s14.cpp:895` | the same |
| `MapView_FillCells` | `src/game/scena_sx.cpp:372` | `RimFill(true)` |
| `MapView_Build` | `src/game/map_layers.cpp:503` | the grown walk, rows -1..55 (grown 0..56), columns `g .. 32 - g` through `Cell`; `BuildCell` takes an `int` layer, `LastOf` (`:84`) routes layer -1 to the record, a layer -1 table cell to the rim's table; Capcom layer 0's `0x8000 | 0x4000` cells go to layer -1, as the PSP's |
| `DrawLayers_Reset` | `src/game/draw_layers.cpp:115` | the record's two lists and the table reset too, for every caller (`AreaMapBD_BuildView` among them) |
| `DrawLayer_Open` / `_Close` | `src/game/draw_emit.cpp:440` (`OpenGrown`), `:505`; `_Close` and `LinkFirst` take layer -1 | section 2 |
| `Sprite_DrawPass` | `src/game/draw_pass.cpp:275` (`GrownLayerAbove`), `:282` (`GrownLayerBelow`), `:326`, `:434` | layer -1 before 0 (Open, its table cells, Close), layer 0x37 after 0x36 (Open, frame node 55, `DrawTable`'s layer-0x37 entries, Close); no sprite at either |
| `MapView_ItemAt` | `src/game/field_misc.cpp:304` | outside Capcom's window, the rim's item |
| `MapCell_DrawFlatFaces` | `src/game/map_field_objects.cpp:296` | the same |
| The graphics participant | `src/render/gfx_snapshot.cpp:203`, `:298` | the rim's record after `wide_rand`'s; the record's version 1 -> 2 |
| Arm | `src/hook/inject_all.cpp:1474` | after every module's self-test, before `DrawPool_Grow` and `GfxSnapshot_Inject` |
| `wide_rand` | `src/game/wide_rand.cpp` | the grown boundary and the grown fuzz (section 4.1); `Row` unchanged (read 4) |

Unchanged, as the read said: `Sprite_UpdateScreen` (`sprite_screen.cpp`),
`Sprite_ProjectA`, `MapView_CellToMap`, the origin writers, the logic
readers, `MapView_LinkPrimAt`, `MagicFx_LinkByDepth`, the fixed-layer
effects, `AreaMapBD_BuildView`, `Gfx_BeginFrame` (it resets 0x38 frame nodes,
55 among them; row -1 has none). `layering.cpp` is FY's and was not
touched: the rim is invisible to it (read 6.4).

## 2. The draw walk

**`MapView_Build`** walks grown rows 0..56 and, in each, grown columns
`g .. 32 - g` ascending, with `g = max(0, i + 2 - L)` clamped to 0..16 (`i`
the original inset, `L` = `Widescreen_InsetColumns()`). Capcom's window
cells go through Capcom's slots, so their draw items are where `layering.cpp`
and `MapView_ItemAt` read them. The walk is a superset of today's: today's
takes Capcom columns `i_w .. 27 - i_w` (`i_w = max(0, i - L)`), grown
`i_w + 2 .. 29 - i_w` on Capcom's even rows and `i_w + 3 .. 30 - i_w` on
odd ones, and `g <= i_w + 2`. Unlike Capcom's walk ours does not re-read
`Scratch_Swap` between cells (the walk is ours; the byte is left as
`Inset()` stored it).

**`DrawLayer_Open`** under the grown ring (`OpenGrown`) takes, for layer
`L` (grown row `L + 1`), the PSP's walk of that row (its `0x101BC`): grown
inset `g` less 2 with `Field_InputFlags` bit 0, first grown column `g + (1
for an even grown row)`, `33 - 2g` columns wrapping at 33. On Capcom's
layers 0..0x36 **Capcom's own walk of the row runs unchanged in the middle**
- its columns, its order (the wrap at inset 0 included), its `wide_rand::Row`
split - and the grown columns left of it run before it, those right of it
after. Each of those is a band cell: it lies outside today's walk, and the
original's walk is inside today's (the original inset is never below the
lowered one), so it lies outside the original's too. Layers -1 and 0x37 are
rows the original's pass never walks: every cell there a band cell. Rim
words come from the rim. DIV-0071's run recording is given Capcom's walk's
cells only, as `layering.cpp`'s copy of the walk knows no others.

## 3. The quicksave

The record (`map_rim.cpp:271`): a version, the inset columns at the take (0
narrow), and when wide the rim's cells and run words, the layer -1 record and
table, and the noted walk. Restored after `wide_rand`'s, after the core has
written the span and the pool back:

- **wide take, wide load** (16:9 and 21:9 alike, the same 33 x 58): the
  record as is; a redraw when the widths differ.
- **wide take, narrow load:** each rim cell holding an item is released to
  the pool (`DrawItemPool_ReleaseCell` on the record's copy); a redraw.
- **narrow take, wide load:** the rim filled afresh (positions, items 0, the
  runs), the layer -1 record emptied; a redraw.

The redraw is `MapView_Redraw` raised to **2**, not RM's 1: the layer lists
are per display buffer and a build fills only the current buffer's, so 1
would leave the other buffer's lists naming the take's rim items for a frame.
`MapView_Redraw` is a picture byte (census 3's picture page `0x905000`).

## 4. The checks

### 4.1 Self-tests (start-up, headless)

From the clean launcher copy `RG/L2` (no ini), `BOF3X_LANG=original`
`BOF3X_SELFTEST_ONLY=1`, `BOF3X_SHADOW=map_rim,wide_rand,map_scroll,
map_layers,draw_layers,draw_emit,draw_pass,field_misc,map_field_objects,
scena_sx,magic_s14,map_view,draw_pool,gfx_snapshot`: exit 0, every
module's line 0 MISMATCHES. The planted-bug check is in the first bullet.

- **`map_rim`'s model test:** 40 sequences, 10,040 steps (shifts in runs of
  one direction, builds that hand cells items, resets, origin jumps of
  0x14 / 0x1E / 0x20 with `MapView_FillCells`) on a 100 x 100 test map with
  3,000 runs, against a 58 x 33 ring of the test's own kept by the PSP's
  twins (heads 0x39 / 0x20, shifts at 0x3A / 0x21, origin `+(-3, +2)`, runs
  at `s' < 0x3A`, `d' < 0x42`): **0 steps** with a cell or run word
  differing, **0** with the pool's count off the items held or an index
  held twice; 512,356 rim run words and 540,503 rim items seen. The control
  (the rim filled by Capcom's `MapView_CellToMap` at the grown row) is
  refused (216 of 244 steps differ). A planted bug - the release of rim
  cells entering Capcom's window dropped - was caught: 8,446 steps with the
  pool's count off, Fatal.
- **`wide_rand`'s grown cases:** the boundary over the grown walk at 53 and
  120 columns - original insets 0..14, the input flag, three heads (0x37,
  0x13 and an even 0), layers -1, 0, 1, 2, 0x35, 0x36, 0x37, every view
  column, 17,640 rows each - every cell handed over is a grown column or one
  of Capcom's walk, a band cell exactly when the original's walk leaves it
  out, Capcom's `Rand` calls the original's cells' in the original's order:
  0 mismatches, 0 out of order (397,488 cells handed over at 53, 75,936 of
  the rim; 475,104 at 120, 102,144 of the rim). The grown fuzz, 3,000 frames
  -1..0x37 at 53 and 120 against narrow's 0..0x36: Capcom's `Rand` as
  narrow's call for call in all 3,000; the control (band cells on Capcom's
  `Rand`) refused in 2,294 frames at each.
- **The narrow fuzz of every touched function:** `map_scroll`, `map_layers`, `draw_layers`, `draw_emit` (`Gfx_CommitPrim`, `DrawLayer_Close`, `DrawLayer_Open`), `draw_pass`, `field_misc`, `map_field_objects`, `scena_sx`, `magic_s14`, `map_view`, `draw_pool` and `gfx_snapshot` each 0 MISMATCHES in the run above; narrow and in every module's fuzz `map_rim::Active()` is false, so the rim code never runs there.
- **The full `BOF3X_SHADOW='*'`:** from `RG/L2` at `12c63c64`, narrow,
  `BOF3X_SHADOW='*'` `BOF3X_LANG=original`: exit 0, no Fatal, 1,089 lines
  naming MISMATCHES and every one reading 0 (803 self-test lines), `inject:
  10087 ours`.

### 4.2 The state hash

Runs: `BOF3X_OPT=none BOF3X_LAYERING=0`, the skip list, no overlay, speed
1, the exe host, from the ini-less launcher copy of `790ef5f5` (the
`12c63c64` change after it only refuses to arm when a site is left original,
which no run here does). Scripts and outputs in
`C:\Users\kerok\bof3x-runs\r_ring\RG` (`chain_hash.py`, `gate.py`,
`narrow_vs_base.py`, `sh/`).

**A trap paid for:** runs in the main checkout's `bof3\` share slot 0 with
every other session's runs. Mine were disturbed twice - my own capture runs
and another session's `shop.txt` swapped slot 0 under the shop and
`dragonTransform` at 21:9 (`slot 0: the lock is pid 11536's now, not ours`),
which loaded another save and parted every page from the load. Those two
routes were run again at all three widths in a game directory of my own
(junctions to `DAT` / `BGM` / `SND`, the root files copied: `RG/game2`),
tag `rgp`; the base and the controls ran there too.

**Narrow, against the round base** (`83345294` built apart, `basew0`), the
claim that matters - nothing changed at 4:3:

| Route | RG narrow against the base, narrow (`statehash.py diff`) | `Rand` |
|---|---|---|
| combat | 8 pages: the exe host's tick-3 pages (VRAM, sound, `0x937000`), `0x7E1000` at the last tick | equal, 2,621 frames |
| shop (`rgp`) | 7: tick 3, the sound page 3..1,710 (119 ticks) | equal, 3,158 |
| worldMapAndAreaTransition | 8: tick 3, the D3D page `0x6C3000` from 1,098 | equal, 2,144 |
| field_menu | **identical on all 1,409 ticks** | equal, 1,455 |
| menu_screens | **identical on all 1,729 ticks** | equal, 1,771 |
| dragonTransform (`rgp`) | 1: the sound page (1,289 ticks) | equal, 4,330 |
| caughFish | 56: tick 3, 48 VRAM pages at ticks 450..456 (one upload a few ticks apart), the sound page, `0x6C3000` | equal, 3,890 |
| masterAndManillo | 8: tick 3, `0x6C3000` | equal, 5,520 |
| cutsceneAndNue | **identical on all 7,425 ticks** | equal, 7,422 |
| whelpBoss | 1: the sound page at ticks 193..199 | equal, 13,182 |
| dolphinBoss | 8: tick 3 and the sound page at isolated ticks, `0x6C3000` | equal, 14,923 |

No game page differs on any route: the sound page, the exe host's tick-3
pages, VRAM a few ticks apart and the D3D cache page `0x6C3000` are the
noise classes [`state-hash.md`](state-hash.md) and census 3 name (the base
ran in `RG/game2`, RG's narrow mostly in `bof3\`).

**Narrow, against `cn_<route>_orig3` / `_orig3b`:** combat identical on all
2,561 ticks (exit 0). The other nine differ from the pair only as the round
base does (above): the music pages of DIV-0092 / DIV-0150 (`field_menu`,
`menu_screens`, `dragonTransform`, `cutsceneAndNue`, `whelpBoss` -
`Music_LoadedTrack`, `0x904000`'s track byte, `0x6BD000`), `Sound_Banks` on
`whelpBoss` from tick 9,541 (`0x6BC000`, HANDOFF's class), the D3D page
`0x6C3000` and the world map's legend pages, and the exe host's tick-3 pages.

**Wide against narrow** (`gate.py`; "picture" is census 3's set - the
static pages, the draw and map-view pages, `Desktop_Width`'s, the window x
words, the message pen x on `0x7E1000` - plus the D3D page `0x6C3000` and
`Display_DesktopWidth`'s `0x7C9000`):

| Route | 16:9 | 21:9 | `Rand` |
|---|---|---|---|
| combat | picture, 10 pages | picture, 11 | equal on all 2,621 frames, both |
| shop (`rgp`) | picture, 16 (5 VRAM at tick 3) | picture, 17 | equal, 3,158 frames |
| worldMapAndAreaTransition | picture, 12 | picture | equal, 2,144 |
| field_menu | picture, 9 | picture, 10 | equal, 1,455 |
| menu_screens | picture, 11 (`0x7E1000` 1235..1490, the pen x) | picture, 12 | equal, 1,771 |
| dragonTransform (`rgp`) | picture, 11 | picture, 17 (5 VRAM) | equal, 4,330 |
| caughFish | picture, 12 | picture, 13 | equal, 3,890 |
| masterAndManillo | picture, 12 | picture, 13 | equal, 5,520 |
| cutsceneAndNue | picture, 13 (pen x 936..5113) | picture, 14 | equal, 7,422 |
| whelpBoss | picture, 14 (pen x 1384..11771) | picture, 15 | equal, 13,182 |
| dolphinBoss | 27 pages, `Sprite_Objects` from 3,235 (see below) | the same 27 | equal, 14,923 |

**`dolphinBoss`:** `Rand` equal at both widths on all 14,923 frames, but
`Sprite_Objects`, `MapView_ScrollX` and `Gfx_UnpackScratch` part from narrow
at ticks 3,235..9,901. Not the ring's: the controls at 16:9 in the same
game directory - **`BOF3X_WIDE_RING=0`** (the 2026-10-13 wide picture)
against the ring on differs on **picture pages only** (13, no object page);
**`BOF3X_WIDE_STAGING=0`** (ring on) against narrow has **no object page**
(the window x words and the field's hidden-object page, picture). So it is
DIV-0122 / DIV-0123's staging, as census 3 found it ("the staging changes
the scene's timing, so the recorded route parts"; its 2026-10-14 run parted
`Rand` from 10,921 too, which this build's does not).

### 4.3 D295

No route's game pages part with the grown walk (4.2): the rim's, row
55's and row -1's patch cells (handlers 0x24 / 0x25) never moved a game page
these routes compare. D295's rule stands as the owner kept it; band cells
still run the patch handlers.

### 4.4 The draw-item pool and the draw table, wide

The last `wide-ring   peak` line of each run (`sh/cn_<route>_rgw<N>.log`):

| Route | 16:9 items (area) | 21:9 items (area) | draw table, most |
|---|---|---|---|
| combat | 711 (29) | 856 (29) | 0 |
| shop | 791 (7) | 928 (7) | 0 |
| worldMapAndAreaTransition | 1,089 (33) | 1,319 (33) | 0 |
| field_menu / menu_screens | 281 (141) | 328 (141) | 0 |
| dragonTransform | 550 | 825 (7) | 0 |
| caughFish | 1,102 (16) | 1,336 (16) | 0 |
| masterAndManillo | 1,225 (45) | **1,469** (45) | 0 |
| cutsceneAndNue | 840 (23) | 1,023 (23) | 0 |
| whelpBoss | 952 (10) | - | 9 |
| the far-camera captures (bridge, area 65, distance 2,560) | - | 1,449 | 0 |

**DIV-0062's 2,048 holds**: the most any run used is 1,469 (72 %), at 21:9.
No `draw table full` and no `no draw item` line in any run. The draw table's
most is 9 of 256 (whelpBoss).

### 4.5 The quicksave across widths

`tools/recipes/quick_worldMapAndAreaTransition.txt` (F6 at recipe frame
595, the party on a path of area 7, mode 2; F7 at 895 and the route again)
live at one width, then `_cross.txt` (F7 at frame 0) in a second process at
another, in my own game directory (`RG/game`, so no other session's slot or
quicksave history was touched), `quicksave_check.py cross`:

| Take -> load | The load's log | Pages that differ from the live run after the restore |
|---|---|---|
| 16:9 -> 4:3 | `taken wide, loaded narrow - the rim's 0 draw items given back, a redraw asked` | 16, every one a picture page (draw, map view, display, VRAM a tick, `0x6C3000`); **no game page** |
| 4:3 -> 16:9 | `taken narrow, loaded wide - the rim filled afresh, a redraw asked` | 16, picture only |
| 21:9 -> 16:9 | `the rim as at the take (taken at inset columns 6, this run 3)` | 17, picture only |
| 16:9 -> 16:9 (the control) | `the rim as at the take` | 6: the sound page and VRAM for a few ticks, `0x6C3000` |

Every load loads; none refuses. The live runs' in-process proofs at 16:9 and
4:3: "the 300 ticks after the restore equal the 300 after the take, tick for
tick". The 4:3 picture 30 and 60 frames after a 16:9 take's load is the field
as the route has it (`RG/qs2/cross_w1_to_w0/shots`). At the field camera the
16:9 take held no rim items (all culled at the picture's edge), so the
"given back" count is 0 there; the release path itself is the model test's
(4.1). A first try with the `_keys` form as the live run compared the cross
load against inputs 300 frames on - not a valid pairing; `qs2` is the run
reported.

### 4.6 Captures

Point filter, 21:9 and 16:9, each with the ring on and with
`BOF3X_WIDE_RING=0`, from launcher copy `RG/L4` in `RG/game2`; shots in
`RG/shots/far_*`, the sheet `RG/sheets/far_bridge_ring_offon.png`.

- **Where the far cameras were taken:** the three areas UC's model names
  (2,176..2,560) are not named in its doc and none is reached by a hash
  route; this source writes 0x980 in area 167's init (story flags 0x8F set,
  0x90 clear), +0xA00 in area 19's handler and a mode state, and 0xA00 in
  chapter 1's scene 3. In place of those scenes the captures **poke
  `Camera_Distance` to 2,176, 2,432 and 2,560** (and the yaw to 0x300 / 0x400)
  with a redraw, an experiment's recipe (`RG/mkcap.py`): on
  `bridgeWalk.txt`'s save at its end (area 65, the bridge over the sea), and
  on the chapter-jump save `ending17` (chapter 17, area 199's dune, from
  recipe frame 2,480).
- **What they show:** at 21:9 and 16:9 the **ring's top edge is the
  difference** - the original's saw-toothed missing row against the clear
  colour at the picture's top is filled by the grown row (rows 0..14 of the
  21:9 picture at 2,560; up to 66 rows with the far camera and yaw 0x400).
  The sides were already covered on this ground (the inset reaches 0 at
  2,560); off-map edges (the area's own border, the grid at the top left)
  are the map's, the same with the ring off. On the ending's dune the
  scene's own camera keeps the picture inside Capcom's ring: ring on and
  off are pixel-identical.
- **Owed the owner's eye:** a far camera in the three real scenes (their
  saves or flags), and kind 0x8D's pull.

## 5. What stays as it is

- `layering.cpp` (DIV-0071, FY's file) sees Capcom's window only (read 6.4).
- The Quake walk recodes Capcom's runs only: the rim's terrain stays opaque
  while MAGIC102's quake runs (read 6.3).
- Kind 0x8D's first frames need more than 33 columns (17.5 against 16.5 half
  columns at 16:9, 23 at 21:9): D292 narrows to them.
- The draw-order tagger names a rim cell `0x100 | grown row` and its grown
  column.

## 6. Ledger text, drafted for the orchestrator

### 6.1 DIV-0041, amended

- **Amended 2026-10-21 (the wide-ring round, group RG,
  [`wide-ring.md`](wide-ring.md), [`wide-ring-read.md`](wide-ring-read.md)):
  the field's view ring grown to the PSP's 33 x 58 for the draw.**
  *Original behaviour:* the field's view is a ring of 56 rows by 28 cells
  (`MapView_CellItems` `0x937FA0`, `MapView_Cells` `0x904F20`, the heads
  `MapView_Row` / `_Column`, `MapView_Origin`); `MapView_Build` and the draw
  pass walk 55 of its rows and at most 28 columns, so at the wide picture a
  far camera (`Camera_Distance` past about 2,200, kind 0x8D's pull) or an
  off-axis yaw over low ground runs out of cells at the picture's edges
  (D292). *New behaviour:* when the cell inset is lowered (the view wide),
  Capcom's ring is kept exactly and a rim of ours holds the 346 cells the
  PSP's 58 x 33 ring has beyond it (its origin Capcom's plus (-3, +2), the
  PSP's `Field_ViewReset` `0xE560`), kept by the same four shifts, the
  resets, `MapView_FillCells` and `MapView_PlaceRuns`, and read by the draw
  alone: `MapView_Build` walks grown rows -1..55 and grown columns
  `g .. 32 - g`, `g = max(0, i + 2 - L)` (today's walk plus one column);
  `DrawLayer_Open` walks the PSP's columns of each row round Capcom's own
  walk, which runs unchanged inside it; `Sprite_DrawPass` draws layer -1
  (a record of ours) and layer 0x37 (Capcom's row 55) - terrain and frame
  nodes, never a sprite; `MapView_ItemAt` and `MapCell_DrawFlatFaces` read
  the rim's draw items. The logic readers of the window - `MoveCmd_TestFB` /
  `FC`, `AreaMap_ClearCell`, `Area40_TileLit`, the Quake walk - keep
  Capcom's 56 x 28 (the PSP grew those too; its `TestFB` twin `0xEDC0` tests
  `s < 0x3A`, `d < 0x42`), and `Sprite_UpdateScreen`'s `0..0x36` test keeps
  setting the off-screen flag and the draw list as on the original: a
  sprite in the grown rows stays hidden. Every cell only the grown walk
  hands to `MapCell_Handlers` - the rim, Capcom's row 55, the column it
  adds inside the window - is a band cell of the random split, its
  `MapCell_DrawRising` numbers from the draw-only generator; D295's rule for
  the patch handlers 0x24 / 0x25 is kept. The rim is a record of the
  graphics participant; a quickload across widths rebuilds it (a wide take
  loaded narrow gives the rim's draw items back, a narrow take loaded wide
  fills it afresh) and asks `MapView_Redraw` 2, never refuses. *Why:* the
  owner, 2026-10-15 (build I37 now) and 2026-10-21 (the PSP's 33 x 58 at
  16:9 and 21:9, Capcom's 28 x 56 at 4:3 byte for byte; DIV-0041's lowered
  inset kept; sprites in the grown rows hidden; D295's rule kept; the walk
  today's plus one; rebuild across widths) - widen the picture, not the
  logic. *Verification:* `BOF3X_SHADOW=map_rim`, a model ring of the PSP's
  shape against Capcom's code plus the rim over 10,040 random steps (shifts,
  builds, resets, origin jumps with `MapView_FillCells`): every grown
  position's map cell and run word equal, the pool's count equal to the
  items held, 0 mismatches, the control (Capcom's `MapView_CellToMap` at the
  grown row) refused; `wide_rand`'s grown boundary at 53 and 120 columns
  (every original inset, the input flag, three heads, layers -1..0x37: band
  exactly where the original's walk leaves a cell out, Capcom's `Rand` calls
  in the original's order) and the grown fuzz (3,000 frames -1..0x37 equal
  to narrow's call for call), 0 mismatches; every touched function's fuzz at
  narrow 0 mismatches. Live: at 4:3 the ten hash routes and `dolphinBoss`
  equal the round base tick for tick but for the exe host's tick-3 and
  sound noise; at 16:9 and 21:9 every route keeps narrow's `Rand` on every
  frame and no game page differs (`dolphinBoss`'s object pages part as with
  the ring off - DIV-0122 / 0123's staging); the draw-item pool's most is
  1,469 of 2,048 at 21:9; a quicksave loads across 4:3, 16:9 and 21:9 both
  ways with only picture pages differing from the live run. **Reversible:**
  `BOF3X_WIDE_RING=0` (Capcom's ring alone at the wide picture); narrow
  nothing runs. Code: `src/game/map_rim.{h,cpp}` and the sites
  [`wide-ring.md`](wide-ring.md) 1.2 lists.

### 6.2 D292, its new text

#### D292 — Effect kind 0x8D's camera pull reaches past the field's cell ring at its first frames: a notch at the picture's edges (ours, the wide picture; by the model, not seen)

**Found:** group UC, by the ring's projection model, 2026-10-13
([`ultrawide-culls.md`](ultrawide-culls.md) 3).

**Established:** at the field camera (distance 0) Capcom's 28-column ring
covers 560 columns once DIV-0041's inset reaches 0. Past `Camera_Distance`
about 2,200 it did not: three areas' scenes at 2,176..2,560, kind 0x8D's
pull and off-axis yaws over low ground needed more ring columns than it
has. **Narrowed 2026-10-21 by DIV-0041's wide-ring amendment**
([`wide-ring.md`](wide-ring.md)): the wide modes draw the PSP's 33 x 58
ring, which by the model covers up to about 3,200 (ground 256 below) to
3,700 (level ground) at 21:9 - every persistent distance our source writes.
What remains is **kind 0x8D's first frames**: its pull starts at 7,296 and
falls by 42 a frame to 1,024, needing 17.5 half-columns at 16:9 and 23 at
21:9 against the grown ring's 16.5 - by the model's table, the first 15 or
so frames at 16:9 and about 80..100 at 21:9. No route reaches kind 0x8D; not
seen. Ours (the PSX has no wide view; the PSP's own 33 x 58 leaves the same
frames).

**Status:** open, narrowed; the owner accepts the remainder (the round's
call 1, 2026-10-21). A ring sized for 7,296 (about 50 columns) is the fix if
it is ever seen.

## 7. Questions for the owner

1. **The far-camera scenes themselves.** No route or recipe save reaches
   the three areas' far cameras; the captures poke the distance on two saves.
   **Recommended:** the owner names (or records a save at) one of area 167's
   init, area 19's camera-out or chapter 1's scene 3, and a capture at 21:9
   with `BOF3X_WIDE_RING` 0 and 1 settles the side edges by eye.
2. **`MapView_Redraw` 2 after a load across widths, not RM's 1.** Both
   display buffers' layer lists named the other width's cells; 2 rebuilds
   both. A picture byte. **Recommended:** keep 2.
3. **The extra column inside the window at the normal camera** (answer 3's
   "plus one"): at 16:9 with the field camera the grown walk adds one
   Capcom-window column a row that today's walk skipped, drawn and walked as
   band cells. Nothing is seen to change (the pool's most stays at 72 %).
   **Recommended:** keep, as decided.
