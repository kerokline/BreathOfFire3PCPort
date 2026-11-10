# Story flags: chapter 12

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Source: `src/game/scena_sc12.cpp` (`Scena12_*`, original `0x55E4E0..0x561DB0`).
The file's wrappers (`scena_sc12.cpp:68..71`): `Bits()` reads `Chapter_FlagRow`
afresh, `Flag(n)` tests, `Set(n)` sets, `Clr(n)` clears. `SetPicked(base, rest)`
(`:142..149`) sets one flag of ten picked by the party set, below. State 0 is the
shared `ScenaShared_State0` (`scena_sc13.cpp:1424`, original `0x5646B0`). The only
`Scena12_*` function in another file is `Scena12_Set752ReloadJoin25`
(`scena_calls.cpp:708`), a party set-up that touches no flag. `scena_sc5.cpp`,
`scena_sc9a.cpp` and `scena_sc9b.cpp` name `Scena12_*` only in comments. No file
names `Cond_Flags + 0x60` or a fixed row 12 (census of `tools/story_flags.py`, and a
grep for `0x60` / `0x903FF0`).

Areas the chapter's code names: `0x65`, `0x79`, `0x82..0x88`, `0xBC`, `0x7E` (the
next chapter's). Event battle: `0x26` (38, Ammonite in `docs/boss-fights.md`),
through `BattleBytes()` = `Field_StartEventBattle(0x26)` (`:1329`) at run 9 steps
`0x16` and `0x1F`.

**The party-set choice.** `Pick(base)` (`:135..140`) reads `AreaW0b_PartySet`
(`Cond_Flags + 0x19C`) `& 0x7F`: values 7..10 pick `base + 0..3`, values 13..18 pick
`base + 4..9`, and 11, 12 or any other value pick none. `SetPicked(base, false)` sets
the picked flag. `SetPicked(base, true)` sets it and clears the other nine. The two
uses with `true` wait for `AreaW3c_Load0F` at 1, which `PartyForm_End` sets when the
formation screen closes (`field_s.cpp:641`). So the ten flags record the party set
chosen at a party screen (reading, not checked).

## Start

- **Who starts it.** `Scena11_Scene9` step 9 (`scena_sc11.cpp:1270..1278`):
  `Field_SetStatus80`, counter 2 (`0x90384A`) = 2, `Field_ChangeArea(0x79,
  0x290000, 0x330000, 7)`. When the change runs, `Area_Enter`
  (`mode_flow.cpp:548..556`) adds one to `Cond_ByteFA` (11 to 12) and calls
  `Scenario_Start(12)`.
- **`Scenario_Start`** (`title_states.cpp:404..424`) zeroes the scenario bytes
  `0x8034E1..0x8034E9` and writes 0 to the **first dword only** of row 12 (flags
  0..31). Flags 32..63 keep whatever they held. It points `Chapter_FlagRow` at
  `Cond_Flags + 0x60`.
- **State 0** (`ScenaShared_State0`): state 1 only.
- **`Scena12_EnterArea`** (state 1, `scena_sc12.cpp:547`, body `EnterAreaBody`
  `:399`): area `0x79` with counter 2 at 2 (the value chapter 11 left):
  `Draw_PassFlags = 0x1F`, `ScriptFlags_Clear40`, the four counters 0
  (`:408..415`). No area test after it matches `0x79`; state 2. No flag is set at
  the start.
- `Scena12_ArriveHook` (`:195`) acts in area `0x79` only with flag 0 set (script
  message 7 through run 2), so it does nothing at the start.

## Flags

Steps are `MoveScript_PartyRecords[0x65]`, runs `MoveScript_Var7`. "Counter 0" is
`0x903848`, which the area's event scripts set.

| Flag | Set by (function, step, file:line) | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena12_Run4` step 2, `:628` | Run 4 from step 0: area `0x85` entered (`:618`), counter 0 at 5; with flag 22; the script flags `\| 0x16`; the run ends | - | `Scena12_ArriveHook` (`:197`): area `0x79`, x at most `0x160000`: run 2 step `0x3C` (script message 7) | progress (reading) |
| 1 | `Scena12_StepHook`, `:231` | Area `0x86`, flag 1 clear, x `0x1F8000..0x208000`, z `0x248000..0x270000`: run 9 step `0xA` (the run that leads to event battle 38); four map cells set to `0x70` | - | `Scena12_StepHook` (`:221`): clear, the first rectangle; set, the second rectangle (member 0's height at most `0x2000000`): run 9 step `0x21`, the chapter's end | progress |
| 2 | `Scena12_Run9` step `0x15`, `:1434` | Counter 0 at `0x18`; then `Party_PlaceForBattle`; step `0x16` starts event battle 38 | - | `BossAmmonite_Enter` (`boss_si.cpp:176`): set, the enemy's data is copied from the record of tag `0x96` (`Battle_CopyEnemyData`) | progress (reading) |
| 20 (`0x14`) | `Scena12_Run5` step 3, `:747` | Run 5 from step 2 (object 11): effect `0x77` placed with `+1` = 2 and script message `0xD`; then counters 0 and 2 = `0xA` | `Scena12_Run5` step `0x16`, `:816` | `EffectKind77_Tick` (`effect_3d.cpp:319`): set, each count of the kind-`0x77` effect plays sound `0x20A` | transient; can be left set |
| 21 (`0x15`) | `Scena12_Run6` step 2, `:949` | Counter 0 at 5, with flag 40; `ChangeArea(0x79, 0x238000, 0x310000, 6)` | - | nothing found | progress (reading) |
| 22 (`0x16`) | `Scena12_Run4` step 2, `:627`; `Scena12_Run5` step `0x14`, `:792` | Run 4: with flag 0. Run 5 (object 10): the tile word `0x939A00`'s low byte in `0x62..0x66`: drop-in 9, step `0x28` | - | nothing found | progress (reading) |
| 23 (`0x17`) | `Scena12_Run4` step `0x15`, `:651` | Run 4 from step `0x14` (object 9): drop-in 1, counter 0 at 6; `ChangeArea(0x65, 0x440000, 0x88000, 0x82)` | - | nothing found | progress (reading) |
| 24 (`0x18`) | `Scena12_Run4` step `0x18`, `:674` | After script message `0x11` and the palette greyed (step `0x17`); `ChangeArea(0x85, 0x1E0000, 0x5D0000, 0x82)`, `Music_Play(0x8F)` | - | nothing found | progress (reading) |
| 25 (`0x19`) | `Scena12_Run4` step `0x1C`, `:699` | Request 6 done (request back to 0); `Party_HealJoined`, `SetPicked(0x1A, false)`, `ChangeArea(0x85, 0x3D0000, 0x2B8000, 0x83)` | - | `Scena12_StepHook` (`:242`): area `0x85`, clear: run 4 step `0xA` (script message `0xB`) | progress (reading) |
| 26..35 (`0x1A..0x23`) | `SetPicked(0x1A, ...)`: `Scena12_Run4` step `0x1C`, `:701` (set only); step `0x1E`, `:707` (set, the other nine cleared) | The party set at run 4's step `0x1C`; step `0x1E` waits for `AreaW3c_Load0F` at 1 | `SetPicked(0x1A, true)`, `:148` (the nine not picked) | nothing found | choice (one of ten) |
| 36 (`0x24`) | `Scena12_Run5` step 8, `:774` | Run 5 from step 5: drop-in 3, area `0x88`; counter 0 at `0xA`; `ChangeArea(0x88, 0xC0000, 0x590000, 0x85)` | - | `EnterAreaBody` (`:447`): area `0x85`, 36 set, 38 clear, `Cond_ByteFD` 2: run 5 step `0x19` | progress (reading) |
| 37 (`0x25`) | `Scena12_Run5` step 9, `:779` | Counter 0 at `0xA`; counter 2 = `0xA`; effect `0x77` placed with `+1` = 0 | - | `Scena12_StepHook` (`:249`): area `0x88`, 37 set, 38 clear, x above `0x1B0000`, z's high word 7..11: run 5 step `0x1B` (script message `0x13`) | progress (reading) |
| 38 (`0x26`) | `Scena12_Run5` step `0x28`, `:850` | Counter 0 at `0x14` (after flag 22's drop-in 9); counter 2 = 3; area `0x85` by the party set (8, 13, 16: entrance `0x86`; 17: `0x85`) | - | `Scena12_StepHook` (`:249`) and `EnterAreaBody` (`:447`): set, those two run-5 starts stop | progress (reading) |
| 39 (`0x27`) | `Scena12_Run5` step `0x29`, `:863` | Counter 0 at `0xA`; area `0x85` by the party set (8: entrance `0x87`, `0x11`: `0x88`); steps `0x2A..0x2D` then start run 6 | - | nothing found | progress (reading) |
| 40 (`0x28`) | `Scena12_Run6` step 2, `:945` | Run 6 (from run 5 step `0x2D`): area `0xBC`, counter 0 at 5 | - | `EnterAreaBody` (`:496`): area `0xBC`, counter 2 at 1: effect `0x76` only while clear | progress (reading) |
| 41 (`0x29`) | `Scena12_Run6` step `0x15`, `:1058` | After the hops through area `0xBC` (steps 6..`0x14`); `ChangeArea(0xBC, 0x120000, 0x400000, 0x8E)` | - | nothing found | progress (reading) |
| 42 (`0x2A`) | `Scena12_Run7` step 1, `:1087` | Run 7 from step 0 (cell hook entry 3): area `0xBC`, counter 0 at 2; the script flags `\| 0x80` | - | `Scena12_CellDoor` (`:305`): set, the cell hook answers `0xFF` (run 7 does not start again) | once-only set-up |
| 43 (`0x2B`) | `Scena12_Run7` step `0xD`, `:1128` | Run 7 from step `0xA`: drop-in `0x10`, area `0xBC`, counter 0 at `0x14`; `ChangeArea(0xBC, 0x120000, 0x70000, 0x92)` | - | `Scena12_StepHook` (`:256`): area `0xBC`, clear: the three rectangles that start run 7 at step 5 or `0xA` | progress (reading) |
| 44 (`0x2C`) | `Scena12_Run7` step `0x14`, `:1170` | After script message `0x17` and request 6; `Party_HealJoined`, `SetPicked(0x32, false)`, counter 2 = 9, `ChangeArea(0xBC, 0x370000, 0x3F8000, 0x94)` | - | nothing found | progress (reading) |
| 45 (`0x2D`) | `Scena12_Run9` step `0x20`, `:1455`; step `0x21`, `:1467` | Step `0x20`: counter 0 at 5, after the battle at step `0x1F`. Step `0x21`: the second rectangle in area `0x86` (the end path) | - | nothing in chapter 12's code (see Unresolved) | progress |
| 46 (`0x2E`) | not by chapter 12's code; `Area136_Tail19` state 10, `area_w3e.cpp:264` | Area `0x88`'s tail with key item 6 held: script message `0x82`, event flag `0x29` toggled | `Area136_Tail19` state 13, `area_w3e.cpp:291` | `EffectKind77_End` (`effect_3d.cpp:333`): while set, the kind-`0x77` effect's end does not start run 5 step `0x19` | transient |
| 47 (`0x2F`) | `EnterAreaBody`, `:468` | Area `0x86`, flag clear, member 0's x below `0x300000`: run 9 step 0 | - | `EnterAreaBody` (`:467`): set, run 9 step 0 does not start again | once-only set-up |
| 48 (`0x30`) | `Scena12_Run9` step 1, `:1345` | Run 9 step 0's drop-in 0 (skipped while member 2's `+0x137` is 3), counter 0 at 1 | - | nothing found | once-only set-up |
| 50..59 (`0x32..0x3B`) | `SetPicked(0x32, ...)`: `Scena12_Run7` step `0x14`, `:1172` (set only); `Scena12_Run8` step `0x14`, `:1286` (set, the other nine cleared) | The party set at run 7's step `0x14`; run 8 step `0x14` (no starter in this file) waits for `AreaW3c_Load0F` at 1 | `SetPicked(0x32, true)`, `:148` | nothing found | choice (one of ten) |
| 63 (`0x3F`) | `Scena12_Run9` step `0x2F`, `:1507` | The chapter's end (below) | - | nothing found | progress |

Flags 3..19, 49 and 60..62 are not set, cleared or tested by any chapter 12 code.

**What starts each run.** Object triggers (`:324..387`, index the object's
`+0x86`): 6 run 1 step 0 (run 1 has no step 0: nothing), 9 run 4 step `0x14`, 10 run
5 step `0x14`, 11 run 5 step 2, 12 run 8 step 0, 13 run 8 step `0x1E`, 14 run 1 step
`0xA`. Cell hook entries 0..2 (`Scena12_CellTalk`, `:291`) run 5 step 0 or 5; entry 3
(`Scena12_CellDoor`) run 7 step 0. Run 5 step `0x2D` starts run 6. The step hook and
`EnterAreaBody` start the rest, as in the table. Run 4 step 0 and step `0x1E`, run 8
step `0x14` and run 9 steps `0x14` and `0x1E` have no starter in this file.

## End

- **The step.** `Scena12_Run9` step `0x2F` (`scena_sc12.cpp:1502..1510`): request
  0, counter 2 = 0, step and run 0, **flag 63**, `Field_SetStatus80`
  (`kSetBit80`), `Field_ChangeArea(0x7E, 0x500000, 0x1E0000, 4)`. `Area_Enter` then
  starts chapter 13 (`mode_flow.cpp:548`).
- **The path.** `Scena12_StepHook` (`:234..239`): area `0x86`, **flag 1 set**,
  member 0's height at most `0x2000000`, x `0x2E0000..0x2F8000`, z
  `0x290000..0x2A0000`: run 9 step `0x21` (`Kind2_Place(2)`, **flag 45**), then steps
  `0x22`, `0x2B`, `0x2C` (effects of kind `0x13`, a transition), `0x2D`, `0x2E`
  (script message `0xB`), `0x2F`.

**The row as left (reading, not checked):**

- Always set on the end path: 1, 45, 63.
- Set on the way through, by the order of the runs: 0, 2, 21..25, 36..44, 47, 48.
- Choices: exactly one of 26..35 and one of 50..59 when the party set is 7..10 or
  13..18; none when it is 11, 12 or another value. Each `true` call clears the
  other nine, so only one stays set.
- Transient: 20 (set at run 5 step 3, cleared at run 5 step `0x16`; left set if
  step 3 runs after the last step `0x16`); 46 (area `0x88`'s tail, set and cleared
  in one sequence).
- Never set: 3..19, 49, 60..62.

**Against the saves** (`observed-rows.md`): saves 20 (42:37), 21 (42:44) and 22
(43:25) all hold chapter 12's row as `0..2 20..25 27 36..45 47..48 52 63`.

- Agree: every flag in the saves has a setter. Every progress flag of the reading
  is set (0..2, 21..25, 36..45, 47, 48, 63).
- Choices: one of each set of ten, 27 (`0x1A + 1`, party set 8) and 52 (`0x32 + 2`,
  party set 9). The party set was different at the two choices.
- Transient: 46 is clear, as the reading expects. 20 is set, so on these routes run
  5 step 3 ran after the last step `0x16`. Event flag 41 (`0x29`, set at step
  `0x16`) is also set in the three saves.
- Not in the saves, as expected: 3..19, 49, 60..62 and the unchosen choice flags.

## Event flags used by this chapter

- **41 (`0x29`)**, set by `Scena12_Run5` step `0x16` (`scena_sc12.cpp:815`,
  `&Cond_Flags[0xa0]`), with flag 20 cleared at the same step.
  `Area136_Tail19` (`area_w3e.cpp:268`, area `0x88`, any chapter) toggles the same
  event flag. The saves past this chapter have it set.
- The engine clears event flag `0x1C` on every area change (`mode_flow.cpp:560`), in
  every chapter.

## Unresolved

- **Steps started elsewhere.** Run 4 step 0 (flags 0, 22), run 4 step `0x1E`, run 8
  step `0x14`, run 9 steps `0x14` (flag 2) and `0x1E` have no starter in this file.
  Event scripts or the area code set them. Until a run shows which steps come in
  which order, the "progress" kinds above are a reading.
- **Flag 2 and the two fights.** Event battle 38 is called at run 9 step `0x16`
  (after flag 2 is set) and at step `0x1F` (no flag set first). `BossAmmonite_Enter`
  copies different enemy data only with flag 2 set. Which fight comes first, and
  whether both are fought, needs a run.
- **Run 9 steps `0x12`, `0x13` and `0x17`.** Step `0x12` loops on held buttons and
  counter 2; step `0x13` and step `0x17` (after the battle at `0x16`) have no case,
  so the run waits there for code outside this file.
- **Run 5 step `0x19`.** `EffectKind77_End` (`effect_3d.cpp:333..340`) starts run 5
  at step `0x19` when the effect ends with flag 46 clear. That is how the run-5
  steps `0x19..0x1A` are reached (reading, not checked).
- **Flag 45 elsewhere.** `EffectKind45_End` (`effect_2d.cpp:213`) clears flag 45 of
  the current row and `EffectKind45_*` test 37 and 38. That effect is chapter 6's
  (the valve gauge, `docs/cheats.md` section 7, spawned at `scena_sc6.cpp:1626`), so
  it is not counted here. Not checked whether any chapter 12 area spawns kind
  `0x45`.
- **Other current-row code.** `BossMap_UpdateFromEnemies` (`boss_h.cpp:355`, flags
  `0x33`/`0x34`) and `BossDodai_*` (`boss_sd.cpp`) set or test flags 51 and 52 of
  the current row, which overlap this chapter's choice set 50..59. Their own
  comments place them in the area-79 fights (`boss_h.cpp:20`), not in this chapter.
  Not checked.
- **Event scripts.** `Area_TestCondition` and the event-script opcodes
  (`event_script.cpp:679..693`, `field_o.cpp:1304`) can set or test row 12 by an
  index from the area data. Those uses are not visible in source.
- **Flags 32..63 at the start.** `Scenario_Start` clears only flags 0..31. The code
  does not clear flags 32..63 when chapter 12 starts. A save editor that builds a
  chapter 12 start must clear them itself. That they are already 0 in a real game
  (nothing sets row 12 earlier) is a reading, not checked.
