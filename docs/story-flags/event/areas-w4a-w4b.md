# Event flags: area_w4a.cpp and area_w4b.cpp

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Both files reach the event bank (`Cond_Flags + 0xA0`, `0x904030`) through a
file-local helper, checked at the top of each file:

- `area_w4a.cpp:65` `Bank()` = `Cond_Flags + 0xa0`; `Test`, `Set` and `Clear`
  (`area_w4a.cpp:66..68`) wrap `Flags_Test` / `Flags_Set` / `Flags_Clear` on it.
  Three sites call `Flags_Toggle(Bank(), n)` directly.
- `area_w4b.cpp:57` `Story()` = `Cond_Flags + 0xa0`. Every site calls
  `Flags_Set` / `Flags_Clear` / `Flags_Test` on it directly, except one helper
  `ChangeIfCounter3` (`area_w4b.cpp:268`) that takes the flag as an argument.

Not event-bank sites, so not listed: `Area155_ChoiceGiveItem4D`
(`area_w4a.cpp:788`, `:793`) sets bit 1 of `Cond_Flags + 0x68` (chapter row 13),
and `Area172_ChoiceFlag12` (`area_w4b.cpp:1028`) tests bit 0x12 of
`Cond_Flags + 0x70` (chapter row 14). No direct byte access to
`0x904030..0x904057` was found in either file.

`Op` uses a fourth value, `toggle`, for the `Flags_Toggle` sites: the flag's bit
is inverted, so the code both reads and writes it. `Cond_ByteFD` and
`Cond_ByteFE` below are bytes the code compares or writes; their meaning is not
settled here. `Field_ChangeArea(a, ...)` names its first argument, the target
area, in decimal.

The tool (`tools/story_flags.py`) missed the three `toggle` sites and the six
call sites of `ChangeIfCounter3`, and placed `area_w4b.cpp:836` in
`Area171_Trigger55`; it is `Area172_ClearFlag4E`.

## Sites

| Flag | Op | Function | File:line | Context |
|---|---|---|---|---|
| 117 | clear | Area152_Init | area_w4a.cpp:103 | Area 152's init clears six flags in order: 0x75, 0x76, 0x8B..0x8E. Unconditional. |
| 118 | clear | Area152_Init | area_w4a.cpp:104 | Same init, second of the six clears. |
| 139 | clear | Area152_Init | area_w4a.cpp:105 | Same init (flag 0x8B). |
| 140 | clear | Area152_Init | area_w4a.cpp:106 | Same init (flag 0x8C). |
| 141 | clear | Area152_Init | area_w4a.cpp:107 | Same init (flag 0x8D). |
| 142 | clear | Area152_Init | area_w4a.cpp:108 | Same init (flag 0x8E). |
| 76 | test | Area167_HeightBy4B4C | area_w4a.cpp:896 | Area 167 script handler 2, `Cond_ByteFD` 2: flag 0x4C set sends the movement script to +3 = 5, position 0xFFFE. |
| 76 | clear | Area167_HeightBy4B4C | area_w4a.cpp:897 | Same branch: 0x4C cleared once consumed (a one-shot set by `Area167_Tail32` case 5). |
| 75 | test | Area167_HeightBy4B4C | area_w4a.cpp:902 | `Cond_ByteFD` 2, 0x4C clear: flag 0x4B clear puts the object's height at 0xFDC0. |
| 76 | test | Area167_HeightBy4B4C | area_w4a.cpp:909 | `Cond_ByteFD` 1: flag 0x4C set sends the script to +3 = 4, position 0xFFFE. |
| 76 | clear | Area167_HeightBy4B4C | area_w4a.cpp:910 | Same branch: 0x4C cleared. |
| 75 | test | Area167_HeightBy4B4C | area_w4a.cpp:915 | `Cond_ByteFD` 1: flag 0x4B set puts the height at 0x9C0; otherwise the height falls to 0x3C0. |
| 75 | toggle | Area167_Toggle4BUnlessLow | area_w4a.cpp:934 | Area 167 handler 3: unless the object's height is 0x3C0, flag 0x4B is inverted. |
| 75 | test | Area167_Skip3Unless4B | area_w4a.cpp:940 | Area 167 handler 4: flag 0x4B clear skips 3 bytes of the movement script. |
| 74 | test | Area167_HeightBy48To4A | area_w4a.cpp:951 | Area 167 handler 5, `Cond_ByteFD` 2: flag 0x4A set sends the script to +3 = 7 or 8 (by the entry zone), position 0xFFFE. |
| 74 | clear | Area167_HeightBy48To4A | area_w4a.cpp:952 | Same branch: 0x4A cleared once consumed (a one-shot set by `Area167_Tail32` case 9). |
| 72 | test | Area167_HeightBy48To4A | area_w4a.cpp:958 | `Cond_ByteFD` 2, 0x4A clear: flag 0x48 clear chooses a height by flag 0x49. |
| 73 | test | Area167_HeightBy48To4A | area_w4a.cpp:959 | Same branch: 0x49 set gives height 0xFDC0, clear gives 0x9C0. |
| 74 | test | Area167_HeightBy48To4A | area_w4a.cpp:966 | `Cond_ByteFD` 1: flag 0x4A set sends the script to +3 = 7, position 0xFFFE. |
| 74 | clear | Area167_HeightBy48To4A | area_w4a.cpp:967 | Same branch: 0x4A cleared. |
| 73 | test | Area167_HeightBy48To4A | area_w4a.cpp:972 | `Cond_ByteFD` 1: flag 0x49 clear gives height 0x9C0; else 0x3C0. |
| 72 | set | Area167_Flags48By49 | area_w4a.cpp:994 | Area 167 handler 6, unless height is 0x3C0: `Cond_ByteFD` 1 sets 0x48 ... |
| 73 | clear | Area167_Flags48By49 | area_w4a.cpp:995 | ... and clears 0x49. |
| 72 | clear | Area167_Flags48By49 | area_w4a.cpp:997 | `Cond_ByteFD` 2 clears 0x48 ... |
| 73 | set | Area167_Flags48By49 | area_w4a.cpp:998 | ... and sets 0x49. |
| 72 | clear | Area167_Flags48By49 | area_w4a.cpp:1000 | `Cond_ByteFD` 3 clears both 0x48 ... |
| 73 | clear | Area167_Flags48By49 | area_w4a.cpp:1001 | ... and 0x49. Other values change neither. |
| 70 | set | Area167_Tail32 | area_w4a.cpp:1089 | Tail kind 32 state 1, once script variable 6 is 0x18: flag 0x46 set, then `Field_ChangeArea(149, ...)`. |
| 75 | toggle | Area167_Tail32 | area_w4a.cpp:1112 | Tail kind 32 state 0xB, variable 6 0x74: `Field_ChangeArea(167, ...)` (entry by `Cond_ByteFD` 2 or not), then flag 0x4B inverted. |
| 76 | set | Area167_Tail32 | area_w4a.cpp:1113 | Same state: flag 0x4C set (consumed by `Area167_HeightBy4B4C`). |
| 72 | clear | Area167_Tail32 | area_w4a.cpp:1138 | Tail kind 32 state 0x18, variable 6 0x54, variable 5 = 0: `Field_ChangeArea(167, ..., 5)`, 0x48 cleared ... |
| 73 | clear | Area167_Tail32 | area_w4a.cpp:1139 | ... and 0x49 cleared. |
| 72 | set | Area167_Tail32 | area_w4a.cpp:1142 | Variable 5 = 1: `Field_ChangeArea(167, ..., 0x83)`, 0x48 set ... |
| 73 | clear | Area167_Tail32 | area_w4a.cpp:1143 | ... and 0x49 cleared. |
| 72 | clear | Area167_Tail32 | area_w4a.cpp:1146 | Variable 5 = 2: `Field_ChangeArea(167, ..., 0x84)`, 0x48 cleared ... |
| 73 | set | Area167_Tail32 | area_w4a.cpp:1147 | ... and 0x49 set. |
| 74 | set | Area167_Tail32 | area_w4a.cpp:1150 | Same state, every way: flag 0x4A set (consumed by `Area167_HeightBy48To4A`). |
| 144 | set | Area167_Tail32 | area_w4a.cpp:1167 | Tail kind 32 state 0x20, countdown done: flag 0x90 set, `Field_ChangeArea(168, ...)`, `Field_ScriptFlags2` bit 6 set. Reached from state 0x1E, which `Area167_Init` arms. |
| var | toggle | Area167_CellHook | area_w4a.cpp:1205 | Area 167 cell hook: when the leader's cell and facing match the one record of `Area167_Switch` (0x63C594), the flag number is that record's byte +3, read from the image's data; the flag is inverted and tail kind 32 armed at state 5. |
| 143 | test | Area167_Init | area_w4a.cpp:1216 | Area 167 init, `Cond_ByteFD` 0: needs flag 0x8F set ... |
| 144 | test | Area167_Init | area_w4a.cpp:1217 | ... and 0x90 clear; then camera distance 0x980 and tail kind 32 armed at state 0x1E (which ends by setting 0x90). |
| 116 | set | HealStates | area_w4b.cpp:240 | Shared tail states 1..3 of areas 169 (tail kind 47) and 171 (tail kind 48): after sound 0x202, `Party_HealJoined`, message 1 closed, flag 0x74 set and the tail ends. |
| var | set | ChangeIfCounter3 | area_w4b.cpp:270 | Area 170 tail kind 37 helper: when counter 3 equals the given count, sets the flag passed in, then `Field_ChangeArea(170, ...)`. Its six callers pass 0x55, 0x56 or 0x57 (rows below). |
| 85 | set | Area170_Tail37 | area_w4b.cpp:481 | Tail kind 37 state 1, through `ChangeIfCounter3` (count 0x24): flag 0x55, `Field_ChangeArea(170, ..., 0x82)`. |
| 85 | set | Area170_Tail37 | area_w4b.cpp:483 | State 6, count 0x24: flag 0x55, change to area 170 entry 0x83. |
| 86 | set | Area170_Tail37 | area_w4b.cpp:485 | State 11, count 0x44: flag 0x56, change to area 170 entry 0x86. |
| 86 | set | Area170_Tail37 | area_w4b.cpp:487 | State 16, count 0x44: flag 0x56, change to area 170 entry 0x87. |
| 87 | set | Area170_Tail37 | area_w4b.cpp:489 | State 21, count 0x64: flag 0x57, change to area 170 entry 0x8A. |
| 87 | set | Area170_Tail37 | area_w4b.cpp:491 | State 26, count 0x64: flag 0x57, change to area 170 entry 0x8B. |
| 143 | set | Area168_ChoiceFlag8FArm59 | area_w4b.cpp:337 | Area 168 choice 5, answer 0: flag 0x8F set, tail kind 59 armed at state 0 (which then changes to area 167, entry 0x89). |
| 145 | set | Area168_Tail59 | area_w4b.cpp:378 | Area 168 tail kind 59 state 11, once counter 0 is 1: sound 0x204, flag 0x91 set. Reached from choice 6 (state 10, `Kind2_Place(0)`). |
| 116 | clear | Area169_InitClearFlag74 | area_w4b.cpp:399 | Area 169 init: `Cond_ByteFD` not 1 clears 0x74. |
| 116 | test | Area169_ArriveHook | area_w4b.cpp:427 | Area 169 arrive hook: `Cond_ByteFD` 1 and 0x74 clear arms the heal tail (kind 47); set means no heal. |
| 87 | clear | Area170_Tail37 | area_w4b.cpp:495 | Tail kind 37 state 29, counter 3 at 0: `Field_ScriptFlags` \|= 0x1010, flag 0x57 cleared, state 53. |
| 85 | clear | Area170_Tail37 | area_w4b.cpp:500 | State 30 (where every `ChangeIfCounter3` leads), counter 3 at 0: 0x55, 0x56, 0x57 all cleared, tail ended. |
| 86 | clear | Area170_Tail37 | area_w4b.cpp:501 | Same state: 0x56 cleared. |
| 87 | clear | Area170_Tail37 | area_w4b.cpp:502 | Same state: 0x57 cleared. |
| 126 | set | Area170_StepHook | area_w4b.cpp:633 | Area 170 step hook, `Cond_ByteFD` 5, step on the second set of edges: tail kind 37 armed at 20 (0x14), flag 0x7E set, `Field_ScriptFlags` &= 0xEFEF. |
| 126 | test | Area170_StepHook | area_w4b.cpp:637 | Same hook: flag 0x7E set stops the two exact-position checks that arm state 50 (pose 7) or write state 60 (pose 3). |
| 126 | clear | Area170_Trigger19 | area_w4b.cpp:654 | Area 170 object trigger 19: tail kind 37 armed at 40, flag 0x7E cleared. |
| 103 | set | Area170_Trigger20 | area_w4b.cpp:660 | Area 170 object trigger 20: flag 0x67 set, nothing else. |
| 121 | test | Area170_Trigger56 | area_w4b.cpp:672 | Area 170 object trigger 56: flag 0x79 clear tries `Inventory_Add(1, 0x4B, 1)`; set gives system message 1. |
| 121 | set | Area170_Trigger56 | area_w4b.cpp:674 | Item added: flag 0x79 set, message 0x22, sound 0x106. Not added: system message 3, flag stays clear. A one-time item pick-up. |
| 115 | set | Area171_Leader89Gate | area_w4b.cpp:715 | Area 171 handler 0: leader's +0x89 is 5 and byte `AreaW4b_GateKey` (0x903DB6) is 0x4B: flag 0x73 set, map cells (2..3, 0x21..0x22) rewritten (0xC0 / 0). |
| 116 | clear | Area171_InitClearFlag74 | area_w4b.cpp:739 | Area 171 init: `Cond_ByteFD` not 3 clears 0x74. |
| 115 | test | Area171_StepHook | area_w4b.cpp:793 | Area 171 step hook, `Cond_ByteFD` 2: flag 0x73 clear lets a step onto map byte 0x89 arm tail kind 48 at 20 (message 2); set disables it. |
| 116 | test | Area171_ArriveHook | area_w4b.cpp:813 | Area 171 arrive hook: `Cond_ByteFD` 3 and 0x74 clear arms the heal tail (kind 48). |
| 78 | clear | Area172_ClearFlag4E | area_w4b.cpp:836 | Area 172 handler 0 (choice 1): flag 0x4E cleared, nothing else. |
| 78 | set | Area172_Tail35 | area_w4b.cpp:1050 | Area 172 tail kind 35 state 1, counter 3 at 0x14: flag 0x4E set, `Field_ChangeArea(172, ..., 0x83)`. |
| 78 | set | Area172_Tail35 | area_w4b.cpp:1060 | State 6 (from choice 0 when chapter row 14 flag 0x12 is set), counter 3 at 0x14: flag 0x4E set, change to area 172 entry 0x82. |

## What each flag means here

Observed means set in the real saves listed in `docs/story-flags/observed-rows.md`;
only saves 20, 21 and 22 (chapters 14, 10, 15) carry any of these flags.

- **70 (0x46)**: set by area 167 tail kind 32 state 1 before the change to area
  149 (`area_w4a.cpp:1089`). Nothing here reads it. Also cleared in
  `area_w3f.cpp:1053` and set in `area_w3g.cpp:266` and `scena_sc13.cpp:2114`
  (those helpers' banks not checked). Not set in any save.
- **72 (0x48) and 73 (0x49)**: a pair that records one of three states of an
  area 167 object (both clear, 0x48 only, 0x49 only), written by handler 6 by
  `Cond_ByteFD` and by tail state 0x18 by script variable 5; read by handler 5
  to pick the object's height. Reading, not checked: the position of a movable
  part of area 167. 73 is set in saves 20..22; 72 is not.
- **74 (0x4A)**: one-shot set by tail state 0x18 after an area change
  (`:1150`), consumed and cleared by handler 5 to redirect the movement script.
  Not set in any save.
- **75 (0x4B)**: an on/off state of an area 167 object, inverted by handler 3
  and by tail state 0xB; read by handlers 2 and 4 for the height and a script
  skip. Set in saves 20..22.
- **76 (0x4C)**: one-shot set by tail state 0xB after an area change, consumed
  and cleared by handler 2. Not set in any save.
- **78 (0x4E)**: set by area 172 tail kind 35 before both changes to area 172;
  cleared by area 172 handler 0. Also set in `scena_sc13.cpp:2401` (bank not
  checked). Nothing here tests it. Not set in any save.
- **85 (0x55), 86 (0x56), 87 (0x57)**: which of three pairs of area 170 tail
  routes was taken (counts 0x24, 0x44, 0x64), set just before each change back
  into area 170; all three cleared by tail state 30, 0x57 also by state 29.
  Reading, not checked: a marker across the area change only. 85 is also set in
  `area_w4f.cpp:683` (bank not checked). None set in any save.
- **103 (0x67)**: set by area 170 object trigger 20. Not tested here;
  `effect_5f.cpp:1022` tests it in the event bank. Set in saves 20..22.
- **115 (0x73)**: set once area 171 handler 0 sees gate key byte 0x4B with the
  leader's +0x89 at 5, and rewrites four map cells; once set, the area 171 step
  hook no longer arms tail state 20. Reading, not checked: an opened gate in
  area 171. Set in saves 20..22.
- **116 (0x74)**: "the party was healed here". Set by the shared heal states
  of areas 169 and 171; tested by both arrive hooks so the heal runs once;
  cleared by area 169's init unless `Cond_ByteFD` is 1 and area 171's unless it
  is 3. Not set in any save (consistent with it being cleared on re-entry,
  reading, not checked).
- **117 (0x75), 118 (0x76), 139..142 (0x8B..0x8E)**: cleared by area 152's init
  every time it runs. The same six clears are in `area_w4e.cpp:433..438`.
  Nothing here sets or tests them; meaning unknown. None set in any save.
- **121 (0x79)**: a one-time item (item 0x4B, kind 1) from area 170 object
  trigger 56; set when the item is added. Set in saves 20..22.
- **126 (0x7E)**: set by the area 170 step hook when it arms tail state 20,
  cleared by object trigger 19 (tail state 40); while set, two exact-position
  checks in the same hook are off. Set in saves 20..22.
- **143 (0x8F)**: set by area 168 choice 5 (answer 0) before the change to area
  167; with 0x90 clear and `Cond_ByteFD` 0 it makes area 167's init start tail
  state 0x1E. Also tested by `effect_5f.cpp:770` (event bank). Set in saves
  20..22.
- **144 (0x90)**: set at the end of that area 167 sequence (tail state 0x20,
  then the change to area 168); stops it from running again. Story progress:
  0x8F then 0x90 form a once-only sequence across areas 168 and 167. Set in
  saves 20..22.
- **145 (0x91)**: set by area 168 tail kind 59 state 11 (from choice 6). Not
  tested here; `effect_4b.cpp:1076` tests it in the event bank. Set in saves
  20..22.
- **var (`area_w4a.cpp:1205`)**: the area 167 switch's flag, a byte in the
  image's data at 0x63C597. Unknown.

## Unresolved

- `Area167_CellHook` (`area_w4a.cpp:1205`) toggles a flag whose number is byte
  +3 of the one `Area167_Switch` record (0x63C594..0x63C597, `kA167Switch` in
  `area_w4a_callees.h:110`). The number is game data; it needs a read of the
  image or a run to name. It may be one of 0x48..0x4C, which the same area
  uses (reading, not checked).
- `ChangeIfCounter3` (`area_w4b.cpp:270`) is resolved by its six callers to
  85, 86 and 87; no other caller exists in the file.
- `Cond_ByteFD` gates most of these sites (values 0..5). Its meaning (a
  scenario sub-state or a visit counter) is not settled from these files.
- The other-file sites named above (`area_w3f`, `area_w3g`, `area_w4e`,
  `area_w4f`, `scena_sc13`) were found by a number search only; their helpers'
  banks were not opened here and belong to those files' maps.
- Whether 0x48/0x49/0x4B describe one object or several in area 167, and what
  0x55..0x57 select in area 170, needs a run.
