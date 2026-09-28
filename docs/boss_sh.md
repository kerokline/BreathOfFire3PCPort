# Group BSH: fights 34, 35, 36, 41, 43, 47 and kinds 41..44, 48, 50, 54, and the Angler's effect task

**Status:** MEASURED (2026-09-28) - round eleven
([`takeover-queue-bosses.md`](takeover-queue-bosses.md)), wave two. 46
functions ours (`src/game/boss_sh.cpp`, shadow `boss_sh`), each read to its
last instruction with capstone and fuzzed through the boss harness
([`boss_harness.md`](boss_harness.md)), one `Run` per unit: 0 mismatches in
@ROUNDS@ rounds. @CONTROLS@ Fuzz only: no recorded route reaches a boss
fight. No divergence.

Enemy and fight names are `tools/boss_rows.py --disc`'s (the US disc's area
records), not memory of the game; which fight a set-up is comes from the
tool's rows and the code, section 1.

## 1. The units

`analysis/boss_funcs.tsv`, group column `BSH`: 46 functions in 14 units, all
taken. **No start dropped, none added**: every start is a function, each
extent the function's last instruction (every gap `nop` padding), and the
tool's clone rows match the reading call site for call site. The tool's
table extents read to the next named address ([`boss_harness.md`](boss_harness.md)
section 6); the counts below are the code's: every kind's `+1` table is
twelve (the dispatcher's twelve states; the next table follows), every hook
table three, the Angler's two `+2` tables two each (each step's code moves
`+2` from 0 to 1 and leaves by `+1` or `0x4376A0`), the effect task's `+1`
table one (`BossAnglerFx_Run` never moves `+1`; kind 44's byte tables follow
at `0x64D8FC`).

| Unit | Fight / enemy (the tool) | Functions | Sibling's image (roots) |
|---|---|--:|---|
| K48 | Sample 3, area 160 | 3 | `BOSS034` |
| B34 | id 34: `BOSS034` row 7 (kinds 40 Mikba, area 43 - group BSG's - and 48) | 4 | `BOSS034` (22) |
| B41 | id 41: `BOSS034` row 7 by the tool (chapter 15's) | 1 | `BOSS034` |
| K41, K42 | Gaist, Torch, area 120 | 4, 4 | `BOSS035` |
| K54 | Sample 9, area 165 | 3 | `BOSS035` |
| B35 | id 35: `BOSS035` row 7 (kinds 41, 42, 54) | 3 | `BOSS035` (17) |
| B47 | id 47: `BOSS047` (the tool: no kinds of its own) | 1 | `BOSS047` = `BOSS035` |
| K43 | Angler, area 75 | 10 (7 shared with K50) | `BOSS036` |
| K50 | Sample 5, area 162 | 3 | `BOSS036` |
| B36, B43 | ids 36 and 43: `BOSS036` row 7 (kinds 43, 50) | 2, 1 | `BOSS036` (17) |
| F3 | the kind-3 dispatcher's slot 3 | 4 | (`BOSS036`: the task kind 43's hook starts) |
| K44 | Elder, area 144 (its set-ups 38 / 44 are group BSI's) | 3 | `BOSS038` |

**Which fight each set-up is.** Three images hold two set-ups each (the
sibling's `docs/loader_records/BOSS.md` section 5: ids 34 and 41 are
`BOSS034`; 35 and 47 one image by section md5; 36 and 43 `BOSS036`), and each
image's kinds are one chapter enemy and one `Sample` (Mikba and Sample 3;
Gaist, Torch and Sample 9; Angler and Sample 5). The plan's section 7 gives
chapter 15's fights as ids 39..48 and the Samples as kinds 46..55; across
the ids the other groups settled (39 is Sample 1's, 46 Sample 8's, 48
Sample10's), id n is kind n + 7's fight, and the code agrees here: the three
chapter-15 set-ups (41, 43, 47) store BH's `BossHook_EndPickWay`, the end
hook of chapter 15's ten, while 34, 35 and 36 have end hooks of their own.
So 41 is taken to be Sample 3's fight, 43 Sample 5's, 47 Sample 9's, and 34,
35, 36 the areas 43, 120, 75 fights - by that pattern and the hooks, **not
proven**: no set-up here compares `0x904AAA`. B47 is one of the plan's five
open set-ups; the owner can settle it from play.

**The Samples reuse their chapter enemy's tables**: kind 48's entrance
stores kind 40's (Mikba's) `+0xFC` / `+0xF8` byte tables, kind 54's Gaist's,
kind 50's the Angler's, and kind 50's `+1` table holds the Angler's own
states 4 and 5 and its hook table the Angler's spawn entry. Each Sample has
its own dispatcher, entrance and hook table.

**The size check**: the sibling's `static_discovery_entry_pcs`
(`analysis/overlay_captures_all.json`) are `BOSS034` 22, `BOSS035` 17,
`BOSS036` 17; the PC's sets are 20 (B34 4 + B41 1 + K48 3 + kind 40's 12,
BSG's) with BH's `BossHook_EndPickWay`, 16 (B35 3 + B47 1 + K41 4 + K42 4 +
K54 3 + BSE's `0x43E790`), and 20 (B36 2 + B43 1 + K43 10 + K50 3 + F3 4) -
the PSX images count some of the tasks and helpers otherwise; no function of
the images is missing from the band's units.

## 2. What each function does

### 2.1 The kinds

Every kind is the generic enemy's state machine (`EnemyOp_Steps`,
[`enemy_ai_ops.md`](enemy_ai_ops.md)) with the kind's own entrance at step
0; the Angler replaces steps 4 and 5 too. The dispatchers jump through the
kind's `.data` table by `Sprite_Current +1`; the hooks through a three-entry
table by the low byte of their word (0 the action pick, 1 the hit, 2
`BattleEnemy_RunAll`'s call). **All seven entrances are one shape** (0x51
bytes): 0x939AD8's `+0xFC`, `+0xF4` (the kind's hook), `+0xF8` (0x939AD8 read
again for each), then its dword `+0x114 |= 8`, `Sprite_Current +1 = 2`, and a
tail jump to `Sprite_ScriptTick` (its `al` the answer).

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43DEF0` / `0x43E540` / `0x43E600` / `0x43E6A0` / `0x43E7C0` / `0x43EA70` / `0x43EC30` | `BossSample3_Dispatch` / `BossGaist_Dispatch` / `BossTorch_Dispatch` / `BossSample9_Dispatch` / `BossAngler_Dispatch` / `BossSample5_Dispatch` / `BossElder_Dispatch` | 0x12 | `BossKind_Table[48 / 41 / 42 / 54 / 43 / 50 / 44]`: `jmp [<Kind>_States + 4 * +1]` (12) |
| `0x43DF10` | `BossSample3_Enter` | 0x51 | `+0xFC = 0x64D6CC`, `+0xF8 = 0x64D6D8` (kind 40's tables, BSG's to name), hook `BossSample3_Hook` |
| `0x43E560` / `0x43E6C0` | `BossGaist_Enter` / `BossSample9_Enter` | 0x51 | `BossGaist_Anims` / `BossGaist_Sounds`, each kind's own hook |
| `0x43E620` | `BossTorch_Enter` | 0x51 | `+0xFC = 0x675F0C` (section 6), `+0xF8 = BossTorch_Sounds`, hook `BossTorch_Hook` |
| `0x43E7E0` / `0x43EA90` | `BossAngler_Enter` / `BossSample5_Enter` | 0x51 | `BossAngler_Anims` / `BossAngler_Sounds`, each kind's own hook |
| `0x43EC50` | `BossElder_Enter` | 0x51 | `BossElder_Anims` / `BossElder_Sounds`, hook `BossElder_Hook` |
| `0x43DF70` / `0x43E720` / `0x43ECB0` | `BossSample3_Hook` / `BossSample9_Hook` / `BossElder_Hook` | 0x10 | three `BareRet` |
| `0x43E5C0` | `BossGaist_Hook` | 0x10 | `BareRet`, `BareRet`, `BossGaist_HookClearBit1` |
| `0x43E5D0` | `BossGaist_HookClearBit1` | 0x24 | hook entry 2: with round-flag bit 1 (`0x904AA8`) set and `MoveScript_WaitWordDA` 0 - the bit cleared (a word `and`) and `Draw_PassFlags = 0` |
| `0x43E680` | `BossTorch_Hook` | 0x10 | `BossTorch_HookTarget3`, `BareRet`, `BareRet` |
| `0x43E690` | `BossTorch_HookTarget3` | 8 | hook entry 0 (the action pick): the target `0x904B44 = 3` (enemy 0) |
| `0x43E840` | `BossAngler_AdvanceDispatch` | 0x12 | step 4 of kinds 43 and 50: `jmp [BossAngler_AdvanceSteps + 4 * +2]` (2) |
| `0x43E860` | `BossAngler_AdvanceStart` | 0x5E | `+9` = the byte `+0x8A` of the area's enemy data record 0x939AD8's `+0xF0` names (`0x8C5652 + 0x8C * index`, the generic step 4's count); `BattleEnemy_SetAnimation(2)`; `BattleEnemy_ScriptTick`; then (Sprite_Current read again for each) the goal `+0x18 = +0x34 + 0x30000`, the step `+0xC = 0x4000`, `+2` up |
| `0x43E8C0` | `BossAngler_Advance` | 0x8F | `+9` down, at 0 the kind's first sound word to `0x437450`; `+0x34 += +0xC`; at `+0x34 == +0x18` (Sprite_Current read again): `Battle_SetTargetFlag40(target)`, `0x4376F0`, `+0x18 -= 0x30000`, `+1` up (to 5), `+2 = 0`; tail jump `BattleEnemy_ScriptTick` |
| `0x43E950` | `BossAngler_RetreatDispatch` | 0x12 | step 5: `jmp [BossAngler_RetreatSteps + 4 * +2]` (2) |
| `0x43E970` | `BossAngler_Retreat` | 0x27 | `+0x34 -= +0xC`; at `+0x34 == +0x18`, `+2` up; tail jump `BattleEnemy_ScriptTick` |
| `0x43E9A0` | `BossAngler_RetreatEnd` | 0x11 | `BattleEnemy_ScriptTick`; round-flag bit 2 set; tail jump `0x4376A0` (the action's end) |
| `0x43E9C0` / `0x43EAF0` | `BossAngler_Hook` / `BossSample5_Hook` | 0x10 | `BareRet`, `BareRet`, `BossAngler_HookSpawnFx` |
| `0x43E9D0` | `BossAngler_HookSpawnFx` | 0x92 | hook entry 2 of kinds 43 and 50: with `+1 == 7` and `+2 == 1`, `slot = BattleTask_Create(3, 3)`; the acting actor's enemy object's first 0x80 bytes (`0x93B960 + 0x128 * (0x904B34 - 3)`) copied into the slot a dword at a time (`rep movsd`); the slot's `+1`, `+2`, `+9` = 0, `+5`, `+6`, `+0x29` = 3 |

So the Angler's action is the generic one moved: step 4 counts its wind-up
as the generic step does, then moves `+0x34` (the word `AreaMap_Elevation`
takes first elsewhere) by 0x4000 a frame for twelve frames to a goal 0x30000
away and flags the target; step 5 moves it back and sets the round flag's
bit 2 as it ends. What that looks like on screen was not looked at.

### 2.2 The effect task (the kind-3 dispatcher's slot 3)

`BattleBossFx_Dispatch` (`0x4357D0`) runs a slot of kind 3 through a stack
table by the slot's `+5`; `BossAngler_HookSpawnFx` creates its task with
parameter 3, and entry 3 is `0x43EB80`. The slot starts as a copy of the
acting enemy's object with `+1 = +2 = 0`.

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43EB80` | `BossAnglerFx_Dispatch` | 0x12 | `jmp [BossAnglerFx_States + 4 * +1]` - one entry |
| `0x43EBA0` | `BossAnglerFx_Run` | 0x2E | by `+2` through a table of three built on the stack: `BossAnglerFx_Start`, `BattleFx_ScriptUntilDone`, `BattleFx_FreeTask`; `call [esp + 4 * +2]` unchecked |
| `0x43EBD0` | `BossAnglerFx_Start` | 0x31 | `+0xB = 0`, `+9 = 0`; `Sprite_SetAnimation(1)`; `Sprite_ScriptTick`, `Sprite_QueueOverlay`; `+2` up |
| `0x43EC10` | `BattleFx_ScriptUntilDone` | 0x1C | `Sprite_ScriptTick`, `Sprite_QueueOverlay`; with round-flag bit 2 (the done flag `BossAngler_RetreatEnd` and the generic step 5 set), `+2` up - so the free follows the enemy's action's end. Three spell tasks' stack tables hold it too (section 8), hence the engine name |

### 2.3 The set-ups

| Address | Name | Bytes | What it does |
|---|---|--:|---|
| `0x43DF80` | `Boss34_Setup` | 0x41 | the byte `0x669730` (scenario code writes it) read once; each party member 0..2 whose `+0x148` equals it leaves its index in `0x675F08` (the last match wins); hooks End `Boss34_End`, Exit `Boss34_Exit`, Event `Boss34_Event` |
| `0x43DFD0` | `Boss34_Event` | 0xEE | code 5: unless round-flag bit 15, `0x904AAD` bit 0, or the picked member's `+0x91` bit 0x20 - `0x904AAD |= 1` and `0x446700(the dword 0x675F08)`, the member put at the front of the round's remaining order. Code 1: with `0x904AAD` bit 0 set, bit 1 clear and the acting actor the picked member - the acting kind `0x904B35 = 4`, the action record `0x904B40`'s `+1 = 4` and word `+2 = 0x40`, the target the member, `0x904B80 = 0x40`, `0x904AAD |= 2`, then `AbilityList_Add(0x40, 4, 0, 0)` and `AbilityList_Add(0x40, member, 0, 1)`. al 0 |
| `0x43E0C0` | `Boss34_End` | 0x148 | the win: each member with `+0` bit 0 - `Battle_RemoveFromTurnOrder(+5)`, then (its `+0x90` read after the call) `Sprite_Current` = the member, `Sprite_PoseFromSet(+8 + (bit 14 of +0x90 ? 0x1C : 4), 0x8C5D80, 0x1800)`; unless `0x904AAD` bit 1, the two `AbilityList_Add` calls; `0x904AE8 |= 8`; `0x446DE0`; the chapter step `0x8034E5` up by one after it. Else `0x446E00` |
| `0x43E210` | `Boss34_Exit` | 0x52 | `BossActor_ClearBit40(0)`; `Sprite_Current = BossActor_Find(0)`; `Sprite_SetAnimationBank(0x1C1)`, `Sprite_SetAnimation(8)`; then `+0x2A = 1`, the words `+0x58` / `+0x5A` = enemy 0's |
| `0x43E270` / `0x43E7A0` / `0x43EB40` | `Boss41_Setup` / `Boss47_Setup` / `Boss43_Setup` | 0x1F | End BH's `BossHook_EndPickWay`, Exit `BareRet`, Event `BareRetZero` |
| `0x43E730` | `Boss35_Setup` | 0x1F | End `Boss35_End`, Exit `0x43E790` (BSE's `BossHook_ExitTransition4`, set-up 26's too), Event `Boss35_Event` |
| `0x43E750` | `Boss35_Event` | 0x20 | code 0 and `Battle_ActorIsOut(3)` (enemy 0) - `0x904AE8 |= 2` (the win). al 0 |
| `0x43E770` / `0x43EB20` | `Boss35_End` / `Boss36_End` | 0x1A | the win: chapter step `0x15` / `8`, `0x446DE0`; else `0x446E00` |
| `0x43EB00` | `Boss36_Setup` | 0x1F | End `Boss36_End`, Exit `BareRet`, Event `BareRetZero` |

Set-up 34 is the one fight here with a script: a party member is marked by a
byte the scenario leaves (`0x669730` against each member's `+0x148`), put
first in the round's order on phase code 5, and on its turn (code 1) its
action is replaced (kind 4, `+2 = 0x40`, aimed at itself) and ability 0x40
given twice - to the list of "member 4" (the third word 0: the member's own
list, `AbilityList_Add`'s reading; which record 4 is was not read) and to the
member's own party copy; the end hook gives it again if the event hook did
not, poses the party and moves the chapter on by one step. What ability 0x40
is, and what the scene is, was not read.

`Boss35_Event` makes enemy 0's fall the win (`0x904AE8` bit 1) - the fight
ends when enemy 0 is out, whatever the others. Which enemy is enemy 0 in
area 120's row 7 was not read.

## 3. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed. Where the original jumps through an index past its table (every
dispatcher and hook, `BossAnglerFx_Run`'s stack table), copies into the slot
`BattleTask_Create`'s 0xFF names, or writes through `BossActor_Find`'s null
(`Boss34_Exit`), ours aborts with a `Fatal` naming the function (the owner's
rule, round9 doc section 6); nothing reaches it. What ours does in C that the
original does in registers, none observable:

- **The dispatchers hand the caller's word on and answer the entry's eax**
  (the original `jmp` leaves both in place; `Port_DroppedCall` reads the
  word). The fuzz checks both: `Port_DroppedCall`'s recorder logs its byte,
  the effect task's one-entry table logs its word, and every dispatcher's
  answer is compared (controls H2, H10, H114).
- **The event hooks answer in `al`**; ours returns 0 in a byte. Every caller
  reads `al` (`ret_mask 0xFF`).
- **`AbilityList_Add`'s second member word** is the first call's eax with
  its low byte replaced (`mov al, [0x675F08]`); ours passes the same dword.
  `AbilityList_Add`'s list lookup `0x591EC0` masks the member to a byte, so
  the upper bytes are carried and never used.

## 4. The fuzz

`BOF3X_SHADOW=boss_sh`, `src/game/boss_sh_fuzz.cpp`, one `Run` per unit
(`BOF3X_BSH_RUN=k48|b34|...|k44` runs one). The kinds' `+1`, `+2` and hook
tables are `DataTable`s (21; the hook tables with one argument word, listed
first, and the effect task's one-entry table with its word, so the
dispatcher's hand-on is seen where no entry reads it); the stack table of
`BossAnglerFx_Run` is three `Imm`s (the harness re-aims them at recorders).
The group's callees: the four engine functions nobody owns (`0x446700` a
byte, `0x437450` a word, `0x4376A0`, `0x4376F0`) and two louder stand-ins
(section below). The group's regions: `0x669730`, `0x675F08..0x675F17`,
`MoveScript_WaitWordDA`, `Draw_PassFlags`.

@RUNTABLE@

**Seeds.** Every dispatcher's other state bytes are drawn inside its table
(the round's lesson: a wrong-byte plant then counts rather than Fatals). The
hooks: the harness's word 0..2 with garbage above the byte half the time.
The event hooks: the code picked by the seed (code 1 or 5 for set-up 34,
code 0 for 35, two times in three; else any), and the round's other inputs
aimed at that code's path - set-up 34's `0x904AAD` bits 0 and 1, round-flag
bit 15, each member's `+0x91` bit 0x20, the actor the picked member or not,
the action record pointer at one of the harness's records. Set-up 34's
set-up: the id byte against each member's `+0x148` equal half the time, one
bit off otherwise. Its end hook: the battle-end byte at 0..3, 0xFD, 0x82, 8,
0xA or any; each member's `+0` bit 0, `+0x91` bit 0x40 and `+8`; the picked
dword at 0..2 with upper bytes 0 (as the only writer leaves it) or not. The
Angler: the data record index 0..7 or any; the count `+9` at 1 (the sound),
0, 2, 0x80; the coordinate, the step and the goal with the goal exactly one
step away half the time (the compare's equal side), one off, or any; the
spawn hook's `+1 == 7` and `+2 == 1` two times in three and each off by one
otherwise, the actor 3..10 or 0..2. The effect task: `+1` 0 (one entry),
`+2` 0..2, round-flag bit 2 half the time. `Gaist`'s hook entry: round-flag
bit 1 and the wait word at 0, 1, 0x100, 0x8000 or any.

**The group's disturbance** moves what the 46 read again after a call and
the standard one does not: the picked member `0x675F08` (after the first
`AbilityList_Add`), `0x904AAD`, a party member's `+0` bit 0 and `+0x91`
bit 0x40 (`Boss34_End`), the wait word. **Louder stand-ins**:
`Battle_RemoveFromTurnOrder` and `Sprite_PoseFromSet` each move a random
member's `+0x91` bit 0x40, `+8` or `+0` bit 0 (`Boss34_End` reads the member's
`+0x90` after the first and the next member's `+0` after the second);
@LOUDER@

@COVERAGE@

**The harness, for the coordinator.** BSH is the first user of `kTask`
(the four functions of the effect task): it needed nothing more - the slot
is `Sprite_Current` (one of the first four task slots), a stack table's
immediates are re-aimed through `Clone::imms`, and an immediate that names
a standard callee (`BattleFx_FreeTask`, `0x4AEE90`) resolves to that
callee's recorder (`RegisterHandler` finds the slot first), which ours
reaches through `BH_CALL`. Nothing was added to the harness in the group's
file.

## 5. Controls

`python controls.py` (the group's scratch script): each control one textual
change to `boss_sh.cpp` anchored on strings that occur once, then rebuild,
run (`BOF3X_SHADOW=boss_sh`, `BOF3X_BSH_RUN` the unit), restore; one rebuild
at the end. The rounds column is the planted function's mismatched rounds
(of 6,000, 8,000 for B34 and K43, 4,000 for B41 / B47 / B43). Plants in the
shared helpers (`Enter`, `Dispatch`, `HookDispatch`, `GiveAbility40`,
`EndWithStep`) are run on one unit and named after the function the table
shows.

@CONTROLTABLE@

## 6. Latent defects (Capcom's, kept)

- **Every dispatcher and hook indexes its table unchecked**, and
  `BossAnglerFx_Run` calls through its three-entry stack table by `+2`
  unchecked (past 2 it calls whatever the stack holds above the table: its
  own return address first). Ours aborts. Nothing in the group's code stores
  a state byte out of range.
- **`BossAngler_HookSpawnFx` uses `BattleTask_Create`'s answer untested**: with
  all 48 slots taken it answers 0xFF, and the original copies 0x80 bytes to
  `0x93A000 + 0x84 * 0xFF` (`0x94237C`), past the slots and past the image -
  a fault. Ours aborts naming the function.
- **`BossAngler_HookSpawnFx` copies from `0x93B960 + 0x128 * (actor - 3)`
  whatever the actor**: the hook runs from `BattleEnemy_RunAll` (word 2) with
  the enemy at `+1 == 7`, `+2 == 1`, and the acting actor `0x904B34` need not
  be an enemy then. With a party member acting (0..2) the source is 0x128..0x378
  bytes below the enemies - inside the task slots - and the task starts as a
  copy of task memory, not of the Angler. Ours copies the same (a read, no
  abort: the original does not fault there, and a Fatal would end a game it
  survives). Whether a member can be acting while the Angler is in state 7
  step 1 was not traced; state 7 (`0x437180`, the generic table's) was not
  read.
- **`Boss34_Event` indexes the party by the byte `0x675F08` unchecked**
  (`+0x91` of record `0x675F08`); only `Boss34_Setup` writes it, with 0..2,
  and it starts 0 (zeroed `.data`). If no member matches `0x669730` the byte
  keeps what an earlier set-up 34 left, or 0 - so the fight marks member 0 when
  nothing matches. Ours reads the same.
- **`Boss34_Exit` writes through `BossActor_Find(0)` untested** (the spawn
  helpers' defect, [`boss_h.md`](boss_h.md) section 6, in one more caller).
  Ours aborts.
- **Kind 42 (Torch) points `+0xFC` at `0x675F0C`**, twelve bytes of `.data`
  that nothing in the exe writes (a scan of the exe for the address finds
  only this store) and that are zero in the file - so every animation byte
  the generic states read for the Torch is 0. Every other kind of the band
  points at a table in the kinds' `.data` block (`0x64C7B0..0x64DDEC`). A
  PSX overlay's data section that the port's link did not carry over is the
  obvious reading, not proven: the sibling's `BOSS035` image was not
  compared.

## 7. What nothing reached

Nothing of the 46 went unreached by the fuzz: the coverage lists every table
entry and every callee each clone calls. What only a fight would show: the
chapter steps the end hooks hand back (0x15, 8, set-up 34's "one up"); the
member set-up 34 marks and what ability 0x40 is; the Angler's move and its
task's animation; Torch's zero animation bytes (section 6).

## 8. Calls across groups

**Out of BSH, raw** (`boss_sh_callees.h`): `0x446700`, `0x437450`,
`0x4376A0`, `0x4376F0` (engine code nobody owns), `0x446DE0`, `0x446E00` (the
end phase's steps, the harness's standard set). **Literals of other groups'
functions** stored as hooks, never called: `0x43E790` (BSE's
`BossHook_ExitTransition4`, set-up 35's exit), BH's `BossHook_EndPickWay`,
`BareRet`, `BareRetZero` (by name); `0x64D6CC` / `0x64D6D8` (kind 40's byte
tables, group BSG's to name) stored by kind 48. Everything else BSH calls is
ours and called by name. BSH calls no function of another wave-two group.

**Into BSH from outside the group** (for the rebinding pass):

- `Boss_SetupTable` entries 34, 35, 36, 41, 43, 47 and `BossKind_Table`
  entries 41..44, 48, 50, 54 (`.data`); `BattleBossFx_Dispatch`'s stack
  table slot 3 (`0x4357F6`, an immediate `0x43EB80`): cells and immediates,
  reaching ours through the entries' `jmp`.
- **`0x43EC10` (`BattleFx_ScriptUntilDone`) from three spell tasks**:
  `magic_s09.cpp`, `magic_s15.cpp` and `magic_s31.cpp` hold it as
  `kScriptUntilDone = 0x43EC10` in their stack tables (and their fuzz
  files' `Imm` rows): the same address, now a `jmp` to ours - the rebinding
  pass can name it `bof3::addr::BattleFx_ScriptUntilDone`.
- **`0x43E9D0` from kind 50**: `BossSample5_Hooks` holds the Angler's spawn
  entry (both kinds are BSH's).
- Otherwise no code of ours names BSH's addresses (a grep of `src/` for
  `0x43DEF0..0x43ECBF` outside `boss_sh*` finds only those spell files and
  BSE's comment on set-up 35).

## 9. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-28): the 46 extents as section
2 gives them. None was there; two host lines covered them (`0043C9F0 189F`,
`0043E290 1120`) and the smaller extents fix both.
