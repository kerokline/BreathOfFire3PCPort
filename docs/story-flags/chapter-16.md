# Story flags: chapter 16

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 16 is the title screen's attract-mode demo, not a story chapter. Its row is
`Cond_Flags + 0x80` (`0x904010`), save block `+0x630`. Source: the `Scena16_*`
functions in `src/game/field_modes.cpp` (lines 172-651), `Scena16_PartyReset` in
`src/game/move_cmds.cpp:494`, and `Scena16_ObjectTrigger` in
`src/game/scena_sc15.cpp:1785`. The flag helpers are `g.flags_test` and
`g.flags_set` on `FlagBits()`, which is `Chapter_FlagRow` (`field_modes.cpp:77`).

`python tools/story_flags.py chapter 16` lists no sites: the extractor does not
see `g.flags_test` / `g.flags_set`. The four flags below were found by reading.

## Start

**Who starts it.** Only the title screen. `Title_StateStartDemo`
(`title_states.cpp:191-206`) calls `Scenario_Start(0x10)` once no transition is
busy, with the play clock and the draw-pass flags zeroed, and creates task 0 on the
demo's entry. `Scenario_Start` (`title_states.cpp:404-423`) zeroes the row's first
dword (flags 0..31) and points `Chapter_FlagRow` at it.

The story never reaches chapter 16. `Area_Enter` steps the chapter byte from 15 to
16 and then straight to 17 (`mode_flow.cpp:551-554`).

**`Scena16_Start`** (state 0, `field_modes.cpp:185`):

- Call-table entry 0, `Scena16_PartyReset` (`move_cmds.cpp:494`):
  `PartySet_Load(0xA, 0xFF, 0xFF, 0)`, both party lists emptied, then
  `Party_Join(0xA)`. It writes `Cond_Flags + 0xD1` (bits 0..2 set) and
  `+0xD2..+0xD7` (`0xFF`). The code calls those party bytes, not a flag row.
- The pass flags `0x1F`, the change to area 4 at (`0x1A0000`, `0x88000`) facing 5,
  the same four written to the pending change, `Field_ScriptFlags |= 0x240`,
  music 6, a wait for the load, the counters 0, state 1.
- No flag set.

## Flags

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena16_Area04`, `field_modes.cpp:251` | First entry to area 4 (from `Scena16_Start`). The pass flags 0, `ScriptFlags_Set40`, run 2 (`Scena16_Scene2`) | - | `Scena16_Area04` line 248: run 2 starts once | once-only set-up |
| 1 | `Scena16_Area1F`, `field_modes.cpp:240` | First entry to area `0x1F` (from `Scena16_Scene2` step 11, line 403). The pass flags 0, run 3 | - | `Scena16_Area1F` line 237 | once-only set-up |
| 2 | `Scena16_Area02`, `field_modes.cpp:278` | First entry to area 2 (from `Scena16_Scene3` step 9, line 520). Counters cleared, elevation `0x300`, `Kind2_Place(0)`, an effect, the leader at (`0x330000`, `0x400000`) | - | `Scena16_Area02` line 269 | once-only set-up |
| 10 (`0xA`) | `Scena16_Area02`, `field_modes.cpp:282` | The next entry to area 2 after flag 2. `g.place_party(0)` (the original's call `0x531F90(0)`), `Field_Kind2X/Z` (`0x260000`, `0x1B8000`) | - | `Scena16_Area02` line 281 | once-only set-up |

These are the same numbers, areas and order as chapter 0's flags 0, 1, 2 and 10
([`story-flags.md`](../story-flags.md) section 3): the demo replays the opening's
areas 4, `0x1F` and 2.

The demo's scenes (`Scena16_Scene2`, `Scene3`, `Scene4`, `field_modes.cpp:323`,
`447`, `560`) and its end (`Scena16_End`, line 299) set no flag.
`Scena16_ObjectTrigger` (`scena_sc15.cpp:1785`) calls the one object-table entry,
`0x43C9F0`, which only answers 0 ([`scena_sc15.md`](../scena_sc15.md) section 3.2).

## End

`Scena16_Scene4` step 12 (`field_modes.cpp:640`) sets run 1. `Scena16_End`
(line 299) turns the badge and the pass flags off, restores the colours, plays two
sound effects, stops the music and ends the task. `Title_StateDemo`
(`title_states.cpp:215`) sees task 0 gone and goes back to title state 0. Start
pressed during the demo sets run 1 too (line 221). **Chapter 16 hands over to no
chapter**: it ends back at the title.

**The row as left** (reading): 0, 1 and 2 are set on every full run of the demo.
Flag 10 needs a second entry to area 2; the demo changes to area 2 once only
(`Scene3` step 9), so 10 is most likely never set (reading, not checked). A demo cut
short by Start leaves fewer.

**Observed rows.** None of the 22 saves in [`observed-rows.md`](observed-rows.md)
lists a chapter 16 row (the page lists only rows with a flag set). That fits: the
demo runs from the title, and its row is not part of any game in progress. Whether a
save made after watching the demo can carry these flags depends on what a new game
or a load writes over `Cond_Flags + 0x80`; not read.

## Event flags used by this chapter

None. No `Scena16_*` function sets, clears or tests `Cond_Flags + 0xA0`.

## Unresolved

- **Flag 10 in the demo.** Whether the demo enters area 2 twice. A demo run with
  the row logged would settle it.
- **`Cond_Flags + 0xD1..+0xD7`** written by `Scena16_PartyReset`. The code treats
  them as party bytes; whether they also fall inside the event-flag range a save
  editor shows is for the save-editor spec, not this page.
- **What resets row 16** for a game after the demo. `Scenario_Start(0x10)` clears
  only flags 0..31 when the demo starts; nothing clears them after it.
