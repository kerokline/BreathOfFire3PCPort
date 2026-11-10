# Story flags: chapter 14

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Source: `src/game/scena_sc13.cpp` (the `Scena14_*` functions, lines 1412..2926),
and the area and effect handlers that write or test row 14: `area_011.cpp`,
`area_w1c.cpp`, `area_w3g.cpp`, `area_w4b.cpp`, `area_w4e.cpp`, `area_w4f.cpp`
and `effect_6c.cpp`. Row 14 is `Cond_Flags + 0x70` (the helper `Bank904000()`
in the area files), save block `+0x620`. Areas are given in hex as the code has
them, with decimal in brackets the first time. "Zone" is `Cond_ByteFD`. "Run r
step s" is `MoveScript_Var7` = r and the step byte `MoveScript_PartyRecords[0x65]`
= s.

## Start

**Who starts it.** `Scena13_Run8` step `0xB` (`scena_sc13.cpp:1232-1238`) sets
chapter 13's flag 30 and calls `Field_SetStatus80`. That is after its
`ChangeArea(0x97, ...)` has finished. So the next area change adds 1 to the
chapter byte and calls `Scenario_Start(14)` (`mode_flow.cpp:548-555`). Reading,
not checked. Which area that is the code does not fix: it is whatever the
player enters next after area `0x97` (151).

**What `Scenario_Start` does to the row** (`title_states.cpp:404-422`): it zeroes
the row's first dword only (flags 0..31) and points `Chapter_FlagRow` at it.
Flags 48, 49 and 53 are in the second dword and are not cleared. Reading, not
checked: on a normal route they are still 0, because nothing earlier writes row
14.

**State 0 is `ScenaShared_State0`** (`:1424`), shared by eight chapters. It only
sets state 1. It sets no flag and starts no run.

**State 1, `Scena14_EnterArea`** (`:1491-1622`) acts by area. The first flag a
new chapter 14 sets is likely **flag 0**, on the first entry to area `0xC1`
(193) (`:1601-1605`): call-table A entry 0 and `Party_DropIn(0)`. Reading, not
checked. Area 193's choice `Area193_ChoiceRunOnYes` (answer 1,
`area_w4f.cpp:441-446`) then starts run 1 at step 0.

## Flags

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `EnterArea14Body`, `scena_sc13.cpp:1602` | First entry to area `0xC1` (193): call-table A entry 0, `Party_DropIn(0)` | - | `EnterArea14Body` `:1601` | once-only set-up |
| 1 | `Scena14_Run1` step 1, `:1645` | Before `ChangeArea(0xBD, ...)` (189), with `Field_StatusBits` bit 0 | `Scena14_Run1` step 7, `:1692`, after messages `0x12..0x14`, back to area `0xC1` | `EnterArea14Body` `:1577`: in area `0xBD`, `Effect_Objects[0].flags |= 0x40` | transient |
| 2 | `Scena14_Run1` step `0x19`, `:1707` | With flag 3 and event flag `0x8A`, then `ChangeArea(0xC1, ..., 0x84)` | - | `Area192_TalkMessage` (`area_w4f.cpp:346`) and `Area191_TalkMessage` (`area_w4e.cpp:1057`), both by the current row: areas 192's and 191's lines come from another table | choice (against step `0x14`) |
| 3 | `Scena14_Run1` step `0x19`, `:1708` | As flag 2 | `Area150_Tail58` state `0x16` (`area_w3g.cpp:898`) and `Area193_Tail57` state `0x16` (`area_w4f.cpp:563`) | Those two tails at state `0x14` (`area_w3g.cpp:882`, `area_w4f.cpp:547`): message `0x25` while set, else `0x20` | transient, choice |
| 4 | `Scena14_Run2` step `0xA`, `:1829` | `ChangeArea(0xBF, ...)` (191). This is after event battle `0x31` (49, Manmo in `boss-fights.md`) at step 8 (`:1824`). Run 2 step 0 comes from `Scena14_LeaveToC4` (`:1945-1954`, change to area `0xC4` (196)) | - | `Area189_StepArrive` (`area_w4e.cpp:670-675`): in area 189, while clear, a walk into block x `0x1600`/`0x1700`, z at most `0x1000` calls `Scena14_LeaveToC4`. `Area189_LeaderControl` (`area_w4e.cpp:517`): while set, a walk into another block shows message 3 and halts. `EffectKind18Sub44_Draw` (`effect_6c.cpp:721`): a second glow is drawn while set | progress |
| 5 | `Scena14_Run2` step `0xA`, `:1830` | With flag 4 | `EnterArea14Body` `:1592`: area `0xBF` zone 1 with 5 and 8 set (flag 9 set there too) | `EnterArea14Body` `:1579`, `:1590`. `Scena14_StepHook` `:2854`: area `0xBF`, a cell of byte `0xA6` starts run 2 step `0x14` (message `0x7D`) while set. `Area191_Init` (`area_w4e.cpp:1078`): with chapter byte 14 and the flag set, area 191's set-up is skipped | transient |
| 6 | `Scena14_Run2` step `0x11`, `:1870` | After message `0x70` in area `0xBF`. Character 8's record `+0x1E` = 9 | - | `EnterArea14Body` `:1580`: area `0xBF` zone 0 opening set-up while clear | progress |
| 7 | `EnterArea14Body`, `:1586` | Area `0xBF` zone 0 with 5 and 6 set: `Party_DropIn(2)` | - | `EnterArea14Body` `:1585` | once-only set-up |
| 8 | `Scena14_Run2` step `0x19`, `:1886` | `Inventory_Add(0, 0x5A, 1)`. Run 2 step `0x19` starts from `Area191_MemberNextRun` (`area_w4e.cpp:873-879`) | - | `EnterArea14Body` `:1590`. `Scena14_ArriveHook` `:2922` | progress |
| 9 | `Scena14_Run2` step `0x1D`, `:1892`, or `EnterArea14Body` `:1591` | The arrive hook in area `0xBF` zone 0 (`:2919-2925`, 8 set and 9 clear), or entry to area `0xBF` zone 1 with 5 and 8 set | - | `Scena14_ArriveHook` `:2923`. `Area189_LeaderProject` (`area_w4e.cpp:461`): with 9 set and 10 clear, area 189's leader effect | progress |
| 10 | `Scena14_Run3` step 1, `:1966` | Area `0x96` (150). Run 3 comes from run 2 step `0x24` (`:1932-1933`) | - | `EnterArea14Body` `:1539`. `Area192_TalkMessage` (`area_w4f.cpp:338`) and `Area192_Init` (`:375`), `Area191_TalkMessage` (`area_w4e.cpp:1049`) and `Area191_Init` (`:1082`): areas 192's and 191's lines and set-ups. `Area189_LeaderProject` (`area_w4e.cpp:461`) | progress |
| 11 | `Scena14_Run3` step 5, `:1990` | In area `0x96` | - | `EnterArea14Body` `:1546` | progress |
| 12 | `Scena14_Run3` step `0xA`, `:2022` | After message `0x15` and request 6 | - | Nothing found in code | progress |
| 13 | `EnterArea14Body`, `:1608` | First entry to area `0xC5` (197): `Party_DropIn(1)`, run 5 step 0 | - | `EnterArea14Body` `:1607` | once-only set-up |
| 14 | `Scena14_Run5` step 6, `:2221` | Run 5 step 5, from `Scena14_Object05` (`:2814-2818`): `Kind2_Place(1)` | - | `EffectKind18Sub55_Wait` (`effect_6c.cpp:1067`): the effect moves on once set | progress |
| 15 | `Scena14_Run5` step `0x10`, `:2276` | After event battle `0x32` (50, Chimera; `:2269`) | - | `EnterArea14Body` `:1613`: area `0xC5` with byte `0x905E68` = 5 starts run 5 step `0xA` while clear | progress |
| 16 | `Scena14_Run4` step 0, `:2080` | `Scena14_StepHook` `:2851` (area `0x94` (148) zone 4), then `ChangeArea(0x94, ...)` | - | `Scena14_StepHook` `:2851` | progress |
| 17 | `Scena14_Run4` step `0x18`, `:2188` | End of run 4: areas `0x95` (149) and `0xA7` (167), messages 1 and 2. Run 4 step `0xA` comes from `Scena14_ArriveHook` `:2909` (area `0x94` zone 0) | - | `Scena14_ArriveHook` `:2909`. `Area149_Init` (`area_w3g.cpp:785`): from area `0x94`, tail kind 33 is armed while set | progress |
| 18 | `Scena14_Run6` step 7, `:2434` | Run 6 step 0, from `Area172_ChoiceFlag12` (answer 0, flag clear, `area_w4b.cpp:1033`), in area `0xAC` (172) | - | `EnterArea14Body` `:1555`. `Area172_ChoiceFlag12` (`area_w4b.cpp:1028`): with the flag set, tail kind 35 instead | progress |
| 19 | `Scena14_Run6` step `0xE`, `:2474` | Before `ChangeArea(0xB, ...)` (11). Run 6 step `0xA` comes from `Scena14_ArriveHook` `:2916` (area `0xAC` zone 0) | - | `Pending1314` (`:1487`): set with 20 clear, the entries to areas `0xB`, `0x2B` (43), `0x67` (103), `0x78` (120), `0x90` (144) and `0xAC` (`:1492-1536`, `:1561`). `Area11_DimBackdrop` (`area_011.cpp:72`) and `Area51_TintBackdrop` (`area_w1c.cpp:717`) | progress |
| 20 | `Scena14_Run6` step `0x46`, `:2582` | After run 6's visits through areas `0x33`, `0x43`, `0x55`, `0x2B`, `0x67`, `0x78`, `0x90` and `0xAC`, in area `0x8D` (141) | - | As flag 19 (the closing test). `EnterArea14Body` `:1507`, `:1562`. `EffectKind18Sub51_Wait` variant 0 (`effect_6c.cpp:913`) | progress |
| 21 | `Scena14_Run7` step 0 (`Farewell`), `:2648` | `Scena14_ArriveHook` `:2882`, area `0x8D` zone 0: `Party_DropIn(1)` | - | `Scena14_ArriveHook` `:2881` | progress |
| 22 | `Scena14_Run7` step 1, `:2649` | The same, next rectangle: `Party_DropIn(2)` and an area set-up | - | `Scena14_ArriveHook` `:2883` | progress |
| 23 | `Scena14_Run7` step 2, `:2650` | `Party_DropIn(3)` and an area set-up | - | `Scena14_ArriveHook` `:2885` | progress |
| 24 | `EnterArea14Body`, `:1515` | First entry to area `0x8D` zone 1: `Party_DropIn(4)` and an area set-up | - | `EnterArea14Body` `:1514` | once-only set-up |
| 25 | `Scena14_Run7` step 3, `:2651` | `Scena14_ArriveHook` `:2891`, area `0x8D` zone 1 with event flag `0x7A` set: `Party_DropIn(6)` | - | `Scena14_ArriveHook` `:2890` | progress |
| 26 | `Scena14_Run7` step 4, `:2652` | As 25, next rectangle: `Party_DropIn(7)` | - | `Scena14_ArriveHook` `:2892` | progress |
| 27 | `EnterArea14Body`, `:1522` | First entry to area `0x8E` (142): call-table A entry `0xA` | - | `EnterArea14Body` `:1521` | once-only set-up |
| 28 | `Scena14_Run7` step 5, `:2653` | `Scena14_ArriveHook` `:2898`, area `0x8E` zone 1: `Party_DropIn(0)` | - | `Scena14_ArriveHook` `:2898` | progress |
| 29 | `EnterArea14Body`, `:1526` | First entry to area `0x8E` zone 2: `Party_DropIn(1)` | - | `EnterArea14Body` `:1525` | once-only set-up |
| 30 | `EnterArea14Body`, `:1530` | First entry to area `0x8E` zone 3: `Party_DropIn(2)`, run 7 step 6 | - | `EnterArea14Body` `:1529` | once-only set-up |
| 31 | `Scena14_Run7` step `0x1A`, `:2758` | After event battles `0x33` (51, Arwan; `:2689`) and `0x34` (52, D>Lord; `:2753`), then `ChangeArea(0xAC, ...)`. Run 7 step `0xA` comes from `Scena14_ArriveHook` `:2899` (area `0x8E` zone 3) | - | `Scena14_ArriveHook` `:2899`. `EnterArea14Body` `:1566`: the next set-up of area `0xAC` | progress |
| 48 (`0x30`) | `EnterArea14Body`, `:1571` | Area `0xAC` zone 0 with 18, 19, 20 and 31 set: call-table A entry `0xC`, `Party_DropIn(0xB)`, effect `0x98` | - | `EnterArea14Body` `:1568` | once-only set-up |
| 49 (`0x31`) | `Scena14_Run7` step `0x20`, `:2779` | Run 7 step `0x1E`, from `Scena14_Object07` (`:2831-2837`): stream 2, message `0x2A`. The chapter's end, below | - | Nothing found in code | progress |
| 53 (`0x35`) | `EnterArea14Body`, `:1537` | First entry to area `0x93` (147) | - | `EnterArea14Body` `:1537` | once-only set-up |

**Run 1's two exits.** Steps `0x14` and `0x19` share one body (`:1699-1719`).
Step `0x14` clears event flag `0x77` and changes to area `0xC1` with flags `0x83`.
Step `0x19` sets flags 2 and 3 instead and changes with flags `0x84`. Both set
event flag `0x8A`. Step `0x14` is started by three answers:
`Area191_ChoiceRun1` (answer 0, `area_w4e.cpp:1112-1119`), `Area191_ChoiceTail53`
(answer 2, `:850-861`) and `Area192_ChoiceArmTail54` (answer 2,
`area_w4f.cpp:205-213`). No code was found that starts step `0x19`.

## End

`Scena14_Run7` step `0x20` (`:2776-2781`) calls `Field_SetStatus80` (`:2778`) and
sets **flag 49**. It changes no area itself, so chapter 15 starts on the next area
change (`mode_flow.cpp:548-555`). Reading, not checked. Step `0x1A` (`:2756-2761`)
has already set flag 31 and changed to area `0xAC`. There `EnterArea14Body` sets
flag 48 (`:1566-1574`).

**The row as left**, by this reading:

- Always set: 0, 4, 6..31, 48, 49.
- Once-only set-ups that the route meets: 0, 7, 13, 24, 27, 29, 30, 48, and 53
  (if area `0x93` is entered).
- Alternatives: flag 2 with flag 3 (run 1 step `0x19`) against neither (step
  `0x14`). Flag 3 is cleared again by area 150's or 193's tail, so only flag 2
  would stay.
- Cleared before the end: 1, 3, 5.

**Against the saves.**

- Save 22 (43:25, chapter 15) has row 14 = **0 4 6..31 48..49 53**. That is
  exactly the "always set" list plus 53: 1, 3 and 5 are clear, and 2 is clear, so
  that player took run 1's step `0x14` exit.
- Save 20 (42:37, inside chapter 14, area 141 = `0x8D`) has **0 4 6..24 53**. That
  agrees with the order read here. The three farewells in area `0x8D` zone 0
  (21..23) and the zone 1 entry (24) are done. Flags 25 and 26 wait for event flag
  `0x7A` (122). Save 20 does not have 122, and save 22 does.
- The saves disagree with nothing in the reading.

## Event flags used by this chapter

The bank at `Cond_Flags + 0xA0` (block `+0x650`), from `Scena14_*` functions:

| Event flag | Site | Use |
|---:|---|---|
| 119 (`0x77`) | `Scena14_Run1` step `0x14`, `:1703` | Cleared |
| 138 (`0x8A`) | `Scena14_Run1` steps `0x14` / `0x19`, `:1712` | Set. Area 150's and 193's tails clear it at state `0x16` (`area_w3g.cpp:899`, `area_w4f.cpp:564`) |
| 70 (`0x46`) | `Scena14_Run4` step `0xE`, `:2114` | Set, before `ChangeArea(0x95, ...)` |
| 78 (`0x4E`) | `Scena14_Run6` step 1, `:2401` | Set, before `ChangeArea(0xAC, ...)` |
| 122 (`0x7A`) | `Scena14_ArriveHook`, `:2889` | Tested: area `0x8D` zone 1 farewells (flags 25, 26) |

In save 20, events 70, 78, 119 and 138 are all clear, though runs 1, 4 and 6 had
run (row 14 flags 16..18 are set). So something clears 70 and 78 later.
Candidates in code are `Area146_ClearFlag46` (`area_w3f.cpp:1053`) for 70 and
`Area171_Trigger55` (`area_w4b.cpp:836`) for 78. Not traced. `EffectKind18Sub51_Wait`
(`effect_6c.cpp:914`) also tests 122..124 (`0x7A..0x7C`) by variant.

## Unresolved

- **Run 1 step `0x19`** (flags 2 and 3): no starter was found in code. It may be
  started by an event script in the game data, or not at all. Flag 3's two
  testers (`area_w3g.cpp:882`, `area_w4f.cpp:547`) and flag 2's
  (`area_w4f.cpp:346`) show that the path was meant to be reachable. A run is
  needed.
- **The chapter's first area.** It is the area entered after area `0x97` in
  chapter 13. Flag 0 on area `0xC1` is the earliest setter found. Not checked.
- **`Area192_TalkMessage` and `Area191_TalkMessage` read `Chapter_FlagRow`** for
  flag 2 (`area_w4f.cpp:346`, `area_w4e.cpp:1057`) but row 14 by address for flag
  10. They read flag 2 as chapter 14's only when they run in chapter 14. Reading.
- **`tools/story_flags.py` misses `area_w4e.cpp`**: its helper `Row14()`
  (`area_w4e.cpp:49`) is not recognised. The six row-14 tests there (flags 4, 5,
  9, 10) were found by reading and are in the table.
- **Flags 12, 49, 53 are not tested in code.** Event scripts can test chapter rows
  (`Area_TestCondition`), and call-table A and `Party_DropIn` run scripts. Not
  read.
- **Steps with no case**: run 2 step 9 (after battle `0x31`), run 5 step `0xF`,
  run 7 steps `0xE`, `0x13` and `0x19`. Each is moved on outside this file (battle
  end or an effect). Not traced.
- **Current-row testers of unknown chapter**: `EffectKind77_Tick` / `_End`
  (`effect_3d.cpp:319`, `:333`), `EffectKindA4_MarkSprites` (`effect_4b.cpp:1030`)
  and `BossMap_UpdateFromEnemies` (`boss_h.cpp:355`). They test `Chapter_FlagRow`.
  Their comments tie them to other chapters, and none of them names an area of
  chapter 14. Not checked.
