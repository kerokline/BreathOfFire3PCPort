# Story flags: chapter 6

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 6's row is `Cond_Flags + 0x30` (`0x903FC0`), save block `+0x5E0`. Main
source: `src/game/scena_sc6.cpp`. Its wrappers (`scena_sc6.cpp:91-98`): `Bits()`
is the current row (`Chapter_FlagRow`), `Flag` / `Set` / `Clr` act on it;
`Story()` is the event flags `Cond_Flags + 0xA0`, with `StoryFlag` / `SetStory` /
`ClrStory`. Several sites call `Flags_Set(bits, n)` on a local `bits = Bits()`,
and three helpers take the flag as an argument: `SetAndEnd0(f)` (`:504`),
`SetAndEnd123(bits, f)` (`:1699`) and `Gift(..., flag)` (`:1906`). Each call of
those is resolved by hand below.

Areas are given in hex as the code has them. 0x5C is area 92 and 0x5E is area 94
in `area_w2c.cpp`; 0x4D is area 77 in `area_w2a.cpp`. Event battle numbers are
the code's hex; `docs/boss-fights.md` numbers them in decimal (0x17 = 23).

## Start

**What starts the chapter.** Chapter 5 sets it up and an area change starts it:

1. `Scena05_Run20` step 6 sets chapter 5's flag 0x32 (`scena_sc5.cpp:1831`).
2. `Scena05_EnterArea`, on entry to area 0x2D with chapter 5's flag 0x32 set and
   0x3E clear, sets chapter 5's flag 0x3E and calls `Field_SetStatus80`, which
   sets bit 7 of `Field_StatusBits` (`scena_sc5.cpp:364-366`,
   `scena_sx.cpp:320`).
3. The next area change sees bit 7, adds one to the chapter byte `Cond_ByteFA`
   and calls `Scenario_Start(6)` (`mode_flow.cpp:548-555`), after the new area is
   loaded and before its placement.

`Scenario_Start` (`title_states.cpp:404-417`) zeroes the scenario bytes
`0x8034E0..E9` (so bit 7 goes too), zeroes **only the first dword** of the row
(flags 0..31), and points `Chapter_FlagRow` at `Cond_Flags + 0x30`. Flags 32..63
of row 6 are not cleared at the start. They are 0 in every observed save inside
chapter 6, so nothing set them earlier.

**State 0.** `Scena06_Start` (`scena_sc6.cpp:245`) calls `Scena06_GuestRecord`
(`:256`), which writes character record 7 (`0x903EEC`): HP, max HP, ATK, DEF,
AGI, INT and the two equipment bytes `+0x12` / `+0x15`, from the eight bytes of
`Scena06_GuestStats`. Then it sets state 1. It sets no flag. It does not pick
an area, a position or a party: the area is the one the triggering area change
went to. Then `Scena06_EnterArea` runs for that area. The code does not say
which area that is (reading, not checked).

**First scene with flags.** Reading, not checked: `Scena06_Run12`, started by
`Scena06_StepHook` in area 0x43 while flag 0 is clear (`:2270`). It holds event
battles 0x17 and 0x18 (`:1505`, `:1559`; fights 23 and 24 in boss-fights.md) and
sets flags 0 and 1. The five saves inside chapter 6 hold exactly flags 0..1 in
row 6 (observed-rows.md saves 13..17).

## Flags

File is `scena_sc6.cpp` unless named. "Step" is the run's step byte. A battle's
end seems to add one to the step (run 8: battle at step 2, step 3 unhandled,
step 4 handled): reading, not checked.

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena06_Run12` step 0xF (`:1565`) | After event battle 0x18; drop-in 3, transition 0xD | - | `Scena06_StepHook` (`:2270`): area 0x43 starts run 12 only while clear | progress |
| 1 | `Scena06_Run12` step 0x11 (`:1578`) | Run 12's end: change into area 0x43, calls by the party selector | - | Nothing in code | progress |
| 3 | `Scena06_Run01` step 2 (`:461`) and step 0xA (`:491`) | Step 2: after the change into area 0x27 (step 1). Step 0xA: the run's end | - | `Scena06_EnterArea` (`:311`): area 0x27's set-up (members' record `+0xB` bit 0 cleared, call B by the selector) runs while clear | progress |
| 4 | `Scena06_Run02` step 1 (`SetAndEnd0`, `:530`); `Scena06_Object04` (`:2071`) | Run 2 from step 0 (object entry 2) after drop-in 3; object entry 4 starting run 2 at step 0xF | - | Nothing in code | unknown |
| 5 | `Scena06_Run02` step 0xB (`:553`) | Run 2 from step 0xA (object entry 3): drop-in 4, 5 or 6 by the lead's direction | - | `Scena06_Object03` (`:2060`): object entry 3 runs once | once-only set-up |
| 7 | `Scena06_Run02` step 0x13 (`:582`) | Run 2 from step 0xF: two effects of kind 0x13, then the end | - | `Scena06_StepHook` (`:2263`): arms the area 0x27 trigger for run 3 | progress |
| 8 | `Scena06_StepHook` (`:2264`) | Area 0x27, the rectangle at z 0x37, flag 7 set: starts run 3 | - | `Scena06_StepHook` (`:2263`): the trigger fires once | progress |
| 9 | `Scena06_Run03` step 0xD (`:665`) | With flag 0xB and `KeyItem_Add(4)` (`:666`), in area 0x35 | - | Nothing in code | progress |
| 10 (0xA) | `Scena06_Run03` step 0xA (`:645`) | The change from area 0x2F into area 0x35 | - | Nothing in code | progress |
| 11 (0xB) | `Scena06_Run03` step 0xD (`:664`) | As flag 9 | - | `Scena06_EnterArea`: area 0x2E starts run 17 (`:334`); area 0x35 call A 0 while clear (`:352`) | progress |
| 12 (0xC) | `Scena06_Object05` (`:2074`) | Object entry 5; that handler does nothing else | - | Nothing in code | unknown |
| 13 (0xD) | `Scena06_Run04` step 6 (`:727`) | After message 0x1A: counter 2 at 1, change into area 0x5E | - | `Scena06_StepHook` (`:2277`): area 0x5E rectangle starts run 4 step 0xA | progress |
| 14 (0xE) | `Scena06_Run04` step 0xC (`:747`) | Run 4 from step 0xA: message 0x29, drop-in 2 | - | `Scena06_StepHook` (`:2277`), once; `Scena06_EnterArea` area 0x35 (`:356`): party calls while 0x11 is clear | progress |
| 15 (0xF) | `Scena06_Run04` step 7 (`:733`) | After the change into area 0x5E at step 6 | - | `Scena06_EnterArea` area 0x5E, counter 2 at 1 (`:408`): the pass flags 0x1F while clear | progress |
| 16 (0x10) | `Scena06_Run04` step 0x16 (`:761`); `Scena06_Run06` step 0xA (`:895`) | Run 4 from step 0x14 (object entry 7): message 0x29, drop-in 0. Run 6's end after the change into area 0x5E at step 9 | - | `Scena06_StepHook` (`:2268`): arms the area 0x39 trigger for run 5 | progress |
| 17 (0x11) | `Scena06_Run05` step 2 (`:795`) | After the change into area 0x35; then stream 0 and a change into area 0x57 | - | `Scena06_StepHook` (`:2268`): run 5 once; `Scena06_EnterArea` area 0x35 (`:356`) and area 0x5E (`:418`): starts run 6 at step 0x1E | progress |
| 18 (0x12) | `Scena06_Run06` step 1 (`:833`) | Run 6 from step 0 (object entry 8): counter 2 at 2, change into area 0x5E | - | Nothing in code | progress |
| 19 (0x13) | `Scena06_Run06` step 9 (`:888`) | After effect kind 0x13: change into area 0x5E, music byte 0x42 | - | `Scena06_EnterArea` area 0x5E, counter 2 at 2 (`:410`): the camera set-up while clear | progress |
| 20 (0x14) | `Scena06_Run06` step 0x18 (`:923`) | Run 6 from step 0x14 (object entry 9): drop-in 6, `Zenny_Add(1000)` at step 0x17 | - | Nothing in code | optional |
| 21 (0x15) | `Area94_ChoiceSetFlag15` (`area_w2c.cpp:589`) | A choice in area 0x5E, answer 1 | `Run07Leave` (`:957`), from run 7 steps 0x14 and 0x24 | Nothing in code | choice |
| 22 (0x16) | `Area94_ChoiceRunByFlags16` (`area_w2c.cpp:601`) | A choice in area 0x5E, answer 1, the first time: starts run 7 at step 0 | - | The same function (`area_w2c.cpp:600`): later answers go to run 7 step 0x19 or 0x32 | once-only set-up |
| 23 (0x17) | `Scena06_Run07` step 0xE (`:1029`) and step 0x20 (`:1096`) | `GuestEquipped` false at step 6 or 0x1F (record 7's weapon or armour byte 0): counter 2 at 2, change into area 0x5C | `Run07Leave` (`:958`) | Nothing in code | transient |
| 26 (0x1A) | `Run07Back` (`:947`), from run 7 steps 0xA and 0x28 | `GuestEquipped` true at step 6 or 0x1F: change into area 0x5C, music byte 0x3C | - | `Scena06_EnterArea` area 0x5C, counter 2 at 1 (`:395`): pass flags 0 while clear; `Area94_ChoiceRunByFlags16` (`area_w2c.cpp:603`): run 7 step 0x19 or 0x32 | choice |
| 27 (0x1B) | `Run07Back` (`:948`); `Scena06_Run07` step 0x12 (`:1037`) | As flag 26; and run 7 from step 0x12 before `Zenny_Add(1000)` at step 0x13 | - | `Area92_ChoiceRunByFlag1B` (`area_w2c.cpp:518`): run 7 step 0x12 while clear, else 0x23 | progress |
| 28 (0x1C) | `Scena06_Run08` step 0x11 (`:1236`) | Run 8 from step 0xA: event battle 0x1A (`:1208`), request 7, message 0x23, then a change into area 0x57 | - | Nothing in code | progress |
| 29 (0x1D) | `Run09Enter` (`:1280`), from run 9 steps 1 and 3 | Change into area 0x5C before event battle 0x1B (`:1320`) | `Run09Leave` (`:1294`), from run 9 steps 0x12 and 0x17 only | Nothing in code | transient |
| 30 (0x1E) | `Area94_GiveKeyItem7` (`area_w2c.cpp:682`) | After `KeyItem_Add(7)`, an area 0x5E handler | - | `Scena06_StepHook` area 0x5E (`:2278`): run 4 step 0x1E while clear, run 10 step 5 once set | progress |
| 31 (0x1F) | `Scena06_Run09` step 7 (`:1327`) | After event battle 0x1B: drop-in 0xE, change into area 0x5C | - | `Scena06_EnterArea` area 0x57, counter 2 at 1 (`:371`): pass flags 0x1F and the run's end while clear | progress |
| 32 (0x20) | `Scena06_Run09` step 0x10 (`:1349`) and step 0x15 (`:1355`) | Run 9 from step 0xF (drop-in 0xB) or 0x14 (drop-in 0xD); transition 0xD | - | `Area94_ChoiceRunByFlag20` (`area_w2c.cpp:619`): run 9 step 0 while clear, else step 2 | optional |
| 33 (0x21) | `Scena06_EnterArea` (`:377`) | Entry to area 0x57 with counter 2 at 2: also ends the run | - | The same test (`:376`): once | once-only set-up |
| 34 (0x22) | `Scena06_Run16` step 1 via `Gift` (`:1942`, set at `:1923`) | Object entry 12: record 7's weapon byte + 1 added as an item of category 1, message 0x94. Not set when `Inventory_Add` answers 0 | - | `Scena06_Object12` (`:2089`): run 16 step 0x14 (system message 1) once set; `Scena07_Object3` (`scena_sc7.cpp:1235`, `:1198`) | optional |
| 35 (0x23) | `Scena06_Run16` step 0xC via `Gift` (`:1951`) | Object entry 13: record 7's armour byte + 1 as category 2, message 0x95 | - | `Scena06_Object13` (`:2098`); `Scena07_Object4` (`scena_sc7.cpp:1239`) | optional |
| 36 (0x24) | `Scena06_Run10` step 5 (`:1410`) | Area 0x5E step hook with flag 0x1E set: drop-in 8 | - | `Scena06_StepHook` (`:2282`): once | once-only set-up |
| 37 (0x25) | `Scena06_Run11` step 9 (`:1465`) | Run 11 from step 5 (cell entry 0, `Inventory_Count(1, 0x47)` not 0), flags 0x27 and 0x25 clear: message 9, then drop-in 0 when counter 0 is 1 | `Scena06_Run13` step 6 (`:1664`) | `Scena06_Run11` (`:1439`): message 8; `Scena06_Run13` (`:1655`); `EffectKind45_Start` / `_Tune` / `_End` (`effect_2d.cpp:125`, `:145`, `:212`): the kind 0x45 effect steers its own trace | transient |
| 38 (0x26) | `EffectKind45_Aim` (`effect_2d.cpp:197`); `EffectKind45_Tune` (`effect_2d.cpp:163`) under DIV-0188 only | Confirm pressed with the trace in its window (or any press with `Cheats_MinigameNoFail`) in the kind 0x45 effect run 13 spawns | `Scena06_Run13` step 1 (`:1627`); `EffectKind45_Aim` (`effect_2d.cpp:201`) on a miss | `Scena06_Run13` step 3 (`:1635`): step 4 (success) or 6; `EffectKind45_End` (`effect_2d.cpp:212`): sound 0x206 or 0x204 | transient |
| 39 (0x27) | `Scena06_Run13` step 5 (`:1645`) | After a success: map byte (0x5D, 0x2E) 0x51, event flag 0x2F, drop-in 1; then music 0x5B | - | `Scena06_Run11` (`:1428`, `:1436`): message 0xA; `Scena06_Cell2` (`:2311`): run 13 while clear; `Scena06_Cell3` (`:2319`); `Scena06_StepHook` area 0x4D (`:2272`, `:2273`) | progress |
| 40 (0x28) | `Scena06_Run14` step 5 (`:1733`) | Area 0x4D step hook: member lines 0x10..0x12, effects of kind 6 | - | `Scena06_StepHook` (`:2272`): once | once-only set-up |
| 41 (0x29) | `Scena06_Run14` step 0xD (`:1748`) | Run 14 from step 0xA (object entry 11): `Scena06_LeaderEffect(0x93)`, member lines 0x14..0x16, drop-in 4 | - | Nothing in code | optional |
| 42 (0x2A) | `Scena06_Run14` step 0x23 (`:1781`) | Area 0x4D step hook: event battle 0x1C (`:1774`, fight 28) | - | `Scena06_StepHook` (`:2273`): once | progress |
| 44 (0x2C) | `Scena06_Run15` step 0x16 (`:1885`) | Run 15 from step 5 (cell entry 3 with flag 0x27 and event flags 5 and 6): event flag 7, transitions, music 0x5C, then `Inventory_Add(0, 0x57, 1)` | - | `Scena06_EnterArea` area 0x4C (`:361`): the chapter's end; `Scena06_Cell3` (`:2320`): message 0x26 once set; `Area77_CellHook` (`area_w2a.cpp:147`): area 0x4D's cell switches stop | progress |
| 45 (0x2D) | `Scena06_Run13` step 1 (`:1628`) | With effect kind 0x45 spawned (`:1626`) | `EffectKind45_End` (`effect_2d.cpp:213`) | `Scena06_Run13` step 2 (`:1632`): waits for the effect to end | transient |
| 59 (0x3B) | `Scena06_EnterArea` (`:419`) | Entry to area 0x5E with flag 0x11 set: starts run 6 at step 0x1E | - | The same test (`:418`): once | once-only set-up |
| 60 (0x3C) | `Scena06_EnterArea` (`:341`) | Entry to area 0x2E with flag 0xB set: starts run 17 | - | The same test (`:334`): once | once-only set-up |
| 61 (0x3D) | `Scena06_Run17` step 8 (`:2017`) | After the change into area 0x3C: change into area 0x57 | - | Nothing in code | progress |
| 62 (0x3E) | `Scena06_Run01` step 1 (`:455`) | Before the change into area 0x27 | `Scena06_Run01` step 0xA (`:490`) | Nothing in chapter 6's code. Chapter 9 also writes it, below | transient |
| 63 (0x3F) | `Scena06_EnterArea` (`:364`) | Entry to area 0x4C with flag 0x2C set: the chapter's end | - | The same test (`:361`): once | progress |

**Never touched by any code found:** 2, 6, 24, 25, 43, 46..58. None of them is
set in the observed saves.

**Run starters, for reading the table.** Object entries (`Scena06_Objects`,
`:2056-2100`): 1 run 1; 2 run 2; 3 run 2 step 0xA; 4 run 2 step 0xF; 6 / 7 run 4
step 0 / 0x14; 8 / 9 run 6 step 0 / 0x14; 10 run 10; 11 run 14 step 0xA; 12 / 13
run 16. Cell entries (`:2303-2327`): 0 run 11, 2 run 13, 3 run 15. Area 0x5C and
0x5E choices (`area_w2c.cpp:485-623`) start runs 7, 8 and 9. Which field object
or cell carries each entry is game data, not code.

**Chapter 9 writes row 6's flag 62** through the literal row `Cond_Flags + 0x30`:
`Scena09_EnterArea` sets it on area 0x25 (`scena_sc9a.cpp:185`), and
`Scena09_Run15` clears it at step 0x14 (`:1447`) and sets it at steps 0x46
(`:1498`) and 0x5E (`:1577`). No code found tests it.

## End

**The path out.** `Scena06_EnterArea`, on entry to area 0x4C with flag 0x2C set
and 0x3F clear, clears the four counters, sets flag 0x3F and calls
`Field_SetStatus80` (`scena_sc6.cpp:361-366`). The next area change then calls
`Scenario_Start(7)` (`mode_flow.cpp:548-555`). Flag 0x2C comes from
`Scena06_Run15` step 0x16 (`:1885`), started by cell entry 3 once flag 0x27 and
event flags 5 and 6 are set (`:2319-2320`). Flag 0x27 comes from `Scena06_Run13`
after the kind 0x45 effect succeeds. So the end needs 0x27, 0x2C, 0x3F.

**The row as left** (reading, not checked):

- Always set by the chapter's chains: 0, 1, 3, 7..11, 13..19, 22, 27, 28, 30,
  31, 33, 39, 42, 44, 61, 63. Event battles 0x17, 0x18, 0x1A, 0x1B and 0x1C
  lie on these chains.
- Set by an entry to an area once its condition holds, so very likely set: 59,
  60, 36 and 40 (by step hooks the player must cross; reading).
- Optional: 4, 5, 12 (object entries 2..5), 20 (object 9), 32 (run 9 steps
  0xF / 0x14), 34, 35 (the guest's gifts), 41 (object 11).
- Alternatives: 26 (guest equipped) or 23 (not equipped) at run 7 steps 6 and
  0x1F; 23 is then cleared by `Run07Leave`. 21 is one choice answer and
  `Run07Leave` clears it.
- Cleared on the way, so normally clear at the end: 45, 62 (in chapter 6), 23.
  29 is cleared only on run 9's step 0xF / 0x14 path. 37 and 38 depend on how
  the kind 0x45 effect went: 38 set and 37 kept on a success, 37 cleared on a
  miss with 37 set.

**Against the saves past chapter 6** (observed-rows.md saves 20..22, chapters 14,
10 and 15; all three rows are the same):
`0..1 3..5 7..22 26..31 33..40 42 44 59..63`.

- Agree: every flag in the "always set" list is set. 2, 6, 24, 25, 43, 45..58
  are clear, as no code sets them or (45) it is cleared.
- Agree: 23 is clear and 26 is set (the guest-equipped path at least once).
- Agree: 32 and 41 are clear (optional paths not taken), and so 29 is still set
  (`Run09Leave` never ran).
- Agree: 37, 38 and 39 set: the last try of the kind 0x45 effect succeeded.
- Not settled: 21 is set although `Run07Leave` clears it. Either the choice was
  answered after the last `Run07Leave`, or `Run07Leave` never ran.
- Not settled: 62 is set, but these saves are past chapter 9, which writes it.
  The saves cannot show chapter 6's own value.

## Event flags used by this chapter

`Cond_Flags + 0xA0`, not a chapter row:

| Event flag | Site | What |
|---:|---|---|
| 5, 6 | `Scena06_Cell3` (`:2319`) tests | With flag 0x27: run 15 from step 5 (the chapter's last scene) |
| 7 | `Scena06_Run15` step 6 (`:1812`) sets | With `Cond_ByteFE = 1`, effect kind 0x4B, drop-in 6 |
| 0x2F (47) | `Scena06_Run13` steps 4 and 6 (`:1638`, `:1659`) set; steps 7 and 8 (`:1668`, `:1677`) clear | Set after the kind 0x45 effect; the success path (steps 4, 5) leaves it set |
| 0x1C (28), and the switch's own flag | `Area77_CellHook` (`area_w2a.cpp:147-158`) | Area 0x4D's cell switches toggle an event flag named in `Area77_CellSwitches` byte 3, unless event flag 0x1C is set; off once row 6 flag 0x2C is set |

Saves 20..22 have event flags 5, 6, 7 and 47 set and 28 clear, which agrees.
That the switches' flags are 5 and 6 is a reading, not checked.

## Unresolved

- **Which area chapter 6 starts in.** It is the destination of the first area
  change after chapter 5's entry to area 0x2D. The code does not name it.
- **Who starts runs 7, 8 and 9 at steps without a code starter.** Run 9 steps
  0xF and 0x14 (which set 32 and clear 29) have no starter in code. Nor do run 7
  step 0x23, run 8 steps 0x14 and 0x28 beyond the area 0x5C choices. These are
  likely field scripts (game data).
- **Which object or cell carries each `Scena06_Objects` / `Scena06_Cells`
  entry.** Game data (`Scena06_Cells` is read by `0x56D800`). So the area of
  object entries 1..13 and cell entries 0, 2, 3 is not known from code.
- **Run 11's counter 0 at step 6** decides whether flag 0x25 is set. What sets
  counter 0 to 1 during message 9 is not found.
- **Run 9 step 6 after `Run09Leave`.** Entry to area 0x57 with counter 2 at 1
  ends the run only while flag 0x1F is clear. With 0x1F set the run seems to
  stay at step 6. Needs a run.
- **The step after an event battle.** The table assumes a battle's end adds one
  to the step (runs 8, 9, 12). Not traced.
- **Data-driven sites.** Field scripts test chapter rows (`Area_TestCondition`,
  `Cond_Flags + 8 * kind`), and may set them. None of that is in this table.
  Flags tested by nothing in code (1, 4, 9, 10, 12, 18, 20, 28, 61) may be read
  there.
- **`Area98_ChoiceFlag9`** (`area_w2d.cpp:337`) sets flag 26 of the current row.
  Whether area 98 is reached in chapter 6 is not known.
- **`BossMap_UpdateFromEnemies`** (`boss_h.cpp:355-356`) sets and clears flags
  0x33 and 0x34 of the current row in boss set-ups 18..20. Whether one of those
  runs in chapter 6 is not established. Both flags are clear in saves 20..22.
- **`Area94_InitPatches`** (`area_w2c.cpp:693`) sets event flag 0x42 and clears
  0x43 on entry to area 0x5E from area 0x79. Not known whether this happens in
  chapter 6.
- **Flag 62 in chapter 9.** What reads row 6's flag 62 is not found in code.
