# Story flags: chapter 1

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 1's row is `Cond_Flags + 8` (`0x903F98`), save block `+0x5B8`. The model
for this page is [`../story-flags.md`](../story-flags.md) section 3. The saves are
in [`observed-rows.md`](observed-rows.md).

Sources read:

- `src/game/scena_sc1.cpp`: the start, the runs (`Scena01_Scene*`), the object
  handlers and the cell handler. Flag wrappers `Flag`, `SetFlag`, `ClearFlag`
  on `Bank()` = `Chapter_FlagRow` (`scena_sc1.cpp:83..86`).
- `src/game/mode_states.cpp`: `Scena01_EnterArea` (original `0x539B20`, state 1)
  and `Scena01_StepHook` (original `0x53D830`). These are chapter 1's code too,
  but the extractor does not attribute them (`g.flags_set` through a struct, and
  `Flag` / `SetFlag` on `Bank()`, `mode_states.cpp:106..108`). Five of the
  chapter's flags are set only here.
- `src/game/rest_1a.cpp:401` and `src/game/map_cells.cpp:93..94`, which the
  extractor lists for this chapter.

"Run" is `MoveScript_Var7`, "step" is byte `0x8034E5`, "counter n" is byte n of
`0x903848`. "Area" is `Game_AreaNumber`, decimal with the code's hex in
brackets. "Event battle n" is `Field_StartEventBattle(n)` (`cl::kSeHelper`,
`scena_sc1_callees.h:67`); [`../boss-fights.md`](../boss-fights.md) names
battles 2 and 3.

## Start

Chapter 0 ends at `Scena00_Run11` step 9 (`scena_sc0.cpp:1126..1134`):
`Field_ChangeArea(9, 0x570000, 0x40000, 7)` and then `Field_SetStatus80`
(`Field_StatusBits |= 0x80`). On the next area entry, `Area_Enter` sees bit 7,
adds one to the chapter byte `Cond_ByteFA` and calls `Scenario_Start` with it
(`mode_flow.cpp:548..556`). So chapter 1 starts on entry to area 9.

`Scena01_Start` (original `0x539AE0`, state 0, `scena_sc1.cpp:194..200`):

- `Scenario_CallA(0)` (the chapter's call table entry 0; what it does is not read).
- `Field_ChangeArea(9, 0x570000, 0x40000, 7)`, the same place chapter 0 sent
  the party to.
- **Writes 0 to the dword `0x903F98` (`scena_sc1.cpp:197`).** That is bytes 0..3
  of chapter 1's row, so flags 0..31 are cleared. Flags 32..63 are not touched.
- Clears the four counters and sets state 1.

It sets no flag, adds no party member and starts no run. The first run comes
from `Scena01_EnterArea`: area 9, flag 0 clear and counter 2 = 0 start run 1
(`mode_states.cpp:600..605`).

## Flags

File names are short: `sc1` = `scena_sc1.cpp`, `ms` = `mode_states.cpp`.
`EnterArea` = `Scena01_EnterArea`'s body, `StepHook` = `Scena01_StepHook`.

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena01_Scene01` step 2, `sc1:232` | Run 1: after the change to area 9 at (0x470000, 0x1D0000), on counter 0 = 0x1E. Then the change to area 10 (0xA) | `Scena01_Start` (dword write, `sc1:197`) | `EnterArea` area 9 (`ms:600`): run 1 starts only while clear | progress |
| 1 | `Scena01_Scene02` step 0xB, `sc1:307` | Run 2, area 10: the leader has settled (counter 0 = 1, `Field_Kind2Hold` 0), after transitions 0xF / 0x10 and music 0xE | `Scena01_Start` | `EnterArea` area 10, counter 2 = 0 or 7 (`ms:609`, `ms:620`): run 2 starts only while clear | progress |
| 2 | `Scena01_Scene03` step 4, `sc1:353` | Run 3 first half: the camera pulled out and back, the pointing hand at (0xD2, 0x3C) until a button | `Scena01_Start` | `StepHook` area 19 (0x13): with flag 27 set and 2 clear, a rectangle starts run 3 at step 0 (`ms:817`); with 2 set and 3 clear, a line starts run 3 at step 0xA (`ms:824`) | progress |
| 3 | `Scena01_Scene03` step 0x16, `sc1:424` | Run 3 second half: member 1 dropped in, four kind-0x13 effects, music 0x14 and 0x13 | `Scena01_Start` | `StepHook` area 19 (`ms:824`); `Scena01_Cell` (`sc1:2090`): the cell hook starts run 5 only with 3 set | progress |
| 4 | `Scena01_Scene05` step 0xC, `sc1:481` | Run 5 (from the cell hook): message 8, sound 0x103, the change to area 8 at (0x540000, 0x1D0000); at the same step `Inventory_Add(0, 0x22, 1)` (`sc1:488`) | `Scena01_Start` | `Scena01_Cell` (`sc1:2090`): the cell hook stops starting run 5 once 4 is set. Run 5 sets 4 only on the branch where the leader's +0x89 is 4 (`sc1:443`); otherwise message 7 and no flag | progress |
| 5 | `Scena01_Scene06` step 0xF, `sc1:559` | Run 6, after transition 4: `Scenario_CallB(0)`, bit 0 of byte +0xB of the character record of id 4 cleared, then the change to area 8 at (0x320000, 0x120000) | `Scena01_Start` | `EnterArea` area 8, counter 2 = 3 (`ms:586`): run 6 step 0xA only while clear. `StepHook` area 8 (`ms:809`): with 6 set and 5 clear, a rectangle changes to area 8 at (0x340000, 0x178000) | progress |
| 6 | `Scena01_Scene06` step 1, `sc1:515` | Run 6 start: member 2 dropped in. Run 6 step 0 waits for the message choice (word `0x7DEE48` = 2, bit 1 of `0x7DEE44`) and bit 4 of bank `0x90410C` | `Scena01_Start` | `EnterArea` area 8, counter 2 = 2 (`ms:580`): run 6 from step 0 only while clear. `StepHook` area 8 (`ms:802`): while clear, cell x 0x548000 starts run 0x15 (message 0x34, then run 6) | progress |
| 7 | `Scena01_Scene06` step 0x1A, `sc1:634` | Run 6 end: after the button loop (animation 0x44 on button 0x40), two effects, member 6 dropped in | `Scena01_Start` | Nothing in the code read tests it | progress |
| 9 | `Scena01_Scene08` step 6, `sc1:727` | Run 8, area 23 (0x17): after the angle tests and timed waits (sounds 0x200, 0x207); then the change to area 23 at (0x480000, 0x60000) with counter 2 = 3 | `Scena01_Start` | `EnterArea` area 23, counter 2 = 0 (`ms:692`): run 8 from step 0 only while clear | progress |
| 10 | `Scena01_Scene08` step 0x3C, `sc1:806` | Run 8 end, after event battle 2 (step 0x39, `sc1:787`) and the party placed at (0x1E0000, 0x1A0000) | `Scena01_Start` | `EnterArea` area 23, counter 2 = 1 or 2 (`ms:699`, `ms:706`); `StepHook` area 23 (`ms:832`): while clear, a rectangle sets counter 2 = 7 and changes to area 23 at (0x180000, 0x180000) | progress |
| 11 (0xB) | `EnterArea` area 22 (0x16), `ms:671` | First entry to area 22: run 9 from step 0 (message 1) | `Scena01_Start` | `EnterArea` area 22 (`ms:667`): the first of the area's five set-ups | progress |
| 12 (0xC) | `Scena01_Scene09` step 5, `sc1:890` | Run 9, area 22: message 2 closed; the four cells (0x51..0x52, 0x1F..0x20) set to 0 | `Scena01_Start` | `EnterArea` area 22 (`ms:672`): run 9 step 3 while clear; `StepHook` area 22 (`ms:839`): the area's three step triggers need it set | progress |
| 13 (0xD) | `EnterArea` area 22, `ms:680` | The area 22 entry after flag 12: run 9 from step 8 (the four cells set back to 0xC0 / 0xA1) | `Scena01_Start` | `EnterArea` area 22 (`ms:675`); `StepHook` area 22 (`ms:840`): while clear, a rectangle changes to area 22 at (0x58000, 0x140000) | progress |
| 14 (0xE) | Not set by chapter 1's code | - | `Scena01_Start` | `Area12_WaitFlagE` (`area_w0a.cpp:413`) tests the current row in area 12. Whether area 12 is reached in chapter 1 is not read | unknown |
| 15 (0xF) | `Scena01_Scene09` step 0xB, `sc1:906` and `sc1:911` | Run 9: on counter 0 = 0x1E (member 2 dropped in) or 0x32 (member 4 dropped in) | `Scena01_Start` | `EnterArea` area 22 (`ms:681`): run 9 step 9 while clear; `StepHook` area 22 (`ms:847`) | progress |
| 16 (0x10) | `Scena01_Scene09` step 0x14, `sc1:944` | Run 9, after message 0xC: on counter 0 = 0x3C, member 5 dropped in | `Scena01_Scene0A` step 2, `sc1:1095` (run 0xA, after message 0x18, unless counter 0 = 0x14) | `EnterArea` area 22 (`ms:684`): run 9 step 0x11 while clear; `StepHook` area 22 (`ms:855`): the trigger for run 9 step 0x12 needs 16 and 18 clear | transient |
| 18 (0x12) | `Scena01_Scene09` step 0x25, `sc1:1034` | Run 9, after event battle 3 (step 0x24, `sc1:1029`); member 0xA dropped in. Flag 63 is cleared at the same step | `Scena01_Start` | `Scena01_Scene09` step 0x11 (`sc1:930`) waits for it; `Scena01_Scene0A` step 0xB (`sc1:1112`): clear goes back to area 23, set goes on to area 5; `StepHook` area 22 (`ms:855`). See also Unresolved (area 21) | progress |
| 20 (0x14) | `Scena01_Object01`, `sc1:1974` | Object handler 1: starts run 0x12 (music 0x1A, member 0xB dropped in) once | `Scena01_Start` | `Scena01_Object01` (`sc1:1969`): once only. `Scena01_Scene0A` step 0xB (`sc1:1116`): with 18 set, 20 picks step 0x14 (`ChangeArea` flags 0x85) over step 0x1E (0x84); both lead to area 5 | optional |
| 21 (0x15) | `Scena01_Scene0C` step 8, `sc1:1315` | Run 0xC, area 5: after message 4; the change to area 5 at (0x540000, 0x250000) | `Scena01_Start` | `EnterArea` area 5, counter 2 = 0 (`ms:542`): run 0xC only while clear | progress |
| 22 (0x16) | `Scena01_Scene0C` step 9, `sc1:1322` | Run 0xC: the change to area 5 at (0x100000, 0xD0000) | `Scena01_Start` | `PartyFormAction_Form0Begin` (`rest_1a.cpp:401`) tests chapter 1's row by its fixed address in every chapter: with 22 set the form-0 pose is 0x50 or 0x40, without it 0x41 | progress |
| 24 (0x18) | `StepHook` area 96 (0x60), `ms:861` | The leader steps on x 0x10000 / 0x18000, z 0x110000..0x128000 in area 96. The hook answers 0 (no scene) | `Scena01_Start` | Nothing in the code read tests it | optional |
| 25 (0x19) | `Scena01_Scene0B` step 0, `sc1:1186` | Run 0xB, first entry to area 7: the change to area 7 at (0x20000, 0x140000) | `Scena01_Start` | `EnterArea` area 7 (`ms:570`): run 0xB only while clear | progress |
| 26 (0x1A) | `Scena01_Scene0B` step 4, `sc1:1207` | Run 0xB: after two kind-0x13 effects at the camera, on counter 0 = 0x12 | `Scena01_Start` | Nothing in the code read tests it | progress |
| 27 (0x1B) | `Scena01_Scene0B` step 0x11, `sc1:1234` | Run 0xB from step 0xF (object handler 0x11): member 1 dropped in, transition 0xD; then the change to area 14 (0xE) at (0xF0000, 0x1F0000) | `Scena01_Start` | `StepHook` area 7 (`ms:790`): while clear, three door lines start run 0xB step 0xA (message 0x20); `StepHook` area 19 (`ms:817`); `EnterArea` area 14 (`ms:646`) | progress |
| 28 (0x1C) | `EnterArea` area 14, `ms:653`; and `Scena01_Scene0B` step 0x13, `sc1:1249` | Entry to area 14 with 27 set and counter 2 = 0: set at once, with `Scenario_CallA(1)` and the camera distance + 0xB80. Run 0xB step 0x13 sets it again after the camera pull | `Scena01_Start` | `EnterArea` area 14 (`ms:650`); `StepHook` area 10 (`ms:784`): while clear, cell x 0x698000 at z 6 / 7 starts run 0x14 (message 0x18) | progress |
| 29 (0x1D) | `Scena01_Scene09` step 0x18, `sc1:968` | Run 9: the first time step 0x18 runs. It picks the change to area 22 at (0x3D0000, 0x470000) with flags 0x87; later passes use 0x8E | `Scena01_Start` | `Scena01_Scene09` step 0x18 (`sc1:965`) | once-only set-up |
| 57 (0x39) | `EnterArea` area 23, counter 2 = 4, `ms:721` | Entry to area 23 from run 8 step 0xE (`sc1:732..734`, between flags 9 and 10): `AreaMap_SetupEntries`, `MoveCmd_TestFB(0x49, 0xC)`, `Field_StatusBits` bit 0 cleared | not cleared (above the dword `Scena01_Start` clears) | `EnterArea` area 23 counter 2 = 4 (`ms:715`): once only | progress |
| 58 (0x3A) | `Scena01_Scene0D` step 5, `sc1:1407` | Run 0xD, area 0: the camera pulled in, music 0; then the change to area 0 at (0x150000, 0x130000) | not cleared | `EnterArea` area 0, counter 2 = 0 or 1 (`ms:522`, `ms:531`): run 0xD and the camera offset only while clear | progress |
| 59 (0x3B) | `Scena01_Scene0D` step 6, `sc1:1418` | Run 0xD end of its first part | not cleared | Nothing in the code read tests it | progress |
| 60 (0x3C) | `Scene0DEnd` (`sc1:1347`) from `Scena01_Scene0D` step 0xC (`sc1:1436`) or `Scene0DChoice` (`sc1:1355`, steps 0xF and 0x12) | Run 0xD from object handlers 6..9: counter 3 = 0x14 ends the run with 60. On steps 0xF / 0x12 (after message 0x36) also bit 1 of `0x7DEE44` | not cleared | `EnterArea` area 13 (0xD) (`ms:634`): with 60 set, 62 clear and `Cond_ByteFD` = 1, run 0xE (the chapter's end) starts | choice |
| 61 (0x3D) | `Scene0DEnd` from step 0xC (`sc1:1440`) or `Scene0DChoice` (`sc1:1359`) | The same run, counter 3 = 0x19 | not cleared | Nothing in the code read tests it. It does not start the end | choice |
| 62 (0x3E) | `Scena01_Scene0E` step 3, `sc1:1492` | Run 0xE: music 0x11, message 0x10; then the change to area 13 at (0x80000, 0x530000) | not cleared | `EnterArea` area 13 (`ms:634`): run 0xE only while clear | progress |
| 63 (0x3F) | `EnterArea` area 23, counter 2 = 4, `ms:722` | With flag 57, the same entry | `Scena01_Scene09` step 0x25, `sc1:1035` (with flag 18, after event battle 3) | Nothing in the code read tests it | transient |

Flags 8, 17, 19, 23 and 30..56 are not set, cleared or tested by any chapter 1
code read.

**`Scena01_Start` clears flags 0..31 but not 32..63.** A chapter-start row
crafted for chapter 1 can carry anything in 32..63 from the save it was made
from. In a real game the row is empty when the chapter starts, so this matters
only for crafted saves.

### The order the code imposes

Reading, not checked: flags 0, 1 (areas 9 and 10); 25, 26, 27, 28 (area 7, then
area 14); 2, 3 (area 19 needs 27); 4 (the cell hook needs 3); 6, 5, 7 (area 8);
9, 57 and 63, 10 (area 23, event battle 2); 11, 12, 13, 15, 16, 29, 18 with 63
cleared (area 22, event battle 3); 16 cleared (run 0xA); 21, 22 (area 5); 58,
59, then 60 or 61 (area 0); 62 (area 13). Flags 20 and 24 fall anywhere they
are reached.

## End

`Scena01_Scene0E` (original `0x53CA20`, run 0xE) ends the chapter. `EnterArea`
starts it on entry to area 13 with flag 60 set, flag 62 clear and
`Cond_ByteFD` = 1 (`ms:632..643`). Step 3 sets flag 62. Step 9
(`sc1:1523..1530`) calls `Field_ChangeArea(0xF, 0x360000, 0x500000, 0)` and
`Field_SetStatus80`. On the entry to area 15, `Area_Enter` steps the chapter
byte to 2 and calls `Scenario_Start(2)` (`mode_flow.cpp:548..556`).
`Scena02_Start` then clears the first dword of chapter 2's row
(`scena_sc2.cpp:215..219`).

Only flag 60 opens the end. Flag 61 is the other answer of run 0xD and leaves the
end closed; object handlers 6..9 can start run 0xD again (reading, not checked).

**The row as left (reading):**

- Always set: 0..7, 9..13, 15, 18, 21, 22, 25..29, 57..59, 60, 62.
- Optional: 20 (object handler 1), 24 (area 96's strip).
- Alternatives: 60 and 61. 60 must be set to leave. 61 can be set as well if
  that answer was taken first.
- Cleared before the end: 16 (run 0xA step 2) and 63 (run 9 step 0x25).
- Never set: 8, 14, 17, 19, 23, 30..56.

**Against the saves.** Every save past chapter 1 (saves 11..22, five sources,
US and JP) has chapter 1's row as exactly
`0..7 9..13 15 18 20..22 24..29 57..60 62`. That is the "always set" list plus
both optional flags 20 and 24, with 60 chosen. The reading and the saves agree
on every flag:

- 16 and 63 are clear in every finished row, as the two clears say.
- 61 is set in no save.
- 63 is set mid-chapter: save 10 (area 23, after event battle 2) has
  `9..10 57 63`, which fits 57 and 63 being set on the area 23 entry between
  flags 9 and 10, and 63 cleared later in area 22.
- 24 is optional in order: saves 7 and 9 have it at 00:57 and 02:19, save 8 at
  01:38 does not. Every finished row has it, so no observed route skipped it.
- Saves 2..6 at the first save point (area 10) have `0..1`: runs 1 and 2 done.
- Saves 7..9 (`0..7` with `25..28`) are between run 6 and run 8.

## Event flags used by this chapter

None. No chapter 1 function (`scena_sc1.cpp`, `Scena01_EnterArea`,
`Scena01_StepHook`) sets, clears or tests the event flags at
`Cond_Flags + 0xA0`.

One test reads another bank. `Scena01_Scene06` step 0 (`sc1:508`) tests bit 4 of
`0x90410C` (`at::kStoryFlags`, `scena_sc1_callees.h:29`). That address is
`Cond_Flags + 0x17C`, block `+0x72C`: the second find bank in
[`../story-flags.md`](../story-flags.md) section 1 (`Field_PassageSteps`). The
callee header calls it "story flag bits". The name is wrong for what the address
holds (reading, not checked).

Event flags 32 and 119 first appear in the saves during chapter 1 (saves 7 and
9). Code outside the chapter sets them: event 32 by `Area95_SpawnEffect37`
(`area_w2d.cpp:196`), event 119 by `Rest_Begin` (`field_s.cpp:245`).

## Unresolved

- **Field-script tests.** `Area_TestCondition` (`map_cells.cpp:93..94`) tests
  any chapter row from area data: kind byte 0x01 is chapter 1's row and 0x21 its
  complement. Which area records test chapter 1's flags is in the game data and
  is not read here. Flags 7, 24, 26, 59 and 61 have no tester in the code read.
  Area data may test them.
- **Area 21 sets flag 18 of the current row.** `Area21_ChoiceTrade`
  (`area_w0b.cpp:826`) sets flag 0x12 of the row `Chapter_FlagRow` points at.
  If that trade can run while chapter 1 is current, it sets chapter 1's flag 18
  early. That would let run 9 step 0x11 pass and change run 0xA's branch. Not
  read: which chapters reach area 21.
- **Area 12 tests flag 14.** `Area12_WaitFlagE` (`area_w0a.cpp:413`) waits on
  flag 0xE of the current row. Nothing in chapter 1 sets 14. Not read: whether
  area 12 is reached in chapter 1.
- **Other current-row code not tied to a chapter.** `EffectKind77_Tick` tests
  flag 0x14 of the current row (`effect_3d.cpp:319`), `EffectKind77_End` tests
  0x2E (`effect_3d.cpp:333`), and `BossMap_UpdateFromEnemies` sets and clears
  0x33 / 0x34 (`boss_h.cpp:355..356`, set-ups 18..20). Whether any of these runs
  in chapter 1 is not read. Event battles 2 and 3 are this chapter's only fights
  in [`../boss-fights.md`](../boss-fights.md).
- **Who sets counter 3 to 0x14 or 0x19** for run 0xD's ends (`sc1:1434`,
  `sc1:1353`), and so which answer is 60 and which is 61. Counter 3 is not
  written for this purpose in the code read. It is probably a field script.
- **Flag 4's other branch.** Run 5 sets flag 4 only when the leader's +0x89 is 4
  (`sc1:443`). What +0x89 holds is not read here.
- **Flag 16 kept set.** `Scena01_Scene0A` step 2 skips the clear when counter 0
  is 0x14 (`sc1:1086`). No save has 16 set, so the clear is the path seen.
- **`Scenario_CallA(0)`, `Scenario_CallA(1)`, `Scenario_CallA(2)` and
  `Scenario_CallB(0)`** (`sc1:195`, `ms:654`, `ms:712`, `sc1:558`) go through the
  chapter's call tables. What they do is not read. None is known to touch a flag
  row.
- Not checked in play: everything on this page is read from source.
