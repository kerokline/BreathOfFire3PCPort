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
without edits to it: 372,000 rounds, 0 mismatches. 126 controls planted one at a time: 125 refused by a count, one
equivalent with its near variant refused (section 6).
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
its `+9` zero or not and its `+0x14` at 0, 1, -1 and the sign's ends; a third of
`EffectKind52_TrailUpdate`'s rounds every trail point the record's (a world
point equal to the one below it projects to the same screen point); kind 0x56's grounds near the heights and never in the
lowest 2^24. **Arguments**: the specks, sparks and trail by their addresses,
`_DrawColumn`'s half-height inside 0..0x20 (and 0, 0xFFFF, 0x8000),
`_DrawDisc`'s radius in 0..0x4B half the time. **Disturbance** (the group's,
from the hash only): `+9`, the timer, the half-height, the two counter bytes,
a screen float.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_2f`,
exit 0): 372,000 rounds over 62 functions, 10,212,640 calls to the stand-ins,
**0 mismatches**; 25,842 bytes of state in 47 regions. Every entry of the
eight swapped tables reached (each state's handler recorder 474..3,032 calls),
the spark states run for real in `EffectKind52_MoveSparks`, `Effect_Release`
8,251 calls, `Sprite_FindFree` 9,666, `0x479970` 180,000, `Math_Ratan2`
85,425, `EffectKind56_DrawCross` 78,423 (the walk's length).

**Every shadow** (this worktree, no `bof3x.ini`, so narrow): `BOF3X_SHADOW='*'`
exit 0, 687 self-test lines, every one 0 mismatches, `inject: 7235 ours, 0 left
original`; `effect_2f` there 372,000 rounds, 0 mismatches. **With
`BOF3X_WIDE=1`**: `'*'` exit 0, 687 self-test lines, no mismatch, 7,235 ours. No
run died silently; neither needed a re-run.

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

Planted one at a time in a scratch copy of `effect_2f.cpp` (scratch `controls.py`: plant on a unique
string, rebuild, run under `BOF3X_E2F_ONLY=<its function>`, restore, rebuild; the committed file has no
switch). Counts are rounds refused of 6,000 (a filter naming two functions refuses in both), in this worktree.

| # | Function | Plant | Refused |
|--:|---|---|--:|
| 1 | `EffectKind4F_Run` | the next entry | 6000 |
| 2 | `EffectKind50_Run` | the next entry | 6000 |
| 3 | `EffectKind51_Run` | the next entry | 6000 |
| 4 | `EffectKind52_Run` | the next entry | 6000 |
| 5 | `EffectKind52_Sparks` | the next entry | 6000 |
| 6 | `EffectKind52_Trail` | the next entry | 6000 |
| 7 | `EffectKind53_Run` | the next entry | 6000 |
| 8 | `EffectKind56_Run` | the next entry | 6000 |
| 9 | `EffectKind4E_DrawDisc` | b = a + 0x81 | 6000 |
| 10 | `EffectKind4E_DrawDisc` | vertex 0 blue the rim | 5973 |
| 11 | `EffectKind4E_DrawDisc` | a depth copied by mov (a signalling NaN not quietened) | 80 |
| 12 | `EffectKind4F_Start` | tint 0xE | 2230 |
| 13 | `EffectKind4F_Start` | z + 0x800 | 2230 |
| 14 | `EffectKind4F_Start` | the first kept on failure | 1479 |
| 15 | `EffectKind4F_Start` | Sprite_Current not put back | 163 |
| 16 | `EffectKind50_Start` | direction 1 | 6000 |
| 17 | `EffectKind50_Emit` | one frame in eight | 732 |
| 18 | `EffectKind50_Emit` | mask 0x1FFF | 3058 |
| 19 | `EffectKind50_Drain` | released while specks live | 6000 |
| 20 | `EffectKind50_ClearSpecks` | 63 specks | 5970 |
| 21 | `EffectKind50_MoveSpecks` | phase 0 count 9 | 5944 |
| 22 | `EffectKind50_MoveSpecks` | shade up 7 | 5933 |
| 23 | `EffectKind50_MoveSpecks` | freed a frame early | 4613 |
| 24 | `EffectKind50_MoveSpecks` | fall + 0x201 | 6000 |
| 25 | `EffectKind50_MoveSpecks` | the answer inverted | 6000 |
| 26 | `EffectKind50_MoveSpecks` | dtd 1 | 6000 |
| 27 | `EffectKind50_SpeckQuad` | size shr 5 | 6000 |
| 28 | `EffectKind50_SpeckQuad` | left x - w - 1 | 6000 |
| 29 | `EffectKind50_SpeckQuad` | v 0x4E | 6000 |
| 30 | `EffectKind50_SpeckQuad` | red from +2 | 5971 |
| 31 | `EffectKind50_SpawnSpeck` | e + 0x401 | 2449 |
| 32 | `EffectKind50_SpawnSpeck` | height shr (logical) | 2239 |
| 33 | `EffectKind50_SpawnSpeck` | step 2 cos | 4542 |
| 34 | `EffectKind50_FreeSpeck` | the first speck skipped | 1531 |
| 35 | `EffectKind51_Start` | timer 0x21 | 4579, 4575 |
| 36 | `EffectKind51_Start` | 0x3F pushes | 6000, 6000 |
| 37 | `EffectKind51_Start2` | sound 0x203 | 6000 |
| 38 | `EffectKind51_Grow` | Rand & 0x7F | 2947, 2996 |
| 39 | `EffectKind51_Wait` | the sway address taken after Rand | 233, 259 |
| 40 | `EffectKind51_Grow` | width every second | 1159, 1130 |
| 41 | `EffectKind51_Grow` | half-height + 2 | 6000, 6000 |
| 42 | `EffectKind51_Grow` | reload 0x95 | 471 |
| 43 | `EffectKind51_Wait` | counter 0xC | 2784, 2784 |
| 44 | `EffectKind51_Shrink` | sound at 3 | 913 |
| 45 | `EffectKind51_Shrink` | reload 0x41 | 471 |
| 46 | `EffectKind51_RingIn` | sound 0x20A | 495 |
| 47 | `EffectKind51_RingOut` | radius + 0x11 | 5996 |
| 48 | `EffectKind51_RingFade` | the radius kept | 5995 |
| 49 | `EffectKind51_Burst` | radius 0x141 | 5933 |
| 50 | `EffectKind51_Burst` | projected to +0x70 | 6000 |
| 51 | `EffectKind51_RingClose` | radius - 0xF | 5996 |
| 52 | `EffectKind51_DrawMoves` | x - 63 | 5127 |
| 53 | `EffectKind51_DrawMoves` | ftol rounding to nearest | 1761 |
| 54 | `EffectKind51_DrawMoves` | y 0x181 | 6000 |
| 55 | `EffectKind51_PushAngle` | word 1 not moved | 6000 |
| 56 | `EffectKind51_DrawFrame` | arguments swapped | 6000 |
| 57 | `EffectKind51_DrawColumn` | root of + 1 | 3985 |
| 58 | `EffectKind51_DrawColumn` | v + 0x42 | 3985 |
| 59 | `EffectKind51_DrawColumn` | row height 2 | 3774 |
| 60 | `EffectKind51_DrawColumn` | the sum reassociated | **0: equivalent** |
| 61 | `EffectKind51_DrawColumn` | one row more | 3985 |
| 62 | `EffectKind51_DrawRing` | start 0xFC01 | 6000 |
| 63 | `EffectKind51_DrawRing` | x0 read after the call | 1687 |
| 64 | `EffectKind51_DrawRing` | u 0x3E | 6000 |
| 65 | `EffectKind51_DrawRing` | v - 0x3F | 6000 |
| 66 | `EffectKind52_SparksClear` | timer 0x3F | 6000 |
| 67 | `EffectKind52_SparksEmit` | sound at 5 | 964 |
| 68 | `EffectKind52_SparksEmit` | a spark every fourth | 548 |
| 69 | `EffectKind52_SparksDrain` | released while sparks live | 6000 |
| 70 | `EffectKind52_Trail` | drawn before updated | 6000 |
| 71 | `EffectKind52_TrailStart` | width 0xC1 | 6000 |
| 72 | `EffectKind52_TrailStart` | x + 1 | 6000 |
| 73 | `EffectKind52_TrailTurn` | ground << 15 | 889 |
| 74 | `EffectKind52_TrailRise` | half a cell | 6000 |
| 75 | `EffectKind52_TrailTurn` | z 0x77 | 889 |
| 76 | `EffectKind52_TrailFall` | reload 0x7F | 889 |
| 77 | `EffectKind52_TrailHold` | reload 9 | 889 |
| 78 | `EffectKind52_TrailFade` | width - 0x17 | 6000 |
| 79 | `EffectKind52_ClearSparks` | seven sparks | 5977 |
| 80 | `EffectSpark_FindFree` | the first spark skipped | 1500 |
| 81 | `EffectKind52_MoveSparks` | answers 2 | 5980 |
| 82 | `EffectKind52_MoveSparks` | the spark not drawn | 5980 |
| 83 | `EffectKind52_SparkGlow` | reload from +3 | 1029 |
| 84 | `EffectSpark_Wait` | reload 0x1F | 1033 |
| 85 | `EffectKind52_SparkRise` | speed + 0x40001 | 6000 |
| 86 | `EffectKind52_SparkRise` | state reset, not freed | 857 |
| 87 | `EffectKind52_TrailUpdate` | point 0 not moved down | 6000 |
| 88 | `EffectKind52_TrailUpdate` | none when either step is 0 | 5995 |
| 89 | `EffectKind52_TrailUpdate` | none left 0x1000 | 1969 (second run; 0 on the first, below) |
| 90 | `EffectKind52_TrailUpdate` | Ratan2 arguments swapped | 6000 |
| 91 | `EffectKind52_TrailUpdate` | last angle from 29 | 4259 |
| 92 | `EffectKind52_TrailUpdate` | the last angle not searched | 1211 |
| 93 | `EffectKind52_TrailDraw` | next shade - 3 | 6000 |
| 94 | `EffectKind52_TrailDraw` | skipped when either is equal | 5997 |
| 95 | `EffectKind52_TrailDraw` | the copy at + 0x400 | 6000 |
| 96 | `EffectKind52_TrailDraw` | the last cap at 0x80 | 6000 |
| 97 | `EffectKind52_TrailDraw` | the copy 0x40 bytes | 6000 |
| 98 | `EffectKind52_TrailDraw` | NaN not equal (fcomp unordered sets C3) | 181 |
| 99 | `EffectTrail_DrawCap` | step 0x101 | 6000 |
| 100 | `EffectTrail_DrawCap` | centre red 0 | 5976 |
| 101 | `EffectKind53_Start` | +9 = 1 | 6000 |
| 102 | `EffectKind53_Beam` | down at 0 | 485 (second run; 0 on the first, below) |
| 103 | `EffectKind53_Beam` | right x + 7 | 5275 |
| 104 | `EffectKind53_Beam` | NaN y clamped to 240 | 214 |
| 105 | `EffectKind53_Beam` | depth >= 0x1E0 | 240 |
| 106 | `EffectKind53_Beam` | shade + 0x21 | 1105 |
| 107 | `EffectKind53_Beam` | Sprite_Current not put back | 5378 |
| 108 | `EffectKind53_Beam` | height halved by floor | 1506 |
| 109 | `EffectKind53_Beam` | v & 0x17 | 2449 |
| 110 | `EffectKind53_TexWindow` | cursor + 0xC | 6000 |
| 111 | `EffectKind53_TexWindow` | h = w | 6000 |
| 112 | `EffectKind56_Start` | state 2 | 6000 |
| 113 | `EffectKind56_Wait` | counter 0xB | 3001 |
| 114 | `EffectKind56_Arm` | state 4 | 6000 |
| 115 | `EffectKind56_Markers` | phase << 19 | 4626 |
| 116 | `EffectKind56_Markers` | red + 0x2B | 472 |
| 117 | `EffectKind56_Markers` | unsigned compare | 1667 |
| 118 | `EffectKind56_Markers` | counter 0xD | 1473 |
| 119 | `EffectKind56_Place` | height scale 2 | 6000 |
| 120 | `EffectKind56_Place` | height from z | 6000 |
| 121 | `EffectKind56_DrawCross` | x - 0x3FFF | 6000 |
| 122 | `EffectKind56_DrawCross` | semi before the commit | 6000 |
| 123 | `EffectKind56_DrawCross` | red from +0x5C | 5976 |
| 124 | `EffectKind51_Grow2` | reload 0x2E | 470 |
| 125 | `EffectKind51_Wait2` | the timer high byte cleared after | 741 |
| 126 | `EffectKind51_DrawColumn` | x - w rounded to a float before + dx (60's near variant) | 2069 |

**125 of 126 refused by a count.** Control 60 is an equivalent mutant: `(x - w) + dx` against `x - (w - dx)`
with w and dx whole numbers the column can hold and x a float - every intermediate is exact at the x87's 53 (or
64) bits, so the one rounding to the stored float is the same; its near variant 126 (the difference rounded to a
float first) is refused. Controls 89 and 102 passed on the first run - the fuzz's fault: no round made all 31
trail steps zero (so no angle was left unfound) and no extra sprite's `+0x14` was 0. The fuzz then seeded whole
trails at the record's point (with `EffectGte_ProjectPoint`'s stand-in projecting equal world points equally) and
the sprite's `+0x14` at its sign's boundary; both refused on the second run. Every other control was refused on
the first run. Control 11 shows the `fld` / `fstp` copies are observable (a signalling NaN in the depth is
quietened by the original), control 98 the `fcomp` unordered case, control 104 the `test ah, 1` NaN case.

## 7. Latent defects (Capcom's, described, not fixed)

- **The dispatchers index unchecked** (D200) (eight tables, and
  `EffectKind52_MoveSparks`' call by a spark's `+1`). Every writer of the bytes
  in the band steps them inside their tables; ours aborts past them.
- **`EffectKind51_DrawColumn` trusts its half-height** (D208): 65 angles and a byte row
  counter, so a half-height above 0x20 reads past `EffectKind51_Angles` and one
  above 0x80 never ends. The kind's states keep it within 0..0x20 (they reset it
  to 0 at states 0 and 6 and raise it once a frame for 0x20 frames); the ring
  states use the same word as a radius (0x140 at state 10) but never draw the
  column.
- **The effect pools overlap.** (D201) `EffectKind30_Shards` (`0x92BF80`) is kind
  0x50's 64 specks of 0x28 (to `0x92C980`), kind 0x52's eight sparks of 0x1C
  (to `0x92C060`) and, straight after them, kind 0x52's trail
  (`0x92C060..0x92C4A0`), as well as FC2's sparks and E1C's shards, pieces and
  debris. Two of these kinds alive at once overwrite each other's records -
  a kind-0x50 speck in use over the trail would be drawn from trail data and
  move it. Whether the game ever runs two together is the owner's to say.
- **Uninitialised words handed on** (D213): `EffectKind50_SpeckQuad` hands
  `EffectGte_ProjectSize` a size whose second word it never wrote (the answer's
  second word is not read); `EffectKind53_Beam` hands `Gte_RotTransPers` a
  vertex whose fourth word it never wrote (not read); `EffectKind52_TrailDraw`
  pushes its shade as a dword whose upper three bytes were never written (the
  cap reads the byte). No effect.
- **`EffectKind56_Markers`' walk never ends** (D208) when the ground `+0x14` lies in
  the lowest 2^24 of the signed range (a ground of cell -0x8000); ours aborts.
- **Kinds 0x51 and 0x56 wait on the chapters' counters** (D202) (`0x903848` = 0xB,
  `0x90384B` = 0xA then 0xE): spawned where no event script sets them, they
  wait for ever (by design, not a fault).
- **`EffectKind4F_Start`** (D238) keeps FC1's `EffectKind3C_Start`'s behaviour: when the
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
