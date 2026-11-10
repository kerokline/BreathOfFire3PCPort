# Story flags: chapter 0

**Status:** IN PROGRESS (2026-11-09, read from source and checked against seven
saves; the area files are not yet searched for tests of this row).

Source: `src/game/scena_sc0.cpp`. Every `Flags_Set` in the file is in the table.
The file has no `Flags_Clear`.

## Start

A new game. `TitleFlow_NewGame` (`save_menu.cpp`) calls `Scenario_Start(0)`.
`Scena00_Start` (original `0x537F30`) goes to area `0x18`, starts run 5 and sets no
flag. It also writes -1 to the 18 dwords `0x904608..0x90464F`, which is save block
`+0xC28..+0xC6F`: those 72 bytes are all `0xFF` one frame into a new game
(MEASURED on `start.DAT`). What the dwords hold later is not established.

## Flags

| Flag | Set by | When | Tested by / what it gates | Kind |
|---:|---|---|---|---|
| 0 | `Scena00_EnterArea`; also `Scena00_Run5` step 0 | First entry to area 4 | Area 4's first-entry scene (run 2) plays once | once-only set-up |
| 1 | `Scena00_EnterArea`; also `Scena00_Run5` step 0 | First entry to area `0x1F` | Area `0x1F`'s first-entry scene (run 3) plays once | once-only set-up |
| 2 | `Scena00_Area02`; also `Scena00_Run5` step 0 | First entry to area 2 | Area 2's first set-up (camera, elevation) runs once | once-only set-up |
| 3 | `Scena00_Area18`; also `Scena00_Run5` step 0 | First set-up of area `0x18`, on the first frame of a new game | Area `0x18`'s opening set-up (music, camera) runs once | once-only set-up |
| 4 | `Scena00_Run5` step `0xA` | The opening sequence ends, before the change into area `0x18` | Nothing in this file tests it | progress |
| 5 | `Scena00_Run5` step `0xF` | Event battle 1 starts | `Scena00_Area18`: the member set-up after that battle | progress |
| 6 | `Scena00_Run8` step 0 | Entering area `0x18` by entrance 0 | `Scena00_StepHook`: arms the trigger for event battle 7, the boss, in area 2 | progress |
| 7 | `Scena00_Run6` step `0xA` | Event battle 4's encounter starts (step trigger in area `0x18`) | The trigger fires once | progress |
| 8 | `Scena00_Run6` step `0x14` | Event battle 5's encounter starts (area `0x18`, entrance 2) | The encounter happens once | optional |
| 9 | `Scena00_Run6` step `0x1E` | Event battle 6's encounter starts (area `0x18`, entrance 4) | The encounter happens once | optional |
| 10 | `Scena00_Area02` | The next entry to area 2 after flag 2 | That entry's set-up runs once | once-only set-up |

"Entrance" is `Cond_ByteFD`, the byte the area set-up switches on. That it is the
entrance used is a reading, not checked.

`Scena00_Run5` step 0 sets flags 0..3 together, so the opening sequence marks
those four first-entry set-ups as done before the player has control.

**Flags are set when an encounter starts, not when its battle is won.**

## End

`Scena00_Run11` step 9 changes to area 9 and sets bit 7 of `Field_StatusBits`
(read by the chapter 1 pass). On that area entry `Area_Enter` starts chapter 1.

**The row as left:** flags 0..7 and 10 on every route. Flags 8 and 9 are the two
optional encounters.

## Checked against saves

| Save | Flags set | Reading |
|---|---|---|
| `start.DAT`, one frame into a new game | 3 | `Scena00_Area18` ran its opening set-up. `Scena00_Run5` step 0 had not run yet |
| `start_normal.DAT`, first save point, 3 battles | 0..7, 10 | Battles 1, 4 and 7 fought. 5 and 6 skipped |
| English psxrecomp card, 0:11, 3 battles | 0..7, 10 | The same three battles as the PC save |
| Authentic psxrecomp JP saves, 0:28, 5:19, 12:48 | 0..7, 10 | The three-battle route, unchanged in later chapters |
| US 0:12 and JP 0:25 (RetroArch), 5 battles | 0..10 | All five battles fought |

The battle counts and the flags agree with the owner's account: the section has
five possible battles ([`boss-fights.md`](../boss-fights.md): event battles 1, 4, 5,
6 and 7).

Block `+0x68B` also differs between the three-battle and five-battle saves, so it
follows the optional encounters too. Not read yet.

## Unresolved

- The area files are not searched for tests of chapter 0's row.
- Chapter 16, the title demo, uses the same flag numbers 0, 1, 2 and 10 for the
  same areas ([`chapter-16.md`](chapter-16.md)).
