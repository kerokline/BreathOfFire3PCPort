# Story flags: chapter 2

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Source: `src/game/scena_sc2.cpp` (the `Scena02_*` functions). Chapter 2's row is
`Cond_Flags + 0x10` (`0x903FA0`, save block `+0x5C0`). The file's wrappers are
`Bank()` (the pointer `Chapter_FlagRow`), `Flag(n)`, `SetFlag(n)` and
`ClearFlag(n)` (`scena_sc2.cpp:84-87`). Other files that reach this row in
chapter 2: `area_w0b.cpp` (area 26's handlers, through `Chapter_FlagRow`),
`effect_5a.cpp` (row 2 by address) and `scena_sc9b.cpp` (one object handler
shared with chapter 9).

Areas are given in hex as the code has them: `0x1A` is area 26, `0x1B` 27,
`0x1C` 28, `0xF` 15, `0xD` 13, `0xB` 11, `0x5A` 90, `0x10` 16. Battle names
are from [`boss-fights.md`](../boss-fights.md).

A run is started in four ways: by `Scena02_EnterArea` on entry to an area, by
the step hook `Scena02_StepHook` (rectangles of the leader's position), by an
object handler `Scena02_ObjectNN` (object `+0x86` = NN, through
`Scena02_ObjectHook`, `scena_sc2.cpp:2310-2316`), or by `Scena02_Scene00`'s
near tests in area `0x1A`. The run number is `MoveScript_Var7` and the
counters `0x903848..B` are script counters, so the area event scripts (game
data) can also start runs and drive steps. Reading, not checked.

## Start

**Who starts it.** Chapter 1 ends in `Scena01_Scene0E` step 9
(`scena_sc1.cpp:1523-1529`): the change to area `0xF` at `(0x360000,
0x500000)`, then `Field_StatusBits |= 0x80` (`0x56D6F0`). `Area_Enter` sees
bit 7 and calls `Scenario_Start(chapter + 1)` = `Scenario_Start(2)`
([`mode-flow.md`](../mode-flow.md) section 4, item 9). `Scenario_Start`
(`title_states.cpp:404-424`) zeroes the scenario bytes `0x8034E0..E9` (so also
`Field_StatusBits`), zeroes the **first dword** of row 2 (flags 0..31) and
points `Chapter_FlagRow` at row 2 (`:415-417`).

**`Scena02_Start`** (state 0, `scena_sc2.cpp:215-219`) sets state 1, zeroes the
first dword of row 2 again (flags 0..31) and the four counters. It sets no flag.
Flags 32..63 are cleared by neither function.

**First area entry.** `Scena02_EnterArea` in area `0xF` with flag 0 clear
(`:303-316`): flag 0 set, `Field_StatusBits |= 1`, script flags `0x80` and
`0x30`, the counters cleared, run 2 at step 0. Run 2 (`Scena02_Scene02`,
`:494-601`) changes to area `0xF` at the same place again (step 0), then sets
flag 4 on counter 0 = 8 (step 2, `:505-512`).

The party at start is whatever chapter 1 left. Members 1 and 2 are dropped in by
run 4 steps 5 and 7 (`:623-656`), which also set flags 5 and 6 and change to
area `0x1A`.

## Flags

Line numbers are `scena_sc2.cpp` unless a file is named.

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena02_EnterArea`, area `0xF`, `:303-304` | First entry to area `0xF` in this chapter; starts run 2 | - | Same site: run 2 starts once | once-only set-up |
| 1 | `Scena02_Scene10` step `0x10`, `:1427` | Run `0x10` (area `0x1C`): after member 2 dropped in (step `0xF`, started by `Scena02_Object10`, `:2396`), `Scenario_CallA(0)`, change to area `0x1C` | - | Nothing in this file | progress |
| 2 | `Scena02_EnterArea`, area `0x5A`, `:402-403` | Entry to area `0x5A` with `Field_StatusBits` bit 0 set: music `0x4D` | `Scena02_EnterArea`, area `0x10`, `:419`, when bit 0 set and the last area was `0x5A` (music `0x11`) | `:402`: the music change runs once per visit | transient |
| 3 | `Scena02_EnterArea`, area 0, `:229` | Entry to area 0 with flag 25 set | - | Nothing in this file | progress |
| 4 [0x4] | `Scena02_Scene02` step 2, `:512`; `Scena02_Scene04` step 1, `:621` | Counter 0 = 8 after the change to area `0xF` | - | Nothing in this file | progress |
| 5 | `Scena02_Scene02` steps 6, 8, `:525`, `:541`; `Scena02_Scene04` steps 6, 8, `:634`, `:651` | Member 1 (step 5) or 2 (step 7) dropped in, counter 0 = `0x22`; change to area `0x1A` | - | `Scena02_Object04` / `05`, `:2338`, `:2347`: with 5 clear, an object starts run 4 at step 5 or 7 | progress |
| 6 | Same four sites as 5, `:526`, `:542`, `:635`, `:652` | With flag 5 | - | Nothing in this file | progress |
| 7 | `Scena02_Scene08` step 3, `:820` | After event battle `0xE` (Rocky, step 2); member `0x12` dropped in, change to area `0x1A` | - | `Scena02_Scene00` `:465`, `:476` (which near tests run); `Scena02_Scene06` step 3 `:708` and `Scena02_Scene07` step 4 `:763` (run `ResetFlags7`); `EnterArea` area `0x1A` counter 2 = 1 `:339`; `StepHook` `:2573` (run 8 start) and `StepHook1A` `:2499` (run 9 start) | progress |
| 8 | `Scena02_EnterArea`, area `0x1B`, `:361-366` | First entry to area `0x1B` with counter 2 = 0: the counters cleared | - | Same site only | once-only set-up |
| 9, 10, 11 [0x9..0xB] | `Scene08Cell` (`:776-787`) from `Scena02_Scene08` step 8, `:858-860` | Leader (first member 0) on one of three cell pairs; counter 3 = 1, 5, `0xA`; steps 9..`0xB` | `ResetFlags7`, `:189-191` | `Scene08Cell` `:777`: each cell fires once | optional |
| 12 [0xC] | `Scena02_Scene08` step 4, `:827` | Counter 0 = `0x31` after step 3; run 8 ends | - | `EnterArea` area `0x1A`, counter 2 = 3, `:347`: step `0xF` armed only while clear | progress |
| 13 [0xD] | `Scena02_Object09`, `:2358` | An object with `+0x86` = 9 fires | - | Nothing found | unknown |
| 14 [0xE] | `Scena09_Object07` (`0x557170`, `scena_sc9b.cpp:980-981`), as `Scena02_ObjectHandlers` entry `0xA` | An object with `+0x86` = `0xA` fires; it sets flag `0xE` of the current row | - | `Area12_WaitFlagE` (`area_w0a.cpp:411-415`) waits on flag `0xE` of the current row; whether it runs in chapter 2 is not known | unknown |
| 15 [0xF] | `Scena02_Scene0A` step 3, `:1002` | Run `0xA` (area `0x1B`): member 3, change to area `0x1B`, counter 0 = 9 | - | `StepHook` area `0x1B`, `:2598`: with 2 members and 15 clear, run `0xA` step 0 | progress |
| 16 [0x10] | `Scena02_Scene0A` step `0xA`, `:1032` | After event battle 8 (step 8, `:1022`) and member `0x12` | - | `StepHook` `:2603`: run `0xA` step 5 (the battle) while clear | progress |
| 17 [0x11] | `Scena02_Scene0C` step 4, `:1160` | After event battle 9 (`:1150`) and member `0x15` | - | `StepHook` `:2610`: run `0xC` while clear | progress |
| 18 [0x12] | `Scena02_Scene0D` step 6, `:1207` | After event battle `0xA` (`:1192`) and member `0x16` | - | Nothing in this file | progress |
| 19 [0x13] | `Scena02_Scene0D` step 6, `:1208` | With 18 | - | `StepHook` `:2617`: run `0xD` while clear | progress |
| 20 [0x14] | `Scena02_Scene0E` step 8, `:1268` | After event battle `0xB` (`:1245`); the second change to area `0x1B` | - | `StepHook` `:2627`: run `0xE` while clear | progress |
| 21 [0x15] | `Scena02_Scene10` step 7, `:1412` | Run `0x10` step 0..7 in area `0x1C` (two changes, transition 1, Kind2 0) | - | `EnterArea` area `0x1C`, counter 2 = 0, `:382`: run `0x10` step 0 while clear | progress |
| 22 [0x16] | `Scena02_EnterArea`, area `0x1C`, `:393` | Entry to area `0x1C` with counter 2 = 2 (set by run `0x10` step `0x15`, `:1459`); `MoveCmd_TestFB(0x27, 0x15)` | - | Nothing in this file | progress |
| 23 [0x17] | `Scena02_Scene0E` step 3, `:1239` | Counter 0 = `0x42`, before the party is placed for event battle `0xB` | - | `EffectKind18_04_Start` / `_Shut`, `effect_5a.cpp:218`, `:232`: the two panels open | progress |
| 24 [0x18] | `Scena02_Scene0F` step `0xA`, `:1349` | Run `0xF` step `0xA` (area `0x1B`): member `0x11` dropped in | - | `StepHook` `:2634`: run `0xF` step `0xA` while clear | progress |
| 25 [0x19] | `Scena02_EnterArea`, area 5, counter 2 = 6, `:271` | Entry to area 5 after run `0x11` step `0x1C` (counter 2 = 6, `:1598-1600`); run `0x11` resumes at step `0x1D` | - | `EnterArea` area 0 `:229` (sets 3), area 5 `:265`; `StepHook` area `0xF` `:2570` (run 5, message `0x3E`, while clear); area `0x1C` `:2641` (run `0x11`, while clear) | progress |
| 26 [0x1A] | `Scena02_Scene12` step 1, `:1624` | Run `0x12` (started by `Scena02_Object11`, `:2402`): member 2 dropped in | - | `EnterArea` area `0xD`, `:292`: with 26 set, 27 clear and `Cond_ByteFD` 1, run `0x12` step 5 | progress |
| 27 [0x1B] | `Scena02_Scene12` step 6, `:1647` | After the change to area `0xD` (step 5), counter 0 = `0x11` | - | `EnterArea` area `0xD`, `:292` | progress |
| 28 [0x1C] | `Scena02_Scene12` step 6, `:1645` | With 27 | - | Nothing in this file | progress |
| 29 [0x1D] | `Scena02_Scene13` step `0xA`, `:1677` | Run `0x13` (area `0xB`), the change to area `0xB` | - | `EnterArea` area `0xB` `:279` (run `0x13` step `0xA` while clear); area 3 `:231` (run `0x15` set-up) | progress |
| 30 [0x1E] | `Scena02_Scene13` step `0x11`, `:1725` | After event battle `0xD` (Balio, Sunder, `:1717`); change to area 3 | - | Nothing in this file | progress |
| 31 [0x1F] | `Scena02_Scene15` step 1, `:1751` | Run `0x15` in area 3 (members 3 and 4's items handed back, `:233-235`), party restored | - | `EnterArea` area 3 `:232`; `StepHook` area 3 `:2558` | progress |
| 32 [0x20] | `Scena02_Scene15` step 7, `:1773` | The chapter's end (see End) | - | `EnterArea` area 3 `:243`; `StepHook` area 3 `:2558` | progress |
| 35 [0x23] | `Scena02_Scene17`, `:1788` | Run `0x17` (started by `Scena02_Object15`, `:2424`): money `0x32` given (`0x591BE0`), sound `0x106` | - | Nothing found | optional |
| 36 [0x24] | `Scene18Pay`, `:1807` | Run `0x18`: the payment of `0x32` (`0x591BC0`) succeeded | `Scena02_Scene06` step 3 `:707`; `Scena02_Scene07` step 4 `:762` | `StepHook1A` `:2517`: while set, the payment rectangle is skipped | optional |
| 37 [0x25] | `Scene18Pay`, `:1808` | With 36 | - | `StepHook1A` `:2518`: run `0x18` starts at step `0xA` (member `0xD`) instead of step 0 | optional |
| 38 [0x26] | `Scena02_Scene19` step `0xA`, `:1906` | Run `0x19` step `0xA` (started by `Scena02_Object17`, `:2438`): `Inventory_Add(4, 2, 1)` | - | Nothing found | optional |
| 39 [0x27] | `Scena02_Scene19` step 0, `:1896` | Run `0x19` step 0 (started by `Scena02_Object16`, `:2429`): item `(4, 2, 1)` taken (`0x591B60`) | - | Nothing found | optional |
| 40 [0x28] | `Area26_Flag28Set`, `area_w0b.cpp:965` | Area 26's choice 4 / handler 2 (event script data) | `Area26_Flag29Set28Clear` `area_w0b.cpp:960`; `Area26_Flag28Clear` `:967` | `Scena02_Scene00` `:463`: the near test `Scena02_Near02` on sprite 0 (starts run 7) | choice |
| 41 [0x29] | `Area26_Flag29Set28Clear`, `area_w0b.cpp:959` | Area 26's choice 2 / handler 0 | `Area26_Flag29Clear` `area_w0b.cpp:963` | `Scena02_Scene00` `:464`: `Scena02_Near01` on sprite 0 | choice |
| 43 [0x2B] | `Area26_Flag2BSet`, `area_w0b.cpp:969` | Area 26's choice 6 / handler 4 | `Area26_Flag2BClear` `area_w0b.cpp:971`; `ResetFlags7` `:184`; `Scena02_Scene08` steps 4 and 8, `:829`, `:867` | `Scena02_Scene00` `:466`, `:469`: `Scena02_Near03` on sprite 5 | transient |
| 44 [0x2C] | `Area26_Flag2CSet`, `area_w0b.cpp:973` | Area 26's choice 8 / handler 6 | `Area26_Flag2CClear` `area_w0b.cpp:975`; `ResetFlags7` `:185`; `Scena02_Scene08` steps 4 and 8, `:828`, `:868` | `Scena02_Scene00` `:467`, `:470`: `Scena02_Near04` on sprite 5 | transient |
| 46 [0x2E] | `Area26_Flag2ESet`, `area_w0b.cpp:1011` | Area 26's choice 15 / handler 13 | - | `Scena02_Scene00` `:481`: clear, `Scena02_Near0C` on sprite 9 and `Scena02_DoorStart` (run `0x1A` step 0); set, `Scena02_DoorSound` (sound `0x20B`, run `0x1A` step `0x1E`) | progress |
| 47 [0x2F] | `Scena02_Scene1A` step 7, `:1973` | Run `0x1A` step 5: a member by the first member, effect `0x92` | - | `StepHook1A` `:2512`: run `0x1A` step 5 while clear | progress |
| 48 [0x30] | `Scena02_Scene1A` step `0x18`, `:2000` | After event battle `0xF` (Pooch, `:1995`) | - | `StepHook1A` `:2513`: run `0x1A` steps `0xF` (message `0x7F`) and `0x14` (the battle) while clear | progress |
| 49 [0x31] | `Scena02_Object18`, `:2448` | An object with `+0x86` = `0x18` fires | - | Nothing found | progress |
| 50 [0x32] | `Scena02_Scene1A` step `0x1A`, `:2019` | Run `0x1A` step `0x19` (started by `Scena02_Object19`, `:2449`): a member by the leader's `+8` | - | `StepHook` area `0x1A`, `:2572`: with 50 set and 7 clear, run 8 step 0 and the change to area `0x1A` at `(0x70000, 0xD0000)` (event battle `0xE`) | progress |
| 51 [0x33] | `Area26_GiveItem`, `area_w0b.cpp:983` | Area 26's choice 10 / handler 8: `Inventory_Add(4, 3, 1)`, sound `0x106` | - | Nothing found | progress |
| 52 [0x34] | `ResetFlags7`, `:187` | Run 6 step 3 or run 7 step 4 with flag 7 set | `Scena02_Scene08` step 8, `:866` | `Scena02_Scene00` `:468`, `:476`; `StepHook` `:2580` (run 8 step 5); `StepHook1A` `:2498` (run 8 step 6), `:2499` (run 9 only while clear) | optional |
| 53 [0x35] | `Scena02_Scene08` step 8, `:864` | Counter 2 = 3: the three cells of steps 9..`0xB` done | `ResetFlags7`, `:188` | `StepHook` `:2580`; `StepHook1A` `:2498`: run 8's second visit only while clear | optional |
| 54 [0x36] | `Scena02_Scene11` step `0x17`, `:1573` | After event battle `0xC` (Amalgam, `:1547`), member 6 and message `0x29`; change to area 0 | - | Nothing found | progress |
| 55 [0x37] | `Scena02_EnterArea`, area 5, `:272` | With 25 | - | `EnterArea` area 0, `:228`: `Draw_PassFlags = 0x1F` only while clear | progress |
| 56 [0x38] | `Scena02_Scene12` step 6, `:1646` | With 27 | - | Nothing found | progress |
| 58 [0x3A] | `Scene18Refused`, `:1828` | Run `0x18`: the payment failed or was turned down (message `0x6D` or `0x6C`) | - | `Scena02_Scene06` step 5, `:720`: counter 1 = 1 while set (`StepHook1A` `:2533` then starts run `0x18` step `0x14`) | optional |
| 62 [0x3E] | `Scena02_Scene08` step 3, `:821`; step 8, `:865` | With 7; and at the end of run 8's second visit | `ResetFlags7`, `:186` | Nothing found | progress |
| 63 [0x3F] | `Scena02_Scene09` step 7, `:971` | Run 9 (`StepHook1A` `:2499`, flag 7 set, 52 clear): members `0x18` and `0xA`, music `0x20`, `Scenario_CallB(0)`, change to area `0x1B` | - | `Scena02_Scene00` `:461`: all of area `0x1A`'s near and door tests stop | progress |

No site names flags 33, 34, 42, 45, 57, 59, 60 or 61.

**`ResetFlags7`** (`:183-192`) runs at the end of run 6 (step 3, after the choice
`0xBE` of message `0x30` and a change to area `0xF`) and run 7 (step 4, after
member 6 and message `0x16`), only when flag 7 is set. It clears 43, 44, 62, 53,
9, 10, 11 and sets 52. That arms run 8's second visit (`StepHook` `:2580`,
`StepHook1A` `:2498`), whose step 8 sets 9..11 and then 53 and 62, and clears 52,
43 and 44 (`:856-875`). Reading, not checked: this is a loop that can repeat each
time run 6 or 7 ends.

**Order, by the gates** (reading, not checked): 0, 4, 5, 6 (area `0xF` to
`0x1A`); 46..51 in area `0x1A`; 7, 62, 12 (event battle `0xE`); 63 (run 9, to
area `0x1B`); 8, 15, 16, 17, 18, 19, 23, 20, 24 (area `0x1B`, battles 8..`0xB`);
21, 1, 22 (area `0x1C`); 54, 25, 55, 3 (battle `0xC`, areas 0 and 5); 26, 28, 56,
27 (area `0xD`); 29, 30 (area `0xB`, battle `0xD`); 31, 32 (area 3).

## End

**The end.** `Scena02_Scene15` step 7 (`:1771-1777`): on counter 0 = `0xF`, flag
32 set, the script flags `^ 0x80`, the counters, step and run cleared, then
`Field_StatusBits |= 0x80` (`0x56D6F0`, `:1776`). The path to it:

1. `Scena02_Scene13` step `0x11` sets 30 and changes to area 3 (`:1720-1727`).
2. `Scena02_EnterArea`, area 3, with 29 set and 31 clear (`:231-242`):
   `Scenario_CallB(1)`, `Scena02_StripMember(3)` and `(4)` hand back members 3
   and 4's ten bytes at `+0x7E` (`:2464-2472`), run `0x15` step 0. With 32
   clear, music `0xF` loaded (`:243-249`).
3. `Scena02_Scene15` step 1 sets 31 (`:1751`).
4. `Scena02_StepHook`, area 3, 31 set and 32 clear, the leader in
   `x 0x558000..0x568000, z 0x1C0000..0x1D8000`: run `0x15` step 5 and the
   change to area 3 at `(0x330000, 0x140000)` (`:2558-2569`).
5. `Scena02_Scene15` steps 5..7: transition 5, then flag 32 and bit 7.

Step 7 changes no area. `Area_Enter` starts chapter 3 at the **next** area
change, whatever starts it (reading, not checked; `mode-flow.md` section 4 item
9). `Scena03_Start` then clears row 3's first dword (`scena_sc3.cpp:161-164`).
Row 2 is left as it is.

**The row as left** (reading):

- Always set (progress): 0, 1, 3..8, 12, 15..32, 46..51, 54..56, 62, 63.
- Set by object or area-handler runs (35, 36, 37, 38, 39): the code does not
  force them, but every save past chapter 2 has them.
- Alternatives: 40 and 41. `Area26_Flag29Set28Clear` sets 41 and clears 40, so
  at most one is set when that handler ran last. The handlers can also leave both
  clear.
- Optional: 9, 10, 11, 52, 53 (the run-8 loop); 13, 14 (object handlers); 58
  (the payment turned down); 43, 44 (area 26 handlers; cleared again by run 8).
- Cleared before the end on the main path: 43 and 44 (run 8 step 4); 2 when the
  party leaves area `0x5A` for `0x10`.

**Against the observed rows.** Every save past chapter 2 (saves 13..22) has
chapter 2's row `0..1 3..8 12 15..32 35..39 46..51 54..56 62..63`, plus 40 (save
19), 41 (saves 13..17) or neither (saves 18, 20..22). This agrees with the
reading:

- Every progress flag is set. Flags 2, 9..11, 13, 14, 43, 44, 52, 53 and 58 are
  clear in every save.
- 52 and 53 are both clear, so `ResetFlags7` never ran on these routes. Either
  path would leave one of them set.
- The two mid-chapter saves agree with the order above. Save 12 (5:19, area 27)
  has flags up to 24, plus 1, 21 and 22, and does not have 25. Save 11 (4:59, area
  90) adds 25, 54 and 55 (after battle `0xC`), but not 3 or 26..32.
- Flag 2 is clear in save 11, which is in area 90 (`0x5A`). That agrees:
  `Scena02_Scene11` step `0x1C` flips `Field_StatusBits` bit 0 off (`:1599`), and
  flag 2 needs bit 0.
- Save 12 and save 19 are the same playthrough (psxrecomp JP). Flag 40 is set at
  5:19 and still set at 12:48, so the row stays as it was left.

No disagreement found.

## Event flags used by this chapter

None. No `Scena02_*` function and no area 26 handler in `area_w0b.cpp` sets, clears
or tests the event bank `Cond_Flags + 0xA0`. The saves agree: the event flags are
`3 25 27 32` in the last chapter 1 save (save 10) and in both chapter 2 saves
(11, 12). Not covered: the generic effects in `effect_5a.cpp` that read the event
dword (sub-kinds 5, 6 and 9) and event-script conditions (`EventCond_StoryFlag`,
`field_o.cpp:1304`), which take their flag numbers from game data.

## Unresolved

- **Area 26's flag handlers** (40, 41, 43, 44, 46, 51; `area_w0b.cpp:958-1011`)
  run from area 26's choice and handler entries, which the area's event script
  calls. The code does not show what triggers each one. They write the current
  row (`Chapter_FlagRow`, `area_w0b.cpp:952-957`). That they run only in chapter
  2 is a reading, not checked.
- **Flags 13 and 14**: which objects carry `+0x86` = 9 or `0xA` is in the area
  data. Entry `0xA` is chapter 9's `0x557170`. Whether `Area12_WaitFlagE`
  (`area_w0a.cpp:411`) can run in chapter 2 is not known.
- **Runs with no start in this file**: run 2 steps 5 and 7 (the member drop-ins,
  flags 5 and 6), and run 4 steps 0 and 1 (flag 4). Run 2 is started only at
  steps 0, `0x14` and `0x1E`; run 4 only at steps 5 and 7 (`Scena02_Object04` /
  `05`). The event scripts may set the run and step. Not checked.
- **`EffectKind18_07` door flags** (`effect_5a.cpp:167-169`) read their row and
  bit from the tables `kFlagRows7` / `kFlagBits7`. Whether an entry names row 2
  needs those tables read.
- **Data-driven reads**: `Area_TestCondition` (row by `kind & 0x1F`) and
  `MoveScript_Variable(2)` (`move_cmds.cpp:440-441`, flags 0..31 of the current
  row) let area data test chapter 2's flags. Not listed.
- **Flags 32..63 at chapter start**: `Scenario_Start` (`title_states.cpp:416`) and
  `Scena02_Start` (`:217`) clear only flags 0..31. What clears 32..63 on a new
  game is not checked.
- **What the flags mean in play**: whether the payment (36, 37) is needed to go
  on; what 40 and 41 choose (the two rectangles on either side of sprite 0); and
  what the run-8 loop is.
- **Which area change starts chapter 3** after `Scena02_Scene15` step 7.
- **Extractor notes**: `tools/story_flags.py` gives some sites the name of the
  function before them. The flag 13 site (`:2358`) is in `Scena02_Object09`, not
  `Object05`. The flag 49 site (`:2448`) is in `Scena02_Object18`, not
  `Object17`. Every `area_w0b.cpp` site is listed as `Area26_Flag29Set28Clear` or
  `Area26_SpawnEffect`, but each is in its own `Area26_Flag*` /
  `Area26_GiveItem` function.
