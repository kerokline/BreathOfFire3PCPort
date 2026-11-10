# Story flags: chapter 19

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Chapter 19's row is `Cond_Flags + 0x98` (`0x904028`), save block `+0x648`, the last
chapter row. The next 8 bytes, `Cond_Flags + 0xA0`, are the event flags, not a
chapter row. Source: `Scena19_Frame`, `Scena19_Run`, `Scena19_Run0` and
`Scena19_ObjectTrigger` in `src/game/scena_sc15.cpp:2750-2777`, with chapter 18's
`Scena18_EnterArea` as its state 1. [`scenario-roots.md`](../scenario-roots.md)
(lines 38-40) says chapters 18 and 19 hold nothing on the PlayStation disc either.

**Chapter 19 has no flags.** None of its functions sets, clears or tests a flag of
any row. `python tools/story_flags.py chapter 19` lists no sites.

## Start

- `Scena19_Frame` (line 2753) jumps through `Scena19_States`: 0 is
  `ScenaShared_State0` (`scena_sc13.cpp:1424`), 1 is `Scena18_EnterArea`
  (line 2758), 2 is `Scena19_Run`.
- `Scena19_Run` (line 2762) has one run-table entry, `Scena19_Run0` (line 2768): on
  `Input_Pressed` bit 8, an effect of kind `0x2F`. It does this every frame the
  bit is set; it sets no flag.
- `Scena19_ObjectTrigger` (line 2775) has one object-table entry, a bare `ret`.

No area, position or party is set. No flag is set at start.

**Who starts it: nothing found.** `Area_Enter`'s step by one
(`mode_flow.cpp:548-556`) reaches 19 only from chapter 18 with `Field_StatusBits`
bit 7 set, and chapter 18 has no code that sets it (see `chapter-18.md`). No call of
`Scenario_Start` passes 19. So chapter 19 is not reached in play (reading).

## Flags

| Flag | Set by | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| - | - | No site names a flag of row 19 | - | - | - |

## End

There is no end: nothing in the chapter changes the area, the chapter byte or the
task. The row as left is empty. No save of the 22 in
[`observed-rows.md`](observed-rows.md) has a chapter 19 row.

## Event flags used by this chapter

None.

## Unresolved

- What effect kind `0x2F` does when `Scena19_Run0` places it. Not a flag question;
  noted because it is the chapter's only work.
- Whether anything can set the chapter byte to 19. Our source has no path.
