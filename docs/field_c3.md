# Group FC3: object steering, two mode frames, the field core's state-2 sub-states

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3, [`takeover-queue-round12.md`](takeover-queue-round12.md) section 8), wave
two, on the round branch's tip `61be26e`. **63 functions ours**
(`src/game/field_c3.cpp`, shadow name `field_c3`): the cut table's 57 rows
for FC3 (`analysis/round12_cut.tsv`), `0x5254A0` (FH's finding,
[`scenario_harness.md`](scenario_harness.md) section 7.6) and five
dispatchers no list had - `FieldCore_State2Steps` entries 4..8 (section 5).
Each read to its last instruction with capstone and fuzzed through the
scenario harness's field mode ([`scenario_harness.md`](scenario_harness.md)
section 7) without edits to it: 189,000 rounds, 0 mismatches. CONTROLS_LINE
Fuzz-only except `FieldCore_ScriptMove`, which the whelp route enters
(section 9).

The band is two runs of the field's resident code: `0x5172C0..0x5195F9`
(object steering among the field objects' kind handlers, whose other
functions are ours since round eight, [`object-kinds.md`](object-kinds.md))
and `0x525390..0x526DAF` (the field core's state 2, whose dispatcher
`FieldCore_State2` and fade are ours, [`mode_states.md`](mode_states.md)).
No Breath of Fire III gameplay is stated here: what a sub-state does is what
its code does; the names say so and no more.

## 1. What each function does

`s` is `Sprite_Current` (an object record, 0xA4 bytes: `+1..+4` the state
bytes, `+5` a CLUT slot, `+8` the facing, `+9` / `+0xA` counters, `+0xC` /
`+0x10` / `+0x14` the velocities, `+0x34` / `+0x38` x / z in 16.16, `+0x3E`
the height, a word), `f` is `Field_State` (the party object, `+0x124..` its
move-script context, `+0x128` the pace, an index into `Field_MoveSpeeds`,
`+0x137` a state the vertical and jump moves announce). Every original
re-reads both after each call, and ours does the same.

### 1.1 Object steering and the fades (`0x5172C0..0x5195F9`)

| Function | Entry | Bytes | Reached from | What (by the code) |
|---|---|--:|---|---|
| `Mode11_FieldFrame` | `0x5172C0` | 0x2D | mode 11's steps `0x496790`, `0x4967B0` (jmp), `0x4967C0` (Capcom's) | `Field_MembersFrame`, `AreaMap_Frame`, FE2's `0x536F10`, `Party_ExtraScreens`, `Party_UpdateScreens`, `0x5372E0`, `Effect_RunObjects`, `MoveScript_TintFrame`, tail `Field_DrawFrame` |
| `Mode8_Step5` | `0x517330` | 0xA | mode 8's step table `0x656AB8[5]` | `0x42D710` (the dispatch on the menu byte `0x929F00`), tail `Field_RunTaskRecords` |
| `Mode8_Step8` | `0x517340` | 0xA | `0x656AB8[8]` | `0x57DFF0` (the other dispatch on `0x929F00`), tail `Field_RunTaskRecords` |
| `MoveCmd_OpE7` | `0x518B20` | 0x19 | `MoveScript_GroupE` (op E7) | `Field_ObjectHandlers[n & 0xFF](Field_ActiveMember)` |
| `Field_ObjectApproachDirection` | `0x518E20` | 0xA3 | `Field_ObjectApproach` | the leader (`ObjTrio +0x34 / +0x38`) within `s`'s half-widths `object +0x98 / +0x9A` (`|d| >> 16`, signed): `Field_ObjectBestDirection(object, 1)` - 0xFF clears `s[9]` and answers 0xFF, a new direction becomes `s[8]` with `object +0x80` bit 3 - answering `s[8]`; else `Field_ObjectRandomTurn`, `Field_ObjectOpenDirection`'s `al` |
| `Field_ObjectAvoidDirection` | `0x518ED0` | 0xA5 | `Field_ObjectAvoid` | the same with `toward` 0 (the differences taken the other way round; the absolute values agree) |
| `Field_ObjectBestDirection` | `0x518F80` | 0x11C | the four above | the leader's point one pace on (`ObjTrio +9` times `+0xC / +0x10`); `s[8]` made odd; four quarter turns, each not `Field_ObjectBlockedAhead` scored by the sum of the two distances from the step to the point, the **largest kept for `toward` not 0, the smallest for 0** (unsigned, in `object +0x94`); the kept facing, 0xFF for none |
| `Field_ObjectFadeOutStart` | `0x5193D0` | 0x39 | `Field_ObjectFadeOutSteps[0]` | `Sprite_SetTint(s, 0x1F, 0x1F, 0x1F, 1)` into `Field_ActiveMember +0x9F`, `s[0]` bit 6 off, `s[4] = 1` |
| `Field_ObjectFadeOutStep` | `0x519410` | 0xC6 | `Field_ObjectFadeOutSteps[1]` | the tint record's `+2..+4` down to 0; at a signed sum of 0 `Tint_Release`, the member's `+0x80` bit 1 off, `s[1] = s[3]`, `s[4] = 0` |
| `Field_ObjectFadeInStart` | `0x519500` | 0x2D | `Field_ObjectFadeInSteps[0]` | `Sprite_SetTint(s, 0, 0, 0, 1)`, `s[4] = 1` |
| `Field_ObjectFadeInStep` | `0x519530` | 0xC9 | `Field_ObjectFadeInSteps[1]` | `+2..+4` up while below 0x1F (signed); at a sum of 0x5D the release, bit 2 off |

**`Field_ObjectBestDirection`, read whole this time:** the 2026-09-22
reading (its first 60 instructions) had the keeping the other way round;
the code keeps the **farthest** step for `toward` 1 (`Field_ObjectApproach`'s
call) and the nearest for 0 (`jae` / `jbe` on `object +0x94` started at 0 and
0x1000000). The names `Field_ObjectApproach*` / `Avoid*` are the callers'
table slots' (entries 1 and 2 of `Field_ObjectHandlers`), kept; what the
objects look like doing it is the owner's to say. The evidence strings say
so.

### 1.2 The field core's state 2 (`0x525390..0x526DAF`)

`FieldCore_State2` (ours) jumps through `FieldCore_State2Steps` by `s[2]`;
entry 2 is the fade (ours). The other eight are this group's, each a
dispatcher on `s[3]` (or `s[4]`) over a table of its own:

| `s[2]` | Dispatcher | Table (entries) | Steps |
|---|---|---|---|
| 0 | `FieldCore_ScriptMove` `0x525390` (0x41) | `FieldCore_ScriptMoveSteps` `0x660140` (6), by `+3` | `_Align` `0x5253E0`, `_Next` `0x5254A0`, `_Step` `0x5256A0`, `_Wait` `0x525710`, `_ShadeLower` `0x525740`, `_ShadeFade` `0x525770` |
| 1 | `FieldCore_Attached` `0x5257A0` (0x101) | - | then `FieldCore_ScriptMove` |
| 3 | `FieldCore_Hop` `0x525960` (0x12) | `FieldCore_HopSteps` `0x660160` (5), `+3` | `_HopBegin` `0x525980`, `_HopLaunch` `0x5259D0`, `_HopRise` `0x525B20`, `_HopFall` `0x525B90`, `_HopLand` `0x525C50` |
| 4 | `FieldCore_Vertical` `0x525CA0` (0x1E) | `FieldCore_VerticalSteps` `0x660174` (2), `+3` | `FieldCore_Up` `0x525CC0`, `FieldCore_Down` `0x525EF0` |
| 4, `+3` 0 | `FieldCore_Up` (0x12) | `FieldCore_UpSteps` `0x66017C` (8), `+4` | `_UpBegin` `0x525CE0`, `_UpOut` `0x525CF0`, `_UpArrive` `0x525D40`, `_VerticalShade` `0x5261C0`, `_UpIn` `0x525E30`, `_UpWait5` `0x525E90`, `_UpWait6` `0x525EB0`, `_UpEnd` `0x525ED0` |
| 4, `+3` 1 | `FieldCore_Down` (0x12) | `FieldCore_DownSteps` `0x66019C` (10), `+4` | `_DownBegin` `0x525F10`, `_DownWait1..4` `0x525F30..0x525F90`, `_DownOut` `0x525FB0`, `_DownArrive` `0x526000`, `_VerticalShade`, `_DownIn` `0x5260A0`, `_DownLand` `0x526120` |
| 5 | `FieldCore_JumpExit` `0x5261E0` (0x1E) | `FieldCore_JumpExitSteps` `0x6601C4` (5), `+3` | `_JumpExitBegin` `0x526200`, `_Out` `0x526240`, `_Arrive` `0x526280`, `_Shade` `0x5263C0`, `_In` `0x5263E0` |
| 6 | `FieldCore_TileD0` `0x526490` (0x23) | `FieldCore_TileD0Steps` `0x6601D8` (2), `+3`, a call then `Sprite_ScriptTick` | `_TileD0Begin` `0x5264C0`, `_TileD0Move` `0x5264F0` (with `_TileD0Exit` `0x5266B0`, `_TileD0Probe` `0x526820`, `_TileD0Slope` `0x526880`) |
| 7 | `FieldCore_Fall` `0x526A90` (0x1E) | `FieldCore_FallSteps` `0x6601E0` (2), `+3` | `_FallBegin` `0x526AB0`, `_FallSpin` `0x526B00` |
| 8 | `FieldCore_Recoil` `0x526B80` (0x12) | `FieldCore_RecoilSteps` `0x6601E8` (2), `+3` | `_RecoilBegin` `0x526BA0`, `_RecoilBlink` `0x526D30` |

The dispatchers of sub-states 4, 5, 6 and 7 first set `f +0x137` (3, 2, 5,
6); the steps that end the move put it back to 0 with the object in state 1
(`s[1] = 1`, `s[2] = s[3] = 0`). Each table's count is the code's: the steps
write the index byte only inside it (the evidence strings list the writes),
and each table ends where the next one starts. `FieldCore_VerticalSteps`'
third word is `FieldCore_UpSteps`' first: the tables are laid back to back.

What the steps do, by the code (the evidence strings in `symbols.toml` have
every store):

- **The move script's walk (sub-state 0).** `_Align` rounds x and z onto the
  half-cell grid (`(v + 0x4800) & ~0x7FFF`) with `MapView_SlopeAt`'s height
  and the direction's animation, and once on it steps `+3` to `_Next`.
  `_Next` counts `f +0x125` down, then `MoveScript_Step(f + 0x124, the script
  at f +0x130)`: 0xFF waits; else by the context's flag byte bit 3 ends the
  move (`Field_ScriptFlags` bit 8 off unless `Field_StatusBits` bit 6,
  `Field_MemberTimers`, state 1), bit 5 goes to `_Wait` (until
  `Field_Request` is not 2), bit 1 to `_ShadeLower` (a shade to 0xC0), bit 2
  to `_ShadeFade` (back to the palette), else a timed step (`_Step`:
  `Field_LeaderStepTick` per frame, `f +0x12B` jumps by `Field_JumpStart`) or
  `+3 = 1`.
- **Attached (1).** With `s[9]` 0 the position and `+0x64..+0x6C` are copied
  from `Sprite_ObjectsExtra[s +0x18]` and `MoveCmd_AttachOffset`'s three
  words added; then the walk.
- **The hop (3).** `_HopBegin` sets the pace 3 and an animation;
  `_HopLaunch`, once the animation's script ticks through, sets the frames
  `0x20 / speed`, the step velocities, a rise `(+0x70 + 1) << 6` and a
  gravity `(-1 - +0x70) << 3`, points the camera's kind-2 target at the
  landing (unless `s[5]`), plays sound `0x104`; `_HopRise` counts the frames
  with the gravity added; `_HopFall` falls until `MapView_GroundAt` is above
  the height, then lands (`+3 = 4`); `_HopLand` waits for the animation and
  `Field_TileD0`, then state 1.
- **The vertical moves (4).** Up: the height climbs 0x10 a frame; past
  `ground + 0x200` `Field_Request = 5` (`_UpOut`, an area change); `_UpArrive`
  places the object 0x200 below the ground, stepped back along the jump
  table's step, with a shade fade when `s[5]`; `_UpIn` climbs to the ground
  at `(0x904EF4, 0x904EF8)` less an s16 of `0x66978C[f +0x89]`; FE2's
  `0x536290`, `0x5362D0`, `0x5363C0`, `0x536440` finish it (`_UpEnd` clears
  `Field_ScriptFlags2` bit 3 unless `s[5]`). Down: FE2's `0x535FE0` and four
  waits, the height down 0x10 a frame, `Field_Request = 5` below
  `ground - 0x200` (`_DownOut`), `_DownArrive` 0x200 above the ground,
  `_DownIn` down to it, `_DownLand` four frames of animation, state 1.
- **The jump out (5).** `_JumpExitBegin` sets `Field_ScriptFlags2` bit 3;
  `_Out` jumps (`Field_JumpStart`) three times, `Field_Request = 5` at the
  third; `_Arrive` steps back three (raised) or two jump steps, pace 2, the
  ground's height, a shade fade with `s[5]`; `_In` jumps `+0xA` times more,
  then state 1 with pace 3.
- **The 0xD0 cells (6).** `Field_TileD0` puts an object standing on them in
  state 2, sub-state 6. `_TileD0Begin` keeps the facing in `+0xB`, pace 4;
  `_TileD0Move`, between steps, leaves (state 1, facing `+0xB`) once no 0xD0
  cell is under the object; else `_TileD0Exit` turns it toward the first of
  the eight directions whose step is not `Field_WayBlocked` and holds no 0xD0
  cell - else toward the odd direction with the lowest ground
  (`_TileD0Probe`) - and with `_TileD0Slope`'s steep edge ahead goes to the
  hop (`s[2] = 3`); else `Field_CellAhead`, the pad's direction
  (`Input_Held & 0xF000`: 0x1000 0, 0x2000 2, 0x3000 1, 0x4000 4, 0x6000 3,
  0x8000 6, 0x9000 7, 0xC000 5) into `+0xB` and its animation,
  `Field_JumpStart`.
- **The fall (7).** Sound `0x107`, `Area_LinkAt(word +0x36, word +0x3A)`, then
  the height rises by a velocity that loses 8 a frame while the facing turns
  one eighth a frame; more than 0x100 below the ground `0x905BA5 |= 2` and
  `Field_Request = 5`.
- **The recoil (8).** The palette back if shaded, bit 6 off, x / z rounded
  onto the grid (with `Field_Kind2X` / `Z` and `Field_ViewReset` unless
  `s[5]`), the facing reversed for `Field_CellAhead` (1 or 3:
  `Field_LeaderPushObjects`, else a jump; otherwise the steps cleared),
  FE2's `0x534C20(+0xB)`, the facing back, `+0xA = 8`, sound `0x108`;
  `_RecoilBlink` applies the velocity for `+9` frames and blinks bit 6 on
  `+0xA`'s parity, then state 1.

### 1.3 The PSX twins

`analysis/pairs_propagated.json` pairs 26 of the 63 (the evidence strings
cite each twin's address and pairing method). The sibling's `names/*.toml`
and `symbols.toml` name none of them, so no name was transferred; the twins
stay hypotheses, not read.

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed. Where the original indexes past a table ours aborts with a `Fatal`
naming the function (the owner's rule, round9 doc section 6): the ten
dispatchers past their tables (`Run` in `field_c3.cpp`) and `MoveCmd_OpE7`
past `Field_ObjectHandlers`' eleven. `FieldCore_HopLaunch` aborts on a speed
of 0 where the original's `idiv` faults. Reads by a byte into the image's
tables stay unchecked, as the original's (`Field_MoveSpeeds[f +0x128]`, the
jump steps `0x6696DC[s[8]]`, `Field_DirectionSteps`, `0x66978C[f +0x89]`),
and so does `Sprite_ObjectsExtra[s +0x18]` (section 10).

DIV-0024 (`Field_ObjectFadeOut` / `FadeIn` stop past their tables) is the
hosts', not this group's: ours' hosts run the four fade cases inline and
never jump through `Field_ObjectFadeOutSteps` / `Field_ObjectFadeInSteps`.
With `BOF3X_ORIGINAL=Field_ObjectFadeOut,Field_ObjectFadeIn` the originals'
jumps reach these four, ours, the same code.

`eax` on return: `Field_ObjectApproachDirection`, `_AvoidDirection`,
`_BestDirection`, `FieldCore_TileD0Probe` and `FieldCore_TileD0Slope` answer
in `al`, which their callers read; every other is `void` - the dispatchers'
callers (`FieldCore_State2`, ours; `0x496430` and mode 11's steps) drop
`eax`, and no table entry reads a word of its dispatcher's caller.

## 3. The arguments pushed with leftovers

Where the original pushes a byte or word in a register whose upper bytes are
leftovers, ours passes the value and the fuzz lists the callee with the mask
its code reads (for the coordinator's fold, round twelve section 7 item 1):

| Callee | Mask | The read |
|---|---|---|
| `Sprite_EnsureAnimation` | the byte | `Sprite_SetAnimationAt` reads the animation as `dl` / `& 0x7F` (its evidence); the standard set logs the dword |
| `MapView_SlopeAt` | x, z, the direction's byte | `AreaMap_Slope` reads "the direction byte n"; also louder: it writes the "sloped" byte `0x903850`, which every caller here reads straight after |
| `MapView_SetElevation` | the low word | only the low 16 bits reach memory (its note: a build taking 16 bits passed the fuzz) |
| `MoveCmd_AttachOffset` | none for the out pointer, the index byte | the pointer is the caller's stack: the original's frame and ours differ, so the standard set's whole-word log could never match a caller passing a local (a fold item: the standard row is wrong for every caller) |
| `Field_WayBlocked` | x, z, `raised & 0xFF`, the ground's word | `raised & 0xFF` (its evidence), the ground compared as a word ([`event_leader.md`](event_leader.md), `Field_WayBlocked4`'s reading) |
| `Area_LinkAt` | two bytes | `x & 0xFF`, `z & 0xFF` (its evidence, `area_entry.cpp`) |
| FE2's `0x534C20` | the byte | `and eax, 0xFF` at `+0xE` |

Two more are re-listed louder, not narrower: `MoveScript_Step` answers
0xFF..0x0F (0xFF ends the walk) and moves the context's flag byte it was
handed; `Field_CellAhead` answers 0..4 (the recoil reads 1 and 3).

## 4. The fuzz (`field_c3_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=field_c3`, `Group::field`, 3,000 rounds a
function (189,000), **0 mismatches** in this worktree; 334,310 calls to the
stand-ins; 23,104 bytes of state in 39 regions; 313 stand-ins registered
(174 of them the field-standard set). Shapes: `kState` for the mode frames,
`kCall` for op E7 (a byte below 11), the three direction picks (a sprite
record; `al`) and `FieldCore_TileD0Probe` (a scratch word out; `al`),
`kSprite` for the rest (`FieldCore_TileD0Slope`'s `al` compared).

- **Tables swapped for recorders**: the ten of section 1.2 and
  `Field_ObjectHandlers`. The latter's eleven entries are **typed stand-ins**
  (one argument logged: the object, `Field_ActiveMember`), which a handler
  recorder would not log.
- **This group's own callees**: `FieldCore_ScriptMove` and
  `FieldCore_ScriptMoveNext` as `kPhase` (tail-jumped / called by address);
  `FieldCore_TileD0Exit` (turns `s[8]`), `FieldCore_TileD0Slope`,
  `FieldCore_TileD0Probe` (writes the word out) and
  `Field_ObjectBestDirection` (0xFF..7, turns `s[8]`) as recorders with
  effects.
- **Regions beyond field mode's**: `Field_ActiveMember`'s cell, the first 44
  tint records (every record's `+0x9F` seeded below 44), the point
  `0x904EF4 / 0x904EF8`.
- **The seed**, every round: the four sprite records' facing (3, 5, 7, 2, 6
  or any), counters `+9` / `+0xA` (0..8 or any), `+5`, `+0x14` around -0x80,
  `+0x18` below 4, x / z on the grid, half off it or anywhere, `+0x5C`,
  `+0x70` (0, 1, 0xFF or any), `+0x9F`; `f +0x124` a single bit half the
  time, `+0x125` / `+0x12B` 0 half the time, the pace 1..5; the
  sloped byte; `Field_Request` 2, 5, 0 or any; the pad's high nibble over the
  eight directions and others. Per function: each dispatcher's index inside
  its table; the leader within the half-widths half the time; the tint
  record near its end; the recoil's, landing's and jump's counters at their
  boundaries.
- **The disturbance** (the group's `disturb`, from the hash it is given):
  `+9`, `+0xA`, `+8`, `+5`, `+0x70`, the height, `+0x14`, a coordinate's low
  word, `f +0x124`, `f +0x12B`, the sloped byte, `Field_ActiveMember`.

`BOF3X_SHADOW='*'`: STAR_LINE

## 5. What the cut and the tool said, settled

- **`0x5253E0` is 0xB3 bytes**, not the tool's 0x2BB: it ends in a tail
  `jmp 0x5254A0`; the code after its padding is `FieldCore_ScriptMoveNext`
  (0x1FB bytes), reached by that jump, by `FieldCore_ScriptMoveStep`'s and by
  `FieldCore_ScriptMoveSteps[1]` - FH's finding, taken as a function.
- **Five starts in no list**: `FieldCore_State2Steps`' entries 4..8
  (`0x525CA0`, `0x5261E0`, `0x526490`, `0x526A90`, `0x526B80`), each a
  dispatcher of 0x12..0x23 bytes in the band between two cut rows (the
  catalogue's `0x525390` extent of 0x520 held them; `band_rows.py --group FC3`
  did not flag them).
  `mode_states.cpp` names all nine entries in a comment and reads the table
  in place; nothing of ours held their code. Taken.
- **Four starts inside a host that is ours**: the fade cases `0x5193D0`,
  `0x519410`, `0x519500`, `0x519530` sit in `Field_ObjectFadeOut`'s and
  `Field_ObjectFadeIn`'s catalogue extents, and **ours' hosts hold their code**
  (`object_kinds.cpp`'s `FadeOutStart` .. `FadeInStep`, run by a switch).
  They are still reached by address - `Field_ObjectFadeOutSteps` /
  `Field_ObjectFadeInSteps` name them, and the original hosts jump through
  them under `BOF3X_ORIGINAL` - so each is taken as its own function; the
  two copies are one behaviour, proved by two fuzzes.
- **`0x517330`, `0x517340`** are "hidden in `Menu_Frame`": `Menu_Frame` is
  0xF bytes and `Shop_Frame` (`0x517300`) 0x28; neither holds them.
- The tool's extents are right for the other 57 (45 cut sizes differ by
  padding only); `0x5264F0`'s 0x1B4 includes its code at
  `0x526650..0x5266A3` past a run of padding.
- No start of the cut is a non-function.

## 6. Controls

CONTROLS_SECTION

## 7. What nothing reached, and the limits

- **The tables' words past their counts** are not run (the seed keeps each
  index inside; ours aborts there).
- **The callees' own behaviour**: every callee is a recorder, so the hop's
  or the fall's frames are compared one call at a time, never as a sequence
  of frames; FE2's nine steps of the vertical moves are recorders until FE2
  merges.
- **`Field_ObjectBestDirection`'s scores** are compared on random positions;
  that the kept direction is the one the game shows is not measured.

## 8. Calls across groups

**Out of FC3, raw** (`field_c3_callees.h`, a recorder each in the fuzz; the
coordinator rebinds after FE2 merges):

| Callee | Owner | Called from |
|---|---|---|
| `0x536F10` | FE2 | `Mode11_FieldFrame` |
| `0x535FC0` | FE2 | `FieldCore_UpBegin` |
| `0x535FE0` | FE2 | `FieldCore_DownBegin` |
| `0x536050`, `0x5360C0`, `0x536130`, `0x536170` | FE2 | `FieldCore_DownWait1..4` |
| `0x536290` | FE2 | `FieldCore_UpIn` |
| `0x5362D0`, `0x5363C0`, `0x536440` | FE2 | `FieldCore_UpWait5`, `_UpWait6`, `_UpEnd` |
| `0x534C20` | FE2 | `FieldCore_RecoilBegin` |
| `0x5372E0`, `0x42D710`, `0x57DFF0` | nobody (the field-standard set, by address) | `Mode11_FieldFrame`, `Mode8_Step5`, `Mode8_Step8` |

These are RT's `--edges` twelve (FH's 7.6 table) and the three unowned.

**Into FC3 from outside the group:**

| Function | Callers |
|---|---|
| `Mode11_FieldFrame` | Capcom's mode-11 steps `0x496790`, `0x4967C0` (call), `0x4967B0` (jmp) - in no group |
| `Mode8_Step5`, `Mode8_Step8` | mode 8's step table `0x656AB8`, read by Capcom's `0x496430` |
| `MoveCmd_OpE7` | ours: `MoveScript_GroupE` (`move_groups.cpp`), by name |
| `Field_ObjectApproachDirection`, `_AvoidDirection`, `_BestDirection` | ours: `Field_ObjectApproach`, `Field_ObjectAvoid` (`object_kinds.cpp`), by name |
| the four fade cases | `Field_ObjectFadeOutSteps` / `Field_ObjectFadeInSteps`, read only by the original hosts |
| `FieldCore_ScriptMove`, `FieldCore_Attached`, the seven dispatchers | `FieldCore_State2Steps`, read in place by ours' `FieldCore_State2` |
| everything else | this group's own tables and calls |

## 9. The live route

Of the 63, only `FieldCore_ScriptMove` appears in the owner's two traces
(`analysis/calltrace/reach_whelp/bof3x.calltrace.tsv`, frame 394, return
address `0x51737D` inside `Field_MembersFrame`, the members' state dispatch -
so `FieldCore_State2` sent a member into sub-state 0); `reach_dragon` enters
none. **The traces could not have seen the hidden starts**: the tracer arms
the starts `entries_logic.txt` lists, and none of the 54 hidden ones had a
line until this group added them (section 11) - `0x525390`'s line then ran to
0x520 bytes over all of sub-state 0's steps. So the coordinator's frame-hash
A/B after the wave covers `FieldCore_ScriptMove` and whatever the new lines
arm on a re-trace; the rest is fuzz-only.

## 10. Latent defects (Capcom's, described, not fixed)

- **Unchecked dispatchers**, the class: ten tables indexed by a state byte
  and `Field_ObjectHandlers` by op E7's operand, each running into the next
  table's words past its count (the tables are back to back:
  `FieldCore_VerticalSteps`' third word is `FieldCore_UpSteps`' first). Ours
  aborts. Nothing measured reaches it: every step writes its index inside its
  own table.
- **`FieldCore_HopLaunch` divides 0x20 by `Field_MoveSpeeds[f +0x128]`**,
  and entry 0 of that table is a speed of 0: a hop launched with the pace at
  0 faults (`idiv`). `FieldCore_HopBegin` sets the pace to 3 first, so only a
  hop entered at its second step from outside could; ours aborts.
- **`FieldCore_Attached` indexes `Sprite_ObjectsExtra` by the dword `+0x18`
  unchecked** (four records; past them it copies whatever follows as a
  position). Kept unchecked: a read, not a jump.
- **`FieldCore_TileD0Slope`'s raised facing-5 path** passes direction 3 to
  its third `MapView_SlopeAt` where the other probes of that path pass 5 -
  a copy slip by the look of it, kept (control C133 proves ours keeps it).
- **The two arrivals test different facings**: `FieldCore_UpArrive` picks
  animation 0x3C for facing 7, `FieldCore_DownArrive` for facing 3. Kept;
  whether that is intent is the owner's to say.
- **`FieldCore_TileD0Exit`'s fall-back** probes the odd directions one plain
  step away, where its first pass scales the step by `+0x70 + 1`. Kept.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29): 56 lines - the 63 but the
seven already exact (`005172C0 2D`, `00518B20 19`, `00518E20 A3`, `00518ED0
A5`, `00518F80 11C`, `005266B0 162`, `00526820 60`). `00525390 41` and
`00526880 20F` cut the listed `00525390 520` and `00526880 530`; the fade
cases' lines cut `005193B0 126` and `005194E0 119` (the hosts, ours).
