# Story flags: chapter 4

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 4's row is `Cond_Flags + 0x20` (`0x903FB0`), save block `+0x5D0`. The code is
`Scena04_*` in `src/game/scena_sc3.cpp` (original `0x545010..0x546370`). Its wrappers
are the same as chapter 3's: `Bank()`, `Flag(n)` and `SetFlag(n)`
(`scena_sc3.cpp:84-86`), and `Flags_Set(bank, n)` with the row read earlier. The file
has no `Flags_Clear` for this row.

Other files reach the row. Area 40's code (`area_w1a.cpp`) writes the fixed row
`Cond_Flags + 0x20` and the current row.

Areas below are decimal, hex in brackets as the code writes them. "Entrance" is
`Cond_ByteFD`.

## Start

- **Who starts it.** `Scena03_Scene8` step 0x14 (`scena_sc3.cpp:1224-1235`) sets
  chapter 3's flag 30 and calls `Field_SetStatus80`. On the next area entry,
  `Area_Enter` (`mode_flow.cpp:549-556`) steps `Cond_ByteFA` from 3 to 4 and calls
  `Scenario_Start(4)`. That writes 0 to the row's first dword (flags 0..31) and points
  `Chapter_FlagRow` at `Cond_Flags + 0x20` (`title_states.cpp:404-418`).
- **`Scena04_Start`** (state 0, `:1531-1534`) sets state 1 and writes 0 to
  `Cond_Flags + 0x20` again. Flags 32..63 are never cleared and never used.
- **State 1** is `Scena04_EnterArea` (`:1538-1613`), once for each area entered. It
  handles areas 40 (0x28), 42 (0x2A), 48 (0x30), 54 (0x36) and 65 (0x41).
- **First area (reading, not checked).** The first entry to area 42 sets flag 1, calls
  `Scenario_CallA(0)` and drops the party in (0) (`:1561-1567`). Save 18, inside
  chapter 4, has flags 0 and 1 only. That fits area 42 as the chapter's first area.
  Chapter 3's end changes no area, so the code read does not say which entry it is.

## Flags

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena04_Object0` (`:2150-2153`), on the row it is given (the current row, `Scena04_ObjectHook` `:2144`) | A field object with `+0x86` 0 is triggered | `Scena04_Start` | Nothing found tests it | optional |
| 1 | `Scena04_EnterArea` (`:1563`) | First entry to area 42 (0x2A). Counters cleared, `Scenario_CallA(0)`, drop-in 0 | `Scena04_Start` | The same site: it runs once | once-only set-up |
| 2 | `Scena04_StepArea28` (`:2181`) | Area 40 (0x28): a step at x cell 0x57..0x5A, z cell 0x13..0x15, with the leader's `+8` not 4..6. Sound 0x204, then run 1 step 1 with 0x1E frames: drop-in 0, effect kind 0x29, a quake, the run's end | `Scena04_Start` | The same site: it happens once. After it, the hook shows messages 2 and 3 instead (until flag 3) | progress |
| 3 | `Scena04_EnterArea` (`:1541`) | Entry to area 40 by entrance 7. Music 0x3F, `Scenario_CallA(1)`, drop-in 1, run 1 step 7 | `Scena04_Start` | That site. `Scena04_EnterArea` (`:1568`): area 42's set-up while flag 4 is clear. `Scena04_StepArea28` (`:2188`): the hook moves on to message 0x26 | progress |
| 4 | `Scena04_Scene1` step 0x14 (`:1804`) | Run 1, after messages 0xD and 0xE, stream 4 and the visit to area 42. The change to area 40 with flags 0x82 follows. Steps 0x15..0x17 change area 40 three more times, with counter 1 = 1, 2, 0 | `Scena04_Start` | `Scena04_EnterArea` (`:1551`): area 40's set-up by counter 1 while flag 5 is clear. `:1568`: area 42's set-up stops | progress |
| 5 | `Area40_ChoiceFlag5` (`area_w1a.cpp:306-316`), current row | Area 40's choice 0. Answer 0 sets run 1's step to 0x1C, any other answer to 0x1E. Run 1 waits at step 0x1B after stream 3 (`:1837-1842`) until this choice moves it | `Scena04_Start` | `Scena04_EnterArea` (`:1551`): area 40's set-up stops | progress |
| 6 | `Scena04_EnterArea` (`:1581`) | First entry to area 48 (0x30) by entrance 3. `Area_EnterTransition` 5, drop-in 3 | `Scena04_Start` | The same site | once-only set-up |
| 7 | `Scene2Body` step 0x10 (`:2059`), run 2 | Run 2, after the 0xA0-frame camera pull. The change to area 65 (0x41) follows. Run 2 starts from the cell hook, `Scena04_Cell0` (`:2225`) | `Scena04_Start` | `Scena04_EnterArea` (`:1585`): area 48's two map bytes (0x83 and 0x84, row 6) are 0x51 while clear. `:1596`: area 65's set-up runs while 7 is set and 12 is clear. `Scena04_StepArea28` (`:2199`): area 40's message 0x26 stops | progress |
| 8..11 (0x8..0xB) | `Area40_ChoiceLever8..B` through `Lever` (`area_w1a.cpp:98-106`, `:317-322`), fixed row `Cond_Flags + 0x20` | Area 40's choices 1..4, answer 0: the lever flag, the gate redrawn, sound 0x203 | `Area40_Init` (`area_w1a.cpp:546-549`): all four, on every entry to area 40 not by entrance 1. Also `Scena04_Start` | `Area40_SetGate` (`area_w1a.cpp:326-333`): the gate's 28 cells open when the four flags (`0x903FB1 & 0xF`) are 5, that is 8 and 10 set, 9 and 11 clear | transient |
| 12 (0xC) | `Scene2Body` step 0x16 (`:2107`), run 2 | Run 2, counter 0 reaches 0x11 after the change to area 54 (0x36). Then `Field_SetStatus80`. This ends the chapter | `Scena04_Start` | `Scena04_EnterArea` (`:1596`): area 65's set-up stops | progress |

### The runs in order (reading, not checked)

1. Area 42's first entry (flag 1).
2. Area 40's step hook, run 1 steps 1..6 (flag 2).
3. Area 40 by entrance 7, run 1 steps 7..0x1B: areas 42 and 40 (flags 3, 4).
4. Area 40's choice 0 (flag 5). Answer 0 gives step 0x1C: `Game_Mode` 7, then a wait
   for mode 2, then step 0x1E. Any other answer goes to step 0x1E directly. The run
   ends at step 0x1E.
5. Area 48 by entrance 3 (flag 6). Area 40's levers (flags 8..11) open its gate.
6. The cell hook, run 2: areas 65 and 54 (flags 7, 12, the chapter's end).

Run 2 shakes the map (`Scena04_Tremor`) on steps 2..12. No step in this chapter starts
an event battle.

## End

`Scene2Body` step 0x16 (`scena_sc3.cpp:2102-2112`) clears script flag 0x40, sets flag
12, calls `StatusBit80()` (`Field_SetStatus80`) and ends the run. The step before it
(0x15, `:2096-2099`) changes to area 54 (0x36). The next `Area_Enter` after flag 12
steps the chapter to 5 (`mode_flow.cpp:549-556`).

**The row as left (reading):**

- Always set: 1..7 and 12.
- Optional: 0 (an object).
- Lever flags 8..11: left as the last visit to area 40 left them. Entry by any
  entrance but 1 clears all four. The gate is open only at the pattern 8 and 10 set,
  9 and 11 clear.
- No chapter-4 code clears a flag during the chapter. The lever flags are cleared by
  area 40's init.

**Against the saves** ([`observed-rows.md`](observed-rows.md)):

- Save 18 (US, 08:17), inside chapter 4: **0..1**. Flag 1 is area 42's first entry.
  Flag 0 is the object. Agree.
- Saves 19..22 (chapter 5 to 15): **0..8 10 12**. Saves 13..17 (JP sibling card,
  07:41, chapter 6): **1..8 10 12**.
- Agree: 1..7 and 12 are set on every save past the chapter.
- Agree: flag 0 is optional. The sibling card's route left it clear.
- Agree: the levers are left at 8 and 10 set, 9 and 11 clear. That is the gate-open
  pattern 5. So on every route the last area 40 visit that the saves keep ended with the
  gate open.
- Flags 13..31 are clear on every save. No setter was found for them.

## Event flags used by this chapter

The `Scena04_*` functions set, clear and test no event flag (`Cond_Flags + 0xA0`).

Area 40's own code uses event flag 0x27 (`area_w1a.cpp:368`, `:408`, `:474`, `:551`,
`:560`). `Area40_Init` clears it on entry from zone 6 by entrance 2. It closes cells
beside the grid while 0x27 is clear. This is area code, which also runs in other
chapters, so it is listed here for context only.

## Unresolved

- **Flag 0.** `Scena04_Object0` sets it. Which object has `+0x86` 0 in chapter 4's
  areas is in the area data, not read here. Nothing found tests the flag.
- **Flags 8..11 outside chapter 4.** `Lever` writes the fixed row `Cond_Flags + 0x20`
  in any chapter. So area 40 visited in a later chapter changes chapter 4's row. The
  saves do not show this. Not checked.
- **Flag 5** is set through the current row (`Area40_ChoiceFlag5`). If area 40's choice
  0 runs in another chapter, it sets that chapter's flag 5 and moves its run's step to
  0x1C or 0x1E. Not checked.
- **`Game_Mode` 7** at run 1 step 0x1C (`:1851-1858`). What mode 7 is was not read
  here. Answer 0 of area 40's choice 0 leads to it.
- **The first area of the chapter.** It is the area whose entry follows chapter 3's
  run 8. That area 42 is first is a reading from save 18.
- **Area 40's step hook messages** 2, 3 and 0x26 (`:2189-2203`) go to run 1 step 0,
  which only waits for the message to close. So they only show messages.
- **The levers in the saves.** The four flags are chapter 4's row, but area 40's init
  clears them on most entries. So their state in a save shows the last area 40 visit,
  not chapter 4's progress.
