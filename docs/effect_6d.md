# Group E6D: kind 0x18's sub-kinds 0x5C, 0x5D, 0x5E, 0x61, 0x62, 0x63, 0x64 and 0x65

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..13),
wave six, from the round branch's tip `c4deb22`. **51 functions ours**
(`src/game/effect_6d.cpp`, shadow name `effect_6d`): the cut table's 50 rows
for E6D (`analysis/round13_cut.tsv`, the band `0x514270..0x516A90`) and one
start the band holds that no list has, `0x5166C0` (sub-kind 0x64's state 5,
section 5); none dropped. Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
204,000 rounds, 0 mismatches; 164 of 166 controls refused, the other two equivalent mutants whose near variants are refused. **Fuzz only**: no recorded route
enters any of the 51 (section 9). Every row is effect code: the two
`hypothesis` rows, `0x5144F0` and `0x516A90`, are draws called only by their
sub-kinds' states, taken. **No full-frame fill of DIV-0041's list is in this
band**; two screen-space overlays drawn from fixed coordinates are (section 2),
for the coordinator.

All eight are sub-kinds of effect kind 0x18: `EffectKind18_Run` (`0x46D830`,
ours) jumps through `EffectKind18_States` by `+1`, which `EffectKind18_Start`
copies from `+0xB`. Five of them dispatch again through a table of their own -
four by `+2`, sub-kind 0x61 by `Cond_ByteFE` - and two are one state.

| Sub-kind | Functions | Reached through |
|---|--:|---|
| 0x5C: two panels of three textured quads each (a vertex table), at a cell a variant names, with a story flag (`Cond_Flags + 0xA0`) set while they are shut and cleared as they open; they slide apart (sound 0x200) when the leader stands at the cell or `Cond_ByteFE` is 1, wait, and slide back (sound 0x201) once the leader is away or `Cond_ByteFE` is 1 again. Not drawn while waiting shut or waiting open | 7 | `EffectKind18_States[0x5C]` (`0x6541DC`), `EffectKind18Sub5C_States` `0x65F374` (5) |
| 0x5E: two panels of one quad each, the ground under every corner, sliding along x when the leader stands at the cell | 7 | `EffectKind18_States[0x5E]` (`0x6541E4`), `EffectKind18Sub5E_States` `0x65F40C` (5) |
| 0x5D: two panels at a variant's cell lying across x or z (the spawn's z cell 0 chooses), a height of their own, linked into the map's depth order | 7 | `EffectKind18_States[0x5D]` (`0x6541E0`), `EffectKind18Sub5D_States` `0x65F424` (5) |
| 0x61: a step by `Cond_ByteFE` - a screen overlay of textured pieces slid or raised, a glow (a disc and four rings) - then every frame the two followers' tracks on the map and a scrolling two-layer mist while `+2` counts | 14 | `EffectKind18_States[0x61]` (`0x6541F0`), `EffectKind18Sub61_Steps` `0x65F450` (7) |
| 0x62: the map's corner heights rippled over a 35 x 35 block about `Field_Kind2`'s cell | 1 | `EffectKind18_States[0x62]` (`0x6541F4`) |
| 0x63: six projected Gouraud quads turning with `Cond_AngleFB`; sets `Cond_ByteFE` 1 | 1 | `EffectKind18_States[0x63]` (`0x6541F8`) |
| 0x64: a crossed pair of textured quads at a variant's cell, animated through six frames, following the state bytes of an enemy record (`+0xC`) once the game is in battle mode 5 | 9 | `EffectKind18_States[0x64]` (`0x6541FC`), `EffectKind18Sub64_States` `0x65F55C` (7) |
| 0x65: CLUT rows 4 and 5 faded in from black once the counter `0x903848` reaches 0xD, then released | 5 | `EffectKind18_States[0x65]` (`0x654200`), `EffectKind18Sub65_States` `0x65F57C` (3) |

What each looks like in the game is not stated here (the owner's to say);
the descriptions are what the code draws and tests. "Panels", "glow", "mist",
"tracks" are words for what the code builds, not established game objects.
**No spawner is found**: a grep of our sources for these sub-kind numbers
stored into `+0xB` finds none (kind 0x18 records come from the areas' effect
lists, data not code).

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`, the five dispatchers and the run `evidence`); no PSX twin gives
a name (`analysis/pairs_propagated.json` has no pair in the band).

## 1. What each function does

### 1.1 Sub-kind 0x5C (`EffectKind18Sub5C_Run` `0x514270`)

| PC | Name | What |
|---|---|---|
| `0x514270` | `EffectKind18Sub5C_Run` | `jmp [EffectKind18Sub5C_States + +2 * 4]`, unbounded (hidden in E6C's `0x5140C0`) |
| `0x514290` | `_Place` (0) | the variant `v` = s16 `+0x36` (room two): `+0xB` = v, the cell bytes of `0x65F388` into `+0x36` / `+0x3A`, `+8` the variant's flag byte (`0x65F38C`), `Flags_Set(Cond_Flags + 0xA0, +8)`; `+0x30` 0, `+2` up; the pair |
| `0x514310` | `_WaitNear` (1) | `Cond_ByteFE` 1 is taken (set 0) and forces the opening; else the leader within 0x8000 of the cell's centre along z and within 0x20000 of the cell past it along x. Opening: `Flags_Clear(.., +8)`, sound 0x200 unless `Field_Request`, `+2` up (read after the calls), the pair. **Otherwise nothing is drawn** |
| `0x5143C0` | `_Open` (2) | `+0x30` up 0x10, at 0xC0 `+2` up; the pair with side 0 at dy -1 (-2 from 0x80) |
| `0x514410` | `_WaitFar` (3) | `Cond_ByteFE` 1 is taken (0) and `+2` up; then the leader more than 0x40000 off the cell's edge along z or 0x30000 off the cell past it along x: `+2` up again. Nothing drawn |
| `0x514470` | `_Close` (4) | `+0x30` down 0x10; at 0 or below `Flags_Set(.., +8)`, sound 0x201 unless `Field_Request`, `+2` = 1; the pair as `_Open` |
| `0x5144F0` | `EffectKind18Sub5C_DrawPanel(unsigned side, int dy)` | x `(+0x36 << 7) - +0x30 * s8 0x65F390[side] + side * 0xC0 - 0x4040` and y `(+0x3A << 7) - 0x4040` stored as floats into `MapView_ScreenXY` (the original uses the global as scratch; ours writes it too); three POLY_FT4s, each after `Gpu_SetDrawMode` (page 0x95) and `MapView_LinkPrimAt(+0x34, +0x38, dy, 0xC)`: four corners from the vertex table `0x65F394` (s16 x, y plus the two floats, each through the CRT's `_ftol` `0x5B9550`, done in place; z the table's), `Gte_RotTransPers4`, `Gte_PrimDepths4_10`, `Prim_SetTexture(0x65F3DC[3 side + 6 +0xB + i], p, 1)`, linked with dy (0x48). A `hypothesis` row of the cut: called only by 0x5C's states |

"The pair" is `DrawPanel(0, dy0)` then `DrawPanel(1, 1)`, `dy0` 1 in the
first two states.

### 1.2 Sub-kind 0x5E (`EffectKind18Sub5E_Run` `0x514690`)

| PC | Name | What |
|---|---|---|
| `0x514690` | `EffectKind18Sub5E_Run` | `jmp [EffectKind18Sub5E_States + +2 * 4]`, unbounded (hidden in `0x5144F0`) |
| `0x5146B0` | `_Start` (0) | `+0x3E` 0xFF80, `+0x30` 0, `+2` up; the leader within 0x8000 of the cell's centre along z and on the cell or the next along x: `+0x30` 0xC0, `+2` = 3. The draw |
| `0x514740` | `_WaitNear` (1) | the same test: sound 0x200 unless `Field_Request`, `+2` up (`Sprite_Current` read after the sound). The draw |
| `0x5147C0` | `_Open` (2) | `+0x30` up 0x10, at 0xC0 `+2` up; a tail jump to the draw |
| `0x5147E0` | `_WaitFar` (3) | the test at 0x20000 both ways fails: `+2` up. The draw |
| `0x514840` | `_Close` (4) | `+0x30` down 0x10; at 0 or below sound 0x201 unless `Field_Request`, `+2` = 1; a tail jump to the draw |
| `0x514880` | `EffectKind18Sub5E_Draw` | the corners' y `(+0x3A << 7) - 0x3FC0`; `Gpu_SetDrawMode` (page 0x95), `Gfx_CommitPrim(6, 0xC)`; two POLY_FT4s, x `(+0x36 << 7) - 0x3F40` / `- 0x4040` plus `+0x30 * s8 0x65F420[i]`, each corner's height from `AreaMap_Elevation` at its own point (four calls; the top two `-ground / 2`, the bottom two 0x180 below), `Gte_RotTransPers4`, `Gte_PrimDepths4_10`, `Prim_SetTexture((0x11D - i) \| 0x1790000, p, 1)`, `Gfx_CommitPrim(6, 0x48)` |

### 1.3 Sub-kind 0x5D (`EffectKind18Sub5D_Run` `0x514A70`)

| PC | Name | What |
|---|---|---|
| `0x514A70` | `EffectKind18Sub5D_Run` | `jmp [EffectKind18Sub5D_States + +2 * 4]`, unbounded (hidden in `0x514880`) |
| `0x514A90` | `_Place` (0) | `+8` = whether s16 `+0x3A` is 0; the variant (room two): the cell (`0x65F448`), `+0x3E` (`0x65F438`), `+0x20` (`0x65F440`), `+0x2E` (`0x65F43C`), `+0x30` 0, `+2` up; the leader at the cell (z first with `+8`): `+0x30` 0x80, `+2` = 3. The draw |
| `0x514BE0` | `_WaitNear` (1) | the test: sound 0x200 unless `Field_Request`, `+2` up. The draw |
| `0x514CC0` | `_Open` (2) | `+0x30` up 0x10, at 0x80 `+2` up; a tail jump to the draw |
| `0x514CE0` | `_WaitFar` (3) | the test at 0x20000 both ways fails: `+2` up. The draw |
| `0x514DB0` | `_Close` (4) | `+0x30` down 0x10; at 0 or below sound 0x201 unless `Field_Request`, `+2` = 1; a tail jump to the draw |
| `0x514DF0` | `EffectKind18Sub5D_Draw` | E5G's `EffectKind18Sub2C_Draw` without the ground: with `+8` the corners' y `(+0x3A << 7) - 0x3FC0` and the panels slide along x, else x and along z; the top corners `+0x3E - +0x2E`, the bottom `+0x3E`; two sides: `Gpu_SetDrawMode` (page 0x95), `MapView_LinkPrimAt(+0x34, +0x38, -1, 0xC)`, a POLY_FT4 slid by `+0x30 * s8 0x65F44C[side]`, projected, `Prim_SetTexture((+0x20 + side) \| (+8 << 21), p, 1)`, linked (0x48) |

### 1.4 Sub-kind 0x61 (`EffectKind18Sub61_Run` `0x515000`)

`EffectKind18Sub61_Run` (hidden in `0x514DF0`) **calls** (not jumps) through
`EffectKind18Sub61_Steps` by `Cond_ByteFE` - a global byte, not the record -
unbounded; then calls the tracks and tail-jumps to the mist, every frame.

| PC | Name | What |
|---|---|---|
| `0x515020` | `_WaitFocus` (step 0) | `MapView_FocusX` 0x16FF: `+2` = 4 (the mist's counter starts) |
| `0x515040` | `_Slide` (1) | `+0x34` down 1 when `Frame_Counter & 3` is 0; the overlay at y `+0x34` |
| `0x515070` | `_Hold` (2) | the overlay at y 0 |
| `0x515080` | `_Rise` (3) | by the dword `+0x38` (signed): above 0x100 the glow; above 0x80 the glow and `+0x38` up 2; above 0x60 up 2; else up 1; then the overlay at y `+0x38` |
| `0x515100` | `_Reset` (4) | `+0x38` 0, `+0x34` 0x54, `+0x3C` 0; the ring's first entry set (two marks); `Cond_ByteFE` 0 |
| `0x515150` | `_Rearm` (5) | `_ClearTracks`, then `Cond_ByteFE` 1 |
| `0x515160` | `_ClearTracks` (6, and 5's call) | the heads word `0x6BC704` 0; every mark's cell x 0xFF in the ring `0x6BC644`; two parity bytes set; `Cond_ByteFE` 0 |
| `0x5151A0` | `_DrawOverlay(int y)` | nothing unless `Draw_PassFlags` bit 2. A texture window of 8 bytes taken from the packet cursor (16, 0, 16 x 16) and `Gpu_SetDrawMode` (page 0x99, the window) committed at slot 7 (0xC); two POLY_FT4s `(0, 0)..(256, 256)` and `(256, 0)..(320, 256)` as screen floats (CLUT 0x79C0, page 0x99, (0x48)); a 256 x 256 window; the eight 14-byte pieces of `0x65F470` at `(x, y + y)..(x + w, y + h + y)`, their `(u, v)..(u + du, v + dv)`, page and CLUT from their page byte, (0x48) |
| `0x515440` | `_DrawTracks` | for `Sprite_Objects[1]` and `[2]` (`+0x34` at `0x7DEF58`, stride 0xA4): when the head mark's cell x is past the follower's `(x + 0x4000) >> 16`, a new mark at the next of 32 entries (x cell, the z cell byte `+0x3A`, the parity flipped); then each of the 32 marks: `MapView_ItemHalfAt` at its cell and one z step on (s8 `0x65F4E4[r]`), each a POLY_FT4 copying the half's four corners, `Prim_SetTexture(0x65F4E0[2r or 2r + 1] + parity + 0x25808000, p, 1)`, `MapView_LinkPrimAt(cell << 16, .., 0, 0x48)` |
| `0x5156B0` | `_DrawGlow` | `Gpu_SetDrawMode` (page 0xB5, additive) (4, 0xC); `+9` up to 0xFF; `t` = `+9 - 0x14` past 0x14: the disc at (0x82, `+9` + 10), radius `min(20t + 0x20, 500)`, shade `min(12t + 0x80, 0xFF)`; four rings about (0x82 + `Math_Sin(0x100) * (0x1E - +9) * m / 4096`, `+9 + 10 + Math_Cos(0x100) * ..`) for m 9, 8, 6, 4, widths 0x40, 0x20, 8, 0x10, shade `min(12 * +9, 0x30)`, `+9` read again for each |
| `0x515950` | `_DrawRing(x, y, radius, shade)` | 64 steps of 0x40: two semi-transparent POLY_G4s each, radius + 2 (black) in to radius (grey `shade`), and radius in to radius - 8 (black); corners `Math_Sin` / `Math_Cos` * reach / 4096 about (x, y) as floats, (4, 0x44) |
| `0x515CC0` | `_DrawDisc(x, y, radius, shade)` | 64 semi-transparent POLY_G3s from (x, y) (grey `shade`) to the rim at radius (black), (4, 0x34) |
| `0x515E00` | `_DrawMist` | nothing while `+2` is 0. Else two layers: a 64 x 64 window and draw mode (page 0x97, dtd) (4, 0xC), two semi-transparent POLY_FT4s 256 x 256 side by side from `(-o, o - 0x20)` (CLUT 0x78C0, page 0x7B, (4, 0x48)), a 256 x 256 window (4, 0xC). Layer one: offset `+2 & 0x3F`, shade `(0x80 - |0x80 - +2|) / 2`; layer two: `(+2 & 0x1F) * 2`, shade `0x80 - |0x80 - +2|`, or `(Math_Cos((+2 - 0x40) << 5) / 128 + 0x60) / 2` within 0x40 of 0x80. Then `+2` up 4 (so it runs 64 frames from 4 and stops at 0) |

`_DrawMist` is reached only by the run's tail jump; it has its own frame and
`ret`, so it is a function of its own (the addendum's rule), not a tail of the
run. Its catalog host `0x515CC0` (extent `AFA`) is `_DrawDisc`.

### 1.5 Sub-kinds 0x62 and 0x63 (one state each)

| PC | Name | What |
|---|---|---|
| `0x516090` | `EffectKind18Sub62_Ripple` | `Camera_Distance` 0x5DC; for 35 columns from `Field_Kind2X`'s cell - 0x14 and 35 rows from `Field_Kind2Z`'s cell - 0x14, inside `AreaMap_Header`'s width and height (re-read each test): `AreaMap_Corners`' bytes 0 and 2 up by `(Math_Sin(((Frame_Counter + 3j) << 7) & 0xFFF) >> 10) - (Math_Sin(that - 0x80) >> 10)`, bytes 1 and 3 by the same at + 3 less + 2. Then `Sprite_ObjectsExtra[0]` `+0x3E` = `AreaMap_Elevation` at its point, `+0x14` 0, `MapView_Redraw` 2 |
| `0x516280` | `EffectKind18Sub63_Glow` | `Cond_ByteFE` 1; `Gpu_SetDrawMode` (page 0xB5) (4, 0xC); six semi-transparent POLY_G4s from the points k and k + 1 of `0x65F4E8` (k the bytes of `0x65F554`) to the same points turned by `Cond_AngleFB + 0x200` (`Math_Sin` / `Math_Cos` times the reach of `0x65F530` / 4096, z the reach table's), `Gte_RotTransPers4`, `Gte_PrimDepths4_10B`, the near corners the points' shades, (4, 0x44); `Gpu_SetDrawMode` (page 0x95) (4, 0xC) |

### 1.6 Sub-kind 0x64 (`EffectKind18Sub64_Run` `0x516480`)

| PC | Name | What |
|---|---|---|
| `0x516480` | `EffectKind18Sub64_Run` | `jmp [EffectKind18Sub64_States + +2 * 4]`, unbounded (hidden in `0x515CC0`) |
| `0x5164A0` | `_Place` (0) | the variant (room two): its cell bytes `<< 16` into `+0x34` / `+0x38`, `+0xC` = `0x93BA88 + 0x128 v` (the enemy record v + 1 of `0x93B960`), `+9` and `+0xA` 0, `+2` up; the draw (0x80, 0x40, 1, 0) |
| `0x516520` | `_WaitBattle` (1) | `Game_Mode` and `Game_Step` both 5: `+2` up; the draw |
| `0x516560` | `_Watch` (2) | `Game_Mode` 5 and `Game_Step` not: `+2` down. The watched record's bytes `+1` / `+2`, read again for each test: (8, 2) `+2` = 4; (6, 1), (6, 2) 6; (6, 3) 5; (6, 4) 3; (3, 1): the draw scaled `(dword 0x904AC8 * 6 + 0x3F) & 0xF8`, else the plain draw |
| `0x516640` | `_Fade` (3) | `+9` up 2; the draw (2a, a, 1, 0) with `a = 0x40 - +9`; past 0x3F a tail jump to `Effect_Release` |
| `0x516690` | `_Hold` (4) | unless the record is (8, 2), `+2` = 2; the draw (0xE0, 0x70, 2, 0) |
| `0x5166C0` | `_Pulse` (5) | `+9` up; `e = 8 - |8 - +9|`; past 0xF `+9` 0 and `+2` = 2; the draw lifted by `(-e) << 4`. **No list had it** (section 5) |
| `0x516730` | `_Swell` (6) | the record (6, 3): `+9` 0, `+2` = 5 and a tail jump to `_Pulse`; else `+9` up 8, past 0x7F `+9` 0 and `+2` = 2, the draw (`|0x40 - +9| + 0x40`, `|0x20 - +9 / 2| + 0x20`, 1, 0) |
| `0x5167C0` | `EffectKind18Sub64_Draw(scale, width, step, lift)` | `+0xA` up by the step byte, less 12 past 11; frames `f0 = +0xA / 2` and `f1 = f0 + 2` (`f0 - 4` past 5), each `+ ((scale << 16) \| 0xBF009111)` a texture word; two crossed POLY_FT4s at `((+0x36 << 7) - 0x3FF0, (+0x3A << 7) - 0x3FF0)`, half-width `width`, the far top edge `lift` higher, top `-(2 width + 0xC0)` and bottom `-0xC0` less half the ground at `+0x34` / `+0x38` (two `AreaMap_Elevation` calls a quad); each after `Gpu_SetDrawMode` (page 0x95) and `MapView_LinkPrimAt(.., 1, 0xC)`, projected, linked (0x48) |

### 1.7 Sub-kind 0x65 (`EffectKind18Sub65_Run` `0x5169F0`)

| PC | Name | What |
|---|---|---|
| `0x5169F0` | `EffectKind18Sub65_Run` | `jmp [EffectKind18Sub65_States + +2 * 4]`, unbounded (hidden in `0x5167C0`) |
| `0x516A10` | `_Start` (0) | the rows at level 0 (black), `+2` up |
| `0x516A30` | `_Wait` (1) | the counter byte `0x903848` 0xD: `+2` up |
| `0x516A50` | `_FadeIn` (2) | `+9` up; the rows at level `+9`; from 0x80 a tail jump to `Effect_Release` |
| `0x516A90` | `EffectKind18Sub65_ShadeCluts(int level)` | each of the 0x200 words of CLUT rows 4 and 5 as loaded (`0x80BD80`, `Gfx_ClutStripSource + 0x800`) with its three 5-bit channels times level / 128 (signed, toward zero) packed back unclamped; a colour that was not 0 and comes out 0 becomes 0x8000; written to the live strip (`0x80FD80`, `Gfx_ClutStrip + 0x800`); `Gfx_ClutStripDirty` 1. A `hypothesis` row of the cut: called only by 0x65's states. E5E's `EffectKind18Sub3F_ShadeClut` is the one-row cousin (it keeps bit 15 instead) |

## 2. Divergence

None: each function is a faithful replacement. `widescreen.cpp`, `cheats.cpp`,
`DIVERGENCE.md` and `scenario_harness.cpp`'s override rows name no address of
the band (a grep of every one). **No DIV-0041 fill is here** (no TILE or quad
`(0, 0)` 320 x 240), but two screen-space overlays drawn at fixed coordinates
are, for the coordinator's widescreen survey: `_DrawOverlay`'s two background
quads `(0, 0)..(320, 256)` (the floats at `0x515207` / `0x515269`), and
`_DrawMist`'s layers `(-o, ..)..(512 - o, ..)` - neither is widened; both
compare the original.

## 3. The tables

**The state tables** (`symbols.toml` `[[data]]`): each its own length to the
data that follows it, checked by hand (`band_rows.py` reads the same counts;
no table runs on into another here):

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind18Sub5C_States` `0x65F374` | 5 | `0x65F388`, 0x5C's variant cells (data) |
| `EffectKind18Sub5E_States` `0x65F40C` | 5 | `0x65F420`, 0x5E's two slide signs (data) |
| `EffectKind18Sub5D_States` `0x65F424` | 5 | `0x65F438`, 0x5D's variant tables (data) |
| `EffectKind18Sub61_Steps` `0x65F450` | 7 | `0x65F46C`, a zero dword, then the overlay's pieces; indexed by `Cond_ByteFE` |
| `EffectKind18Sub64_States` `0x65F55C` | 7 | `0x65F578`, 0x64's variant cells (data); entry 5 is `0x5166C0` |
| `EffectKind18Sub65_States` `0x65F57C` | 3 | `0x65F588`, `Text_DrawAt`'s colour pairs (data) |

**The data read in place** (raw in `effect_6d_callees.h`, not named in
`symbols.toml`; their bytes are not copied here): 0x5C's cells `0x65F388`
(room two), flag bytes `0x65F38C`, slide signs `0x65F390`, vertex table
`0x65F394` (three quads of four), textures `0x65F3DC` (two variants of six);
0x5E's signs `0x65F420`; 0x5D's `0x65F438` / `0x65F43C` / `0x65F440` /
`0x65F448` (room two each) and signs `0x65F44C`; 0x61's pieces `0x65F470`
(eight of 14 bytes), track texture bytes `0x65F4E0` and steps `0x65F4E4`; 0x63's
points `0x65F4E8` (nine of 8), reaches `0x65F530` (nine of 4), order `0x65F554`
(six); 0x64's cells `0x65F578` (room two). Ours aborts on an index past a
table's room (section 7).

## 4. The fuzz (`effect_6d_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_6d`, effect mode (`g.effect`; kind 0x18
for every clone), 4,000 rounds a function (`BOF3X_E6D_ONLY=<name>` runs the
clones whose name holds it). Shapes: 45 `kEffect` (the five dispatchers'
`sub_span` their table's length; the run, the steps and the draws that take
no argument), six `kCall` (`_DrawPanel`, `_DrawOverlay`, `_DrawRing`,
`_DrawDisc`, `EffectKind18Sub64_Draw`, `_ShadeCluts`). No function answers. The
six tables are `DataTable`s, swapped for recorders on both sides. **Regions**
beyond effect mode's: the tracks' ring and heads `0x6BC644` (0xC4), the battle
dword `0x904AC8`, the two enemy records from `0x93BA88` (0x250), CLUT rows 4
and 5 as loaded `0x80BD80` and live `0x80FD80` (0x400 each) - 50 regions,
27,596 bytes of state.

**Callees**: the standard and effect-standard rows for every one of ours -
`Flags_Set` / `Flags_Clear` (the row reads the index's byte: `_Close` pushes
`eax` with `Sprite_Current`'s upper bytes above it), `AreaMap_Elevation`,
`MapView_ItemHalfAt` (0 or the harness's half), `MapView_LinkPrimAt` (the
cursor moved two times in three), `Math_Sin` (garbage), `Math_Cos` (never 0 or
-1), `Gte_RotTransPers4`, `Gte_PrimDepths4_10` / `_10B`, `Prim_SetTexture`,
`Gfx_CommitPrim`, the `Gpu_*`, `Sound_PlayEffect`, `Effect_Release` (the
louder one); the CRT's `_ftol` is `kThrough` (called for real by the copies;
ours does it in place). None re-listed: no caller here needs another reading
(`Gpu_SetDrawMode`'s window pointer is logged as a value - both sides take
the same 8 bytes from the packet cursor - and the window's words are compared
in the packet buffer). **The group's own** called directly, listed by name with
`FxCurrent`, which logs `Sprite_Current` and its dwords `+0`, `+8`, `+0x30`:
`_DrawPanel` at two whole words, `_DrawOverlay` at one, `_DrawRing` /
`_DrawDisc` at three whole words and the shade byte, `EffectKind18Sub64_Draw`
whole but the step byte, `_ShadeCluts` at one; `Sub5E_Draw`, `Sub5D_Draw`,
`_DrawTracks`, `_DrawGlow`, `_DrawMist`, `_ClearTracks`, `Sub64_Pulse` at none.

**Seeds** (per function, after the harness's per-round fill): on all 20
records the slide `+0x30` at and one step about its ends (0, 0x10, 0x70, 0x80,
0xB0, 0xC0, ..), `+8` 0 half the time, `+9` at the states' and the glow's
boundaries (0x14, 0x1F, 0x2C, 0x3F, 0x78, 0x7F, 0xFF, ..), `+0xA` about 11,
`+0xB` a variant below two (0x5C's draw indexes by it), `+0xC` one of the two
enemy records (0x64's states read through it), `+0x38` about 0x60 / 0x80 /
0x100; the current record's cells small two times in three, the z cell 0 half
the time; for a place state the variant inside its room and the leader about
the variant's cell; the leader seven times in eight a whole cell -5..+5 off
with a fraction at the tests' boundaries; `Field_Request` 0 half the time;
`Frame_Counter`'s low two bits 0 half the time; `Cond_ByteFE` 1 half the time
(below 7, every step, for the run); `MapView_FocusX` 0x16FF half; `Draw_PassFlags`
bit 2 half; the ring's heads inside the ring and the head marks' cells about
the followers' (one either side); the mist's `+2` at 0, 0x40, 0x80, 0xC0 and
0xFC; `Field_Kind2`'s cells about the map's (the field mode keeps the header
below 0x20); `Game_Mode` / `Game_Step` 5 half; the enemy records' bytes `+1`
/ `+2` one of the eight pairs the watch tests; `0x903848` 0xD half.
**Arguments**: the side below two and dy -2..1 or any; the overlay's y small
or any; the ring's and the disc's centre near the screen's and the radius
below 0x200 two times in three; the 0x64 draw at its callers' values two times
in three; the level 0..0x80 two times in three. **Disturbance** (the group's,
from the hash only): the slide, `+8`, `+9`, `+0xA`, the leader's x or z, a cell
word, `+0x34` / `+0x38`.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_6d`,
exit 0): 204,000 rounds over 51 functions, 10,534,193 calls to the stand-ins, **0 mismatches**; 27,596 bytes of state in 50 regions; 399 stand-ins. Every entry of the six tables reached (each handler recorder 542..1,351 calls; `0x5166C0` as `_Pulse` 880, `0x515160` as `_ClearTracks` 4,552); `MapView_ItemHalfAt` 426,588, `AreaMap_Elevation` 52,000, `Flags_Set` 5,154, `Flags_Clear` 2,049, `Effect_Release` 1,680.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` ran to its end after this paragraph was first written (slow under four parallel groups, it outlived its launcher): read from the log, 714 self-test lines, no mismatch, none failed; no exit code ([`takeover-queue-round13.md`](takeover-queue-round13.md) section 17, which gives the count; this paragraph's earlier "over 940 self-test lines" while it ran was not reconciled with it). This group ran no `BOF3X_WIDE=1` star run; the round tip's (`61001f7`, section 17) exited 0. `ledger_check`: 72 entries, 0 errors (8,501 impl lines, 8,501 functions detoured).

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 51 (9,985 bytes against the cut's
  10,366); 33 differ by padding only and one by code - `0x516690`, which the
  cut gives 160 bytes: it ends at its `ret` `0x5166BE` (0x2F), and the code
  after it is `0x5166C0`, a function of its own.
- **`0x5166C0`** (no list had it): `EffectKind18Sub64_States[5]` and the
  target of `_Swell`'s tail jump; its own frame (`push esi; push edi` .. `ret`).
  Taken as `EffectKind18Sub64_Pulse`.
- **`0x515E00`** (the cut's row, hidden in `0x515CC0`): reached only by the
  run's tail jump, but with its own frame and `ret` - a function, taken whole
  (the addendum's rule), not a shared tail.
- **Hidden starts**: 39 of the cut's and `0x5166C0`, each an entry by address
  - a cell of `EffectKind18_States` or of a sub-kind's table, or a direct call
  or tail jump - not a case of a switch, not a second entry. Their recorded
  hosts: E6C's `0x5140C0` (0x5C's six, which run past its `ret`), and this
  band's `0x5144F0`, `0x514880`, `0x514DF0`, `0x515CC0`, `0x5167C0`, whose
  catalog extents (`38B`, `56B`, `36D`, `AFA`, `2C3`) span the hidden starts
  after their own `ret` (`0x514683`, `0x514A67`, `0x514FFC`, `0x515DFB`,
  `0x5169EC`). None contains another's code as a fall-through.
- **The sub-kind dispatchers**: every `EffectKind18_States` cell pointing into
  the band is one of this group's rows (`[0x5C]`, `[0x5D]`, `[0x5E]`, `[0x61]`
  .. `[0x65]`); `[0x5F]` and `[0x60]` (`0x6541E8`, `0x6541EC`) point outside
  it (`0x41D620`, `0x421F10`).
- **The cut's columns**: the unit hints (tables `0x65F40C`, `0x65F424`,
  `0x65F450`, `0x65F55C`, `0x65F57C` and `EffectKind18_States`) are right; the
  "(12)", "(8)" counts are rows per hint, not table lengths; the label "Area
  overlays, world 4" on 0x5C's states is the catalog's, not what they are.
- **PSX twins**: none in the band.

## 6. Controls

Planted one at a time by a scratch script (`controls.py`: each plant anchored
on a unique string of `effect_6d.cpp`, rebuilt, run under
`BOF3X_E6D_ONLY=<filter>`, the file restored and rebuilt at the end; the
committed file has no switch). The harness stops at the first differing round,
so the column is the round that refused it (0-based) in this worktree; every
refused run exited 3 on a `MISMATCH` line. **164 of 166 refused.** Not refused: **33**, an equivalent mutant - `EffectKind18Sub5E_Draw` reads the second ground point's x from vertex 1, and vertex 3 holds the same word (both get the `- 0x4040` x), so reading vertex 3 cannot differ; its near variant **164** (vertex 2, the other x) is refused. **94**, an equivalent mutant - the glow's `t = b > 0x14 ? b - 0x14 : 0` and `b > 0x13 ? ..` agree at b = 0x14 (both 0); its near variant **165** (`b > 0x12`) is refused. In the first run 165 was not refused: the glow steps `+9` before it tests, and the seed's counters held 0x13 but not 0x12 - the fuzz's fault; the seed now holds one below each boundary, and every control was run again on it (the table is that second run). The first run's **6** (`step ^ 1`) mismatched at round 0 and then faulted on the step past the table; it is replaced by `(step + 1) % 7`. **166** and the `s = S()` controls (40, 49) drop a `Sprite_Current` re-read after a call (the disturbance moves it).

| # | Run (`_ONLY`) | Plant | Refused at |
|--:|---|---|---|
| 1 | `Sub5C_Run` | `nd18Sub5E_States), EffectKind18Sub5E_Sta` for `nd18Sub5C_States), EffectKind18Sub5C_Sta` | round 0 |
| 2 | `Sub5E_Run` | `nd18Sub5D_States), EffectKind18Sub5D_Sta` for `nd18Sub5E_States), EffectKind18Sub5E_Sta` | round 0 |
| 3 | `Sub5D_Run` | `nd18Sub5C_States), EffectKind18Sub5C_Sta` for `nd18Sub5D_States), EffectKind18Sub5D_Sta` | round 0 |
| 4 | `Sub64_Run` | `_States) + 4, 6u)` for `_States), EffectKind18Sub64_States_count)` | round 0 |
| 5 | `Sub65_Run` | `nd18Sub64_States), 3u)` for `nd18Sub65_States), EffectKind18Sub65_States_count)` | round 0 |
| 6 | `Sub61_Run` | `) + 4 * ((step + 1) % 7))` for `) + 4 * step)` | round 0 |
| 7 | `Sub61_Run` | `b61_DrawMist)(); SH_CALL(EffectKind18Sub61_DrawTracks)();` for `b61_DrawTracks)(); SH_CALL(EffectKind18Sub61_DrawMist)();` | round 0 |
| 8 | `Sub5C_Place` | `char>(v + 1);` for `char>(v);` | round 0 |
| 9 | `Sub5C_Place` | `lags + v + 1)[0]` for `lags + v)[0]` | round 0 |
| 10 | `Sub5C_Place` | `oryFlags + 1), s` for `oryFlags), s` | round 0 |
| 11 | `Sub5C_Place` | `+ 2 * v)[0]` for `+ 2 * v + 1)[0]` | round 0 |
| 12 | `Sub5C_WaitNear` | `u) <= 0x1F000;` for `u) <= 0x20000;` | round 1202 |
| 13 | `Sub5C_WaitNear` | `f (!near) re` for `f (!near && !forced) re` | round 1 |
| 14 | `Sub5C_WaitNear` | `L(Flags_Set)(At` for `L(Flags_Clear)(At` | round 0 |
| 15 | `Sub5C_WaitNear` | `yteFE = 2;` for `yteFE = 0;` | round 0 |
| 16 | `Sub5C_WaitNear` | `::kSoundShut));` for `::kSoundOpen));` | round 12 |
| 17 | `Sub5C_Open` | `0) >= 0xB0) S` for `0) >= 0xC0) S` | round 12 |
| 18 | `Sub5C_Open` | `0) >= 0x90 ?` for `0) >= 0x80 ?` | round 1 |
| 19 | `Sub5C_WaitFar` | `))) > 0x3F000` for `))) > 0x40000` | round 28 |
| 20 | `Sub5C_WaitFar` | `u) > 0x31000)` for `u) > 0x30000)` | round 58 |
| 21 | `Sub5C_WaitFar` | `Step` for `Cond_ByteFE = 0; Step` | round 0 |
| 22 | `Sub5C_Close` | `s), S()[9]);` for `s), S()[8]);` | round 0 |
| 23 | `Sub5C_Close` | `FFF0u) < 0)` for `FFF0u) <= 0)` | round 0 |
| 24 | `Sub5C_DrawPanel` | `u - 0x4000u;` for `u - 0x4040u;` | round 0 |
| 25 | `Sub5C_DrawPanel` | `- 0x4041u;` for `- 0x4040u;` | round 0 |
| 26 | `Sub5C_DrawPanel` | `ord(t + 2));` for `ord(t + 4));` | round 0 |
| 27 | `Sub5C_DrawPanel` | `riant + 2 - i))` for `riant + i))` | round 0 |
| 28 | `Sub5C_DrawPanel` | `(dy, 0x44);` for `(dy, 0x48);` | round 0 |
| 29 | `Sub5C_DrawPanel` | `CSides)[1 - side` for `CSides)[side` | round 0 |
| 30 | `Sub5C_DrawPanel` | `reenXY)) + 0.5L));` for `reenXY))));` | round 0 |
| 31 | `Sub5E_Draw` | `(0x11Du + i)` for `(0x11Du - i)` | round 0 |
| 32 | `Sub5E_Draw` | `x14, 0x100u -` for `x14, 0x180u -` | round 0 |
| 33 | `Sub5E_Draw` | `und(v + 0x18);` for `und(v + 8);` | **not refused** |
| 34 | `Sub5E_Draw` | `lid - 0x3F40u)` for `lid - 0x4040u)` | round 0 |
| 35 | `Sub5E_Draw` | `m)(6, 0x8);` for `m)(6, 0xC);` | round 0 |
| 36 | `Sub5E_Draw` | `(v + 4, 1u -` for `(v + 4, 0u -` | round 0 |
| 37 | `Sub5E_Start` | `E, 0xFF81u);` for `E, 0xFF80u);` | round 0 |
| 38 | `Sub5E_Start` | `0x30, 0xB0);` for `0x30, 0xC0);` | round 70 |
| 39 | `Sub5E_WaitNear` | `erAt(s, false, 0` for `erAt(s, true, 0` | round 120 |
| 40 | `Sub5E_WaitNear` | `dOpen));` for `dOpen)); s = S();` | round 284 |
| 41 | `Sub5E_Open` | `0) >= 0xD0) S` for `0) >= 0xC0) S` | round 3 |
| 42 | `Sub5E_WaitFar` | `0000, 0x1F000)` for `0000, 0x20000)` | round 34 |
| 43 | `Sub5E_Close` | `ide(0xFFE0u)` for `ide(0xFFF0u)` | round 0 |
| 44 | `Sub5D_Place` | `::kSub5DLifts +` for `::kSub5DHeights +` | round 0 |
| 45 | `Sub5D_Place` | `+ 4 * v) + 1);` for `+ 4 * v));` | round 0 |
| 46 | `Sub5D_Place` | `0x30, 0x70);` for `0x30, 0x80);` | round 1 |
| 47 | `Sub5D_Place` | `(s + 0x36) ==` for `(s + 0x3A) ==` | round 0 |
| 48 | `Sub5D_WaitNear` | `s, s[8] == 0,` for `s, s[8] != 0,` | round 120 |
| 49 | `Sub5D_WaitNear` | `dOpen));` for `dOpen)); s = S();` | round 477 |
| 50 | `Sub5D_WaitFar` | `000, 0x21000)` for `000, 0x20000)` | round 68 |
| 51 | `Sub5D_Open` | `0) >= 0x90) S` for `0) >= 0x80) S` | round 1 |
| 52 | `Sub5D_Close` | `0u) <= 0x10) Sh` for `0u) <= 0) Sh` | round 40 |
| 53 | `Sub5D_Close` | `Request != 0)` for `Request == 0)` | round 0 |
| 54 | `Sub5D_Draw` | `+ 0x3E) + Wor` for `+ 0x3E) - Wor` | round 0 |
| 55 | `Sub5D_Draw` | `8]) << 20)` for `8]) << 21)` | round 3 |
| 56 | `Sub5D_Draw` | `Link(-2, 0x` for `Link(-1, 0x` | round 0 |
| 57 | `Sub5D_Draw` | `+ 0x3A) + sid` for `+ 0x3A) - sid` | round 0 |
| 58 | `Sub5D_Draw` | `0x18, x + sli` for `0x18, x - sli` | round 3 |
| 59 | `Sub61_WaitFocus` | `== 0x16FE) S(` for `== 0x16FF) S(` | round 0 |
| 60 | `Sub61_WaitFocus` | `()[2] = 5;` for `()[2] = 4;` | round 0 |
| 61 | `Sub61_Slide` | `unter & 7) ==` for `unter & 3) ==` | round 4 |
| 62 | `Sub61_Slide` | `() + 0x38)));` for `() + 0x34)));` | round 0 |
| 63 | `Sub61_Hold` | `verlay)(1); }` for `verlay)(0); }` | round 0 |
| 64 | `Sub61_Rise` | `(I(v) >= 0x1` for `(I(v) > 0x1` | round 34 |
| 65 | `Sub61_Rise` | `(v) > 0x5F) {` for `(v) > 0x60) {` | round 5 |
| 66 | `Sub61_Rise` | `0x38) + 3);` for `0x38) + 2);` | round 10 |
| 67 | `Sub61_Rise` | `38, v + 2);` for `38, v + 1);` | round 2 |
| 68 | `Sub61_Reset` | `3] = 0x55;` for `3] = 0x54;` | round 0 |
| 69 | `Sub61_Reset` | `x34, 0x53);` for `x34, 0x54);` | round 0 |
| 70 | `Sub61_Reset` | `yteFE = 1; }` for `yteFE = 0; }` | round 0 |
| 71 | `Sub61_ClearTracks` | `At(at::kTrackHeads)[0] = 0;` for `SetWord(At(at::kTrackHeads), 0);` | round 0 |
| 72 | `Sub61_ClearTracks` | `g)[5] = 1;` for `g)[5] = 0;` | round 0 |
| 73 | `Sub61_ClearTracks` | `(U k = 1; k` for `(U k = 0; k` | round 0 |
| 74 | `Sub61_Rearm` | `yteFE = 2;` for `yteFE = 1;` | round 0 |
| 75 | `Sub61_DrawOverlay` | `Flags & 2) ==` for `Flags & 4) ==` | round 2 |
| 76 | `Sub61_DrawOverlay` | `0x10, 0x20);` for `0x10, 0x10);` | round 0 |
| 77 | `Sub61_DrawOverlay` | `Fu) \| 0x80u);` for `Fu) \| 0x90u);` | round 0 |
| 78 | `Sub61_DrawOverlay` | `) + 0x1E4u) <` for `) + 0x1E5u) <` | round 0 |
| 79 | `Sub61_DrawOverlay` | `ar>(e[0xB] +` for `ar>(e[0xC] +` | round 0 |
| 80 | `Sub61_DrawOverlay` | `2)) + (h);` for `2)) + (dy + h);` | round 0 |
| 81 | `Sub61_DrawOverlay` | `4] = 0x41;` for `4] = 0x40;` | round 0 |
| 82 | `Sub61_DrawOverlay` | `0x18, f256);` for `0x18, f320);` | round 0 |
| 83 | `Sub61_DrawDisc` | `)(4, 0x30);` for `)(4, 0x34);` | round 0 |
| 84 | `Sub61_DrawDisc` | `r>(shade + 1);` for `r>(shade);` | round 0 |
| 85 | `Sub61_DrawDisc` | `((a + 0x800u)` for `((a + 0x1000u)` | round 0 |
| 86 | `Sub61_DrawDisc` | `* r) + x));` for `* r) + y));` | round 0 |
| 87 | `Sub61_DrawRing` | `r = r + 3, in` for `r = r + 2, in` | round 0 |
| 88 | `Sub61_DrawRing` | `r = r - 7;` for `r = r - 8;` | round 0 |
| 89 | `Sub61_DrawRing` | `ours(p, 1, c)` for `ours(p, 0, c)` | round 0 |
| 90 | `Sub61_DrawRing` | `each) + x));` for `each) + y));` | round 0 |
| 91 | `Sub61_DrawRing` | `rn I(v) >> 12;` for `rn I(v) / 4096;` | round 0 |
| 92 | `Sub61_DrawGlow` | `) > 0x1F0) radius = 0x1F0;` for `) > 0x1F4) radius = 0x1F4;` | round 1 |
| 93 | `Sub61_DrawGlow` | `12 + 0x7F;` for `12 + 0x80;` | round 0 |
| 94 | `Sub61_DrawGlow` | `b > 0x13 ? b` for `b > 0x14 ? b` | **not refused** |
| 95 | `Sub61_DrawGlow` | `th) > 0x2F) width = 0x2F;` for `th) > 0x30) width = 0x30;` | round 0 |
| 96 | `Sub61_DrawGlow` | `6, 8}, {5, 0x` for `6, 8}, {4, 0x` | round 0 |
| 97 | `Sub61_DrawGlow` | `U b2 = b;` for `U b2 = S()[9];` | round 1 |
| 98 | `Sub61_DrawGlow` | `9] < 0xFE) S(` for `9] < 0xFF) S(` | round 16 |
| 99 | `Sub61_DrawMist` | `U>(e >> 2);` for `U>(e >> 1);` | round 0 |
| 100 | `Sub61_DrawMist` | `f (off >= 0x4` for `f (off > 0x4` | round 5 |
| 101 | `Sub61_DrawMist` | `7) + 0x61) >>` for `7) + 0x60) >>` | round 1 |
| 102 | `Sub61_DrawMist` | `(c2 & 0x3F) <` for `(c2 & 0x1F) <` | round 0 |
| 103 | `Sub61_DrawMist` | `()[2] + 3);` for `()[2] + 4);` | round 0 |
| 104 | `Sub61_DrawMist` | `tBits(0x1FF - o` for `tBits(0x200 - o` | round 0 |
| 105 | `Sub61_DrawMist` | `6, 0x78C1);` for `6, 0x78C0);` | round 0 |
| 106 | `Sub61_DrawMist` | `ow(0, 0x80);` for `ow(0, 0x40);` | round 0 |
| 107 | `Sub61_DrawMist` | `ext, 0, 0, 0x` for `ext, 0, 1, 0x` | round 0 |
| 108 | `Sub61_DrawTracks` | `st[0]) >= cel` for `st[0]) > cel` | round 17 |
| 109 | `Sub61_DrawTracks` | `(last[2]) &` for `(last[2] - 1) &` | round 0 |
| 110 | `Sub61_DrawTracks` | `ollower[4];` for `ollower[6];` | round 0 |
| 111 | `Sub61_DrawTracks` | `er) + 0x8000u` for `er) + 0x4000u` | round 1 |
| 112 | `Sub61_DrawTracks` | `x25808001u` for `x25808000u` | round 0 |
| 113 | `Sub61_DrawTracks` | `+ 2 * r)[0]` for `+ 2 * r + 1)[0]` | round 0 |
| 114 | `Sub61_DrawTracks` | `teps + r + 1)[0]` for `teps + r)[0]` | round 0 |
| 115 | `Sub61_DrawTracks` | `1) & 0xF;` for `1) & 0x1F;` | round 0 |
| 116 | `Sub61_DrawTracks` | `corner, 8);` for `corner, 12);` | round 0 |
| 117 | `Sub62_Ripple` | `e = 0x5DD;` for `e = 0x5DC;` | round 0 |
| 118 | `Sub62_Ripple` | `st<U>((s1 >> 10) - (s2 >>` for `st<U>((s2 >> 10) - (s1 >>` | round 11 |
| 119 | `Sub62_Ripple` | `7) - 0x40u)` for `7) - 0x80u)` | round 0 |
| 120 | `Sub62_Ripple` | `_Header[0]))` for `_Header[1]))` | round 2 |
| 121 | `Sub62_Ripple` | `t1] + up02);` for `t1] + up13);` | round 11 |
| 122 | `Sub62_Ripple` | `edraw = 1;` for `edraw = 2;` | round 0 |
| 123 | `Sub62_Ripple` | `st<U>(h) + 1);` for `st<U>(h));` | round 0 |
| 124 | `Sub62_Ripple` | `l) - 0x13;` for `l) - 0x14;` | round 11 |
| 125 | `Sub63_Glow` | `yteFE = 2;` for `yteFE = 1;` | round 0 |
| 126 | `Sub63_Glow` | `+ 4 * k2)));` for `+ 4 * k2 + 2)));` | round 0 |
| 127 | `Sub63_Glow` | `n)[0] + 6)[0]` for `n)[0] + 0xE)[0]` | round 0 |
| 128 | `Sub63_Glow` | `pths4_10)(p)` for `pths4_10B)(p)` | round 0 |
| 129 | `Sub63_Glow` | `B)) + 0x100)` for `B)) + 0x200)` | round 0 |
| 130 | `Sub63_Glow` | `* k2 + 4));` for `* k2 + 2));` | round 0 |
| 131 | `Sub64_Place` | `tride * (v ^ 1));` for `tride * v);` | round 0 |
| 132 | `Sub64_Place` | `0]) << 15);` for `0]) << 16);` | round 0 |
| 133 | `Sub64_WaitBattle` | `de == 5 \|\| Gam` for `de == 5 && Gam` | round 1 |
| 134 | `Sub64_Watch` | `Step != 4) S(` for `Step != 5) S(` | round 5 |
| 135 | `Sub64_Watch` | `()[2] = 4;` for `()[2] = 5;` | round 24 |
| 136 | `Sub64_Watch` | `u) & 0xF0u;` for `u) & 0xF8u;` | round 118 |
| 137 | `Sub64_Watch` | `edIs(6, 5)) S` for `edIs(6, 4)) S` | round 5 |
| 138 | `Sub64_Fade` | `[9] > 0x40) SH` for `[9] > 0x3F) SH` | round 48 |
| 139 | `Sub64_Fade` | `, I(a), 2, 0)` for `, I(a), 1, 0)` | round 0 |
| 140 | `Sub64_Hold` | `edIs(8, 1)) s` for `edIs(8, 2)) s` | round 0 |
| 141 | `Sub64_Hold` | `, 0x70, 1, 0)` for `, 0x70, 2, 0)` | round 0 |
| 142 | `Sub64_Pulse` | `2_t e = 9 - A` for `2_t e = 8 - A` | round 0 |
| 143 | `Sub64_Pulse` | `(c > 0x10) {` for `(c > 0xF) {` | round 24 |
| 144 | `Sub64_Pulse` | `(-e) << 3)` for `(-e) << 4)` | round 0 |
| 145 | `Sub64_Swell` | `)) + 0x21;` for `)) + 0x20;` | round 0 |
| 146 | `Sub64_Swell` | `()[9] + 7);` for `()[9] + 8);` | round 0 |
| 147 | `Sub64_Swell` | `()[2] = 6;` for `()[2] = 5;` | round 24 |
| 148 | `Sub64_Draw` | `(f > 0xC) f` for `(f > 0xB) f` | round 14 |
| 149 | `Sub64_Draw` | `? f0 - 3 : f` for `? f0 - 4 : f` | round 0 |
| 150 | `Sub64_Draw` | `xBF009110u;` for `xBF009111u;` | round 0 |
| 151 | `Sub64_Draw` | `xFFFFFF41u -` for `xFFFFFF40u -` | round 0 |
| 152 | `Sub64_Draw` | `(0u - (w +` for `(0u - (2 * w +` | round 0 |
| 153 | `Sub64_Draw` | `- along + l);` for `- along - l);` | round 0 |
| 154 | `Sub64_Draw` | `Link(0, 0x` for `Link(1, 0x` | round 0 |
| 155 | `Sub64_Draw` | `) - 0x3FE0u;` for `) - 0x3FF0u;` | round 0 |
| 156 | `Sub65_ShadeCluts` | `= 0x8001;` for `= 0x8000;` | round 3 |
| 157 | `Sub65_ShadeCluts` | `F) * l) >> 7);` for `F) * l) / 128);` | round 1 |
| 158 | `Sub65_ShadeCluts` | `Dirty = 2;` for `Dirty = 1;` | round 0 |
| 159 | `Sub65_ShadeCluts` | `0; k < 0x100; ++` for `0; k < at::kClutWords; ++` | round 0 |
| 160 | `Sub65_Start` | `eCluts)(1);` for `eCluts)(0);` | round 0 |
| 161 | `Sub65_Wait` | `0] == 0xC) St` for `0] == 0xD) St` | round 1 |
| 162 | `Sub65_FadeIn` | `S()[9] > 0x8` for `S()[9] >= 0x8` | round 131 |
| 163 | `Sub65_FadeIn` | `)(S()[9] + 1);` for `)(S()[9]);` | round 0 |
| 164 | `Sub5E_Draw` | `und(v + 0x10);` for `und(v + 8);` | round 0 |
| 165 | `Sub61_DrawGlow` | `b > 0x12 ? b` for `b > 0x14 ? b` | round 50 |
| 166 | `Sub5C_WaitNear` | `s[2] = static_cast<unsigned char>(s[2] + 1);` for `StepUp();` | round 73 |

## 7. Latent defects (Capcom's, described, not fixed)

- **Sub-kind 0x5C's far wait can step `+2` past its table.** (D225) `_WaitFar` steps
  `+2` once when `Cond_ByteFE` is 1 and once more when the leader is away:
  both in one frame take it from 3 to 5, and the next frame's dispatch jumps
  through `0x65F388` - the variant cells read as a code pointer. Ours steps it
  the same and aborts in the dispatcher. Ordinary play reaches it only if the
  area's script sets `Cond_ByteFE` to 1 while the leader is more than three
  cells from the open panels; not measured. The game's nearest answer would
  be 4 (`_Close`); the fix is the owner's word.
- **Unchecked indexes** (D200): the five dispatchers and the run do not bound their
  index (every writer in the band keeps `+2` inside its table but the case
  above; `Cond_ByteFE` is the areas' scripts'); the variant index is the
  spawn's x cell, s16 and unchecked, into tables with room for two (0x5C,
  0x5D, 0x64); 0x5C's draw indexes its textures by `+0xB` (room two) and its
  signs by the side (its callers push 0 and 1); the tracks' ring by a head
  byte the code keeps below 32 (`& 0x1F`) but `_Reset` never writes. Ours
  aborts past any.
- **The INT_MIN distance** (D212): the waiting states' `cdq; xor; sub` absolute value
  leaves `0x80000000` negative, so a leader exactly 0x8000 cells away counts as
  near. Unreachable on a map; ours computes it the same way.
- **0x5C draws nothing while waiting shut or open** (D203, D238), and 0x64's `_Watch` can
  step `+2` down below 2 (`Game_Mode` 5 and `Game_Step` not, in state 2 gives
  1, `_WaitBattle`, which steps it back): as read, perhaps deliberate.
- **`MapView_ScreenXY` as scratch** (D213): 0x5C's draw stores its two base floats
  into the map view's screen point, which `MapView_Build` owns; harmless if
  the map view rewrites it before reading, not measured. Ours writes it too.
- **0x62's ripple accumulates** (D211): each frame adds the difference of two sine
  steps to the map's corner bytes, wrapping at 256, and never restores them.

## 8. Calls across groups

**Outbound**: none raw (`band_rows.py --edges`: no call from E6D into another
group). By name, already ours: `Flags_Set`, `Flags_Clear`, `AreaMap_Elevation`,
`MapView_ItemHalfAt`, `MapView_LinkPrimAt`, `Math_Sin`, `Math_Cos`,
`Sound_PlayEffect`, `Effect_Release`, `Gfx_CommitPrim`, the `Gpu_*`,
`Gte_RotTransPers4`, `Gte_PrimDepths4_10`, `Gte_PrimDepths4_10B`,
`Prim_SetTexture`. The CRT's `_ftol` `0x5B9550` is done in place (effect_5d's
`Ftol`).

**Inbound from outside the group** (for the rebinding pass): none by code.
`EffectKind18_Run` `0x46D830` (ours, worldmap_area) reaches the eight
dispatchers and states through `EffectKind18_States` `[0x5C]`, `[0x5D]`,
`[0x5E]`, `[0x61]`..`[0x65]`, read in place.

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 50 of the cut's rows (`0x5166C0` has no row), and no
first-call trace under `analysis/calltrace` names any of the 51 (a scan of
every file: the hits are the extent lists `entries*.txt`, not traces). **Fuzz
only.** No live run was made (the brief). No spawner is known; a recorded
route through an area whose effect list holds one of these sub-kinds would let
the coordinator's frame-hash A/B cover it.

## 10. The rebinding

`grep -rn -i` of the 51 addresses and the six tables in `src/game`
(`band_rows.py --refs`: 0 references). **Nothing to rebind**; nothing left
raw; no raw reference to the band in another group's files.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): 40 lines, the read extents
of the 39 hidden starts and `0x5166C0`. The five host lines `005144F0 38B`,
`00514880 56B`, `00514DF0 36D`, `00515CC0 AFA`, `005167C0 2C3` are left in
place (each spans the hidden starts after its own `ret`; the code is 0x194,
0x1E8, 0x20D, 0x13C, 0x22D), as E5G left its hosts'.
