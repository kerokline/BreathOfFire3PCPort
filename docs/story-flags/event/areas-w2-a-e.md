# Event flags: areas of world 2, files a to e

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Source: `src/game/area_w2a.cpp`, `area_w2b.cpp`, `area_w2c.cpp`,
`area_w2d.cpp`, `area_w2e.cpp`. The event bank is `Cond_Flags + 0xA0`
(0x904030, save block `+0x650`). Each file reaches it by its own spelling:

- `area_w2a.cpp`: inline `Mem(::bof3::Img(Cond_Flags_addr) + 0xa0)`. Its
  `FlagRow()` (line 47) is the chapter row pointer at `Chapter_FlagRow_addr`,
  not the event bank.
- `area_w2b.cpp`: `Bank()` (line 67).
- `area_w2c.cpp`: `StoryFlags()` (line 46). `FlagRow()` (line 45) is the
  chapter row.
- `area_w2d.cpp`: `StoryFlags()` (line 53). `FlagRow()` (line 55) is the
  chapter row.
- `area_w2e.cpp`: `Bank()` (line 73).

Chapter-row sites in these files (`FlagRow()`, `Cond_Flags + 0x18`,
`Cond_Flags + 0x30`) are left out, as the brief says. The observed column
uses [`observed-rows.md`](../observed-rows.md): saves 20, 21 and 22 are the
three late saves (chapters 14, 10, 15).

## Sites

| Flag | Op | Function | File:line | Context |
|---|---|---|---|---|
| 28 | test | Area77_CellHook | area_w2a.cpp:155 | Area 77 (0x4D) cell hook. After a cell and pose match a record of `Area77_CellSwitches`, flag 28 set makes the hook answer 0 (no switch). |
| var | set | Area77_CellHook | area_w2a.cpp:156 | Area 77 cell hook: `FlagsToggle` (0x57C160) on the event bank. The flag number is byte 3 of the matched 4-byte record of the table `Area77_CellSwitches`. A toggle: sets the flag when it is clear. Then sound effect 0x201 and an effect of kind 4 (argument 0xF). |
| var | clear | Area77_CellHook | area_w2a.cpp:156 | Same toggle as the row above: clears the flag when it is set. |
| 4 | set | Area77_SetFlag4 | area_w2a.cpp:209 | Area 77 handler 5 (`Area77_Handlers[5]`). The handler does only this. The file header says choice k + 2 runs handler k (reading, not checked). |
| 49 | set | Area84_SetFlag31At5 | area_w2a.cpp:498 | Area 84 handler 0. Clears bit 0 of the active member's +0x80; then, only when the leader's byte +0x89 is 5, sets flag 49 (0x31), calls `MoveCmd_TestFB(9, 0x10)` and clears the running object's byte +0. |
| 28 | test | Area86_SwitchHook | area_w2b.cpp:271 | Area 86 switch hook. After a switch record of `Area86_Switches` matches the cell and facing, and no party member is near its rectangle, flag 28 set makes the hook answer 0. |
| var | set | Area86_SwitchHook | area_w2b.cpp:272 | Area 86: `kFlagsToggle` on the event bank. The flag number is byte 3 of the matched record of `Area86_Switches`. Sets it when clear. Then sound effect 0x200 and an effect of kind 4 (0xF); the comment at line 257 says that effect sets flag 0x1C (28). |
| var | clear | Area86_SwitchHook | area_w2b.cpp:272 | Same toggle as the row above: clears the flag when it is set. |
| 107 | test | Area91_State2Grow | area_w2c.cpp:230 | Area 91 object state 2, on its 0x1E-th frame. Flag 107 clear: set it and `Party_DropIn(0)`. Set: `Party_DropIn(1)`. |
| 107 | set | Area91_State2Grow | area_w2c.cpp:231 | Same function: the first pass sets flag 107, so later passes take `Party_DropIn(1)`. |
| 66 | set | Area94_InitPatches | area_w2c.cpp:695 | Area 94 init (+0x40), only when the previous area (`AreaW2c_LastArea`) is 0x79. Sets 66, clears 67, then applies the area map's patch chain. |
| 67 | clear | Area94_InitPatches | area_w2c.cpp:696 | Same entry from area 0x79: clears flag 67. |
| 32 | set | Area95_SpawnEffect37 | area_w2d.cpp:196 | Handler shared by areas 95 and 96 (+0x3C[1]). Spawns an effect of kind 0x37, sound 0x202; in area 96 (`Game_AreaNumber == 0x60`) sets flag 32. |
| 33 | set | Area95_SpawnEffect37 | area_w2d.cpp:199 | Same handler outside area 96: sets 33 when the chapter byte `Cond_ByteFA` (signed) is below 7. |
| 34 | set | Area95_SpawnEffect37 | area_w2d.cpp:199 | Same line: sets 34 when `Cond_ByteFA` is 7 or more. |
| 35 | set | Area97_FlagIfKeyItem5 | area_w2d.cpp:317 | `WorldMap_FieldHooks[11]`, in area 97's block. Sets flag 35 when key item 5 is held (`KeyItem_Has(5)`). |
| 148 | test | Area100_ChangeAreaByFlags | area_w2d.cpp:585 | Area 100 handler 0. Flags 148 and 147 choose the `Field_ChangeArea` target: neither 0x3B, 148 only 0x24, 147 only 0x74, both 0x70. |
| 147 | test | Area100_ChangeAreaByFlags | area_w2d.cpp:586 | Same function: the second selector bit (see row above). |
| 69 | set | Area100_SetFlag45 | area_w2d.cpp:596 | Area 100 handler 2. Sets flag 69 (0x45) and nothing else. |
| 147 | clear | Area100_ChoiceFlags93 | area_w2d.cpp:632 | Area 100 choice (handler 8), answer 1: clears 147, sets 148 and 51. The message comes from `Area100_ChoiceMessages[answer]`. |
| 148 | set | Area100_ChoiceFlags93 | area_w2d.cpp:633 | Answer 1: sets 148. |
| 51 | set | Area100_ChoiceFlags93 | area_w2d.cpp:634 | Answer 1: sets 51. |
| 147 | clear | Area100_ChoiceFlags93 | area_w2d.cpp:637 | Answer 2: clears 147 and 148, sets 51. |
| 148 | clear | Area100_ChoiceFlags93 | area_w2d.cpp:638 | Answer 2: clears 148. |
| 51 | set | Area100_ChoiceFlags93 | area_w2d.cpp:639 | Answer 2: sets 51. |
| 147 | set | Area100_ChoiceFlags93 | area_w2d.cpp:643 | Answer 3 with key item 6 held: sets 147, 148 and 51. |
| 148 | set | Area100_ChoiceFlags93 | area_w2d.cpp:644 | Answer 3 with key item 6: sets 148. |
| 51 | set | Area100_ChoiceFlags93 | area_w2d.cpp:645 | Answer 3 with key item 6: sets 51. |
| 51 | clear | Area100_ChoiceFlags93 | area_w2d.cpp:649 | Answer 3 without key item 6: message 9 and flag 51 cleared (147 and 148 unchanged). |
| 147 | set | Area100_ChoiceFlags93 | area_w2d.cpp:653 | Any answer other than 1..4: sets 147, clears 148, sets 51. Answer 4 changes no flag. |
| 148 | clear | Area100_ChoiceFlags93 | area_w2d.cpp:654 | Default answer: clears 148. |
| 51 | set | Area100_ChoiceFlags93 | area_w2d.cpp:655 | Default answer: sets 51. |
| 51 | test | Area100_StepHook | area_w2d.cpp:708 | Area 100 step hook. With `Cond_ByteFD` 3 and flag 51 set, a step on x 0x45..0x47, z 0x33..0x35 with leader pose 0, 7 or 6 sets counter 0 to 0 and calls `Party_DropIn(0)`. |
| 69 | set | Area100_Trigger16 | area_w2d.cpp:722 | Object trigger 16 (area 100's block). Sets 69, `MoveCmd_TestFB(0x4A, 0x1E)`, sound 0x109, and sets the four gate cells to 0xA1. |
| 64 | set | Area100_Trigger13 | area_w2d.cpp:732 | Object trigger 13. Sets 64, `MoveCmd_TestFB(0x4A, 0x2F)`, arms mode-tail kind 28 (`Area100_TailEffect9B`: effect 0x9B, then message script 3). |
| 69 | test | Area100_InitCells | area_w2d.cpp:749 | Area 100 init (+0x40). Flag 69 clear: the four gate cells are set to 0x50. Set: they are left as the map has them. |
| 91 | test | Area104_Tail40 | area_w2e.cpp:1066 | Area 104 mode-tail kind 40, state 2 (the init's path with key item 0xA). Flag 91 clear: set it and open message script 2. Set: message script 3. |
| 91 | set | Area104_Tail40 | area_w2e.cpp:1067 | Same state: the first pass sets 91, so script 2 runs once. |
| 79 | set | Area105_Init | area_w2e.cpp:1358 | Area 105 init, only when the previous area is 0x57. Sets 79 when `Cond_ByteFD` is 0. |
| 79 | clear | Area105_Init | area_w2e.cpp:1359 | Same entry: clears 79 when `Cond_ByteFD` is 1. |
| 81 | test | Area106_PlaceByFlags | area_w2e.cpp:1387 | Area 106 handler 1. Flag 81 picks the running object's position: set (0x128000, 0xB8000), clear one of two other places by flag 79. |
| 79 | test | Area106_PlaceByFlags | area_w2e.cpp:1390 | With 81 set: 79 picks height 0x850 and script + 2, or height 0x600 and script + 4. |
| 79 | test | Area106_PlaceByFlags | area_w2e.cpp:1399 | With 81 clear: 79 set places at (0x128000, 0x238000) height 0x850; clear at (0x118000, 0x1F8000) height 0x300, script + 4. |
| 82 | test | Area106_PlaceByPair | area_w2e.cpp:1417 | Area 106 handler 2. k = flag 82 (bit 0) and flag 83 (bit 1) selects the cell pair k of `Area106_Cells`. |
| 83 | test | Area106_PlaceByPair | area_w2e.cpp:1418 | Same function: bit 1 of k. |
| 79 | test | Area106_PlaceByPair | area_w2e.cpp:1423 | Same function: 79 set gives height 0x850 and script + 8; clear gives 0x280 and script + 2k. |
| 81 | set | Area106_Tail36 | area_w2e.cpp:1455 | Area 106 mode-tail kind 36, state 0: `Party_DropIn(0)` and flag 81 set. |
| 81 | clear | Area106_Tail36 | area_w2e.cpp:1456 | State 2: `Party_DropIn(1)` and flag 81 cleared. |
| 82 | set | Area106_Tail36 | area_w2e.cpp:1457 | State 0xA: `Party_DropIn(2)`, 82 set, 83 cleared. |
| 83 | clear | Area106_Tail36 | area_w2e.cpp:1457 | State 0xA: 83 cleared. |
| 82 | clear | Area106_Tail36 | area_w2e.cpp:1458 | State 0xC: `Party_DropIn(3)`, 82 cleared, 83 set. |
| 83 | set | Area106_Tail36 | area_w2e.cpp:1458 | State 0xC: 83 set. |
| 82 | set | Area106_Tail36 | area_w2e.cpp:1459 | State 0xE: `Party_DropIn(4)`, 82 and 83 set. |
| 83 | set | Area106_Tail36 | area_w2e.cpp:1459 | State 0xE: 83 set. |
| 82 | clear | Area106_Tail36 | area_w2e.cpp:1460 | State 0x10: `Party_DropIn(5)`, 82 and 83 cleared. |
| 83 | clear | Area106_Tail36 | area_w2e.cpp:1460 | State 0x10: 83 cleared. |
| 79 | test | Area106_StepHook | area_w2e.cpp:1478 | Area 106 step hook (only when `Cond_ByteFD` is 0). Flag 79 set: the hook looks at the two cells that arm tail 36 with state 0 or 2, by flag 81. |
| 81 | test | Area106_StepHook | area_w2e.cpp:1479 | With 79 set: 81 clear arms state 0 at z 0x248000, x 0x11..0x13; 81 set arms state 2 at z 0xA8000, x 0x12..0x13. |
| 82 | test | Area106_StepHook | area_w2e.cpp:1488 | With 79 clear: 82 set arms a state at z 0x258000, x 0x1A..0x1B. |
| 83 | test | Area106_StepHook | area_w2e.cpp:1490 | With 79 clear and 82 set: 83 set arms state 0x10, clear 0xC. |
| 83 | test | Area106_StepHook | area_w2e.cpp:1493 | With 79 and 82 clear: 83 set arms state 0xE at x 0x268000, z 0x24..0x25; clear arms 0xA at x 0x188000, z 0x15..0x16. |

## What each flag means here

- **4**: set by area 77 handler 5 (area_w2a.cpp:209). Nothing here tests or
  clears it. Meaning unknown. Set in saves 20..22 and not in the early saves.
- **28**: tested by the switch hooks of areas 77 and 86 (area_w2a.cpp:155,
  area_w2b.cpp:271). When set, the switches stop toggling. A code comment says
  an effect of kind 4 sets it (area_w2b.cpp:257); that effect is spawned right
  after each toggle, so reading, not checked: a switch locks after one use
  until something clears 28. Nothing in these files sets or clears it. Not set
  in any observed save.
- **32**: set by the shared kind-0x37 handler in area 96 (area_w2d.cpp:196).
  Set in every save from save 7 (chapter 1, 00:57) on, which is far earlier
  than area 96 in the area order, so another site probably sets it too
  (reading, not checked).
- **33**: set by the same handler outside area 96 below chapter 7
  (area_w2d.cpp:199). Not set in any observed save.
- **34**: set by the same handler outside area 96 from chapter 7 on
  (area_w2d.cpp:199). Not set in any observed save.
- **35**: set by world map field hook 11 when key item 5 is held
  (area_w2d.cpp:317). Set in saves 20..22.
- **49**: set by area 84 handler 0 when the leader's byte +0x89 is 5
  (area_w2a.cpp:498). Set in saves 20..22.
- **51**: set by every flag-changing answer of the area 100 choice except
  answer 3 without key item 6, which clears it (area_w2d.cpp:634..655).
  Tested by the area 100 step hook, which then drops the party in
  (`Party_DropIn(0)`) at one cell (area_w2d.cpp:708). Set in saves 20..22.
- **64**: set by object trigger 13 in area 100 (area_w2d.cpp:732), which runs
  the tail with effect 0x9B. Nothing here tests it. Set in saves 20..22.
- **66**: set on entry to area 94 from area 0x79 (area_w2c.cpp:695). Nothing
  here tests it. Not set in saves 20..22, so some other site clears it later
  (reading, not checked).
- **67**: cleared on entry to area 94 from area 0x79 (area_w2c.cpp:696).
  Set in saves 20..22, so some other site sets it.
- **69**: set by area 100 handler 2 and object trigger 16 (area_w2d.cpp:596,
  722). The init tests it: clear keeps the four gate cells at 0x50; trigger 16
  writes 0xA1 to them. Reading, not checked: 69 records that the area 100
  gate is open. Set in saves 20..22.
- **79**: set or cleared on entry to area 105 from area 0x57 by `Cond_ByteFD`
  0 or 1 (area_w2e.cpp:1358..1359). In area 106 it picks object positions
  and heights and which step-hook cells are live (area_w2e.cpp:1390..1478).
  Set in saves 20..22.
- **81**: set and cleared by area 106 tail 36 states 0 and 2
  (area_w2e.cpp:1455..1456); tested for placement and the step hook. With 79,
  a two-position switch. Set in saves 20..22.
- **82**, **83**: a two-bit value k set by area 106 tail 36 states 0xA..0x10
  (area_w2e.cpp:1457..1460), read as a cell-pair index by handler 2 and as
  the step-hook selector. 82 is set in saves 20..22; 83 is not.
- **91**: set the first time area 104's tail 40 reaches state 2; it picks
  message script 2 (first time) or 3 (later) (area_w2e.cpp:1066..1067). Set in
  saves 20..22.
- **107**: set the first time area 91's object state 2 completes; it picks
  `Party_DropIn(0)` (first time) or `Party_DropIn(1)` (area_w2c.cpp:230..231).
  Set in saves 20..22.
- **147**, **148**: written by the area 100 choice (area_w2d.cpp:632..654)
  and read by area 100 handler 0 to choose the destination area: neither
  0x3B, 148 0x24, 147 0x74, both 0x70 (area_w2d.cpp:585..586). Both values
  together need answer 3 with key item 6. Reading, not checked: a
  destination selector. Both are set in saves 20..22 (destination 0x70).
- **var (areas 77 and 86 switches)**: toggled by the switch hooks; the flag
  numbers are in the tables `Area77_CellSwitches` and `Area86_Switches`.

## Unresolved

- The switch flags of areas 77 and 86 (area_w2a.cpp:156, area_w2b.cpp:272)
  come from byte 3 of each record of `Area77_CellSwitches` and
  `Area86_Switches` (`kArea77CellSwitchCount` records; three in area 86).
  These tables are game data in the image and were not read. A read of them
  settles which event flags the switches own.
- Flag 28's setter: the comment at area_w2b.cpp:257 credits the kind-4 effect
  spawned by `kSpawnKind4` / 0x469FE0. That code is not in these files; where
  28 is cleared is also not here.
- `Area98_ChoiceFlag9` (area_w2d.cpp:332) sets flag 9 of a bank at 0x904654
  (`kFlags904654`, area_w2d_callees.h:46). That address is outside the event
  bank (0x904030..0x904057), so it is not listed in Sites. What 0x904654 is
  needs its own note.
- `Area104_Trigger36` (area_w2e.cpp:1305) receives 0x904030 as its second
  argument (the object trigger's flags pointer, per the comment at line 1303)
  but does not read it. Not a flag access; the trigger table entry may name
  an event flag in data that the engine tests before the call. Needs a read of
  `Field_ObjectTriggers`.
- `Cond_ByteFD` decides flag 79 (area 105) and gates the area 100 and 106 step
  hooks. Its meaning is not shown in these files.
- Flags 32, 66 and 67 do not match the late saves on the sites here alone;
  their other sites are in other files.
- Nothing here is checked in play.
