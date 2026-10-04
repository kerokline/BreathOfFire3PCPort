# Group E5C: effect kind 0x18's sub-kinds 0x10, 0x11, 0x12, 0x15, 0x16, 0x17, 0x50, 0x56, 0x57, 0x58

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..15),
wave five, from the round branch's tip `42b2388`. **62 functions ours**
(`src/game/effect_5c.cpp`, shadow name `effect_5c`): the cut table's 60 rows
for E5C (`analysis/round13_cut.tsv`, the band `0x501500..0x503D30`) and two
starts no list of the cut holds - `0x5015E0`, sub-kind 0x50's state 1, which
the catalog's `0x501520` ran on through, and `0x5016E0`, its state 3
(section 5). Each read to its last instruction with capstone and fuzzed
through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
248,000 rounds, 0 mismatches; 157 of 161 controls refused by a count, the other four equivalent mutants (three with a near variant refused, the fourth a near variant of one of those that turned out equivalent too). **Fuzz only**: no recorded route
enters any of the 62 (section 9). Every row is effect code: the cut's three
`hypothesis` rows (`0x502020`, `0x502670`, `0x503660`) are a sub-kind
dispatcher, a sub-kind's draw and a sub-kind dispatcher - all taken, none left
original.

Every function is kind 0x18's: `Effect_RunObjects` (ours) makes each live
record of `Effect_Objects` `Sprite_Current` and calls
`Effect_KindHandlers[+5]`; kind 0x18's handler `EffectKind18_Run` jumps
through `EffectKind18_States` by `+1`, which `EffectKind18_Start` set from the
record's `+0xB` (the sub-kind). Ten of those entries point into this band:

| Sub-kind (`EffectKind18_States[n]`) | Functions | Reached through |
|---|--:|---|
| 0x50 (80): a textured wall (a vertical quad) at one of two cells by the record's x byte, its texture picked by a row-9 flag (`Cond_Flags + 0x48`), a flickering line above it; while another row-9 flag is clear it sinks over nine frames, waits, and flashes a grey quad back over it | 8 | `0x6541AC`, `EffectKind18Sub50_Run` by `+2`, `EffectKind18Sub50_States` `0x65E098` (5) |
| 0x10 (16): a textured panel at cell x 0x41 sliding along z by `+0x30` when the leader comes near (x 0x42, z 0x3E..0x3F) or `Cond_ByteFE` is set, back when the leader leaves; the map bytes of cells (0x41, 0x3E) and (0x41, 0x3F) set to 0x52 and, once row-9 flag 0xF is set, 0xA1; sounds 0x200 / 0x201 | 8 | `0x6540AC`, `EffectKind18Sub10_Run`, `EffectKind18Sub10_States` `0x65E0B4` (6) |
| 0x56 (86): once `Cond_ByteFE` is set, three fans of eight textured rays and a centre quad faded in over 0x20 frames, held, faded out and released at 0x78 | 4 | `0x6541C4`, `EffectKind18Sub56_Run`, `EffectKind18Sub56_States` `0x65E0CC` (2) |
| 0x57 (87): while `Cond_ByteFE` is set, a sub-kind 0x58 record spawned every 0x14..0x23 frames at one of eight cells in turn | 3 | `0x6541C8`, `EffectKind18Sub57_Run`, `EffectKind18Sub57_States` `0x65E0D4` (2) |
| 0x58 (88): a column of eight textured squares at the record's point, grown over eight frames, held 0x1F frames, faded over 0x18 and released; sound 0x202 | 6 | `0x6541CC`, `EffectKind18Sub58_Run`, `EffectKind18Sub58_States` `0x65E0FC` (4) |
| 0x11 (17): fifteen textured floor tiles over a table's cells, the map bytes of (0x45, 0x3C) and (0x46, 0x3C) 0x10 while they stand; on story flag 0x22 (or 0x21 with `Cond_ByteFA` at most 7) they hold ten frames, the cells cleared, then sink and fade - or, the flag set at the start, the record is released at once | 6 | `0x6540B0`, `EffectKind18Sub11_Run`, `EffectKind18Sub11_States` `0x65E10C` (4) |
| 0x12 (18): the same on story flag 0x20 | 6 | `0x6540B4`, `EffectKind18Sub12_Run`, `EffectKind18Sub12_States` `0x65E14C` (4) |
| 0x15 (21): the camera's first angle and the geometry offset set, then every frame a 320-wide band of textured strips and gradient quads committed to draw layer 15's third list, and the map's corner heights of columns 0x2D..0x30 round `Field_Kind2Z`'s row rebuilt as a slope | 4 | `0x6540C0`, `EffectKind18Sub15_Run`, `EffectKind18Sub15_States` `0x65E18C` (2) |
| 0x16 (22): a ribbon of sixteen shaded textured quads waving with `Frame_Counter` at a point set on the first frame | 1 | `0x6540C4` (one handler, no sub-state table) |
| 0x17 (23): a scripted sequence keyed on the counter `0x903848`: a point moved in steps, group E5D's draw-move and quad draws every frame, a second record of the sub-kind spawned at sub-state 10, waits on bit 14 of two enemies' `+0x12` in game mode 5 step 5 | 16 | `0x6540C8`, `EffectKind18Sub17_Run`, `EffectKind18Sub17_States` `0x65E1A8` (16, the last E5D's `0x503DE0`) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`; the nine dispatchers `evidence`). "Wall", "panel", "tiles",
"rays", "column", "ribbon" name the code's shape - the primitives it commits
and the cells it changes - not a play-tested fact: where the game shows these
sub-kinds and what they look like was not traced (section 9; the owner's
word, not this doc's). No spawner of these sub-kinds was found in our source:
kind 0x18 records take their sub-kind from `+0xB` at `EffectKind18_Start`,
and the move-script ops that spawn effects (`0x5786C0`'s 8C, `0x578A00`'s
90..9D) take the kind from script data. Inside the band, sub-kind 0x57 spawns
sub-kind 0x58 records and sub-kind 0x17's state 1 spawns one of its own kind
at sub-state 10.

## 1. What each function does

Every dispatcher below is `mov ecx, [Sprite_Current]; xor eax, eax; mov al,
[ecx + 2]; jmp [eax * 4 + T]` with no compare.

### 1.1 Sub-kind 0x50 (`EffectKind18Sub50_Run` `0x501500`)

| Sub-state | Function | What |
|---|---|---|
| 0 | `EffectKind18Sub50_Start` `0x501520` | `+0xB` = the x byte `+0x36` (the variant, 0 or 1); the point `+0x34` / `+0x38` = `EffectKind18Sub50_Cells[variant]` (x, z bytes << 16); `+0x3E` = -(`AreaMap_Elevation` / 2) - `EffectKind18Sub50_Heights[variant]`; `+0xB` up; row-9 flag `EffectKind18Sub50_Flags[+0xB]` set: `+2` up and a tail jump to state 1; clear: `+2` = 3 and a tail jump to state 3 |
| 1 | `EffectKind18Sub50_Shine` `0x5015E0` | that flag clear: `+9` = 9, `+2` up; the wall, texture 1 + `+0xB` + 4 x (row-9 flag `0x65E034[+0xB]`), height 0x40; a tail jump to the line |
| 2 | `EffectKind18Sub50_Sink` `0x501660` | `+9` down, at 0 `+2` up; the bare wall (texture 0) and the lit wall at height 8 `+9`; a tail jump to the line |
| 3 | `EffectKind18Sub50_WaitSet` `0x5016E0` | the flag set: `+9` = 9, `+2` up; the bare wall |
| 4 | `EffectKind18Sub50_Flash` `0x501730` | `+9` down, at 0 `+2` = 1; a draw mode (page 0xB5) linked at the point (0xC); the lit wall; a grey semi-transparent POLY_F4 (shade 0x1E `+9`) over the wall's vertices, linked (0x38); the line |
| - | `EffectKind18Sub50_DrawWall` `0x501850` | `(texture, height)`: a vertical POLY_FT4 in `Prim_VertexScratch` - x (`+0x36` << 7) - 0x3FC0, z (`+0x3A` << 7) - 0x4008 + h to - 0x3FF8 - h, each end's bottom 8 - h and top + h off -(the ground / 2) - `+0x3E` (four `AreaMap_Elevation` calls); texture + 0x285000F3; linked at the point (0x48) |
| - | `EffectKind18Sub50_DrawLine` `0x501A10` | a draw mode (page 0x95, dtd) linked (0xC); a semi-transparent LINE_F2 (shade 0 / 0x80 by `Frame_Counter`'s bit 0) from vertex a0 to a8, both ends 2 (`Frame_Counter` % 48) - 0x3C - `+0x3E` - the ground / 2 high (projected with `Gte_RotTransPers3`, the third point - b0, the wall's - into `MapView_ScreenXY`), linked (0x20); a draw mode (page 0x95) linked (0xC) |

### 1.2 Sub-kind 0x10 (`EffectKind18Sub10_Run` `0x501BA0`)

The leader test: `ObjTrio` record 0's x within 0x8000 of 0x428000 and its z
within 0x10000 of 0x3E0000 or 0x3F0000 (absolute values by `cdq` / `xor` /
`sub`, signed compares); sub-state 3 tests the same box at 0x18000 both ways.

| Sub-state | Function | What |
|---|---|---|
| 0 | `EffectKind18Sub10_Start` `0x501BC0` | `+0x30` = 0; row-9 flag 0xF clear: the two cells' `AreaMap_Bytes` 0x52, `+2` = 5; set: `+2` up, and with the leader near `+0x30` = 0xFF00 and `+2` = 3; the panel |
| 1 | `EffectKind18Sub10_Wait` `0x501CA0` | `Cond_ByteFE` set, then the leader near (each on its own): sound 0x200 (unless `Field_Request`), `+2` = 2; the panel |
| 2 | `EffectKind18Sub10_SlideOut` `0x501D40` | `+0x30` down 0x20; at -0x100 or below `+2` up; the panel |
| 3 | `EffectKind18Sub10_Hold` `0x501D60` | `Cond_ByteFE` clear and the leader outside the wider box: `+2` = 4; nothing drawn |
| 4 | `EffectKind18Sub10_SlideIn` `0x501DD0` | `+0x30` up 0x20; still below 0: the panel. Else state 1's test (which draws), and unless it set `+2` to 2: `+2` = 1, sound 0x201, the panel |
| 5 | `EffectKind18Sub10_WaitFlag` `0x501E20` | row-9 flag 0xF set: the cells 0xA1, `+2` = 1; the panel |
| - | `EffectKind18Sub10_DrawPanel` `0x501E80` | with `Draw_PassFlags` bit 2: a draw mode (page 0x95) at slot 6; a POLY_FT4 at x 0xE0C0, z `+0x30` - 0x2040 to - 0x2140, bottoms 0x40 and tops 0x180 off -(the ground / 2) at x 0x418000; texture 0x25500127; slot 6 (0x48) |

### 1.3 Sub-kinds 0x56, 0x57, 0x58

| Function | What |
|---|---|
| `EffectKind18Sub56_Run` `0x502020` | the dispatcher (`0x65E0CC`, 2) |
| `EffectKind18Sub56_Wait` `0x502040` (0) | `Cond_ByteFE` set: `+2` up |
| `EffectKind18Sub56_Glow` `0x502060` (1) | the level 0x80, or 4 `+9` below 0x20, or 4 (0x78 - `+9`) above 0x58; at 0x78 or more `Effect_Release` (and on); the rays at that level; `+9` up |
| `EffectKind18Sub56_DrawRays` `0x5020C0` | `(level)`: for z -0x2120, -0x20C0, -0x2060 (j 0..2): eight semi-transparent POLY_FT4 rays, x spread by `Math_Sin`(16 (`Frame_Counter` & 7) + 0x80 i) >> 5 and leaning by (j - 1) times it, y (-0x32 - 8 i - f) << 3, red / blue (126 L - 16 L i - 2 f L) / 128, green (63 L - 8 L i - f L) / 128, slot 4 (0x48); then the point (0xE0C0, z, 0xFE40) projected into `MapView_ScreenXY` and a centre quad of half-size 16 + \|3 - ((`Frame_Counter` / 3 + j) & 7)\| round it (the corners through the FPU, `fild` / `fsub` / `fadd`), shaded 96 L / 128 and 48 L / 128, linked at (0x3F0000, 0x3D0000) (0x48) |
| `EffectKind18Sub57_Run` `0x502470` | the dispatcher (`0x65E0D4`, 2) |
| `EffectKind18Sub57_Wait` `0x502490` (0) | `Cond_ByteFE` set: `+9` = 1, `+0xB` = 0, `+2` up |
| `EffectKind18Sub57_Spawn` `0x5024C0` (1) | `Cond_ByteFE` clear: a tail jump to `Effect_Release`. Else `+9` down; at 0 `Effect_FindFree` (none: nothing), the record kind 0x18, `+1` 0x58, `+2` 0, `+0` 1, at `EffectKind18Sub57_Points[+0xB]` (x, z words << 12); `+9` = 0x14 + (`Rand` & 0xF); `+0xB` = (`+0xB` + 1) & 7 |
| `EffectKind18Sub58_Run` `0x502580` | the dispatcher (`0x65E0FC`, 4) |
| `EffectKind18Sub58_Start` `0x5025A0` (0) | `+9` = 0, sound 0x202, `+2` up |
| `EffectKind18Sub58_Grow` `0x5025C0` (1) | the column at `+9`; `+9` up; past 7: 0 and `+2` up |
| `EffectKind18Sub58_Hold` `0x502600` (2) | the column at 7; `+9` up; past 0x1E: 0x1F and `+2` up |
| `EffectKind18Sub58_Fade` `0x502630` (3) | the column at `+9` >> 2; `+9` down; below 8 a tail jump to `Effect_Release` |
| `EffectKind18Sub58_DrawColumn` `0x502670` | `(n)`: eight squares (c 0..7) up the record's point (x, z = `+0x34` / `+0x38` >> 9 - 0x4000): y -0x80 - (b + 8 c) n (b 4 on odd frames), projected into `MapView_ScreenXY`; half-size (m n) / 2 + (b n) / 16 + 4 (m = c for the upper four, 4 below); corners through the FPU; texture 0xBA009124 \| ((y' - (b << 19)) / 4 / (8 - n)) & 0xF80000 for y' = 0x1F80000 down 0x400000 a square; the primitive's byte `+5` halved; linked at the point (0x48) |

### 1.4 Sub-kinds 0x11 and 0x12

The two are one code over two tables: sub-kind 0x12's functions are sub-kind
0x11's instruction for instruction but the test (story flag 0x20 alone) and
the tile table (`EffectKind18Sub12_Tiles`, whose 45 bytes are the same as
`EffectKind18Sub11_Tiles`' - section 6, control 95).

| Sub-state | Functions | What |
|---|---|---|
| 0 | `_Start` `0x502830` / `0x502B50` | the end test (0x11: story flag 0x22, or 0x21 with `Cond_ByteFA` (s8) at most 7; 0x12: flag 0x20): `Cond_ByteFE` = 1 and a tail jump to `Effect_Release`. Else the cells (0x45, 0x3C) and (0x46, 0x3C) 0x10, the tiles at (-0x100, 0x80), `+2` up |
| 1 | `_Wait` `0x5028C0` / `0x502BC0` | the end test: `Cond_ByteFE` = 1, `+0x3C` = -0x100, `+0xB` = 0x80, `+9` = 0xA, `+2` up, the tiles. Else nothing - not drawn |
| 2 | `_Open` `0x502940` / `0x502C20` | the tiles at (`+0x3C`, 0x80); `+9` down; at 0 the cells 0, `+2` up |
| 3 | `_Sink` `0x5029B0` / `0x502C90` | the tiles at (`+0x3C`, `+0xB` & 0xF8); `+0x3C` up 1; `+0xB` down 2; above -0xC0 a tail jump to `Effect_Release` |
| - | `_DrawTiles` `0x502A00` / `0x502CE0` | `(y, shade)`: for each (x, z, row) byte triplet of the table, a horizontal POLY_FT4 a cell wide at height y, its z edges widened by the shade, texture shade << 16 \| 0xB600B102, `MapView_LinkPrimAt(x << 16, z << 16, row, 0x48)` |

### 1.5 Sub-kinds 0x15, 0x16

| Function | What |
|---|---|
| `EffectKind18Sub15_Run` `0x502E10` | the dispatcher (`0x65E18C`, 2) |
| `EffectKind18Sub15_Start` `0x502E30` (0) | `Camera_Angles`' first word 0xFCC0; `Gte_SetGeomOffset(0xA0, 0x90)`; `+2` up |
| `EffectKind18Sub15_Draw` `0x502E60` (1) | a texture-window RECT (0xE0, 0, 32, 32) written at the cursor and skipped, a draw mode (page 0x95) with it at slot 7; an unshaded textured quad 0..320 x 56..88 at slot 7 (0x48); a 256 x 256 window; three textured quads (64 high) scrolled by (`Frame_Counter` >> 3) & 0xFF, their u edges 0x00 / 0xFF / 0x3F where cut; a draw mode (page 0xB5, dtd); four semi-transparent POLY_G4 gradients (white above, black below) over `EffectKind18Sub15_Columns`' x edges, their lower edges `EffectKind18Sub15_Rows` - ((`Field_Kind2X` - 0x2D0000) >> 15) - each committed by `_LinkLayer`, built 0x44 apart from the cursor as it stood; a draw mode (page 0xB5); then `AreaMap_Corners` of columns 0x2D..0x30 rewritten round `Field_Kind2Z`'s row r: rows r + 3 and r - 3 flat (0x20 each corner), r + 4 .. r + 13 and r - 4 .. r - 17 a slope rising a half step a row |
| `EffectKind18Sub15_LinkLayer` `0x5032C0` | `(size)`: `Gfx_CommitPrim`'s room test against draw layer 15's third list - when (`Gfx_BufferIndex` << 16) + 0x7F1BAC is above the cursor + (size & 0xFF): `Gpu_LinkPrim` from the list's last pointer (`0x802594` + 8 `Gfx_BufferIndex`), the cursor its new last, the cursor up the size; else nothing |
| `EffectKind18Sub16_Run` `0x503320` | `EffectKind18_States[22]` itself: on `+2` = 0 the point (-0x3900, -0x3880, -0x7F0), `+2` up; then (past a `jmp` over padding to `0x503360`) `Gte_PushMatrix`; a draw mode (page 0x95, dtd) at slot 4; the point turned by `Gte_RotTrans` into a MATRIX's translation, `Gte_RotMatrix` of zero angles, `Gte_MulMatrix0(Camera_Matrix)`, set; sixteen POLY_GT4 quads (x 16 b .. 16 b + 16, y -0xC0 .. 0, z `Math_Sin`((F - b) << 9) b >> 12), shaded `Math_Cos`((F + b) << 9) b >> 11 + 0x80, u stepping down from 0x28, `Gte_PrimDepths4_14`, slot 4 (0x54); a draw mode (page 0x95); `Gte_PopMatrix` |

### 1.6 Sub-kind 0x17 (`EffectKind18Sub17_Run` `0x503660`)

"The tail" is: every fifth frame E5D's `0x5043B0`((`Frame_Counter` / 5) &
3); E5D's `0x503FA0`(0) and (1); a tail jump to E5D's `0x503E50`. "The step
+9 / 5 + k" is E5D's `0x5043B0` on every fifth `+9`.

| Sub-state | Function | What |
|---|---|---|
| 0 | `_Start` `0x503680` | the point (0x570000, 0x140000), `+0x3E` 0xFF00, `+0x32` 0, `+9` 0, `+0xB` 0; `+2` up |
| 1 | `_WaitCue` `0x5036D0` | at the counter `0x903848` = 0xC: `+2` up, `Cond_ByteFE` = 0x23, `Effect_FindFree`'s record made kind 0x18, `+1` 0x17, `+2` 0xA (unchecked - section 7); the tail |
| 2 | `_Rise` `0x503760` | `+0x32` (s16) below 0x80 up one; else `+2` up and the counter = 0xD; the tail |
| 3 | `_MoveZ` `0x5037D0` | the z cell `+0x3A` below 0x19: `+0x38` up 0x1000; else `+2` up; the tail |
| 4 | `_MoveXZ` `0x503830` | `+0x3A` below 0x1C: `+0x34` down and `+0x38` up 0x800; else `+2` up; the tail |
| 5 | `_MoveX` `0x5038A0` | the x cell `+0x36` at least 0x4F: `+0x34` down 0x1000; else with (`Frame_Counter` / 5) & 3 = 3 `+2` up; the tail |
| 6 | `_Count` `0x503920` | the step `+9` / 5 + 4; `+9` up; `0x503FA0`(1); at `+9` / 5 = 3 `Cond_ByteFE` = 0, `+9` = 1, `+2` up |
| 7 | `_WaitEnemy0` `0x5039B0` | in `Game_Mode` 5 `Game_Step` 5 with bit 14 of the dword at `0x93B9F2` (`EnemyWorkingRecords` record 0's `+0x12`): `+2` up; `+9` at 0xF wraps to 0; the step `+9` / 5 + 7; `+9` up; `0x503FA0`(1) |
| 8 | `_Settle` `0x503A40` | `+9` wraps at 0xF; `+0x32` above 0 down one, else at `+9` / 5 = 0 `+2` up; the step `+9` / 5 + 7; `+9` up; `0x503FA0`(1) |
| 9 | `_End` `0x503AE0` | at `+9` / 5 = 4 `Effect_Release`; else the step 7 - `+9` / 5, `+9` up, `0x503FA0`(1) |
| 10 | `_Start10` `0x503B50` | the point (0x430000, 0x1A0000), `+0x3E` 0xFF00, `+0x32` 0x80, `+9` 0, `+0xB` 1; `MoveCmd_TestFB(0x55, 0x19)`; `+2` up |
| 11 | `_WaitCue11` `0x503BB0` | at the counter = 0xF with (`Frame_Counter` / 5) & 3 = 3: `0x5043B0`(4), `MoveCmd_TestFB(0x43, 0x1A)`, `+2` up; else every fifth frame `0x5043B0`((F / 5) & 3) and `0x503FA0`(2) |
| 12 | `_Count12` `0x503C20` | the step `+9` / 5 + 4; `+9` up; at `+9` / 5 = 3 `+2` up, `+9` = 1 |
| 13 | `_WaitEnemy1` `0x503CA0` | state 7's test on `0x93BB1A` (record 1's `+0x12`) and its `+9` step, no quads |
| 14 | `_Settle14` `0x503D30` | state 8's steps, then `MoveCmd_TestFB(0x42, +0x32 / 6 + 0x18)` |
| 15 | E5D's `0x503DE0` | not this group's: at `+9` / 5 = 4 `MoveCmd_TestFC(0x43, 0x1A)` and `Effect_Release`; else the step 7 - `+9` / 5, `+9` up |

## 2. Divergence

None. Each function is a faithful replacement; where the original jumps
through a state table past its end, indexes one of the image's small tables
past its entries (sub-kind 0x50's two variants and four flag bytes,
sub-kind 0x57's eight points), divides by zero (sub-kind 0x58's column at
n = 8) or writes the record `Effect_FindFree` did not find (sub-kind 0x17's
spawn on 0xFF), ours aborts with a message instead (the round-nine rule, no
`DIVERGENCE.md` entry; section 7 says which ordinary play can reach). The
x87 sequences of the ray fans' centre quad and the column's squares are
inline assembly of the original's own instructions (`fild` / `fld` / `fsub` /
`fadd` / `fst` / `fstp` in its order), so rounding and NaN quieting are the
original's. `DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no address
of the band. **Not a DIV-0041 site**: sub-kind 0x15's strip and gradients are
320 wide (x 0..320) but not full-frame fills (y 56..88, 0..64 and the
gradients' rows); they are left as the original draws them and named here for
the widescreen survey (section 7).

## 3. The tables

Named in `symbols.toml` (`[[data]]`, `evidence`), each read in place:

| Table | Count | What |
|---|--:|---|
| `EffectKind18Sub50_Flags` `0x65E094` | 4 bytes | the row-9 flag numbers sub-kind 0x50 waits on by `+0xB` |
| `EffectKind18Sub50_States` `0x65E098` | 5 | `_Start`, `_Shine`, `_Sink`, `_WaitSet`, `_Flash` |
| `EffectKind18Sub50_Cells` `0x65E0AC` | 4 bytes | two (x, z) cell pairs by the variant |
| `EffectKind18Sub50_Heights` `0x65E0B0` | 2 words | the variant's height |
| `EffectKind18Sub10_States` `0x65E0B4` | 6 | `_Start`, `_Wait`, `_SlideOut`, `_Hold`, `_SlideIn`, `_WaitFlag` |
| `EffectKind18Sub56_States` `0x65E0CC` | 2 | `_Wait`, `_Glow` - the tool's run of four code pointers is two tables |
| `EffectKind18Sub57_States` `0x65E0D4` | 2 | `_Wait`, `_Spawn` |
| `EffectKind18Sub57_Points` `0x65E0DC` | 16 words | eight (x, z) cells |
| `EffectKind18Sub58_States` `0x65E0FC` | 4 | `_Start`, `_Grow`, `_Hold`, `_Fade` (the tool's run counted on through sub-kind 0x11's to 8) |
| `EffectKind18Sub11_States` `0x65E10C` | 4 | `_Start`, `_Wait`, `_Open`, `_Sink` |
| `EffectKind18Sub11_Tiles` `0x65E11C` | 45 bytes | fifteen (x, z, row) triplets |
| `EffectKind18Sub12_States` `0x65E14C` | 4 | as sub-kind 0x11's |
| `EffectKind18Sub12_Tiles` `0x65E15C` | 45 bytes | fifteen triplets (the same bytes as `_Sub11_Tiles`) |
| `EffectKind18Sub15_States` `0x65E18C` | 2 | `_Start`, `_Draw` |
| `EffectKind18Sub15_Columns` `0x65E194` | 5 words | the gradients' x edges |
| `EffectKind18Sub15_Rows` `0x65E1A0` | 5 bytes | their lower edges before the shift |
| `EffectKind18Sub17_States` `0x65E1A8` | 16 | sub-states 0..14 here and 15, E5D's `0x503DE0` (`_Settle14` steps `+2` to 15; data follows at `0x65E1E8`) |

The counts are each dispatcher's own reach, read against the sub-states'
stores into `+2` (none of the nine dispatchers bounds its index). **Not
named**: the four bytes at `0x65E034` sub-kind 0x50's states index by `+0xB`
for the wall's texture flag - group E5B's `0x50096F` and `0x5011E0` read them
too (`effect_5c_callees.h` `kSub50Lights`, the coordinator's to name).

## 4. The fuzz (`effect_5c_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_5c`, effect mode (`g.effect`, kind 0x18
only), 4,000 rounds a function (`BOF3X_E5C_ONLY=<name>` runs the clones whose
name holds it). Shapes: 56 `kEffect` (the nine dispatchers' `sub_span` their
table's length, sub-kind 0x16's handler 2), six `kCall` (the wall, the rays,
the column, the two tile draws, the layer commit). No function answers. The
nine state tables are `DataTable`s, swapped for recorders on both sides
(E5D's entry 15 among them). **Regions** beyond effect mode's standard ones:
`0x93B9F2` and `0x93BB1A` (4 each, the enemies' words), `0x802594` (0xC:
layer 15's two last pointers), `Gfx_BufferIndex` (1). Everything else the
band touches is standard: the records, `Sprite_Current`, `ObjTrio` (the
leader's point), `Prim_VertexScratch`, `MapView_ScreenXY`, `Camera_Angles`,
`Camera_Matrix`, `Cond_ByteFE`, `Cond_ByteFA`, the row-9 and story flags,
the counter `0x903848`, `Field_Request`, `Game_Mode` / `Game_Step`,
`Draw_PassFlags`, `Frame_Counter`, `Field_Kind2Z` / `X`, `AreaMap_Header`,
`AreaMap_Bytes` and `AreaMap_Corners` inside the area block, the packet and
scratch buffers.

**Callees**: the standard, field-standard and effect-standard rows for
`AreaMap_Elevation`, `Flags_Test`, `Sound_PlayEffect`, `Effect_Release`
(clears `+0..+4`), `Rand`, `MoveCmd_TestFB`, `Math_Sin` / `Cos` (never 0 or
-1), `Gte_RotTransPers` (the screen point filled with fractional floats),
`Gte_RotTransPers3` / `4`, `Gte_StoreDepthF3`, `Gte_RotTrans`, `Gte_RotMatrix`,
`Gte_MulMatrix0`, `Gte_SetRotMatrix` / `SetTransMatrix` (the translation
noted), `Gte_PushMatrix` / `PopMatrix`, `Gte_SetGeomOffset`, `Gpu_LinkPrim`
(writes through the tail when writable), `Gfx_CommitPrim`, `Prim_SetTexture`,
the `Gpu_Set*` and `Gte_PrimDepth*`; E5D's `0x503FA0` (the harness's row).
**Re-listed**: `Effect_FindFree` (E1B's form: a free record from a start the
answer picks, none a quarter of the time, but never none for
`EffectKind18Sub17_WaitCue`, which writes the answer untested);
`MapView_LinkPrimAt` with the row at a byte (the tile draws pass it in `dl`
over a leftover `edx`; the callee reads a signed byte) and the cursor moved
by the size two times in three. **The group's own** called directly: the
line, the panel, sub-kind 0x10's state 1 and sub-kind 0x50's states 1 and 3
(the tail jumps) as `kPhase`; the wall at (texture whole, height's low
word), moving the cursor 0x48 two times in three (`_Flash` builds at the
cursor after it); the rays at their level, the column at n (both whole); the
tiles at two words; the layer commit at a byte, moving the cursor by its
size two times in three. **Group E5D's, raw**: `0x5043B0` (the step whole -
an index - and `Sprite_Current` with its `+0xB` logged), `0x503E50` (no
argument; `Sprite_Current` and its `+0x34` / `+0x38` logged).

**Seeds** (per function, after the harness's fill): on all 20 records `+9`
at the compares' values (0, 1, 2, 4, 5, 7, 8, 9, 0xE, 0xF, 0x10, 0x13, 0x14,
0x18, 0x19, 0x1E, 0x1F, 0x20, 0x21, 0x58, 0x59, 0x77, 0x78, 0x79, 0xFF or
random), `+0xB` below what the function indexes by it (two for sub-kind
0x50's start, with `+0x36` likewise; four for its states; eight for sub-kind
0x57's spawn), `+0x30`, `+0x32`, `+0x36`, `+0x3A`, `+0x3C` at their compares;
`Cond_ByteFE`, `Field_Request`, the counter at 0xC / 0xD / 0xF, a multiple
of five for `Frame_Counter` half the time, game mode and step 5 and the
enemies' bit 14 half the time, `Cond_ByteFA` at 7 / 8 / 0x80, the leader at
and either side of both boxes' edges, `Draw_PassFlags` bit 2 half the time,
`Field_Kind2Z`'s row 17..0x33 (so that rows r - 17 .. r + 13 of a 0x1F-wide
map stay inside the area block's 8 KiB), `Gfx_BufferIndex` 0 or 1, layer 15's
last pointers into the scratch buffer half the time; for the layer commit
the cursor at the real pool's room test half the time (nothing is written
there); sub-kind 0x57's `+9` at 1 half the time. **Arguments**: the wall's
texture 0..6 and height 0x40 / 0 / 0x48 / 8; the rays' level 0, 0x80, 0x7C,
1, -4, 0x40, -0x80; the column's n 0..7, 9, 0x3F, 0xFF or random - never 8
(the original divides by zero there); the tiles' y -0x100 / -0xC0 / 0 and
shade 0x80 / 0x78 / 0xF8 / 0. **Disturbance** (the group's, from the hash
only): `+9`, `+0xB` (0 or 1: inside every table), `+0x30`, `+0x32`, `+0x3C`,
`Cond_ByteFE`, the counter at its cues, `Field_Request`.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_5c`,
exit 0, at the branch's tip): 248,000 rounds over 62 functions, 2,986,977
calls to the stand-ins, **0 mismatches**; 24,777 bytes of state in 49
regions; 411 stand-ins. Every entry of the nine tables reached (the handler
recorders 225..2,067 calls each, E5D's `0x503DE0` 279); `Effect_FindFree`
1,990, `Rand` 949 (sub-kind 0x57's spawns), `Effect_Release` 13,253,
`Gpu_LinkPrim` 809 (the commit's room test passed), `0x5043B0` 17,171,
`0x503FA0` 59,554, `0x503E50` 20,000, `Sound_PlayEffect` 5,766,
`MoveCmd_TestFB` 8,155. The first run had 5,625 mismatches, every one at some
record's `+9`: the group's `Disturb` drew from the harness's `Next()` (the
trap the brief names); fixed, then 0. Two seeds were added after the controls'
first pass, each to load a compare the controls showed thin: sub-kind 0x57's
`+9` at 1 (its spawn ran in 54 rounds of 4,000, now about 900) and sub-kind
0x15's scroll at the 320 edge (control 160 refused in 12 rounds, then 188).

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0,
707 self-test lines, no `MISMATCH` line but the "0 MISMATCHES" counts,
`inject: 8165 ours, 0 left original`; `effect_5c` there 248,000 rounds,
2,988,184 calls, 0 mismatches. **With `BOF3X_WIDE=1`**: `'*'` exit 0, 707
self-test lines, no mismatch, the same counts. Neither run died silently.
`tools/ledger_check.py`: 70 entries, 0 errors.

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 60 and one more (9,881 bytes against
  the cut's 10,393: 49 differ by padding only, one by code); each checked by
  hand to its `ret` or tail `jmp`. The one by code is `0x501660` (the cut's
  208 bytes ran on through `0x5016E0`). **Two starts added**: `0x5016E0`
  (the tool's "code no list has": `EffectKind18Sub50_States[3]` and the
  `jmp` at `0x5015D2`), and `0x5015E0`, which no list or tool line holds:
  the catalog's and the tool's `0x501520` (0x137 bytes) ran on through it,
  but it is `EffectKind18Sub50_States[1]` - an entry by address - and
  `0x501520` reaches it by a `jmp` at `0x5015C9`. So `0x501520` is taken to
  its `jmp` at `0x5015D2` (0xB7 bytes) and `0x5015E0` (0x77) as its own
  function; its clone's `jmp` into it is a call site.
- **Hidden starts**: 51, each an entry by address (a cell of
  `EffectKind18_States` or of a sub-kind's table), not a case. Their recorded
  hosts: E5B's `0x500EF0` (sub-kind 0x50's five) and this band's `0x501A10`,
  `0x501CA0`, `0x501E80`, `0x5020C0`, `0x502670`, `0x502A00`, `0x502CE0`,
  `0x5032C0`, whose catalog extents span the hidden starts after their `ret`
  (section 11). None contains another's code as a fall-through.
- **One function past a jump**: `0x503320` (sub-kind 0x16's handler) `jmp`s
  over padding to `0x503360`; nothing else reaches `0x503360` (no dword of the
  image holds the address, and `band_rows.py` lists no call or jump to it),
  so the code there is its own, as the tool's 0x33E extent counts it.
- **Not a shared tail**: every tail jump in the band lands on a function with
  its own frame and `ret` (the line, the panel, the sub-states, E5D's
  `0x503E50`), taken whole or called.
- **The dispatchers**: every `EffectKind18_States` cell into the band is one
  of the ten rows; no `Effect_KindHandlers` cell points into it. The tool's
  table counts ran on (sub-kind 0x58's 8, 0x10's 10, 0x56's 4); the counts
  above are the dispatchers' reach.
- **PSX twins**: `0x502060` (`0x801F3608`) and `0x5020C0` (`0x801F369C`), both
  by callers; the sibling's `names/area_records.toml` lists them only as an
  AREA overlay's `handler[2]` / `handler[6]` entries - overlay copies, no name
  to transfer, none held.
- **The cut's columns**: the "Table EffectKind18_States" labels are right for
  the ten dispatchers (and the handler `0x503320`); the "Area overlays, world
  3" labels on sub-kinds 0x56..0x58 are resident kind-0x18 code. The
  `hypothesis` tier on `0x502020`, `0x502670`, `0x503660` was the labelling
  pass's; read, all three are effect code.

## 6. Controls

A script in the session scratchpad (`e5c/controls.py`) plants each mutation
in `effect_5c.cpp` (anchored on a unique string), rebuilds, runs
`BOF3X_SHADOW=effect_5c` with `BOF3X_E5C_ONLY=<function>`, restores and
rebuilds; results in `e5c/controls.tsv`. **161 planted: 157 refused by a
count (exit 3 on a `MISMATCH` count; none by an abort of ours), 4 equivalent
mutants**; every one of the 62 functions has at least one. The equivalents:

- **48** (`EffectKind18Sub56_Glow`, `t > 0x58` -> `t >= 0x58`): at `+9` =
  0x58 the original's level is 0x80 and the mutant's (0x78 - 0x58) x 4 =
  0x80. No input tells them apart. Near variant **158** (`> 0x59`) refused.
- **95** (`EffectKind18Sub12_DrawTiles` over sub-kind 0x11's table): the two
  tables hold the same 45 bytes. Near variant **96** (the table from its
  second triplet) refused.
- **104** (`EffectKind18Sub15_Draw`, `right > 0x140` -> `>= 0x140`): at a
  right edge of exactly 320 the original stores 320.0 through `fild` and u
  0xFF, the cut branch 320.0 as a constant and u 0x3F - 0x40 = 0xFF - the same
  bytes. Near variant **160** (`> 0x141`) refused (188 rounds, after the
  scroll seed; 12 before it).
- **159** (`> 0x13F`) was meant as that near variant and changes only the
  same edge - equivalent for the same reason; recorded, and 160 planted.

Controls 115 and 116 test the commit's last pointer: 115 (the cursor as read
before `Gpu_LinkPrim` instead of after) is refused in 37 rounds - the
harness's disturbance can move the cursor during the call, which the
original's re-read sees; 116 (the pointer + 4) in 850.

| # | Function | Planted | Result |
|--:|---|---|---|
| 1 | `EffectKind18Sub50_Run` | `EffectKind18Sub50_States` -> `EffectKind18Sub17_States` | refused in 4000 rounds |
| 2 | `EffectKind18Sub50_Start` | `s[0xB] = s[0x36];` -> `s[0xB] = s[0x37] & 1;` | refused in 1990 rounds |
| 3 | `EffectKind18Sub50_Start` | `...0_Cells) + 2 * v + 1)[0]) << 16);` -> `...0_Cells) + 2 * v + 1)[0]) << 15);` | refused in 4000 rounds |
| 4 | `EffectKind18Sub50_Start` | `(0u - HalfOf(ground)) - Word(At(AddressOf(EffectKind18Sub50_Heights) + 2 * v)));` -> `(0u - HalfOf(ground)) + Word(At(AddressOf(EffectKind18Sub50_Heights) + 2 * v)));` | refused in 3953 rounds |
| 5 | `EffectKind18Sub50_Start` | `s[2] = 3;` -> `s[2] = 4;` | refused in 1307 rounds |
| 6 | `EffectKind18Sub50_Start` | `SH_CALL(EffectKind18Sub50_Shine)();` -> `SH_CALL(EffectKind18Sub50_WaitSet)();` | refused in 2693 rounds |
| 7 | `EffectKind18Sub50_Shine` | `s[9] = 9;` -> `s[9] = 8;` | refused in 1303 rounds |
| 8 | `EffectKind18Sub50_Shine` | `...awWall)(s[0xB] + light * 4u + 1u, 0x40);` -> `...awWall)(s[0xB] + light * 4u + 2u, 0x40);` | refused in 4000 rounds |
| 9 | `EffectKind18Sub50_Sink` | `...ght = static_cast<U>(s[9]) << 3;` -> `...ght = static_cast<U>(s[9]) << 2;` | refused in 3830 rounds |
| 10 | `EffectKind18Sub50_Sink` | `...L(EffectKind18Sub50_DrawWall)(0, 0x40);` -> `...L(EffectKind18Sub50_DrawWall)(1, 0x40);` | refused in 4000 rounds |
| 11 | `EffectKind18Sub50_WaitSet` | `...("EffectKind18Sub50_WaitSet", AddressOf(EffectKind18Sub50_Flags), s[0xB])` -> `...("EffectKind18Sub50_WaitSet", at::kSub50Lights, s[0xB])` | refused in 4000 rounds |
| 12 | `EffectKind18Sub50_Flash` | `0x1E` -> `0x1F` | refused in 3845 rounds |
| 13 | `EffectKind18Sub50_Flash` | `0xB5` -> `0xB4` | refused in 4000 rounds |
| 14 | `EffectKind18Sub50_Flash` | `Project4(prim, 8, 0x14, 0x20, 0x2C);` -> `Project4(prim, 8, 0x14, 0x2C, 0x20);` | refused in 4000 rounds |
| 15 | `EffectKind18Sub50_Flash` | `if (s[9] == 0) s[2] = 1;` -> `if (s[9] == 0) s[2] = 2;` | refused in 166 rounds |
| 16 | `EffectKind18Sub50_DrawWall` | `0x3FC0u` -> `0x3FC1u` | refused in 4000 rounds |
| 17 | `EffectKind18Sub50_DrawWall` | `...cast<U>(Word(s + 0x3A)) << 7) + b - 0x4008u;` -> `...cast<U>(Word(s + 0x3A)) << 7) - b - 0x4008u;` | refused in 3204 rounds |
| 18 | `EffectKind18Sub50_DrawWall` | `...f(ground)) - Word(S() + 0x3E) + b);` -> `...f(ground)) - Word(S() + 0x3E) - b);` | refused in 3204 rounds |
| 19 | `EffectKind18Sub50_DrawWall` | `kTexture50` -> `kTexture50 + 1` | refused in 4000 rounds |
| 20 | `EffectKind18Sub50_DrawWall` | `const U ez2 = VertexCell(0xA);` -> `const U ez2 = VertexCell(8);` | refused in 3988 rounds |
| 21 | `EffectKind18Sub50_DrawLine` | `...char>((Frame_Counter & 1u) << 7);` -> `...char>((Frame_Counter & 1u) << 6);` | refused in 2319 rounds |
| 22 | `EffectKind18Sub50_DrawLine` | `0x3Cu` -> `0x3Bu` | refused in 4000 rounds |
| 23 | `EffectKind18Sub50_DrawLine` | `...Gte_StoreDepthF3)(F(prim + 0x10), F(prim + 0x1C), &third);` -> `...Gte_StoreDepthF3)(F(prim + 0x1C), F(prim + 0x10), &third);` | refused in 3999 rounds |
| 24 | `EffectKind18Sub50_DrawLine` | `0x20` -> `0x21` | refused in 4000 rounds |
| 25 | `EffectKind18Sub10_Run` | `EffectKind18Sub10_States` -> `EffectKind18Sub17_States` | refused in 4000 rounds |
| 26 | `EffectKind18Sub10_Start` | `0x52` -> `0x53` | refused in 1322 rounds |
| 27 | `EffectKind18Sub10_Start` | `S()[2] = 5;` -> `S()[2] = 4;` | refused in 1322 rounds |
| 28 | `EffectKind18Sub10_Start` | `0xFF00` -> `0xFF01` | refused in 452 rounds |
| 29 | `EffectKind18Sub10_Wait` | `...xr) && I(Abs(z - 0x3E0000u)) <= I(zr)) return true;` -> `...xr) && I(Abs(z - 0x3E0000u)) < I(zr)) return true;` | refused in 34 rounds |
| 30 | `EffectKind18Sub10_Wait` | `0x3F0000u` -> `0x3F0001u` | refused in 31 rounds |
| 31 | `EffectKind18Sub10_Start` | `0x428000u` -> `0x427FFFu` | refused in 291 rounds |
| 32 | `EffectKind18Sub10_Wait` | `if (Cond_ByteFE != 0) {` -> `if (Cond_ByteFE == 1) {` | refused in 1415 rounds |
| 33 | `EffectKind18Sub10_Wait` | `if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(id);` -> `if (Field_Request != 2) SH_CALL(Sound_PlayEffect)(id);` | refused in 1076 rounds |
| 34 | `EffectKind18Sub10_SlideOut` | `0xFFE0u` -> `0xFFE1u` | refused in 3985 rounds |
| 35 | `EffectKind18Sub10_SlideOut` | `if (SW(Word(s + 0x30)) <= -0x100)` -> `if (SW(Word(s + 0x30)) < -0x100)` | refused in 339 rounds |
| 36 | `EffectKind18Sub10_Hold` | `0x18000` -> `0x17FFF` | refused in 108 rounds |
| 37 | `EffectKind18Sub10_Hold` | `S()[2] = 4;` -> `S()[2] = 5;` | refused in 747 rounds |
| 38 | `EffectKind18Sub10_SlideIn` | `0x20u` -> `0x21u` | refused in 3975 rounds |
| 39 | `EffectKind18Sub10_SlideIn` | `if (s[2] == 2) return;` -> `if (s[2] == 3) return;` | refused in 10 rounds |
| 40 | `EffectKind18Sub10_SlideIn` | `0x201` -> `0x202` | refused in 563 rounds |
| 41 | `EffectKind18Sub10_WaitFlag` | `0xA1` -> `0xA0` | refused in 2678 rounds |
| 42 | `EffectKind18Sub10_DrawPanel` | `if ((Draw_PassFlags & 4) == 0) return;` -> `if ((Draw_PassFlags & 2) == 0) return;` | refused in 2005 rounds |
| 43 | `EffectKind18Sub10_DrawPanel` | `0x2140u` -> `0x2141u` | refused in 2045 rounds |
| 44 | `EffectKind18Sub10_DrawPanel` | `0x180u` -> `0x181u` | refused in 2045 rounds |
| 45 | `EffectKind18Sub10_DrawPanel` | `0x25500127` -> `0x25500128` | refused in 2045 rounds |
| 46 | `EffectKind18Sub56_Run` | `EffectKind18Sub56_States` -> `EffectKind18Sub57_States` | refused in 4000 rounds |
| 47 | `EffectKind18Sub56_Wait` | `if (Cond_ByteFE != 0) S()[2]` -> `if (Cond_ByteFE == 0) S()[2]` | refused in 4000 rounds |
| 48 | `EffectKind18Sub56_Glow` | `if (t > 0x58) level = (0x78u - t) * 4u;` -> `if (t >= 0x58) level = (0x78u - t) * 4u;` | equivalent: at +9 = 0x58 both give 0x80; near variant 158 refused |
| 49 | `EffectKind18Sub56_Glow` | `if (t >= 0x78) SH_CALL(Effect_Release)();` -> `if (t > 0x78) SH_CALL(Effect_Release)();` | refused in 145 rounds |
| 50 | `EffectKind18Sub56_Glow` | `if (t < 0x20) level = t * 4u;` -> `if (t < 0x20) level = t * 3u;` | refused in 2466 rounds |
| 51 | `EffectKind18Sub56_DrawRays` | `const U lean = (j - 1u) * spread;` -> `const U lean = (j - 2u) * spread;` | refused in 4000 rounds |
| 52 | `EffectKind18Sub56_DrawRays` | `...(0xFFFFFFCEu - i * 8u - f) << 3;` -> `...(0xFFFFFFCEu - i * 8u - f) << 2;` | refused in 4000 rounds |
| 53 | `EffectKind18Sub56_DrawRays` | `...c_cast<unsigned char>(I(green - c) / 128);` -> `...c_cast<unsigned char>(I(green + c) / 128);` | refused in 2942 rounds |
| 54 | `EffectKind18Sub56_DrawRays` | `0x10u` -> `0x11u` | refused in 4000 rounds |
| 55 | `EffectKind18Sub56_DrawRays` | `...ScreenXY, I(half), I(half * 2u), prim);` -> `...ScreenXY, I(half), I(half * 2u + 1u), prim);` | refused in 4000 rounds |
| 56 | `EffectKind18Sub56_DrawRays` | `0x3D0000` -> `0x3E0000` | refused in 4000 rounds |
| 57 | `EffectKind18Sub56_DrawRays` | `...urn = 3u - (((Frame_Counter / 3u) + j) & 7u);` -> `...urn = 3u - (((Frame_Counter / 4u) + j) & 7u);` | refused in 3941 rounds |
| 58 | `EffectKind18Sub56_DrawRays` | `} while (I(z) < I(0xFFFFE000u));` -> `} while (I(z) <= I(0xFFFFE000u));` | refused in 4000 rounds |
| 59 | `EffectKind18Sub56_DrawRays` | the centre quad's `fadd st(1)` before `fstps [+0x2C]` dropped (asm) | refused in 4000 rounds |
| 60 | `EffectKind18Sub57_Run` | `EffectKind18Sub57_States` -> `EffectKind18Sub56_States` | refused in 4000 rounds |
| 61 | `EffectKind18Sub57_Wait` | `s[9] = 1;` -> `s[9] = 2;` | refused in 2437 rounds |
| 62 | `EffectKind18Sub57_Spawn` | `0x58` -> `0x57` | refused in 918 rounds |
| 63 | `EffectKind18Sub57_Spawn` | `Points) + 4 * k + 2))) << 12);` -> `Points) + 4 * k + 2))) << 11);` | refused in 918 rounds |
| 64 | `EffectKind18Sub57_Spawn` | `0x14` -> `0x15` | refused in 918 rounds |
| 65 | `EffectKind18Sub57_Spawn` | `...unsigned char>((s[0xB] + 1) & 7);` -> `...unsigned char>((s[0xB] + 1) & 3);` | refused in 436 rounds |
| 66 | `EffectKind18Sub57_Spawn` | `rec[2] = 0;` -> `rec[2] = 1;` | refused in 918 rounds |
| 67 | `EffectKind18Sub58_Run` | `EffectKind18Sub58_States` -> `EffectKind18Sub11_States` | refused in 4000 rounds |
| 68 | `EffectKind18Sub58_Start` | `0x202` -> `0x203` | refused in 4000 rounds |
| 69 | `EffectKind18Sub58_Grow` | `if (s[9] > 7) {` -> `if (s[9] > 8) {` | refused in 172 rounds |
| 70 | `EffectKind18Sub58_Hold` | `...EffectKind18Sub58_DrawColumn)(7);` -> `...EffectKind18Sub58_DrawColumn)(6);` | refused in 4000 rounds |
| 71 | `EffectKind18Sub58_Hold` | `0x1F` -> `0x1E` | refused in 1513 rounds |
| 72 | `EffectKind18Sub58_Fade` | `if (s[9] < 8) SH_CALL(Effect_Release)();` -> `if (s[9] < 9) SH_CALL(Effect_Release)();` | refused in 139 rounds |
| 73 | `EffectKind18Sub58_Fade` | `...tic_cast<unsigned>(S()[9]) >> 2);` -> `...tic_cast<unsigned>(S()[9]) >> 1);` | refused in 3691 rounds |
| 74 | `EffectKind18Sub58_DrawColumn` | `...t = static_cast<U>(I(b * n) / 16);` -> `...t = static_cast<U>(I(b * n) / 8);` | refused in 1667 rounds |
| 75 | `EffectKind18Sub58_DrawColumn` | `const U m = I(y) < 0x1380000 ? 4u : c;` -> `const U m = I(y) <= 0x1380000 ? 4u : c;` | refused in 3690 rounds |
| 76 | `EffectKind18Sub58_DrawColumn` | `0xF80000u` -> `0xFC0000u` | refused in 3177 rounds |
| 77 | `EffectKind18Sub58_DrawColumn` | `...ast<unsigned char>(prim[5] >> 1);` -> `...ast<unsigned char>(prim[5] >> 2);` | refused in 4000 rounds |
| 78 | `EffectKind18Sub58_DrawColumn` | `...VS(4), 0xFFFFFF80u - (b + c * 8u) * n);` -> `...VS(4), 0xFFFFFF80u - (b + c * 4u) * n);` | refused in 3690 rounds |
| 79 | `EffectKind18Sub58_DrawColumn` | `"fadds (%[xy])\n\t"` -> `"fadds 4(%[xy])\n\t"` | refused in 4000 rounds |
| 80 | `EffectKind18Sub58_DrawColumn` | `...U b = (Frame_Counter & 1u) << 2;` -> `...U b = (Frame_Counter & 1u) << 3;` | refused in 2008 rounds |
| 81 | `EffectKind18Sub11_Run` | `EffectKind18Sub11_States` -> `EffectKind18Sub58_States` | refused in 4000 rounds |
| 82 | `EffectKind18Sub11_Start` | `0x22` -> `0x23` | refused in 4000 rounds |
| 83 | `EffectKind18Sub11_Wait` | `return Cond_ByteFA <= 7;` -> `return Cond_ByteFA < 7;` | refused in 132 rounds |
| 84 | `EffectKind18Sub12_Wait` | `0x20` -> `0x21` | refused in 4000 rounds |
| 85 | `EffectKind18Sub11_Open` | `0x46u` -> `0x47u` | refused in 154 rounds |
| 86 | `EffectKind18Sub12_Start` | `0x10` -> `0x11` | refused in 1322 rounds |
| 87 | `EffectKind18Sub11_Wait` | `s[9] = 0xA;` -> `s[9] = 0xB;` | refused in 3241 rounds |
| 88 | `EffectKind18Sub12_Open` | `TileCells(0);` -> `TileCells(1);` | refused in 154 rounds |
| 89 | `EffectKind18Sub11_Sink` | `0xF8u` -> `0xFCu` | refused in 2004 rounds |
| 90 | `EffectKind18Sub12_Sink` | `if (I(UL(s + 0x3C)) > -0xC0)` -> `if (I(UL(s + 0x3C)) >= -0xC0)` | refused in 563 rounds |
| 91 | `EffectKind18Sub11_DrawTiles` | `SetWord(VS(0x1A), z + shade - 0x4040u);` -> `SetWord(VS(0x1A), z - shade - 0x4040u);` | refused in 3215 rounds |
| 92 | `EffectKind18Sub12_DrawTiles` | `const U texture = (shade << 16) \| at::kTextureTiles;` -> `const U texture = (shade << 15) \| at::kTextureTiles;` | refused in 3214 rounds |
| 93 | `EffectKind18Sub11_DrawTiles` | `0x3FC0u` -> `0x3FC1u` | refused in 4000 rounds |
| 94 | `EffectKind18Sub12_DrawTiles` | `At(e + 1)[0], 0x48);` -> `At(e + 1)[0] + 1, 0x48);` | refused in 4000 rounds |
| 95 | `EffectKind18Sub12_DrawTiles` | `EffectKind18Sub12_Tiles` -> `EffectKind18Sub11_Tiles` | equivalent: the two tile tables hold the same 45 bytes; near variant 96 refused |
| 96 | `EffectKind18Sub12_DrawTiles` | `...essOf(EffectKind18Sub12_Tiles), y, shade)` -> `...essOf(EffectKind18Sub12_Tiles) + 3, y, shade)` | refused in 4000 rounds |
| 97 | `EffectKind18Sub11_Sink` | `...etUL(s + 0x3C, UL(s + 0x3C) + 1u);` -> `...etUL(s + 0x3C, UL(s + 0x3C) + 2u);` | refused in 3987 rounds |
| 98 | `EffectKind18Sub15_Run` | `AddressOf(EffectKind18Sub15_States), EffectKind18Sub15_States_count` -> `AddressOf(EffectKind18Sub56_States), EffectKind18Sub15_States_count` | refused in 4000 rounds |
| 99 | `EffectKind18Sub15_Start` | `SH_CALL(Gte_SetGeomOffset)(0xA0, 0x90);` -> `SH_CALL(Gte_SetGeomOffset)(0x90, 0xA0);` | refused in 4000 rounds |
| 100 | `EffectKind18Sub15_Start` | `0xFCC0` -> `0xFCC1` | refused in 4000 rounds |
| 101 | `EffectKind18Sub15_Draw` | `0x42B00000u` -> `0x42B10000u` | refused in 3763 rounds |
| 102 | `EffectKind18Sub15_Draw` | `0x100` -> `0xFF` | refused in 3834 rounds |
| 103 | `EffectKind18Sub15_Draw` | `...t U left = ((Frame_Counter >> 3) & 0xFFu) + col - 0x100u;` -> `...t U left = ((Frame_Counter >> 2) & 0xFFu) + col - 0x100u;` | refused in 3989 rounds |
| 104 | `EffectKind18Sub15_Draw` | `if (I(right) > 0x140) {` -> `if (I(right) >= 0x140) {` | equivalent: at a right edge of 0x140 both write 320.0 and u 0xFF; near variant 160 refused |
| 105 | `EffectKind18Sub15_Draw` | `0x3Fu` -> `0x40u` | refused in 3598 rounds |
| 106 | `EffectKind18Sub15_Draw` | `...tDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);` -> `...tDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);` | refused in 4000 rounds |
| 107 | `EffectKind18Sub15_Draw` | `...>(Field_Kind2X) - 0x2D0000u, 15);` -> `...>(Field_Kind2X) - 0x2D0000u, 14);` | refused in 4000 rounds |
| 108 | `EffectKind18Sub15_Draw` | `k + 1` -> `k` | refused in 4000 rounds |
| 109 | `EffectKind18Sub15_Draw` | `...or (U k = 0; k < 4; ++k, quad += 0x44) {` -> `...or (U k = 0; k < 4; ++k, quad = Gfx_PacketNext) {` | refused in 3216 rounds |
| 110 | `EffectKind18Sub15_Draw` | `0x20u` -> `0x21u` | refused in 3881 rounds |
| 111 | `EffectKind18Sub15_Draw` | `const U row = centre - j - 4u;` -> `const U row = centre - j - 3u;` | refused in 3881 rounds |
| 112 | `EffectKind18Sub15_Draw` | `slope` -> `slope + 1u` | refused in 3881 rounds |
| 113 | `EffectKind18Sub15_LinkLayer` | `if (limit <= AddressOf(next) + bytes) return;` -> `if (limit < AddressOf(next) + bytes) return;` | refused in 389 rounds |
| 114 | `EffectKind18Sub15_LinkLayer` | `bytes` -> `bytes + 1` | refused in 850 rounds |
| 115 | `EffectKind18Sub15_LinkLayer` | `... Gfx_BufferIndex), AddressOf(now));` -> `... Gfx_BufferIndex), AddressOf(next));` | refused in 37 rounds |
| 116 | `EffectKind18Sub15_LinkLayer` | `...x_BufferIndex), AddressOf(now));` -> `...x_BufferIndex), AddressOf(now) + 4);` | refused in 850 rounds |
| 117 | `EffectKind18Sub16_Run` | `0xFFFFC780u` -> `0xFFFFC781u` | refused in 2020 rounds |
| 118 | `EffectKind18Sub16_Run` | `0x26u` -> `0x27u` | refused in 4000 rounds |
| 119 | `EffectKind18Sub16_Run` | `before` -> `b` | refused in 4000 rounds |
| 120 | `EffectKind18Sub16_Run` | `...cast<U>(sine) * (b + 1u)) >> 12));` -> `...cast<U>(sine) * (b + 1u)) >> 11));` | refused in 4000 rounds |
| 121 | `EffectKind18Sub16_Run` | `...onst short angles[3] = {0, 0, 0};` -> `...onst short angles[3] = {0, 0, 1};` | refused in 4000 rounds |
| 122 | `EffectKind18Sub16_Run` | `Project4(prim, 8, 0x1C, 0x30, 0x44);` -> `Project4(prim, 8, 0x1C, 0x44, 0x30);` | refused in 4000 rounds |
| 123 | `EffectKind18Sub16_Run` | `0xFF40` -> `0xFF41` | refused in 4000 rounds |
| 124 | `EffectKind18Sub16_Run` | `... U next = Frame_Counter - b - 1u;` -> `... U next = Frame_Counter - b - 2u;` | refused in 4000 rounds |
| 125 | `EffectKind18Sub16_Run` | `SH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);` -> `SH_CALL(Gte_MulMatrix0)(m.m, Camera_Matrix, m.m);` | refused in 4000 rounds |
| 126 | `EffectKind18Sub16_Run` | added `m.t[2] = 0;` | refused in 4000 rounds |
| 127 | `EffectKind18Sub17_Run` | the table read one entry on (`+ 4`, count - 1) | refused |
| 128 | `EffectKind18Sub17_Start` | `0x140000` -> `0x140001` | refused in 4000 rounds |
| 129 | `EffectKind18Sub17_WaitCue` | `if (At(at::kCounter)[0] == 0xC) {` -> `if (At(at::kCounter)[0] == 0xD) {` | refused in 1273 rounds |
| 130 | `EffectKind18Sub17_WaitCue` | `rec[2] = 0xA;` -> `rec[2] = 0xB;` | refused in 613 rounds |
| 131 | `EffectKind18Sub17_WaitCue` | `0x23` -> `0x22` | refused in 594 rounds |
| 132 | `EffectKind18Sub17_Rise` | `...(f % 5u == 0) Step((f / 5u) & 3u);` -> `...(f % 5u == 0) Step((f / 5u) & 7u);` | refused in 663 rounds |
| 133 | `EffectKind18Sub17_MoveZ` | the tail's `Quads(0); Quads(1);` swapped | refused in 4000 rounds |
| 134 | `EffectKind18Sub17_Rise` | `if (SW(w) < 0x80) {` -> `if (SW(w) <= 0x80) {` | refused in 378 rounds |
| 135 | `EffectKind18Sub17_Rise` | `At(at::kCounter)[0] = 0xD;` -> `At(at::kCounter)[0] = 0xE;` | refused in 1138 rounds |
| 136 | `EffectKind18Sub17_MoveZ` | `0x19` -> `0x1A` | refused in 447 rounds |
| 137 | `EffectKind18Sub17_MoveXZ` | `0x800u` -> `0x801u` | refused in 2407 rounds |
| 138 | `EffectKind18Sub17_MoveX` | `if (SW(Word(s + 0x36)) >= 0x4F)` -> `if (SW(Word(s + 0x36)) > 0x4F)` | refused in 674 rounds |
| 139 | `EffectKind18Sub17_MoveX` | `...(Frame_Counter / 5u) & 3u) == 3)` -> `...(Frame_Counter / 5u) & 3u) == 2)` | refused in 828 rounds |
| 140 | `EffectKind18Sub17_Count` | `Step(t / 5 + 4);` -> `Step(t / 5 + 5);` | refused in 1206 rounds |
| 141 | `EffectKind18Sub17_Count` | `Cond_ByteFE = 0;` -> `Cond_ByteFE = 1;` | refused in 462 rounds |
| 142 | `EffectKind18Sub17_WaitEnemy0` | `0x4000u` -> `0x8000u` | refused in 510 rounds |
| 143 | `EffectKind18Sub17_WaitEnemy0` | `...ame_Mode == 5 && Game_Step == 5` -> `...ame_Mode == 5 && Game_Step == 4` | refused in 931 rounds |
| 144 | `EffectKind18Sub17_WaitEnemy1` | `Step(t / 5 + 7);` -> `Step(t / 5 + 6);` | refused in 1261 rounds |
| 145 | `EffectKind18Sub17_WaitEnemy1` | `kEnemy1Bits` -> `kEnemy0Bits` | refused in 501 rounds |
| 146 | `EffectKind18Sub17_Settle` | `} else if (s[9] / 5 == 0) {` -> `} else if (s[9] / 5 == 1) {` | refused in 1140 rounds |
| 147 | `EffectKind18Sub17_Settle14` | `SetWord(s + 0x32, w - 1u);` -> `SetWord(s + 0x32, w - 2u);` | refused in 2386 rounds |
| 148 | `EffectKind18Sub17_End` | `if (q == 4) {` -> `if (q == 3) {` | refused in 800 rounds |
| 149 | `EffectKind18Sub17_End` | `Step(7 - q);` -> `Step(8 - q);` | refused in 1096 rounds |
| 150 | `EffectKind18Sub17_Start10` | `0x19` -> `0x18` | refused in 4000 rounds |
| 151 | `EffectKind18Sub17_Start10` | `0x80` -> `0x81` | refused in 3983 rounds |
| 152 | `EffectKind18Sub17_WaitCue11` | `if (cue == 0xF && ((f / 5u) & 3u) == 3) {` -> `if (cue == 0xE && ((f / 5u) & 3u) == 3) {` | refused in 169 rounds |
| 153 | `EffectKind18Sub17_WaitCue11` | `Quads(2);` -> `Quads(1);` | refused in 3831 rounds |
| 154 | `EffectKind18Sub17_Count12` | `S()[9] = 1;` -> `S()[9] = 2;` | refused in 452 rounds |
| 155 | `EffectKind18Sub17_Settle14` | `0x18` -> `0x19` | refused in 4000 rounds |
| 156 | `EffectKind18Sub17_Settle14` | `...lean = SW(Word(S() + 0x32)) / 6;` -> `...lean = SW(Word(S() + 0x32)) / 5;` | refused in 2565 rounds |
| 157 | `EffectKind18Sub17_Settle` | `if (s[9] == 0xF) {` -> `if (s[9] == 0xE) {` | refused in 618 rounds |
| 158 | `EffectKind18Sub56_Glow` | `0x58` -> `0x59` | refused in 132 rounds |
| 159 | `EffectKind18Sub15_Draw` | `0x140` -> `0x13F` | equivalent (a near variant planted wrong: it differs only at the same edge 0x140); 160 refused |
| 160 | `EffectKind18Sub15_Draw` | `0x140` -> `0x141` | refused in 188 rounds |
| 161 | `EffectKind18Sub12_Run` | `EffectKind18Sub12_States` -> `EffectKind18Sub11_States` | refused in 4000 rounds |

## 7. Latent defects (Capcom's, described, not fixed)

- **Sub-kind 0x17's spawn writes a record it did not find.** (D220)
  `EffectKind18Sub17_WaitCue` takes `Effect_FindFree`'s answer & 0xFF and
  writes `+0`, `+5`, `+1`, `+2` of that record untested: with all twenty
  records live (0xFF) the four bytes land 0x7F80 past `Effect_Objects`, at
  `0x7E9160`, `0x7E9165`, `0x7E9161`, `0x7E9162` - inside display buffer 0's
  packet pool (`0x7E1C00..0x7F1BFF`). Ours aborts
  there. Ordinary play reaches it only if the effect pool is full on the frame
  the counter `0x903848` becomes 0xC; not measured. The game's nearest answer
  would be to skip the spawn, as `EffectKind18Sub57_Spawn` does.
- **Sub-kind 0x58's column divides by 8 - n.** (D207) n = 8 is an `idiv` by zero.
  Its callers pass `+9` (0..7 - `_Grow` steps on past 7), 7, and `+9` >> 2
  (2..7 while `_Fade` runs from 0x1F down to 8): unreachable in ordinary play.
- **Sub-kind 0x15's gradients are built past a cursor that may not have
  moved.** (D209) `EffectKind18Sub15_Draw` builds its four POLY_G4s 0x44 apart from
  the cursor as it stood before them and never reads it again; when the
  pool has no room the commit leaves the cursor and the next quad is written
  0x44 further on all the same - up to 0x11C bytes past a cursor already
  within 0x54 of the pool's end, so up to about 0xC8 bytes past the pool.
- **Sub-kind 0x15 rewrites the map's corner heights unchecked** (D211): rows r - 17
  .. r + 13 of columns 0x2D..0x30 round `Field_Kind2Z`'s row r, every frame,
  with no test against the map's height - a row near the map's edge writes
  outside its corner grid.
- **Sub-kind 0x15's draws are 320 wide** (D238) (the strip 0..320 x 56..88, the
  scrolled quads to 320, the gradients over 0..320): under the wide picture
  (DIV-0041) they leave the bands either side. Not one of DIV-0041's listed
  full-frame fills; for the owner's eye and the coordinator.
- **Unchecked indexes** (D200): the nine dispatchers do not bound `+2` (every writer
  in the band keeps it in its table); sub-kind 0x50's variant from the
  record's x byte indexes two cells (its heights follow) and `+0xB` four flag
  bytes; sub-kind 0x57's `+0xB` eight points (its own `& 7` keeps it after the
  first); `Gfx_BufferIndex` into layer 15's two last pointers (as
  `Gfx_CommitPrim` leaves its slot). Ours aborts past each but the last.
- **Draws twice, or not at all** (D205, D203): `EffectKind18Sub10_SlideIn` calls state 1
  (which draws the panel) and then draws it again unless state 1 moved on -
  the panel committed twice that frame; sub-kinds 0x11's and 0x12's state 1
  draws its tiles only on the frame it moves on, so from the start until the
  flag the tiles are not drawn (the map cells stay 0x10) - as read, perhaps
  deliberate.
- (D213) `EffectKind18Sub50_DrawWall` hands `Gte_RotTransPers4` its own second
  argument's slot as the flag out (the compiler's reuse of a dead slot);
  harmless.

## 8. Calls across groups

**Outbound, raw** (group E5D, wave five, which merges before E5C; 34 sites,
`band_rows.py --edges`): `0x5043B0` (14 calls), `0x503FA0` (15 calls; the
harness's row), `0x503E50` (5 tail jumps), from sub-kind 0x17's states -
`effect_5c_callees.h` `kDrawMoveStep`, `kDrawQuads`, `kDrawMoves`; and
`EffectKind18Sub17_States`' entry 15 is E5D's `0x503DE0` (a cell, read in
place). **By name, already ours**: `AreaMap_Elevation`, `Flags_Test`,
`Sound_PlayEffect`, `Effect_FindFree`, `Effect_Release`, `Rand`,
`MoveCmd_TestFB`, `MapView_LinkPrimAt`, `Gfx_CommitPrim`, `Gpu_LinkPrim`,
`Prim_SetTexture`, the `Gpu_*` / `Gte_*` / `Math_*` primitives. EGT's helpers
are not called.

**Inbound from outside the group**: none by code (`band_rows.py --refs`: 0
references in `src/game`); the ten `EffectKind18_States` cells
(`EffectKind18_Run`, ours, reads them in place).

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 60 rows, and no first-call trace under
`analysis/calltrace` (38 `bof3x.calltrace.tsv` files, their entry column)
names any of the 62 addresses. **Fuzz only.** No live run was made (the
brief). A route that shows one of these sub-kinds - the spawner is script
data, not traced - would let the coordinator's frame-hash A/B cover it.

## 10. The rebinding

`grep -rn -i` of the 62 addresses and the tables in `src/game`
(`band_rows.py --refs`: 0 references): **nothing to rebind**. **Left raw**,
for the rebinding pass after E5D merges: E5D's three in
`effect_5c_callees.h` and the fuzz file's rows `E5C_RAW(0x5043B0)` /
`E5C_RAW(0x503E50)` and `CallSite` targets (keys and labels). The table
`0x65E034` (E5B's and this group's) left unnamed.

## 11. For `analysis/calltrace/entries_logic.txt`

The main checkout's file (2026-10-03): 53 lines appended (the read extents
of the 62 less nine already listed by address), and eight of those nine
corrected in place to the code's extent - `00501A10 189` (was 28C),
`00501CA0 9D` (1D8), `00501E80 198` (234), `005020C0 3A9` (5A6),
`00502670 19F` (389), `00502A00 129` (2D9), `00502CE0 129` (5DD),
`005032C0 5A` (CD2); `00501850 1B8` was right. E5B's host line `00500EF0 954`
spans sub-kind 0x50's first five starts here; left for E5B and the
consolidation.
