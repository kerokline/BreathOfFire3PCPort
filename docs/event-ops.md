# The leader's walk, its button tests, and the event script's last placements

**Status:** IN PROGRESS (2026-09-23) - 35 functions ours
(`src/game/event_ops.cpp`, shadow name `event_ops`): the 32 of the sixth
round's row V1 and three pointer-less helpers found inside it. Each read to
its last instruction by capstone (the PSX twin beside it where one is paired)
and fuzzed headless against a byte copy of Capcom's with every call re-aimed
at a recorder: **35 x 10,000 rounds, 559,326 stand-in calls, 0 mismatches**;
**86 negative controls, 84 refused by a count, 2 not refused - both changes
that change nothing** (section 9). The fuzz also caught a **clang miscompile**
(section 8). **Not yet through the live check** - the shop A/B runs centrally
after the merge (section 10).

Group V1 of the sixth round
([`takeover-queue-round6.md`](takeover-queue-round6.md)), "the event script's
shop and scene ops". Read, the row is three things, none of them a shop:

- **the event script's last three placement ops** - `5x`, `9x` and `Dx`, the
  handlers [`event-script.md`](event-script.md) section 1 left Capcom's;
- **the party leader's walk** - sub-state 1 of `Field_LeaderControl`
  ([`field-event.md`](field-event.md) section 1, the table `0x660954`), the
  step's target and pace, the six button tests `Field_LeaderStand` and the
  walk both make first, and what those call (the object and cell the leader
  faces, the area's passage list, talking to an object, the encounter test);
- **the chapter's and the area's step hooks** the step consults - two
  dispatchers through the chapter records at `0x662C80`, two switches over
  the area number into per-area handlers, and a return gate.

The shop is only where the owner's route happened to walk and talk. No
divergence, no defect found (section 5 lists the candidates and why none is
reachable).

## 1. The functions

Bytes are the true extent - last reachable instruction, jump table and
index table included - by linear disassembly of each. "Shop" is the call
count in the owner's shop route (`analysis/calltrace/recipe_shop/`); "-"
means the function is pointer-less and not in the trace list.

| PC | Name | PSX | Bytes | Shop | What |
|---|---|---|--:|--:|---|
| `0x57B310` | `EventOp_5x` | `FUN_801A7F60` | 0x1C9 | 21 | place `Sprite_Objects[count]`, 18 bytes |
| `0x57B500` | `EventOp_Dx` | `FUN_801A82F0` | 0x30 | 3 | `Field_ActiveMember` +0x86 or word +0x88 |
| `0x57B530` | `EventOp_9x` | `FUN_801A8338` | 0x24C | 7 | place an object of state 6, 13 bytes |
| `0x52DB90` | `Field_LeaderWalk` | `FUN_801B1560` | 0x4C4 | 253 | the leader's sub-state 1 |
| `0x52E060` | `Field_LeaderSetPace` | `FUN_801B1C80` | 0xA8 | 97 | walk (3) or run (4) |
| `0x52E140` | `Field_LeaderStepTick` | `FUN_801B1DF4` | 0x19 | 520 | the step, then the script tick when running |
| `0x52E160` | `Field_LeaderStepTarget` | `FUN_801B1E3C` | 0x3AC | 148 | where the step goes and what is there |
| `0x52E510` | `Field_LeaderStepCell` | - | 0x65 | - | the target's cell (found here) |
| `0x530030` | `Field_EncounterDue` | `FUN_801B4EA8` | 0x1B3 | 94 | an encounter due? |
| `0x5301F0` | `Field_LeaderIdleTest` | `FUN_801B6444` | 0xC6 | 120 | the idle fidget |
| `0x5302C0` | `Field_LeaderSwapTest` | `FUN_801B667C` | 0x5E | 318 | the swap button |
| `0x530320` | `Party_CanSwap` | - | 0x5D | - | whether the party may swap (found here) |
| `0x530380` | `Field_LeaderMenuTest` | `FUN_801B6750` | 0x54 | 318 | the menu button |
| `0x5303E0` | `Field_LeaderRequest4Test` | `FUN_801B687C` | 0x4B | 318 | button 0x903586 held: request 4 |
| `0x530430` | `Field_LeaderRequest9Test` | `FUN_801B6908` | 0x4E | 318 | the pad's 0x800: request 9 |
| `0x530480` | `Field_LeaderDirection` | `FUN_801B69AC` | 0xAE | 180 | the direction held |
| `0x530530` | `Field_EffectAhead` | - | 0x73 | - | the effect ahead (found here) |
| `0x530600` | `Area_PassageAhead` | `FUN_801B6C4C` | 0x1B2 | 3 | the passage entry faced |
| `0x530800` | `Field_LeaderCheckTest` | `FUN_801B6EB4` | 0x5C | 318 | button 0x90358C |
| `0x530860` | `Field_LeaderEffectTest` | `FUN_801B6F60` | 0x68 | 148 | an effect ahead while running |
| `0x5308D0` | `Field_LeaderPushCount` | `FUN_801B703C` | 0x4D | 48 | pushing on, sixteen frames |
| `0x530920` | `Field_LeaderTalkTest` | `FUN_801B7250` | 0x12E | 322 | the confirm button |
| `0x530A50` | `Field_FacingObject` | `FUN_801B5148` | 0x19B | 11 | the object faced |
| `0x530BF0` | `Field_ObjectAhead` | `FUN_801B53C8` | 0x98 | 11 | the object at a point, if reachable |
| `0x530C90` | `Field_CellAround` | `FUN_801B548C` | 0x28 | 24 | a cell next to the leader |
| `0x530CC0` | `Field_CellAroundNear` | `FUN_801B5D74` | 0x45E | 24 | the same for a sprite without +0x70 |
| `0x531660` | `Field_LeaderTalkTo` | `FUN_801B70D4` | 0x183 | 3 | talking to object n |
| `0x531950` | `Field_LeaderCellEvent` | `FUN_801B766C` | 0x198 | 120 | the cell stood on, on foot |
| `0x531DF0` | `Field_LeaderPushObjects` | `FUN_801BEF00` | 0x118 | 176 | the objects walked into |
| `0x539AC0` | `Scenario_NoHook` | - | 0x3 | 100 | a hook that declines |
| `0x56D700` | `Scenario_StepHook` | - | 0x42 | 148 | the chapter's step hook, else the area's |
| `0x56D750` | `Scenario_ArriveHook` | - | 0x42 | 100 | the chapter's arrive hook, else the area's |
| `0x56E050` | `Area_StepHook` | - | 0x3E6 | 148 | the gate, then one of 38 areas' handlers |
| `0x56E440` | `Area_ReturnGate` | `FUN_801A9A98` | 0x96 | 148 | the return gate |
| `0x56E4E0` | `Area_ArriveHook` | - | 0x182 | 100 | one of 8 areas' handlers |

The PSX twins are `GAME.EMI` section 0 (the sibling's
`analysis/ghidra/GAME_EMI0_80196800_decomp/`). `FUN_801B6F60` and
`FUN_801B703C` are paired here by position - `FUN_801B1560` calls them where
`Field_LeaderWalk` calls `0x530860` and `0x5308D0` - where
`analysis/pairs_propagated.json` had no pair. Names are `hypothesis` where
they say what a button means; the code only says which word it tests.

**The queue's extents** were wrong in three places: `0x52E160`'s 0x420 bytes
ran on into `Field_LeaderStepCell` `0x52E510` (the function ends with an
8-entry jump table at `0x52E4EC`); `0x539AC0` is **three bytes**
(`xor al, al / ret`), not 16 calling eleven - the eleven are `0x539AD0`'s;
the catalogue's "callers" `0x52E160` for `0x530030`, `0x530860` and `0x56D750`
are really `0x52E580`, a pointer-less function in no list (section 11). No
listed entry was a case label.

## 2. The addresses they touch

Named in `symbols.toml` and used by name: `Sprite_Current`, `Field_State`,
`Field_ActiveMember`, `Field_ScriptFlags` / `2`, `Field_InputFlags`,
`Field_InputHeld`, `Input_Pressed`, `Field_MenuButton`, `Field_MemberCount`,
`Field_EdgeBits`, `Field_Request`, `Field_MoveSpeeds`,
`Field_DirectionSteps`, `MoveScript_F3Divisor`, `MoveScript_FAWord`,
`Sprite_Objects`, `Sprite_ObjectsExtra`, `Effect_Objects`, `Game_AreaNumber`,
`Area_Descriptors`, `Cond_ByteFA` (the chapter), `Cond_ByteFD`. The rest are
constants in `src/game/event_ops_callees.h`, so that no other group of the
round could bind one twice:

| Address | What |
|---|---|
| `0x802D40` | ObjTrio member 0, the leader; `Field_State` points at it. `+0x128` the pace (3 walk, 4 run, 2 after the walk's answer 3), `+0x134` the zone counter, `+0x136` the idle frame count, `+0x137` a state byte the walk sets, `+0x138` flags, `+0x139` the object talked to, `+0x148` the actor record index, `+0x89` a kind byte the tests read |
| `0x903850` / `0x903852` | the scratch words (PSX `0x1F800000`): a placement's count and bank; a found cell's x and z (`Field_CellAroundNear`); the byte `0x903850` is also AreaMap_Slope's "sloped" flag and `0x903851` indexes the step sounds `0x660960` |
| `0x903854` / `0x903856` | the step target's cell (`Field_LeaderStepCell`) |
| `0x903858` / `0x90385C` | the step target, 16.16 |
| `0x90384C`, `0x903860`, `0x937F82`, `0x905B88` | an exit: z, x, the area word and the flags byte for `Field_ChangeArea` |
| `0x904148` | the return point: x, z, the area word at +8, a counter byte at +0xA |
| `0x904030` | the story flags (`Cond_Flags + 0xA0`); flag 0x77 arms the return gate |
| `0x9040CC` | the bits `EventOp_9x` tests by sprite +5 (PSX `0x80144FC0`) |
| `0x904062` | the two 3-byte party lists; `0x66972C` a member id's actor record, `0x669738` its slot byte |
| `0x903A70` | the actor records, `0xA4` apart |
| `0x903580`..`0x90358C` | the field's buttons: confirm, run, menu (`Field_MenuButton`), held (request 4), swap, check ([`input-script.md`](input-script.md) section 4 for which is which per save) |
| `0x903A5E` | the byte "the run button is held" is compared with |
| `0x905B82` | a byte that blocks the confirm test (`Field_LeaderFrame` counts it down) |
| `0x9039F3`, `0x9045FA` | set by the cell event: 0x37 / 0x1B; a 0..2 cycle |
| `0x929EE0` | a dword the encounter test wants at most 0xF0 when not on foot |
| `0x7E06E0` | the members' positions, x and z per member, 8 bytes apart |
| `0x6696DC` | the walk's unit per direction, x and z dwords |
| `0x66971C` | the cell step per direction, two signed bytes |
| `0x660A34`, `0x660A38` | three bytes a member's +2 may be for a swap; a byte per +0x89 choosing the facing pose |
| `0x662C80` | the chapter records by `Cond_ByteFA`: `+8` the step hook, `+0xC` the arrive hook (and `+4`, `+0x10` read elsewhere) |

## 3. What each does

**The placements** (`EventScript_Op`'s handlers for 5x, 9x and Dx). `5x` is
`EventOp_1x` with a different layout: `op[1]:op[2]` the bank, `op[3]` /
`op[4]` object words +0x98 / +0x9A, `op[5..8]` x and z, `op[9]` +1,
`op[0xA]` the speed index +0x84, `op[0xB]` +2, `op[0xC]` the flags byte,
`op[0xD]:op[0xE]` word +0x88, `op[0xF]` +0x83, `op[0x10]` / `op[0x11]`
+0x18 / +0x1C, and +0x5C = 2 where 1x takes it from `op[3]`. `9x` places an
object of state 6: x and z from `op[4..7]`, +5 `op[8]`, the dword +0x18 from
`op[9]:op[0xA]`, the flags `op[0xB]`, +0xB `op[0xC]`, +0x2A bit 4 of +7; then
by bit (+5) of the bits at `0x9040CC`: clear, `Sprite_SetAnimation(0)`; set
and `op[0xC]` bit 0, the object is switched off (+0 = 0) **and the count is
not advanced** - the next placement takes the same slot; set otherwise, a
sprite on a whole cell stamps 0x10 into the area map under it (`0x579F00`)
and `Sprite_SetAnimation(1)`. `Dx`: with `op[1]`, `Field_ActiveMember` +0x86
= `op[3]`; without, its word +0x88 = `op[2]:op[3]`. The PSX twins agree
store for store.

**The walk** (`Field_LeaderWalk`, sub-state 1). Nothing under bit 8 of
`Field_ScriptFlags` or bit 6 of `Field_ScriptFlags2`. Then the six button
tests in the order `Field_LeaderStand` makes them - talk (`0x530920`), swap,
menu, check, request 4, request 9 - any one that fires ends the frame. The
frame count +0xA counts down a frame at a time. With no direction held
(`Field_LeaderDirection`), standing: the pose, pace 3, +7 / +9 / +0xB and the
two script words cleared, sub-state 0. Else `Field_LeaderStepTarget`'s
answer:

| answer | the walk |
|--:|---|
| 0 | a push if anything is in the way (`Field_LeaderPushObjects`: sub-state 1, `Field_LeaderPushCount`); else the step - `+0xB` 0, the pace, V2's `0x5345E0` / `0x535F50`, on foot the zone counter under bits 12/13 (run down by +6, or ended by a non-zero cell ahead) or the encounter test (then the exit is set to (0x19, 0x19) and `+6` = 4), the walking pose (+8), +9 down, `Field_LeaderStepTick`, sub-state 2 |
| 1 | the effect test; failing it, stop (+9 = 0, sub-state 1) |
| 2 | state 2 sub-state 4, +3 by the direction, `Field_ScriptFlags2` bit 7 |
| 3 | pace 2, state 2 sub-state 5, `Field_ScriptFlags2` bit 8 |
| 5 | state 2 sub-state 3 |
| 6 | state 8, sub-state 0 on the diagonals 1 / 7 and 1 else |
| 0xFF | stop |
| 4, 7..0xFE | nothing |

**The step's target** (`Field_LeaderStepTarget`). +9, the frames the step
takes, is 0x20 on the diagonals 2 and 6 and 0x10 else, divided by
`Field_MoveSpeeds[pace]`; the target is the position plus the walk unit
(`0x6696DC`) times +9 times the speed - twice that for a sprite with +0x70.
The code from `0x526DB0` (group Z's) says what is there. Unless
`Field_Request` is 9, `Scenario_StepHook` may take the step: then 0xFF for a
sprite in state 2, else code 0, and the target is read back (the hook may
move it). By the code: 0 -> 1 (walk); 1 -> 0; 2 the height at the target
through `0x5725C0` (AreaMap_Slope's wrapper, group M's): with the slope byte
`0x903850` 0 and the height more than 0x80 from the sprite's, 1 (and
`Field_Request` 5 unless a link is at the target's cell); with it set and the
height above 0x40, 4 - for the leader, `MoveCmd_TestFB` at its cell chooses a
sound from `0x660960` and pose (+8 >> 1) + 0x10, else the pose +8, then
`Field_Request` 5, unless a link is there; otherwise 0, with
`Field_ScriptFlags2` bits 5-6 and `Field_Request` 5 unless a link is there;
3 the same height test - above 0x40 the facing is put back and 1, else
`MapView_GroundAt` at the target and 0; 4 / 5 1 over a link, else 2 / 3 with
`Field_ScriptFlags2` bits 6-7 / bits 6 and 8; 6 -> 5; 7 -> 6.

**The tests.** `Field_EncounterDue`: the zone the leader stands in
(`Area_ZoneAt`) must allow encounters (+4), the zone counter be set and at
most `Field_EdgeBits` - the steps taken, counted by `0x52E580` - or 7/10 of
them while walking, `ObjTrio +0x138` bit 0 clear, and no member with bit 6;
on foot the cell +9 steps on must be 0, and otherwise (bit 0 of
`Field_InputFlags` clear) `0x591F30(4, 0)` and every member's `0x535C50` must
answer. `Field_LeaderIdleTest`: past 0xF0 standing frames, one Rand in two, a
fidget (state 6) unless another member is in state 9. The swap, menu, check,
request-4 and request-9 tests are one condition each, table in
`symbols.toml`. `Field_LeaderTalkTest`: the object faced
(`Field_FacingObject`, tried twice) is talked to (`Field_LeaderTalkTo`); else
a 0x51 cell the chapter's `+0x10` hook (`0x56D7A0`) takes; else a 0x50 / 0x53
/ 0x55 / 0x52 cell and the passage faced (`Area_PassageAhead`) whose
`entry[2]` low nibble is the facing or 8 - state 7.
`Field_LeaderCellEvent`: on foot, confirm on 0xAE, or on any 0xAx cell with
its exit (`0x531AF0`); the pad's 0x100 cycles `0x9045FA`; the pad's 0x800
with an exit from `0x531820` saves the return point `0x904148`; the check
button on 0xA0 / 0xA1.

**The object and the cells faced.** `Field_FacingObject`: an even direction
tries the two odd ones beside it first, turning to one that answers; an odd
one looks (+0x70 + 2) steps ahead and, unless that cell is 0x30, one step.
`Field_ObjectAhead`: `Sprite_ObjectAt` at the point, refused for kind 8, a
word +0x88 of 0xFFFF, or more than 0x100 up or down. `Field_CellAroundNear`
(a sprite without +0x70): on a whole cell, the cell ahead's column then row -
an even direction turns to the diagonal between (1 / 5, 7 / 3), an odd one
must already be it - then, for an odd direction, the two cells 90 degrees
round (turning); between cells on x, the two cells of the row ahead (the
diagonals 7 / 3 look one row back and one on and turn to 1 / 5); between
cells on z, by columns (1 / 5 turning to 7 / 3). The found cell is left in
the scratch words for the caller. `Area_PassageAhead`: the area descriptor's
`+0x2C` list (`+0x31` the last index) of 8-byte runs - `entry[3]` cells from
(`entry[0]`, `entry[1]`), along z when `entry[2]` bit 7 - against the one or
two cells ahead. `Field_LeaderTalkTo(n)`: an object not of kind 9 gets bit 5
of its script context +0x80 and
`Field_ScriptFlags` bit 8; one of kind 9 turns the leader to the facing pose
and sets `Field_State +0x139` = n, state 0xB. (Bit 5 of +0x80 is the context's
"touched" bit `Field_ObjectIdle` `0x517F30` acts on,
[`object-kinds.md`](object-kinds.md).) `Field_LeaderPushObjects`:
every object in reach of the point ahead gets its word +0x9C counted up.

**The hooks.** `Scenario_StepHook(x, z)`: the chapter record's `+8`, its
answer sign-extended, and if 0 `Area_StepHook`; `Sprite_Current` put back.
`Scenario_ArriveHook` likewise with `+0xC` and `Area_ArriveHook` - called by
`0x52E580` when a step lands. `Area_StepHook`: `Area_ReturnGate` (1 if it
fires), then the area's handler. `Area_ReturnGate`: outside area 0xBB, with
`Field_InputFlags` exactly 0x40 and `Cond_ByteFD` 1, stepping onto a 0xA1 cell
at z 0x10.8 or more while story flag 0x77 is set: the flag cleared, the return
point's counter 0, `Field_ChangeArea` to the return point with flags 4.

## 4. Quirks kept

Each is said in a comment where it is implemented, and each was checked by a
negative control (section 9):

- **The re-reads.** The placements write the object's own bytes by the count
  read again after each call, and every function reads `Sprite_Current` /
  `Field_State` again after every call; `Field_EncounterDue` reads the member
  count again after each call; `Field_LeaderStepTarget` reads its target back
  after the hook.
- **Answers in al.** Every `unsigned char` function's answer is al; the rest
  of eax is whatever the last instruction left (`Field_LeaderStepTick`'s is
  `Field_State`'s low byte on the path that does not tick). The hooks'
  answers are sign-extended ints and are tested whole - `Field_LeaderTalkTest`
  tests `0x56D7A0`'s whole eax, not al.
- **`Field_ObjectAhead` compares the index signed** with 30: an answer of 0x80
  or more reads an object before `Sprite_Objects`. `Sprite_ObjectAt` answers
  0..33 or 0xFF, so nothing reaches it.
- **Whole bytes into tables of eight.** The direction indexes
  `Field_DirectionSteps`, the walk unit and the cell steps unmasked, and the
  pace indexes `Field_MoveSpeeds` - whose zeros (indices 0, 6, 7, 9..11, 13)
  are a divide fault in `Field_LeaderStepTarget`, kept as `idiv`.
- **`Field_LeaderStepTarget` passes the direction in Sprite_Current's
  register**: `0x5725C0`'s third argument is `Sprite_Current` with its low
  byte replaced (`mov eax, [0x937F88]` / `mov al, [eax+8]`), and AreaMap_Slope
  reads bytes 1..3 of it for a direction of 8..15 (area_slope.cpp). Ours
  passes the same dword. For a direction of 16 or more AreaMap_Slope reads
  its caller's stack, which no reimplementation reproduces; no direction the
  field sets is 8 or more.
- **`Area_PassageAhead` compares a cell as a byte but a run's end
  (`entry[0]` or `entry[1]` + k) as a whole int**: a run past 0xFF never
  matches there.
- **`Field_CellAroundNear`**, between cells on an axis: a match whose odd
  direction is not the diagonal wanted goes on to the next cell rather than
  failing.
- **`Field_LeaderTalkTo`'s index is not bounded above** - an n past 33 is an
  object past the four extras.
- **`Area_StepHook`'s case for area 0x4C** calls `0x40EB90` with nothing
  pushed; `0x40EB90` reads no argument (it tests the byte `0x9039F3` and
  clears three bytes there), so nothing differs.
- **`EventOp_9x`'s switched-off object keeps the count** - the PSX's too.
- **`Field_LeaderSetPace` compares the run option byte whole** with 0 / 1: a
  value of 2 or more always runs. The PSX the same.

## 5. The port against the PSX, and defect candidates

Read side by side, the PC differs from the PSX in one place: **the PC tests
`Area_PassageAhead`'s answer for null** in `Field_LeaderTalkTest`, where
`FUN_801B7250` reads `entry[2]` through whatever `FUN_801B6C4C` returned - a
null read on the PlayStation when a 0x50 / 0x52 / 0x53 / 0x55 cell has no
passage entry. The porting house's fix, kept.

Candidate defects, none reachable by what the field sets, so none is in
[`known-defects.md`](known-defects.md): the signed object index (section 4),
the divide by a zero speed (the pace is only ever set to 2, 3 or 4 - by
`Field_LeaderStart`, the walk and `Field_LeaderSetPace`), and AreaMap_Slope's
reads past its direction byte (directions are 0..7).

## 6. The per-area handlers

`Area_StepHook` and `Area_ArriveHook` are switches over the area number that
the compiler turned into a byte index table and a jump table inside each
function's body (`0x56E394` / `0x56E2F8`, `0x56E5DC` / `0x56E5B8`); ours is
the list of areas. Each handler takes the step's (x, z) and answers in al.

| `Area_StepHook` areas | handler |
|---|---|
| 0x24, 0x2A, 0x2E, 0x31, 0x3B, 0x4B | `0x404F80`, `0x406DE0`, `0x408EB0`, `0x409440`, `0x40B410`, `0x40E120` |
| 0x4C | `0x40EB90`, no arguments pushed |
| 0x61, 0x64, 0x69, 0x6A, 0x70, 0x74 | `0x413980`, `0x414330`, `0x4163B0`, `0x416770`, `0x418620`, `0x419DA0` |
| 0x87, 0x8C, 0x8F, 0x91, 0x92, 0x96 | `0x41E5E0`, `0x41F960`, `0x420A10`, `0x4215B0`, `0x422000`, `0x423910` |
| 0xAA, 0xAB, 0xAC | `0x427270`, `0x427A80`, `0x4281A0` |
| 0xAE | `Scenario_NoHook` `0x539AC0` |
| 0xAF..0xB9 (eleven) | `0x429DC0`, one handler for all eleven |
| 0xBF, 0xC0, 0xC1, 0xC5 | `0x42B9F0`, `0x42C000`, `0x42C700`, `0x42CE30` |

| `Area_ArriveHook` areas | handler |
|---|---|
| 0x28, 0x30, 0x6F, 0x94 | `0x405B00`, `0x409340`, `0x417ED0`, `0x422790` |
| 0xA7, 0xA9, 0xAB, 0xAD | `0x426470`, `0x426B20`, `0x427B50`, `0x4287E0` |

The handlers are the per-area scripts compiled into the exe (the
`0x401000`..`0x42F000` block); none is read or owned here.

## 7. The fuzz

`BOF3X_SHADOW=event_ops` (`src/game/event_ops_fuzz.cpp`;
`BOF3X_EVENT_OPS_ONLY=<name>` runs one function's). Every one of the 35
originals is byte-copied with **every** call re-aimed at a recording
stand-in - calls between this file's functions included, so each is tested
alone - and ours runs with the same stand-ins through `event_ops::g`. The
three jump tables are moved into their copies; the chapter table operand
`0x662C80` in the two hooks' copies is moved onto a 256-entry table of the
fuzz's own (entry i is one of four numbered records, so a wrong index shows);
the 46 per-area call sites are each re-aimed at a stand-in numbered by its
case, and area 0x4C's is a stand-in that takes no arguments.

A round: the state randomised - ObjTrio, the scratch words, both flag words,
the input, the story flags and party lists, the chapter bytes, and per test
the objects -4..33, the extras 0..7, the effect records, the actor records -
then the pointers set valid (`Sprite_Current` at a member or an object,
`Field_State` at a member), the fields each branch turns on seeded to their
boundaries (the pace from the speeds that do not divide by zero, directions
0..7 with 8, 9, 0xF, 0xFF and random bytes, fractions 0, the counters one
short of their limits, the buttons overlapping what is pressed, the areas the
hooks switch on and their neighbours, a passage list built to cover the cell
ahead), theirs, the same state again, ours; every byte of the state, the
stand-ins' log (a count, a hash of every entry with its arguments, the first
48 kept) and the answer compared. Stand-ins give back what their callee
leaves for the caller - a found cell's words, a moved step target, an exit,
`0x591F30`'s moved `Sprite_Current`, AreaMap_Slope's slope byte - and now and
then move `Sprite_Current`, `Field_State`, the count or the member count,
which the callers read again. An argument the original pushes with other bits
in its register is recorded at the width the callee reads.

**35 x 10,000 rounds, 559,326 stand-in calls, 0 mismatches**, about a second
headless. `BOF3X_SHADOW='*'` with every module: 0 mismatches, 618 functions
ours.

## 8. A clang miscompile the fuzz caught

The first run refused `Field_LeaderStepTarget` in 1,640 of 10,000 rounds:
`0x5725C0`'s third argument was `Sprite_Current[8]` alone where Capcom's is
`Sprite_Current` with its low byte replaced. The source said
`(Address(Sprite_Current) & 0xFFFFFF00u) | Sprite_Current[8]`, and **clang
22.1.8 at -O2 compiled it to `movzbl 8(%eax), %eax`** - the pointer's bits
gone. Reduced (scratch test, 2026-09-23):

    uint32_t h(uint32_t a) { return (a & 0xFFFFFF00u) | ((unsigned char *)a)[8]; }

prints `0000005A` for an `a` of `001D5058` at -O2 and `009A505A` at -O0. The
same with the byte from an unrelated pointer compiles correctly; the fold
needs the byte to be loaded through the value being masked. Ours now writes
that one expression as `movb 8(%1), %b0` in inline assembly
(`DirectionInPointer`), which is also the original's instruction. **Anything
else in the tree that merges a pointer's high bits with a byte read through
it is suspect** - the pattern is rare (it models a partial-register write),
and the fuzz is what sees it.

## 9. The negative controls

One planted bug each, built and run through that function's fuzz alone
(`BOF3X_EVENT_OPS_ONLY`), by a scratch runner. **84 of 86 refused**, each by a
count of mismatching rounds and none by a hang or a fault:

| # | control | refused |
|--:|---|--:|
| 0 | 5x: +0x18 from op[0x11] | 7,928 |
| 1 | 5x: the count test "above 30" | 595 |
| 2 | 5x: word +0x88's bytes swapped | 7,950 |
| 3 | 5x: the object's bytes at the count on entry | 2,877 |
| 4 | 5x: +0x5C is 0 | 7,966 |
| 5 | Dx: the choice by op[2] | 1,720 |
| 6 | 9x: the count advanced when switched off | 1,617 |
| 7 | 9x: the flag's byte bit >> 4 | 3,217 |
| 8 | 9x: the cell stamped when either word is 0 | 704 |
| 9 | 9x: +0x2A from bit 3 of +7 | 3,897 |
| 10 | Walk: the menu and check tests swapped | 7,395 |
| 11 | Walk: the frame count does not end the frame | 3,010 |
| 12 | Walk: answer 6's sub-state inverted | 118 |
| 13 | Walk: answer 0xFF leaves +0xB | 249 |
| 14 | Walk: bits 12/13 read as bit 13 only | 18 |
| 15 | Walk: answer 4 taken as 5 | 113 |
| 16 | Walk: the step pose without the +8 | 112 |
| 17 | StepTarget: the 0x20 count on directions 1 / 7 | 3,060 |
| 18 | StepTarget: a large sprite not doubled | 3,211 |
| 19 | StepTarget: the hook skipped for Field_Request 5 | 2,028 |
| 20 | StepTarget: the target not read back | 276 |
| 21 | StepTarget: slope >= 0x40 | 25 |
| 22 | StepTarget: the drop at 0x7F | 17 |
| 23 | StepTarget: not only the leader | 51 |
| 24 | StepTarget: case 3 keeps the new facing | 33 |
| 25 | StepTarget: code 7 answers 5 | 445 |
| 26 | StepTarget: the slope's direction without the pointer bits | 818 |
| 27 | StepCell: a zero unit counts as positive | 2,477 |
| 28 | Encounter: walking 7/8 | 37 |
| 29 | Encounter: pace 4 counts as walking | 45 |
| 30 | Encounter: the screen counter at 0xF0 | 154 |
| 31 | Encounter: Sprite_Current not put back | 492 |
| 32 | Encounter: the member count read once | 62 |
| 33 | Idle: the count past 0xEF | 183 |
| 34 | Idle: the leader counted as busy | 209 |
| 35 | Swap: one member enough | 350 |
| 36 | CanSwap: bits 0..1 only | 291 |
| 37 | CanSwap: two kinds of the three | 344 |
| 38 | Menu: sound 0x106 | 3,879 |
| 39 | Request4: Field_Request 5 | 5,317 |
| 40 | Request9: input flags 0x68 | 262 |
| 41 | Direction: 0x3000 is 7 | 739 |
| 42 | EffectAhead: kind 0x16 | 1,483 |
| 43 | EffectAhead: the sprite's height read once | 1,027 |
| 44 | Passage: along z by bit 6 | 1,932 |
| 45 | Passage: a run's end compared as a byte | 19 |
| 46 | Passage: one cell when between cells | 342 |
| 47 | Check: kind 9 allowed | 141 |
| 48 | EffectTest: the whole argument compared | 1,619 |
| 49 | PushCount: at 0xF | 2,064 |
| 50 | Talk: the facing not put back before the second try | 252 |
| 51 | Talk: a passage of way 7 always | 155 |
| 52 | Talk: the cell hook tested in al only | 106 |
| 53 | Facing: right before left | 4,508 |
| 54 | Facing: left as (d + 7) & 7 | **not refused** |
| 55 | Facing: the far cell 0x31 | 444 |
| 56 | ObjectAhead: the index unsigned | 448 |
| 57 | ObjectAhead: the height at 0x100 | 1,536 |
| 58 | CellAround: the pair swapped | 10,000 |
| 59 | CellNear: an even direction turns 5 / 1 | 164 |
| 60 | CellNear: a match on the wrong diagonal fails | 126 |
| 61 | CellNear: the diagonal row not z - 1 | 426 |
| 62 | CellNear: the first probe's row not stored | 791 |
| 63 | TalkTo: 30 is an object | 263 |
| 64 | TalkTo: +0x80 bit 4 | 6,313 |
| 65 | TalkTo: facing pose + 0x33 | 136 |
| 66 | CellEvent: the cycle to 3 | 120 |
| 67 | CellEvent: the return count not advanced | 85 |
| 68 | CellEvent: 0xAF for 0x37 | 224 |
| 69 | CellEvent: the exit read before the cell's exit call | 1,138 |
| 70 | PushObjects: the height at 0x100 | 28 |
| 71 | PushObjects: three extras | 5,000 |
| 72 | PushObjects: two steps under bit 13 | 3,526 |
| 73 | StepHook: the hook's answer zero-extended | 2,535 |
| 74 | StepHook: Sprite_Current not put back | 1,060 |
| 75 | ArriveHook: the step record | 10,000 |
| 76 | AreaStep: 0xBA for 0xB9 | 110 |
| 77 | AreaStep: the gate's answer ignored | 3,298 |
| 78 | AreaStep: case 6 given its arguments | **not refused** |
| 79 | ReturnGate: z at 0x108000 refused | 90 |
| 80 | ReturnGate: the row not rounded | 1,577 |
| 81 | ArriveHook: 0xAE for 0xAD | 513 |
| 82 | NoHook: answers 1 | 10,000 |
| 83 | StepTick: the tick from pace 5 | 3,059 |
| 84 | SetPace: record bit 6 | 1,953 |
| 85 | SetPace: the run option compared as a bit | 734 |

The two not refused are **changes that change nothing**: `(d - 1) & 7` and
`(d + 7) & 7` are one value for every byte; and area 0x4C's handler reads no
argument, so pushing two or none is the same to it - the stand-in, like
`0x40EB90`, reads none. Both are kept in the list as the record of that.

Six controls were **first** not refused, or refused by fewer than ten rounds,
and each showed a blind spot, now closed: the member count read once (32) and
the effect records (43) needed stand-ins and seeds that change what the
function reads again or tests (a member-count change in `0x535C50`'s
stand-in; effect records of kind 0x17); the swap kinds (37) needed members
in state 2 with +2 among 3 / 6 / 8; the object index (56) and the 0x100 height
(57) needed the objects the signed and unsigned readings reach, seeded within
0x100 of the sprite - five extra regions; the cycle (66) needed the byte at 2;
the drop at 0x80 (22, 1 round) and the cell hook's al (52, 4 rounds) needed
heights exactly 0x80 away and hook answers with al 0.

## 10. What the shop route reaches, and what nothing reached

The route (`tools/recipes/shop.txt`) reaches every function in the row
(section 1's counts), so the central shop A/B (`analysis/validate_shop.sh`)
covers all 32 listed; the three found here run inside them. It reaches
`EventOp_5x` 21 times, `9x` 7, `Dx` 3 (the shop's areas' placement scripts),
the walk and its tests every frame the leader is controlled,
`Field_LeaderTalkTo` 3 times, the passage search 3 times, and the chapter
and area hooks on each step. Which branches inside each ran is not measured;
the call counts are all the trace gives.

**Not established as reached - fuzz only until a check shows otherwise:**

- `Field_CellAroundNear`'s paths between cells, and every path for a sprite
  with +0x70 (`0x531120`, Capcom's);
- `Field_LeaderStepTarget`'s codes 2 to 7, and `Field_LeaderWalk`'s answers
  2, 3, 5 and 6 with them - which codes `0x526DB0` gave on the route is not
  measured;
- the encounter test answering yes, and the zone counter branch (bits 12 /
  13 of `Field_ScriptFlags2`);
- `Field_LeaderIdleTest`'s fidget (0xF0 standing frames and a Rand);
- `Area_ReturnGate` firing, and which area handlers ran - the handlers are
  not in the trace list;
- `EventOp_9x`'s switched-off path (a set bit and `op[0xC]` bit 0).

## 11. Found and not taken

Pointer-less or unlisted functions this cluster calls, left Capcom's (each is
a stand-in in the fuzz):

| PC | What | Why not |
|---|---|---|
| `0x52E110` | the leader's sub-state 2 (table `0x660954` entry 2): +0x137 = 1, +9 down, then `0x52E140` or, at 0, a tail jump to `0x52E580` | small, but its tail is `0x52E580` |
| `0x52E580` | the step's landing: V2's object tests (`0x535270`, `0x5350C0`, `0x534A00`, `0x535120`, `0x535240`, `0x534920`, `0x534990`, `0x535390`, `0x535C50`, `0x534F10`), cells 0xAF / 0xC0, the map edge, `Field_EdgeBits` counted, the encounter test, `Scenario_ArriveHook`, `Field_LeaderWalk` again while a direction is held - 0x445 bytes | entangled with group V2's functions; the natural next takeover once V1 and V2 are merged |
| `0x5317F0` | the encounter's area word `0x937F82` from the table `0x660A90` | tiny; not called by anything listed but the walk |
| `0x531820`, `0x531920`, `0x531AF0` | `Field_LeaderCellEvent`'s exits: the gateway tables `0x660AB8` / `0x660B08`, the cell list `0x660B10`, the cell's exit through `0x462AC0` | three more functions and `0x462AC0` unread |
| `0x531120`, `0x531540` | `Field_CellAround` for a sprite with +0x70 (0x420 and 0x116 bytes) | `Field_CellAroundNear`'s twin for the large sprite, not reached by the route |
| `0x56D7A0`, `0x56E670` | the chapter's `+0x10` cell hook and the per-area table `0x662F28` behind it | a third hook chain of the same shape |
| `0x579F00` | AreaMap's byte store (`EventOp_9x`'s stamp) | an area-map writer, not this cluster's |

## 12. Names

`symbols.toml`'s 2026-09-23 group V1 block: 35 functions with `impl`, three
moved into the block (`EventOp_5x`, `Dx`, `9x` existed as hypotheses). No
data entries - the addresses of section 2 are constants in
`event_ops_callees.h`. Calls into other groups' functions of the round (V2's
`0x5345E0`, `0x535F50`, `0x535C50`, `0x536670`; M's `0x5725C0` and
`MoveCmd_TestFB`; Z's `0x526DB0`) are raw addresses there.
