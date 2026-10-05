# Group R3E: the effect engine's first band, what round thirteen left

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave three, on the
round branch's tip `7f116a2`. **50 functions ours** (`src/game/rest_3e.cpp`,
declarations in `src/game/rest_3e.h`, the cells and the one raw callee it
names by address in `src/game/rest_3e_callees.h`, shadow name `rest_3e`):
the cut's 50 rows for R3E (`analysis/round14_cut.tsv`), each read to its last
instruction with capstone and fuzzed through the scenario harness in effect
mode ([`scenario_harness.md`](scenario_harness.md) section 8), used
unchanged: 200,000 rounds, 0 mismatches. 94 controls planted one at a time: 93 refused, 1 an equivalent mutant with its near variant refused (section 6). One table named
(`EffectGlowSparks_States`). One function has a live route
(`EffectKind41_DrawNumber`, section 9); the rest are fuzz only.

**The band is effect code, all of it**, whatever the cut's classes said (26
rows paired with SCENA code by the catalog, 6 `hypothesis` rows). The SCENA
pairs are overlay addresses the scenario banks share with the PlayStation's
effect overlay (`0x801F6FAC..0x801F9F8C`); the sibling has no names for
them (only `0x801F719C` appears there, as chapter 2's vtable entry - another
overlay's code at the same address). Five things:

- **three kinds' dispatchers** - `EffectKind37_Run`, `EffectKind17_Run`,
  `EffectKind1B_Run` (`Effect_KindHandlers[0x37]`, `[0x17]`, `[0x1B]`), whose
  state tables FC1 named in round twelve and left "catalog part 2";
- **the helpers five merged kinds call by address** - FC2's kind 0x41's
  number (`EffectKind41_DrawNumber`), area 135's shards
  (`EffectShards_LoadModel`, `EffectShards_Step`: the same calls FC2's kind
  0x30 makes), E1C's kind 0x1D's speck (`EffectKind1D_DrawSpeck`), E1D's kind
  0x21's arm (`EffectKind21_ArmPoints`, `_DrawArm`, `_DrawArmTriangle`) and
  kind 0x24's ray (`EffectKind24_DrawRay`);
- **the pools kinds 0x48, 0x49 and 0x52 draw with** (E2D, E2E, E2F) - the
  eight glow sparks at `EffectKind30_Shards` (`EffectGlowSparks_*`, with their
  state table `EffectGlowSparks_States` `0x654660`), the trail record
  `0x92C060` (`EffectGlowTrail_Update`, `_Draw`), the spiral record
  `0x92C4A4` (`EffectSpiral_Init`, `_StepDraw`), a ring
  (`EffectRing_Draw`) and the 64 dust records at `0x92D1DC`
  (`EffectDust_*`);
- **kinds 0x5D and 0x5E's states** (`EffectKind5D_States` 0..3,
  `EffectKind5E_States` 0..2) and their draws: three windows that open, show
  an item (`+6`) with its count and a board of marks, and close; kind 0x5E
  picks one of the eight items `0x4E..0x55` with the pad and adds it to the
  counts at `0x903A10`;
- **kind 0x5F's states 1..9** (`EffectKind5F_States`; its state 0 is
  `Task_StartHold60`): the record placed at fixed points, moved by a velocity
  under a fall, drawn by R3F's `0x480300`, and released.

The names say what the code draws and steps, not what the player sees: where
the game shows kinds 0x5D..0x5F (their spawners: chapter 7, areas 104 / 121,
chapter 9b - [`effect_2g.md`](effect_2g.md) section 1) was not traced here.

## 1. What each function does

| Address | Name | What |
|---|---|---|
| `0x46A320` | `EffectKind37_Run` | `jmp [EffectKind37_States + +1 * 4]`, unbounded (ours aborts past 3) |
| `0x46ABB0` | `EffectKind17_Run` | the same through `EffectKind17_States` (6) |
| `0x46B7A0` | `EffectKind1B_Run` | the same through `EffectKind1B_States` (4) |
| `0x46D5F0` | `EffectKind41_DrawNumber(x, y, unused, clut)` | `Crt_sprintf(0x904BA0, Area08_MessageFormat, +6)`; a draw mode (page 0xF) committed to the slot `+0x29`; each character from the first (taken whatever it is - the loop tests for the NUL after it) to a NUL: a space skipped, else the character less `'0'` written back into the text and an 8 x 8 SPRT at `(x + 8 i, y)` (the low words, signed), u `(c + 0x16) * 8`, v 0xD0, CLUT `((clut & 0xFF) << 4, 0x1E0)`, committed (0x1C) |
| `0x46D710` | `EffectShards_LoadModel` | the byte `*(+0x54)`, signed, times 0x28 bytes copied one at a time from `*(+0x50)` (`+0x50` stepped each byte) to `0x8C5D80`; `+0x50` = `0x8C5D80`; `EffectKind30_ShardsInit`, `EffectKind30_SparksInit`; `+9` = 0x10 |
| `0x46D770` | `EffectShards_Step` | `EffectKind30_ShardsStep`, a tail `jmp` to `EffectKind30_SparksDraw` |
| `0x46E190` | `EffectKind1D_DrawSpeck(speck)` | the point `+4` projected; a TILE_1, opaque, white on an odd `Rand` else black; `Gfx_CommitPrim(1, 0x14)` (E2A's `EffectSpecks_Draw` links the same tile) |
| `0x46F570` | `EffectKind21_ArmPoints(arm)` | `c, s` = cos, sin of `+0x4C` times the length `+0x52`, `<< 4`; a matrix `EffectGte_SetDiagonalOne`, `Gte_RotMatrixY(+0x4E)`, `Gte_RotMatrixZ(+0x50)`; one `Gte_ApplyMatrixLV` of a stack vector never written, its answer dropped (L1); the four points `+0xC + 0x10 i` from `(0, 0)`, `(c, s)`, `(c, 0)`, `(c, -s)` through the matrix: x, y added to `+0`, `+4`, z `<< 8` added to `+8` |
| `0x46F690` | `EffectKind21_DrawArm(arm)` | a draw mode (`Gpu_GetTPage(0, 1, 0x3C0, 0)`, dtd 1, slot 1); `EffectGte_LoadMapCamera`; the triangles of points 0, 1, 2 and 0, 3, 2 |
| `0x46F6F0` | `EffectKind21_DrawArmTriangle(arm, a, b, c)` | a POLY_G3, semi-transparent, of the points a, b, c (bytes) projected; shades 0x40, 0, 0x80; `Gfx_CommitPrim(1, 0x34)` |
| `0x46FAE0` | `EffectKind24_DrawRay(ray)` | a draw mode; the direction `cos(+4) sin(+2) sar 8`, `sin(+4) sin(+2) sar 8`, the height step `cos(+2) << 4`; four LINE_G2, semi-transparent, the direction a quarter turned each, from the leader's point (ObjTrio `+0x34..+0x3C`) out by `(+6 * +0) sar 8` to `(+8 * +0) sar 8`, coloured the scale word times `+0xA..+0xC`, `sar 8`; `Gfx_CommitPrim(1, 0x24)` |
| `0x4790C0` | `EffectGlowSparks_Clear` | `+0` of the eight sparks (0x1C apart from `EffectKind30_Shards`) = 0; the sound flags `0x6761C9`, `0x6761C8` = 0 |
| `0x4790F0` | `EffectGlowSparks_StartRise(spark)` | `+0` 1, `+1` 0, `+2` 8, `+3` 0, `+8` 0, `+4` 0x40; x, z = `((Rand & 0xFF) - 0x80) << 11` round the record's point (read after each `Rand`), the height its `+0x3C` |
| `0x479160` | `EffectGlowSparks_StartBurst(spark)` | the same but `+4` 1, on a sphere: `a = Rand & 0x7FF`, `b = Rand & 0xFFF`, r the low word, signed, of `((Rand % 2) << 12 \| Rand & 0xFFF)`; x, z `(sin a cos b sar 12) r sar 8`, `(sin a sin b sar 12) r sar 8` round the record, the height `cos a * r + +0x3C + 0x2000000` |
| `0x479260` | `EffectGlowSparks_Run` | E2F's `EffectKind52_MoveSparks` instruction for instruction but its table: a draw mode, `EffectGte_LoadMapCamera`, each spark in use called through `EffectGlowSparks_States` by `+1` (handed the spark) and drawn; al 1 when one was in use |
| `0x4792E0` | `EffectGlowSparks_Draw(spark)` | E3A's `EffectKind68_DrawMote` but the size 0x20 and slot 1: 16 POLY_G3 discs round the projected point, radius the projected size + `(Frame_Counter & 1)`, centre the shade `+3`, rim black |
| `0x479420` | `EffectGlowSparks_Glow(spark)` | `EffectGlowSparks_States[0]`: `+3` up 6, `+2` down, at 0 `+2` = `+4`, `+1` up and `Sound_PlayEffect(0x200)` once (flag `0x6761C9`) |
| `0x479470` | `EffectGlowSparks_Rise(spark)` | `EffectGlowSparks_States[2]`: once (flag `0x6761C8`, set either way) sound 0x201 unless the record's `+6`; `+8` up 0x40000, `+0x14` up by it, `+3` up 6, `+2` down, at 0 freed |
| `0x4794D0` | `EffectGlowTrail_Update(trail)` | E2F's `EffectKind52_TrailUpdate` (`0x47D040`) and E3D's `EffectKind80_TrailStep` (`0x4873E0`) instruction for instruction but the call targets, which are the same callees: **ours calls `EffectKind52_TrailUpdate`** (section 2) |
| `0x4796B0` | `EffectGlowTrail_Draw(trail)` | E2F's `EffectKind52_TrailDraw` without its depth copies: the caps at points 0 and 31 (`EffectTrail_DrawCap`), two semi-transparent POLY_G4 per pair of points whose screen x or y differ (each pair's offset vertices at `angle +/- 0x400`, width `+0x1E`), shaded 0x80 down 4 a pair; the quads' depth words not written |
| `0x4799C0` | `EffectSpiral_Init(spiral)` | `EffectGte_LoadMapCamera`; the light `+0xD14` = `(0, 0x1000, 0x1000)` normalised in place; ring 0's 32 points (angle `0x80 i` under the tilt `+0xD10`): the shade 0x70 when the light's dot is at most 0, else `((0x5A7A90(dot), at most 0xFFF) sar 5 & 0x78) + 7`; the point `(x + a << 5, z + b << 5, y + c << 13)` projected to `+0x10 + 12 i`; each copied to rings 1..7 |
| `0x479B70` | `EffectSpiral_StepDraw(spiral)` | rings 0..6's points moved out one (the shades stay); ring 0 again (dark shade 7); a draw mode; 7 x 32 POLY_G4 between rings `k - 1` and `k`, shaded ring `k - 1`'s shades `i`, `i + 1` times `(8 - k) / 7` and ring `k`'s shade `i` twice times `(7 - k) / 7` (L2); `Gfx_CommitPrim(1, 0x44)` |
| `0x479EE0` | `EffectRing_Draw(ring)` | 32 POLY_G4 round the point (`+0`, `+4`, `+8`): the rim `(x + cos t << 5, z + sin t << 5)` at the height and at the height + 0x7000000, t 0x80 apart; before each a draw mode linked at the new rim point (`MapView_LinkPrimAt(x, z, 0, 0xC)`), the quad linked there (0x44); shades `min((s * +0x10) & 0xFFFF >> 7, 0xFF)`, `s` a byte from 0x80 down 4 for quads 4..19, else up 4 |
| `0x47A110` | `EffectDust_Clear` | `+0` of the 64 dust records = 0 |
| `0x47A130` | `EffectDust_FindFree` | the first dust record whose `+0` is 0, or null |
| `0x47A150` | `EffectDust_Start(dust)` | `+0` 1, `+1` 0, `+2` 0x10, `+0x14` = `+0x18` = 0; radius `((Rand % 2) << 8 \| Rand & 0xFF) << 8`, angle `Rand & 0xFC0`, round the record's point; `+0x1C` = `+0xC` = `AreaMap_Elevation(x, z) << 16` |
| `0x47A200` | `EffectDust_Run` | a draw mode; `EffectGte_LoadMapCamera`; each record in use: its column, its fan when the column answered 1; `+0x18` up 0x100000, `+0xC` up by it, `+0x14` up 0x2000, `+2` down, at 0 freed; al 1 when one was in use |
| `0x47A2B0` | `EffectDust_DrawColumn(dust)` | a POLY_F4, colour 0x40, between `(x, z, +0xC + (+0x14 << 8))` and `(x, z, +0xC - (+0x14 << 8))`, the bottom raised to the ground `+0x1C` (al 1 then, else 0); each end's screen x -/+ the float at `0x5C41B8`; `Gfx_CommitPrim(1, 0x38)` |
| `0x47A3D0` | `EffectDust_DrawFan(dust)` | 16 POLY_G3 round the dust's point at the ground, the centre shade 0x40, the rim black at `(x + cos t * 4, z + sin t * 4)` - the rim's height `AreaMap_Elevation` of the dust's own point, read again for each |
| `0x47F2D0` | `EffectKind5D_Start` | `Sprite_SetAnimationBank(0x1EA)`; `+0x3E` 0x64, `+0x48`, `+0x24`, `+0x2A`, `+9` 0, `+0xA` 0x1E, `+0x29` 1; `Sprite_SetAnimation(+6 - 0x4E)`; `+1` = 1 |
| `0x47F340` | `EffectKind5D_Open` | the three windows of `0x654858` (bit j of `+7` hides window j) outlined (`Window_DrawOutline`) at `min(w, n)` x `min(h, n)`, `n = +9 * 16`; `+9` up; when all six sides were full, `+1` = 2 |
| `0x47F3E0` | `EffectKind5D_Show` | the shown windows framed; window 0: the sprite at its corner (`+0x2E`, `+0x30`), `Sprite_ScriptTick`, `Sprite_QueueOverlay`; window 1: the item `+6`'s name and its `Inventory_Count` drawn; the board; `+0xA` down, at 0 any held button sets `+1` = 3 |
| `0x47F550` | `EffectKind5D_Close` | the outlines as `_Open`; `+9` down, at 0 `+1` = 4 (`Effect_StateRelease`) |
| `0x47F5F0` | `EffectKind5E_Start` | `+6` = 0x4E; `EffectKind5D_Start`; `+2` = 0 |
| `0x47F610` | `EffectKind5E_Pick` | the menu; `Input_Pressed` 0x1000 / 0x4000 move the pick `+2` (& 7, sound 0x206, `+6` = 0x4E + it, `Sprite_SetAnimation`); then 0x10: sound 0x209, `+0xA` 0x14, `+1` = 2; else 0x40: sound 0x20C, `+1` = 3; else 0x20: the item held - `0x903A10[+2]` up, held at 100, `Inventory_Remove(0, +6, 1)`, sound 0x20A - or sound 0x20D |
| `0x47F720` | `EffectKind5E_Wait` | the board's frame and the board; `+0xA` down, at 0 any pressed button: sound 0x20C, `+1` = 1 |
| `0x47F7A0` | `EffectKind5D_DrawBoard(x, y, w, h)` | an FT4 (page `(0, 0, 0x2C0, 0x100)`, CLUT `(0x40, 0x1E3)`) w x h at (x, y), the low words; then for each of the 8 items `min(0x903A10[i], 3)` marks from row i of `0x6548BC` (0xA none), `EffectKind5D_DrawMark` at (x, y) plus the mark's offsets from `0x6548D4` |
| `0x47F910` | `EffectKind5D_DrawMark(x, y)` | a 16 x 16 FT4 (CLUT `(0x30, 0x1E3)`), u 0xB0 / 0xC0, v 0x50 / 0x60 |
| `0x47F9E0` | `EffectKind5E_DrawMenu` | the menu box (`Menu_DrawBox`, flags 2) and its title (the text at `0x66A098`); the list; the hand at the pick; the panel; the sprite at the panel's corner and the panel's frame; `Sprite_ScriptTick`, a tail `jmp` to `Sprite_QueueOverlay` |
| `0x47FAF0` | `EffectKind5E_DrawList` | the list box; for items 0x4E..0x55 a line 0xF apart: the name in colour 0 when held, else 7 (`Text_CharCount`, `Text_DrawAt`), the count (`Text_DrawFont8`) |
| `0x47FBE0` | `EffectKind5E_DrawPanel` | the panel box and an FT4 over it (CLUT `(0x20, 0x1E3)`), u `0xC0 / w - 0x41`, v `0x50 / h + 0x50` |
| `0x47FDC0` | `EffectKind5F_Launch` | `EffectKind5F_States[1]`: `+9` down, at 0 the point `(0x510000, 0x420000, 0x2C00000)`, the rise `+0x14` 0x800000, `+1` up, sounds 0x202, 0x203 |
| `0x47FE30` | `EffectKind5F_Hop` | `[2]`: x down 0x10000, the height up by the rise, the rise down 0x200000; drawn (`0x480300(+0x34, 0x20, 8, 0)`); at x 0x490000 the velocity `(0x38E3, 0x10000, 0xB8E38E)`, `+1` up, sounds |
| `0x47FEE0` | `EffectKind5F_Fly` | `[3]`: the point moved by the velocity `+0xC..+0x14`, the rise down 0x200000; drawn; at z 0x4B0000 `+9` 0x3C, `+1` up |
| `0x47FF60` | `EffectKind5F_Bounce` | `[4]`: `+9` down, at 0 x = z = 0x4B0000 on the ground (`AreaMap_Elevation`), the velocity `(0xFFFF4925, 0xFFFF0000, 0xFFCC30C4 + 0x800000)`, `+1` up, sounds |
| `0x480010` | `EffectKind5F_FlyAway` | `[5]`: as `_Fly` with the rise down 0x100000; at z 0x360000 a tail `jmp` to `Effect_Release` |
| `0x480080` | `EffectKind5F_PlaceHigh` | `[6]`: the point `(0x270000, 0x80000, 0x8000000)`, the rise 0, `+1` up, sounds 0x204, 0x205 |
| `0x4800E0` | `EffectKind5F_Slide` | `[7]`: z up 0x10000, the fall as `_Hop`'s; drawn; at x 0xF0000 `Effect_Release` (x does not change in this state: L5) |
| `0x480140` | `EffectKind5F_PlaceLow` | `[8]`: the point `(0x190000, 0xF0000, 0x6000000)`, the rise 0, `+1` up, sound 0x20A |
| `0x480190` | `EffectKind5F_SlideOut` | `[9]`: as `_Slide`, drawn with the fourth word 4; at z 0x190000 or more `Effect_Release` |

## 2. Starts, extents, the cut

Every extent is the band tool's (`--clones`), read again by hand; the cut's
sizes run on into padding for 24 rows and are the catalog's guess. No start
is a case or a shared tail; **no code in the band is in no list** (the tool's
"code no list has": none). The hidden starts:

- `0x46A320` and `0x46ABB0` lie inside `0x46A1E0`'s catalog extent and
  `0x46B7A0` inside `0x46B6D0`'s; the hosts are FC1's `EffectKind32_Arc`
  (ends `0x46A287`) and `EffectKind17_CellBlocked` (ends `0x46B799`), whose
  code does not reach them. Each is `Effect_KindHandlers`' cell, so a
  function of its own.
- `0x479420`, `0x479470` lie in `0x4792E0`'s catalog extent (`004792E0 1E8`
  in `entries_logic.txt`): the draw ends at `0x47941D`; both are cells of
  `EffectGlowSparks_States`.
- `0x47F340..0x47F720` lie in `0x47F2D0`'s (`0047F2D0 300`), the kind 0x5D /
  0x5E states, each a cell of their tables; `0x47FDC0..0x480190` lie in
  `0x47FBE0`'s catalog extent, each a cell of `EffectKind5F_States`.

**Twins.** `EffectGlowTrail_Update` is byte for byte E2F's
`EffectKind52_TrailUpdate` (and E3D's `EffectKind80_TrailStep`) but the
relative call targets, which name the same callees; ours calls E2F's function
directly (not through the harness: its callees go through the harness inside
it), so the three stay one body. `EffectGlowSparks_Run` is
`EffectKind52_MoveSparks` but its table; `EffectGlowSparks_Draw` is E3A's
`EffectKind68_DrawMote` but two immediates; `EffectGlowTrail_Draw` is
`EffectKind52_TrailDraw` less its four depth copies. Those three are written
out (their tables and constants differ). De-duplicating the rest is the
round's refactor, not this group's.

## 3. The tables

| Table | Count | Read by | Entries |
|---|--:|---|---|
| `EffectGlowSparks_States` `0x654660` (named here) | 3 | `EffectGlowSparks_Run`, called through by a spark's `+1`, unbounded, each handed the spark | `EffectGlowSparks_Glow`, `EffectSpark_Wait` (E2F's), `EffectGlowSparks_Rise`; the next dword is `0x65466C` (kind 0x4A's table, named by its own dispatcher) |
| `EffectKind37_States` `0x653F58` | 3 | `EffectKind37_Run` | FC1's |
| `EffectKind17_States` `0x653FBC` | 6 | `EffectKind17_Run` | FC1's |
| `EffectKind1B_States` `0x653FD4` | 4 | `EffectKind1B_Run` | FC1's |

The states only write `+1` inside each count (`_Glow` steps 0 to 1,
`EffectSpark_Wait` 1 to 2, `_Rise` frees). The layout words of kinds 0x5D /
0x5E (`0x654858..0x654895`), the marks' rows (`0x6548BC`, 8 rows of 3, every
index below 10 or 0xA - checked at the image) and offsets (`0x6548D4`, room
11 to the format at `0x654900`) are read in place; their readers are this
group's, but they are not handler tables and are named only in
`rest_3e_callees.h`.

## 4. The fuzz (`rest_3e_fuzz.cpp`)

The scenario harness in effect mode, used unchanged; 4,000 rounds per
function, 200,000 in all, 9,265,112 calls to the stand-ins, **0
mismatches** (this worktree).

- **Shapes**: `kEffect` for the states, the dispatchers (each with its
  table's count as `state_span`) and the void helpers (`Sprite_Current` one
  of the 20 records, `+5` the clone's kind); `kCall` for the helpers with
  arguments - the arm, ray, speck and ring handed an effect record
  (`Arg::kEffect`); a spark, a dust record, the trail `0x92C060` or the
  spiral `0x92C4A4` set by `Args`; the arm triangle's points below 4 mostly.
  `ret_mask` 0xFF on `EffectGlowSparks_Run`, `EffectDust_Run` and
  `EffectDust_DrawColumn` (each answers in al), the whole eax on
  `EffectDust_FindFree` (a pointer).
- **Tables**: the three kinds' state tables are DataTables.
  `EffectGlowSparks_States`' handlers take the spark, so the table holds three
  typed stand-ins of the fuzz file's while the group runs (each logs the spark
  under its handler's address and moves the spark's shade, which the draw
  after it reads) and is put back after.
- **Regions** beyond effect mode's: `0x6761C4..0x6761D3` (the sound flags),
  `0x92C5C4..0x92D9DC` (the spiral record past the shards' standard 0x644,
  the dust after it), the model copy `0x8C5D80` (24 faces of 0x28).
- **Seeds**: the sparks (half in use, `+1` below 3, `+2` at 1 often, the
  flags 0 or set); the trail's screen floats (half the pairs equal or a
  float apart) and some angle words 0x1000; the dust (half in use, `+2` at
  1, the bottom at the ground and either side of it); `EffectShards_LoadModel`'s
  count byte -2..24 and source in the area block; kind 0x5D's `+9` at each
  window side's `/ 16` and either side, `+7`'s bits; the pad bits each state
  tests and `Input_Held`; the counts at 0, 3, 99, 100; kind 0x5F's `+9` at 1
  and each moving state's position one step from its compare.
- **Callees re-listed**: the group's own called directly (the arm triangle,
  the spark draw, the dust column and fan, kind 0x5D's start, the menu's
  three draws as `kPhase`, the board and the mark at their low words);
  FC2's `EffectKind30_ShardsInit` / `_SparksInit` / `_ShardsStep` /
  `_SparksDraw` (no standard row); E2F's `EffectTrail_DrawCap` and
  `EffectAngle_Mean` by address (as E2F lists them); R3F's `0x480300` by
  address; `Window_DrawFrame` / `_DrawOutline` at `{k16, k16, k8, k8}` (what
  `window_task.cpp` reads; the originals push whole registers);
  `Item_NamePtr`, `Inventory_Count` (0 a third of the time),
  `Inventory_Remove` at bytes; `Crt_sprintf` writing one to seven digits and
  spaces (the standard row can write none, on which the number's loop would
  read past the text); `EffectGte_SetDiagonalOne` and `Gte_ApplyMatrixLV`
  (below).
- **The arm's first `Gte_ApplyMatrixLV`** hands a stack vector the original
  never wrote (L1). The stand-in hashes the matrix always and the vector on
  every call but the first after `EffectGte_SetDiagonalOne` (whose stand-in
  starts the count again), and fills the out.
- **Disturbance** (the group's case, from its hash): `Sprite_Current`'s `+9`,
  `+0xA`, `+6`, `+2`; the two sound flags; `Input_Pressed`.

**What nothing reached**: every callee and every table entry was called
(the coverage line). The fuzz cannot show that ours reads what the state
reads *after* each call where no disturbance moves it; the controls that
move a read before a call (C73) are refused through the disturbance of
`Input_Pressed`.

## 5. Latent defects and ranges (Capcom's, described, not fixed)

- **L1. A stack vector never written.** `EffectKind21_ArmPoints` calls
  `Gte_ApplyMatrixLV` once before its loop with the loop's vector local
  still unwritten and drops the answer (the loop overwrites the out). The
  callee is pure; nothing reaches a draw. Ours passes zeros.
- **L2. The spiral's fourth vertex.** `EffectSpiral_StepDraw` shades each
  quad's fourth vertex (ring `k`, point `i + 1`) with ring `k`'s shade of
  point `i`, read twice; the third and fourth vertices always match. Ours
  reads the same.
- **L3. Stack words the callees do not read.** `EffectGlowSparks_Draw`'s
  size `{0x20, ?}` (the second word never written; `EffectGte_ProjectSize`
  reads the first), `EffectGlowTrail_Draw`'s last cap shade (a dword whose
  upper bytes are stack; the cap reads the byte), `EffectKind41_DrawNumber`'s
  third argument (unread). None reaches anything.
- **L4. The number's text.** `EffectKind41_DrawNumber` writes each digit
  less `'0'` back into the text scratch `0x904BA0`, and takes the first
  character whatever it is: an empty text would run the loop on into the
  bytes after its NUL (the index is a byte, 256 at most). A number's text
  is never empty; ours does the same.
- **L5. Kind 0x5F's `_Slide`** releases the record at `x == 0xF0000`, but
  nothing in the state changes x (`_PlaceHigh` puts it at 0x270000): reached
  from `_PlaceHigh`, the state runs until something else frees the record,
  its z climbing 0x10000 a frame and its height falling. Whether play
  reaches state 7 is the spawner's (chapter 7's `scena_sc7.cpp`); not
  traced.
- **L6. `EffectShards_LoadModel`'s count** is the byte `*(+0x54)` signed
  times 0x28 bytes copied to `0x8C5D80` with no bound (the shards read 24
  faces). Its caller hands the model it loaded; ours copies the same.
- **L7. Unbounded indexes** (ours aborts; nothing the fuzz or any state
  writes reaches them): the three dispatchers past their tables; a spark's
  `+1` past `EffectGlowSparks_States`' three; a mark index past the 11 the
  offsets have room for (the image's rows hold 0..9 and 0xA).

**Needs a ledger entry:** none. No read of memory the original never wrote
reaches a draw or a decision (L1 and L3 are unread or dropped).

## 6. Controls

94 planted by `scratchpad/r3e/control.py` with `controls_list.py` (plant on a
unique anchor, rebuild, run `BOF3X_R3E_ONLY=<clone>`, restore, rebuild), all
in `rest_3e.cpp`; the count is the rounds refused of 4,000:

| Id | Function | Plant | Result |
|---|---|---|---|
| C01, C02 | `EffectKind37_Run`, `EffectKind1B_Run` | dispatched through `EffectKind1B_States` / `EffectKind17_States` | refused (4000, 4000) |
| C04..C07 | `EffectKind41_DrawNumber` | v 0xD1; the space test on 0x21; the step 9; y zero-extended | refused (3815, 2569, 3253, 1927) |
| C08..C10 | `EffectShards_LoadModel` | `+9` 0x11; 0x27 bytes a face; `+0x50` stepped 2 | refused (4000, 3553, 3553) |
| C11 | `EffectShards_Step` | the two calls swapped | refused (4000) |
| C12, C13 | `EffectKind1D_DrawSpeck` | committed 0x15; `Rand & 2` | refused (4000, 1665) |
| C14..C16 | `EffectKind21_ArmPoints` | point 3 at `(c, s)`; z `<< 7`; the Y turn by `+0x50` | refused (3999, 4000, 4000) |
| C17 | `EffectKind21_DrawArm` | the second triangle 0, 3, 1 | refused (4000) |
| C18, C19 | `EffectKind21_DrawArmTriangle` | the third shade 0x7F; the third vertex point b | refused (4000, 3295) |
| C20..C22 | `EffectKind24_DrawRay` | the quarter turn's sign; `+5` from `+0xC`; the outer height + 1 | refused (3997, 3973, 4000) |
| C23 | `EffectGlowSparks_Clear` | the rise flag set to 1 | refused (4000) |
| C24, C25 | `EffectGlowSparks_StartRise` | `+4` 0x41; z `<< 10` | refused (4000, 3984) |
| C26, C27 | `EffectGlowSparks_StartBurst` | the height + 0x2000001; z `sar 7` | refused (4000, 4000) |
| C28 | | `Rand % 2` taken unsigned | **not refused: equivalent** - it differs only for a negative odd `Rand`; the game's `Rand` is 0..0x7FFF and the harness's answers a whole word only when it is a multiple of 4 |
| C28b | | near variant: `% 4` | refused (1661) |
| C29, C30 | `EffectGlowSparks_Run` | answers 2; state 1's spark not drawn | refused (3987, 3087) |
| C31..C33 | `EffectGlowSparks_Draw` | the angle step 0x101; `Frame_Counter & 2`; the size 0x18 | refused (4000, 3125, 4000) |
| C34, C35 | `EffectGlowSparks_Glow` | the shade + 5; the reload + 1 | refused (4000, 708) |
| C36, C37 | `EffectGlowSparks_Rise` | the sound when `+6` is 1; the speed + 0x40001 | refused (983, 4000) |
| C38 | `EffectGlowTrail_Update` | handed the trail + 0x20 | refused (4000) |
| C39..C41 | `EffectGlowTrail_Draw` | the shade step 3; the skip on either axis equal; the copy's last y into x | refused (4000, 3988, 4000) |
| C42..C44 | `EffectSpiral_Init` | dark 0x71; rings 1..6 only; the shade mask 0x7C | refused (4000, 4000, 4000) |
| C45..C47 | `EffectSpiral_StepDraw` | the fourth vertex's shade from n (L2 "fixed"); `/ 6`; ring 7 not moved | refused (4000, 4000, 4000) |
| C48..C50 | `EffectRing_Draw` | the fade from quad 5; the draw mode linked at dy 1; the product not masked to 16 bits | refused (3954, 4000, 3922) |
| C51 | `EffectDust_Clear` | the last record left | refused (1981) |
| C52 | `EffectDust_FindFree` | free by `+1` | refused (3986) |
| C53, C54 | `EffectDust_Start` | `+2` 0x11; the angle `& 0xFE0` | refused (4000, 1957) |
| C55, C56 | `EffectDust_Run` | the spread + 0x2001; the fan when the column answered 0 | refused (4000, 4000) |
| C57, C58 | `EffectDust_DrawColumn` | the top-left x + the half width; the clamp at `<=` | refused (4000, 411) |
| C59, C60 | `EffectDust_DrawFan` | the rim's z `<< 3`; the old rim shade 1 | refused (4000, 4000) |
| C61, C62 | `EffectKind5D_Start` | `+0xA` 0x1F; the animation `+6 - 0x4D` | refused (3976, 4000) |
| C63, C64 | `EffectKind5D_Open` | open at five full sides; the width full at `<` | refused (376, **5**) |
| C65..C67 | `EffectKind5D_Show` | the name's count 9; `+1` = 2; window 1 by bit 2 | refused (1921, 755, 1991) |
| C68 | `EffectKind5D_Close` | `+1` = 3 | refused (772) |
| C69 | `EffectKind5E_Start` | `+2` = 1 | refused (4000) |
| C70..C73 | `EffectKind5E_Pick` | the count held at `<=` 100; sound 0x208; the move's direction by 0x4000; `Input_Pressed` not read again after the move | refused (34, 1293, 1969, 118) |
| C74 | `EffectKind5E_Wait` | `+1` = 2 | refused (1770) |
| C75, C76 | `EffectKind5D_DrawBoard` | at most 2 marks; v 0x99 | refused (2408, 4000) |
| C77 | `EffectKind5D_DrawMark` | 17 high | refused (4000) |
| C78, C79 | `EffectKind5E_DrawMenu` | the hand's step 16; the box's flags 1 | refused (3507, 4000) |
| C80, C81 | `EffectKind5E_DrawList` | colour 6; the count's line + 2 | refused (3837, 4000) |
| C82 | `EffectKind5E_DrawPanel` | u - 0x40 | refused (4000) |
| C83..C94 | kind 0x5F's states | `_Launch`'s height + 1; `_Hop`'s rise + 1 and its compare at 0x4A0000; `_Fly`'s `+9` 0x3B; `_Bounce`'s rise + 1; `_FlyAway`'s fall 0x200000 and its release as `+0 = 0`; `_PlaceHigh`'s sounds swapped; `_Slide`'s compare 0xF0001; `_PlaceLow`'s sound 0x20B; `_SlideOut`'s fourth word 0 and its compare `>` | refused (1997; 1909, 1909; 1917; 1997; 4000, 1932; 4000; 1923; 4000; 4000, 468) |
| C95 | `EffectKind5D_Open` | the outlines' size (`+9 * 16`) read once, before the three outlines | refused (88) |
| C96, C97 | `EffectKind5D_Open`, `EffectKind5D_Close` | `+9` read before the outlines, not again after | refused (274, 274) |
| C98, C99 | `EffectGlowSparks_Glow`, `EffectGlowSparks_Rise` | the sound flag set before the sound, not after | refused (50, 136) |

C64 is refused by 5 rounds: the width test `<=` against `<` differs only where
a window's width is exactly `+9 * 16`, and the layout's widths are not all
multiples of 16; refused, a seed on each width's own value would make it
louder. C70 (34) and C73 (118) are the pick's 100 edge and the pad re-read
through the disturbance: refused.

**Under the repaired disturbance (2026-10-05, round fourteen's review item
1).** Before `b9dfe34` the group's cases 0 (`+9`) and 3 (the two sound
flags) never ran; C01..C94 were refused by the other cases. The 94 were
re-run on the repaired fuzz (a copy of the driver, `controls/rest_3e/` in
session 8cb2a236's scratchpad, anchors converted for a CRLF checkout, none
repaired): **93 refused, C28 not (the equivalent, as before)**; 10 counts
moved by at most 17 (C72), C64 still 5, C70 34 to 36, C73 118 to 106.
C95..C99 are new. Case 0: `Outlines` reads `+9` again after each
`Window_DrawOutline`, `_Open` and `_Close` after `Outlines`. Case 3: no
function re-reads a sound flag after a call - `_Glow` and `_Rise` read their
flag before their one call (the sound) and write it after - so the test the
case allows is the write's order, C98 and C99. With the formerly dead cases
skipped (a scratch gate, not committed) C95..C97 keep 76, 233 and 233 of 88,
274 and 274: `+9` is also moved by the other cases. C98 and C99 were refused
in 0 and 3 rounds by case 3 alone: the fuzz now re-lists `Sound_PlayEffect`
(`FxSoundFlags`: a quarter of the time one sound flag moved to 0 or
2..0xFF). On that fuzz the shadow is `200000 rounds over 50 functions (4000
each), 9265089 calls to the stand-ins, 0 MISMATCHES`, and all 99 were run
again: C01..C94 as above (93 refused, C28 not, C70 37), and the counts of
C95..C99 in the table are that run.

## 7. Calls across groups

- **Out**: R3F's `0x480300` (kind 0x5F's five moving states, 5 sites), by
  address through `rest_3e_callees.h` `kR3FRing` until R3F merges (R3F merges
  before this group by the round's order, section 4 of the plan: the
  rebinding pass turns it into R3F's name). Every other callee is ours or
  Capcom's library (`Rand`, `Crt_sprintf`, `0x5A7A90`, `_ftol`), by name;
  `EffectKind52_TrailUpdate` (E2F) is called directly.
- **In** (from outside the group, all ours, all by address until this
  merge): FC2's kind 0x41 (`EffectKind41_Hold`, `_Bounce`, `_Fade`:
  `field_c2_callees.h` `kDrawNumber`); area 135's `Area135_SpawnCopy` /
  `_SpawnCountdown` (`area_w3d_callees.h`); E1C's `EffectKind1D_MoveShards`
  (`kShardTile2`); E1D's kinds 0x21, 0x24 (`kArmVertices`, `kArmDraw`,
  `kRayDraw`); E2D's kind 0x49 (eight constants), E2E's kind 0x48 (eight),
  E2F's kind 0x52 (`kSparkInit`, `kSparkDraw`). The dispatchers and the
  states are reached only through their `.data` cells.
- **Harness rows** naming this group's functions (`scenario_harness.cpp`,
  not edited; each keyed by address, so each keeps working once ours is
  injected): kField's `{"0x46D5F0", .., {kU16, kU16, 0, kU8}}` (matches: x
  and y read as low words, the third unread, the clut byte); kEffectStd's
  `FX_RAW` rows for `0x46E190` (`{kAll}`: the speck pointer - matches),
  `0x46F570` (`{0}`, 84 bytes hashed - matches what it reads, to `+0x54`),
  `0x46F690`, `0x46FAE0` (`{0}`, 13 bytes - the ray read to `+0xC`),
  `0x4790C0`, `0x4790F0` and `0x479160` (24 bytes written - `0x479160` writes
  `+0..+0x17` as said; `0x4790F0` too), `0x479260` (`kFlag`: al - matches),
  `0x4794D0` (16 of 0x440 hashed - weaker than the reads, as section 8.9 of
  the harness doc says), `0x4796B0`, `0x4799C0`, `0x479B70` (16 bytes: the
  same weakness), `0x479EE0` (18 bytes: the ring reads `+0..+0x11`), `0x47A110`,
  `0x47A130` (`FxDustFindFree`: the real one's answer), `0x47A150` (8 read,
  32 written: the dust's `+0..+0x1F`), `0x47A200` (`kFlag`). None lists a
  function of this group as Capcom's by name (no `SH_THEIRS`), so no row
  stops the self-test; the round-end fold can move them to `FX_OURS`.

## 8. The rebinding

Rebound (the value unchanged, so the fuzz keys stand): `field_c2_callees.h` `kDrawNumber`; `area_w3d_callees.h` `kEngine46D710`, `kEngine46D770`; `effect_1c_callees.h` `kShardTile2`; `effect_1d_callees.h` `kArmVertices`, `kArmDraw`, `kRayDraw`; `effect_2d_callees.h` `kPuffsClear`, `kPuffStart`, `kPuffsStep`, `kGlowStep`, `kGlowDraw`, `kDustClear`, `kDustFindFree`, `kDustStart`, `kDustStep`; `effect_2e_callees.h` `kRing`, `kSpiralInit`, `kSpiralDraw`, `kSparksInit`, `kSparkSet`, `kSparksRun`, `kBurstStep`, `kBurstDraw`; `effect_2f_callees.h` `kSparkInit`, `kSparkDraw` - 26 constants in seven headers, each `= bof3::addr::<Name>` with the old address first in its comment (`area_w3d_callees.h` and `effect_1d_callees.h` now include `bof3/symbols.gen.h`). None of those files is another group's of this round.
Left raw on purpose: the fuzz files' call-site tables (the targets the
originals' bytes show) and their callee rows' address strings; the harness
rows above (the harness is not this group's to edit); comments naming the
addresses. R3F's `0x480300` stays raw in `rest_3e_callees.h`.

## 9. The live route

`analysis/calltrace`: **`EffectKind41_DrawNumber` (`0x46D5F0`) is entered 13
times in `hash_r13_nue_ours`, `_orig` and `_origb`** (the owner's
`cutsceneAndNue.txt`, recorded in round thirteen), always from FC2's kind
0x41. No trace names any other start of the band, and the cut's reach column
is empty for all 50. So `cutsceneAndNue.txt`'s state hash after the merge is
this group's live check for the number; the other 49 are **fuzz only**.

## 10. Self-tests and the entry list

- `BOF3X_SHADOW=rest_3e`: exit 0, 200,000 rounds, 0 mismatches (this
  worktree).
- `BOF3X_SHADOW='*'` in this worktree: exit 0 (1,080 s), and with `BOF3X_WIDE=1` exit 0 (1,081 s); 733 self-test lines each, no non-zero MISMATCHES line, `inject: 9393 ours`; `rest_3e`'s line 0 mismatches (9,265,953 calls - another stream than alone). Neither died silently.
- `tools/ledger_check.py`: 73 ledger entries, 0 errors (2 notes, not this group's).
- `analysis/calltrace/entries_logic.txt` (main checkout): the 20 starts that
  had no line appended with the extents read. The host lines `004792E0 1E8`
  (over `0x479420`, `0x479470`) and `0047F2D0 300` (over `0x47F340..0x47F720`)
  are left for the round's split; `0047FBE0 1A0`, `0046E190 70`,
  `0046F6F0 A0`, `0046D770 10` run into padding past the extents read.
- Nothing in `DIVERGENCE.md`, `cheats.cpp` or `widescreen.cpp` names an
  address of the band, and no full-frame fill is drawn in it.
