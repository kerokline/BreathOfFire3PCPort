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
169 controls planted, 167 refused by a count, two equivalent (each with a near variant refused). **Fuzz only**: no recorded route enters any of the 53
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

One `scenario_harness::Group` (`effect_2c`), effect mode, the kinds `0x3E,
0x3F, 0x40, 0x42, 0x43, 0x44, 0x6B`, 6,000 rounds a function.
`BOF3X_E2C_ONLY=<names>` (a comma list, names whole) runs those clones only.

**Clones.** The table `band_rows.py --group E2C --clones --harness scenario`
printed (extents as read; the cut's sizes were padding for 26 and wrong for
`0x4762D0`, section 5), with `0x476680` added by hand. The seven dispatchers
and the states are `kEffect` with the kind and, for a dispatcher, `state_span`
its table's count (so `+1` is drawn inside the table; `EffectKind3E_Bolts` has
`sub_span` 2 for its sub-state table); the void helpers that read the record
are `kEffect` with no span; the helpers taking arguments `kCall`, a point or
screen point in the harness's scratch buffers (`Arg::kScratch`), a record
(shard, spark, ring) handed in by `args` from the records the seed wrote.
`EffectKind43_SparksRun` is compared on `al` (`ret_mask` 0xFF), the two finds
on `eax`. `EffectKind43_SparksRun`'s five-case jump table is moved into its
copy (`JumpTable {0x39, 0x27C, 5}`).

**Tables.** The eight tables are `DataTable`s, swapped for recorders on both
sides while the fuzz runs: every entry was reached (coverage: `phase 0x...`,
0x46A310 `Effect_StateRelease` and E2G's `0x47EEC0` among them).

**Regions** beyond the effect-standard ones: kind 0x43's 256 records past the
shards' 0x644 (to `0x92DB80`); kind 0x6B's first 80 particles (`0x92EC80`,
0x640); the cells `0x67610C..0x6761C3`; and `EffectKind6B_Frame`'s four words,
seeded - the original's 0x50 x 0x48 makes 5,760 particles (0x1C200 bytes), past
the 64 KiB of state the harness holds, so the fuzz draws w and h with w x h at
most 80 (w up to the original's 0x50 with one row), and 0, negative and
0x8000 values. 32,112 bytes of state in 49 regions.

**Stand-ins** (the group's `Callee` list, registered before the standard sets):
the group's own 23 callees by name - record and packet pointers logged by value
and their points hashed (`deref` 12), a caller's local hashed only, radii and
angles masked to the word the callee reads (`movsx` / `and 0xFFFF`: the callers
push whole registers); `EffectKind40_ShardDraw` logs the shard cursor it reads;
`EffectKind40_ShardFind` / `EffectKind44_SparkFind` answer 0 a third of the
time, else one of their records (the callers write through the answer).
Re-listed louder than the standard rows:

| Callee | Why |
|---|---|
| `EffectGte_ProjectPoint` | the standard row logs both pointers; the point is often a local here (different in the copy and ours): hashed, and `out` logged only when it is not a local (a ring's or spark's cells), and filled with finite floats |
| `EffectGte_ProjectSize` | as above; `out` filled (a small radius half the time) |
| `Gte_VectorNormal` | in place on a local in `EffectKind3E_BoltGlow`: hashed, `out` filled |
| `Sprite_UpdateScreen` | the standard row logs nothing: here the record it draws is a borrowed effect record - `Sprite_Current` and its 0x80 bytes logged |
| `0x4941B0` | the effect-standard row writes 8 bytes at each of its three pointers; the real one only reads them (the ring's projections): hashed (8 each), nothing written |
| `0x59E930` | the rectangle hashed, its target logged and filled `w * h` words (at most 0x100) |

**Seeds**, per function: `+9` at 1, 2, 0, 0x10 or anything; `Field_Request` 5
half the time for kind 0x3E's sub-states; the counter byte `0x903848` at 0x19
(kind 0x40) or 0x11 (kind 0x43) a third of the time; the radius `+0x2E` round
0x3C0..0x401 and the sign; kind 0x40's 32 shards in use or not, their heights
`+8` never 0 (the original divides by it, and a disturbed cursor may land on
any), their offsets inside the rim or not, and the disc's rim 32 points of a
circle anticlockwise (inside test passing), clockwise, all at one point (every
product 0.0) or noise; the in-use bytes of the shard, kind-0x43 and kind-0x44
records all taken a quarter of the time; kind 0x43's sparks in every phase
(and past the five), counts at 1, both sides, the record's `+6` 0..3; kind
0x6B's rectangle as above, its heights round the particles' y, its particles
in every flag phase with counts at 1 and columns inside the 80; kind 0x44's
ring cell on one of eight records, their radius words round 0x150..0x181, their
projected points on and off the screen; `Frame_Counter & 3` 0 half the time;
the screen points handed to the cap, band and glow finite three times in four.

**Disturbance** (the group's case, from the hash only): the shard and spark
cursors among their records, the ring cell among the eight, the counter byte
(0x19 / 0x11 / anything), `+9`, the particle count (at most 80), the words
`+0x32` and `+0x2E`.

**Result** (2026-09-29, this worktree): `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=effect_2c`, exit 0 - **318,000 rounds over 53 functions,
5,884,722 calls to the stand-ins, 0 mismatches**. Coverage, calls the
originals made: every standard callee the band calls, the three raw ones
(`0x4941B0` 96,000, `0x59E930` 2,004, `0x5A7570` 48,000), every table entry,
`Effect_FindFree` 6,000, `Effect_Release` 7,591. `BOF3X_SHADOW='*'` (every earlier group's fuzz and this one), exit 0, every
self-test line 0 mismatches, `inject: 7226 ours, 0 left original`
(`effect_2c` there: 318,000 rounds, 5,881,143 calls, 0 mismatches - another
stream); and again with `BOF3X_WIDE=1`: exit 0, every self-test line 0 mismatches. Neither run died silently.

### 4.1 Controls

A script (scratch `controls.py`) plants each mutation in `effect_2c.cpp`,
rebuilds (a second build directory, `build2`), runs the self-test on the
planted functions' clones only, restores and rebuilds. Plants in different
functions share a build and a run (`BOF3X_E2C_ONLY` a list): a plant changes
ours of its own function only (every call out of ours goes to a recorder), so
each is judged by its own clone's count ("`<name>` mismatched in N rounds").
**169 planted, 167 refused by a count, 2 not refused - both equivalent**:
`W3` (`>= 0x400` as `> 0x400` before setting 0x400: at 0x400 both leave
0x400), its near variant `W3b` (the compare unsigned) refused; `A1` (the climb
`v / 32` as `v sar 5`: v is a multiple of 0x10000, so both are exact), its
near variant `A1b` (of `v - 1`, where truncation and flooring differ) refused.

| Function | Controls (id: plant - rounds refused of 6,000) |
|---|---|
| `EffectKind3E_Run` | D3E: kind 3E state xor 1 - 6000 |
| `EffectKind3F_Run` | D3F: kind 3F 1 as 0 - 3016 |
| `EffectKind40_Run` | D40: kind 40 2 as 1 - 2012 |
| `EffectKind42_Run` | D42: kind 42 always 0 - 2993 |
| `EffectKind43_Run` | D43: kind 43 3 as 2 - 1497 |
| `EffectKind6B_Run` | D6B: kind 6B 4 as 3 - 1198 |
| `EffectKind44_Run` | D44: kind 44 5 as 4 - 1052 |
| `EffectKind3E_Start` | S3E1: x 0x318001 - 6000; S3E2: sound 0x209 - 6000; S3E3: +2 = 1 - 6000 |
| `EffectKind3E_Bolts` | B3E1: x1 | 0x8001 - 6000; B3E2: ends swapped - 6000; B3E3: bolts in another order - 6000; B3E4: sub-state xor 1 - 6000 |
| `EffectKind3E_SubWait` | W3E: request 4 - 3056 |
| `EffectKind3E_SubSound` | U3E: sound 0x20A - 2995 |
| `EffectKind3E_DrawBolt` | L1: x step / 32 - 6000; L2: z step sar not divide - 2843; L3: last joint jittered - 6000; L4: jitter - 0x7FF - 6000; L5: grey 0x7F - 6000; L6: abr 2 - 6000 |
| `EffectKind3E_BoltGlow` | G1: no fchs - 4768; G2: nx * 4 - 4480; G3: second quad x - ny - 5378; G4: shade 0x41 - 5953; G5: x from y - 5570; G6: copy 0x40 not 0x44 - 6000 |
| `EffectKind3F_Draw` | F3F: to + 0x1C - 6000 |
| `EffectKind3F_DrawBeam` | M1: cap angle + 0x401 - 6000; M2: frame bit 1 - 5031; M3: size (0x10, 1) - 6000; M4: dy negated - 6000; M5: band a1 = a0 - 6000 |
| `EffectKind3F_DrawCap` | C1: step 0x80 - 6000; C2: radius whole - 5814; C3: centre blue 1 - 6000; C4: depth from y - 5917 |
| `EffectKind3F_DrawBand` | N1: p1 rim + 0x400 - 6000; N2: second quad at the cursor - 1528; N3: a0 + 0x800 masked - 192; N4: second quad p0 radius r1 - 5059; N5: green 0x81 - 6000 |
| `EffectKind40_Start` | S40: radius 1 - 6000 |
| `EffectKind40_Grow` | W1: counter 0x18 - 1970; W2: + 0x41 - 1749; W3: > 0x400 (equivalent: sets the same) - **not refused** (equivalent); W3b: unsigned compare - 1071; W4: radius + 1 - 4020 |
| `EffectKind40_ShardsClear` | X1: 31 records - 6000 |
| `EffectKind40_ShardsStep` | T1: fall 0x1F - 6000; T2: <= 0x80 - 4798; T3: one spawn - 6000; T4: slot 1 - 6000; T5: cursor not read after the draw - 587 |
| `EffectKind40_ShardDraw` | R1: << 6 - 5350; R2: > 0.0 - 1205; R3: cross negated - 675; R4: shade << 5 - 1202; R5: semi-transparent - 1495; R6: rim & 0xF - 371 |
| `EffectKind40_ShardSpawn` | P1: height 0x281 - 6000; P2: - 0x7F - 6000 |
| `EffectKind40_ShardFind` | F1: 31 searched - 1496; F2: cursor not kept - 3736 |
| `EffectKind40_DrawDisc` | Q1: first angle 0xFF00 - 6000; Q2: centre y from depth - 6000; Q3: rim kept as centre - 6000; Q4: centre 0xFE - 6000; Q5: radius unsigned - 2136; Q6: Sprite_Current not read after Math_Sin - 5995 |
| `EffectKind42_Glow` | K42: z from height - 6000 |
| `EffectKind43_Start` | A43S1: count 0x77 - 5953; A43S2: sound 0x202 - 6000 |
| `EffectKind43_Gather` | A43G1: three sparks - 6000; A43G2: count 0x3D - 1979 |
| `EffectKind43_Wait` | A43W1: counter 0x12 - 1895; A43W2: phase 3 - 1989 |
| `EffectKind43_Fade` | A43F: inverted - 6000 |
| `EffectKind43_Setup` | A43U1: z + 1 - 6000; A43U2: 255 cleared - 5974 |
| `EffectKind43_AddSpark` | A43A1: side not flipped - 4488; A43A2: phase 1 - 4505 |
| `EffectKind43_SparksRun` | A0: z - 0x29000 - 6000; A1: climb sar not divide (equivalent: v a multiple of 0x10000) - **not refused** (equivalent); A2: drift 0xB00 - 5977; A3: angle 0x401 - 5842; A4: phase + 2 - 5978; A5: circle - 0x5000 - 6000; A6: angle & 0x1FFF - 6000; A7: phase test 1 - 2308; A8: speed + 0x81 - 1541; A9: fly sar 5 - 6000; A10: al 2 - 6000; A11: phases past 4 not drawn - 6000; A12: Sprite_Current read before Math_Sin - 4316; A1b: climb of v - 1: truncation shows - 5783 |
| `EffectKind43_SparkDraw` | K1: x 0x3C0 - 6000; K2: << 6 - 3928; K3: x - 0.5 - 6000; K4: h 4.0 - 6000; K5: link 0x18 - 6000 |
| `EffectKind6B_Capture` | CP1: clear h 0x80 - 4471; CP2: | 0x80 - 2215; CP3: +9 3 - 4518; CP4: release the wrong record - 4493; CP5: copy 0x7C - 4510 |
| `EffectKind6B_Store` | ST1: y 0x101 - 2085; ST2: to + 2 - 1998 |
| `EffectKind6B_Scatter` | SC1: column + 1 - 1270; SC2: height + 1 - 620; SC3: + row - 728; SC4: speed & 0x3F - 1300; SC5: pixels 4 apart - 1265; SC6: depth from y - 1250; SC7: h 0 enters (equivalent: the row loop is a do-while on h) - 1265 |
| `EffectKind6B_Arm` | AR1: flags 1 - 5004; AR2: sound 0x201 - 6000 |
| `EffectKind6B_Fall` | FA1: | 0x20 - 4479; FA2: flags + 0x20 - 2854; FA3: speed + 3 - 3589; FA4: height + 1 - 69; FA5: green >> 3 - 4599; FA6: one fewer - 2505; FA7: release inverted - 6000; FA8: x * 0.5 - 4415 |
| `EffectKind44_Start` | E44S1: top 0x400 up - 6000; E44S2: shade 0x21 - 6000; E44S3: +9 0x11 - 6000; E44S4: ring cell + 4 - 5967 |
| `EffectKind44_FadeIn` | E44I1: shade + 2 - 6000; E44I2: +9 0x77 - 1892 |
| `EffectKind44_Sparks` | E44P1: +9 0x1F - 1782; E44P2: emit before step - 6000 |
| `EffectKind44_Widen` | E44W1: + 0x31 - 1879; E44W2: clamp 0x17F - 3365; E44W3: +9 0x79 - 1698 |
| `EffectKind44_Follow` | E44F1: z from height - 6000; E44F2: +9 0x11 - 1759 |
| `EffectKind44_FadeOut` | E44O1: shade - 4 - 6000; E44O2: release at 1 - 2609 |
| `EffectKind44_RingProject` | RP1: x sar 5 - 5229; RP2: angles 0x80 apart - 6000; RP3: centre to +0x2C - 6000 |
| `EffectKind44_RingDraw` | RD1: lowest + 1 - 5997; RD2: side inverted - 6000; RD3: middle of i alone - 4792; RD4: below to 0xEF - 6000; RD5: j & 7 - 6000; RD6: highest - 1 - 5935 |
| `EffectKind44_RingCone` | RC1: >= 0 - 1; RC2: shade +0x23 - 5355; RC3: winding order - 6000 |
| `EffectKind44_SparksClear` | SL: the defect fixed: all eight cleared - 6000 |
| `EffectKind44_SparksStep` | SS1: x by vz - 5981; SS2: dies at 1 - 5504; SS3: cursor not read after the trail - 282 |
| `EffectKind44_SparkTrail` | TR1: red fades by half - 5948; TR2: two shifted - 6000; TR3: v3 from a - 6000; TR4: v3 red as green - 5984 |
| `EffectKind44_SparkEmit` | E1: every other frame - 466; E2: top 0x500 up - 2297; E3: speed angle + 0x400 - 2299; E4: another colour - 2299; E5: life 0x21 - 2367; E6: two copies - 2452; E7: ground x << 7 - 2452 |
| `EffectKind44_SparkFind` | SF: seven searched - 1516 |


## 5. What the cut and the tool said, settled

- **Extents.** `band_rows.py`'s extents are right for every row: 26 of the
  cut's sizes are padding to the next 16-byte start, and `0x4762D0`'s 944
  spans `0x476560`, kind 0x44's dispatcher and its first two states (ours
  0x290: the code to the `ret` at `0x476548` and its five-case jump table at
  `0x47654C`).
- **Added: `0x476560`** (0x114 bytes), called by `0x4762D0` only, "code no list
  has" - `EffectKind43_SparkDraw`. **Added: `0x476680`** (0x12), kind 0x44's
  dispatcher, `Effect_KindHandlers[0x44]`: a catalog part-2 row ("Table
  Effect_KindHandlers", hidden in `0x4762D0`) the cut does not list, found by
  scanning `Effect_KindHandlers` for entries in the band (the brief's
  addendum). Its table's cell `0x6544D4` is named by it alone.
- **No start dropped.** None of the 51 is a case, a shared tail or a second
  entry: each is reached by a `.data` cell or an `E8` from this group's own
  code, and none is reached by a `jmp` (the `jmp` in `0x4762D0` goes through
  its own table).
- **The cut's `unit` and `unit_desc` are wrong for 30 rows**: `0x474F60`,
  `0x474FC0` are kind 0x3E's states (not kind 0x35's run `0x654410`); the
  kind-0x40 and kind-0x42 rows are not `Fn_474FC0`'s; the rows from `0x475D10`
  on are kinds 0x43, 0x6B and 0x44 (not one run `0x6544AC`): section 3.
- **The labels** "world 1 / 2 / 3" are the PSX twins' overlays; the twins
  (`0x801F2F30`, `0x801F311C`, `0x801F3468`, `0x801F2DE0..0x801F3B98`, 14 in all)
  are AREA overlay copies with no name in the sibling - cited in the evidence
  strings, the names ours.

## 6. Aborts

Where the original reads or jumps through what it indexes past, ours aborts
(`bof3::Fatal`) with a message naming the function and this section; the fuzz
keeps every one of them out of reach (its seeds), and no ordinary play reaches
any:

- **A state byte past its table** (each dispatcher, and `EffectKind3E_Bolts`'
  sub-state): only the kind's own states write `+1` / `+2`, inside their tables.
- **`EffectKind40_ShardDraw` with a shard's height 0**: the original's `idiv`
  faults. A shard starts at 0x280 and is stepped down 0x20 a frame; it is taken
  out of use below 0x80 (at 0x60) and drawn once more there: its height is
  never 0.
- **`EffectKind6B_Scatter` / `_Fall` past the 80 column heights** (a column of
  0x50 or more): the rectangle's w is 0x50, so the column is below it.
- **`EffectKind6B_Scatter` with w or h above 0xFF**: the row and column are
  bytes, so the original never ends; the rectangle is 0x50 x 0x48.
- **`EffectKind6B_Capture` with `Effect_FindFree` above 19**: the callee
  answers 0..19 or 0xFF.

## 7. Latent defects (Capcom's, described, not fixed)

- **`EffectKind44_SparksClear` clears one spark of eight.** It sets the cursor
  `0x6761B8` to the first spark record and then clears the in-use byte the
  cursor points at eight times, never moving it. Sparks 1..7 keep whatever
  their bytes held: the records lie inside the shared buffer at `0x92BF80`
  (kind 0x43's spark records 9..32, kind 0x6B's read-back pixels, kind 0x40's
  shards and disc), so a kind-0x44 ring started after one of those kinds ran
  may step and draw up to seven stale "sparks" - their life byte up to 0xFF
  frames, their speed and colour whatever lay there. Kind 0x44's first state
  and its fade-in both call it. Reach: area 78 / 80's choice (section 9);
  whether stale bytes are left there in play depends on what ran before - not
  measured.
- **`EffectKind6B_Scatter` writes particles without a bound.** Its rectangle is
  0x50 x 0x48 (5,760 pixels); each pixel not 0 becomes a particle of 0x14 at
  `0x92EC80 + 0x14 n`. The 1,882nd lands on `0x937F84` (`Gfx_CurrentEnv`,
  then `Sprite_Current`, `Frame_Counter`, `MapView_CellItems`, the desktop
  cells at `0x939A2C..`): a captured sprite with more than 1,881 opaque
  pixels (of 5,760) overwrites `Sprite_Current`'s bytes 1..3 with a count and a
  colour, and the next state's `Sprite_Current` access goes astray. What the
  PSX reserved there is not read here; on the PC the unnamed room before
  `0x937F84` holds 1,881. Reach: chapter 10's run 13 captures sprite record 2;
  its pixel count is the sprite's - not measured.
- **`EffectKind43_SparksRun` phases above 4 never end.** A spark whose phase is
  5 or more is drawn each frame and never taken out of use. The phase after
  rising is `1 + the record's +6 + 1`, and only kind 0x43's states write `+6`
  (0, 1, 2): not reachable from them.

## 8. Calls across groups

- **Out of the group, to groups of this round**: none (`band_rows.py
  --edges`: 27 edges, all to EGT's `EffectGte_LoadMapCamera` /
  `EffectGte_ProjectPoint` / `EffectGte_ProjectSize`, merged and ours, called
  by name). Kind 0x42's table holds `0x47EEC0` (group E2G's state 0): read in
  place, not called by ours.
- **Raw, to nobody's** (in `effect_2c_callees.h`, called through `SH_AT`):
  `0x59E930` (the VRAM shadow read back into memory), `0x4941B0` (the winding
  test, EGT's doc section 7), `0x5A7570` (libgpu's `SetPolyF3`).
- **Inbound from outside the group**: none - no call, `jmp` or pointer to any
  of the 53 outside this group's code and its tables (`band_rows.py` reach
  column, and a raw scan of the image for every address: only the
  `Effect_KindHandlers` cells and this group's tables).
- **Harness gaps met** (not edited; for the coordinator's fold): the
  standard rows of `EffectGte_ProjectPoint` / `EffectGte_ProjectSize` /
  `Gte_VectorNormal` log their pointers by value, which fails when a caller
  hands a local; `Sprite_UpdateScreen`'s logs nothing of the record it draws;
  the effect-standard `0x4941B0` writes where the real one only reads;
  `MapView_LinkPrimAt`'s row does not move the packet cursor (kind 0x43's
  spark draw writes its primitives at one place in the fuzz).

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
