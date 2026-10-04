# Group E5D: effect kind 0x18's sub-kinds 0x14, 0x18, 0x19, 0x1B..0x1E, 0x21, 0x22, 0x43, 0x52, 0x68

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..15),
wave five, from the round branch's tip `42b2388`. **52 functions ours**
(`src/game/effect_5d.cpp`, shadow name `effect_5d`): the cut table's 49 rows
for E5D (`analysis/round13_cut.tsv`, the band `0x503DE0..0x506860`) and three
starts no list of the cut has - sub-kind 0x17's texture scroll `0x503E50`
(code no list has, which E5C's states tail-jump to) and the dispatchers of
sub-kinds 0x1C `0x505100` and 0x1D `0x505540` (`EffectKind18_States[28]` /
`[29]`, catalog part 2 rows no group of the round held; section 5). Each read
to its last instruction with capstone and fuzzed through the scenario harness
in effect mode ([`scenario_harness.md`](scenario_harness.md) section 8)
without edits to it: 208,000 rounds, 0 mismatches; 116 of 117 controls refused, the other an equivalent mutant whose near variant is refused. **Fuzz
only**: no recorded route enters any of the 52 (section 9).

Every row is effect code, the three `hypothesis` rows (`0x504F70`, `0x505AB0`,
`0x505E20`) among them - each a sub-kind's dispatcher. Kind 0x18
(`Effect_KindHandlers[0x18]`, ours: `EffectKind18_Run` `0x46D830`) jumps
through `EffectKind18_States` by `+1` to a sub-kind; each sub-kind here is a
dispatcher by `+2` through its own table, or a single state:

| Sub-kind (`EffectKind18_States[n]`) | Functions | What the code does |
|---|--:|---|
| 0x17 ([23], E5C's dispatcher `0x503660`, table `0x65E1A8`) | 4 helpers | a 4 x 4 patch of textured quads laid on the ground's elevation; a VRAM frame copy; a 16 x 32 texture scrolled by the record's point; the closing state (`0x65E1A8[15]`) |
| 0x18 ([24]) | 7 | a column of eight textured quads and a ring of 64 dots that grow, hold and shrink round the record's point, then release |
| 0x19 ([25]) | 1 | placed from a two-entry table, a fan of sixteen shaded `POLY_GT4` strips turning in the camera's frame when the camera is near |
| 0x68 ([104]) | 1 | when the camera is near (0x2D, 0x23): 256 dots in eight arcs through the GTE's own projection |
| 0x1B ([27]) | 1 | a timer: two `MoveCmd_TestFB` calls, then story flag 0x1C cleared and the record released |
| 0x43 ([67]) | 4 | a textured quad that grows while the counter `0x903848` is 0xE, then waits for 0x1C; its table's entries 4..8 are sub-kind 0x1C's states |
| 0x1C ([28]) | 8 | from a map cell: the cell's map byte cleared, a random wait, a column of eight shrinking squares rising (map byte 0x8A), held, falling, again |
| 0x1D ([29]) | 7 | 0x1C's column leaning along a direction chosen by the ground's slope; a party member standing one or two cells along it is hurt (`Field_FloorHurt(5)`) |
| 0x14 ([20]) / 0x1E ([30]) | 7 / 1 | a CLUT strip dimmed once area flag 0x3E is set; a checkerboard of shaded squares over one of two map rectangles until a flag of the rectangle's is set, then two effect records spawned (kinds 0x6E, 0x6D) and the squares faded. 0x1E's table is 0x14's from its entry 2 |
| 0x21 ([33]) | 3 | a screen-centred spiral of a `POLY_G3` and two `POLY_G4`s, its phase stepped |
| 0x52 ([82]) | 1 | sixteen rings of sixteen dots round the record's point, brightening, when the camera is within 20 cells |
| 0x22 ([34]) | 7 | a platform of textured pieces over one of four map rectangles that sinks and rises with a story flag, writing the map's bytes, corner dwords and heights as it moves |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`; the eight dispatchers `evidence`): "column", "ring", "fan",
"patch", "platform", "hurt" name the code's shape, not a play-tested fact.
What the game shows with these sub-kinds and where was not traced (section 9;
the owner's word, as the brief says). No spawner of kind 0x18 in `src/game`
names one of these sub-kinds (`EffectKind18_Start` sets `+1` from `+0xB`; the
spawners write `+0xB` from scripts or code not yet read for it). The PSX twins
`pairs_propagated.json` gives (`0x801F2C48..0x801F478C`) are AREA overlay
copies with no name in the sibling: the four that appear in its
`names/area_records.toml` (`0x801F2C48`, `0x801F2D04`, `0x801F2E28`,
`0x801F3630`) are other areas' handler entries at the same overlay load
address, not these functions.

## 1. What each function does

Ours is `src/game/effect_5d.cpp`; every function carries its original's
address and a one-paragraph reading, and `symbols.toml` the extent and the
reading. In outline:

### 1.1 Sub-kind 0x17's helpers (E5C's sub-kind)

- `EffectKind18Sub17_Close` `0x503DE0` (`0x65E1A8[15]`): `q = +9 / 5`; at 4,
  `MoveCmd_TestFC(0x43, 0x1A)` and `Effect_Release`; else every fifth frame
  `_CopyFrame(7 - q)`; `+9` up.
- `EffectKind18Sub17_ScrollTexture` `0x503E50`: four `Gpu_SetDrawMove` copies
  that scroll a 16 x 32 VRAM block at (0x260, 0x100) by bits of the record's
  x and z.
- `EffectKind18Sub17_DrawPatch(variant)` `0x503FA0`: with `Draw_PassFlags`
  bit 2 **set** (nothing otherwise), `MapView_ScreenXY` the record's point
  `>> 9` less 0x4000, then sixteen `POLY_FT4`s in a 4 x 4 grid of 128-unit
  cells, each corner `Ftol`'d on the x87 from the screen point +-64.0,
  elevated by `AreaMap_Elevation / 2` less `+0x32` times a height byte of the
  quad's record in `0x65E1E8` / 128; the record's flag byte picks the corner
  order and `Gte_StoreDepthF4` or `Gte_PrimDepths4_10`; the texture its flag,
  the variant's byte (bytes 5..7 of the record) and page 0x2180.. (variant 0)
  or 0x2580..; a draw mode and the quad each linked on the record's row.
- `EffectKind18Sub17_CopyFrame(frame)` `0x5043B0`: a 32 x 64 VRAM block from
  (0x240, 0x100) plus frame `frame`'s (u, v) pair (`0x65E268`, ten) to
  ((`+0xB` + 12) * 48, 0x100).

### 1.2 Sub-kind 0x18

- `EffectKind18Sub18_Run` `0x504430`: the dispatcher (`EffectKind18Sub18_States`, 4).
- `_Grow` / `_Hold` / `_Shrink` / `_End` (`0x504450`, `0x5044A0`, `0x5044F0`,
  `0x504550`): the column at `+9`, 7, then `(0x1F - +9) / 4`; the ring at `+9`
  while it is below 0x10; `+9` counted to 7, 0x1F, 0x18, 0x1C; the last a tail
  jump to `Effect_Release`.
- `EffectKind18Sub18_DrawColumn(grow, unused)` `0x504570`: eight `POLY_FT4`s
  stacked over the record's cell, each level 8 `grow` higher and 2 `grow`
  wider, texture `0xBA009124 | level << 19`. **The second argument is dead**:
  the code multiplies it into a value it stores back into the argument's own
  stack slot and reads nowhere else (the next pass multiplies by that), so it
  changes nothing the caller or the draw can see; ours does not compute it.
- `EffectKind18Sub18_DrawRing(radius)` `0x5047A0`: `MapView_ScreenXY` the
  record's projected point; 64 semi-transparent `TILE_1` dots on a ring of
  radius `4 radius` (`Math_Sin >> 14`, `Math_Cos >> 15`, 8.0 down).

### 1.3 Sub-kinds 0x19, 0x68, 0x1B

- `EffectKind18Sub19_Run` `0x504900`: at `+2` 0 the place (`+0xB` the x
  cell's byte, the point from `0x65E28C` / `0x65E290` / `0x65E294`), `+2` up;
  then, `Draw_PassFlags` bit 2 set and the camera's cell within 10 of the
  place's (less 6), a `jmp` to its tail `0x5049D0` (no other reference; its
  own frame, kept inside this function's extent and clone): the GTE matrix
  pushed, the camera matrix turned about z by `0x40 - |0x80 - 2 (frame &
  0x7F)|` and moved to the record's point (`Gte_RotTrans`, `Gte_RotMatrix`,
  `Gte_MulMatrix0`, `Gte_SetRotMatrix`, `Gte_SetTransMatrix`), sixteen
  `POLY_GT4` strips waved by `Math_Sin` and shaded by `Math_Cos` of the
  frame, `Gte_PrimDepths4_14`, popped.
- `EffectKind18Sub68_Draw` `0x504CE0`: `MapView_ScreenXY` (-10624.0,
  -11904.0) and eight arcs of 32 dots at `((frame & 0xF) + 16 k) * 5 / 65536`
  of `Math_Sin` / `Math_Cos` (unsigned shifts), `Gte_LoadVertex` / `Gte_Rtps` /
  `Gte_StoreScreenXY`; a draw mode every fourth arc.
- `EffectKind18Sub1B_Run` `0x504F00`: section 0's table.

### 1.4 Sub-kind 0x43

`_Run` `0x504F70` (`EffectKind18Sub43_States`, 9: `WeretigerFx_Next` (`+2`
up), `_Glow`, `WeretigerFx_Next`, `_End`, then sub-kind 0x1C's five);
`_Glow` `0x504F90` draws `_DrawQuad(+9, +9)` while the counter is 0xE and past
0x6C sets `Cond_ByteFE` 1 and the counter 0xF; `_End` `0x504FE0` waits for
0x1C; `_DrawQuad(grow, shade)` `0x505000` one quad, texture `0x25100100 +
((shade & 0xF8) << 16)`, committed to slot 5.

### 1.5 Sub-kinds 0x1C and 0x1D

`_Run` `0x505100` / `0x505540` (five each). `_Start` sets `+0x3E` the ground's
height and the wait `+0xA` from a four-byte table by the cell's parity (0x1C:
plus `Rand & 0x2F`, and the cell's map byte 0; 0x1D: the direction `(+0xC,
+0x10)` (0, 1) or (1, 0) by whether the ground half a cell along is higher).
`_Wait` (`0x5051A0`, `0x505610`, the same code) counts `+9` down while
`Field_Request` is 0 and at 0 plays sound 0x200 if
`EffectKind18Sub1C_OnScreen` (`0x505480`) answers 1 - the cell projected, both
screen floats in [-20.0, 340.0] (x87 compares; a NaN outside). `_Rise`,
`_Hold`, `_Fall` (0x1D's `_Hurt` in `_Hold`'s place) draw the column at
`+9`, 7 and `+9 >> 2`; 0x1C's set the map byte 0x8A and then 0. **The
columns** (`_Draw(rise)`, `0x5052D0` / `0x5057D0`) are eight squares at
screen points, half size `(level or 4) * rise / 2 + odd * rise / 16 + 4`,
texture `((0x1F80000 - level 0x400000 - odd << 21) / 4 / (8 - rise)) &
0xF80000 | 0xBA009124` - an **unchecked divide** (section 7). `_Hurt`
`0x505690` probes the cells one and two along the direction with
`Party_MemberAt`; on a member it points `Field_State` and `Sprite_Current` at
its `ObjTrio` record, and unless `Actor_EquipCount(+0x148, 3, 8)` answers,
flashes its CLUT (`Sprite_FlashClut(2)`, unless bit 5 of its
`Field_ActorStates` byte) and calls `Field_FloorHurt(5)`; `Sprite_Current` is
put back, `Field_State` is not.

### 1.6 Sub-kinds 0x14 and 0x1E

`_Run` `0x5059A0` (five) / `EffectKind18Sub1E_Run` `0x505AB0` (three, 0x14's
table from entry 2). `_Start` `0x5059C0`: area flag 0x3E (`0x903FD8`) set -
the strip at half and `Effect_Release`; `+2` up either way. `_Dim`
`0x5059F0`: once the flag is set, the strip at `0x80 - +9 / 2`, released at
0x80. `_ScaleClut(scale)` `0x505A40`: sixteen 15-bit colours at `0x80BCC0`
to `0x80FCC0` with red and blue scaled by `scale / 128`, green and bit 15
kept; `Gfx_ClutStripDirty` 1. `_WaitFlag` `0x505AD0`: the place's flag
(`0x65E31C`) set - sound 0x20A, a record made kind 0x6E at the place's point
(`0x65E30C`, height 0x7800000) and one kind 0x6D with `+6` the place; `+9` 0,
`+2` up; the tiles at full shade. `_Fade` `0x505BB0`: `+9` up on even
frames, `Effect_Release` (a call) past 0x7F, the tiles at `0x80 - +9`.
`_DrawTiles(shade, unused)` `0x505BF0`: over the place's rectangle
(`0x65E320`), every cell with odd column + row a semi-transparent square on
the plane z -0x340, half size `|3 - ((frame / 3 + column + row) & 7)| + 0x18`
(the corners on the x87, `(sx - h) + 2h` unrounded between), linked at the
cell; the second argument is not read.

### 1.7 Sub-kind 0x21

`_Run` `0x505E20` **calls** (not jumps) through `EffectKind18Sub21_States`
(two: `MagicFx_ClearCount9`, `_Step` `0x505E50`, `+9` up two) and then draws
`_DrawSpiral(+9)` `0x505E60`: sixteen steps, each a `POLY_G3` from (160.0,
120.0) and two `POLY_G4`s outward, corners `Math_Cos >> 4 / 3 + 160` and
`Math_Sin >> 5 / 3 + 120` at three radii, grey `|0x80 - n| / 2`, the rim's
red and blue `Math_Cos` of n's phases; between two draw modes.

### 1.8 Sub-kind 0x52

`EffectKind18Sub52_Draw` `0x506290`: section 0's table; the turn's sign by the
ring's parity from `0x65E330`; each dot through `Gte_RotTransPers` into its
own packet.

### 1.9 Sub-kind 0x22

`_Run` `0x5064A0` (five: `_Start`, `_Move`, `_WaitClear`, `_Move`,
`_WaitSet`). `_Start` `0x5064C0`: `+0x34`'s low word the place's story flag
(`0x65E348` by the place `+0x36`, four places), the height `+0x3E` from
`0x65E34C` (less 0x80 when the flag is set, `+2` 2; else `+2` 4). `_WaitClear`
/ `_WaitSet` turn on the flag's change (`+0x3A` 8 / -8, sounds 0x202 and
0x201), `_Move` steps `+0x3E` by `+0x3A` for 16 frames; each change calls
`_SetMap` `0x506640` and every sub-state ends in a tail jump to `_Draw`
`0x506860`. `_SetMap` writes the place's map rectangle (`0x65E354`): place
0's `AreaMap_Bytes` cells 0x11 (0 at sub-state 2), the others'
`AreaMap_Corners` dwords from `0x65E364`'s row by the flag (eleven bytes, a
running count), then every cell's height byte `AreaMap_Header +
AreaMap_HeightBase * 4 + cell` to `(-ground - 2 +0x3E) / 32`. `_Draw`: the
place's pieces (`0x65E38C`'s range of `0x65E3D0`'s 89 records), each a quad
from one of five shapes (`0x65E394`) about its cell over `+0x3E`, linked at
its cell with a row offset by its kind and the sub-state (`0x65E37C`), or,
negative, committed to slot 3.

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed. `cheats.cpp` and `widescreen.cpp` name no address of the band
(grepped 2026-10-03). **`DIVERGENCE.md` names one**: DIV-0041's list of culls
not moved for the wide view includes "the unnamed `0x5054E3` `[-20, 340]`" -
that is inside `EffectKind18Sub1C_OnScreen` `0x505480`, the `[-20.0, 340.0]`
test of `0x5C422C` / `0x5C4228`; ours keeps the original bounds, as the entry
says (the coordinator may now name it there). Where the original indexes one
of the image's tables past it by a record byte or word, jumps through a
sub-state table past its end, or divides by a count that can be 0, ours aborts
with a `Fatal` naming the function (the round-nine rule; section 7).

## 3. The tables and the arguments

**The sub-state tables** (`symbols.toml` `[[data]]`): each its own length,
checked by hand to the next table a dispatcher indexes or the first dword that
is not code (the tables overlap: two lie inside others):

| Table | Count | Ends at |
|---|--:|---|
| `EffectKind18Sub18_States` `0x65E27C` | 4 | bytes at `0x65E28C` (sub-kind 0x19's tables) |
| `EffectKind18Sub43_States` `0x65E2B8` | 9 | bytes at `0x65E2DC` (the waits); entries 4..8 are 0x1C's table |
| `EffectKind18Sub1C_States` `0x65E2C8` | 5 | `0x65E2DC`, as above |
| `EffectKind18Sub1D_States` `0x65E2E0` | 5 | bytes at `0x65E2F4` |
| `EffectKind18Sub14_States` `0x65E2F8` | 5 | `0x65E30C` (the spawn points); entries 2..4 are 0x1E's table |
| `EffectKind18Sub1E_States` `0x65E300` | 3 | `0x65E30C` |
| `EffectKind18Sub21_States` `0x65E328` | 2 | `0x65E330` (two sign bytes); called through, not jumped |
| `EffectKind18Sub22_States` `0x65E334` | 5 | bytes at `0x65E348` |

`EffectKind18_States` itself (ours, `count` 160, an upper bound) holds the
twelve entries 20, 24, 25, 27..30, 33, 34, 67, 82, 104 that point here; E5C's
`0x65E1A8` holds `EffectKind18Sub17_Close` at 15.

**The data tables** read in place (addresses in `effect_5d_callees.h`, not
named in `symbols.toml`; no bytes copied anywhere): the patch records
`0x65E1E8` (16 x 8), the frames `0x65E268` (10 pairs), sub-kind 0x19's
`0x65E28C` / `0x65E290` / `0x65E294` (two places; `0x65E298..0x65E2B7` is data
no code names), the waits `0x65E2DC` / `0x65E2F4`, sub-kind 0x14's
`0x65E30C` / `0x65E31C` / `0x65E320` (two places), `0x65E330`, sub-kind 0x22's
`0x65E348..0x65E697` (four places, 89 pieces), and the floats `0x5C41E4`
(64.0), `0x5C41CC` (8.0), `0x5C4228` (340.0), `0x5C422C` (-20.0).

**Arguments**: E5C's states push 0, 1 or 2 to `_DrawPatch` and 0..9 to
`_CopyFrame`, whole words. `MapView_LinkPrimAt` reads `dy`'s low byte (signed)
and `size`'s; `_Sub68_Draw` pushes `dy` as a `sete` over a register's upper
bytes (the arcs' draw mode) and as a whole stack word whose upper three bytes
it never wrote (the dots): harmless, and re-listed in the fuzz with byte
masks on both (section 4). Every other argument is a whole immediate, a zero-
extended byte or a pointer.

## 4. The fuzz (`effect_5d_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_5d`, effect mode (`g.effect`, kind 0x18),
4,000 rounds a function (`BOF3X_E5D_ONLY=<name>` runs the clones whose name
holds it, `BOF3X_E5D_ROUNDS` sets the rounds). 41 clones are `kEffect` (the
eight dispatchers with `sub_span` their table's length;
`EffectKind18Sub1C_OnScreen` with `ret_mask` 0xFFFFFFFF: its callers `test
eax, eax`), the eleven with arguments `kCall`. The six outer tables are
`DataTable`s, swapped for recorders on both sides (`WeretigerFx_Next` and
`MagicFx_ClearCount9` among the entries); the two inner ones are swapped with
them (listing a table twice would put recorders back on restore). **Regions**
beyond the standard: the CLUT strip `0x80BCC0` and its copy `0x80FCC0` (0x20
each). Everything else is standard: the records, `Prim_VertexScratch`,
`MapView_ScreenXY`, the camera cells `0x905E62` / `0x905E66`, `Cond_ByteFE`,
`Draw_PassFlags`, the counters, the flags, `ObjTrio`, `Field_State`,
`Field_ActorStates[0]`, `Gfx_ClutStripDirty`, the area block (header, corners,
heights, `AreaMap_Bytes`).

**Callees**: the effect-standard rows (`Effect_Release`, `Party_MemberAt`
0..2 / 0xFF, `Math_Cos` never 0 or -1, the draw primitives, `Gte_StoreDepthF`,
`Gte_LoadVertex`, `Gte_StoreScreenXY`, `Gpu_SetDrawMove`, ...), the field and
standard rows (`AreaMap_Elevation`, `AreaMap_SetByte` (u16, u16, u8),
`Flags_Test` / `_Clear` (byte index), `MoveCmd_TestFB` / `FC`, `Rand`,
`Sound_PlayEffect`, `Actor_EquipCount`, `Gfx_CommitPrim`, `Prim_SetTexture`,
`Gte_StoreDepthF4`). **Listed in the group**: its own twelve called directly
(the draws with whole-word masks, `_OnScreen` `kBool`, `_SetMap` / `_Draw`
`kPhase`). **Re-listed louder**: `Gte_RotTrans`, `Gte_RotMatrix`,
`Gte_MulMatrix0`, `Gte_SetRotMatrix`, `Gte_SetTransMatrix`,
`Gte_RotTransPers4` - the stack pointers never logged (sub-kind 0x19's
matrix and point are locals whose addresses differ between the copy and ours),
the vectors and matrices hashed (6 / 18 bytes), the outs filled;
`Gte_RotTransPers` - the screen point a third of the time at or one ulp either
side of `_OnScreen`'s -20.0 / 340.0, or a quiet NaN (the x87's unordered
compare), else fractional floats; `MapView_LinkPrimAt` - `dy` and `size` by
their low byte (section 3), the cursor moved two times in three;
`Effect_FindFree` - the effect-mode row's answers, but never
`Sprite_Current`'s own record: in the game the running record is in use, and
the harness's disturbance can leave `Sprite_Current` on a free one, whose
spawn would then overwrite the place word `_WaitFlag` reads again after it
(the first full run aborted on exactly that, an index of 8).

**Seeds** (every round, after the harness's fill): all 20 records' place word
`+0x36` below the places the function's tables hold (two; four for sub-kind
0x22), `+0xB` below sub-kind 0x19's two, `+9` a count that is neither 8 nor 8
after `>> 2` (the columns' divisor; below 40 for `_Close`, whose frame index
`7 - +9 / 5` goes negative past it); `Sprite_Current`'s `+9` at the compares'
boundaries; `+2` at 0, 2, 1, 3, 4 for the three functions that compare it (a
dispatcher's is the harness's, below its table); `Draw_PassFlags` bit 2 half
the time; `Field_Request` mostly 0; the counter at 0xE, 0x1C, 0xD, 0xF, 0x1B;
half the time the camera's cell at, 5, 10, 11 and 20 cells either side of the
place's (sub-kinds 0x19, 0x68, 0x52); the members' `+0x148` below 8 half the
time; for sub-kind 0x22's map writes `AreaMap_HeightBase` below 0x700 (so the
height bytes stay in the 8 KiB the harness compares - the widest cell is
`0x22 + 0x1F * 0x1F`); `_Hurt`'s direction at 0, 1, -1; the stored height at
its sign boundaries. **Args**: the variant below 3, the frame below 10, the
columns' rise never 8, the rest at their compares' boundaries. **Disturbance**
(the group's, from the hash only): a safe `+9`, `Field_Request`, the counter,
`+0x3E`, `+0x3A`, `Draw_PassFlags` bit 2.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_5d`,
exit 0): 208,000 rounds over 52 functions, 6,415,196 calls to
the stand-ins, **0 mismatches**; 24,820 bytes of state in 47 regions. Every
entry of the six tables reached (each handler recorder 424..3,022 calls;
`WeretigerFx_Next` 3,022, `MagicFx_ClearCount9` 1,997), the group's own
helpers 681..16,000 (`_OnScreen` 1,611), `Effect_FindFree` 5,292,
`Effect_Release` 7,018, `Party_MemberAt` 8,000 (`Field_FloorHurt` 1,657),
`Flags_Clear` 434, `MoveCmd_TestFB` 901, `_TestFC` 830, sub-kind 0x19's GTE
matrix calls 253 each.

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0,
706 self-test lines, none with a mismatch, `inject: 8155 ours, 0 left
original`; `effect_5d` there 208,000 rounds, 6,375,331 calls, 0 mismatches.
**With `BOF3X_WIDE=1`**: `'*'` exit 0, 706 self-test lines, none with a
mismatch. Neither run died silently. `tools/ledger_check.py`: 70 entries,
0 errors.

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the 49 to the byte (10,509 bytes against
  the cut's 11,181; 36 differ by padding only), with one difference by code:
  `0x503DE0`'s cut size 434 runs over `0x503E50`, a function of its own
  ("code no list has": its own frame and `ret`, reached only by E5C's five
  tail jumps). Taken as `EffectKind18Sub17_ScrollTexture`.
- **Added `0x505100` and `0x505540`**: the dispatchers of
  `EffectKind18_States[28]` and `[29]`, catalog rows ("Table
  EffectKind18_States", part 2) in no group of the round, inside the band, not
  flagged by the tool (they sit in its "padding" after `0x505000` and
  `0x505480`); their tables' states are rows of this group. Taken (the
  addendum's rule). `0x504F70`'s table `0x65E2B8` contains `0x505100`'s.
- **`0x504900`'s tail `0x5049D0`**: reached only by `0x504900`'s own `jmp`
  (an image-wide scan for calls, jumps and dwords naming it), so it stays
  inside `0x504900`'s extent and clone - a static helper in ours, not a
  function of its own. **`0x506860`** (`_Draw`) has its own frame and `ret`
  and five tail jumps: taken whole (it was a cut row).
- **Hidden starts**: 37 of the cut's, each an entry by address - a cell of
  `EffectKind18_States` or a sub-kind's table - none a case or a shared tail.
  `0x503DE0`'s recorded host `0x5032C0` is E5C's; the band's own hosts
  (`0x5043B0`, `0x5047A0`, `0x505000`, `0x505480`, `0x5057D0`, `0x505A40`,
  `0x505BF0`, `0x505E60`, `0x506640`) are functions of their own that end
  before the hidden starts their recorded extents covered.
- **The `hypothesis` rows** (`0x504F70`, `0x505AB0`, `0x505E20`): dispatchers
  - effect code, taken.
- **Data that looks like a band address**: `.data` / `.rdata` dwords at
  `0x5F0BEC`, `0x656F70`, `0x65704C`, `0x65AAB0`, `0x663260`, `0x663670` hold
  values inside the band that are not entries of any function here (mid-
  function or instruction boundaries no code jumps to); data, not references.
- **The cut's `unit` / `label` columns** ("Area overlays, world 2",
  "Fn_504F70 (EffectKind18_States[67])" for sub-kind 0x1C's states, ...) are
  the catalog's guesses; the code says the sub-kinds above.
- **Not taken, in no group**: nothing: the band holds no other code.

## 6. Controls

Planted in `effect_5d.cpp` one at a time by a script (scratch `controls.py`:
a unique anchor replaced, rebuild, run under `BOF3X_E5D_ONLY=<filter>`,
restore, rebuild; never with a commit in between), against the final fuzz.
Counts are rounds refused of 4,000 per function run, in this worktree; every
refused run exited 3. At least one plant a function, several for the draws;
each dispatcher sent to another table of at least its length or to the next
entry. **116 of 117 refused**; the one not refused is an equivalent mutant.

| # | Run (`_ONLY`) | Plant | Refused |
|--:|---|---|---|
| 1 | `Sub17_Close` | end at q 3 | 1745 of 4000 |
| 2 | `Sub17_Close` | frame 6 - q | 2007 of 4000 |
| 3 | `Sub17_Scroll` | x 0x271 | 4000 of 4000 |
| 4 | `Sub17_Scroll` | v >> 11 | 3881 of 4000 |
| 5 | `Sub17_DrawPatch` | bit 1 | 1949 of 4000 |
| 6 | `Sub17_DrawPatch` | height byte 2 | 2003 of 4000 |
| 7 | `Sub17_DrawPatch` | page 0x2190 | 703 of 4000 |
| 8 | `Sub17_DrawPatch` | flag order | 2003 of 4000 |
| 9 | `Sub17_DrawPatch` | >> 7 (floor) | 1744 of 4000 |
| 10 | `Sub17_DrawPatch` | x rounded to float first | **not refused**: equivalent (below) |
| 11 | `Sub17_DrawPatch` | x less 0.5 (the near variant) | 992 of 4000 |
| 12 | `Sub17_CopyFrame` | +0xB + 13 | 4000 of 4000 |
| 13 | `Sub17_CopyFrame` | v from u | 3603 of 4000 |
| 14 | `Sub18_Run` | 0x22 table | 4000 of 4000 |
| 15 | `Sub18_Grow` | > 6 | 138 of 4000 |
| 16 | `Sub18_Hold` | <= 0x10 | 148 of 4000 |
| 17 | `Sub18_Shrink` | > 0x17 | 160 of 4000 |
| 18 | `Sub18_Shrink` | t >> 2 (floor) | 765 of 4000 |
| 19 | `Sub18_End` | > 0x1D | 165 of 4000 |
| 20 | `Sub18_DrawColumn` | start 9 | 4000 of 4000 |
| 21 | `Sub18_DrawColumn` | shade - 7 | 4000 of 4000 |
| 22 | `Sub18_DrawColumn` | dy 3 | 4000 of 4000 |
| 23 | `Sub18_DrawRing` | ^ 0x3E | 4000 of 4000 |
| 24 | `Sub18_DrawRing` | cos >> 14 | 3217 of 4000 |
| 25 | `Sub18_DrawRing` | no drop | 3824 of 4000 |
| 26 | `Sub19` | x - 0x3FC0 | 715 of 4000 |
| 27 | `Sub19` | x within 11 | 94 of 4000 |
| 28 | `Sub19` | strip x half | 268 of 4000 |
| 29 | `Sub19` | clut 0x78CE | 268 of 4000 |
| 30 | `Sub19` | z -0x8F | 268 of 4000 |
| 31 | `Sub19` | turn about y | 264 of 4000 |
| 32 | `Sub68` | x 0x2E | 84 of 4000 |
| 33 | `Sub68` | lift >> 4 | 239 of 4000 |
| 34 | `Sub68` | x * 4 | 239 of 4000 |
| 35 | `Sub68` | first 4 arcs | 239 of 4000 |
| 36 | `Sub1B` | at 6 | 864 of 4000 |
| 37 | `Sub1B` | flag 0x1D | 474 of 4000 |
| 38 | `Sub1B` | z + 1 | 436 of 4000 |
| 39 | `Sub43_Run` | the next entry | 4000 of 4000 |
| 40 | `Sub43_Glow` | > 0x6B | 26 of 4000 |
| 41 | `Sub43_Glow` | counter 0x10 | 181 of 4000 |
| 42 | `Sub43_End` | at 0x1D | 680 of 4000 |
| 43 | `Sub43_DrawQuad` | & 0xFC | 1233 of 4000 |
| 44 | `Sub43_DrawQuad` | y0 + 4 | 4000 of 4000 |
| 45 | `Sub1C_Run` | the next entry | 4000 of 4000 |
| 46 | `Sub1C_Start` | & 0x3F | 2019 of 4000 |
| 47 | `Sub1C_Start` | byte 1 | 4000 of 4000 |
| 48 | `Sub1C_Start` | parity swapped | 2015 of 4000 |
| 49 | `Sub1C_Wait` | sound 0x201 | 510 of 4000 |
| 50 | `Wait` | request 1 passes | 1961 of 20000 |
| 51 | `Sub1C_Rise` | byte 0x8B | 3231 of 4000 |
| 52 | `Sub1C_Hold` | > 0x1D | 160 of 4000 |
| 53 | `Sub1C_Fall` | >> 1 | 3671 of 4000 |
| 54 | `Sub1C_Draw` | <= (the fourth) | 3475 of 4000 |
| 55 | `Sub1C_Draw` | + 5 | 3915 of 4000 |
| 56 | `Sub1C_Draw` | y for x | 4000 of 4000 |
| 57 | `Sub1C_Draw` | & 0xFC0000 | 2948 of 4000 |
| 58 | `Sub1C_Draw` | >> 4 (floor) | 312 of 4000 |
| 59 | `Sub1C_OnScreen` | >= 340 | 117 of 4000 |
| 60 | `Sub1C_OnScreen` | NaN inside | 98 of 4000 |
| 61 | `Sub1C_OnScreen` | x - 0x7F | 4000 of 4000 |
| 62 | `Sub1D_Run` | 0x1C table | 4000 of 4000 |
| 63 | `Sub1D_Start` | >= | 230 of 4000 |
| 64 | `Sub1D_Start` | x + 0x8001 | 4000 of 4000 |
| 65 | `Sub1D_Rise` | +9 + 1 | 4000 of 4000 |
| 66 | `Sub1D_Hurt` | one cell | 4000 of 4000 |
| 67 | `Sub1D_Hurt` | bit 4 | 60 of 4000 |
| 68 | `Sub1D_Hurt` | not put back | 3201 of 4000 |
| 69 | `Sub1D_Hurt` | hurt 6 | 1472 of 4000 |
| 70 | `Sub1D_Hurt` | Field_State left | 2225 of 4000 |
| 71 | `Sub1D_Fall` | >> 3 | 3669 of 4000 |
| 72 | `Sub1D_Draw` | x by +0x10 | 3479 of 4000 |
| 73 | `Sub1D_Draw` | green 0 | 4000 of 4000 |
| 74 | `Sub14_Run` | 0x1D table | 4000 of 4000 |
| 75 | `Sub14_Start` | 0x41 | 2680 of 4000 |
| 76 | `Sub14_Dim` | >= 0x7F | 86 of 4000 |
| 77 | `Sub14_Dim` | >> 2 | 2467 of 4000 |
| 78 | `Sub14_ScaleClut` | bit 15 lost | 3883 of 4000 |
| 79 | `Sub14_ScaleClut` | dirty 2 | 4000 of 4000 |
| 80 | `Sub14_ScaleClut` | red >> 7 (floor) | 698 of 4000 |
| 81 | `Sub1E_Run` | 0x14 table | 4000 of 4000 |
| 82 | `Sub14_WaitFlag` | height + 1 | 1995 of 4000 |
| 83 | `Sub14_WaitFlag` | kind 0x6C | 1998 of 4000 |
| 84 | `Sub14_WaitFlag` | shade 0x81 | 4000 of 4000 |
| 85 | `Sub14_WaitFlag` | z from x | 1995 of 4000 |
| 86 | `Sub14_Fade` | odd frames | 4000 of 4000 |
| 87 | `Sub14_DrawTiles` | even cells | 4000 of 4000 |
| 88 | `Sub14_DrawTiles` | + 0x19 | 4000 of 4000 |
| 89 | `Sub14_DrawTiles` | clut 0x78C4 | 4000 of 4000 |
| 90 | `Sub14_DrawTiles` | rounded between | 2430 of 4000 |
| 91 | `Sub21_Run` | +9 + 1 | 4000 of 4000 |
| 92 | `Sub21_Step` | + 3 | 4000 of 4000 |
| 93 | `Sub21_DrawSpiral` | x + 0xA1 | 2003 of 4000 |
| 94 | `Sub21_DrawSpiral` | phase - 0xFF | 2003 of 4000 |
| 95 | `Sub21_DrawSpiral` | g3 +0x25 1 | 2003 of 4000 |
| 96 | `Sub21_DrawSpiral` | radius >> 3 | 2003 of 4000 |
| 97 | `Sub52` | > 0x13 | 169 of 4000 |
| 98 | `Sub52` | << 2 | 796 of 4000 |
| 99 | `Sub52` | +9 + 1 | 796 of 4000 |
| 100 | `Sub52` | sign swapped | 796 of 4000 |
| 101 | `Sub22_Run` | 0x14 table | 4000 of 4000 |
| 102 | `Sub22_Start` | - 0x7F | 2662 of 4000 |
| 103 | `Sub22_Start` | +2 3 | 1338 of 4000 |
| 104 | `Sub22_Start` | flag + 1 | 4000 of 4000 |
| 105 | `Sub22_WaitSet` | step -9 | 2564 of 4000 |
| 106 | `Sub22_Move` | > 0x10 | 172 of 4000 |
| 107 | `Sub22_WaitClear` | step 9 | 1287 of 4000 |
| 108 | `Sub22_Wait` | sound 0x200 | 3963 of 8000 |
| 109 | `Sub22_SetMap` | byte 0x12 | 838 of 4000 |
| 110 | `Sub22_SetMap` | corner 3 kept | 2998 of 4000 |
| 111 | `Sub22_SetMap` | >> 5 (floor) | 2834 of 4000 |
| 112 | `Sub22_SetMap` | other row | 2998 of 4000 |
| 113 | `Sub22_SetMap` | height once | 3997 of 4000 |
| 114 | `Sub22_Draw` | row by != 2 | 4000 of 4000 |
| 115 | `Sub22_Draw` | dy << 6 | 3653 of 4000 |
| 116 | `Sub22_Draw` | slot 4 | 145 of 4000 |
| 117 | `Sub22_Draw` | x - 0x3F80 | 4000 of 4000 |

**Control 10 is equivalent**: rounding the patch corner's `fx + SX` to a float
before subtracting 64.0 cannot change it, because `_DrawPatch` itself stores
`MapView_ScreenXY` as a whole number (`fild` of `(x >> 9) - 0x4000`) and `fx`
is a multiple of 128, so the sum is an integer below 2^24 and exact as a
float. No input can tell. Its near variant, control 11 (the corner 0.5 less
before the truncation), is refused. (Control 11 was not refused on the second
run, when the seeds kept every record's `+0x36` small, so every patch lay at
negative x, where truncating `n - 0.5` gives `n` again: the seeds now bound the
place word only for the functions that index by it - the fuzz's fault, fixed.
The first run's other two not refused - `_Glow`'s `> 0x6B` and `_Sub1D_Start`'s
`>=` - were the seeds' and the elevation stand-in's too: `+9` now sits at
every compare's value and the one below, and `AreaMap_Elevation` answers one
of four heights half the time so two calls can agree.)

## 7. Latent defects (Capcom's, described, not fixed)

- **The columns divide by `8 - rise` unchecked** (D207) (`0x5052D0`, `0x5057D0`).
  `rise` is `+9` at sub-state 2 (0..7), 7 at sub-state 3 and `+9 >> 2` at
  sub-state 4 (`+9` from 0x1F down to 8: 7..2) - never 8 in play, but a
  record reaching sub-state 2 or 4 with `+9` 8 or 32..35 faults the original
  (`idiv` by 0). Ours aborts naming the function.
- **`_DrawColumn`'s second argument is dead** (D238) (section 1.2), and
  `_DrawTiles`' is never read; the callers pass 0x80 / `(0x1F - +9) * 4` and
  0.
- **Sub-kind 0x14's `_Start` writes the record after releasing it** (D204):
  `Effect_Release` clears `+0..+4`, then `+2` is stepped to 1 on the free
  record; `_Fade` releases by a call and still draws the tiles with the freed
  record's `+9` that frame (sub-kind 0x18's `_End` and 0x1B's are tail jumps
  and do not).
- **`_Hurt` leaves `Field_State` on the party member** (D210) it pointed it at
  (`Sprite_Current` is put back).
- **The place indexes are unchecked** (D200): sub-kinds 0x14 / 0x1E index their
  tables by the x cell word `+0x36` (two places), 0x22 by it (four places),
  0x19 by `+0xB` (the x cell's low byte, two places); E5C's `_DrawPatch` by
  its argument (three variant bytes) and `_CopyFrame` by its (ten frames).
  The spawner sets the cell; past the tables the original reads the data that
  follows (another table, code pointers). Ours aborts.
- **`_SetMap`'s corner row** (D200, D211) is read by a running count that is never
  bounded: the four rectangles have 7, 11, 11 and 11 cells, so it stays in
  its eleven - ours aborts past them. Its writes into the area block
  (`AreaMap_Bytes`, `AreaMap_Corners`, the height bytes) are by the
  rectangles' cells and the area's own width, unchecked, as the game's other
  map writers are.
- **The eight dispatchers do not bound `+2`.** (D200) Every writer in the band stays
  inside its table; ours aborts past any of them.
- **`_DrawSpiral` is centred on the 320 x 240 screen** (D238) (160.0, 120.0 as
  constants): under DIV-0041's wide view it would sit left of centre. Not a
  defect of the original; named for the coordinator (section 8), not changed.

## 8. Calls across groups

**Outbound, raw**: none (`band_rows.py --edges`: E5D calls no group's
function). By name, already ours: `Effect_Release`, `Effect_FindFree`,
`Party_MemberAt`, `Actor_EquipCount`, `Sprite_FlashClut`, `Field_FloorHurt`,
`Flags_Test`, `Flags_Clear`, `MoveCmd_TestFB` / `FC`, `AreaMap_Elevation`,
`AreaMap_SetByte`, `Sound_PlayEffect`, the GTE and GPU helpers,
`MapView_LinkPrimAt`, `Gfx_CommitPrim`, `Prim_SetTexture`, `Math_Sin` /
`Cos`; Capcom's `Rand` by name; the CRT's `_ftol` `0x5B9550` done in place (as
`effect_2a` does); through the tables `WeretigerFx_Next` and
`MagicFx_ClearCount9` (ours).

**Inbound from outside the group** (for the rebinding pass):
- **E5C** (`band_rows.py --edges`, 34 sites, called raw until E5D merges;
  E5D merges first): `0x503FA0` `_DrawPatch` 15 calls (`0x5036D0`,
  `0x503760`, `0x5037D0`, `0x503830`, `0x5038A0` two each; `0x503920`,
  `0x5039B0`, `0x503A40`, `0x503AE0`, `0x503BB0` one each), `0x5043B0`
  `_CopyFrame` 14 calls (`0x5036D0`..`0x503D30`), `0x503E50`
  `_ScrollTexture` 5 tail jumps (`0x5036D0`, `0x503760`, `0x5037D0`,
  `0x503830`, `0x5038A0`), and `0x503DE0` `_Close` as entry 15 of E5C's table
  `0x65E1A8`.
- `EffectKind18_Run` `0x46D830` (ours) reaches the twelve dispatchers and
  single states through `EffectKind18_States` (read in place).

**The harness's `0x503FA0` row** (`kEffectOverrides`, `FxShadow`, standing in
for E5C's 15 calls): its shape matches what the code does - sixteen quads,
each a draw mode of 0xC and a quad linked at 0x48 (16 x 0x54), the vertex
scratch and `MapView_ScreenXY` written, the variant a whole word - **but its
`Draw_PassFlags` test is inverted**: `FxShadow` returns early when bit 2 is
set, while the original (`test al, 4; je` to its exit) draws only when bit 2
is set and does nothing when it is clear. For the coordinator's fold; E5C's
fuzz meanwhile sees a stand-in that draws in the opposite half of its rounds.
It also fills the vertex scratch's four pad words, which the original never
writes (louder, harmless).

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for every row of the band, `0x505100` and `0x505540`
included. No first-call trace names any of the 52
(`analysis/calltrace/reach_dragon`, `reach_whelp`,
`reach_balioAndSunder_1_1003`, `_2_1003`, `reach_bossAndFlash_1003`,
`reach_dragonGene_1003`: every file grepped for the addresses). **Fuzz only.**
No live run was made (the brief). Which scenes spawn these sub-kinds is the
owner's to say; a recorded walk through one would let the coordinator's
frame-hash A/B cover it.

## 10. The rebinding

`grep -rn -i` of the 52 addresses and the eight tables' in `src/game`
(`band_rows.py --refs`: three references, all to `0x503FA0`):

- **Rebound**: none.
- **Left raw, the harness's** (`src/game/scenario_harness.cpp`, which no group
  edits): the `kEffectOverrides` row `{FX_RAW(0x503FA0), ...}` (line 1012)
  and its two comments (lines 802, 855). It stays a raw row while E5C calls
  `0x503FA0` raw; at the round's fold it can become
  `FX_OURS(EffectKind18Sub17_DrawPatch)` (with its bit-2 test turned round,
  section 8).
- **E5C's raw calls** (its own file, written this wave): 34 sites, section 8.

## 11. For `analysis/calltrace/entries_logic.txt`

The main checkout's file (2026-10-03): 40 lines appended for the functions not
listed before; three were listed at the read extent already (`00503FA0`,
`00504570`, `005052D0`); and nine host lines whose catalog extents ran over the hidden
starts cut to the read extents - `005043B0 1BE` to `75`, `005047A0 856` to
`15F`, `00505000 2CF` to `F2`, `00505480 34F` to `BA`, `005057D0 26E` to
`1C4`, `00505A40 1AF` to `6A`, `00505BF0 26A` to `22A`, `00505E60 7DC` to
`42B`, `00506640 B4F` to `21B`. `005032C0` (E5C's host of `0x503DE0`) is
E5C's to fix.
