# Event flags: the chapter files

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Source: the chapter files in `src/game/`: `scena_sc0.cpp`, `scena_sc1.cpp`,
`scena_sc2.cpp`, `scena_sc3.cpp`, `scena_sc5.cpp`, `scena_sc6.cpp`,
`scena_sc7.cpp`, `scena_sc9a.cpp`, `scena_sc9b.cpp`, `scena_sc11.cpp`,
`scena_sc12.cpp`, `scena_sc13.cpp`, `scena_sc15.cpp`, `scena_se.cpp`,
`scena_sx.cpp`, `scena_sx2.cpp`, `scena_calls.cpp`.

The event bank is `Cond_Flags + 0xA0` (0x904030, save block `+0x650`). Each file
reaches it by its own spelling:

- `scena_sc5.cpp`: `Story()` (line 77). `Bits()`, `Flag()`, `Set()`, `Clr()` are
  the chapter row (`Chapter_FlagRow`).
- `scena_sc6.cpp`: `Story()` (line 92), with `StoryFlag()`, `SetStory()`,
  `ClrStory()` (lines 94, 97, 98). `Bits()`, `Flag()`, `Set()`, `Clr()` are the
  chapter row.
- `scena_sc13.cpp`: `Story()` (line 82). `Bits()` and its helpers are the
  chapter row.
- `scena_sc7.cpp`, `scena_sx2.cpp`: inline `At(::bof3::Img(Cond_Flags_addr) + 0xa0)`.
- `scena_sc9a.cpp`, `scena_sc9b.cpp`: `SetAt` / `ClrAt` (sc9a lines 86-87, sc9b
  lines 102-103) on the literal `Img(Cond_Flags_addr) + 0xa0`. The same helpers
  also write chapter rows (`+ 0x30`), which are left out here.
- `scena_sc12.cpp`: inline `&Cond_Flags[0xa0]`.
- `scena_sc15.cpp`: `SetAt` / `ClearAt` / `TestAt` (lines 201-204) on
  `Img(Cond_Flags_addr) + 0xa0`. The same helpers write rows 14 and 15
  (`+ 0x70`, `+ 0x78`), left out here.

Every other flag helper in these files (`Bank()`, `Bits()`, `Row()`, `Flags()`,
and the local `bank`, `bits`, `row`, `f` pointers) was checked and is
`Chapter_FlagRow`, the current chapter's row. Two addresses near the bank are not
event flags: `0x90405C` (sc13:1651, :1908, sc15:533) is past the bank's end
(0x904057), and `at::kStoryFlags` in `scena_sc1.cpp:508` is `0x90410C`
(`scena_sc1_callees.h:29`), the second find bank.

No event-flag site was found in `scena_sc0.cpp`, `scena_sc1.cpp`,
`scena_sc2.cpp`, `scena_sc3.cpp`, `scena_sc11.cpp`, `scena_se.cpp`,
`scena_sx.cpp` or `scena_calls.cpp` (grep for every spelling above, for
`0x9040[3-5]x` and for `Cond_Flags[0xa0..0xc7]`).

The observed column uses [`observed-rows.md`](../observed-rows.md): saves 20, 21
and 22 are the three late saves (chapters 14, 10, 15; save 21 is probably a copy
of save 20).

## Sites

| Flag | Op | Function | File:line | Context |
|---|---|---|---|---|
| 38 | set | Scena05_Cell0 | scena_sc5.cpp:261 | Chapter 5, cell hook entry 0 (`:273`) through `CellCue(0x26, 1)`: only with chapter flag 6 set and 9 clear. Also script bit 0x40, counters 2 and 3 zeroed, sound 0x202, the current run's step set to 1. |
| 37 | set | Scena05_Cell1 | scena_sc5.cpp:261 | Chapter 5, cell hook entry 1 (`:276`) through `CellCue(0x25, 0x19)`: same gate (chapter flag 6 set, 9 clear); the current run's step set to 0x19. |
| 38 | clear | EnterAreaBody | scena_sc5.cpp:400 | Chapter 5 area entry, area 0x34 with chapter flag 9 clear, counter 2 = 4: counter 0 zeroed and flag 38 cleared. |
| 47 | set | Scena06_Run13 | scena_sc6.cpp:1638 | Chapter 6 run 13 step 4 (reached when chapter flag 0x26 is set after the kind 0x45 effect): map cell (0x5D, 0x2E) set to 0x51, sound 0x201. Step 5 then sets chapter flag 0x27 and the run ends with 47 left set. |
| 47 | set | Scena06_Run13 | scena_sc6.cpp:1659 | Chapter 6 run 13 step 6 (chapter flag 0x26 clear), with chapter flag 0x25 set: set, chapter flag 0x25 cleared, step 8. |
| 47 | clear | Scena06_Run13 | scena_sc6.cpp:1668 | Chapter 6 run 13 step 7 (step 6 with chapter flag 0x25 clear): cleared, run ends. |
| 47 | clear | Scena06_Run13 | scena_sc6.cpp:1677 | Chapter 6 run 13 step 8, once counter 0 is 1: cleared, run ends. So only the step 4/5 path leaves 47 set. |
| 7 | set | Scena06_Run15 | scena_sc6.cpp:1812 | Chapter 6 run 15 step 6: effect kind 0x4B, drop-in 6, `Cond_ByteFE = 1`, sounds 0x207 and 0x201, then step 9. |
| 5 | test | Scena06_Cell3 | scena_sc6.cpp:2319 | Chapter 6 cell hook entry 3: with chapter flag 0x27 and event flags 5 and 6 set, run 15 starts at step 5 (chapter flag 0x2C clear) or step 0x1E (0x2C set); otherwise run 15 from step 0. |
| 6 | test | Scena06_Cell3 | scena_sc6.cpp:2319 | Same test as the row above (both 5 and 6 must be set). |
| 79 | set | Scena07_EnterArea | scena_sc7.cpp:257 | Chapter 7 area entry: set on every entry to area 0x69, no other condition. |
| 67 | set | Scena08_Start | scena_sc7.cpp:1384 | Chapter 8 start (state 0): row 8 zeroed, counters zeroed, record 0 party bits, key item 4 swapped for 0xE, then flag 67 set. |
| 69 | clear | Scena09_Run15 | scena_sc9a.cpp:1497 | Chapter 9 run 15 step 0x46 (when `ObjTrio[1]` byte 0x137 is not 3): message 0x87, step 0x47; also sets chapter 6's flag 62 (row 6, `+0x30`). |
| 64 | set | Scena09_Run15 | scena_sc9a.cpp:1660 | Chapter 9 run 15 step 0x7E, counter 0 = 5: script byte xor 0x80, step 0x7F, then `ChangeArea(0x3B, ...)`. |
| 51 | set | Scena09_Run15 | scena_sc9a.cpp:1729 | Chapter 9 run 15 step 0x96, after step 0x95 set chapter flag 0x23 and changed into area 0x2D: set, draw pass 0x1F, run ends. |
| 79 | set | EnterArea10Body | scena_sc9b.cpp:412 | Chapter 10 area entry, area 0x69, `Cond_ByteFD` 0, chapter flag 0x35 clear. |
| 79 | clear | Scena10_Run12 | scena_sc9b.cpp:1866 | Chapter 10 run 12 step 2, after the shake timer. Run 12 is started by `Scena10_ArriveHook` in area 0x69 (`:2366-2372`, `Cond_ByteFD` 0, chapter flag 0x35 clear, z cell above 0x1C); its step 5 sets chapter flag 0x35, so the entry stops setting 79 afterwards. |
| 67 | set | Scena10_ArriveHook | scena_sc9b.cpp:2362 | Chapter 10 arrive hook, area 0x3A, chapter flag 8 set and 9 clear, z at or above 0x300000, x cell near 0x1C: sets chapter flag 9 and event flag 67, counter 0 = 0xA. |
| 41 | set | Scena12_Run5 | scena_sc12.cpp:815 | Chapter 12 run 5 step 0x16, counter 0 = 0xA: run ends, then flag 41 set, chapter flag 20 cleared, an effect of kind 0x77. |
| 67 | set | Scena13_Start | scena_sc13.cpp:183 | Chapter 13 start (state 0): script bit 0x40, pass 0x1F, party pass, call A 0, then 67 set. |
| 66 | clear | Scena13_Start | scena_sc13.cpp:184 | Same step as the row above: 66 cleared. |
| 45 | test | Scena13_StepHook | scena_sc13.cpp:1361 | Chapter 13 step hook, area 0x91: with 45 set, chapter flag 0xB clear and the position in a box, by the lead's facing, starts run 3 at step 0x14 (drop-in 9). |
| 119 | clear | Scena14_Run1 | scena_sc13.cpp:1703 | Chapter 14 run 1 step 0x14 (not 0x19): cleared, byte 0x904152 zeroed, `ChangeArea(0xC1, ..., 0x83)`. |
| 138 | set | Scena14_Run1 | scena_sc13.cpp:1712 | Chapter 14 run 1 steps 0x14 and 0x19, both paths, after the area change into 0xC1. Step 0x19 also sets chapter flags 2 and 3 and uses entrance 0x84. |
| 70 | set | Scena14_Run4 | scena_sc13.cpp:2114 | Chapter 14 run 4 step 0xE: set before `ChangeArea(0x95, ...)`. |
| 78 | set | Scena14_Run6 | scena_sc13.cpp:2401 | Chapter 14 run 6 step 1, counter 3 = 0x14: set before `ChangeArea(0xAC, ...)`. |
| 122 | test | Scena14_ArriveHook | scena_sc13.cpp:2889 | Chapter 14 arrive hook, area 0x8D, `Cond_ByteFD` 1: only with 122 set, run 7 starts at step 3 (chapter flag 25 clear) or step 4 (25 set, 26 clear), each in a position box. |
| 119 | clear | Scena15_Run1 | scena_sc15.cpp:571 | Chapter 15 run 1 step 0x14 (not 0x19): cleared, `AreaW3c_CampFlag` zeroed, `ChangeArea(0xC1, ..., 0x83)`. Same shape as chapter 14's run 1. |
| 138 | set | Scena15_Run1 | scena_sc15.cpp:580 | Chapter 15 run 1 steps 0x14 and 0x19, both paths. Step 0x19 also sets row 14's flags 2 and 3 (`+0x70`, lines 575-576) and uses entrance 0x84. |
| 129 | test | Scena15_Run3 | scena_sc15.cpp:665 | Chapter 15 run 3 step 4: waits (returns each frame) until 129 is set, then places effect 0x13 and goes to step 5. |
| 88 | set | Scena15_Run6 | scena_sc15.cpp:1520 | Chapter 15 run 6 step 0x29: chapter flag 21 (0x15) set, then 88 set, then `ChangeArea(0xAD, ..., 0)`, music 0xA1. |
| 129 | test | Scena15_StepHook | scena_sc15.cpp:1713 | Chapter 15 step hook, area 0xAD, `Cond_ByteFD` 2: only while 129 is clear, x cell above 0x2C and z cell 0x78..0x7F start run 3 at step 0. |
| 146 | set | Scena17_EndReturn | scena_sc15.cpp:2684 | Chapter 17 end step 0, the return after the ending: 146 set, then row 15's flags 9..22 cleared (`+0x78`, line 2685), area 0x8F, chapter byte 15. |
| 28 | set | Effect_HoldFlag1C | scena_sx2.cpp:104 | Not chapter code: any chapter. Takes a free effect record as kind 4 for `frames` frames, then sets 28. No free record: nothing, 28 not set. The clear is in `EffectKind04_HoldTick` (not in these files), per the comment at line 90. |

34 sites, 23 distinct flags.

## What each flag means here

- **5, 6**: tested together by `Scena06_Cell3` (chapter 6); with chapter flag
  0x27 they open run 15 at step 5 or 0x1E. No chapter file sets them. Set in
  saves 20..22.
- **7**: set by `Scena06_Run15` step 6 (chapter 6). Nothing in these files tests
  or clears it. Meaning unknown. Set in saves 20..22.
- **28**: set by `Effect_HoldFlag1C` (any chapter) for a number of frames. The
  comment at `scena_sx2.cpp:94-95` reads it as a switch cool-down. Clear in all
  saves.
- **37**: set by `Scena05_Cell1` (chapter 5, area 0x34 cell cue 1). Not cleared
  in these files. Clear in all saves.
- **38**: set by `Scena05_Cell0` (chapter 5, area 0x34 cell cue 0), cleared on
  entry to area 0x34 at counter 2 = 4. Transient. Clear in all saves.
- **41**: set by `Scena12_Run5` step 0x16 (chapter 12). Set in saves 20..22.
- **45**: tested by `Scena13_StepHook` (chapter 13) to arm run 3 step 0x14 in
  area 0x91. Not set in these files. Set in saves 20..22.
- **47**: set and cleared by `Scena06_Run13` (chapter 6); left set only on the
  step 4/5 path, which also sets chapter flag 0x27. Set in saves 20..22.
- **51**: set by `Scena09_Run15` step 0x96 (chapter 9). Set in saves 20..22.
- **64**: set by `Scena09_Run15` step 0x7E (chapter 9). Set in saves 20..22.
- **66**: cleared by `Scena13_Start` (chapter 13). Not set in these files. Clear
  in all saves.
- **67**: set at three points: `Scena08_Start` (chapter 8),
  `Scena10_ArriveHook` (chapter 10, with chapter flag 9) and `Scena13_Start`
  (chapter 13). Never cleared or tested in these files, so the second and third
  sets change nothing unless code elsewhere clears it (unknown). Set in saves
  20..22.
- **69**: cleared by `Scena09_Run15` step 0x46 (chapter 9). Set in saves 20..22,
  so something outside these files sets it later (unknown).
- **70**: set by `Scena14_Run4` step 0xE (chapter 14). Clear in save 20.
- **78**: set by `Scena14_Run6` step 1 (chapter 14). Clear in save 20.
- **79**: set on entry to area 0x69 in chapter 7 (always) and chapter 10 (until
  chapter flag 0x35); cleared by `Scena10_Run12` step 2 (chapter 10). Set in
  saves 20..22, so it is set again after chapter 10 by code outside these files
  (unknown).
- **88**: set by `Scena15_Run6` step 0x29 (chapter 15) before area 0xAD. Clear in
  save 22.
- **119**: cleared by run 1 step 0x14 in chapters 14 and 15. Set in saves 9, 18,
  19 (chapters 1, 4, 5); clear in saves 20..22. Its setter is outside these
  files (`Rest_Begin`, per the chapter 1 map).
- **122**: tested by `Scena14_ArriveHook` (chapter 14); gates run 7 steps 3 and 4
  in area 0x8D. Not set in these files. Set only in save 22 (chapter 15), clear
  in save 20 (chapter 14).
- **129**: tested by `Scena15_Run3` step 4 (waits for it) and by
  `Scena15_StepHook` (starts run 3 at step 0 only while clear) (chapter 15). Not
  set in these files. Clear in save 22.
- **138**: set by run 1 steps 0x14 and 0x19 in chapters 14 and 15, beside the
  change into area 0xC1. Not cleared in these files. Clear in saves 20..22.
- **146**: set by `Scena17_EndReturn` (chapter 17), the return after the ending.
  Clear in all saves.

Story progress across chapters, reading, not checked: 67 is the one flag set by
three chapters' starts or arrivals (8, 10, 13). 79 follows area 0x69 across
chapters 7 and 10. 119 and 138 are written by the same run-1 shape in chapters
14 and 15. 146 marks the game cleared. The others are each used by one chapter.

## Corrections to the chapter maps

- **chapter-00.md** has no "Event flags used by this chapter" section. The
  answer is: none. No `Scena00_*` function in `scena_sc0.cpp` sets, clears or
  tests the event bank. Event flags 3, 25 and 27, set one frame into a new game,
  are not set by `scena_sc0.cpp`.
- **chapter-05.md**: correct. Note: `CellCue` sets only the step of the current
  run; "run 5" depends on run 5 being current, which the map shows elsewhere
  (row for flag 6). Not a fault.
- **chapter-06.md**: the flag 5/6 row is incomplete. `Scena06_Cell3` starts run 15
  at step 5 only while chapter flag 0x2C is clear; with 0x2C set it starts step
  0x1E; without 0x27, 5 and 6 it starts run 15 at step 0. That run 15 is "the
  chapter's last scene" is not shown at this site. The flag 47 row is right.
- **chapter-09.md**: correct. Missing context: step 0x46 also sets chapter 6's
  flag 62 at the same step (`scena_sc9a.cpp:1498`).
- **chapter-10.md**: the flag 79 rows are right but miss how they connect:
  `Scena10_ArriveHook` (`scena_sc9b.cpp:2366-2372`) starts run 12 in area 0x69
  while chapter flag 0x35 is clear; run 12 clears 79 at step 2 and sets 0x35 at
  step 5. So in chapter 10 flag 79 is set on entry and cleared by the run.
- **chapter-14.md**: correct on sites. "farewells" for flag 122 is the map's
  word; the code shows only that 122 gates run 7 steps 3 and 4 in area 0x8D
  (entrance 1). Step 0x19 also sets chapter flags 2 and 3 beside flag 138.
- **chapter-17.md**: cites line 2682 for the set of 146; 2682 is the function's
  first line, the set is line 2684.
- chapters 1, 2, 3, 4, 7, 8, 11, 12, 13, 15, 16, 18, 19: their chapter-file
  statements agree with the code. Their area-code notes were not checked here.
- Not in any map: `Effect_HoldFlag1C` (`scena_sx2.cpp:104`) sets flag 28; the maps
  of chapters 6, 8 and 9 name 28 (0x1C) as a latch but not this setter.

## Unresolved

- Who sets 3, 25, 27 (new game), 13 and 32: not in these files.
- Who sets 5, 6, 45, 122 and 129, which chapter code tests: not in these files.
  Chapter 6's map reads 5 and 6 as the area 0x4D cell switches (not checked).
- Who sets 69 again after chapter 9, and 79 again after chapter 10.
- Who clears 7, 37, 41, 67, 70, 78, 88, 138, if anything: not in these files.
  70, 78 and 138 are clear in save 20 after being set, so something does.
- Whether 66 and 67 are a pair: `Scena13_Start` sets one and clears the other;
  nothing in these files sets 66.
- Event-script conditions in the game data can test event flags; those are not
  in our source.
