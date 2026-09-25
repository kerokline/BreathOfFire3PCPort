# The enemy AI script ops

**Status:** IN PROGRESS (2026-09-25). Twenty-two functions are ours
(`src/game/enemy_ai_ops.cpp`). Each is fuzzed headless against a copy of
Capcom's with every call re-aimed at a recorder: 22,000 rounds, 0
mismatches. 112 negative controls were planted, and all 112 were refused.
Not yet through a live check: the coordinator runs the combat A/B after the
merge.

This is group CF of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)). Every function is a
*faithful* replacement, so no `DIVERGENCE.md` entry is owed. No defect of the
original was found that a player can reach; the latent ones are in section 5
(known-defects.md D60, D64, D59).

The name "script ops" is the queue's. What the code is: the handlers of an
enemy object's **state 0**. `BattleEnemy_RunAll` (ours, `battle_flow.cpp`)
calls the entry of the state table `0x64B084` by the object's byte `+0x100`;
state 0's entry is `EnemyOp_StepDispatch`, which jumps through a second
table by the step byte `+1`. An op's own sub-steps `+2` and `+3` pick from
further tables. None of these functions is ever a `call rel32` target: every
one is reached through a table (`pe_xref.py`, 2026-09-25).

Read against the sibling's `docs/BATTLE_RAM.md` and
[`battle_flow.md`](battle_flow.md):

- **Enemy objects:** PC `0x93B960` + `0x128` n, PSX `0x801EB5A0` + `0x118` n
  (the working record, PSX `0x801EB620`, is `+0x80` of each).
- **Sprite_Current** (`0x937F88`) and **the current enemy** (`0x939AD8`,
  PSX `0x801EB458`) both hold the object being run while `RunAll`
  dispatches. The ops read one or the other at each access, and ours read
  the same one at the same point.
- **The action being resolved:** the result record pointer `0x904B60`
  (`+4` HP delta, `+6` AP delta, `+8` flags), its target `0x904B54` - the
  sibling's `0x8014639C` / `0x80146390` ("Commands, Auto, Run and the enemy
  AI": the handler fills the record, `Effect_ApplyResult` applies it).
- **The action kind** `0x904B35`: 1 attack, 4 ability, 5 item, as the
  sibling's command bytes `C+0x119` are.

## 1. What is ours

Each extent was read to its last instruction (capstone, 2026-09-25); the
queue's extents were `pe_hidden.py`'s upper bounds and ran into the padding.
PSX twins are the catalogue's pairs, not read here.

| Function | Entry | Bytes | PSX | Slot |
|---|---|--:|---|---|
| `EnemyOp_StepDispatch` | `0x4360F0` | 0x12 | `0x801E3140` | `BattleEnemy_States` 0 |
| `EnemyOp_Begin` | `0x436110` | 0x55 | `0x801E3184` | `EnemyOp_Steps` 0 |
| `EnemyOp_EnterDispatch` | `0x436170` | 0x12 | `0x801E3224` | `EnemyOp_Steps` 1 |
| `EnemyOp_ScaleInDispatch` | `0x436190` | 0x12 | `0x801E3268` | `EnemyOp_EnterSubs` 0 |
| `EnemyOp_ScaleInStart` | `0x4361B0` | 0x59 | `0x801E32AC` | `EnemyOp_ScaleInSubs` 0 |
| `EnemyOp_ScaleInStep` | `0x436210` | 0x5E | `0x801E3310` | `EnemyOp_ScaleInSubs` 1 |
| `EnemyOp_Idle` | `0x4363B0` | 0x2A | `0x801E3550` | `EnemyOp_Steps` 2 |
| `EnemyOp_Wait` | `0x4363E0` | 0x127 | `0x801E35B0` | `EnemyOp_Steps` 3 |
| `EnemyOp_HighlightOn` | `0x436510` | 0x46 | `0x801E37B4` | `EnemyOp_WaitSubs` 0 |
| `EnemyOp_HighlightPulse` | `0x436560` | 0x6C | `0x801E3838` | `EnemyOp_WaitSubs` 1 |
| `EnemyOp_ActDispatch` | `0x4366E0` | 0x12 | `0x801E3AE0` | `EnemyOp_Steps` 6 |
| `EnemyOp_ActBegin` | `0x436700` | 0x14 | `0x801E3B24` | `EnemyOp_ActSubs` 0 and 2 |
| `EnemyOp_HitDispatch` | `0x436720` | 0x12 | `0x801E3B48` | `EnemyOp_ActSubs` 1 |
| `EnemyOp_ReceiveAction` | `0x436740` | 0x2BC | `0x801E3B8C` (the sibling's `Battle_ResolveAction_Enemy`) | `EnemyOp_HitSubs` 0 |
| `EnemyOp_WaitAnimOnce` | `0x436A00` | 0x12 | `0x801E406C` | `EnemyOp_HitSubs` 1 |
| `EnemyOp_HitEnd` | `0x436A20` | 0x12C | `0x801E40B4` | `EnemyOp_HitSubs` 2 |
| `EnemyOp_DeathDispatch` | `0x436D90` | 0x12 | `0x801E46B8` | `EnemyOp_ActSubs` 4 |
| `EnemyOp_DeathSwellStart` | `0x436DB0` | 0x44 | `0x801E46FC` | `EnemyOp_DeathSubs` 0 |
| `EnemyOp_DeathSwell` | `0x436E00` | 0x3E | `0x801E4738` | `EnemyOp_DeathSubs` 1 |
| `EnemyOp_DeathFlash` | `0x436E40` | 0x7D | `0x801E4794` | `EnemyOp_DeathSubs` 2 |
| `EnemyOp_DeathSquash` | `0x436EC0` | 0x32 | `0x801E482C` | `EnemyOp_DeathSubs` 3 |
| `EnemyOp_HitPose` | `0x437420` | 0x28 | `0x801E51C8` | `EnemyOp_Steps` 11 |

Most of these ops also sit in the boss and event tables of section 2
(`EnemyOp_StepsB`..`F`), so the same code runs for those enemies too.

Every name except the dispatchers', `EnemyOp_ActBegin`'s and
`EnemyOp_WaitAnimOnce`'s is a hypothesis about the role (`status =
"hypothesis"`); the behaviour under each name is read.

## 2. The tables named

Each is a `[[data]]` entry in `symbols.toml` with `ctype = "unsigned long"`
and its count. The count runs to the next table that some code indexes
(`pe_xref.py`); the tables are packed, so a step byte past a table's end
runs the next table's entries (no op checks its index).

| Table | At | Count | Indexed by | Holds |
|---|---|--:|---|---|
| `BattleEnemy_States` | `0x64B084` | 64 | `BattleEnemy_RunAll`, by `+0x100` (and from `0x64B088` in an event battle) | the enemy states; entry 1 is a null |
| `EnemyOp_Steps` | `0x64B1A0` | 12 | `EnemyOp_StepDispatch`, by `+1` | state 0's steps; a null follows at `0x64B1D0` |
| `EnemyOp_EnterSubs` | `0x64B1D4` | 2 | `EnemyOp_EnterDispatch`, by `+2` | `0x436190`, `0x436270` |
| `EnemyOp_ScaleInSubs` | `0x64B1DC` | 2 | `EnemyOp_ScaleInDispatch`, by `+3` | `0x4361B0`, `0x436210` |
| `EnemyOp_EnterSubs2` | `0x64B1E4` | 2 | `0x436270` (nobody's), by `+3` | `0x436290`, `0x436330` |
| `EnemyOp_WaitSubs` | `0x64B1EC` | 2 | `EnemyOp_Wait`, by `+2` (a call, not a jump) | the highlight on and its pulse |
| `EnemyOp_Step5Subs` | `0x64B1F4` | 2 | `0x436620` (nobody's), by `+2` | `0x436640`, `0x4366B0` |
| `EnemyOp_ActSubs` | `0x64B1FC` | 6 | `EnemyOp_ActDispatch`, by `+2` | `0x436700` `0x436720` `0x436700` `0x436BC0` `0x436D90` `0x436F00` |
| `EnemyOp_HitSubs` | `0x64B214` | 3 | `EnemyOp_HitDispatch`, by `+3` | `0x436740` `0x436A00` `0x436A20` |
| `EnemyOp_Act3Subs` | `0x64B220` | 5 | `0x436BC0` (nobody's), by `+3` | `0x436BE0` `0x436C40` `0x436C90` `0x436D50` `0x436A20` |
| `EnemyOp_DeathSubs` | `0x64B234` | 4 | `EnemyOp_DeathDispatch`, by `+3` | the death's four parts |
| `EnemyOp_Act5Subs` | `0x64B244` | 3 | `0x436F00` (nobody's), by `+3` | `0x436F20` `0x436A00` `0x436FD0` |
| `EnemyOp_StepsB` / `C` | `0x64C7B0` / `0x64C810` | 12 each | states 7 and 8 (`0x437A10`, `0x437B70`), by `+1` | `EnemyOp_Steps` with their own entries 0, 1, 6 and 11 |
| `EnemyOp_ActSubsB` / `C` / `E` | `0x64C7E0` / `0x64C840` / `0x64C904` | 6 each | the step-6 ops of those tables, by `+2` | `EnemyOp_ActSubs` with `0x43B550` for the death |
| `EnemyOp_StepsD` / `E` / `F` | `0x64C890` / `0x64C8D4` / `0x64C928` | 12 each | jumps inside `0x437CC0`'s extent (the event hooks [`battle_flow.md`](battle_flow.md) names) | `EnemyOp_Steps` with their own entries |

Two more tables sit after `EnemyOp_Act5Subs`, at `0x64B250` and `0x64B25C`
(`pe_xref.py`: a jmp at `0x43703B` and a call at `0x43718B`). Their extents
were not read, and they are not named.

`0x4DF820`, in several slots (`EnemyOp_Steps` 10, `EnemyOp_StepsB` 1, ...),
is a bare `ret`.

## 3. What the ops do

The short version. The full account is above each function in
`enemy_ai_ops.cpp`, and in its `evidence` field.

- **The steps of state 0**, by `+1`: 0 `EnemyOp_Begin`, 1 the entrance, 2
  `EnemyOp_Idle`, 3 `EnemyOp_Wait`, 6 an action taken or received
  (`EnemyOp_ActDispatch`), 11 `EnemyOp_HitPose`. Steps 4, 5, 7, 8 and 9
  (`0x4365D0`, `0x436620`, `0x437030`, `0x437180`, `0x437240`) are
  nobody's yet.
- **The entrance.** `EnemyOp_Begin` points the enemy's animation table
  `+0xFC` at `0x64B078` (the bytes 0..7), stores the two hooks `0x904B64` /
  `0x904B68` (`0x437720` / `0x437750`, group CE's), sets animation 0 and
  `Sprite_Current +0 |= 0x40`, and passes the status word to
  `Battle_StatusTint`. The scale-in (`EnemyOp_ScaleInStart` / `Step`)
  clears that 0x40 again and grows the dword `+0x40` from 0 to `0x10000`
  (1.0). The speed `+0xC` starts at 0 and gains `+0x18` = `0x666` a frame;
  `+0x44` follows it. At the end the steps become 3, 0, 0 (the idle loop).
  The reading of `+0x40` / `+0x44` as the sprite's scale (x, y) is
  inference: `0x10000` is the start value of the death's swell as well.
- **The idle loop.** `EnemyOp_Wait` runs every frame.
  - First the highlight: while a target is being picked (`0x904AAF`) and
    the command's target byte (at the pointer `0x939FA0`) is this actor, or
    has 0x40 (a whole side), `EnemyOp_HighlightOn` takes a tint record.
    `EnemyOp_HighlightPulse` then sets its r, g and b to the menu cursor's
    pulse `0x904AC8` every frame, and releases it when the target moves
    away.
  - Then, while the status `+0x92` has 0x20, a bob: a counter steps between
    -12 and 12, its direction in bit 11 of `+0x110`. The sprite's `+0x38`
    (or `+0x34` for modes 1 and 3) becomes a base plus the counter << 9.
    Status 0x20 is presumably a floating enemy (a guess from the motion).
  - Last, the animation tick, unless bit 12 of `+0x110` is set.
- **The hit taken** (step 6, `+2` = 1): `EnemyOp_ReceiveAction`, then
  `EnemyOp_WaitAnimOnce` until the animation ends, then `EnemyOp_HitEnd`.
  `EnemyOp_ReceiveAction`:
  1. Points the result record `0x904B60` at the enemy's `+0x104` and zeroes
     it. Copies the enemy's 32 bytes `+0xB0..` to `0x939F80`.
  2. Applies the action. An attack goes through
     `Battle_ApplyDamage(0x904B34, 0x904B44)`, whose answer is the HP
     delta; an ability or an item goes through `Effect_ApplyResult`.
  3. A miss (flags bit 0 with a delta of 0) ends it there with `+2` = 3.
  4. Otherwise it picks the hit animation 4 (with exemptions by `+0x110`
     bit 9, the item's class and the ability's record) and shows the
     pop-ups. It ORs the pending status `0x904B98` into `+0x92` and runs
     `EnemyAI_TurnCheck`.
  5. A kill (a positive delta with HP `+0xA4` at 0) sets the status to
     0x4000 - it replaces the status rather than adding to it - with a
     tint of (0x10, 0x10, 0x10) under round flag 0x80.
  6. A negative delta (a heal) plays `0x206`. Last, unless round flag
     0x2000, the hit sound and pop-up for an attack (and for an ability
     whose record's `+0xD` has 8). The sibling's `se_cues.toml` records
     `0x206` from `0x801E3F94`, inside this op's PSX twin (and `0x204`
     from `0x801E47A4`, inside `EnemyOp_DeathFlash`'s).
- **The hit's end.** `EnemyOp_HitEnd` sends a dead enemy (`+0x93` bit 6)
  to the death: `+1..+3` = 6, 4, 0, which `EnemyOp_ActDispatch` runs as
  `EnemyOp_DeathDispatch`. Otherwise it handles round flag 0x40. A set flag
  becomes 0x1000; a clear one may be set by `BattleEnemy_Chance70`, for an
  attack or for an ability whose record allows it.
  - These are `Chance70`'s only two callers. What 0x40 / 0x1000 mean is
    not read here. The steal the sibling's `STEAL.md` rules out used the
    same test.
  - Then the actor's turn bit is cleared and the steps become 2, 0, 0 (the
    idle loop again).
- **The death.**
  1. The swell: the scale from 1.0 to 1.25 (`0x14000`).
  2. The flash: `0x204`, and in an event battle the enemy's cue word `+6`
     through `0x437450`. The old tint is released and a new one set to
     (0xFF, 0xFF, 0xFF).
  3. The squash: `+0x44` falls with the speed `+0x10`, which gains
     `-0x2000` a frame, until it is below 0.
  4. `Battle_EnemyDefeated`, by a tail jump. On the combat route the kill's
     single `Battle_EnemyDefeated` call comes this way.
- **`EnemyOp_HitPose`** (step 11): animation 4, then the animation tick by
  bit 4 or 5 of `+0x110`.

## 4. The fuzz and its negative controls

The fuzz runs at start-up under `BOF3X_SHADOW=enemy_ai_ops`
(`src/game/enemy_ai_ops_fuzz.cpp`). It makes 22 byte-copies. Every call and
tail jump out is re-aimed at a recorder, using `CloneCall` with `expected`.
Two copies start with their call (`0x436A00`, `0x436A20`), and offset 0 is
named for both. It also swaps these for recorders, and puts every one back
afterwards:

- all 46 code pointers from `0x64B1A0` to `0x64B254`. Each recorder logs
  its slot, so a dispatch through the wrong table, or by the wrong step
  byte, shows;
- every enemy's `+0xF4` hook.

Every enemy's `+0xF8` points into a cue buffer of the fuzz's own, at a
different offset per enemy. The command's target pointer `0x939FA0` points
into a target buffer.

**Each round** starts from random bytes over the following regions (16 KB):

- the task slots and the enemy objects;
- `Sprite_Current` and `0x939AD8`;
- `0x939F80..0x939FA3`;
- the battle globals `0x904AA0..0x904D00`;
- tint records 0..63;
- the ability records 0..255.

The pointers and indices are then put back inside their arrays: step bytes
inside the swapped block, actor bytes mostly 3..10 (at most 63), tint
indices below 64. Each branch's boundaries are seeded:

- every dispatch index, mostly in its table and sometimes past its end,
  still inside the block;
- the scales at `0xFFFF` / `0x10000` / `0x10001` / `0x13FFF` / `0x14000` and
  the sign's edges;
- the bob counter at ±11, ±12, ±13 and `0x7F` / `0x80`, and modes 1 and 3;
- the targeting on and off, with the target byte the actor, a side, or
  neither;
- action kinds 0..6;
- deltas 0, 1, -1, `0x7FFF`, `0x8000`;
- HP 0;
- the round flags 0x40, 0x80 and 0x2000;
- the dead bit;
- the squash at 0, 1 and -1.

**The recorders are not quiet.** Each one moves what its caller reads again
after the call:

- `0x939AD8` after nearly every callee;
- `Sprite_Current` after `SetAnimation`, `Sprite_SetTint`,
  `Sprite_ReleaseTint` and `Battle_ClearActorBit`;
- the result pointer `0x904B60` after `Battle_ApplyDamage`;
- the id `0x904B80` after `Chance70`;
- the HP after the turn check.

A general disturber also moves one of 24 watched bytes three calls in
four.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1`:

    enemy_ai_ops self-test: 22000 rounds over 22 functions (1000 each), 28115 calls to the stand-ins, 0 MISMATCHES
    coverage: op-table slots reached 46 of 46
      (stand-in ids: 1 SetAnimation, 2 ScriptTick, 3 ScriptTickOnce, 4 Chance70, 5 EnemyDefeated, 6 Sprite_ScriptTickOnce,
       7 StatusTint, 8 SetTint, 9 Tint_Release, 10 ReleaseTint, 11 ApplyDamage, 12 Effect_ApplyResult, 13 0x591810,
       14 damage pop-up, 15 AP pop-up, 16 TurnCheck, 17 0x437450, 18 Sound_PlayEffect, 19 hit sound, 20 hit pop-up,
       21 ClearActorBit, 22 the +0xF4 hook; "a:n" is n rounds of 1000 that called a)
      Begin 1:1000 7:1000; ScaleInStart 1:1000; ScaleInStep 2:1000; Idle 1:1000 2:1000; Wait 2:760;
      HighlightOn no calls 470, 8:530; HighlightPulse no calls 555, 9:445; ActBegin no calls 1000;
      ReceiveAction 1:261 2:993 8:51 11:180 12:390 13:139 14:234 15:232 16:993 17:62 18:501 19:191 20:191 22:246;
      WaitAnimOnce 3:1000; HitEnd 4:185 6:1000 10:337 21:663; DeathSwellStart / DeathSwell no calls 1000;
      DeathFlash 8:1000 10:1000 17:509 18:1000; DeathSquash no calls 525, 5:475; HitPose 1:1000 2:365 3:182

The run with `BOF3X_SHADOW='*'` exited 0 with `inject: 1046 ours`. Every
self-test in the log reported 0 mismatches, and there was no Fatal.

**Negative controls.** A script (`cf/controls.py` in the session scratchpad)
planted each one alone: replace, build, self-test, revert. **112 were
planted and all 112 were refused.** The table gives the rounds that refused
each one, of 1,000. The self-test exited 3 (a Fatal) for every one.

| | Planted | Refused in |
|---|---|--:|
| X1 | StepDispatch: by +2 | 944 |
| X2 | EnterDispatch: the table one entry on | 1000 |
| X3 | ScaleInDispatch: by +2 | 880 |
| X4 | ActDispatch: 0x939AD8's +2 | 275 |
| X5 | HitDispatch: by +2 | 868 |
| X6 | DeathDispatch: through EnemyOp_HitSubs | 1000 |
| B1 | Begin: the animation table 0x64B07C | 1000 |
| B2 | Begin: the two hooks the same | 1000 |
| B3 | Begin: animation 1 | 1000 |
| B4 | Begin: +0 |= 0x40 before the call | 345 |
| B5 | Begin: Sprite_Current's status word | 711 |
| B6 | Begin: the step +2 on | 1000 |
| S1 | ScaleInStart: 0x40 kept | 501 |
| S2 | ScaleInStart: +0x18 = 0x667 | 1000 |
| S3 | ScaleInStart: +0x48 before the call | 484 |
| T1 | ScaleInStep: up to 0x10000 inclusive | 67 |
| T2 | ScaleInStep: the scale unsigned | 316 |
| T3 | ScaleInStep: the speed stepped before +0x44 | 453 |
| T4 | ScaleInStep: no tick at the end | 547 |
| T5 | ScaleInStep: the end step 4 | 523 |
| T6 | ScaleInStep: +0x48 kept | 543 |
| I1 | Idle: bit 2 of +0x110 | 486 |
| I2 | Idle: the step on the Sprite_Current of before the call | 24 |
| I3 | Idle: animation 8 and 0 inverted | 1000 |
| W1 | Wait: the bob on status 0x10 | 480 |
| W2 | Wait: the flip at 11 | 32 |
| W3 | Wait: the flip at -11 | 29 |
| W4 | Wait: the direction from bit 10 | 417 |
| W5 | Wait: x for modes 1 and 2 | 378 |
| W6 | Wait: the bob << 8 | 821 |
| W7 | Wait: the counter unsigned | 427 |
| W8 | Wait: the tick gate bit 13 | 495 |
| W9 | Wait: 0x939AD8 read before the call | 18 |
| W10 | Wait: the sub-op by +3 | 888 |
| W11 | Wait: the flip test before the step | 68 |
| H1 | HighlightOn: a side by 0x80 | 279 |
| H2 | HighlightOn: tint a = 1 | 530 |
| H3 | HighlightOn: the index to the Sprite_Current of before the call | 237 |
| H4 | HighlightOn: no targeting test | 115 |
| P1 | HighlightPulse: +5 not +4 | 1000 |
| P2 | HighlightPulse: released while it is the actor | 530 |
| P3 | HighlightPulse: released by +6 | 442 |
| P4 | HighlightPulse: +2 on the Sprite_Current of before the call | 7 |
| A1 | ActBegin: +4 = 1 | 1000 |
| A2 | ActBegin: +2 = 2 | 1000 |
| R1 | ReceiveAction: the target byte 0x939AD8's +5 | 263 |
| R2 | ReceiveAction: the record at +0x100 | 1000 |
| R3 | ReceiveAction: the AP delta not zeroed | 785 |
| R4 | ReceiveAction: 28 bytes copied | 1000 |
| R5 | ReceiveAction: attacker and target swapped | 178 |
| R6 | ReceiveAction: the delta through the record of before the call | 57 |
| R7 | ReceiveAction: kind 3 an effect | 116 |
| R8 | ReceiveAction: kind 6 an effect | 99 |
| R9 | ReceiveAction: the hook with 2 | 246 |
| R10 | ReceiveAction: 0x939AD8 not re-read after the hook | 86 |
| R11 | ReceiveAction: the miss on the AP delta | 6 |
| R12 | ReceiveAction: a miss +2 = 2 | 7 |
| R13 | ReceiveAction: the hit animation gate bit 8 | 236 |
| R14 | ReceiveAction: the item class bit 3 | 74 |
| R15 | ReceiveAction: the item class arguments swapped | 136 |
| R16 | ReceiveAction: the ability's +8 bit 4 | 50 |
| R17 | ReceiveAction: 0x939AD8 not re-read after the item | 73 |
| R18 | ReceiveAction: the pop-up's actor from 0x939AD8 | 197 |
| R19 | ReceiveAction: the AP pop-up on bit 2 | 217 |
| R20 | ReceiveAction: 0x904B98 not zeroed | 59 |
| R21 | ReceiveAction: the status ORed after the turn check | 66 |
| R22 | ReceiveAction: 0x939AD8 not re-read after the turn check | 336 |
| R23 | ReceiveAction: a zero delta as damage | 201 |
| R24 | ReceiveAction: the dead status ORed | 64 |
| R25 | ReceiveAction: the death tint on round flag 0x40 | 30 |
| R26 | ReceiveAction: the cue word +6 | 62 |
| R27 | ReceiveAction: the heal sound on the HP delta only | 153 |
| R28 | ReceiveAction: the heal sound 0x205 | 501 |
| R29 | ReceiveAction: the hit gate round flag 0x1000 | 129 |
| R30 | ReceiveAction: the ability's hit on +0xD bit 2 | 48 |
| R31 | ReceiveAction: round flag 0x80 kept | 735 |
| R32 | ReceiveAction: +3 on the Sprite_Current of the start | 225 |
| R33 | ReceiveAction: +9 zeroed on 0x939AD8's object | 56 |
| R34 | ReceiveAction: kind read afresh for the item test not done (kind of the start) | 11 |
| O1 | WaitAnimOnce: inverted | 1000 |
| O2 | WaitAnimOnce: BattleEnemy_ScriptTick | 1000 |
| E1 | HitEnd: BattleEnemy_ScriptTickOnce | 1000 |
| E2 | HitEnd: the dead bit on +0x92 | 509 |
| E3 | HitEnd: the death +3 = 1 | 337 |
| E4 | HitEnd: the death steps on the Sprite_Current of before the call | 141 |
| E5 | HitEnd: 0x40 becomes 0x2000 | 102 |
| E6 | HitEnd: 0x40 kept with 0x1000 | 126 |
| E7 | HitEnd: a side by 0x40 only | 18 |
| E8 | HitEnd: an ability's roll without the +0xD test | 102 |
| E9 | HitEnd: the +8 test before Chance70 | 36 |
| E10 | HitEnd: +0x110 bit 7 cleared too | 319 |
| E11 | HitEnd: bit 9 kept | 347 |
| E12 | HitEnd: the actor bit by 0x939AD8's +5 | 316 |
| E13 | HitEnd: the end +3 = 1 | 663 |
| E14 | HitEnd: an attack rolls whatever the kind | 261 |
| D1 | DeathSwellStart: +0x40 = 0 | 1000 |
| D2 | DeathSwellStart: +0x48 = 1 | 1000 |
| L1 | DeathSwell: to 0x10000 | 214 |
| L2 | DeathSwell: the end +3 = 3 | 340 |
| L3 | DeathSwell: +0x44 not grown | 660 |
| F1 | DeathFlash: sound 0x205 | 1000 |
| F2 | DeathFlash: the cue word +4 | 509 |
| F3 | DeathFlash: the tint set before the release | 1000 |
| F4 | DeathFlash: tint a = 1 | 1000 |
| F5 | DeathFlash: +0x1C = -0x1000 | 1000 |
| F6 | DeathFlash: no cue in an event battle | 509 |
| Q1 | DeathSquash: at 0 too | 61 |
| Q2 | DeathSquash: the speed stepped first | 1000 |
| Q3 | DeathSquash: +0x44 += +0x1C | 1000 |
| K1 | HitPose: animation 5 | 1000 |
| K2 | HitPose: bit 5 first | 146 |
| K3 | HitPose: +0x110 read before the call | 273 |

**The thinnest:** R11 (6), P4 (7), R12 (7), R34 (11), E7 (18), W9 (18) and
I2 (24). They all depend on a narrow input. R11 and R12 need a miss:
flags bit 0 and a zero delta that the attack's stand-in did not overwrite.
P4, W9 and I2 need a stand-in that moved `Sprite_Current` or `0x939AD8`
during that one call.

**What the fuzz cannot see:**

- anything the callees really do;
- the order of stores with no call between them;
- the ops' `eax` on return. The originals return whatever their last call
  or tail jump left, and ours are `void`. No caller reads it:
  `BattleEnemy_RunAll` drops it, and `EnemyOp_Wait`'s call through
  `EnemyOp_WaitSubs` reloads `eax`. Every dispatcher over these tables that
  is not ours (`0x437A10`, `0x437B70`, the jumps in `0x437CC0`'s extent,
  `0x436BC0`, `0x436F00`, `0x436270`, `0x436620`) is a tail `jmp` whose
  own caller is `RunAll` or an enemy's `+0xF4` hook, whose answer
  `EnemyOp_ReceiveAction` and `RunAll` drop too;
- the upper bits of the originals' pushes. They push bytes and words in
  registers whose upper bits are left over (`mov dx, [..]; push edx`). The
  stand-ins record what each callee reads, checked for each one:
  - `Battle_StatusTint` reads bit 7;
  - `Battle_ApplyDamage` and `Battle_CalcDamage` mask to a byte;
  - `0x591810` reads bytes (battle_sprites.md);
  - `0x453EB0` reads a word and a byte (its `movsx` of `[esp+0x10]` and
    `and eax, 0xFF`);
  - `0x437450` compares the low word, and passes the dword on to
    `Sound_PlayEffect(unsigned short)`.

  Ours pass them clean.

## 5. Found on the way

- **Latent, as the original has it:**
  - **A tint index of `0xFF`** (known-defects.md D60). `Sprite_SetTint` answers `0xFF` when all 32
    records are taken, and `EnemyOp_HighlightOn` stores it in `+7`
    untested. `EnemyOp_HighlightPulse` then writes the pulse into
    `0x7E12F2..0x7E12F4`, which is `0x7E0700` + 12 x 255, past the records.
    It then calls `Tint_Release(0xFF)`, which reads the record there. The
    fuzz keeps the index below 64 and does not reach this. The PSX twin
    was not read.
  - **The bob's slot is indexed by the actor** (known-defects.md D64). `EnemyOp_Wait` indexes
    the 0x84-byte battle-task slots by the actor byte `+5`, unchecked. An
    enemy's actor number is 3..10, so the slots used are 3..10. Whether
    those slots really belong to the actors was not read; it looks as if
    the battle keeps one task per actor at the start of the array. A
    corrupt actor byte of 0x74 or more would read past `.data` and fault.
  - **No op checks its table index** (known-defects.md D59). A step byte past a table's end runs
    the next table's entries, and past the last, whatever follows
    (`0x64B258` on is more tables, then bytes).
- **`EnemyOp_ReceiveAction` replaces the status on a kill**
  (`mov word [+0x92], 0x4000`), where `Battle_EnemyDefeated` later ORs in
  0x4000 (`+0x93 |= 0x40`). This looks deliberate - a dead enemy keeps no
  other status - and is kept.
- **Other groups' and nobody's addresses**, reached through raw addresses in
  `enemy_ai_ops_callees.h` and not bound here:
  - `0x437450`, 18 bytes: `Sound_PlayEffect(id)` unless the low word is
    `0xFFFF`. It is nobody's. It is already in `entries_logic.txt`
    (`00437450 12`). `0x4FC030` calls it too.
  - `0x453EB0`: `Battle_SetDamagePopup`'s AP twin (a task of kind 0 /
    parameter 1 through `0x435180`, `+0xB` = 1, the actor's record to
    `+0x80`, the magnitude of the s16 amount). Nobody's.
  - `0x591810`: nobody's (battle_sprites.md section 3's table).
  - `0x437720` and `0x437750` (group CE's) are stored by `EnemyOp_Begin`
    into `0x904B64` / `0x904B68`. That is how CE's two are reached. They
    are data here, not calls.
- **The rest of the op tables is nobody's.** Eighteen ops in the tables of
  section 2 are in no group of this round, besides `0x43B550` and the boss
  entries:
  - `0x436270`, `0x436290`, `0x436330`, `0x4365D0`, `0x436620`, `0x436640`,
    `0x4366B0`;
  - `0x436BC0`, `0x436BE0`, `0x436C40`, `0x436C90`, `0x436D50`;
  - `0x436F00`, `0x436F20`, `0x436FD0`;
  - `0x437030`, `0x437180`, `0x437240`.

  The boss entries run `0x437A30`..`0x438120`.

  They are presumably what the combat route did not reach: the second
  entrance, the enemy's own action (`0x436BC0` and `EnemyOp_Act3Subs`), steps 4, 5 and 7..9.
  They are the natural next group, and taking them needs only their own
  entries: the tables are named.
- **`entries_logic.txt`**: the two corrections [`battle_flow.md`](battle_flow.md)
  section 7 asked for were not applied. They are `004360C0 A8C` → `29` and
  `00436B50 8F8` → `6F`, and both old extents still cover this group's
  functions. This group's 22 lines are appended with their read sizes;
  the old lines are left for the merger.

Nothing here is misfiled in another group. The queue's "folded into"
column is right for all 22: `0x4360F0`..`0x436A20` fall inside
`BattleEnemy_ScriptTickOnce`'s old extent, and `0x436D90`..`0x437420`
inside `BattleEnemy_Chance70`'s.

## 6. For the batch check

The combat A/B `analysis/validate_combat.sh` reaches all 22 functions:

- the entrance and the idle loop every frame of the fight;
- the hit and its end on every blow;
- the death on the kill frame.

The fuzz alone covers:

- the event-battle paths (`0x904AAA` non-zero: the `+0xF4` hook and the
  cues);
- the item and ability branches of `EnemyOp_ReceiveAction`;
- a heal;
- the highlight on a side.

Not reached by either:

- the tint index `0xFF`;
- step bytes past a table's end.

## 7. Left original

None of the group's 22.

## 8. Open

- What `0x904AAF`, round flags 0x40 / 0x1000 and status 0x20 mean. The
  readings here (the targeting, a follow-up of some kind, a floating enemy)
  are guesses from use.
- The ability record's byte `+8` bits 2 and 4 and byte `+0xD` bit 3.
  Here they gate the hit animation, the 0x40 roll and the hit sound.
- The PSX twins were not read. The pairs are the catalogue's.
