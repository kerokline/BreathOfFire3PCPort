# Group E2C: effect kinds 0x3E, 0x3F, 0x40, 0x42, 0x43, 0x44 and 0x6B

**Status:** MEASURED (2026-09-29) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9 and 10),
wave two, from the round branch's tip `dcef661`. **53 functions ours**
(`src/game/effect_2c.cpp`, shadow name `effect_2c`): the cut table's 51 rows
for E2C (`analysis/round13_cut.tsv`, the band `0x474F40..0x477180`) and two
starts no list holds that the band's code reaches - `0x476560` (a callee of
kind 0x43's, `band_rows.py`'s "code no list has") and `0x476680` (kind 0x44's
dispatcher, `Effect_KindHandlers[0x44]`, a catalog part-2 row); no start
dropped. Each read to its last instruction with capstone and fuzzed through the
scenario harness in effect mode ([`scenario_harness.md`](scenario_harness.md)
section 8) without edits to it: 318,000 rounds, 0 mismatches.
CONTROLS_SUMMARY **Fuzz only**: no recorded route enters any of the 53
(section 9).

All 53 are effect code. The two `hypothesis` rows of the cut (`0x475090`,
`0x475760`, kinds 0x3F's and 0x40's dispatchers) are what the labelling pass
said they were, and are taken.

| Kind | Functions | Reached through | Spawned by (ours) |
|---|--:|---|---|
| 0x3E: four lightning bolts between fixed cells, a sound while `Field_Request` is 5 | 7 | `Effect_KindHandlers[0x3E]` (`0x655448`), `EffectKind3E_States` `0x654468` (2), `EffectKind3E_SubStates` `0x654480` (2, by `+2`) | `Area145_Tail20` (`area_w3f.cpp`) |
| 0x3F: a beam between two world points for one frame | 5 | `Effect_KindHandlers[0x3F]` (`0x65544C`), `EffectKind3F_States` `0x654488` (2, the second `Effect_StateRelease`) | `Area145_SpawnTrail` (`area_w3f.cpp`) |
| 0x40: a disc growing round its point, 32 shards falling inside it | 9 | `Effect_KindHandlers[0x40]` (`0x655450`), `EffectKind40_States` `0x654490` (3, the third `Effect_StateRelease`) | `Area28_SpawnEffect40` (`area_w0c.cpp`) |
| 0x42: a glow cylinder at its point | 2 | `Effect_KindHandlers[0x42]` (`0x655458`), `EffectKind42_States` `0x65449C` (2: E2G's `0x47EEC0`, ours) | none found |
| 0x43: 256 sparks round a fixed cell that rise, circle and fly out | 9 | `Effect_KindHandlers[0x43]` (`0x65545C`), `EffectKind43_States` `0x6544B0` (4) | `Scena10_Run13` step 0x10 (`scena_sc9b.cpp`) |
| 0x6B: sprite 2 drawn off screen, read back and crumbled into falling pixels | 6 | `Effect_KindHandlers[0x6B]` (`0x6554FC`), `EffectKind6B_States` `0x6544C0` (5) | `Scena10_Run13` step 0x18 |
| 0x44: a ring of light on sprite 1 with a cone and trailing sparks | 15 | `Effect_KindHandlers[0x44]` (`0x655460`), `EffectKind44_States` `0x6544D4` (6) | `Area78_SpawnEffect44` (`area_w2a.cpp`; area 80's choice 6 too) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`; the seven dispatchers `evidence`). "Bolt", "beam", "disc",
"shard", "spark", "ring", "cone", "pixel" name the code's shapes - the points it
builds, the primitives it commits - not a play-tested fact: where the game
shows these kinds and what they look like on screen was not traced (section
9; the owner's word, not this doc's). The spawners are ours from earlier
rounds, found by the kind byte they store into a record's `+5`; they say where
each kind is started, not what it looks like. No spawner of kind 0x42 was
found by that grep (its state 0 is group E2G's).

## 1. What each function does

`Effect_RunObjects` (ours) makes each live record of `Effect_Objects` (20 of
0x80 bytes, `0x7E11E0`) `Sprite_Current` and calls `Effect_KindHandlers[+5]`.
Each kind here is a four-instruction dispatcher - `mov ecx, [Sprite_Current];
xor eax, eax; mov al, [ecx + 1]; jmp [eax * 4 + T]`, no compare - and the
states its table names. `+1` is the state, `+9` a frame count, `+6` kind
0x43's phase; the rest per kind below. Every function's comment in
`effect_2c.cpp` is the full read; each `symbols.toml` evidence string the
summary. "Sprite_Current read afresh" means the original reloads the pointer
before an access (after a call, the harness's disturbance may have moved it);
ours reads it where the original does. Every x87 operation is done in x87 as
the original does it (section 2).

Four kinds keep their working records outside `Effect_Objects`, in the buffer
that starts at `EffectKind30_Shards` `0x92BF80` and runs on to the particles
at `0x92EC80`: kind 0x40's 32 shards of 0x18 and its disc's screen centre and
rim (`0x92C280`, `0x92C28C`); kind 0x43's 256 sparks of 0x1C; kind 0x44's ring
record (`0x92BF80`, through the cell `EffectKind44_RingCell`) and its eight
sparks of 0xA0 (`0x92C07C`); kind 0x6B's read-back pixels (w x h words) and its
particles of 0x14 (`0x92EC80`). The cells that walk them are named
([[data]], section 3): `EffectKind40_ShardCursor` `0x67610C`,
`EffectKind6B_PixelCount` `0x676110`, `EffectKind6B_Heights` `0x676114` (80
s16), `EffectKind43_SparkSide` `0x6761B4`, `EffectKind44_SparkCursor`
`0x6761B8`, `EffectKind44_RingCell` `0x6761C0`.

### 1.1 Kind 0x3E (`EffectKind3E_Run` `0x474F40`)

| Address | Name | What |
|---|---|---|
| `0x474F60` | `EffectKind3E_Start` | state 0: the point `(0x318000, 0x638000)`, the heights `+0x14` / `+0x20` 0x500000; `Sound_PlayById(0x208)`; `+1` up, `+2` = 0 |
| `0x474FC0` | `EffectKind3E_Bolts` | state 1: four bolts, each from the cell centre `(c0, c1)` to `(c2, c3)` of `EffectKind3E_BoltCells` (`c << 16 \| 0x8000` into `+0xC`, `+0x10`, `+0x18`, `+0x1C`), drawn by `EffectKind3E_DrawBolt(record + 0xC, record + 0x18)`; then a **call** through `EffectKind3E_SubStates` by `+2`, unbounded |
| `0x475050` | `EffectKind3E_SubWait` | sub-state 0: `Field_Request` not 5: `+2` = 1 |
| `0x475070` | `EffectKind3E_SubSound` | sub-state 1: `Field_Request` 5: `Sound_PlayEffect(0x209)` (each frame it is 5) |
| `0x4750D0` | `EffectKind3E_DrawBolt(from, to)` | `EffectGte_LoadMapCamera`; a draw mode (`Gpu_GetTPage(0, 1, 0x3C0, 0)`, slot 1); `from`'s x, z, height projected; 16 grey (0x80) semi-transparent `LINE_F2` joints from the last projection to the next: x and z stepped by `(to - from) / 16` (a signed divide toward zero), the height `from`'s plus `((Rand & 0xFFF) - 0x800) << 12` on all but the last; each committed 0x20 and glowed. `to`'s height is not read |
| `0x475240` | `EffectKind3E_BoltGlow(p0, p1)` | the normal `Gte_VectorNormal` (in place) of `(_ftol(-(p1.y - p0.y)), _ftol(p1.x - p0.x), 0)` times 5 `sar` 12 = n; a semi-transparent `POLY_G4` of `p0`, `p1` (shade 0x40) and `p0 + n`, `p1 + n` (black), committed 0x44; copied (`rep movsd`) to the next packet with `p0 - n`, `p1 - n`, committed 0x44 |

`EffectKind3E_DrawBolt` also copies `from + 0xC` into a local it never reads
(its projection reads 12 bytes of the four it copied): ours does not read it.

### 1.2 Kind 0x3F (`EffectKind3F_Run` `0x475090`)

| Address | Name | What |
|---|---|---|
| `0x4750B0` | `EffectKind3F_Draw` | state 0: `EffectKind3F_DrawBeam(record + 0xC, record + 0x18)`; `+1` up (to `Effect_StateRelease`: one frame) |
| `0x475390` | `EffectKind3F_DrawBeam(from, to)` | a draw mode (abr 1, slot 1); `EffectGte_LoadMapCamera`; both ends projected; the angle `Math_Ratan2(dy, dx)` of the screen segment; each end's radius `EffectGte_ProjectSize((0x10, 0))` at it plus `Frame_Counter & 1`; a cap at `from` (angle + 0x400) and at `to` (angle + 0xC00), the band between |
| `0x4754A0` | `EffectKind3F_DrawCap(centre, r, a)` | eight semi-transparent `POLY_G3` of a half-disc: the rim `(cos, sin)(t) * r sar 12` for `t = a + 0x100 k` masked to 16 bits; centre yellow `(0x80, 0x80, 0)`, rim black; committed 0x34 each. `r` read as s16 (its argument slot then reused as the loop count) |
| `0x4755B0` | `EffectKind3F_DrawBand(p0, r0, a0, p1, r1, a1)` | at the cursor a semi-transparent `POLY_G4` of `p0`, `p1` (yellow) and their rims at `a0` and `a1 + 0x800` (black), committed 0x44; 0x44 bytes on - the cursor **not** read again - the same quad with the rims at `a0 + 0x800` (passed whole) and `a1`, committed 0x44 |

### 1.3 Kind 0x40 (`EffectKind40_Run` `0x475760`)

| Address | Name | What |
|---|---|---|
| `0x475780` | `EffectKind40_Start` | state 0: `EffectKind40_ShardsClear`; the radius word `+0x2E` = 0; `+1` up |
| `0x4757A0` | `EffectKind40_Grow` | state 1: the counter byte `0x903848` at 0x19 - `+1` up (to `Effect_StateRelease`); else `EffectKind40_DrawDisc(+0x2E)`, `EffectKind40_ShardsStep`, `+0x2E` up 0x40 to at most 0x400 (signed) |
| `0x4757F0` | `EffectKind40_ShardsClear` | the 32 shards out of use through the cursor (left past the last) |
| `0x475820` | `EffectKind40_ShardsStep` | `EffectGte_LoadMapCamera`; a draw mode (abr 1, slot 3); each live shard (the cursor on it, read again after the draw): its height word `+8` down 0x20, below 0x80 out of use, drawn either way; then twice a free shard found (the cursor its answer) and started |
| `0x4758D0` | `EffectKind40_ShardDraw` | on the cursor's shard: its screen point the disc's centre plus `(s16 +4 << 7) / s16 +8` and `(s16 +6 << 7) / s16 +8`, its depth the centre's; drawn only inside the disc's rim (32 edges, each cross product 0.0 or more, x87) - a `TILE_1`, **not** semi-transparent, shade `(0x80 - s16 +8) * 192 / 512 - 0x40`, slot 3 |
| `0x475A20` | `EffectKind40_ShardSpawn(shard)` | in use; `+4`, `+6` `(Rand & 0xFF) - 0x80`; `+8` = 0x280 |
| `0x475A60` | `EffectKind40_ShardFind` | the first free shard (the cursor left on it), 0 when none (`eax`) |
| `0x475A90` | `EffectKind40_DrawDisc(r)` | a draw mode (abr 2, slot 3); `EffectGte_LoadMapCamera`; the record's point projected - the centre, kept at `0x92C280`; 32 semi-transparent `POLY_G3` of a disc (centre white, rim black), the rim at `(cos, sin) * r sar 4` from the point at `0xFF80 + 0x80 k` (Sprite_Current read afresh after each Math call), each new rim point kept at `0x92C28C + 0xC k`; slot 3. `r` read as s16 |

A shard falls from height 0x280 by 0x20 a frame and is drawn at its offset
scaled by `128 / height` from the disc's centre, only while it lies inside the
disc - so its point spreads outward as it falls; `EffectKind40_Grow` grows the
disc to its full radius over 16 frames.

### 1.4 Kind 0x42 (`EffectKind42_Run` `0x475CA0`)

| Address | Name | What |
|---|---|---|
| `0x475CC0` | `EffectKind42_Glow` | state 1: `Area146_DrawGlowCylinder` at a stack copy of the record's point; `+1` unchanged (the record runs until something releases it). State 0 is `0x47EEC0` (group E2G's: the point `(0xA0000, 0x630000)` on the ground, `+1` up) |

### 1.5 Kind 0x43 (`EffectKind43_Run` `0x475CF0`)

| Address | Name | What |
|---|---|---|
| `0x475D10` | `EffectKind43_Start` | state 0: `EffectKind43_Setup`; `+0x32` = 0x78, the phase `+6` = 0; `+1` up; `Sound_PlayEffect(0x201)` |
| `0x475D40` | `EffectKind43_Gather` | state 1: four sparks added, all run; `+0x32` down, at 0 `+6` = 1, `+0x32` = 0x3C, `+1` up |
| `0x475D90` | `EffectKind43_Wait` | state 2: the sparks run; the counter byte `0x903848` at 0x11 - sounds 0x203 and 0x202, `+6` = 2, `+1` up |
| `0x475DD0` | `EffectKind43_Fade` | state 3: the sparks run; none live - `Effect_Release` (a tail `jmp`) |
| `0x476230` | `EffectKind43_Setup` | the record's point `(0x250000, 0x748000)` on the ground (`AreaMap_Elevation`'s s16 `<< 16`); the 256 sparks out of use |
| `0x476290` | `EffectKind43_AddSpark` | the first free spark (none: nothing): in use, its side `+3` `EffectKind43_SparkSide` (then flipped), its phase `+2` 0 |
| `0x4762D0` | `EffectKind43_SparksRun` | `EffectGte_LoadMapCamera`; each live spark by its phase through the five-case jump table `0x47654C` (above 4 nothing): **0** 0x180 above the record's point, z 0x28000 behind (side 1: ahead), a climb `+4` from `Rand`, count 0x20; **1** rising and drifting, at the count's end an angle from `Rand` and the phase + the record's `+6` + 1; **2** circling the record's point (`(cos - 0x5800) << 4`, `sin << 4`), counted, then out of use; **3** circling, and at the record's `+6` = 2 a speed from `Rand`, count 0x10, phase 4; **4** flying out `(cos, sin) * +4 sar 4`, counted, then out of use; each then drawn. `al` 1 when any was live |
| `0x476560` | `EffectKind43_SparkDraw(spark)` | a draw mode (`Gpu_GetTPage(0, 1, 0x380, 0x100)`, dtd 0) linked at the spark (`MapView_LinkPrimAt(x, z, 0, 0xC)`); the spark projected; a semi-transparent `TILE_1` `(0x80, 0x80, (Rand & 1) << 7)` linked 0x14; a semi-transparent 3 x 3 `TILE` one pixel up and left, `(0x20, 0x20, (Rand & 1) << 5)`, linked 0x1C |

The phase a spark takes after rising is 2 while the record's `+6` is 0 (the
gather), 3 once it is 1 (`EffectKind43_Gather`'s end): so sparks added during
the gather circle and die, later ones circle until the record's `+6` is 2
(`EffectKind43_Wait`, at the counter byte 0x11) and then fly out.

### 1.6 Kind 0x6B (`EffectKind6B_Run` `0x475DE0`)

| Address | Name | What |
|---|---|---|
| `0x475E00` | `EffectKind6B_Capture` | state 0: a free record (`Effect_FindFree`; none: `+1` up and nothing else); VRAM `(0x340, 0x100, 0x80, 0x100)` cleared (`Gfx_ClearRect`); `Sprite_Objects` record 2's first 0x80 bytes copied into the free record, in use, its point again, its screen position `+0x2E` / `+0x30` `EffectKind6B_Frame`'s x / y, `+0x24 \| 0x88`; with `Sprite_Current` that record `Sprite_UpdateScreen` and `Effect_Release` (the record borrowed for one draw); `Sprite_Current` back, `+9` = 2, `+1` up |
| `0x475EC0` | `EffectKind6B_Store` | state 1: `+9` down; at 0 the rectangle `(0x340, 0x100, w, h)` of `Gfx_VramShadow` copied to `0x92BF80` (`0x59E930`), `+1` up |
| `0x475F20` | `EffectKind6B_Scatter` | state 2: the 80 heights cleared; `EffectGte_LoadMapCamera`; sprite 2's point projected (o); for each row below h and column below w (s16; the row and column are **bytes**), a pixel not 0 becomes a particle: colour `+2`, screen point `((o.x - x + column) * 16, (o.y - y + row) * 16)` floats `+4` / `+8`, depth `+0xC`, its column's height raised to its y (`_ftol` of the stored float; the compare on the unrounded one), count `+1` = `(Rand & 7) - row + h`, speed `+0x10` = `Rand & 0x1F`, column `+0x12`; `+1` up |
| `0x4760E0` | `EffectKind6B_Arm` | state 3: every particle flagged 3 (live, counting); `Sound_PlayEffect(0x200)`; `+1` up |
| `0x476120` | `EffectKind6B_Fall` | state 4: `Sprite_Objects` record 2's `+0 \| 0x40`; each live particle - flags' high nibble 0: count down, at 0 count 0x10 and flags + 0x10; 0x10: speed + 2, y + speed (x87), past its column's height out of use - drawn either way as a `TILE_1` at `(x, y) * 0.0625` in its 15-bit colour (`<< 3`, `>> 2`, `>> 7`, each `& 0xF8`), slot 2; none live (or none made): `Effect_Release` |

`EffectKind6B_Frame` (`0x6544A8`, four s16) holds the rectangle: the sprite is
drawn at (0x30, 0x36) and read back 0x50 x 0x48 - so the scatter makes up to
5,760 particles, a pixel each, bottom rows first (their counts are the
smallest), and each falls to its column's lowest pixel and vanishes: the sprite
crumbles from the bottom up (the code's shape; section 7 has what it
overwrites past 1,881 particles).

### 1.7 Kind 0x44 (`EffectKind44_Run` `0x476680`)

| Address | Name | What |
|---|---|---|
| `0x4766A0` | `EffectKind44_Start` | state 0: the record's point sprite 1's; `EffectKind44_RingCell` = `0x92BF80`, the ring there: its top the point `+ 0x8000000` up, its centre the point, radius 0, shades `+0x22` 0x20 / `+0x23` 0; the sparks cleared; `+9` = 0x10, `+1` up |
| `0x476750` | `EffectKind44_FadeIn` | state 1: the ring's shade `+0x23` up 3; projected, drawn; `+9` down, at 0 the sparks cleared, `+9` = 0x78, `+1` up |
| `0x4767B0` | `EffectKind44_Sparks` | state 2: projected, drawn, the sparks stepped and one emitted; `+9` down, at 0 `+9` = 0x1E, `+1` up |
| `0x476800` | `EffectKind44_Widen` | state 3: the radius `+0x20` up 0x30 to at most 0x180; projected, drawn, the cone, the sparks; `+9` down, at 0 `+9` = 0x78, `+1` up |
| `0x476870` | `EffectKind44_Follow` | state 4: the ring's centre sprite 1's point; the same draws; `+9` down, at 0 `+9` = 0x10, `+1` up |
| `0x4768F0` | `EffectKind44_FadeOut` | state 5: the shade down 3; the same draws; `+9` down, at 0 `Effect_Release` (a tail `jmp`) |
| `0x476950` | `EffectKind44_RingProject(ring)` | `EffectGte_LoadMapCamera`; the top projected to `+0x24`, the centre to `+0x30`; 16 points of the circle of radius s16 `+0x20` round the centre (`(cos, sin) * r sar 4`, 0x100 apart) projected to `+0x3C + 0xC i` |
| `0x476A00` | `EffectKind44_RingDraw(ring)` | a draw mode (abr 2, slot 1); for each ring edge a semi-transparent `POLY_F4` from the edge to the screen's side (x 320.0 when the edge's middle is right of the centre's projection, else 0), shade `+0x23`, committed 0x38, the lowest and highest screen y kept (`_ftol`, from 0 and 0xF0); two `TILE`s 320 wide above the highest and below the lowest, the same shade, committed 0x1C (their `+0x10` never written) |
| `0x476C00` | `EffectKind44_RingCone(ring)` | a draw mode (abr 1, slot 1); the triangles top - point i - point i+1 that `0x4941B0` says face the eye (`ax` above 0): semi-transparent `POLY_F3` (`0x5A7570`), shade `+0x22`, committed 0x2C |
| `0x476D00` | `EffectKind44_SparksClear` | the cursor `0x92C07C`, then eight times the in-use byte **it** points at cleared - the cursor never moves (section 7) |
| `0x476D20` | `EffectKind44_SparksStep` | each live spark of the eight (the cursor on it, read again after each call): its ground point moved by its speed, its height the ground's, its trail drawn, its life down, at 0 out of use |
| `0x476DB0` | `EffectKind44_SparkTrail(spark)` | a draw mode; `EffectGte_LoadMapCamera`; the two histories of four screen points (`+0x40`, `+0x70`) shifted and the newest projected (the high end `+0x18`, the ground end `+0x28`); a semi-transparent `LINE_F2` between them in the colour `+4`; three semi-transparent `POLY_G4` between successive pairs, the colour down a quarter of itself at each step (bytes, wrapping) |
| `0x476FC0` | `EffectKind44_SparkEmit` | a free spark (the cursor its answer); on frames with `Frame_Counter & 3` = 0: its high end the record's point `+ 0x6000000` up, its ground end at a `Rand` angle (`<< 8`) round it on the ground, its speed `(cos, sin) << 4` of that angle + 0x800 + a `Rand` spread of -0x100..0xFF, its colour `EffectKind44_SparkColours[Rand & 7]`, life 0x20; both ends projected and copied down the histories |
| `0x477180` | `EffectKind44_SparkFind` | the first free spark of the eight (the cursor left on it), 0 when none |

## 2. Divergence

None. Each function is a faithful replacement: every store, every read of
`Sprite_Current` and of the cursors where the original makes it, every
callee's arguments as pushed. The x87 is done as x87: loads through an inline
`flds` (so the optimizer cannot narrow an operation of two floats to SSE,
whose rule for two NaN operands differs), every operation in `long double` at
the game's control word, stores through `fstp dword`; `_ftol` as its 64-bit
truncation (NaN and out of range: the integer indefinite, low dword 0). Where
the original indexes past what it indexes, ours aborts with a message (the
round-nine rule, [`takeover-queue-round9.md`](takeover-queue-round9.md)
section 6), and no ordinary play reaches any of them (section 6).

## 3. Tables and cells named

`[[data]]` in `symbols.toml`, each read by hand against a raw scan of the image
for every cell address (scratch `scan.py`): no dispatcher bounds its index, so
each count is the table's own length to the next table a dispatcher names.

| Address | Name | Count | What |
|---|---|--:|---|
| `0x654468` | `EffectKind3E_States` | 2 | `_Start`, `_Bolts`; bytes follow |
| `0x654470` | `EffectKind3E_BoltCells` | 16 | four bolts of four cell bytes |
| `0x654480` | `EffectKind3E_SubStates` | 2 | `_SubWait`, `_SubSound` (called by `+2`) |
| `0x654488` | `EffectKind3F_States` | 2 | `_Draw`, `Effect_StateRelease` |
| `0x654490` | `EffectKind40_States` | 3 | `_Start`, `_Grow`, `Effect_StateRelease` |
| `0x65449C` | `EffectKind42_States` | 2 | `0x47EEC0` (E2G's), `_Glow`; a 0 dword follows |
| `0x6544A8` | `EffectKind6B_Frame` | 4 (s16) | x, y, w, h |
| `0x6544B0` | `EffectKind43_States` | 4 | `_Start`, `_Gather`, `_Wait`, `_Fade` |
| `0x6544C0` | `EffectKind6B_States` | 5 | `_Capture`, `_Store`, `_Scatter`, `_Arm`, `_Fall` |
| `0x6544D4` | `EffectKind44_States` | 6 | `_Start`, `_FadeIn`, `_Sparks`, `_Widen`, `_Follow`, `_FadeOut` |
| `0x6544EC` | `EffectKind44_SparkColours` | 8 | the sparks' colours |
| `0x67610C` | `EffectKind40_ShardCursor` | | kind 0x40's shard being stepped |
| `0x676110` | `EffectKind6B_PixelCount` | | kind 0x6B's particles made (u16) |
| `0x676114` | `EffectKind6B_Heights` | 80 (s16) | kind 0x6B's columns' lowest pixel |
| `0x6761B4` | `EffectKind43_SparkSide` | | the next spark's side |
| `0x6761B8` | `EffectKind44_SparkCursor` | | kind 0x44's spark being stepped |
| `0x6761C0` | `EffectKind44_RingCell` | | kind 0x44's ring (`0x92BF80`) |

`band_rows.py` read `0x6544AC..0x6544E8` as one run of eleven code pointers
(its first dword `0x480050` is kind 0x6B's w and h read as a pointer) and filed
all eleven under kind 0x6B: `0x47668E` in kind 0x44's dispatcher names
`0x6544D4`, so kind 0x6B's table is five and kind 0x44's six. It likewise
filed `0x474F60` / `0x474FC0` as entries 22 and 23 of kind 0x35's run
`0x654410` (group E2B's): kind 0x3E's dispatcher names `0x654468`, so E2B's
table ends before it.

## 4. The fuzz (`effect_2c_fuzz.cpp`)

FUZZ_SECTION

## 5. What the cut and the tool said, settled

TOOL_SECTION

## 6. Aborts

ABORTS_SECTION

## 7. Latent defects (Capcom's, described, not fixed)

DEFECTS_SECTION

## 8. Calls across groups

CROSS_SECTION

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 53 rows. `analysis/calltrace/reach_dragon` and
`reach_whelp` (`bof3x.calltrace.tsv`): neither enters any address of the band
`0x474F40..0x4771A5`, while `Effect_RunObjects` runs on both (first seen at
frames 334 and 284). Of the 53, the 22 not hidden were listed in
`entries_logic.txt` then (so armed and not reached); the hidden starts were
not listed and are no evidence either way. **Fuzz only**: the coordinator's
frame-hash A/B covers none of the 53 until the owner records a route through
a place that shows these kinds - area 145 (kinds 0x3E, 0x3F), area 28 (0x40),
areas 78 / 80 (0x44), or chapter 10's run 13 (0x43, 0x6B).

## 10. The rebinding

`band_rows.py --refs --group E2C` and `grep -rn -i` of the 53 addresses and
the eight tables' in `src/game`: **no raw reference** to any of them outside
this group's files. Nothing to rebind, nothing left raw for the coordinator.
This group's own fuzz file lists its eight tables by address (a `DataTable`
takes the address; the named `[[data]]` entries are macros of
`symbols.gen.h`, which `bof3::addr::` cannot spell), and `effect_2c_callees.h`
spells the named cells by address with their names beside them for the same
reason.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29, a commented block, 35 lines):
the 29 hidden starts, `0x476560` and `0x476680`, and the smaller extents of
`0x4755B0` (0x1A4; its line 0x233 spans kind 0x40's dispatcher and two
states), `0x475A90` (0x201; 0x798 spans kinds 0x42, 0x43 and 0x6B),
`0x4762D0` (0x290; 0x678 spans `0x476560` and kind 0x44's states) and
`0x477180` (0x25; 0x373 runs into group E2D's band) - their old lines left for
the merger. `0x474D20`'s line (0x3AE) covers this group's first seven hidden
starts: it is group E2B's host, and E2B cut it.
