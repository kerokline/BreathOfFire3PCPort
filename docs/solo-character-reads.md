# The solo-character round: group M's reads (M2..M9)

**Status:** DONE (2026-11-12). Read-only: the source at `192a2075`,
`symbols.toml`, the docs named below, and `bof3/BOF3.exe` read through
`tools/pe_xref.py` and short capstone scripts. Nothing under `src/` or
`tools/` changed; the game was not run.

The design is [`solo-character-round.md`](solo-character-round.md) (M1 is in
its section 2). Each section gives the answer first, then the evidence. "Read"
means the function's body in `src/` was opened; ours are faithful
replacements, so a body read is a read of Capcom's behaviour. An inference is
marked as one.

**Method for the binary scans.** `python tools/pe_xref.py <addr>` and
`--range a:b`, run from the main checkout (it opens `bof3/BOF3.exe` by a
relative path; it writes only `analysis/pc_xref.json`, gitignored). Each hit
was mapped to its function with a scratch script that takes the greatest
`symbols.toml` `[[func]]` pc at or below the site
(`D:\bof3x-runs\solo1112\M\scratch\fn.py`). These scans see absolute
displacements only (`[0x904ab0]`, `[eax*4 + 0x903a7b]`). A write through a
pointer held in a register is not seen; where that matters the section says
what else was done.

## M2. Where a battle's results reach `CharacterRecords`, and when

**Answer.** The battle writes the records of its own members only, at four
points, all of them the game's own:

| What | Function | When |
|---|---|---|
| EXP (+0xC) | `BattleResult_AddExp` `0x4468B0` | the win's result screen (phase 5 step 1, the EXP tick) |
| Level gains | `Char_LevelUp` `0x498DE0`, from `BattleResult_Setup` `0x431D70` | the win's result, after the EXP |
| HP +0x18, AP +0x1A, +0x1C, status +0x10 (kept to bits `0x60A0`) | `Battle_WriteBackMember` `0x446A80`, ×3 from `Battle_WriteBackParty` `0x431770` | exit step 4.0, on every way out that returns to the field |
| A fallen member: HP 1, +0x1C 1, +0x1E up to 5, status `0x4000` and +0xB bit 2 cleared, `Char_RecalcStats` | `BattleEnd_Finish` `0x4317F0` | exit step 4.2 |

A record is found through the member's ObjTrio record: `+0x148` (the record
index `Field_PartyLoad` stored) for the write-back and the fallen, `+0x89`
through `CharId_ToRosterIndex` `0x4469D0` for the EXP. **A reserve member's
record is not touched by a battle, EXP included.** In the original a member
who is not in the battle gets no EXP. A member who is in it but out also gets
none.

So the champion's results reach its record by the game's own code, as soon as
the champion is the battle's member 0 (its `+0x148` is its own record). CB
does not need a write-back of its own (see "What the round doc must change").

**Evidence.**

- `Battle_WriteBackParty` calls `0x446A80(0)`, `(1)`, `(2)`, then
  `Window_ResetAll`, `LoadDatFile(2)` (`src/game/battle/battle_action.cpp:2192-2199`).
  `Battle_WriteBackMember` returns at once for a slot without `+0` bit 0, and
  writes the four fields to `CharRecord(member->char_index)`
  (`src/game/battle/battle_3.cpp:2134-2147`).
- `BattleResult_AddExp` loops the slots below `0x904AB0` (re-read each pass);
  `BattleResult_MemberTakesExp` answers 0 for an out member or one with
  `+0x134` bit 10; the record is `CharId_ToRosterIndex(slot +0x89)`; the sum
  is capped at 9,999,999 (`battle_3.cpp:2055-2079`).
  `BattleResult_SplitExp` divides the total by `BattleResult_CountExpShares`
  `0x4319B0`, the same test over the same slots
  (`src/game/battle/battle_result.cpp:140-153`, `:155+`).
- `CharId_ToRosterIndex` reads `Character_IndexById` `0x66972C` and answers 0
  when the byte is 7 (`battle_3.cpp:2085-2088`): EXP of a member whose id maps
  to record 7 is credited to record 0. (A fact of the code; what it means in
  the game is not asserted here.)
- `BattleEnd_Finish` (`battle_action.cpp:2220-2250`) then calls
  `Battle_ReloadPartyRecords` `0x446600`: each member below `0x904AB0` gets
  its `+0x80` copied back from its record (`battle_3.cpp:1955-1961`).
- Every other absolute-displacement write into the records block from battle
  code indexes by a member's own record: `pe_xref.py --range
  0x903A70:0x903F90`, writes grouped by function: `BattleObj_Fall`
  `0x4420A0` (a breaking accessory's slot, `src/game/battle/battle_obj.cpp:882-885`),
  `Effect_ActorStepThenHpToOne` `0x44DFD0` and `Effect118_RaiseCharByte1E`
  `0x44E720` (the target's `+0x1E`, `src/game/effect/effect.cpp:1943`,
  `:2073`), and `BattleResult_AddExp`. The pointer-held writes
  (`Battle_WriteBackMember`, `BattleEnd_Finish`) were read in the source.
- The dragon takeover copies into the ObjTrio records and `0x939AE0`, never
  into `CharacterRecords` (IDEAS I56; `BattleFx_RestoreParty`,
  `src/game/battle/battle_fx.cpp:1193+`).

Section 6 open 1 (EXP for the reserve): the original gives a non-member
nothing, so "the champion takes the whole total" is the original's rule for a
party of one. Any share for the reserve would be a new rule for the owner.

## M3. Every way out of a battle

**Answer.** Phase 5 (`BattleEnd_Steps` `0x64AF44`, by `0x904AA1`) has five
steps. The dragon reverts run first, in step 0 or step 3, before any way out
is taken. Every way out that returns to the field then goes through exit step
4 and `BattleEnd_Finish`, which steps `Game_Step` from 5 to 6. Mode 5 then
runs `GameMode5_Place` (`0x496130`, step 6) and `GameMode5_Leave`
(`0x496150`, step 7), which rebuilds the walkers with `Field_PartyLoad(0)`.

| Way out | Path | Reverts | Returns to the field |
|---|---|---|---|
| Win | step 0 (tasks, reverts) → `BattleEnd_AwaitMemberTasks` picks step 1 → win banner, result (EXP, level-ups, zenny, drops) → `BattleResult_AwardDrops` sets step 4 | step 0 | yes, through 4.0 / 4.1 / 4.2 |
| Loss (game over) | step 0 → step 2 (`BattleEnd_LossDispatch` `0x431540`) → `BattleLoss_Dispatch` `0x432430` → `BattleLoss_Restart` `0x432750`: `Task_Restart(Boot_Task)` | step 0 | **no**: the task restarts at boot; no write-back runs |
| Flee | `Escape_End` `0x44A4F0` sets phase 5 step 3 → `BattleEnd_RestoreDispatch` `0x4315B0`: tasks, `BattleEnd_AwaitRestore` `0x4315C0`, sound bank, step 4 | step 3 | yes |
| Event battle, any result | step 0's `BattleEnd_AwaitMemberTasks` does not pick a way out when `0x904AAA` is set; the end hook `[0x904B64]` (each boss set-up's) tail-jumps to `BattleEnd_EnterStep1` / `2` / `3` (`0x446DE0` / `0x446E00` / `0x446E20`) | step 0 | for steps 1 and 3; step 2 is the loss screen |
| A loss the story continues | an end hook sends the loss to step 3 (e.g. `BattleHook_Area189Script` `0x437720` with `0x904AE8` bit 0; boss hooks `Boss01`, `Boss04`, `Boss07`, `Boss25` end at `0x446E20`) | step 0 | yes, as a flee |
| Dragon revert | not a way out: `BattleEnd_StartMemberTask` `0x431220` starts a slot-12 task per member with `+0x134 & 3`; the task ends in `BattleFx_RestoreParty` `0x433DA0`; `BattleEnd_AwaitMemberTasks` / `BattleEnd_AwaitRestore` wait for every member's `+0x130` bit `0x2000` to clear | - | - |
| One battle into another | no battle code starts another battle: the only writers of `EventBattle_Id` `0x904AAA` are `Field_StartEventBattle`, `GameMode_Start`, `GameMode5_Leave` (to 0) and `BattleLoss_Restart` (to 0). A chain is two full passes of mode 5 with mode 2 between them | each | each |

**The point "after the battle has fully ended, after the game's own
reverts"**, once for every path that returns: **`GameMode5_Place` `0x496130`,
on entry** (`src/game/core/transition.cpp:1004-1009`). It runs once per
battle, after `BattleEnd_Finish`, after the write-backs, and before
`Party_PlaceAtSlots` looks for the leader by `0x904062[0]` and before step 7's
`Field_PartyLoad(0)`. The loss's path never gets there; the state it leaves is
replaced by the boot (and a load), so CB's buffer must be disarmed there
instead (`BattleLoss_Restart`, or `GameMode_Start` `0x495840`, which clears
`EventBattle_Id`).

**One exit hook already restores a party of its own** (see M4): chapter 6's
`Boss25_Exit` `0x43C1B0`, at exit step 4.1, sets `Field_MemberCount` to 3 and
copies the saved second list back from `0x939A10`
(`src/game/boss/boss_25.cpp:213-219`). It runs before `GameMode5_Place`. A CB
restore at step 6 that writes back what it saved at the battle's start would
undo it (inference from the order; not run). CB must restore only what still
holds its own values, or skip a battle whose party the game has already cut
to one.

**Evidence.**

- The step tables: `docs/battle_turn_steps.md` sections 1 and 2 (read
  2026-09-29; `BattleEnd_Steps`, `_TaskSteps`, `_WinSteps`, `_ExitSteps`,
  `_ResultSteps`, and the two unnamed tables `0x64AF70` / `0x64AF7C`, now
  named `BattleEnd_LossSteps` / `BattleEnd_RestoreSteps` in the source).
- `BattleEnd_AwaitMemberTasks` (`battle_action.cpp:2018-2058`): the wait on
  `0x2000`, the way out by `0x904AE8` only when `EventBattle_Id == 0`
  (`:2049-2052`), the hook `BattleHook_End` (`:2053`).
- `BattleEnd_RestoreDispatch` and its five steps (`battle_action.cpp:2117-2179`):
  `BattleEnd_TasksBegin` and `BattleEnd_StartMemberTask` again, then
  `BattleEnd_AwaitRestore`, then `BattleEnd_RestoreAwaitBank` sets step 4.
- `Escape_End` (`src/game/battle/battle_item_cmd.cpp:2437-2445`);
  `BattleResult_AwardDrops` sets `0x904AA1..A4` to 4, 0, 0, 0
  (`battle_result.cpp:453-466`).
- `BattleEnd_EnterStep1..3` (`battle_3.cpp:2267-2287`); their callers are the
  boss end hooks (`grep` of `kEndWin` / `kEndThird` / `0x446E20` in
  `src/game/boss/`) and `BattleHook_Area189Script`
  (`src/game/battle/enemy_op.cpp:1591-1606`), which `0x436110` stores at
  `0x904B64` (`symbols.toml` 0x437720's evidence).
- `BattleLoss_Restart` clears `EventBattle_Id` and the phase bytes and calls
  `Task_Restart(Boot_Task)` (`src/game/battle/battle_loss.cpp:195-215`;
  `docs/battle_e1.md` 1.6).
- Writers of `0x904AAA`: `pe_xref.py 0x904AAA`, the `mov byte ptr
  [0x904aaa], ...` lines: `0x4327A7` `BattleLoss_Restart`, `0x4410BB`
  `Field_StartEventBattle`, `0x495899` `GameMode_Start`, `0x4961E3`
  `GameMode5_Leave`.
- `GameMode5_Steps` `0x656A84` (8 dwords read from the exe):
  `0x495EA0 0x495EC0 0x495F90 0x495FD0 0x4960D0 0x42E370 0x496130 0x496150`.
  Step 5 is the battle frame `0x42E370` (`symbols.toml`). `BattleEnd_Finish`
  ends with `Game_Step + 1` (`battle_action.cpp:2243`), so step 6 follows it.
  `GameMode5_Leave` calls `Field_PartyLoad(0)` and `Party_PlacesByList`
  (`transition.cpp:1022-1027`).

## M4. Event battles, the battle count, members read by id

**Answer, part 1: event battles enter through `GameMode5_Script`. Confirmed.**
`Field_StartEventBattle` `0x4410B0` sets `EventBattle_Id` and puts the leader
in state 5. Leader state 5's sub-state 1, `Field_EncounterWait` `0x52F490`,
is the one immediate writer of `Field_Request = 3`; `GameMode_Field` turns
request 3 into `Game_Mode` 5, step 0. A random encounter takes the same
path. So every battle runs `GameMode5_Steps` 0..7, step 4 included. The event
battle's flags `0x904AE5` skip parts of steps 1, 2 and 4 (bit 1, 2, 4), not
the steps themselves.

**Correction to section 0: the battle's member records are built in step 3,
not step 4.** `GameMode5_Load` `0x495FD0` (step 3) calls
`Field_PartyLoad(1)`: `Field_MemberCount` records from the **second** list
`0x904065`, each record copied whole from `CharacterRecords` and its `+0x148`
set (`src/game/field/field_6.cpp:2092-2119`). `GameMode5_Script` (step 4)
only counts the second list into `0x904AB0`. And step 1, `GameMode5_Turn`,
already loaded the battle graphics of the current party set by
`PartySet_Select(0x90412C & 0x7F, 1 or 2)` unless `0x904AE5` bit 4
(`transition.cpp:909-913`). So the champion must be in the set before step 1,
and in the second list with `Field_MemberCount` = 1 before step 3.

**The game already does exactly this once.** Chapter 6's `SortParty`
(`Scena06_Run08` step 0, `src/game/scenario/scena_06_3.cpp:324-344`) sorts
the three ObjTrio records, copies the second list to `0x939A10` and its count
to `0x939A02`, sets the second list to `(0, 0xFF, 0xFF)` and
`Field_MemberCount` to 1, and leaves the first list as it was. The event
battle then has one member, and `Boss25_Exit` puts the list and the count of
3 back at exit step 4.1 (`boss_25.cpp:213-219`). `GameMode5_Leave`'s
`Field_PartyLoad(0)` then reloads the three walkers from the first list. This
is the round's combat team, built by Capcom, with no ObjTrio backup at all.

**Answer, part 2: the writers of `0x904AB0`.** Three, by `pe_xref.py
0x904AB0` (102 references, three of them stores):

| Site | Function | Writes |
|---|---|---|
| `0x496126` | `GameMode5_Script` `0x4960D0` | `Party_Count(1)` (`transition.cpp:997-999`) |
| `0x4EB478` | `Accession_ApplyB` `0x4EB350` | 1, after saving the count in `0x904B8F` (`src/game/magic/accession.cpp:325-332`) |
| `0x433DED` | `BattleFx_RestoreParty` `0x433DA0` | the saved count back (`battle_fx.cpp:1197-1199`) |

Our DLL writes it only in the fuzz files (`*_fuzz.cpp`, under
`BOF3X_SHADOW`) and the harnesses' set-ups. Not seen by the scan: a block
clear through a pointer over `0x904AB0`; none was found in the source's
battle resets (`Battle_Init` `0x42E4A0`'s list in `symbols.toml` does not
include it).

**Answer, part 3: battle and boss code that reads a member by slot or by
id.** No battle code was found that needs a given character to be present.
What exists:

- **Behaviour by the acting member's id** (the member is there because it
  acts): `char_id == 6` (`battle_action.cpp:1814`), `!= 4` in
  `BattleEnd_StartMemberTask` (`:1986`), `== 4` (`battle_obj.cpp:1094`),
  `== 0` in Accession (`accession.cpp:154`, `:196`), `== 4` in Kaiser
  (`src/game/magic/kaiser.cpp:217`), `== 0xA` (`effect.cpp:1073`), `== 7`
  (`src/game/effect/effect_kind_9d.cpp:401`). Grep of `char_id ==/!=` and
  `[0x89] ==/!=` over `src/game/battle`, `boss`, `magic`, `effect`.
- **`Boss_SetByLeaderId` `0x43B130`** (`src/game/boss/boss_nina.cpp:879-887`):
  set-ups 18..20's hooks pick `0x92BF18` by ObjTrio record 0's id (0, 1, 5, 6;
  any other id leaves the byte as it was). `Party_PlacesByList` later passes
  that byte to `Party_DropIn` when `0x904AE5` bit 7 is set
  (`field_6.cpp:1532-1535`). With a champion of another id, the byte keeps an
  older value. For the boss sweep.
- **`Boss25_End`** writes saved HP and AP into ObjTrio record 0
  (`boss_25.cpp:200-207`): the fight's one member, whoever it is.
- **The field's placement after a battle and in scenes**, which is not
  battle code: `Party_DropIn` `0x531F90` kind 0 needs exactly N members and
  each wanted id in an ObjTrio record, else it returns 1 and sets nothing up
  (`field_6.cpp:1066-1116`; `docs/area-entry.md` 1). That is M8's problem.

## M5. A quicksave taken mid-battle

**Answer.** The snapshot copies Capcom's `.data` and `.bss` and the draw pool:
`0x5DA000` to `0x987000` in the exe host, `0x5DA000` to `0x93E000` (the page
after `.bss`'s end `0x93D6EC`) plus the pool in the hosted build. Our DLL's
own globals are outside it. `CharacterRecords`, both lists, the ObjTrio
records, `0x904AB0`, `0x90412C` and `0x939A10` are inside it, so the game's
half of a mid-battle state comes back by itself. CB's saved map team does
not.

Two ways to carry it:

1. **A participant** (`Snapshot_Register`): a small versioned record, saved
   at the take, restored after the span. Three exist; `BEHAVIOURS`
   (`src/game/field/field_7.cpp:767-802`, registered at `:1341`) is the
   pattern, including a refusal when the record needs a behaviour that is now
   off (`:801-802`).
2. **Bytes inside the span**: the 0x914 bytes from `0x93D6EC` to `0x93E000`
   are in the span in both hosts and are not Capcom's `.bss`. Nothing in
   `src/` uses them today (grep of `0x93D6EC` / `kBssEnd`). Using them is a
   new layout claim that the state hash, the census and the exe host would
   all have to know about.

**The participant is simpler**: an established pattern, no layout change,
and the record can say "armed / not armed" with a version. A participant that
is not armed writes a record that restores to "not armed", so a quicksave
taken with the mode off restores the same with it on.

**Also found.** The quicksave file starts with an ordinary save half built by
`Save_BuildBlock` from the live block (`docs/quicksave.md` table at line 103).
Taken mid-battle in the mode, that block's second list (`+0x685`) holds the
champion alone while the first (`+0x682`) holds the map team. Whether that
half is ever loaded as an ordinary save was not settled here; if it is, it
is a save whose two lists disagree, which the save editor's check refuses
(`src/launcher/save/block.cpp:689-711`).

**Evidence.** `src/hook/snapshot.h:1-23` (the span, participants);
`src/hook/snapshot.cpp:196-223` (`CurrentSpan`: `.data` to `kOwnVa` in the
exe host; to the page after `kBssEnd` hosted); `src/hook/exe_layout.h:28-48`
(`.game` `0x5C4000..0x93F000`, `kBssEnd` `0x93D6EC`, `.pool`
`0x93F000..0x987000`, ours from `0x987000`); participants by `grep
Snapshot_Register(`: `BEHAVIOURS`, `SOUND` (`src/game/platform/sound_ours.cpp:1289`),
`GFX` (`src/render/gfx_snapshot.cpp:449`).

## M6. The character ids, the party sets, and "joined"

**Answer, part 1: ids to characters.** The game maps an id to a character
record through `Character_IndexById` `0x66972C` (24 bytes, read from the
exe):

| Ids | Record | The record's default name (DIV-0020) |
|---|---|---|
| 0, 7, 9 | 0 | Ryu |
| 1, 8 | 1 | Nina |
| 2 | 2 | Garr |
| 3 | 3 | Teepo |
| 4, 0xE | 4 | Rei |
| 5 | 5 | Momo |
| 6 | 6 | Peco |
| 0xA | 7 | Whelp |

Ids 0xB..0x17 other than 0xE map to record 0. The names are the eight default
records' names in record order, as DIV-0020 lists them (read from the disc's
default records into the eight name fields, `docs/DIVERGENCE.md` DIV-0020;
`NewGame_InitCharacters` copies records 0..6 and the whelp's into slot 7,
`src/game/battle/battle.cpp:120-136`). `Character_IndexById`'s tier is
`hypothesis` in `symbols.toml`, but its note cites four readers that use it
as id-to-record (`Area_Enter`, `Area192_RestoreCharacters`,
`BattleEquipWin_Draw`, `Item_CanUse`'s callers), and `Field_PartyLoad`,
`Party_Join` and `Party_SwapMembers` use it the same way (`field_6.cpp:2107`,
`:2171`; `docs/area-entry.md` 1). What ids 7, 8, 9 and 0xE mean in the story
(why a record has more than one id) is not asserted here.

**Record 7 is not one character all game.** At New Game it is the whelp's
record. `Scena06_GuestRecord` `0x54E700` overwrites its HP, two equipment
bytes and stats for chapter 6's guest (`scena_06_3.cpp:2124-2147`), and the
battle extra uses it as its guest (`docs/save-block/range-b-characters.md`
section 4). A champion list that offers record 7 needs the owner's word.

**Answer, part 2: the party-set rows.** Rows 0..6 use ids 0..6 only. Rows
7..18 use ids 2..8 and 0xA, plus 9 in one row; ids 0 and 1 are in none of
them. By the table above, the rows differ by which id records 0 and 1 carry:
0 and 1 in rows 0..6, 7 and 8 in rows 7..18 (9 once). The scripts agree:
the chapter call tables in `scena_01.cpp`..`scena_07.cpp` (and their parts)
build parties from ids 0..6 and 9; `scena_08.cpp`..`scena_16.cpp` from 2..8
and 0xA, with one `PartySet_Load(0, 1, 5)` in chapter 14 (a grep of the
`Join(` / `Leave(` / `Set(a, b, c)` calls per file; the file per chapter is
the source's naming of `Scenario_CallA`'s table `0x660B84[chapter]`,
`field_6.cpp:2262-2263`). That the two halves are two eras of the story is
an inference.

**Answer, part 3: how the game records "joined".** By **`CharacterRecords`
+0xB bit 0**, per record. The lists hold who walks; the reserve is not
stored anywhere:

- **The reserve list `0x6BDFD4` is the Tactics screen's working copy.**
  `TacticsMembers_Build` `0x58FEF0` fills it each time the screen opens: every
  record of the eight with bit 0 whose `+9` is not in the first list
  (`src/game/field/field_ability_2.cpp:3397-3428`). The second members screen,
  `PartyForm` (`ShopMode_States[6]`, opened by `Field_Request 6`), builds its
  own reserve `0x6BC894` the same way (`src/game/field/party_form.cpp:138-173`).
  Neither is in the save block.
- **Bit 0 is cleared when the story takes a character away**, and set when
  one joins or comes back. Writers, by `pe_xref.py --range 0x903A70:0x903F90`
  filtered to `+0xB`: set by `Party_Join` (even when refused,
  `field_6.cpp:2168-2178`), `Party_AddToLists` (`src/game/menu/char.cpp:1010-1013`),
  `Scena05_ReloadJoin516`, `Scena05_Run20` (records 1, 5, 6;
  `src/game/scenario/scena_05_2.cpp:1675-1677`), `Scena06_Run03` (records 6
  and 2; `scena_06_3.cpp:813-814`), `Scena08_PartyBitsToRecord0` (all eight,
  then 1..7 cleared; `src/game/scenario/scena_08_2.cpp:217-220`); cleared by
  `Scena01_Set934Party9`, `Scena01_Scene06`, `Scena02_Leave43`,
  `Scena05_EnterArea` (records 5 / 6 by turns, `scena_05_2.cpp:292-344`),
  `Scena06_EnterArea` (`scena_06_3.cpp:478-489`), `Scena07_Leave2Out1562`,
  `Scena08_Party7`, `Scena10_Leave5`, `Scena14_Out10Party748`. The pattern
  in the removers' comments is "loses +0xB bit 0, then leaves" (e.g.
  `src/game/scenario/scena_02.cpp:39-41`).
- **`Party_Remove` does not clear it** (`field_6.cpp:2205-2260`): a member
  dropped from the lists by a scene that does not also clear the bit is in
  the reserve.
- Scripts can test it: condition 15, `EventCond_RecordBit0` `0x57C2D0`
  (`src/game/field/event_cond.cpp:674-676`), by record index.

**The proposed test, "the champion is present":** the champion is a
**record** (0..7), not an id; present means `CharacterRecords[R] +0xB & 1`.
The id to put in the lists is the one the record holds now. Of the ids that
map to a record, the game's own code takes the record's `+9`
(`Party_SwapMembers` copies the record, and `+0x89` is then the record's
`+9`, `docs/area-entry.md` 1). **Open:** the only writers of a record's `+9`
the scans found are `Scena01_Set934Party9` (record 0 to 9) and
`Scena01_Party034` (back to 0) (`src/game/scenario/scena_01.cpp:46-58`;
`pe_xref.py` for `+9` and a capstone pass over every function that loads a
record base); yet the save spec records record 0's id as 0, 7 or 9 in
measured saves (`docs/save-block/range-b-characters.md` section 4), and the
rows of chapters 8..16 use 7 and 8. Whether record 0 holds 7 (and record 1
holds 8) in those chapters, and who writes it, would be settled by a peek of
`0x903A79` and `0x903B1D` on a chapter-8-or-later save, or by finding the
writer (a block copy or a pointer write the absolute scan cannot see). Until
then, SC should pick the id as: the record's `+9` if a party-set row holds it
with the walker, else the id among those mapping to R that the current set
row holds.

## M7. The "must stay" mark, `+0xB` bit 1 (and bit 0)

**Answer.** Bit 1 is set at New Game on record 0 and is never cleared there;
it is set and cleared on two other records by two chapter scenes. It is
saved. It is read by both members screens and by `Party_AddToLists`.

Every writer of bit 1 found:

| Writer | Record (id) | What |
|---|---|---|
| New Game, `NewGame_InitCharacters` `0x437820` | 0 | the default record's `+0xB` is 3 (bits 0 and 1); records 1..6 and the whelp's are 0 (a byte read of `0x64B390 + 0xA4 k + 0xB` and `0x64B80C + 0xB`) |
| `Party_AddToLists` `0x591CC0` | any | `|= 3` (`char.cpp:1013`); its one caller is `Scena06_Run12` step 8 with id 2 (`scena_06_3.cpp:1572`) |
| `Scena06_Run12` step 0xB | 2 | `&= 0xFD` (`scena_06_3.cpp:1583`) |
| `Scena08_Scene11` step 7 | 0 (id 7), 4 (id 4) | both `|= 2` (`scena_08_2.cpp:1309-1313`) |
| `Scena08_Scene11` step 0x13 | 0 (id 7), 4 (id 4) | id 7's `|= 2`, id 4's `&= 0xFD` (`scena_08_2.cpp:1385-1391`) |

So **record 0 keeps bit 1 for the whole game**: no writer found clears it.
Bit 0's writers are M6's list. Both bits are in the save block (the records
are saved whole at `+0x90`, M9) and come back with a load.

Readers of bit 1: `TacticsMembers_Swap` refuses to move a marked member
(`field_ability_2.cpp:3488-3489`); `TacticsMembers_Enter` marks the row
(`:3150`); `PartyForm_Swap` refuses the same way (`party_form.cpp:562-563`);
`PartyForm_Draw` and `MenuList_ReserveWinDraw` `0x59AA80` draw it dimmed;
`Party_AddToLists` will not replace a marked member (`char.cpp:1024`). Sites
by `pe_xref.py` (`test byte ptr [eax*4 + 0x903a7b], 2` or `cl`): `0x58F854`,
`0x590195`, `0x5901BF`, `0x581655`, `0x58167D`, `0x591D43`, and the byte loads
in `0x581720` and `0x59AA80`.

**Consequence for FS (owner's answer 4):** if "must stay" means bit 1, the
map team is always the walker **plus record 0's character**, unless the walker
is that character. A solo walk without record 0 never happens. That may not
be what the owner meant; it needs the owner's word (see "What the round doc
must change").

Method limit: the absolute scan covers `[reg*4 + 0x903a7b]` and fixed
records; the capstone pass over functions that load a record base found no
`[reg + 0xb]` store to a record. A store through a record pointer passed in
from elsewhere would not be seen; none was found in the source by grep of
`[0xB]` / `.flags` stores on `CharRecord` / `CharacterRecords[...]`.

## M8. Telling from outside that a script changed the party

**Answer.** There are about fifteen writers of the lists and the count, in
the field, the scripts, two menus and one boss exit. Hooking each is fragile.
The cleanest test is **a signature compared at one point**: the two lists,
`Field_MemberCount`, and the eight records' `+0xB` bits 0 and 1. If it
differs from what FS left after its last fold, a script (or a menu) changed
the party.

The point is **`Field_LeaderStand` `0x52DA70`, after its first three early
returns** (`src/game/field/field_4.cpp:455-458`): `Field_ScriptFlags` bit 8
clear, `Field_ScriptFlags2` bit 6 clear, `Field_Request` not 5. This is the
leader's state 1, sub-state 0, the first frame where the six button tests
(the L swap among them, `Field_LeaderWalk`'s list, `:489-497`) can run. It is
the frame control is back with the player: a scene that sets
`Field_ScriptFlags` bit 8 (`Party_DropIn` does, `field_6.cpp:1113`) holds the
leader out of it until the scene clears the bit. Walking passes through it
too (the stand starts the walk, `:465-477`). A fold here comes before the L
swap's test in the same frame.

The writers (for FS's own list and the self-test):

- `Party_Join` `0x533EF0`, `Party_Remove` `0x534030`, `Party_AddToLists`
  `0x591CC0` (both lists and the count);
- `Party_DropIn` `0x531F90` (393 call sites): rewrites the first list's
  order for a kind-0 entry, never the second list or the count
  (`field_6.cpp:1075-1091`);
- scene builders that write the lists directly: `Scena03_Set012Party0`,
  `Scena04_Set015Party01`, `Scena14_Party015Lists785`, `Scena16_PartyReset`
  (all six bytes `0xFF`), `Scena08_Scene8`, `Scena06_Run08`'s `SortParty`
  (second list only), `Boss25_Exit` (second list and count), from `pe_xref.py
  --range 0x904062:0x904068` (absolute stores); and those `docs/save-block/range-c-party.md`
  lists for `+0x682` / `+0x685` (`Field_LeaderStepLands`, the formation
  swaps);
- the two members screens: `TacticsMembers_Swap` `0x590020` and
  `PartyForm_Swap` `0x581590`. `PartyForm` is the screen `Field_Request 6`
  opens, which `Party_Join` raises when a fourth member joins
  (`field_6.cpp:2175-2177`) and which scenes ask for directly (`Request(6)`,
  `scena_08_2.cpp:1308`, `:1386`; `scena_06_3.cpp:1573`). It ends in
  `PartyForm_Reload` (`party_form.cpp:338-427`): `PartySet_Load` of the first
  list, then `Field_PartyLoad(0)`;
- `Scenario_CallA` `0x5341A0` entries: the party builders above are its
  entries; the call itself is a tail jump through `0x660B84[chapter]`
  (`field_6.cpp:2262-2300`).

**A risk the fold does not cover.** Scenes that test the party they find,
rather than building one, see the folded map team. `Party_DropIn` kind 0
(exactly N members, each wanted id held) returns 1 and sets nothing up when
the team is one walker; kind 0x80 (at least N) the same. Most callers ignore
the answer (`grep` of the area callers; `docs/area-entry.md` 1: 207 of 393
pass it on). What a scene does with members whose scripts were never set up
is not read here (inference: it can hang). The same holds for script
condition 15, which tests bit 0 and so sees reserve members as "with us".
This is for RT's routes and the owner; see "What the round doc must change".

## M9. What a save holds of the party, and what a load rebuilds

**Answer.** `Save_BuildBlock` `0x5806F0` copies the live block
`0x9039E0..0x904A90` (0x10B0 bytes) whole into the staging copy and sums it.
Of the party that is: the **eight whole character records** (`+0x90`, 8 x
0xA4), **both lists** (`+0x682`, `+0x685`), the formation `+0x680` and its
unlocked cells `+0x681`, the party set `+0x74C`, and a summary at `+0xCA0`
built from the first list (the leader's name, three portraits). It does
**not** hold the ObjTrio records (`0x802D40` is outside the block; only the
leader's position and facing are copied in), `Field_MemberCount`, or any
reserve list.

A load (`LoadMenu_Read` then `LoadMenu_Apply`) copies the block back,
then: `Field_MemberCount = Party_Count(0)`; `PartySet_Select(+0x74C & 0x7F,
3)` and `(…, 0)`, each waited for; `Field_PartyLoad(0)`, which rebuilds the
walkers from the first list and the records.

**So option (b) of section 6 open 2 is cheap:** the block holds ids, not
member records. A save in the game's own shape needs, in the written copy
only: the two lists filled to the members wanted (same ids, `0xFF` last), the
party set `+0x74C` set to a row that holds them (with bit 7, as a field save
leaves it), the summary's portraits `+0xCA5` from the new first list, and the
checksum word `+0x70` summed again. The plainest way is to fill the live lists
and the set byte just before `Save_BuildBlock` and put them back just after,
so the game's own code builds the portraits and the checksum (inference; not
built). The records need nothing: every joined character's record is in the
block already. The quicksave's save half (`Save_QuickWrite` and the snapshot)
goes through the same function.

**Evidence.** `Save_BuildBlock` (`src/game/shop/field_save.cpp:932-974`):
the leader's fields `0x903A07..0x903A0C` from ObjTrio record 0, the summary's
three ids from `0x904062..` at `0x904685..` (`:956`), the copy and sum of
`rest_2c::at::kBlockBytes` into the staging buffer, the checksum
(`:965-972`). Block offsets: `docs/save-editor-spec.md` section 7 and
`docs/save-block/range-c-party.md` (the lists, `+0x74C`, and "Must agree with
something else"). The load: `LoadMenu_Read` and `LoadMenu_Apply`
(`src/game/core/sound.cpp:1060-1091`, `:1136-1170`).

## What the round doc must change

| Q | Premise of the round doc (sections 0, 3, 4) | Verdict |
|---|---|---|
| M2 | CB "the champion's results to `CharacterRecords`" (section 4) | **Corrected:** the game writes them itself (EXP, levels, HP / AP / status, the fallen) because the champion is the battle's member with its own `+0x148`. CB writes nothing back. Section 6 open 1: the original gives non-members no EXP. |
| M3 | Section 0 and CB: restore "after the battle has fully ended, after the game's own reverts", once per way out | **Confirmed, and it is one point:** `GameMode5_Place` `0x496130` on entry, for every path that returns. The loss without continuation never returns (boot restart): disarm there or at `GameMode_Start`. Needs a decision for CB: `Boss25_Exit` restores its own party at step 4.1, before that point. |
| M4 | Section 0: build the champion "in `GameMode5_Script` before it reads `Party_Count(1)`", "the map team's records and both lists go to a buffer" | **Corrected:** the records are built in step 3 (`GameMode5_Load`, `Field_PartyLoad(1)` from the second list); the battle graphics load in step 1. Set the second list to the champion and `Field_MemberCount` to 1 before step 3, the party set before step 1. No ObjTrio backup is needed: step 7's `Field_PartyLoad(0)` rebuilds the walkers from the records. Chapter 6's `SortParty` + `Boss25_Exit` is Capcom's own version of this. Event battles enter the same way (confirmed). |
| M4 | CB "lists and count to one member" | **Refine:** leave the first list alone (as `SortParty` does) and set the formation `0x904060` to 0 while one member fights, as `Party_Remove` does when a party drops to one (`field_6.cpp:2254-2258`), putting it back after. |
| M5 | CB "M5 says where [the buffer] lives" | **Decided:** a snapshot participant (the `BEHAVIOURS` pattern), not bytes in the image. |
| M6 | Section 3: "the champion is in a party list or the reserve" | **Corrected:** the test is `CharacterRecords[R] +0xB` bit 0 for the champion's record R; the reserve is a screen's working copy built from that bit. The champion is a record, not an id (records 0 and 1 have several ids). **Open:** which id record 0 / 1 hold in chapters 8+ (see M6). **Needs a decision:** record 7 is reused (whelp, chapter 6 guest, battle extra guest). |
| M7 | Section 3: "must-stay members walk too" | **Needs the owner's word:** bit 1 is on record 0 from New Game and never cleared, so the rule as written makes record 0's character walk beside every walker all game. The two other marks are short scenes in chapters 6 and 8. Saved: yes. |
| M8 | FS "the fold at M8's frame" | **Confirmed, with the frame named:** `Field_LeaderStand` after its first three returns, by a signature (two lists, count, the eight bits 0 and 1). **Needs a decision:** scenes that test the party they find (`Party_DropIn` kind 0 / 0x80, condition 15) see the folded team; the round's "cutscenes get every member they ask for" holds only for scenes that build their own party. RT's routes are where this shows. |
| M9 | Section 6 open 2: "(b) if M9 shows the block holds ids and not records" | **Confirmed: ids.** (b) is cheap: lists, `+0x74C`, portraits and checksum in the written copy; the records are already whole in the block. |

## Found on the way

- **A second members screen.** `PartyForm` (`ShopMode_States[6]`,
  `src/game/field/party_form.cpp`) is the screen `Field_Request 6` opens: a
  fourth member joining (`Party_Join`), or a scene. Its reserve `0x6BC894`
  has the same flag-overlap defect as the Tactics screen's from the fifth
  reserve member (its own comment, `party_form.cpp:134-137`;
  [`field_s.md`](field_s.md) "L1 `PartyForm_Setup` writes the reserve's
  flags past their three pairs"). The round's MS group lists only the
  Tactics screen; a team of one with five in reserve reaches both.
- **The game already walks one member** in `PartyForm_Reload`'s "lone" path
  (camp flag `0x904152`, story flag `0x77`, area `0xBB` / not `0x85`):
  record 0 alone, `Field_MemberCount` 1 (`party_form.cpp:343-360`). A
  reference for FS's one-walker field.
- **`CharId_ToRosterIndex` sends record 7's EXP to record 0**
  (`battle_3.cpp:2085-2088`): Capcom's rule, noted for CB's checks with a
  champion whose id maps to record 7.
- **`Party_Join` marks a refused join as joined** (it sets bit 0 before the
  three-member test, `field_6.cpp:2163-2178`; the source already says so).
  With a map team of one the join is never refused, so `Field_Request 6`
  (the choose-who-leaves screen) would not open where the original opens it
  (inference). For FS.
- No defect was found that needs a `known-defects.md` number.
