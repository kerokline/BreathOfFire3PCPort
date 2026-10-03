# Group E5D: effect kind 0x18's sub-kinds 0x14, 0x18, 0x19, 0x1B..0x1E, 0x21, 0x22, 0x43, 0x52, 0x68

**Status:** MEASURED (2026-10-03) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..15),
wave five, from the round branch's tip `0834edf`. **52 functions ours**
(`src/game/effect_5d.cpp`, shadow name `effect_5d`): the cut table's 49 rows
for E5D (`analysis/round13_cut.tsv`, the band `0x503DE0..0x506860`) and three
starts no list of the cut has - sub-kind 0x17's texture scroll `0x503E50`
(code no list has, which E5C's states tail-jump to) and the dispatchers of
sub-kinds 0x1C `0x505100` and 0x1D `0x505540` (`EffectKind18_States[28]` /
`[29]`, catalog part 2 rows no group of the round held; section 5). Each read
to its last instruction with capstone and fuzzed through the scenario harness
in effect mode ([`scenario_harness.md`](scenario_harness.md) section 8)
without edits to it: 208,000 rounds, 0 mismatches; CONTROLS_SUMMARY. **Fuzz
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
exit 0): RESULT_LINE

STAR_LINES

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

CONTROLS_TABLE

## 7. Latent defects (Capcom's, described, not fixed)

- **The columns divide by `8 - rise` unchecked** (`0x5052D0`, `0x5057D0`).
  `rise` is `+9` at sub-state 2 (0..7), 7 at sub-state 3 and `+9 >> 2` at
  sub-state 4 (`+9` from 0x1F down to 8: 7..2) - never 8 in play, but a
  record reaching sub-state 2 or 4 with `+9` 8 or 32..35 faults the original
  (`idiv` by 0). Ours aborts naming the function.
- **`_DrawColumn`'s second argument is dead** (section 1.2), and
  `_DrawTiles`' is never read; the callers pass 0x80 / `(0x1F - +9) * 4` and
  0.
- **Sub-kind 0x14's `_Start` writes the record after releasing it**:
  `Effect_Release` clears `+0..+4`, then `+2` is stepped to 1 on the free
  record; `_Fade` releases by a call and still draws the tiles with the freed
  record's `+9` that frame (sub-kind 0x18's `_End` and 0x1B's are tail jumps
  and do not).
- **`_Hurt` leaves `Field_State` on the party member** it pointed it at
  (`Sprite_Current` is put back).
- **The place indexes are unchecked**: sub-kinds 0x14 / 0x1E index their
  tables by the x cell word `+0x36` (two places), 0x22 by it (four places),
  0x19 by `+0xB` (the x cell's low byte, two places); E5C's `_DrawPatch` by
  its argument (three variant bytes) and `_CopyFrame` by its (ten frames).
  The spawner sets the cell; past the tables the original reads the data that
  follows (another table, code pointers). Ours aborts.
- **`_SetMap`'s corner row** is read by a running count that is never
  bounded: the four rectangles have 7, 11, 11 and 11 cells, so it stays in
  its eleven - ours aborts past them. Its writes into the area block
  (`AreaMap_Bytes`, `AreaMap_Corners`, the height bytes) are by the
  rectangles' cells and the area's own width, unchecked, as the game's other
  map writers are.
- **The eight dispatchers do not bound `+2`.** Every writer in the band stays
  inside its table; ours aborts past any of them.
- **`_DrawSpiral` is centred on the 320 x 240 screen** (160.0, 120.0 as
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
