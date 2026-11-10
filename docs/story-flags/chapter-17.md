# Story flags: chapter 17

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 17 is the ending: the intro in area `0xC7`, the staff roll and the step
after it ([`ending-backdrop.md`](../ending-backdrop.md),
[`scena_sc15.md`](../scena_sc15.md) section 3.3). Its row is `Cond_Flags + 0x88`
(`0x904018`), save block `+0x638`. Source: the `Scena17_*` functions in
`src/game/scena_sc15.cpp` (lines 1790-2726).

**Chapter 17 uses no flag of its own row.** No `Scena17_*` function sets, clears or
tests the current row. Its one flag work is on other rows, at the very end
(`Scena17_EndReturn`, below). `python tools/story_flags.py chapter 17` lists no
sites, which agrees.

## Start

**Who starts it.** Chapter 15's `Scena15_Run6` step `0x36`
(`scena_sc15.cpp:1581-1587`) changes to area `0xC7` and calls `Field_SetStatus80`.
`Area_Enter` (`mode_flow.cpp:548-556`) then steps the chapter byte 15 + 1 = 16,
skips 16 to 17 and calls `Scenario_Start(17)`. `Scenario_Start`
(`title_states.cpp:404-423`) zeroes flags 0..31 of row 17 and points
`Chapter_FlagRow` at it.

A crafted save with chapter 17, state 0 and area `0xC7` also starts it
(`ending-backdrop.md` section 1, recipe `ending17`). Loading a save sets
`Chapter_FlagRow` from the chapter byte (`save_menu.cpp:1255-1257`) and does not
zero the row.

**`Scena17_Start`** (state 0, line 1856): the colours restored, the pass flags
`0x1F`, the change to area `0xC7` at (`0xB60000`, `0x128000`) facing 7, the four
script counters 0, state 1. No flag set. The party is placed by the chapter, not
taken from the save (`ending-backdrop.md` section 1).

**`Scena17_EnterArea`** (state 1, line 1871): in area `0xC7`, the script flags' bit
`0x40` (`ScriptFlags_Set40`, not a story flag), the camera, one field frame drawn by
hand; then part 1. No flag set.

## Flags

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| - | - | No site names a flag of row 17 | - | - | - |

`Scena17_ObjectTrigger` (line 2346) calls the one object-table entry,
`0x43C9F0`, which only answers 0.

## End

Part 3, `Scena17_Outro`, ends in a choice:

1. `Scena17_Outro10` (line 2308) opens script message 1 once the transition is done.
2. `Scena17_Outro11` (line 2326) waits for the message to close and reads its answer
   in the step byte `0x8034E5`:
   - **Not 0:** `Sound_StopChannels` and `Task_Restart(Boot_Task)` (lines
     2332-2333). No flag is written. The game goes back to boot with chapter 17 in
     memory.
   - **0:** the next step, `Scena17_Outro12` (line 2338): part 0, step 0, and the
     task restarted at `Scena17_EndTask` (line 2665).
3. `Scena17_EndTask` runs `Scena17_EndSteps[Game_Step]` each frame:
   - **Step 0, `Scena17_EndReturn`** (line 2682): sets **event flag 146
     (`0x92`)** (line 2684); **clears flags 9..22 of chapter 15's row**
     (`Cond_Flags + 0x78`, line 2685); the party's pass; `Game_AreaNumber` `0x8F`;
     the leader at (`0xF0000`, `0x218000`) facing 3; `Music_Track` `0x6F`; **the
     chapter byte 15** (line 2693) with part, run and step 0; menu and shop bytes;
     file `0x31B` loaded a frame at a time; two colour rows copied; `Game_Step` + 1.
   - **Step 1, `Scena17_EndField`** (line 2712): the field's task records, then
     `ShopMode_Dispatch` each frame.
   - **Step 2, `Scena17_EndRestart`** (line 2719): the four counters 0, the sound
     stopped, `Task_Restart(Boot_Task)`.

**Chapter 17 hands back to chapter 15**, by writing the chapter byte, not by
`Scenario_Start`. So chapter 15's row is not zeroed: it keeps what it had, less
flags 9..22. `Chapter_FlagRow` is not rewritten in `Scena17_EndReturn`; it still
points at row 17 until the next load or chapter start (reading).

**Event flag 146 after the ending.** `Save_BuildBlock` (`rest_2c.cpp:401-402`)
copies it into the save's byte `0x904694` when a save is written.
`CommuPose_KindB` (`rest_4a.cpp:1090`) tests it for a pose. So 146 marks a game
that has passed `Scena17_EndReturn` (reading, not checked).

**The row as left:** row 17 is empty, flags 0..31 zeroed by `Scenario_Start`,
32..63 not touched. Chapter 15's row: see `chapter-15.md`, End.

**Observed rows.** No save of the 22 is in chapter 17 or after it. Nothing here is
checked against a save.

## Event flags used by this chapter

| Event flag | Site | What |
|---:|---|---|
| 146 (`0x92`) | Set by `Scena17_EndReturn`, line 2684 | Set only on the answer-0 path after the roll. Tested by `Save_BuildBlock` and `CommuPose_KindB` (above) |

## Unresolved

- **What message 1's two answers are.** The code shows answer 0 leads to
  `Scena17_EndReturn` (flags written, chapter 15, area `0x8F`) and any other
  answer to a plain restart. The message text is in the game data.
- **What `ShopMode_Dispatch` shows at end step 1**, and what moves `Game_Step` from
  1 to 2. A save menu fits `Save_BuildBlock` testing event flag 146, but that is a
  reading, not checked.
- **Whether a save written at end step 1 holds chapter 15 with flags 9..22 clear.**
  A run to the end of the roll with a save would settle it.
- **`Chapter_FlagRow` after `Scena17_EndReturn`.** If anything in end step 1 writes
  "the current row", it writes row 17, not row 15. Nothing found does; not checked.
