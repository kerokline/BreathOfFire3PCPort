# Event flags: areas of worlds 0 and 1

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

The event-flag bank is `Cond_Flags + 0xA0` (`0x904030`), save block `+0x650`.
Files read: `src/game/area_w0a.cpp`, `area_w0b.cpp`, `area_w0c.cpp`,
`area_w1a.cpp`, `area_w1b.cpp`, `area_w1c.cpp`, `area_w1d.cpp`, `area_w1e.cpp`,
`area_w1f.cpp`.

How each file reaches the bank:

- `area_w0a.cpp`, `area_w0c.cpp`, `area_w1a.cpp`, `area_w1c.cpp`, `area_w1d.cpp`,
  `area_w1f.cpp` write it out at each call: `Mem(::bof3::Img(Cond_Flags_addr) + 0xa0)`.
- `area_w1b.cpp` uses `Bank()` (`area_w1b.cpp:57`), which is `Cond_Flags + 0xa0`.
- `area_w1c.cpp` `FlagsToggle()` (`area_w1c.cpp:54`) and `area_w1b.cpp:625` call
  `Flags_Toggle` (`bof3::addr::Flags_Toggle`, the comments name it `0x57C160`). A
  toggle tests the flag and then sets or clears it. Those rows have Op `toggle`.
- Not the event bank, so left out of the table: `area_w0b.cpp` `FlagRow()`
  (`area_w0b.cpp:952`, the pointer `0x929ED0`, the chapter row) and every
  `Chapter_FlagRow` site (`area_w0a.cpp:412-413`, `area_w0c.cpp:536-537`,
  `area_w1a.cpp:308-311`, `area_w1a.cpp:614-617`, `area_w1c.cpp:383`,
  `area_w1c.cpp:533`); fixed chapter rows `Cond_Flags + 0x18 / 0x20 / 0x28 / 0x38 /
  0x48 / 0x50 / 0x70` (`area_w1a.cpp:103`, `area_w1c.cpp:445-447`,
  `area_w1c.cpp:717-718`, `area_w1d.cpp:113`, `area_w1f.cpp:410-415`,
  `area_w1f.cpp:643`, `area_w1f.cpp:824`, `area_w1f.cpp:964`, `area_w1f.cpp:975`);
  and the bank `0x9040CC` (`kFlagsCC`, `area_w1b.cpp:190`, `area_w1b.cpp:375`),
  which lies past the 40-byte event bank (see Unresolved).
- `area_w1e.cpp` has no event-flag site. Its object triggers receive `0x904030`
  as an argument and do not read it (`area_w1e.cpp:878`).

"Tail" is the field mode tail: kind byte `AreaW0a_TailKind`, state byte
`AreaW0c_TailState`, sub-kind byte `AreaW0c_TailSub`. A flag set or cleared in
a loop, or through a wrapper that is called with several numbers, gets one row
per flag at the same line.

## Sites

| Flag | Op | Function | File:line | Context |
|---|---|---|---|---|
| 30 | set | Area02_ChoiceArmTail | area_w0a.cpp:181 | Area 2, choice 0: cursor row 0 or 1 arms tail kind 5 with sub-kind 5 or 4, then sets the flag. Another row returns before the set. |
| 31 | set | Area04_ChoiceArmTail | area_w0a.cpp:241 | Area 4, choice 0: cursor row 0 or 1 arms tail kind 5 with sub-kind 0 or 1, then sets the flag. |
| 1 | set | RunDropInTail (Area27_TailDropIn1) | area_w0c.cpp:119 | Area 27, tail kind 1, state 0 (`area_w0c.cpp:277` passes flag 1): set with ScriptFlags_Set40 when Field_Request leaves 2; then Party_DropIn(1) and Field_ChangeArea(0x1B, ...). |
| 0 | set | RunDropInTail (Area27_TailDropIn2) | area_w0c.cpp:119 | Area 27, tail kind 2, state 0 (`area_w0c.cpp:282` passes flag 0): the same machine with Party_DropIn(2). |
| 1 | clear | RunDropInTail (Area27_TailDropIn1) | area_w0c.cpp:145 | Area 27, tail kind 1, state 4: once counter 3 is 0x31, the flag is cleared with ScriptFlags_Clear40 and the tail disarmed. The flag is held only while the tail runs. |
| 0 | clear | RunDropInTail (Area27_TailDropIn2) | area_w0c.cpp:145 | Area 27, tail kind 2, state 4: as above for flag 0. |
| 30 | set | Area34_ChoiceTail5 | area_w0c.cpp:532 | Area 34, choice 0: answer 0 or 1 arms tail kind 5 with sub-kind 5 or 4 and sets the flag (the same shape as area 2's site). |
| 109 | set | Area34_Trigger51 | area_w0c.cpp:567 | Area 34, Field_ObjectTriggers id 51: sets the flag, then tests 109..112. |
| 109 | test | Area34_Trigger51 | area_w0c.cpp:569 | Loop over flags 109..112; any clear returns before flag 113 is set. |
| 110 | test | Area34_Trigger51 | area_w0c.cpp:569 | As above. |
| 111 | test | Area34_Trigger51 | area_w0c.cpp:569 | As above. |
| 112 | test | Area34_Trigger51 | area_w0c.cpp:569 | As above. |
| 113 | set | Area34_Trigger51 | area_w0c.cpp:570 | Set when flags 109..112 are all set. |
| 51 | test | Area36_StepHook | area_w0c.cpp:602 | Area 36 step hook: with Cond_ByteFD 2 and the flag set, a step in x 0x46..0x48, z 0x23..0x25 with the leader's pose 0, 6 or 7 runs Party_DropIn(0). Clear: no drop-in. |
| 114 | set | Area37_TailLeave | area_w0c.cpp:689 | Area 37, tail kind 0x2E (armed by choice 21, answer 0), state 0: sets the flag and calls Field_ChangeArea(0x25, 0x2C8000, 0x270000, 1); message 0xB5 follows. |
| 39 | test | Area40_TailPuzzle | area_w1a.cpp:368 | Area 40, tail kind 0x10, state 0: flag set goes to state 0xA (the puzzle is skipped); clear runs Party_DropIn(4) and starts the grid puzzle. |
| 39 | test | Area40_ArriveHook | area_w1a.cpp:408 | Area 40 arrive hook: with the flag set, the grid cells are not toggled. |
| 39 | set | Area40_PuzzleSolved | area_w1a.cpp:474 | Area 40: every grid cell matches Area40_Pattern. Sets the flag, opens cells (0x92..0x93, 0x1C..0x1D), sound 0x103. |
| 39 | test | Area40_Init | area_w1a.cpp:551 | Area 40 init: with Cond_ByteFD 6 and the flag clear, the cells beside the grid are closed. |
| 39 | clear | Area40_Init | area_w1a.cpp:560 | Area 40 init: entered from zone 6 with Cond_ByteFD 2, the flag is cleared (and Field_ScriptFlags bit 12 and counter 0). |
| var | set | Area41_TriggerFlag | area_w1a.cpp:724 | Area 41, Field_ObjectTriggers ids 59 and 60: the flag is the byte at 0x5F5F2D + the object's +0x86 (Area41_TriggerFlags 0x5F5F68 per `area_w1a_callees.h:104`); sound 0x201. |
| 11 | test | Area42_StartTimer | area_w1b.cpp:123 | Area 42, handler 0: with the leader's character index 1, the timer starts only if flag 11 is clear. |
| 17 | test | Area42_StartTimer | area_w1b.cpp:124 | Area 42, handler 0: the timer starts only if flag 17 is clear. |
| 11 | set | Area42_StartTimer | area_w1b.cpp:125 | Area 42: sets the flag, Field_ScriptFlags bit 5, and arms tail kind 7, state 0xA, countdown 0x384 frames. Flag 11 marks the timed round as running. |
| var | clear | Area42_ClearMemberFlag | area_w1b.cpp:135 | Area 42, handler 1: clears flag (Field_ActiveMember byte +0xA0) + 0x10. Probably one of 18..21; reading, not checked. |
| 18 | test | Touch42 (Area42_Touch12) | area_w1b.cpp:147 | Area 42, handler 2 (`area_w1b.cpp:157`): with the leader's character index 1 and the flag clear, go on. |
| 19 | test | Touch42 (Area42_Touch13) | area_w1b.cpp:147 | Area 42, handler 3 (`area_w1b.cpp:159`): as above. |
| 20 | test | Touch42 (Area42_Touch14) | area_w1b.cpp:147 | Area 42, handler 4 (`area_w1b.cpp:161`): as above. |
| 21 | test | Touch42 (Area42_Touch15) | area_w1b.cpp:147 | Area 42, handler 5 (`area_w1b.cpp:163`): as above. |
| 18 | set | Touch42 (Area42_Touch12) | area_w1b.cpp:148 | Area 42: sets the flag, then calls Area42_CheckAll. |
| 19 | set | Touch42 (Area42_Touch13) | area_w1b.cpp:148 | As above. |
| 20 | set | Touch42 (Area42_Touch14) | area_w1b.cpp:148 | As above. |
| 21 | set | Touch42 (Area42_Touch15) | area_w1b.cpp:148 | As above. |
| 11 | test | Touch42 | area_w1b.cpp:150 | Area 42: flag 11 clear moves the active member's script position on by 1. |
| 18 | test | Area42_CheckAll | area_w1b.cpp:177 | Area 42: loop over flags 18..21; any clear answers 0. |
| 19 | test | Area42_CheckAll | area_w1b.cpp:177 | As above. |
| 20 | test | Area42_CheckAll | area_w1b.cpp:177 | As above. |
| 21 | test | Area42_CheckAll | area_w1b.cpp:177 | As above. |
| 17 | test | Area42_CheckAll | area_w1b.cpp:178 | Area 42: with 18..21 all set and flag 17 clear, the round ends: tail kind 7 state 0, a rank 0..2 from the seconds left (Area42_Rank). |
| 17 | set | Area42_TimerTail | area_w1b.cpp:292 | Area 42, tail kind 7, state 3: after the camera turn, sets flag 17 (the round is won). |
| 11 | clear | Area42_TimerTail | area_w1b.cpp:308 | Area 42, tail kind 7, state 6: clears flag 11 and ends the tail. |
| 11 | clear | Area42_TimerTail | area_w1b.cpp:321 | Area 42, tail kind 7, state 0xA: the countdown reached 0. Clears 11 and 18..21 (the round is lost and reset). |
| 18 | clear | Area42_TimerTail | area_w1b.cpp:322 | As above. |
| 19 | clear | Area42_TimerTail | area_w1b.cpp:323 | As above. |
| 20 | clear | Area42_TimerTail | area_w1b.cpp:324 | As above. |
| 21 | clear | Area42_TimerTail | area_w1b.cpp:325 | As above. |
| 17 | clear | Area42_StepHook | area_w1b.cpp:348 | Area 42 step hook: a step at z 0x308000 with x 0x7B..0x7C, or z 0x368000 with x 0x1D..0x1F, clears 17, 11 and 18..21. |
| 11 | clear | Area42_StepHook | area_w1b.cpp:349 | As above. |
| 18 | clear | Area42_StepHook | area_w1b.cpp:350 | As above. |
| 19 | clear | Area42_StepHook | area_w1b.cpp:351 | As above. |
| 20 | clear | Area42_StepHook | area_w1b.cpp:352 | As above. |
| 21 | clear | Area42_StepHook | area_w1b.cpp:353 | As above. |
| 17 | test | Area42_Init | area_w1b.cpp:372 | Area 42 init, Cond_ByteFD 0: with flag 17 clear, Area42_Rank is picked from bank 0x9040CC flags 0x5C..0x5E. |
| 127 | set | Area43_Trigger58 | area_w1b.cpp:499 | Field_ObjectTriggers id 58 (placed in area 43's block): sets the flag, MoveCmd_TestFB(0x44, 0xB), sound 0x103. |
| 22 | clear | Area44_Init | area_w1b.cpp:526 | Area 44 init, Cond_ByteFD 0 or 2: the gate bytes zeroed and flags 22 and 23 cleared. |
| 23 | clear | Area44_Init | area_w1b.cpp:527 | As above. |
| var | toggle | Area44_SwitchHook | area_w1b.cpp:625 | Area 44 cell hook: toggles the flag in byte 3 of the matching Area44_Switches record (5 bytes each), then arms the record's tail kind (8 or 9). Probably 22 or 23; reading, not checked. |
| 22 | test | Area44_GateTailA | area_w1b.cpp:656 | Area 44, tail kind 8, state 0: gate A at 0 and flag 22 set moves gate A to 1. |
| 22 | test | Area44_GateTailA | area_w1b.cpp:664 | Area 44, tail kind 8, state 0: gate A at 1 and flag 22 clear moves gate A to 0. |
| 22 | test | Area44_GateTailA | area_w1b.cpp:680 | Area 44, tail kind 8, state 0xA: gate B at 0 and flag 22 set moves gate B to 1. |
| 22 | test | Area44_GateTailA | area_w1b.cpp:688 | Area 44, tail kind 8, state 0xA: gate B at 1 and flag 22 clear moves gate B to 0. |
| 23 | test | Area44_GateTailB | area_w1b.cpp:788 | Area 44, tail kind 9, state 0: gate B at 0 and flag 23 set moves gate B to 2. |
| 23 | test | Area44_GateTailB | area_w1b.cpp:796 | Area 44, tail kind 9, state 0: gate B at 2 and flag 23 clear moves gate B to 0. |
| 23 | test | Area44_GateTailB | area_w1b.cpp:814 | Area 44, tail kind 9, state 0xA: gate A at 0 and flag 23 set moves gate A to 2. |
| 23 | test | Area44_GateTailB | area_w1b.cpp:820 | Area 44, tail kind 9, state 0xA: gate A at 2 and flag 23 clear moves gate A to 0. |
| 112 | set | Area46_Trigger54 | area_w1b.cpp:1525 | Field_ObjectTriggers id 54 (placed in area 46's block): sets the flag, then tests 109..112. |
| 109 | test | Area46_Trigger54 | area_w1b.cpp:1527 | Loop over flags 109..112; any clear returns before flag 113 is set. |
| 110 | test | Area46_Trigger54 | area_w1b.cpp:1527 | As above. |
| 111 | test | Area46_Trigger54 | area_w1b.cpp:1527 | As above. |
| 112 | test | Area46_Trigger54 | area_w1b.cpp:1527 | As above. |
| 113 | set | Area46_Trigger54 | area_w1b.cpp:1528 | Set when flags 109..112 are all set. |
| 36 | clear | EndTimed (Area52_TailBlock) | area_w1c.cpp:254 | Area 52, tail kind 15, state 2 (call at `area_w1c.cpp:939`): the timed scene begun at state 0 ends; the flag is cleared and extra object 0 hidden. |
| 38 | clear | EndTimed (Area52_TailBlock) | area_w1c.cpp:254 | Area 52, tail kind 15, state 0x16 (call at `area_w1c.cpp:957`): as above for the scene begun at state 0x14. |
| 37 | clear | EndTimed (Area52_TailBlock) | area_w1c.cpp:254 | Area 52, tail kind 15, state 0x2A (call at `area_w1c.cpp:966`): as above for the scene begun at state 0x28. |
| 13 | toggle | Area48_ToggleFlagD | area_w1c.cpp:290 | Area 48, choice 1 (handler 0): with the leader's character index 1, toggles flag 13. |
| 2 | clear | Area49_SwapFlags2To3 | area_w1c.cpp:404 | Area 49: called by Area49_StepHook (a step at x 0x4D8000 with Cond_ByteFD 0, `area_w1c.cpp:397`) and by Area49_EffectFrame (Field_Request 5 and a pending area change, `area_w1c.cpp:432`). Clears flag 2. |
| 3 | set | Area49_SwapFlags2To3 | area_w1c.cpp:405 | Same calls: sets flag 3. |
| var | test | Area49_EffectFrame | area_w1c.cpp:462 | Area 49, effect kind 0x70: a member entering one of the seven Area49_Zones gets the zone's direction (byte 4), turned by 4 when the flag in the zone's byte 5 is set. |
| var | test | Area49_EffectFrame | area_w1c.cpp:471 | Same flag (zone byte 5) for a member already moving in the zone. |
| 28 | test | Area49_CellHook | area_w1c.cpp:524 | Area 49 cell hook: with flag 28 set, the switch is not worked (answer 0). |
| var | toggle | Area49_CellHook | area_w1c.cpp:525 | Area 49: toggles the flag in byte 3 of the matching Area49_CellSwitches record (nine records, 4 bytes each); sound 0x202. These are the flags Area49_EffectFrame reads; reading, not checked. |
| var | toggle | Area52_CellHook | area_w1c.cpp:895 | Area 52 cell hook: unless a member stands in Area52_Rects, toggles the flag in byte 3 of the matching Area52_CellSwitches record (four records, 5 bytes each), and arms tail kind 15 at the record's state (byte 4). Probably 36..38; reading, not checked. |
| 37 | clear | Area52_ClearFlag25 | area_w1c.cpp:1000 | Area 52, handler 5: clears flag 37. |
| 110 | set | FlagsGather (Area56_Trigger52) | area_w1d.cpp:85 | Area 56, Field_ObjectTriggers id 52 (`area_w1d.cpp:357` passes 0x6E). Then tests 109..112. |
| 111 | set | FlagsGather (Area57_Trigger53) | area_w1d.cpp:85 | Area 57, Field_ObjectTriggers id 53 (`area_w1d.cpp:386` passes 0x6F). Then tests 109..112. |
| 109 | test | FlagsGather | area_w1d.cpp:87 | Loop over flags 109..112; any clear returns before flag 113 is set. |
| 110 | test | FlagsGather | area_w1d.cpp:87 | As above. |
| 111 | test | FlagsGather | area_w1d.cpp:87 | As above. |
| 112 | test | FlagsGather | area_w1d.cpp:87 | As above. |
| 113 | set | FlagsGather | area_w1d.cpp:88 | Set when flags 109..112 are all set. |
| 51 | test | Area59_StepHook | area_w1d.cpp:419 | Area 59 step hook: as area 36's, with z 0x29..0x2B. Cond_ByteFD 2 and the flag set allow Party_DropIn(0). |
| 104 | set | Area62_TailGiveItem59 | area_w1d.cpp:558 | Area 62, tail kind 0x2A, state 1: Inventory_Add(0, 0x59, 1) succeeds, then sound 0x106, Msg_OpenSystem(2) and the flag. A full inventory (Msg_OpenSystem(3)) does not set it. |
| 65 | set | Area75_TailPresses | area_w1f.cpp:640 | Area 75, tail kind 30, state 2: sets the flag, then Field_ChangeArea(0x4B, 0x170000, 0x190000, 0x82) and music 0x58. |
| 65 | clear | Area75_TailPresses | area_w1f.cpp:799 | Area 75, tail kind 30, state 0x22: clears the flag, then Field_ChangeArea(0x4B, 0x198000, 0x1B8000, 0x83). |
| 65 | clear | Area75_PhaseReached | area_w1f.cpp:973 | Area 75, Area75_Phases[4]: clears the flag; with chapter byte Cond_ByteFA 0xA, sets chapter row 10's flag 0 and runs Party_DropIn(5). |

## What each flag means here

- **0**: set and cleared by area 27's tail kind 2 (`area_w0c.cpp:119`, `:145`). Held only while that tail runs. Meaning unknown. Not set in any real save.
- **1**: the same for area 27's tail kind 1. Held only while the tail runs. Not set in any real save.
- **2**: cleared by Area49_SwapFlags2To3 (`area_w1c.cpp:404`). Nothing here sets or tests it. Not set in any real save.
- **3**: set by Area49_SwapFlags2To3 (`area_w1c.cpp:405`). Set in every real save; it is also set one frame into a new game, so the area 49 site may only set it again. Meaning unknown.
- **11**: area 42's timed round is running. Set by Area42_StartTimer, cleared when the round ends, times out, or by the step hook. Not set in any real save.
- **13**: toggled by area 48's handler 0 (`area_w1c.cpp:290`). Set in saves 13..17, 19, 20..22, clear in the others. Meaning unknown.
- **17**: area 42's round is won. Set by Area42_TimerTail state 3; tested to stop the timer starting again and to skip the rank pick; cleared by the step hook (`area_w1b.cpp:348`). Not set in any real save, so the step hook (or another site) probably clears it on leaving; reading, not checked.
- **18..21**: area 42's four touch targets (handlers 2..5). Each set once; all four set ends the round. Cleared on time-out and by the step hook. Not set in any real save.
- **22**, **23**: area 44's two switches. Cleared on area entry with Cond_ByteFD 0 or 2; they decide which way gates A and B move (tail kinds 8 and 9). Not set in any real save.
- **28**: blocks area 49's cell switches while set (`area_w1c.cpp:524`). Outside these files it is set by Effect_HoldFlag1C (`scena_sx2.cpp:104`) and cleared on every area entry (`mode_flow.cpp:560`), so it looks like a short hold; reading, not checked. Not set in any real save.
- **30**: set by area 2's choice and area 34's choice when they arm tail kind 5. Outside these files `field_e2.cpp:1212` clears 30 and 31 at the end of a tail; reading, not checked. Not set in any real save.
- **31**: set by area 4's choice when it arms tail kind 5. Cleared at `field_e2.cpp:1213`. Not set in any real save.
- **36**, **37**, **38**: area 52's three timed scenes; each is cleared when its scene ends. 37 is also cleared by area 52's handler 5. Probably set by the area 52 switch toggle; reading, not checked. Not set in any real save.
- **39**: area 40's grid puzzle is solved. Set by Area40_PuzzleSolved; skips the puzzle when set; cleared by Area40_Init on entry from zone 6 with Cond_ByteFD 2. Not set in any real save.
- **51**: gates the drop-in steps in area 36 and area 59 (with Cond_ByteFD 2). Not set here. Also tested in `area_w2d.cpp:708`, `area_w3a.cpp:749` and `area_w3f.cpp:1023`. Set in saves 20..22.
- **65**: held while area 75's tail kind 30 runs between its two area changes; cleared when that part ends or by Area75_PhaseReached. Not set in any real save.
- **104**: area 62 gave item (0, 0x59). Set in saves 20..22. Nothing here tests it.
- **109..112**: four object triggers, one each: 109 area 34 id 51, 110 area 56 id 52, 111 area 57 id 53, 112 area 46 id 54. Each trigger sets its own flag and sets 113 when all four are set. Set in saves 20..22.
- **113**: all four of 109..112 are set. Set by any of the four triggers. Set in saves 20..22. Nothing here tests it.
- **114**: set by area 37's tail kind 0x2E as it changes to area 0x25. Set in saves 20..22. Nothing here tests it.
- **127**: set by object trigger id 58 (area 43's block) with MoveCmd_TestFB(0x44, 0xB). Set in saves 20..22. Nothing here tests it.
- **var** sites: area 41 triggers 59 and 60 (table at 0x5F5F2D), area 42 handler 1 (member byte +0xA0 + 0x10), area 44 switches, area 49 zones and switches, area 52 switches. See Unresolved.

## Unresolved

- **Op `toggle`.** Four sites call Flags_Toggle (`area_w1b.cpp:625`, `area_w1c.cpp:290`, `:525`, `:895`). The column allows set, clear or test; a toggle does a test and then a set or a clear.
- **Flag numbers in game data.** These need a read of the tables, which this pass does not do: Area41_TriggerFlags (byte at 0x5F5F2D + object +0x86, `area_w1a.cpp:724`); Area44_Switches byte 3 (`area_w1b.cpp:625`); Area49_Zones byte 5 (`area_w1c.cpp:462`, `:471`); Area49_CellSwitches byte 3 (`area_w1c.cpp:525`); Area52_CellSwitches byte 3 (`area_w1c.cpp:895`). The guesses (22/23 for area 44, 36..38 for area 52) come from which flags the same area tests or clears.
- **Area42_ClearMemberFlag** (`area_w1b.cpp:135`): the flag is the active member's byte +0xA0 plus 0x10. Which object is active and what its +0xA0 holds needs a run.
- **Bank 0x9040CC** (`kFlagsCC`, `area_w1b.cpp:190`, `:375`): area 42 tests its flags 0x5C..0x5E. That is byte 0x9040D7, outside the event bank (`0x904030..0x904057`), so it is not an event flag. What that bank is, and where it is saved, is open.
- **Cond_ByteFD** gates several sites (areas 36, 40, 42, 44, 48, 49, 59). Its meaning is not settled here.
- **Flag 3**: whether area 49's set is ever the first set, given it is set one frame into a new game, needs a run.
- **Flags 17, 39 and 22/23 are clear in every real save** although the code sets them on success. Something clears them again (the step hook for 17; area entry for 22/23; Area40_Init for 39 only in one case). Whether 39 should persist needs a run.
