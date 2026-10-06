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
section 7) without edits to it: 189,000 rounds, 0 mismatches. 160 controls planted, all refused by a count (section 6).
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

Three more are re-listed louder, not narrower: `MapView_GroundAt` answers
at the height's boundaries (section 4); `MoveScript_Step` answers
0xFF..0x0F (0xFF ends the walk) and moves the context's flag byte it was
handed; `Field_CellAhead` answers 0..4 (the recoil reads 1 and 3).

## 4. The fuzz (`field_c3_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=field_c3`, `Group::field`, 3,000 rounds a
function (189,000), **0 mismatches** in this worktree; 334,000 calls to the
stand-ins; 23,104 bytes of state in 39 regions; 313 stand-ins registered
(174 of them the field-standard set). Shapes: `kState` for the mode frames,
`kCall` for op E7 (a byte below 11), the three direction picks (a sprite
record; `al`) and `FieldCore_TileD0Probe` (a scratch word out; `al`),
`kSprite` for the rest (`FieldCore_TileD0Slope`'s `al` compared).

- **Tables swapped for recorders**: the ten of section 1.2 and
  `Field_ObjectHandlers`. The latter's eleven entries are **typed stand-ins**
  (one argument logged: the object, `Field_ActiveMember`), which a handler
  recorder would not log.
- **Answers at the boundaries**: `MapView_GroundAt` (re-listed) answers two
  times in three at `Sprite_Current`'s height -0x200, +0x200, +0x100, -0x100
  or 0, give or take one; `MapView_SlopeAt` 0x3F..0x41 half the time and sets
  the sloped byte `0x903850`.
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

`BOF3X_SHADOW='*'` in this worktree: exit 0 (791 s), `inject: 6616 ours, 0 left
original` (wave one's 6,553 and these 63), 970 lines of `0 MISMATCHES` and
none other; `field_c3` there 189,000 rounds, 0 mismatches (333,486 calls:
the generator is shared, so the counts move, as section 7.9 of the harness
doc says). `tools/ledger_check.py`: 0 errors.

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

A script (`controls.py` in the session scratchpad, `fc3/`) planted all 160
mutants at once in `field_c3.cpp`, each behind an environment switch
(`BOF3X_FC3_CTL=n`, a one-off build, never committed) and anchored on a
string that must occur once; built once, ran the shadow once per control,
restored the file and rebuilt (FH's form, section 7.8 of the harness doc).
Every one of the 63 functions has at least one; the dispatchers two (the
wrong table, the wrong `+0x137`).

**The first run refused 154 of 160.** The six not refused were the fuzz's
fault, each a boundary its answers never hit: C75 / C94 (`ground ± 0x200`),
C142 (`0x100`), C131 / C135 (a slope of exactly 0x40), C129 (two probes
equal). The stand-ins of `MapView_GroundAt` (answers at `Sprite_Current`'s
height less or plus 0x200, 0x100 or 0, give or take one, two times in three),
`MapView_SlopeAt` (0x3F..0x41 half the time) and `FieldCore_TileD0Probe` (a
word near the height) were made to hit them; all six then refused, and **the
second run, on the committed fuzz, refused all 160 by a count** (exit 3, the
mutated function's rounds below; the baseline with no switch 0 mismatches).
No equivalent mutant was met.

| # | Function | Mutant | Rounds refused (of 3,000) |
|---|---|---|--:|
| C1 | `Mode11_FieldFrame` | Party_UpdateScreens skipped | 3000 |
| C2 | `Mode11_FieldFrame` | 0x536F10 skipped | 3000 |
| C3 | `Mode8_Step5` | 0x57DFF0 called for 0x42D710 | 3000 |
| C4 | `Mode8_Step8` | Field_RunTaskRecords skipped | 3000 |
| C5 | `MoveCmd_OpE7` | Sprite_Current passed for the member | 2252 |
| C6 | `MoveCmd_OpE7` | the neighbouring handler | 3000 |
| C7 | `Field_ObjectApproachDirection` | toward 0 | 558 |
| C8 | `Field_ObjectAvoidDirection` | toward 1 | 535 |
| C9 | `Field_ObjectApproachDirection` | the x half-width test >= | 119 (also `Field_ObjectAvoidDirection` 118) |
| C10 | `Field_ObjectApproachDirection` | the z test against the x half-width | 589 (also `Field_ObjectAvoidDirection` 555) |
| C11 | `Field_ObjectApproachDirection` | +9 kept at 0xFF | 42 (also `Field_ObjectAvoidDirection` 46) |
| C12 | `Field_ObjectApproachDirection` | context bit 2 for 3 | 318 (also `Field_ObjectAvoidDirection` 309) |
| C13 | `Field_ObjectApproachDirection` | dz from the leader x | 520 (also `Field_ObjectAvoidDirection` 490) |
| C14 | `Field_ObjectBestDirection` | the kept comparison reversed | 2423 |
| C15 | `Field_ObjectBestDirection` | no odd start | 1315 |
| C16 | `Field_ObjectBestDirection` | +0x94 start 0xFFFFFF | 1506 |
| C17 | `Field_ObjectBestDirection` | the leader x point by its z step | 924 |
| C18 | `Field_ObjectBestDirection` | eighth turns | 3000 |
| C19 | `Field_ObjectFadeOutStart` | red 0x1E | 3000 |
| C20 | `Field_ObjectFadeOutStart` | bit 7 cleared for 6 | 2233 |
| C21 | `Field_ObjectFadeOutStep` | end at a sum above 0 only | 1045 |
| C22 | `Field_ObjectFadeOutStep` | context bit 0 cleared | 679 |
| C23 | `Field_ObjectFadeOutStep` | +4 not decremented at 1 | 2127 |
| C24 | `Field_ObjectFadeInStart` | +4 = 2 | 3000 |
| C25 | `Field_ObjectFadeInStep` | the climb unsigned | 1317 |
| C26 | `Field_ObjectFadeInStep` | end at 0x5E | 660 |
| C27 | `Field_ObjectFadeInStep` | context bit 1 for 2 | 191 |
| C28 | `Field_ObjectFadeOutStep` | the pose from +2 | 902 (also `Field_ObjectFadeInStep` 248) |
| C29 | `FieldCore_ScriptMove` | +0x138 bit 3 | 2247 |
| C30 | `FieldCore_ScriptMove` | the tick by bit 5 | 749 |
| C31 | `FieldCore_ScriptMove` | the up table | 3000 |
| C32 | `FieldCore_ScriptMove` | no tick test on +7 bit 5 (bit 6) | 1466 |
| C33 | `FieldCore_ScriptMoveAlign` | +0x24 bit 4 | 1009 |
| C34 | `FieldCore_ScriptMoveAlign` | animation +9 | 1452 |
| C35 | `FieldCore_ScriptMoveAlign` | x rounded by 0x4000 | 51 |
| C36 | `FieldCore_ScriptMoveAlign` | +3 up by 2 | 1531 |
| C37 | `FieldCore_ScriptMoveNext` | +0x125 not counted down | 1438 |
| C38 | `FieldCore_ScriptMoveNext` | Field_StatusBits bit 5 | 150 |
| C39 | `FieldCore_ScriptMoveNext` | +3 = 2 on bit 5 | 143 |
| C40 | `FieldCore_ScriptMoveNext` | shade 0x80 | 258 |
| C41 | `FieldCore_ScriptMoveNext` | +3 = 4 on bit 2 | 138 |
| C42 | `FieldCore_ScriptMoveNext` | +0x12B not counted down | 138 |
| C43 | `FieldCore_ScriptMoveNext` | the step animation without +8 | 186 |
| C44 | `FieldCore_ScriptMoveNext` | the last test on bit 7 | 141 |
| C45 | `FieldCore_ScriptMoveNext` | the context at +0x125 | 1562 |
| C46 | `FieldCore_ScriptMoveStep` | the rise kept on bit 6 | 1 |
| C47 | `FieldCore_ScriptMoveStep` | no tick while +9 counts | 2453 |
| C48 | `FieldCore_ScriptMoveWait` | the test against 5 | 1527 |
| C49 | `FieldCore_ScriptMoveShadeLower` | step 8 | 3000 |
| C50 | `FieldCore_ScriptMoveShadeFade` | bit 3 cleared | 986 |
| C51 | `FieldCore_Attached` | +0x24 bit 4 | 523 |
| C52 | `FieldCore_Attached` | z by the x offset | 593 |
| C53 | `FieldCore_Attached` | +0x6C not copied | 593 |
| C54 | `FieldCore_Attached` | stride 0xA0 | 427 |
| C55 | `FieldCore_Hop` | the jump-exit table | 3000 |
| C56 | `FieldCore_HopBegin` | the facing test inverted | 3000 |
| C57 | `FieldCore_HopLaunch` | +0x14 << 5 | 1959 |
| C58 | `FieldCore_HopLaunch` | MoveScript_F3Divisor speed * 4 | 1014 |
| C59 | `FieldCore_HopLaunch` | sound 0x105 | 1990 |
| C60 | `FieldCore_HopLaunch` | gravity -(+0x70) << 3 | 1990 |
| C61 | `FieldCore_HopLaunch` | Kind2Z by the x step | 970 |
| C62 | `FieldCore_HopLaunch` | frames 0x40 / speed | 1990 |
| C63 | `FieldCore_HopRise` | +3 = 4 at the top | 521 |
| C64 | `FieldCore_HopRise` | the elevation with +5 set | 3000 |
| C65 | `FieldCore_HopFall` | the fall pose from -0x7F | 57 |
| C66 | `FieldCore_HopFall` | the flags on bit 2 | 588 |
| C67 | `FieldCore_HopFall` | the landing test unsigned | 524 |
| C68 | `FieldCore_HopLand` | Field_TileD0 ignored | 1325 |
| C69 | `FieldCore_HopLand` | +0x137 = 1 | 688 |
| C70 | `FieldCore_Vertical` | +0x137 = 4 | 3000 |
| C71 | `FieldCore_Vertical` | the fall table | 3000 |
| C72 | `FieldCore_Up` | the down table | 3000 |
| C73 | `FieldCore_Down` | the up table (read past its eight into the down table) | 3000 |
| C74 | `FieldCore_UpBegin` | +4 up by 2 | 3000 |
| C75 | `FieldCore_UpOut` | the margin 0x1FF | 103 |
| C76 | `FieldCore_UpOut` | up 0x11 | 2973 |
| C77 | `FieldCore_UpArrive` | x back << 4 | 2400 |
| C78 | `FieldCore_UpArrive` | facing 3 | 972 |
| C79 | `FieldCore_UpArrive` | shade to +4 = 2 | 1440 |
| C80 | `FieldCore_UpArrive` | +4 = 5 | 1560 |
| C81 | `FieldCore_UpArrive` | the ground 0x100 below | 2976 |
| C82 | `FieldCore_UpIn` | the stop test > | 82 |
| C83 | `FieldCore_UpIn` | the offset by +0x88 | 507 |
| C84 | `FieldCore_UpIn` | x and z swapped | 3000 |
| C85 | `FieldCore_UpWait5` | +4 up by 2 | 1982 |
| C86 | `FieldCore_UpWait6` | 0x5362D0 for 0x5363C0 | 3000 |
| C87 | `FieldCore_UpEnd` | the flag cleared with +5 set | 1000 |
| C88 | `FieldCore_DownBegin` | +4 = 2 | 3000 |
| C89 | `FieldCore_DownBegin` | flag bit 4 | 2231 |
| C90 | `FieldCore_DownWait1` | +4 = 3 | 2033 |
| C91 | `FieldCore_DownWait2` | 0x536050 for 0x5360C0 | 3000 |
| C92 | `FieldCore_DownWait3` | +4 = 5 | 2010 |
| C93 | `FieldCore_DownWait4` | 0x536130 for 0x536170 | 3000 |
| C94 | `FieldCore_DownOut` | the margin 0x1FF | 80 |
| C95 | `FieldCore_DownOut` | down 8 | 2972 |
| C96 | `FieldCore_DownArrive` | the ground 0x100 above | 2971 |
| C97 | `FieldCore_DownArrive` | facing 7 | 1016 |
| C98 | `FieldCore_DownArrive` | shade to +4 = 8 | 1493 |
| C99 | `FieldCore_DownIn` | +9 = 3 | 1434 |
| C100 | `FieldCore_DownIn` | the poses swapped | 1434 |
| C101 | `FieldCore_DownIn` | the first ground kept | 1358 |
| C102 | `FieldCore_DownLand` | pose 6 - +0xA | 246 |
| C103 | `FieldCore_DownLand` | the end at 3 | 1095 |
| C104 | `FieldCore_DownLand` | +9 = 1 between poses | 798 |
| C105 | `FieldCore_VerticalShade` | step 4 | 3000 |
| C106 | `FieldCore_JumpExit` | +0x137 = 3 | 3000 |
| C107 | `FieldCore_JumpExit` | the hop table | 3000 |
| C108 | `FieldCore_JumpExitBegin` | animation +7 | 3000 |
| C109 | `FieldCore_JumpExitBegin` | +3 up by 2 | 3000 |
| C110 | `FieldCore_JumpExitOut` | the change at 2 | 170 |
| C111 | `FieldCore_JumpExitOut` | no Field_JumpStart | 565 |
| C112 | `FieldCore_JumpExitArrive` | back by twice the step | 1421 |
| C113 | `FieldCore_JumpExitArrive` | +0xA = 6 | 1199 |
| C114 | `FieldCore_JumpExitArrive` | pace 3 | 3000 |
| C115 | `FieldCore_JumpExitArrive` | +3 = 5 | 1544 |
| C116 | `FieldCore_JumpExitShade` | +3 up by 2 | 1984 |
| C117 | `FieldCore_JumpExitIn` | pace 4 | 194 |
| C118 | `FieldCore_JumpExitIn` | +0xC kept | 194 |
| C119 | `FieldCore_TileD0` | +0x137 = 6 | 3000 |
| C120 | `FieldCore_TileD0` | no tick after | 3000 |
| C121 | `FieldCore_TileD0Begin` | pace 3 | 3000 |
| C122 | `FieldCore_TileD0Begin` | +0xB from +9 | 2950 |
| C123 | `FieldCore_TileD0Move` | 0x9000 as 6 | 9 |
| C124 | `FieldCore_TileD0Move` | the cells 0xD1 | 566 |
| C125 | `FieldCore_TileD0Move` | the hop at +2 = 2 | 127 |
| C126 | `FieldCore_TileD0Move` | no second Field_CellAhead | 14 |
| C127 | `FieldCore_TileD0Exit` | raised 0 to Field_WayBlocked | 2295 |
| C128 | `FieldCore_TileD0Exit` | the even directions | 381 |
| C129 | `FieldCore_TileD0Exit` | the lowest by >= | 89 |
| C130 | `FieldCore_TileD0Exit` | the step not scaled by +0x70 | 2295 |
| C131 | `FieldCore_TileD0Probe` | steep from 0x40 | 327 |
| C132 | `FieldCore_TileD0Probe` | the slope left as the ground | 2583 |
| C133 | `FieldCore_TileD0Slope` | the raised 5 path last probe direction 5 | 18 |
| C134 | `FieldCore_TileD0Slope` | the flat 3 probe a cell on | 289 |
| C135 | `FieldCore_TileD0Slope` | steep from 0x40 | 171 |
| C136 | `FieldCore_TileD0Slope` | the raised 3 z test inverted | 21 |
| C137 | `FieldCore_Fall` | +0x137 = 7 | 3000 |
| C138 | `FieldCore_Fall` | the recoil table | 3000 |
| C139 | `FieldCore_FallBegin` | the link point swapped | 2980 |
| C140 | `FieldCore_FallBegin` | sound 0x108 | 3000 |
| C141 | `FieldCore_FallSpin` | gravity 4 | 3000 |
| C142 | `FieldCore_FallSpin` | the change at 0xFF | 91 |
| C143 | `FieldCore_FallSpin` | the high flag bit 0 | 547 |
| C144 | `FieldCore_FallSpin` | turning backwards | 3000 |
| C145 | `FieldCore_Recoil` | the tile-d0 table | 3000 |
| C146 | `FieldCore_RecoilBegin` | turned a quarter | 325 |
| C147 | `FieldCore_RecoilBegin` | push on 2 not 3 | 1206 |
| C148 | `FieldCore_RecoilBegin` | +0xA = 7 | 2968 |
| C149 | `FieldCore_RecoilBegin` | bit 4 cleared for 5 | 1185 |
| C150 | `FieldCore_RecoilBegin` | 0x534C20 given +0xA | 2693 |
| C151 | `FieldCore_RecoilBegin` | the view with +5 set | 1919 |
| C152 | `FieldCore_RecoilBlink` | the blink swapped | 2441 |
| C153 | `FieldCore_RecoilBlink` | no Sprite_ClearSteps at 0 | 533 |
| C154 | `FieldCore_ScriptMoveNext` | bit 5 path ignores +7 bit 3 | 73 |
| C155 | `FieldCore_ScriptMoveStep` | no Field_JumpStart | 261 |
| C156 | `FieldCore_ScriptMoveShadeLower` | +3 = 2 | 1944 |
| C157 | `FieldCore_ScriptMoveWait` | +3 = 1 | 2229 |
| C158 | `FieldCore_HopLaunch` | no Kind2 with +5 clear (the test inverted) | 1990 |
| C159 | `FieldCore_VerticalShade` | +4 up by 2 | 2026 |
| C160 | `FieldCore_JumpExitIn` | the end at +0xA 1 | 302 |

**2026-10-05, under the repaired disturbance (round fourteen's review item
1).** The fuzz's `Disturb` switched on `h % 12` and is reached only with
`h % 3 != 0`, so its cases 0 (`+9`), 3 (`+5`), 6 (the sloped byte) and 9 (a
coordinate's low word) never ran; `b9dfe34` draws the case through
`sh::DisturbCase`. The same script, re-run on `451edeb` (every one of the 160
anchors still occurs once, none repaired): **160 planted, 160 refused by a
count**, the baseline 0 mismatches. 43 counts are the table's, 117 moved (the
generator's stream changed), none to 0: the smallest are C46 2 (was 1), C123
6 (was 9), C133 14 (was 18), C136 18 (was 21); the largest fall C135 171 to
139.

New controls for the formerly dead cases, planted the same way (one build,
`BOF3X_FC3_CTL=n`). Each was also run against the old switch (a temporary
`BOF3X_DISTURB_OLD=1` toggle in the fuzz, `h % 12`, not committed): C161..C171
are refused under both - the harness's own case 4 (`Sprite_Current` to another
record) already exercised these re-reads -, so each has a twin C172..C182,
the same mutant only while `Sprite_Current` is the record read before the call,
which only the group's case can refuse: **every twin is 0 under the old switch**.
Read before the call means the value the function read or stored before the
callee named.

| # | Function | Mutant | Case | Rounds refused (of 3,000), old switch in brackets |
|---|---|---|---|--:|
| C161 | `FieldCore_ScriptMoveStep` | +9 not re-read after Field_JumpStart | 0 | 7 (7) |
| C162 | `FieldCore_TileD0Move` | +9 not re-read after Field_JumpStart | 0 | 5 (4) |
| C163 | `FieldCore_HopLaunch` | +9 not re-read after the animation and the sound | 0 | 130 (116) |
| C164 | `FieldCore_HopRise` | +5 not re-read after Field_LeaderStepTick | 3 | 51 (45) |
| C165 | `FieldCore_HopFall` | +5 not re-read after the ground and the animation | 3 | 35 (34) |
| C166 | `FieldCore_UpArrive` | +5 not re-read after the ground and the animation | 3 | 93 (84) |
| C167 | `FieldCore_TileD0Slope` | flat, facing 3: the z low word not re-read after the slope | 9 | 4 (4) |
| C168 | `FieldCore_TileD0Slope` | flat, facing 5: the x low word not re-read after the slope | 9 | 1 (1) |
| C169 | `FieldCore_TileD0Slope` | raised, facing 3: the z low word not re-read after two slopes | 9 | **0** (0); 10 of 30,000 (10) |
| C170 | `FieldCore_TileD0Slope` | raised, facing 5: the x low word not re-read after two slopes | 9 | 1 (1) |
| C171 | `FieldCore_ScriptMoveAlign` | x / z not re-read after the slope and the animation | 9 | 68 (57) |
| C172 | `FieldCore_ScriptMoveStep` | C161 with Sprite_Current kept | 0 | **0** (0); 8 of 30,000 (0) |
| C173 | `FieldCore_TileD0Move` | C162 with Sprite_Current kept | 0 | 1 (0) |
| C174 | `FieldCore_HopLaunch` | C163 with Sprite_Current kept | 0 | 15 (0) |
| C175 | `FieldCore_HopRise` | C164 with Sprite_Current kept | 3 | 6 (0) |
| C176 | `FieldCore_HopFall` | C165 with Sprite_Current kept | 3 | 1 (0) |
| C177 | `FieldCore_UpArrive` | C166 with Sprite_Current kept | 3 | 9 (0) |
| C178 | `FieldCore_TileD0Slope` | C167 with Sprite_Current kept | 9 | **0** (0); 1 of 30,000 (0) |
| C179 | `FieldCore_TileD0Slope` | C168 with Sprite_Current kept | 9 | **0** (0); 0 of 30,000 |
| C180 | `FieldCore_TileD0Slope` | C169 with Sprite_Current kept | 9 | **0** (0); 0 of 30,000 |
| C181 | `FieldCore_TileD0Slope` | C170 with Sprite_Current kept | 9 | **0** (0); 0 of 30,000 |
| C182 | `FieldCore_ScriptMoveAlign` | C171 with Sprite_Current kept | 9 | 11 (0) |

The 30,000-round runs used a temporary rounds toggle (`BOF3X_FC3_ROUNDS`, not
committed). What they say: cases 0, 3 and 9 are live tests now (C173..C177,
C182); the raised and the facing-5 paths of `FieldCore_TileD0Slope` (C169,
C179..C181) are thin, not blind - reaching a second slope read needs two steep
answers in a row and a disturbance between them. C169 and C178 are refused at
30,000; C179..C181 were not, and are not equivalent by construction (a low
word moved between the slope reads changes the answer), so they mark where the
fuzz's reach ends. The fuzz was not changed: raising the group's rounds would
move every count above, and the committed run already refuses C167, C168 and
C170 on the same function.

**2026-10-05, the round's end (debt 17): a louder slope.** The reach ended
because a second slope read needs a steep answer and a moved low word in the
same call, and the group's case 9 is one call in some hundreds.
`MapView_SlopeAt`'s stand-in (`SlopeEffect`) now, a third of the time and only
for the directions 3 and 5, turns `Sprite_Current`'s low coordinate word along
the direction pushed (5 the x word, 3 the z word) to 0 or from 0 and answers
0x41 with the sloped byte set - the cell `FieldCore_TileD0Slope` reads again
after its slopes, seen at the call. The other two thirds are as before. The
same script, in this worktree, 3,000 rounds (the committed count): the
baseline 0 mismatches (189,000 rounds); **160 of 160 refused**, the smallest
C46 2, C123 6, C126 14, C133 24 (each as thin as before or less so); and the
22 of the table above:

| # | Rounds refused (of 3,000), before -> with the louder slope |
|---|--:|
| C167 / C178 | 4 / 0 -> 119 / 116 |
| C168 / C179 | 1 / 0 -> 107 / 105 |
| C169 / C180 | 0 / 0 -> 46 / 43 |
| C170 / C181 | 1 / 0 -> 20 / 18 |
| C171 / C182 | 68 / 11 -> 124 / 68 |
| C161..C166 | unchanged (7, 5, 130, 51, 35, 93) |
| C172..C177 | 0, 1, 15, 6, 1, 9 (C172 still refused only at 30,000) |

C179..C181 are refused: not equivalent, as section 6 said, and the slope's
re-read is now a live test. A first try that moved the word without the steep
answer refused the raised paths only 7..11 times (the second slope needs two
steep answers in a row).
**Case 6 is noise for this group**: the sloped byte `0x903850` is read only
straight after `MapView_SlopeAt` (`FieldCore_TileD0Probe`, `Steep`), and that
stand-in's effect (`SlopeEffect`) writes the byte after the harness's
`Disturb()` has run (`scenario_harness.cpp`, `Stub`: `Disturb()` then
`s.effect`), so no re-read can see what case 6 wrote; no control was planted
for it.

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

**The rebinding.** Nothing to rebind: every raw reference to the 63 or their
tables in `src/` is left raw on purpose - the fuzz keys of other modules
(`move_groups.cpp`'s `case 0x518B20` and `kCallsE`; `object_kinds.cpp`'s
stub cases, `CallSite` rows and its fade-table patch expectations
`0x65F654` / `0x65F65C`), the harness's own (`scenario_harness.cpp`'s field
runs, `scenario_harness_fh.cpp`'s copy of `0x525CC0` - which is why
`FieldC3_Inject` runs after `ScenarioHarnessFh_Inject`), and comments
(`mode_states.cpp`, `mode_states_callees.h`, `area_w4f.cpp`). The callers
that are ours (`MoveScript_GroupE`, `Field_ObjectApproach` / `Avoid`) already
call by name, and the name now binds to ours. A note for the fold: a data
name with a `ctype` is a macro, so `bof3::addr::<table>` does not compile for
it - the fuzz names the tables by `Key(<name>)`.

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
