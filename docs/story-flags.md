# Story flags: what each chapter's flags mean, and what sets them

**Status:** IN PROGRESS (2026-11-09). Chapter 0 is complete. The other chapters
are not started.

Reference material for the save editor's story points
([`save-editor-spec.md`](save-editor-spec.md) section 12.2) and for the wiki. A
table of flag numbers and meanings, read from our source and checked against
saves. It holds no save and no dump of one.

## 1. Where the flags are

- **Chapter flags.** `Cond_Flags` `0x903F90`, save block `+0x5B0`: one 8-byte row
  per chapter, so chapter `c`'s row is at block `+0x5B0 + 8c`.
  `Chapter_FlagRow` `0x929ED0` points at the current chapter's row, as
  `Scenario_Start` sets it.
- **Numbering.** Flag `n` is bit `n & 7` of byte `n >> 3` of the row, low bit
  first. MEASURED: flags 0..7 and 10 of chapter 0 read back as bits 0..7 and 10 of
  the row in `start_normal.DAT`.
- **Event flags** at block `+0x650` are a separate array with their own setters.
  Not covered yet. Three are set one frame into a new game (3, 25, 27).

## 2. Method

A chapter's code names its flags outright: `Flags_Set(Row(), n)`,
`Flags_Test(Row(), n)` and `Flags_Clear(Row(), n)` in `src/game/scena_sc*.cpp`
(109, 38 and 22 calls across the chapters). So:

1. List every call in the chapter's file with its function and step.
2. Read the function around each one for what the flag gates.
3. Check the reading against real saves from that chapter.

Code outside the chapter files can reach the same rows (`Area_TestCondition`
tests `Cond_Flags + 8 * (kind & 0x1F)`), so step 1 must also search the area files
for a chapter before its table is called complete. For chapter 0 that search is
still owed.

## 3. Chapter 0 (the opening: the whelp)

Source: `src/game/scena_sc0.cpp`. Every `Flags_Set` in the file is in this table.
The file has no `Flags_Clear`.

| Flag | Set by | When | What it gates |
|---:|---|---|---|
| 0 | `Scena00_EnterArea`; also `Scena00_Run5` step 0 | First entry to area 4 | Area 4's first-entry scene (run 2) plays once |
| 1 | `Scena00_EnterArea`; also `Scena00_Run5` step 0 | First entry to area `0x1F` | Area `0x1F`'s first-entry scene (run 3) plays once |
| 2 | `Scena00_Area02`; also `Scena00_Run5` step 0 | First entry to area 2 | Area 2's first set-up (camera, elevation) runs once |
| 3 | `Scena00_Area18`; also `Scena00_Run5` step 0 | First set-up of area `0x18`, on the first frame of a new game | Area `0x18`'s opening set-up (music, camera) runs once |
| 4 | `Scena00_Run5` step `0xA` | The opening sequence ends, before the change into area `0x18` | Nothing in this file tests it |
| 5 | `Scena00_Run5` step `0xF` | Event battle 1 starts | `Scena00_Area18`: the member set-up after that battle |
| 6 | `Scena00_Run8` step 0 | Entering area `0x18` by entrance 0 | `Scena00_StepHook`: arms the trigger for event battle 7, the boss, in area 2 |
| 7 | `Scena00_Run6` step `0xA` | Event battle 4's encounter starts (step trigger in area `0x18`) | The trigger fires once |
| 8 | `Scena00_Run6` step `0x14` | Event battle 5's encounter starts (area `0x18`, entrance 2) | The encounter happens once |
| 9 | `Scena00_Run6` step `0x1E` | Event battle 6's encounter starts (area `0x18`, entrance 4) | The encounter happens once |
| 10 | `Scena00_Area02` | The next entry to area 2 after flag 2 | That entry's set-up runs once |

"Entrance" is `Cond_ByteFD`, the byte the area set-up switches on. That it is the
entrance used is a reading, not checked.

`Scena00_Run5` step 0 sets flags 0..3 together, so the opening sequence marks
those four first-entry set-ups as done before the player has control.

**Flags are set when an encounter starts, not when its battle is won.**

### Checked against saves

| Save | Flags set | Reading |
|---|---|---|
| `start.DAT`, one frame into a new game | 3 | `Scena00_Area18` ran its opening set-up. `Scena00_Run5` step 0 had not run yet |
| `start_normal.DAT`, first save point, 3 battles | 0..7, 10 | Battles 1, 4 and 7 fought. 5 and 6 skipped |
| US 0:12 and JP 0:25 (RetroArch), 5 battles | 0..10 | All five battles fought |

The battle counts and the flags agree with the owner's account: the section has
five possible battles ([`boss-fights.md`](boss-fights.md): event battles 1, 4, 5,
6 and 7), three fought on PC and all five on the disc saves.

**Baseline on leaving chapter 0:** flags 0..7 and 10 are set on every route.
Flags 8 and 9 are the two optional encounters.

Chapter 1's row has flags 0 and 1 set at the first save point on all three saves.
Their setters belong to chapter 1's table.

### Also set by the chapter's start

`Scena00_Start` (original `0x537F30`) writes -1 to the 18 dwords
`0x904608..0x90464F`. That is block `+0xC28..+0xC6F`, and those 72 bytes are all
`0xFF` in `start.DAT` (MEASURED). This is the writer the save editor spec was
looking for. What the dwords hold later is not established: in later saves one
byte of each of the first eight stays `0xFF` and the other three vary.

## 4. To do

- Chapter 0: search the area files for tests of chapter 0's row.
- Chapters 1..15 and 17, by the same method.
- The event flags at `+0x650` and their setters.
