# The battle object states

**Status:** IN PROGRESS (2026-09-25). Twenty-two functions are ours
(`src/game/battle_obj_states.cpp`, shadow name `battle_obj_states`). Each is
fuzzed headless against a copy of Capcom's with every call re-aimed at a
recorder and every table operand at a table of recorders: 22,000 rounds,
0 mismatches. 115 negative controls were planted: 114 were refused by a count, and one was a change that changes nothing. `BOF3X_SHADOW='*'` passes. No live check yet:
the coordinator's combat A/B after the merge is the first.

This is group CG of the eighth parallel round
([`takeover-queue-round8.md`](takeover-queue-round8.md)): the handlers that
`BattleObj_RunState` `0x4411E0` ([`battle_windows.md`](battle_windows.md))
reaches through the state table `0x64DFE0` by a party object's state byte
+1, the sub-state handlers behind them, and the action's end `0x442DD0` that
two of them tail-jump to. Every function is a *faithful* replacement, so no
`DIVERGENCE.md` entry is owed. No defect of the original was found.

## 1. What is ours

Each extent was read to its last instruction (capstone, 2026-09-25, a
scratch walker over `BOF3.exe`); the catalogue's extents (`pe_hidden.py`, to
the next start) were upper bounds and three were wrong (section 6). The PSX
twins are `analysis/pairs_propagated.json`'s in `BATTLE.EMI` (section
`8a80230e...`), with their tier; **none was read** this round.

| Function | Entry | Bytes | Reached through | PSX twin (tier) |
|---|---|--:|---|---|
| `BattleObj_StateInit` | `0x441200` | 0xA4 | state 0 | `0x801DEEC8` (table-anchored; the sibling's `Battle_InitMemberActor`) |
| `BattleObj_StateStand` | `0x441550` | 0x13 | state 2 | `0x801DF36C` (gap31) |
| `BattleObj_StateIdle` | `0x441570` | 0x22E | state 3 | `0x801DF3AC` (gap31) |
| `BattleObj_IdleTintOn` | `0x4417A0` | 0x46 | state 3's sub-state 0 (`0x64E014`; slot 13) | `0x801DF71C` (gap31) |
| `BattleObj_IdleTintOff` | `0x4417F0` | 0x6C | state 3's sub-state 1 (slot 14) | `0x801DF7A0` (gap31) |
| `BattleObj_StateAttack` | `0x441860` | 0x23 | state 4 | `0x801DF860` (gap31; `Attack_DispatchByCharacter`) |
| `BattleObj_AttackStart` | `0x441890` | 0x91 | state 4's by-character jump (slots 15..25) | `0x801DF8C8` (gap31) |
| `BattleObj_StateSwing` | `0x441930` | 0x12 | state 5 | `0x801DF9D0` (gap31) |
| `BattleObj_SwingCue` | `0x441950` | 0x72 | state 5's sub-state 0 (`0x64E074`) | `0x801DFA14` (table-anchored; `Battle_SwingCue_Step`) |
| `BattleObj_SwingEnd` | `0x4419D0` | 0x39 | state 5's sub-state 1 | `0x801DFB18` (table-anchored) |
| `BattleObj_StateCast` | `0x4429E0` | 0x12 | state 7 | `0x801E1604` (gap31) |
| `BattleObj_CastStart` | `0x442A00` | 0x112 | state 7's sub-state 0 (`0x64E0E4`) | `0x801E1648` (table-anchored) |
| `BattleObj_CastCue` | `0x442B20` | 0x54 | state 7's sub-state 1 | `0x801E1814` (table-anchored) |
| `BattleObj_CastWait` | `0x442B80` | 0x20 | state 7's sub-state 2 | `0x801E18C4` (table-anchored) |
| `BattleObj_StateCastDone` | `0x442BA0` | 0x17 + 0xF | state 8 | `0x801E191C` (table-anchored) |
| `BattleObj_CastDoneTick` | `0x442BC0` | 0xE | state 8's sub-state 0 (`0x64E118`) | `0x801E1968` (table-anchored) |
| `BattleObj_CastDone` | `0x442BD0` | 0x64 | state 8's sub-state 1 | `0x801E19A0` (table-anchored; `Actor_SkillItemDone`) |
| `BattleObj_CastDoneWait` | `0x442C50` | 0xF | state 8's sub-state 4 | `0x801E1A88` (table-anchored) |
| `BattleObj_State12` | `0x442D60` | 0x12 | state 12 | `0x801E1C5C` (table-anchored) |
| `BattleObj_State12Pose` | `0x442D80` | 0x2B | state 12's sub-state 0 (`0x64E134`) | `0x801E1CA0` (table-anchored) |
| `BattleObj_State12Wait` | `0x442DB0` | 0x1D | state 12's sub-state 1 | `0x801E1D0C` (table-anchored) |
| `BattleObj_EndAction` | `0x442DD0` | 0x62 | tail jumps of `0x4419D0` and `0x442BA0` | none paired |

Every name is a hypothesis from what the code does, and says so in
`symbols.toml` (`status = "hypothesis"`). "Swing", "cast" and "state 12" are
guesses from the cues and poses; which battle commands put an object in
states 5, 7 and 12 was not traced (it is whoever stores +1, outside this
group). The PSX names the sibling gave four twins are cited, not adopted.

**Not taken: `0x4414E0`.** It is on the queue (112 bytes by the catalogue, "indirect call",
first caller `0x441550`), but it is **case 5 of `BattleObj_PickPose`'s own
switch** (`0x4412B0`, BC's, already ours): the table at `0x4414F8` holds
`0x441424`, `0x441457`, `0x44146C`, `0x4414F5`, `0x441482`, `0x4414E0`
(capstone and `pe_data`-style read, 2026-09-25), and `0x4414E0` is the fifth
slot (`.text 0x44150C`, the catalogue's "pointer in"). Its "first caller"
`0x441550` is the return address `StateStand`'s `call 0x4412B0` leaves. It
has no frame and ends in `ret`, so a detour there would work, but under our
`BattleObj_PickPose` it is never entered - ours does case 5 itself
(`pose(4)` for action 5). As the round's rule says for a switch case, it goes
with its host, which is ours already.

**Taken beyond the list: `0x442DD0`.** It sits inside `0x442DB0`'s catalogue
extent (144 bytes, to `0x442E40`); `0x442DB0` really ends at `0x442DCC`.
It is a function of its own, reached only by two tail jumps of this group
(`0x441A03`, `0x442C69`), so it is this group's.

**`0x442BA0` has two chunks.** `0x442BA0..0x442BB6` ends in `jmp 0x442C60`,
and `0x442C60..0x442C6E` (inside `0x442C50`'s catalogue extent) tests
`0x904AA8` bit 2 and tail-jumps to `0x442DD0`. Nothing else reaches
`0x442C60`. The copy in the fuzz spans both (0xD0 bytes; the functions
between are never run in it).

## 2. The table, and what each state does

**`BattleObj_StateTable` `0x64DFE0`, 27 dwords** (`symbols.toml` `[[data]]`),
read by nothing but `BattleObj_RunState`'s `jmp [ecx*4 + 0x64DFE0]`
(`pe_xref.py`, 2026-09-25):

| Slot | Handler | |
|--:|---|---|
| 0 | `0x441200` | ours: the set-up |
| 1, 9 | `0x437CC0` | a bare `ret` |
| 2 | `0x441550` | ours: stand |
| 3 | `0x441570` | ours: idle |
| 4 | `0x441860` | ours: attack, by character |
| 5 | `0x441930` | ours: swing |
| 6 | `0x441A10` | not in any group: `jmp [0x64E07C + 4 * +2]`, 26 dwords to `0x64E0E0` (`0x441A30` .. `0x442980`) |
| 7 | `0x4429E0` | ours: cast |
| 8 | `0x442BA0` | ours: the cast's end |
| 10 | `0x442C70` | not in any group: `jmp [0x64E12C + 4 * +2]` (`0x442C90`, `0x442D10`) |
| 11 | `0x442D30` | not in any group: `call 0x441510`, then a tick by `+0x130` bits 4 / 5 |
| 12 | `0x442D60` | ours: state 12 |
| 13, 14 | `0x4417A0`, `0x4417F0` | ours: state 3's two sub-states |
| 15..25 | `0x441890` | ours: state 4's by-character handler |
| 26 | `0x442E40` | not in any group: `jmp [0x64E13C + 4 * +2]` (`0x442E60`, `0x442F10`, `0x442F80`) |

**The states proper are 0..12.** Slots 13 and on are two other tables that
share the array: state 3 calls through `0x64E014` (= slot 13) by the
sub-state +2, and state 4 tail-jumps through `0x64E01C` (= slot 15) by the
character `Field_State +0x89` - or, while `+0x134` has bit 0, through the
dword `0x64E048` (= slot 26). So the character table's twelfth entry and the
special attack are the same handler, `0x442E40`. After slot 26 come byte
tables (`0x64E04C`, `0x64E058`) and then the sub-state tables named in
`symbols.toml`: `BattleObj_SwingSubs` `0x64E074` (2), `BattleObj_CastSubs`
`0x64E0E4` (3), `BattleObj_CastDoneSubs` `0x64E118` (5),
`BattleObj_State12Subs` `0x64E134` (2). State 6's (`0x64E07C`), state 10's
(`0x64E12C`) and state 26's (`0x64E13C`) tables are not named: their states
are not this group's.

**Every index is unchecked** (known-defects.md D59), as `BattleObj_RunState`'s: a byte, through a
table whose neighbours are other tables. Ours indexes the same way; nothing
aborts, since a wrong index lands on another handler, not outside the image
(the byte tables after `0x64E048` would be jumped through as code pointers -
the original's behaviour, kept).

**The objects.** `Sprite_Current` and `Field_State` both point at the party
object being run (ObjTrio `0x802D40`, 0x14C each; `BattleParty_RunStates`
sets both). The fields read here, beyond `battle_windows.md` section 2's:
+2 the sub-state, +3 / +4, +5 the actor (also used as a battle-task slot
index), +7 a tint record, +9 / +0xA / +0xB counters, +0x29, +0x2B, +0x3E the
ground height, +0x48, +0x5D..+0x5F a colour, +0x9A a u16 the skill cost is
taken from (MP, by that - hypothesis), +0xBA a percentage.

What each does (each function's comment in `battle_obj_states.cpp` is the
full read, and each `evidence` field cites the instructions):

- **State 0, `StateInit`:** dwords +0xC..+0x20 zeroed, +0x29 = 4, the pose
  base +8 from `0x904AAC`, +0x48 = 0, +0x3E = `AreaMap_Elevation(+0x34,
  +0x38)`, then state 2 with +2..+4 zeroed.
- **State 2, `StateStand`:** `BattleObj_PickPose`, a tick, state 3.
- **State 3, `StateIdle`,** each frame: the sub-state (tint on / off, below);
  the colour +0x5D..+0x5F zeroed, or in phase `0x904AA0` = 1 with `0x904AA1`
  above 1 set to 0x78 (this member is `0x904AAE`'s character) or 0xE2 (a
  targeting highlight is a guess, not checked in game). Then, outside phase 5 and while
  `Field_State +0x90` has bit 5, a bob: battle-task slot +5's s8 +9 steps
  one a frame between -12 and 12 (the direction bit 11 of +0x130 flips at
  each end) and the object's y (poses 1 and 3: x) is the slot's plus that
  step << 9. In phase 5, x and y from `0x903780` / `0x903784` (with
  `0x904AA8` bit 15 and +0x134 bit 1) or the slot's. Then
  `BattleObj_ScriptTick` unless +0x130 bit 12.
- **`IdleTintOn` / `IdleTintOff`:** while `0x904AAF` is set and the byte
  `*0x939FA0` names this actor (+5) or all (bit 7), the object is tinted
  (`Sprite_SetTint(obj, 0, 0, 0, 0)` into +7); each frame the record's three
  channel bytes follow `0x904AC8`; when the targeting moves off, the tint is
  released (what it looks like was not checked in game).
- **State 4, `StateAttack`:** the by-character jump above. **`AttackStart`**
  (eleven characters): +9 and +0xA from `0x64E04C` by character (or
  `0x64E058` by `0x904B89` with +0x134 bit 1), pose +8 + 0xC, a tick, state 5.
- **State 5, `StateSwing`:** **`SwingCue`** counts +9 down with a tick once
  per frame; at 0 the cue - 3 when `Battle_RollPendingFlag` answers, else
  `Rand() % 100` below `Field_State +0xBA` sets `0x904AA8` bit 7 and cue 3,
  otherwise cue 2 - then cue 4. **`SwingEnd`**, after the tick finishes:
  with bit 7, `BattleTask_Create(0, 2)`; `Battle_SetTargetFlag40(0x904B44)`,
  `0x904AA8 |= 4`, the action's end. What bit 7 and the percentage mean in
  game was not traced.
- **State 7, `StateCast`:** **`CastStart`** waits for `File_LoadDone`; for
  the action 4 it loads the character's sound (`Battle_LoadSoundByKey(char,
  0x904B8D)`) and sets +9 to 0 or 0x1E by its answer; a skill whose record
  has bit 3 of `0x65C4DD` goes straight to state 8, else +9 from `0x64E0F0`
  / `0x64E0FC`, pose +8 + 0x2C. **`CastCue`** counts +9 down, then cue 5 (and
  3, with +0x134 bit 1 and `0x904B89` none of 4, 7, 8). **`CastWait`**: once
  the tick finishes, state 8.
- **State 8, `StateCastDone`:** the sub-state, then the action's end when
  `0x904AA8` bit 2. **`CastDoneTick`** a tick; **`CastDone`** takes the cost
  `0x904B88` from +0x9A for the action 4, sets `0x904AA8` bit 11, clears the
  action, and goes to sub-state 2 (the bare `ret`) when the skill's word has
  bit 11, else ticks and goes to 4; **`CastDoneWait`** stores the tick's
  answer in +0xB.
- **State 12:** **`State12Pose`** after `File_LoadDone`, pose +8 + 0x24;
  **`State12Wait`** then state 8.
- **`EndAction`:** state 2, +2..+4 zeroed, `Battle_ClearActorBit(+5)`,
  +0x130 bit 9 cleared, the action +0x125 cleared unless `0x904AA8` bit 6.

**Points that matter for faithfulness:**

- **eax.** The original handlers leave whatever eax their last instruction
  had; ours are `void`. The only reader would be `BattleObj_RunState`'s tail
  jump, then `BattleParty_RunStates`' tail jump, then the battle frame
  `0x42E2F0`, whose next instruction after `call 0x441100` (`0x42E39E`) is
  `call 0x435830`: dead. The sub-state calls of states 3 and 8 are followed
  by stores and a memory test.
- **Re-reads after calls** are kept where the original has them:
  `Sprite_Current` after every call, `Field_State` after `Rand`, after cue 5
  and in `CastStart` after the sound load, and each table index re-read for
  its second use (`AttackStart`, `IdleTintOff`'s three stores).
- **Upper bits.** The original pushes bytes with whatever the register's
  upper bits held (`mov al, [0x904B44]; push eax`, `mov dl, [ecx+5]; push
  edx`); every callee reads the low byte (`Battle_SetTargetFlag40`,
  `Battle_ClearActorBit` `& 31`, `Tint_Release`, `Battle_LoadSoundByKey`
  `& 0xFF`, `Battle_PlayActorCue` `& 0xFF`, read in their sources), and the
  stand-ins record only that.
- **Signed.** `SwingCue`'s remainder is `cdq; idiv`'s, compared signed
  (`jle`) with the zero-extended percentage; `StateIdle`'s ends are `jl 12` /
  `jg -12` on an s8.

## 3. Other groups' addresses, and neighbours not in any group

Every callee here is ours already (`BattleObj_PickPose`, `ScriptTick`,
`ScriptTickOnce`, `AreaMap_Elevation`, `Sprite_EnsureAnimation`,
`Sprite_SetTint`, `Tint_Release`, `Battle_RollPendingFlag`,
`Battle_PlayActorCue`, `BattleTask_Create`, `Battle_SetTargetFlag40`,
`File_LoadDone`, `Battle_LoadSoundByKey`, `Battle_ClearActorBit`) or the CRT
`Rand`. No eighth-round group's address is called.

Found on the way, **not taken and not on the queue** (the routes did not
enter them): the handlers `0x441A10` (state 6) and its table `0x64E07C`
(26 dwords, `0x441A30` .. `0x442980`), `0x442C70` (state 10) with `0x442C90`
and `0x442D10`, `0x442D30` (state 11) and the pose helper it calls,
`0x441510` (between `PickPose`'s switch table and `0x441550`: pose +8 + 0x34
when `Field_State +0x91` has bit 3, else + 0x10), `0x442C40` (`jmp
0x441180`, state 8's sub-state 3), and `0x442E40` (state 26 / the special
attack) with `0x442E60`, `0x442F10`, `0x442F80`. They are the rest of the
party objects' state machine, and a natural next group.

`0x442420` (the catalogue's "folded into" host for the second half) is a
real function (`0x442420..0x44249E`): a scan of the three members that
answers al 0 or 1 by each running member's `+0x90` word (`& 0x4004`,
`& 0x4000`), `+0x130` bit 2, `+0x134` bit 1 and the bytes `+0x95..+0x97`; its
`entries_logic.txt` extent `B77` runs over everything to `0x442F97`. Not
this group's; noted for whoever takes it.

## 4. The fuzz and its negative controls

The fuzz runs at start-up under `BOF3X_SHADOW=battle_obj_states`
(`src/game/battle_obj_states_fuzz.cpp`). Twenty-two byte-copies:

- every call and tail jump out re-aimed at a recorder (`CloneCall` with
  `expected`, so a site already re-aimed is refused, not copied);
- the seven table operands (`0x44157F`, `0x441870`, `0x44187F`, `0x44193E`,
  `0x4429EE`, `0x442BAE`, `0x442D6E`) checked against the table they name,
  then aimed at 256-entry tables of our own, one per role, each a different
  rotation of 64 recording handlers; `g` points ours at the same tables.
  (The rotations are 13 apart: 11 apart made the character table's slot 11
  and the special cell the same recorder, which would have hidden control
  A3.)

**Each round** starts from random bytes over ObjTrio, the battle globals
`0x904AA0..0x904B9F`, the 48 battle-task slots, `Sprite_Current`,
`Field_State`, the target pointer, `0x903780..0x903787`, 64 tint records,
the byte tables `0x64E04C` and `0x64E0F0` (0x28 each) and 256 skill records
(`0x65C4DC`). Then the pointers are put back inside ObjTrio (the same object
three rounds in four), +5 inside the task slots, +7 inside the tint
records, the target pointer inside a buffer of ours, the skill below 256;
and each branch's boundaries are seeded (phases 0 / 1 / 5, `0x904AA1` 0 / 1 /
2 / 255, the matching character, the step at ±10..±13 and ±127, the tint
gate and target (this actor, all, another), +0x134 bits, `0x904B89` 4 / 7 / 8
and others, the action 4, counters 0 / 1 / 2, percentages 0 / 1 / 50 / 99 /
100 / 255, and the round flag bits each function tests).

**The recorders are not quiet.** Each logs what its callee would see (both
object pointers, +1 / +2 / +8 / +9, its arguments' low bytes) and three calls
in four moves one of twenty bytes some handler reads after a call: either
object pointer, +1, +2, +5, +7, +8, +9, the character, the action, +0x130 /
+0x134 bits, the round flags, the phase, the tint gate, the target byte, the
cast kind, the skill, the sound set / swing target / cost, the step byte,
+0xBA or +0x9A. On top of that, half the time, the stand-ins whose caller
re-reads an object pointer after the call move it to another member:
`Sprite_Current` after the ticks, `AreaMap_Elevation` and `Sprite_SetTint`,
`Field_State` after `Rand`, the cues, `Battle_LoadSoundByKey` and
`Battle_ClearActorBit` (and `Rand` bumps +0xBA). `Rand`'s answers then land
on the edge of the percentage the caller will read (the percentage itself,
one either side, and negatives). Where a stand-in moves the object, the
seeds give every member the same counter, action and flags most rounds, so
that the branch under test is still reached.

Result (2026-09-25), `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=battle_obj_states`,
exit 0:

    battle_obj_states self-test: 22000 rounds over 22 functions (1000 each), 30001 calls to the stand-ins, 0 MISMATCHES
    coverage: table entries 64 of 64 (6000 calls); pick pose 1000, ticks 2737 / once 8838, heights 1000, poses 1986,
      tints 350, releases 688, pending 517, rands 170, cues 1332, tasks 328, targets 671, loads 2000, sounds 222,
      actor bits 1000, action ends 1162; idle: flips 163, homes 333, steps 552, colours 95 / 80; tinted 350,
      released 688; rolls under 78 over 92; costs 509; ends keeping the action 500

The run with `BOF3X_SHADOW='*'` exited 0 with `inject: 1046 ours, 0 left original by BOF3X_ORIGINAL` and no mismatch or Fatal anywhere in the log (397 self-test lines, every one 0 MISMATCHES).

**Negative controls.** A script (`controls.py` in the session scratchpad)
planted each one alone in `battle_obj_states.cpp`: replace, build,
self-test, revert. The table gives the rounds that refused each one, of
1,000; exit 3 each (`bof3::Fatal`).

| | Planted | Refused in |
|---|---|---|
| I1 | StateInit: the dwords to +0x1C only | 1000 |
| I2 | StateInit: +0x29 = 5 | 1000 |
| I3 | StateInit: the pose base from 0x904AAD | 980 |
| I4 | StateInit: the elevation arguments swapped | 1000 |
| I5 | StateInit: the height to +0x3C | 1000 |
| I6 | StateInit: state 3 | 1000 |
| I7 | StateInit: +0x48 kept | 997 |
| I8 | StateInit: Sprite_Current not re-read after the call | 525 |
| T1 | StateStand: a tick once | 1000 |
| T2 | StateStand: the state + 2 | 1000 |
| D1 | StateIdle: the sub-state from +3 | 978 |
| D2 | StateIdle: the colour not zeroed | 816 |
| D3 | StateIdle: 0x904AA1 at 1 or more | 70 |
| D4 | StateIdle: the two colours swapped | 184 |
| D5 | StateIdle: members 0x140 apart | 66 |
| D7 | StateIdle: home on bit 14 | 76 |
| D8 | StateIdle: home without the +0x134 test | 80 |
| D9 | StateIdle: home x from 0x903784 | 91 |
| D10 | StateIdle: task slots 0x80 apart | 599 |
| D11 | StateIdle: the step on +0x90 bit 4 | 336 |
| D12 | StateIdle: the direction on +0x130 bit 10 | 261 |
| D13 | StateIdle: the upper end 11 | 21 |
| D14 | StateIdle: the lower end -11 | 21 |
| D15 | StateIdle: the flip not stored | 149 |
| D16 | StateIdle: poses 1 and 2 on x | 199 |
| D17 | StateIdle: the step << 8 | 543 |
| D18 | StateIdle: the step unsigned | 273 |
| D19 | StateIdle: the tick gate bit 13 | 451 |
| D20 | StateIdle: no tick without the step | 88 |
| D21 | StateIdle: a tick once | 649 |
| D22 | StateIdle: Field_State for the tick not re-read (the pointer of before) | **not refused; no input could tell** |
| N1 | IdleTintOn: the gate inverted | 663 |
| N2 | IdleTintOn: all on bit 6 | 188 |
| N3 | IdleTintOn: the tint a = 1 | 350 |
| N4 | IdleTintOn: the record to +6 | 350 |
| N5 | IdleTintOn: sub-state 2 | 350 |
| N6 | IdleTintOn: Sprite_Current not re-read after the call | 179 |
| O1 | IdleTintOff: +5 for +4 | 1000 |
| O2 | IdleTintOff: records 16 apart | 986 |
| O3 | IdleTintOff: kept when the target is another | 321 |
| O4 | IdleTintOff: "all" released | 166 |
| O5 | IdleTintOff: the gate inverted | 650 |
| O6 | IdleTintOff: sub-state 1 after the release | 688 |
| O7 | IdleTintOff: the release of +5 | 672 |
| A1 | StateAttack: the special on bit 1 | 487 |
| A2 | StateAttack: the character +0x88 | 493 |
| A3 | StateAttack: the special as slot 11 of the character table | 498 |
| S1 | AttackStart: the by-kind counts on bit 0 | 490 |
| S2 | AttackStart: the character through the by-kind table | 497 |
| S3 | AttackStart: +0xA left by kind | 502 |
| S4 | AttackStart: the pose + 0x10 | 1000 |
| S5 | AttackStart: a tick once | 1000 |
| S6 | AttackStart: the state on before the tick | 1000 |
| P1 | StateSwing: the sub-state +3 | 985 |
| P2 | StateCast: through the swing table | 1000 |
| W1 | SwingCue: no tick | 1000 |
| W2 | SwingCue: the cue at 1 | 664 |
| W3 | SwingCue: the pending test inverted | 517 |
| W4 | SwingCue: mod 99 | 20 |
| W5 | SwingCue: at or below | 15 |
| W6 | SwingCue: the remainder unsigned | 15 |
| W7 | SwingCue: flag 0x40 | 99 |
| W8 | SwingCue: the miss cue 1 | 38 |
| W9 | SwingCue: no cue 4 | 517 |
| W10 | SwingCue: +0xBA read before Rand | 21 |
| W11 | SwingCue: the state on, not the sub-state | 517 |
| E1 | SwingEnd: the task whatever bit 7 | 343 |
| E2 | SwingEnd: kind and parameter swapped | 328 |
| E3 | SwingEnd: the target 0x904B45 | 669 |
| E4 | SwingEnd: flag 8 | 511 |
| E5 | SwingEnd: no action end | 671 |
| E6 | SwingEnd: the tick gate inverted | 1000 |
| C1 | CastStart: the load gate inverted | 1000 |
| C2 | CastStart: action 3 | 274 |
| C3 | CastStart: 0x1F frames | 32 |
| C4 | CastStart: the sound arguments swapped | 172 |
| C5 | CastStart: kind 6 for 7 | 37 |
| C6 | CastStart: 0x1E on a zero answer by kind too | 14 |
| C7 | CastStart: +9 left for other actions | 122 |
| C8 | CastStart: the skill bit 2 | 336 |
| C9 | CastStart: skill records 16 apart | 328 |
| C10 | CastStart: the character through the by-kind counts | 149 |
| C11 | CastStart: the pose + 0x28 | 324 |
| C12 | CastStart: the skill path moves the sub-state | 334 |
| C13 | CastStart: no tick | 658 |
| C14 | CastStart: Field_State of before the calls for the count | 11 |
| Q1 | CastCue: down by 2 | 782 |
| Q2 | CastCue: cue 6 | 218 |
| Q3 | CastCue: bit 0 | 75 |
| Q4 | CastCue: kind 8 cued | 5 |
| Q5 | CastCue: +0x134 read before cue 5 | 13 |
| Q6 | CastCue: no tick while counting | 782 |
| V1 | CastWait: sub-state 1 | 683 |
| V2 | CastWait: the gate inverted | 1000 |
| X1 | StateCastDone: the end on bit 3 | 509 |
| X2 | StateCastDone: the sub-state +1 | 983 |
| Y1 | CastDoneTick: + 2 | 1000 |
| Z1 | CastDone: the cost from 0x904B89 | 504 |
| Z2 | CastDone: a cost for any action | 480 |
| Z3 | CastDone: flag 0x400 | 758 |
| Z4 | CastDone: the action kept | 983 |
| Z5 | CastDone: the skill bit 10 | 504 |
| Z6 | CastDone: sub-state 3 | 518 |
| Z7 | CastDone: the cost on a byte | 243 |
| U1 | CastDoneWait: the answer to +0xA | 1000 |
| R1 | State12: the sub-state +3 | 983 |
| R2 | State12Pose: the pose + 0x20 | 662 |
| R3 | State12Pose: the gate inverted | 1000 |
| R4 | State12Wait: state 9 | 648 |
| H1 | EndAction: state 3 | 952 |
| H2 | EndAction: +4 kept | 996 |
| H3 | EndAction: the bit of +7 | 981 |
| H4 | EndAction: bit 8 of +0x130 | 764 |
| H5 | EndAction: the action kept on bit 7 | 509 |
| H6 | EndAction: +0x130 cleared before the call | 375 |

**D22** changes nothing: no call separates the two reads of `Field_State`,
so both see the same object. It is not counted.

**Six were thin in the first run** and are refused more now: I8 (41, then
525), N6 (4, then 179), W10 (2, then 21), H6 (32, then 375), Q5 (5, then 13)
and C14 (not refused, then 11). Each needed a stand-in that moves the object
pointer its caller re-reads after the call; the recorders were given that
side effect (section 4 above), and the seeds then copy the fields under test
into every member so the branch is still reached.

**The thinnest now:** Q4 (5), C14 (11), Q5 (13), C6 (14), W5 (15) and W6
(15). W6 plants an unsigned remainder, which only a negative `Rand` shows;
the CRT's `rand` never answers one, so it guards the code, not the game.

**What the fuzz cannot see:**

- anything the callees really do, and what the unowned table entries do;
- the order of stores with no call between them (`StateInit`'s, `EndAction`'s
  four, the colour bytes);
- eax on return (dead, section 2);
- the upper bits of the pushed bytes (the callees read the low byte);
- `StateIdle`'s phase-1 re-test of the phase for 5 (no call separates it from
  the first read, so it is 1).

## 5. For the batch check

The combat route enters all twenty-two (the first-call trace of
[`remaining-catalog.md`](remaining-catalog.md) section 4, all original);
after the merge they should be absent from that trace and counted by
`inject:`. Which of the route's actions puts a member in states 5, 7 and 12
was not traced, so what the A/B shows for each state is the check. Likely
fuzz only (not measured on the route): phase-5 homing
with bit 15, the tint targeting of "all", a special attack (+0x134 bit 0),
the skill-cost path, `0x904AA8` bit 6 at an action's end.

## 6. For `analysis/calltrace/entries_logic.txt`

Added (2026-09-25, a section of its own at the end): the twenty-two with the
sizes of section 1 (`00442BA0 17`; its chunk `0x442C60..0x442C6E` is not
listed). Two older lines overlap them and should be cut by the merger:
`004412B0 105F` (BC's `PickPose`, really 0x260 - [`battle_windows.md`](battle_windows.md)
section 1) and `00442420 B77` (really to `0x44249E`, section 3). The
catalogue's extents that were wrong: `0x441550`'s ran on to `0x441570`
(0x20; really 0x13), `0x442DB0`'s over `0x442DD0` (0x90; really 0x1D), and
`0x442C50`'s over `0x442C60` (0x20; really 0xF).

## 7. Open

- Which commands put a party object in states 5, 7 and 12, and what states
  6, 10, 11 and 26 are.
- What `0x904AA8` bit 7 (set by the swing's roll) and bit 11 (the cast's
  end) mean downstream.
- Whether +0x9A is MP.
