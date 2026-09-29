# Group BE3: the enemy ops round eleven left, the party objects' hit states, two party helpers

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3), wave one, stage B, on the round branch's tip `642def1`. **49 functions
ours** (`src/game/battle_e3.cpp`, shadow name `battle_e3`): the cut table's
47 rows for BE3 (`analysis/round12_cut.tsv`), plus `0x437230` (flagged by
`tools/band_rows.py` as code no list has) and `0x441510` (called, in no start
list - EH's finding, docs/boss_harness.md section 10.7). Each read to its
last instruction with capstone and fuzzed through the boss harness's engine
frame ([`boss_harness.md`](boss_harness.md) section 10) without edits to it:
two `Run`s, 294,000 rounds, 0 mismatches. 93 controls planted one at a time, all refused by a count.
Fuzz-only except the four the dragon route enters (section 9).

| Part | Functions | Reached through |
|---|--:|---|
| The enemy ops: `EnemyOp_Steps` entries 7 (the cast), 8 (its end), 9 (the leave) and their sub-steps | 11 | `EnemyOp_Steps` `0x64B1A0` and the 59 boss step tables' entries 7..9 (read in place), `BossNina_WalkSteps[2]` |
| The enemy action's end, the bit-7 roll, the cue, a fixed-point helper | 4 | direct calls from ours (boss states, `EnemyOp_*`, `BattleActor_PlaySound`, `Paralyzer_Start`, area 198's and Myria's effect) and BE2 |
| The party objects' state 6 (the hit taken) with five sub-trees | 25 | `BattleObj_StateTable[6]` `0x64E07C..` |
| States 8 (sub-state 3), 10, 11 and 26 | 8 | `BattleObj_StateTable` (`0x64DFE0`) slots 10, 11, 26; `BattleObj_CastDoneSubs[3]` |
| Party helpers: the pose `0x441510`, the stat pass `0x442310`, the loss test `0x442420` | 3 | direct calls (ours of this group, BE2's, Capcom's `0x433A50`) |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`); "cast", "leave", "fall", "revive", "special" name the code's
shape, not a play-tested fact - which command or item puts a member in state
6's fall or state 26 was not traced (section 10).

## 1. What each function does

Every function's comment in `battle_e3.cpp` is the full read and each
`symbols.toml` `evidence` string cites its instructions; this section is the
map. SC is `Sprite_Current`, FS `Field_State`.

### 1.1 The enemy ops (Sprite_Current an enemy, 0x939AD8 the enemy run)

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `EnemyOp_CastDispatch` | `0x437030` | 0x12 | step 7: `jmp [EnemyOp_CastSubs + 4 * +2]`, 3 entries (`0x437050`, `0x437120`, BSF's `BossMyria_State7End` `0x4404F0`, shared) |
| `EnemyOp_CastStart` | `0x437050` | 0xC6 | waits for `File_LoadDone`; by `+0x105` 4 in an event battle a sound load sets `+9` to 0 / 0x1E; the ability's byte `+5` bit 3 goes straight to state 8, else `+9` from the enemy data record `+0x8B`, animation 6; tail `BattleEnemy_ScriptTickOnce` |
| `EnemyOp_CastCue` | `0x437120` | 0x59 | `+9` down; at 0 the cue (event battle: `Sound_PlayEffectUnlessNone` of the `+0xF8` table's word `+2`; else `Sound_PlayEffect(0x601 + 2 * +0xF0)`) |
| `EnemyOp_CastDoneDispatch` | `0x437180` | 0x17 | step 8: `call [EnemyOp_CastDoneSubs + 4 * +2]` (BSF's `BossMyria_State8Tick`, `0x4371A0`, `0x437200`), then `EnemyOp_CastDoneCheck` |
| `EnemyOp_CastDoneCost` | `0x4371A0` | 0x5A | the cost `0x904B88` off `+0xA6` for `+0x105` 4; `0x904AA8 |= 0x800`; `+0x105 = 0`; a tick unless the ability's word `+4` bit 11 |
| `EnemyOp_CastDoneTickUnless` | `0x437200` | 0x25 | a tick unless the ability's byte `+5` bit 3; `+9` up by one each frame |
| `EnemyOp_CastDoneCheck` | `0x437230` | 0xF | `0x904AA8` bit 2: `EnemyOp_EndAction` (not in the cut: section 5) |
| `EnemyOp_LeaveDispatch` | `0x437240` | 0x12 | step 9: `jmp [EnemyOp_LeaveSubs + 4 * +2]` (`0x437260`, `0x437320`, `0x4373C0`) |
| `EnemyOp_LeaveStart` | `0x437260` | 0xBC | the velocity `+0x18 = -0x1000` turned by the pose (BE4's `0x4467C0`), the pose flipped, a tint `(-4, -4, -4, 1)`, `+0 |= 0x20`, sound 0x102 |
| `EnemyOp_LeaveStep` | `0x437320` | 0x94 | a 0x10-frame wait, then eight frames of moving and fading the colour by 0x10 |
| `EnemyOp_LeaveEnd` | `0x4373C0` | 0x54 | out of the turn order, `0x904AB3` down (0: `0x904AE8 |= 2`, the win), tint released, window 4's `+3 = 2`, banners cleared, `Effect_Release`. No `Battle_EnemyDefeated`: the enemy leaves the count undefeated |
| `Sound_PlayEffectUnlessNone` | `0x437450` | 0x12 | `Sound_PlayEffect(word)` unless 0xFFFF |
| `EnemyOp_EndAction` | `0x4376A0` | 0x50 | `+1 = 2`, `+2 = 0`, the actor bit cleared, `0x939AD8 +0x110 &= ~0x200`, `+0x105 = 0` unless `0x904AA8` bit 6 (the enemy twin of `BattleObj_EndAction`) |
| `EnemyOp_RollBit80Task` | `0x4376F0` | 0x2B | with `0x939AD8 +0x90` bit 3 and `Rand` bit 0: `0x904AA8 |= 0x80`, `BattleTask_Create(0, 2)` (the pair `BattleObj_SwingCue` / `SwingEnd` make for a member) |
| `Fixed_HighRoundUp` | `0x441090` | 0x1E | `ax` = the high word of a 16.16 value, one more when the sign word is not negative and the low word is not 0 |

The step tables (`EnemyOp_CastSubs`, `_CastDoneSubs`, `_LeaveSubs`) hold two
of BSF's Myria functions: round eleven named them for the kind that reached
them first, but they are the generic steps (`BossMyria_State7End` is "tick,
then `+1` up", `BossMyria_State8Tick` "tick, `+2` up"). Their names are
BSF's to change.

### 1.2 The party objects' state 6: the hit taken (Sprite_Current a member)

`BattleObj_StateHit` (`0x441A10`) dispatches by `+2` through
`BattleObj_HitSubs` (6): the entry twice, then four sub-trees each
dispatched by `+3` (the fall's task by `+4`):

| `+2` | Sub-tree | Functions |
|--:|---|---|
| 0, 2 | the entry | `BattleObj_HitEnter` `0x441A30`: FS `+0x12F` / `+0x140` from SC `+0x4B` / `+0x58`, `+4 = 0`, `+2 = 1` |
| 1 | the hit received, `BattleObj_HitReceiveSubs` (3) | `BattleObj_HitReceiveDispatch` `0x441A70`; `BattleObj_HitReceive` `0x441A90` (0x2B1 bytes: the result record `0x904B60 = FS + 0x124`, 32 stat bytes to `0x939F80`, the evade byte `0x939F9B` +25 to 100, damage by kind 1 (`Battle_ApplyDamage`) or 4 / 5 (`Effect_ApplyResult`), the pose `BattleObj_HitPose`, the two popups, the KO (`+0x90 = 0x4000`, or `+0x91 |= 0x40` under `+0x134` bit 1), the hit sound and popup); `BattleObj_HitWaitPose` `0x441D50`; `BattleObj_HitEnd` `0x441D80` (the round flag `0x40` roll through BE4's `0x446810`, the loss by `BattleParty_AllDown`, back to state 2 - or state 6's fall for `+0x91` bit 6) |
| 3 | a step out and back, `BattleObj_HitStepSubs` (5) | `_HitStepDispatch` `0x441EB0`; `_HitStepStart` `0x441ED0` (velocity `-0x2000` turned by BE4's `0x446770`, sound 0x205); `_HitStepOut` `0x441F50` (four frames out); `_HitStepBack` `0x441FA0` (four back, then a popup or `BattleTask_Create(0, 1)`); `_HitStepPose` `0x442050`; then `BattleObj_HitEnd` |
| 4 | the fall, `BattleObj_FallSubs` (7) | `BattleObj_FallDispatch` `0x442080`; `BattleObj_Fall` `0x4420A0` (0x26F bytes, by FS: `+0x130` bit 2 - the revive; `+0x134` bit 1 - the task below; `+0x96` / `+0x97` at 0x18 or `+0x95` at 0x43 - the character record's byte zeroed and the revive by it; else the member counted out of `0x904AB1`, its status bytes cleared, BE6's `0x453300`, the loss when `BattleParty_AllDown` or `0x904AB1` reaches 0); `BattleObj_Revive` `0x4424A0` (HP 1, a task, a banner); `BattleObj_ReviveEnd` `0x4425A0`; `BattleObj_ReviveByEquip` `0x442600` (HP to `+0xA0`, `+0x95` also recalculates the party through `BattleParty_RecalcStats`, message 0x2E or 0x3B); `BattleObj_FallTaskDispatch` `0x442740` by `+4` over `BattleObj_FallTaskSubs` (2): `_FallTaskStart` `0x442760` (one member at a time: `+0x130` bit 13 and task 0xB for character 4, else 0xC), `_FallTaskWait` `0x442800` |
| 5 | an HP change, `BattleObj_HpSubs` (3) | `BattleObj_HpDispatch` `0x442870`; `BattleObj_HpApply` `0x442890` (a KO at HP <= the s16 change, else the change taken and HP capped at `+0xA0`); `BattleObj_HitWaitPose`; `BattleObj_HpEnd` `0x442980` |

"Fall" and "revive" are the code's shape (`+0x90 = 0x4000`, HP 0, then HP 1
or `+0xA0` with a message); which items the bytes 0x18 / 0x43 in `+0x95..+0x97`
are was not traced.

### 1.3 States 8 (sub-state 3), 10, 11, 26 and the helpers

| Function | Entry | Bytes | What |
|---|---|--:|---|
| `BattleObj_CastDoneScript` | `0x442C40` | 0x5 | `BattleObj_CastDoneSubs[3]`: `jmp BattleObj_ScriptTick` |
| `BattleObj_State10` | `0x442C70` | 0x12 | by `+2` over `BattleObj_State10Subs` (2) |
| `BattleObj_State10Task` | `0x442C90` | 0x78 | `BattleTask_Create(0, 5)` placed at SC's `+0x2E` / `+0x30` less the s8 pair `0x6699A0[2 * character]` |
| `BattleObj_State10End` | `0x442D10` | 0x14 | `+1 = 2`, `+2 = 0` |
| `BattleObj_State11` | `0x442D30` | 0x23 | `BattleObj_HitPose`, then a tick by FS `+0x130` bits 4 / 5 |
| `BattleObj_StateSpecial` | `0x442E40` | 0x12 | slot 26 (state 4's jump under `+0x134` bit 0 and the character table's twelfth): by `+2` over `BattleObj_SpecialSubs` (3) |
| `BattleObj_SpecialStart` | `0x442E60` | 0xB0 | `BattleTask_Create(0, 0xF)` seeded with 0x80 bytes of the acting member `0x904B34`, SC `+0 |= 0x40`, `+9 = +0xA = 0x10` |
| `BattleObj_SpecialCue` | `0x442F10` | 0x6D | `BattleObj_SwingCue` without its leading tick (the sibling's `Battle_SwingCue_Step2` of the PSX twin says the same) |
| `BattleObj_SpecialWait` | `0x442F80` | 0x17 | once the task clears SC `+0` bit 6: `0x904AA8 |= 4`, `BattleObj_EndAction` |
| `BattleObj_HitPose` | `0x441510` | 0x37 | pose `+8 + 0x34` under FS `+0x91` bit 3, else `+8 + 0x10` (not in the cut: section 5) |
| `BattleParty_RecalcStats` | `0x442310` | 0x10E | `Char_RecalcStats(FS's record)`; each member's `+0x92..+0x97` and 32 bytes `+0xC0` from its character record; `Formation_ApplyStatMods`; `+0xC0` to `+0xA0`; BE5's `0x44FDE0`; BE6's `0x453300(member)` for each |
| `BattleParty_AllDown` | `0x442420` | 0x7F | `al` 0 when any in-use member is up (the tests `BattleObj_Fall` revives from included), else 1 |

### 1.4 The PSX twins

`analysis/pairs_propagated.json` pairs 45 of the 49 (none for `0x437230`, `0x4376F0`, `0x441090`, `0x441510`) (tiers table-anchored,
gap31, gap15, gap34, call-anchored, callers); each twin's address is in its
`symbols.toml` evidence. The sibling names three of them: `0x801DFC68`
`Battle_ResolveAction_Party` (hypothesis there; the PC `0x441A90` does what
that description says - a stat copy, the evade +25 under a flag, damage, the
KO - so the name is cited, and ours is named `BattleObj_HitReceive` for its
place in the table), `0x801E0400` `Defend_Action` (a callstack-venn
hypothesis there for `0x441EB0`; nothing in the PC code reads a defend
command, so it is not adopted), `0x801E1FA8` `Battle_SwingCue_Step2`
(decompiled there; `0x442F10` agrees). No PSX code was read this round.

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed. Where the original indexes past a table ours aborts with a `Fatal`
naming the function (the owner's rule, round9 doc section 6; nothing
reaches it): the eleven dispatchers past their tables, the six task spawns on
`BattleTask_Create`'s 0xFF, the two ObjTrio loops (`BattleParty_RecalcStats`,
`BattleObj_FallTaskStart`) on a party count `0x904AB0` past 3, and
`BattleObj_SpecialStart` on an actor `0x904B34` past 2. Two reads by a
character or enemy byte stay unchecked, as in the rest of our battle code
(`BattleObj_AttackStart`'s `0x64E04C[+0x89]`): `CharacterRecords[+0x148]` and
`0x6699A0[2 * +0x89]`, and the enemy data record by `+0xF0`.

`eax` on return: the functions answering in `al` answer it
(`EnemyOp_CastStart` / `_CastCue` the tick's, `BattleParty_AllDown`,
`Fixed_HighRoundUp` in `ax` with the sign's high half); the enemy
dispatchers forward the caller's word and answer the entry's `eax` (the
kDispatch contract); every other is `void` - its callers drop `eax`
(`EnemyOp_StepDispatch` and `BattleObj_RunState` are ours and `void`,
[`battle_obj_states.md`](battle_obj_states.md) section 2).

## 3. The arguments pushed with leftovers

Where the original pushes a byte or word in a register whose upper bytes are
a callee's or the caller's leftovers, ours passes the value and the fuzz
lists the callee with the mask its code reads (each read, capstone):
`Battle_LoadSoundByKey` (two bytes), `Battle_SetDamagePopup` and BE6's
`0x453EB0` (a word, then `cmp al, 2` on the actor), `Battle_ReturnQueuedItem`
(`cmp al, 2`, then bytes), BE6's `0x453300` (a byte; its `0x453560` masks the
dword it is handed), BE4's `0x44A910` / `0x44AA90` (`and eax, 0xFF`). The
one exception is `Battle_ApplyDamage`'s target in `BattleObj_HitReceive`:
the register held `Field_State`, so its upper bytes are that pointer's, and
ours passes them so (the harness compares the whole word; control O7).

## 4. The fuzz (`battle_e3_fuzz.cpp`)

Two `Run`s under `BOF3X_SHADOW=battle_e3` (`BOF3X_BE3_RUN=EO` or `OBJ` runs
one), both `Group::engine`, 6,000 rounds a function:

- **EO** (15 functions): the three enemy dispatchers `kDispatch` (`+2`
  drawn below 3), the states `kState` (`ret_mask 0xFF` for the two that
  answer the tick), the action's end, the roll, the cue and the fixed-point
  helper `kCallee` (enemy frame; `ret_mask 0xFFFF` for the last);
  `DataTable`s `EnemyOp_CastSubs`, `_CastDoneSubs`, `_LeaveSubs`.
- **OBJ** (34): every party-object function `kMember` (the eight
  dispatchers' byte drawn below their tables: `+2` 6, `+3` 3 / 5 / 7 / 3,
  `+4` 2, `+2` 2 / 3), the three helpers `kHelper` (`ret_mask 0xFF` for
  `BattleParty_AllDown`); the eight state-6, 10 and 26 tables as `DataTable`s.

**Callees beyond the standard sets:** the group's own called directly, by
name (`Sound_PlayEffectUnlessNone`, `EnemyOp_CastDoneCheck`,
`EnemyOp_EndAction`, `BattleObj_HitPose`, `BattleParty_RecalcStats` -
`kPhase`, logging the sprite they ran for - and `BattleParty_AllDown`,
`kFlag`); the other groups' by raw address (section 7); the three narrower
standard listings of section 3. `Battle_ApplyDamage` and `Effect_ApplyResult` are listed
louder: they fill the result record `0x904B60` names (`+4`, `+6`, the flag
byte `+8` - for `BattleObj_HitReceive` that is `Field_State +0x128..+0x12C`,
which it zeroes before the call) with drawn values after noting what the
caller left, and `Battle_ApplyDamage` moves `Field_State` half the time (section
6 says why). The harness's own rows for `0x437230` and
`0x441510` (by address, `kEngineStandard`) are shadowed by this group's named
rows, which register first.

**Regions beyond the engine frame:** the first 256 ability records
`0x65C4D8..` (their flag bits drawn by the fill; `0x904B80` seeded below
256) and `CharacterRecords` past the engine region to ten records.

**Seeds:** the ability id; the enemy's `+0x105` at 4 half the time, its
data record `+0xF0` inside the eight (and `SettleEO` keeps it there after the
harness's case 11), the fight byte 0 a third of the time, the counters `+9` /
`+0xA` at their ends, `0x904AB3` at 1 and 0, the round flags' bits 2 and 6,
the enemy's `+0x90` bit 3, the cue word 0xFFFF half the time, the helper's
sign below / at / above 0; for the members every `+0x148` below 10, the party
count 0..3, and per function the branch bytes on the member `Field_State`
names and, two times in three, on all three (so that a disturbance moving
`Field_State` still reaches the branch): the action kind 1 / 4 / 5 / other,
`+0x12C` bits, a signed change against the HP it meets (equal, one either
side, 0, negative, the s16 ends; HP 0 under a positive change a third of the
time), the max HP one either side of the result, `+0x130` / `+0x131` /
`+0x134` bits, the equipment bytes at 0x18 / 0x43 and one off, `0x904AB1`
at 1 / 0, `0x904B8A` the member's actor half the time, `0x904AE9` 0 half the
time, the fall's step byte 3 / 5 / 6 / other, the percentage `+0xBA` at
0 / 1 / 50 / 99 / 100 / 255, the evade byte 74..76, SC `+0` bit 6, and for
`BattleParty_AllDown` each member's in-use bit, `+0x90` at 0x4000 / 4 /
0x4004 / 0 / 0x8000, and the three revive bytes.

**Disturbance** (the group's case 14, from the hash only): EO moves the
ability id (inside the region), the enemy's `+0x105` and `+0x90`, the fight
byte, `0x904AB3`, the cost, the round flags' bits 2 / 6; OBJ moves a
member's `+0x90` / `+0x91` / `+0x12C` / `+0x130` / `+0x131` / `+0x134` bits,
`+0x128..+0x12B`, a revive byte, the action kind, the round flags,
`0x904AE9`, `0x904AB1`, `0x904B8A`, the ability id, the party count (0..3).

Result (this worktree, 2026-09-29), `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=battle_e3`, exit 0:

    battle_e3 self-test: 90000 rounds over 15 functions (6000 each), 131207 calls to the stand-ins, 0 MISMATCHES; 43556 bytes of state (29 regions) and the stand-ins' log compared
    battle_e3 self-test: 204000 rounds over 34 functions (6000 each), 303055 calls to the stand-ins, 0 MISMATCHES; 43556 bytes of state (29 regions) and the stand-ins' log compared

Every recorder of both runs was called (the coverage lines), every table
entry included.

`BOF3X_SHADOW='*'` at the final commit (this worktree, 2026-09-29): exit 0,
951 self-test lines every one `0 MISMATCHES`, no Fatal, `inject: 6286 ours, 0
left original by BOF3X_ORIGINAL` (11 minutes; the earlier `'*'` run at the
first commit passed alike). It did not die silently either time.

## 5. What the cut and the tool said, settled

- **`0x437200` is 0x25 bytes**, not the cut's 64: its padding ends at
  `0x437230`, a function of its own (0xF bytes: `0x904AA8` bit 2, then
  `jmp 0x4376A0`) reached only by `0x437180`'s tail jump. Taken as
  `EnemyOp_CastDoneCheck`.
- **`0x441510`** (0x37 bytes, after `BattleObj_PickPose`'s switch table at
  `0x4414F8`) is called by `0x441A90` and `0x442D30`; nothing else reaches
  it. Taken as `BattleObj_HitPose`. `BattleObj_PickPose` (ours) does not
  contain it (its case table's six entries end at `0x4414E0`).
- **Hidden in hosts that are ours:** ten starts in `BattleEnemy_Chance70`'s
  (`0x436B50`) catalogue extent and thirteen in `BattleObj_PickPose`'s
  (`0x4412B0`): neither host's source contains their code (both hosts end
  long before, [`enemy_ai_ops.md`](enemy_ai_ops.md), `battle_windows.cpp`),
  and each is reached by address (a `.data` table or a call).
- **`0x442420` is 0x7F bytes** (the loss test), not the catalogue's 0x5C0;
  the eighteen handlers after it were in its extent (41 hidden starts in all, the cut's count).
- The tool's extents are right for all 49; 39 cut sizes differ by padding
  only, `0x437200`'s by code.

## 6. Controls

A script (`controls.py` in the session scratchpad) planted each one alone in
`battle_e3.cpp` - anchored on a string that must occur once - rebuilt,
self-tested with `BOF3X_BE3_RUN` set to its run, restored, and rebuilt at the
end. **All 93 refused by a count** (exit 3, the rounds of the function's
6,000 that mismatched), none by a Fatal alone:

| | Function | Planted | Refused in |
|---|---|---|--:|
| E1 | `EnemyOp_CastDispatch` | by +3, not +2 | 4,057 |
| E2 | `EnemyOp_CastStart` | 0x1F frames | 273 |
| E3 | `EnemyOp_CastStart` | the skill bit 2 | 2,432 |
| E4 | `EnemyOp_CastStart` | 0x939AD8 not read again after the sound load | 23 |
| E5 | `EnemyOp_CastCue` | sound 0x600 + 2n | 1,114 |
| E6 | `EnemyOp_CastCue` | the table word +4 | 2,074 |
| E7 | `EnemyOp_CastDoneDispatch` | no check after the step | 6,000 |
| E8 | `EnemyOp_CastDoneCost` | flag 0x400 | 4,505 |
| E9 | `EnemyOp_CastDoneCost` | the cost added | 2,503 |
| E10 | `EnemyOp_CastDoneTickUnless` | +9 up by 2 | 6,000 |
| E11 | `EnemyOp_CastDoneCheck` | bit 3 | 2,992 |
| E12 | `EnemyOp_LeaveDispatch` | by +3 | 3,975 |
| E13 | `EnemyOp_LeaveStart` | velocity -0x2000 | 5,998 |
| E14 | `EnemyOp_LeaveStart` | tint a = 0 | 6,000 |
| E15 | `EnemyOp_LeaveStep` | red - 0xF | 393 |
| E16 | `EnemyOp_LeaveStep` | vy += +0x18 | 393 |
| E17 | `EnemyOp_LeaveEnd` | the loss bit | 988 |
| E18 | `EnemyOp_LeaveEnd` | window 4 +3 = 3 | 5,998 |
| E19 | `Sound_PlayEffectUnlessNone` | the low byte | 1,018 |
| E20 | `EnemyOp_EndAction` | bit 5 | 2,707 |
| E21 | `EnemyOp_EndAction` | +0x110 bit 8 | 4,519 |
| E22 | `EnemyOp_RollBit80Task` | Rand bit 1 | 1,258 |
| E23 | `EnemyOp_RollBit80Task` | task (0, 3) | 1,482 |
| E24 | `Fixed_HighRoundUp` | sign 0 as negative | 210 |
| O1 | `BattleObj_StateHit` | by +3 | 4,382 |
| O2 | `BattleObj_HitEnter` | +2 = 2 | 6,000 |
| O3 | `BattleObj_HitEnter` | word +0x5A | 5,999 |
| O4 | `BattleObj_HitReceiveDispatch` | by +2 | 3,931 |
| O5 | `BattleObj_HitReceive` | the evade cap 99 | 2,116 |
| O6 | `BattleObj_HitReceive` | +0x90 bit 10 | 2,960 |
| O7 | `BattleObj_HitReceive` | the target without Field_State's upper bytes | 775 |
| O8 | `BattleObj_HitReceive` | Effect_ApplyResult for kind 6 | 409 |
| O9 | `BattleObj_HitReceive` | the early end on +0x12C bit 1 | 284 |
| O69 | `BattleObj_HitReceive` | the result flags byte = 1 | 6,000 |
| O10 | `BattleObj_HitReceive` | item class bit 3 | 117 |
| O11 | `BattleObj_HitReceive` | +0x90 = 0x4001 | 164 |
| O12 | `BattleObj_HitReceive` | the sound at 0 too | 3,773 |
| O13 | `BattleObj_HitReceive` | bit 6 cleared, not 7 | 2,115 |
| O14 | `BattleObj_HitReceive` | Field_State not read again after Battle_ApplyDamage | 142 |
| O15 | `BattleObj_HitWaitPose` | +0x90 bit 3 | 1,985 |
| O16 | `BattleObj_HitEnd` | state 7 | 1,542 |
| O17 | `BattleObj_HitEnd` | flag 0x2000 | 1,679 |
| O18 | `BattleObj_HitEnd` | the target bit 7 only | 37 |
| O19 | `BattleObj_HitEnd` | the ability bit 5 | 76 |
| O20 | `BattleObj_HitEnd` | the win bit | 2,226 |
| O21 | `BattleObj_HitStepDispatch` | by +2 | 4,762 |
| O22 | `BattleObj_HitStepStart` | velocity -0x1000 | 2,955 |
| O23 | `BattleObj_HitStepStart` | sound 0x206 | 2,955 |
| O24 | `BattleObj_HitStepOut` | +0xA = 5 | 1,008 |
| O25 | `BattleObj_HitStepBack` | the task +7 = 2 | 1,675 |
| O26 | `BattleObj_HitStepBack` | +0x12C bit 5 | 1,619 |
| O27 | `BattleObj_HitStepPose` | +0x90 bit 1 | 3,000 |
| O28 | `BattleObj_FallDispatch` | by +2 | 5,149 |
| O29 | `BattleObj_Fall` | +0x96 path to +3 = 4 | 1,444 |
| O30 | `BattleObj_Fall` | +0x95 at 0x42 | 2,119 |
| O31 | `BattleObj_Fall` | +0x134 bit 8 kept | 585 |
| O32 | `BattleObj_Fall` | the loss at 1 up | 84 |
| O33 | `BattleObj_Fall` | bit 14 against +4 | 445 |
| O34 | `BattleObj_Fall` | +0x134 bit 1 at the end | 591 |
| O35 | `BattleParty_RecalcStats` | +0x97 not copied | 5,504 |
| O36 | `BattleParty_RecalcStats` | 28 bytes to +0xA0 | 5,500 |
| O37 | `BattleParty_RecalcStats` | 0x453300 of the next member | 5,499 |
| O38 | `BattleParty_AllDown` | +0x90 bit 2 not counted | 308 |
| O39 | `BattleParty_AllDown` | +0x95 at 0x42 | 417 |
| O40 | `BattleObj_Revive` | HP 2 | 4,311 |
| O41 | `BattleObj_Revive` | the task +7 = 0 | 4,311 |
| O42 | `BattleObj_Revive` | the banner pair (1, n) | 2,179 |
| O43 | `BattleObj_ReviveEnd` | +0x130 bit 4 kept | 3,009 |
| O44 | `BattleObj_ReviveByEquip` | message 0x2F | 462 |
| O45 | `BattleObj_ReviveByEquip` | HP from +0xA2 | 4,354 |
| O46 | `BattleObj_ReviveByEquip` | +0x96 for step 6 | 1,144 |
| O47 | `BattleObj_FallTaskDispatch` | the table one entry on | 6,000 |
| O48 | `BattleObj_FallTaskStart` | task 0xD | 1,500 |
| O49 | `BattleObj_FallTaskStart` | bit 12 | 2,561 |
| O50 | `BattleObj_FallTaskWait` | +4 = 1 | 2,927 |
| O51 | `BattleObj_HpDispatch` | by +2 | 4,060 |
| O52 | `BattleObj_HpApply` | the fall below, not at | 544 |
| O53 | `BattleObj_HpApply` | the cap one above | 915 |
| O54 | `BattleObj_HpEnd` | state 4 | 3,032 |
| O55 | `BattleObj_State10` | through state 6's table | 6,000 |
| O56 | `BattleObj_State10Task` | y from the x byte | 3,969 |
| O57 | `BattleObj_State10Task` | task (0, 6) | 6,000 |
| O58 | `BattleObj_State10End` | state 3 | 6,000 |
| O59 | `BattleObj_State11` | bit 5 for the tick | 2,948 |
| O60 | `BattleObj_StateSpecial` | by +3 | 4,069 |
| O61 | `BattleObj_SpecialStart` | the slot +5 = 0xE | 6,000 |
| O62 | `BattleObj_SpecialStart` | 0x7C bytes copied | 6,000 |
| O63 | `BattleObj_SpecialStart` | +0xB = slot + 1 | 6,000 |
| O64 | `BattleObj_SpecialCue` | at or below | 15 |
| O65 | `BattleObj_SpecialCue` | cue 5 | 4,401 |
| O66 | `BattleObj_SpecialWait` | flag 8 | 2,194 |
| O67 | `BattleObj_HitPose` | pose + 0x35 | 2,957 |
| O68 | `BattleObj_CastDoneScript` | the tick once | 6,000 |

**Thin, and why.** E4 (0x939AD8 not re-read after the sound load) is reached only
when the load is made (kind 4 in an event battle) and the disturbance then moves
0x939AD8; O18 (the target's bit 6 dropped from the test) needs a target byte
with bit 6 and not 7 on the kind 1 / 4 path; O64 (at or below the percentage)
needs `Rand() % 100` equal to `+0xBA`, which the harness's `Rand` hint hits a
third of the time for a percentage below 100 only.

**Found by the controls, fixed in the fuzz before the table above.** The first
run left O9 (the early end on `+0x12C` bit 1) and O53 (the HP cap one above)
unrefused, and a crash replaced O55's count. O9: `BattleObj_HitReceive` zeroes
the result record `Field_State + 0x124`'s `+4`, `+6` and `+8` before the damage
call - that is `+0x128`, `+0x12A`, `+0x12C` - so the popup bits and the
change are only ever what `Battle_ApplyDamage` / `Effect_ApplyResult` write; the
harness's quiet stand-ins wrote nothing, and the branches after were reached
only through the disturbance. Both are now listed with an effect that fills the
record (noting what the caller left first), and `Battle_ApplyDamage`'s moves
`Field_State` half the time (O14, the re-read after it, went from 1 round to
the count above). O53: the seed put the max HP one *above* the result, not one
below. O55's first plant (the table one entry on) landed on
`BattleObj_State12Subs[0]`, which no recorder stands in for, so Capcom's code ran
on one side and faulted; replaced by the dispatch through state 6's table.
Three multi-line anchors missed on the first run (the file's CRLF); the
script now matches either.

## 7. Calls across groups

**Out of BE3, raw** (`battle_e3_callees.h`, a recorder each in the fuzz; the
coordinator rebinds after the owner merges):

| Callee | Owner | Called from |
|---|---|---|
| `0x446770` | BE4 | `BattleObj_HitStepStart` |
| `0x4467C0` | BE4 | `EnemyOp_LeaveStart` |
| `0x446810` | BE4 | `BattleObj_HitEnd` (twice) |
| `0x44A910`, `0x44AA90` | BE4 | `BattleObj_Revive` |
| `0x44FDE0` | BE5 | `BattleParty_RecalcStats` |
| `0x453300` | BE6 | `BattleObj_Fall`, `BattleParty_RecalcStats` |
| `0x453EB0` | BE6 | `BattleObj_HitReceive`, `BattleObj_HpApply` |
| `0x591810` | nobody (standard set) | `BattleObj_HitReceive` |

**Into BE3 from outside the group:**

| Function | Callers |
|---|---|
| `Sound_PlayEffectUnlessNone` `0x437450` | ours: `EnemyOp_ReceiveAction`, `EnemyOp_DeathFlash` (enemy_ai_ops), `BattleActor_PlaySound` (battle_items), `BossZig_Step5Count` (boss_se), `BossGazer_State4Wait`, `BossMyria_State4Wait` (boss_sf), `BossWeretigr_State4Cue` (boss_sa), `BossAngler_Advance` (boss_sh), `Paralyzer_Start` (magic_engine); BE2's `0x436640` |
| `EnemyOp_EndAction` `0x4376A0` | ours: `BossZig_Step5Fire`, `BossGazer_State5Close`, `BossWeretigr_State4End`, `BossAngler_RetreatEnd`, `BossArwan_State5Await`, `BossMyria_State8Close`, `BossMyria_State8Check`; BE2's `0x4366B0` |
| `EnemyOp_RollBit80Task` `0x4376F0` | ours: `BossGazer_State5Close`, `BossWeretigr_State4End`, `BossAngler_Advance`, `BossArwan_State5Await`; BE2's `0x4366B0` |
| `Fixed_HighRoundUp` `0x441090` | ours: `Area198_EffectA6Follow` (area_w4f), `BossMyriaFx_Follow` (boss_sj) |
| `BattleParty_RecalcStats` `0x442310` | BE2's `0x433DA0`, `0x434340`; Capcom's `0x433A50` (in no group) |
| the step and state tables | read in place by ours: `EnemyOp_StepDispatch` (`EnemyOp_Steps` 7..9), the boss kinds' dispatchers (their step tables' entries 7..9), `BossNina_WalkStep` (`BossNina_WalkSteps[2]`), `BattleObj_RunState` (`BattleObj_StateTable` 6, 10, 11, 26), `BattleObj_StateAttack` (`0x64E048`), `BattleObj_StateCastDone` (`BattleObj_CastDoneSubs[3]`) - no rebinding needed |

**For the coordinator:** BE2 calls `0x442310`, `0x437450`, `0x4376A0`,
`0x4376F0` raw; after both merge those become names.

## 8. The rebinding

**Rebound** (the round-ten form: the same value, so every fuzz key stands):
18 constants in 10 files - `battle_items_callees.h` `kEnemySound`,
`boss_sa_callees.h` `kPlayCue` / `kTurnClose` / `kTurnChance`,
`boss_se_callees.h` `kEnemySound` / `kEnemyActEnd`, `boss_sf_callees.h` and
`boss_sh_callees.h` `kEnemySound` / `kEnemyActEnd` / `kEnemyActChance`,
`boss_si_callees.h` `kTurnClose` / `kTurnChance`, `boss_sj_callees.h` and
`area_w4f_callees.h` `kRoundHigh`, `enemy_ai_ops_callees.h` and
`magic_engine.cpp` `kPlayCue` - each now `bof3::addr::<BE3's name>`.

**Left raw, on purpose:** the fuzz files' `CallSite` tables and callee rows
(`boss_sa/se/sf/sh/si/sj_fuzz.cpp`, `area_w4f_fuzz.cpp`,
`magic_engine_fuzz.cpp`, `battle_items_fuzz.cpp`: the keys, the round-ten
rule); comments naming the addresses (`boss_sg_callees.h`, `boss_sc.cpp`,
`magic_engine.cpp`, `battle_obj_states.cpp`, the boss `.cpp`s' reads);
**`boss_harness.cpp`'s two `kEngineStandard` rows `"0x437230"` and
`"0x441510"`** (the harness is not a group's to edit; this group's named rows
register first and stand, so the rows only matter to a group that calls those
two raw - none now; the coordinator's fold may drop them); and
**`boss_harness_eh.cpp`'s copies of `0x441A10` / `0x441A30`** (the harness's
self-test clones Capcom's bytes; it runs before `BattleE3_Inject`, which is
placed after `BossHarnessEh_Inject` in `inject_all.cpp` for that reason, and
must stay raw).

## 9. The live route

`tools/recipes/dragonTransform.txt`'s first-call trace
(`analysis/calltrace/reach_dragon/reach_dragon_new.txt`) enters four of the
49: `EnemyOp_RollBit80Task` and `EnemyOp_EndAction` (frame 1,747),
`BattleParty_AllDown` (1,769, from `BattleObj_PickPose`'s catalogue extent -
that is, `BattleObj_HitEnd` or `BattleObj_Fall`) and `BattleParty_RecalcStats`
(2,731, from BE2's `0x433D60` extent). The trace armed entries at the
catalogue's granularity, so hidden handlers inside a host's extent show only
through their callees: `0x453EB0 <- 0x00442420` at frame 2,526 names a call
inside `0x442420`'s 0x5C0 extent, where only `BattleObj_HpApply` calls
`0x453EB0` - so state 6's HP change was very likely entered too (an
inference). The rest are fuzz-only until the coordinator's live check.

## 10. Latent defects (Capcom's, described, not fixed)

- **Unchecked indexes**, as everywhere in the engine: the eleven dispatchers,
  the six `BattleTask_Create` answers written through (`BattleObj_HitStepBack`,
  `_Revive`, `_ReviveByEquip`, `_FallTaskStart`, `_State10Task`,
  `_SpecialStart`: the D163 class), the party count loops and the special
  attack's actor. Ours aborts on each (section 2).
- **`EnemyOp_CastCue` reads `0x939AD8 +0xF8` as a pointer in any event
  battle** (`0x904AAA` set): `EnemyOp_Begin` stores `+0xFC` but not `+0xF8`,
  so a non-boss enemy casting in an event battle follows whatever its `+0xF8`
  holds - the Paralyzer row's pattern (round9 doc) and
  `EnemyOp_ReceiveAction`'s. A crash candidate only where an event battle
  has an ordinary caster.
- **`BattleObj_Fall` zeroes a byte of `CharacterRecords[+0x148]`** with the
  character byte unchecked (the revive-by-equipment paths); a member whose
  `+0x148` is past the records would write outside them.
- **`EnemyOp_CastStart` keeps `+9` for an action-4 cast outside an event
  battle**; `+9` is then overwritten on the non-bit-3 path and not read on
  the bit-3 path, so nothing shows.
- **`EnemyOp_CastDoneTickUnless` increments `+9` every frame** until the
  action ends; nothing in the three steps reads it after.

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29): 43 lines (the 49 but
`00437450 12`, `004376A0 50`, `00441090 1E`, `00442310 10E`, already exact,
`004376F0 30` - 0x2B of code and padding - and `00442420`, whose listed
`5C0` is really 0x7F; that line is the merger's to fix). The hidden starts'
lines cut the hosts `00436B50`, `004412B0 105F` and `00442420 5C0`.
