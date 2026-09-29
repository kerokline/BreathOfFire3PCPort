# Group BE2: the action tasks, the enemy's action pick, the enemy ops (`0x433650..0x437030`)

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section 3),
wave one, stage B, on the harness group EH widened
([`boss_harness.md`](boss_harness.md) section 10). All 48 functions the cut
table `analysis/round12_cut.tsv` lists for BE2 are ours
(`src/game/battle_e2.cpp`, shadow name `battle_e2`), each read to its last
instruction with capstone and fuzzed through `boss_harness` as an engine group
without edits to it: three `Run`s, 288,000 rounds, 0 mismatches. 61 controls
planted, all refused by a count (section 6). `tools/ledger_check.py`: 0
errors. Every function is a faithful replacement: no `DIVERGENCE.md` entry is
owed. The live route `dragonTransform.txt` enters four of them (section 10);
the rest are fuzz-only.

`tools/band_rows.py --group BE2` (2026-09-29) reads the 48 starts and flags
nothing: no start of the cut is a non-function, no code between them is
missing from the list, 0 extents differ from the cut by code (33 by trailing
padding only - the extents below are the tool's, which the reading
confirmed). 36 are hidden starts: 16 inside the catalogue extents of
functions that are ours (`BattleFx_RollingDigits` `0x432F10` 1,
`BattleEnemy_ScriptTickOnce` `0x4360C0` 7, `BattleEnemy_Chance70`
`0x436B50` 8), 20 inside two of this group's own (`0x433D60` 4, `0x434870`
16). **None of the host functions already held the code**: each host's own
source was read (`battle_fx_tasks.cpp`, `battle_flow.cpp`) - the first ends
at `0x433190` (`BattleFx_PoseTask` follows), the other two are 0x29 and 0x6F
bytes - the catalogue cut them at the next *listed* start, and these starts
were not listed. Each hidden start is reached by address (a stack table's
immediate or a `.data` cell, section 1), so each is taken as its own function.

## 1. What each function does

Names are from what the code does and who calls it; a PSX twin from
`analysis/pairs_propagated.json` is cited as a hypothesis in the evidence
string and was not read, except `0x436640`, whose sibling name
(`Battle_PlayCreatureCue`, `names/functions.toml`, PSX `0x801E39BC`) was
verified by this reading and transferred. No gameplay fact is stated from
memory: "Accession's single-member form" is `magic_s33.cpp`'s reading of
`0x904AA8` bit 15 and `0x904B8F`, "the formation" is `0x904060` as
`inventory_ops_callees.h` names it.

### 1.1 The effect tasks (`BattleFx_Dispatch` slots and the actor watch)

`BattleTask_RunAll` runs a kind-0 slot through `BattleFx_Dispatch`'s
19-entry stack table by the slot's `+5` ([`battle_fx_tasks.md`](battle_fx_tasks.md)).
Slots 9, 10, 13, 14 and 17 are this group's; slot 12 is `0x433B80`
(Capcom's, in no group), whose stack table holds this group's two states.

| Function | Entry | Bytes | Reached from | What (by the code) |
|---|---|--:|---|---|
| `BattleFx_WatchIcon` | `0x433650` | 0x140 | `BattleFx_ActorWatch`'s stack table, state 3 (imm `0x433482`) | with the round flags' `0x400`: state + 1 when the owner's actor (`0x93B940` `+5`) is `0x904B34`, or the kind `0x904B35` is 4 with the ability `0x97` / `0xE1`. Else `+0xA` counts down; at 0 `BattleFx_NextStatusIcon` (0xFF frees the slot; else `+0x27` 0xFF, `+0xA` 0x3C, a new icon's animation `0x64B048[i]` and `+0xB` = i); state 0 once the owner's status lacks `0x58`; `BattleFx_PlaceOverOwner`, `Sprite_ScriptTick`, `Sprite_QueueOverlay`; state 0 when the owner is out and its `+0` bit 0 clear |
| `BattleFx_PlaceOverOwner` | `0x434730` | 0x139 | calls from `BattleFx_WatchIcon` and `0x433550` (state 1, Capcom's) | `Sprite_Current` over the owner: a member's x by `BattleWin_MemberTargetOffsets[(+8 + 4 x +0x89) x 2]`, y by `0x64DFC8` / `0x64DFC9` `[+0x89 x 2]` (column 0..1 / 2..3) - 8; an enemy's x by its s8 `+0xF2`, y by its data record's byte `+0x87` (the area's records `0x8C55C8`, stride 0x8C, by `+0xF0`) - 8; `+0x32` copied |
| `BattleFx_NextStatusIcon` | `0x434870` | 0x6D | the same two callers | the owner's status `& 0x58` (a member's `+0x90`, an enemy's `+0x92`) searched from `+0xB` + 1 upward modulo 16: the first bit set, else `+0xB` (al) |
| `Battle_BackupFlagged` | `0x433D60` | 0x40 | a call from `0x433C00` (Capcom's, no group) | 1 when, of the backup records `0x939AE0`, one in use has `+0x134` bit 0 before one not in use; else 0 (al) |
| `BattleFx_RestoreParty` | `0x433DA0` | 0x562 | `0x433B80`'s stack table, state 0 (slot 12) | section 1.2 |
| `BattleFx_RestoreMemberTask` | `0x434310` | 0x26 | `BattleFx_Dispatch` slot 17 | by `+1` through a stack table: `BattleFx_RestoreMember`, `BattleFx_RestoreFade` |
| `BattleFx_RestoreMember` | `0x434340` | 0x37B | slot 17's state 0 | section 1.2 |
| `BattleFx_RestoreFade` | `0x4346C0` | 0x6D | state 1 of slot 12 and of slot 17 | the owner's `+0` bit 0x40 cleared; its u32 `+0x40` up by 0x2000 a frame to 0x10000; then its `+0x48` = 0, its member's `+0x130` loses 0x2000 and `+0x134` & `0xFFFCFFF9`, and a tail `jmp` to `BattleTask_FreeCurrent` |
| `BattleFx_GridMark` | `0x4348E0` | 0x42 | slot 9 | with the sprite pose pool `0x9039D8` = `0x8C5D80` (put back to `0x8B3580` after), by `+1`: `_GridMarkStart`, `_GridMarkRun`, `BattleFx_FreeTask` |
| `BattleFx_GridMarkStart` | `0x434930` | 0xD9 | slot 9 state 0 | the marker's ten sprite fields (`+0x24..+0x2C`, `+0x3C`); placed at cell `+0xB` of a six-column grid in window record 21 (x = `+4` + 30 (b % 6) + 0x13, y = 32 (b / 6) + `+6` + 0x30); `+1` on; animation `+0x4B`; the overlay queued |
| `BattleFx_GridMarkRun` | `0x434A10` | 0x50 | slot 9 state 1 | by `+2`: `_GridMarkCount`, `_GridMarkPlace`; record 21 closed: `+1` 2, `+2` 0; else the script ticked, the overlay queued |
| `BattleFx_GridMarkCount` | `0x434A60` | 0xA3 | slot 9 state 1.0 | placed at its cell; `+2` up once per byte of the list `0x904B84` (count `0x904B87`, re-read) equal to `+0xB` |
| `BattleFx_GridMarkPlace` | `0x434B10` | 0x71 | slot 9 state 1.1 | at the list entry equal to `+0xB`: x = `+4` + 24 i + 0x41, y = `+6` + 0x12; none: `+2` down |
| `BattleFx_ListHand` | `0x434B90` | 0x42 | slot 10 | the pose pool around `_ListHandStart`, `_ListHandRun`, `BattleFx_FreeTask` by `+1` |
| `BattleFx_ListHandStart` | `0x434BE0` | 0xA0 | slot 10 state 0 | the marker's fields; x = record 21 `+4` + 0x14, y = `+6` + 0x12; `+1` on; animation 0x13; a tail `jmp` to `Sprite_QueueOverlay` |
| `BattleFx_ListHandRun` | `0x434C80` | 0x71 | slot 10 state 1 | by `+2`: `_ListHandPress`, `_ListHandWait`; then, drawn (`+0` bit 0): record 21 closed, `+1` 2 and `+2` 0; else placed again, the overlay queued |
| `BattleFx_ListHandPress` | `0x434D00` | 0x26 | slot 10 state 1.0 | animation 0x12; no command tapped (`0x904AA6` 0xFF): animation 0x13, `+2` on |
| `BattleFx_ListHandWait` | `0x434D30` | 0x31 | slot 10 state 1.1 | a command tapped: `+2` back; record 21 closed: `+1` on, `+2` 0 |
| `BattleFx_Win18Cursor` | `0x434D70` | 0x42 | slot 13 | the pose pool around `_Win18CursorStart`, `_Win18CursorRun`, `BattleFx_FreeTask` by `+1` |
| `BattleFx_Win18CursorStart` | `0x434DC0` | 0xBF | slot 13 state 0 | the marker's fields, `+0x4B` 0xFF; the byte of the 18 dwords `0x904608` at row (`+9` + the s16 record 18 `+0x12`) x 4 + column `+0xA` as the animation unless 0xFF; record 18 `+0x1E` = its `+0x12`; `+1` on |
| `BattleFx_Win18CursorRun` | `0x434E80` | 0xB4 | slot 13 state 1 | record 18 closed: `+1` 2, `+2` 0; its row base moved (`+0x1E` not `+0x12`): `+1` 0; else x = `+4` + 22 (`+0xA`) + 0x44, y = 32 (`+9`) + `+6` + 0x22, the grid byte (unless 0xFF) into `Sprite_EnsureAnimation`, the script ticked, the overlay queued |
| `BattleFx_Win19Cursor`, `_Win19CursorStart`, `_Win19CursorRun` | `0x434F40`, `0x434F90`, `0x435050` | 0x42, 0xBF, 0xB4 | slot 14 | the same over record 19 (`0x80340C`) and the grid `0x904620` |

The two cursor families are one body each in ours (`CursorStart` /
`CursorRun` with the record and the grid as arguments); the four markers share
`MarkerLook` and `GridPlace`. What windows 18, 19 and 21 show was not
looked at (BE7 owns the battle windows); the grid of six columns and the list
of up to `0x904B87` bytes are what the code reads.

### 1.2 The party put back from its backup records

Slot 12's task (`0x433B80`'s state 0) and slot 17's (one per other member)
put a member's record (ObjTrio, `0x802D40` stride 0x14C) back from the
backup array `0x939AE0` (the same stride; `battle_turn_steps_callees.h`
`kCopies`). The common part, `RestoreBody` in ours, with the owner as
`Sprite_Current` and `Field_State` its member:

1. the record of the owner's `+5` copied from the backup, 0x14C bytes dword by
   dword (`rep movsd`); `+1..+4` = 6, 4, 4, 1 (slot 12 keeps `+8` across the
   copy), `+0x48` = 2, u32 `+0x40` = 0, `+0x130` |= 0x2000;
2. task slot `+5` placed at the member's spot `0x7E06E0` (x, z), on the ground
   (`AreaMap_Elevation` << 16 into `+0x3C`);
3. for a character `+0x89` of 7 or 0 the cells slot 12 kept in
   `0x675ECC..0x675ED7` come back into `+0x9A`, `+0x90`, `+0x130`, `+0x134`,
   `+0x9E` from `Field_ActorStates` (`+0x148`'s record, stride 0xA4),
   `+0x138..+0x13F` are cleared and `0x442310` (BE3's) recomputes the member;
4. `0x453300(+5)` (BE6's); the member's window (records 13 + i) values
   `+0x14` / `+0x16` and gauges `+0xB` / `+0xC` = 55 x HP / max HP, 55 x AP /
   max AP (signed `idiv`); animation `+8` + 4; the palette from `0x80D380` +
   0x40 x `+5`; `Battle_StatusTint(+0x90)`; `Sprite_SetClutStp`.

`BattleFx_RestoreParty` first waits for `File_LoadDone`, releases the owner's
tint, and after Accession's single-member form (round flags `0x8000`) puts
the party count back (`0x904AB0` = `0x904B8F`, `0x904AB1` = the backup
members whose `+0x91` lacks 0x40, window record 1's `+0xA` / `+4`) and
starts `BattleTask_Create(0, 0x11)` - slot 17's task - owned by each other
member; for a character 7 or 0 it keeps `+0x9A`, `+0x90` & ~0x4000,
`+0x130` & ~0x18000, `+0x134` & `0xFFF884FD` in `0x675ECC..`; after the
common part `0x9045FD` / `FE` (or the member's pair, 3 apart) = `0x904B94` /
`95`, the flags' 0x8000 cleared, `Sprite_Current` back and its state + 1.
`BattleFx_RestoreMember` zeroes the owner's `+0x40` first, runs the common
part without keeping `+8`, and clears the owner's `+0` bit 0x40 after.

### 1.3 The enemy's action and its target (the action's begin)

| Function | Entry | Bytes | Callers | What |
|---|---|--:|---|---|
| `BattleEnemy_PickAction` | `0x435AB0` | 0x188 | `Battle_BeginAction` (`0x42F2C1`, actor - 3) | the kind `0x904B35`: two bits of `0x65563C[enemy +0x8E]` by `Rand() & 3` (0 with the flags' 0x4000 and u16 `+0xBA` <= 1), then a four-case jump table (0 -> 1; 1 -> 2 with `+0x90`'s 0x80 and `+0x110` \|= 2, else 1; 2 -> 3 with 0x40, else 1; 3 -> 4 with 0x20 and the ability `+0x106` = byte `+0x9C[Rand() & 7]`, else 1). An enemy with `+0x92`'s 0x20 or `+0x114`'s 0x4000 acts at random (kind 1, `BattleEnemy_PickAnyTarget`); else `0x904B44` = `BattleEnemy_PickTarget`, each only when `0x452DD0(actor)` allows. In an event battle the enemy becomes `0x939AD8` and `Sprite_Current` and its `+0xF4` hook is called with 0. Target 0xFF: kind and `+0x105` 0; else `+0x105` = the kind |
| `BattleEnemy_PickTarget` | `0x435CF0` | 0x114 | `BattleEnemy_PickAction` | by the kind: 1 - `+0x90` bit 0 another enemy (`BattleEnemy_OtherEnemy`, a member when none), bit 4 the weakest member, else a random member; 4 - by the ability's flag byte (`NameTable_Abilities[+0x106]` +0): 0x10 a side code (0xC0, or 0x80 / 0x40 by 0x20, swapped by `+0x90` bit 0), no 0x40 the enemy itself (+ 3), 0x20 with bit 0 `BattleEnemy_OtherOrMember`, no 0x20 without bit 0 the weakest enemy, else the weakest or a random member by bit 4; other kinds the enemy's `+5` |
| `BattleEnemy_PickAnyTarget` | `0x435C40` | 0x35 | `BattleEnemy_PickAction` | `0x904B44` = a random member three times in eight (`Rand() & 7` below 3) or with one enemy left, else another enemy |
| `Battle_RandomEnemy` | `0x435C80` | 0x64 | the three above; `Battle_MemberAutoTarget` (`0x45418C`) | of actors 3..10 not out and not the argument, the one at `Rand() %` their count |
| `Battle_RandomMember` | `0x435EF0` | 0x179 | `BattleEnemy_PickTarget`, `_PickAnyTarget`, `_OtherOrMember` | weighted by the formation: 3 members `0x64B18C[f x 3 + m]`, 2 members `0x64B184[f x 2 + m]` (f `0x904060`'s low byte), over members not out; `Rand() %` the byte sum picks; another count 0 |
| `Battle_EnemyLowestHp` | `0x435E10` | 0x56 | `BattleEnemy_PickTarget` | the enemy not out with the lowest HP below 0xFFFF; 0 for none |
| `Battle_MemberLowestHp` | `0x435E90` | 0x55 | `BattleEnemy_PickTarget` | the member not out with the lowest HP below 10000; 0 for none |
| `BattleEnemy_OtherOrMember` | `0x435E70` | 0x1E | `BattleEnemy_PickTarget` | one enemy left: a random member (tail `jmp`); else another enemy |
| `BattleEnemy_OtherEnemy` | `0x436070` | 0x1C | `BattleEnemy_PickTarget` | one enemy left: 0xFF; else another enemy |

### 1.4 The enemy ops

Reached through `.data` tables: `EnemyOp_Steps` and every boss kind's
`Steps` table (the tool lists the cells) by `+1`, `EnemyOp_EnterSubs`,
`EnemyOp_ActSubs` and the kinds' `ActSubs` by `+2`. The four second-level
tables were named by round eight (group CF) and keep their counts
(`EnemyOp_EnterSubs2` 2, `Step5Subs` 2, `Act3Subs` 5, `Act5Subs` 3); the
counts agree with the entries' own steps (Act3's four steps and
`EnemyOp_HitEnd`, Act5's HP change, `EnemyOp_WaitAnimOnce` and its end).

| Function | Entry | Bytes | Table cell | What |
|---|---|--:|---|---|
| `EnemyOp_SlideDispatch` | `0x436270` | 0x12 | `EnemyOp_EnterSubs[1]` | `jmp` through `EnemyOp_EnterSubs2` by `+3` |
| `EnemyOp_SlideStart` | `0x436290` | 0x9B | `EnterSubs2[0]` | `+0` bit 0x40 cleared, animation 0; the destination `+0x18` / `+0x1C` = the place; the velocity (0x80000, 0) turned by the facing (`0x446770`, BE4's) taken off the place (section 7), then (0x8000, 0) turned; `+2` on |
| `EnemyOp_SlideStep` | `0x436330` | 0x76 | `EnterSubs2[1]` | `BattleEnemy_ScriptTick`; the place moved by the velocity, `+0x3E` its ground; at the destination steps 3, 0, 0 |
| `EnemyOp_TurnStart` | `0x4365D0` | 0x4A | `EnemyOp_Steps[4]` and the kinds' | animation 2; `+9` = the byte `+0x8A` of the enemy's data record; `+1` on, `+2` 0 |
| `EnemyOp_CueDispatch` | `0x436620` | 0x12 | `EnemyOp_Steps[5]` and the kinds' | `jmp` through `EnemyOp_Step5Subs` by `+2` |
| `EnemyOp_PlayCreatureCue` | `0x436640` | 0x69 | `Step5Subs[0]` | script tick; `+9` down; at 0 the cue - in an event battle `0x437450` (BE3's) with the u16 at the enemy's `+0xF8` table, else `Sound_PlayEffect(0x600 + 2 x +0xF0)` - and `+2` on. The sibling's `Battle_PlayCreatureCue` (PSX `0x801E39BC`) |
| `EnemyOp_CueEnd` | `0x4366B0` | 0x29 | `Step5Subs[1]` | once the script ends: `Battle_SetTargetFlag40(0x904B44)`, `0x4376F0`, the flags \|= 4, tail `jmp` `0x4376A0` (both BE3's) |
| `EnemyOp_Act3Dispatch` | `0x436BC0` | 0x12 | `EnemyOp_ActSubs[3]` and the kinds' | `jmp` through `EnemyOp_Act3Subs` by `+3` |
| `EnemyOp_KnockStart` | `0x436BE0` | 0x55 | `Act3Subs[0]` | the enemy's `+0x108` 0; the velocity (-0x2000, 0) turned; `+0xA` 4; script tick; `Sound_PlayEffect(0x205)`; `+3` on |
| `EnemyOp_KnockBack` | `0x436C40` | 0x4F | `Act3Subs[1]` | `+0xA` down (0: 4 again, `+3` on); the place moved by the velocity; tail `jmp` to the script tick (al answered) |
| `EnemyOp_KnockReturn` | `0x436C90` | 0xB2 | `Act3Subs[2]` | `+0xA` down; at 0 `Battle_SetDamagePopup(+0x108, +5)` with the enemy's `+0x10C` bit 4, else a kind-0 task of effect 1 (the damage popup) owned by `Sprite_Current`, `+7` 3, `+0x27` 0; `+3` on; the place moved back; script tick |
| `EnemyOp_KnockPose` | `0x436D50` | 0x38 | `Act3Subs[3]` | once `BattleEnemy_ScriptTickOnce` ends: `+3` on, animation 8 (`+0x110` bit 1) or 0 |
| `EnemyOp_Act5Dispatch` | `0x436F00` | 0x12 | `EnemyOp_ActSubs[5]` and the kinds' | `jmp` through `EnemyOp_Act5Subs` by `+3` |
| `EnemyOp_ApplyHpChange` | `0x436F20` | 0xAE | `Act5Subs[0]` | HP `+0xA4` against the s16 `+0x108`: at or below it HP 0 and the status `+0x92` = 0x4000; else (a negative change with `Sound_PlayEffect(0x206)`) HP minus it, held at the maximum `+0xB0`; the popup with `+0x10C` bit 0; script tick; `+3` on |
| `EnemyOp_Act5End` | `0x436FD0` | 0x56 | `Act5Subs[2]` | the enemy's `+0x93` bit 6: steps 6, 4, 0; else `Battle_ClearActorBit(+5)`, steps 2, 0, 0 |

The four dispatchers take the harness's `kDispatch` contract: ours hands the
caller's stack word on to the entry and answers its eax (the original's `jmp`
leaves both), and aborts past its table.

## 2. Divergence

None. Where the original indexes past a table, divides by a value that can be
0, or spins without end, ours aborts with a message (section 7); the owner's
rule for an unchecked index (round9 doc section 6) makes that no entry.

## 3. Calls across groups

Called raw through `battle_e2_callees.h` and listed in the fuzz's callees
(docs/boss_harness.md section 10.6); the coordinator rebinds after both
merge:

| Callee | Owner | Callers here | Listed as |
|---|---|---|---|
| `0x442310` | BE3 | `BattleFx_RestoreParty`, `_RestoreMember` | `kPhase`, logging `Field_State` |
| `0x453300` | BE6 | the same | one byte |
| `0x446770` | BE4 | `EnemyOp_SlideStart` (twice), `EnemyOp_KnockStart` | one pointer, with an effect that turns the velocity pair as the real one does (the callers read it back) |
| `0x437450` | BE3 | `EnemyOp_PlayCreatureCue` | one short |
| `0x4376F0` | BE3 | `EnemyOp_CueEnd` | `kPhase` |
| `0x4376A0` | BE3 | `EnemyOp_CueEnd` (tail `jmp`) | `kPhase`, its eax answered |
| `0x452DD0` | nobody (catalogue part 7) | `BattleEnemy_PickAction` | one byte (the standard set lists it whole; every caller pushes a register's stale upper bytes) |

**Inbound calls from outside the group** (for the rebinding pass): ours -
`Battle_BeginAction` (`battle_actions.cpp`, through `kEnemyPickAction`),
`Battle_MemberAutoTarget` (`battle_sprites.cpp`, `pick_enemy_a`),
`BattleFx_Dispatch` and `BattleFx_ActorWatch` (`battle_fx_tasks.cpp`, their
stack tables), `EnemyOp_EnterDispatch` / `EnemyOp_ActDispatch` and
`BattleEnemy_RunAll` through the `.data` tables; Capcom's, in no group -
`0x433550` (the watch's state 1: `BattleFx_PlaceOverOwner`,
`BattleFx_NextStatusIcon`), `0x433B80` (slot 12: `BattleFx_RestoreParty`,
`BattleFx_RestoreFade`), `0x433C00` (`Battle_BackupFlagged`). No other group
of this wave calls into BE2 (`--edges`).

## 4. The rebinding

Every raw reference to a BE2 address in `src/game` (`band_rows.py --refs`: 43
references to 13 functions), rebound in the round-ten form - the value
unchanged, so the other fuzzes' keys stand:

- `battle_fx_tasks.cpp`: `kOriginals.fx` slots 9, 10, 13, 14, 17 and
  `.watch[3]` read `H(bof3::addr::BattleFx_GridMark)` and so on;
  `battle_fx_tasks_fuzz.cpp`'s `kFxImm` / `kWatchImm` values likewise; the
  comments in `battle_fx_tasks.cpp` / `_callees.h` name them.
- `battle_actions_callees.h`: `kEnemyPickAction = bof3::addr::BattleEnemy_PickAction`
  (still a constant: the fuzz keys on it); `battle_actions_fuzz.cpp`'s
  `case` and `kBeginCalls` row likewise; the comments.
- `battle_sprites.cpp`: `Raw<...>(bof3::addr::Battle_RandomEnemy)`;
  `battle_sprites_fuzz.cpp`'s two rows likewise; the comments.
- Comments naming the functions: `enemy_ai_ops.cpp`, `boss_sg_callees.h`,
  `boss_si.cpp`, `boss_h.cpp`, `boss_sc.cpp`.

**Left raw:** `boss_harness.cpp` line 537's comment (`0x4365D0`) - the
harness is not a group's to edit; the coordinator's fold. The shadows
`battle_fx_tasks`, `battle_actions`, `battle_sprites` pass with the rebound
values (0 mismatches, this worktree).

## 5. The fuzz

`src/game/battle_e2_fuzz.cpp`, `BOF3X_SHADOW=battle_e2`, one
`boss_harness::Run` per family, `Group::engine` set, 6,000 rounds a
function:

| Run | Functions | Shapes | Rounds |
|---|--:|---|--:|
| `tasks` | 24 | `kTask` (the dispatchers with `states` 2 / 3 at `+1` / `+2`, their stack tables' immediates re-aimed); `Battle_BackupFlagged` `kHelper` | 144,000 |
| `begin` | 9 | `kHelper`, the enemy index as the word | 54,000 |
| `ops` | 15 | `kState`; the four dispatchers `kDispatch` (`state_at` 3 / 2, `states` 2, 2, 5, 3) over their `.data` tables | 90,000 |

`BOF3X_BE2_RUN=<tasks|begin|ops>` runs one family, `BOF3X_BE2_ONLY=<name>`
one function (the controls script's shortcuts).

**The group's listings** beyond the standard set: `Rand` answering 15 bits
(the CRT's `0x5B93D2` masks `& 0x7FFF`; the standard recorder's garbage would
be a negative remainder the two picks index their frames by, which no real
`Rand` gives); `Battle_ActorIsOut` with one actor per round ruled in (below);
`0x452DD0` and `Battle_SetDamagePopup`'s actor word by the byte their callers
really set (both push a register's stale upper bytes); the cross-group
callees of section 3; the group's own functions called directly - each with
the answer range its callers test (`BattleFx_NextStatusIcon` 0xFF..0x0F,
`Battle_RandomEnemy` 3..10, `BattleEnemy_OtherEnemy` 0xFF..10, ...).

**Answers compared** (`Clone::ret_mask 0xFF`, al): `BattleFx_NextStatusIcon`,
`Battle_BackupFlagged`, the eight target helpers (not `BattleEnemy_PickAction`,
whose caller reads nothing back), `EnemyOp_CueEnd` and `EnemyOp_KnockBack`
(the two that end in the script tick's answer).

**For the harness's fold** (not edited here): the standard `Rand`'s garbage
quarter is a negative value no CRT `Rand` gives - an engine caller that
`idiv`s by it indexes by a negative remainder; `0x452DD0` and
`Battle_SetDamagePopup`'s actor word are listed whole where every BE2 caller
sets only the low byte; `0x446770` (BE4's) wants the turning effect above
wherever a caller reads the velocity back.

**Regions** beyond the engine frame: the backup records past the standard
`0x939AD0..0x939B20` (to `0x939EC0`), the pose pool pointer `0x9039D8`, the
kept cells `0x675ECC..0x675ED7`, the formation `0x904060`, the members' spots
`0x7E06E0`.

**Seeds** (`Seed(k)`, by function):

- the watch and markers: the owner a party member or an enemy with its own
  actor byte two times in three (the harness's record otherwise), the actor
  0..10 always; the round flags' 0x400, `0x904B34` equal to it, the kind 4 and
  the abilities 0x97 / 0xE1 and neighbours; `+0xA` at 0 and 1; `+0xB` 0..15;
  windows 18, 19, 21 open half the time, their row bases -1..3 and the copies
  equal half the time, grid cells 0..17, the pick list around `+0xB` with
  counts 0..3, grid bytes 0xFF a quarter of the time, `0x904AA6` 0xFF;
- `BattleFx_NextStatusIcon`: `+0xB` of 16 or more a third of the time, and
  then a status bit always (with none the original spins, section 7);
- the restore: the owner a party member with its actor 0..2, every member's
  and backup's maxima non-zero, characters 0 and 7 often, the backup's actor
  byte 0..2 (the copy brings it into the owner, which is the member itself),
  `0x904B8F` 0..4, the flags' 0x8000 half the time, the fade's `+0x40` at
  0x10000 and neighbours;
- the begin: enemies left 0..3, the kinds 1 / 4 / others, ability ids drawn
  alike from the four branches of `BattleEnemy_PickTarget` (classified at
  start-up from `NameTable_Abilities`' flag bytes in the loaded image), HP
  at the thresholds 0xFFFF / 10000; for `Battle_RandomMember` the party count
  2 / 3 / others and a formation whose weights are all non-zero and sum below
  0x100 (found at start-up in the loaded image, `FindFormations`; formations
  0..31 are scanned, so rows past the real table are inputs too) with one
  member ruled in - together the sum is never 0; for `Battle_RandomEnemy` one
  enemy ruled in and never the one left out;
- the ops: the facing `+8` 0..4, the counters `+9` / `+0xA` at 0, 1, 2, the
  event battle 0 half the time, the slide's destination at the next step
  half the time, HP against the change at every boundary (equal, one each
  side, 0x7FFF / 0x8000, negative) and the maximum around the result; the
  dispatchers' other state bytes inside their tables (`OtherStates`).

**`Settle`** after every disturbance: a watch's or marker's owner actor back
below 11, a restore's owner and `Sprite_Current` actor below 3 - the cells
the originals index by without a check (section 7). **`Disturb`** (the
group's case): the owner's actor, `0x904AB0`, record 21's `+0`, `0x904AA6`,
the enemies left, the flags' high byte.

**Totals** (this worktree, 2026-09-29):

    shadow      battle_e2 self-test: 144000 rounds over 24 functions (6000 each), 198419 calls to the stand-ins, 0 MISMATCHES; 36924 bytes of state (32 regions) and the stand-ins' log compared
    shadow      battle_e2 self-test: 54000 rounds over 9 functions (6000 each), 175178 calls to the stand-ins, 0 MISMATCHES; 36924 bytes of state (32 regions) and the stand-ins' log compared
    shadow      battle_e2 self-test: 90000 rounds over 15 functions (6000 each), 142055 calls to the stand-ins, 0 MISMATCHES; 36924 bytes of state (32 regions) and the stand-ins' log compared

`BOF3X_SHADOW='*'` (this worktree, the same build): exit 0, every shadow's
totals line at 0 mismatches, `inject: 6285 ours` - the three lines above
among them, with the counts above.

The coverage lines: every recorder the group lists was called, every handler
of the stack tables and of the four `.data` tables (`phase 0x...` with
`EnemyOp_HitEnd` `0x436A20` and `EnemyOp_WaitAnimOnce` `0x436A00`, the
tables' entries that are other groups'), and the `+0xF4` hook.

## 6. Controls

`controls.py` (the session's scratchpad): each plants one change in ours,
rebuilds, runs that function alone (`BOF3X_BE2_ONLY`), restores and
rebuilds. 61 planted, **61 refused by a count** (the rounds that mismatched,
of 6,000). Two first tries hit ours' abort instead of a count and were
replaced: C4's bits 0x18 for 0x58 left a status with no bit for `+0xB` >= 16
(ours aborts where the original spins), C31's actor 10 left out emptied the
list when the round's ruled-in enemy was 10 (ours aborts where the original
divides by 0).

| # | Function | Planted | Refused in |
|--:|---|---|--:|
| 1 | `BattleFx_WatchIcon` | the icon period 0x3C for 0x3B | 1,198 |
| 2 | | the out test clears `+2`, not `+1` | 1,164 |
| 3 | | a member's shown bits 0x18 for 0x58 | 173 |
| 4 | `BattleFx_NextStatusIcon` | the search from `+0xB` + 2 | 321 |
| 5 | `BattleFx_PlaceOverOwner` | `+0x32` copied from `+0x30` | 6,000 |
| 6 | | the row byte's column bound 1 for 2 | 197 |
| 7 | `Battle_BackupFlagged` | a record not in use skipped, not the end | 1,339 |
| 8 | `BattleFx_RestoreParty` | `+8` not kept across the copy | 4,922 |
| 9 | | the kept `+0x134` mask | 825 |
| 10 | | the task loop bounded by `0x904B8F`, not the re-read `0x904AB0` | 13 |
| 59 | | `Sprite_Current` not re-read after `0x453300` | 444 |
| 11 | `BattleFx_RestoreMember` | the HP gauge 54 for 55 | 4,457 |
| 12 | | bit 0x80 cleared for 0x40 | 4,553 |
| 13 | | the spot's z by 4, not 8 | 3,952 |
| 14 | `BattleFx_RestoreFade` | the fade step 0x1000 | 4,403 |
| 15 | `BattleFx_RestoreMemberTask` | the two states swapped | 6,000 |
| 16 | `BattleFx_GridMark` | states 1 and 2 swapped | 4,033 |
| 17 | `BattleFx_GridMarkStart` | six columns for five | 4,451 |
| 18 | `BattleFx_GridMarkRun` | `+2` not cleared when record 21 closes | 1,477 |
| 19 | `BattleFx_GridMarkCount` | counting the others | 4,432 |
| 20 | `BattleFx_GridMarkPlace` | the list step 25 | 1,351 |
| 55 | `BattleFx_ListHand` | states 0 and 1 swapped | 3,966 |
| 21 | `BattleFx_ListHandStart` | animation 0x14 | 6,000 |
| 22 | `BattleFx_ListHandRun` | the drawn bit 2 | 2,698 |
| 23 | `BattleFx_ListHandPress` | no command 0xFE | 1,623 |
| 24 | `BattleFx_ListHandWait` | `+2` kept at the close | 1,955 |
| 27 | `BattleFx_Win18Cursor` | the pose pool not put back | 6,000 |
| 25 | `BattleFx_Win18CursorStart` | the row copy into `+0x1C` | 6,000 |
| 56 | `BattleFx_Win18CursorRun` | the other grid | 1,323 |
| 57 | `BattleFx_Win19Cursor` | state 1 runs the start | 1,999 |
| 58 | `BattleFx_Win19CursorStart` | record 18 for 19 | 5,461 |
| 26 | `BattleFx_Win19CursorRun` | the column step 21 | 1,242 |
| 28 | `BattleEnemy_PickAction` | the `+0xBA` bound < 1 | 79 |
| 29 | | the kind's bit pair off by one | 881 |
| 30 | | the none target 0xFE | 1,278 |
| 31 | `Battle_RandomEnemy` | the whole word compared, not its low byte | 490 |
| 32 | `Battle_RandomMember` | the running sum `>=` for `>` | 67 |
| 33 | `Battle_EnemyLowestHp` | the last of equals | 725 |
| 34 | `Battle_MemberLowestHp` | the start 10001 | 272 |
| 35 | `BattleEnemy_PickTarget` | the side codes swapped | 86 |
| 36 | | kind 1's bit 5 for 4 | 236 |
| 37 | `BattleEnemy_PickAnyTarget` | four in eight | 820 |
| 38 | `BattleEnemy_OtherEnemy` | none 0xFE | 1,606 |
| 39 | `BattleEnemy_OtherOrMember` | enemy + 2 | 4,394 |
| 40 | `EnemyOp_SlideStart` | the second half off `+0x38` (the latent slip "fixed", section 7) | 1,655 |
| 41 | `EnemyOp_SlideStep` | `+3` kept at the arrival | 2,716 |
| 42 | `EnemyOp_TurnStart` | the record stride 0x8B | 5,006 |
| 43 | `EnemyOp_PlayCreatureCue` | the cue base 0x601 | 869 |
| 44 | `EnemyOp_CueEnd` | the round flag 8 | 2,866 |
| 45 | `EnemyOp_KnockStart` | the sound 0x206 | 6,000 |
| 46 | `EnemyOp_KnockBack` | the count 3 | 1,650 |
| 47 | `EnemyOp_KnockReturn` | the popup task's mode 2 | 803 |
| 60 | | `Sprite_Current` not re-read after the popup | 31 |
| 48 | `EnemyOp_KnockPose` | animation 9 | 1,996 |
| 49 | `EnemyOp_ApplyHpChange` | HP equal to the change kept | 417 |
| 61 | | `0x939AD8` not re-read after the sound | 95 |
| 50 | `EnemyOp_Act5End` | step 5 for 6 | 2,978 |
| 51 | `EnemyOp_Act3Dispatch` | by `+2` for `+3` | 4,754 |
| 52 | `EnemyOp_CueDispatch` | by `+3` for `+2` | 3,034 |
| 53 | `EnemyOp_SlideDispatch` | by `+1` for `+3` | 3,065 |
| 54 | `EnemyOp_Act5Dispatch` | the next entry, wrapped | 6,000 |

## 7. Latent defects (Capcom's, kept)

Described, not fixed; ours aborts where the original would run on through
memory or never return.

- **`BattleFx_NextStatusIcon` never returns** when the owner's status has
  none of 0x58 and `+0xB` is 16 or more: the search runs modulo 16 and stops
  only on `+0xB` itself. `BattleFx_WatchIcon` stores the answer (0..15) in
  `+0xB`, so only a first call with `+0xB` at 16 or more can spin; who sets
  `+0xB` before (the watch's states 1 and 2, `0x433550` / `0x433640`) was not
  read. Its answer 0xFF, which `BattleFx_WatchIcon` tests to free the slot,
  never comes: that branch is dead in the original (the fuzz reaches it
  through the recorder).
- **`EnemyOp_SlideStart` takes the velocity's second component off `+0x34`**
  (the place's x), not `+0x38`, and steps `+2`, not `+3`. Reached through
  `EnemyOp_EnterSubs[1]` (`+2` = 1), it leaves `+2` = 2, and the next frame's
  `EnemyOp_EnterDispatch` reads `EnemyOp_EnterSubs[2]` - past the table's two
  entries, the next table's first cell (`EnemyOp_ScaleInStart`). Whether any
  enemy enters with `+2` = 1 was not looked at. Kept (control C40 is the
  "fix", refused).
- **`BattleTask_Create`'s 0xFF is not tested** by `BattleFx_RestoreParty`
  (the owner at slot 0xFF's `+0x80`, `0x9423FC`) or `EnemyOp_KnockReturn`
  (`+0x80`, `+7`, `+0x27` there) - past the 48 slots and past the image; the
  family of known-defects D163. Ours aborts.
- **Divisions by a value that can be 0**: `Battle_RandomEnemy` by the count
  of enemies in (every enemy out but the excluded one), `Battle_RandomMember`
  by the byte sum of the weights (every member out, or weights summing to
  0x100), the restore's gauges by a member's max HP / AP. Ours aborts.
- **Actor indices unchecked**: the watch, `BattleFx_PlaceOverOwner` and
  `BattleFx_NextStatusIcon` index the enemies by the owner's `+5` - 3 (past 25
  past `.data`); the restore steps index the party, the task slots and the
  windows by the owner's and `Sprite_Current`'s `+5` (past 2 they write into
  the window records and past the slots); `BattleEnemy_PickAction` /
  `_PickTarget` index the enemies by the argument's low byte (past 7).
  Battle_BeginAction passes 0..7 from the turn queue. Ours aborts on the
  writes' indices and on the enemy argument; the watch's reads are kept.
- **The four dispatchers index their tables unchecked** (as every EnemyOp
  table does; `enemy_ai_ops.cpp` keeps that for its own). Ours aborts.
- **`Battle_RandomEnemy` takes `Rand`'s answer signed** and indexes its frame
  by the remainder: safe only because the CRT's `Rand` answers 0..0x7FFF.

## 8. What nothing reached

Every recorder and handler the three families list was called (section 5),
and every control was refused. What only a battle would show: which windows
18, 19 and 21 are and what their grids hold, the backup records' use outside
Accession, and whether the slide-in path is ever taken.

## 9. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29): 39 lines - the 36 hidden
starts' extents, `00433D60 40` and `00434870 6D` (which fix the host lines
`00433D60 9CD` and `00434870 894` that covered 23 of them), and
`00435AB0 188` (the catalogue's `18D` ran into padding). The other nine
non-hidden starts were already listed with these extents. The host line
`00436B50 240` (`BattleEnemy_Chance70`, really 0x6F bytes), which covered
eight of them, is cut at the new lines.

## 10. The live route's coverage

`tools/recipes/dragonTransform.txt` (the owner's, 2026-09-28) enters
`BattleEnemy_PickAction`, `BattleEnemy_PickTarget`, `Battle_RandomMember`
(recipe frame 1,702) and `Battle_EnemyLowestHp` (1,983) -
`analysis/calltrace/reach_dragon/reach_dragon_new.txt`, from
`Battle_PhaseDispatch`'s action phase. The same trace shows two more that
were not armed: `0x442310 <- 0x433D60` at frame 2,731 (the tracer names the
caller by the host extent `0x433D60` had, which points at
`BattleFx_RestoreParty` or `BattleFx_RestoreMember`) and `BattleFx_FreeTask <- 0x434870` at 1,679
(one of slots 9, 10, 13, 14, hidden in `0x434870`'s extent, freed its task).
With section 9's lines the coordinator's live check arms them; the other
functions are fuzz-only.
