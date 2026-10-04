# Group E6B: kind 0x18's sub-kinds 0x33..0x35, 0x37, 0x38, 0x3B, 0x3D, 0x40, 0x41, 0x44 and 0x54

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..13),
wave six, from the round branch's tip `c4deb22`. **50 functions ours**
(`src/game/effect_6b.cpp`, shadow name `effect_6b`): the cut table's 50 rows
for E6B (`analysis/round13_cut.tsv`, the band `0x50E400..0x510C80`), none
added, none dropped. Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
200,000 rounds, 0 mismatches; **112 of 112 controls refused**. **Fuzz only**: no recorded
route enters any of the 50 (section 10). Every row is effect code: the one
`hypothesis` row (`0x50FD30`) is a sub-kind dispatcher like the others, so
nothing is left original. **DIV-0041's last listed fill `0x50F7B5` is in this
band** (sub-kind 0x54, section 2): it now draws through `Widescreen_FillX` /
`Widescreen_Fill`; no new ledger entry (the coordinator amends DIV-0041).
**Nothing here needs a ledger entry**: no function reads memory it never
wrote into what it draws (section 8).

All eleven are sub-kinds of effect kind 0x18: `EffectKind18_Run` (`0x46D830`,
ours) jumps through `EffectKind18_States` by `+1`, which `EffectKind18_Start`
copies from `+0xB`. Nine of them dispatch again by `+2` through a table of
their own; two are one state.

| Sub-kind | Functions | Reached through |
|---|--:|---|
| 0x33: two panels at a cell a variant table picks by the spawn's x cell, drawn by E6A's `0x50E1C0`; when the leader stands at the cell's front they slide 0x100 over eight frames (sound 0x200), and back once the leader is two cells away (sound 0x201) | 6 | `EffectKind18_States[0x33]` (`0x654138`), `EffectKind18Sub33_States` `0x65EBC0` (5) |
| 0x34: 0x33's states with their own variant table | 6 | `[0x34]` (`0x65413C`), `EffectKind18Sub34_States` `0x65EBD8` (5) |
| 0x35: 0x33's cycle sliding the other way (0 .. -0x100), drawn by its own quad on the ground (`EffectKind18Sub35_Draw`) | 7 | `[0x35]` (`0x654140`), `EffectKind18Sub35_States` `0x65EC08` (5) |
| 0x37: the panels placed open (0x100), held eight frames, slid shut by 0x10 a frame (sound 0x201), then E6A's draw held | 4 | `[0x37]` (`0x654148`), `EffectKind18Sub37_States` `0x65EBF4` (4: three here and E6A's `0x50E1C0`) |
| 0x38: placed shut (-0x100) when the leader is at the front, held eight frames, slid open to 0 (sound 0x201), then 0x35's draw held; placed open and held when the leader is not there | 3 | `[0x38]` (`0x65414C`), `EffectKind18Sub38_States` `0x65EC28` (4: `_Place`, 0x37's `_Hold`, `_Close`, 0x35's draw) |
| 0x3B: 0x33's cycle at 0xC0 by 0x10, a lift from the variant, drawn as two flat quads (`EffectKind18Sub3B_Draw`) | 7 | `[0x3B]` (`0x654158`), `EffectKind18Sub3B_States` `0x65EC40` (5) |
| 0x3D: while `Cond_ByteFE` is set, a ripple run through the map's corner heights over 35 x 35 cells about the leader, the leader's height then read again; never ends by its own code | 1 | `[0x3D]` (`0x654160`) |
| 0x40: three semi-transparent textured panes (a 16 x 16 texture window) that wait on the chapter's run 0xC at step 2, then on `Cond_ByteFE` 1 and 2 (sounds 0x206, 0x203), then move a step a frame until `Cond_ByteFE` is 0 and release | 6 | `[0x40]` (`0x65416C`), `EffectKind18Sub40_States` `0x65EC64` (4) |
| 0x41: four textured quads turned about y by an angle from a curve table, sound 0x200 / 0x201 by story flag 0x4F, story flag 0x50 set at the curve's end, two of Capcom's helpers in no group called while flag 0x4F holds, released after 0x31 frames | 6 | `[0x41]` (`0x654170`), `EffectKind18Sub41_States` `0x65EC9C` (4) |
| 0x44: a sprite animation bank 0x2A8 set on the record, then E6C's two functions every frame | 3 | `[0x44]` (`0x65417C`), `EffectKind18Sub44_States` `0x65EE78` (2) |
| 0x54: a semi-transparent, additive blue fill over the frame, its blue stepped through four levels every eighth frame - E5G's `EffectKind18Sub36_Pulse` with its own table; never ends by its own code | 1 | `[0x54]` (`0x6541BC`) |

What each looks like in the game is not stated here (the owner's to say);
the descriptions are what the code draws and tests. "Panels" and "panes" are
words for textured quads; whether they are doors, gates or something else is
not established. **No spawner is found**: a grep of our sources for these
sub-kind numbers stored into `+0xB` finds none (kind 0x18 records come from
the areas' effect lists, data not code, as E5G found for its sub-kinds); the
spawn's `+0x34` (the 16.16 x) carries the variant in its cell for the panel
sub-kinds and the z cell's 0 chooses the axis (`+8`).

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`, the nine dispatchers `evidence`). **PSX twins**
(`analysis/pairs_propagated.json`): five, all in the overlay AREA106 (md5
`caf425f1...`; the sibling's `names/places.toml` gives area 106 the
developers' name "なみのかいろう 2") - `0x50FD50` / `0x801F2C8C`, `0x50FD60` /
`0x801F2CB4` (table-anchored), `0x50FD90` / `0x801F2D0C`, `0x50FF10` /
`0x801F2F38` (callers), `0x50FE20` / `0x801F2DDC` (gap67): sub-kind 0x41's
states and draw. The sibling names none of them (`names/area_records.toml`
lists those PSX addresses only as other areas' handler entries, which share
the overlay base address); the twins say what the code is, not a name.

## 1. What each function does

### 1.1 The panel sub-kinds 0x33, 0x34, 0x35, 0x3B (five states each)

| PC (0x33 / 0x34 / 0x35 / 0x3B) | Name | What |
|---|---|---|
| `0x50E400` / `0x50E750` / `0x50EB80` / `0x50F230` | `EffectKind18SubNN_Run` | `jmp [EffectKind18SubNN_States + +2 * 4]`, unbounded |
| `0x50E420` / `0x50E770` / `0x50EBA0` / `0x50F250` | `_Place` (0) | `+8` = whether the spawn's z cell (s16 `+0x3A`) is 0; the variant `v` = s16 `+0x36` (read first, unchecked) names a byte pair at the cell table (`0x65EBD4`, `0x65EBEC`, `0x65EC1C`, `0x65EC58`) put into `+0x36` / `+0x3A`; 0x3B also its s16 lift `0x65EC54[v]` into `+0x3E`; `+0x30` 0, `+2` up; the leader already at the cell's front (z first when `+8`): `+0x30` = the open slide (0x100, 0x100, `0xFF00`, 0xC0) and `+2` = 3. Then the draw |
| `0x50E540` / `0x50E890` / `0x50ECC0` / `0x50F380` | `_WaitNear` (1) | the leader at the cell's front: sound 0x200 unless `Field_Request`, `+2` up (`Sprite_Current` read after the sound). The draw |
| `0x50E620` / `0x50E970` / `0x50EDA0` / `0x50F460` | `_Open` (2) | `+0x30` moved by 0x20, 0x20, -0x20, 0x10; at the open slide (signed, `>=` / `<=`) `+2` up. A tail jump to the draw |
| `0x50E640` / `0x50E990` / `0x50EDC0` / `0x50F480` | `_WaitFar` (3) | the front test at 0x20000 both ways fails: `+2` up. The draw |
| `0x50E710` / `0x50EA60` / `0x50EE90` / `0x50F550` | `_Close` (4) | `+0x30` moved back by the step; at 0 (`<= 0`; 0x35 `>= 0`) sound 0x201 unless `Field_Request`, `+2` = 1. A tail jump to the draw |

The draws: E6A's `0x50E1C0` (0x33, 0x34; raw), `EffectKind18Sub35_Draw`,
`EffectKind18Sub3B_Draw`. **The front test** is E5G's (its section 1.1): the
leader's point (`ObjTrio +0x34` / `+0x38`) against the cell - across the first
axis `|p - ((cell + 1) << 16 | 0x8000)|` within 0x8000, along the second
`|p - (cell << 16)|` or `|p - (cell << 16) - 0x10000|` within 0x10000; the far
test the same at 0x20000 both.

### 1.2 Sub-kinds 0x37 and 0x38 (four states each)

| PC | Name | What |
|---|---|---|
| `0x50EAA0` | `EffectKind18Sub37_Run` | dispatcher (hidden in E6A's `0x50E1C0`) |
| `0x50EAC0` | `EffectKind18Sub37_Place` (0) | `+8`; the cell pair `0x65EC04[v]`; `+0x3E` = `0xFF80`, `+0x30` = 0x100, `+2` up. **Nothing drawn** |
| `0x50EB20` | `EffectKind18Sub37_Hold` (1; also `EffectKind18Sub38_States[1]`) | `+9 >= 8` (unsigned): `+2` up; `+9` up every frame. Nothing drawn |
| `0x50EB40` | `EffectKind18Sub37_Close` (2) | `+0x30` -= 0x10; at 0 or below sound 0x201 unless `Field_Request` and `+2` **up** (to state 3: E6A's draw held). A tail jump to E6A's draw |
| `0x50F110` | `EffectKind18Sub38_Run` | dispatcher (hidden in `0x50EED0`) |
| `0x50F130` | `EffectKind18Sub38_Place` (0) | `+8`; the cell pair `0x65EC38[v]`; `+0x3E` = `0xFF80`; the leader at the front **z first whatever `+8` says**: `+0x30` = `0xFF00`, `+2` up (to the hold); else `+0x30` = 0, `+2` = 3. Nothing drawn |
| `0x50F1F0` | `EffectKind18Sub38_Close` (2) | `+0x30` += 0x10; at 0 or above sound 0x201 unless `Field_Request` and `+2` up (to 0x35's draw held). A tail jump to 0x35's draw |

`_Hold` counts `+9` from whatever the spawn left (`EffectKind18_Start` is
not read here); `+9` wraps past 0xFF and the test is unsigned.

### 1.3 The draws

| PC | Name | What |
|---|---|---|
| `0x50EED0` | `EffectKind18Sub35_Draw` (`EffectKind18Sub38_States[3]`) | a draw mode (page 0x95) committed at slot 6 (0xC); one POLY_FT4 in `Prim_VertexScratch`: with `+8` its z `(+0x3A << 7) - 0x3FC0` and x from `(+0x36 << 7) - +0x30 - 0x3F40` to `- 0x4040`, else x `(+0x36 << 7) - 0x3FC0` and z from `(+0x3A << 7) + +0x30 - 0x3F40` to `- 0x4040`; heights 0x40 and 0x180 less half the ground (`AreaMap_Elevation` at vertex 0's point for corners 0, 2, at vertex 1's for 1, 3 - four calls); `Gte_RotTransPers4`, `Gte_PrimDepths4_10`, `Prim_SetTexture((+8 << 21) \| 0x151010F, p, 1)`, committed at slot 6 (0x48) |
| `0x50F590` | `EffectKind18Sub3B_Draw` | with `+8` the corners' z `(+0x3A << 7) - 0x3FC0`, else their x `(+0x36 << 7) - 0x3FC0`; heights `+0x3E - 0x180` (corners 0, 1) and `+0x3E` (2, 3), all read before any call; a draw mode (page 0x95) at slot 6; two POLY_FT4s, side 0 and 1: the other axis `(cell << 7) - 0x3F40` / `- 0x4040` plus `+0x30` times the s8 `0x65EC5C[side]` (a 16-bit `imul`); projected, `Prim_SetTexture((+8 << 21) \| (0x10C - side) \| 0x12510000, p, 1)`, slot 6 (0x48) |
| `0x50FAE0` | `EffectKind18Sub40_Draw(int step)` | a RECT `{0x60, 0x10, 0x10, 0x10}` put at the packet cursor (the cursor past it) as the texture window of a draw mode (page 0x1B) at slot 5 (0xC); three semi-transparent POLY_FT4s (tpage 0x7B, clut 0x78C6, colour 0x80): pane `i` from the four bytes `t` at `0x65EC74 + 4i` - x `((t0 << 5) + step) * 4 - 0x4040` to `(((t0 + t2) << 5) + step) * 4 - 0x4040`, z likewise from `t1`, `t3`; every height `6 * step - 0x380`; pane 2's corner 0 moved by `8 * step` in x and z; u, v up to `(t2 << 4) - 1`, `(t3 << 4) - 1`; projected, slot 5 (0x48). Then a RECT `{0, 0, 0x100, 0x100}` likewise. The original stores `6 * step - 0x380` over its own argument's stack slot (the callers pop it unread) |
| `0x50FF10` | `EffectKind18Sub41_Draw(unsigned variant, int angle)` | a draw mode (page 0x95) at slot 5; `Gte_PushMatrix`; the point `(0xC840, 0xD240, 0xFB80)` through `Gte_RotTrans` into a MATRIX's translation, `Gte_RotMatrix` of `(0, -angle, 0)`, `Gte_MulMatrix0(Camera_Matrix, m, m)`, `Gte_SetRotMatrix`, `Gte_SetTransMatrix`; four POLY_FT4s, quad `q = 0x65ED08[variant * 4 + i]`: its vertices from the twelve bytes at `0x65ECD8 + 12q` (x, z, -y, each `<< 7`), projected, `Prim_SetTexture(0x65ED10[q] + (((s8 0x65ED20[q] * angle) & ~0x7FF) << 8), p, 1)`, slot 5 (0x48); `Gte_PopMatrix` |

### 1.4 The single-state sub-kinds

| PC | Name | What |
|---|---|---|
| `0x50F780` | `EffectKind18Sub54_Pulse` | a draw mode (page 0xB5: blend 1, additive) at slot 3 (0xC); a semi-transparent POLY_F4 over the frame - **DIV-0041's listed fill** (`0x50F7B5` is the `mov ecx, 320.0f`) - corners `(0, 0)`, `(320, 0)`, `(0, 240)`, `(320, 240)` as floats, colour `(0, 0x30, 0x65EC60[+2] << 3)`, slot 3 (0x38); when `Frame_Counter & 7` is 0 (read after the commit) `+2` = `(+2 + 1) & 3`. Byte for byte E5G's `EffectKind18Sub36_Pulse` but the table. Its index `+2` is unbounded (room four) |
| `0x50F820` | `EffectKind18Sub3D_Ripple` | nothing while `Cond_ByteFE` is 0. Otherwise 35 rows from the leader's cell row (the high word of `Field_Kind2Z`, `0x905E62`) - 0x14, the row's phase `k` = 0, 3, .. 0x66: `a = (sin(((F + k) << 7)) >> 10) - (sin(((F + k) << 7) - 0x80) >> 10)`, `b = (sin((F + k + 3) << 7) >> 10) - (sin((F + k + 2) << 7) >> 10)`, each angle `& 0xFFF`, `F` `Frame_Counter` read before each `Math_Sin`; then 35 columns from the leader's cell column (`0x905E66`) - 0x14: every cell inside the map (`AreaMap_Header`'s width and height bytes, signed tests) gets its `AreaMap_Corners` bytes 0 and 1 raised by `a`, 2 and 3 by `b` (byte adds). Then the leader's height word `ObjTrio +0x3E` from `AreaMap_Elevation` at its point and `MapView_Redraw` = 2. `Sprite_Current` is not read |

### 1.5 Sub-kinds 0x40, 0x41, 0x44

| PC | Name | What |
|---|---|---|
| `0x50FA00` | `EffectKind18Sub40_Run` | dispatcher (hidden in `0x50F590`) |
| `0x50FA20` | `_WaitCue` (0) | `MoveScript_Var7` (the chapter's run byte `0x8034E4`) 0xC and the step byte `0x8034E5` 2: sound 0x202, `+2` up. Nothing drawn |
| `0x50FA50` | `_WaitOne` (1) | `Cond_ByteFE` 1: `+2` up and the draw at step 0; **otherwise nothing drawn** |
| `0x50FA70` | `_WaitTwo` (2) | `Cond_ByteFE` 2: sounds 0x206 then 0x203, `+2` up. The draw at step 0 |
| `0x50FAB0` | `_Move` (3) | `+9` up; `Cond_ByteFE` 0: `Effect_Release`. The draw at step `+9` (read after the release) |
| `0x50FD30` | `EffectKind18Sub41_Run` | dispatcher (hidden in `0x50FAE0`) |
| `0x50FD50` | `_Start` (0) | `Cond_ByteFE` = 2, `+2` up |
| `0x50FD60` | `_WaitOne` (1) | `Cond_ByteFE` 1: `+9` = 0, `+2` up, the draw (0, 0); otherwise nothing drawn |
| `0x50FD90` | `_Turn` (2) | the draw (variant `+9 > 0xB`, angle `0x65EC80[+9] << 4`); `+9` up; past 0xF: sound 0x200 when story flag 0x4F is set (`Flags_Test(0x904030, 0x4F)`), else 0x201; `+0xA` = 0, `+2` up |
| `0x50FE20` | `_Hold` (3) | while `+9` is not 0: the draw (1, `0x65EC80[+9] << 4`) and `+9` up; at 0x17: story flag 0x50 set, `MoveCmd_TestFB(0x13, 0x25)`, `+9` = 0 (and so no draw from then on). While flag 0x4F is set: Capcom's `0x5100B0(0x65ECAC[+0xA] << 2, 0x65ECC8[+0xA >> 1])` when `+0xA` < 0x19, then `0x5101C0(+0xA)`. `+0xA` up; past 0x30 a tail jump to `Effect_Release` |
| `0x510C20` | `EffectKind18Sub44_Run` | dispatcher (hidden, its catalog host Capcom's `0x510BB0`) |
| `0x510C40` | `_Start` (0) | `Sprite_SetAnimationBank(0x2A8)`, `+0x24` = 0, `Sprite_SetAnimation(0)`, `+0x29` = 6, `+0x48` = 0, `+2` up (each read afresh) |
| `0x510C80` | `_Step` (1) | `call` E6C's `0x510C90`, `jmp` E6C's `0x510EB0` |

## 2. Divergence

**DIV-0041's fill** (`docs/widescreen.md` section 5's table, `0x50F7B5`, the
last of its nine listed sites): `EffectKind18Sub54_Pulse` draws its POLY_F4's
left corners at `Widescreen_FillX()` and its right at `320 +
Widescreen_Fill()` - E5G's form for `0x50B4B5` - which is the original's
`0.0f` / `320.0f` until `Widescreen_ArmFills` has run and whenever the picture
is narrow. The fuzz runs before the arm (`Effect6B_Inject` sits before
`FishingText_Arm` / `Widescreen_ArmFills` in `inject_all.cpp`), so it compares
the original's 320 x 240. The table row in `widescreen.md` now says widened.
**No new ledger entry**: the coordinator amends DIV-0041's list. Otherwise no
divergence: each function is a faithful replacement. `widescreen.cpp`,
`cheats.cpp` and `DIVERGENCE.md` patch no byte inside the band (a grep of
every address; `DIVERGENCE.md` names only `0x50F7B5`, as the listed site).

## 3. The tables

**The sub-state tables** (`symbols.toml` `[[data]]`): each its own length to
the data that follows it, checked by hand against what the states store into
`+2` (`band_rows.py` read the same counts):

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind18Sub33_States` `0x65EBC0` | 5 | `0x65EBD4`, 0x33's cell pairs |
| `EffectKind18Sub34_States` `0x65EBD8` | 5 | `0x65EBEC`, 0x34's cell pairs |
| `EffectKind18Sub37_States` `0x65EBF4` | 4 | `0x65EC04`, 0x37's cell pairs; entry 3 is E6A's `0x50E1C0` |
| `EffectKind18Sub35_States` `0x65EC08` | 5 | `0x65EC1C`, 0x35's cell pairs |
| `EffectKind18Sub38_States` `0x65EC28` | 4 | `0x65EC38`, 0x38's cell pairs; entry 1 is `EffectKind18Sub37_Hold`, entry 3 `EffectKind18Sub35_Draw` |
| `EffectKind18Sub3B_States` `0x65EC40` | 5 | `0x65EC54`, 0x3B's lifts |
| `EffectKind18Sub40_States` `0x65EC64` | 4 | `0x65EC74`, 0x40's pane bytes |
| `EffectKind18Sub41_States` `0x65EC9C` | 4 | `0x65ECAC`, 0x41's first mark bytes |
| `EffectKind18Sub44_States` `0x65EE78` | 2 | `0x65EE80`, a dword that is not code |

**The data read in place** (raw in `effect_6b_callees.h`, not named in
`symbols.toml`; their bytes are not copied here): the panels' cell pairs -
`0x65EBD4`, `0x65EBEC`, `0x65EC04`, `0x65EC58` with room for two variants,
`0x65EC1C`, `0x65EC38` for four (the fourth all zero); 0x34's and 0x35's are
followed by an s8 pair (+1, -1) at `0x65EBF0` / `0x65EC24` that no code reads
(`pe_xref`: no reference), taken as the end of their room - 0x3B's lifts
`0x65EC54` (two s16) and its draw's signs `0x65EC5C` (room four); 0x54's four
blue steps `0x65EC60`; 0x40's three panes `0x65EC74`; 0x41's curve
`0x65EC80` (room 0x1C: 24 bytes used, four zero), its mark tables `0x65ECAC`
(0x19) and `0x65ECC8` (0xD), its four quads `0x65ECD8`, two variants' quad
orders `0x65ED08`, the textures `0x65ED10` and the turns `0x65ED20` (four
each). Ours aborts on an index past a table's room (section 7).

## 4. Cells and records

The panel states keep their slide in `+0x30` (a signed word), the axis in
`+8`, the cell in `+0x36` / `+0x3A`, the lift in `+0x3E`; `+9` counts
(0x37 / 0x38's hold, 0x40's step, 0x41's curve index) and `+0xA` is 0x41's
second count. Sub-kind 0x44 writes `+0x24`, `+0x29`, `+0x48` - sprite-record
fields (an animation state), on an effect record. Cells beyond the record:
the leader's point and height (`ObjTrio +0x34`, `+0x38`, `+0x3E`), the
leader's cell words `0x905E62` / `0x905E66`, `Cond_ByteFE` (read by 0x3D,
0x40, 0x41; written by 0x41), the chapter's run and step bytes, the story
flags `0x904030`, `AreaMap_Header` / `AreaMap_Corners`, `MapView_Redraw`.

## 5. The fuzz (`effect_6b_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_6b`, effect mode (`g.effect`; kind 0x18
for every clone), 4,000 rounds a function (`BOF3X_E6B_ONLY=<name>` runs the
clones whose name holds it). Shapes: 48 `kEffect` (the nine dispatchers'
`sub_span` their table's length, the pulse's 4), two `kCall`
(`EffectKind18Sub40_Draw`, `EffectKind18Sub41_Draw`). No function answers (no
`ret_mask`). The nine tables are `DataTable`s, swapped for recorders on both
sides. **Regions**: none beyond effect mode's standard ones - the records,
`Sprite_Current`, `ObjTrio`, `Prim_VertexScratch`, `Cond_ByteFE`
(`0x905E20..`), `Field_Kind2Z` / `X` and `MapView_Redraw` (`0x905E60..`), the
area block (`AreaMap_Header` and its first 8 KiB, the corners inside it), the
chapter bytes, `Cond_Flags` (the story flags), `Field_Request`,
`Frame_Counter`, the packet buffer. The variant, step, curve and quad tables
are the image's, read-only, left in place.

**Callees**: the standard and effect-standard rows for `AreaMap_Elevation`,
`Sound_PlayEffect`, `Math_Sin`, `Flags_Test`, `Flags_Set`, `MoveCmd_TestFB`,
`Effect_Release` (clears `+0..+4`), `Sprite_SetAnimationBank`,
`Sprite_SetAnimation`, the `Gte_*` (`Gte_RotTrans`, `Gte_RotMatrix`,
`Gte_MulMatrix0` filling their outs; `Gte_SetRotMatrix` hashing the matrix,
`Gte_SetTransMatrix` noting the translation - so the matrix built from the
angle is compared), `Gte_RotTransPers4`, `Gte_PrimDepths4_10`,
`Prim_SetTexture`, `Gfx_CommitPrim`, the `Gpu_*` (`Gpu_SetDrawMode`'s window
argument logged as a value: the RECT is in the packet buffer, compared), and
`kEffectStd`'s rows for Capcom's `0x5100B0` (two words) and `0x5101C0` (one).
None re-listed: no caller here needs another reading (`EffectGte_Project*`,
`Math_Cos`, `MapView_LinkPrimAt` are not called). **Listed by the group**:
its own draws called directly, by name - `EffectKind18Sub35_Draw` and
`EffectKind18Sub3B_Draw` with no argument, `EffectKind18Sub40_Draw` at one
whole word, `EffectKind18Sub41_Draw` at two (the callers push immediates or a
byte zero-extended); E6A's `0x50E1C0` raw at no argument; E6C's `0x510C90` and
`0x510EB0` raw as `kPhase` (they log `Sprite_Current`). The three draws
without arguments (ours and E6A's) log `Sprite_Current`, `+8` and its words
`+0x30..+0x3F` (`FxCurrent`), so a draw on the wrong record or before a write
it should follow shows.

**Seeds** (per function, after the harness's per-round fill): on all 20
records the slide `+0x30` one step from each end and either side of it (0,
0x10, 0x20, 0xC0, 0x100, -0x100, -0x20, ... each +-1), `+8` 0 half the time,
`+9` below 0x18; the current record's cell words small (0..0x7F) two times in
three; for a place state the variant inside its room and the cell the leader
is tested against taken from the variant's table (read from the image at run
time), the z cell 0 half the time; the leader about that cell seven times in
eight - a whole cell -3..+4 off and a fraction at the tests' boundaries (0, 1,
0x7FFF, 0x8000, 0x8001, 0xFFFF, random); `Field_Request` 0 half the time;
`Frame_Counter`'s low three bits 0 half the time; `Cond_ByteFE` 0..3 two times
in three; the chapter's run 0xC and its step 2 half the time each. Per
function: `_Hold`'s `+9` at 6..9, 0, 0xFF or any; `_Turn`'s `+9` at the
variant's (0xA..0xC) and the end's (0xE..0x10) boundaries; `_Hold`'s (0x41)
`+9` about 0x17 and `+0xA` about 0x19 and 0x30; the ripple's map 1..31 cells
each way (the area header's bytes - the harness compares the block's first 8
KiB) and the leader's cells about it and past both edges, `Cond_ByteFE` any
not 0 two times in three. **Arguments**: 0x40's step a byte two times in
three, else any word; 0x41's variant 0 or 1, the angle a curve value
(`< 0x42 << 4`) half the time, else any word. **Disturbance** (the group's,
from the hash only): the slide at or about 0x100 / any, `+8`, the leader's x
or z, a cell word, `+9` (below 0x18), `+0xA`, `Cond_ByteFE` (0..3).

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_6b`,
exit 0): 200,000 rounds over 50 functions, 1,056,102 calls to the stand-ins,
**0 mismatches**; 24,756 bytes of state in 45 regions; 398 stand-ins. Every
entry of the nine tables reached (each handler recorder 775..2,016 calls);
E6A's `0x50E1C0` 45,027, E6C's two 4,000 each, `0x5100B0` 898, `0x5101C0`
2,674, `Math_Sin` 528,220, `AreaMap_Elevation` 19,773, `Effect_Release`
2,118, `Flags_Test` 5,272, `Flags_Set` and `MoveCmd_TestFB` 509 each.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` - 714
self-test lines, every one 0 mismatches, `inject: 8499 ours, 0 left
original`; `effect_6b` there 200,000 rounds, 1,057,173 calls, 0 mismatches.
The first run's launcher was stopped by this session's tool timeout (not a
Fatal); its game process (pid 21440, ours) ran on to the end and wrote the
`inject:` line, so the exit code of that run is not recorded. **With
`BOF3X_WIDE=1`**: `'*'` exit 0, 714 self-test lines, no mismatch, the same
counts. `ledger_check`: 72 entries, 0 errors (8,500 impl lines, 8,500
functions detoured). Neither run died silently.

## 6. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 50 (7,083 bytes against the cut's
  7,419: 43 differ by padding only, none by code); each extent checked by
  hand to its `ret` or tail `jmp`. No shared tail, no case of a switch, no
  second entry among them; the tool lists no code of the band no list has.
- **Hidden starts**: 46, each an entry by address - a cell of
  `EffectKind18_States` or of a sub-kind's table - not a case. Their recorded
  hosts: E6A's `0x50E1C0` (22 of them; its `entries_logic` extent `D0B` spans
  them, its code ends at `0x50E3FA`), this band's `0x50EED0`, `0x50F590`,
  `0x50FAE0`, whose catalog extents (`6BB`, `54E`, `423`) span the hidden
  starts after their `ret` (their code is 0x23B, 0x1E2, 0x249), and Capcom's
  `0x510BB0` (`DA`; its code ends at `0x510C17`). None of the hosts contains
  another's code as a fall-through.
- **The sub-kind dispatchers**: every `EffectKind18_States` cell pointing
  into the band is one of this group's rows (`[0x33]`, `[0x34]`, `[0x35]`,
  `[0x37]`, `[0x38]`, `[0x3B]`, `[0x3D]`, `[0x40]`, `[0x41]`, `[0x44]`,
  `[0x54]`).
- **The draws as states**: `0x50EED0` is both a callee and
  `EffectKind18Sub38_States[3]`; it takes no argument and reads only
  `Sprite_Current`, so it is one function, fuzzed as a state.
- **Cells that are not pointers**: `band_rows.py` lists `.data` cells holding
  `0x0050FE20` (`0x6001E0`) and `0x0050FF10` (18 cells from `0x61D430`).
  Each sits among s16 pairs (`0xFE10 0x00D0`, `0x00E0 0x0090`, ...): two
  coordinates whose bits match the address, not a pointer. Nothing reaches
  `0x50FE20` or `0x50FF10` through them.
- **The `hypothesis` row** `0x50FD30` is `EffectKind18_States[0x41]`, a
  sub-kind dispatcher: effect code, taken.
- **The cut's columns**: the unit hints (the nine tables and
  `EffectKind18_States`) are right; the "(7)", "(6)" counts are rows per hint.
  The `part5` rows' "Area overlays, world 2" label is the PSX twins' overlay
  (AREA106), not an owner.

## 7. Controls

Planted one at a time by a scratch script (`controls.py`: each plant anchored
on a unique string of `effect_6b.cpp` or `effect_6b_callees.h`, rebuilt, run
under `BOF3X_E6B_ONLY=<filter>`, the file restored and rebuilt at the end;
the committed file has no switch). The harness logs every differing round;
the column is the first (0-based) in this worktree; every refused run exited
3 on a `MISMATCH` line. **112 of 112 refused** in one run (one anchor, 87, matched a comment too and
was re-anchored and run alone). 18 and 32 drop a `Sprite_Current` read after
a call (the disturbance moves it): refused at rounds 835 and 24, the late
end, as E5G found for its 14. 107..109 aim a state at the wrong draw or
test; 112 drops the matrix pop. No equivalent mutant was planted.

| # | Run (`_ONLY`) | Plant | Refused at |
|--:|---|---|---|
| 1 | `Sub33_Run` | `AddressOf(EffectKind18Sub34_States), EffectKind18Sub34_States_count` for `AddressOf(EffectKind18Sub33_States), EffectKind18Sub33_States_count` | round 0 |
| 2 | `Sub34_Run` | `AddressOf(EffectKind18Sub33_States), EffectKind18Sub33_States_count` for `AddressOf(EffectKind18Sub34_States), EffectKind18Sub34_States_count` | round 0 |
| 3 | `Sub37_Run` | `AddressOf(EffectKind18Sub38_States), EffectKind18Sub38_States_count` for `AddressOf(EffectKind18Sub37_States), EffectKind18Sub37_States_count` | round 0 |
| 4 | `Sub35_Run` | `AddressOf(EffectKind18Sub3B_States), EffectKind18Sub3B_States_count` for `AddressOf(EffectKind18Sub35_States), EffectKind18Sub35_States_count` | round 0 |
| 5 | `Sub38_Run` | `AddressOf(EffectKind18Sub37_States), EffectKind18Sub37_States_count` for `AddressOf(EffectKind18Sub38_States), EffectKind18Sub38_States_count` | round 0 |
| 6 | `Sub3B_Run` | `AddressOf(EffectKind18Sub35_States), EffectKind18Sub35_States_count` for `AddressOf(EffectKind18Sub3B_States), EffectKind18Sub3B_States_count` | round 0 |
| 7 | `Sub40_Run` | `AddressOf(EffectKind18Sub41_States), EffectKind18Sub41_States_count` for `AddressOf(EffectKind18Sub40_States), EffectKind18Sub40_States_count` | round 0 |
| 8 | `Sub41_Run` | `AddressOf(EffectKind18Sub40_States), EffectKind18Sub40_States_count` for `AddressOf(EffectKind18Sub41_States), EffectKind18Sub41_States_count` | round 0 |
| 9 | `Sub44_Run` | `AddressOf(EffectKind18Sub41_States), 2u` for `AddressOf(EffectKind18Sub44_States), EffectKind18Sub44_States_count` | round 0 |
| 10 | `Sub33_Place` | `kSub33Cells, 0, at::kRoom2, 0xF0` for `kSub33Cells, 0, at::kRoom2, 0x100` | round 9 |
| 11 | `Sub34_Place` | `at::kSub33Cells, 0, at::kRoom2` for `at::kSub34Cells, 0, at::kRoom2` | round 0 |
| 12 | `Sub35_Place` | `kRoom4, 0xFF10, &Draw35` for `kRoom4, 0xFF00, &Draw35` | round 9 |
| 13 | `Sub3B_Place` | `at::kSub3BCells, at::kSub3BHeights + 2,` for `at::kSub3BCells, at::kSub3BHeights,` | round 0 |
| 14 | `Sub33_Place` | `s[8] = Word(s + 0x3A) == 0 ? 1 : 2; SetWord(S() + 0x36, At(cells` for `s[8] = Word(s + 0x3A) == 0 ? 1 : 0; SetWord(S() + 0x36, At(cells` | round 2 |
| 15 | `Sub34_Place` | `SetWord(S() + 0x3A, At(cells + 2 * v)[0]);` for `SetWord(S() + 0x3A, At(cells + 2 * v + 1)[0]);` | round 0 |
| 16 | `Sub35_Place` | `SetWord(s + 0x30, open); S()[2] = 2;` for `SetWord(s + 0x30, open); S()[2] = 3;` | round 9 |
| 17 | `Sub3B_Place` | `s = S(); if (LeaderAt(s, s[8] == 0, 0x8000, 0x10000)) { SetWord(s + 0x30, open);` for `s = S(); if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) { SetWord(s + 0x30, open);` | round 19 |
| 18 | `Sub33_WaitNear` | `SH_CALL(Sound_PlayEffect)(at::kSoundOpen);` for `SH_CALL(Sound_PlayEffect)(at::kSoundOpen); s = S();` | round 835 |
| 19 | `Sub34_WaitNear` | `if (LeaderAt(s, s[8] != 0, 0x8000, 0xF000)) { if (Field_Request == 0)` for `if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) { if (Field_Request == 0)` | round 10 |
| 20 | `Sub35_WaitFar` | `!LeaderAt(s, s[8] != 0, 0x20000, 0x1F000)` for `!LeaderAt(s, s[8] != 0, 0x20000, 0x20000)` | round 22 |
| 21 | `Sub3B_WaitNear` | `return Abs(b + 0x10000u) <= second;` for `return Abs(b - 0x10000u) <= second;` | round 10 |
| 22 | `Sub33_WaitFar` | `<< 16) \| 0x4000u); }` for `<< 16) \| 0x8000u); }` | round 5 |
| 23 | `Sub33_Open` | `if (Slide(0x20) >= 0xF0) S()[2] = static_cast<unsigned char>(S()[2] + 1); DrawE6A(); } // original 0x50E640` for `if (Slide(0x20) >= 0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1); DrawE6A(); } // original 0x50E640` | round 2 |
| 24 | `Sub34_Close` | `Slide(0xFFE0u) <= 1) Shut(); DrawE6A(); } // Sub-kind 0x37` for `Slide(0xFFE0u) <= 0) Shut(); DrawE6A(); } // Sub-kind 0x37` | round 91 |
| 25 | `Sub33_Close` | `Slide(0xFFF0u) <= 0) Shut(); DrawE6A(); } // Sub-kind 0x34` for `Slide(0xFFE0u) <= 0) Shut(); DrawE6A(); } // Sub-kind 0x34` | round 0 |
| 26 | `Sub3B_Close` | `if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(at::kSoundShut); S()[2] = 2;` for `if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(at::kSoundShut); S()[2] = 1;` | round 1 |
| 27 | `Sub37_Place` | `SetWord(S() + 0x30, 0x110);` for `SetWord(S() + 0x30, 0x100);` | round 0 |
| 28 | `Sub37_Place` | `SetWord(S() + 0x3E, 0xFF00); SetWord(S() + 0x30, 0x100);` for `SetWord(S() + 0x3E, 0xFF80); SetWord(S() + 0x30, 0x100);` | round 0 |
| 29 | `Sub37_Hold` | `if (s[9] >= 9) {` for `if (s[9] >= 8) {` | round 0 |
| 30 | `Sub37_Hold` | `s[9] = static_cast<unsigned char>(s[9] + 2); }` for `s[9] = static_cast<unsigned char>(s[9] + 1); }` | round 0 |
| 31 | `Sub37_Close` | `if (Slide(0xFFF0u) <= 0) Shut(); DrawE6A();` for `if (Slide(0xFFF0u) <= 0) ShutOn(); DrawE6A();` | round 1 |
| 32 | `Sub37_Close` | `SH_CALL(Sound_PlayEffect)(at::kSoundShut);` for `SH_CALL(Sound_PlayEffect)(at::kSoundShut); s = S();` | round 24 |
| 33 | `Sub35_Open` | `Slide(0xFFE0u) <= -0xE0` for `Slide(0xFFE0u) <= -0x100` | round 29 |
| 34 | `Sub35_Close` | `if (Slide(0x20) >= 0x20) Shut();` for `if (Slide(0x20) >= 0) Shut();` | round 3 |
| 35 | `Sub35_Draw` | `0x41u - Half(h)); h = SH_CALL(AreaMap_Elevation)(x0, z0);` for `0x40u - Half(h)); h = SH_CALL(AreaMap_Elevation)(x0, z0);` | round 0 |
| 36 | `Sub35_Draw` | `\| 0x151011Fu` for `\| 0x151010Fu` | round 0 |
| 37 | `Sub35_Draw` | `<< 7) + Word(s + 0x30) - 0x3F40u;` for `<< 7) - Word(s + 0x30) - 0x3F40u;` | round 1 |
| 38 | `Sub35_Draw` | `const long x1 = Ground(v + 0x10), z1 = Ground(v + 0x12);` for `const long x1 = Ground(v + 8), z1 = Ground(v + 0xA);` | round 0 |
| 39 | `Sub35_Draw` | `& 0xFFFFu) >> 1); }` for `& 0xFFFFu) / 2); }` | round 0 |
| 40 | `Sub35_Draw` | `<< 7) - Word(s + 0x30) - 0x3F40u;` for `<< 7) + Word(s + 0x30) - 0x3F40u;` | round 0 |
| 41 | `Sub35_Draw` | `SH_CALL(Gfx_CommitPrim)(6, 0x44); } // Sub-kind 0x38` for `SH_CALL(Gfx_CommitPrim)(6, 0x48); } // Sub-kind 0x38` | round 0 |
| 42 | `Sub38_Place` | `if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {` for `if (LeaderAt(s, true, 0x8000, 0x10000)) {` | round 45 |
| 43 | `Sub38_Place` | `SetWord(s + 0x30, 0xFF10);` for `SetWord(s + 0x30, 0xFF00);` | round 9 |
| 44 | `Sub38_Place` | `SetWord(s + 0x30, 0); S()[2] = 2;` for `SetWord(s + 0x30, 0); S()[2] = 3;` | round 0 |
| 45 | `Sub38_Close` | `if (Slide(0x20) >= 0) ShutOn();` for `if (Slide(0x10) >= 0) ShutOn();` | round 0 |
| 46 | `Sub3B_Open` | `Slide(0x10) >= 0xB0` for `Slide(0x10) >= 0xC0` | round 18 |
| 47 | `Sub3B_Close` | `if (Slide(0xFFE0u) <= 0) Shut(); Draw3B();` for `if (Slide(0xFFF0u) <= 0) Shut(); Draw3B();` | round 0 |
| 48 | `Sub3B_Draw` | `\| texture \| 0x12500000u` for `\| texture \| 0x12510000u` | round 0 |
| 49 | `Sub3B_Draw` | `U texture = 0x10D;` for `U texture = 0x10C;` | round 0 |
| 50 | `Sub3B_Draw` | `SetWord(v + 0xC, Word(s + 0x3E) - 0x100u);` for `SetWord(v + 0xC, Word(s + 0x3E) - 0x180u);` | round 0 |
| 51 | `Sub3B_Draw` | `SignedByte(At(at::kSub3BSides)[1 - side])` for `SignedByte(At(at::kSub3BSides)[side])` | round 0 |
| 52 | `Sub3B_Draw` | `Word(s + 0x30) * sign + (static_cast<U>(Word(s + 0x3A)) << 7) - 0x4000u;` for `Word(s + 0x30) * sign + (static_cast<U>(Word(s + 0x3A)) << 7) - 0x4040u;` | round 0 |
| 53 | `Sub54_Pulse` | `p[5] = 0x31;` for `p[5] = 0x30;` | round 0 |
| 54 | `Sub54_Pulse` | `At(at::kSub54Blues)[step] << 2` for `At(at::kSub54Blues)[step] << 3` | round 0 |
| 55 | `Sub54_Pulse` | `(Frame_Counter & 3) == 0` for `(Frame_Counter & 7) == 0` | round 45 |
| 56 | `Sub54_Pulse` | `SetUL(p + 0x2C, left);` for `SetUL(p + 0x2C, right);` | round 0 |
| 57 | `Sub3D_Ripple` | `if (Cond_ByteFE == 1) return;` for `if (Cond_ByteFE == 0) return;` | round 2 |
| 58 | `Sub3D_Ripple` | `- 0x40u) & 0xFFFu` for `- 0x80u) & 0xFFFu` | round 0 |
| 59 | `Sub3D_Ripple` | `c[2] = static_cast<unsigned char>(c[2] + a);` for `c[2] = static_cast<unsigned char>(c[2] + b);` | round 6 |
| 60 | `Sub3D_Ripple` | `if (col < 0 \|\| col > width) continue;` for `if (col < 0 \|\| col >= width) continue;` | round 6 |
| 61 | `Sub3D_Ripple` | `MapView_Redraw = 1;` for `MapView_Redraw = 2;` | round 0 |
| 62 | `Sub3D_Ripple` | `S16(At(at::kLeaderCellZ)) - 0x13;` for `S16(At(at::kLeaderCellZ)) - 0x14;` | round 6 |
| 63 | `Sub3D_Ripple` | `for (U k = 0; k < 0x66; k += 3, ++row)` for `for (U k = 0; k < 0x69; k += 3, ++row)` | round 0 |
| 64 | `Sub3D_Ripple` | `(Frame_Counter + k + 1) << 7` for `(Frame_Counter + k + 2) << 7` | round 0 |
| 65 | `Sub40_WaitCue` | `!= 0xC \|\| At(at::kStep)[0] != 3` for `!= 0xC \|\| At(at::kStep)[0] != 2` | round 5 |
| 66 | `Sub40_WaitCue` | `Sound_PlayEffect)(at::kSoundEnd);` for `Sound_PlayEffect)(at::kSoundCue);` | round 5 |
| 67 | `Sub40_WaitOne` | `if (Cond_ByteFE != 1) return; S()[2] = static_cast<unsigned char>(S()[2] + 1); SH_CALL(EffectKind18Sub40_Draw)` for `if (Cond_ByteFE != 1) return; S()[2] = static_cast<unsigned char>(S()[2] + 1); SH_CALL(EffectKind18Sub40_Draw)` | round 11 |
| 68 | `Sub40_WaitTwo` | `SH_CALL(Sound_PlayEffect)(at::kSoundEnd); SH_CALL(Sound_PlayEffect)(at::kSoundEndA);` for `SH_CALL(Sound_PlayEffect)(at::kSoundEndA); SH_CALL(Sound_PlayEffect)(at::kSoundEnd);` | round 2 |
| 69 | `Sub40_WaitTwo` | `if (Cond_ByteFE == 3) {` for `if (Cond_ByteFE == 2) {` | round 0 |
| 70 | `Sub40_Move` | `SH_CALL(EffectKind18Sub40_Draw)(S()[9] + 1);` for `SH_CALL(EffectKind18Sub40_Draw)(S()[9]);` | round 0 |
| 71 | `Sub40_Move` | `if (Cond_ByteFE == 1) SH_CALL(Effect_Release)();` for `if (Cond_ByteFE == 0) SH_CALL(Effect_Release)();` | round 3 |
| 72 | `Sub40_Draw` | `SetWord(rect, 0x61);` for `SetWord(rect, 0x60);` | round 0 |
| 73 | `Sub40_Draw` | `const U height = 6 * f - 0x370u;` for `const U height = 6 * f - 0x380u;` | round 0 |
| 74 | `Sub40_Draw` | `SetWord(v, UL(v + 0x10) + 4 * f);` for `SetWord(v, UL(v + 0x10) + 8 * f);` | round 0 |
| 75 | `Sub40_Draw` | `if (i == 1) {` for `if (i == 2) {` | round 0 |
| 76 | `Sub40_Draw` | `p[0x35] = static_cast<unsigned char>((t[2] << 4) - 1);` for `p[0x35] = static_cast<unsigned char>((t[3] << 4) - 1);` | round 0 |
| 77 | `Sub40_Draw` | `SetWord(p + 0x16, 0x78C7);` for `SetWord(p + 0x16, 0x78C6);` | round 0 |
| 78 | `Sub40_Draw` | `SetWord(rect + 6, 0x101);` for `SetWord(rect + 6, 0x100);` | round 0 |
| 79 | `Sub40_Draw` | `const U z1 = ((static_cast<U>(t[1] + t[2]) << 5) + f) * 4 - 0x4040u;` for `const U z1 = ((static_cast<U>(t[1] + t[3]) << 5) + f) * 4 - 0x4040u;` | round 0 |
| 80 | `Sub41_Start` | `Cond_ByteFE = 3;` for `Cond_ByteFE = 2;` | round 0 |
| 81 | `Sub41_WaitOne` | `S()[9] = 1; S()[2]` for `S()[9] = 0; S()[2]` | round 11 |
| 82 | `Sub41_WaitOne` | `SH_CALL(EffectKind18Sub41_Draw)(0, 1);` for `SH_CALL(EffectKind18Sub41_Draw)(0, 0);` | round 11 |
| 83 | `Sub41_Turn` | `c > 0xA ? 1u : 0u` for `c > 0xB ? 1u : 0u` | round 5 |
| 84 | `Sub41_Turn` | `if (S()[9] <= 0xE) return;` for `if (S()[9] <= 0xF) return;` | round 0 |
| 85 | `Sub41_Turn` | `FlagSet(0x4F) ? at::kSoundShut : at::kSoundOpen` for `FlagSet(0x4F) ? at::kSoundOpen : at::kSoundShut` | round 2 |
| 86 | `Sub41_Turn` | `S()[0xA] = 1; S()[2]` for `S()[0xA] = 0; S()[2]` | round 2 |
| 87 | `Sub41_Turn` | `<< 12) / 128);` for `<< 12) / 256);` | round 0 |
| 88 | `Sub41_Hold` | `if (S()[9] == 0x16) {` for `if (S()[9] == 0x17) {` | round 0 |
| 89 | `Sub41_Hold` | `SH_CALL(MoveCmd_TestFB)(0x25, 0x13);` for `SH_CALL(MoveCmd_TestFB)(0x13, 0x25);` | round 0 |
| 90 | `Sub41_Hold` | `if (d < 0x18) {` for `if (d < 0x19) {` | round 4 |
| 91 | `Sub41_Hold` | `At(at::kSub41MarkB)[d >> 2]` for `At(at::kSub41MarkB)[d >> 1]` | round 0 |
| 92 | `Sub41_Hold` | `at::kWaveStep)(S()[0xA] + 1u);` for `at::kWaveStep)(S()[0xA]);` | round 0 |
| 93 | `Sub41_Hold` | `if (S()[0xA] > 0x2F)` for `if (S()[0xA] > 0x30)` | round 23 |
| 94 | `Sub41_Hold` | `SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x4F);` for `SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x50);` | round 0 |
| 95 | `Sub41_Hold` | `SH_CALL(EffectKind18Sub41_Draw)(0, static_cast<int>(angle));` for `SH_CALL(EffectKind18Sub41_Draw)(1, static_cast<int>(angle));` | round 0 |
| 96 | `Sub41_Draw` | `SetWord(point + 2, 0xD250);` for `SetWord(point + 2, 0xD240);` | round 0 |
| 97 | `Sub41_Draw` | `SetWord(angles + 2, static_cast<U>(angle));` for `SetWord(angles + 2, 0u - static_cast<U>(angle));` | round 0 |
| 98 | `Sub41_Draw` | `(0u - b[3 * j + 1]) << 7` for `(0u - b[3 * j + 2]) << 7` | round 0 |
| 99 | `Sub41_Draw` | `& 0xFFFFF000u;` for `& 0xFFFFF800u;` | round 0 |
| 100 | `Sub41_Draw` | `At(at::kSub41Order + 4 * variant)[3 - i]` for `At(at::kSub41Order + 4 * variant)[i]` | round 0 |
| 101 | `Sub41_Draw` | `Gfx_CommitPrim)(5, 0x44); } SH_CALL(Gte_PopMatrix)();` for `Gfx_CommitPrim)(5, 0x48); } SH_CALL(Gte_PopMatrix)();` | round 0 |
| 102 | `Sub44_Start` | `SH_CALL(Sprite_SetAnimationBank)(0x2A9);` for `SH_CALL(Sprite_SetAnimationBank)(0x2A8);` | round 0 |
| 103 | `Sub44_Start` | `S()[0x29] = 7;` for `S()[0x29] = 6;` | round 0 |
| 104 | `Sub44_Step` | `scenario_harness::Phase(at::kE6CTail)(); scenario_harness::Phase(at::kE6CStep)();` for `scenario_harness::Phase(at::kE6CStep)(); scenario_harness::Phase(at::kE6CTail)();` | round 0 |
| 105 | `Sub33_WaitNear` | `kSoundOpen = 0x204;` for `kSoundOpen = 0x200;` (`effect_6b_callees.h`) | round 10 |
| 106 | `Sub33_Place` | `kSub33Cells = 0x65EBEC;` for `kSub33Cells = 0x65EBD4;` (`effect_6b_callees.h`) | round 0 |
| 107 | `Sub37_Close` | `if (Slide(0xFFF0u) <= 0) ShutOn(); Draw35();` for `if (Slide(0xFFF0u) <= 0) ShutOn(); DrawE6A();` | round 0 |
| 108 | `Sub35_WaitFar` | `EffectKind18Sub35_WaitFar(void) { WaitFar(&Draw3B); }` for `EffectKind18Sub35_WaitFar(void) { WaitFar(&Draw35); }` | round 0 |
| 109 | `Sub34_WaitFar` | `EffectKind18Sub34_WaitFar(void) { WaitNear(&DrawE6A); }` for `EffectKind18Sub34_WaitFar(void) { WaitFar(&DrawE6A); }` | round 0 |
| 110 | `Sub3B_Draw` | `SetWord(v + 0x1C, Word(s + 0x3E)); SetWord(v + 0x14, Word(s + 0x3E) + 1u);` for `SetWord(v + 0x1C, Word(s + 0x3E)); SetWord(v + 0x14, Word(s + 0x3E));` | round 0 |
| 111 | `Sub3D_Ripple` | `SetWord(At(at::kLeaderHeight), static_cast<U>(h) + 1u);` for `SetWord(At(at::kLeaderHeight), static_cast<U>(h));` | round 0 |
| 112 | `Sub41_Draw` | `}` for `SH_CALL(Gte_PopMatrix)(); }` | round 0 |

## 8. Latent defects (Capcom's, described, not fixed)

- **Unchecked indexes** (D200): the nine dispatchers do not bound `+2` (every writer
  in the band keeps it inside its table); the variant index is the spawn's x
  cell, s16 and unchecked, into cell tables with room for two or four - a
  spawn whose x cell is past them reads the next table's bytes (state-table
  code pointers for 0x33 and 0x37) as a cell; 0x54's `+2` into four bytes
  (kept 0..3 by its own `& 3`, but read before that wrap); 0x41's `+9` into
  its curve (room 0x1C; the states keep it below 0x18) and the draw's variant
  into two rows (its callers pass 0 or 1). Ours aborts past any.
- **The INT_MIN distance** (D212): the waiting states' `cdq; xor; sub` absolute value
  leaves `0x80000000` negative, so a leader exactly 0x8000 cells away counts
  as near. Unreachable on a map (cells are bytes); ours computes it the same
  way.
- **0x38's place tests the leader z first whatever `+8` says** (D238), where every
  other place and wait state picks the axis by `+8`: as read, perhaps a slip.
- **0x37's and 0x38's places draw nothing** (D203) in their first frame, and 0x40's
  `_WaitOne` / 0x41's `_WaitOne` draw nothing until their cue: as read.
- **0x41's `_Hold` stops drawing for good** (D203) once `+9` reaches 0x17 (it is set
  to 0, and only a non-zero `+9` draws); the record lives on to `+0xA` 0x31.
- **0x3D and 0x54 never end** (D202, D211): no state of them releases the record. 0x3D
  adds to the map's corner bytes every frame `Cond_ByteFE` is set (byte adds
  that wrap), so the corners drift by the running sum of the frames' `a` /
  `b`, which the sine differences keep oscillating rather than growing.
- **0x40's draw writes over its own argument slot** (D213) (`mov [esp + 0x1C],
  ecx`): harmless, the callers pop it unread.
- **No defect reads memory the original never wrote**: the projection's tenth
  argument (a flag local) and the matrix's pad words are never read by the
  callees; nothing here needs a ledger entry.

## 9. Calls across groups

**Outbound, raw** (`band_rows.py --edges`; `effect_6b_callees.h`):

| Callee | Group | Sites |
|---|---|---|
| `0x50E1C0` (void, the panel draw) | E6A (wave six) | 13: calls and tail jumps from 0x33's, 0x34's and 0x37's states; also `EffectKind18Sub37_States[3]` |
| `0x510C90`, `0x510EB0` (void) | E6C (wave six) | 2: `EffectKind18Sub44_Step`'s call and tail jump |
| `0x5100B0`, `0x5101C0` | **no group** (catalog part 7, "Unlabelled", in no list of this round) | 2: `EffectKind18Sub41_Hold` |

**Code in the band that is in no group**: Capcom's `0x5100B0` (0x101 bytes in
the catalog) and `0x5101C0` (0x467) - the two helpers 0x41's `_Hold` calls -
and `0x510630`, `0x510780`, `0x510BB0` between `0x5100B0` and `0x510C20`
(catalog part 7; `0x510BB0`, ending at `0x510C17`, sets a POLY_FT4's colour,
semi-transparency off, and its UVs and texture page bits from a word
argument, and is the recorded host of `0x510C20`). Not taken (not in the cut);
for the coordinator to place.

**By name, already ours**: `AreaMap_Elevation`, `Sound_PlayEffect`,
`Math_Sin`, `Flags_Test`, `Flags_Set`, `MoveCmd_TestFB`, `Effect_Release`,
`Sprite_SetAnimationBank`, `Sprite_SetAnimation`, `Gfx_CommitPrim`, the
`Gpu_*`, the `Gte_*`, `Prim_SetTexture`.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Group | Calls |
|---|---|---|
| `EffectKind18_Run` `0x46D830` through `EffectKind18_States` `[0x33]`, `[0x34]`, `[0x35]`, `[0x37]`, `[0x38]`, `[0x3B]`, `[0x3D]`, `[0x40]`, `[0x41]`, `[0x44]`, `[0x54]` | ours | the nine dispatchers, the ripple and the pulse, read in place |

No other code reaches the 50 (`band_rows.py`: every caller of a draw is this
group's own; the `.data` cells above are not pointers).

## 10. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 50 rows, and no first-call trace under
`analysis/calltrace` names any of the 50 (a grep of every file: the hits are
the extent lists `entries*.txt`, not traces). **Fuzz only.** No live run was
made (the brief). No spawner is known; the PSX twins put sub-kind 0x41 in the
overlay of area 106, so a route recorded through that area (if its effect list
holds sub-kind 0x41) would let the coordinator's frame-hash A/B cover it.

## 11. The rebinding

`grep -rn -i` of the 50 addresses and the nine tables in `src/game`
(`band_rows.py --refs`: 0 references). **Nothing to rebind**; nothing of
another group's names one of these. Left raw in this group's own files, for
the round's rebinding pass: E6A's `0x50E1C0` and E6C's `0x510C90` /
`0x510EB0` (`effect_6b_callees.h` `kE6ADraw`, `kE6CStep`, `kE6CTail`, and the
fuzz file's `E6B_RAW` rows) - each becomes a name when its group merges; the
`kEffectStd` rows for `0x5100B0` / `0x5101C0` stay raw (no group owns them).
`scenario_harness.cpp`'s `kEffectOverrides` / `kEffectStd` name none of the 50.

## 12. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-03): 46 lines, the read
extents of the hidden starts. The host lines `0050EED0 6BB`, `0050F590 54E`,
`0050FAE0 423` (ours now, code 0x23B, 0x1E2, 0x249) and `0050E1C0 D0B` (E6A's),
`00510BB0 DA` (no group) are left in place, as E5G left its hosts'.
`0050FF10 19A` was already exact.
