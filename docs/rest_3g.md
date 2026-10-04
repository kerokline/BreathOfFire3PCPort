# Group R3G: the effect bands' leftovers and the top-level rows between them

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave three, from
the round branch's tip `7f116a2`. **32 functions ours**
(`src/game/rest_3g.cpp`, shadow name `rest_3g`): the cut table's 32 rows for
R3G (`analysis/round14_cut.tsv`, the band `0x4925C0..0x5171FB`), none added,
none dropped (no start is a case, a shared tail or data). Each read to its
last instruction with capstone and fuzzed through the scenario harness in
effect mode ([`scenario_harness.md`](scenario_harness.md) section 8) without
edits to it: 128,000 rounds, 0 mismatches; 138 of 139 controls refused, the other an equivalent mutant whose near variant is refused. **Fuzz only**:
no recorded route enters any of the 32 (section 9). Thirteen `hypothesis`
rows of the cut are all functions. No full-frame fill is built by any of the
32 (the gradient kind 0xAC's states call is R3F's `0x492400`). No divergence;
**no ledger entry needed**. Two patched sites are read back: DIV-0041's four
cull bounds and DIV-0062's item array and item bound, all inside
`AreaMapBD_BuildView` (section 2).

The band is not one subsystem. By the code:

| Rows | What | Names |
|--:|---|---|
| 7 | effect states of kinds 0xAC, 0xAD, 0xAE and 0xBA, entries of E4F's tables | `EffectKindAC_Start` / `_FadeIn` / `_FadeOut`, `EffectKindAD_Start` / `_Rise`, `EffectKindAE_Start`, `EffectKindBA_Line` |
| 1 | the z of a screen triangle's cross product (three callers' back-face test) | `Screen_TriangleWinding` |
| 3 | the boss actors copied into the enemy records at a boss encounter; the enemies' state bytes cleared | `Battle_PlaceBossActors`, `Battle_PlaceBossActor`, `BattleEnemy_ClearStates` |
| 12 | game modes 8..11: the four step dispatchers (`GameMode_Handlers[8..11]`) and their steps not already ours | `GameMode8_Run` / `_Enter` / `_Leave`, `GameMode9_Run` / `_Enter` / `_Leave`, `GameMode10_Run` / `_Enter`, `GameMode11_Run` / `_Frame` / `_Look` / `_LookEnd` |
| 1 | a BMAGIC cell vertex's height from Quake's lift table | `Quake_VertexLift` |
| 2 | area 109's switch (`Area_CellHooks`' entry for area 0x6D) and the three-flag pattern it and E5A's sub-kind 9 read | `Area109_SwitchHook`, `Area109_SwitchPattern` |
| 2 | kind 0x18 sub-kind 0x41's two draws (E6B's `_Hold` calls them) | `EffectKind18Sub41_DrawPanels`, `_DrawRings` |
| 3 | area 0xBD's view: the frame, its build from a block map, a cell's texture | `AreaMap_FrameAreaBD` (named before, now ours), `AreaMapBD_BuildView`, `AreaMapBD_CellTexture` |
| 1 | a string's characters (kind 0xF's title lines) | `EffectKind0F_CharCount` |

What any of it looks like in play is not stated here (the owner's to say):
the descriptions are what the code reads, draws and writes. The names are
from the code; no PSX twin gives a name (the sibling's `names/*.toml` and
`symbols.toml` name none of the twins `analysis/pairs_propagated.json`
gives - section 5).

## 1. What each function does

### 1.1 The effect states

Each runs with `Sprite_Current` a record of `Effect_Objects`; E4F's
dispatchers jump to them through its tables
([`effect_4f.md`](effect_4f.md) section 3).

| PC | Name | Entry | What |
|---|---|---|---|
| `0x4925C0` | `EffectKindAC_Start` | `EffectKindAC_States[0]` | `+9` = 8, `+1` up |
| `0x4925E0` | `EffectKindAC_FadeIn` | `[1]` | R3F's gradient `0x492400` with the shade `s8(+9) * -0x20` (`imul cl`, ax; the push carries `Sprite_Current`'s upper half above it, the callee reads the byte); `+9` down; at 0 `+9` = 8 and `+1` up. `+9` running 8..1 gives the shades 0x00..0xE0 |
| `0x492620` | `EffectKindAC_FadeOut` | `[2]` | the gradient with 0xFF while `+9` is 8, else `+9 << 5` (a byte; `Sprite_Current`'s upper 24 bits above it); `+9` down; at 0 a tail jump to `Effect_Release` |
| `0x492680` | `EffectKindAD_Start` | `EffectKindAD_States[0]` | `+0x34` / `+0x38` the third party member's point (`ObjTrio` record 2's `+0x34` / `+0x38`, `0x80300C` / `0x803010`); `+0x3C` the ground there (`AreaMap_Elevation`, its s16 `<< 16`); the three copied to `+0xC..+0x14`; `+0x5D` 0, `+0x5E` 0x80, `+9` 0x20, `+1` up |
| `0x492710` | `EffectKindAD_Rise` | `[1]` | `+0x3C` up 0x800000; `EffectKindAD_DrawArc(+0x34, +0xC, +0x5D, +0x5E)`; `+1` up once the counter byte `0x903848` is 0xB |
| `0x492780` | `EffectKindAE_Start` | `EffectKindAE_States[0]` | `+0x34` 0x188000, `+0x38` 0x120000, `+0x3C` (ground + 0x800) `<< 16`; `+0x2E` 0x80, `+0x30` 0x180, `+0x32` 0, `+9` 8, `+1` up; `EffectGte_LoadMapCamera`; the point projected (a) and again 0x1000000 lower in its third word (b); `+0x32` = `Math_Ratan2(b.y - a.y, b.x - a.x) + 0x400`, each difference rounded to a float as `fstp` leaves it; sound 0x202 |
| `0x493E50` | `EffectKindBA_Line` | `EffectKindBA_States[1]` | from the angle `+0x6C` (four `Math_Sin` / `Math_Cos` calls, `+0x6C` read again for each): d1 = `((3 sin - cos) * 3 << 13) sar 12`, d2 = `((-sin - 3 cos) * 3 << 13) sar 12`; `+0x34` / `+0x38` / `+0x3C` = `Sprite_ObjectsExtra` record 1's x + d1, z + d2, y + 0x1000000; `+0xC` its x `- (s16 +0x2E << 15) + d1`, `+0x10` / `+0x14` as `+0x38` / `+0x3C`; `EffectKindBA_DrawLine(+0x34, +0xC, 0x40)`; the word `+0x2E` up, at 0x14A sound 0x216 and `+1` up |

`0x4925C0`..`0x492780` are hidden in R3F's `0x492400`; `0x493E50` in
`EffectKindB9_DrawShard` `0x493C60` (ours, E4F's). Each is entered only by its
table cell (`band_rows.py`).

### 1.2 `Screen_TriangleWinding` `0x4941B0`

`(const float *a, const float *b, const float *c)`, cdecl: on x87,
`(c.y - b.y)(b.x - a.x) - (b.y - a.y)(c.x - b.x)` - the z of `(b - a) x (c - b)`
of three screen points - and a tail jump to the CRT's `_ftol` `0x5B9550`
(truncated; eax answered). Its three callers test ax:
`EffectKind44_RingCone` (E2C), `EyeBeam_DrawCylinder` (S32) and
`Shisu_DrawModel` (R2B). Ours computes the same on x87 (`long double`, under
the game's own precision control) and truncates through a 64-bit integer, as
`_ftol`'s `fistp` does (section 7: the ax test).

### 1.3 The boss actors and the enemy records

| PC | Name | What |
|---|---|---|
| `0x494500` | `Battle_PlaceBossActors` | the encounter row `EventBattle_Records[0x904AAA]` byte `+2` names (`0x64DDEE + 4 * event`); `0x904AB2` 0; for each of the row's eight kinds not 0xFF, `Battle_PlaceBossActor(slot, the count so far, the kind)`; then `0x904AB3` = `0x904AB2`. Called first by `Battle_InitBossEncounter` `0x4942A0` |
| `0x494570` | `Battle_PlaceBossActor` | `BossActor_Find(slot)`; none, nothing. Else its first 0x80 bytes into enemy record `count` (`0x93B960 + 0x128 * count`, `rep movsd`); `Sprite_Current` = that record; `+1..+4` 0, `+0x29` 4, `+5` count + 3, the dwords `+0xC..+0x20` 0, the bytes `+0x5C..+0x5F`, `+6`, `+7`, `+0x2B` 0, `+8` the formation byte `0x904AAC ^ 2`, `+0x48` 0; `Battle_CopyEnemyData(count, kind)`, `Battle_SetEnemyOffset(count, kind's byte)`; the record's `+0x8F` 0; the actor's `+0` bit 6 set; `0x904AB2` up |
| `0x494E70` | `BattleEnemy_ClearStates` | bytes `+0..+4` of the eight enemy records 0. Called by `BattleEnd_Finish`, `BattleLoss_ResetParty`, `BattleLoss_Restart`, `Escape_Leave` |

`Battle_PlaceBossActors` passes the slot and the count from stack bytes whose
upper three bytes it never wrote (`sub esp, 8`, then byte stores); its callee
reads their low bytes only (BossActor_Find the tag's byte, the copy and the
offset their slot's byte), so nothing reaches what is drawn or decided.

### 1.4 Game modes 8..11

`Field_Task` calls `GameMode_Handlers[Game_Mode]`; entries 8..11 are this
group's dispatchers, each `xor eax, eax; mov ax, Game_Step; jmp [table +
eax * 4]`, unchecked. **The step tables are four, not one**:
`0x656AB8` is mode 8's (four entries), and modes 9, 10 and 11 jump through
`0x656AC8`, `0x656AD4` and `0x656AE0` (three each) - the words after mode 8's
fourth are mode 9's table, read by mode 9's own dispatcher.

| Table | Entries |
|---|---|
| `GameMode8_Steps` `0x656AB8` | `GameMode8_Enter`, `GameMode8_Frame` (E1F's), `GameMode8_Leave`, `GameMode8_TradeStep` (E1F's) |
| `GameMode9_Steps` `0x656AC8` | `GameMode9_Enter`, `Mode8_Step5` `0x517330` (FC3's), `GameMode9_Leave` |
| `GameMode10_Steps` `0x656AD4` | `GameMode10_Enter`, `Mode8_Step8` `0x517340` (FC3's), `GameMode9_Leave` |
| `GameMode11_Steps` `0x656AE0` | `GameMode11_Frame`, `GameMode11_Look`, `GameMode11_LookEnd` |

**For the coordinator**: FC3's `Mode8_Step5` and `Mode8_Step8` (and R2B's doc,
which calls the `Shisu_*` screen "game mode 8's step 8") read `0x656AB8` as
one table of thirteen; they are modes 9's and 10's step 1. Their names are not
this group's to change.

| PC | Name | What |
|---|---|---|
| `0x496440` | `GameMode8_Enter` | the dword `0x904144` up; `Fish_Spawn`; `Transition_Start(1)`; `AreaMap_Frame`; `Field_DrawFrame`; a draw-area move of the rectangle (0, buffer * 0xF0 + 0x50, 0x40, 0x78) to (0x340, 0x100) (`Gpu_SetDrawMove`), committed at slot 5 (0x18); `GameMode8_Frame`; `Field_ScriptFlags2` bit 6 cleared, `Game_Step` up, `Field_EdgeBits` 0, `Field_Request` 0 |
| `0x4964E0` | `GameMode8_Leave` | `Field_ScriptFlags2` bit 6 set; `0x90412C` bit 7 set and `Snd_LoadBankFile` of its low seven bits + 0x2C2, waited for (`GameMode8_WaitFrame`, `Task_Sleep(1)` a try); `Music_FadeOut(0x10)` when `Music_Track` is not 0xFF and not the byte `0x904CD0`; `Transition_Start(0)` and the wait word's framed wait; `Field_ChangeArea(the word 0x802290, the dwords 0x7E091C, 0x7E0920, 4)`; `Music_FadeOutStop(0xA)` on the same test; `Game_Mode` 1, `Game_Step` 0 |
| `0x4965D0` | `GameMode9_Enter` | `Transition_Start(2)` and its wait (`Field_LoadingFrame`, a sleep); two sleeps; `LoadDatFile(0xCB)` and its wait (a sleep a try); `Gfx_ClutStripCopyRow(1)`, `(2)`; `Game_Step` up; `Gfx_ClutStripDirty` 1 |
| `0x4966F0` | `GameMode10_Enter` | the same instruction for instruction with the file 0x31A |
| `0x496660` | `GameMode9_Leave` | (modes 9's and 10's step 2) two sleeps; the sound bank as above, not waited for; `Transition_Start(3)` and its wait; `Game_Mode` 2, `Game_Step` 0, `Field_Request` 0; a tail jump to `Field_Frame` |
| `0x496790` | `GameMode11_Frame` | `Mode11_ObjectFrame`, `Mode11_FieldFrame`; with `Field_Request` 0, `Game_Mode` 2 |
| `0x4967B0` | `GameMode11_Look` | `Look_PadControl`; a tail jump to `Mode11_FieldFrame` |
| `0x4967C0` | `GameMode11_LookEnd` | `Look_Return`, `Mode11_FieldFrame`; with the yaw word `0x929EC8` 0xFD56 and the pitch word `0x929ECC` 0x200, `Game_Step` 0 |

The dispatchers: `GameMode8_Run` `0x496430`, `GameMode9_Run` `0x4965C0`,
`GameMode10_Run` `0x4966E0`, `GameMode11_Run` `0x496780`. All twelve are
hidden in `GameMode_LookEnd`'s catalog extent (`0x496250`, ours, whose code
ends at `0x496281`).

### 1.5 `Quake_VertexLift` `0x4CF4B0`

`(long x, long z)`, cdecl, ax read by `MapCell_DrawTexQuads`,
`_DrawShadedQuads` and `_DrawSpinQuads` (BE6): the vertex as cells of the
block Quake heaves (S23's `0x695C2C` / `0x695C2E`, `magic_s23_callees.h`):
`(v + 0x4000) sar 6` less twice the block's first column / row; Quake's
facing bit 0 (`0x6959CC`) picks which axis is a and which b. Outside a
0..0x1F, b 0..0x1B: ax 0 (the upper half a's). Inside, with i = `(a >> 1) *
15 + (b >> 1)` into the lift table `0x695A2C`: both even `-(s8 [i + 1] + s8
[i + 0xF]) << 3`; b odd and a even `-(s8 [i + 0x10] + s8 [i + 1]) << 3`; b
even and a odd `-(s8 [i + 0x10] + s8 [i + 0xF]) << 3`; both odd `-s8 [i +
0x10] << 4`.

### 1.6 Area 109's switch

| PC | Name | What |
|---|---|---|
| `0x4FEE70` | `Area109_SwitchPattern` | the three story flags `0x65DE60` lists (`Flags_Test` on `0x904030`) as bits 0..2, plus 1: eax 1..8. Called by E5A's `EffectKind18_09_Pattern` (twice) and the hook |
| `0x4FEEB0` | `Area109_SwitchHook` | `Area_CellHooks`' function for area 0x6D, `(x, z)` cell bytes, al: at cell (0x1C, 6) with the leader's facing (`ObjTrio +8`) 3 and story flag 0x1C clear, the pattern mod 6 (`idiv`) written back as the three flags - each cleared, then set by its bit -, `Effect_HoldFlag1C(0xF)`, sounds 0x206 and 0x202, al 1; otherwise al 0 (the rest of eax the caller's) |

The pattern 1..8 mod 6 is 1..5 or 0..2: from 1..5 the flags step to the next
pattern (bits 1..5), and from 6, 7, 8 to bits 0, 1, 2 - the switch cycles the
three flags through six of their eight settings.

### 1.7 Sub-kind 0x41's two draws

| PC | Name | What |
|---|---|---|
| `0x5100B0` | `EffectKind18Sub41_DrawPanels` | `(lift, texture)`: a draw mode (page 0x95) committed at slot 5 (0xC); two POLY_FT4 from the eight three-byte vertices at `0x65ED30` - x = `b0 << 7 - 0x37C0`, z = `b1 << 7 - 0x2DC0`, y = `-0x3F8 - b2 * lift` (words) - projected (`Gte_RotTransPers4`, `Gte_PrimDepths4_10`), the texture word `(texture << 16) \| 0xBB009120`, committed at slot 5 (0x48) |
| `0x5101C0` | `EffectKind18Sub41_DrawRings` | `(size)`: a draw mode (dtd, page 0xB5) at slot 5; the vertex word z 0xFC08 once; three rings from r = size, c = (0x28 - size) * 5, each next r - 0x10, c + 0x10 (r below 0 taken as 0, c clamped 0..0xFF; R = `(r << 12) sar 10`): a LINE_F2 coloured (c / 2, c / 2, c) from (0xC840, R - 0x2CC0) to (0xC9C0, R - 0x2CC0); 16 chords of the circle of radius r about (-13888, -11456) at angles 0..0x400 by 0x40, each end `(sin * r sar 10 + x, cos * r sar 10 + y)` through `_ftol` and coloured c / 2; a line from (R - 0x3640, 0xD240) to (R - 0x3640, 0xD340); 16 chords about (-13888, -11712) at 0x400..0x800. Each line semi-transparent, projected end by end (`Gte_RotTransPers`, the depth stored with `Gte_StoreDepthF`) and committed at slot 5 (0x20); the draw mode again (no dtd) at the end |

The circles' centres are written into `MapView_ScreenXY` and read back from
there for every chord end (`fadd dword [0x903820]`); ours does the same.

### 1.8 Area 0xBD's view

| PC | Name | What |
|---|---|---|
| `0x510630` | `AreaMap_FrameAreaBD` | (`AreaMap_Frame`'s whole work in area 0xBD) `MapView_FocusX` / `Z` = `(0x800000 - Field_Kind2X / Z) sar 8`; with any of the three angle words moved from `Camera_AnglesDrawn`, `MapView_Redraw` 3 and both dwords copied; `Gte_RotMatrix(Camera_Angles, Camera_Matrix)`; the vector `(((FocusX >> 1) - 0x4000) & 0x7FFF) - 0x4000`, the same of `FocusZ`, `MapView_Elevation >> 1` (words, read after the matrix) through `Gte_ApplyMatrix`, plus `Camera_ShiftX`, `Camera_ShiftY` and `Camera_Distance` + 0x1194, as the translation; the matrix set; with `MapView_Redraw`, `AreaMapBD_BuildView` and the byte down |
| `0x510780` | `AreaMapBD_BuildView` | `DrawTable_Count` 0; this buffer's lists 0 and 1 of the 0x38 `DrawLayers` emptied; the camera's octant `((Cond_AngleFB + 0x100) sar 9) & 7` picks a record of six s8 at `0x65ED48` (x0, dx a column, dx a row, z0, dz a column, dz a row); the first cell x0 - `(byte (FocusX >> 8) - 0x80)` + 0x100 and z0 likewise (the words `0x903850` / `0x903852` and `MapView_Origin`); an odd octant walks 0x38 rows of 0x1C cells, an even one 0x2C of 0x28 (the dwords `0x903854` / `0x903858`, read back as the bounds); `DrawItemPool_Top` 0. Each cell, while `DrawItemPool_Top` is below its bound (0x400): x = `row * dxr / 2 + col * dxc + x0`, z = `(row + 1) * dzr / 2 + col * dzc + z0` (the halves toward zero); the block map `0x65ED78[((x sar 4) & 0xF) \| (z & 0xF0)]` gives the map cell `((block & 0xF0) \| (z & 0xF)) * 0x60 + ((block & 0xF) << 4 \| (x & 0xF))`; its first corner projected (`Gte_LoadVertex`, `Gte_Rtps`, `Gte_StoreScreenXY` into `MapView_ScreenXY`); kept when y > 120 and -200 < x < 520, or (y < 121 or unordered) and -50 < x < 370. A kept cell takes the next draw item (`DrawItemPool_Top` up), its texture word from the area block (`(height * width + 1) / 2 + offset + tile`, as `MapView_CellTextures` reads it) through `AreaMapBD_CellTexture`, its first screen point (copied through the FPU), its other three corners (`Gte_LoadVertices3`, `Gte_Rtpt`), a link into list 0 of layer 0x37 - row (`Gpu_LinkPrim`), `Gte_StoreScreenXY3`, `Gte_PrimDepths4_10` |
| `0x510BB0` | `AreaMapBD_CellTexture` | `(texture, quad)`: `Gpu_SetSemiTrans(quad, 0)`; the colour 0x80 x 3; the CLUT word `(((texture sar 24) & 0xF) + 0x1E3) << 6`, the page word 0x95; the 16 x 16 tile of the texture's low byte: u from `(texture & 0xF) << 4` to + 0xF, v from `texture & 0xF0` to + 0xF |

`AreaMapBD_BuildView` is the area's own form of `MapView_Build` (ours,
`map_layers.cpp`): the same cull floats (the narrow pair is `MapView_Build`'s
own `0x5C4234` / `0x5C4230`), but a bump index into `DrawItems` instead of
the pool's allocator, no side items, no draw table.

### 1.9 `EffectKind0F_CharCount` `0x5171E0`

`(const unsigned char *text)`, cdecl, eax: the characters to the string's
NUL, a byte with bit 7 taking the next byte with it (that byte not tested for
the NUL). Called by E1A's `EffectKind0F_Title`, `_TitleClose` and
`_LineType` (twice).

## 2. Divergence, and the patched sites read back

None: each function is a faithful replacement. `DIVERGENCE.md`,
`cheats.cpp`, `widescreen.cpp`, `draw_pool.cpp`, `labels.cpp` and the layout
files patch nothing inside 31 of the 32. Inside `AreaMapBD_BuildView`:

- **DIV-0041** (`Widescreen_Inject`, `BOF3X_WIDE=1`) re-aims the operands of
  the four `fcomp` x bounds (`0x51097E`, `0x510991`, `0x5109BB`, `0x5109D2`)
  at wider copies in our DLL. Ours reads each bound through the operand
  (`Bound(at::kWideLoAt)` ...), so it follows whichever is in place, as the
  original does; the two y bounds (`0x510968`, `0x5109A4`) are not patched
  and are read through their operands the same way.
- **DIV-0062** (`DrawPool_Grow`, last in `InjectAll`) re-aims the item
  array's immediate at `0x5109FC` and raises the bound word at `0x51087F` from
  0x400 to 0x800. Ours reads both from those bytes. So nothing in
  `draw_pool.cpp` needs to know about it: under `BOF3X_ORIGINAL=DrawPool`
  (PatchBytes leaves the sites) ours reads the original's array and bound,
  and under `BOF3X_ORIGINAL=AreaMapBD_BuildView` Capcom's code reads the
  patched ones - consistent either way.
- `DIVERGENCE.md`'s DIV-0041 and DIV-0062 text name the function at
  `0x510780` "`AreaMap_FrameAreaBD` ... Capcom's"; `AreaMap_FrameAreaBD` is
  `0x510630`, and `0x510780` is `AreaMapBD_BuildView`, now ours. **For the
  coordinator**: the two entries' text (not edited here).

No full-frame fill is built by any of the 32 (no 320.0 / 240.0 operand; the
gradient kind 0xAC's states call is R3F's `0x492400`).

## 3. The tables

**Named** (`symbols.toml` `[[data]]`; this group's functions read them):
`GameMode8_Steps` `0x656AB8` (4), `GameMode9_Steps` `0x656AC8` (3),
`GameMode10_Steps` `0x656AD4` (3), `GameMode11_Steps` `0x656AE0` (3), each
counted to the next table its own dispatcher reads (mode 11's to the button
map `0x656AEC` `Game_Boot` copies, data). The effect states' tables are E4F's
(`EffectKindAC_States` 3, `_AD_` 5, `_AE_` 5, `_BA_` 3), `Area_CellHooks` is
ART's, `Encounter_Rows` and `EventBattle_Records` are named already.

**Read in place, not named** (raw in `rest_3g_callees.h`; their bytes are
not copied here): the octant records `0x65ED48` (8 x 6 s8) and the block map
`0x65ED78` (256 bytes; both nibbles of every byte at most 5, so a map row or
column at most 0x5F), sub-kind 0x41's vertices `0x65ED30` (8 x 3), the
pattern's flag numbers `0x65DE60` (3, E5A's `kStoryBits`), Quake's cells
(S23's names), the event battles' row bytes `0x64DDEE + 4 * event`.

## 4. The fuzz (`rest_3g_fuzz.cpp`)

Two `Run`s under `BOF3X_SHADOW=rest_3g`, effect mode (`g.effect`; kinds
0xAC, 0xAD, 0xAE, 0xBA), 4,000 rounds a function: the 31, and
`AreaMapBD_BuildView` alone (`rest_3g build`). `BOF3X_R3G_ONLY=<name>` runs
the clones whose name holds it. Shapes: the seven effect states `kEffect`
with their kind; the cdecl helpers `kCall` (`Screen_TriangleWinding` its
three points and `EffectKind0F_CharCount` its string in the harness's
scratch, `AreaMapBD_CellTexture` its quad in the packet buffer); the
dispatchers, the mode steps, `Battle_PlaceBossActors`,
`BattleEnemy_ClearStates`, `AreaMap_FrameAreaBD` and the build `kState`.
Answers compared: `Screen_TriangleWinding` eax, `Quake_VertexLift` ax,
`Area109_SwitchPattern` eax, `Area109_SwitchHook` al, `EffectKind0F_CharCount`
eax. The four mode step tables are `DataTable`s.

**Regions** beyond effect mode's: the battle bytes `0x904AA0` (0x20), the
eight enemy records, `Encounter_Rows`, Quake's .bss `0x695990` (0x2A4, S23's
region), the byte `0x904CD0`, mode 8's return x / z `0x7E091C`, the return
area word with `DrawLayers` (`0x802290`, 0xA90), `Camera_AnglesDrawn`,
`MapView_Origin`, `DrawItemPool_Top`, the first 48 `DrawItems`. 37,548 bytes
in 56 regions.

**Callees** re-listed or added: the group's own called directly
(`Battle_PlaceBossActor` its three bytes; `Area109_SwitchPattern` answering
1..8 whole, as `lea eax, [edi + 1]` does; `AreaMapBD_BuildView` `kPhase`;
`AreaMapBD_CellTexture` the texture's bits 24..27 and low byte and the quad);
R3F's `0x492400` by address, its byte; `EffectKindAD_DrawArc` and
`EffectKindBA_DrawLine` their two points hashed and their shade bytes;
`BossActor_Find` answering null a quarter of the time, else a
`Sprite_Objects` record (its caller copies 0x80 bytes from it);
`Battle_CopyEnemyData` (bytes), `Battle_SetEnemyOffset` (byte, word);
`Fish_Spawn`, `GameMode8_Frame` / `_WaitFrame`, `Field_Frame`,
`Mode11_ObjectFrame` / `_FieldFrame`, `Look_PadControl` / `_Return` as
`kPhase`; `Effect_HoldFlag1C` its byte; `Gte_ApplyMatrix` (the matrix and the
vector hashed, the out filled), `Gte_LoadVertices3` (24 bytes),
`Gte_Rtpt`, `Gte_StoreScreenXY3` (three points filled) and
**`Gte_StoreScreenXY` louder**: the build's cull reads the point it stores,
so the stand-in writes a point far outside both ranges 47 times in 48 (a kept
cell takes a draw item, and only 48 are compared) and otherwise each
coordinate at or about a bound of the two ranges (-200, 520, -50, 370; 120,
121, one either side), or a quiet or a signalling NaN.

**Seeds** (per function, after the harness's fill): `+9` at 1, 2, 7, 8, 9, 0
for the fade states; the counter byte at 0xB for `_Rise`; `+0x2E` at 0x149 /
0x14A for `_Line`; three small float points for the winding (two equal a
quarter of the time); the event battle below 24 (each of those names a row
below 8) and half the encounter row bytes 0xFF; a count below 8 for
`Battle_PlaceBossActor`; `Game_Step` below each dispatcher's table; the wait
word 0 two times in three, `Music_Track` 0xFF, equal to `0x904CD0` or not for
the leaving steps; the yaw and pitch words at their rest values for
`_LookEnd`; `Gfx_BufferIndex` 0 or 1 for the draws (a random byte indexes
`DrawLayers` past its end); Quake's block corner small and each coordinate a
cell of the block from -2 to 0x22 with a fraction (both bounds and past
them); the switch's cell bytes (0x1C, 6) and the facing 3 most of the time;
`DrawRings`' size about its clamps (0, 0x10, 0x28, 0x29, -1, -0x10, -0x18,
-0x30, ...); a string of Latin and two-byte characters with its NUL anywhere
and two at the scratch's end; the angles drawn equal to the angles half the
time for the frame.

**Disturbance** (the group's, from the hash only - the harness hands it a
hash whose bits 4..7 are fixed and which is never a multiple of 3, so the
case is drawn from bits 8 up): `+9`, `+0x2E`, the counter byte, `Music_Track`,
`MapView_Redraw`, the yaw / pitch words, the build's row and column bounds
(cutting a walk short), `DrawItemPool_Top` (at 0x3FE..0x401: the bound), the
build's first column / row words, `0x904AB2`, a vertex word, a screen
coordinate.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=rest_3g`,
exit 0): 124,000 rounds over the 31, 5,042,930 calls to the stand-ins, **0
mismatches**; the build 4,000 rounds, 1,356,297 calls, **0 mismatches**;
37,548 bytes of state in 56 regions each. Every entry of the four step tables
reached (each handler recorder about 1,000..2,700 calls), `Battle_PlaceBossActor`
15,875 calls from the placement, `Area109_SwitchPattern` about 300 from the
hook, `Effect_Release` 585 from the fade-out, `GameMode8_WaitFrame` 37,642
(the leave's framed waits); the build kept about 3,000 cells over its 4,000
rounds (`AreaMapBD_CellTexture`, `Gte_LoadVertices3`, `Gpu_LinkPrim` each).
The first run found no mismatch class.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0,
734 self-test lines, no `MISMATCHES` line but 0, `inject: 9375 ours, 0 left
original`; **with `BOF3X_WIDE=1`** exit 0, 734 lines, no mismatch, the same
`inject` line (`rest_3g` there 5,039,418 calls, the build 1,338,909 narrow /
1,321,428 wide - the build's count moves with the stream). Neither run died
silently. `tools/ledger_check.py`: 73 entries, 0 errors.

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` (through the round's `band14.py`) read the 32:
  5,700 bytes against the cut's 5,820, nineteen differing by padding only,
  none by code; each checked by hand to its `ret` or tail `jmp`. No shared
  tail, no case, no second entry; no code of the band no list has.
- **Hidden starts**: 20, each an entry by address - a cell of E4F's state
  tables (7), of `GameMode_Handlers` (4), of this group's four step tables
  (8), of `Area_CellHooks` (1). Their catalog hosts: R3F's `0x492400` (whose
  code ends at `0x49250F`), `EffectKindB9_DrawShard` `0x493C60` (ends
  `0x493E0D`), `GameMode_LookEnd` `0x496250` (ends `0x496281`), and
  `0x4FEE70` (this group's, ends `0x4FEEA4`). None contains another's code.
- **The cut's columns**: the `unit` hints are mostly wrong - "Top-level modes"
  for the winding, the boss placement and the enemy clear; "field core" and
  "table 0x656AC8 read by 0x4965C0" for mode steps (right in substance: the
  table is mode 9's); "battle_e6" for Quake's lift; "Scenario effects
  (SCE1xEF)" / "SCENA" for kind 0xAE's and 0xBA's states (effect states; their
  PSX twins are in those overlays, section 9).
- **PSX twins** (`pairs_propagated.json`): `0x492780` 0x801D10AC (callers),
  `0x493E50` 0x801F7C24 (call-disputed), `0x4941B0` 0x801B0F88,
  `0x494E70` 0x800A9EB0 (call-anchored), the mode steps 0x801994E8,
  0x80198620, 0x80199780, 0x8019983C, 0x80199940, 0x80199B00, 0x80199B44,
  0x80199B6C (callers or table-anchored). None has a name in the sibling's
  `names/*.toml` or `symbols.toml`; none was read, so each is cited in its
  evidence string as a pairing, not as a name. `AreaMap_FrameAreaBD`'s `psx`
  (0x801F2C04, transferred earlier) is an area-overlay entry the sibling's
  `names/area_records.toml` lists for many areas; kept, not verified.
- **The harnesses' rows for these addresses** (not edited): `scenario_harness`'s
  `kEffectStd` `FX_RAW` rows for `0x4941B0` (three points of 8 hashed,
  garbage - matches), `0x4FEE70` (`kByte` 1..8 whole, `FxPattern` - matches),
  `0x5100B0` (two whole words - wider than the low words the function reads,
  harmless for its one caller), `0x5101C0` (one word - matches), `0x5171E0`
  (the string, garbage where its callers compare the count - E1A's note);
  `boss_harness`'s `0x494E70` (`kThrough`) and `0x4CF4B0` (two words,
  garbage where ax is read). All are keyed by address, so none stops a
  self-test once the names are ours: a raw call still finds its row, and a
  call by name finds this group's listing.

## 6. Controls

Planted one at a time by a scratch script (`controls.py` in the group's scratch: each plant anchored on a
unique string of `rest_3g.cpp`, the DLL rebuilt, the shadow run under `BOF3X_R3G_ONLY=<filter>`, the file
restored and rebuilt at the end; the committed file has no switch). The column is the first differing round
(0-based) in this worktree; every refused run exited 3 on a `MISMATCH` line. **138 of 139 refused.**
Not refused: **133**, an equivalent mutant - the kept cell's screen x copied with `memcpy` instead of
through the FPU: a cell is kept only when its x compared ordered against two bounds, so x is never a NaN
and the two copies agree on every input; the near variant **139** (the y copy, which a kept cell may carry
as a signalling NaN through the narrow test) is refused. A first pass found **37** (`0x904AB3` written 0
instead of copied from `0x904AB2`) not refused: the group's disturbance then drew its case from the low
bits of a hash the harness hands over with bits 4..7 fixed and never a multiple of 3, so four of its twelve
cases never ran; the case is drawn from bits 8 up since, and 37 is refused.

| # | Run (`_ONLY`) | Plant | For | Refused at |
|--:|---|---|---|---|
| 1 | `AC_Start` | `S()[9] = 9; S()[1] = static_cast<unsigned char>(S()[1] + 1); } // o...` | `S()[9] = 8; S()[1] = static_cast<unsigned char>(S()[1] + 1); } // o...` | round 0 |
| 2 | `AC_Start` | `S()[9] = 8; S()[1] = static_cast<unsigned char>(S()[1] + 2); } // o...` | `S()[9] = 8; S()[1] = static_cast<unsigned char>(S()[1] + 1); } // o...` | round 0 |
| 3 | `AC_FadeIn` | `S8(s[9]) * -0x10)` | `S8(s[9]) * -0x20)` | round 1 |
| 4 | `AC_FadeIn` | `if (S()[9] != 1) return; S()[9] = 8;` | `if (S()[9] != 0) return; S()[9] = 8;` | round 2 |
| 5 | `AC_FadeIn` | `(AddressOf(s) & 0xFFFF0000u) \| (product + 1)` | `(AddressOf(s) & 0xFFFF0000u) \| product` | round 0 |
| 6 | `AC_FadeOut` | `n == 7 ? 0xFFu` | `n == 8 ? 0xFFu` | round 1 |
| 7 | `AC_FadeOut` | `static_cast<unsigned char>(n << 4)` | `static_cast<unsigned char>(n << 5)` | round 2 |
| 8 | `AC_FadeOut` | `if (S()[9] == 1) SH_CALL(Effect_Release)();` | `if (S()[9] == 0) SH_CALL(Effect_Release)();` | round 2 |
| 9 | `AD_Start` | `SetUL(S() + 0x34, UL(at::kMember2Z));` | `SetUL(S() + 0x34, UL(at::kMember2X));` | round 0 |
| 10 | `AD_Start` | `static_cast<U>(static_cast<std::int16_t>(h)) << 15);` | `static_cast<U>(static_cast<std::int16_t>(h)) << 16);` | round 0 |
| 11 | `AD_Start` | `s[0x5E] = 0x81;` | `s[0x5E] = 0x80;` | round 0 |
| 12 | `AD_Start` | `s[9] = 0x21;` | `s[9] = 0x20;` | round 0 |
| 13 | `AD_Start` | `SetUL(s + 0x14, UL(s + 0x38));` | `SetUL(s + 0x14, UL(s + 0x3C));` | round 0 |
| 14 | `AD_Rise` | `UL(s + 0x3C) + 0x400000u` | `UL(s + 0x3C) + 0x800000u` | round 0 |
| 15 | `AD_Rise` | `s[0x5E], s[0x5D]);` | `s[0x5D], s[0x5E]);` | round 0 |
| 16 | `AD_Rise` | `if (At(at::kCounter)[0] == 0xC)` | `if (At(at::kCounter)[0] == 0xB)` | round 1 |
| 17 | `AE_Start` | `SetUL(S() + 0x38, 0x128000u);` | `SetUL(S() + 0x38, 0x120000u);` | round 0 |
| 18 | `AE_Start` | `static_cast<std::int16_t>(h) + 0x400) << 16` | `static_cast<std::int16_t>(h) + 0x800) << 16` | round 0 |
| 19 | `AE_Start` | `SetWord(s + 0x30, 0x181);` | `SetWord(s + 0x30, 0x180);` | round 0 |
| 20 | `AE_Start` | `static_cast<U>(point[2]) - 0x800000u` | `static_cast<U>(point[2]) - 0x1000000u` | round 0 |
| 21 | `AE_Start` | `SH_CALL(Math_Ratan2)(dx, dy);` | `SH_CALL(Math_Ratan2)(dy, dx);` | round 0 |
| 22 | `AE_Start` | `static_cast<U>(angle) + 0x200u);` | `static_cast<U>(angle) + 0x400u);` | round 0 |
| 23 | `AE_Start` | `SH_CALL(Sound_PlayEffect)(0x203); } // ====` | `SH_CALL(Sound_PlayEffect)(0x202); } // ====` | round 0 |
| 24 | `AE_Start` | `const float dx = static_cast<float>(static_cast<long double>(b[0]) ...` | `const float dx = static_cast<float>(static_cast<long double>(b[0]) ...` | round 0 |
| 25 | `BA_Line` | `* 3u) << 13, 11); const int cos2` | `* 3u) << 13, 12); const int cos2` | round 0 |
| 26 | `BA_Line` | `SetUL(s + 0x38, UL(at::kExtra1Z) + d1);` | `SetUL(s + 0x38, UL(at::kExtra1Z) + d2);` | round 0 |
| 27 | `BA_Line` | `(static_cast<U>(S16(s + 0x2E)) << 14)` | `(static_cast<U>(S16(s + 0x2E)) << 15)` | round 0 |
| 28 | `BA_Line` | `if (Word(S() + 0x2E) != 0x149) return;` | `if (Word(S() + 0x2E) != 0x14A) return;` | round 1 |
| 29 | `BA_Line` | `reinterpret_cast<const long*>(s + 0xC), 0x41);` | `reinterpret_cast<const long*>(s + 0xC), 0x40);` | round 0 |
| 30 | `BA_Line` | `SH_CALL(Sound_PlayEffect)(0x215);` | `SH_CALL(Sound_PlayEffect)(0x216);` | round 1 |
| 31 | `BA_Line` | `const int cos2 = SH_CALL(Math_Sin)(Long(S() + 0x6C));` | `const int cos2 = SH_CALL(Math_Cos)(Long(S() + 0x6C));` | round 0 |
| 32 | `Winding` | `(static_cast<long double>(c[1]) - static_cast<long double>(a[1])) *` | `(static_cast<long double>(c[1]) - static_cast<long double>(b[1])) *` | round 0 |
| 33 | `Winding` | `Ftol(second - first)` | `Ftol(first - second)` | round 0 |
| 34 | `Winding` | `Ftol(first - second + 0.5L)` | `Ftol(first - second)` | round 3 |
| 35 | `PlaceBossActors` | `if (kind == 0xFE) continue;` | `if (kind == 0xFF) continue;` | round 0 |
| 36 | `PlaceBossActors` | `SH_CALL(Battle_PlaceBossActor)(slot, count + 1u, kind);` | `SH_CALL(Battle_PlaceBossActor)(slot, count, kind);` | round 0 |
| 37 | `PlaceBossActors` | `At(at::kPlacedTotal)[0] = 0;` | `At(at::kPlacedTotal)[0] = At(at::kPlaced)[0];` | round 50 |
| 38 | `PlaceBossActors` | `for (unsigned slot = 0; slot < 7; ++slot)` | `for (unsigned slot = 0; slot < 8; ++slot)` | round 3 |
| 39 | `PlaceBossActor` | `for (unsigned i = 0; i < 0x7C; i += 4)` | `for (unsigned i = 0; i < 0x80; i += 4)` | round 1 |
| 40 | `PlaceBossActor` | `s[5] = static_cast<unsigned char>(count + 2);` | `s[5] = static_cast<unsigned char>(count + 3);` | round 1 |
| 41 | `PlaceBossActor` | `At(at::kFormation)[0] ^ 1);` | `At(at::kFormation)[0] ^ 2);` | round 1 |
| 42 | `PlaceBossActor` | `SH_CALL(Battle_SetEnemyOffset)(count, kind & 0x7F);` | `SH_CALL(Battle_SetEnemyOffset)(count, kind & 0xFF);` | round 1 |
| 43 | `PlaceBossActor` | `actor[0] = static_cast<unsigned char>(actor[0] \| 0x20);` | `actor[0] = static_cast<unsigned char>(actor[0] \| 0x40);` | round 1 |
| 44 | `PlaceBossActor` | `+ 0x8E)[0] = 0;` | `+ 0x8F)[0] = 0;` | round 1 |
| 45 | `PlaceBossActor` | `SetUL(s + 0x24, 0);` | `SetUL(s + 0x20, 0);` | round 1 |
| 46 | `ClearStates` | `for (unsigned k = 0; k < 4; ++k) e[k] = 0;` | `for (unsigned k = 0; k < 5; ++k) e[k] = 0;` | round 0 |
| 47 | `ClearStates` | `for (unsigned i = 0; i < 7; ++i) { unsigned char* const e` | `for (unsigned i = 0; i < at::kEnemyCount; ++i) { unsigned char* con...` | round 0 |
| 48 | `GameMode8_Run` | `AddressOf(GameMode9_Steps), GameMode8_Steps_count);` | `AddressOf(GameMode8_Steps), GameMode8_Steps_count);` | round 0 |
| 49 | `GameMode8_Enter` | `static_cast<U>(Gfx_BufferIndex) * 0xF0u + 0x40u);` | `static_cast<U>(Gfx_BufferIndex) * 0xF0u + 0x50u);` | round 0 |
| 50 | `GameMode8_Enter` | `rect, 0x340, 0x101);` | `rect, 0x340, 0x100);` | round 0 |
| 51 | `GameMode8_Enter` | `Word(At(at::kFlags2)) & 0xFF7Fu);` | `Word(At(at::kFlags2)) & 0xFFBFu);` | round 0 |
| 52 | `GameMode8_Enter` | `SH_CALL(Transition_Start)(2); (void)0;` | `SH_CALL(Transition_Start)(1);` | round 0 |
| 53 | `GameMode8_Leave` | `static_cast<unsigned char>(At(at::kFlags2)[0] \| 0x20);` | `static_cast<unsigned char>(At(at::kFlags2)[0] \| 0x40);` | round 1 |
| 54 | `GameMode8_Leave` | `SH_CALL(Snd_LoadBankFile)((b & 0x7Fu) + 0x2C3u); } WaitLoad` | `SH_CALL(Snd_LoadBankFile)((b & 0x7Fu) + 0x2C2u); } WaitLoad` | round 0 |
| 55 | `GameMode8_Leave` | `SH_CALL(Music_FadeOut)(0x11);` | `SH_CALL(Music_FadeOut)(0x10);` | round 0 |
| 56 | `GameMode8_Leave` | `SH_CALL(Field_ChangeArea)(area, I(z), I(x), 4);` | `SH_CALL(Field_ChangeArea)(area, I(x), I(z), 4);` | round 0 |
| 57 | `GameMode8_Leave` | `Game_Mode = 2;` | `Game_Mode = 1;` | round 0 |
| 58 | `GameMode8_Leave` | `WaitLoad(nullptr);` | `WaitLoad(&Mode8Frame);` | round 6 |
| 59 | `Leave` | `return track != 0xFE && At(at::kMusicPlaying)[0] != track;` | `return track != 0xFF && At(at::kMusicPlaying)[0] != track;` | round 4 |
| 60 | `GameMode9_Run` | `AddressOf(GameMode10_Steps), GameMode9_Steps_count);` | `AddressOf(GameMode9_Steps), GameMode9_Steps_count);` | round 0 |
| 61 | `GameMode9_Enter` | `void MenuModeEnter(int file) { SH_CALL(Transition_Start)(3);` | `void MenuModeEnter(int file) { SH_CALL(Transition_Start)(2);` | round 0 |
| 62 | `GameMode9_Enter` | `Gfx_ClutStripDirty = 2;` | `Gfx_ClutStripDirty = 1;` | round 0 |
| 63 | `GameMode9_Enter` | `SH_CALL(Gfx_ClutStripCopyRow)(3);` | `SH_CALL(Gfx_ClutStripCopyRow)(2);` | round 0 |
| 64 | `GameMode9_Enter` | `WaitWord(&LoadingFrame); SH_CALL(Task_Sleep)(1); SH_CALL(LoadDatFile)` | `WaitWord(&LoadingFrame); SH_CALL(Task_Sleep)(1); SH_CALL(Task_Sleep...` | round 0 |
| 65 | `GameMode9_Enter` | `MenuModeEnter(0xCC); }` | `MenuModeEnter(0xCB); }` | round 0 |
| 66 | `GameMode9_Leave` | `SH_CALL(Transition_Start)(4);` | `SH_CALL(Transition_Start)(3);` | round 0 |
| 67 | `GameMode9_Leave` | `Game_Mode = 2; Game_Step = 0; Field_Request = 1;` | `Game_Mode = 2; Game_Step = 0; Field_Request = 0;` | round 0 |
| 68 | `GameMode9_Leave` | `` | `LoadBank();` | round 0 |
| 69 | `GameMode10_Run` | `AddressOf(GameMode11_Steps), GameMode10_Steps_count);` | `AddressOf(GameMode10_Steps), GameMode10_Steps_count);` | round 0 |
| 70 | `GameMode10_Enter` | `MenuModeEnter(0x31B); }` | `MenuModeEnter(0x31A); }` | round 0 |
| 71 | `GameMode11_Run` | `AddressOf(GameMode10_Steps), GameMode11_Steps_count);` | `AddressOf(GameMode11_Steps), GameMode11_Steps_count);` | round 0 |
| 72 | `GameMode11_Frame` | `if (Field_Request == 0) Game_Mode = 3;` | `if (Field_Request == 0) Game_Mode = 2;` | round 2 |
| 73 | `GameMode11_Look` | `SH_CALL(Look_Return)();` | `SH_CALL(Look_PadControl)();` | round 0 |
| 74 | `GameMode11_LookEnd` | `Word(At(at::kPitch)) == 0x201` | `Word(At(at::kPitch)) == 0x200` | round 1 |
| 75 | `GameMode11_LookEnd` | `== 0xFD57 &&` | `== 0xFD56 &&` | round 1 |
| 76 | `Quake` | `(static_cast<U>(S16(At(at::kQuakeX))) << 2)` | `(static_cast<U>(S16(At(at::kQuakeX))) << 1)` | round 3 |
| 77 | `Quake` | `I(b) >= 0x1D` | `I(b) >= 0x1C` | round 103 |
| 78 | `Quake` | `return (0u - (t(1) + t(0xF))) << 4;` | `return (0u - (t(1) + t(0xF))) << 3;` | round 4 |
| 79 | `Quake` | `return (0u - (t(0x10) + t(2))) << 3;` | `return (0u - (t(0x10) + t(1))) << 3;` | round 8 |
| 80 | `Quake` | `return (0u - t(0x10)) << 3;` | `return (0u - t(0x10)) << 4;` | round 5 |
| 81 | `Quake` | `const U i = (Sar(a, 1) * 14u) + Sar(b, 1);` | `const U i = (Sar(a, 1) * 15u) + Sar(b, 1);` | round 3 |
| 82 | `Quake` | `if (At(at::kQuakeFacing)[0] & 2) {` | `if (At(at::kQuakeFacing)[0] & 1) {` | round 3 |
| 83 | `Quake` | `return a & 0xFFFF0001u;` | `return a & 0xFFFF0000u;` | round 0 |
| 84 | `SwitchPattern` | `if (set != 0) bits \|= 2u << i;` | `if (set != 0) bits \|= 1u << i;` | round 0 |
| 85 | `SwitchPattern` | `return static_cast<int>(bits + 2);` | `return static_cast<int>(bits + 1);` | round 0 |
| 86 | `SwitchHook` | `(static_cast<U>(x) & 0xFF) != 0x1D` | `(static_cast<U>(x) & 0xFF) != 0x1C` | round 2 |
| 87 | `SwitchHook` | `At(at::kLeaderFacing)[0] != 2) return 0;` | `At(at::kLeaderFacing)[0] != 3) return 0;` | round 2 |
| 88 | `SwitchHook` | `int bits = pattern % 7;` | `int bits = pattern % 6;` | round 2 |
| 89 | `SwitchHook` | `SH_CALL(Effect_HoldFlag1C)(0xE);` | `SH_CALL(Effect_HoldFlag1C)(0xF);` | round 2 |
| 90 | `SwitchHook` | `SH_CALL(Sound_PlayEffect)(0x207);` | `SH_CALL(Sound_PlayEffect)(0x206);` | round 2 |
| 91 | `SwitchHook` | `return 2; }` | `return 1; }` | round 2 |
| 92 | `DrawPanels` | `(static_cast<U>(b[0]) << 7) - 0x37C1u` | `(static_cast<U>(b[0]) << 7) - 0x37C0u` | round 0 |
| 93 | `DrawPanels` | `0xFFFFFC08u - static_cast<U>(b[1]) * lift` | `0xFFFFFC08u - static_cast<U>(b[2]) * lift` | round 0 |
| 94 | `DrawPanels` | `(texture << 15) \| 0xBB009120u` | `(texture << 16) \| 0xBB009120u` | round 0 |
| 95 | `DrawPanels` | `from += 6)` | `from += 12)` | round 0 |
| 96 | `DrawPanels` | `SH_CALL(Gfx_CommitPrim)(5, 0x44);` | `SH_CALL(Gfx_CommitPrim)(5, 0x48);` | round 0 |
| 97 | `DrawRings` | `U colour = (0x28u - static_cast<U>(size)) * 4u;` | `U colour = (0x28u - static_cast<U>(size)) * 5u;` | round 0 |
| 98 | `DrawRings` | `if (I(c) > 0xFE) c = 0xFE;` | `if (I(c) > 0xFF) c = 0xFF;` | round 4 |
| 99 | `DrawRings` | `const U big = Sar(r << 12, 11);` | `const U big = Sar(r << 12, 10);` | round 0 |
| 100 | `DrawRings` | `SetWord(v + 2, big - 0x2CC1u); end(p, 8, 0x10);` | `SetWord(v + 2, big - 0x2CC0u); end(p, 8, 0x10);` | round 0 |
| 101 | `DrawRings` | `SetUL(AddressOf(MapView_ScreenXY) + 4, 0xC6340000u);` | `SetUL(AddressOf(MapView_ScreenXY) + 4, 0xC6330000u);` | round 0 |
| 102 | `DrawRings` | `radius_arg -= 0x8;` | `radius_arg -= 0x10;` | round 0 |
| 103 | `DrawRings` | `if (k == 1) angle += 0x20;` | `if (k == 1) angle += 0x40;` | round 0 |
| 104 | `DrawRings` | `const U r = I(radius_arg) < 0 ? 1u : radius_arg;` | `const U r = I(radius_arg) < 0 ? 0u : radius_arg;` | round 2 |
| 105 | `DrawRings` | `p[6] = static_cast<unsigned char>(c8);` | `p[6] = half;` | round 0 |
| 106 | `DrawRings` | `static_cast<long double>(Fl(AddressOf(MapView_ScreenXY))) + 0.5L));` | `static_cast<long double>(Fl(AddressOf(MapView_ScreenXY)))));` | round 0 |
| 107 | `DrawRings` | `SetWord(v + 4, 0xFC09);` | `SetWord(v + 4, 0xFC08);` | round 0 |
| 108 | `FrameAreaBD` | `Sar(0x800000u - static_cast<U>(Field_Kind2X), 7)` | `Sar(0x800000u - static_cast<U>(Field_Kind2X), 8)` | round 0 |
| 109 | `FrameAreaBD` | `(second & 0xFFFFu) != Word(drawn + 6)` | `(second & 0xFFFFu) != Word(drawn + 4)` | round 0 |
| 110 | `FrameAreaBD` | `MapView_Redraw = 2;` | `MapView_Redraw = 3;` | round 2 |
| 111 | `FrameAreaBD` | `static_cast<U>(moved[2]) + 0x1193u` | `static_cast<U>(moved[2]) + 0x1194u` | round 0 |
| 112 | `FrameAreaBD` | `static_cast<U>(MapView_Elevation) >> 2);` | `static_cast<U>(MapView_Elevation) >> 1);` | round 0 |
| 113 | `FrameAreaBD` | `& 0x3FFFu) - 0x4000u); vector[1]` | `& 0x7FFFu) - 0x4000u); vector[1]` | round 1 |
| 114 | `FrameAreaBD` | `MapView_Redraw = static_cast<unsigned char>(MapView_Redraw - 2);` | `MapView_Redraw = static_cast<unsigned char>(MapView_Redraw - 1);` | round 1 |
| 115 | `CellTexture` | `((Sar(t, 24) & 0xFu) + 0x1E4u) << 6);` | `((Sar(t, 24) & 0xFu) + 0x1E3u) << 6);` | round 0 |
| 116 | `CellTexture` | `quad[0x35] = static_cast<unsigned char>(v0 + 0xE);` | `quad[0x35] = static_cast<unsigned char>(v0 + 0xF);` | round 0 |
| 117 | `CellTexture` | `SetWord(quad + 0x26, 0x96);` | `SetWord(quad + 0x26, 0x95);` | round 0 |
| 118 | `CellTexture` | `SH_CALL(Gpu_SetSemiTrans)(quad, 1);` | `SH_CALL(Gpu_SetSemiTrans)(quad, 0);` | round 0 |
| 119 | `CharCount` | `if (c & 0x40) ++text;` | `if (c & 0x80) ++text;` | round 0 |
| 120 | `CharCount` | `if (c == 0) return 1;` | `if (c == 0) return 0;` | round 14 |
| 121 | `BuildView` | `const U even = octant & 1u;` | `const U even = (~octant) & 1u;` | round 0 |
| 122 | `BuildView` | `SetUL(list + 4, AddressOf(list) + 4);` | `SetUL(list + 4, AddressOf(list));` | round 0 |
| 123 | `BuildView` | `static_cast<U>(S8(rec[0])) - fx + 0x101u;` | `static_cast<U>(S8(rec[0])) - fx + 0x100u;` | round 0 |
| 124 | `BuildView` | `if (DrawItemPool_Top > Word(At(at::kTopBoundAt))) continue;` | `if (DrawItemPool_Top >= Word(At(at::kTopBoundAt))) continue;` | round 2 |
| 125 | `BuildView` | `const U a = static_cast<U>(S8(rec[1])) * row;` | `const U a = static_cast<U>(S8(rec[2])) * row;` | round 1 |
| 126 | `BuildView` | `const U z = static_cast<U>(I(b) >> 1)` | `const U z = static_cast<U>(I(b) / 2)` | round 5 |
| 127 | `BuildView` | `const U map_row = (block & 0xF0u) \| (x & 0xFu);` | `const U map_row = (block & 0xF0u) \| (z & 0xFu);` | round 0 |
| 128 | `BuildView` | `bool keep = sy >= Bound(at::kWideYAt)` | `bool keep = sy > Bound(at::kWideYAt)` | round 14 |
| 129 | `BuildView` | `keep = y_ok && sx >= Bound(at::kNarrowLoAt)` | `keep = y_ok && sx > Bound(at::kNarrowLoAt)` | round 14 |
| 130 | `BuildView` | `const bool y_ok = sy < Bound(at::kNarrowYAt);` | `const bool y_ok = !(sy >= Bound(at::kNarrowYAt));` | round 23 |
| 131 | `BuildView` | `sx > Bound(at::kNarrowLoAt)` | `sx > Bound(at::kWideLoAt)` | round 4 |
| 132 | `BuildView` | `FpuCopy(item + Gfx_BufferIndex * 0x48u + 0xC, MapView_ScreenXY);` | `FpuCopy(item + Gfx_BufferIndex * 0x48u + 0xC, MapView_ScreenXY + 1);` | round 0 |
| 133 | `BuildView` | `std::memcpy(item + Gfx_BufferIndex * 0x48u + 8, MapView_ScreenXY, 4);` | `FpuCopy(item + Gfx_BufferIndex * 0x48u + 8, MapView_ScreenXY);` | **not refused** |
| 134 | `BuildView` | `const U last = at::kLayerLast + (Gfx_BufferIndex - row * 6u + 2u) *...` | `const U last = at::kLayerLast + (Gfx_BufferIndex - row * 6u) * 8u;` | round 0 |
| 135 | `BuildView` | `const U x1 = xy + 0x81u;` | `const U x1 = xy + 0x80u;` | round 0 |
| 136 | `BuildView` | `static_cast<unsigned char>(UL(corner) >> 8)` | `static_cast<unsigned char>(UL(corner) >> 16)` | round 3 |
| 137 | `BuildView` | `DrawItemPool_Top = static_cast<unsigned short>(top + 2);` | `DrawItemPool_Top = static_cast<unsigned short>(top + 1);` | round 2 |
| 138 | `BuildView` | `static_cast<U>(I(product) / 2) + offset + tile + 1u;` | `static_cast<U>(I(product) / 2) + offset + tile;` | round 0 |
| 139 | `BuildView` | `std::memcpy(item + Gfx_BufferIndex * 0x48u + 0xC, MapView_ScreenXY ...` | `FpuCopy(item + Gfx_BufferIndex * 0x48u + 0xC, MapView_ScreenXY + 1);` | round 23 |

## 7. Latent defects (Capcom's, described, not fixed)

- **Unchecked step dispatchers**: `GameMode8_Run` .. `GameMode11_Run` index
  their tables by the word `Game_Step`, unbounded; every writer of this group
  keeps it inside (steps go up by one or back to 0); the steps that are other
  groups' (`GameMode8_Frame`, `_TradeStep`, `Mode8_Step5`, `Mode8_Step8`) were
  not read here. Ours aborts past each table.
- **The encounter row unchecked**: `Battle_PlaceBossActors` takes the row
  from `EventBattle_Records[0x904AAA] + 2` and reads `Encounter_Rows + 9 *
  row` for eight bytes; the first 24 records (the battle side's table has
  24) name rows 0..7, but nothing bounds the event or the row. Ours aborts on a
  row past the eight. `Battle_PlaceBossActor` indexes the enemy records by
  the count, at most 7 from its one caller; ours aborts past eight.
- **Stack bytes passed above a byte**: `Battle_PlaceBossActors` passes the
  slot and count with three upper bytes it never wrote; the callee reads the
  bytes only (section 1.3). Nothing to level.
- **The winding's ax**: `Screen_TriangleWinding` answers the truncated cross
  product's 32 bits and its callers test ax: a cross product of 0x8000 or
  more in size reads with the wrong sign, or as 0 at multiples of 0x10000 -
  a large forward-facing triangle can be taken as backward. As read; ours
  answers the same eax.
- **`EffectKind0F_CharCount` skips a byte unchecked**: after a byte with bit
  7 set it skips the next without testing it for the NUL, so a string ending
  on a lone lead byte is counted on past its terminator. As read.
- **`AreaMapBD_BuildView`'s item bump**: it hands out `DrawItems` by
  `DrawItemPool_Top` from 0 each build, beside the pool's own allocator that
  `MapView_Build` and the effect sub-kinds use; in area 0xBD both would hand
  out the same items if anything there allocates from the pool. Described,
  not shown to happen.
- **The cull's y overlap**: a point with 120 < y < 121 is tested against the
  wide x range first and the narrow one after. As designed or not, the
  result is the union of the two tests; ours computes the same.
- **`EffectKindAD_Rise` waits on the counter byte `0x903848` being exactly
  0xB** with no other way on.

**Needs a ledger entry: none.** No function reads memory it never wrote into
what is drawn or decided (the stack bytes above are masked by their only
reader).

## 8. Calls across groups

**Outbound, raw** (`rest_3g_callees.h`): `0x492400`, R3F's (wave three),
from `EffectKindAC_FadeIn` and `_FadeOut`: one byte argument; raw until the
round's rebinding. Everything else by name, ours already: E4F's
`EffectKindAD_DrawArc`, `EffectKindBA_DrawLine`; EGT's
`EffectGte_LoadMapCamera`, `_ProjectPoint`; R1G's `Fish_Spawn` (merged);
E1F's `GameMode8_Frame`, `_WaitFrame`; FE2's `Mode11_ObjectFrame`, FC3's
`Mode11_FieldFrame`, `Field_Frame`; `Look_PadControl`, `Look_Return`;
BH's `BossActor_Find`; `Battle_CopyEnemyData`, `Battle_SetEnemyOffset`;
SX2's `Effect_HoldFlag1C`; the GTE, GPU, sound, file, task and flag
functions.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Owner | Calls |
|---|---|---|
| `EffectKindAC_Run` / `_AD_Run` / `_AE_Run` / `_BA_Run` through their tables | E4F | the seven effect states, read in place |
| `EffectKind44_RingCone`, `EyeBeam_DrawCylinder`, `Shisu_DrawModel` | E2C, S32, R2B | `Screen_TriangleWinding` (raw `kWinding` / `kFacing` constants: rebound, section 10) |
| `Battle_InitBossEncounter` (`g.boss_common`) | battle_sprites | `Battle_PlaceBossActors` |
| `BattleEnd_Finish`, `BattleLoss_ResetParty`, `BattleLoss_Restart`, `Escape_Leave` | BTS, BE1, BE4 | `BattleEnemy_ClearStates` |
| `Field_Task` through `GameMode_Handlers[8..11]` | ours | the four dispatchers, read in place |
| `MapCell_DrawTexQuads`, `_DrawShadedQuads`, `_DrawSpinQuads` | BE6 | `Quake_VertexLift` |
| `EffectKind18_09_Pattern` | E5A | `Area109_SwitchPattern` |
| `Area_CellHook` `0x56E670` through `Area_CellHooks` | ours | `Area109_SwitchHook`, read in place |
| `EffectKind18Sub41_Hold` | E6B | `EffectKind18Sub41_DrawPanels`, `_DrawRings` |
| `AreaMap_Frame` (`g.frame_bd`) | map_layers | `AreaMap_FrameAreaBD` |
| `EffectKind0F_Title`, `_TitleClose`, `_LineType` | E1A | `EffectKind0F_CharCount` |

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for every row but the twelve mode rows, whose world-map
column holds "-18": the host `GameMode_LookEnd` was entered, an upper bound,
not these. No trace under `analysis/calltrace` names any of the 32 (a grep of
every file: the hits are the extent lists `entries*.txt`). **Fuzz only.** No
live run was made (the brief). A route that enters the fishing spot's mode
(8), the two menu modes (9, 10), mode 11's look, a boss encounter, Quake,
area 109's switch or area 0xBD would let the coordinator's state hash cover
these.

## 10. The rebinding

`grep -rn -i` of the 32 addresses in `src` (`band_rows.py --refs`: 99
references to 21 of them). **Rebound**, the value unchanged so every fuzz key
stands, each on the line it changes:

| File | Was | Now |
|---|---|---|
| `battle_e1_callees.h`, `battle_e4_callees.h` | `kEnemiesClear = 0x494E70` | `bof3::addr::BattleEnemy_ClearStates` |
| `battle_turn_steps_callees.h` | `kClearEnemies = 0x494E70` | the same |
| `battle_e6_callees.h` | `kCellHeight = 0x4CF4B0` | `bof3::addr::Quake_VertexLift` |
| `battle_sprites.cpp` | `Raw<void (__cdecl*)()>(0x494500)` in `kOriginals` | `Battle_PlaceBossActors` |
| `effect_1a_callees.h` | `kStringCount = 0x5171E0` | `bof3::addr::EffectKind0F_CharCount` |
| `effect_2c_callees.h`, `rest_2b_callees.h` | `kWinding = 0x4941B0` | `bof3::addr::Screen_TriangleWinding` (`rest_2b_callees.h` now includes `symbols.gen.h`) |
| `magic_s32.cpp` | `kFacing = 0x4941B0` | the same |
| `effect_5a_callees.h` | `kPattern = 0x4FEE70` | `bof3::addr::Area109_SwitchPattern` (now includes `symbols.gen.h`) |
| `effect_6b_callees.h` | `kWaveMark = 0x5100B0`, `kWaveStep = 0x5101C0` | `EffectKind18Sub41_DrawPanels`, `_DrawRings` |

Comments naming the functions as Capcom's or nobody's were brought up to
date on their own lines (`battle_e1_callees.h`, `battle_e4_callees.h`,
`battle_e6_callees.h`, `battle_e6.cpp`, `battle_sprites_callees.h`,
`battle_turn_steps_callees.h`, `effect_6b.cpp`, `mode_states.cpp`,
`field_e2.cpp`, `scena_sx2.cpp`, `widescreen.cpp`, `widescreen.h` (two
lines), `draw_pool.cpp`).

**Left raw, on purpose**: the fuzz files' `CallSite` tables and stand-in rows
keyed by the address (`battle_e1_fuzz`, `battle_e4_fuzz`, `battle_e6_fuzz`,
`battle_sprites_fuzz`, `battle_turn_steps_fuzz`, `effect_1a_fuzz`,
`effect_2c_fuzz`, `effect_5a_fuzz`, `effect_6b_fuzz`, `magic_s32_fuzz`,
`map_layers_fuzz`, `rest_2b_fuzz`: the copies' call targets, the round-ten
rule); the harnesses' rows (`scenario_harness.cpp`'s five `FX_RAW`,
`boss_harness.cpp`'s two - not this group's to edit, and keyed by address so
they still serve, section 5); comments that only cite an address beside what
it does (`effect_4f.cpp`'s "part 6" notes, `effect_2c.cpp`, `effect_5a.cpp`,
`effect_6b.cpp` 801-802, `battle_e1.cpp`, `battle_e4.cpp`,
`battle_turn_steps.cpp`, `rest_2b.cpp`, `magic_s32.cpp` 636, `widescreen.cpp`
62). None is in a module of this round's groups (`rest_*`) but `rest_2b_callees.h` (wave two's, merged).

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-04): 20 lines, the read extents
of the hidden starts (`004925C0 12` .. `004FEEB0 A0`). The twelve visible
functions already had lines (`004941B0 2B`, `00494500 6F`, `00494570 145`,
`00494E70 23`, `005100B0 101`, `005101C0 467`, `00510630 141`, `00510780
424`, `005171E0 1C` exact; `004CF4B0 140`, `00510BB0 70` padding past the
code's 0x138 / 0x68), not duplicated. **Left for the coordinator**:
`004FEE70 E0`, the catalog's extent of `Area109_SwitchPattern` over
`Area109_SwitchHook` (the code is 0x35).
