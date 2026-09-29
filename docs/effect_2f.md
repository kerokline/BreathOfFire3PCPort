# Group E2F: effect kinds 0x4F, 0x50, 0x51, 0x52, 0x53, 0x56 and kind 0x4E's disc

**Status:** MEASURED (2026-09-29) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..12),
wave two, on the round branch's tip `dcef661`. **62 functions ours**
(`src/game/effect_2f.cpp`, declarations in `effect_2f.h`, shadow name
`effect_2f`): the cut table's 52 rows for E2F (`analysis/round13_cut.tsv`, the
band `0x47B7D0..0x47DAC0`), `0x47C4A0` (code no list has, in `0x47C370`'s
span) and nine catalogue rows of part 7 that no group of the cut holds but that
are states or callees of these kinds (`0x47C0F0`, `0x47C190`, `0x47C320`,
`0x47CF20`, `0x47CF40`, `0x47CFC0`, `0x47CFF0`, `0x47D010`, `0x47D8B0`: the
kinds taken whole, section 5). No start dropped: `0x47C420` (the cut's `NOTFN`
row) is case 3 of `EffectKind50_MoveSpecks`' switch, taken inside it. Each read
to its last instruction with capstone and fuzzed through the scenario harness
in effect mode ([`scenario_harness.md`](scenario_harness.md) section 8)
without edits to it: 372,000 rounds, 0 mismatches. CONTROLS_SUMMARY
**Fuzz only**: no recorded route enters any of the 62 (section 9).

All 62 are effect code, the four `hypothesis` rows of the labelling pass among
them (`0x47B7D0` kind 0x4E's disc, `0x47B970` and `0x47D600` two kinds'
dispatchers, `0x47C150` a kind-0x52 sub-state): none is left original.

| Kind | Functions | Reached through |
|---|--:|---|
| 0x4E (E2E's): the disc its states 1..3 draw | 1 | a call from E2E's `0x47B6E0`, `0x47B720`, `0x47B790` |
| 0x4F: two sprites tinted and set apart (FC1's `EffectKind3C_Start` with its own op), then FC1's `EffectKind3C_Hold` and E2D's `0x478160` | 2 | `Effect_KindHandlers[0x4F]` (`0x65548C`), `EffectKind4F_States` `0x6546A4` (3) |
| 0x50: specks thrown from the record's point in a turning direction, 64 of 0x28 at `EffectKind30_Shards` | 9 | `Effect_KindHandlers[0x50]` (`0x655490`), `EffectKind50_States` `0x6546C4` (3) |
| 0x51: a swaying column of quad pairs on a history of 65 angles, then a ring of sixteen quad pairs | 18 | `Effect_KindHandlers[0x51]` (`0x655494`), `EffectKind51_States` `0x6546D0` (12) |
| 0x52: sparks rising round the record, then a trail of 32 points dragged along two fixed paths | 21 | `Effect_KindHandlers[0x52]` (`0x655498`), `EffectKind52_States` `0x654700` (2), `EffectKind52_SparkSubStates` `0x654708` (3) and `EffectKind52_TrailSubStates` `0x654714` (6) by `+2`, `EffectKind52_SparkStates` `0x65472C` (3) by a spark's `+1` |
| 0x53: a textured beam from the top of the screen down onto an extra sprite | 4 | `Effect_KindHandlers[0x53]` (`0x65549C`), `EffectKind53_States` `0x654738` (2) |
| 0x56: a column of red crosses from `Sprite_ObjectsExtra[0]`'s height down to the ground | 7 | `Effect_KindHandlers[0x56]` (`0x6554A8`), `EffectKind56_States` `0x654740` (4) |

`Effect_KindHandlers[0x54]` and `[0x55]` point past the band (`0x47E680`,
`0x47EA90`, E2G's); `[0x4E]` is E2E's `0x47B630`. Every name is a hypothesis
from what the code does (`symbols.toml` status `hypothesis`): "speck", "spark",
"column", "ring", "trail", "beam", "cross" name the primitives the code builds,
not a play-tested look. **Who spawns them**, from our own sources (not traced
live): area 32 (`area_w0c.cpp`: kinds 0x4E, 0x4F and two of 0x50, the latter
with the running object's `+8` in `+6` - kind 0x50's direction test reads it),
area 34 (0x51 at state 6), area 108 (`area_w2f.cpp`: 0x51 at state 0xA), area
135 (`area_w3d.cpp`: 0x53), chapter 7's runs 1 and 3 (`scena_sc7.cpp`: 0x52 on
the counter's 4 and 7, and 0x51). No store of kind 0x56 was found in `src/game`.
Kinds 0x51 and 0x56 wait on the chapters' counter bytes `0x903848` (0xB) and
`0x90384B` (0xA, 0xE).

## 1. What each function does

`Effect_RunObjects` (ours) makes each live record of `Effect_Objects` (20 of
0x80 bytes, `0x7E11E0`) `Sprite_Current` and calls `Effect_KindHandlers[+5]`.
Each kind here is a four-instruction dispatcher - `mov ecx, [Sprite_Current];
xor eax, eax; mov al, [ecx + 1]; jmp [eax * 4 + T]`, no compare - and the states
its table names. Every function's comment in `effect_2f.cpp` is the full read,
each `symbols.toml` evidence string the summary; "Sprite_Current read afresh"
means the original reloads the pointer before an access (after a call the
harness's disturbance may have moved it) and ours reads it where the original
does. The record's words by kind: `+9` a frame count; `+0x5A` a timer word
(0x51, 0x52); `+0x32` a half-height or radius and `+0x2C` a sway width and
`+0x58` a sway angle (0x51); `+0xC` a direction (0x50); `+0x14` a ground height
(0x56); `+0x34..+0x3C` the world point; `+0x74..+0x7C` the screen point and
depth (0x4E, 0x51); `+0x5D` a red (0x56); `+0x60` a depth (0x53).

### 1.1 Kind 0x4E's disc, kind 0x4F

- **`EffectKind4E_DrawDisc(radius, centre, rim)` `0x47B7D0`**: a draw mode, then
  32 Gouraud triangles of a fan round the record's screen point (`+0x74`,
  `+0x78` copied; the rim `(cos, sin)(a) * radius sar 12` added as floats), the
  depth `+0x7C` at every vertex (after a `Gte_StoreDepthF` into the primitive
  that the copy overwrites). E1D's `EffectKind25_DrawDisc` with the point from
  the record instead of arguments.
- **`EffectKind4F_Run` `0x47B970`**, **`EffectKind4F_Start` `0x47B990`**: FC1's
  `EffectKind3C_Start` byte for byte but for its op, `EffectKind4F_Op`
  `0x6546B0` (named: 20 bytes to the next table) - two free sprites into `+3` /
  `+4`, an event op for each, moved apart and tinted red and blue; `+1 = 1`,
  which is FC1's `EffectKind3C_Hold`.

### 1.2 Kind 0x50

- **`_Run` `0x47BB70`**; **`_Start` `0x47BB90`** the specks cleared, the
  direction `+0xC` = 0; **`_Emit` `0x47BBB0`** one frame in four a speck, the
  specks moved, the direction turned 0x80, `+9` frames; **`_Drain` `0x47BC00`**
  the specks moved until none is left, then `Effect_Release`.
- **`EffectKind50_ClearSpecks` `0x47C350`**, **`_FreeSpeck` `0x47C6C0`** (the first
  free of 64, or null), **`_SpawnSpeck` `0x47C5F0`** (at the record's point, a
  step of 3 `(cos, sin)(e)`, `e` from the direction's sine plus 0x800 when the
  record's `+6` is 7, else 0x400).
- **`EffectKind50_MoveSpecks` `0x47C370`** (answers `al`): each speck's phase
  `+2` through a four-case switch (count, shade up, hold, shade down and free),
  drawn and moved. Its switch table sits at `0x47C48C`; case 3 is the cut's
  `NOTFN` row `0x47C420`.
- **`EffectKind50_SpeckQuad(speck)` `0x47C4A0`**: E1C's shard quad
  (`EffectKind1E_ShardQuad`) with the size `+0x24 shr 6`, the colour red only
  and committed in slot 1 instead of linked by depth.

### 1.3 Kind 0x51 (twelve states)

States 0..5 and 6..11 are two runs: **`_Start` / `_Start2`** (the history of
angles filled by 0x40 random steps; sounds 0x208 / 0x202), **`_Grow` /
`_Grow2`** (the column's half-height up to 0x20, the width up every fourth
frame; reload 0x96 / 0x2D), **`_Wait` / `_Wait2`** (the same code: until the
counter `0x903848` is 0xB), then **`_Shrink`** (the width down, sound 0x20C)
and **`_RingIn`** / **`_RingOut`** (the ring closing, then opening 0x10 a frame
to its release); or **`_RingFade`** (state 9) and **`_Burst`** (state 10: the
point projected, the ring at 0x140) / **`_RingClose`** (11). The helpers:
**`EffectKind51_DrawFrame` `0x47C810`** (project, `_DrawMoves`, push the sway,
the column), **`_DrawMoves` `0x47C6E0`** (two `Gpu_SetDrawMove` primitives of a
0x40 x 0x80 rectangle at the screen point: a copy of that part of the frame
buffer), **`_PushAngle` `0x47C7E0`** (`EffectKind51_Angles` `0x6761DC`, 65
words, named), **`_DrawColumn(half, sway)` `0x47C870`** (rows `0 .. 2 half - 1`,
each two textured quads whose half-width is the circle's chord `sqrt(half^2 -
k^2)` through the library's `0x5A7A90`, shifted by `sin(angle[i]) * sway`),
**`_DrawRing(radius)` `0x47CBD0`** (sixteen pairs of textured quads).

### 1.4 Kind 0x52

- **`_Run` `0x47C090`**; state 0 **`EffectKind52_Sparks` `0x47C0B0`** jumps by
  `+2`: **`_SparksClear`** (8 sparks cleared, the timer 0x40), **`_SparksEmit`**
  (a sound at 4, a spark every eighth frame set going by `0x4790F0`, the sparks
  moved), **`_SparksDrain`** (moved until none, then released).
- State 1 **`EffectKind52_Trail` `0x47C160`** calls by `+2` through
  `EffectKind52_TrailSubStates` - **`_TrailStart`** (the trail's width
  `0x92C49E` = 0xC0, the record at cell (0x6B, 0x46) on the ground), **`_TrailRise`**
  (z up a cell a frame for 0x20), **`_TrailTurn`** (to (0x64, 0x76)),
  **`_TrailFall`**, **`_TrailHold`**, **`_TrailFade`** (the width down 0x18 a
  frame, then released) - then **`_TrailUpdate(0x92C060)`** and
  **`_TrailDraw(0x92C060)`**.
- **`EffectKind52_ClearSparks` `0x47CF00`**, **`EffectSpark_FindFree` `0x47CF20`**
  (also E2D's and E2E's: the pool is shared), **`EffectKind52_MoveSparks`
  `0x47CF40`** (each spark in use called through `EffectKind52_SparkStates` by
  its `+1`, handed the spark, then drawn by `0x4792E0`), the spark states
  **`_SparkGlow`**, **`EffectSpark_Wait`** (also entry 1 of E2E's table
  `0x654660`), **`_SparkRise`**.
- **`EffectKind52_TrailUpdate(trail)` `0x47D040`**: 32 points of 0x20 move down
  one, the record's point enters at 0; each projected, its width scaled; 31
  angles from `Math_Ratan2` of the screen steps (0x1000 for a step of (0, 0),
  each of those then filled from the first later real angle, else the first
  earlier, else 0 - the original reloads its 0x1000 every step, so a fill does
  not change what the later ones are compared with); each point's angle the
  mean of its two neighbours' (E2E's `0x479970`).
- **`EffectKind52_TrailDraw(trail)` `0x47D220`**: a cap, then two Gouraud quads
  per pair of points whose screen x or y differ (the second a copy of the first
  with its offset side mirrored), shaded from 0x80 down 4 a drawn segment, and
  the end cap.
- **`EffectTrail_DrawCap(point, size, angle, shade)` `0x47D4F0`**: a half fan of
  eight triangles; E2E's `0x4796B0` and E3D's `0x4875C0` draw their trails' caps
  with it too.

### 1.5 Kind 0x53

**`_Run` `0x47D600`**, **`_Start` `0x47D620`** (E1A's `Effect1A_Kind01Start`
shape: `+0x54` from the extra sprite `+0x18` names), **`_Beam` `0x47D650`** (PSX
twin `0x801F2D20`, unnamed in the sibling): `Sprite_Current` is the extra
sprite while the record copies its point and the texture window is linked at
it; a quad from y 0 down to the sprite's projected attach point (x - 8 .. x + 8,
y clamped to 240), v scrolled by the record's `+9`; by its depth opaque,
half-transparent or not linked; `Sprite_Current` put back before the second
texture window. **`EffectKind53_TexWindow(x, y, w, h)` `0x47D8B0`**: the
8-byte rectangle and the code-0xF0 primitive `Window_DrawFrame` uses for a
texture window.

### 1.6 Kind 0x56

**`_Run` `0x47D910`**, **`_Start`** (placed, `+1 = 1`), **`_Wait`** (the counter
`0x90384B` at 0xA), **`_Arm`**, **`_Markers` `0x47D980`** (crosses from the
height down by 2^24 while above the ground, the red rising 0x2A to 0x80, then
an opaque one in 0xFF on the ground; back to state 1 at 0xE),
**`EffectKind56_Place` `0x47DA60`** (the record at `Sprite_ObjectsExtra[0]`'s
point, the ground under it), **`EffectKind56_DrawCross(x, z, height, semi)`
`0x47DAC0`** (two red lines of 0x8000 across the point).

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed (`DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no address of
the band nor its tables; checked 2026-09-29). Where the original indexes past a
table or pool, or would loop for ever, ours aborts with a `Fatal` naming the
function (round9 doc section 6); nothing in play reaches any of them:

- the eight state tables and the spark table (the originals jump or call
  through the dword after);
- a `Sprite_Objects` index past 30 in `EffectKind4F_Start` (from
  `Sprite_FindFree`, which answers 0..29 or 0xFF), an extra sprite index past 4
  in `EffectKind53_Start` / `_Beam` (the record's dword `+0x18`);
- `EffectKind51_DrawColumn`'s row past the 65 angles (a half-height above 0x20;
  past 0x80 the byte row counter would never reach its bound);
- `EffectKind56_Markers`' walk when the ground lies in the lowest 2^24 of the
  range (the heights it steps through repeat every 256 steps, so after 256 it
  never ends).

The x87 arithmetic is done in `long double`, which the compiler keeps on the
x87 at the control word's precision, as the originals' chains are; the pure
copies through the FPU (`fld` / `fstp` of a float, which quietens a signalling
NaN) are inline `flds` / `fstps` so the compiler cannot fold them into a move
(control 11); `_ftol` is the CRT's truncation, 0 in the low word for NaN and out
of range.

## 3. The arguments pushed with leftovers

| Callee | Pushed | Read by the callee |
|---|---|---|
| `EffectKind4E_DrawDisc` | radius `cx` (`movzx cx, byte +9`, or `+9 & 1 + 0x3C` in `edx`), 0x80, 0 | radius `movsx` 16 bits; centre, rim bytes |
| `EffectKind51_PushAngle` | `dx` / `cx` from the sway word, the upper half the register's | `mov ax, [esp + 4]` |
| `EffectKind51_DrawColumn` | `ax` (upper half `Sprite_Current`'s), `dx` | both `movsx` 16 bits |
| `EffectKind51_DrawRing` | `cx` / `ax` | `movsx` 16 bits |
| `EffectTrail_DrawCap` | the size word, the angle `cx - 0x400` / `ax + 0x400` (16-bit arithmetic, upper halves stale), the shade: 0x80, or a dword local whose upper three bytes were never written | size `movsx`, angle `and 0xFFFF`, shade its byte |
| `0x479970` (E2E's) | two angle words in whole registers | each `and 0xFFF` |
| `EffectKind56_DrawCross` | x, z, height dwords, 1 / 0 | semi `and 0xFF` |

The fuzz lists each with that mask.

## 4. The fuzz (`effect_2f_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_2f`, effect mode (`g.effect`, kinds 0x4F,
0x50, 0x51, 0x52, 0x53, 0x56), 6,000 rounds a function (`BOF3X_E2F_ONLY=<name>`
runs the clones whose name holds it). Shapes: 40 `kEffect` (each clone's own
kind; the eight dispatchers' `state_span` / `sub_span` their table's length - 3,
3, 12, 2, 3 (by `+2`), 6 (by `+2`), 2, 4), 22 `kCall` (the helpers). Eight
tables are `DataTable`s, swapped for recorders on both sides;
`EffectKind52_SparkStates` is left in place (its handlers take the spark as an
argument, which a handler recorder does not log), so both sides run Capcom's
three there, which write only the spark (compared). One `JumpTable`
(`EffectKind50_MoveSpecks`' four cases, moved into its copy). Regions beyond
effect mode's: the specks past `EffectKind30_Shards`' standard 0x644 (to
`0x92C980`) and `EffectKind51_Angles`.

**Callees**: the effect-standard rows for `Effect_Release`, `Sprite_FindFree`
(0..29 or 0xFF), `Sound_PlayEffect`, `Rand`, `AreaMap_Elevation`,
`MapView_GroundAt`, `MapView_LinkPrimAt`, `Math_*`, the `Gpu_*`,
`Gfx_CommitPrim` (moves the cursor), `EffectGte_LoadMapCamera`,
`MoveCmd_AttachOffset`, `Gte_StoreDepthF`, `Gte_PrimDepthFlat4_10`, `0x4790F0`,
`0x5A7840`, `0x5A7A90`; the CRT's `_ftol` `kThrough`. Re-listed in the group:
the nineteen of its own called by name (`kPhase` for the void ones, typed with
the masks of section 3 for the rest; `EffectKind50_FreeSpeck` and
`EffectSpark_FindFree` answer null a quarter of the time, else a speck or spark
record); `0x479970` (E2E's, masks `0xFFF`) and `0x4792E0` (nobody's, the spark
hashed to `+0x18`); `EffectGte_ProjectPoint` and `EffectGte_ProjectSize` (the
point and the outs are often stack locals, whose addresses differ between the
copy and ours: hashed, not logged; the size's second word hashed out - the
original never writes it in `EffectKind50_SpeckQuad`); `ProjectPoint`'s out
filled a quarter of the time with the point 0x20 below it exactly, a quarter
of the time with its x / y moved by -2..2 (so the trail's zero steps and
equal points happen), else fresh floats; `Gte_RotTransPers` (the vertex's
unwritten fourth word hashed out; the screen point fractions, y at and about
240, NaN and past 2^63 one time in eight; the answer at and about 0x1E0 /
0x220 half the time, from the log's noise only).

**Seeds** (per function, after the harness's fill): `+9` at 0, 1, 2, 0x20, 0x80,
0xFF; the timer `+0x5A` at 0..5, 8, 0x10, 0x20, 0x96, 0xFFFF; the screen point
`+0x74..+0x7C` half the time a coordinate with a fraction of 1/64ths; the
counter bytes at 0xB and 0xA / 0xE; `EffectKind4F_Start`: every record's `+3` /
`+4` inside the thirty (the disturbance moves `Sprite_Current` between its two
`Sprite_FindFree` calls, and the original reads the new record's `+3`); kind
0x50's specks and kind 0x52's sparks half in use, the phases and states inside
their switches and table, the counts at 0..2, all in use half the time for the
two finders; `SpawnSpeck`'s `+6` at 7; the trail's screen floats with equal
and near neighbours and angle words at 0x1000; the extra sprite index below 4,
its `+9` zero or not; kind 0x56's grounds near the heights and never in the
lowest 2^24. **Arguments**: the specks, sparks and trail by their addresses,
`_DrawColumn`'s half-height inside 0..0x20 (and 0, 0xFFFF, 0x8000),
`_DrawDisc`'s radius in 0..0x4B half the time. **Disturbance** (the group's,
from the hash only): `+9`, the timer, the half-height, the two counter bytes,
a screen float.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_2f`,
exit 0): 372,000 rounds over 62 functions, 10,253,313 calls to the stand-ins,
**0 mismatches**; 25,842 bytes of state in 47 regions. Every entry of the
eight swapped tables reached (each state's handler recorder 473..3,045 calls),
the spark states run for real in `EffectKind52_MoveSparks`, `Effect_Release`
8,343 calls, `Sprite_FindFree` 9,609, `0x479970` 180,000, `Math_Ratan2`
131,262, `EffectKind56_DrawCross` 73,622 (the walk's length).

STAR_RESULTS

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read the cut's 52 to the byte (7,836 bytes against
  the cut's 8,006): 37 differ by padding only; `0x47C370` by code - the cut's
  176 stops at its case 3 (`0x47C420`); the extent read, 0x12C, runs to the end
  of the switch table at `0x47C48C`. The recorded `entries_logic.txt` lines of
  `0x47B7D0` (0xB77), `0x47C370` (0x27E), `0x47CF40` (0xFF), `0x47D4F0`
  (0x3B1), `0x47D8B0` (0x1AB) and `0x47DAC0` (0x34E) span later starts: section 11.
- **Hidden starts**: 36 of the cut's rows (hosts `0x47B7D0`, `0x47D4F0`,
  `0x47D8B0`), each an entry by address - a cell of `Effect_KindHandlers` or of a
  state table -, none a case or a shared tail; none of the hosts reaches them by
  fall-through.
- **`0x47C420`** (`NOTFN`): case 3 of `EffectKind50_MoveSpecks`' switch
  (`jmp [eax*4 + 0x47C48C]`, entry 3); its call to `0x47C4A0` is the switch's
  common tail. Taken inside its host, not as a function.
- **`0x47C4A0`**, code no list has (the tool's flag): the speck quad, reached by
  one call from `0x47C370`'s tail. Taken.
- **Nine catalogue rows in no group** (part 7, "Unlabelled"; the labelling pass
  did not propose them as effect objects, so the join dropped them): four
  states of kind 0x52's sub-state tables (`0x47C0F0`, `0x47C190`, `0x47C320`),
  its spark finder and mover (`0x47CF20`, `0x47CF40`), its three spark states
  (`0x47CFC0`, `0x47CFF0`, `0x47D010`) and kind 0x53's texture window
  (`0x47D8B0`, called only by `0x47D650`). Taken with their kinds (the round's
  rule: a group takes its kinds whole); none is in another group's list.
- **Callees outside the band, not taken**: the spark init `0x4790F0` and the
  spark draw `0x4792E0` (catalogue part 7, in E2E's band `0x4789D0..0x47B790`
  but in no group's list) - called raw (section 8).

## 6. Controls

CONTROLS_TABLE

## 7. Latent defects (Capcom's, described, not fixed)

- **The dispatchers index unchecked** (eight tables, and
  `EffectKind52_MoveSparks`' call by a spark's `+1`). Every writer of the bytes
  in the band steps them inside their tables; ours aborts past them.
- **`EffectKind51_DrawColumn` trusts its half-height**: 65 angles and a byte row
  counter, so a half-height above 0x20 reads past `EffectKind51_Angles` and one
  above 0x80 never ends. The kind's states keep it within 0..0x20 (they reset it
  to 0 at states 0 and 6 and raise it once a frame for 0x20 frames); the ring
  states use the same word as a radius (0x140 at state 10) but never draw the
  column.
- **The effect pools overlap.** `EffectKind30_Shards` (`0x92BF80`) is kind
  0x50's 64 specks of 0x28 (to `0x92C980`), kind 0x52's eight sparks of 0x1C
  (to `0x92C060`) and, straight after them, kind 0x52's trail
  (`0x92C060..0x92C4A0`), as well as FC2's sparks and E1C's shards, pieces and
  debris. Two of these kinds alive at once overwrite each other's records -
  a kind-0x50 speck in use over the trail would be drawn from trail data and
  move it. Whether the game ever runs two together is the owner's to say.
- **Uninitialised words handed on**: `EffectKind50_SpeckQuad` hands
  `EffectGte_ProjectSize` a size whose second word it never wrote (the answer's
  second word is not read); `EffectKind53_Beam` hands `Gte_RotTransPers` a
  vertex whose fourth word it never wrote (not read); `EffectKind52_TrailDraw`
  pushes its shade as a dword whose upper three bytes were never written (the
  cap reads the byte). No effect.
- **`EffectKind56_Markers`' walk never ends** when the ground `+0x14` lies in
  the lowest 2^24 of the signed range (a ground of cell -0x8000); ours aborts.
- **Kinds 0x51 and 0x56 wait on the chapters' counters** (`0x903848` = 0xB,
  `0x90384B` = 0xA then 0xE): spawned where no event script sets them, they
  wait for ever (by design, not a fault).
- **`EffectKind4F_Start`** keeps FC1's `EffectKind3C_Start`'s behaviour: when the
  second sprite is not found the first is given back but `+3` keeps its index
  and the state stays 0, so the next frame tries again.

## 8. Calls across groups

**Outbound, raw** (`effect_2f_callees.h`): `0x479970` (E2E, wave two - one site,
run 30 times by `EffectKind52_TrailUpdate`'s loop), `0x4790F0` (the spark init, 1 site)
and `0x4792E0` (the spark draw, 1 site) - catalogue part 7, **in no group of the
round**: the coordinator's to place (both lie in E2E's band); the library
layer's `0x5A7A90` (fild, fsqrt, `_ftol`) and `0x5A7840` (a primitive holding a
rectangle's address). **By name, already ours**: EGT's `EffectGte_*` (14
sites), and the engine's. **Cells read in place**: `EffectKind4F_States[1]`
FC1's `EffectKind3C_Hold`, `[2]` E2D's `0x478160`.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Group | Site(s) | Callee |
|---|---|---|---|
| `0x47B6E0`, `0x47B720`, `0x47B790` (kind 0x4E's states 1..3) | E2E | `0x47B6F2`, `0x47B73B`, `0x47B7A2` | `EffectKind4E_DrawDisc` |
| `0x4796B0` (a trail draw) | E2E (in no list) | `0x47970C`, `0x47995E` | `EffectTrail_DrawCap` |
| `0x4875C0` | E3D | `0x48761C`, `0x487880` | `EffectTrail_DrawCap` |
| `0x4785B0` | E2D | `0x4785BB` | `EffectSpark_FindFree` |
| `0x478CC0` | E2E | `0x478CDA` | `EffectSpark_FindFree` |
| the table at `0x654660` (entry 1, cell `0x654664`), which E2E's `0x479260` calls through | E2E | - | `EffectSpark_Wait` |

`Effect_RunObjects` (ours) reaches the six dispatchers through
`Effect_KindHandlers` (read in place: no rebinding).

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 62. The first-call traces under `analysis/calltrace`
(`reach_dragon`, `reach_whelp`, the `ab*` runs; `*.tsv` searched for every
address of the band) enter none of them. **Fuzz only**: the coordinator's
frame-hash A/B covers none of the 62 until the owner records a route through
a place that shows these kinds (the spawners listed at the top: areas 32, 34,
108, 135, chapter 7).

## 10. The rebinding

`grep -rn -i` of the 62 addresses and the eight tables in `src/game` (and
`band_rows.py --refs`, which counts 0 for the cut's rows): the only raw
references are three rows of `scenario_harness.cpp`'s `kEffectStd` -
`FX_RAW(0x47CF20)` (`EffectSpark_FindFree`), `FX_RAW(0x47CF40)`
(`EffectKind52_MoveSparks`), `FX_RAW(0x47D8B0)` (`EffectKind53_TexWindow`).
**Left raw, for the coordinator**: the brief forbids a group to edit a harness;
the rows are keyed by address and still serve the E2D / E2E clones that call
`0x47CF20` by `E8` (the fold would spell them `FX_OURS(...)`, which keys them
by our function too). No `_callees.h` constant or fuzz key of another file names
an E2F address. **Beside it**: `symbols.toml`'s evidence for `Window_DrawFrame`
names `0x47DAC0` among its callers; it is not one (`EffectKind56_DrawCross`
draws lines) - the three calls near it are at `0x47DCC9`, `0x47DD19`,
`0x47DD69`, in E2G's `0x47DCA0..0x47DD40` rows. Left for the coordinator.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29, a commented block): 49
lines - the 36 hidden starts and the not-hidden ones not listed, and the smaller
extents of `0x47B7D0` (0x19D), `0x47C370` (0x12C), `0x47CF40` (0x73),
`0x47D4F0` (0x109), `0x47D8B0` (0x5A) and `0x47DAC0` (0x111), whose old lines
run over the next starts (left for the merger to drop); thirteen functions
already had their exact line.
