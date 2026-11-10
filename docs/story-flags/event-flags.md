# Event flags: every site, by flag number

**Status:** IN PROGRESS (2026-11-09). Merged by a script from the nine reading passes in
[`event/`](event/), one pass per group of source files. Read from source; not checked in
play. The sites are one reading pass each. Flag numbers and code references only.

The event flags are the bank at `0x904030` (`Cond_Flags + 0xA0`), save block `+0x650`.
[`../story-flags.md`](../story-flags.md) section 9 says how they start, how scripts reach
them and what the kinds are. "Saves" names the real saves of
[`observed-rows.md`](observed-rows.md) in which the flag is set.

454 sites name 132 flags. 53 more sites take their flag number from the game data
(listed at the end).

## Flag 0 (0x00)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | RunDropInTail (Area27_TailDropIn2) | area_w0c.cpp:119 | Area 27, tail kind 2, state 0 (`area_w0c.cpp:282` passes flag 0): the same machine with Party_DropIn(2). |
| clear | RunDropInTail (Area27_TailDropIn2) | area_w0c.cpp:145 | Area 27, tail kind 2, state 4: as above for flag 0. |

## Flag 1 (0x01)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | RunDropInTail (Area27_TailDropIn1) | area_w0c.cpp:119 | Area 27, tail kind 1, state 0 (`area_w0c.cpp:277` passes flag 1): set with ScriptFlags_Set40 when Field_Request leaves 2; then Party_DropIn(1) and Field_ChangeArea(0x1B, ...). |
| clear | RunDropInTail (Area27_TailDropIn1) | area_w0c.cpp:145 | Area 27, tail kind 1, state 4: once counter 3 is 0x31, the flag is cleared with ScriptFlags_Clear40 and the tail disarmed. The flag is held only while the tail runs. |

## Flag 2 (0x02)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | Area49_SwapFlags2To3 | area_w1c.cpp:404 | Area 49: called by Area49_StepHook (a step at x 0x4D8000 with Cond_ByteFD 0, `area_w1c.cpp:397`) and by Area49_EffectFrame (Field_Request 5 and a pending area change, `area_w1c.cpp:432`). Clears flag 2. |

## Flag 3 (0x03)

Saves: all 21.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area49_SwapFlags2To3 | area_w1c.cpp:405 | Same calls: sets flag 3. |

## Flag 4 (0x04)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area77_SetFlag4 | area_w2a.cpp:209 | Area 77 handler 5 (`Area77_Handlers[5]`). The handler does only this. The file header says choice k + 2 runs handler k (reading, not checked). |
| test | On | effect_5a.cpp:158 | Kind 0x18 sub-kind 5's test reads the bank's first dword: "on" when flags 4..7, masked by bits 4..7 of the word +0x3A, equal bits 0..3 of +0x3A. Callers: `EffectKind18_05_Start` (321), `_Idle` (343), `_Pulse` (392); on starts or keeps a palette pulse. Which of flags 4..7 count comes from +0x3A. |

## Flag 5 (0x05)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | On | effect_5a.cpp:158 | As the row for flag 4: the same dword read. |
| test | Scena06_Cell3 | scena_sc6.cpp:2319 | Chapter 6 cell hook entry 3: with chapter flag 0x27 and event flags 5 and 6 set, run 15 starts at step 5 (chapter flag 0x2C clear) or step 0x1E (0x2C set); otherwise run 15 from step 0. |

## Flag 6 (0x06)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | On | effect_5a.cpp:158 | As the row for flag 4: the same dword read. |
| test | Scena06_Cell3 | scena_sc6.cpp:2319 | Same test as the row above (both 5 and 6 must be set). |

## Flag 7 (0x07)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Scena06_Run15 | scena_sc6.cpp:1812 | Chapter 6 run 15 step 6: effect kind 0x4B, drop-in 6, `Cond_ByteFE = 1`, sounds 0x207 and 0x201, then step 9. |
| test | On | effect_5a.cpp:158 | As the row for flag 4: the same dword read. |
| test | EffectKind18_1F_Start | effect_5a.cpp:527 | Kind 0x18 sub-kind 0x1F, state 0. Set: state 3 (palette cycle runs at once); clear: state 1. |
| test | EffectKind18_1F_Wait | effect_5a.cpp:537 | Sub-kind 0x1F, state 1. Once set: brighten (state 2), then the cycle. |

## Flag 11 (0x0B)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area42_StartTimer | area_w1b.cpp:125 | Area 42: sets the flag, Field_ScriptFlags bit 5, and arms tail kind 7, state 0xA, countdown 0x384 frames. Flag 11 marks the timed round as running. |
| clear | Area42_TimerTail | area_w1b.cpp:308 | Area 42, tail kind 7, state 6: clears flag 11 and ends the tail. |
| clear | Area42_TimerTail | area_w1b.cpp:321 | Area 42, tail kind 7, state 0xA: the countdown reached 0. Clears 11 and 18..21 (the round is lost and reset). |
| clear | Area42_StepHook | area_w1b.cpp:349 | As above. |
| test | Area42_StartTimer | area_w1b.cpp:123 | Area 42, handler 0: with the leader's character index 1, the timer starts only if flag 11 is clear. |
| test | Touch42 | area_w1b.cpp:150 | Area 42: flag 11 clear moves the active member's script position on by 1. |
| test | EffectKind28_Start | effect_2a.cpp:349 | Kind 0x28, state 0. Clear: state 1 (wait); set: state 2 (beam drawn). |
| test | EffectKind28_WaitFlag | effect_2a.cpp:356 | Kind 0x28, state 1. Once set, back to state 0, which then goes to the beam state. |
| test | EffectKind28_Beam | effect_2a.cpp:365 | Kind 0x28, state 2: draws the beam and pushes party members off its row (`EffectKind28_PushParty`). Once clear, back to state 0. So the beam stands while the flag is set. |

## Flag 12 (0x0C)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area135_Tail18 | area_w3d.cpp:622 | Area 135, tail kind 18, state 0xA: flag 0xC clear: message 0x83 and flag 0xC set. |
| clear | Area135_ClearFlagC | area_w3d.cpp:206 | Area 135, handler 0 (`Area135_Handlers[0]`): flag 0xC cleared and the tail's sub byte (a count) set to 0. |
| clear | PlaceMarker | area_w3d.cpp:769 | Area 135 init (`Area135_Init`, area_w3d.cpp:779): in zone 4 entered from zone 1, the marker placed by `EventOp_Bx`, flag 0xC cleared, sub byte 0. Also reached from area_w3d.cpp:785 under DIV-0180 when the marker was never placed. |
| test | Area135_StepHook | area_w3d.cpp:566 | Area 135 step hook: in zones 4, 6, 7 (`Cond_ByteFD`) with flag 0xC set and the party near the marker object, `Party_DropIn(0)`. |
| test | Area135_Tail18 | area_w3d.cpp:615 | Area 135, tail kind 18, state 0xA: flag 0xC set: the count in the tail's sub byte printed into message 0x84. |

## Flag 13 (0x0D)

Saves: 13..17 19..20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | EffectKind2A_Start | effect_2a.cpp:623 | Kind 0x2A, state 0. Clear: state 2 (two beams); set: state 1 (one beam). |
| test | EffectKind2A_Beam | effect_2a.cpp:635 | Kind 0x2A, state 1: one beam at z 0x118000, its row pushed. Once clear, state 2. |
| test | EffectKind2A_Beams | effect_2a.cpp:648 | Kind 0x2A, state 2: two beams at z 0xD8000 and 0x158000, rows pushed. Once set, state 1. The flag picks which beams stand. |
| test | EffectKind18Sub0D_Start | effect_5b.cpp:306 | Kind 0x18 sub-kind 0x0D, state 0. Column 0 (+0x36 = 0) with the flag set: frame 3, state 3 (lit). Other columns use `AreaW1c_Count48`, not a flag. |
| test | EffectKind18Sub0D_Wait | effect_5b.cpp:319 | Sub-kind 0x0D, state 1. Column 0 with the flag set: sound 0x20D, opens. |
| test | EffectKind18Sub0D_Idle | effect_5b.cpp:343 | Sub-kind 0x0D, state 3. Column 0: the light stays while the flag is set, closes when clear. |
| toggle | Area48_ToggleFlagD | area_w1c.cpp:290 | Area 48, choice 1 (handler 0): with the leader's character index 1, toggles flag 13. |

## Flag 14 (0x0E)

Saves: 20 22.

No site in our source names this flag.

## Flag 15 (0x0F)

Saves: 20 22.

No site in our source names this flag.

## Flag 17 (0x11)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area42_TimerTail | area_w1b.cpp:292 | Area 42, tail kind 7, state 3: after the camera turn, sets flag 17 (the round is won). |
| clear | Area42_StepHook | area_w1b.cpp:348 | Area 42 step hook: a step at z 0x308000 with x 0x7B..0x7C, or z 0x368000 with x 0x1D..0x1F, clears 17, 11 and 18..21. |
| test | Area42_StartTimer | area_w1b.cpp:124 | Area 42, handler 0: the timer starts only if flag 17 is clear. |
| test | Area42_CheckAll | area_w1b.cpp:178 | Area 42: with 18..21 all set and flag 17 clear, the round ends: tail kind 7 state 0, a rank 0..2 from the seconds left (Area42_Rank). |
| test | Area42_Init | area_w1b.cpp:372 | Area 42 init, Cond_ByteFD 0: with flag 17 clear, Area42_Rank is picked from bank 0x9040CC flags 0x5C..0x5E. |
| test | EffectKind18Sub0B_Start | effect_5b.cpp:494 | Kind 0x18 sub-kind 0x0B, state 0. Waits until set; then sound 0x202, 18 tiles rise and fade, and the effect releases. |

## Flag 18 (0x12)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Touch42 (Area42_Touch12) | area_w1b.cpp:148 | Area 42: sets the flag, then calls Area42_CheckAll. |
| clear | Area42_TimerTail | area_w1b.cpp:322 | As above. |
| clear | Area42_StepHook | area_w1b.cpp:350 | As above. |
| test | Touch42 (Area42_Touch12) | area_w1b.cpp:147 | Area 42, handler 2 (`area_w1b.cpp:157`): with the leader's character index 1 and the flag clear, go on. |
| test | Area42_CheckAll | area_w1b.cpp:177 | Area 42: loop over flags 18..21; any clear answers 0. |

## Flag 19 (0x13)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Touch42 (Area42_Touch13) | area_w1b.cpp:148 | As above. |
| clear | Area42_TimerTail | area_w1b.cpp:323 | As above. |
| clear | Area42_StepHook | area_w1b.cpp:351 | As above. |
| test | Touch42 (Area42_Touch13) | area_w1b.cpp:147 | Area 42, handler 3 (`area_w1b.cpp:159`): as above. |
| test | Area42_CheckAll | area_w1b.cpp:177 | As above. |

## Flag 20 (0x14)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Touch42 (Area42_Touch14) | area_w1b.cpp:148 | As above. |
| clear | Area42_TimerTail | area_w1b.cpp:324 | As above. |
| clear | Area42_StepHook | area_w1b.cpp:352 | As above. |
| test | Touch42 (Area42_Touch14) | area_w1b.cpp:147 | Area 42, handler 4 (`area_w1b.cpp:161`): as above. |
| test | Area42_CheckAll | area_w1b.cpp:177 | As above. |

## Flag 21 (0x15)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Touch42 (Area42_Touch15) | area_w1b.cpp:148 | As above. |
| clear | Area42_TimerTail | area_w1b.cpp:325 | As above. |
| clear | Area42_StepHook | area_w1b.cpp:353 | As above. |
| test | Touch42 (Area42_Touch15) | area_w1b.cpp:147 | Area 42, handler 5 (`area_w1b.cpp:163`): as above. |
| test | Area42_CheckAll | area_w1b.cpp:177 | As above. |

## Flag 22 (0x16)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | Area44_Init | area_w1b.cpp:526 | Area 44 init, Cond_ByteFD 0 or 2: the gate bytes zeroed and flags 22 and 23 cleared. |
| test | Area44_GateTailA | area_w1b.cpp:656 | Area 44, tail kind 8, state 0: gate A at 0 and flag 22 set moves gate A to 1. |
| test | Area44_GateTailA | area_w1b.cpp:664 | Area 44, tail kind 8, state 0: gate A at 1 and flag 22 clear moves gate A to 0. |
| test | Area44_GateTailA | area_w1b.cpp:680 | Area 44, tail kind 8, state 0xA: gate B at 0 and flag 22 set moves gate B to 1. |
| test | Area44_GateTailA | area_w1b.cpp:688 | Area 44, tail kind 8, state 0xA: gate B at 1 and flag 22 clear moves gate B to 0. |

## Flag 23 (0x17)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | Area44_Init | area_w1b.cpp:527 | As above. |
| test | Area44_GateTailB | area_w1b.cpp:788 | Area 44, tail kind 9, state 0: gate B at 0 and flag 23 set moves gate B to 2. |
| test | Area44_GateTailB | area_w1b.cpp:796 | Area 44, tail kind 9, state 0: gate B at 2 and flag 23 clear moves gate B to 0. |
| test | Area44_GateTailB | area_w1b.cpp:814 | Area 44, tail kind 9, state 0xA: gate A at 0 and flag 23 set moves gate A to 2. |
| test | Area44_GateTailB | area_w1b.cpp:820 | Area 44, tail kind 9, state 0xA: gate A at 2 and flag 23 clear moves gate A to 0. |

## Flag 24 (0x18)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | Area108_PlaceScene | area_w2f.cpp:268 | Area 108 handler `+0x3C[4]`, place 0. Flag 24 clear: the active member's script = 1. |

## Flag 25 (0x19)

Saves: all 21.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | Area108_PlaceScene | area_w2f.cpp:270 | Place 0, flag 24 set: flag 25 clear gives `Area108_SetCells(1)` and script 2. |

## Flag 26 (0x1A)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | Area108_PlaceScene | area_w2f.cpp:273 | Place 0, flags 24 and 25 set: flag 26 clear gives script 3, set gives script 4. |

## Flag 27 (0x1B)

Saves: all 21.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | Area108_PlaceScene | area_w2f.cpp:294 | Place 4: script 9 when flag 27 is set, else script 8. Script 9 is the only path to chapter 8's run 3 (see the section on area 108 below). |

## Flag 28 (0x1C)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area112_CellHook | area_w2f.cpp:1153 | Through `Effect_HoldFlag1C(0xF)` (`scena_sx2.cpp:97-104`): sets flag 28 if an effect slot is free; effect kind 4 clears it after 15 frames (`field_c1.cpp:263-269`). |
| set | Area108_CellHook | area_w2f.cpp:442 | Set only if `Effect_FindFree` gives a slot; that slot gets effect kind `0x18`, sub-kind (`+0xB`) `0x1B`, at the cell. |
| set | SwitchHook | area_w3a.cpp:1011 | Through `Effect_HoldFlag1C(0xF)`: flag 28 set for 15 frames, as at `area_w2f.cpp:1153`. Then sound `0x203`. |
| set | Effect_HoldFlag1C | scena_sx2.cpp:104 | Not chapter code: any chapter. Takes a free effect record as kind 4 for `frames` frames, then sets 28. No free record: nothing, 28 not set. The clear is in `EffectKind04_HoldTick` (not in these files), per the comment at line 90. |
| clear | `EffectKind04_HoldTick` | `field_c1.cpp:267` | Cleared when effect kind 4's countdown `+9` reaches 0. `Effect_HoldFlag1C` (`scena_sx2.cpp:97`) sets it and spawns this effect. |
| clear | `Area_Enter` | `mode_flow.cpp:560` | Cleared on every area entry, after the placement script and the area hook. |
| clear | EffectKind18Sub1B_Run | effect_5d.cpp:647 | Not in my files: the effect spawned at `area_w2f.cpp:442-449` clears flag 28 when its counter `+9` reaches `0xF` (15 frames), then releases itself. Listed because it ends the latch `Area108_CellHook` sets. |
| clear | EffectKind18Sub1B_Run | effect_5d.cpp:648 | Kind 0x18 sub-kind 0x1B: a 15-frame run with two `MoveCmd_TestFB` calls on its cell; at frame 0xF it clears the flag and releases. Reading, not checked: the flag is a request that this effect consumes. |
| test | Area49_CellHook | area_w1c.cpp:524 | Area 49 cell hook: with flag 28 set, the switch is not worked (answer 0). |
| test | Area77_CellHook | area_w2a.cpp:155 | Area 77 (0x4D) cell hook. After a cell and pose match a record of `Area77_CellSwitches`, flag 28 set makes the hook answer 0 (no switch). |
| test | Area86_SwitchHook | area_w2b.cpp:271 | Area 86 switch hook. After a switch record of `Area86_Switches` matches the cell and facing, and no party member is near its rectangle, flag 28 set makes the hook answer 0. |
| test | Area112_CellHook | area_w2f.cpp:1133 | Area 112 cell hook: with a matching `Area112_HookCells` entry (pose not 3), flag 28 set makes it answer 0. |
| test | Area108_CellHook | area_w2f.cpp:436 | Area 108 cell hook: with a matching `Area108_HookCells` entry, flag 28 set makes the hook answer 0 (no toggle). Flag 28 is the switch cool-down latch. |
| test | SwitchHook | area_w3a.cpp:1009 | Areas 117 / 118 cell hook (`Area117_SwitchHook` / `Area118_SwitchHook`): with a matching switch record, flag 28 set makes it answer 0. |
| test | Area109_SwitchHook | rest_3g.cpp:611 | Area 109 cell hook, cell (0x1C, 6) with the leader facing 3: flag 0x1C set makes the hook answer 0 (the switch does nothing). |

## Flag 29 (0x1D)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area108_TailPlace | area_w2f.cpp:534 | Tail kind `0xC`, state 5, when the timer reaches 0: flag 29, `MoveCmd_TestFB(0x20, 0x2F)`, state 9, extra object 0's `+0` = 0, timer `0x1E`. State 9 then starts run 3. |

## Flag 30 (0x1E)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area02_ChoiceArmTail | area_w0a.cpp:181 | Area 2, choice 0: cursor row 0 or 1 arms tail kind 5 with sub-kind 5 or 4, then sets the flag. Another row returns before the set. |
| set | Area34_ChoiceTail5 | area_w0c.cpp:532 | Area 34, choice 0: answer 0 or 1 arms tail kind 5 with sub-kind 5 or 4 and sets the flag (the same shape as area 2's site). |
| set | Area108_ChoiceArmTail5 | area_w2f.cpp:219 | Area 108 choice 0 (`+0x34[0]`). Answer 0: tail sub-kind 3, flag 30 (chosen at `:212`), then tail kind 5. |
| clear | `FieldTail_DropInMove` | `field_e2.cpp:1212` | State 4, when counter byte `0x90384B` is `0x31`, after the area change. |
| test | Area108_ScriptByFlags1E | area_w2f.cpp:316 | Area 108 handler 7: flag 30 set gives the active member script `0x12`. Script position `0xFFFE` either way. |

## Flag 31 (0x1F)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area04_ChoiceArmTail | area_w0a.cpp:241 | Area 4, choice 0: cursor row 0 or 1 arms tail kind 5 with sub-kind 0 or 1, then sets the flag. |
| set | Area108_ChoiceArmTail5 | area_w2f.cpp:219 | Same site, answer 1: tail sub-kind 2, flag 31 (chosen at `:215`), then tail kind 5. Other answers set nothing. |
| clear | `FieldTail_DropInMove` | `field_e2.cpp:1213` | The same step as flag 30. |
| test | Area108_ScriptByFlags1E | area_w2f.cpp:318 | Same handler: flag 30 clear and flag 31 set gives script `0x11`; neither keeps the script. |

## Flag 32 (0x20)

Saves: 7 9..20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area95_SpawnEffect37 | area_w2d.cpp:196 | Handler shared by areas 95 and 96 (+0x3C[1]). Spawns an effect of kind 0x37, sound 0x202; in area 96 (`Game_AreaNumber == 0x60`) sets flag 32. |
| test | Sub12Done | effect_5c.cpp:882 | Kind 0x18 sub-kind 0x12's end test (callers `EffectKind18Sub12_Start` 988, `_Wait` 990). Set: the same tile opening as sub-kind 0x11. |

## Flag 33 (0x21)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area95_SpawnEffect37 | area_w2d.cpp:199 | Same handler outside area 96: sets 33 when the chapter byte `Cond_ByteFA` (signed) is below 7. |
| test | Sub11Done | effect_5c.cpp:879 | Same test: if flag 34 is clear, flag 33 set with `Cond_ByteFA` (signed) at most 7 is also done. Done: `Cond_ByteFE` = 1, the tiles at cells (0x45..0x46, 0x3C) open and sink; at start, done releases at once. |

## Flag 34 (0x22)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area95_SpawnEffect37 | area_w2d.cpp:199 | Same line: sets 34 when `Cond_ByteFA` is 7 or more. |
| test | Sub11Done | effect_5c.cpp:878 | Kind 0x18 sub-kind 0x11's end test (callers `EffectKind18Sub11_Start` 972, `_Wait` 974). Set: done. |

## Flag 35 (0x23)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area97_FlagIfKeyItem5 | area_w2d.cpp:317 | `WorldMap_FieldHooks[11]`, in area 97's block. Sets flag 35 when key item 5 is held (`KeyItem_Has(5)`). |

## Flag 36 (0x24)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | EndTimed (Area52_TailBlock) | area_w1c.cpp:254 | Area 52, tail kind 15, state 2 (call at `area_w1c.cpp:939`): the timed scene begun at state 0 ends; the flag is cleared and extra object 0 hidden. |

## Flag 37 (0x25)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Scena05_Cell1 | scena_sc5.cpp:261 | Chapter 5, cell hook entry 1 (`:276`) through `CellCue(0x25, 0x19)`: same gate (chapter flag 6 set, 9 clear); the current run's step set to 0x19. |
| clear | Area52_ClearFlag25 | area_w1c.cpp:1000 | Area 52, handler 5: clears flag 37. |
| clear | EndTimed (Area52_TailBlock) | area_w1c.cpp:254 | Area 52, tail kind 15, state 0x2A (call at `area_w1c.cpp:966`): as above for the scene begun at state 0x28. |

## Flag 38 (0x26)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Scena05_Cell0 | scena_sc5.cpp:261 | Chapter 5, cell hook entry 0 (`:273`) through `CellCue(0x26, 1)`: only with chapter flag 6 set and 9 clear. Also script bit 0x40, counters 2 and 3 zeroed, sound 0x202, the current run's step set to 1. |
| clear | EndTimed (Area52_TailBlock) | area_w1c.cpp:254 | Area 52, tail kind 15, state 0x16 (call at `area_w1c.cpp:957`): as above for the scene begun at state 0x14. |
| clear | EnterAreaBody | scena_sc5.cpp:400 | Chapter 5 area entry, area 0x34 with chapter flag 9 clear, counter 2 = 4: counter 0 zeroed and flag 38 cleared. |

## Flag 39 (0x27)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area40_PuzzleSolved | area_w1a.cpp:474 | Area 40: every grid cell matches Area40_Pattern. Sets the flag, opens cells (0x92..0x93, 0x1C..0x1D), sound 0x103. |
| clear | Area40_Init | area_w1a.cpp:560 | Area 40 init: entered from zone 6 with Cond_ByteFD 2, the flag is cleared (and Field_ScriptFlags bit 12 and counter 0). |
| test | Area40_TailPuzzle | area_w1a.cpp:368 | Area 40, tail kind 0x10, state 0: flag set goes to state 0xA (the puzzle is skipped); clear runs Party_DropIn(4) and starts the grid puzzle. |
| test | Area40_ArriveHook | area_w1a.cpp:408 | Area 40 arrive hook: with the flag set, the grid cells are not toggled. |
| test | Area40_Init | area_w1a.cpp:551 | Area 40 init: with Cond_ByteFD 6 and the flag clear, the cells beside the grid are closed. |

## Flag 40 (0x28)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area111_TailGate | area_w2f.cpp:858 | Area 111 tail kind `0x11`, state `0x16`: `MoveCmd_TestFB(0x11, 0xAD)`, map bytes (`0x11..0x12`, `0xAD`) = `0xA1` and (`0x11..0x12`, `0xAE`) = `0xC0`, sound `0x205`, flag 40. |
| clear | Area111_ArriveHook | area_w2f.cpp:901 | Area 111 arrive hook: flag 40 cleared whenever `Cond_ByteFD` is 0. |
| clear | Area111_Init | area_w2f.cpp:988 | Area 111 init: flag 40 cleared when `Cond_ByteFD` is 0. |
| test | Area111_ArriveHook | area_w2f.cpp:945 | With `Cond_ByteFD` 1, at cell x high word `0x11..0x12`, z `0xB7..0xB8`, and flag 40 clear: `ScriptFlags_Set40`, tail kind `0x11`, state `0x14` (the run that ends at `:858`). |
| test | Area111_Init | area_w2f.cpp:994 | Init with `Cond_ByteFD` 1: flag 40 clear writes map bytes `0x50` / 0 at (`0x11..0x12`, `0xAD..0xAE`); set writes `0xA1` / `0xC0`, the same bytes as `:853-856`. |
| test | EffectKind18Sub23_Start | effect_5e.cpp:191 | Kind 0x18 sub-kind 0x23, state 0. Set: releases at once; clear: waits. Flag is `kSub23Flag` = 0x28 (`effect_5e_callees.h:37`). |
| test | EffectKind18Sub23_WaitFlag | effect_5e.cpp:201 | Sub-kind 0x23, state 1. Once set: slides (+0x30 up to 0x100) and releases. |

## Flag 41 (0x29)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Scena12_Run5 | scena_sc12.cpp:815 | Chapter 12 run 5 step 0x16, counter 0 = 0xA: run ends, then flag 41 set, chapter flag 20 cleared, an effect of kind 0x77. |
| toggle | Area136_Tail19 | area_w3e.cpp:268 | Area 136, tail kind 19, state 10 (reached when `KeyItem_Has(6)`): message 0x82, flag 0x29 toggled; the next states drop in and change to area 0x88 (136). Paired with the current chapter row's flag 0x2E, set at area_w3e.cpp:264 and cleared at :291. |

## Flag 42 (0x2A)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | EffectKind18Sub20_Start | effect_2g.cpp:280 | Kind 0x18 sub-kind 0x20, state 0. Set: starts lowered (height -0x180, state 3); clear: raised (-0x90, state 1). Flag is `kSub20Flag` = 0x2A (`effect_2g_callees.h:28`). |
| test | EffectKind18Sub20_WaitSet | effect_2g.cpp:294 | Sub-kind 0x20, state 1. Once set: sound 0x202, then it lowers (state 2) and `MoveCmd_TestFB(0x27, 0x2F)`. |
| test | EffectKind18Sub20_WaitClear | effect_2g.cpp:317 | Sub-kind 0x20, state 3. Once clear: `MoveCmd_TestFB(0x27, 0x2F)`, sound 0x202, it rises. The flag holds the object down. |
| toggle | Area135_Tail18 | area_w3d.cpp:602 | Area 135, tail kind 18, state 5: when the party is not in the box (`Area135_PartyInBox` 0), flag 0x2A toggled, sound 0x203, a 10-frame wait. |

## Flag 44 (0x2C)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area145_Tail20 | area_w3f.cpp:778 | Area 145, tail kind 20, state 0xA (armed by the cell hook, which needs chapter-13 row flag 9): flag 0x2C set, sound 0x200, `Kind2_Place(0)`, an effect of kind 0x13. |
| clear | Area145_Tail20 | area_w3f.cpp:825 | Area 145, tail kind 20, state 0x16: flag 0x2C cleared, tail disarmed. 0x2C is held only while tail 20 runs from state 0xA. |

## Flag 45 (0x2D)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area145_Tail20 | area_w3f.cpp:872 | Area 145, tail kind 20, state 0x22 (after transitions 8 and 9 and an effect of kind 0x3E): flag 0x2D set, `Field_ChangeArea(0x91, 0xA0000, 0x630000, 0x80)`. |
| test | Area145_StepHook | area_w3f.cpp:722 | Area 145 step hook, zone 4: with flag 0x2D set, at x high 9..0xB, z high 0x62..0x64 and a standing pose, `Party_DropIn(3)`. |
| test | Area145_Tail20 | area_w3f.cpp:798 | Area 145, tail kind 20, state 0xC: when `Area145_TrailStart` answers 0 and flag 0x2D is clear, sounds 0x205 and 0x207 and state 0x1E (the sequence that sets 0x2D); otherwise state 0x14. |
| test | Scena13_StepHook | scena_sc13.cpp:1361 | Chapter 13 step hook, area 0x91: with 45 set, chapter flag 0xB clear and the position in a box, by the lead's facing, starts run 3 at step 0x14 (drop-in 9). |

## Flag 46 (0x2E)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area145_Tail20 | area_w3f.cpp:842 | Area 145, same branch: flag 0x2E set, so sound 0x20C plays only the first time. |
| test | Area145_Tail20 | area_w3f.cpp:840 | Area 145, tail kind 20, state 0x1A (armed by the zone-0 step hook): after `Field_ChangeArea(0x91, ...)`, flag 0x2E clear plays sound 0x20C. |

## Flag 47 (0x2F)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Scena06_Run13 | scena_sc6.cpp:1638 | Chapter 6 run 13 step 4 (reached when chapter flag 0x26 is set after the kind 0x45 effect): map cell (0x5D, 0x2E) set to 0x51, sound 0x201. Step 5 then sets chapter flag 0x27 and the run ends with 47 left set. |
| set | Scena06_Run13 | scena_sc6.cpp:1659 | Chapter 6 run 13 step 6 (chapter flag 0x26 clear), with chapter flag 0x25 set: set, chapter flag 0x25 cleared, step 8. |
| clear | Scena06_Run13 | scena_sc6.cpp:1668 | Chapter 6 run 13 step 7 (step 6 with chapter flag 0x25 clear): cleared, run ends. |
| clear | Scena06_Run13 | scena_sc6.cpp:1677 | Chapter 6 run 13 step 8, once counter 0 is 1: cleared, run ends. So only the step 4/5 path leaves 47 set. |

## Flag 48 (0x30)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area108_FlagIfEffectState5 | area_w2f.cpp:331 | Area 108 handler 8: when `Character_IndexById[leader's +0x89]` is 5, flag 48, `MoveCmd_TestFB(1, 0xB)`, and the running object's `+0` = 0. |

## Flag 49 (0x31)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area84_SetFlag31At5 | area_w2a.cpp:498 | Area 84 handler 0. Clears bit 0 of the active member's +0x80; then, only when the leader's byte +0x89 is 5, sets flag 49 (0x31), calls `MoveCmd_TestFB(9, 0x10)` and clears the running object's byte +0. |

## Flag 50 (0x32)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area127_ClearActive80 | area_w3c.cpp:192 | Area 127. Set (0x32) when the leader's byte `+0x89` is 5, then `MoveCmd_TestFB(0x4E, 0x33)` and the running object's `+0` = 0. |

## Flag 51 (0x33)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area100_ChoiceFlags93 | area_w2d.cpp:634 | Answer 1: sets 51. |
| set | Area100_ChoiceFlags93 | area_w2d.cpp:639 | Answer 2: sets 51. |
| set | Area100_ChoiceFlags93 | area_w2d.cpp:645 | Answer 3 with key item 6: sets 51. |
| set | Area100_ChoiceFlags93 | area_w2d.cpp:655 | Default answer: sets 51. |
| set | Area112_ChoiceFlags34 | area_w2f.cpp:1058 | Area 112 choice 0 (handler 3): message = answer + 3, flag 51 set before the answer is read. |
| set | Scena09_Run15 | scena_sc9a.cpp:1729 | Chapter 9 run 15 step 0x96, after step 0x95 set chapter flag 0x23 and changed into area 0x2D: set, draw pass 0x1F, run ends. |
| clear | Area100_ChoiceFlags93 | area_w2d.cpp:649 | Answer 3 without key item 6: message 9 and flag 51 cleared (147 and 148 unchanged). |
| clear | Area112_ChoiceFlags34 | area_w2f.cpp:1080 | After the answer: if chapter row 13's flag 16 is clear (`:1078`), message 2 and flag 51 cleared again. Flags 52 and 53 keep their new values. |
| test | Area36_StepHook | area_w0c.cpp:602 | Area 36 step hook: with Cond_ByteFD 2 and the flag set, a step in x 0x46..0x48, z 0x23..0x25 with the leader's pose 0, 6 or 7 runs Party_DropIn(0). Clear: no drop-in. |
| test | Area59_StepHook | area_w1d.cpp:419 | Area 59 step hook: as area 36's, with z 0x29..0x2B. Cond_ByteFD 2 and the flag set allow Party_DropIn(0). |
| test | Area100_StepHook | area_w2d.cpp:708 | Area 100 step hook. With `Cond_ByteFD` 3 and flag 51 set, a step on x 0x45..0x47, z 0x33..0x35 with leader pose 0, 7 or 6 sets counter 0 to 0 and calls `Party_DropIn(0)`. |
| test | Area112_StepHook | area_w2f.cpp:1105 | Area 112 step hook: needs `Cond_ByteFD` 3, flag 51, chapter row 13's flag 16 (`:1106`), x high word `0xB..0xD`, z `0x57..0x59`, pose 0, 6 or 7. Then counter 0 = 0, `Party_DropIn(0)`, al 1. |
| test | Area116_StepHook | area_w3a.cpp:749 | Area 116 step hook: needs `Cond_ByteFD` 2, flag 51, x high word `0x46..0x48`, z `0x33..0x35`, pose 0, 6 or 7. Then counter 0 = 0, `Party_DropIn(0)`, al 1. No chapter-row test here, unlike area 112's. |
| test | Area146_StepHook | area_w3f.cpp:1023 | Area 146 step hook, zone 1: with flag 0x33 set, at x high 0x44..0x46, z high 0x2A..0x2C and a standing pose, `Party_DropIn(0)`. |

## Flag 52 (0x34)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area112_ChoiceFlags34 | area_w2f.cpp:1065 | Answer 2: flag 52 set. |
| set | Area112_ChoiceFlags34 | area_w2f.cpp:1069 | Answer 3: flag 52 set. |
| clear | Area112_ChoiceFlags34 | area_w2f.cpp:1061 | Answer 1: flag 52 cleared. |
| clear | Area112_ChoiceFlags34 | area_w2f.cpp:1074 | Any other answer: flag 52 cleared. |
| test | Area112_ChangeAreaByFlags | area_w2f.cpp:1025 | Flag 52 is bit 1. Case 2 (52 only): area `0x92`; neither: area `0x91`. |

## Flag 53 (0x35)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area112_ChoiceFlags34 | area_w2f.cpp:1062 | Answer 1: flag 53 set. |
| set | Area112_ChoiceFlags34 | area_w2f.cpp:1070 | Answer 3: flag 53 set. Answer 4 changes neither flag (message `0xFFFF`). |
| clear | Area112_ChoiceFlags34 | area_w2f.cpp:1066 | Answer 2: flag 53 cleared. |
| clear | Area112_ChoiceFlags34 | area_w2f.cpp:1075 | Any other answer: flag 53 cleared. |
| test | Area112_ChangeAreaByFlags | area_w2f.cpp:1024 | Area 112 handler 0: flag 53 is bit 0 of the case. Case 1 (53 only): `Field_ChangeArea(0x8F, ...)`; case 3 (both): area `0x64`. |

## Flag 54 (0x36)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | `EffectKind30_Slide` | `field_c2.cpp:204` | In area `0x54`, set when the kind-`0x30` object stops at exactly (`0x118000`, `0x4A8000`). |
| clear | `EffectKind30_Slide` | `field_c2.cpp:206` | In area `0x54`, cleared when it stops anywhere else. |

## Flag 55 (0x37)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area135_ChoiceFlag37 | area_w3d.cpp:194 | Area 135, choice 3 (`Area135_Choices[3]`): answer 0 sets flag 0x37. |
| clear | Area135_ChoiceFlag37 | area_w3d.cpp:196 | Area 135, choice 3: any answer other than 0 clears flag 0x37. |

## Flag 56 (0x38)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | Area112_ChoiceClearFlags | area_w2f.cpp:1095 | Then a loop clears flags 56..59 (`0x38..0x3B`), then sound `0x200`. |

## Flag 57 (0x39)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | Area112_ChoiceClearFlags | area_w2f.cpp:1095 | Same loop. |

## Flag 58 (0x3A)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | Area112_ChoiceClearFlags | area_w2f.cpp:1095 | Same loop. |

## Flag 59 (0x3B)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | Area112_ChoiceClearFlags | area_w2f.cpp:1095 | Same loop. |

## Flag 60 (0x3C)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | Area112_ChoiceClearFlags | area_w2f.cpp:1094 | Area 112 choice 2 (handler 5), answer 0: a loop clears flags 60..63 (`0x3C..0x3F`). |

## Flag 61 (0x3D)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | Area112_ChoiceClearFlags | area_w2f.cpp:1094 | Same loop. |

## Flag 62 (0x3E)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | Area112_ChoiceClearFlags | area_w2f.cpp:1094 | Same loop. |

## Flag 63 (0x3F)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| clear | Area112_ChoiceClearFlags | area_w2f.cpp:1094 | Same loop. |

## Flag 64 (0x40)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area100_Trigger13 | area_w2d.cpp:732 | Object trigger 13. Sets 64, `MoveCmd_TestFB(0x4A, 0x2F)`, arms mode-tail kind 28 (`Area100_TailEffect9B`: effect 0x9B, then message script 3). |
| set | Scena09_Run15 | scena_sc9a.cpp:1660 | Chapter 9 run 15 step 0x7E, counter 0 = 5: script byte xor 0x80, step 0x7F, then `ChangeArea(0x3B, ...)`. |

## Flag 65 (0x41)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area75_TailPresses | area_w1f.cpp:640 | Area 75, tail kind 30, state 2: sets the flag, then Field_ChangeArea(0x4B, 0x170000, 0x190000, 0x82) and music 0x58. |
| clear | Area75_TailPresses | area_w1f.cpp:799 | Area 75, tail kind 30, state 0x22: clears the flag, then Field_ChangeArea(0x4B, 0x198000, 0x1B8000, 0x83). |
| clear | Area75_PhaseReached | area_w1f.cpp:973 | Area 75, Area75_Phases[4]: clears the flag; with chapter byte Cond_ByteFA 0xA, sets chapter row 10's flag 0 and runs Party_DropIn(5). |

## Flag 66 (0x42)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area94_InitPatches | area_w2c.cpp:695 | Area 94 init (+0x40), only when the previous area (`AreaW2c_LastArea`) is 0x79. Sets 66, clears 67, then applies the area map's patch chain. |
| clear | Area128_InitPatches | area_w3c.cpp:284 | Area 128 init, same branch: flag 0x42 cleared when the area is entered from area 121. |
| clear | Scena13_Start | scena_sc13.cpp:184 | Same step as the row above: 66 cleared. |

## Flag 67 (0x43)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area128_InitPatches | area_w3c.cpp:283 | Area 128 init: when the previous area is 0x79 (121), flag 0x43 is set, 0x42 cleared and the patch chain applied. |
| set | Scena13_Start | scena_sc13.cpp:183 | Chapter 13 start (state 0): script bit 0x40, pass 0x1F, party pass, call A 0, then 67 set. |
| set | Scena08_Start | scena_sc7.cpp:1384 | Chapter 8 start (state 0): row 8 zeroed, counters zeroed, record 0 party bits, key item 4 swapped for 0xE, then flag 67 set. |
| set | Scena10_ArriveHook | scena_sc9b.cpp:2362 | Chapter 10 arrive hook, area 0x3A, chapter flag 8 set and 9 clear, z at or above 0x300000, x cell near 0x1C: sets chapter flag 9 and event flag 67, counter 0 = 0xA. |
| clear | Area94_InitPatches | area_w2c.cpp:696 | Same entry from area 0x79: clears flag 67. |
| test | Area128_InitPatches | area_w3c.cpp:288 | Area 128 init: with the chapter byte `Cond_ByteFA` above 10 and flag 0x43 set, map cells (0x1F..0x20, 0x39) become 0xC0 and (0x1F..0x20, 0x3A) become 0xA1. |

## Flag 68 (0x44)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | `EffectKind30_Shatter` | `field_c2.cpp:228` | In area `0x92`, set when the kind-`0x30` object's shatter count reaches 0. |

## Flag 69 (0x45)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area100_SetFlag45 | area_w2d.cpp:596 | Area 100 handler 2. Sets flag 69 (0x45) and nothing else. |
| set | Area100_Trigger16 | area_w2d.cpp:722 | Object trigger 16 (area 100's block). Sets 69, `MoveCmd_TestFB(0x4A, 0x1E)`, sound 0x109, and sets the four gate cells to 0xA1. |
| clear | Scena09_Run15 | scena_sc9a.cpp:1497 | Chapter 9 run 15 step 0x46 (when `ObjTrio[1]` byte 0x137 is not 3): message 0x87, step 0x47; also sets chapter 6's flag 62 (row 6, `+0x30`). |
| test | Area100_InitCells | area_w2d.cpp:749 | Area 100 init (+0x40). Flag 69 clear: the four gate cells are set to 0x50. Set: they are left as the map has them. |

## Flag 70 (0x46)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area148_Tail31 | area_w3g.cpp:266 | Area 148, tail kind 31, state 1: when the script byte (`kScriptVar`) reaches 0x18, tail disarmed, flag 0x46 set, `Field_ChangeArea(0x95, ...)` to area 149. |
| set | Area167_Tail32 | area_w4a.cpp:1089 | Tail kind 32 state 1, once script variable 6 is 0x18: flag 0x46 set, then `Field_ChangeArea(149, ...)`. |
| set | Scena14_Run4 | scena_sc13.cpp:2114 | Chapter 14 run 4 step 0xE: set before `ChangeArea(0x95, ...)`. |
| clear | Area146_ClearFlag46 | area_w3f.cpp:1053 | Handler in area 146's block, used as area 148 `+0x3C[1]` and area 167 `+0x3C[1]`: flag 0x46 cleared and `Cond_ByteFE` = 0. |

## Flag 71 (0x47)

Saves: 20 22.

No site in our source names this flag.

## Flag 72 (0x48)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area167_Tail32 | area_w4a.cpp:1142 | Variable 5 = 1: `Field_ChangeArea(167, ..., 0x83)`, 0x48 set ... |
| set | Area167_Flags48By49 | area_w4a.cpp:994 | Area 167 handler 6, unless height is 0x3C0: `Cond_ByteFD` 1 sets 0x48 ... |
| clear | Area167_Flags48By49 | area_w4a.cpp:1000 | `Cond_ByteFD` 3 clears both 0x48 ... |
| clear | Area167_Tail32 | area_w4a.cpp:1138 | Tail kind 32 state 0x18, variable 6 0x54, variable 5 = 0: `Field_ChangeArea(167, ..., 5)`, 0x48 cleared ... |
| clear | Area167_Tail32 | area_w4a.cpp:1146 | Variable 5 = 2: `Field_ChangeArea(167, ..., 0x84)`, 0x48 cleared ... |
| clear | Area167_Flags48By49 | area_w4a.cpp:997 | `Cond_ByteFD` 2 clears 0x48 ... |
| test | Area167_HeightBy48To4A | area_w4a.cpp:958 | `Cond_ByteFD` 2, 0x4A clear: flag 0x48 clear chooses a height by flag 0x49. |

## Flag 73 (0x49)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area167_Tail32 | area_w4a.cpp:1147 | ... and 0x49 set. |
| set | Area167_Flags48By49 | area_w4a.cpp:998 | ... and sets 0x49. |
| clear | Area167_Flags48By49 | area_w4a.cpp:1001 | ... and 0x49. Other values change neither. |
| clear | Area167_Tail32 | area_w4a.cpp:1139 | ... and 0x49 cleared. |
| clear | Area167_Tail32 | area_w4a.cpp:1143 | ... and 0x49 cleared. |
| clear | Area167_Flags48By49 | area_w4a.cpp:995 | ... and clears 0x49. |
| test | Area167_HeightBy48To4A | area_w4a.cpp:959 | Same branch: 0x49 set gives height 0xFDC0, clear gives 0x9C0. |
| test | Area167_HeightBy48To4A | area_w4a.cpp:972 | `Cond_ByteFD` 1: flag 0x49 clear gives height 0x9C0; else 0x3C0. |

## Flag 74 (0x4A)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area167_Tail32 | area_w4a.cpp:1150 | Same state, every way: flag 0x4A set (consumed by `Area167_HeightBy48To4A`). |
| clear | Area167_HeightBy48To4A | area_w4a.cpp:952 | Same branch: 0x4A cleared once consumed (a one-shot set by `Area167_Tail32` case 9). |
| clear | Area167_HeightBy48To4A | area_w4a.cpp:967 | Same branch: 0x4A cleared. |
| test | Area167_HeightBy48To4A | area_w4a.cpp:951 | Area 167 handler 5, `Cond_ByteFD` 2: flag 0x4A set sends the script to +3 = 7 or 8 (by the entry zone), position 0xFFFE. |
| test | Area167_HeightBy48To4A | area_w4a.cpp:966 | `Cond_ByteFD` 1: flag 0x4A set sends the script to +3 = 7, position 0xFFFE. |

## Flag 75 (0x4B)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | Area167_HeightBy4B4C | area_w4a.cpp:902 | `Cond_ByteFD` 2, 0x4C clear: flag 0x4B clear puts the object's height at 0xFDC0. |
| test | Area167_HeightBy4B4C | area_w4a.cpp:915 | `Cond_ByteFD` 1: flag 0x4B set puts the height at 0x9C0; otherwise the height falls to 0x3C0. |
| test | Area167_Skip3Unless4B | area_w4a.cpp:940 | Area 167 handler 4: flag 0x4B clear skips 3 bytes of the movement script. |
| toggle | Area167_Tail32 | area_w4a.cpp:1112 | Tail kind 32 state 0xB, variable 6 0x74: `Field_ChangeArea(167, ...)` (entry by `Cond_ByteFD` 2 or not), then flag 0x4B inverted. |
| toggle | Area167_Toggle4BUnlessLow | area_w4a.cpp:934 | Area 167 handler 3: unless the object's height is 0x3C0, flag 0x4B is inverted. |

## Flag 76 (0x4C)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area167_Tail32 | area_w4a.cpp:1113 | Same state: flag 0x4C set (consumed by `Area167_HeightBy4B4C`). |
| clear | Area167_HeightBy4B4C | area_w4a.cpp:897 | Same branch: 0x4C cleared once consumed (a one-shot set by `Area167_Tail32` case 5). |
| clear | Area167_HeightBy4B4C | area_w4a.cpp:910 | Same branch: 0x4C cleared. |
| test | Area167_HeightBy4B4C | area_w4a.cpp:896 | Area 167 script handler 2, `Cond_ByteFD` 2: flag 0x4C set sends the movement script to +3 = 5, position 0xFFFE. |
| test | Area167_HeightBy4B4C | area_w4a.cpp:909 | `Cond_ByteFD` 1: flag 0x4C set sends the script to +3 = 4, position 0xFFFE. |

## Flag 77 (0x4D)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area139_FlagIfLeader89Is5 | area_w3e.cpp:321 | Area 139: when the leader's byte `+0x89` is 5, flag 0x4D set, `MoveCmd_TestFB(4, 0xD)`. |

## Flag 78 (0x4E)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area172_Tail35 | area_w4b.cpp:1050 | Area 172 tail kind 35 state 1, counter 3 at 0x14: flag 0x4E set, `Field_ChangeArea(172, ..., 0x83)`. |
| set | Area172_Tail35 | area_w4b.cpp:1060 | State 6 (from choice 0 when chapter row 14 flag 0x12 is set), counter 3 at 0x14: flag 0x4E set, change to area 172 entry 0x82. |
| set | Scena14_Run6 | scena_sc13.cpp:2401 | Chapter 14 run 6 step 1, counter 3 = 0x14: set before `ChangeArea(0xAC, ...)`. |
| clear | Area172_ClearFlag4E | area_w4b.cpp:836 | Area 172 handler 0 (choice 1): flag 0x4E cleared, nothing else. |

## Flag 79 (0x4F)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area105_Init | area_w2e.cpp:1358 | Area 105 init, only when the previous area is 0x57. Sets 79 when `Cond_ByteFD` is 0. |
| set | Scena07_EnterArea | scena_sc7.cpp:257 | Chapter 7 area entry: set on every entry to area 0x69, no other condition. |
| set | EnterArea10Body | scena_sc9b.cpp:412 | Chapter 10 area entry, area 0x69, `Cond_ByteFD` 0, chapter flag 0x35 clear. |
| clear | Area105_Init | area_w2e.cpp:1359 | Same entry: clears 79 when `Cond_ByteFD` is 1. |
| clear | Scena10_Run12 | scena_sc9b.cpp:1866 | Chapter 10 run 12 step 2, after the shake timer. Run 12 is started by `Scena10_ArriveHook` in area 0x69 (`:2366-2372`, `Cond_ByteFD` 0, chapter flag 0x35 clear, z cell above 0x1C); its step 5 sets chapter flag 0x35, so the entry stops setting 79 afterwards. |
| test | Area106_PlaceByFlags | area_w2e.cpp:1390 | With 81 set: 79 picks height 0x850 and script + 2, or height 0x600 and script + 4. |
| test | Area106_PlaceByFlags | area_w2e.cpp:1399 | With 81 clear: 79 set places at (0x128000, 0x238000) height 0x850; clear at (0x118000, 0x1F8000) height 0x300, script + 4. |
| test | Area106_PlaceByPair | area_w2e.cpp:1423 | Same function: 79 set gives height 0x850 and script + 8; clear gives 0x280 and script + 2k. |
| test | Area106_StepHook | area_w2e.cpp:1478 | Area 106 step hook (only when `Cond_ByteFD` is 0). Flag 79 set: the hook looks at the two cells that arm tail 36 with state 0 or 2, by flag 81. |
| test | FlagSet | effect_6b.cpp:763 | Kind 0x18 sub-kind 0x41's helper, called with 0x4F only. `EffectKind18Sub41_Turn` (794): at the end of the turn, sound `kSoundOpen` when set, `kSoundShut` when clear. `EffectKind18Sub41_Hold` (816): while set, two extra Capcom calls (0x5100B0 / 0x5101C0) run each frame. |
| toggle | EffectKind65_WaitInput | effect_3b.cpp:478 | Kind 0x65, state 2 (reached only when `Game_BattleCount` is a multiple of 5): a held button plays sound 0x202 and flips the flag (`Flags_Toggle`), then a screen shake and `Msg_OpenScript(1)`. Flag is `kKind65Flag` = 0x4F (`effect_3b_callees.h:44`). Op is a toggle, not a plain set or clear. |

## Flag 80 (0x50)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | EffectKind18Sub41_Hold | effect_6b.cpp:812 | Sub-kind 0x41, state 3: at frame +9 = 0x17 it sets the flag and calls `MoveCmd_TestFB(0x13, 0x25)`. |

## Flag 81 (0x51)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area106_Tail36 | area_w2e.cpp:1455 | Area 106 mode-tail kind 36, state 0: `Party_DropIn(0)` and flag 81 set. |
| clear | Area106_Tail36 | area_w2e.cpp:1456 | State 2: `Party_DropIn(1)` and flag 81 cleared. |
| test | Area106_PlaceByFlags | area_w2e.cpp:1387 | Area 106 handler 1. Flag 81 picks the running object's position: set (0x128000, 0xB8000), clear one of two other places by flag 79. |
| test | Area106_StepHook | area_w2e.cpp:1479 | With 79 set: 81 clear arms state 0 at z 0x248000, x 0x11..0x13; 81 set arms state 2 at z 0xA8000, x 0x12..0x13. |

## Flag 82 (0x52)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area106_Tail36 | area_w2e.cpp:1457 | State 0xA: `Party_DropIn(2)`, 82 set, 83 cleared. |
| set | Area106_Tail36 | area_w2e.cpp:1459 | State 0xE: `Party_DropIn(4)`, 82 and 83 set. |
| clear | Area106_Tail36 | area_w2e.cpp:1458 | State 0xC: `Party_DropIn(3)`, 82 cleared, 83 set. |
| clear | Area106_Tail36 | area_w2e.cpp:1460 | State 0x10: `Party_DropIn(5)`, 82 and 83 cleared. |
| test | Area106_PlaceByPair | area_w2e.cpp:1417 | Area 106 handler 2. k = flag 82 (bit 0) and flag 83 (bit 1) selects the cell pair k of `Area106_Cells`. |
| test | Area106_StepHook | area_w2e.cpp:1488 | With 79 clear: 82 set arms a state at z 0x258000, x 0x1A..0x1B. |

## Flag 83 (0x53)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area106_Tail36 | area_w2e.cpp:1458 | State 0xC: 83 set. |
| set | Area106_Tail36 | area_w2e.cpp:1459 | State 0xE: 83 set. |
| clear | Area106_Tail36 | area_w2e.cpp:1457 | State 0xA: 83 cleared. |
| clear | Area106_Tail36 | area_w2e.cpp:1460 | State 0x10: 83 cleared. |
| test | Area106_PlaceByPair | area_w2e.cpp:1418 | Same function: bit 1 of k. |
| test | Area106_StepHook | area_w2e.cpp:1490 | With 79 clear and 82 set: 83 set arms state 0x10, clear 0xC. |
| test | Area106_StepHook | area_w2e.cpp:1493 | With 79 and 82 clear: 83 set arms state 0xE at x 0x268000, z 0x24..0x25; clear arms 0xA at x 0x188000, z 0x15..0x16. |

## Flag 84 (0x54)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area140_FlagIfLeader89Is567 | area_w3e.cpp:364 | Area 140: when the leader's byte `+0x89` is 5, 6 or 7, flag 0x54 set, `MoveCmd_TestFB(2, 7)`. |
| test | EffectKind18Sub28_Start | effect_5f.cpp:357 | Kind 0x18 sub-kind 0x28 (a gate), state 0. Clear: state 5 (wait for the flag). Set: the gate works by the leader's distance (opens when near, closes when far). |
| test | EffectKind18Sub28_WaitFlag | effect_5f.cpp:403 | Sub-kind 0x28, state 5. Once set: sound 0x200, state 2 (opening). |

## Flag 85 (0x55)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area170_Tail37 | area_w4b.cpp:481 | Tail kind 37 state 1, through `ChangeIfCounter3` (count 0x24): flag 0x55, `Field_ChangeArea(170, ..., 0x82)`. |
| set | Area170_Tail37 | area_w4b.cpp:483 | State 6, count 0x24: flag 0x55, change to area 170 entry 0x83. |
| set | Area197_Tail51 | area_w4f.cpp:683 | Area 197, tail kind 51, state 1, counter 3 at 0x24: flag 0x55 set, `Field_ChangeArea(170, 0x60000, 0x3F0000, 0x82)`, state 0x1E. |
| clear | Area170_Tail37 | area_w4b.cpp:500 | State 30 (where every `ChangeIfCounter3` leads), counter 3 at 0: 0x55, 0x56, 0x57 all cleared, tail ended. |

## Flag 86 (0x56)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area170_Tail37 | area_w4b.cpp:485 | State 11, count 0x44: flag 0x56, change to area 170 entry 0x86. |
| set | Area170_Tail37 | area_w4b.cpp:487 | State 16, count 0x44: flag 0x56, change to area 170 entry 0x87. |
| clear | Area170_Tail37 | area_w4b.cpp:501 | Same state: 0x56 cleared. |

## Flag 87 (0x57)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area170_Tail37 | area_w4b.cpp:489 | State 21, count 0x64: flag 0x57, change to area 170 entry 0x8A. |
| set | Area170_Tail37 | area_w4b.cpp:491 | State 26, count 0x64: flag 0x57, change to area 170 entry 0x8B. |
| clear | Area170_Tail37 | area_w4b.cpp:495 | Tail kind 37 state 29, counter 3 at 0: `Field_ScriptFlags` \ | = 0x1010, flag 0x57 cleared, state 53. |
| clear | Area170_Tail37 | area_w4b.cpp:502 | Same state: 0x57 cleared. |

## Flag 88 (0x58)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area173_Tail38 | area_w4c.cpp:259 | Area 173, tail kind 38, state 1, counter 3 at 0x18: `Field_ChangeArea(186, 0x48000, ..., 0x80)` (z by `Cond_ByteFD` zone 2), then flag 0x58 set, tail disarmed. Tail 38 is armed by `Area173_ArriveHook` on two exact (x, z) spots. |
| set | Scena15_Run6 | scena_sc15.cpp:1520 | Chapter 15 run 6 step 0x29: chapter flag 21 (0x15) set, then 88 set, then `ChangeArea(0xAD, ..., 0)`, music 0xA1. |
| clear | Area173_Tail38 | area_w4c.cpp:279 | Same tail, state 12, counter 3 at 0: flag 0x58 cleared, tail disarmed. States 10..12 are the return leg the init starts. |
| test | Area173_Init | area_w4c.cpp:313 | Area 173's init: flag 0x58 set arms tail kind 38 at state 10 (the return leg); clear, nothing. |
| test | Area186_Init | area_w4d.cpp:626 | Area 186's init: with flag 0x58 set, entry zone 1 starts `Area186_Start(0xA)` and zone 2 `Area186_Start(0)` (tail kind 39, which leaves back to area 173 at entry 0x81 or 0x82). Clear: nothing. |

## Flag 89 (0x59)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area148_ChoiceSetFlag59 | area_w3g.cpp:234 | Area 148, choice 1: answer 0 gives message 5 and sets flag 0x59; other answers no message. |

## Flag 90 (0x5A)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area148_Tail31 | area_w3g.cpp:284 | Area 148, tail kind 31, state 0xC: when the script byte reaches 0x20, flag 0x5A set, timer 0x20, then a second kind-0x13 effect. |
| test | EffectKind18Sub39_Start | effect_5e.cpp:1213 | Kind 0x18 sub-kind 0x39, state 0. Set: drawn open (+0x30 0x80, state 3); clear: closed, waits. Flag is `kSub39Flag` = 0x5A (`effect_5e_callees.h:38`). |
| test | EffectKind18Sub39_WaitFlag | effect_5e.cpp:1228 | Sub-kind 0x39, state 1. Once set: sound 0x208 (unless `Field_Request`), opens. |

## Flag 91 (0x5B)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area104_Tail40 | area_w2e.cpp:1067 | Same state: the first pass sets 91, so script 2 runs once. |
| test | Area104_Tail40 | area_w2e.cpp:1066 | Area 104 mode-tail kind 40, state 2 (the init's path with key item 0xA). Flag 91 clear: set it and open message script 2. Set: message script 3. |

## Flag 92 (0x5C)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | EffectKind6F_Start | effect_3c.cpp:487 | Kind 0x6F, state 0. `Cond_ByteFD` 2 with the flag clear: state 1 (the line along z). |
| test | EffectKind6F_AlongZ | effect_3c.cpp:544 | Kind 0x6F, state 1: draws the line at x 0x5C0000 and pushes the party. Once set: sound 0x206, state 3 (line gone). |
| test | EffectKind6F_Watch | effect_3c.cpp:585 | Kind 0x6F, state 3. `Cond_ByteFD` 2 with the flag clear: sound 0x206, back to state 0. |

## Flag 93 (0x5D)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | EffectKind6F_Start | effect_3c.cpp:491 | Kind 0x6F, state 0. `Cond_ByteFD` 3 with the flag set: state 2 (the line along x). Else state 3. |
| test | EffectKind6F_AlongX | effect_3c.cpp:576 | Kind 0x6F, state 2: draws the line at z 0x440000 and pushes the party. Once clear: sound 0x206, state 3. |
| test | EffectKind6F_Watch | effect_3c.cpp:589 | Kind 0x6F, state 3. `Cond_ByteFD` 3 with the flag set: sound 0x206, back to state 0. |

## Flag 96 (0x60)

Saves: 20 22.

No site in our source names this flag.

## Flag 97 (0x61)

Saves: 20 22.

No site in our source names this flag.

## Flag 98 (0x62)

Saves: 20 22.

No site in our source names this flag.

## Flag 99 (0x63)

Saves: 20 22.

No site in our source names this flag.

## Flag 100 (0x64)

Saves: 20 22.

No site in our source names this flag.

## Flag 101 (0x65)

Saves: 20 22.

No site in our source names this flag.

## Flag 102 (0x66)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area140_Tail41 | area_w3e.cpp:447 | Area 140, tail kind 41, state 0: `Party_DropIn(0)`, `Field_ScriptFlags | 0x1270`, leader `+0x48` = 1, flag 0x66 set. Armed by choice answer 1 (`Area140_ChoiceArmTail41`). |
| clear | Area140_Tail41 | area_w3e.cpp:483 | Area 140, tail kind 41, state 7: `Field_ScriptFlags & 0xED8F`, leader `+0x48` = 0, flag 0x66 cleared, encounter roll reset, tail disarmed. |
| test | Area140_FollowAndAct | area_w3e.cpp:383 | Area 140 handler: flag 0x66 clear skips the handler (script position + 4); set, the running object follows the leader and the action button runs `Area140_CellHook`. |
| test | Area140_StepHook | area_w3e.cpp:509 | Area 140 step hook: zone 0, flag 0x66 set and the position x 0x558000, z high word 0xA..0xB: tail state 5 (which runs to state 7, the clear). |

## Flag 103 (0x67)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area170_Trigger20 | area_w4b.cpp:660 | Area 170 object trigger 20: flag 0x67 set, nothing else. |
| test | EffectKind18Sub3C_WaitFlag | effect_5f.cpp:1022 | Kind 0x18 sub-kind 0x3C, state 1. Once set: sound 0x200, rises, then `MoveCmd_TestFB(9, 0xF)` and releases. |

## Flag 104 (0x68)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area62_TailGiveItem59 | area_w1d.cpp:558 | Area 62, tail kind 0x2A, state 1: Inventory_Add(0, 0x59, 1) succeeds, then sound 0x106, Msg_OpenSystem(2) and the flag. A full inventory (Msg_OpenSystem(3)) does not set it. |

## Flag 105 (0x69)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| toggle | Area188_Tail43 | area_w4e.cpp:305 | Area 188, tail kind 43, state 0xA (reached when `KeyItem_Has(6)`): message 0x82, then flag 0x69 toggled; then `Party_DropIn(6)` and `Field_ChangeArea(188, ...)` at entry 0x87 or 0x88. Without key item 6 the tail shows message 0x83 and stops. |

## Flag 106 (0x6A)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| test | Area148_Tail31 | area_w3g.cpp:305 | Area 148, same state: flag 0x6A now set gives `Gfx_ClutAdjust(0, 0x380, -6, -4, -4)`; clear gives `Gfx_ClutAdjust(0, 0x180, 0, 0, 0)`. |
| test | Area148_DrawBeam | area_w3g.cpp:493 | Area 148 beam drawer: draws nothing while flag 0x6A is clear. |
| toggle | Area148_Tail31 | area_w3g.cpp:303 | Area 148, tail kind 31, state 0x14: flag 0x6A toggled, sound 0x204. |

## Flag 107 (0x6B)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area91_State2Grow | area_w2c.cpp:231 | Same function: the first pass sets flag 107, so later passes take `Party_DropIn(1)`. |
| test | Area91_State2Grow | area_w2c.cpp:230 | Area 91 object state 2, on its 0x1E-th frame. Flag 107 clear: set it and `Party_DropIn(0)`. Set: `Party_DropIn(1)`. |

## Flag 108 (0x6C)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | MasterGrant_Step1Member | rest_2d.cpp:474 | Same: when the add takes, flag 0x6C set, the member's name to the text, message 0xE. The flag makes this grant once only. |
| test | MasterGrant_Step1Member | rest_2d.cpp:473 | Master grant, step 1, master 0xC only: when the apprentice has gained 3 or more levels and flag 0x6C is clear, `Inventory_Add(3, 0x16, 1)`. |
| test | MasterWin_Available | rest_2h.cpp:556 | Whether master `index` is available: master 0xC answers flag 0x6C. Masters 0xB, 0xD, 0xE answer bits 3, 4, 5 of `0x904061` instead. |

## Flag 109 (0x6D)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area34_Trigger51 | area_w0c.cpp:567 | Area 34, Field_ObjectTriggers id 51: sets the flag, then tests 109..112. |
| test | Area34_Trigger51 | area_w0c.cpp:569 | Loop over flags 109..112; any clear returns before flag 113 is set. |
| test | Area46_Trigger54 | area_w1b.cpp:1527 | Loop over flags 109..112; any clear returns before flag 113 is set. |
| test | FlagsGather | area_w1d.cpp:87 | Loop over flags 109..112; any clear returns before flag 113 is set. |

## Flag 110 (0x6E)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | FlagsGather (Area56_Trigger52) | area_w1d.cpp:85 | Area 56, Field_ObjectTriggers id 52 (`area_w1d.cpp:357` passes 0x6E). Then tests 109..112. |
| test | Area34_Trigger51 | area_w0c.cpp:569 | As above. |
| test | Area46_Trigger54 | area_w1b.cpp:1527 | As above. |
| test | FlagsGather | area_w1d.cpp:87 | As above. |

## Flag 111 (0x6F)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | FlagsGather (Area57_Trigger53) | area_w1d.cpp:85 | Area 57, Field_ObjectTriggers id 53 (`area_w1d.cpp:386` passes 0x6F). Then tests 109..112. |
| test | Area34_Trigger51 | area_w0c.cpp:569 | As above. |
| test | Area46_Trigger54 | area_w1b.cpp:1527 | As above. |
| test | FlagsGather | area_w1d.cpp:87 | As above. |

## Flag 112 (0x70)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area46_Trigger54 | area_w1b.cpp:1525 | Field_ObjectTriggers id 54 (placed in area 46's block): sets the flag, then tests 109..112. |
| test | Area34_Trigger51 | area_w0c.cpp:569 | As above. |
| test | Area46_Trigger54 | area_w1b.cpp:1527 | As above. |
| test | FlagsGather | area_w1d.cpp:87 | As above. |

## Flag 113 (0x71)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area34_Trigger51 | area_w0c.cpp:570 | Set when flags 109..112 are all set. |
| set | Area46_Trigger54 | area_w1b.cpp:1528 | Set when flags 109..112 are all set. |
| set | FlagsGather | area_w1d.cpp:88 | Set when flags 109..112 are all set. |

## Flag 114 (0x72)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area37_TailLeave | area_w0c.cpp:689 | Area 37, tail kind 0x2E (armed by choice 21, answer 0), state 0: sets the flag and calls Field_ChangeArea(0x25, 0x2C8000, 0x270000, 1); message 0xB5 follows. |

## Flag 115 (0x73)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area171_Leader89Gate | area_w4b.cpp:715 | Area 171 handler 0: leader's +0x89 is 5 and byte `AreaW4b_GateKey` (0x903DB6) is 0x4B: flag 0x73 set, map cells (2..3, 0x21..0x22) rewritten (0xC0 / 0). |
| test | Area171_StepHook | area_w4b.cpp:793 | Area 171 step hook, `Cond_ByteFD` 2: flag 0x73 clear lets a step onto map byte 0x89 arm tail kind 48 at 20 (message 2); set disables it. |

## Flag 116 (0x74)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | HealStates | area_w4b.cpp:240 | Shared tail states 1..3 of areas 169 (tail kind 47) and 171 (tail kind 48): after sound 0x202, `Party_HealJoined`, message 1 closed, flag 0x74 set and the tail ends. |
| clear | Area169_InitClearFlag74 | area_w4b.cpp:399 | Area 169 init: `Cond_ByteFD` not 1 clears 0x74. |
| clear | Area171_InitClearFlag74 | area_w4b.cpp:739 | Area 171 init: `Cond_ByteFD` not 3 clears 0x74. |
| test | Area169_ArriveHook | area_w4b.cpp:427 | Area 169 arrive hook: `Cond_ByteFD` 1 and 0x74 clear arms the heal tail (kind 47); set means no heal. |
| test | Area171_ArriveHook | area_w4b.cpp:813 | Area 171 arrive hook: `Cond_ByteFD` 3 and 0x74 clear arms the heal tail (kind 48). |

## Flag 117 (0x75)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area189_StepArrive | area_w4e.cpp:705 | Area 189 walk: when the step count reaches the pace word, the flag of the block the leader stands on is set, from the local table `kMarks` (`:702`): block (0xB00, 0xF00) sets 0x75. Then a change to area 125 at (0x190000, 0x190000), flags 4. |
| clear | Area152_Init | area_w4a.cpp:103 | Area 152's init clears six flags in order: 0x75, 0x76, 0x8B..0x8E. Unconditional. |
| clear | Area189_LeaderStart | area_w4e.cpp:433 | Area 189, leader state 0 (the walk set up): after the pace is chosen, six flags cleared: 0x75. Unconditional. |

## Flag 118 (0x76)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area189_StepArrive | area_w4e.cpp:705 | Same table, block (0x1500, 0x1100) sets 0x76. |
| clear | Area152_Init | area_w4a.cpp:104 | Same init, second of the six clears. |
| clear | Area189_LeaderStart | area_w4e.cpp:434 | Same, flag 0x76. |

## Flag 119 (0x77)

Saves: 9 18..19.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | `Rest_Begin` | `field_s.cpp:245` | Otherwise set, unless the chapter is `0xE` and its row's flag 5 is set. Then `Transition_Start(0)`. |
| clear | `Area_ReturnGate` | `event_ops.cpp:1395` | Cleared, then `Field_ChangeArea` to the return point `AreaW2d_ReturnPoint`. |
| clear | `Rest_PlaceParty` | `field_s.cpp:285` | Cleared when the area is `0x85` (place kind 1). |
| clear | `GameMode3_Leave` | `mode_rest.cpp:187` | `at::kTripFlag` (`mode_rest_callees.h:45`) is `0x77`. Cleared on the branch that clears `AreaW3c_CampFlag` and changes area to the kept return point. |
| clear | Area131_TailLeave34 | area_w3c.cpp:503 | Area 131, tail kind 34, state 3: `Field_ScriptFlags ^ 0x16`, tail disarmed, the byte `0x904152` (`AreaW3c_CampFlag`) = 0, flag 0x77 cleared, `Field_ChangeArea(0x79, ...)` to area 121. Tail 34 is armed by object trigger 15. |
| clear | Area187_TailMessage2 | area_w4d.cpp:708 | Area 187, tail kind 29 (armed by object trigger 14), state 2 after message 2 closes: the byte `0x904152` (`AreaW3c_CampFlag`) = 0, flag 0x77 cleared, change to the saved return point with flags 4. |
| clear | Area191_Tail53 | area_w4e.cpp:982 | Same tail, state 0xA: change back to the saved return area with flags 4, `0x904152` = 0, flag 0x77 cleared. |
| clear | Area191_Tail53 | area_w4e.cpp:998 | Same tail, state 0x1E: `Field_ChangeArea(150, 0x180000, 0x300000, 1)`, `0x904152` = 0, flag 0x77 cleared. |
| clear | Area192_Tail54 | area_w4f.cpp:273 | Same tail, state 0xA: change to the saved return point with flags 4, `0x904152` = 0, flag 0x77 cleared. |
| clear | Area192_Tail54 | area_w4f.cpp:288 | Same tail, state 0x1E: `Field_ChangeArea(150, 0x180000, 0x300000, 1)`, `0x904152` = 0, flag 0x77 cleared. |
| clear | Area193_Tail57 | area_w4f.cpp:517 | Area 193, tail kind 57 (armed by `Area193_StepHook` at state 0xA, message 0x1C), state 0xC: flag 0x77 cleared, `Field_ChangeArea(189, 0x12000000, 0x17FF0000, 2)`, walk bytes `0x90405C..0x90405F` reset. |
| clear | Area193_Tail57 | area_w4f.cpp:534 | Same tail, state 0xE: flag 0x77 cleared, `Field_ChangeArea(151, 0x160000, 0x150000, 3)`. |
| clear | Scena14_Run1 | scena_sc13.cpp:1703 | Chapter 14 run 1 step 0x14 (not 0x19): cleared, byte 0x904152 zeroed, `ChangeArea(0xC1, ..., 0x83)`. |
| clear | Scena15_Run1 | scena_sc15.cpp:571 | Chapter 15 run 1 step 0x14 (not 0x19): cleared, `AreaW3c_CampFlag` zeroed, `ChangeArea(0xC1, ..., 0x83)`. Same shape as chapter 14's run 1. |
| test | `Area_ReturnGate` | `event_ops.cpp:1393` | A step onto a `0xA1` cell (input flags `0x40`, `Cond_ByteFD` 1, not area `0xBB`) does nothing unless the flag is set. |
| test | `Field_PartySetUp` | `field_event.cpp:555` | With input flag `0x40`, outside areas `0xBD` and `0xBB`, a set flag places the whole party, not the leader alone. |
| test | `Rest_Begin` | `field_s.cpp:238` | When set: state 6 and script message `0xEF`, nothing more. |
| test | `PartyForm_Reload` | `field_s.cpp:548` | With `AreaW3c_CampFlag` set: a clear flag, or area `0xBB`, outside area `0x85`, reloads the leader alone. |
| test | Area191_Init | area_w4e.cpp:1080 | Area 191's init: flag 0x77 set returns before the six objects are placed. Clear: an object list chosen by chapter row 14 bit 0xA, the level byte and the walk count. |
| test | Area191_Kind18Flag77 | area_w4e.cpp:1100 | Effect kind 18 state 77: flag 0x77 set resets the walk word, the step counters, adds 1 to the walk count and sets `Field_StatusBits` bit 0; clear, nothing. Then `Effect_Release`. |
| test | Area192_Init | area_w4f.cpp:373 | Area 192's init: flag 0x77 set returns before the six place-ops run. Twin of `Area191_Init`. |
| test | Area192_Effect18Release77 | area_w4f.cpp:426 | Effect kind 18 state 78: flag 0x77 set resets the walk word to 0x1E0 and clears `Field_StatusBits` bit 0; then `Effect_Release`. |

## Flag 121 (0x79)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area170_Trigger56 | area_w4b.cpp:674 | Item added: flag 0x79 set, message 0x22, sound 0x106. Not added: system message 3, flag stays clear. A one-time item pick-up. |
| test | Area170_Trigger56 | area_w4b.cpp:672 | Area 170 object trigger 56: flag 0x79 clear tries `Inventory_Add(1, 0x4B, 1)`; set gives system message 1. |

## Flag 122 (0x7A)

Saves: 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area141_Tail52 | area_w3e.cpp:847 | Area 141, tail kind 52, state 0xE (after message 4 and `Kind2_Place(2)`, counter 0 at 1): flag 0x7A set, runs 2 and 3 off. |
| test | Area141_InitCells | area_w3e.cpp:755 | Area 141 init, zone 1: runs 2 and 3 painted on while flag 0x7A is clear, off while set (test in `PaintPairByFlag`, area_w3e.cpp:740). |
| test | Scena14_ArriveHook | scena_sc13.cpp:2889 | Chapter 14 arrive hook, area 0x8D, `Cond_ByteFD` 1: only with 122 set, run 7 starts at step 3 (chapter flag 25 clear) or step 4 (25 set, 26 clear), each in a position box. |

## Flag 123 (0x7B)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area141_Tail52 | area_w3e.cpp:870 | Area 141, tail kind 52, state 0x1E (`FlagRunsOff`, area_w3e.cpp:770): flag 0x7B set, runs 4 and 5 off, a 0x3C-frame wait. |
| test | Area141_InitCells | area_w3e.cpp:756 | Area 141 init, zone 1: runs 4 and 5 by flag 0x7B, as above. |
| test | Area141_Tail52 | area_w3e.cpp:870 | Area 141, tail kind 52, state 0x1E (`FlagRunsOff`, area_w3e.cpp:766): flag 0x7B already set ends the tail (state 0x14). |

## Flag 124 (0x7C)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area141_Tail52 | area_w3e.cpp:874 | Area 141, tail kind 52, state 0x28 (`FlagRunsOff`, area_w3e.cpp:770): flag 0x7C set, runs 6 and 7 off. |
| test | Area141_InitCells | area_w3e.cpp:757 | Area 141 init, zone 1: runs 6 and 7 by flag 0x7C, as above. |
| test | Area141_Tail52 | area_w3e.cpp:874 | Area 141, tail kind 52, state 0x28 (`FlagRunsOff`, area_w3e.cpp:766): flag 0x7C already set ends the tail. |

## Flag 125 (0x7D)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area141_ChoiceFlag7D | area_w3e.cpp:724 | Area 141, choice 0: flag 0x7D set for every answer; answer 0 then arms tail 52 at state 0x32 (message 5), else message 6. |

## Flag 126 (0x7E)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area170_StepHook | area_w4b.cpp:633 | Area 170 step hook, `Cond_ByteFD` 5, step on the second set of edges: tail kind 37 armed at 20 (0x14), flag 0x7E set, `Field_ScriptFlags` &= 0xEFEF. |
| clear | Area170_Trigger19 | area_w4b.cpp:654 | Area 170 object trigger 19: tail kind 37 armed at 40, flag 0x7E cleared. |
| test | Area170_StepHook | area_w4b.cpp:637 | Same hook: flag 0x7E set stops the two exact-position checks that arm state 50 (pose 7) or write state 60 (pose 3). |
| test | EffectKind7D_SetMap | effect_3d.cpp:686 | Kind 0x7D's map writer (also called by `Area170_Init`). Clear: each of the 7 x 9 cells is on or off by the three dial grids; set: every cell is off (height 0). Flag is `kDialsSolvedFlag` = 0x7E (`effect_3d_callees.h:58`). |

## Flag 127 (0x7F)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area43_Trigger58 | area_w1b.cpp:499 | Field_ObjectTriggers id 58 (placed in area 43's block): sets the flag, MoveCmd_TestFB(0x44, 0xB), sound 0x103. |

## Flag 128 (0x80)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | EffectKind18Sub5A_Set | effect_6c.cpp:1483 | Sub-kind 0x5A, state 3, after the sweep: sets the flag and writes cells (0x11..0x12, 0x61). So the flag records that the sweep is done. |
| test | EffectKind18Sub5A_Start | effect_6c.cpp:1459 | Kind 0x18 sub-kind 0x5A, state 0. Set: state 5 (the second cycle); clear: waits for `Cond_ByteFE` 2 and sweeps the map rows. |

## Flag 129 (0x81)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | EffectKind18Sub5B_Lift | effect_6c.cpp:1652 | Sub-kind 0x5B, state 5: after the three parts slide and lift, sets the flag, `MoveCmd_TestFB(0x30, 0x71)`, releases. |
| clear | EffectKind18Sub5B_Reopen | effect_6c.cpp:1661 | Sub-kind 0x5B, state 6: once `Cond_ByteFE` is 2, clears the flag, sound, parts drawn lifted. |
| test | EffectKind18Sub5B_Start | effect_6c.cpp:1578 | Kind 0x18 sub-kind 0x5B, state 0. Set: state 6 ("opened already"); clear: `Cond_ByteFE` 3, waits. |
| test | Scena15_StepHook | scena_sc15.cpp:1713 | Chapter 15 step hook, area 0xAD, `Cond_ByteFD` 2: only while 129 is clear, x cell above 0x2C and z cell 0x78..0x7F start run 3 at step 0. |
| test | Scena15_Run3 | scena_sc15.cpp:665 | Chapter 15 run 3 step 4: waits (returns each frame) until 129 is set, then places effect 0x13 and goes to step 5. |

## Flag 130 (0x82)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area191_Tail53 | area_w4e.cpp:965 | Area 191, tail kind 53 (armed by `Area191_StepHook` on a map cell of byte 0xA6, or by a choice at state 0x1E), state 4 once the sound stream ends: flag 0x82 set, the walk word `0x90405C` = 0, the step counters cleared, the walk count `0x90405E` + 1. Message 0x6D opens the tail. |
| set | Area192_Tail54 | area_w4f.cpp:255 | Area 192, tail kind 54 (armed by `Area192_StepHook` on a 0xA6 cell, or answer 1 of choice 5 at state 0x1E), state 4 once the stream ends: flag 0x82 set, the walk word = 0x1E0, step counters cleared, `Field_StatusBits` bit 0 cleared. Twin of `Area191_Tail53`. |
| test | Area189_Init | area_w4e.cpp:347 | Area 189's init: flag 0x82 set makes `Draw_PassFlags` = 0x1F. Nothing else. |

## Flag 131 (0x83)

Saves: 20 22.

No site in our source names this flag.

## Flag 132 (0x84)

Saves: 20 22.

No site in our source names this flag.

## Flag 133 (0x85)

Saves: 20 22.

No site in our source names this flag.

## Flag 136 (0x88)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area128_TailLeave56 | area_w3c.cpp:254 | Area 128, tail kind 56, state 0: `Party_DropIn(6)`, then flag 0x88 set and the tail cleared. Tail 56 state 0 is armed by object trigger 61 (`Area128_Trigger61`). |

## Flag 137 (0x89)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area128_TailLeave56 | area_w3c.cpp:258 | Area 128, tail kind 56, state 0xA: flag 0x89 set, then `Field_ChangeArea(0x79, ...)` to area 121. State 0xA is armed by choice answer 0 (`Area128_ChoiceArmTail56`, message 0x49). |

## Flag 138 (0x8A)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Scena14_Run1 | scena_sc13.cpp:1712 | Chapter 14 run 1 steps 0x14 and 0x19, both paths, after the area change into 0xC1. Step 0x19 also sets chapter flags 2 and 3 and uses entrance 0x84. |
| set | Scena15_Run1 | scena_sc15.cpp:580 | Chapter 15 run 1 steps 0x14 and 0x19, both paths. Step 0x19 also sets row 14's flags 2 and 3 (`+0x70`, lines 575-576) and uses entrance 0x84. |
| clear | Area150_Tail58 | area_w3g.cpp:899 | Area 150, tail kind 58, state 0x16 (after `Party_HealJoined` and a sound stream): `Music_Play(0x95, 0x10)`, chapter-14 row flag 3 cleared and event flag 0x8A cleared, tail disarmed. |
| clear | Area193_Tail57 | area_w4f.cpp:564 | Same tail, state 0x16 (after message 0x25 or 0x20 by chapter row 14 flag 3, `Party_HealJoined` and a stream): `Music_Play(0x95, 0x10)`, `Draw_PassFlags` 0x1F, chapter row 14 flag 3 cleared and flag 0x8A cleared, tail disarmed. |
| test | Area193_Init | area_w4f.cpp:586 | Area 193's init: flag 0x8A set starts tail kind 57 at state 0x14 with `Draw_PassFlags` 0; clear sets the pending-area byte `0x937F98` to 1. |

## Flag 139 (0x8B)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area189_StepArrive | area_w4e.cpp:705 | Same table, block (0xC00, 0x1400) sets 0x8B. |
| clear | Area152_Init | area_w4a.cpp:105 | Same init (flag 0x8B). |
| clear | Area189_LeaderStart | area_w4e.cpp:435 | Same, flag 0x8B. |

## Flag 140 (0x8C)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area189_StepArrive | area_w4e.cpp:705 | Same table, block (0x1000, 0x1100) sets 0x8C. |
| clear | Area152_Init | area_w4a.cpp:106 | Same init (flag 0x8C). |
| clear | Area189_LeaderStart | area_w4e.cpp:436 | Same, flag 0x8C. |

## Flag 141 (0x8D)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area189_StepArrive | area_w4e.cpp:705 | Same table, block (0x1D00, 0x1700) sets 0x8D. |
| clear | Area152_Init | area_w4a.cpp:107 | Same init (flag 0x8D). |
| clear | Area189_LeaderStart | area_w4e.cpp:437 | Same, flag 0x8D. |

## Flag 142 (0x8E)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area189_StepArrive | area_w4e.cpp:705 | Same table, block (0x1400, 0x1400) sets 0x8E. |
| clear | Area152_Init | area_w4a.cpp:108 | Same init (flag 0x8E). |
| clear | Area189_LeaderStart | area_w4e.cpp:438 | Same, flag 0x8E. |

## Flag 143 (0x8F)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area168_ChoiceFlag8FArm59 | area_w4b.cpp:337 | Area 168 choice 5, answer 0: flag 0x8F set, tail kind 59 armed at state 0 (which then changes to area 167, entry 0x89). |
| test | Area167_Init | area_w4a.cpp:1216 | Area 167 init, `Cond_ByteFD` 0: needs flag 0x8F set ... |
| test | EffectKind18Sub49_WaitCue | effect_5f.cpp:770 | Kind 0x18 sub-kind 0x49 (a gate), state 1. Set, with the leader near the gate: a step on (open). `Cond_ByteFE` 0x20 is the other trigger. |

## Flag 144 (0x90)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area167_Tail32 | area_w4a.cpp:1167 | Tail kind 32 state 0x20, countdown done: flag 0x90 set, `Field_ChangeArea(168, ...)`, `Field_ScriptFlags2` bit 6 set. Reached from state 0x1E, which `Area167_Init` arms. |
| test | Area167_Init | area_w4a.cpp:1217 | ... and 0x90 clear; then camera distance 0x980 and tail kind 32 armed at state 0x1E (which ends by setting 0x90). |

## Flag 145 (0x91)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area168_Tail59 | area_w4b.cpp:378 | Area 168 tail kind 59 state 11, once counter 0 is 1: sound 0x204, flag 0x91 set. Reached from choice 6 (state 10, `Kind2_Place(0)`). |
| test | EffectKind8A_Lines | effect_4b.cpp:1076 | Kind 0x8A, state 1: draws three lines at x 0x100000. Once set: state 2, which zeroes six map cells and releases the effect. |

## Flag 146 (0x92)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Scena17_EndReturn | scena_sc15.cpp:2684 | Chapter 17 end step 0, the return after the ending: 146 set, then row 15's flags 9..22 cleared (`+0x78`, line 2685), area 0x8F, chapter byte 15. |
| test | Save_BuildBlock | rest_2c.cpp:402 | Building the save block: `Flags_Test(event bank, 0x92)`'s al is stored at `0x904694`, a byte of the slot summary (`0x904680`). |
| test | CommuPose_KindB | rest_4a.cpp:1090 | A building's resident pose (kind B): for the second resident (count 1), flag 0x92 set gives pose 0xC00B, clear gives 0xD4. |

## Flag 147 (0x93)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area100_ChoiceFlags93 | area_w2d.cpp:643 | Answer 3 with key item 6 held: sets 147, 148 and 51. |
| set | Area100_ChoiceFlags93 | area_w2d.cpp:653 | Any answer other than 1..4: sets 147, clears 148, sets 51. Answer 4 changes no flag. |
| clear | Area100_ChoiceFlags93 | area_w2d.cpp:632 | Area 100 choice (handler 8), answer 1: clears 147, sets 148 and 51. The message comes from `Area100_ChoiceMessages[answer]`. |
| clear | Area100_ChoiceFlags93 | area_w2d.cpp:637 | Answer 2: clears 147 and 148, sets 51. |
| test | Area100_ChangeAreaByFlags | area_w2d.cpp:586 | Same function: the second selector bit (see row above). |

## Flag 148 (0x94)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area100_ChoiceFlags93 | area_w2d.cpp:633 | Answer 1: sets 148. |
| set | Area100_ChoiceFlags93 | area_w2d.cpp:644 | Answer 3 with key item 6: sets 148. |
| clear | Area100_ChoiceFlags93 | area_w2d.cpp:638 | Answer 2: clears 148. |
| clear | Area100_ChoiceFlags93 | area_w2d.cpp:654 | Default answer: clears 148. |
| test | Area100_ChangeAreaByFlags | area_w2d.cpp:585 | Area 100 handler 0. Flags 148 and 147 choose the `Field_ChangeArea` target: neither 0x3B, 148 only 0x24, 147 only 0x74, both 0x70. |

## Flag 149 (0x95)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area175_Trigger64 | area_w4d.cpp:594 | Area 175, object trigger id 64: the object's +1 = 4, +0x84 = 2, +0x83 = 0xD, flag 0x95 set, `Party_DropIn(4)`, counter 0 cleared. Unconditional. |

## Flag 150 (0x96)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area113_Reward | area_w2f.cpp:1338 | Rank `0xA`, flag 150 clear: `Inventory_Add(3, 0x2D, 3)`, flag 150, message 2, answer 5. |
| set | Area113_Reward | area_w2f.cpp:1353 | Rank `0xB`, flag 150 clear: `Inventory_Add(3, 0x2D, 3)`, flag 150, message 3. |
| set | Area113_Reward | area_w2f.cpp:1370 | Rank `0xC`, flag 150 clear: `Inventory_Add(3, 0x2D, 3)`, flag 150, message 4. |
| test | Area113_Reward | area_w2f.cpp:1333 | Area 113 tail kind `0x3E` reward, byte `0x9045F4` (`FieldE1_Rank`) = `0xA`: flag 150 set gives message 5, answer 1 (nothing given). |
| test | Area113_Reward | area_w2f.cpp:1351 | Rank `0xB`: then flag 150 set ends there (message 6). |
| test | Area113_Reward | area_w2f.cpp:1368 | Rank `0xC`: flag 150 set ends with message 7. |

## Flag 151 (0x97)

Saves: none.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area113_Reward | area_w2f.cpp:1348 | Rank `0xB`, flag 151 clear: `Inventory_Add(3, 0x33, 1)`, flag 151, message 6, answer 5. |
| set | Area113_Reward | area_w2f.cpp:1366 | Rank `0xC`, flag 151 clear: `Inventory_Add(3, 0x33, 1)`, flag 151, message 7. |
| test | Area113_Reward | area_w2f.cpp:1343 | Rank `0xB`: flag 151 set gives message 5, answer 1. |
| test | Area113_Reward | area_w2f.cpp:1364 | Rank `0xC`, after `KeyItem_Add(0xF)` (only if key item `0xF` is not held): flag 151 set ends with message 8, answer `0xA`. |

## Flag 152 (0x98)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area130_TailGiveItem | area_w3c.cpp:394 | Area 130, tail kind 63, step 1: after an item from a six-entry pick is added (step 0, message 0x49), sound 0x106 and flag 0x98 set, then the tail disarmed. |

## Flag 245 (0xF5)

Saves: 20 22.

| Op | Function | File:line | Context |
|---|---|---|---|
| set | `Field_AfterBattleTally` | `field_e2.cpp:239` | Set when clear and `Rest4a_LevelLit` is 4 or more. |
| test | `Field_AfterBattleTally` | `field_e2.cpp:238` | Tested after each return from battle. |

## Flag 246 (0xF6)

Saves: 20 22.

No site in our source names this flag.

## Flag 247 (0xF7)

Saves: 20 22.

No site in our source names this flag.

## Flag 248 (0xF8)

Saves: 20 22.

No site in our source names this flag.

## Flag 249 (0xF9)

Saves: 20 22.

No site in our source names this flag.

## Flag 250 (0xFA)

Saves: 20 22.

No site in our source names this flag.

## Flag 251 (0xFB)

Saves: 20 22.

No site in our source names this flag.

## Flag 252 (0xFC)

Saves: 20 22.

No site in our source names this flag.

## Flag 253 (0xFD)

Saves: 20 22.

No site in our source names this flag.

## Flag 254 (0xFE)

Saves: 20 22.

No site in our source names this flag.

## Flag 255 (0xFF)

Saves: 20 22.

No site in our source names this flag.

## Sites whose flag number comes from the game data

| Op | Function | File:line | Context |
|---|---|---|---|
| set | Area41_TriggerFlag | area_w1a.cpp:724 | Area 41, Field_ObjectTriggers ids 59 and 60: the flag is the byte at 0x5F5F2D + the object's +0x86 (Area41_TriggerFlags 0x5F5F68 per `area_w1a_callees.h:104`); sound 0x201. |
| clear | Area42_ClearMemberFlag | area_w1b.cpp:135 | Area 42, handler 1: clears flag (Field_ActiveMember byte +0xA0) + 0x10. Probably one of 18..21; reading, not checked. |
| toggle | Area44_SwitchHook | area_w1b.cpp:625 | Area 44 cell hook: toggles the flag in byte 3 of the matching Area44_Switches record (5 bytes each), then arms the record's tail kind (8 or 9). Probably 22 or 23; reading, not checked. |
| test | Area49_EffectFrame | area_w1c.cpp:462 | Area 49, effect kind 0x70: a member entering one of the seven Area49_Zones gets the zone's direction (byte 4), turned by 4 when the flag in the zone's byte 5 is set. |
| test | Area49_EffectFrame | area_w1c.cpp:471 | Same flag (zone byte 5) for a member already moving in the zone. |
| toggle | Area49_CellHook | area_w1c.cpp:525 | Area 49: toggles the flag in byte 3 of the matching Area49_CellSwitches record (nine records, 4 bytes each); sound 0x202. These are the flags Area49_EffectFrame reads; reading, not checked. |
| toggle | Area52_CellHook | area_w1c.cpp:895 | Area 52 cell hook: unless a member stands in Area52_Rects, toggles the flag in byte 3 of the matching Area52_CellSwitches record (four records, 5 bytes each), and arms tail kind 15 at the record's state (byte 4). Probably 36..38; reading, not checked. |
| set | Area77_CellHook | area_w2a.cpp:156 | Area 77 cell hook: `FlagsToggle` (0x57C160) on the event bank. The flag number is byte 3 of the matched 4-byte record of the table `Area77_CellSwitches`. A toggle: sets the flag when it is clear. Then sound effect 0x201 and an effect of kind 4 (argument 0xF). |
| clear | Area77_CellHook | area_w2a.cpp:156 | Same toggle as the row above: clears the flag when it is set. |
| set | Area86_SwitchHook | area_w2b.cpp:272 | Area 86: `kFlagsToggle` on the event bank. The flag number is byte 3 of the matched record of `Area86_Switches`. Sets it when clear. Then sound effect 0x200 and an effect of kind 4 (0xF); the comment at line 257 says that effect sets flag 0x1C (28). |
| clear | Area86_SwitchHook | area_w2b.cpp:272 | Same toggle as the row above: clears the flag when it is set. |
| toggle | Area108_CellHook | area_w2f.cpp:438 | The flag is byte 3 of the matched `Area108_HookCells` entry (data). Then sound `0x206`. chapter-08.md reads these as 24..27 (reading, not checked). |
| test | Area112_CellHook | area_w2f.cpp:1135 | The flag is byte 3 of the matched `Area112_HookCells` entry (data). Set: the hook answers 0 (each cell works once). |
| clear | Area112_CellHook | area_w2f.cpp:1147 | For each set bit 0..3 of the entry's byte 2: flag `Area112_HookFlags[bit & 3]` (data) cleared. |
| set | Area112_CellHook | area_w2f.cpp:1149 | For each set bit 4..7 of the entry's byte 2: flag `Area112_HookFlags[bit & 3]` (data) set. |
| set | Area112_CellHook | area_w2f.cpp:1151 | The entry's own flag (byte 3) set, then sound `0x200`. Skipped when `Area112_MemberNearBoxes` is true (tail kind `0xA`, state `0xA`, sub-kind `0x15` instead). |
| test | MembersFrame | area_w3a.cpp:957 | Areas 117 / 118 member frame (`Area117_MembersFrame` / `Area118_MembersFrame`), a member entering a rectangle: the flag is byte 5 of the rectangle record (`t.rects + 4 + found * 6 + 1`, data). Set: the member's facing is reversed (`^ 4`). |
| test | MembersFrame | area_w3a.cpp:967 | Same flag, a member already marked inside the rectangle: the facing reversed while set; a changed facing negates the step and sets `+9` = 8 - `+9`. |
| toggle | SwitchHook | area_w3a.cpp:1010 | The flag is byte 3 of the matched switch record (`Area117_Switches` `0x62160C` / `Area118_Switches` `0x621E3C`, data). docs/area_w3a.md:154 gives it as `0x85` (133) for both areas. |
| toggle | Area139_CellHook | area_w3e.cpp:332 | Area 139 cell hook: the matched entry of `Area139_CellEntries` gives the flag number in its byte `+3`; that flag toggled, sound 0x204. |
| test | Area140_CellHook | area_w3e.cpp:531 | Area 140 cell hook: the flag number is byte `+3` of the matched `Area140_CellEntries` entry; already set, the hook does nothing. |
| set | Area140_CellHook | area_w3e.cpp:549 | Area 140 cell hook: unless a party member stands in the entry's rectangle (then tail 41 state 0xA, message 1), the entry's flag set, `MoveCmd_TestFB(+0, +1)`, and with `+4` an effect of kind 0x71 that holds the flag number in its `+0xB`. |
| clear | Area140_Effect71End | area_w3e.cpp:600 | Area 140, effect kind 0x71, state 2 (after 0xD2 frames or `Field_Request` 5): the flag in the effect's byte `+0xB` (copied from `Area140_CellEntries +3`) cleared, `MoveCmd_TestFB` at its cell. |
| test | Area109_SwitchPattern | rest_3g.cpp:598 | Area 109 switch: tests three flags whose numbers are the bytes of the .data table `0x65DE60` (`at::kPatternFlags`), as bits 0..2; answers 1..8. |
| clear | Area109_SwitchHook | rest_3g.cpp:615 | Same hook, flag 28 clear: the pattern mod 6 is written back as the three `0x65DE60` flags, each cleared first. |
| set | Area109_SwitchHook | rest_3g.cpp:616 | Same loop: each `0x65DE60` flag whose bit of (pattern mod 6) is 1 is set. Then `Effect_HoldFlag1C(0xF)` and sounds 0x206, 0x202. |
| toggle | Area167_CellHook | area_w4a.cpp:1205 | Area 167 cell hook: when the leader's cell and facing match the one record of `Area167_Switch` (0x63C594), the flag number is that record's byte +3, read from the image's data; the flag is inverted and tail kind 32 armed at state 5. |
| set | ChangeIfCounter3 | area_w4b.cpp:270 | Area 170 tail kind 37 helper: when counter 3 equals the given count, sets the flag passed in, then `Field_ChangeArea(170, ...)`. Its six callers pass 0x55, 0x56 or 0x57 (rows below). |
| test | DoorFlag | effect_5a.cpp:168 | Kind 0x18 sub-kind 7 (door): tests `Cond_Flags + 8 * row`, row from byte table 0x65DDB4[k] (`kFlagRows7`), bit from 0x65DDB8[k] (`kFlagBits7`), k the door +0xB (0..2). It is the event bank only if the row byte is 0x14..0x18. Callers `EffectKind18_07_Start` (694: set opens at once) and `_Wait` (712: once set, the door slides open). |
| test | EffectKind18_06_Wait | effect_5a.cpp:584 | Kind 0x18 sub-kind 6, state 1. Flag is +0x34, written in state 0 (`EffectKind18_06_Start`, 573) from byte table 0x65DB84 (`kFlags6`) by the set index +0x36 (0..2). Once set: sounds 0x202 and 0x201, state 2 (turn). |
| clear | EffectKind18_06_Turn | effect_5a.cpp:603 | Sub-kind 6, state 2: after 0x10 frames the level +0x3A steps to (+0x3A + 1) & 3, the flag +0x34 is cleared and it waits again. The flag is a one-shot "turn once" request. |
| test | EffectKind18Sub0C_Start | effect_5b.cpp:581 | Kind 0x18 sub-kind 0x0C, state 0. Flag from byte table 0x65DFC4 (`kSub0CFlags`, 8 entries) by the column +0x36 (`Sub0CFlag`, line 566). Set: frame 3, state 3; clear: frame 0, state 1. |
| test | EffectKind18Sub0C_Wait | effect_5b.cpp:592 | Sub-kind 0x0C, state 1. Same flag; once set: sound 0x201, opens. |
| test | EffectKind18Sub0C_Idle | effect_5b.cpp:609 | Sub-kind 0x0C, state 3. Same flag; once clear: closes. |
| test | Sub22Flag | effect_5d.cpp:1372 | Kind 0x18 sub-kind 0x22. Flag is +0x34, written in state 0 from byte table 0x65E348 (`kSub22Flags`, 4 places) by place +0x36. Callers: `EffectKind18Sub22_Start` (1391: set starts 0x80 lower), `_WaitSet` (1408: once set, lowers), `_WaitClear` (1436: once clear, rises). |
| test | EffectKind18Sub22_SetMap | effect_5d.cpp:1453 | Sub-kind 0x22's map writer: same flag +0x34; its answer picks the row of table 0x65E364 written to the cells. |
| test | EffectKind18Sub42_WaitFlag | effect_5f.cpp:518 | Kind 0x18 sub-kind 0x42, state 1. Flag is +0xB, from byte 3 of the 4-byte place record at 0x65E8E0 (`kSub42Places`, 3 places) by +0x36. When the flag differs from the variant +8 and no member is near: sound 0x205, raises. |
| test | EffectKind18Sub42_WaitFlagBack | effect_5f.cpp:595 | Sub-kind 0x42, state 4. Same flag +0xB; when "flag clear" differs from +8: sound 0x205, lowers. |
| test | EffectKind18Sub51_Wait | effect_6c.cpp:914 | Kind 0x18 sub-kind 0x51, state 1. Variant +0x36 of 2, 3 or 4 tests flag 0x7A + (variant - 2), so 122, 123 or 124. Once set: the cells fade. Variant 0 uses chapter 14's row, variant 1 `Cond_ByteFE`. |
| set | EffectKind18Sub5C_Place | effect_6d.cpp:271 | Kind 0x18 sub-kind 0x5C (a two-panel door), state 0. Flag is +8, from byte table 0x65F38C (`kSub5CFlags`) by the variant +0xB (0..1). Sets it when placed. |
| clear | EffectKind18Sub5C_WaitNear | effect_6d.cpp:292 | Sub-kind 0x5C, state 1. Leader near (or `Cond_ByteFE` 1): clears flag +8, sound, opens. |
| set | EffectKind18Sub5C_Close | effect_6d.cpp:324 | Sub-kind 0x5C, state 4. Closed again: sets flag +8. Reading, not checked: the flag says "this door is shut" while the effect lives. |
| test | `Field_AfterBattleTally` | `field_e2.cpp:210` | Loop over 246..255 (`0xF6..0xFF`) for the first clear flag. Only from chapter 8 with `FieldE2_LevelA` not 0. |
| set | `Field_AfterBattleTally` | `field_e2.cpp:228` | That first clear flag (246..255) is set when the counter `Rest4a_ClockA` reaches its threshold. The threshold depends on records counted at `Community_Records`. |
| test | `FieldTail_StoryWarp` | `field_e2.cpp:1416` | Loop from 255 down to 246 for the highest set flag. It picks the area `0xB9 - (255 - n)`, or `0xAF` when none is set, for `Field_ChangeArea`. |
| test | `ItemUse_FaerieTiara` | `item_use.cpp:270` | Loop from 255 down to 246. `ModeRest_AreaAsked` = `0xB9` less one for each clear flag above the first set one. Keeps the return point. |
| test | `EventCond_StoryFlag` | `field_o.cpp:1304` | Event-script condition 12. The flag is the script's operand byte. |
| test | `EventCond_Flag` | `event_script.cpp:707` | Event-script condition 2, on row `EventScript_FlagBank`. The event bank after op `F9 14`. |
| test | `Area_TestCondition` | `map_cells.cpp:93` | Map-data code kind `0x14`: the event flag in the code's low byte. |
| test | `Area_TestCondition` | `map_cells.cpp:94` | Map-data code kind `0x34`: the same flag, inverted. |
| test | `Area_ZoneTrack` | `window_task.cpp:222` | A music-set entry with row 20 tests event flag `entry[2]`. |
| test | `MoveScript_Variable` | `move_cmds.cpp:449` | Variable 12 is the dword of flags 0..31, compared or masked by `MoveScript_Flow` ops `04..09`. |
| set | `Scenario_SubInit` | `title_states.cpp:450` | Flags 0..127 overwritten from the 16-byte table `0x662CD8` on the first scenario frame of a New Game. |
