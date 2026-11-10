# Story flags: chapter 3

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 3's row is `Cond_Flags + 0x18` (`0x903FA8`), save block `+0x5C8`. The code is
`Scena03_*` in `src/game/scena_sc3.cpp` (original `0x5428C0..0x544FE0`). Its wrappers
are `Bank()` (the dword `Chapter_FlagRow`, read afresh), `Flag(n)` and `SetFlag(n)`
(`scena_sc3.cpp:84-86`). Some steps call `Flags_Set(bank, n)` with the row read
earlier. The file has no `Flags_Clear` for this row.

Other files reach the row at its fixed address (`Cond_Flags + 0x18`): `area_w1d.cpp`
(area 56), `area_w2d.cpp` (area 99) and `effect_5a.cpp` (effect kind 0x18, sub-kind 8).
Area 99 also toggles a flag of the current row.

Areas below are decimal, hex in brackets as the code writes them. "Entrance" is
`Cond_ByteFD`, as in chapter 0's section.

## Start

- **Who starts it.** `Scena02_Scene15` step 7 (`scena_sc2.cpp:1773-1777`) sets
  chapter 2's flag 32 (0x20) and calls `Field_SetStatus80` (`Field_StatusBits |= 0x80`).
  On the next area entry, `Area_Enter` (`mode_flow.cpp:549-556`) sees bit 7, steps
  `Cond_ByteFA` from 2 to 3 and calls `Scenario_Start(3)`.
- **`Scenario_Start`** (`title_states.cpp:404`) zeroes the scenario bytes
  `0x8034E1..0x8034E9`, writes 0 to the row's first dword (flags 0..31) and points
  `Chapter_FlagRow` at `Cond_Flags + 0x18`.
- **`Scena03_Start`** (state 0, `scena_sc3.cpp:161-164`) sets state 1 and writes 0
  to `Cond_Flags + 0x18` again. So flags 0..31 start clear. Flags 32..63 are never
  cleared and never used by this chapter.
- **State 1** is `Scena03_EnterArea`, which runs once for each area entered. The
  start sets no flag, no area, no party and no run. Which area the chapter starts in is
  the area whose entry steps the chapter. Chapter 2's code does not name it
  (Unresolved).
- **The first scene that the code shows** is the step hook in area 51 (0x33),
  `Scena03_StepArea33` (`:1400-1409`). With flags 1 and 0 clear, a step at x cell
  2..4, z `0x60000` starts run 2. Run 2 (`Scena03_Scene2`, `:342-352`) drops the
  party in (1) and hands on to run 1 step 1 when counter 0 reaches 0x28. Reading, not
  checked.

## Flags

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | nothing found | - | `Scena03_Start` (`:163`) | `Scena03_StepArea33` (`:1402`, `:1406`): area 51. With flag 0 clear, the step at x cell 2..4, z `0x60000` starts run 2. With it set, the step at x cell 3..4, z `0x40000` starts run 1 | unknown |
| 1 | `Scena03_Scene1` step 4 (`:306`) | Counter 0 reaches 0x32 in run 1. In the same step: item event `0x97`, gene bit 0 (`:305`), change to area 45 (0x2D) | `Scena03_Start` | `Scena03_StepArea33` (`:1401`): area 51's hook is off. `Scena03_EnterArea` (`:189`): area 45's set-up while flag 2 is clear | progress |
| 2 | `Scena03_Scene1` step 7 (`:327`) | Counter 0 reaches 2 in area 45. The change to area 39 (0x27) follows, then run 3 step 0 | `Scena03_Start` | `Scena03_EnterArea` (`:189`): area 45's first set-up runs once | progress |
| 3 | `Scena03_Scene3` step 7 (`:417`) | Counter 0 reaches 0x25 in run 3, after the change to area 41 (0x29). The party is restored | `Scena03_Start` | `Scena03_EnterArea` (`:176`, `:180`): the set-ups of area 39 and area 41 run until it is set | progress |
| 4 | `Scena03_Scene3` step 6 (`:412`) | Counter 0 reaches 0x18 in run 3, after music 0x55 | `Scena03_Start` | Nothing in the file tests it | progress |
| 5 | `Scena03_Scene3` step 8 (`:425`) | Counter 1 reaches 0x15 in run 3 | `Scena03_Start` | `EffectKind18_08_Wait` (`effect_5a.cpp:794`): the door effect starts its shake | progress |
| 6 | `Scena03_Scene3` step 9 (`:432`) | Counter 0 reaches 0x2B. Run 3 ends (run 0) | `Scena03_Start` | `EffectKind18_08_Start` (`effect_5a.cpp:780`): the door is drawn as already toppled. `EffectKind18_08_WaitPush` (`:819`): the push and the topple | progress |
| 7 | `Scena03_StepArea29` (`:1416`) | A step in area 41 (0x29) at z cell 7..8, x `0x5C0000`. Run 3 step 0xA starts. Step 0xB starts event battle 16 (`LeaderState5(0x10)`, `:447`) | `Scena03_Start` | `Scena03_StepArea29` (`:1414`): the encounter happens once | progress |
| 8..14 (0x8..0xE) | `Area56_ChoicePoseFlag8..E` through `ChoicePoseFlag` (`area_w1d.cpp:109-116`, `:258-264`) | Area 56 (0x38), choices 0..6. Each choice sets its flag on one answer: answer 1 for choices 0, 1, 2, 4 and 6, answer 0 for choices 3 and 5. The other answer gives the leader's pose 2/7 instead | `Scena03_Start` | `Area56_ChoicePoseIfAllFlags` (`area_w1d.cpp:287-294`): choice 14, answer 0, gives the pose only when all seven are set (byte `0x903FA9 & 0x7F`) | choice |
| 15 (0xF) | `Scena03_EnterArea` (`:217`) | First entry to area 56 (0x38) by entrance 1. Counters cleared, party drop-in 0 | `Scena03_Start` | The same site: it runs once | once-only set-up |
| 16..19 (0x10..0x13) | `Scena03_Object1..4` (`:1320-1335`), into the row they are given (the current row, `Scena03_ObjectHook` `:1305`) | Field objects with `+0x86` 1..4 are triggered | `Scena03_Start` | `Scena03_ObjectsAllFour` (`:1340-1346`): with all four set (`Cond_Flags[0x1A] & 0xF`), step 6 with 0x1E frames. Reading, not checked: run 4 is then waiting at step 5, so this leads to run 4 step 6 and flag 21 | progress |
| 20 (0x14) | `Scena03_Object0` (`:1313`), on the row it is given | A field object with `+0x86` 0 is triggered. Run 4 step 0 starts: drop-in 0, message 0x20, change to area 37 (0x25) | `Scena03_Start` | Nothing in the file tests it | progress |
| 21 (0x15) | `Scena03_Scene4` step 6 (`:582`) | Run 4, after the 0x1E-frame wait. The change to area 37 (0x25) with flags 0x82 follows. Then run 4 ends | `Scena03_Start` | `Scena03_EnterArea` (`:171`): area 37's pass flags 0x1F until set. `Scena03_StepArea25` (`:1425`): the step at z `0x418000`, x cell 0x20..0x23, shows message 0x33 until set | progress |
| 22 (0x16) | `Scena03_EnterArea` (`:226`) | First entry to area 69 (0x45) by entrance 0. Party drop-in 3 | `Scena03_Start` | The same site | once-only set-up |
| 23 (0x17) | `Scena03_EnterArea` (`:242`) | First entry to area 69 by entrance 1. `CallB(0)`, drop-in 4, sound 0x201 | `Scena03_Start` | The same site | once-only set-up |
| 24 (0x18) | `Scena03_EnterArea` (`:233`) | A later entry to area 69 by entrance 0, from entry zone 1. Music 0x14, drop-in 5, then run 5 step 2 | `Scena03_Start` | That site. `Scena03_StepArea45` (`:1436`): area 69's three doors show message 0x12 until set. `Scena03_EnterArea` (`:197`): area 45's second set-up while flag 25 is clear | progress |
| 25 (0x19) | `Scena03_Scene5` step 0xE (`:714`) | Run 5, counter 0 reaches 0x15. The change to area 50 (0x32) with flags 0x84 follows | `Scena03_Start` | `Scena03_EnterArea` (`:197`): area 45's second set-up runs once | progress |
| 26 (0x1A) | `Scena03_Scene5` step 0xF (`:720`) | Run 5, counter 0 reaches 7. Run 5 goes back to step 0 | `Scena03_Start` | `Scena03_StepArea32` (`:1448`): area 50, the step at x cell 0x15, z cell 0x34..0x35, starts run 5 step 0x10 (message 0x1F, then the run ends) | progress |
| 27 (0x1B) | `Scena03_EnterArea` (`:265`) | Entry to area 99 (0x63) by entrance 3. Drop-in 1, then run 6 step 6 with 0x1E frames | `Scena03_Start` | `Scena03_EnterArea` (`:250`): area 99's entrance 2 and 3 set-ups. `Scena03_StepArea63` (`:1460`): without it, entrance 2's step shows message 0xC. With it, entrance 3's step starts run 6 | progress |
| 28 (0x1C) | `Scena03_Scene6` step 0x1C (`:926`) | Run 6, counter 0 reaches 0xD. `Cond_ByteFD` is set to 3 | `Scena03_Start` | `Scena03_StepArea63` (`:1470`): without it, run 6 step 0xF. With it, step 0xD and message 0x11. `Area99_InitCell` (`area_w2d.cpp:567-573`): cell (0x23, 0x4F) is 0x51 while 28 is set and 29 is clear | progress |
| 29 (0x1D) | `Scena03_Scene7` step 0xD (`:1038`) | Run 7, counter 0 reaches 0x34. Run 7 starts from `Scena03_Object8` or from the cell hook `Scena03_Cell0` (message 0x19) | `Scena03_Start` | `Area99_InitCell` (as above) | progress |
| 30 (0x1E) | `Scena03_Scene8` step 0x14 (`:1228`) | Run 8, counter 0 reaches 0x27: music 0x51, gene bit 6 (`:1229`), then `Field_SetStatus80`. This ends the chapter | `Scena03_Start` | `Scena03_EnterArea` (`:245`): area 71 (0x47) gets three map bytes until set. `Scena03_ArriveArea47` (`:1490`): arriving at z `0x490000`, x cell 0xE..0x11, starts run 8 step 2 | progress |
| 31 (0x1F) | `Area99_ToggleRowFlag1F` (`area_w2d.cpp:560`), current row | Area 99's handler 5 toggles it (`0x57C160`) | the same toggle | Nothing found tests it | unknown |

### The runs in order (reading, not checked)

1. Run 2, then run 1: area 51, then area 45 (flags 1, 2).
2. Run 3: areas 39 and 41 (flags 4, 3, 5, 6). The door effect follows flags 5 and 6.
3. Run 3 from step 0xA: area 41's step hook, event battle 16 (flag 7). Step 0xC
   branches on counter 0 (0x14 or above). Both branches end at step 0x1A.
4. Run 4: object 0, then objects 1..4, area 37 (flags 20, 16..19, 21).
5. Area 56 on entrance 1 (flag 15) and its seven choices (flags 8..14).
6. Area 69, then run 5: areas 45 and 50 (flags 22, 23, 24, 25, 26).
7. Run 6: area 99 (flags 27, 28). Run 7: area 99 (flag 29).
8. Run 8: arriving in area 71 (flag 30, gene 6, the chapter's end).

The code does not fix the order of steps 4 and 5. It is a reading from the flags each
step tests.

### Genes beside chapter flags

- Gene 0: `Scena03_Scene1` step 4 (`:305`), with flag 1 and item event `0x97`.
- Gene 6: `Scena03_Scene8` step 0x14 (`:1229`), with flag 30.

Both agree with [`../story-flags.md`](../story-flags.md) section 1.

## End

`Scena03_Scene8` step 0x14 (`:1224-1235`) sets flag 30 and gene 6, clears script
flag 0x40, ends the run, then calls `StatusBit80()` (`Field_SetStatus80`,
`scena_sc3_callees.h:106`). This step changes no area. The next `Area_Enter` steps
the chapter to 4 and calls `Scenario_Start(4)` (`mode_flow.cpp:549-556`). Run 8
begins in area 71 (0x47), from the arrive hook.

**The row as left (reading):**

- Always set: 1..7, 15, 16..30, on the path that the code requires. Flags 16..19 must
  all be set before flag 21 can be set.
- Choice: 8..14. Each depends on one answer in area 56. Nothing found makes them
  necessary for the chapter's end.
- Never set: 0. Flag 31 is toggled by area 99 and can be left either way.
- Nothing clears a flag of this row during the chapter.

**Against the saves** ([`observed-rows.md`](observed-rows.md)): every save past chapter
3 (saves 13..22: JP and US, 07:41 to 43:25) has chapter 3's row as exactly **1..30**.

- Agree: flag 0 is clear everywhere. This matches no setter found.
- Agree: 1..7 and 15..30 are all set.
- Agree: flag 31 is clear. So it was toggled an even number of times or not at all.
- All of 8..14 are set on every save. So every route in the saves gave the flag answer
  to all seven choices. That these flags are optional is a reading, not a measurement.
- No save held is inside chapter 3, so the order of the steps is not checked.

## Event flags used by this chapter

The `Scena03_*` functions set, clear and test no event flag (`Cond_Flags + 0xA0`).
`scena_sc3.cpp` has no reference to that bank.

Area code in the chapter's areas does use event flags. Those areas are also entered in
other chapters, so the uses are listed here for context only:

- Area 56 and 57: `FlagsGather` (`area_w1d.cpp:84-90`) sets event flag 0x6D..0x70 and,
  when all four are set, 0x71.
- Area 41: `Area41_TriggerFlag` (`area_w1a.cpp:723-727`) sets the event flag that a
  table names by the object's `+0x86` (table `0x5F5F2D`, not read here).

## Unresolved

- **Flag 0.** It is tested in area 51's step hook (`:1402`, `:1406`). No setter was
  found in `scena_sc3.cpp` or in the area, effect and boss files searched for this row.
  The saves agree that it stays clear. A setter may be in an event script in the game
  data (`Area_TestCondition`-style tests reach rows by kind). Not searched.
- **Flag 31.** `Area99_ToggleRowFlag1F` toggles the current row's flag 31, but nothing
  found shows when area 99's handler 5 runs or what tests the flag.
- **`Scena03_ObjectsAllFour`** (`:1340`) sets only the step (6) and the timer. It does
  not set the run. That run 4 is the current run at that moment (waiting at step 5,
  `:573-575`) is a reading, not checked. Its header says "run 6's step 6".
- **Object handlers** take the current row (`Scena03_ObjectHook`, `:1305`). In chapter
  3 that is this row. If an object with these `+0x86` values triggers in another
  chapter, the flag goes into that chapter's row. Not checked.
- **Area 98's `Area98_ChoiceFlag9`** (`area_w2d.cpp:329-338`) sets flag 26 (0x1A) of the
  current row on a non-zero answer. Whether area 98 is entered during chapter 3 is not
  known. If it is, that choice sets this chapter's flag 26 outside run 5.
- **Area 41's `Area41_ChoiceGiveItem`** (`area_w1a.cpp:610-618`) gives item 0x28 and sets
  flag 6 of the current row, once. During chapter 3, flag 6 is also run 3's last step.
  Whether the choice is reached in chapter 3 is not known.
- **The first area of the chapter.** `Scena02_Scene15` step 7 changes no area. The
  area entry that starts chapter 3 comes from code not read here.
- **Run 3 step 0xC** branches on counter 0 after event battle 16 (`:451-463`): 0x14 to
  step 0xD, above that to step 0x14. What sets the counter (the battle's result?) is
  not known.
- **The door effect** (`effect_5a.cpp:777-830`): its area is not named in the code read.
- **Area 69's entrance 0 with entry zone 1** (`:227`): "entry zone" is
  `AreaW1a_EntryZone`, the entrance of the area left. That reading is not checked.
