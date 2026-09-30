# Group E2D: effect kinds 0x45..0x49

**Status:** MEASURED (2026-09-29) - round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9..12),
wave two, on the round branch's tip `dcef661`. **54 functions ours**
(`src/game/effect_2d.cpp`, shadow name `effect_2d`): the cut table's 54 rows
for E2D (`analysis/round13_cut.tsv`, the band `0x4771B0..0x4789C1`), no start
dropped and none added. Each read to its last instruction with capstone and
fuzzed through the scenario harness in effect mode
([`scenario_harness.md`](scenario_harness.md) section 8) without edits to it:
324,000 rounds, 0 mismatches. 148 controls planted one at a time, 145 refused by a count, three equivalent mutants each with a near variant refused. **Fuzz only**: no recorded
route enters any of the 54 (section 9).

Every row is effect code, the two `hypothesis` rows (`0x477E10`, `0x4780D0`)
included: kind 0x47's dispatcher and its state 1.

| Kind | Functions | Reached through |
|---|--:|---|
| 0x45: a 72 x 72 panel with a grid and a sine trace; the wave slowed and grown while a flag is set, then the confirm button read against it | 14 | `Effect_KindHandlers[0x45]` (`0x655464`), `EffectKind45_States` `0x65450C` (4) |
| 0x46: a screen-wide tile stepped up and down in colour five times, then the chapters' counter up; the field sprites redrawn each frame | 8 | `Effect_KindHandlers[0x46]` (`0x655468`), `EffectKind46_States` `0x654520` (5) |
| 0x47: two tinted sprites beside the leader or sprite object 2, the model hidden for four frames | 4 | `Effect_KindHandlers[0x47]` (`0x65546C`), `EffectKind47_States` `0x654534` (3) |
| 0x48: rings of four sparks every four frames, `+0xB` rings | 9 | `Effect_KindHandlers[0x48]` (`0x655470`), `EffectKind48_States` `0x654578` (3) |
| 0x49: ten variants picked by `+1` at the spawn; variants 0..4 here (5..9 are E2E's) | 19 | `Effect_KindHandlers[0x49]` (`0x655474`), `EffectKind49_Variants` `0x654584` (10), `EffectKind49_V0States`..`_V4States` (3/2/8/3/4) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`). "Panel", "trace", "flash", "sparks", "puffs", "glow", "dust"
name the code's shape - the primitives it commits, the records it steps - not
a play-tested fact: where the game shows these kinds, and what they look like
on screen, was not traced (section 9; the owner's word, not this doc's). The
spawners, from our own source: kind 0x45 by chapter 6's run 13
(`Scena06_Run13`, `scena_sc6.cpp`); kind 0x46 by chapter 7's step 0x15
(`scena_sc7.cpp`, which then waits on the counter byte `0x903848` the kind's
state 3 bumps); kind 0x47 by area 85's handlers 3 and 4
(`Area85_SpawnEffect47A` / `B`, `area_w2b.cpp`, `+0xB` 1 or 0); kind 0x48 by
area 52's handlers 6 and 7 (`area_w1c.cpp`); kind 0x49 by
`Scena07_TakeEffect49(variant)` (`scena_sc7.cpp`: variants 0, 1, 2, 3 with 4,
5 with 8, 6 with 7, and 9).

## 1. What each function does

`Effect_RunObjects` (ours) makes each live record of `Effect_Objects` (20 of
0x80 at `0x7E11E0`) `Sprite_Current` and calls `Effect_KindHandlers[+5]`. Each
kind's handler is the four-instruction dispatcher `mov ecx,
[Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [T + eax * 4]` with no
bound; kind 0x49's variants 0..4 dispatch the same way by `+2`. Ours reads the
entry in place and aborts past the table (section 2). `symbols.toml` holds each
function's full reading; in short:

### 1.1 Kind 0x45 (`EffectKind45_Run` `0x4771B0`)

The record's words: `+0x2E` the phase (0..0x47), `+0x30` the frequency,
`+0x32` the amplitude, `+0x36` the offset; `+0x5D` the markers' shade. The
flags are the chapter's row (`*0x929ED0`).

- `_Start` `0x4771D0` (state 0): the first 32 of the 36 history dwords at
  `EffectKind30_Shards` cleared, byte `0x9039A3 |= 1` (`Field_ScriptFlags`
  bit 8); phase 0, frequency 0xA00, amplitude 0, shade 0; `+1` up; with flag
  0x25, `Music_FadeOutStop(10)` and `Sound_PlayEffect(0x203)`.
- `_Tune` `0x477250` (state 1): the panel; twice: the phase up (0 at 0x48);
  with flag 0x25 the frequency down 0x10 (not below 0x200), the amplitude up 1
  on a frame a multiple of four (not above 0x18), the offset 0; a sample
  pushed. The trace. Frequency 0x200 and amplitude 0x18: state 2; else a
  cancel or confirm button in `Input_Pressed`: state 3.
- `_Aim` `0x477370` (state 2): the markers lit (shade 0x80) at offset 8 and
  phase 0x20; the panel; twice: the phase up, at 0x48 back to 0 and the offset
  down 8 (five bits); a sample; `Sound_PlayEffect(0x205)` at phase 2. The
  trace. Confirm: flag 0x26 set when the offset is 8 and the phase within
  0x1C..0x24, else cleared; state 3. Cancel: state 3.
- `_End` `0x4774A0` (state 3): with flag 0x25, `Sound_PlayEffect(0x206)` if
  flag 0x26 else `0x204`; flag 0x2D cleared; `Effect_Release`.
- `_DrawPanel` `0x477500` `(x, y)`: blend 2 and a grey 0x48 square, blend 1
  and a green one; an outer and an inner border; seven grid lines each way
  (the vertical one at 0x24 red, the rest green); four marker ticks in red
  0x80 with green and blue the markers' shade, which then falls 0x10 a frame.
  The colour is the three bytes `0x6761C4..0x6761C6` the line and rectangle
  helpers commit (as blue, green, red at `+6`, `+5`, `+4`).
- `_DrawLine` `0x4776E0`, `_FillRect` `0x477B20`, `_Plot` `0x477AC0`: a
  `LINE_F2`, a `TILE`, a `TILE_1` at `Gfx_PacketNext`, semi-transparent, the
  s16 coordinates as floats; `_DrawMode` `0x477760` `(abr)`: a draw mode of
  that blend.
- `_PushSample` `0x4777A0`: the history moved up one and `(x + phase + 4, y +
  0x24 + Math_Sin(((phase - offset - 0x20) * (frequency << 4) / 64) & 0xFFFF)
  * amplitude >> 12)` put first - 16-bit arguments, 32-bit products, the divide
  toward zero.
- `_DrawTrace` `0x477820` `(x, y)`: the clip `(x + 4, y + 4, 0x40, 0x40)`;
  the newest sample plotted when inside the clip's columns;
  `_DrawPoints(0x48, history, clip)`.
- `_DrawPoints` `0x4778A0`: for `count - 1` pairs, the second point inside the
  clip's columns is plotted when the first's x is less, else joined by
  `_DrawSegment`; the shade 0x40 falls 2 a pair.
- `_DrawSegment` `0x477940`: a stepped line of `_Plot` pixels between two
  packed points, 16-bit throughout, along y when dy is not 0 and dx not above
  it, else along x, from the lower end; the step count is a byte (section 2).

### 1.2 Kind 0x46 (`EffectKind46_Run` `0x477BA0`)

- `_Start` `0x477BC0`: `+9` 0xC, `+1` 1, `+2` 0, the colour `+0x5D..+0x5F`
  0; the flash and the redraw.
- `_FadeIn` `0x477C10` / `_FadeOut` `0x477C70`: `+9` down; not 0: each colour
  byte up (down) 4; at 0 `+9` 0xC and state 3. The flash, the redraw.
- `_Count` `0x477CD0`: `+2` up; at 5 the counter byte `0x903848` up and state
  4; else state `1 + (+2 & 1)`. The flash, the redraw.
- `_Hold` `0x477D10`: the flash and the redraw each frame; nothing here ends
  the record.
- `_DrawFlash` `0x477D20`: a draw mode (`Gpu_SetDrawMode(0, 1, 0x4F, 0)`) and
  a 320 x 240 `TILE` of the colour, semi-transparent, both at slot 5.
- `_RedrawSprites` `0x477DB0`: the tail the five states jump to (a whole
  function, its own frame and `ret`): each `Sprite_Objects` record with `+0`
  bit 0 made `Sprite_Current`, its `+0x29` 5, `Sprite_UpdateScreen`; the
  leader likewise; `Sprite_Current` put back.

### 1.3 Kind 0x47 (`EffectKind47_Run` `0x477E10`)

- `_Start` `0x477E30`: two free sprites into `+3` / `+4`, marked in use (none:
  nothing; only the second: the first given back). The model - the leader,
  or sprite object 2 when `+0xB` is 0 - has its animation byte `+0x4B` looked
  up among five `(key, animation, start)` triples at `0x654564`; for each
  sprite `DamageScratch` = its index, `EventOp_0x(0x654540 + 17 * +0xB)`,
  `Sprite_SetAnimationAt(animation, start)`. `Sprite_Current` put back; the
  two moved 0x1000 apart on each axis, at the leader's height, `+0 |= 0x20`,
  `+0x5C` 0, tinted `(0xF, 0, 0, 1)` and `(0, 0, 0xF, 1)`; `+9` 4, state 1.
  The same shape as FC1's `EffectKind14_Start` / `EffectKind3C_Start`.
- `_Blink` `0x4780D0`: the model's `+0 |= 0x40` while `+9` counts down; at 0
  the bit cleared, both sprites freed, state 2.
- `EffectTwinSprites_Release` `0x478160`: `Sprite_ReleaseTint` for both sprites,
  `Effect_Release`. Also state 2 of kinds 0x14 and 0x3C (`EffectKind14_States`,
  `EffectKind3C_States`, FC1's), hence the name.

### 1.4 Kind 0x48 (`EffectKind48_Run` `0x4781B0`)

Sixteen sparks of 0x18 at `EffectKind30_Shards`: `+0` x, `+4` z, `+8`
height, `+0x10` size, `+0x14` shade, `+0x15` live, `+0x16` life.

- `_Start` `0x4781D0`: another live kind-0x48 record: this one released.
  Else `+6` and `+9` 0, the sparks cleared, `Sound_PlayEffect(0x203)`, state 1.
- `_Burst` `0x478230`: a ring when `+9` is 0; `+9` up, at 4 back to 0 and `+6`
  up; the sparks stepped; `+6` at `+0xB`: state 2.
- `_Fade` `0x478280`: the sparks stepped; none live: `Effect_Release`.
- `_SpawnRing` `0x478290`: four sparks at angles 0x80 + 0x100 i round the
  record's point (`Math_Cos` / `Math_Sin` << 4), life 8, size 0x1800, shade
  0xC0. `_FindFreeSpark` `0x478320`, `_ClearSparks` `0x478340`.
- `_StepSparks` `0x478360`: the map camera; each live spark drawn, then height
  +0x3000, shade - 0x18, life down (at 0 no longer live). Answers 1 in `al`
  when any was live.
- `_DrawSpark` `0x4783C0`: FC2's `EffectKind30_SparkQuad` shape - a
  semi-transparent `POLY_FT4` of the point `(x, z, height << 8)` projected and
  its size `+0x10 >> 6` scaled at that depth - committed at slot 1 instead of
  linked.

### 1.5 Kind 0x49 (`EffectKind49_Run` `0x478550`)

`+1` is the variant `Scena07_TakeEffect49` stores; each variant dispatches by
`+2`.

- Variant 0 (`_V0Run` `0x478570`): `_V0Start` the puffs cleared (`0x4790C0`),
  `+0xC` 0x40; `_V0Emit` a puff started every eighth count (`0x47CF20`,
  `0x4790F0`), the puffs stepped (`0x479260`), `+0xC` down; `_V0End` the puffs
  stepped until none live, `Effect_Release`.
- Variant 1 (`_V1Run` `0x478600`): `_V1Start` `+0x10` -8; `_V1Fade`
  `Area85_ClutShift(+0x10)` (CLUT row 6's channels shifted), `+0x10` up 1 every
  16th frame, at 0 `Effect_Release`.
- Variant 2 (`_V2Run` `0x478680`, eight states, 5 and 6 `WeretigerFx_Next`, 7
  `Effect_StateRelease`): `_V2Launch` the glow word `0x92C49E` 0xC0, the record
  lifted along an arc in 16 steps at once, `Sound_PlayEffect(0x202)`, the speed
  negated; `_V2Fly` the glow (`0x4794D0` / `0x4796B0` on `0x92C060`), x down
  0x10000 a frame, the height by the speed, the speed up 0x800000, at the
  target `+0xC` / `+0x10` `+9` 0x2F; `_V2Wait` `+9` down, then
  `Sound_PlayEffect(0x203)`; `_V2Rise` / `_V2Sink` the glow word up 6 / down
  0xC for 0x20 frames each.
- Variant 3 (`_V3Run` `0x478880`): `_V3Start` on the ground
  (`AreaMap_Elevation`), the dust cleared (`0x47A110`), `+0xC` 0x5F,
  `Sound_PlayEffect(0x205)`; `_V3Burst` by step `+9`: when the count's low
  byte misses the step's mask (`0x6545EC[+9]`), the word `+0x10` behind the
  pointer `0x6761D0` up the step's count (`0x6545F4[+9]`) and that many dust
  records started (`0x47A130` / `0x47A150`); a count with its low nibble 0
  moves the step on; the dust stepped (`0x47A200`); `_V3End` until none live.
- Variant 4: `_V4Run` `0x4789B0` only (its four states are E2E's).

## 2. Divergence

None: every function is a faithful replacement, no `DIVERGENCE.md` entry is
owed (`DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no address of
the band nor its tables; checked 2026-09-29). Where the original goes where
ours cannot follow, ours aborts with a `Fatal` naming the function (the
owner's rule, round9 doc section 6; nothing in play reaches it):

- the ten dispatchers past their tables (the original jumps through the next
  table's entry or data);
- `EffectKind47_Start` / `_Blink` writing a sprite record at `+3` / `+4` past
  the thirty (`Sprite_FindFree` answers 0..29 or `0xFF`);
- `EffectKind45_DrawSegment` with a distance past 0xFF on the stepping axis:
  its step count is a byte compared with the 16-bit distance, so the original
  never returns (section 7).

`EffectTwinSprites_Release` hands `Sprite_ReleaseTint` the record addresses
unchecked, as the original does (the callee compares them, it reads nothing
through them). `EffectKind48_DrawSpark` hands `EffectGte_ProjectSize` a size
whose second word is the original's uninitialised stack; that quotient is
never read, and ours passes 0 there. The corners' x87 arithmetic is
reproduced at the game's 53-bit precision (FC2's form).

## 3. The arguments pushed with leftovers

| Callee | Pushed | Read by the callee |
|---|---|---|
| `EffectKind45_PushSample` | `0x82`, `0xA`, then `ax` / `dx` / `cx` / `dx` words loaded from the record (upper halves the caller's) | all six as 16 bits (`movsx`, or a `lea` whose low word is stored) |
| `EffectKind45_DrawLine`, `_FillRect` | whole dwords (`lea` sums) | each `movsx` 16 bits |
| `EffectKind45_DrawMode` | an immediate | `and eax, 0xFF` |
| `EffectKind45_DrawPoints` / `_DrawSegment` / `_Plot` | the shade as a dword whose upper bytes are the caller's counter and stack | `mov al, [arg]`: a byte |

The fuzz lists each with those masks.

## 4. The fuzz (`effect_2d_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=effect_2d`, effect mode (kinds 0x45..0x49),
6,000 rounds a function (`BOF3X_E2D_ONLY=<name>` runs the clones whose name
holds it). Shapes: 44 `kEffect` (each clone's own kind; the dispatchers'
`state_span` 4, 5, 3, 3, 10 and the variants' `sub_span` 3, 2, 8, 3, 4), ten
`kCall` (the helpers with arguments). Two answer: `EffectKind48_StepSparks`
in `al` (`ret_mask` `0xFF`; its callers `test al, al`) and
`EffectKind48_FindFreeSpark` a pointer in `eax` (`ret_mask` `0xFFFFFFFF`). The ten tables are `DataTable`s,
swapped for recorders on both sides. Regions beyond effect mode's: the panel's
colour bytes and the dust anchor (`0x6761C4`, 0x10), the anchor's word and the
64 dust records (`0x92D1C8`, 0x814).

**Callees**: the effect-standard rows (`Effect_Release` clearing `+0..+4`,
`Sprite_FindFree` 0..29 or `0xFF`, the `Gpu_*`, `Gfx_CommitPrim` moving the
cursor, `Flags_*`, `Sound_PlayEffect`, `Math_*`, `EventOp_0x`,
`Sprite_SetAnimationAt`, `Sprite_SetTint`, `Area85_ClutShift`,
`AreaMap_Elevation`, `EffectGte_LoadMapCamera`, and the unnamed `0x4790C0`,
`0x4790F0`, `0x479260`, `0x4794D0`, `0x4796B0`, `0x47A110`, `0x47A150`,
`0x47A200`). Re-listed in the group: its own fourteen called directly, by name
(the masks of section 3; the two draws' colour bytes noted at each call, so a
colour set wrong and overwritten before the end shows; the clip and the
stepping point, the callers' stack, hashed and not logged; `_FindFreeSpark` as
the real one, since its callers write through it); `Sprite_UpdateScreen` as a
phase recorder, logging the `Sprite_Current` it runs on;
`Sprite_ReleaseTint` with the record address logged; `EffectGte_ProjectPoint`
/ `_ProjectSize` with the stack points hashed (the second size word, the
original's uninitialised stack, not) and the outs filled with fractional
floats; `0x47CF20` / `0x47A130` answering as the real finds (the first free
record, or null), so the null test is reached.

**Seeds** (per function, after the harness's fill): `+9` at 0..5, 0xC or any;
kind 0x45's phase at the window's edges after its two steps (0x19..0x1E,
0x20..0x25), the wrap (0x46..0x48), the frequency at the 0x200 floor
(0x1FF..0x211), the amplitude at the 0x18 cap, the offset at 8, and the
confirm button pressed half the time; the history's points about the clip's
columns; for `_DrawPoints` the clip in its scratch; for `_DrawSegment` two
points a byte's distance apart at most on each axis, anywhere in the s16
plane; kind 0x46's `+2` at 3..6; kind 0x47's `+0xB` 0 or 1, every record's
`+3` / `+4` inside the pool (the states read them from whichever record is
current after a call), the model's byte one of the five keys; kind 0x48's
other records of kind 0x48 absent half the time (and then one free record of
kind 0x48 half of those), `+6` at `+0xB`, the sparks all free, all live,
mixed, or one on its last frame; variant 0's count at its mask, the puffs
free or not; variant 1's `+0x10` at -1..1; variant 2's target one step away
and the speed 0; variant 3's step below eight in every record and the anchor
at the compared word. **Disturbance** (the group's, from the hash only):
`+9` below 8, `+0xC`, `+0x10`, the phase and offset words, `+6` against
`+0xB`, a spark's live byte.

**Result** (in this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=effect_2d`,
exit 0): 324,000 rounds over 54 functions, 2,093,653 calls to the
stand-ins, **0 mismatches**; 26,840 bytes of state in 47 regions. Every entry
of the ten tables reached (each handler recorder 564..3,006 calls in the `'*'`
run below; E2E's nine cells among them), `Effect_Release` 22,242,
`Sprite_FindFree` 9,639, `Flags_Set` 478 (the window hit),
`EffectKind45_DrawSegment` 52,943, `0x47CF20` 2,302 and `0x47A130` 8,069
(answering null and not).

**Every shadow** (this worktree, no `bof3x.ini`): `BOF3X_SHADOW='*'` exit 0,
687 self-test lines, none with a mismatch, `inject: 7227 ours, 0 left
original`; `effect_2d` there 324,000 rounds, 2,123,101 calls, 0 mismatches.
**With `BOF3X_WIDE=1`**: the same - exit 0, 687 lines, 7,227 ours, 0
mismatches (scratch `e2d/star_narrow.log`, `star_wide.log`).

## 5. What the cut and the tool said, settled

- **Extents**: `band_rows.py` read all 54 to the byte (5,683 bytes against the
  cut's 6,065: 38 differ by padding only, none by code).
- **Hidden starts**: 39, each an entry by address - a cell of
  `Effect_KindHandlers` or of a state table - but one: `0x477DB0`
  (`EffectKind46_RedrawSprites`), which the plan's section 4 flagged as a
  shared tail, reached only by `jmp` from the five kind-0x46 states. It is a
  whole function (its own `push ebp..ret`, nothing falls into it), so it is
  taken as one, its five callers' tail jumps re-aimed like calls. The recorded
  hosts: `0x477180` (E2C's, whose extent line covers `0x4771B0..0x4774F2`),
  and three of ours whose `entries_logic.txt` lines are longer than their code
  - `0x477B20` (0x1FA over `EffectKind45_FillRect`'s 0x7D), `0x477D20` (0x56F
  over 0x86), `0x4783C0` (0xCFC over 0x18A). None contains another row's code
  as a fall-through.
- **The `hypothesis` rows** `0x477E10` and `0x4780D0`: kind 0x47's dispatcher
  and its state 1, read: effect code, taken.
- **The cut's hints**: the `unit_desc` "Fn_4781B0 kind 0x48" on the eighteen
  rows from `0x478570` is wrong - they are kind 0x49's (`0x478550` is
  `Effect_KindHandlers[0x49]`; its table `0x654584` begins three entries into
  kind 0x48's run); "no dispatcher found" on `0x4771D0..0x477B20` is kind
  0x45's table `0x65450C` (the unit scan read the run from `0x654504`, two
  dwords of data before it).
- **No start dropped, none added**: `band_rows.py` printed 0 "code no list
  has" in the band, and every `Effect_KindHandlers` entry that points into the
  band (`[0x45]..[0x49]`) is a row here.

## 6. Controls

Planted one at a time behind `BOF3X_E2D_CTL=<n>` in a scratch copy of
`effect_2d.cpp` (scratch `plant.py`, `ctl.sh`: plant, rebuild, run each under
`BOF3X_E2D_ONLY=<its function>`, restore, rebuild; the committed file has no
switch). Counts are rounds refused of 6,000, in this worktree, all exit 3 but
the three equivalents.

| # | Function | Plant | Refused |
|--:|---|---|--:|
| 1 | `EffectKind45_Run` | dispatch EffectKind45_Run to the next entry | 6,000 |
| 2 | `EffectKind46_Run` | dispatch EffectKind46_Run to the next entry | 6,000 |
| 3 | `EffectKind47_Run` | dispatch EffectKind47_Run to the next entry | 6,000 |
| 4 | `EffectKind48_Run` | dispatch EffectKind48_Run to the next entry | 6,000 |
| 5 | `EffectKind49_Run` | dispatch EffectKind49_Run to the next entry | 6,000 |
| 6 | `EffectKind49_V0Run` | dispatch EffectKind49_V0Run to the next entry | 6,000 |
| 7 | `EffectKind49_V1Run` | dispatch EffectKind49_V1Run to the next entry | 6,000 |
| 8 | `EffectKind49_V2Run` | dispatch EffectKind49_V2Run to the next entry | 5,263 |
| 9 | `EffectKind49_V3Run` | dispatch EffectKind49_V3Run to the next entry | 6,000 |
| 10 | `EffectKind49_V4Run` | dispatch EffectKind49_V4Run to the next entry | 6,000 |
| 11 | `EffectKind45_Start` | clears 31 samples | 6,000 |
| 12 | `EffectKind45_Start` | script flag bit 2 | 4,464 |
| 13 | `EffectKind45_Start` | frequency 0x9F0 | 6,000 |
| 14 | `EffectKind45_Start` | flag 0x26 tested | 6,000 |
| 15 | `EffectKind45_Start` | fade 11 frames | 3,987 |
| 16 | `EffectKind45_Tune` | phase wraps at 0x47 | 766 |
| 17 | `EffectKind45_Tune` | frequency floor 0x1F0 | 2,977 |
| 18 | `EffectKind45_Tune` | amplitude on odd frames | 664 |
| 19 | `EffectKind45_Tune` | amplitude cap unsigned | 904 |
| 20 | `EffectKind45_Tune` | offset not zeroed | 5,283 |
| 21 | `EffectKind45_Tune` | one pass | 6,000 |
| 22 | `EffectKind45_Tune` | confirm only | 25 |
| 23 | `EffectKind45_Tune` | done on amplitude alone | 992 |
| 24 | `EffectKind45_Aim` | markers at phase 0x21 | 142 |
| 25 | `EffectKind45_Aim` | offset down 7 | 968 |
| 26 | `EffectKind45_Aim` | sound at phase 3 | 851 |
| 27 | `EffectKind45_Aim` | window from 0x1D | 60 |
| 28 | `EffectKind45_Aim` | window to 0x23 | 69 |
| 29 | `EffectKind45_Aim` | cancel ignored | 12 |
| 30 | `EffectKind45_Aim` | stale record after the push | 19 |
| 31 | `EffectKind45_End` | sounds swapped | 3,987 |
| 32 | `EffectKind45_End` | flag 0x2C cleared | 6,000 |
| 33 | `EffectKind45_DrawPanel` | square 0x47 | 6,000 |
| 34 | `EffectKind45_DrawPanel` | grid step 7 | 6,000 |
| 35 | `EffectKind45_DrawPanel` | middle line green | 6,000 |
| 36 | `EffectKind45_DrawPanel` | marker shade down 8 | 5,640 |
| 37 | `EffectKind45_DrawPanel` | tick 0x3F | 6,000 |
| 38 | `EffectKind45_DrawPanel` | marker red 0x70 | 6,000 |
| 39 | `EffectKind45_DrawLine` | colour order | 5,981 |
| 40 | `EffectKind45_DrawLine` | x1 unsigned | 3,009 |
| 41 | `EffectKind45_DrawMode` | page 0x3C1 | 6,000 |
| 42 | `EffectKind45_PushSample` | 35 kept | 6,000 |
| 43 | `EffectKind45_PushSample` | x + 5 | 6,000 |
| 44 | `EffectKind45_PushSample` | divide by a shift (floor) | 1,553 |
| 45 | `EffectKind45_PushSample` | phase less offset + 0x1F | 5,999 |
| 46 | `EffectKind45_PushSample` | wave >> 11 | 6,000 |
| 47 | `EffectKind45_PushSample` | amplitude unsigned | 3,042 |
| 48 | `EffectKind45_DrawTrace` | clip w 0x3F | 6,000 |
| 49 | `EffectKind45_DrawTrace` | first plotted at <= | 718 |
| 50 | `EffectKind45_DrawTrace` | 0x47 points | 6,000 |
| 51 | `EffectKind45_DrawPoints` | shade down 1 | 2,854 |
| 52 | `EffectKind45_DrawPoints` | plot at <= | 2,096 |
| 53 | `EffectKind45_DrawPoints` | clip w read as unsigned | 1,757 |
| 54 | `EffectKind45_DrawPoints` | one pair fewer | 1,405 |
| 55 | `EffectKind45_DrawSegment` | y-major start at the upper y | 3,603 |
| 56 | `EffectKind45_DrawSegment` | y-major error >= 0 | 168 |
| 57 | `EffectKind45_DrawSegment` | x-major step sign | **0: equivalent** |
| 58 | `EffectKind45_DrawSegment` | x-major when equal | 662 |
| 59 | `EffectKind45_DrawSegment` | x-major one pixel fewer | 1,895 |
| 60 | `EffectKind45_DrawSegment` | x-major down 2dy | 1,392 |
| 61 | `EffectKind45_Plot` | y as x | 6,000 |
| 62 | `EffectKind45_Plot` | commit 0x10 | 6,000 |
| 63 | `EffectKind45_FillRect` | h as w | 6,000 |
| 64 | `EffectKind46_Start` | +9 = 0xB | 5,923 |
| 65 | `EffectKind46_Start` | +0x5E kept | 5,979 |
| 66 | `EffectKind46_FadeIn` | up 5 | 4,774 |
| 67 | `EffectKind46_FadeOut` | to state 2 | 1,226 |
| 68 | `EffectKind46_FadeIn` | blue not stepped | 4,774 |
| 69 | `EffectKind46_Count` | ends at 6 | 2,035 |
| 70 | `EffectKind46_Count` | odd and even swapped | 4,965 |
| 71 | `EffectKind46_Count` | counter not moved | 940 |
| 72 | `EffectKind46_Hold` | no redraw | 6,000 |
| 73 | `EffectKind46_DrawFlash` | height 239 | 6,000 |
| 74 | `EffectKind46_DrawFlash` | slot 4 | 6,000 |
| 75 | `EffectKind46_DrawFlash` | colour green and blue swapped | 5,971 |
| 76 | `EffectKind46_RedrawSprites` | every record redrawn | 6,000 |
| 77 | `EffectKind46_RedrawSprites` | Sprite_Current not put back | 5,991 |
| 78 | `EffectKind46_RedrawSprites` | +0x29 = 4 | 6,000 |
| 79 | `EffectKind46_RedrawSprites` | the leader not current | 6,000 |
| 80 | `EffectKind47_Start` | first not given back | 1,413 |
| 81 | `EffectKind47_Start` | model by +0xB inverted | 465 |
| 82 | `EffectKind47_Start` | four keys | 964 |
| 83 | `EffectKind47_Start` | op by 16 | 1,430 |
| 84 | `EffectKind47_Start` | second scratch the first index | 2,135 |
| 85 | `EffectKind47_Start` | Sprite_Current not put back | 307 |
| 86 | `EffectKind47_Start` | first moved +0x1000 x | 2,215 |
| 87 | `EffectKind47_Start` | height of +0x38 | 2,215 |
| 88 | `EffectKind47_Start` | tints swapped | 2,215 |
| 89 | `EffectKind47_Start` | +9 = 5 | 2,215 |
| 90 | `EffectKind47_Start` | flag 0x10 | 2,215 |
| 91 | `EffectKind47_Start` | animation start from the key | 1,251 |
| 92 | `EffectKind47_Blink` | bit 0x20 | 3,591 |
| 93 | `EffectKind47_Blink` | model by +0xB inverted | 4,451 |
| 94 | `EffectKind47_Blink` | second sprite kept | 1,136 |
| 95 | `EffectTwinSprites_Release` | the first twice | 1,998 |
| 96 | `EffectKind48_Start` | itself counted | 2,987 |
| 97 | `EffectKind48_Start` | in use not tested | 1,441 |
| 98 | `EffectKind48_Start` | sound 0x204 | 2,987 |
| 99 | `EffectKind48_Burst` | rings every 3 | 1,257 |
| 100 | `EffectKind48_Burst` | ends at +6 > +0xB | 3,231 |
| 101 | `EffectKind48_Fade` | released while live | 6,000 |
| 102 | `EffectKind48_SpawnRing` | angle step 0x80 | 4,485 |
| 103 | `EffectKind48_SpawnRing` | life 9 | 4,493 |
| 104 | `EffectKind48_SpawnRing` | z from the cosine | 4,493 |
| 105 | `EffectKind48_SpawnRing` | x << 3 | 4,493 |
| 106 | `EffectKind48_FindFreeSpark` | the last free | 4,470 |
| 107 | `EffectKind48_ClearSparks` | fifteen | 5,981 |
| 108 | `EffectKind48_StepSparks` | height + 0x2000 | 4,524 |
| 109 | `EffectKind48_StepSparks` | any only while still live | 1,481 |
| 110 | `EffectKind48_StepSparks` | shade - 0x10 | 4,524 |
| 111 | `EffectKind48_DrawSpark` | size >> 5 | 5,999 |
| 112 | `EffectKind48_DrawSpark` | height << 7 | 6,000 |
| 113 | `EffectKind48_DrawSpark` | corner in float arithmetic | **0: equivalent** |
| 114 | `EffectKind48_DrawSpark` | v 0x4E | 6,000 |
| 115 | `EffectKind48_DrawSpark` | depth at +0x40 the y | 6,000 |
| 116 | `EffectKind48_DrawSpark` | clut 0x1E2 | 6,000 |
| 117 | `EffectKind49_V0Start` | count 0x3F | 6,000 |
| 118 | `EffectKind49_V0Emit` | mask 3 | 96 |
| 119 | `EffectKind49_V0Emit` | null started | 15 |
| 120 | `EffectKind49_V0End` | release inverted | 6,000 |
| 121 | `EffectKind49_V1Start` | -7 | 6,000 |
| 122 | `EffectKind49_V1Fade` | every 8 frames | 336 |
| 123 | `EffectKind49_V1Fade` | shift after the step | 6,000 |
| 124 | `EffectKind49_V2Launch` | 15 steps | 6,000 |
| 125 | `EffectKind49_V2Launch` | glow 0xC1 | 6,000 |
| 126 | `EffectKind49_V2Launch` | speed not negated | 6,000 |
| 127 | `EffectKind49_V2Fly` | target on x alone | 1,384 |
| 128 | `EffectKind49_V2Fly` | speed 0 stepped | 1,872 |
| 129 | `EffectKind49_V2Fly` | +9 = 0x30 | 1,325 |
| 130 | `EffectKind49_V2Wait` | no sound | 684 |
| 131 | `EffectKind49_V2Wait` | stale record after the sound | 29 |
| 132 | `EffectKind49_V2Rise` | up 5 | 6,000 |
| 133 | `EffectKind49_V2Sink` | down 0xB | 6,000 |
| 134 | `EffectKind49_V2Sink` | glow not stepped | 6,000 |
| 135 | `EffectKind49_V3Start` | height << 15 | 6,000 |
| 136 | `EffectKind49_V3Start` | elevation (z, x) | 6,000 |
| 137 | `EffectKind49_V3Start` | count 0x60 | 5,961 |
| 138 | `EffectKind49_V3Burst` | mask ignored | 872 |
| 139 | `EffectKind49_V3Burst` | anchor + 2 | 5,128 |
| 140 | `EffectKind49_V3Burst` | count read once | 224 |
| 141 | `EffectKind49_V3Burst` | step on nibble 7 | 59 |
| 142 | `EffectKind49_V3End` | release inverted | 6,000 |
| 143 | `EffectKind49_V2Fly` | x down 0x8000 | 6,000 |
| 144 | `EffectKind45_DrawSegment` | y-major step sign | **0: equivalent** |
| 145 | `EffectKind48_StepSparks` | camera not loaded | 6,000 |
| 146 | `EffectKind45_DrawSegment` | x-major step inverted (57's near variant) | 1,400 |
| 147 | `EffectKind45_DrawSegment` | y-major step inverted (144's near variant) | 2,119 |
| 148 | `EffectKind48_DrawSpark` | corner from the screen x truncated (113's near variant) | 4,955 |

**145 of 148 refused by a count.** Three equivalent mutants, each with its
near variant refused:

- **57 and 144** (`EffectKind45_DrawSegment`'s step on the minor axis at `<`
  instead of `<=`): the two differ only when the ends' minor coordinates are
  equal, and then the minor distance is 0, its error increment `2 * 0` is 0 and
  the error starts at minus the major distance, so it never turns positive and
  the step is never taken. No input tells them apart. Near variants 146 and
  147 (the step inverted) refused.
- **113** (`EffectKind48_DrawSpark`'s corner computed in float arithmetic
  instead of the x87's 53 bits then a float): a float screen x less a 16-bit
  whole number is exact in 53 bits unless the x is far below 1 in magnitude,
  so both round once to the same float; the rest differ only on a
  double-rounding tie no random draw reaches. Near variant 148 (the corner
  from the screen x truncated) refused.

Four passed on the first run and were the fuzz's fault, refused once it was
fixed: 27 (the window's lower edge: the phase was never seeded two steps below
it), 35 (the middle grid line's colour: the colour bytes are overwritten before
the end, so the line helpers now note them at each call), 97 (the in-use test:
a free record of kind 0x48 with no live one was never seeded), 119 (a null
puff started: the effect-standard `0x47CF20` answers garbage, now the real
find). The lowest counts (12..29 rounds: 22, 29, 30, 119, 131) are single
button or null paths inside a state with several calls.

## 7. Latent defects (Capcom's, described, not fixed)

- **Kind 0x45's trace reads 72 points from a 36-point history.**
  `_PushSample` keeps 36 samples (it moves 35 dwords), `_DrawTrace` hands
  `_DrawPoints` a count of 0x48, so the 36 dwords after the history
  (`0x92C010..0x92C09F`, inside `EffectKind30_Shards`' block, where kind 0x49's
  glow record `0x92C060` lies) are read as points: whatever lies there is
  plotted, or joined to its neighbour, when its x falls in the panel's clip
  columns. And `_Start` clears 32 samples of the 36.
- **A stale point can hang the game.** `_DrawSegment` counts its steps in a
  byte and compares with the 16-bit distance: past 0xFF on the stepping axis
  it never returns. The trace's own samples stay close (the amplitude is capped
  at 0x18), but a segment from one of the stale points above to a sample can
  be longer. Ours aborts with a message there.
- **Kind 0x45's state 0 leaves the offset `+0x36` unset**: without flag 0x25
  the samples are computed with whatever the record held.
- **Kind 0x46 never ends itself**: its state 4 draws the flash every frame;
  its spawner (chapter 7) waits on the counter and something else must free the
  record. The redraw leaves every live sprite's and the leader's `+0x29` at 5.
- **Kind 0x47's lookup reads a sixth triple** past the five at `0x654564` when
  the model's animation byte is none of the keys (`0x654573..0x654575`, zeros in
  the image): animation 0 from 0.
- **Variant 3 writes through a pointer another variant sets.** `_V3Burst` adds
  to the word `+0x10` behind `0x6761D0`, which only E2E's `0x4789D0` (variant
  4's state 0) sets, to `0x92D1C8`; the image's initial value is not an address
  of the game. Chapter 7 spawns variants 3 and 4 together, so variant 4 runs
  first in practice; a variant 3 alone would write through the stale value.
- **The five dispatchers and the five variant dispatchers index unchecked.**
  `Scena07_TakeEffect49` stores its argument into `+1` unchecked (its callers
  pass 0..9).

## 8. Calls across groups

**Outbound, raw** (`effect_2d_callees.h`): `0x4790C0`, `0x47CF20`, `0x4790F0`,
`0x479260` (variant 0's puffs), `0x4794D0`, `0x4796B0` (variant 2's glow),
`0x47A110`, `0x47A130`, `0x47A150`, `0x47A200` (variant 3's dust) - **in no
group of this round** (`analysis/round13_cut.tsv` lists none of them; they lie
in E2E's band `0x4789D0..0x47B790` and `0x47CF20` in E2F's): the coordinator's
to place. Read for what they take and answer (the comments in
`effect_2d_callees.h`); the scenario harness's effect-standard rows stand in
for them. **By name, already ours**: EGT's `EffectGte_LoadMapCamera`,
`_ProjectPoint`, `_ProjectSize` (`band_rows.py --edges`: E2D -> EGT 3), S14's
`WeretigerFx_Next` and FC1's `Effect_StateRelease` (table cells),
`Area85_ClutShift` (AR2B's), and the engine's.

**Into E2E** (not calls - table cells): `EffectKind49_Variants[5..9]` and
`EffectKind49_V4States[0..3]` hold E2E's `0x478B30`, `0x478C30`, `0x478D30`,
`0x478E40`, `0x478EF0` and `0x4789D0`, `0x478A40`, `0x478AB0`, `0x478AF0`; the
tables are named here (their dispatchers are this group's), the fuzz swaps
their cells for recorders. **E2E should not name `0x654584` or `0x6545FC`
again.**

**Inbound from outside the group** (for the rebinding pass): none by call.
`Effect_RunObjects` (ours) reaches the five dispatchers through
`Effect_KindHandlers` `0x655464..0x655474` (read in place); FC1's
`EffectKind14_States[2]` and `EffectKind3C_States[2]` hold
`EffectTwinSprites_Release` (read in place); the table `0x65461C[36]` also
holds it (not named, not this group's). The spawners store the kinds
(`Scena06_Run13`, `Area85_SpawnEffect47A` / `B`, area 52's handlers,
`Scena07_TakeEffect49`, chapter 7's step 0x15).

## 9. The live route

`analysis/remaining_catalog.tsv`'s reach columns (attract, shop, world map,
combat) are empty for all 54 rows. None of the 31 first-call traces under
`analysis/calltrace` (`reach_dragon`, `reach_whelp` and the A/B runs) has an
entry at any of the 54 addresses; fifteen of them (the non-hidden ones) were
in `entries_logic.txt` at the time, so armed. **Fuzz only**: the coordinator's
frame-hash A/B covers none until the owner records a route through chapter 6's
run 13, area 52, area 85 or chapter 7's steps that spawn these kinds.

## 10. The rebinding

`grep -rn -i` of the 54 addresses and the ten tables' and five handler cells'
in `src/` (and `band_rows.py --refs`): one raw reference, a comment in
`effect_1c.cpp` (E1C's, merged in wave one) naming "E2D's 0x478160" -
**rebound** to `EffectTwinSprites_Release`. No constant, call site or fuzz key
of another file names an E2D address; `scenario_harness*.cpp` lists none of
them. Left as they are: `symbols.toml` evidence strings of FC1 and AR2B that
cite `0x478160` / `0x478649` as the addresses they read (history, not
references).

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29, a commented block): the 39
hidden starts, and the smaller extents of `0x477B20` (0x7D), `0x477D20` (0x86)
and `0x4783C0` (0x18A), their old lines left for the merger to drop. The
twelve other non-hidden rows already had lines matching the code.
