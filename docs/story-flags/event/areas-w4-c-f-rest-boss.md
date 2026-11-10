# Event flags: areas of world 4 (c to f), area 11, the rest_ and boss_ files

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Files read: `area_w4c.cpp`, `area_w4d.cpp`, `area_w4e.cpp`, `area_w4f.cpp`,
`area_011.cpp`, every `rest_*.cpp` (`rest_0a`, `rest_1a..1g`, `rest_2a..2h`,
`rest_3a..3g`, `rest_4a..4f`) and every `boss_*.cpp` (`boss_h`, `boss_sa..sj`,
`boss_spawn`). The `_fuzz`, `_callees.h` and harness files were read only for
constant values.

How each file reaches the event bank (`Cond_Flags + 0xA0`, `0x904030`):

- `area_w4c.cpp`: `Mem(::bof3::Img(Cond_Flags_addr) + 0xa0)` written inline at
  each site.
- `area_w4d.cpp:60`, `area_w4e.cpp:48`, `area_w4f.cpp:61`: a local
  `StoryFlags()` = `Cond_Flags + 0xa0`. In `area_w4e.cpp`, `Test()` (`:52`)
  wraps `Flags_Test`; `Row14()` (`:49`) is chapter row 14 and `FlagRow()`
  (`:51`) the current chapter row, not the event bank.
- `rest_2c.cpp`, `rest_2d.cpp`, `rest_2h.cpp`, `rest_3g.cpp`, `rest_4a.cpp`:
  `At(::bof3::Img(Cond_Flags_addr) + 0xa0)` inline.
- `area_011.cpp`, every other `rest_*.cpp` and every `boss_*.cpp`: no
  event-bank site. Their `Flags_*` calls are on chapter rows (see "Other banks
  met").

No direct byte read or write inside `0x904030..0x904057` was found. The bytes
`Cond_Flags + 0xCC..0xCF` (`0x90405C..0x90405F`) that `area_w4e.cpp` and
`area_w4f.cpp` write are past the bank's 40 bytes, so they are not event flags.

`Op` uses a fourth value, `toggle`, for the one `Flags_Toggle` site: the bit is
inverted. `Field_ChangeArea(a, ...)` names the target area in decimal. The
tool missed the `toggle` site and placed the six flags of the walk table in one
row as `m.flag`; they are split below.

## Sites

| Flag | Op | Function | File:line | Context |
|---|---|---|---|---|
| 88 | set | Area173_Tail38 | area_w4c.cpp:259 | Area 173, tail kind 38, state 1, counter 3 at 0x18: `Field_ChangeArea(186, 0x48000, ..., 0x80)` (z by `Cond_ByteFD` zone 2), then flag 0x58 set, tail disarmed. Tail 38 is armed by `Area173_ArriveHook` on two exact (x, z) spots. |
| 88 | clear | Area173_Tail38 | area_w4c.cpp:279 | Same tail, state 12, counter 3 at 0: flag 0x58 cleared, tail disarmed. States 10..12 are the return leg the init starts. |
| 88 | test | Area173_Init | area_w4c.cpp:313 | Area 173's init: flag 0x58 set arms tail kind 38 at state 10 (the return leg); clear, nothing. |
| 149 | set | Area175_Trigger64 | area_w4d.cpp:594 | Area 175, object trigger id 64: the object's +1 = 4, +0x84 = 2, +0x83 = 0xD, flag 0x95 set, `Party_DropIn(4)`, counter 0 cleared. Unconditional. |
| 88 | test | Area186_Init | area_w4d.cpp:626 | Area 186's init: with flag 0x58 set, entry zone 1 starts `Area186_Start(0xA)` and zone 2 `Area186_Start(0)` (tail kind 39, which leaves back to area 173 at entry 0x81 or 0x82). Clear: nothing. |
| 119 | clear | Area187_TailMessage2 | area_w4d.cpp:708 | Area 187, tail kind 29 (armed by object trigger 14), state 2 after message 2 closes: the byte `0x904152` (`AreaW3c_CampFlag`) = 0, flag 0x77 cleared, change to the saved return point with flags 4. |
| 105 | toggle | Area188_Tail43 | area_w4e.cpp:305 | Area 188, tail kind 43, state 0xA (reached when `KeyItem_Has(6)`): message 0x82, then flag 0x69 toggled; then `Party_DropIn(6)` and `Field_ChangeArea(188, ...)` at entry 0x87 or 0x88. Without key item 6 the tail shows message 0x83 and stops. |
| 130 | test | Area189_Init | area_w4e.cpp:347 | Area 189's init: flag 0x82 set makes `Draw_PassFlags` = 0x1F. Nothing else. |
| 117 | clear | Area189_LeaderStart | area_w4e.cpp:433 | Area 189, leader state 0 (the walk set up): after the pace is chosen, six flags cleared: 0x75. Unconditional. |
| 118 | clear | Area189_LeaderStart | area_w4e.cpp:434 | Same, flag 0x76. |
| 139 | clear | Area189_LeaderStart | area_w4e.cpp:435 | Same, flag 0x8B. |
| 140 | clear | Area189_LeaderStart | area_w4e.cpp:436 | Same, flag 0x8C. |
| 141 | clear | Area189_LeaderStart | area_w4e.cpp:437 | Same, flag 0x8D. |
| 142 | clear | Area189_LeaderStart | area_w4e.cpp:438 | Same, flag 0x8E. |
| 117 | set | Area189_StepArrive | area_w4e.cpp:705 | Area 189 walk: when the step count reaches the pace word, the flag of the block the leader stands on is set, from the local table `kMarks` (`:702`): block (0xB00, 0xF00) sets 0x75. Then a change to area 125 at (0x190000, 0x190000), flags 4. |
| 118 | set | Area189_StepArrive | area_w4e.cpp:705 | Same table, block (0x1500, 0x1100) sets 0x76. |
| 139 | set | Area189_StepArrive | area_w4e.cpp:705 | Same table, block (0xC00, 0x1400) sets 0x8B. |
| 140 | set | Area189_StepArrive | area_w4e.cpp:705 | Same table, block (0x1000, 0x1100) sets 0x8C. |
| 141 | set | Area189_StepArrive | area_w4e.cpp:705 | Same table, block (0x1D00, 0x1700) sets 0x8D. |
| 142 | set | Area189_StepArrive | area_w4e.cpp:705 | Same table, block (0x1400, 0x1400) sets 0x8E. |
| 130 | set | Area191_Tail53 | area_w4e.cpp:965 | Area 191, tail kind 53 (armed by `Area191_StepHook` on a map cell of byte 0xA6, or by a choice at state 0x1E), state 4 once the sound stream ends: flag 0x82 set, the walk word `0x90405C` = 0, the step counters cleared, the walk count `0x90405E` + 1. Message 0x6D opens the tail. |
| 119 | clear | Area191_Tail53 | area_w4e.cpp:982 | Same tail, state 0xA: change back to the saved return area with flags 4, `0x904152` = 0, flag 0x77 cleared. |
| 119 | clear | Area191_Tail53 | area_w4e.cpp:998 | Same tail, state 0x1E: `Field_ChangeArea(150, 0x180000, 0x300000, 1)`, `0x904152` = 0, flag 0x77 cleared. |
| 119 | test | Area191_Init | area_w4e.cpp:1080 | Area 191's init: flag 0x77 set returns before the six objects are placed. Clear: an object list chosen by chapter row 14 bit 0xA, the level byte and the walk count. |
| 119 | test | Area191_Kind18Flag77 | area_w4e.cpp:1100 | Effect kind 18 state 77: flag 0x77 set resets the walk word, the step counters, adds 1 to the walk count and sets `Field_StatusBits` bit 0; clear, nothing. Then `Effect_Release`. |
| 130 | set | Area192_Tail54 | area_w4f.cpp:255 | Area 192, tail kind 54 (armed by `Area192_StepHook` on a 0xA6 cell, or answer 1 of choice 5 at state 0x1E), state 4 once the stream ends: flag 0x82 set, the walk word = 0x1E0, step counters cleared, `Field_StatusBits` bit 0 cleared. Twin of `Area191_Tail53`. |
| 119 | clear | Area192_Tail54 | area_w4f.cpp:273 | Same tail, state 0xA: change to the saved return point with flags 4, `0x904152` = 0, flag 0x77 cleared. |
| 119 | clear | Area192_Tail54 | area_w4f.cpp:288 | Same tail, state 0x1E: `Field_ChangeArea(150, 0x180000, 0x300000, 1)`, `0x904152` = 0, flag 0x77 cleared. |
| 119 | test | Area192_Init | area_w4f.cpp:373 | Area 192's init: flag 0x77 set returns before the six place-ops run. Twin of `Area191_Init`. |
| 119 | test | Area192_Effect18Release77 | area_w4f.cpp:426 | Effect kind 18 state 78: flag 0x77 set resets the walk word to 0x1E0 and clears `Field_StatusBits` bit 0; then `Effect_Release`. |
| 119 | clear | Area193_Tail57 | area_w4f.cpp:517 | Area 193, tail kind 57 (armed by `Area193_StepHook` at state 0xA, message 0x1C), state 0xC: flag 0x77 cleared, `Field_ChangeArea(189, 0x12000000, 0x17FF0000, 2)`, walk bytes `0x90405C..0x90405F` reset. |
| 119 | clear | Area193_Tail57 | area_w4f.cpp:534 | Same tail, state 0xE: flag 0x77 cleared, `Field_ChangeArea(151, 0x160000, 0x150000, 3)`. |
| 138 | clear | Area193_Tail57 | area_w4f.cpp:564 | Same tail, state 0x16 (after message 0x25 or 0x20 by chapter row 14 flag 3, `Party_HealJoined` and a stream): `Music_Play(0x95, 0x10)`, `Draw_PassFlags` 0x1F, chapter row 14 flag 3 cleared and flag 0x8A cleared, tail disarmed. |
| 138 | test | Area193_Init | area_w4f.cpp:586 | Area 193's init: flag 0x8A set starts tail kind 57 at state 0x14 with `Draw_PassFlags` 0; clear sets the pending-area byte `0x937F98` to 1. |
| 85 | set | Area197_Tail51 | area_w4f.cpp:683 | Area 197, tail kind 51, state 1, counter 3 at 0x24: flag 0x55 set, `Field_ChangeArea(170, 0x60000, 0x3F0000, 0x82)`, state 0x1E. |
| 146 | test | Save_BuildBlock | rest_2c.cpp:402 | Building the save block: `Flags_Test(event bank, 0x92)`'s al is stored at `0x904694`, a byte of the slot summary (`0x904680`). |
| 108 | test | MasterGrant_Step1Member | rest_2d.cpp:473 | Master grant, step 1, master 0xC only: when the apprentice has gained 3 or more levels and flag 0x6C is clear, `Inventory_Add(3, 0x16, 1)`. |
| 108 | set | MasterGrant_Step1Member | rest_2d.cpp:474 | Same: when the add takes, flag 0x6C set, the member's name to the text, message 0xE. The flag makes this grant once only. |
| 108 | test | MasterWin_Available | rest_2h.cpp:556 | Whether master `index` is available: master 0xC answers flag 0x6C. Masters 0xB, 0xD, 0xE answer bits 3, 4, 5 of `0x904061` instead. |
| var | test | Area109_SwitchPattern | rest_3g.cpp:598 | Area 109 switch: tests three flags whose numbers are the bytes of the .data table `0x65DE60` (`at::kPatternFlags`), as bits 0..2; answers 1..8. |
| 28 | test | Area109_SwitchHook | rest_3g.cpp:611 | Area 109 cell hook, cell (0x1C, 6) with the leader facing 3: flag 0x1C set makes the hook answer 0 (the switch does nothing). |
| var | clear | Area109_SwitchHook | rest_3g.cpp:615 | Same hook, flag 28 clear: the pattern mod 6 is written back as the three `0x65DE60` flags, each cleared first. |
| var | set | Area109_SwitchHook | rest_3g.cpp:616 | Same loop: each `0x65DE60` flag whose bit of (pattern mod 6) is 1 is set. Then `Effect_HoldFlag1C(0xF)` and sounds 0x206, 0x202. |
| 146 | test | CommuPose_KindB | rest_4a.cpp:1090 | A building's resident pose (kind B): for the second resident (count 1), flag 0x92 set gives pose 0xC00B, clear gives 0xD4. |

## What each flag means here

- **28 (0x1C)**: tested only, at `rest_3g.cpp:611`. Set, it disables the area
  109 switch. The same "set disables the switch" test is in areas 77 and 86
  (`docs/story-flags/event/areas-w2-a-e.md`). Nothing in these files sets it.
  Not set in any of the 22 real saves.
- **85 (0x55)**: set at `area_w4f.cpp:683` on the way from area 197 to area 170
  entry 0x82. Area 170's tail sets and clears the same flag
  (`areas-w4a-w4b.md`), so it reads as a one-trip "arriving by this route"
  marker (reading, not checked). Not set in the real saves.
- **88 (0x58)**: a transit flag between areas 173 and 186. Set when area 173
  sends the party to area 186; area 186's init tests it to run its scripted
  entry; area 173's init tests it to run the return leg, which clears it.
  Not set in the real saves, which fits a flag that lives only during the
  trip.
- **105 (0x69)**: toggled in area 188 when key item 6 is held, before the party
  changes area. What reads it is not in these files: unknown. Not set in the
  real saves.
- **108 (0x6C)**: master 0xC's grant done. Set once with the item grant
  (`rest_2d.cpp:474`); read by `MasterWin_Available` as master 0xC's
  availability. The other masters keep the same fact in `0x904061`. Not set in
  the real saves.
- **117, 118, 139, 140, 141, 142 (0x75, 0x76, 0x8B..0x8E)**: six "the walk in
  area 189 ended on this block" markers. All six cleared when the area 189 walk
  starts; one set when a walk ends on one of six blocks, before the change to
  area 125. Area 152's init also clears all six (`areas-w4a-w4b.md`). What
  reads them is not in these files. None set in the real saves.
- **119 (0x77)**: an "away on the side trip" flag. Cleared on every way back
  out of areas 187, 191, 192 and 193 (with `0x904152` = 0 at most of them);
  set, it stops areas 191 and 192 placing their objects and drives effect
  kind 18 states 77 and 78. It is set elsewhere: not in these files. Area 131
  also clears it (`areas-w3-c-g.md`). Set in real saves 9, 18 and 19, clear in
  the others.
- **130 (0x82)**: set at the end of the area 191 and area 192 tails (after a
  message and a sound stream); tested by area 189's init for
  `Draw_PassFlags` 0x1F. Never cleared in these files. Not set in the real
  saves.
- **138 (0x8A)**: area 193's init starts tail 57 at state 0x14 when it is set;
  that tail clears it with chapter row 14 flag 3. Area 150's tail 58 does the
  same pair of clears (`areas-w3-c-g.md`). Who sets it is not in these files.
  Not set in the real saves.
- **146 (0x92)**: the after-the-ending flag. Copied into the save summary
  (`rest_2c.cpp:402`) and chooses a resident's pose in the building code
  (`rest_4a.cpp:1090`). Not set in the real saves.
- **149 (0x95)**: set by object trigger 64 in area 175, with
  `Party_DropIn(4)`. Nothing in these files reads it. Set in real saves 20, 21
  and 22 (the three late saves), clear in the others.
- **var (the three `0x65DE60` flags)**: the area 109 switch's pattern, 1..8,
  cycled by the hook (pattern mod 6) and read by effect kind 18 state 9
  (`docs/effect_5a.md`). Numbers unresolved.

## Other banks met

- **Chapter row 14** (`Cond_Flags + 0x70`, `0x903FF8`): `area_011.cpp:72..73`
  (bits 0x13 and 0x14 gate `Area11_DimBackdrop`), `area_w4e.cpp` through
  `Row14()` (bits 4, 9, 0xA), `area_w4f.cpp:338`, `:375`, `:547`, `:563`
  (bits 0xA and 3). Documented with the chapter maps.
- **Chapter row 1** (`Cond_Flags + 8`, `0x903F98`): `rest_1a.cpp:401`, bit
  0x16, the party form pose.
- **The current chapter row** (the pointer `Chapter_FlagRow`, `0x929ED0`):
  `boss_h.cpp:355..356` (bits 0x34 and 0x33 by each enemy's status),
  `boss_sd.cpp:113..117`, `:140..141`, `:332..348` (bits 0x1E, 0x23, 0x24,
  0x33, 0x34 and a passed-in flag), `boss_si.cpp:176` (bit 2),
  `area_w4e.cpp:1057`, `:1078` (bits 2 and 5), `area_w4f.cpp:346` (bit 2).
- **`kObjectFlags`** (`at::kObjectFlags`, by object stride):
  `rest_1a.cpp:85` sets bit 0 of a found object's byte.
- **`kMetFlags` `0x904654`** and **`kIntroFlags` `0x904657`**
  (`rest_2c_callees.h:83..84`): the masters' "introduction opened" and
  "introduction done" bits, by master number, at `rest_2c.cpp:719`, `:735`,
  `:746`; `rest_2h.cpp` reads the `0x904657` bits too (comment at `:453`).
- **`Gene_Bits` `0x904650`**: read at `rest_2h.cpp:257`, the 18 gene bits.
- **`Cond_Flags + 0xD1` (`0x904061`)**: bits 3, 4, 5 are masters 0xB, 0xD,
  0xE's grant-done bits (`rest_2d.cpp:480..481`, `rest_2h.cpp:555..558`).
  Past the event bank.
- **`0x904088`**: the masters' ability-grant bits, per the comment at
  `rest_2d.cpp:457..458`.

## Unresolved

- The three flag numbers of the area 109 switch live in the image's .data at
  `0x65DE60` (`rest_3g_callees.h:54`). They are game data and were not read.
  A read of that table (by the owner, or a run) would settle them.
- Who sets 119 (0x77), 138 (0x8A) and 28 (0x1C): not in these files.
- Who reads 105 (0x69), 117/118/139..142, 130 outside area 189's init, and
  149 (0x95): not in these files.
- The meaning of 105 (0x69): the code shows only that it flips once each time
  the area 188 tail runs with key item 6.
- The `Draw_PassFlags` value 0x1F that 130 and 138 set or clear is a drawing
  mode; what the player sees from it needs a run.
