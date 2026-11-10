# Story flags: chapter 18

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 18's row is `Cond_Flags + 0x90` (`0x904020`), save block `+0x640`. Source:
`Scena18_Frame`, `Scena18_EnterArea`, `Scena18_Run` and `Scena18_ObjectTrigger` in
`src/game/scena_sc15.cpp:2728-2758`. [`scenario-roots.md`](../scenario-roots.md)
(lines 38-40) says chapters 18 and 19 hold nothing on the PlayStation disc either.

**Chapter 18 has no flags.** None of its functions sets, clears or tests a flag of
any row. `python tools/story_flags.py chapter 18` lists no sites.

## Start

- `Scena18_Frame` (line 2734) jumps through `Scena18_States`: 0 is
  `ScenaShared_State0` (`scena_sc13.cpp:1424`, the state byte to 1), 1 is
  `Scena18_EnterArea` (line 2758, the state byte to 2), 2 is `Scena18_Run`.
- `Scena18_Run` (line 2740) has one run-table entry, a bare `ret`.
- `Scena18_ObjectTrigger` (line 2746) has one object-table entry, a bare `ret`.

No area, position or party is set. No flag is set at start.

**Who starts it: nothing found.** The callers of `Scenario_Start` are the title's
demo (chapter 16, `title_states.cpp:204`), a new game (chapter 0,
`save_menu.cpp:1027`) and `Area_Enter`'s step by one (`mode_flow.cpp:548-556`). The
step reaches 18 only from chapter 17 with `Field_StatusBits` bit 7 set, and no
`Scena17_*` function sets that bit. Chapter 17 ends by writing chapter 15
instead (`scena_sc15.cpp:2693`). So chapter 18 is not reached in play (reading).

## Flags

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| - | - | No site names a flag of row 18 | - | - | - |

## End

There is no end: nothing in the chapter changes the area, the chapter byte or the
task. The row as left is empty (flags 0..31 zeroed by `Scenario_Start` if it ever
ran, 32..63 untouched). No save of the 22 in [`observed-rows.md`](observed-rows.md)
has a chapter 18 row.

## Event flags used by this chapter

None.

## Unresolved

- Whether any event script or crafted save can set the chapter byte to 18. Our
  source has no path. A crafted save with chapter 18 would run a field with no
  scenario work at all; not tried.
