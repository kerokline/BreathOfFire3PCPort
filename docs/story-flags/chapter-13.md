# Story flags: chapter 13

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Source: `src/game/scena_sc13.cpp` (the `Scena13_*` functions, lines 167..1410),
and the area handlers that write or test row 13: `area_w3f.cpp` (areas 143, 144,
145), `area_w2f.cpp` (areas 111, 112) and `area_w4a.cpp` (area 155). Row 13 is
`Cond_Flags + 0x68`, save block `+0x5B0 + 0x68` = `+0x618`. Areas are given in
hex as the code has them, with decimal in brackets the first time. "Zone" is
`Cond_ByteFD`. "Run r step s" is `MoveScript_Var7` = r and the step byte
`MoveScript_PartyRecords[0x65]` = s.

## Start

**Who starts it.** `Scena12_Run9` step `0x2F` (`scena_sc12.cpp:1503-1509`) sets
chapter 12's flag `0x3F`, calls `Field_SetStatus80` (`Field_StatusBits |= 0x80`)
and then `Field_ChangeArea(0x7E, 0x500000, 0x1E0000, 4)`. The area set-up sees bit
`0x80`, adds 1 to the chapter byte `Cond_ByteFA` and calls `Scenario_Start(13)`
(`mode_flow.cpp:548-555`). So chapter 13 starts on arrival in area `0x7E` (126).

**What `Scenario_Start` does to the row** (`title_states.cpp:404-422`): it zeroes
the row's first dword only (`SetLong`, flags 0..31) and points `Chapter_FlagRow`
at it. Flags 32..63 are not cleared. Reading, not checked: on a normal route
they are still 0, because no earlier code writes row 13.

**State 0, `Scena13_Start`** (`scena_sc13.cpp:178-186`): `ScriptFlags_Set40`,
`Draw_PassFlags` `0x1F`, `Party_HealJoined` (the helper `PartyPass`, `:96`),
call-table A entry 0, event flag `0x43` (67) set and `0x42` (66) cleared, then
state 1. It sets no chapter flag.

**State 1, `Scena13_EnterArea`** (`:192-295`). In area `0x7E` with flag 0 clear
(`:222-228`) it calls `Party_DropIn(0)`, sets the camera and starts run 1 at
step 0. Run 1 (`:305-350`) plays a camera effect, waits for counter 0 to reach 8,
and at step 3 sets **flag 0** (`:329`). Then it calls
`Field_ChangeArea(0x7E, 0x4E0000, 0x2C8000, 1)`.

## Flags

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena13_Run1` step 3, `scena_sc13.cpp:329` | The opening scene in area `0x7E` (126), before `ChangeArea(0x7E, ...)` again | - | `EnterArea13Body` `:222`: run 1 starts in area `0x7E` while clear | progress |
| 1 | `Area155_ChoiceGiveItem4D`, `area_w4a.cpp:788` and `:793` | A choice in area 155 (`0x9B`). Answer 2 gives item 1/`0x4D` (message `0xB`); any other answer, or a full inventory, shows message `0xA`. The flag is set on both paths | - | Nothing found in code | unknown |
| 2 | `Scena13_Run2` step 2, `:373` | After the first effect `0x75` in area `0xC2` (194) | - | `Scena13_StepHook` `:1379`: area `0x9B` zone 0, the cell that starts run 2 is live while clear. `EnterArea13Body` `:276`: area `0xC2` set-up A (`Scena13_SpawnPairA`) while clear | progress |
| 3 | `Scena13_Run2` step 4, `:386` | The second step of run 2 in area `0xC2` | - | `EnterArea13Body` `:286`: with 3 set and 4 clear, area `0xC2` set-up B (`Scena13_SpawnPairB`) | progress |
| 4 | `Scena13_Run2` step 6, `:398` | The third step of run 2 in area `0xC2` | - | `EnterArea13Body` `:286` (see flag 3) | progress |
| 5 | `Scena13_Run2` step 9, `:419` | Call-table A entry 3 and stream 0 start. Step `0xA` then changes to area `0x97` (151) | - | Nothing found in code | progress |
| 6 | `Scena13_StepHook`, `:1321` | Area `0x7F` (127) zone 0, a step on x `0x38000`, z `6..8` | - | Nothing found in code | unknown |
| 7 | `Scena13_Run3` step 4, `:510` | End of run 3's first part. It starts in area `0x91` (145) zone 0 (`:265-269`) | - | `EnterArea13Body` `:265`: run 3 step 0 starts while clear | progress |
| 8 | `EnterArea13Body`, `:272` | First entry to area `0x91` zone 4: `Party_DropIn(6)` | - | `EnterArea13Body` `:270`, the same entry | once-only set-up |
| 9 | `Scena13_Run3` step `0xD`, `:552` | After messages `0x18` and `0x19`, before `ChangeArea(0x91, 0x3F8000, 0x5E0000, 0x88)`. Run 3 step `0xA` is started by `Area145_ChoiceRun3` (answer 0, `area_w3f.cpp:597-603`) | - | `Area145_CellHook` (`area_w3f.cpp:744`, row 13 by address): area `0x91`'s cell records arm tail kind 20 only while set | progress |
| 10 | `Scena13_ArriveHook`, `:1407` | Area `0x91` zone 5, arrival at x `0x40..0x41`, z `0x6A`; then `MoveCmd_TestFB(0x40, 0x6B)` | - | `Scena13_ArriveHook` `:1404`: the arrival acts once | once-only set-up |
| 11 | `Scena13_Run3` step `0x18`, `:592` | Run 3's second part: area `0x58` (88), effect `0x80`, then `ChangeArea(0x70, ...)`. It starts from `Scena13_StepHook` `:1361` (area `0x91`, event flag `0x2D` set) | - | `Scena13_StepHook` `:1361` (the trigger works once). `EnterArea13Body` `:209`: area `0x58` camera set-up from area `0x91` while clear | progress |
| 12 | `EnterArea13Body`, `:219` | First entry to area `0x70` (112) zone 4: `Party_DropIn(2)` | - | `EnterArea13Body` `:196`: with 12 set and 13 clear, area `0x56` (86) zone 1 from area `0x70` starts run 4 step `0xA` | once-only set-up |
| 13 | `Scena13_Run4` step `0xB`, `:625` | After the effect `Effect_SpawnAt(4, 5, ...)` at member 0 in area `0x56` | - | `EnterArea13Body` `:196`, `:202` | progress |
| 14 | `Scena13_Run4` step `0x17`, `:647` | Run 4 step `0x14` in area `0x56` (message 2), before `ChangeArea(0x58, ...)`. It starts from `Scena13_StepHook` `:1305` | - | `Scena13_StepHook` `:1305`: area `0x56` zone 1 trigger while clear. `EnterArea13Body` `:202`: area `0x56` zone 0 with 13 set and 14 clear calls call-table A entry 4 | progress |
| 15 | `Scena13_Run4` step `0x1D`, `:689` | After message `0x14` and request 6 in area `0x56` | - | `Scena13_StepHook` `:1310`: in area `0x70` zone 4, while clear, a step that does not face 0, 6 or 7 starts run 4 step 0 (message `0x12`) | progress |
| 16 | `Scena13_Run5` step `0x10`, `:832` | Run 5 is a dial. Its success branch (angle `0x4A0..0x4E0`, steps `0xA..0x10`, message 5, stream 5) sets it. Step 5 is the fail branch and sets nothing. Run 5 starts from `Area111_ChoiceStartVar7` (area 111, answer 0, `area_w2f.cpp:607-615`); reading, not checked | - | `Area112_ChoiceFlags34` (`area_w2f.cpp:1078`) and `Area112_StepHook` (`:1106`), row 13 by address: area 112's choice and step trigger work only while set | progress |
| 17 | `Scena13_Run6` step 4, `:868` | Run 6 in area `0x8F` (143), with flag 33. Run 6 step 0 starts from `Scena13_StepHook` `:1331` | - | `Scena13_StepHook` `:1331` (trigger while clear). `EnterArea13Body` `:231`, `:247`: area `0x8F` set-ups | progress |
| 18 | `Scena13_Run6` step `0xF`, `:944` | End of run 6's first part, with `ScriptFlags` bit `0x10` | - | `EnterArea13Body` `:236`, `:250` | progress |
| 19 | `EnterArea13Body`, `:243` | Area `0x8F` zone 0 with 17 and 18 set: call-table A entry 6, `Party_DropIn(6)` | - | `EnterArea13Body` `:240` | once-only set-up |
| 20 | `Scena13_Run6` step `0x1A`, `:973` | Run 6 step `0x19`, started by `Scena13_Object02` (`:1267-1271`): call-table A entry 7, then `ChangeArea(0x8F, ...)` | - | `Scena13_StepHook` `:1326`: area `0x8F` zone 0, while clear, a step starts run 6 step `0x14` (message `0x26`) | progress |
| 21 | `Scena13_Run6` step `0x1C`, `:989` | After message `0x2A` | - | Nothing found in code | progress |
| 22 | `Scena13_Run6` step `0x1E`, `:1001` | End of run 6's second part | - | Nothing found in code | progress |
| 23 | `Scena13_Run7` step `0xA`, `:1129` | After captions 6..9 (music `0x6F`). Run 7 step 0 starts in area `0x90` (144) zone 2 while clear (`:257-262`) | - | `EnterArea13Body` `:257`. `Area144_SceneByRowFlags` (`area_w3f.cpp:449`): scene 2 while clear | progress |
| 24 | `Area144_SceneByRowFlags`, `area_w3f.cpp:458` | Area 144's script, 23 set, 24 clear, leader byte `+0x89` = 2: scene 4 | - | `Area144_SceneByRowFlags` `:455`. `Scena13_StepHook` `:1343` (area `0x90` drop-in by leader) | progress |
| 25 | `Area144_SceneByRowFlags`, `area_w3f.cpp:469` | 24 set, 25 clear, leader byte 8: scene 7 | - | `Area144_SceneByRowFlags` `:465`. `Scena13_StepHook` `:1345` | progress |
| 26 | `Scena13_Run7` step `0x1D`, `:1191` | After event battle `0x25` (37, Elder in `boss-fights.md`; `:1177`). Run 7 step `0x14` starts from `Area144_ChoiceDropIn` (answer 0, `area_w3f.cpp:574-582`) | - | `Area144_SceneByRowFlags` `:476`. `Scena13_StepHook` `:1349` | progress |
| 27 | `Area144_SceneByRowFlags`, `area_w3f.cpp:485` | 26 set, 27 clear, leader byte 7: scene `0xD` | - | `Area144_SceneByRowFlags` `:482`. `Scena13_StepHook` `:1341`: area `0x90`'s drop-in trigger works only while clear | progress |
| 28 | `Scena13_Object03`, `:1276` | Object handler entry 3: tail kind `0x2C` | - | `Scena13_ArriveHook` `:1395`: with 28 set and 29 clear, area `0x8F` zone 0 starts run 8 step 0 | progress |
| 29 | `Scena13_Run8` step 0, `:1207` | Party records 1 and 2 `+0x137` both 0: `Party_DropIn(0xA)` | - | `Scena13_ArriveHook` `:1395`. `Scena13_StepHook` `:1327`: with 29 set and 30 clear, area `0x8F` zone 0 starts run 8 step 5 (message `0x4E`) | progress |
| 30 | `Scena13_Run8` step `0xB`, `:1234` | After `ChangeArea(0x97, 0x360000, 0x1D0000, 3)`. Run 8 step 8 or `0xA` starts from `Area143_ChoiceRun8` (answer 0, `area_w3f.cpp:338-350`). The chapter's end, below | - | `Scena13_StepHook` `:1327` | progress |
| 31 | `EnterArea13Body`, `:256` | Every entry to area `0x90` | - | Nothing found in code | progress (reading) |
| 32 (`0x20`) | `Scena13_Run6` step 3, `:862` | Run 6's first change into area `0x8F` | `Scena13_Run6` step 5, `:877` | Nothing found in code | transient |
| 33 (`0x21`) | `Scena13_Run6` step 4, `:869` | With flag 17 | `Scena13_Run6` step `0xC`, `:924` | Nothing found in code | transient |
| 34 (`0x22`) | `Scena13_Run6` step `0xC`, `:923` | After message `0x14` | `Scena13_Run6` step `0x1A`, `:974` | Nothing found in code | transient |

**Flags are set at a step, not when a battle is won.** Flag 26 is the one flag
after a battle. Step `0x1A` starts the battle and sets step `0x1B`. Step `0x1B` has
no case, so the step is moved on to `0x1C` outside this file (by the battle's end;
reading, not checked).

## End

`Scena13_Run8` step `0xA` (`:1227-1231`) calls
`Field_ChangeArea(0x97, 0x360000, 0x1D0000, 3)`. Step `0xB` (`:1232-1238`) waits for
`Field_Request` 0, sets **flag 30** and calls `Field_SetStatus80` (`:1237`). The
change into area `0x97` happens before bit `0x80` is set. So chapter 14 starts on
the next area change after that (`mode_flow.cpp:548-555`). Reading, not checked.

**The row as left**, by this reading:

- Always set: 0, 2..5, 7, 9, 11, 13..18, 20..23, 26, 28..30, and 31 (if area
  `0x90` was entered at all: the chapter's run 7 starts there).
- Set on the way by area entries or choices, always reached on the main route by
  this reading: 8, 10, 12, 19 (first-entry set-ups) and 24, 25, 27 (area 144's
  scenes, each needs a set leader). That the route needs them is a reading, not
  checked.
- Unknown whether optional: 1 (set on either answer of area 155's choice), 6
  (a step in area `0x7F`).
- No alternatives: no flag in this chapter is set in place of another.
- Cleared before the end: 32, 33, 34.

**Against the saves.** Saves 20, 21 and 22 (42:37 and 42:44 inside chapter 14 and
10, 43:25 in chapter 15) all show row 13 as **0..31**, and 32..34 clear. That agrees
with the reading: every flag 0..31 has a setter, and the three transient flags are
cleared. The saves also show flags 1 and 6 set, so the one player who made them
took both.

## Event flags used by this chapter

The bank at `Cond_Flags + 0xA0` (block `+0x650`), from `Scena13_*` functions:

| Event flag | Site | Use |
|---:|---|---|
| 67 (`0x43`) | `Scena13_Start`, `:183` | Set at the chapter's start |
| 66 (`0x42`) | `Scena13_Start`, `:184` | Cleared at the chapter's start |
| 45 (`0x2D`) | `Scena13_StepHook`, `:1361` | Tested: area `0x91` trigger for run 3 step `0x14` |

The saves agree: 67 is set and 66 is clear in saves 20..22, and 45 is set.

Area 145's handlers (`area_w3f.cpp`) also set 44 (`0x2C`), 45 and 46 and test
them. Their chapter is not settled by name, so they are not listed here.

## Unresolved

- **Flag 1, and whether it is optional.** Area 155's choice sets it on every
  answer (`area_w4a.cpp:788`, `:793`). Nothing in code tests it. Whether the choice
  is on the main route needs a run.
- **Flag 6.** `Scena13_StepHook` sets it on one cell of area `0x7F`. Nothing tests
  it in code.
- **Flags 21, 22, 31, 32..34 are not tested in code.** Event scripts in the game
  data can test chapter rows (`Area_TestCondition`), and `Party_DropIn` and
  call-table A run event scripts. Those were not read.
- **Area handlers by the current row.** `Area144_SceneByRowFlags`
  (`area_w3f.cpp:448-489`) writes `Chapter_FlagRow`, not row 13 by address. It is
  chapter 13's only if area 144's script runs in chapter 13. The saves agree (24,
  25, 27 set), but this is not checked in play.
- **Starters of runs outside this file.** Run 5 from `Area111_ChoiceStartVar7`,
  run 3 step `0xA` from `Area145_ChoiceRun3`, run 7 step `0x14` from
  `Area144_ChoiceDropIn`, and run 8 from `Area143_ChoiceRun8`. Each writes the run
  of the current chapter. That they fire in chapter 13 is a reading.
- **The chapter boundary.** That chapter 14 starts on the area change after
  `0x97` is a reading of the step order (`:1227-1238`), not checked.
- **Steps with no case.** Run 7 step `0x1B`, after event battle `0x25`, is moved
  on outside this file. Not traced.
