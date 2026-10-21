# The field's view ring, read for growing it (wide-ring round, group RM)

**Status:** MEASURED (2026-10-21), a reading group: no `src/` change. Stage A
of [`wide-ring-round.md`](wide-ring-round.md), at the round base `31249d0c`.
Everything RG needs to grow the field's view ring from Capcom's 28 x 56 to the
PSP's 33 x 58 in the wide modes with 4:3 byte-identical: every reader and
writer of the ring in the image and in our source, the two PSP sites the
placements reading left unpaired, the off-screen flag, the random split, the
quicksave, and the plan. Nothing here was seen in play; every claim cites the
image scan, the PSP `BOOT.BIN`, or a line of our source.

Measurements: `tools/ring_census.py` (new, section 1.1) over `BOF3.exe`;
`tools/psp_elf.py dis` over the PSP's Japanese `BOOT.BIN` (extracted by the PSP
round into a scratch directory, never the tree) at the twins CD1's pairing
named (`psp_cd/pairs2.json`); `tools/pe_xref.py` for the arrays' neighbours.
PSP offsets are `BOOT.BIN` link-time offsets, JP.

## 0. The answer

- **No room in place** (1.2). `MapView_CellItems` ends at `0x939820` and the
  byte right after it is read by `BattleResultWin_ExpToNext`;
  `MapView_Cells` ends at `0x905B60`, `MenuLists_CampChosen`'s address;
  `DrawLayers` ends at `0x802D20`, the window records' pass byte. The PSP
  relocated all three.
- **41 functions of the image touch the ring or its heads** (1.3): the 23 of
  [`psp-widescreen-placements.md`](psp-widescreen-placements.md) 2.4, eleven
  more that read or write it, and seven address folds that only look like it.
  Our DLL has two more readers the image cannot show: DIV-0071's
  `layering.cpp` and the draw-order tagger.
- **The two unpaired PSP sites have PC twins** (2): PSX `0x801565fc` is
  **`DrawLayer_Open`** `0x56FD20` (PSP `0x101BC` walks rows mod `0x3A` and
  `33 - 2 x inset` columns mod `0x21`), and BATTLE.EMI#15 `0x800ac020` is
  **`MagicFx_LinkByDepth`** `0x4FB880` (by position before a call-anchored
  pair, and its PSP twin `0x1487D4` bounds the depth at `0x3A` where ours does
  at `0x38`).
- **The ring has logic readers** (1.4). `MoveCmd_TestFB` / `FC` (the
  movement script's ops, sixty-odd callers), `AreaMap_ClearCell`,
  `Area40_TileLit` and the Quake spell's walk decide by whether a map cell is
  inside the 56 x 28 window, and change area records or answer the puzzle
  from it. **The PSP grew those windows too** (its `TestFB` twin `0xEDC0` tests
  `s < 0x3A` and `d < 0x42`): its logic reaches cells the PlayStation's
  cannot. Ours must not, by the owner's rule for the wide modes (widen the
  picture, not the logic; DIV-0041's 2026-10-13 amendment). The same holds
  for `MapView_Origin` and the heads, which those readers read.
- **So the plan (6) is Capcom's ring plus a rim of ours**, not a relocated
  ring: Capcom's arrays, heads and origin stay exactly as they are at every
  width, logic and narrow untouched by construction; the 346 cells the PSP's
  ring has beyond Capcom's (one row above, one below, two and a half columns
  either side) live in a rim store in the DLL, kept by the same shifts, read
  only by the draw. The grown ring's geometry is the PSP's: its origin is
  Capcom's plus `(-3, +2)` (PSP `Field_ViewReset` `0xE560`: `-0x1B` and `+5`
  against our `-0x18` and `+3`), its row `r' = r + 1`, its half-column
  `d' = d + 5`.
- **The off-screen flag** (3) stays Capcom's with no split at all: a sprite
  in the grown rows stays hidden and undrawn, as narrow. The grown rows draw
  terrain and frame-node primitives only; `Sprite_DrawPass` must not draw a
  sprite at the extra layers, or `Sprite_Draw` would write bit 7 for a type
  0x0A sprite the original never draws.
- **`wide_rand::Row` keeps its 28** (4): the original walk never exceeds 28
  columns. A rim cell, and every cell of Capcom's row 55 (never walked by
  Capcom's `DrawLayer_Open`), is a band cell without asking `Row`.
- **The quicksave** (5): Capcom's ring is in the span; the rim is not, so it
  is a record of the graphics participant. A load across widths is cheap to
  **rebuild** rather than refuse, because Capcom's ring is complete and
  valid at every width under this plan: release the rim's items going
  narrow, fill the rim fresh going wide, ask a redraw.
- **Four owner questions** (7), each with a recommendation.

## 1. The ring's storage, and every reader and writer

### 1.1 The census

`python tools/ring_census.py --exe <main>/bof3/BOF3.exe --funcs <main>/analysis/pc_funcs.json`
(and `--self-test`). A linear capstone sweep of `.text`, as `pe_xref.py`
does, but keeping every operand: a memory displacement with or without a base
or index register, and an immediate of any instruction (`pe_xref` keeps
`mov` / `push` only). It reports each instruction whose operand lands inside
`MapView_CellItems`, `MapView_Cells` or `DrawLayers`, or on
`MapView_Row` / `_Column` / `_Inset` / `_Origin`; the bound and stride
immediates of each such function (`0x1B`, `0x1C`, `0x36..0x39`, `0x70`,
`0x310`, `0x620`, `0xA80`, `0xC40`, `0x1880`); a raw search of the whole
image for each array's base and end held as data; and every function holding
both a row bound and a column bound without touching a ring address (a copy
reached through a pointer). Functions are `pc_funcs.json`'s 3,004 entries
joined with `symbols.toml`'s, so a handler `pc_funcs` folds into its
neighbour (`MapCell_DrawFlatFaces` inside `0x56FF00`) is its own. The
script prints addresses, mnemonics and our names only.

**Result (2026-10-21):** 41 functions; **0** pointers to an array base or end
outside `.text`; 62 functions with both bounds and no ring address. The 62
were read by name and evidence line (not instruction by instruction): menus,
windows, scene steps, effect draws, battle windows - `0x1C` / `0x38` there are
record offsets and step codes. The one cell handler among them,
`MapCell_DrawUprights`, is handed a record, not a ring slot. A first run with
a 0x40-byte margin round each array caught 1,363 functions through the
arrays' neighbours (`Frame_Counter` `0x937F94`, `Game_AreaNumber`
`0x904EFC`): every neighbour is another named variable, so the census counts
in-array operands only.

### 1.2 The arrays, and no room after them

| Array | Address, size | Shape | After it (`pe_xref.py`, `symbols.toml`) |
|---|---|---|---|
| `MapView_CellItems` | `0x937FA0`, `0x1880` (6,272) | 56 rows x 28 cells of 4 bytes: map x, map y, a word (draw item in bits 0..11, bits 12..15 from `MapView_CellTextures`); row stride `0x70` = 112 | `0x939820` read by `BattleResultWin_ExpToNext` `0x598810` (`mov eax, 0x939820`); `Effect1g_TradeState` `0x93985C`. The PSP's needs 7,656 |
| `MapView_Cells` | `0x904F20`, `0xC40` (3,136) | 56 x 28 u16: a run's dword offset + 1, or 0 | `0x905B60` is `MenuLists_CampChosen` (3 references) |
| `DrawLayers` | `0x8022A0`, `0xA80` | 56 records of `0x30`: three list pairs (first, last) per display buffer, interleaved; `+0x20` is `Gfx_FrameNodes` | `0x802D20` is the window records' pass byte (19 references) |
| `MapView_Row`, `MapView_Column` | `0x929F24`, `0x929F20` | the heads: the slot before view row / column 0 | - |
| `MapView_Origin` | `0x7E0688` | the map cell of view (0, 0)'s frame | - |

`DrawTable` (`0x801C00`, 256 dwords, count a byte) is keyed by layer, not
sized by the ring; it is in 6.3. All of these are in `.data`, so in the
quicksave's span (5).

### 1.3 The 41 functions, classed

File:line is the `// original 0x...` comment in our source; every one is
ours. **Logic** means a game decision or game state follows from the ring's
window; **draw** means only the picture does.

**The ring's own writers** (they keep CellItems, the heads and the origin):

| Function | PSX / PSP | Ours | Ring constants |
|---|---|---|---|
| `MapView_ShiftColumnPrev` `0x56E9A0` | `0x80153758` / `0xCD98` | `src/game/map_scroll.cpp:73` | 56 rows, view column -1, head wrap 0 -> 0x1B |
| `MapView_ShiftColumnNext` `0x56EA30` | `0x80153858` / `0xCE94` | `map_scroll.cpp:98` | view column 0x1C, wrap 0x1B -> 0 |
| `MapView_ShiftRowsPrev` `0x56EAB0` | `0x80153950` / `0xCF8C` | `map_scroll.cpp:117` | 28 columns, view rows -2 / -3, wrap 0 -> 0x37 |
| `MapView_ShiftRowsNext` `0x56EB50` | `0x80153A6C` / `0xD0A8` | `map_scroll.cpp:139` | view rows 0x38 / 0x39 |
| `Field_ViewReset` `0x56F670` | `0x801548A4` / `0xE560` | `map_scroll.cpp:213` (origin `:241`, `:245`; the 56 x 28 fill `:269`) | heads 0x1B / 0x37; origin x - 0x18, z + 3 |
| `Weretiger_ResetMapView` `0x4B5350` | MAGIC064 `0x801F103C` / `0x178834` | `src/game/magic_s14.cpp:838` (heads `:895`) | MAGIC064's copy of `Field_ViewReset`; the placements doc's "Field_ViewReset (MAGIC064's copy)" is this function |
| `MapView_FillCells` `0x56FCA0` | `0x80155154` / `0xD48C` | `src/game/scena_sx.cpp:353` (loop `:361`) | 56 x 28 refill, items kept |
| `MapView_PlaceRuns` `0x571FF0` | `0x801595E0` / `0x11AE8` | `map_scroll.cpp:175` | MapView_Cells zeroed (`0x310` dwords), each run placed when `0 <= s, d < 0x38` |

**Logic readers of the window** (keep Capcom's at every width):

| Function | PSX / PSP | Ours | What the window decides |
|---|---|---|---|
| `MoveCmd_TestFB` / `FC` `0x572650` / `0x572790` | `0x80155B38` / `0xEDC0`, `0x80155CD4` / `0xEF9C` | `src/game/field_misc.cpp:302` (`TileTest` `:169`, `RingCell` `:111`) | a cell outside answers 0 and patches nothing; inside, kind 0x23 records become 0x24 and a patch entry is switched (area state) |
| `AreaMap_ClearCell` `0x5728D0` | `0x80155E74` / `0xF17C` | `src/game/field_e2.cpp:1572` | inside, the cell's records of kind 3 / 9 / 0x27 become 0x30 (area state); reads `MapView_Cells`, writes the records |
| `Area40_TileLit` `0x405BF0` | AREA040 `0x801F30B8` / `0x22A5FC` | `src/game/area_w1a.cpp:426` | the puzzle's answer; outside, 0 |
| `Quake_Start` / `Quake_End` `0x4CE140` / `0x4CEA20` | MAGIC102 `0x801EEC80` / `0x19DE14`, `0x801EFD7C` / `0x19DB00` | `src/game/magic_s23.cpp:870`, `:1049` (the walk `Walk` `:166`) | every run named in `MapView_Cells` recoded semi-transparent and back (area records) |

**Draw readers** (may see the grown ring):

| Function | PSX / PSP | Ours | Ring constants |
|---|---|---|---|
| `MapView_Build` `0x56EC00` | `0x80153B8C` / `0xD998` | `src/game/map_layers.cpp:471` (walk `:480..489`, `Columns` `:240`, `BuildCell` `:249`, layer links `:344..383`) | 0x37 layers, `(14 - inset) * 2` columns, wraps 0x37 / 0x1B |
| `DrawLayer_Open` `0x56FD20` | `0x801565FC` / `0x101BC` | `src/game/draw_emit.cpp:350` (body `:368`, `wide_rand::Row` `:389`) | row mod 0x38, `(14 - inset) * 2` columns from head + inset + (row even) |
| `DrawLayer_Close` `0x56FE80` | - | `draw_emit.cpp:333` | a layer's list 1 |
| `DrawLayers_Reset` `0x56F5B0` | `0x80154780` / `0xD1C0` | `src/game/draw_layers.cpp:97` | 0x38 records |
| `Gfx_BeginFrame` `0x4FD230` | `0x8014AF98` (`Packet_FrameReset`) / `0x778` | `src/game/gfx_frame.cpp:9` (loop `:39`) | 0x38 frame nodes; the PSP's loops `0x3A` records of `0x30` after its 8 OT pointers (paired by shape here) |
| `Sprite_DrawPass` `0x593060` | `0x8014BB64` / `0x1540` | `src/game/draw_pass.cpp:262` (loop `:287`) | 0x37 layers; the PSP's `0x39` |
| `MapCell_DrawFlatFaces` `0x570A00` | - | `src/game/map_field_objects.cpp:280` (window `:291..297`) | `MapView_ItemAt` inlined: a cell handler's item by map cell |
| `MapView_ItemAt` `0x572ED0` | `0x801563AC` / `0xF344` | `field_misc.cpp:290` | a map cell's draw item, 0 outside. Its ten callers (`AreaMap_ApplyPatch` kind 1, `MapView_ItemHalfAt`, `Area40_DrawGrid`, `EffectKind7D_SetMap`, `EffectKind18Sub42_Mark`, `Sub51_Place` / `_Fade`, `Sub5A_Set` / `_SetTiles`) all use the item to texture, draw or allocate draw items only (`effect_6c.cpp:895`, `:929`, `effect_5f.cpp:566`, `effect_3d.cpp:713`, `area_w1a.cpp:520`) |
| `MapView_LinkPrimAt` `0x572FA0` | `0x801564C4` / `0xF478` | `src/game/world_map.cpp:338` (bound `:358`) | a primitive into the frame node of view row 0..0x37 of a map point |
| `MagicFx_LinkByDepth` `0x4FB880` | BATTLE.EMI#15 `0x800AC020` / `0x1487D4` | `src/game/magic_lib.cpp:325` (bound `:344`, `kDepths` `:57`) | the same by depth, for battle effects |
| `EffectKind18Sub15_LinkLayer` `0x5032C0`, `EffectKind18Sub44_Commit` `0x511BB0` | - | `src/game/effect_5c.cpp:1245`, `src/game/effect_6c.cpp:220` | fixed layers 15 and 45 (frame nodes) |
| `Sprite_UpdateScreen` `0x588F20` | `0x8014D184` / `0x2450` | `src/game/sprite_screen.cpp:67` (test `:92`) | key 0..0x36 or hidden (3); the PSP's `0x39` |

**Origin only** (read the origin to key or place; unchanged under the plan,
since the origin is): `MapView_CellToMap` `0x56F910` (`src/game/map_view.cpp:182`),
`Sprite_ProjectA` `0x57B860` (`src/game/field_frame.cpp:382`; its key is not
range-tested), and the scene shifts that move the origin by 0x14 / 0x1E /
0x20 cells and then call `MapView_FillCells`: `Area149_ViewShiftBack`,
`Area149_Tail33` (`area_w3g.cpp:727`, `:735`, through `ShiftMap` `:620`),
`Area186_TailShift` (`area_w4d.cpp:605`, `Area186Shift` `:122`),
`Scena00_Run3`, `Scena00_Run11` (`scena_sc0.cpp:425`, `:1012`),
`Scena14_ScrollView` (`scena_sc13.cpp:2057`), `Scena16_Scene3`
(`field_modes.cpp:397`). Every one is followed by `FillCells` (the fuzz call
tables `area_w3g_fuzz.cpp:424..426`, `area_w4d_fuzz.cpp:55`; `ViewShift()` in
the scene files).

**Area 189's own grid:** `AreaMapBD_BuildView` `0x510780`
(`src/game/rest_3g.cpp:809`) writes the origin and walks a fixed grid of its
own (0x38 x 0x1C or 0x2C x 0x28 by the octant) into `DrawLayers`;
`AreaMap_Frame` branches to it for area 0xBD and never calls `MapView_Build`
there. Not the ring; out of this round ([`ultrawide-culls.md`](ultrawide-culls.md) 3.4).

**Address folds** (in a ring array's range by arithmetic, not ring
accesses): `Field_PendingNext` / `_PendingRelease` / `Party_Remove`
(`[n * 4 + 0x802BF4]` with `n * 4` a multiple of `0x14C`: `ObjTrio`'s member
records `0x802D40` less one record), `Party_ExtraFrame` (`cmp edi, 0x802310`:
the end of `Sprite_ObjectsExtra`'s four `0xA4` objects plus the `+0x80`
context, `field_frame.cpp:35`), `Quake_Start`'s `cmp esi, 0x8022C8` (an end
pointer of an object walk at field `+0x38`).

### 1.4 Ours only (DLL code the image cannot show)

- **`src/game/layering.cpp` (DIV-0071, FY's file this round):** `Defer` maps
  draw items to view cells from `MapView_CellItems`' words (`:387`), and
  `IsFloor` / `ListClear` read a cell's map position back through it (`:127`,
  `:162`); `RecordsClear` (`:181..185`) and `RunsClear` (`:259..269`) repeat
  `DrawLayer_Open`'s row and walk over `MapView_Cells`. Under the plan its
  view of Capcom's window is unchanged; rim cells are invisible to it (6.4).
- **`src/hook/draw_order.cpp:150`** (the `BOF3X_DRAWORDER` tagger): a cell's
  map position by view cell index. An instrument.
- **`src/game/wide_rand.cpp`**: `Row` and its self-test (4).

## 2. The two PSP sites without a PC twin

**PSX `0x801565fc` -> PSP `0x101BC` -> ours `DrawLayer_Open` `0x56FD20`.**
The PSP function, disassembled: the row `head + layer + 1` less `0x3A` once
(`slti 0x3A` / `addiu -0x3A`, `0x10250`); the inset less 2 when an input bit
0 is set (`slti 2`, `0x10278`); `+1` for an even row (`sltiu ..., 1`,
`0x10298`); `0x21 - 2 x inset` columns (`ori 0x21`, `0x102A4`), each wrapping
at `0x21` (`0x102AC`); an indirect call per record (`jalr`, `0x10394`). That
is `DrawLayer_Open`'s body (`draw_emit.cpp:378..396`) at 58 x 33. The
sibling's name is not set; the pairing is by this reading.

**BATTLE.EMI#15 `0x800ac020` -> PSP `0x1487D4` -> ours `MagicFx_LinkByDepth`
`0x4FB880`.** `pairs_propagated.json` pairs the next function, `0x800AC1DC`,
with PC `0x4FB9F0` (call-anchored), and `0x4FB9F0` is the first function
after `MagicFx_LinkByDepth` (`0x4FB880..0x4FB9EE`); the PSX one is
`0x1BC` bytes before its anchor, ours `0x170`. The PSP twin computes a depth
from two `lh` of the origin and the `lo == 0` tests (`sltiu`, `0x1487FC`,
`0x148824`) and bounds it at `slti 0x3A` (`0x148840`): `kDepths` `0x38` grown
by two, as `MapView_LinkPrimAt`'s. By position and shape; not a recorded
pair.

**The PSP's other bounds, read for the plan:** `Field_ViewReset` `0xE560`:
origin x - `0x1B` (`0xE6F0`), z + 5 (`0xE700`), heads column `0x20`
(`0xE870`) and row `0x39` (`0xE87C`), the fill `0x3A` rows x `0x21` at
stride `0x84` (`0xE83C`, `0xE82C`, `0xE844`). `MapView_Build` `0xD998`:
`0x39` layers (`0xE51C`), `0x21 - 2 x inset` columns (`0xDB58`, `0xE4E8`),
row wrap `0x39` (`0xDB84`); its screen cull widened to x `-0x60..0x1A0`
(`0xDCE8`, `0xDCF0`). `Sprite_UpdateScreen` `0x2450`: key `< 0x39`
(`0x24F8`). `Sprite_DrawPass` `0x1540`: `0x39` layers (`0x1BE4`).
`MoveCmd_TestFB` `0xEDC0`: `s < 0x3A`, `d < 0x42`, columns `0x21`
(`0xEDFC`, `0xEE34`, `0xEE78`). `Packet_FrameReset` `0x778`: `0x3A` frame
nodes (`0x810`).

**The grown ring's geometry.** From `MapView_CellToMap` (`x = Ox + col +
(row + 1) / 2`, `z = Oz + row / 2 - col`, truncating) and the two origins:
a map cell at Capcom's view row `r` and half-column `d = 2c + (r odd)` sits
at the PSP's `r' = r + 1`, `d' = d + 5`. So the grown ring is Capcom's plus
one row above (`r = -1`), one below (`r = 56`), and five half-columns either
side: Capcom column `c` is the PSP's `c + 2` on Capcom's even rows and
`c + 3` on its odd ones. 58 x 33 - 56 x 28 = **346 rim cells**. The centre
does not move: Capcom's view (28, 14) and the PSP's (29, 16) are the same
map cell, `(focus x + 4, focus z + 3)`.

A rim cell's map position must be computed in the PSP's frame
(`r' >= 0`, origin `+(-3, +2)`), not by Capcom's `MapView_CellToMap` at
`r = -1`: the truncating `(row + 1) / 2` and `row / 2` put Capcom's row -1 on
row 0's cells.

## 3. The off-screen flag and the layer test

`Sprite_UpdateScreen` (`sprite_screen.cpp:67`): the key is
`z + x - Oz - Ox + adjust + 2` in 16 bits, the sprite's view row; outside
`0..0x36` (`:92`) the sprite gets `flags |= 0x80` - byte 0's bit 7, the bit
nine logic functions read ([`ultrawide-culls.md`](ultrawide-culls.md) 2) -
and returns before the projection and the draw list. So a sprite in Capcom's
row 55 or beyond is hidden and never reaches `Sprite_Draw`. The PSP tests
`< 0x39` on its key, which is ours plus one (its origin sum is one less):
rows -1..55 in Capcom's terms.

**The rule for RG: keep the test, flag and listing both, at `0..0x36` at
every width.** DIV-0041's bit-7 split for `Sprite_Draw` (draw by the wide
bound, set the bit by Capcom's) has no cheap twin here: to draw a sprite in a
grown row, `Sprite_UpdateScreen` would have to project it and put it on the
draw list, and that writes the record's depth, screen x / y (both forms) and
the low half of its key - fields of `Sprite_Objects`, a game page - that the
original never writes for it; then `Sprite_Draw` would clear bit 7 for one
inside its x bound. A split would need those writes into a shadow of ours and
`Sprite_Draw` reading the shadow (question 7.1). Kept, the grown rows draw
terrain only, and a sprite there behaves exactly as narrow: hidden. Those
rows are the ring's top and bottom, which yaw 0x200 views reach only over
low ground ([`ultrawide-culls.md`](ultrawide-culls.md) 3.3: the top needs
21.6 rows over ground 256 below, the ring has 21).

**A second guard in the pass.** `Sprite_ProjectA` (type 0x0A sprites,
`field_frame.cpp:382`) does not range-test its key, so such a sprite can sit
on the draw list at layer 0x37 and is never drawn because the pass stops at
0x36. When the wide pass walks layer 0x37 for the terrain of Capcom's row
55, it must not draw sprites there (`draw_pass.cpp`'s sprite run, `:326..366`),
or `Sprite_Draw` writes bit 7 for a sprite the original leaves alone.

## 4. `wide_rand::Row` and the band test

`Row` (`wide_rand.h:42`, `int columns_[28]`) lists the ring columns
`DrawLayer_Open`'s walk of one row takes at the **original** inset, at most
`(14 - 0) * 2 = 28`. Under the plan Capcom's ring and its columns are
unchanged, so the original's walk is still at most 28 Capcom columns: **the
array stays 28.** What changes is who asks it:

- a window cell (in Capcom's 56 x 28) asks `Row::Band(its Capcom ring
  column)`, as today;
- **a rim cell is a band cell** without asking;
- **every cell of Capcom's row 55 is a band cell**: Capcom's
  `DrawLayer_Open` is called for layers 0..0x36 (rows 0..54) only, so the
  wide pass's layer 0x37 walks a row the original never does;
- row -1 is rim.

So `Row` needs no new state; `DrawLayer_Open`'s wide walk sets
`wide_rand::SetBand(rim || layer == 0x37 || split.Band(column))`. The
self-test's boundary loop (`wide_rand.cpp:177..240`) grows a case: at each
original inset, flag and parity, every cell of the grown walk is the
original's exactly when Capcom's walk takes it.

What a band cell's handlers do beyond `Rand` is unchanged from
[`ultrawide-rand.md`](ultrawide-rand.md) 3.3: the patch handlers 0x24 / 0x25
apply an area patch from a band cell on a frame the original's walk has not
reached (D295). The rim adds cells to that class (question 7.2).

## 5. The quicksave

**Capcom's ring is in the span.** `MapView_CellItems`, `MapView_Cells`,
`DrawLayers`, `DrawTable`, the heads and the origin are all in `.data`,
inside `0x5DA000..0x93E000` ([`quicksave.md`](quicksave.md) 3); so is the
draw-item pool, either in the span or as DIV-0062's array the core copies.
The rim, a store in `bof3x.dll`'s `.bss`, is not: it needs a record. It
belongs in the graphics participant `GFX` (`src/render/gfx_snapshot.cpp`,
beside `wide_rand::QuickSave` at `:201` / `:295`), or a tag of its own.

**The record:** the columns at the take (0, 53 or 120), then, if wide, the
rim's 346 cells (4 bytes) and run words (2 bytes), and the extra layer's
`0x30`-byte record (6.3). About 2.1 KB. Its self-pointers name `bof3x.dll`,
which a load already requires at the same base (4a).

**Across widths, rebuild, do not refuse.** Under the plan Capcom's ring is
complete at every width - its cells, heads, origin, run words and its own
draw items, since the wide walk keeps building the window's items in
Capcom's slots. So:

- **16:9 <-> 21:9:** the same 33 x 58 ring (call 1): the record as is.
- **wide take, narrow load:** each rim cell's item released
  (`DrawItemPool_ReleaseCell` on the record's cells, after the core has
  written the pool back - [`quicksave.md`](quicksave.md) 4: the span and the
  pool, then the participants); the rim dropped. Capcom's lists name no rim
  item after the next `MapView_Build`, so `MapView_Redraw` is raised to 1 if
  it is 0.
- **narrow take, wide load:** the rim filled fresh: each cell's map position
  in the PSP's frame, item 0, run words from the area's run list (the rim
  half of `MapView_PlaceRuns`); `MapView_Redraw` raised to 1.

Both are a loop over 346 cells through functions RG writes anyway. Refusing
("Quickload refused: taken at 16:9, this game runs at 4:3") would be a
check function and a sentence, a little cheaper, but it would cost the
player every quicksave on a switch of the launcher's width. Raising
`MapView_Redraw` writes a span byte the census already counts as picture
(`0x905000`, [`ultrawide-census-3.md`](ultrawide-census-3.md) 2); a load
across widths is not a hash comparison's case. Question 7.4.

## 6. The plan

### 6.1 Where the grown ring lives: Capcom's ring plus a rim

Capcom's arrays, heads and origin do exactly what they do now, at every
width: every function in 1.3 that writes them keeps its body. When the view
is wide (`Widescreen_InsetColumns() != 0`, the gate DIV-0041 already uses):

- **a rim store of ours**, indexed by grown view position (`r'` 0..57,
  `c'` 0..32, only the 346 rim positions used): cell (map x, map y, word)
  and run word. Not a ring: the shifts move its entries, as below.
- **one accessor**, `Grown(r', c')`: the Capcom window cell
  (`r = r' - 1`, `c = c' - 2 - (r odd)`, in range) is Capcom's slot
  `((MapView_Row + 1 + r) mod 56, (MapView_Column + 1 + c) mod 28)` in
  `MapView_CellItems` / `MapView_Cells`; any other is the rim's.
- **the grown map position**, `GrownCellToMap(r', c')`: `MapView_CellToMap`'s
  arithmetic with the origin `+(-3, +2)`, never Capcom's function at a
  negative row (2).

Why not the PSP's relocation (all of Capcom's ring moved to a 33 x 58 buffer
when wide): the logic readers (1.3) would read a bigger window (the PSP's
changed logic), or each would need a Capcom-window test re-added; DIV-0071's
`layering.cpp` and `MapView_ItemAt` read draw items in Capcom's slots; and a
load across widths would have to hand every item from one ring to the other.
Why not a full 33 x 58 copy of ours beside Capcom's: the window's draw items
would live in ours, so `layering.cpp`, `ItemAt` and `DrawFlatFaces` would all
need re-aiming for every cell, not only the rim's; the rim plan re-aims them
for rim cells only.

**What a shift does to the rim.** A scroll never keeps a cell's draw item
across the window's edge, in Capcom's model: a cell leaving or entering
Capcom's 56 x 28 is released and refilled (`map_scroll.cpp:81..161`). The
rim follows that: `ShiftColumnNext` moves the grown view one column on
(`c' -> c' - 1`, `r'` kept), `ShiftColumnPrev` back, `ShiftRowsNext` two rows
on (`r' -> r' - 2`), `ShiftRowsPrev` two back. For each rim position after
the move: if the cell there came from another rim position, it moves with
its item; if it came out of Capcom's window, or is new at the grown ring's
edge, it is filled fresh (`GrownCellToMap`, item 0 - Capcom's own shift has
already released the window cell's item); each rim cell that leaves the
grown ring, or enters Capcom's window, is released
(`DrawItemPool_ReleaseCell`). One function, four directions; called after
Capcom's body, so the origin it reads is the new one. Run words are not
moved: `AreaMap_Frame` calls `MapView_PlaceRuns` after any shift
(`map_layers.cpp:141..167`), and its rim half rewrites them.

### 6.2 Each site, its change and cost

Unchanged at every width: the logic readers (`TileTest`, `ClearCell`,
`TileLit`, the Quake walk), `Sprite_UpdateScreen`, `Sprite_ProjectA`,
`MapView_CellToMap`, the origin writers, `MagicFx_LinkByDepth`,
`MapView_LinkPrimAt`, the two fixed-layer effects, `AreaMapBD_BuildView`,
`Gfx_BeginFrame` (row -1 has no frame node: `LinkPrimAt` and
`LinkByDepth` reject a depth below 0, `world_map.cpp:358`,
`magic_lib.cpp:344`). Changed only when wide:

| # | Site | Change | Cost |
|---|---|---|---|
| 1 | new `map_rim.{h,cpp}` (RG's name) | the store, `Grown`, `GrownCellToMap`, `RimShift(direction)`, `RimFill()`, `RimRuns()`, the record (5), the self-test (6.5) | the bulk: ~250 lines |
| 2 | the four shifts, `map_scroll.cpp:73..161` | after the body: `RimShift(dir)` | a line each |
| 3 | `Field_ViewReset` `map_scroll.cpp:213`, `Weretiger_ResetMapView` `magic_s14.cpp:838` | after the 56 x 28 fill: `RimFill()` with items 0, no release (both reset the pool after) | a line each |
| 4 | `MapView_FillCells` `scena_sx.cpp:353` | the rim refilled in place, items kept, as Capcom's (`:361..367` keeps the word) | a line |
| 5 | `MapView_PlaceRuns` `map_scroll.cpp:175` | after the body: `RimRuns()` - each run with `0 <= s' < 0x3A`, `0 <= d' < 0x42` (`s' = s + 1`, `d' = d + 5`) and outside Capcom's window to the rim's word | ~20 lines |
| 6 | `MapView_Build` `map_layers.cpp:471` | the walk over grown rows 0..56 (Capcom layers -1..55) and columns `g .. 32 - g` through `Grown` (the inset `g`, 6.3); `BuildCell`'s layer -1 to the extra record; its "layer before" (`:345..349`) in grown terms, so Capcom layer 0's 0x4000 / 0x8000 cells go to layer -1, as the PSP's | ~40 lines |
| 7 | `DrawLayer_Open` / `_Close` `draw_emit.cpp:350`, `:333` | layer -1 (the extra record) and 0x37; the walk over grown columns by the PSP's arithmetic (2), words through `Grown`; `SetBand` as in 4 | ~40 lines |
| 8 | `Sprite_DrawPass` `draw_pass.cpp:287` | layer -1 before 0 (Open and Close only) and 0x37 after 0x36 (Open, frame node 55, table entries of layer 0x37, Close; no sprites, 3) | ~20 lines |
| 9 | `DrawLayers_Reset` `draw_layers.cpp:97` | the extra record's two list pairs reset too | a few lines |
| 10 | `MapView_ItemAt` `field_misc.cpp:290`, `MapCell_DrawFlatFaces` `map_field_objects.cpp:280` | outside Capcom's window but inside the grown one (`s' < 0x3A`, `d' < 0x42`): the rim's item. Every caller is draw (1.3) | ~10 lines each |
| 11 | `wide_rand.{h,cpp}` | `Row` unchanged; the band rule of 4 at the call; the self-test case | ~30 lines |
| 12 | `gfx_snapshot.cpp` (SP-G's) | the record and the two rebuilds (5) | ~40 lines |

### 6.3 The details RG must not guess

- **The walk's columns (call 2: the inset rule unchanged).** Today the wide
  walk takes Capcom columns `i_w .. 27 - i_w`, `i_w = max(0, i - L)`, `i` the
  original inset and `L` = 3 or 6 (`map_layers.cpp:216`). The grown walk
  takes grown columns `g .. 32 - g` with **`g = max(0, i + 2 - L)`**: today's
  walk plus the 33rd column whenever today's lowered inset is above 0, and
  into the rim only where today's would be clipped at 0. The PSP's own
  `33 - 2 x i_w` (`g = i_w`) walks two and a half columns more each side at
  every camera, all of them band cells. Question 7.3. `MapView_Inset` and
  `Scratch_Swap` keep today's values (`i_w`): `g` is derived, not stored.
  `DrawLayer_Open`'s "less 2 with input bit 0" applies to `g` as the PSP's
  `0x10278` applies it.
- **The rows.** Capcom's `MapView_Build` and pass stop at layer 0x36 (row
  54); row 55 exists in the ring and is filled (`ShiftRowsNext` refills view
  rows 54 and 55; `PlaceRuns` covers `s < 0x38`) but never drawn. The grown
  draw is Capcom rows -1..55 (the PSP's 0..56); Capcom's row 56 (the PSP's
  57) is filled, never drawn, as Capcom's 55 is today.
- **The extra layer for row -1** is one `0x30`-byte record of ours, laid out
  as Capcom's (`DrawLayers`' three pairs per buffer). Its table-class cells
  (`map_layers.cpp:355`: word bit 0x4000 with `Draw_OtSlot` 6) cannot go in
  `DrawTable`, whose key holds the layer in a byte above layer 0's: they go
  into the record's list 0 in key order (at most 33, sorted as
  `DrawTable_Sort` sorts, `draw_layers.cpp`). No sprite is ever at layer -1
  (3), so nothing interleaves there. Row 55 uses Capcom's 56th record and
  `DrawTable` with layer byte 0x37.
- **`DrawTable` is 256 entries with a byte count** that wraps
  (`map_layers.cpp:369..380`, a log line since 2026-09-27). The rim adds
  table cells on wide frames. RG logs its peak wide on the routes beside the
  pool's ([`ultrawide-culls.md`](ultrawide-culls.md) 4: the pool's 2,048
  should hold); a wrap is a visible loss, and a table of ours for the wide
  build is the fix if it happens.
- **The Quake walk stays Capcom's**: its recoding of run records is area
  state. The rim's terrain stays opaque while MAGIC102's quake runs - a
  picture remainder at the band, not a question.
- **Scene origin jumps** (1.3) need nothing beyond `FillCells`' rim refill:
  the grown origin is derived from Capcom's.

### 6.4 What stays as it is, and is not perfect

- `layering.cpp` (DIV-0071) sees Capcom's window only: a rim run or rim item
  under a sprite is not considered in `Defer`. Sprites live in Capcom's
  layers 0..0x36 and the rim is the ring's outermost band, so a sprite that
  close to the rim is at the picture's edge at a far camera. FY owns the file
  this round; a follow-up if it shows.
- The draw-order tagger (`draw_order.cpp:150`) names rim cells by index
  only.

### 6.5 Order for RG, and the fuzz each touched function needs

1. **The rim and its model test first.** A self-test (`BOF3X_SHADOW=map_rim`)
   that keeps a full 58 x 33 ring of its own by the PSP's arithmetic (twins of
   the four shifts at `0x21` / `0x3A`, the reset with heads `0x20` / `0x39`,
   origin `+(-3, +2)`), and runs random sequences - shifts, resets,
   `FillCells`, origin jumps of 0x14 / 0x1E / 0x20 - against Capcom's code plus
   the rim, comparing every grown position's map cell and run word through
   `Grown` with the model's, and the pool's free count against the items held
   (no leak, no double release). Its control: the rim with Capcom's
   `MapView_CellToMap` at row -1, refused.
2. Sites 2..5 (the writers). Fuzz at narrow, 0 mismatches: `map_scroll`'s
   shifts, `Field_ViewReset`, `PlaceRuns`; `magic_s14`'s
   `Weretiger_ResetMapView`; `scena_sx`'s `FillCells`. Narrow, the rim code
   must not run.
3. Site 6 and 9 (`MapView_Build`, the extra record): `map_layers`' and
   `draw_layers`' fuzz at narrow. Columns first, rows -1 and 55 second, a
   capture between.
4. Sites 7, 8, 11 (`DrawLayer_Open` / `_Close`, the pass, the band):
   `draw_emit`'s, `draw_pass`'s and `wide_rand`'s self-tests at narrow;
   `wide_rand`'s boundary case at 53 and 120 over the grown walk.
5. Site 10 (`ItemAt`, `DrawFlatFaces`): `field_misc`'s and
   `map_field_objects`' fuzz at narrow.
6. Site 12, the quicksave both ways across 4:3 and 16:9 (round doc 3).
7. The gate: narrow identical to `cn_<route>_orig3` on the ten routes; at
   `BOF3X_WIDE=1` and `=2` the narrow `Rand` and game pages. `MapView_Cells`'
   page `0x904000` is a game page (`Party_Zenny` is on it) and must match
   narrow - it will, since Capcom's half of `PlaceRuns` is untouched.

## 7. Questions for the owner

1. **Sprites in the grown rows** (the ring's top and bottom row). Keep
   Capcom's layer test for both the flag and the drawing, so a sprite there
   stays hidden as on the original; or draw it, with the flag set, as
   DIV-0041 does at the sides. **Recommended: keep.** Drawing it means a
   shadow of the sprite's screen fields and `Sprite_Draw` reading it (3);
   the rows are reached only over low ground at the screen's top.
2. **Patch cells in the rim** (D295's class, "leave for now" since
   2026-10-14). A rim cell's 0x24 / 0x25 records apply their area patch when
   the wide walk reaches them, earlier than the original. Keep D295's rule,
   or skip those two handlers in every band cell so the original's walk
   applies each patch at Capcom's frame. **Recommended: keep D295's rule and
   let the gate decide**; if a route's game pages part through a patch cell,
   RG skips them in band cells and says so.
3. **The walk's width.** `g = max(0, i + 2 - L)` (today's walk plus one
   column, reaching into the rim only where needed), or the PSP's
   `33 - 2 x i_w` (two and a half columns more each side at every camera).
   **Recommended: the first** - the field camera is already clean at 16:9
   and 21:9, and every extra column is band cells walked through handlers
   for nothing on screen.
4. **A quickload across widths:** rebuild (5) or refuse with a sentence.
   **Recommended: rebuild** - a loop over 346 cells either way, and a player
   who changes the launcher's width keeps their quicksaves.

## 8. Ledger text, drafted for the orchestrator

No ledger entry from this group: a reading. RG's DIV-0041 amendment will
need, from this doc: Capcom's ring kept at every width with a rim of the
PSP's geometry for the draw; the logic readers' window Capcom's (the PSP
grew it); sprites in the grown rows hidden as on the original; rim and row
55 cells band cells; D295 widened to the rim (or not, per 7.2); the
quicksave's rule (7.4).

**`docs/README.md`, one row:** `wide-ring-read.md` | The wide-ring round's
read pass (group RM, 2026-10-21): the view ring's arrays have no room in
place; 41 image functions touch the ring (the PSP's 23, eleven more, seven
address folds) and DIV-0071 reads it too; the two unpaired PSP sites are
`DrawLayer_Open` and `MagicFx_LinkByDepth`; the PSP grew the logic's window,
ours keeps Capcom's; the plan is Capcom's ring plus a 346-cell rim of the
PSP's geometry, the sprite test unchanged, `wide_rand::Row` unchanged, the
quicksave rebuilt across widths. `tools/ring_census.py`. MEASURED.
