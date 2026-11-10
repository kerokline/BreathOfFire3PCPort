# Story flags: chapter 10

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 10's row is `Cond_Flags + 0x50` (`0x903FE0`), save block `+0x600`. The
chapter's code is the second part of `src/game/scena_sc9b.cpp` (from
`Scena10_Frame`, `:1149`). Its wrappers are `Flag(n)`, `Set(n)`, `Clr(n)` on
`Bits()` (the row `Chapter_FlagRow` points at) and `SetAt(row, n)` /
`ClrAt(row, n)` on a literal row (`scena_sc9b.cpp:97-103`). The helpers
without a prefix serve chapter 10: `EnterArea10Body` is
`Scena10_EnterArea`'s body (`:1176`), `Run6Body` is `Scena10_Run6`'s (`:1811`),
`Run7Body` is `Scena10_Run7`'s (`:1820`), `PickupAt` is objects 5's and
`Scena10_PickupPose`'s search (`:376`).

Three files outside the chapter write the fixed row 10: `area_w1f.cpp` (area
0x4B = 75, a button-press minigame), `effect_5e.cpp` (effect sub-kind 0x24) and
`area_w2d.cpp` (area 0x62 = 98, through the current row).

"Entrance" below is `Cond_ByteFD`. "Leader kind" is `ObjTrio[0].char_id`;
"member kind" is a member record's `+0x89`. Event battle names are those of
[`../boss-fights.md`](../boss-fights.md).

## Start

- **Who starts the chapter.** Chapter 9's `StepHook9` in area 0x52 sets chapter
  9's flag 33 and starts run 11 at step 0x1E (`scena_sc9b.cpp:302-307`).
  `Scena09_Run11` step 0x1E calls `Party_DropIn(3)` and `Field_SetStatus80`
  (`scena_sc9a.cpp:1343-1347`). The next `Area_Enter` adds one to
  `Cond_ByteFA` and calls `Scenario_Start(10)` (`mode_flow.cpp:548-556`).
  Reading, not checked.
- **`Scenario_Start(10)`** (`title_states.cpp:404`) points `Chapter_FlagRow` at
  `0x903FE0` and zeroes the row's first dword only (flags 0..31,
  `title_states.cpp:416`).
- **State 0** is `ScenaShared_State0` (`scena_sc13.cpp:1424`): the state byte 1
  and nothing else. No area, party, run or flag.
- **State 1** is `Scena10_EnterArea` (`EnterArea10Body`, `scena_sc9b.cpp:389`).
  In area 0x52 at entrance 0 with flag 4 clear it clears flag 0x33 (`:391`).
  Which area the first entry is, is not in the code.

## Flags

All line numbers are `scena_sc9b.cpp` unless the cell names another file.

| Flag | Set by (function, step, file:line) | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Area75_PhaseReached`, `area_w1f.cpp:975` | Area 0x4B's press minigame reached, with `Cond_ByteFA` 0xA: run 5 started at step 0, drop-in 5 | - | nothing found | progress |
| 1 | `Area75_PhaseMissed`, `area_w1f.cpp:964` | A round of the minigame missed (early presses, idle frames or the gap) | - | `Area75_TailPresses` state 2, `area_w1f.cpp:643`: counter 0 starts at 0x13, not 0, on a retry | optional |
| 2 | `Scena10_Run5` step 8, :1787 | After event battle 0x24 (Angler, step 6, :1783): area change into 0x4B | - | `Area75_StepHook`, `area_w1f.cpp:824`: the minigame's trigger while clear | progress |
| 4 | `EffectKind18Sub24_Start`, `effect_5e.cpp:331` | Effect sub-kind 0x24 when counter 0 reaches 5. Reading: run 1 (counter 0 from 1, steps 0..2) holds it | - | `EnterArea10Body` :391, 392, 406: area 0x52 starts run 1 while clear; area 0x62's call entry and drop-in 1 while set and 8 clear; `Area82_OpenMessageByRowFlag4`, `area_w2a.cpp:481` (message 8 or 3) | progress |
| 5 | `Scena10_Run1` step 0xA, :1261 | Run 1 (area 0x52, entrance 1, flag 0x3D set, 4 clear, `Scena10_StartRun1` :1183): after area 0x42 and the greyed palette, area change into 0x52 | - | nothing found | progress |
| 6 | `Scena10_Run1` step 0xC, :1283 | Run 1 ends | - | nothing found | progress |
| 7 | `Scena10_StepHook`, :2304 | Area 0x52, entrance 0, flag 0x3D set, x 0x458000, z cell 0x23..0x24 | - | nothing found | optional |
| 8 | `Scena10_Run2` step 0x19, :1477 | Run 2 (object 2): load 0x301, effects in area 0x62, music 0x7B; ends | - | `EnterArea10Body` :406; `Scena10_ArriveHook` :2359 | progress |
| 9 | `Scena10_ArriveHook`, :2361 | Area 0x3A, flag 8 set, z at least 0x300000, x cell 0x1C..0x20; with event flag 0x43 | - | `Scena10_ArriveHook` :2359: once only | once-only |
| 10 (0xA) | `Scena10_Run3` step 5, :1528 | Run 3 from step 0 (`Scena10_ArriveHook` :2384, area 0x80, flag 0xA clear): a message by the party or a wait | - | `Scena10_ArriveHook` :2384 | progress |
| 11 (0xB) | `Scena10_Run3` step 0x15, :1595 | Run 3 from step 0xA (object 3): area change into 0x80, key item 9 (:1569), stream 2 | - | `EnterArea10Body` :445; `Scena10_StepHook` :2332; `Scena10_ArriveHook` :2390 | progress |
| 12 (0xC) | `EnterArea10Body`, :445 | Area 0x80, entrance 3, flag 0x12 clear, 0xB set | `Scena10_StepHook`, :2346 (the same moment it starts run 4 step 0) | `Scena10_StepHook` :2342: area 0x80, entrance 3, the spot: run 4 step 0 | transient |
| 13 (0xD) | `Scena10_Run4` step 0xA, :1677 | Run 4 from step 0xA (`Scena10_ArriveHook` :2390, area 0x80, flag 0xB set, 0xD clear): drop-in 3 | - | `Scena10_ArriveHook` :2390 | progress |
| 14 (0xE) | `Scena10_Run4` step 0x18, :1709 | Run 4 from step 0x14 (`Scena10_StepHook` :2332, area 0x80, entrance 0, flag 0xB set, 0xE clear, facing 0..2): request 6, area change into 0x58 | - | `Scena10_StepHook` :2332 | progress |
| 15 (0xF) | `Scena10_Object04`, :2147 | Object 4: drop-in 0, the object posed 5 | - | `Scena10_Run4` step 1 :1614 (message 0x26 while clear; else 0x28 with an item 0x4E..0x55 held, or 0x27); step 0x1E :1716 (drop-in 7 and stop while clear) | progress |
| 16 (0x10) | `Scena10_Run4` step 0x1F, :1729 | Run 4 from step 0x1E (`Scena10_StepHook` :2276, area 0x4B, entrance 1, flag 0x10 clear) with flag 0xF set: drop-in 8; area change into 0x4B | - | `Scena10_StepHook` :2276 | progress |
| 17 (0x11) | `Scena10_Run4` step 4, :1654 | Run 4 from step 0: message 0x28, the effect 0x5E, then `Scena10_TallyMet` (:2067) true: drop-in 4, then run 6 (:1664) | - | nothing found | progress |
| 18 (0x12) | `Run7Body` step 3, :868, and step 0xF, :948 | Run 7: area change into 0x79 (step 3); again at its last step | - | `EnterArea10Body` :399 (area 0x5E map bytes cleared while clear), :445, :447 (area 0x83 sound and shake) | progress |
| 19 (0x13) | `Scena10_Object06`, :2182, or `Scena10_Object07`, :2190 | Object 6 (drop-in 0xA, pose 0x1E) or object 7 (drop-in 0xB, pose 0x1F) | - | nothing found | choice (either object, reading) |
| 20 (0x14) | `Run6Body` step 0x13, :620 | Run 6 (from run 4): areas 0x80, 0x79, 0x84; the run waits at step 0x14 | - | `EnterArea10Body` :456: area 0x84, entrance 1: sound 0x204, shake and a camera effect while clear | progress |
| 21 (0x15) | `Scena10_Object08`, :2198 | Object 8: drop-in 1, the step + 1 | - | `EnterArea10Body` :435: area 0x79's camera and kind-2 sprite while 0x15 is set and 0x16 clear | progress (reading) |
| 22 (0x16) | `Run6Body` step 0x2F, :806 | Run 6: after area 0x79, area change into 0x5E | - | `EnterArea10Body` :395, 435 | progress |
| 23 (0x17) | `Run6Body` step 0x32, :829; `EnterArea10Body`, :397 | Run 6 ends after message 0x96; or an area 0x5E entry with 0x16 set and 0x17 clear | - | `EnterArea10Body` :395 | progress |
| 24 (0x18) | `Run7Body` step 7, :913 | Run 7: area change into 0x83, kind 0x31 effect | - | nothing found | progress |
| 25 (0x19) | `Scena10_Object09`, :2205 | Object 9 | - | nothing found | optional |
| 26 (0x1A) | `Area98_ChoiceFlag9`, `area_w2d.cpp:337` (current row) | Area 0x62's choice 0, an answer other than 0 (counter 1 = 1); with master bit 9 of `0x904654` | - | `Run7Body` step 0xF, :952: master bit 9 of `0x904657` set at the chapter's end | choice (reading) |
| 32..47 (0x20..0x2F) | `Scena10_Object05`, :2164: flag 0x20 + the pickup record's `+0` | A pickup (one of 16 `Scena10_Pickups` records) taken: item `+1` + 0x4E added, system message 2, sound 0x106. The bag full: system message 3 and no flag | - | `Scena10_PickupPose`, :2234: a taken pickup's sprite is removed (areas 0x4B and 0x56 name it) | optional (one per pickup) |
| 51 (0x33) | `Scena10_StepHook`, :2295 | Area 0x52, entrance 0, the cell, the facing, `Scena07_PartyHas89State2` true, flags 0x3D and 0x33 clear: run 11 step 0x23 (drop-in 4) | `EnterArea10Body`, :391 (area 0x52, entrance 0, flag 4 clear) | `Scena10_StepHook` :2294 | transient |
| 52 (0x34) | `Scena10_Run10` step 1, :1830 | Run 10 (object 11): drop-in 2 | - | nothing found | optional |
| 53 (0x35) | `Scena10_Run12` step 5, :1907 | Run 12 (`Scena10_ArriveHook` :2368, area 0x69, entrance 0, flag 0x35 clear, z cell above 0x1C): message 2, a button, the shake | - | `EnterArea10Body` :412; `Scena10_ArriveHook` :2368 | progress |
| 54 (0x36) | `EnterArea10Body`, :413 | Any area 0x69 entry at entrance 1 | - | nothing found | progress (reading) |
| 55 (0x37) | `Scena10_StepHook`, :2315 | Area 0x78, entrance 0, x 0x58000, z cell 0x11..0x14, a member of kind 2: run 13 step 0, area change into 0x78 | - | `Scena10_StepHook` :2310 | progress |
| 56 (0x38) | `Scena10_Run13` step 0, :1923 | Run 13: area change into 0x78, music 0x7B | - | nothing found | progress |
| 57 (0x39) | `Scena10_Run13` step 1, :1930 | Run 13 step 0 done | - | `EnterArea10Body` :416: area 0x78's drop-ins while 0x3D is clear | progress |
| 58 (0x3A) | `Scena10_Run13` step 5, :1939 | Run 13 from step 5 (object 12): drop-in 3 | - | `EnterArea10Body` :417 | progress |
| 59 (0x3B) | `Scena10_Run13` step 8, :1949 | Run 13 at step 8: `Scenario_CallB(3)`, drop-in 4 | - | `EnterArea10Body` :419; `Scena10_StepHook` :2324 (message 0x3A); `Scena10_ArriveHook` :2377 | progress |
| 60 (0x3C) | `Scena10_Run13` step 0x15, :2002 | After event battle 0x23 (Gaist, Torch; step 0x13, :1998; started by `Scena10_ArriveHook` :2377, area 0x78, entrance 5): area change into 0x78, music 0x49 | - | `EnterArea10Body` :428; `Scena10_StepHook` :2324; `Scena10_ArriveHook` :2377 | progress |
| 61 (0x3D) | `Scena10_Run13` step 0x19, :2036 | Run 13 ends after effect 0x6B | - | `EnterArea10Body` :392 (arms run 1 in area 0x52), :415; `Scena10_StepHook` :2294, 2304 | progress |

Flags 3, 27..31, 48..50, 62 and 63 have no setter, clearer or test in the code
read.

**Order (reading, not checked).** The tests chain the runs: area 0x78's run 13
(55..61) must come before run 1 (61 arms it; 4..6), run 1 before area 0x62's
run 2 (4 then 8), run 2 before area 0x3A (9), and area 0x80's runs 3 and 4
(10, 11, 12, 13, 14, 15, 17) before run 6 (20, 22, 23) and run 7 (18, 24).
Flags 16, 0 and 2 (area 0x4B: the minigame, event battle 0x24) follow flag 15.

## End

`Run7Body` step 0xF (`scena_sc9b.cpp:946-956`): counter 0 cleared, flag 0x12
(18) set, `Field_ChangeArea(0x79, 0x270000, 0x370000, 7)`, master bit 9 of
`0x904657` when flag 0x1A (26) is set (`:952`), then `Field_SetStatus80`
(`:953`). The area entry that follows starts chapter 11
(`mode_flow.cpp:548-556`). Run 7's step 0x14 (`:957-965`) also calls
`Field_SetStatus80`, with `Transition_Start(0xB)` and no area change of its
own.

Run 7 is not started by any code in the chapter's file. Its step 7 leaves it at
step 9, which has no case; something outside the file moves it on to 0xA.
Reading: the field script does both.

**The row as left (reading):**

- Always set: 0, 2, 4..6, 8..11, 13..18, 20..24, 53..61.
- Optional: 1 (a missed minigame round), 7, 25, 52, 32..47 (one per pickup).
- Alternatives: 19 (object 6 or object 7, both set it); 26 (area 0x62's answer).
- Cleared before the end: 12, 51.

**Against the saves.** Saves 20 and 22 are past chapter 10 (chapters 14 and 15)
and hold: 0 2 4..11 13..26 32..44 46..47 52..61. The reading agrees: every flag
there has a setter above, and the always-set list is in it. 1 is clear (no
missed round). 3 has no setter and is clear. 12 and 51 are clear, as read. 7,
19, 25, 26 and 52 are set. Pickup flags 32..47 are set except 45: one of the
16 pickups was not taken.

Save 21 has the chapter byte 10 and the same row without 59..61: 0 2 4..11
13..26 32..44 46..47 52..58. In the reading that is a point inside run 13,
before its step 8. But save 21's rows for chapters 11..14 and its event flags
are the same as save 20's, so it is not a plain save from inside chapter 10.
See Unresolved.

## Event flags used by this chapter

Event flags are `Cond_Flags + 0xA0` (`0x904030`):

| Event flag | Op | Site |
|---:|---|---|
| 79 (0x4F) | set | `EnterArea10Body` :412: area 0x69, entrance 0, chapter flag 0x35 clear |
| 79 (0x4F) | cleared | `Scena10_Run12` step 2, :1866 |
| 67 (0x43) | set | `Scena10_ArriveHook` :2362, with chapter flag 9 |
| 65 (0x41) | set | `Area75_TailPresses` state 2, `area_w1f.cpp:641` (area 0x4B, area code) |
| 65 (0x41) | cleared | `Area75_PhaseReached`, `area_w1f.cpp:973` |

In saves 20..22, 67 is set and 65 is clear, as read. 79 is set there although
run 12 clears it and the entry sets it only while chapter flag 0x35 is clear:
something later sets it again. Not found.

**Other bit arrays.** `Run7Body` step 0xF sets bit 9 of `0x904657` (the
masters' introduced bits, [`../rest_2c.md`](../rest_2c.md) line 119) when
chapter flag 26 is set. `Area98_ChoiceFlag9` sets bit 9 of `0x904654` (the
masters' met bits, `rest_2c_callees.h:83`).

## Unresolved

- **Run starters outside the file.** Run 5 is started by `Area75_PhaseReached`
  (`MoveScript_Var7` 5, `area_w1f.cpp:974-990`); [`../boss-fights.md`](../boss-fights.md)
  row 36 says "no starter in chapter 10's code (a field script)", and this is
  the starter. Run 7 (the chapter's end) has no starter found. Steps that wait
  for an outside move: run 5 step 7 to 8 (after battle 0x24), run 6 step 0x14
  to 0x15, run 7 step 9 to 0xA and step 0x14, run 13 step 8 and step 0x14 to
  0x15 (after battle 0x23). Needs a run.
- **The chapter's first area.** Not in the code.
- **Flag 4's setter.** Effect sub-kind 0x24 sets it at counter 0 = 5 in any
  chapter, since it writes the fixed row. That run 1 places this effect is a
  reading of the counters, not checked; sub-kind 0x3F's scene also drives
  sub-kind 0x24 ([`../effect_5e.md`](../effect_5e.md) line 31).
- **Flag 26** comes from `Area98_ChoiceFlag9` through the current row. Reading:
  area 0x62 is visited only by chapter 10's code, so the row is chapter 10's.
  Not checked.
- **Area 0x5E's choices** (`Area94_ChoiceSetFlag15`, `Area94_ChoiceRunByFlags16`,
  `Area94_ChoiceRunByFlag20`, `Area94_GiveKeyItem7`, `area_w2c.cpp:586-682`)
  write flags 21, 22, 30 of the current row and start runs 7 and 9 at steps 0,
  0x19, 0x32 and 2. Chapter 10 visits area 0x5E (`Run6Body` :808), but its run
  table has run 9 as 0 and its run 7 has no step 0x19 or 0x32. Chapter 6's run 7
  has both (`scena_sc6.cpp:975`). Reading: these are chapter 6's, and they are
  left out of the table.
- **Save 21.** Chapter byte 10, chapters 11..14 filled, and chapter 10 without
  59..61. Nothing in the code read clears 59..61, and `Scenario_Start` clears
  only flags 0..31. How a save reaches this state is not known: a later return
  to chapter 10, or an edited save. Needs the save's history.
- **Event scripts.** Event conditions in the game data can test any chapter
  row. Those tests are not in our source.
- **The extractor** (`tools/story_flags.py`) cannot give a chapter to the
  helpers without a prefix: the sites in `EnterArea10Body`, `Run6Body` and
  `Run7Body` (flags 12, 18, 20, 22..24, 54 and others) show only under
  `file scena_sc9b.cpp`, row "cur". It misses `Scena10_Object09` (25), and it
  gives `Area98_ChoiceFlag9` (26) to no chapter. The
  pickup flags are a non-literal `B(rec) + 0x20`; the 16 records' `+0` bytes
  are 0..15 in `BOF3.exe` at `0x661528` (read 2026-11-09), so the flags are
  32..47.
