# Story flags: chapter 9

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 9's row is `Cond_Flags + 0x48` (`0x903FD8`), save block `+0x5F8`. The
chapter's code is `src/game/scena_sc9a.cpp` (state 1, runs 1..11 and 14..16,
objects 1..4) and the first part of `src/game/scena_sc9b.cpp` (objects 7..15,
`Scena09_StepHook`, `Scena09_CellHook` and cells 0..13). In both files the
wrappers are `Flag(n)`, `Set(n)`, `Clr(n)` on `Bits()` (the row
`Chapter_FlagRow` points at), and `SetAt(row, n)` / `ClrAt(row, n)` on a
literal row (`scena_sc9a.cpp:80-87`, `scena_sc9b.cpp:97-103`). The helpers
without a prefix in these files serve chapter 9: `EnterAreaBody` is
`Scena09_EnterArea`'s body (`scena_sc9a.cpp:389`), `SetAndEnd` and `SweepAt`
are run 15's (`scena_sc9a.cpp:1400`), `StepHook9` is `Scena09_StepHook`'s body
(`scena_sc9b.cpp:1027`), `CellLeader` and `CellRun8` are cells 1, 2 and 6..10
(`scena_sc9b.cpp:1050-1101`).

"Entrance" below is `Cond_ByteFD`, as in chapter 0's table. "Leader kind" is
`ObjTrio[0].char_id`. Event battle names are those of
[`../boss-fights.md`](../boss-fights.md).

## Start

- **Who starts the chapter.** Chapter 8's `Scena08_Scene11` (the run that holds
  event battle 0x22, Mikba) ends at step 0x15 with `Field_SetStatus80`
  (`scena_sc7.cpp:2591`). That sets bit 7 of `Field_StatusBits`. The next
  `Area_Enter` sees the bit, adds one to `Cond_ByteFA` and calls
  `Scenario_Start(9)` (`mode_flow.cpp:548-556`). The run makes no area change
  of its own after the bit, so the chapter starts on the next area change after
  that step. Reading, not checked.
- **What `Scenario_Start(9)` does** (`title_states.cpp:404`): the chapter byte
  9, the scenario bytes zeroed, `Chapter_FlagRow` = `0x903FD8`. It zeroes only
  the first dword of the row (`SetLong`, `title_states.cpp:416`): flags 0..31.
  Flags 32..63 are not cleared at the chapter's start.
- **State 0** is `ScenaShared_State0` (`scena_sc13.cpp:1424`, original
  `0x5646B0`): it only sets the state byte to 1. It sets no flag, no area, no
  party and no run.
- **State 1** is `Scena09_EnterArea` (`EnterAreaBody`, `scena_sc9a.cpp:179`).
  The first scene with a fresh row is area 0x45 with flag 0 clear: run 1 is
  started (`scena_sc9a.cpp:260`), and run 1 step 0 sets flag 0 and changes into
  area 0x45 again (`scena_sc9a.cpp:401`). Which area the chapter's first entry
  is, is not in the code. Reading, not checked.

## Flags

Line numbers are `scena_sc9a.cpp` unless the cell names another file. "9b" is
`scena_sc9b.cpp`.

| Flag | Set by (function, step, file:line) | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena09_Run1` step 0, :401 | Run 1 starts (area 0x45 entry); area change into 0x45 | - | `EnterAreaBody` :260: area 0x45 entry starts run 1 while clear; effect sub-kinds 0x13 and 0x50 (table `0x65E010` / `0x65E094` entry 0, below) | progress |
| 1 | `Scena09_Run1` step 2, :410 | Counter 0 reaches 0x11; run 1 ends | - | nothing found | progress |
| 2 | `Scena09_Run2` step 2, :432 | Run 2 (started by `StepHook9` in area 0x31, 9b:263) changes into area 0x31 | - | `StepHook9` 9b:263, 268: which of run 2's two starts the area 0x31 rectangle gives; `EnterAreaBody` :276 | progress |
| 3 | `Scena09_Run2` step 7, :455 | Run 2 ends, after stream 3 and music 0x10 | - | `EnterAreaBody` :276: area 0x31's kind-2 sprite and call entries while 2 is set and 3 clear | progress |
| 4 | `Scena09_Run2` step 0xA, :461, and step 0xB, :466 | Run 2 from step 0xA (`StepHook9` 9b:268, area 0x31, flag 2 set, 4 clear): area change into 0x31 | - | `StepHook9` 9b:268; `EnterAreaBody` :241: area 0x37 starts run 3 while 4 is set and 5 clear | progress |
| 5 | `Scena09_Run3` step 7, :521 | Run 3 (area 0x37 entry) changes into area 0x37 | - | `EnterAreaBody` :241 | progress |
| 6 | `Scena09_Run3` step 0xB, :547 | Stream 3 done, `Scenario_CallA(5)` | - | `EnterAreaBody` :290: area 0x31 starts run 4 while 6 is set and 7 clear | progress |
| 7 | `Scena09_Run4` step 0, :572 | Run 4 starts; area change into 0x31 | - | `EnterAreaBody` :290 | progress |
| 8 | `Scena09_Run4` step 0x10, :640 | Counter 0 at 0xA; area change into 0x31 by counter 1 (entrance 0x8B or 0x8C) | - | nothing found | progress |
| 11 (0xB) | `Scena09_Run4` step 0x22, :678 | Run 4 from step 0x1E: drop-in 0xD, two area changes into 0x75; ends | - | nothing found | progress |
| 12 (0xC) | `Scena09_Run6` step 0x18, :800 | Run 6 from step 0x14 (cell 0, leader kind 5, flag 0xC clear): area changes into 0x75; `Cond_ByteFE` 1 | - | Cells 0, 1, 4 (9b:1044, 1050, 1077): which run 6 step a cell starts; `effect_5b.cpp:659, 727` (sub-kind 0x0E gate open); tables of sub-kinds 0x13, 0x0F, 0x50 (below) | progress |
| 13 (0xD) | `Scena09_Run6` step 0x23, :833 | Run 6 from step 0x1E (cell 1): area 0x76 and back to 0x75; `Cond_ByteFE` 2 | - | Cells 1, 4, 5 (9b:1050, 1079, 1088); tables of sub-kinds 0x13, 0x0F, 0x50 | progress |
| 14 (0xE) | `Scena09_Object07`, 9b:981 | Object 7 triggered | - | Cells 1, 2, 4 (9b:1050, 1054, 1078): run 6 step 3 while clear | optional |
| 15 (0xF) | `Scena09_Run6` step 0x2D, :867 | Run 6 from step 0x28 (cell 2): area 0x76 and back to 0x75; `Cond_ByteFE` 3 | - | Cells 2, 5 (9b:1054, 1090); `effect_5c.cpp:477, 545` (sub-kind 0x10 panel); tables of sub-kinds 0x13, 0x0F, 0x50 | progress |
| 16 (0x10) | `Scena09_Object09`, 9b:991 | Object 9 triggered | - | Cells 2, 3, 5 (9b:1054, 1060, 1089) | optional |
| 17 (0x11) | `Scena09_Run6` step 0xA, :772, and step 0x38, :918 | Step 0xA: message 0x2B answered with counter 0 0x14 (cell 3 entry, step 7). Step 0x38: run 6 from step 0x32 (cell 3, flag 0x1D clear): area 0x76, `Cond_ByteFE` 4 | `EnterAreaBody` :301: area 0x31, entrance 1, flag 0x1E clear | Cell 3 (9b:1069): run 6 step 7 while clear; tables of sub-kinds 0x13 and 0x0F (entry 3) | progress (cleared and set again until 0x1E) |
| 18 (0x12) | `Scena09_Run8` step 0x10, :1028 | Run 8 from step 0xF (object 11): drop-in 0, counter 0 at 1 | - | Cell 3 (9b:1061) | progress |
| 19 (0x13) | `Scena09_Run7` step 1, :986 | Run 7 (area 0x76, entrance 4, flag 0x13 clear, `EnterAreaBody` :319): drop-in 0 done | - | `EnterAreaBody` :319: which effects area 0x76 places | progress |
| 20 (0x14) | `Scena09_Object10`, 9b:994 | Object 10 triggered | - | nothing found | optional |
| 21 (0x15) | `StepHook9`, 9b:285 | Area 0x77, flag 0x3D set, the rectangle (0x420000..0x448000, 0x3C0000..0x3E8000): run 8 step 0x14 (sound 0x206, drop-in 2) | - | `StepHook9` 9b:284: once only | once-only |
| 22 (0x16) | `EnterAreaBody`, :342 | Area 0x77, entrance 2, flag 0x3D set: run 8 step 0x1E, which holds event battle 0x35 (HugeSlug) at step 0x24, :1107 | - | `EnterAreaBody` :335; `StepHook9` 9b:288: the second rectangle gives run 8 step 0x19 (message 0x2C) while clear | progress |
| 23 (0x17) | `Scena09_Run8` step 0x28, :1123 | After event battle 0x35: drop-in 4 done | - | nothing found | progress |
| 24..28 (0x18..0x1C) | `CellRun8`, 9b:356 (cells 6..10, flag `0x18 + step`) | A cell 6..10 stepped on with flag 0x3D set: run 8 at step 0..4 (messages 0x23..0x27), flag 0x18 + step | - | nothing found | optional (one per cell) |
| 29 (0x1D) | `Scena09_Run6` step 0x38, :919 | With flag 0x11 (above) | - | Cell 3 (9b:1062): run 6 step 0x32 while clear | progress |
| 30 (0x1E) | `Scena09_Run9` step 5, :1168 | Run 9 (`StepHook9` 9b:290, area 0x77, flag 0x1E clear) after event battle 0x36 (Shroom) at step 3, :1154; area change into 0x77 | - | `StepHook9` 9b:271, 290; `EnterAreaBody` :301 | progress |
| 32 (0x20) | `Scena09_Run11` step 0xE, :1339 | Run 11 from step 0 (`StepHook9` 9b:313: area 0x52, entrance 0, the cell, leader kind 2, flag 0x20 clear): the long effect run ends | - | `StepHook9` 9b:302, 314, 320: arms the chapter's end | progress |
| 33 (0x21) | `StepHook9`, 9b:303 | Area 0x52, entrance 0, flag 0x20 set and 0x21 clear, the cell (z 0x40..0x44, x 0x22..0x25): run 11 step 0x1E, the chapter's end | - | `StepHook9` 9b:302, 320 | progress (the end) |
| 34 (0x22) | `SetAndEnd` from `Scena09_Run15` step 0x83, :1673 | Run 15 from step 0x82 (`StepHook9` 9b:278, area 0x3B, flag 0x3A set, 0x22 clear): area change into 0x3B | - | `StepHook9` 9b:277 | progress |
| 35 (0x23) | `Scena09_Run15` step 0x95, :1725 | Run 15 from step 0x8C (`StepHook9` 9b:279, area 0x3B, flag 0x22 set): area 0x35, stream 0, message 0x37, request 6; area change into 0x2D | - | `StepHook9` 9b:248, 254, 279 | progress |
| 36 (0x24) | `Scena09_Run16` step 1, :1748 | Run 16 (`StepHook9` 9b:254, area 0x2E, flag 0x23 set, 0x24 clear): drop-in 2 | - | `StepHook9` 9b:254 | progress |
| 37 (0x25) | `StepHook9`, 9b:265 | Area 0x31, flag 2 clear, the rectangle: run 2 starts | `Scena09_Run2` step 7, :456 | `area_w1c.cpp:445` (`Area49_EffectFrame`, row 9 fixed): clears each member's bit while set | transient |
| 38, 39 (0x26, 0x27) | `Scena09_Run4` step 6 (0x26) / step 5 (0x27), :592 | Object 3 starts run 4 at step 5; step 6 has no starter in the file. With `ObjTrio` height at least 0x8000000 each step sets its flag and goes on only when the other is set too | - | `Scena09_Run4` :593; `effect_5d.cpp:1072` (sub-kind 0x14, table `0x65E31C` = 38, 39: sound 0x20A and a kind 0x6E effect) | progress (both needed) |
| 44 (0x2C) | `Scena09_Run9` step 0x16, :1204 | Run 9 from step 0x14 (cell 12): message 0x4B answered with counter 0 0xA, drop-in 8 | - | Cell 12 (9b:1119), `StepHook9` 9b:292 | choice (the other answer ends the run with no flag) |
| 45 (0x2D) | `SetAndEnd` from `Scena09_Run15` step 1, :1423 | Run 15 step 0 (object 13): drop-in 1 | - | nothing found | optional |
| 46 (0x2E) | `Scena09_Run14` step 4, :1387 | Run 14 (`StepHook9` 9b:271, area 0x31, flag 0x1E set): areas 0x35 and 0x41 | - | `EnterAreaBody` :183, 239; `StepHook9` 9b:240, 271 | progress |
| 47 (0x2F) | `EnterAreaBody`, :187; `SetAndEnd` from `Scena09_Run15` step 0xC, :1436 | First area 0x25 entry with flag 0x2E: run 15 step 0xA; call entries by the party set | - | `EnterAreaBody` :183 | progress |
| 48 (0x30) | `SetAndEnd` from `Scena09_Run15` step 0x16, :1460 | Run 15 from step 0x14 (object 14): drop-in 2, area change into 0x27 | - | `EnterAreaBody` :209: area 0x27's kind-2 sprite while clear | progress |
| 49 (0x31) | `SetAndEnd` from `Scena09_Run15` step 0x29, :1470 | Run 15 from step 0x28 (object 15): drop-in 9 | - | `EnterAreaBody` :210 | progress |
| 50 (0x32) | `EnterAreaBody`, :222 | First area 0x29 entry: run 15 step 0x3C | - | `EnterAreaBody` :221 | once-only |
| 51 (0x33) | `SetAndEnd` from `Scena09_Run15` step 0x33, :1478 | Run 15 from step 0x32 (cell 13, leader kind 4): drop-in 0xA | - | Cell 13 (9b:1129): any other leader gets message 0xA5 and no flag | choice |
| 52 (0x34) | `Scena09_Run15` step 0x4B, :1520 | Run 15 from step 0x46 (area 0x27, entrance 0, flags 0x36 and 0x31, `EnterAreaBody` :210): messages 0x87, 0x88; area change into 0x27 | - | `EnterAreaBody` :210 | progress |
| 54 (0x36) | `SetAndEnd` from `Scena09_Run15` step 0x3E, :1490 | Run 15 from step 0x3C (first area 0x29 entry) | - | `EnterAreaBody` :210 | progress |
| 55 (0x37) | `Scena09_Run15` step 0x5B, :1559 | Stream 2, the camera sweeps; area change into 0x29 | - | nothing found | progress |
| 56 (0x38) | `Scena09_Run15` step 0x5E, :1579 | End of run 15 from 0x46; counter 2 = 1 | - | `EnterAreaBody` :229, 307; `StepHook9` 9b:240, 248 | progress |
| 57 (0x39) | `Scena09_Run15` step 0x7C, :1646 | Run 15 from step 0x78 (area 0x64, entrance 2, counter 2 at 2, `EnterAreaBody` :307): area change into 0x64 | - | nothing found | progress |
| 58 (0x3A) | `SetAndEnd` from `Scena09_Run15` step 0x7F, :1665 | After the area change into 0x3B at step 0x7E | - | `StepHook9` 9b:276: arms runs 15 at 0x82 and 0x8C | progress |
| 59 (0x3B) | `Area49_ChoiceMessageFlag3B`, `area_w1c.cpp:383` (current row) | Area 0x31's choice 0, answer 0 | - | nothing found | unknown (chapter not pinned, see Unresolved) |
| 60 (0x3C) | `Area119_SetRowFlag3C`, `area_w3a.cpp:1044` (current row) | Area 0x77's handler 10, sound 0x203 | `Area119_ClearRowFlag3C`, `area_w3a.cpp:1049` | nothing found in the source | transient (reading, not checked) |
| 61 (0x3D) | `Scena09_Cell11`, 9b:1107 | Cell 11 stepped on while clear: sound 0x203 | - | Cells 6..11 (9b:347, 1106): cells 6..10 give run 8 step 5 while clear; `EnterAreaBody` :335; `StepHook9` 9b:284, 288 | progress |
| 62 (0x3E) | `Scena09_Run4` step 0xF, :633 | Counter 0 at 9, after the camera sweep in area 0x31 | - | `effect_5d.cpp:1027, 1037` (sub-kind 0x14: CLUT strip dimmed, then released) | progress |
| 63 (0x3F) | `Scena09_Run5` step 1, :700; maybe also `Area49_SetRowFlag3F`, `area_w1c.cpp:533` (current row) | Run 5 from step 0 (object 1): drop-in 0xC, counter 0 at 9 | - | nothing found | progress (reading) |

Flags 9, 10, 31, 40..43 and 53 have no setter, clearer or test in the code
read.

**The flag tables in the effect files.** The effect sub-kinds read row 9 at its
fixed address, so they work in any chapter. Their flag numbers are in tables in
`BOF3.exe`, read 2026-11-09 for this page (four bytes each, flag numbers only):

| Table | Read by | Flags |
|---|---|---|
| `0x65E010` | sub-kind 0x13 panel shown (`effect_5b.cpp:799, 813, 839`) | 0, 12, 13, 15 |
| `0x65E014` | sub-kind 0x13 face (`effect_5b.cpp:817, 831, 857`) | 12, 13, 15, 17 |
| `0x65E034` | sub-kind 0x0F gate (`effect_5b.cpp:999, 1067`, by `+0x3A`); sub-kind 0x50 texture (`effect_5c.cpp:302, 320, 352`) | 12, 13, 15, 17 |
| `0x65E094` | sub-kind 0x50 wall (`effect_5c.cpp:279, 296, 331`, entries 1..2) | 0, 12, 13, 15 |
| `0x65E31C` | sub-kind 0x14 (`effect_5d.cpp:1072`, two entries) | 38, 39 |

So flags 12, 13, 15 and 17, set by run 6's four scenes, are the ones the
gates, panels and walls show. Reading, not checked.

### Another chapter's row: chapter 6's flag 62

Chapter 9 sets and clears flag 62 (0x3E) of **chapter 6's** row
(`Cond_Flags + 0x30`, `0x903FC0`) with `SetAt` / `ClrAt`:

- set by `EnterAreaBody` :185 (first area 0x25 entry with flag 0x2E), by
  `Scena09_Run15` step 0x46 :1498 and step 0x5E :1577;
- cleared by `Scena09_Run15` step 0x14 :1447.

Its last write on the main path is a set (step 0x5E). Saves 20..22 have chapter
6's flag 62 set. What tests it is not found.

## End

`StepHook9` (`scena_sc9b.cpp:302`): in area 0x52, entrance 0, with flag 0x20
(32) set and 0x21 (33) clear, the cell (z 0x40..0x44, x 0x22..0x25) sets flag
33 and starts run 11 at step 0x1E. `Scena09_Run11` step 0x1E (`:1343`) calls
`Party_DropIn(3)`, sets counter 0 to 0x20 and calls `Field_SetStatus80`
(`:1347`). The next `Area_Enter` starts chapter 10. The area change itself is
not in this code; reading, the drop-in 3 script makes it.

Flag 32 is set by run 11 from step 0, which the same hook starts when the leader
kind is 2 and 32 is clear (`scena_sc9b.cpp:313`). With another leader the hook
only sets counter 0 to 0x10 (`:320`).

**The row as left (reading):**

- Always set: 0..8, 11..13, 15, 17..19, 21..23, 29, 30, 32..36, 38, 39, 46..50,
  52, 54..58, 61, 62, 63.
- Optional: 14, 16, 20 (objects 7, 9, 10), 24..28 (cells 6..10), 45
  (object 13).
- Choices: 44 (run 9's answer), 51 (cell 13 with leader kind 4, or message
  0xA5 with no flag).
- Cleared before the end: 37. Flag 17 is cleared on some area 0x31 entries
  before flag 30 is set, and set again at run 6's step 0x38.
- Unknown: 59, 60 (area handlers, chapter not pinned).

**Against the saves.** Saves 20, 21 and 22 are past chapter 9 and hold the same
row: 0..8 11..19 21..30 32..36 38..39 44 46..50 52 54..58 61..63. The reading
agrees: every flag in the row has a setter above. The always-set list is in
it. Of the optional flags, 14, 16 and 24..28 are set and 20 and 45 are not. Of
the choices, 44 is set and 51 is not. 37 is cleared, as read. 59 and 60 are not
set: this is consistent with 60 as transient. 31, 40..43 and 53 have no
setter and are clear.

## Event flags used by this chapter

Event flags are `Cond_Flags + 0xA0` (`0x904030`), set through `SetAt` /
`ClrAt` on the literal row:

| Event flag | Op | Site |
|---:|---|---|
| 69 (0x45) | cleared | `Scena09_Run15` step 0x46, :1497 (with message 0x87) |
| 64 (0x40) | set | `Scena09_Run15` step 0x7E, :1660 (before the area change into 0x3B) |
| 51 (0x33) | set | `Scena09_Run15` step 0x96, :1729 (after the area change into 0x2D) |

All three are set in saves 20..22. 69 is set there although chapter 9 clears
it: something later sets it again. Not found.

`Area49_CellHook` (`area_w1c.cpp:525`) in area 0x31 toggles an event flag from
its cell table while event flag 0x1C is clear. That is area code, not chapter
code.

## Unresolved

- **Run 4's step 6** sets flag 0x26 (38). Object 3 starts step 5 only
  (`Scena09_Object03`, `scena_sc9a.cpp:1774`). No code in the
  chapter files sets step 6. Reading: the field script does. The two flags
  are written as `Set(five ? 0x27 : 0x26)` (`:592`), resolved here by reading.
- **Run starters outside the chapter files**: run 4 step 0x1E (flag 11), run 6
  step 0x64 (object 8 gives it), run 8 step 0x26 (after battle 0x35), run 9
  step 4 (after battle 0x36), run 11 step 0x23. Reading: the field scripts and
  the battle ends move these steps. Needs a run.
- **The chapter's first area.** Not in the code. Needs a run (or the drop-in
  script of chapter 8's last scene).
- **Flags 59 and 60.** `Area49_ChoiceMessageFlag3B` and `Area49_SetRowFlag3F`
  write the current row in area 0x31, which chapters 5 and 9 both visit
  (`scena_sc5.cpp:216`). Chapter 5's row in the saves has 59 set, chapter 9's
  has 63 set: reading, flag 59 is chapter 5's and flag 63 may be set by both
  writers in chapter 9. `Area119_SetRowFlag3C` / `ClearRowFlag3C` write the
  current row in area 0x77, which only chapter 9's code visits: reading, flag
  60 is chapter 9's and is clear at the end. Not checked.
- **`Area41_ChoiceGiveItem`** (`area_w1a.cpp:614-617`) tests and sets flag 6 of
  the current row in area 0x29 (item 0x28). Chapters 3 and 9 both visit area
  0x29. If it runs in chapter 9, flag 6 is already set by run 3 and the item is
  never given. Not checked.
- **Event scripts.** The game data's event conditions (`Area_TestCondition`)
  can test any chapter row. Those tests are not in our source and are not in
  this table.
- **Objects 5 and 6** of `Scena09_Objects` are `Scena10_Object11` and
  `Scena10_Object12` (`scena_sc9b.cpp:2207-2222`). In chapter 9 they start run
  0xA at step 1 (chapter 9's run 10 acts only at step 0) and run 0xD (a bare
  `ret` in chapter 9). They set no chapter 9 flag. Whether they ever trigger in
  chapter 9 is not known.
- **The extractor** (`tools/story_flags.py`) misses the one-line object handlers
  (`Scena09_Object07/09/10`, flags 14, 16, 20), the `SetAndEnd` sites (flags
  34, 45, 47..49, 51, 54, 58) and `CellRun8` (24..28). It gives no chapter to
  `EnterAreaBody` and `StepHook9` (row "cur" under `file`). They are in the
  table above, read by hand.
