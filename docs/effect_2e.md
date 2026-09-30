# Group E2E: kind 0x48's states 7..12, effect kinds 0x4A..0x4E

**Status:** MEASURED (2026-09-29) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..12),
wave two, from the round branch's tip `dcef661`. **51 functions ours**
(`src/game/effect_2e.cpp`, shadow name `effect_2e`): the cut table's 51 rows
for E2E (`analysis/round13_cut.tsv`, the band `0x4789D0..0x47B7CF`), no start
dropped and none added. Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
204,000 rounds, 0 mismatches. 94 controls planted one at a time, 93 refused by a count, one equivalent with its near variants refused. **Fuzz only**: no recorded
route enters any of the 51 (section 9).

All 51 are effect code, the three `hypothesis` rows included (section 5).

| What | Functions | Reached through |
|---|--:|---|
| kind 0x48's state 7: a ring record at `0x92D1C8` that swells, widens and lifts | 4 | E2D's `0x4789B0` (kind 0x48's state 7, kind 0x49's state 4) by `+2` through `0x6545FC` (4) |
| state 8: a spiral record at `0x92C4A4`, turning up | 3 | `0x654578[8]` / `0x654584[5]`, `EffectKind48_State8_Steps` `0x65461C` (2) |
| state 9: sparks round the leader | 4 | `[9]` / `[6]`, `EffectKind48_State9_Steps` `0x654624` (3) |
| state 10: the spiral at the leader, turning down | 3 | `[10]` / `[7]`, `EffectKind48_State10_Steps` `0x654630` (2) |
| state 11: a lit sphere at the leader | 3 | `[11]` / `[8]`, `EffectKind48_State11_Steps` `0x654638` (2) |
| state 12: a burst record at `0x92C060` that travels, holds, grows, shrinks | 6 | `[12]` / `[9]`, `EffectKind48_State12_Steps` `0x654640` (8) |
| helpers: `EffectAngle_Mean`, `EffectSphere_Build`, `EffectSphere_Draw` | 3 | calls |
| kind 0x4A: a trail of 64 points after `Sprite_Objects[1]`, a glow at its head | 7 | `Effect_KindHandlers[0x4A]` (`0x655478`), `EffectKind4A_States` `0x65466C` (2) |
| kind 0x4B: 32 debris scattering at a fixed cell | 4 | `Effect_KindHandlers[0x4B]` (`0x65547C`), `EffectKind4B_States` `0x654674` (2) |
| kind 0x4C: random spark lines at a fixed cell for 300 frames | 4 | `Effect_KindHandlers[0x4C]` (`0x655480`), `EffectKind4C_States` `0x65467C` (2) |
| kind 0x4D: a textured column rising at its point | 5 | `Effect_KindHandlers[0x4D]` (`0x655484`), `EffectKind4D_States` `0x654684` (3) |
| kind 0x4E: a disc at the leader's screen point (E2F's `0x47B7D0` draws it) | 5 | `Effect_KindHandlers[0x4E]` (`0x655488`), `EffectKind4E_States` `0x654690` (5) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`). "Ring", "spiral", "sphere", "trail", "debris", "column",
"disc" name the code's shape - the records it keeps and the primitives it
commits - not a play-tested fact: where the game shows these kinds and what
they look like on screen was not traced (section 9; the owner's word, not
this doc's). The spawners found in our source (`grep` of `src/game/area_w*.cpp`
for a kind stored into `+5`): kind 0x48 by area 52's handlers 6 and 7
(`area_w1c.cpp`, `SpawnEffect48`, with `+0xB` a count kind 0x48's E2D states
compare), kinds 0x4A and 0x4C by area 77's handlers 17 and 18
(`Area77_SpawnEffect4A` / `4C`, `area_w2a.cpp`), kind 0x4D by area 103
(`Area103_SpawnEffect4D`, `area_w2d.cpp`, at the running object's point),
kind 0x4E by area 32 (`Area32_SpawnEffect4E`, `area_w0c.cpp`); none stores
0x4B. How kind 0x48 reaches its states 7..12 (its `+1` set past its own
E2D states) is E2D's code and was not traced here.

## 1. What each function does

`Effect_RunObjects` (ours) makes each live record of `Effect_Objects` (20 of
0x80 bytes, `0x7E11E0`) `Sprite_Current` and calls `Effect_KindHandlers[+5]`.
Each dispatcher is four instructions - `mov ecx, [Sprite_Current]; xor eax,
eax; mov al, [ecx + 1 or 2]; jmp [eax * 4 + T]`, no compare. `+1` is the
state, `+2` the sub-state (kind 0x48's states 7..12), `+9` a byte count, `+0xC`
a dword count, `+6` a flag, `+0x34 / +0x38 / +0x3C` the point (x, z, height in
16.16). Every function's comment in `effect_2e.cpp` is the full read; each
`symbols.toml` evidence string the summary. "Sprite_Current read afresh"
means the original reloads the pointer before an access (after a call, the
harness's disturbance may have moved it); ours reads it where the original
does.

### 1.1 Kind 0x48's states 7..12

Kind 0x48's dispatcher `0x4781B0` (E2D's, `Effect_KindHandlers[0x48]`) jumps
by `+1` through the run at `0x654578`; kind 0x49's `0x478550` (E2D's) through
`0x654584`, the same run three entries on. So `0x654578[7..12]` =
`0x654584[4..9]` are six sub-dispatchers by `+2`: `0x4789B0` (E2D's, table
`0x6545FC`, whose four states are ours) and ours `0x478B30`, `0x478C30`,
`0x478D30`, `0x478E40`, `0x478EF0`. The records they keep are global scratch
(the `0x92BF80..0x931174` run, section 7):

- **State 7** (`0x6545FC`): `EffectKind48_State7_Start` puts the pointer
  `0x6761D0` at the ring record `0x92D1C8` (the record's point, its word
  `+0x10` 0), `+9` 0, `+0xC` 0x5F. `_Swell`: unless `+0xC`'s low byte meets the
  byte `0x65460C[+9]`, the ring's `+0x10` up `0x654614[+9]` (two byte tables
  after the state table, read in place by the whole byte `+9`); `+9` up every
  16 frames; the ring drawn (`0x479EE0`, nobody's); at `+0xC` 0 it is 0x10 and
  the next step. `_Widen`: `+0x10` up 4 for 16 frames. `_Lift`: the ring's
  height `+8` up 0x1000000 a frame, released at 0x10000000.
- **State 8**: `EffectKind48_State8_Start` puts the pointer `0x6761CC` at the
  spiral record `0x92C4A4` (the record's point, 0x2000000 higher; its turn word
  `+0xD10` 0), sets it up (`0x4799C0`), `+0xC` 0x28, then sound 0x209.
  `_Turn` steps and draws it (`0x479B70`) and while `+0xC` is above 8 turns it
  0x40; released at 0.
- **State 9**: `_Start` at the leader's x / z and the ground, the sparks cleared
  (`0x4790C0`), `+6` 1. `_Emit` steps the spiral, and while `+0xC` is above 8
  looks for a free spark (`0x47CF20`, one of 8 records of 0x1C at `0x92BF80`)
  and every fourth frame sets it (`0x479160`); runs the sparks (`0x479260`); at
  0 `_Drain` runs them until none is left, then releases.
- **State 10**: as state 8 at the leader, the turn word 0x800 and turning down,
  no sound.
- **State 11**: `_Start` at the leader, `EffectSphere_Build`, `+0xC` 0x78;
  `_Draw` draws the sphere for 0x78 frames and releases.
- **State 12** (`0x654640`, eight: five ours, then `WeretigerFx_Next` twice and
  `Effect_StateRelease`): `_Start` sets the burst record's size word `0x92C49E`
  to 0xC0 and steps the burst (`0x4794D0`, nobody's) 32 times, sound 0x203 when
  `+6` is set; `_Travel` steps and draws it (`0x4796B0`) and moves the point by
  `+0xC / +0x10 / +0x14` until it reaches `+0x18 / +0x1C` (then `+9` 0x2F);
  `_Hold` counts `+9` down and releases there when `+6` is 0, else sound 0x204;
  `_Grow` grows the size 6 a frame for 0x20, `_Shrink` shrinks it 0xC a frame
  for 0x20, then `+2` walks on through `WeretigerFx_Next` (a "+2 on" handler of
  the spell round, `magic_s14.md`) to `Effect_StateRelease`.

### 1.2 The helpers

- `EffectAngle_Mean` `0x479970` (cdecl `(a, b)`, eax): the two angles' low 12
  bits ordered, their mean, plus 0x800 when they lie 0x800 or more apart - the
  mean the short way round, **not wrapped** (up to 0x13FF). Called by
  `0x4794D0` (nobody's), E2F's `0x47D040` and E3D's `0x4873E0`.
- `EffectSphere_Build` `0x47A560`: the unit sphere (0x1000 = 1) into
  `0x92D9DC` - a pole, 15 rings at the polar angles 0x80..0x780 of 32 points
  each, the other pole: 0x1E2 vertices of 8 bytes; 0x200 quads of four u16
  vertex numbers at `0x92FF84` (the caps as degenerate quads); the light (0,
  0x1000, 0x1000) normalised in place at `0x931168` (`Gte_VectorNormal`); a
  shade byte a vertex at `0x930F84`, the square root (`0x5A7A90`) of its dot
  with the light, at most 0xFFF, `sar 7` (0 when the dot is not above 0).
  al 0, never read.
- `EffectSphere_Draw` `0x47A780`: each vertex projected
  (`EffectGte_ProjectPoint`) as the world point (x << 5, y << 5, (z + 0x1000)
  << 13) about the record's point - a radius of two cells in 16.16, its lowest point on
  the record's height - into `0x92E8EC`, then 0x200 semi-transparent
  Gouraud quads, each corner shaded its vertex's shade. al 0, never read.

### 1.3 Kinds 0x4A..0x4E

- **0x4A**: `_Start` at `Sprite_Objects[1]`'s point (`0x7DEF58..`), `+0xC`
  0xB4, the trail set (`EffectKind4A_TrailInit`: 64 points of 0x34 at
  `0x92BF80` at the record's point). `_Follow`, each frame inside
  `Gte_PushMatrix` / `Gte_PopMatrix`: at `+0xC` 0xA5 the spin byte `0x92CC82`
  set; while above 0x5A the record follows the sprite; `_TrailStep` moves the
  points on (the newest bobbing by `Math_Sin` of the frame count, its angle the
  turn word `0x92CC80`, which turns 0x40 a frame while the spin byte is set
  until it comes round to 0) and gives each point two edge points at its angle;
  `_TrailDraw` 63 Gouraud quads between successive edge pairs, fading;
  `_DrawGlow(0x92BF80, 0x80, 0x80, 0)` a fan of 32 triangles round the head's
  screen point, its radius 0x80 scaled to the depth (`EffectGte_ProjectSize`).
- **0x4B**: `_Start` at the cell (0x738000, 0xB8000), 0x80 above the ground,
  32 debris set (`EffectKind4B_DebrisInit`: E1C's `EffectKind1E_DebrisInitOne`
  with edges of 0x20 and a scale 1..16, no push of its own); `_Scatter` draws
  each (E3C's `0x485030`), turning it 0x10 a frame, brightening 0x20 while
  `+9` is below 4 else fading 2, for 0xB9 frames.
- **0x4C**: `_Start` at the cell (0x740000, 0x60000), `+0xC` 300; `_Crackle`
  plays 0x20B every 30 frames and for the first 15 of each 30 draws a spark
  (`EffectKind4C_DrawSpark`: a grey line between two points, each x and z
  moved from the record's by ((Rand & 0xFF) - 0x80) << 8 in 16.16 - up to half
  a cell either way).
- **0x4D**: `_Start` keeps the base (`+0xC..+0x14`), the top `+0x1C` = the base
  height, the turn word `+0x20`, the fade `+0x24` 0x1000000, `+9` 0x3C, and
  plays 0x208 or 0x209 (the byte `0x6761D8`). `_Rise` raises the top 0x1000000 a
  frame to at most 0x8000000 above the base, turns 0x200 back and draws
  (`EffectKind4D_DrawColumn`) for 0x3C frames; `_Fade` for 0x10 more, the fade
  0x100000 less each, then releases. The draw: textured quads (tpage (0x140,
  0x100), clut (0, 0x1E7)) up a column about the base - the ring point at
  (cos t, sin t) << 5 in 16.16, two cells out - turning 0x100 a quad and
  rising 0x100000 a quad from the base to the top.
- **0x4E**: kind 0x25's shape (E1D) on E2F's disc: `_Start` projects the
  leader's point into the record's `+0x74` (screen floats), `+0x60`, `+0x7C`;
  `_Grow` radius `+9` up 0xF to past 0x3C, `_Glow` radius 0x3C + a flicker for
  0xFF frames (sound 0x203 at 0xD7), `_Shrink` down to below 0, then
  `Effect_StateRelease`.

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed (`DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no address of
the band nor its tables; checked 2026-09-29). Where the original indexes past a
table ours aborts with a `Fatal` naming the function (round9 doc section 6;
nothing in play reaches it): the ten dispatchers past their tables (the
original jumps through the next table's entries - they lie back to back), and
`EffectSphere_Draw` on a quad naming a vertex past the 0x1E2 (the original
reads past the projected points and the shades into the next arrays;
`EffectSphere_Build` writes only numbers below 0x1E2). The two byte tables
`EffectKind48_State7_Swell` indexes by the whole byte `+9` are read in place
at any index, as the original reads them: the image's bytes are there
(`0x65460C + 0xFF` lies inside `.data`), so no abort is needed and none is
taken; in play `+9` stays below 6 (section 7).

`EffectKind4A_DrawGlow`'s `fild` / `fadd dword` / `fstp` are x87 inline
assembly (`magic_s32.cpp`'s idiom), so the sum rounds under whatever control
word the thread holds, as the original's does; the other float moves are
integer copies, as the original's `mov`s are.

## 3. The arguments pushed with leftovers

| Callee | Pushed | Read by the callee |
|---|---|---|
| E2F's `0x47B7D0` (kind 0x4E's disc) | radius `movzx cx, byte +9` or `0x3C + (+9 & 1)` into `edx` over `xor dx, dx` - the upper halves the caller's (`Sprite_Current`'s high word through the dispatcher's `ecx`); 0x80; 0 | radius `movsx edi, word [esp + 0x38]` at `0x47B808` (16 bits); the rim `mov bl, [esp + 0x40]` (a byte) |
| `EffectKind4A_DrawGlow` | 0x92BF80, 0x80, 0x80, 0 (immediates) | point whole; size `mov cx, word [esp + 0x34]`; centre and rim bytes |
| `EffectGte_ProjectSize` (from `_DrawGlow`) | `size` = the address of the caller's own first argument slot, with `size`'s low word stored over the point's low word | `size[0]` = the size word; `size[1]` = the point's address's high word (read, its quotient written to `out[1]` and never used) |

The fuzz lists the disc and `_DrawGlow` with those masks; ours passes
`_DrawGlow`'s size pair as `{size, point >> 16}`, so `out[1]` is the
original's too.

## 4. The fuzz (`effect_2e_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_2e`, effect mode (`g.effect`, kinds
0x48..0x4E), 4,000 rounds a function (`BOF3X_E2E_ONLY=<name>` runs the clones
whose name holds it). Shapes: 38 `kEffect` (each clone's own kind; the five
sub-dispatchers' `sub_span` and the five kind dispatchers' `state_span` their
table's length - 2, 3, 2, 2, 8 and 2, 2, 2, 3, 5), 13 `kCall` (the helpers:
`EffectAngle_Mean` with `ret_mask` `0xFFFFFFFF`, the sphere's two with `0xFF`,
the rest void). The ten tables are `DataTable`s, swapped for recorders on both
sides. **Regions** beyond effect mode's standard ones: the scratch past
`EffectKind30_Shards`' standard 0x644 to the light's end (`0x92C5C4..0x931174`,
19,376 bytes: the trail, the debris, the spiral, the ring, the sphere's four
arrays) and `0x6761CC..0x6761DB` (the two pointers and the sound byte).

**Callees**: the effect-standard rows for `Effect_Release` (clears `+0..+4`),
`Sound_PlayEffect`, `Rand`, `AreaMap_Elevation`, `Math_Sin` / `Cos`, the
`Gpu_*`, `Gfx_CommitPrim`, `Gte_Push` / `PopMatrix`, `Gte_VectorNormal`,
`Gte_RotMatrixX / Y / Z`, `EffectGte_LoadMapCamera`, `EffectGte_SetDiagonalOne`
and the part-6 / part-7 callees nobody owns (`0x479EE0`, `0x4799C0`,
`0x479B70`, `0x4790C0`, `0x4794D0`, `0x4796B0`, `0x5A7A90`, `0x5A7C70`: EKH's
rows, 16 or 18 bytes of their records hashed). Re-listed in the group: the
group's own helpers by name (the void ones `kPhase`, logging the state they
ran with; `_DrawGlow` and `_DebrisInit` with their masks); E3C's `0x485030`
(the debris' 0x2C bytes hashed) and E2F's `0x47B7D0` (section 3); `0x47CF20`
answering a spark record or 0 (the standard row answers garbage, which the
`test eax, eax` never sees as none), `0x479160` logging the record it is handed
and filling 24 bytes, `0x479260` answering a flag; the square root `0x5A7A90`
answering about the sphere's clamp half the time (0xF7F, 0xF80, 0xFFE..0x1000
among its answers, else any eax) and `Gte_VectorNormal` writing the light as
0, as the real unit (0, 0xB50, 0xB50) or as noise (controls 39 and 91); `EffectGte_ProjectPoint` and
`_ProjectSize` with their stack pointers not logged (their addresses differ
between the copy and ours), the points and the size pair hashed, the outs
filled (floats; the size one time in two at 0, 1, 0x7FFF, 0x8000, 0xFFFF or
below 0x100); `Gte_RotTransPers` with `sxy` (the record's `+0x74`) logged and
filled with two floats; `Gte_StoreDepthF` logging its cell.

**Seeds** (per function, after the harness's per-round fill): the pointers
`0x6761D0` / `0x6761CC` at their records two times in three, else elsewhere in
the regions; `+0xC` at every compare (0, 1, 2, 8, 9, 0x10, 0x11, 0x20, 0x5A,
0x5B, 0xA5, 0xB8, 0xB9, 30, 31, 44, 45, 60, 0x12C, -1, -15, the sign's edges,
small or any); `+9` (0..5, 7, 8, 0x2D, 0x2E, 0x3C, 0x3D, 0x7F..0x81, 0xD7,
0xD8, 0xFF, any); `+6` 0 half the time; `_Swell`: `+9` below 8 two times in
three, else below 0x40, `+0xC` at its masks; `_Lift`: the ring's height at
0xF000000 and its neighbours; `_Travel`: the goal one step away on one or both
axes; `_TrailStep`: the turn word at 0, 0xFFC0, 0xFF80, the spin byte 0 or
not; the sphere draws: the 0x800 vertex numbers below 0x1E2 (0 and 0x1E1 one
in eight); kind 0x4D: every record's base height and top within 0xA000000 of
one height (so the column's loop - which reads the top again after its calls,
when the harness may have moved `Sprite_Current` to another record - stays
under about 180 quads), `_Rise`'s top at the cap and its neighbours;
`EffectAngle_Mean`'s two angles 0, 1, 0x7FF, 0x800, 0x801 or 0xFFF apart, with
upper bits; `_DrawGlow`'s point one of the trail's, `_DebrisInit`'s record one
of the debris. **Disturbance** (the group's, from the hash only): `+9`,
`+0xC`, `+6`, the spin byte, the burst's size word, the ring's height.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_2e`,
exit 0): 204,000 rounds over 51 functions, 22,653,155 calls to the stand-ins,
**0 mismatches**; 44,148 bytes of state in 47 regions. Every entry of the ten
tables reached (each handler recorder 476..2,039 calls; `WeretigerFx_Next`
996, `Effect_StateRelease` 1,367), `Effect_Release` 5,548, `0x47CF20` 2,758
and `0x479160` 604 (a spark set), the sphere's `0x5A7A90` 964,377.

SHADOW_ALL

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read all 51 (6,120 bytes against the cut's
  6,496: 39 differ by padding only, none by code). Three `entries_logic.txt`
  lines were longer than the code: `0x47A780` 0x4FE (ours ends at `0x47A94D`,
  0x1CE; the line spans kinds 0x4A..0x4C's hidden starts), `0x47B180` 0x265
  (0x121; spans kind 0x4D's), `0x47B3F0` 0x3D7 (0x238; spans kind 0x4E's):
  section 11.
- **Hidden starts**: 41, each an entry by address - a cell of
  `Effect_KindHandlers` or of a state table - not a case or a shared tail.
  Their recorded hosts: `0x4783C0` (E2D's, 23 of ours after its code),
  `0x47A780`, `0x47B180`, `0x47B3F0` (ours now). None of the hosts contains our
  code as a fall-through: each start is reached only by its cell.
  `EffectKind4D_Fade` ends in a tail `jmp` to `EffectKind4D_DrawColumn`, which
  is an entry of its own (called by `_Rise`), not a shared tail.
- **The `hypothesis` rows** (`0x479970`, `0x47A560`, `0x47A780`; the
  labelling pass's "kind 73" by address): an angle helper three effect
  functions call and the sphere's build and draw, which kind 0x48's state 11
  calls - effect code, taken.
- **No start dropped, none added**: the tool printed 0 "code no list has".
  The code between the rows (`0x4790C0..0x47996F`, `0x4799C0..0x47A55F`) is in
  no group of the cut: catalog part 7 (`0x4790C0`, `0x4790F0`, `0x479160`,
  `0x479260`, `0x4792E0` with the hidden `0x479420` / `0x479470`, `0x4794D0`,
  `0x4796B0`, `0x4799C0`) and part 6 (`0x479B70`, `0x479EE0`, `0x47A110`,
  `0x47A130`, `0x47A150`, `0x47A200`, `0x47A2B0`, `0x47A3D0`): the sparks, the
  burst, the spiral and the ring kind 0x48's states call, and kind 0x48's
  E2D-side helpers (`0x47A200` is called by E2D's `0x4789A0`). For the
  coordinator to place.
- **`Effect_KindHandlers` and `EffectKind18_States`**: every entry pointing
  into the band is one of this group's five dispatchers (a scan of both
  tables, 2026-09-29).

## 6. Controls

Planted one at a time by scratch `controls.py` in `effect_2e.cpp` (plant anchored on a unique string, rebuild, run the clone alone under `BOF3X_E2E_ONLY=<its function>`, restore, rebuild at the end; the committed file has no switch). Counts are rounds refused of 4,000, in this worktree, on the final fuzz file.

| # | Function | Plant | Refused |
|--:|---|---|--:|
| 1 | `EffectKind48_State7_Start` | the ring's word `+0x10` = 1 | 4000 |
| 2 | `EffectKind48_State7_Start` | `+0xC` = 0x5E | 4000 |
| 3 | `EffectKind48_State7_Swell` | reload 0x11 | 494 |
| 4 | `EffectKind48_State7_Swell` | the add table one on | 2297 |
| 5 | `EffectKind48_State7_Swell` | `+9` up every 8 frames | 31 |
| 6 | `EffectKind48_State7_Widen` | widen 5 | 4000 |
| 7 | `EffectKind48_State7_Lift` | released above 0x10000000 only | 535 |
| 8 | `EffectKind48_State8_Run` | state 9's table | 4000 |
| 9 | `EffectKind48_State8_Start` | turn word 1 | 4000 |
| 10 | `EffectKind48_State8_Start` | sound 0x20A | 4000 |
| 11 | `EffectKind48_State8_Start` | height + 0x2000001 | 4000 |
| 12 | `EffectKind48_State8_Turn` | turn 0x41 | 2781 |
| 13 | `EffectKind48_State8_Turn` | turns at `+0xC` = 8 too | 147 |
| 14 | `EffectKind48_State9_Run` | state 10's table | 4000 |
| 15 | `EffectKind48_State9_Start` | `+6` = 2 | 4000 |
| 16 | `EffectKind48_State9_Emit` | a spark every other frame | 253 |
| 17 | `EffectKind48_State9_Emit` | emits at `+0xC` = 8 too | 147 |
| 18 | `EffectKind48_State9_Drain` | released unless al is 1 | 2677 |
| 19 | `EffectKind48_State10_Run` | state 11's table | 4000 |
| 20 | `EffectKind48_State10_Start` | turn word 0x801 | 4000 |
| 21 | `EffectKind48_State10_Turn` | turn - 0x3F | 2781 |
| 22 | `EffectKind48_State11_Run` | state 12's table | 4000 |
| 23 | `EffectKind48_State11_Start` | `+0xC` = 0x77 | 4000 |
| 24 | `EffectKind48_State11_Draw` | `+0xC` down twice | 4000 |
| 25 | `EffectKind48_State12_Run` | state 11's table | 4000 |
| 26 | `EffectKind48_State12_Start` | size 0xC1 | 2813 |
| 27 | `EffectKind48_State12_Start` | 31 steps | 4000 |
| 28 | `EffectKind48_State12_Travel` | `+9` = 0x2E | 1616 |
| 29 | `EffectKind48_State12_Travel` | height moved by `+0x10` | 4000 |
| 30 | `EffectKind48_State12_Hold` | `+9` = 0x21 | 98 |
| 31 | `EffectKind48_State12_Hold` | sound 0x205 | 98 |
| 32 | `EffectKind48_State12_Grow` | grow 7 | 4000 |
| 33 | `EffectKind48_State12_Shrink` | shrink 0xB | 4000 |
| 34 | `EffectAngle_Mean` | `<= 0x800` | 571 |
| 35 | `EffectAngle_Mean` | b masked to 13 bits | 1830 |
| 36 | `EffectSphere_Build` | pole z 0xF001 | 4000 |
| 37 | `EffectSphere_Build` | shade `sar 6` | 2999 |
| 38 | `EffectSphere_Build` | last cap quad's fourth vertex 0x1E0 | 4000 |
| 39 | `EffectSphere_Build` | clamp at 0xFFE | **0: equivalent** |
| 40 | `EffectSphere_Draw` | height `<< 12` | 4000 |
| 41 | `EffectSphere_Draw` | shade of the neighbour vertex | 4000 |
| 42 | `EffectKind4A_Run` | kind 0x4B's table | 4000 |
| 43 | `EffectKind4A_Start` | `+0xC` = 0xB5 | 3958 |
| 44 | `EffectKind4A_Follow` | spin at 0xA6 | 160 |
| 45 | `EffectKind4A_Follow` | follows at 0x5A too | 152 |
| 46 | `EffectKind4A_Follow` | glow centre 0x81 | 4000 |
| 47 | `EffectKind4A_TrailInit` | angle word 1 | 4000 |
| 48 | `EffectKind4A_TrailStep` | bob `<< 11` | 4000 |
| 49 | `EffectKind4A_TrailStep` | spin 0x41 | 2642 |
| 50 | `EffectKind4A_TrailStep` | lower edge `<< 7` | 4000 |
| 51 | `EffectKind4A_TrailStep` | 61 points moved | 4000 |
| 52 | `EffectKind4A_TrailDraw` | red 0x41 | 4000 |
| 53 | `EffectKind4A_TrailDraw` | green / blue a quarter | 4000 |
| 54 | `EffectKind4A_DrawGlow` | `Frame_Counter & 3` | 1855 |
| 55 | `EffectKind4A_DrawGlow` | size[1] `>> 15` | 4000 |
| 56 | `EffectKind4A_DrawGlow` | vertex 1 depth from L[1] | 4000 |
| 57 | `EffectKind4B_Run` | kind 0x4C's table | 4000 |
| 58 | `EffectKind4B_Start` | lift 0x81 | 4000 |
| 59 | `EffectKind4B_Scatter` | brighten below 5 | 192 |
| 60 | `EffectKind4B_Scatter` | released above 0xB9 | 33 |
| 61 | `EffectKind4B_Scatter` | draw mode dtd 1 | 4000 |
| 62 | `EffectKind4B_DebrisInit` | scale + 2 | 4000 |
| 63 | `EffectKind4B_DebrisInit` | edge angle 0x21 | 4000 |
| 64 | `EffectKind4C_Run` | kind 0x4D's table | 4000 |
| 65 | `EffectKind4C_Start` | `+0xC` = 0x12D | 4000 |
| 66 | `EffectKind4C_Crackle` | draws 16 of 30 | 337 |
| 67 | `EffectKind4C_Crackle` | sound 0x20C | 826 |
| 68 | `EffectKind4C_DrawSpark` | grey 0xC1 | 4000 |
| 69 | `EffectKind4C_DrawSpark` | jitter - 0x7F | 4000 |
| 70 | `EffectKind4D_Run` | kind 0x4E's table | 4000 |
| 71 | `EffectKind4D_Start` | sound 0x207 / 0x208 | 4000 |
| 72 | `EffectKind4D_Start` | fade 0x1000001 | 4000 |
| 73 | `EffectKind4D_Rise` | cap 0x7FFFFFF | 1708 |
| 74 | `EffectKind4D_Rise` | reload 0x11 | 202 |
| 75 | `EffectKind4D_Fade` | fade - 0xFFFFF | 3771 |
| 76 | `EffectKind4D_DrawColumn` | y up 0x100001 | 3909 |
| 77 | `EffectKind4D_DrawColumn` | uv byte 0xBE | 4000 |
| 78 | `EffectKind4D_DrawColumn` | fade read from `+0x20` | 3929 |
| 79 | `EffectKind4D_DrawColumn` | clut y 0x1E6 | 4000 |
| 80 | `EffectKind4E_Run` | kind 0x4D's table | 4000 |
| 81 | `EffectKind4E_Start` | height `>> 1` (floor) | 1053 |
| 82 | `EffectKind4E_Start` | sxy `+0x70` | 4000 |
| 83 | `EffectKind4E_Grow` | past 0x3C at 0x3C | 203 |
| 84 | `EffectKind4E_Glow` | sound at 0xD8 | 445 |
| 85 | `EffectKind4E_Glow` | flicker `& 3` | 1595 |
| 86 | `EffectKind4E_Shrink` | ends at 0 | 198 |
| 87 | `EffectKind48_State12_Hold` | released when `+6` is 1 | 140 |
| 88 | `EffectKind48_State7_Lift` | lift 0x1000001 | 4000 |
| 89 | `EffectSphere_Build` | clamp at 0xF7F (39's near variant) | 2999 |
| 90 | `EffectSphere_Build` | clamp unsigned (39's near variant) | 2999 |
| 91 | `EffectSphere_Build` | the root taken at a dot of 0 | 1004 |
| 92 | `EffectKind4D_DrawColumn` | the top read on the record before `ProjectPoint` (no reload) | 1634 |
| 93 | `EffectKind4A_TrailStep` | the spin byte read on the record before `Math_Sin` (no reload) | 170 |
| 94 | `EffectKind4E_Start` | the answer to `+0x64` | 4000 |

**93 of 94 refused by a count.** Control 39 is an equivalent mutant: the clamp keeps the root below 0x1000 only for `sar 7`, and 0xFFE and 0xFFF both give 0x1F - no root tells the two clamps apart; its near variants 89 and 90 are refused. Controls 39 and 91 first passed against EKH's rows for the square root (garbage eax) and `Gte_VectorNormal` (noise), which never reach the clamp or a dot of 0: the fuzz's fault, fixed by re-listing both (section 4); every other control was refused on the first run.

## 7. Latent defects (Capcom's, described, not fixed)

- **The kinds share one scratch.** Kind 0x4A's trail (`0x92BF80..0x92CC83`),
  kind 0x4B's debris (`0x92CC84..0x92D203`), kind 0x48's spiral
  (`0x92C4A4..0x92D1C3`), ring (`0x92D1C8..0x92D1D9`), burst
  (`0x92C060..0x92C49F`) and sparks (`0x92BF80..0x92C05F`) overlap one another
  and `EffectKind30_Shards` (kinds 0x1C..0x1F, E1C's); and each kind keeps one
  copy, not one a record. Two such effects alive at once - two kind-0x4A
  records, or a kind 0x4B beside a kind 0x48 in state 7 (debris 31 lies over
  the ring) - write over each other's points and counts. Whether the game ever
  spawns two at once was not traced.
- **`EffectAngle_Mean` does not wrap its answer**: two angles in the upper
  half and 0x800 or more apart give up to 0x13FF. Its callers were not read
  for whether they mask it.
- **`EffectKind4A_DrawGlow` hands `EffectGte_ProjectSize` its own argument
  slot** as the size pair, the size word stored over the point's low word, so
  the callee's second size is the high word of the point's address (0x0092
  for the trail's head) and its second quotient, written to the stack pair's
  second word, is never read: harmless as compiled. A depth of 0 at the head
  is the callee's divide fault (EGT's latent defect, `effect_gte.md`).
- **`EffectKind48_State7_Swell` indexes two eight-byte tables by `+9` with no
  bound**; `+9` rises every 16 frames of the 0x5F, so 0..5 in play - inside.
- **`EffectSphere_Draw` trusts the quads' vertex numbers** (u16, unchecked):
  only `EffectSphere_Build` writes them, below 0x1E2; ours aborts past it.

## 8. Calls across groups

| From | To | Owner | How ours calls it |
|---|---|---|---|
| `EffectKind4B_Scatter` | `0x485030` (a debris record drawn) | E3C (wave three) | raw, `effect_2e::at::kDebrisDraw`, `SH_AT` |
| `EffectKind4E_Grow` / `_Glow` / `_Shrink` | `0x47B7D0` (the disc at the record's screen point) | E2F (this wave) | raw, `at::kDiscDraw` |
| many | `EffectGte_LoadMapCamera`, `_ProjectPoint`, `_SetDiagonalOne`, `_ProjectSize` | EGT (ours) | by name (`game/effect_gte.h`) |

**Inbound** (for the rebinding pass): `EffectAngle_Mean` `0x479970` is
called by `0x4794D0` (nobody's), E2F's `0x47D040` and E3D's `0x4873E0`; E2D's
`0x4789B0` dispatches into the four state-7 functions through `0x6545FC`, and
E2D's kind 0x48 / 0x49 run `0x654578` holds the five sub-dispatchers
(`0x654598..0x6545A8`) - E2D's fuzz swaps those cells, and the names are in
`symbols.toml` for its rebinding.

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 51 rows and for the part-6 / part-7 functions
between them. The 31 first-call traces under `analysis/calltrace`
(`*/bof3x.calltrace.tsv`, among them `reach_dragon`, `reach_whelp` and the
round-twelve recipe runs) enter none of the 51 nor the host `0x4783C0`
(`grep` of each entry, 2026-09-29). **Fuzz only**: the coordinator's
frame-hash A/B covers none of the 51 until the owner records a route through
a place that shows these kinds (areas 32, 52, 77, 103 spawn kinds 0x4E, 0x48,
0x4A / 0x4C, 0x4D).

## 10. The rebinding

`grep -rn -i` of the 51 addresses and the ten tables' in `src/game` (and
`band_rows.py --refs`): the only raw references are in two harness files -
`scenario_harness_ekh.cpp` (EKH's self-test copies `0x479970` from the image:
its clone line and comment) and `scenario_harness.cpp` (a comment on EKH's
row for `0x4794D0`, "calls 0x479970"). **Left raw, for the coordinator**: a
group may not edit a harness; the addresses there name Capcom's original being
copied and stay valid (the fold would spell it
`bof3::addr::EffectAngle_Mean`). No constant, call site or fuzz key of another
file names an E2E address; E2F and E3D (this round) call `0x479970` from their
own code, theirs to rebind. Our fuzz file lists the ten tables by address
beside their names in `symbols.toml`.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29, a commented block): the 41
hidden starts and the smaller extents of `0x47A780` (0x1CE), `0x47B180`
(0x121) and `0x47B3F0` (0x238) - their old lines (0x4FE, 0x265, 0x3D7) left
for the merger to drop. The other seven not-hidden rows' lines already held
the extents read here.
