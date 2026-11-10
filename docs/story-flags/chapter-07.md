# Story flags: chapter 7

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 7's row is `Cond_Flags + 0x38` (`0x903FC8`), save block `+0x5E8`. Source:
`src/game/scena_sc7.cpp` (the `Scena07_*` functions), plus `Area74_PushToggleRow7`
(`area_w1f.cpp`), which names row 7 outright, and `Area82_OpenMessageByRowFlag4`
(`area_w2a.cpp`), which tests the current row. The file's wrappers are `Flag(n)` and
`SetFlag(n)` around `Bank()`, the dword `Chapter_FlagRow` read afresh
(`scena_sc7.cpp:115-117`). Clears call `Flags_Clear` directly.

Event battle names below are from [`../boss-fights.md`](../boss-fights.md) (fights
29..31). "Zone" is `Cond_ByteFD`, the byte the set-up switches on, as in chapter 0.

## Start

**What starts the chapter.** `Area_Enter` (`mode_flow.cpp:548-556`) steps the chapter
by one when `Field_StatusBits` has bit 7, and calls `Scenario_Start` with the new
number. Chapter 6 sets that bit in `Scena06_EnterArea` (`scena_sc6.cpp:361-366`):
on entry to area `0x4C` with chapter 6's flag `0x2C` set and `0x3F` clear, it sets
flag `0x3F` and calls `Field_SetStatus80`. The bit is set during that entry's state 1,
after `Area_Enter`'s chapter test, so chapter 7 starts at the **next** area entry.
Which area that is depends on the exit the player takes from area `0x4C`; the code
does not name it (reading, not checked).

**`Scenario_Start(7)`** (`title_states.cpp:404`) zeroes the scenario bytes
`0x8034E1..0x8034E9` (bit 7 of `Field_StatusBits` with them), zeroes the first dword
of the row (flags 0..31) and points `Chapter_FlagRow` at it.

**`Scena07_Start`** (state 0, `scena_sc7.cpp:206-209`) zeroes the same dword again
and sets state 1. It sets no flag, places nobody and starts no run. Flags 32..63 of
the row are not cleared by either function (reading): flags 34 and 35 below keep
whatever the row held.

`Scena07_EnterArea` (state 1) then runs for the area being entered.

## Flags

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena07_Scene1` case 3, `scena_sc7.cpp:315` | Run 1 ends: message 1, a fade in, then counter 0 reaches `0x15`. Run 1 is armed by `Scena07_EnterArea` in area `0xAF` zone 1 while flag 0 is clear (`:258-268`) | - | `Scena07_EnterArea:259` (run 1 plays once); `Scena07_StepHook:1304` (run 2 needs flag 0) | progress |
| 1 | `Scena07_Scene2` case `0x1A`, `:475` | Run 2 starts event battle `0x1D` (fight 29, Dolphin) at `:474`, in area `0xAF`. Run 2 is armed by the step hook in area `0xAF` (`:1303-1311`) | - | `Scena07_StepHook:1305` (run 2 plays once); `Scena07_EnterArea:270` (run 3 needs flag 1) | progress |
| 2 | `Scena07_Scene3` case 4, `:523` | Run 3: after stream 0 ends, the change to area `0xAF` (`0x2D8000, 0x5E0000`). Run 3 is armed by `Scena07_EnterArea` in area `0xAF` zone 1 with flag 1 set and flag 2 clear (`:270-278`) | - | `Scena07_EnterArea:270` (run 3 plays once) | progress |
| 3 | `Scena07_Scene4` case `0xC`, `:665` | Run 4 after event battle `0x1E` (fight 30, Gisshan, Scylla, Charyb, `:661`). Step `0xC` is set outside this file, after the battle (reading). Run 4 is armed by the arrive hook in area `0x67` (`:1319-1328`) | - | `Scena07_ArriveHook:1321` (run 4 plays once) | progress |
| 4 | `Scena07_EnterArea`, `:227-228` | First entry to area `0x4A` in zone 1: the kind-2 sprite placed, drop-in 0, the music faded, `Music_Track` 1. An entry in another zone returns at `:223-225` before this test | - | `Scena07_StepHook:1260` (run 5 needs flag 4); `:1269` (area `0x52`: with flag 4 and a member in state 2, the counter-0 `0x10` block is skipped); `Scena07_EnterArea:235` (while clear, flags 5 and 6 are cleared on entry to any other area); `Area82_OpenMessageByRowFlag4` (`area_w2a.cpp:481`: message 8 when set, else 3) | progress |
| 5 | `Area74_PushToggleRow7`, `area_w1f.cpp:413` | Area 74 (`0x4A`) handler 0: the leader (member id 2) pushes the running object in direction 5 while flag 6 is clear | `Scena07_EnterArea:236`, on entry to any area but `0x4A` while flag 4 is clear | Nothing in source tests it | unknown |
| 6 | `Area74_PushToggleRow7`, `area_w1f.cpp:415` | The same push in direction 1 | `Area74_PushToggleRow7:411` (a push in direction 5 while set); `Scena07_EnterArea:237` (as flag 5) | `Area74_PushToggleRow7:410` | transient |
| 7 | `Scena07_Scene5` case 2, `:692` | Run 5: the change to area `0x35` with track `0x4D`; on counter 0 = 9, flag 7 and the change to area `0x58`. Run 5 is armed by the step hook in area `0x4A` (flag 4 set, flag 7 clear, x `0x348000`, z cells `0x13..0x16`, `:1260-1265`) | - | `Scena07_StepHook:1260` (run 5 plays once); `Scena07_EnterArea:216` (area `0x35`: while clear, scenario call A 1, drop-in 2, height `0xD80`) | progress |
| 8 | `Scena07_Scene6` case 1, `:773` | Run 6 at step 0: on entry to area `0x55` while flag 8 is clear (`:239-246`), scenario call B 0 on counter 0 = 5, then flag 8 on counter 0 = 7 | - | `Scena07_EnterArea:240` (that set-up plays once) | progress |
| 9 | `Scena07_Scene6` case `0x14`, `:882` | Run 6 after event battle `0x1F` (fight 31, Garr, `:876`), with the change to area `0x55`. Step `0x14` is set outside this file, after the battle (reading). This part of run 6 starts at step `0xA` from `Scena07_Cell0` (`:1343-1347`) | - | `Scena07_EnterArea:247` (area `0x55`: map cells `(0x25, 0x3D)` and `(0x26, 0x3D)` set to `0x51` while clear; with flag 9, DAT 0 loaded and the cells set to `0x50`); `Scena07_StepHook:1282` (area `0x55`: message 2 and run 6 step 5 only while clear) | progress |
| 10 [0xA] | `Scena07_Object0`, `:1208` | Object handler 0: drop-in 3, the object to state 4, animation 5 | - | Nothing in source tests it | optional |
| 11 [0xB] | `Scena07_Object1`, `:1217` | Object handler 1: key item 5 given, drop-in 4, the object to state 4, animation 6 | - | Nothing in source tests it | unknown |
| 12 [0xC] | `Scena07_Object2`, `:1227` | Object handler 2: drop-in 9, the object to state 4, animation `0x1D` | - | Nothing in source tests it | unknown |
| 34 [0x22] | `Scena07_Scene7`, `:1124` with `0x22` from `:1136` | Run 7 step 1: the first reward. On counter 0 = `0x1F`, `Inventory_Add` of category 1; when it succeeds, message `0x94`, flag `0x22`, sound `0x106`. A full inventory ends the run with no flag. Run 7 is started by `Scena07_Object3` | - | Not tested in chapter 7's row. `ObjectReward` (`:1198`) tests flag `0x22` of **chapter 6's** row (`Cond_Flags + 0x30`): when set, run 7 starts at step `0x14` (system message 1, no reward) | optional |
| 35 [0x23] | `Scena07_Scene7`, `:1124` with `0x23` from `:1145` | Run 7 step `0xC`: the second reward, category 2, message `0x95`. Run 7 at step `0xA` is started by `Scena07_Object4` | - | As flag 34: `ObjectReward` tests chapter 6's flag `0x23` | optional |

The object handlers (`Scena07_Object0..4`) are called by `Scena07_ObjectHook`
(`:1175-1179`) through `Scena07_ObjectHandlers` by the object's byte `+0x86`. The game
data decides which object has which index and when it triggers, so "optional" for
10, 34 and 35 is a reading.

Run 6 has three entry points. Step 0 is the area `0x55` set-up (flag 8). Step 5 is the
step hook's message 2 in area `0x55` (`:1279-1289`); steps 5 and 6 end the run with
no flag. Step `0xA` is the cell hook's handler `Scena07_Cell0`, the long part that
holds event battle `0x1F` and ends the chapter. `Scena07_CellHook` (`:1335-1340`) tests
no flag: whether the cell record holds a condition is in the data.

`Scena07_Scene8` (run 8, started by the step hook in area `0x69`, `:1292-1301`) sets no
flag.

## End

**The chapter ends at `Scena07_Scene6` case `0x27`** (`:1078-1089`), the last step of
run 6. On counter 0 = `0x29` it ends the run, calls `Field_SetStatus80` (`:1084`) and
changes to area `0x20` (`0x900000, 0x550000`, `:1085`). The entry to area `0x20` steps
the chapter to 8 (`mode_flow.cpp:548-556`). Flag 9 is set earlier on the same path
(case `0x14`); the end step sets no flag.

**The row as left** (reading):

- Always set on the way through: 0, 1, 2, 3, 4, 7, 8, 9.
- Optional: 10 (object handler 0); 34 and 35 (the rewards, only when chapter 6's
  flags `0x22` / `0x23` are clear).
- Kind not established: 11 and 12 (object handlers 1 and 2); 5 and 6 (the push
  state in area 74).
- Cleared before the end: 6 by a push in direction 5. Flags 5 and 6 are cleared on
  every area entry until flag 4 is set; after that only the push clears 6.
- Alternatives: none.

**Against the saves.** Saves 20, 21 and 22 of
[`observed-rows.md`](observed-rows.md) are past this chapter. All three hold
chapter 7 row **0..5, 7..9, 11..12**, the same row, so they count as one data point.

- 0..4 and 7..9 set: agree with "always set".
- 5 set, 6 clear: agree with the push reading (a last push in direction 5, or no push
  in direction 1).
- 10 clear: agrees with "optional", object handler 0 not triggered.
- 11 and 12 set: consistent with the source. One play cannot tell whether they
  are optional.
- 34 and 35 clear: agree. Chapter 6's row in the same saves has 34 and 35 set, so
  run 7 took the step `0x14` branch and gave no reward.

## Event flags used by this chapter

The event flags are `Cond_Flags + 0xA0`, save block `+0x650`.

| Event flag | Site | What |
|---:|---|---|
| 79 [0x4F] | `Scena07_EnterArea:257` | Set on every entry to area `0x69`. Set in saves 20..22 |

No `Scena07_*` function tests or clears an event flag.

## Unresolved

- **The entry area of the chapter.** Chapter 7 starts on the first area entry after
  `Scena06_EnterArea` sets bit 7 in area `0x4C`. The code does not name the
  destination.
- **`Scena07_Scene7`'s flag argument** (`:1124`, non-literal) is 34 or 35 from its
  two call sites (`:1136`, `:1145`). It sets the flag in chapter 7's row, but
  `ObjectReward` (`:1198`) tests chapter 6's row. After a reward taken in chapter 7,
  chapter 6's flag is still clear, so a second trigger of the same object would give
  the reward again. Reading, not checked: the game data may remove the object. This
  is Capcom's code as read, not a known defect entry. Check it in play, or against
  the PSX twin, before using it.
- **Steps set from outside.** `Scena07_Scene4` step `0xC` and `Scena07_Scene6` step
  `0x14` wait for a step this file never sets. The reading is that the battle's end
  sets it (as `boss-fights.md` has for the other chapters). Flags 3 and 9 depend on it.
- **Flags 5, 11, 12** have no test in source. Game-data event scripts can test the
  current row through event-script condition 2 (`EventCond_Flag`,
  `event_script.cpp:706-708`, row `EventScript_FlagBank`, by default the chapter).
  Map-cell records can test row 7 through `Area_TestCondition` codes `0x07nn`
  (`map_cells.cpp:81-97`). Reading the data, or a run, is needed to know what they
  gate.
- **`Area82_OpenMessageByRowFlag4`** (`area_w2a.cpp:481`) tests flag 4 of whatever
  row is current. That it runs in chapter 7 is a reading from `Scena07_StepHook`'s
  own area `0x52` test. Which chapters reach area 82's handler 3 is in the data.
- **`Scena07_CellHook`** has one cell record (`Scena07_Cells`). Whether the record
  carries a condition, which would gate the chapter's last run, is in the data.
