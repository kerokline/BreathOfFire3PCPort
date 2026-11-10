# Event flags: area_w2f.cpp and area_w3a.cpp

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

The event bank is `Cond_Flags + 0xA0` (`0x904030`, save block `+0x650`). In
`area_w2f.cpp` the helpers `Test`, `Set` and `Clear` (`:61-63`) call `Flags_Test`,
`Flags_Set` and `Flags_Clear` on `StoryFlags()` (`:60`), which is
`Cond_Flags + 0xa0`: the event bank. In `area_w3a.cpp` the helper `Bank()` (`:71`) is
also `Cond_Flags + 0xa0`. Flag numbers below are decimal.

Sites that are **not** event flags, and are left out of the table:

- `area_w2f.cpp:1078` and `:1106` test flag 16 of `Cond_Flags + 0x68`, which is
  chapter row 13 (`0x68 = 8 * 13`). They are paired with event flag 51 (see below).
- `area_w3a.cpp:1044` and `:1049` (`Area119_SetRowFlag3C` / `Area119_ClearRowFlag3C`)
  set and clear flag 60 of `FlagRow()`, which is `Chapter_FlagRow` (`:77`), the current
  chapter's row. That is a chapter-row flag, not event flag 60.
- `area_w3a.cpp:807` reads `Cond_Flags + 0xd2 + member`. Offset `0xD2` is past the
  event bank (`0xA0..0xC7`); it is a member id byte, not a flag.

Op `toggle` means `Flags_Toggle`: the flag is inverted.

## Sites

| Flag | Op | Function | File:line | Context |
|---|---|---|---|---|
| 30 | set | Area108_ChoiceArmTail5 | area_w2f.cpp:219 | Area 108 choice 0 (`+0x34[0]`). Answer 0: tail sub-kind 3, flag 30 (chosen at `:212`), then tail kind 5. |
| 31 | set | Area108_ChoiceArmTail5 | area_w2f.cpp:219 | Same site, answer 1: tail sub-kind 2, flag 31 (chosen at `:215`), then tail kind 5. Other answers set nothing. |
| 24 | test | Area108_PlaceScene | area_w2f.cpp:268 | Area 108 handler `+0x3C[4]`, place 0. Flag 24 clear: the active member's script = 1. |
| 25 | test | Area108_PlaceScene | area_w2f.cpp:270 | Place 0, flag 24 set: flag 25 clear gives `Area108_SetCells(1)` and script 2. |
| 26 | test | Area108_PlaceScene | area_w2f.cpp:273 | Place 0, flags 24 and 25 set: flag 26 clear gives script 3, set gives script 4. |
| 27 | test | Area108_PlaceScene | area_w2f.cpp:294 | Place 4: script 9 when flag 27 is set, else script 8. Script 9 is the only path to chapter 8's run 3 (see the section on area 108 below). |
| 30 | test | Area108_ScriptByFlags1E | area_w2f.cpp:316 | Area 108 handler 7: flag 30 set gives the active member script `0x12`. Script position `0xFFFE` either way. |
| 31 | test | Area108_ScriptByFlags1E | area_w2f.cpp:318 | Same handler: flag 30 clear and flag 31 set gives script `0x11`; neither keeps the script. |
| 48 | set | Area108_FlagIfEffectState5 | area_w2f.cpp:331 | Area 108 handler 8: when `Character_IndexById[leader's +0x89]` is 5, flag 48, `MoveCmd_TestFB(1, 0xB)`, and the running object's `+0` = 0. |
| 28 | test | Area108_CellHook | area_w2f.cpp:436 | Area 108 cell hook: with a matching `Area108_HookCells` entry, flag 28 set makes the hook answer 0 (no toggle). Flag 28 is the switch cool-down latch. |
| var | toggle | Area108_CellHook | area_w2f.cpp:438 | The flag is byte 3 of the matched `Area108_HookCells` entry (data). Then sound `0x206`. chapter-08.md reads these as 24..27 (reading, not checked). |
| 28 | set | Area108_CellHook | area_w2f.cpp:442 | Set only if `Effect_FindFree` gives a slot; that slot gets effect kind `0x18`, sub-kind (`+0xB`) `0x1B`, at the cell. |
| 28 | clear | EffectKind18Sub1B_Run | effect_5d.cpp:647 | Not in my files: the effect spawned at `area_w2f.cpp:442-449` clears flag 28 when its counter `+9` reaches `0xF` (15 frames), then releases itself. Listed because it ends the latch `Area108_CellHook` sets. |
| 29 | set | Area108_TailPlace | area_w2f.cpp:534 | Tail kind `0xC`, state 5, when the timer reaches 0: flag 29, `MoveCmd_TestFB(0x20, 0x2F)`, state 9, extra object 0's `+0` = 0, timer `0x1E`. State 9 then starts run 3. |
| 40 | set | Area111_TailGate | area_w2f.cpp:858 | Area 111 tail kind `0x11`, state `0x16`: `MoveCmd_TestFB(0x11, 0xAD)`, map bytes (`0x11..0x12`, `0xAD`) = `0xA1` and (`0x11..0x12`, `0xAE`) = `0xC0`, sound `0x205`, flag 40. |
| 40 | clear | Area111_ArriveHook | area_w2f.cpp:901 | Area 111 arrive hook: flag 40 cleared whenever `Cond_ByteFD` is 0. |
| 40 | test | Area111_ArriveHook | area_w2f.cpp:945 | With `Cond_ByteFD` 1, at cell x high word `0x11..0x12`, z `0xB7..0xB8`, and flag 40 clear: `ScriptFlags_Set40`, tail kind `0x11`, state `0x14` (the run that ends at `:858`). |
| 40 | clear | Area111_Init | area_w2f.cpp:988 | Area 111 init: flag 40 cleared when `Cond_ByteFD` is 0. |
| 40 | test | Area111_Init | area_w2f.cpp:994 | Init with `Cond_ByteFD` 1: flag 40 clear writes map bytes `0x50` / 0 at (`0x11..0x12`, `0xAD..0xAE`); set writes `0xA1` / `0xC0`, the same bytes as `:853-856`. |
| 53 | test | Area112_ChangeAreaByFlags | area_w2f.cpp:1024 | Area 112 handler 0: flag 53 is bit 0 of the case. Case 1 (53 only): `Field_ChangeArea(0x8F, ...)`; case 3 (both): area `0x64`. |
| 52 | test | Area112_ChangeAreaByFlags | area_w2f.cpp:1025 | Flag 52 is bit 1. Case 2 (52 only): area `0x92`; neither: area `0x91`. |
| 51 | set | Area112_ChoiceFlags34 | area_w2f.cpp:1058 | Area 112 choice 0 (handler 3): message = answer + 3, flag 51 set before the answer is read. |
| 52 | clear | Area112_ChoiceFlags34 | area_w2f.cpp:1061 | Answer 1: flag 52 cleared. |
| 53 | set | Area112_ChoiceFlags34 | area_w2f.cpp:1062 | Answer 1: flag 53 set. |
| 52 | set | Area112_ChoiceFlags34 | area_w2f.cpp:1065 | Answer 2: flag 52 set. |
| 53 | clear | Area112_ChoiceFlags34 | area_w2f.cpp:1066 | Answer 2: flag 53 cleared. |
| 52 | set | Area112_ChoiceFlags34 | area_w2f.cpp:1069 | Answer 3: flag 52 set. |
| 53 | set | Area112_ChoiceFlags34 | area_w2f.cpp:1070 | Answer 3: flag 53 set. Answer 4 changes neither flag (message `0xFFFF`). |
| 52 | clear | Area112_ChoiceFlags34 | area_w2f.cpp:1074 | Any other answer: flag 52 cleared. |
| 53 | clear | Area112_ChoiceFlags34 | area_w2f.cpp:1075 | Any other answer: flag 53 cleared. |
| 51 | clear | Area112_ChoiceFlags34 | area_w2f.cpp:1080 | After the answer: if chapter row 13's flag 16 is clear (`:1078`), message 2 and flag 51 cleared again. Flags 52 and 53 keep their new values. |
| 60 | clear | Area112_ChoiceClearFlags | area_w2f.cpp:1094 | Area 112 choice 2 (handler 5), answer 0: a loop clears flags 60..63 (`0x3C..0x3F`). |
| 61 | clear | Area112_ChoiceClearFlags | area_w2f.cpp:1094 | Same loop. |
| 62 | clear | Area112_ChoiceClearFlags | area_w2f.cpp:1094 | Same loop. |
| 63 | clear | Area112_ChoiceClearFlags | area_w2f.cpp:1094 | Same loop. |
| 56 | clear | Area112_ChoiceClearFlags | area_w2f.cpp:1095 | Then a loop clears flags 56..59 (`0x38..0x3B`), then sound `0x200`. |
| 57 | clear | Area112_ChoiceClearFlags | area_w2f.cpp:1095 | Same loop. |
| 58 | clear | Area112_ChoiceClearFlags | area_w2f.cpp:1095 | Same loop. |
| 59 | clear | Area112_ChoiceClearFlags | area_w2f.cpp:1095 | Same loop. |
| 51 | test | Area112_StepHook | area_w2f.cpp:1105 | Area 112 step hook: needs `Cond_ByteFD` 3, flag 51, chapter row 13's flag 16 (`:1106`), x high word `0xB..0xD`, z `0x57..0x59`, pose 0, 6 or 7. Then counter 0 = 0, `Party_DropIn(0)`, al 1. |
| 28 | test | Area112_CellHook | area_w2f.cpp:1133 | Area 112 cell hook: with a matching `Area112_HookCells` entry (pose not 3), flag 28 set makes it answer 0. |
| var | test | Area112_CellHook | area_w2f.cpp:1135 | The flag is byte 3 of the matched `Area112_HookCells` entry (data). Set: the hook answers 0 (each cell works once). |
| var | clear | Area112_CellHook | area_w2f.cpp:1147 | For each set bit 0..3 of the entry's byte 2: flag `Area112_HookFlags[bit & 3]` (data) cleared. |
| var | set | Area112_CellHook | area_w2f.cpp:1149 | For each set bit 4..7 of the entry's byte 2: flag `Area112_HookFlags[bit & 3]` (data) set. |
| var | set | Area112_CellHook | area_w2f.cpp:1151 | The entry's own flag (byte 3) set, then sound `0x200`. Skipped when `Area112_MemberNearBoxes` is true (tail kind `0xA`, state `0xA`, sub-kind `0x15` instead). |
| 28 | set | Area112_CellHook | area_w2f.cpp:1153 | Through `Effect_HoldFlag1C(0xF)` (`scena_sx2.cpp:97-104`): sets flag 28 if an effect slot is free; effect kind 4 clears it after 15 frames (`field_c1.cpp:263-269`). |
| 150 | test | Area113_Reward | area_w2f.cpp:1333 | Area 113 tail kind `0x3E` reward, byte `0x9045F4` (`FieldE1_Rank`) = `0xA`: flag 150 set gives message 5, answer 1 (nothing given). |
| 150 | set | Area113_Reward | area_w2f.cpp:1338 | Rank `0xA`, flag 150 clear: `Inventory_Add(3, 0x2D, 3)`, flag 150, message 2, answer 5. |
| 151 | test | Area113_Reward | area_w2f.cpp:1343 | Rank `0xB`: flag 151 set gives message 5, answer 1. |
| 151 | set | Area113_Reward | area_w2f.cpp:1348 | Rank `0xB`, flag 151 clear: `Inventory_Add(3, 0x33, 1)`, flag 151, message 6, answer 5. |
| 150 | test | Area113_Reward | area_w2f.cpp:1351 | Rank `0xB`: then flag 150 set ends there (message 6). |
| 150 | set | Area113_Reward | area_w2f.cpp:1353 | Rank `0xB`, flag 150 clear: `Inventory_Add(3, 0x2D, 3)`, flag 150, message 3. |
| 151 | test | Area113_Reward | area_w2f.cpp:1364 | Rank `0xC`, after `KeyItem_Add(0xF)` (only if key item `0xF` is not held): flag 151 set ends with message 8, answer `0xA`. |
| 151 | set | Area113_Reward | area_w2f.cpp:1366 | Rank `0xC`, flag 151 clear: `Inventory_Add(3, 0x33, 1)`, flag 151, message 7. |
| 150 | test | Area113_Reward | area_w2f.cpp:1368 | Rank `0xC`: flag 150 set ends with message 7. |
| 150 | set | Area113_Reward | area_w2f.cpp:1370 | Rank `0xC`, flag 150 clear: `Inventory_Add(3, 0x2D, 3)`, flag 150, message 4. |
| 51 | test | Area116_StepHook | area_w3a.cpp:749 | Area 116 step hook: needs `Cond_ByteFD` 2, flag 51, x high word `0x46..0x48`, z `0x33..0x35`, pose 0, 6 or 7. Then counter 0 = 0, `Party_DropIn(0)`, al 1. No chapter-row test here, unlike area 112's. |
| var | test | MembersFrame | area_w3a.cpp:957 | Areas 117 / 118 member frame (`Area117_MembersFrame` / `Area118_MembersFrame`), a member entering a rectangle: the flag is byte 5 of the rectangle record (`t.rects + 4 + found * 6 + 1`, data). Set: the member's facing is reversed (`^ 4`). |
| var | test | MembersFrame | area_w3a.cpp:967 | Same flag, a member already marked inside the rectangle: the facing reversed while set; a changed facing negates the step and sets `+9` = 8 - `+9`. |
| 28 | test | SwitchHook | area_w3a.cpp:1009 | Areas 117 / 118 cell hook (`Area117_SwitchHook` / `Area118_SwitchHook`): with a matching switch record, flag 28 set makes it answer 0. |
| var | toggle | SwitchHook | area_w3a.cpp:1010 | The flag is byte 3 of the matched switch record (`Area117_Switches` `0x62160C` / `Area118_Switches` `0x621E3C`, data). docs/area_w3a.md:154 gives it as `0x85` (133) for both areas. |
| 28 | set | SwitchHook | area_w3a.cpp:1011 | Through `Effect_HoldFlag1C(0xF)`: flag 28 set for 15 frames, as at `area_w2f.cpp:1153`. Then sound `0x203`. |

## Area 108: the path to chapter 8's run 3

Read from `area_w2f.cpp:254-303` and `:475-570`. It agrees with chapter-08.md and adds
the details below.

1. **The trigger.** `Area108_PlaceScene` is area 108's handler `+0x3C[4]`, so the
   area's event script calls it (data). It clears the active member's `+0x80` bit 0.
   It continues only if the leader's member id (`ObjTrio +0x89`) is 2 (`:256`). It
   then looks for the first `Area108_Places` entry (3 bytes: x, z, direction; data)
   whose x and z are extra object 1's high words and whose direction is the leader's
   `+8 & 7` (`:257-264`). No match: nothing more.
2. **Place 4 and event flag 27.** For place 4 it drops in the members with z below
   `0x41` and x `0x1C..0x1D` (`:293`). It gives the active member script 9 if event
   flag 27 (`0x1B`) is set, else script 8 (`:294`). Then, for every place, it sets bit
   0 of `MoveScript_PartyRecords + 0x10`, calls `ScriptFlags_Set40`, and arms tail kind
   `0xC` at state 0 with timer `0xF` (`:298-302`).
3. **Tail states 0 and 1** (`:479-492`). State 0 waits for the timer, then aims the
   kind-2 sprite at extra object 1. State 1 waits for `Field_Kind2Hold` = 0, then
   `Kind2_Place(extra object 1's +0x83 - 1)` and counter 3 = 1.
4. **Tail state 2, the branch** (`:493-506`). It waits for counter 3 = 2 (something
   else writes it; likely the member script, reading, not checked). If extra object
   1's `+0x83` is 9, state 4. Else the sprite goes back to the leader and state 3
   disarms the tail (`:507-513`) with no run.
5. **Tail state 4** (`:514-531`). It waits for counter 3 = 4. It places effect
   `0x35` at (`0x218000`, `0x2F8000`) if a slot is free, sets
   `Area108_Model +3 = (… & 0xF1) | 0x41`, timer `0x1E`, state 5.
6. **Tail state 5, event flag 29** (`:532-560`). Each frame it moves the model's
   records by their velocity and dims extra object 0. When the timer reaches 0: event
   flag 29 (`0x1D`) set, `MoveCmd_TestFB(0x20, 0x2F)`, state 9, extra object 0's `+0`
   = 0, timer `0x1E`.
7. **Tail state 9, run 3** (`:561-567`). After `0x1E` frames: `MoveScript_Var7` = 3
   (the run byte), tail state and kind 0, and `MoveScript_PartyRecords + 0x65` = 0
   (the run's step 0).

What the source shows about the gates:

- Event flag 27 is tested at `:294` on the **active member**'s script byte. State 2
  tests **extra object 1**'s `+0x83`. They agree only if the active member is extra
  object 1 when the handler runs (reading, not checked).
- Event flag 29 is set on the way to run 3, but no site in either file tests it. No
  site here stops the path from running again; any guard is in area 108's script
  (data).
- No chapter row and no `Cond_ByteFA` test is on the path. Only the leader's id,
  the place, event flag 27 and the counter-3 handshake decide it.
- Event flag 27 is set one frame into a new game and is set in all 22 real saves
  (observed-rows.md). So, unless area 108's cell hook has toggled it off, place 4
  gives script 9 by default (reading, not checked). Flag 29 is set only in saves
  20..22.
- Event flags 24..27 are changed by `Area108_CellHook` (`:426-452`), which toggles
  the flag named by byte 3 of its matched `Area108_HookCells` entry. That byte is
  data; that the entries name 24..27 is chapter-08.md's reading and is not checked
  here. After a toggle the hook sets latch flag 28 and spawns effect `0x18` / `0x1B`,
  which clears flag 28 fifteen frames later (`effect_5d.cpp:631-648`), so one step on
  a cell toggles once.

## What each flag means here

- **24** (`0x18`): tested by `Area108_PlaceScene` place 0 (`:268`) to pick member
  script 1 or go on. Possibly toggled by `Area108_CellHook` (data). Set in saves
  20..22 only. Meaning beyond that: unknown.
- **25** (`0x19`): place 0, picks script 2 with `Area108_SetCells(1)` when clear
  (`:270`). Possibly toggled by the cell hook. Set in all 22 saves (set one frame
  into a new game).
- **26** (`0x1A`): place 0, script 3 or 4 (`:273`). Possibly toggled by the cell hook.
  Set in saves 20..22 only.
- **27** (`0x1B`): place 4, script 9 (the run-3 path) when set, else 8 (`:294`).
  Possibly toggled by the cell hook. Set in all 22 saves (set one frame into a new
  game).
- **28** (`0x1C`): a switch cool-down latch. Tested by the cell hooks of areas 108,
  112, 117 and 118 (`area_w2f.cpp:436`, `:1133`; `area_w3a.cpp:1009`). Set by
  `Area108_CellHook` (`:442`) and through `Effect_HoldFlag1C` (`area_w2f.cpp:1153`,
  `area_w3a.cpp:1011`). Cleared by effects 15 frames later, and by `Area_Enter`
  (`mode_flow.cpp:560`). Not set in any save. Shared with areas 49, 77, 86, 109
  (`scena_sx2.cpp:93-95`).
- **29** (`0x1D`): set by `Area108_TailPlace` state 5 (`:534`), two states before run
  3 starts. Not tested in these files. Set in saves 20..22.
- **30** (`0x1E`): set by area 108 choice 0 on answer 0 (`:219`); also set by
  `Area34_ChoiceTail5` (chapter-08.md, not in these files). Tested by area 108
  handler 7 to give script `0x12` (`:316`). Not set in any save.
- **31** (`0x1F`): set by area 108 choice 0 on answer 1 (`:219`). Tested by handler 7
  to give script `0x11` when flag 30 is clear (`:318`). Not set in any save.
- **40** (`0x28`): area 111's gate. Set when tail state `0x16` rewrites the map bytes
  at (`0x11..0x12`, `0xAD..0xAE`) (`:858`). With it set, init writes the same bytes
  again (`:994`) and the arrive hook does not start the gate run (`:945`). Cleared by
  the arrive hook and init whenever `Cond_ByteFD` is 0 (`:901`, `:988`), so it does
  not outlast a visit with `Cond_ByteFD` 0 (reading). Not set in any save.
- **48** (`0x30`): set by area 108 handler 8 when the leader's character index is 5
  (`:331`). Not tested in these files. Set in saves 20..22.
- **51** (`0x33`): set by area 112 choice 0 (`:1058`), cleared again at once unless
  chapter row 13's flag 16 is set (`:1078-1080`). Gates area 112's step hook
  (`Cond_ByteFD` 3, `:1105`) and area 116's step hook (`Cond_ByteFD` 2,
  `area_w3a.cpp:749`). Set in saves 20..22. docs/area_w3a.md:129 says area 116's hook
  is `Area100_StepHook` with two constants moved, so area 100 likely tests it too
  (reading, not checked).
- **52** (`0x34`): set or cleared by area 112 choice 0's answer (`:1061-1075`). Bit 1
  of `Area112_ChangeAreaByFlags`'s case (`:1025`): the destination area. Set in saves
  20..22.
- **53** (`0x35`): set or cleared by the same choice. Bit 0 of the destination case
  (`:1024`). Not set in any save. With 52 set and 53 clear the handler goes to area
  `0x92` (146).
- **56..63** (`0x38..0x3F`): all eight cleared by area 112 choice 2, answer 0
  (`:1094-1095`). Area 112's cell hook sets and clears flags named by data
  (`Area112_HookFlags`, `Area112_HookCells`); whether these are those flags is not
  shown in the code. Set in saves 20..22. Note: chapter row 13's own flag 60 is a
  different bit (`area_w3a.cpp:1044` uses the chapter row).
- **133** (`0x85`, per docs/area_w3a.md:154, from data): toggled by the floor switch
  of areas 117 and 118 (`area_w3a.cpp:1010`); while set the member frame reverses the
  facing in the rectangle (`:957`, `:967`). Set in saves 20..22.
- **150** (`0x96`): area 113 reward, once only: the 3 of item `0x2D` (category 3) at
  rank `0xA`, `0xB` or `0xC` (`:1333-1370`). Not set in any save.
- **151** (`0x97`): area 113 reward, once only: item `0x33` (category 3) at rank `0xB`
  or `0xC` (`:1343-1366`). Not set in any save.

## Unresolved

- **`Area108_HookCells` byte 3** (`area_w2f.cpp:438`): which flags area 108's cell hook
  toggles. chapter-08.md reads them as 24..27; that needs a read of the table (game
  data, `Area108_HookCells_addr`) or a run.
- **`Area112_HookCells` byte 3 and `Area112_HookFlags`** (`area_w2f.cpp:1135-1151`):
  the flags area 112's cell hook tests, sets and clears. Possibly 56..63, since choice
  2 clears those (reading, not checked). Needs the tables.
- **Areas 117 / 118 rectangle and switch flag bytes** (`area_w3a.cpp:957`, `:967`,
  `:1010`): docs/area_w3a.md:154 gives 133 (`0x85`) for both. That came from the data
  in an earlier round; it was not re-read here.
- **Run 3's guard.** Nothing in these files stops `Area108_PlaceScene` from starting
  the run-3 path again once flag 29 is set. Whether area 108's event script checks
  flag 29, flag 27, or a chapter flag before calling handler `+0x3C[4]` needs a read
  of the script or a run.
- **Who writes counter 3 = 2 and = 4** for tail states 2 and 4 of `Area108_TailPlace`.
  Likely the member scripts 8 / 9 (data); not shown in these files.
- **Active member versus extra object 1** (`:294` versus `:495`): the source does not
  show they are the same object when handler `+0x3C[4]` runs.
- **`Cond_ByteFD`** gates flag 40 (area 111), and flag 51's step hooks (3 in area 112,
  2 in area 116). What sets `Cond_ByteFD` is outside these files.
- **Op column**: four rows use `toggle` (`Flags_Toggle`), which is neither set nor
  clear. Two rows (`effect_5d.cpp:647`, and the `Effect_HoldFlag1C` rows) are
  indirect sites outside or through a callee, listed because they end or start a
  latch these files depend on.
