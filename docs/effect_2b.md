# Group E2B: effect kinds 0x2F, 0x33, 0x35, 0x38, 0x39, 0x3B and 0x3D

**Status:** MEASURED (2026-09-29) - round thirteen, wave two
([`takeover-queue-round13.md`](takeover-queue-round13.md) section 10), on
the round branch's `dcef661`. **52 functions ours** (`src/game/effect_2b.cpp`,
declarations generated from `symbols.toml`, the module header
`src/game/effect_2b.h`, shadow name `effect_2b`): the cut's 51 rows for E2B
(`analysis/round13_cut.tsv`) and the one start inside them no list had
(`0x473F10`, kind 0x38's tile), each read to its last instruction with
capstone and fuzzed through the scenario harness's effect mode
([`scenario_harness.md`](scenario_harness.md) section 8), the harness
unchanged: 208,000 rounds, 0 mismatches. CONTROLS_SUMMARY All seven kinds are
fuzz only: no recorded route enters them (section 10).

Every row is effect code: seven kinds of the `Effect_Objects` pool (20
records of 0x80 at `0x7E11E0`; `Effect_RunObjects` calls
`Effect_KindHandlers[+5]` with `Sprite_Current` the record). No row is a
`hypothesis` row that turned out to be something else. The spawners
(a scan of `BOF3.exe` for `mov byte [reg + 5], kind`, 2026-09-29, and our
chapter code): kind 0x2F `Area27_SpawnEffect2F` and `Scena19_Run0` (on
`Input_Pressed` bit 8); kind 0x33 `Area26_PlaceEffect`; kind 0x35
`Area108_TailPlace`; kind 0x39 `Area175_SpawnEffect39` (its screen point
the spawner's `+0x2E` / `+0x30`); kind 0x3B `Scena07_Scene4` step 4 (at
sprite 0's point, the height up 0x100); kind 0x3D `Area47_SpawnEffect3D`;
kind 0x38 by nothing the scan finds (`Effect_Spawn`, the event ops' spawner,
takes its kind from script data). What each effect looks like on screen is
not stated here: the descriptions are of what the code draws.

| Kind | Dispatcher (`Effect_KindHandlers` cell) | Table (named here) | States |
|---|---|---|---|
| 0x2F | `EffectKind2F_Run` `0x4731A0` (`0x65540C`) | `EffectKind2F_States` `0x6543D8` (2) | `_Start`, `_Spin` |
| 0x33 | `EffectKind33_Run` `0x4734F0` (`0x65541C`) | `EffectKind33_States` `0x6543E0` (4) | `_Start`, `_Grow`, `Effect_StateRelease` twice |
| | | `EffectKind33_SignX` `0x6543F0`, `_SignZ` `0x654400` (4 longs each, data) | the quarters' link offsets |
| 0x35 | `EffectKind35_Run` `0x4737A0` (`0x655424`) | `EffectKind35_States` `0x654410` (3) | `_Start`, `_Burst`, `_FreeModel` |
| | | `EffectKind35_ShardStates` `0x65441C` (2, by a shard's +1) | `_ShardFly`, `_ShardFade` |
| 0x38 | `EffectKind38_Run` `0x473DD0` (`0x655430`) | `EffectKind38_States` `0x654424` (5) | `_Start`, `_FadeIn`, `_WaitMessage`, `_FadeOut`, `Effect_StateRelease` |
| 0x39 | `EffectKind39_Run` `0x473FA0` (`0x655434`) | `EffectKind39_States` `0x654438` (4) | `_Start`, `_Grow`, `_Shrink`, `Effect_StateRelease` |
| 0x3B | `EffectKind3B_Run` `0x4741E0` (`0x65543C`) | `EffectKind3B_States` `0x654448` (4) | `_Start`, `_Rise`, `_Fade`, `Effect_StateRelease` |
| 0x3D | `EffectKind3D_Run` `0x474940` (`0x655444`) | `EffectKind3D_States` `0x654458` (4) | `_Start`, `_Wait`, `_Glow`, `_Fade` |

No dispatcher bounds its index. Each table's length is read by hand
(`e2b/e2bdis.py`, scratch) to the next table a dispatcher or a call names:
`0x6543D8` two, then kind 0x33's `0x6543E0` four, then two tables of four
longs (1 / -1) `EffectKind33_DrawDisc` reads, then `0x654410` three, then the
shard states `0x65441C` two (called by `EffectKind35_ShardsDraw`), then kind
0x38's `0x654424` five, 0x39's `0x654438` four, 0x3B's `0x654448` four,
0x3D's `0x654458` four, then kind 0x3E's `0x654468` (`0x474F40`'s, the next
group's). `band_rows.py` names these cells `run 0x6543ac[11..14]` and
`run 0x654410[0..21]` (one run of code pointers through six tables) and
attributes nine rows to kind 0x2E's unit (`Fn_472060`): the cut's `unit`
column is a hint, and wrong here - they are kinds 0x2F's and 0x33's.

## 1. What each function does

Offsets are of the effect record (`Sprite_Current`) unless a record is named.
`+1` is the kind's state, `+9` a frame count, the point `+0x34` / `+0x38` /
`+0x3C` (x, z, height; the height's integer part in its high word). The
part records all lie in `EffectKind30_Shards` `0x92BF80` (section 7).

### 1.1 Kind 0x2F: three points circling down, each with a trail

Three trail records of 0x198 at `0x92BF80`: the centre `+0` / `+4`, the height
`+8`, the radius `+0x10`, an angle word `+0x14`, then 32 projected points of
three floats (screen x, y, depth) from `+0x18`.

- **`_Start` `0x4731C0`** (`EffectKind2F_States[0]`, hidden in E2A's
  `0x473100`'s recorded extent): each trail's centre (0x20000, 0xC0000),
  height 0xB0000, radius 0x20000, angle 0x1000 * i / 3 (0, 0x555, 0xAAA);
  `EffectKind2F_InitTrail`; then `Sound_PlayEffect(0x206)`, `+9` 0x40, `+1`
  up.
- **`_Spin` `0x473240`** (`[1]`): each trail four sub-steps (height down
  0x400, radius down 0x200, angle up 0x40, `EffectKind2F_StepTrail`), then
  `EffectKind2F_DrawTrail`. `+9` down; at 0, `Effect_Release` (kind 0x2F has
  no release state). Over its 0x40 frames the radius runs 0x20000 to 0 and
  the height 0xB0000 to 0x70000.
- **`_StepTrail` `0x4732D0`** `(trail)`: the 32 points moved down one (from
  the last, dword by dword); the head - `Math_Cos` / `Math_Sin` of the
  zero-extended angle word times the radius, `>> 12`, round the centre; the
  height `<< 8` - projected into the first after `EffectGte_LoadMapCamera`.
- **`_DrawTrail` `0x473360`** `(trail)`: a draw mode (the page at (0x380,
  0x100), abr 1, dtd 0) committed at slot 1; 31 semi-transparent `LINE_G2`
  from point j to j + 1, red `(0x2000 - 0x100 j) / 32` at the first end and
  `(0x1F00 - 0x100 j) / 32` at the second (each clamped to 0xFF by a 16-bit
  compare: only the first line's first end, 0x100, is), green and blue 0;
  each committed at slot 1 (0x24 bytes). A fading red tail.
- **`_InitTrail` `0x473460`** `(trail)`: the head projected into the first
  point and copied into the other 31.

### 1.2 Kind 0x33: a widening disc

- **`_Start` `0x473510`** (`[0]`): the disc's centre `+0xC..+0x14` the point
  `+0x34..+0x3C`, its radius `+0x1C` 0; `+9` 0xA, `+1` up.
- **`_Grow` `0x473550`** (`[1]`): `EffectKind33_DrawDisc(+0xC, +0x1C)`; the
  radius up 0x1199 (the record read before the call); `+9` down - at 0,
  `Sound_PlayEffect(0x20D)` and `+1` up. Ten frames; states 2 and 3 are
  `Effect_StateRelease` (3 unreachable).
- **`_DrawDisc` `0x4735B0`** `(centre, radius)`: `EffectGte_LoadMapCamera`;
  four quarters `EffectKind33_DrawQuarter(centre, radius, 0x400 i,
  EffectKind33_SignX[i], EffectKind33_SignZ[i])`.
- **`_DrawQuarter` `0x473600`** `(centre, radius, angle, dx, dz)`: the
  centre projected; the point at the angle's low 16 bits and the radius
  round it (`>> 12`; the height the centre's, read once) projected; then four
  times: a draw mode (page (0x380, 0x100), abr 2, dtd 0) linked at (x + dx,
  z + dz) by `MapView_LinkPrimAt(.., 0, 0x38)`; a semi-transparent `POLY_F3`
  (the library's `0x5A7570`) at `Gfx_PacketNext` (read after the link) from
  the centre to the last point and the point 0x100 further round (the angle
  kept whole in its argument's slot), grey 0x80, linked the same. Sixteen
  flat triangles in all; the four quarters' link points one unit apart so
  each quarter sorts on its own side of the centre.

### 1.3 Kind 0x35: kind 0x1E's break-apart on the second extra sprite

The same code as E1C's kind 0x1E ([`effect_1c.md`](effect_1c.md) 1.3),
instruction for instruction except where named (a diff of the two
disassemblies, `e2b/cmp.py`): the model is `Sprite_ObjectsExtra[1]`'s (`+0x50`
at `0x8020F4`) with **55** faces instead of 27, **sixteen** shards instead
of eight, its own cursor `EffectKind35_ShardCursor` `0x676108`, the pieces at
`0x92C200` (0x18 each) and the face copies at `0x92C728` (0x28 each, to
`0x92CFC0`).

- **`_Start` `0x4737C0`**: `_ShardsInit`, `_SplitModel`; `+1` up (no debris,
  no line pass: kind 0x1E's `_Start` / `_Glow` have them).
- **`_Burst` `0x4737E0`**: `_StepPieces`; `_ShardsDraw` answering 0, `+1` up.
- **`_FreeModel` `0x473800`**: `Sprite_ObjectsExtra[1]`'s `+0` (`0x8020A4`)
  0; a tail jump to `Effect_Release`.
- **`_ShardsInit` `0x473810`**: the cursor at `0x92BF80`, sixteen times E1C's
  `EffectKind1E_ShardInit(cursor)` (ours, called by name) and the cursor up
  0x28.
- **`_ShardsDraw` `0x473850`**: `EffectGte_LoadMapCamera`; each shard in use
  stepped by `call [EffectKind35_ShardStates + 4 * +1]` and drawn by
  `_ShardQuad`; `al` 1 when any was in use.
- **`_ShardQuad` `0x4738A0`** `(shard)`: kind 0x1E's, but the page's abr 2
  (`Gpu_GetTPage(0, 2, 0x2C0, 0x100)`) instead of 1.
- **`_ShardFly` `0x473A90`**, **`_ShardFade` `0x473B00`**: kind 0x1E's two,
  on this cursor. Kind 0x1E's two unreachable "next" states have no
  counterpart: this table has two entries.
- **`_SplitModel` `0x473B60`**: kind 0x1E's with 55 faces and the normalised
  centre `>> 7` (kind 0x1E's `>> 8`: the pieces fly twice as fast); the
  vertices made relative in a loop rather than unrolled.
- **`_StepPieces` `0x473C80`**: kind 0x1E's with the z velocity up 1 every
  frame (kind 0x1E's by the frame's parity: this falls twice as fast).
- **`_TurnPiece` `0x473CE0`** `(copy, face, piece)`: kind 0x1E's as a loop
  of four vertices; the same calls in the same order, Gte_RotTrans handed the
  third argument's slot as its flag.

### 1.4 Kind 0x38: a screen-wide tint round a message

- **`_Start` `0x473DF0`**: `+9` 0x10, `+1` 1, the colour `+0x5D..+0x5F` 0.
  Draws nothing.
- **`_FadeIn` `0x473E30`**: `+9` down - not 0, the three colour bytes up 6;
  at 0, `Msg_OpenScript(0x1F)`, `Field_Request` 2, `+1` 2. The tile drawn
  (a tail jump to `_DrawTint`).
- **`_WaitMessage` `0x473E90`**: `Field_Request` no longer 2, `+9` 0x10 and
  `+1` 3. The tile drawn.
- **`_FadeOut` `0x473EC0`**: `+9` down - not 0, the colour down 6; at 0, `+1`
  4 (`Effect_StateRelease`). The tile drawn (its own tail).
- **`_DrawTint` `0x473F10`**: `Gpu_SetDrawMode(packet, 0, 1, 0x4F, 0)`
  committed at slot 2; a semi-transparent untextured `TILE` at `(0, 0)`,
  320.0 x 240.0 (the floats `0x43A00000` / `0x43700000`), coloured
  `+0x5D..+0x5F`, committed at slot 2 (0x1C bytes). The colour rises to 0x5A
  over fifteen frames, holds while message 0x1F is up, falls back.

### 1.5 Kind 0x39: a fan at a screen point

- **`_Start` `0x473FC0`**: `+9` 0, `+1` up, `Sound_PlayEffect(0x20E)`.
- **`_Grow` `0x473FE0`**: `_DrawDisc(+0x2E, +0x30, +9)` (each pushed with its
  register's upper half; only the words are read); `+9` up - past 0x1E, 0x1E
  and `+1` up.
- **`_Shrink` `0x474030`**: the same draw; `+9` down - at 0, `+1` up.
- **`_DrawDisc` `0x474070`** `(x, y, radius)`, three s16: a draw mode (page
  (0x3C0, 0), abr 1, dtd 1) committed at slot 1; 32 semi-transparent G3
  triangles fanned round the screen point, the rim at the radius (trig * r
  `>> 12` + the centre, an int then a float), grey 0x80 at the centre and
  black at the rim, no depth written; each committed at slot 1 (0x34).

### 1.6 Kind 0x3B: a glow and three spirals

Three spiral records of 0x4BC at `0x92BF80`: centre `+0` / `+4`, height
`+8`, words angle `+0x10`, radius `+0x12`, shade `+0x14` (the rest unused by
this code).

- **`_Start` `0x474200`**: `_InitSpirals`; `+9` 0x80, `+0xA` 0, `+1` up;
  `Sound_PlayEffect(0x200)` and `(0x20A)`.
- **`_Rise` `0x474240`**: a draw mode (page (0x380, 0x100), abr 1, dtd 1)
  committed; `_DrawGlow(a copy of the point, +0xA << 2, 0x80, 0)`;
  `EffectGte_LoadMapCamera`; each spiral drawn, its shade up 4, the record's
  `+0xA` up 1 (three times a frame) - past 0x20, 0x20 and that spiral's
  shade 0x80 -, its angle up 0x40, radius down 0x18, height down 7.0 (`+
  0xFFF90000`); `+9` down (the record last read in the loop) - at 0, `+9`
  0x20 and `+1` up.
- **`_Fade` `0x474340`**: the glow at `+9 << 2`; each spiral drawn, angle up
  0x40, shade down 8; `+9` down - at 0, `+1` up.
- **`_DrawSpiral` `0x474410`** `(spiral)`: 32 semi-transparent G4 quads. The
  first point at the angle and radius (s16; trig * r `>> 4`) round the
  centre at the height, projected into both edges; for each quad the angle
  down 0x20 and the radius up 0x18, the next point; the direction from the
  last point (z 0) normalised in place (`Gte_VectorNormal`) and the square
  root (`0x5A7A90`) of its answer added to a running length L; the two new
  edges the point plus and minus the perpendicular times `Sin(0x40) * L /
  Cos(0x80) >> 12` (both trig calls made afresh for each of the four
  products), projected; the quad from the last edges to the new, the first
  two corners the shade clamped to 0..0xFF and the last two that less 4; each
  committed at slot 1 (0x44). A ribbon that widens as it winds out.
- **`_InitSpirals` `0x474770`**: the spirals round the record's point (read
  once), the height `+0x3C` + 0x380 (`<< 16`), radius 0xC00, shade 0, angle
  0x1000 * i / 3.
- **`_DrawGlow` `0x4747D0`** `(point, size, centre, rim)`:
  `EffectGte_LoadMapCamera`; the point projected; the size's word scaled at
  its depth (`EffectGte_ProjectSize(point, in, in + 2)` - the original's
  second `in` word is a stack leftover, section 7) plus the frame's parity;
  32 semi-transparent G3 triangles fanned round the screen point at steps of
  0x80 (the first angle whole, the second `& 0xFFF`), the centre's shade the
  `centre` byte and the rim's the `rim` byte, the depth at each vertex;
  committed at slot 1 (0x34).

### 1.7 Kind 0x3D: a glow at the leader, rings rising

Sixteen ring records of 0x18 at `0x92BF80`: the point `+0..+8`, the width
`+0x10` and half-thickness `+0x12` (words), in use `+0x14`, state `+0x15`,
count `+0x16`, shade `+0x17`.

- **`_Start` `0x474960`**: the timer `+0xC` (a dword) 0x3C, `+1` up.
- **`_Wait` `0x474980`**: the timer down - at 0, 0x230, the rings cleared
  (`_RingsClear`), `+1` up.
- **`_Glow` `0x4749B0`**: `_DrawGlow(0x80)`, `_RingsStep`, every eighth frame
  (`Frame_Counter & 7` 0) `_RingSpawn`; the timer down - at 0, 0x10 and `+1`
  up.
- **`_Fade` `0x474A00`**: the timer down while above 0 (signed); the glow at
  the timer's word `<< 3`; `_RingsStep` answering 0, a tail jump to
  `Effect_Release`.
- **`_DrawGlow` `0x474A40`** `(size)`: a draw mode (page (0x380, 0x100), abr
  1, dtd 1) committed; `_DrawFan(the leader's point - ObjTrio +0x34, +0x38,
  +0x3C + 0x800000 -, size, 0x60, 0)`.
- **`_DrawFan` `0x474AC0`**: `EffectKind3B_DrawGlow` with twice the frame's
  parity in the radius (a diff: nothing else differs).
- **`_RingsClear` `0x474C30`**, **`_RingSpawn` `0x474C50`** (the first free
  ring in use, state 0; none free, nothing).
- **`_RingsStep` `0x474C80`**: each ring in use by its state - 0: width 0,
  half-thickness 0x10, shade and count 0x20, state 1; 1: the point the
  leader's (height + 0x800000), width up 8, shade and count down (count 0:
  freed), `_DrawRing`; another: nothing. `al` 1 when any was in use.
- **`_DrawRing` `0x474D20`** `(ring)` (PSX twin `0x801F2F60`,
  `pairs_propagated.json` by callers; the sibling names it nothing): the
  ring's point projected; twice - the half-thickness added, then taken off
  -: `{w + h, w}` scaled at the depth (`EffectGte_ProjectSize`, out into the
  original's argument slot), and 32 semi-transparent G4 quads round the
  screen point, the outer corners (at `w + h`) black and the inner (at `w`)
  the shade; committed at slot 1 (0x44). A ring bright at its middle radius
  and fading to both edges.

## 2. What the cut and the tool said, settled

- **52, not 51.** `0x473F10` (0x86 bytes) is the tail of `0x473EC0`
  (`EffectKind38_FadeOut`): `0x473EC0` falls into it by a `jmp` over nothing,
  and `0x473E30` and `0x473E90` end with a `jmp` to it - three tail jumps,
  no call, no cell. It has no frame of its own but ends in its own `ret`
  with the stack balanced, so it is a function its three callers tail-call,
  not a conditional branch into a neighbour's body (the tool did not refuse
  it: every jump into it is unconditional and the last instruction of its
  caller). Taken as `EffectKind38_DrawTint`, a clone and an inject of its
  own; `_FadeIn` and `_WaitMessage` call it by name (`kPhase`, as the tool's
  clone of each re-aims the `jmp`), `_FadeOut` runs it in place (its clone
  is the tool's extent 0xD6 with the tail inside). The harness chapter
  (8.5) listed it among the second entries.
- **Extents.** The tool's read extents are right for all 51; the cut's sizes
  are the catalog's, padding included (29 differ by padding only).
- **Hidden starts.** 30 rows are hidden in a recorded host: the seven
  dispatchers and their states in `0x473100` (E2A's), `0x473460`,
  `0x473600`, `0x4738A0`, `0x473CE0`, `0x474070`, `0x4747D0` - each an entry
  by its own `.data` cell, none a fall-through of its host.
- **No case, no dropped start.** Nothing in the band that no list holds but
  `0x473F10`; every `Effect_KindHandlers` cell that points into the band
  (0x2F, 0x33, 0x35, 0x38, 0x39, 0x3B, 0x3D) is one of the seven
  dispatchers; kind 0x3E's `0x474F40` (at the band's end, `0x474F34`) is the
  next group's.

## 3. Calling convention, arguments, answers

All cdecl. The state handlers and dispatchers take nothing. The thirteen
with arguments: the trail's three `(trail)`; `_DrawDisc (centre, radius)`;
`_DrawQuarter (centre, radius, angle, dx, dz)` (the angle's low word read,
then kept whole in its slot); `_ShardQuad (shard)`; `_TurnPiece (copy, face,
piece)`; `EffectKind39_DrawDisc (x, y, radius)` (three words read); 
`_DrawSpiral (spiral)`; `EffectKind3B_DrawGlow` / `EffectKind3D_DrawFan
(point, size, centre, rim)` (a point, a word, two bytes);
`EffectKind3D_DrawGlow (size)` (a word, read by `_DrawFan`);
`_DrawRing (ring)`. Answers read by a caller: `EffectKind35_ShardsDraw` and
`EffectKind3D_RingsStep` in `al`.

## 4. The fuzz (`effect_2b_fuzz.cpp`)

`scenario_harness::Run` in effect mode (`g.effect`, kinds 0x2F, 0x33, 0x35,
0x38, 0x39, 0x3B, 0x3D), 4,000 rounds a function. The 39 without arguments
are `kEffect` clones, each with its kind (`+5`) and its table's length as
`state_span` (2, 4, 3, 5, 4, 4, 4); the thirteen with arguments `kCall`, the
records pointed into by `Args` (a trail, a shard, a spiral, a ring; the
disc's centre an effect record's `+0xC`; `_TurnPiece` a copy, the model's
face and its piece; the two glows' point a scratch buffer, `ArgAt(0,
kScratch)`). `ret_mask` `0xFF` on the two answering in `al`.
`BOF3X_E2B_ONLY=<name>` runs the clones whose name contains it.

**Tables swapped** (`DataTable`, both sides): `0x6543D8` (2), `0x6543E0`
(4), `0x654410` (3), `0x65441C` (2), `0x654424` (5), `0x654438` (4),
`0x654448` (4), `0x654458` (4). The sign tables are read in place.

**Callees.** The group's own called directly by name (`kPhase` for the void
ones without arguments, `kFlag` for the two answering `al`; the angle of
`_DrawQuarter` masked to 16 bits, the three words of
`EffectKind39_DrawDisc` to 16 each (the callers push whole registers), the
glows' point hashed (12 bytes, it is on the caller's stack) with the size
word and the two shade bytes masked); E1C's `EffectKind1E_ShardInit` by
name. **Re-listed louder** than the standard rows:
`EffectGte_ProjectPoint` (the point hashed, the three floats of `out`
filled; the standard row logs both pointers - on the stack here - and fills
nothing), `EffectGte_ProjectSize` (the point hashed; the size's first word
only where `out` is four bytes past `in` - the glows, whose second word is
the original's stack leftover - both words elsewhere; `out` filled),
`Gte_VectorNormal` (both pointers on the stack: `in` hashed, `out` filled,
neither logged by value - the standard row logs `out`'s address),
`Gte_RotTrans` (the SVECTOR hashed to 6 bytes), `Gte_SetTransMatrix` (the
translation noted), `Gte_SetRotMatrix` (18 bytes), and `Math_Cos` answering
anything but 0 and -1 (`_DrawSpiral` divides by `Math_Cos(0x80)`; the real
one answers a cosine near 0xFB1, and a garbage 0 would fault the original's
copy).

**Regions** beyond effect mode's: `0x92C5C4..0x92CFC0` (the rest of the
shared pool past the standard 0x644: kind 0x35's pieces and copies, kind
0x3B's spirals), the shard cursor `0x676108`, the fuzz's own 0x898-byte model.

**Seeds**: `Sprite_ObjectsExtra[1]`'s `+0x50` at the fuzz's model; the shard
cursor at one of the sixteen; `+9` at 0, 1, 2, 0x1D..0x1F, 0x10, 0x40, 0x80,
0xFF or random (each compare's boundary); `Frame_Counter`'s low three bits
0 or 1 half the time. Per function: kind 0x35's shards half in use, their
state below 2, their count 1, 2, 0 or any; `Field_Request` 2 half the time
for `_WaitMessage`; kind 0x3B's `+0xA` at 0, 0x1F, 0x20, 0x21, 0xFF or any
and the spirals' shade at the clamps' edges (0, 3, 4, 0x7C, 0x80, 0xFF,
0x100, 0x103, 0xFFFF, 0x8000); kind 0x3D's timer at 0, 1, 2, 0x80000000, -1,
0x10 or any and the rings a third free, state 0, 1 or another, count 1, 2, 0
or any.

**Disturbance** (the group's case, from its hash only): `+9`, `+0xA`, the
timer `+0xC`, `Field_Request`, a shard's in-use byte, state (below 2) and
count, a ring's in-use byte, state and count, a spiral's shade. Not the
shard cursor (E1C's reason: the originals' handlers never move it).

**Result** (2026-09-29, this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=effect_2b`, exit 0): **208,000 rounds over 52 functions,
9,302,326 calls to the stand-ins, 0 mismatches**; 29,516 bytes in 48
regions and the log compared. Every table entry reached (the 28 `phase`
lines of the coverage: 757..16,080 each, `Effect_StateRelease` 4,835), every
callee reached (the rarest: `Msg_OpenScript` 665, `EffectKind3D_RingsClear`
1,002, `EffectKind3D_RingSpawn` 1,169).

STAR_RESULTS

## 5. Divergence

None. The 52 are faithful; `DIVERGENCE.md`, `cheats.cpp` and
`widescreen.cpp` name no byte in `0x4731A0..0x474F34` (grep, 2026-09-29).
Where the original would jump through a table past its code, or divide by a
zero `Math_Cos(0x80)`, ours aborts with a message (section 7) - the project's
rule for a fault.

## 6. Calls across groups

**Out of E2B**: none raw to another group of this round
(`band_rows.py --edges`: 24 edges to EGT's four helpers and one to E1C's
`EffectKind1E_ShardInit`, all merged and called by name). The library
layer's `0x5A7570` (`POLY_F3`'s setter) and `0x5A7A90` (an integer square
root) are nobody's, called raw (`effect_2b_callees.h`).

**Into E2B from outside**: no code. The engine reaches the seven dispatchers
through `Effect_KindHandlers[0x2F, 0x33, 0x35, 0x38, 0x39, 0x3B, 0x3D]` and
every state through its table's cell (in place; `Inject`'s detour at each
entry serves them). The spawners (the preamble) write the kind byte; none
calls into the band.

## 7. Latent defects (Capcom's, described, not fixed)

- **One pool, many layouts.** `EffectKind30_Shards` `0x92BF80` is laid over
  by kind 0x2F's three trails (to `0x92C448`), kind 0x35's sixteen shards,
  55 pieces and 55 face copies (to `0x92CFC0`), kind 0x3B's three spirals (to
  `0x92CDB4`) and kind 0x3D's sixteen rings (to `0x92C100`) - as by E1C's
  kinds 0x1C..0x1F and FC2's kind 0x30. Two of them alive at once write over
  each other; nothing in the code prevents it. `symbols.toml` gives the pool
  1,344 bytes (to `0x92C4C0`); kinds 0x35 and 0x3B use past that.
- **The unbounded dispatchers** (seven) and `EffectKind35_ShardsDraw`'s
  `call [0x65441C + 4 * +1]`: a state byte past its table jumps through the
  next table's cells or data. Ours aborts.
- **`EffectKind3B_DrawGlow` and `EffectKind3D_DrawFan` hand
  `EffectGte_ProjectSize` an uninitialised word**: only `in[0]` is written;
  `in[1]` is whatever the stack held, and the helper scales it into `out[1]`,
  which neither reads. Harmless (no divide by it, and a depth of 0 is the
  helper's own abort); ours writes 0 there.
- **`EffectKind3B_DrawSpiral` divides by `Math_Cos(0x80)`**, a constant the
  compiler did not fold (four times a quad). Never 0 in play; ours aborts on
  0 and on the one quotient idiv cannot hold.
- **`EffectKind3B_Rise` steps `+0xA` three times a frame**, once per spiral,
  and resets only the spiral it is on to shade 0x80 when it caps: the three
  spirals' shades are not stepped alike. Reproduced; whether that is the
  intent is for the owner's eye.
- **Dead states.** Kind 0x33's state 3 (`Effect_StateRelease` twice: state 2
  releases first); a ring whose state is past 1 stays in use for ever,
  undrawn, and keeps `EffectKind3D_RingsStep` answering 1, so kind 0x3D
  never releases - nothing writes such a state.
- **`EffectKind2F_DrawTrail`'s clamp** catches only the first line's first
  end (0x2000 / 32 = 0x100): the head is 0xFF, not 0x100's low byte 0.

## 8. Controls

`e2b/controls.py` (scratch): each plant replaces a string inside one function
of `effect_2b.cpp` (found after that function's own first line, or its
helper's), rebuilds, runs the self-test on the clone it touches
(`BOF3X_E2B_ONLY`), restores the file and rebuilds. The count is the rounds
that mismatched of 4,000. CONTROLS_DETAIL

CONTROLS_TABLE

## 9. The rebinding

- **In ours**: the eight state tables, the two sign tables and the shard
  cursor by name (`AddressOf(EffectKind2F_States)` ...,
  `EffectKind33_SignX` / `_SignZ`, `EffectKind35_ShardCursor`);
  `effect_2b_callees.h` keeps the two library callees and the unnamed pools
  and cells (`0x8020A4`, `0x8020F4`, `0x802D74`).
- **Comments in merged files** (the round-ten form): `effect_1c.cpp`'s
  mention of `0x473810` now names `EffectKind35_ShardsInit`.
- **Left raw**: `symbols.toml`'s evidence string for `EffectKind1E_ShardInit`
  cites `0x473810` as a measurement; left as written. No `_callees.h` of
  another group keys on an E2B address (`band_rows.py --refs`: the one
  comment above).

## 10. The live route

The catalog's reach columns (attract, shop, worldmap, combat) are empty for
all 51 rows, and no first-call trace under `analysis/calltrace` names any
of the 52 (a scan of the 673 files there: the addresses appear only in the
entry lists `entries*.txt`). **Fuzz only.** Kind 0x33 (area 26), 0x2F (area
27), 0x3D (area 47), 0x35 (area 108), 0x39 (area 175) and 0x3B (chapter 7's
scene 4) would be covered by a recorded route through those spawners and the
coordinator's frame-hash A/B.

## 11. For `analysis/calltrace/entries_logic.txt`

38 lines appended to the main checkout's file under a comment (the other 14
of the 52 were there with the same extent): the 30 hidden starts, the six
whose recorded extent ran over them (`00473460 143`, `00473600 20C`,
`004738A0 2BA`, `00473CE0 38A`, `00474070 391`, `004747D0 267`,
`00474D20 3AE` - each cut by these), `00473EC0 50` and `00473F10 86`
(new).
