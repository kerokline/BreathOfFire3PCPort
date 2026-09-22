# The field objects' kind handlers

**Status:** IN PROGRESS (2026-09-22) - twenty functions ours
(`src/game/object_kinds.cpp`): every entry of the handler table `0x65F5F8`
that was not already, the pace table `0x65F5DC` behind them, the field
object loops' three other per-object calls, the talk, and three helpers only
these use. Each is fuzzed at start-up against a copy of Capcom's with every
callee a recording stand-in (`BOF3X_SHADOW=object_kinds`), with negative
controls (section 6). **Not yet through the live batch check** - that runs
centrally after the merge; ten of the twenty are never reached by the
attract cycle (section 7).

Group C of the parallel takeover of the field's frame (2026-09-22). The
loops that call all of this - `0x517490` (the 30 field objects) and
`0x57B7B0` (the party) - are group A's; [`movement-script.md`](movement-script.md)
has the object and script-context layout this builds on (object `+0x80` is
the script context).

## 1. The dispatch

For each live object the loop `0x517490` sets `Sprite_Current` and
`Field_ActiveMember` to it, then (read from its disassembly, 2026-09-22):

| condition (object bytes) | call |
|---|---|
| no timed move (`+9` = 0), context bits 5 and 4 (`+0x80` & `0x30`) | `MoveScript_SetTurnTarget(object + 0x80)` - the talk |
| no timed move, bit 5 without 4 | `Field_ObjectIdle(object)` |
| context bit 0 | `Field_ObjectLinked()` - no argument |
| the push counter word `+0x9C` above `0x1E`, no timed move | `Field_ObjectIdleLong(object)` |
| otherwise | `[0x65F5F8 + object[1] * 4](object)` - the kind handler |

`object[1]` is the sprite's **pose**: the scripted kind 4 and the wander
kinds switch it (to 5 for a wait, 8 / 9 for a fade, 10 for a turn), keeping
the pose to return to in `[3]`. So "kind" is the table's word; most entries
are states an object passes through rather than what it is. The table has no
bound; `MoveCmd_OpE7` (op E7) calls it too, with `Field_ActiveMember`.

| entry | PC | name | PSX twin | what it does |
|---|---|---|---|---|
| 0 | `0x517640` | `Field_ObjectWander` | `FUN_801A1AEC` | a random walk |
| 1 | `0x517790` | `Field_ObjectApproach` | `FUN_801A1D04` | toward the leader, when near |
| 2 | `0x517900` | `Field_ObjectAvoid` | `FUN_801A1F2C` | away from the leader, when near |
| 3 | `0x517A70` | `Field_ObjectWanderHome` | `FUN_801A2154` | a random walk kept inside a home box |
| 4 | `0x517BF0` | `Field_ObjectUpdate` | `FUN_801A238C` | the movement script (field_objects.cpp) |
| 5 | `0x518AC0` | `Field_ObjectWait` | `0x801A3724` | a wait counting down `+0x81`, then the pose back |
| 6 | `0x517FE0` | `Field_ObjectStill` | `0x801A2A2C` | context bit 4, unless the sprite's type is 8 |
| 7 | `0x5192A0` | `Field_ObjectFollow` | `ov_entry_801A4418` | attached to another object (field_objects.cpp) |
| 8 | `0x5193B0` | `Field_ObjectFadeOut` | `0x801A459C` | tint bytes 31 down to 0 (read as a fade out; unobserved) |
| 9 | `0x5194E0` | `Field_ObjectFadeIn` | `0x801A47DC` | tint bytes up to 31 (read as a fade in) |
| 10 | `0x5196E0` | `Field_ObjectTurn` | `FUN_801A4B84` | an eighth turn every four frames, then a step |

The pace table `0x65F5DC`, by the sprite's byte `[2]`, entries 0..3 all
`0x5187C0` (`Field_Pace0`), 4..6 `Field_Pace4` / `5` / `6`. The wander kinds
call it twice a frame: `(object, 0, 1)` - a pace 4..6 counts a step up on
a pass where a bit of the counter byte `0x905B80` changed since the last
pass (`0x8034E8`, which the loop sets) - and `(object, speed, 2)` - a timed
move's frames, `0x20 / speed`, in `[9]` and half of them in `[0xA]`.

PSX twins are from `analysis/pairs_propagated.json`, all in `GAME.EMI`'s
overlay section 0: `gap44` pairs for the table's entries, `call` /
`call-anchored` for the rest. Twelve PSX bodies were read against ours in
the sibling's Ghidra output and match branch for branch: `FUN_801A1AEC`,
`801A1D04`, `801A2154`, `801A27A8`, `801A28D0`, `801A2A60`, `801A3264`,
`801A37CC`, `801A3A0C`, `801A3C18`, `801A4A10`, `801A4B84`. The Ghidra
output has no function at `0x801A3724`, `0x801A2A2C`, `0x801A32D4`,
`0x801A33A4`, `0x801A3474`, `0x801A459C` or `0x801A47DC`, and
`FUN_801A1F2C` (Avoid) was not read: those pairs are the tier's word only.

## 2. The wander kinds, 0 to 3

One shape, read from all four (`0x517640` 335 bytes, `0x517790` 362,
`0x517900` 362, `0x517A70` 371):

1. keep the entry direction `[8]`; pace `(object, 0, 1)`;
2. a timed move running (`[9]`): only `Field_ObjectMotion`;
3. no steps left (`+0x87`): with a speed index (`+0x84`) and a pace
   `(object, Field_MoveSpeeds[+0x84], 2)` that returns non-zero, a new
   direction - `Field_ObjectRandomTurn` (0, 3), `Field_ObjectApproachDirection`
   (1), `Field_ObjectAvoidDirection` (2) - and, if a timed move began,
   `(Rand & 3) + 3` steps; steps left: the pace's second call and one step
   fewer, the direction for 1 / 2 from `Field_ObjectBestDirection(object, 1 / 0)`;
4. 0 and 3: `Field_ObjectOpenDirection`, quarter turns off a blocked
   direction; 3 also refuses a step out of the home box (`Field_ObjectInHome`);
5. a direction with a timed move: pose 10 with it as the target `+0x85`,
   `Field_ObjectTurn` at once, and - if the turn is not done - return;
   otherwise the move is cancelled and the entry direction put back; then
   `Field_ObjectSettle` and `Field_ObjectMotion`.

Two differences between them are the 2001 code's, on both platforms:
**1 and 2 return before the settle and the motion when there is no speed**
(0 and 3 go on), and **0 and 3's steps-left branch calls the pace with the
speed unguarded** - with speed index 0, `Field_Pace0` divides by zero
(section 5).

## 3. The rest of the table, and the loops' other calls

- **5, `Field_ObjectWait`** (49 bytes): `Sprite_SetPoseWait` sets pose 5 and
  `+0x81`; this counts it down with context bit 4, then puts `[1]` back from
  `[3]`. 742 calls a cycle.
- **6, `Field_ObjectStill`** (23 bytes): context bit 4 unless the sprite's
  type `[6]` is 8. **The hottest kind: 12,468 calls a cycle**, 3,342 of them
  from the party loop.
- **8 / 9, the fades**: a jump table each on the sprite's sub-state `[4]`.
  Case 0 opens a tint record with `Sprite_SetTint` (31, 31, 31 or 0, 0, 0,
  alpha 1), its index to `Field_ActiveMember +0x9F`; case 1 steps the
  record's three bytes one a frame towards 0 or 31 and, when their signed
  sum reaches 0 or 93, releases it (`Tint_Release`), clears the context bit
  that chose the pose (1 or 2; `Field_ObjectUpdate` turns context bits 1 and 2 -
  masks 2 and 4 - into poses 8 and 9) and puts the pose back. The fade out also clears
  sprite bit 0.6.
- **10, `Field_ObjectTurn`** (259 bytes): with a countdown `[0xA]` running,
  one less; facing the target, the pose back and one step
  (`MoveCmd_Move(context, [8])`) unless `Field_ObjectBlockedAhead`;
  otherwise an eighth turn the short way - a half turn goes -1 - and 3
  frames. The PSX `FUN_801A4B84` the same.
- **`Field_ObjectIdle`** (`0x517F30`, 167 bytes): an object touched (context
  bit 5) that does not talk. With `+0x86` set it runs `Field_ObjectTrigger`
  (never in the attract cycle). A sprite of type 5 then starts what the
  byte writes suggest is an encounter: `0x929F00` / `0x929F01` = 0,
  `Game_Mode` 7, `0x929F0C` = the object's index; any other turns to face
  `+0x85` (unless the flag `0x8034E1` bit 6) and clears bit 8 of
  `Field_ScriptFlags`. The mode 7 reading is a guess from the stores alone.
- **`Field_ObjectLinked`** (`0x519600`, 111 bytes, no argument): an object
  with context bit 0 hands itself to `Area_ObjectHandler(byte 0x802DC9)` -
  a dispatcher into the area descriptor's `+0x3C` table by the object's
  `+0xA0` - unless it has none (`+0xA0 & 0x7F` = `0x7F`) and its type does
  not force the call, which clears bit 0.
- **`Field_ObjectIdleLong`** (`0x518B40`, 346 bytes): an object pushed for
  more than 30 frames (the loop's test of `+0x9C`, which `Field_ObjectMotion`
  zeroes on arrival) steps aside: four candidate directions from the row of
  `0x65F624` for the leader's facing (`0x802D48`), the first open one (and,
  for pose 3, inside the home box). It is "idle long" by its provisional
  name only.
- **`MoveScript_SetTurnTarget`** (`0x517E90`, 149 bytes) is **the object's
  talk**, not a turn: the word at context `+8` names an event script (bit 15,
  through the area descriptor's `+4` table and `EventScript_Run`), a system
  message (bit 13) or a script message, and sets `Field_Request` 2. It is
  called by ops E0 E1 E3 (all 18 calls of the attract cycle) and by the loops
  for bits 5 and 4. It returns 1, or 0 for the word `0xFFFF` - the loop tests
  it, so its `symbols.toml` type is now `unsigned char` (`move_groups.cpp`'s
  pointer follows).

## 4. The helpers

- `Field_ObjectOpenDirection` `0x518CA0` (106 bytes): up to four quarter
  turns while `Field_ObjectBlockedAhead`; returns the direction or `0xFF`.
- `Field_ObjectRandomTurn` `0x518DD0` (68 bytes): `(Rand & 6) | 1`, one of
  the four odd directions.
- `Field_ObjectInHome` `0x518000` (113 bytes): the next step's position
  (high words of the 16.16 `+0x34` / `+0x38` plus the step table `0x6697B0`)
  within `+0x98` / `+0x9A` of the home `+0x8E` / `+0x92`. Reads everything
  from its argument, never `Sprite_Current`.
- The pace entries, section 1.

Left original, behind stand-ins, each with a typed `symbols.toml` entry
(`# group C callees`): `Field_ObjectBlockedAhead` `0x518080` (called from
outside the group, `0x40C2B0` and `0x41F790`), `Field_ObjectApproachDirection`
`0x518E20`, `Field_ObjectAvoidDirection` `0x518ED0` and
`Field_ObjectBestDirection` `0x518F80` (unreached, 163 / 165 / 284 bytes),
`Tint_Release` `0x454D60`, `Area_ObjectHandler` `0x537540`,
`Field_ObjectTrigger` `0x56D6B0`, `EventScript_Run` `0x5797C0`. Also
behind stand-ins: `Rand`, `MoveCmd_Move`, `Sprite_SetTint`,
`Msg_OpenScript` / `Msg_OpenSystem`, and `Field_ObjectSettle` /
`Field_ObjectMotion` (ours, field_objects.cpp).

## 5. Quirks kept, and defects

Kept as the original has them, each with its comment in the source; the
negative control that shows it matters is in section 6.

- `Field_ObjectOpenDirection` marks a direction change against the entry
  direction `& 7` but compares it with the unmasked byte, so a sprite that
  was moving (bit 3) always counts as turned. The PSX compares the same way.
- `Field_ObjectRandomTurn` compares `[8] & 7`, returns the unmasked `[8]`;
  pushes `0x80` for `Rand`, which takes nothing (the PSX passes it too).
- `Field_ObjectTurn` takes an absolute value of `(target - facing) & 7`,
  never negative - kept as the plain difference (no control: nothing can
  tell them apart); it passes the whole direction byte, bit 3 and all, to
  `MoveCmd_Move`.
- `Field_ObjectIdleLong` puts the entry direction back masked on one exit
  and unmasked on another; tests `n < 4` a second time where it cannot fail.
- `Field_Pace5` caps at `0xFA` before adding 2, so `0xF9` becomes `0xFB`.
- The talk reads its word again after the event script and writes back the
  one it found on entry.
- `Field_ObjectIdle` divides the object's offset from `Sprite_Objects` by
  `0xA4` signed.
- Byte arguments arrive with noise above them (`push eax` of a register
  whose low byte was loaded); the original reads bytes, and so does ours:
  the pace entries take `unsigned` and mask.

Defects of the 2001 code, **written down, not fixed** (a fix is the owner's
call and a `DIVERGENCE.md` entry):

- **Speed 0 with steps left divides by zero** in `Field_ObjectWander` /
  `Field_ObjectWanderHome` (the pace's second call is unguarded) - through
  `Field_Pace0`'s `idiv`; the PSX's `trap(0x1C00)` is the same fault. Latent:
  it needs a wander object with `+0x84` = 0 and `+0x87` set. The same family
  as `Field_MoveSpeeds` entry 0 in `Field_ObjectUpdate`.
- **The fades' jump tables have no bound.** `Field_ObjectFadeOut` with
  sub-state 2 or 3 runs the fade-in's cases (the tables are adjacent);
  from 4, and `Field_ObjectFadeIn` from 2, the original jumps into the cases
  of another function (`0x65F664`'s, at `0x5198xx`) in the wrong frame. Ours
  runs 2 and 3 as the original does and **stops with a Fatal** past them -
  the one place ours does not reproduce the original, because what it
  reproduces is a jump into another function's body. Not reachable while
  `[4]` is only ever set to 0 and 1 by these two; unmeasured in game.
- The handler table `0x65F5F8` and the pace table `0x65F5DC` have no bound
  either; ours reads them where the original does, so an index past them
  behaves the same.

## 6. The fuzz, and what it does not reach

`BOF3X_SHADOW=object_kinds` copies the originals (nineteen copies; the two
fades are one block, `0x5193B0..0x5195F9`, with their two jump tables
moved into it), re-aims every relative call at a recorder and every
`call [reg*4 + 0x65F5DC]` at a table of 256 recording pace entries (eight
distinct recorders, so an index error shows). 30,000 rounds, 1,500 per
function; one round seeds a random object, sprite and member - the sprite
the object half the time, as in the loops - every global the twenty touch
(`Sprite_Objects` through `MoveScript_TintRecords`, `ObjTrio`,
`Field_Request`, `Game_Mode`, the counters, the talk word, the encounter
bytes), then the boundaries: timed move 0 or not, steps 0 / 1 / 2 and
`0xF8..0xFB`, speed indexes 0..5, poses 3 / 4 / 6 / 10, types 1 / 3 / 4 /
5 / 8, the turn's target at 0 and 3..5 eighths off, tint bytes around 0,
31 and the sign, the home box's edges on both axes, the talk word's four
forms, and `Field_ObjectIdle`'s object inside and below `Sprite_Objects`.
The recorders disturb what the real callees write (steps, frames, pose,
direction, countdown, flags, the talk word, `Field_Request`) and, rarely,
which object and member are current - so a cached `Sprite_Current` would
show. Compared: the three buffers, the regions, the result byte, and the
recorders' log.

Result, 2026-09-22, `build/bof3x.log` (alone and with `BOF3X_SHADOW=*`,
beside every other module's self-test, 178 functions ours):

```
shadow      object_kinds self-test: 30000 rounds (1500 per function, 20 functions), 34540 calls to the stand-ins, 0 MISMATCHES; ...
```

Negative controls - each planted in ours, built, run under
`BOF3X_SELFTEST_ONLY=1`, then reverted. **All 41 refused (exit 3)**, each by a
count of mismatches in the function it touched, none by a hang:

| control | refused in | mismatches |
|---|---|---|
| after a finished turn, the entry direction not put back | Wander | 44 |
| WanderHome without `Field_ObjectInHome` | WanderHome | 167 |
| Wander / WanderHome: `Sprite_Current` cached across the turn call | Approach | 11 |
| Wander: the steps-left pace call guarded by a speed (the defect "fixed") | WanderHome | 159 |
| Wander: steps `(Rand & 3) + 2` | Wander | 31 |
| Wander: steps counted down from the value before the pace call | Wander | 112 |
| Approach / Avoid: no early return without speed | Approach | 70 |
| Avoid: `Field_ObjectBestDirection(object, 1)` | Avoid | 612 |
| Approach / Avoid: the direction unmasked into `+0x85` | Avoid | 276 |
| Wait: context bit 5 for 4 | Wait | 592 |
| Wait: at 0 the pose left | Wait | 689 |
| Still: type 9 exempt for 8 | Still | 43 |
| fades: the tint sum over unsigned bytes | FadeOut | 5 |
| FadeIn: climbs below `0x1F` unsigned | FadeIn | 878 |
| FadeOut: sub-state 2 runs its own start, not FadeIn's | FadeOut | 386 |
| FadeOut: clears context bit 2 for 1 | FadeOut | 6 |
| FadeOut: sprite bit 0.6 left | FadeOut | 195 |
| Turn: a half turn goes +1 (`<= 4`) | Turn | 40 |
| Turn: the step's direction masked `& 7` | Turn | 138 |
| Turn: the countdown after a step `[9]`, not `[9] / 2` | Turn | 96 |
| Linked: type 3 not in the list | Linked | 7 |
| Linked with bit 0.6: the member's `+0xA0` not tested | Linked | 172 |
| IdleLong: the direction put back unmasked when none is open | IdleLong | 375 |
| IdleLong: only `[1]` == 3 asks the home box | IdleLong | 79 |
| IdleLong: the pose-4 wait exit masks the direction | IdleLong | 9 (0 at first - see below) |
| Idle: the object's index divided unsigned | Idle | 5 |
| Idle: `Field_Request` not tested again after the trigger | Idle | 27 |
| Idle: `Game_Mode` 6 | Idle | 66 |
| Idle: the type-5 branch goes on | Idle | 50 |
| talk: the word not read again after the event script | SetTurnTarget | 20 |
| talk: the word not put back | SetTurnTarget | 20 |
| talk: `Msg_OpenSystem` given the word unmasked | SetTurnTarget | 376 |
| talk: returns 2 | SetTurnTarget | 1,119 |
| Pace0: `[0xA]` = `[9]` | Pace0 | 657 |
| Pace4..6: the cap `<= 0xFA` | Pace5 | 85 |
| Pace5: adds 1 | Pace5 | 183 |
| Pace6: on counter bit 0 | Pace6 | 247 |
| OpenDirection: compares the masked direction (the quirk removed) | OpenDirection | 196 |
| OpenDirection: gives up after three turns | OpenDirection | 183 |
| RandomTurn: compares the unmasked direction | RandomTurn | 125 |
| InHome: the x half-width exclusive | InHome | 205 |

The one control that was **not refused at first**: `Field_ObjectIdleLong`'s
exit on `[3]` == 4 during a wait. It returned on `[3]` == 4 at entry, so the
exit is reachable only if a callee changes `[3]` in between - and no stand-in
did. The stand-ins now disturb `[3]` too, and the control is refused (9).
Whether the real callees (`Field_Pace0`, `Field_ObjectBlockedAhead`,
`Field_ObjectInHome`) ever change `[3]` is unread; if they never do, the exit
is dead code in both builds (the PSX has it too).

Not planted, because nothing could tell the two apart: `(Rand & 6) | 1`
against `(Rand & 7) | 1` (the same values).

Not reached by the fuzz: the callees' own behaviour (stand-ins throughout);
`Field_ObjectFadeOut` / `FadeIn` past their tables (Fatal by design); speed
0 where the pace divides (the fault is the original's too); the real
`0x65F5DC` / `0x65F5F8` contents past their entries.

## 7. Reached by the attract cycle, and what would test the rest

Call counts from `analysis/calltrace/hidden_b` (16,128 frames; the table's
entries are reached through pointers and show only there), `all_b` in
brackets:

| function | calls | |
|---|---|---|
| `Field_ObjectStill` | 12,468 | 9,126 object loop, 3,342 party loop |
| `Field_Pace0` | 2,872 (2,336) | all from `Field_ObjectWanderHome` |
| `Field_ObjectWanderHome` | 2,381 | |
| `Field_ObjectIdle` | 2,181 (1,593) | its trigger call never |
| `Field_ObjectTurn` | 905 (729) | 640 table, 265 from WanderHome; its step never |
| `Field_ObjectWait` | 742 | 637 / 105 |
| `Field_ObjectOpenDirection` | 491 (387) | |
| `Field_ObjectInHome` | 491 (387) | |
| `Field_ObjectRandomTurn` | 94 (74) | |
| `MoveScript_SetTurnTarget` | 18 (14) | all from op E0 / E1 / E3; the loops' call never |

**Never reached** (the batch check's attract run cannot test them): the
wander 0, approach 1 and avoid 2 kinds, both fades, `Field_ObjectLinked`,
`Field_ObjectIdleLong`, and `Field_Pace4` / `5` / `6`. What would: a field
where an NPC walks toward or away from the party (1 / 2), a script fade
(context bits 2 / 4 - a character appearing or vanishing), an object with
an area handler (context bit 0), and pushing against an NPC for a second
(`+0x9C` above 30) - the new-game capture and a save's field
(`tools/recipes/`) are the candidates; which of them do is unmeasured.

## 8. Names

The three provisional names registered before the takeover are kept, and so
is `MoveScript_SetTurnTarget`; each reads wrong now that the body is read.
Proposed, not applied (a rename touches every file that calls them):

| now | proposed | why |
|---|---|---|
| `MoveScript_SetTurnTarget` | `Field_ObjectTalk` | it opens the object's message or event script and sets `Field_Request` 2; nothing turns (the sibling's `Script_ShowMessage` says the same) |
| `Field_ObjectIdle` | `Field_ObjectTouched` | context bit 5 without 4: the object was touched and does not talk |
| `Field_ObjectIdleLong` | `Field_ObjectStepAside` | run after 30 frames of pushing (`+0x9C`), it moves the object out of the leader's way |
| `Field_ObjectLinked` | `Field_ObjectAreaHandler` | it hands the object to the area's own handler table |

The twenty new names are `hypothesis`-grade descriptions of what the bodies
do; the kinds' own meaning in the game (which NPCs wander, which approach)
is not observed.
